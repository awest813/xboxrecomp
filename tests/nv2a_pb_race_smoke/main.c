/* The XDK checks GET, then patches a NOP(5) into queued commands >=8 KB
 * ahead. Reproduce a GPU scope forcing the CPU to stop between those stores.
 * The synchronous path deliberately waits once, modelling a slow readback:
 * the CPU can run concurrently there, while exclusive GPU work must exclude
 * it. This fixture exercises the real walker, CPU/RAM gates and dispatcher. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "kernel/kernel_guest.h"
#include "kernel/kernel_dispatcher.h"
#include "platform/guest_memory.h"
#include "../../src/kernel/nv2a_pb_scan.c"

static uint32_t arena[0x8000/4];
static uint8_t event_header[16];
static HANDLE go,attempted,patched;
static volatile LONG published_get,first,exclusive,traps;
static int protect;
static uint32_t word_limit;
static NTSTATUS result;
XBOX_THREAD_LOCAL uint32_t g_fs_base;
ptrdiff_t xbox_GetMemoryOffset(void)
{ return (ptrdiff_t)((uintptr_t)arena-0x80000000u); }
size_t xbox_GetMappedSize(void) { return 0; }
void xbox_Nv2aDmaGet(uint32_t get) { InterlockedExchange(&published_get,(LONG)get); }
void nv2a_render_memory_read(const void *p,size_t n) { (void)p;(void)n; }
void nv2a_pb_exec_report(void) {}
void xbox_log(int level,const char *subsystem,const char *fmt,...)
{ (void)level;(void)subsystem;(void)fmt;assert(!"Unexpected IRQL warning"); }
static void signal_event(void) { xbox_DispatcherSetEvent(event_header); }
void nv2a_pb_exec_method(uint32_t subch,uint32_t method,uint32_t parameter)
{
    assert(!subch && method==0x100);
    if(InterlockedExchange(&first,0) && !xbox_MemoryGpuOwned())
        assert(WaitForSingleObject(patched,3000)==WAIT_OBJECT_0);
    if(parameter==5) { InterlockedIncrement(&traps);xbox_GuestCall(signal_event); }
}
static DWORD WINAPI gpu_worker(LPVOID unused)
{
    int software=0,writer;
    (void)unused;
    assert(WaitForSingleObject(go,3000)==WAIT_OBJECT_0);
    writer=xbox_MemoryTryGpu();InterlockedExchange(&exclusive,writer);
    SetEvent(attempted);
    nv2a_pb_scan_slice(0x7000,word_limit,0,writer,&software);
    if(writer)xbox_MemoryEndGpu();
    if(software)nv2a_pb_scan_slice(0x7000,1,0,0,NULL);
    assert(WaitForSingleObject(patched,3000)==WAIT_OBJECT_0);
    nv2a_pb_scan(0x7000);
    return 0;
}
static void producer(void)
{
    int cpu;
    LARGE_INTEGER timeout;
    XboxDispatcherWaitItem item={event_header,NULL};
    /* Original BlockOnTime's >=0x2000 safety check. */
    uint32_t checked=(uint32_t)InterlockedCompareExchange(&published_get,0,0);
    assert(0x3014u-checked>=0x2000u);
    cpu=xbox_GuestSuspend();SetEvent(go);
    assert(WaitForSingleObject(attempted,3000)==WAIT_OBJECT_0);
    xbox_GuestResume(cpu); /* A GPU writer forces this to wait until GET passes the patch. */
    arena[0x3014/4]=5;SetEvent(patched);
    xbox_MemoryEnter();cpu=xbox_GuestSuspend(); /* Actual passive bridge pattern. */
    timeout.QuadPart=-5000000;
    result=xbox_DispatcherWait(1,&item,0,FALSE,&timeout);
    xbox_GuestResume(cpu);xbox_MemoryLeave();
}
static void run(int guarded)
{
    unsigned at;
    HANDLE thread;
    protect=guarded;memset(arena,0,sizeof arena);memset(event_header,0,sizeof event_header);
    event_header[2]=4;
    for(at=0x1000;at<0x7000;at+=8)arena[at/4]=(1u<<18)|0x100;
    ResetEvent(go);ResetEvent(attempted);ResetEvent(patched);
    published_get=0x1000;first=1;exclusive=-1;traps=0;
    s_exec_enabled=1;nv2a_pb_resync(0x1000);
    xbox_GuestEnter();
    thread=CreateThread(NULL,0,gpu_worker,NULL,0,NULL);assert(thread);
    if(protect)xbox_MemoryCall(producer);else producer();
    xbox_GuestLeave();
    assert(WaitForSingleObject(thread,3000)==WAIT_OBJECT_0);CloseHandle(thread);
    if(guarded)assert(!exclusive && traps==1 && result==STATUS_SUCCESS);
    else assert(exclusive && !traps && result==STATUS_TIMEOUT);
}
int main(void)
{
    assert(xbox_MemoryGateEnabled());
    go=CreateEvent(NULL,FALSE,FALSE,NULL);attempted=CreateEvent(NULL,FALSE,FALSE,NULL);
    patched=CreateEvent(NULL,TRUE,FALSE,NULL);assert(go && attempted && patched);
    word_limit=256;run(1);word_limit=512;run(1);
    for(word_limit=4096;word_limit<=16384;word_limit*=4) {
        run(0);run(1);
    }
    CloseHandle(go);CloseHandle(attempted);CloseHandle(patched);
    puts("256/512-word guarded continuation passed; reproduced missed queued NOP(5) at 4096/16384 words; command-publication lease preserves it through preemption and actual dispatcher wait");
    return 0;
}
