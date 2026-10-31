#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../../src/kernel/kernel_guest.h"

void xbox_log(int level, const char *subsystem, const char *fmt, ...)
{ (void)level; (void)subsystem; (void)fmt; assert(!"Unexpected IRQL warning"); }
static HANDLE started, finished;
static volatile LONG inside;
static unsigned shared;
static DWORD WINAPI worker(LPVOID unused)
{
    unsigned i;
    (void)unused;
    SetEvent(started);
    for (i = 0; i < 4000; i++) {
        xbox_GuestEnter();
        assert(InterlockedIncrement(&inside) == 1);
        shared++;
        assert(InterlockedDecrement(&inside) == 0);
        xbox_GuestLeave();
    }
    SetEvent(finished);
    return 0;
}
static DWORD WINAPI interrupt_worker(LPVOID unused)
{
    KIRQL old;
    (void)unused;
    old = xbox_KfRaiseIrql(26);
    SetEvent(started); /* Gate held, now queue for the CPU. */
    xbox_GuestEnter();
    assert(xbox_CurrentIrql() == 26);
    xbox_GuestLeave();
    xbox_KfLowerIrql(old);
    SetEvent(finished);
    return 0;
}
int main(void)
{
    HANDLE thread, threads[4];
    unsigned i;
    int depth;
    ULONGLONG begin;
    KIRQL old;
    started = CreateEvent(NULL, TRUE, FALSE, NULL);
    finished = CreateEvent(NULL, TRUE, FALSE, NULL);
    assert(started && finished);
    xbox_GuestEnter(); xbox_GuestEnter();
    thread = CreateThread(NULL,0,worker,NULL,0,NULL); assert(thread);
    assert(WaitForSingleObject(started,1000)==WAIT_OBJECT_0);
    assert(WaitForSingleObject(finished,10)==WAIT_TIMEOUT);
    depth = xbox_GuestSuspend(); assert(depth == 2);
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);
    xbox_GuestResume(depth); xbox_GuestLeave(); xbox_GuestLeave(); CloseHandle(thread);
    for (i=0; i<4; i++) { threads[i]=CreateThread(NULL,0,worker,NULL,0,NULL); assert(threads[i]); }
    assert(WaitForMultipleObjects(4,threads,TRUE,10000)==WAIT_OBJECT_0);
    assert(shared==20000 && inside==0);
    for(i=0;i<4;i++) CloseHandle(threads[i]);

    ResetEvent(started); ResetEvent(finished);
    xbox_GuestEnter();
    thread=CreateThread(NULL,0,worker,NULL,0,NULL); assert(thread);
    assert(WaitForSingleObject(started,1000)==WAIT_OBJECT_0);
    begin=GetTickCount64();
    /* A pure guest poll must allow the worker it waits for to run. */
    while(WaitForSingleObject(finished,0)==WAIT_TIMEOUT && GetTickCount64()-begin<3000)
        recomp_preempt();
    assert(WaitForSingleObject(finished,0)==WAIT_OBJECT_0);
    xbox_GuestLeave(); CloseHandle(thread);

    ResetEvent(started); ResetEvent(finished);
    xbox_GuestEnter();
    thread=CreateThread(NULL,0,interrupt_worker,NULL,0,NULL); assert(thread);
    assert(WaitForSingleObject(started,1000)==WAIT_OBJECT_0);
    /* Previously inverted CPU -> dispatch versus ISR dispatch -> CPU. */
    old=xbox_KfRaiseIrql(DISPATCH_LEVEL);
    xbox_KfLowerIrql(old);
    xbox_GuestLeave();
    assert(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0); CloseHandle(thread);

    ResetEvent(started); ResetEvent(finished);
    xbox_GuestEnter(); old=xbox_KeRaiseIrqlToDpcLevel();
    thread=CreateThread(NULL,0,worker,NULL,0,NULL); assert(thread);
    assert(WaitForSingleObject(started,1000)==WAIT_OBJECT_0);
    Sleep(2); recomp_preempt();
    assert(WaitForSingleObject(finished,0)==WAIT_TIMEOUT);
    xbox_KfLowerIrql(old); xbox_GuestLeave();
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0); CloseHandle(thread);
    CloseHandle(started); CloseHandle(finished);
    puts("Guest CPU exclusion, nested host waits, poll preemption, ISR lock order and raised-IRQL retention passed");
    return 0;
}
