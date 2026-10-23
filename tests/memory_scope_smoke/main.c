/* Exercise the CPU, passive bridge, native DMA and actual dispatcher leases. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "kernel/kernel_guest.h"
#include "kernel/kernel_dispatcher.h"
#include "platform/guest_memory.h"

static HANDLE ready, proceed, finished;
static volatile LONG packet[2], stop;
static int automatic_yield;
static uint8_t event_header[16]={0,0,4};
void xbox_log(int level,const char *subsystem,const char *fmt,...)
{ (void)level;(void)subsystem;(void)fmt;assert(!"Unexpected IRQL warning"); }

static int acquire_gpu(void)
{
    ULONGLONG start=GetTickCount64();
    while(!xbox_MemoryTryGpu()) {
        if(GetTickCount64()-start>3000)return 0;
        SwitchToThread();
    }
    return 1;
}
static DWORD WINAPI bridge_wait(LPVOID unused)
{
    int cpu,memory;
    (void)unused;
    xbox_GuestEnter();xbox_GuestEnter();
    xbox_MemoryEnter(); /* Same extra reference as kernel_thunk_dispatch. */
    cpu=xbox_GuestSuspend();assert(cpu==2);
    SetEvent(ready);
    assert(WaitForSingleObject(proceed,3000)==WAIT_OBJECT_0);
    memory=xbox_MemorySuspend();assert(memory==1);
    SetEvent(finished);
    assert(WaitForSingleObject(proceed,3000)==WAIT_OBJECT_0);
    xbox_MemoryResume(memory);
    assert(packet[0]==77 && packet[1]==77);
    xbox_GuestResume(cpu);
    xbox_MemoryLeave();xbox_GuestLeave();xbox_GuestLeave();
    SetEvent(ready);
    return 0;
}
static DWORD WINAPI dispatcher_wait(LPVOID unused)
{
    XboxDispatcherWaitItem item={event_header,NULL};
    LARGE_INTEGER timeout;
    (void)unused;timeout.QuadPart=-30000000;
    xbox_MemoryEnter();SetEvent(ready);
    assert(xbox_DispatcherWait(1,&item,0,FALSE,&timeout)==STATUS_SUCCESS);
    assert(packet[0]==88 && packet[1]==88);
    xbox_MemoryLeave();SetEvent(finished);
    return 0;
}
static DWORD WINAPI dma_reader(LPVOID unused)
{
    (void)unused;SetEvent(ready);
    while(!InterlockedCompareExchange(&stop,0,0)) {
        xbox_MemoryEnter();assert(packet[0]==packet[1]);xbox_MemoryLeave();
        SwitchToThread();
    }
    return 0;
}
static DWORD WINAPI polling_gpu(LPVOID unused)
{
    (void)unused;
    assert(!xbox_MemoryTryGpu()); /* CPU's raised-IRQL reader is on another thread. */
    SetEvent(finished);return 0;
}
static void callback_body(void) { assert(packet[0]==99 && packet[1]==99); }
static DWORD WINAPI callback(LPVOID unused)
{
    (void)unused;SetEvent(ready);xbox_GuestCall(callback_body);SetEvent(finished);return 0;
}
static DWORD WINAPI host_wait(LPVOID opaque)
{
    unsigned kind=(unsigned)(uintptr_t)opaque;
    LARGE_INTEGER timeout;
    HANDLE handles[1]={proceed};NTSTATUS status;
    timeout.QuadPart=-30000000;
    xbox_MemoryEnter();xbox_MemoryEnter();SetEvent(ready);
    switch(kind) {
    case 0:status=xbox_NtWaitForSingleObject(proceed,FALSE,&timeout);break;
    case 1:status=xbox_NtWaitForSingleObjectEx(proceed,KernelMode,FALSE,&timeout);break;
    case 2:status=xbox_NtWaitForMultipleObjectsEx(1,handles,1,FALSE,&timeout);break;
    case 3:status=xbox_KeWaitForSingleObject(proceed,0,KernelMode,FALSE,&timeout);break;
    default:status=xbox_KeWaitForMultipleObjects(1,(PVOID*)handles,1,0,KernelMode,FALSE,&timeout,NULL);break;
    }
    assert(status==STATUS_SUCCESS && packet[0]==(LONG)(100+kind) && packet[1]==packet[0]);
    xbox_MemoryLeave();xbox_MemoryLeave();SetEvent(finished);return 0;
}
static DWORD WINAPI yield_gpu(LPVOID requested)
{
    ULONGLONG start=GetTickCount64();
    LONG value=(LONG)(uintptr_t)requested;
    assert(!xbox_MemoryTryGpu());SetEvent(ready);
    while(!xbox_MemoryTryGpu()) {
        if(GetTickCount64()-start>2000){SetEvent(finished);return 1;}
        SwitchToThread();
    }
    packet[0]=value;SwitchToThread();packet[1]=value;
    xbox_MemoryEndGpu();SetEvent(finished);return 0;
}
static void test_gpu_safe_point(unsigned iteration)
{
    HANDLE thread;DWORD result;ULONGLONG start;unsigned i;KIRQL irql;int depth;
    LONG value=(LONG)(0x123000+iteration);
    ResetEvent(ready);ResetEvent(finished);
    xbox_GuestEnter();xbox_MemoryEnter(); /* Publication/bridge reference. */
    thread=CreateThread(NULL,0,yield_gpu,(LPVOID)(uintptr_t)value,0,NULL);assert(thread);
    assert(WaitForSingleObject(ready,1000)==WAIT_OBJECT_0);
    for(i=0;i<100;i++)recomp_preempt();
    assert(WaitForSingleObject(finished,0)==WAIT_TIMEOUT);
    xbox_MemoryLeave();
    irql=xbox_KeRaiseIrqlToDpcLevel();
    for(i=0;i<100;i++)recomp_preempt();
    assert(WaitForSingleObject(finished,0)==WAIT_TIMEOUT);
    xbox_KfLowerIrql(irql);
    if(automatic_yield) {
        start=GetTickCount64();
        while(WaitForSingleObject(finished,0)==WAIT_TIMEOUT && GetTickCount64()-start<3000) {
            recomp_preempt();assert(packet[0]==packet[1]);SwitchToThread();
        }
    } else {
        for(i=0;i<100;i++)recomp_preempt();
        assert(WaitForSingleObject(finished,0)==WAIT_TIMEOUT);
        depth=xbox_GuestSuspend();
        assert(WaitForSingleObject(finished,3000)==WAIT_OBJECT_0);
        xbox_GuestResume(depth);
    }
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);
    assert(GetExitCodeThread(thread,&result) && result==0);
    assert(packet[0]==value && packet[1]==value);
    xbox_GuestLeave();CloseHandle(thread);
}
int main(void)
{
    HANDLE thread;
    unsigned i;
    int depth;
    KIRQL irql;
    const char *request=getenv("RECOMP_GPU_YIELD");
    automatic_yield=request && *request=='1';
    assert(xbox_MemoryGateEnabled());
    ready=CreateEvent(NULL,FALSE,FALSE,NULL);
    proceed=CreateEvent(NULL,FALSE,FALSE,NULL);
    finished=CreateEvent(NULL,FALSE,FALSE,NULL);
    assert(ready && proceed && finished);
    thread=CreateThread(NULL,0,bridge_wait,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(ready,3000)==WAIT_OBJECT_0);
    for(i=0;i<1000;i++)assert(!xbox_MemoryTryGpu()); /* CPU suspended, bridge live. */
    SetEvent(proceed);
    assert(WaitForSingleObject(finished,3000)==WAIT_OBJECT_0);
    assert(acquire_gpu());SetEvent(proceed);
    assert(WaitForSingleObject(ready,20)==WAIT_TIMEOUT);
    packet[0]=77;packet[1]=77;
    xbox_MemoryEnter();xbox_MemoryLeave(); /* GPU's nested RAM helper. */
    xbox_MemoryEndGpu();
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);
    ResetEvent(ready);

    thread=CreateThread(NULL,0,dispatcher_wait,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(ready,3000)==WAIT_OBJECT_0);
    assert(acquire_gpu());
    xbox_DispatcherSetEvent(event_header);
    assert(WaitForSingleObject(finished,20)==WAIT_TIMEOUT);
    packet[0]=88;packet[1]=88;xbox_MemoryEndGpu();
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);

    ResetEvent(finished);
    xbox_GuestEnter();irql=xbox_KeRaiseIrqlToDpcLevel();
    for(i=0;i<1000;i++)assert(!xbox_MemoryTryGpu()); /* Raised-IRQL GPU poll never blocks GPU. */
    thread=CreateThread(NULL,0,polling_gpu,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(finished,1000)==WAIT_OBJECT_0);
    assert(WaitForSingleObject(thread,1000)==WAIT_OBJECT_0);CloseHandle(thread);
    xbox_KfLowerIrql(irql);xbox_GuestLeave();
    xbox_GuestEnter();depth=xbox_GuestSuspend();
    assert(acquire_gpu());xbox_MemoryEndGpu();xbox_GuestResume(depth);xbox_GuestLeave();

    assert(acquire_gpu());ResetEvent(ready);ResetEvent(finished);
    thread=CreateThread(NULL,0,callback,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(ready,1000)==WAIT_OBJECT_0);
    assert(WaitForSingleObject(finished,20)==WAIT_TIMEOUT);
    packet[0]=99;packet[1]=99;xbox_MemoryEndGpu();
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);

    for(i=0;i<5;i++) {
        ResetEvent(ready);ResetEvent(finished);
        thread=CreateThread(NULL,0,host_wait,(LPVOID)(uintptr_t)i,0,NULL);assert(thread);
        assert(WaitForSingleObject(ready,1000)==WAIT_OBJECT_0);assert(acquire_gpu());
        SetEvent(proceed);assert(WaitForSingleObject(finished,20)==WAIT_TIMEOUT);
        packet[0]=packet[1]=(LONG)(100+i);xbox_MemoryEndGpu();
        assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);
    }

    thread=CreateThread(NULL,0,dma_reader,NULL,0,NULL);assert(thread);
    assert(WaitForSingleObject(ready,3000)==WAIT_OBJECT_0);
    for(i=0;i<2000;i++) {
        assert(acquire_gpu());packet[0]=(LONG)i;SwitchToThread();packet[1]=(LONG)i;
        xbox_MemoryEndGpu();SwitchToThread();
    }
    InterlockedExchange(&stop,1);
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);
    for(i=0;i<32;i++)test_gpu_safe_point(i);
    puts(automatic_yield
        ? "32 GPU safe-point handoffs without a competing guest CPU; extra RAM references and raised IRQL remain protected"
        : "32 manual GPU handoffs with requests disabled; ordinary safe points retain the guest reader");
    CloseHandle(ready);CloseHandle(proceed);CloseHandle(finished);
    puts("RAM scopes: bridge retention, host/dispatcher waits, raised-IRQL fallback and 2000 coherent DMA commits passed");
    return 0;
}
