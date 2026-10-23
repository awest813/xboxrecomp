#include "guest_memory.h"
#include "xbox_winnt.h"
#include <assert.h>
#include <stdlib.h>

static SRWLOCK s_memory=SRWLOCK_INIT;
static volatile LONG s_enabled=-1;
static XBOX_THREAD_LOCAL int s_read_depth,s_gpu_owned;
volatile int32_t g_guest_gpu_contended;
static volatile LONG s_gpu_yield_enabled=-1;
static int gpu_yield_enabled(void)
{
    if(s_gpu_yield_enabled<0) {
        const char *e=getenv("RECOMP_GPU_YIELD");
        InterlockedCompareExchange(&s_gpu_yield_enabled,e && *e=='1',-1);
    }
    return s_gpu_yield_enabled!=0;
}

/* Optional aggregate timing; threads may overlap, so reader time is summed
 * across threads, not a percentage of elapsed run time. No history or I/O.
 * Keep all acquisition/release semantics identical when timing is enabled. */
static volatile LONG s_timing_enabled=-1;
static volatile LONGLONG s_timing_frequency,s_timing_read_ticks,s_timing_read_count;
static volatile LONGLONG s_timing_read_max,s_timing_gpu_ticks,s_timing_gpu_count;
static volatile LONGLONG s_timing_gpu_max,s_timing_gpu_attempts;
static XBOX_THREAD_LOCAL uint64_t s_gpu_start;
static uint64_t memory_clock(void)
{
    LARGE_INTEGER t;
    if(s_timing_enabled<0) {
        const char *e=getenv("RECOMP_GPU_TIMING");
        if(e && *e=='1') {
            QueryPerformanceFrequency(&t);
            InterlockedCompareExchange64(&s_timing_frequency,t.QuadPart,0);
        }
        InterlockedCompareExchange(&s_timing_enabled,e && *e=='1',-1);
    }
    if(!s_timing_enabled)return 0;
    QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;
}
static void timing_sum(volatile LONGLONG *counter,LONGLONG value)
{
    LONGLONG old=InterlockedCompareExchange64(counter,0,0),actual;
    do { actual=old;old=InterlockedCompareExchange64(counter,actual+value,actual); }
    while(old!=actual);
}
static void timing_max(volatile LONGLONG *counter,LONGLONG value)
{
    LONGLONG old=InterlockedCompareExchange64(counter,0,0),actual;
    while(value>old) {
        actual=old;old=InterlockedCompareExchange64(counter,value,actual);
        if(old==actual)break;
    }
}
static void acquire_reader(void)
{
    uint64_t start=memory_clock();
    AcquireSRWLockShared(&s_memory);
    if(start) {
        LONGLONG elapsed=(LONGLONG)(memory_clock()-start);
        timing_sum(&s_timing_read_ticks,elapsed);timing_sum(&s_timing_read_count,1);
        timing_max(&s_timing_read_max,elapsed);
    }
}
typedef struct MemoryTraceRecord {
    uint64_t sequence,ticks,a,b,c,d;
    uint32_t kind,thread,writer,padding;
} MemoryTraceRecord;
static SRWLOCK s_trace_lock=SRWLOCK_INIT;
static volatile LONG s_trace_enabled=-1;
static uint64_t s_trace_sequence;
static uintptr_t s_trace_watch;
static MemoryTraceRecord s_memory_trace[65536];
static int trace_enabled(void)
{
    if(s_trace_enabled<0) {
        const char *e=getenv("RECOMP_MEMORY_TRACE");
        InterlockedCompareExchange(&s_trace_enabled,e && *e=='1',-1);
    }
    return s_trace_enabled!=0;
}
void xbox_MemoryTrace(unsigned kind,uint64_t a,uint64_t b,uint64_t c,uint64_t d)
{
    MemoryTraceRecord *r;
    if(!trace_enabled())return;
    AcquireSRWLockExclusive(&s_trace_lock);
    r=&s_memory_trace[s_trace_sequence%65536];
    r->ticks=GetTickCount64();r->a=a;r->b=b;r->c=c;r->d=d;
    r->kind=kind;r->thread=GetCurrentThreadId();r->writer=s_gpu_owned;r->padding=0;
    r->sequence=++s_trace_sequence;
    ReleaseSRWLockExclusive(&s_trace_lock);
}
void xbox_MemoryTraceWatch(uintptr_t object)
{
    if(!trace_enabled())return;
    AcquireSRWLockExclusive(&s_trace_lock);s_trace_watch=object;
    ReleaseSRWLockExclusive(&s_trace_lock);
}
int xbox_MemoryTraceWatched(uintptr_t object)
{
    int match;
    if(!trace_enabled())return 0;
    AcquireSRWLockShared(&s_trace_lock);match=object && object==s_trace_watch;
    ReleaseSRWLockShared(&s_trace_lock);return match;
}

int xbox_MemoryGateEnabled(void)
{
    if(s_enabled<0) {
        const char *e=getenv("RECOMP_GPU_SCOPES");
        InterlockedCompareExchange(&s_enabled,e && *e=='1',-1);
    }
    return s_enabled!=0;
}
void xbox_MemoryEnter(void)
{
    if(!xbox_MemoryGateEnabled())return;
    if(s_read_depth++==0 && !s_gpu_owned)acquire_reader();
}
void xbox_MemoryLeave(void)
{
    if(!xbox_MemoryGateEnabled())return;
    assert(s_read_depth>0);
    if(--s_read_depth==0 && !s_gpu_owned)ReleaseSRWLockShared(&s_memory);
}
void xbox_MemoryCall(void (*fn)(void))
{
    xbox_MemoryEnter();fn();xbox_MemoryLeave();
}
int xbox_MemorySuspend(void)
{
    int depth=s_read_depth;
    assert(!s_gpu_owned);
    if(depth) { s_read_depth=0;ReleaseSRWLockShared(&s_memory); }
    return depth;
}
void xbox_MemoryResume(int depth)
{
    assert(!s_gpu_owned && !s_read_depth);
    if(depth) { acquire_reader();s_read_depth=depth; }
}
int xbox_MemoryTryGpu(void)
{
    if(!xbox_MemoryGateEnabled() || s_gpu_owned || s_read_depth)return 0;
    if(memory_clock())timing_sum(&s_timing_gpu_attempts,1);
    if(!TryAcquireSRWLockExclusive(&s_memory)) {
        InterlockedExchange((volatile LONG *)&g_guest_gpu_contended,gpu_yield_enabled());return 0;
    }
    InterlockedExchange((volatile LONG *)&g_guest_gpu_contended,0);
    s_gpu_owned=1;s_gpu_start=memory_clock();return 1;
}
void xbox_MemoryEndGpu(void)
{
    assert(s_gpu_owned && !s_read_depth);
    uint64_t elapsed=s_gpu_start?memory_clock()-s_gpu_start:0;
    s_gpu_owned=0;ReleaseSRWLockExclusive(&s_memory);
    if(s_gpu_start) {
        timing_sum(&s_timing_gpu_ticks,(LONGLONG)elapsed);timing_sum(&s_timing_gpu_count,1);
        timing_max(&s_timing_gpu_max,(LONGLONG)elapsed);s_gpu_start=0;
    }
}
int xbox_MemoryGpuOwned(void) { return s_gpu_owned; }
int xbox_MemoryGpuYieldAllowed(void) { return gpu_yield_enabled() && s_read_depth==1 && !s_gpu_owned; }
