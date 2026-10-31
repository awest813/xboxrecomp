/* Exercise real host critical sections through the kernel scheduling policy. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "kernel/kernel_guest.h"
#include "kernel/xbox_memory_layout.h"
#include "platform/guest_memory.h"
RECOMP_TLS uint32_t g_esp,g_xbox_kernel_caller;
uint32_t g_xbox_code_lo,g_xbox_code_hi;
ptrdiff_t g_xbox_mem_offset;
void xbox_log(int level,const char *subsystem,const char *fmt,...)
{ (void)level;(void)subsystem;(void)fmt; }
static RTL_CRITICAL_SECTION guest_cs;
static HANDLE owner_waiting,release_owner,waiter_called,waiter_done,contender_ready;
static volatile LONG contender_ran,packet[2],gpu_committed;
static void take(void) { xbox_RtlEnterCriticalSection(&guest_cs); }
static void drop(void) { xbox_RtlLeaveCriticalSection(&guest_cs); }
static void host_wait(void)
{
    LARGE_INTEGER timeout;timeout.QuadPart=-50000000;
    SetEvent(owner_waiting);
    assert(xbox_NtWaitForSingleObject(release_owner,FALSE,&timeout)==STATUS_SUCCESS);
}
static DWORD WINAPI owner(LPVOID unused)
{
    (void)unused;xbox_GuestEnter();xbox_GuestKernelCall(277,take);
    xbox_GuestKernelCall(158,host_wait);xbox_GuestKernelCall(294,drop);
    xbox_GuestLeave();return 0;
}
static void contended_take(void) { SetEvent(waiter_called);take(); }
static DWORD WINAPI waiter(LPVOID unused)
{
    (void)unused;xbox_GuestEnter();xbox_GuestEnter();
    xbox_GuestKernelCall(277,contended_take);
    if(gpu_committed)assert(packet[0]==0x123 && packet[1]==0x123);
    xbox_GuestKernelCall(278,take); /* Same thread may recurse. */
    xbox_GuestKernelCall(295,drop);xbox_GuestKernelCall(294,drop);
    xbox_GuestLeave();xbox_GuestLeave();SetEvent(waiter_done);return 0;
}
static DWORD WINAPI contender(LPVOID unused)
{
    (void)unused;SetEvent(contender_ready);xbox_GuestEnter();
    InterlockedExchange(&contender_ran,1);xbox_GuestLeave();return 0;
}
int main(void)
{
    HANDLE held,waiting,thread;ULONGLONG deadline;int acquired=0,fast,gil;
    const char *e=getenv("RECOMP_CS_GUEST_FASTPATH");fast=e && *e=='1';
    e=getenv("RECOMP_GIL");gil=!(e && *e=='0');
    assert(xbox_MemoryGateEnabled());
    owner_waiting=CreateEvent(NULL,FALSE,FALSE,NULL);release_owner=CreateEvent(NULL,FALSE,FALSE,NULL);
    waiter_called=CreateEvent(NULL,FALSE,FALSE,NULL);waiter_done=CreateEvent(NULL,FALSE,FALSE,NULL);
    contender_ready=CreateEvent(NULL,FALSE,FALSE,NULL);
    assert(owner_waiting && release_owner && waiter_called && waiter_done && contender_ready);
    xbox_RtlInitializeCriticalSection(&guest_cs);
    if(gil) {
        xbox_GuestEnter();xbox_GuestKernelCall(277,take);
        thread=CreateThread(NULL,0,contender,NULL,0,NULL);assert(thread);
        assert(WaitForSingleObject(contender_ready,1000)==WAIT_OBJECT_0);
        deadline=GetTickCount64()+2000;
        while(!g_guest_contended && GetTickCount64()<deadline)SwitchToThread();
        assert(g_guest_contended && !contender_ran);
        xbox_GuestKernelCall(294,drop);
        assert(contender_ran==!fast); /* FIFO handoff only in the old path. */
        xbox_GuestLeave();
        assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);
    }
    held=CreateThread(NULL,0,owner,NULL,0,NULL);assert(held);
    assert(WaitForSingleObject(owner_waiting,3000)==WAIT_OBJECT_0);
    waiting=CreateThread(NULL,0,waiter,NULL,0,NULL);assert(waiting);
    assert(WaitForSingleObject(waiter_called,3000)==WAIT_OBJECT_0);
    deadline=GetTickCount64()+1000;
    while(!(acquired=xbox_MemoryTryGpu()) && GetTickCount64()<deadline)SwitchToThread();
    if(acquired) {
        SetEvent(release_owner);
        assert(WaitForSingleObject(waiter_done,20)==WAIT_TIMEOUT);
        packet[0]=0x123;SwitchToThread();packet[1]=0x123;
        InterlockedExchange(&gpu_committed,1);xbox_MemoryEndGpu();
    } else SetEvent(release_owner);
    assert(WaitForSingleObject(held,3000)==WAIT_OBJECT_0);
    assert(WaitForSingleObject(waiting,3000)==WAIT_OBJECT_0);
    CloseHandle(held);CloseHandle(waiting);
    assert(acquired && "A blocked critical section retains CPU or RAM");
    xbox_GuestEnter();
    { KIRQL old=xbox_KeRaiseIrqlToDpcLevel();
      xbox_GuestKernelCall(277,take);xbox_GuestKernelCall(294,drop);
      assert(xbox_CurrentIrql()==DISPATCH_LEVEL && !xbox_MemoryTryGpu());
      xbox_KfLowerIrql(old); }
    xbox_GuestLeave();
    CloseHandle(owner_waiting);CloseHandle(release_owner);CloseHandle(waiter_called);
    CloseHandle(waiter_done);CloseHandle(contender_ready);
    printf("Critical sections: fastpath%d GIL%d; FIFO policy, real contention, recursion, RAM commit and raised IRQL passed\n",fast,gil);
    return 0;
}
