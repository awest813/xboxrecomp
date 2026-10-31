#include "guest_memory.h"
#include "xbox_winnt.h"
#include <assert.h>
#include <stdlib.h>
#if defined(_MSC_VER)
#include <intrin.h>
#define MEMORY_CALLER ((uintptr_t)_ReturnAddress())
#else
#define MEMORY_CALLER ((uintptr_t)__builtin_return_address(0))
#endif

/* Optional debugger-readable ownership records, never consulted for locking.
 * On x64 each record is32 bytes. Read only equal even sequence snapshots;
 * a zero thread/caller is an unused or incompletely published record. */
typedef struct MemoryOwner {
    volatile LONG sequence,thread;
    uintptr_t caller,tag;
    uint64_t since_ms;
} MemoryOwner;
#if UINTPTR_MAX == UINT64_MAX
typedef char MemoryOwnerRecordSize[(sizeof(MemoryOwner)==32)?1:-1];
#endif
static MemoryOwner s_memory_owners[64];
static volatile LONG s_owner_trace=-1;
static XBOX_THREAD_LOCAL int s_owner_slot=-1;
static XBOX_THREAD_LOCAL uintptr_t s_owner_tag;
static int owner_trace(void)
{
    if(s_owner_trace<0) {
        const char *e=getenv("RECOMP_GPU_OWNER_TRACE");
        InterlockedCompareExchange(&s_owner_trace,e && *e=='1',-1);
    }
    return s_owner_trace!=0;
}
static void owner_enter(uintptr_t caller)
{
    unsigned i;LONG thread;
    if(!owner_trace())return;
    thread=(LONG)GetCurrentThreadId();
    for(i=0;i<64;i++)if(InterlockedCompareExchange(&s_memory_owners[i].thread,thread,0)==0) {
        MemoryOwner *o=&s_memory_owners[i];s_owner_slot=(int)i;
        InterlockedIncrement(&o->sequence);o->caller=caller;o->tag=s_owner_tag;
        o->since_ms=GetTickCount64();InterlockedIncrement(&o->sequence);return;
    }
}
static void owner_leave(void)
{
    if(s_owner_slot>=0) {
        MemoryOwner *o=&s_memory_owners[s_owner_slot];
        InterlockedIncrement(&o->sequence);o->caller=o->tag=0;o->since_ms=0;
        InterlockedIncrement(&o->sequence);InterlockedExchange(&o->thread,0);s_owner_slot=-1;
    }
}
static uintptr_t owner_tag(uintptr_t tag)
{
    uintptr_t old=s_owner_tag;
    /* Keep nested tag lifetime balanced even if a debugger changes tracing
     * while a publication callback is live. Tags never affect ownership. */
    s_owner_tag=tag;
    if(s_owner_slot>=0) {
        MemoryOwner *o=&s_memory_owners[s_owner_slot];
        InterlockedIncrement(&o->sequence);o->tag=tag;InterlockedIncrement(&o->sequence);
    }
    return old;
}

static SRWLOCK s_memory=SRWLOCK_INIT;
static volatile LONG s_active_readers;
static volatile LONG s_enabled=-1;
static XBOX_THREAD_LOCAL int s_read_depth,s_gpu_owned;
volatile int32_t g_guest_gpu_contended;
static volatile LONG s_gpu_yield_enabled=-1;
#ifdef _WIN32
static volatile LONG s_gpu_handoff_us=-1;
static volatile LONG s_gpu_accepted;
static volatile LONGLONG s_handoff_frequency,s_handoff_waits,s_handoff_success,s_handoff_skipped_readers;
#endif
static int gpu_yield_enabled(void)
{
    if(s_gpu_yield_enabled<0) {
        const char *e=getenv("RECOMP_GPU_YIELD");
        InterlockedCompareExchange(&s_gpu_yield_enabled,e && *e=='1',-1);
    }
    return s_gpu_yield_enabled!=0;
}

