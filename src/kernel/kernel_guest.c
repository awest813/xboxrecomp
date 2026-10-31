/* The Xbox has one CPU. Following nfsu2-sw's GIL, serialize lifted threads
 * and callbacks, release around host kernel work, and yield at generated
 * safe points. A FIFO ticket prevents a polling thread taking the lock back
 * ahead of the worker/interrupt it is waiting for. Raised IRQL never preempts.
 */
#include "kernel_guest.h"
#include "platform/guest_memory.h"
#include <stdio.h>
#include <stdlib.h>

static SRWLOCK s_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE s_changed = CONDITION_VARIABLE_INIT;
static uint64_t s_next, s_serving;
static volatile LONG s_enabled = -1;
static XBOX_THREAD_LOCAL int s_depth;
volatile LONG g_guest_contended;

static int enabled(void)
{
    if (s_enabled < 0) {
        AcquireSRWLockExclusive(&s_lock);
        if (s_enabled < 0) {
            const char *e = getenv("RECOMP_GIL");
            InterlockedExchange(&s_enabled, !(e && *e == '0'));
            if (s_enabled) fprintf(stderr, "  [THREAD] lifted CPU serialized (RECOMP_GIL)\n");
        }
        ReleaseSRWLockExclusive(&s_lock);
    }
    return s_enabled;
}

static void lock_cpu(void)
{
    uint64_t ticket;
    AcquireSRWLockExclusive(&s_lock);
    ticket = s_next++;
    if (ticket != s_serving && xbox_CurrentIrql() >= DISPATCH_LEVEL)
        InterlockedExchange(&g_guest_contended, 1);
    while (ticket != s_serving) {
        if (!SleepConditionVariableSRW(&s_changed, &s_lock, 1, 0))
            InterlockedExchange(&g_guest_contended, 1);
    }
    InterlockedExchange(&g_guest_contended, s_next - s_serving > 1);
    ReleaseSRWLockExclusive(&s_lock);
}

static void unlock_cpu(void)
{
    AcquireSRWLockExclusive(&s_lock);
    s_serving++;
    WakeAllConditionVariable(&s_changed);
    ReleaseSRWLockExclusive(&s_lock);
}

void xbox_GuestEnter(void)
{
    if (s_depth++ == 0) {
        if(enabled())lock_cpu();
        xbox_MemoryEnter();
    }
}
void xbox_GuestLeave(void)
{
    if (s_depth > 0 && --s_depth == 0) {
        xbox_MemoryLeave();
        if(enabled())unlock_cpu();
    }
}
int xbox_GuestSuspend(void)
{
    int depth = s_depth;
    if (depth > 0) {
        xbox_MemoryLeave();s_depth=0;
        if(enabled())unlock_cpu();
    }
    return depth;
}
void xbox_GuestResume(int depth)
{
    if (depth > 0) {
        if(enabled())lock_cpu();
        xbox_MemoryEnter();s_depth=depth;
    }
}
void xbox_GuestCall(void (*fn)(void))
{
    xbox_GuestEnter(); fn(); xbox_GuestLeave();
}
void recomp_preempt(void)
{
    int depth,handoff;
    uint32_t ticket=0;
    if ((!g_guest_contended && !(g_guest_gpu_contended && xbox_MemoryGpuYieldAllowed()))
        || s_depth <= 0 || xbox_CurrentIrql() >= DISPATCH_LEVEL) return;
    handoff=xbox_MemoryPrepareGpuYield(&ticket);
    depth = xbox_GuestSuspend();
    if(handoff)xbox_MemoryWaitGpu(ticket);
    else SwitchToThread();
    xbox_GuestResume(depth);
}
