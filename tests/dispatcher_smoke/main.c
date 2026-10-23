#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../src/kernel/kernel_dispatcher.h"
#include "platform/guest_memory.h"

static uint32_t event[4], semaphore[5];
static HANDLE waiting;
static NTSTATUS transient_result;
static uint32_t second_event[4];
static int transient_all;
static void init_event(unsigned type, unsigned state)
{
    event[0] = type | (4u << 16);
    event[1] = state;
    event[2] = event[3] = 0;
}
static NTSTATUS poll(void *p)
{
    LARGE_INTEGER zero = {0};
    XboxDispatcherWaitItem item = {p, NULL};
    return xbox_DispatcherWait(1, &item, 0, FALSE, &zero);
}
static DWORD WINAPI waiter(LPVOID unused)
{
    LARGE_INTEGER timeout;
    XboxDispatcherWaitItem item = {event, NULL};
    (void)unused;
    timeout.QuadPart = -20000000;
    SetEvent(waiting);
    assert(xbox_DispatcherWait(1, &item, 0, FALSE, &timeout) == STATUS_SUCCESS);
    return 0;
}
static DWORD WINAPI abandoner(LPVOID handle)
{
    assert(WaitForSingleObject((HANDLE)handle, 1000) == WAIT_OBJECT_0);
    return 0; /* Deliberately abandon a native mutant. */
}
static DWORD WINAPI transient_waiter(LPVOID unused)
{
    LARGE_INTEGER timeout;
    XboxDispatcherWaitItem items[2]={{event,NULL},{second_event,NULL}};
    (void)unused;timeout.QuadPart=-5000000;
    xbox_MemoryEnter();SetEvent(waiting);
    transient_result=xbox_DispatcherWait(transient_all ? 2 : 1,items,transient_all,FALSE,&timeout);
    xbox_MemoryLeave();return 0;
}
static int transient_set(unsigned type,int wait_all)
{
    HANDLE thread;
    ULONGLONG start;
    init_event(type,0);
    transient_all=wait_all;
    memset(second_event,0,sizeof second_event);second_event[0]=type|(4u<<16);
    waiting=CreateEvent(NULL,TRUE,FALSE,NULL);assert(waiting);
    thread=CreateThread(NULL,0,transient_waiter,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(waiting,1000)==WAIT_OBJECT_0);
    start=GetTickCount64();
    while(!xbox_MemoryTryGpu()) {
        assert(GetTickCount64()-start<3000);SwitchToThread();
    }
    /* RAM exclusion holds the registered waiter at its resume, so clearing
     * the state before it runs is deterministic. A satisfied wait must stay
     * complete even when an XDK inline store clears the event immediately. */
    xbox_DispatcherSetEvent(event);
    if(wait_all) {
        assert(event[1]==1 && !second_event[1]); /* Incomplete WaitAll consumes nothing. */
        event[1]=0;xbox_DispatcherSetEvent(second_event);
        assert(!event[1] && second_event[1]==1); /* Earlier Set is not a future token. */
        xbox_DispatcherSetEvent(event);
    }
    event[1]=0;second_event[1]=0;
    xbox_MemoryEndGpu();
    assert(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0);
    CloseHandle(thread);CloseHandle(waiting);
    if(transient_result!=STATUS_SUCCESS) {
        fprintf(stderr,"Set/inline-clear lost registered type%u waiter: %08X\n",
            type,(unsigned)transient_result);return 0;
    }
    assert(poll(event)==STATUS_TIMEOUT);return 1;
}
int main(void)
{
    HANDLE thread, host;
    XboxDispatcherWaitItem items[2];
    LARGE_INTEGER zero = {0};
    LONG previous;
    unsigned i;
    init_event(0, 0);
    assert(xbox_DispatcherIsGuestObject(event));
    assert(poll(event) == STATUS_TIMEOUT);
    assert(xbox_DispatcherSetEvent(event) == 0);
    assert(xbox_DispatcherSetEvent(event) == 1);
    assert(poll(event) == STATUS_SUCCESS && poll(event) == STATUS_SUCCESS);
    /* The XDK does this inline, without calling KeResetEvent. Reuse the
     * exact storage: a cached Windows manual event incorrectly stays set. */
    event[1] = 0;
    assert(poll(event) == STATUS_TIMEOUT);
    for (i = 0; i < 1000; i++) {
        init_event(i & 1, 0);
        assert(poll(event) == STATUS_TIMEOUT);
        assert(xbox_DispatcherSetEvent(event) == 0);
        assert(poll(event) == STATUS_SUCCESS);
        assert(poll(event) == ((i & 1) ? STATUS_TIMEOUT : STATUS_SUCCESS));
    }
    init_event(0, 0);
    waiting = CreateEvent(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, waiter, NULL, 0, NULL);
    assert(waiting && thread);
    assert(WaitForSingleObject(waiting, 1000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(thread, 10) == WAIT_TIMEOUT);
    assert(xbox_DispatcherSetEvent(event) == 0);
    assert(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    CloseHandle(thread); CloseHandle(waiting);
    assert(xbox_DispatcherResetEvent(event) == 1 && poll(event) == STATUS_TIMEOUT);
    init_event(1, 0);
    assert(xbox_DispatcherPulseEvent(event) == 0 && poll(event) == STATUS_TIMEOUT);
    waiting = CreateEvent(NULL, TRUE, FALSE, NULL);
    thread = CreateThread(NULL, 0, waiter, NULL, 0, NULL);
    assert(waiting && thread && WaitForSingleObject(waiting, 1000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(thread, 20) == WAIT_TIMEOUT);
    assert(xbox_DispatcherPulseEvent(event) == 0);
    assert(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0);
    assert(poll(event) == STATUS_TIMEOUT);
    CloseHandle(thread); CloseHandle(waiting);
    init_event(0, 0);

    semaphore[0] = 5 | (5u << 16); semaphore[1] = 0; semaphore[4] = 3;
    assert(xbox_DispatcherIsGuestObject(semaphore));
    assert(xbox_DispatcherReleaseSemaphore(semaphore, 2, &previous) == STATUS_SUCCESS && previous == 0);
    assert(poll(semaphore) == STATUS_SUCCESS && semaphore[1] == 1);
    assert(xbox_DispatcherReleaseSemaphore(semaphore, 3, &previous) == (NTSTATUS)0xC0000047u);
    assert(semaphore[1] == 1 && poll(semaphore) == STATUS_SUCCESS);
    assert(poll(semaphore) == STATUS_TIMEOUT);

    /* WaitAll must not consume a signalled host auto event while a guest
     * object is unsignalled, nor consume a guest semaphore if a host is not. */
    host = CreateEvent(NULL, FALSE, TRUE, NULL);
    assert(host);
    items[0] = (XboxDispatcherWaitItem){event, NULL};
    items[1] = (XboxDispatcherWaitItem){NULL, host};
    assert(xbox_DispatcherWait(2, items, 1, FALSE, &zero) == STATUS_TIMEOUT);
    assert(WaitForSingleObject(host, 0) == WAIT_OBJECT_0);
    SetEvent(host); xbox_DispatcherSetEvent(event);
    assert(xbox_DispatcherWait(2, items, 1, FALSE, &zero) == STATUS_SUCCESS);
    assert(WaitForSingleObject(host, 0) == WAIT_TIMEOUT);
    xbox_DispatcherResetEvent(event); SetEvent(host);
    assert(xbox_DispatcherWait(2, items, 0, FALSE, &zero) == 1);
    assert(WaitForSingleObject(host, 0) == WAIT_TIMEOUT);
    items[0].guest = semaphore;
    xbox_DispatcherReleaseSemaphore(semaphore, 1, NULL);
    assert(xbox_DispatcherWait(2, items, 1, FALSE, &zero) == STATUS_TIMEOUT && semaphore[1] == 1);
    CloseHandle(host);
    host = CreateMutex(NULL, FALSE, NULL); assert(host);
    thread = CreateThread(NULL, 0, abandoner, host, 0, NULL); assert(thread);
    assert(WaitForSingleObject(thread, 1000) == WAIT_OBJECT_0); CloseHandle(thread);
    items[1].host = host;
    assert(xbox_DispatcherWait(2, items, 1, FALSE, &zero) == 0x81 && semaphore[1] == 0);
    assert(ReleaseMutex(host)); CloseHandle(host);
    items[0].guest = event; items[1] = items[0];
    assert(xbox_DispatcherWait(2, items, 1, FALSE, &zero) == STATUS_INVALID_PARAMETER);
    if(xbox_MemoryGateEnabled() && (!transient_set(0,0) || !transient_set(1,0)
        || !transient_set(0,1) || !transient_set(1,1)))return 2;
    puts("Guest dispatcher registered Set/clear, WaitAll, inline reuse, pulse, semaphores and mixed/abandoned waits passed");
    return 0;
}