int xbox_MemoryPrepareGpuYield(uint32_t *ticket)
{
#ifdef _WIN32
    if(!g_guest_gpu_contended || !xbox_MemoryGpuYieldAllowed())return 0;
    if(s_gpu_handoff_us<0) {
        const char *e=getenv("RECOMP_GPU_HANDOFF_US");char *end=NULL;
        unsigned long us=e && *e?strtoul(e,&end,10):0;
        if((end && *end) || us>2000)us=0;
        InterlockedCompareExchange(&s_gpu_handoff_us,(LONG)us,-1);
    }
    if(!s_gpu_handoff_us)return 0;
    /* Yielding this CPU cannot grant exclusion while another bridge/DMA
     * reader is still live. Do not burn the bounded wait on that case. */
    if(InterlockedCompareExchange(&s_active_readers,0,0)!=1) {
        InterlockedIncrement64(&s_handoff_skipped_readers);return 0;
    }
    if(!s_handoff_frequency) {
        LARGE_INTEGER frequency;
        if(!QueryPerformanceFrequency(&frequency) || frequency.QuadPart<=0)return 0;
        InterlockedCompareExchange64(&s_handoff_frequency,frequency.QuadPart,0);
    }
    *ticket=(uint32_t)InterlockedCompareExchange(&s_gpu_accepted,0,0);
    return 1;
#else
    (void)ticket;return 0;
#endif
}
void xbox_MemoryWaitGpu(uint32_t ticket)
{
#ifdef _WIN32
    LARGE_INTEGER begin,now;
    uint64_t ticks;
    LONG us=s_gpu_handoff_us;
    assert(!s_read_depth && !s_gpu_owned);
    if(us<=0 || us>2000 || !s_handoff_frequency)return;
    ticks=(uint64_t)us*(uint64_t)s_handoff_frequency/1000000;
    QueryPerformanceCounter(&begin);
    InterlockedIncrement64(&s_handoff_waits);
    /* Yield runnable CPU time, but do not use a coarse timer which can turn
     * a microsecond experiment into a whole scheduler-tick guest stall.
     * A missing/blocked GPU always times out; no ownership is fabricated. */
    while((uint32_t)InterlockedCompareExchange(&s_gpu_accepted,0,0)==ticket) {
        if(!g_guest_gpu_contended)return; /* synchronous fallback finished */
        SwitchToThread();QueryPerformanceCounter(&now);
        if((uint64_t)(now.QuadPart-begin.QuadPart)>=ticks)return;
    }
    InterlockedIncrement64(&s_handoff_success);
#else
    (void)ticket;
#endif
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
static void acquire_reader(uintptr_t caller)
{
    uint64_t start=memory_clock();
    AcquireSRWLockShared(&s_memory);
    InterlockedIncrement(&s_active_readers);
    owner_enter(caller);
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
    if(s_read_depth++==0 && !s_gpu_owned)acquire_reader(MEMORY_CALLER);
}
void xbox_MemoryLeave(void)
{
    if(!xbox_MemoryGateEnabled())return;
    assert(s_read_depth>0);
    if(--s_read_depth==0 && !s_gpu_owned) {
        owner_leave();
        InterlockedDecrement(&s_active_readers);ReleaseSRWLockShared(&s_memory);
    }
}
void xbox_MemoryCall(void (*fn)(void))
{
    uintptr_t old;
    xbox_MemoryEnter();old=owner_tag((uintptr_t)fn);fn();owner_tag(old);xbox_MemoryLeave();
}
int xbox_MemorySuspend(void)
{
    int depth=s_read_depth;
    assert(!s_gpu_owned);
    if(depth) {
        owner_leave();
        s_read_depth=0;InterlockedDecrement(&s_active_readers);ReleaseSRWLockShared(&s_memory);
    }
    return depth;
}
void xbox_MemoryResume(int depth)
{
    assert(!s_gpu_owned && !s_read_depth);
    if(depth) { acquire_reader(MEMORY_CALLER);s_read_depth=depth; }
}
int xbox_MemoryTryGpu(void)
{
    if(!xbox_MemoryGateEnabled() || s_gpu_owned || s_read_depth)return 0;
    if(memory_clock())timing_sum(&s_timing_gpu_attempts,1);
    if(!TryAcquireSRWLockExclusive(&s_memory)) {
        InterlockedExchange((volatile LONG *)&g_guest_gpu_contended,gpu_yield_enabled());return 0;
    }
    s_gpu_owned=1;
#ifdef _WIN32
    if(s_gpu_handoff_us>0)InterlockedIncrement(&s_gpu_accepted);
#endif
    InterlockedExchange((volatile LONG *)&g_guest_gpu_contended,0);
    s_gpu_start=memory_clock();return 1;
}
void xbox_MemoryGpuIdle(void)
{
    InterlockedExchange((volatile LONG *)&g_guest_gpu_contended,0);
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
