/* Guest-resident dispatcher state, following the reference port's kdisp path.
 * XDK macros initialize/clear these objects with ordinary guest stores. Never
 * copy their state into a persistent Windows event: a reused stack event would
 * retain an old signal and let a cancelled USB request outlive its stack frame.
 * A condition variable supplies wakeups; periodic checks observe inline stores.
 */
#include "kernel_dispatcher.h"
#include "platform/guest_memory.h"
#include "platform/host_clock.h"
#include <stdint.h>

static SRWLOCK s_lock = SRWLOCK_INIT;
static CONDITION_VARIABLE s_changed = CONDITION_VARIABLE_INIT;

typedef struct DispatcherWaiter {
    const XboxDispatcherWaitItem *items;
    ULONG count;
    int wait_all, completed;
    BOOLEAN alertable;
    NTSTATUS result;
    struct DispatcherWaiter *next;
} DispatcherWaiter;
static DispatcherWaiter *s_waiters;
static void complete_guest_waiters(void *header);

static volatile LONG *signal_state(void *header)
{ return (volatile LONG *)((uint8_t *)header + 4); }

int xbox_DispatcherIsGuestObject(const void *header)
{
    const uint8_t *p = header;
    return p && (((p[0] == 0 || p[0] == 1) && p[2] == 4)
                 || (p[0] == 5 && p[2] == 5));
}

static int acquire_object(void *header, int consume)
{
    uint8_t type = *(uint8_t *)header;
    LONG state = *signal_state(header);
    if (state <= 0) return 0;
    if (consume && type == 1) *signal_state(header) = 0;
    if (consume && type == 5) *signal_state(header) = state - 1;
    return 1;
}

LONG xbox_DispatcherSetEvent(void *header)
{
    LONG previous;
    AcquireSRWLockExclusive(&s_lock);
    previous = *signal_state(header);
    *signal_state(header) = 1;
    if(xbox_MemoryTraceWatched((uintptr_t)header))
        xbox_MemoryTrace(XBOX_TRACE_SET,(uintptr_t)header,previous,1,0);
    /* Satisfy registered waits while holding the dispatcher lock. An inline
     * XDK clear after SetEvent must not revoke a wait already released, even
     * if that thread cannot resume RAM access until a GPU commit completes. */
    complete_guest_waiters(header);
    WakeAllConditionVariable(&s_changed);
    ReleaseSRWLockExclusive(&s_lock);
    return previous;
}

LONG xbox_DispatcherResetEvent(void *header)
{
    LONG previous;
    AcquireSRWLockExclusive(&s_lock);
    previous = *signal_state(header);
    *signal_state(header) = 0;
    if(xbox_MemoryTraceWatched((uintptr_t)header))
        xbox_MemoryTrace(XBOX_TRACE_RESET,(uintptr_t)header,previous,0,0);
    ReleaseSRWLockExclusive(&s_lock);
    return previous;
}

NTSTATUS xbox_DispatcherReleaseSemaphore(void *header, LONG amount, LONG *previous)
{
    LONG state, limit;
    NTSTATUS status = STATUS_SUCCESS;
    AcquireSRWLockExclusive(&s_lock);
    state = *signal_state(header);
    limit = *(LONG *)((uint8_t *)header + 16);
    if (previous) *previous = state;
    if (amount <= 0 || (int64_t)state + amount > limit)
        status = (NTSTATUS)0xC0000047u; /* STATUS_SEMAPHORE_LIMIT_EXCEEDED */
    else {
        *signal_state(header) = state + amount;
        complete_guest_waiters(header);
        WakeAllConditionVariable(&s_changed);
    }
    ReleaseSRWLockExclusive(&s_lock);
    return status;
}

static ULONGLONG timeout_ms(const LARGE_INTEGER *timeout)
{
    int64_t ticks;
    LARGE_INTEGER now;
    if (!timeout) return UINT64_MAX;
    ticks = timeout->QuadPart;
    if (ticks > 0) {
        GetSystemTimeAsFileTime((FILETIME *)&now);
        ticks = ticks > now.QuadPart ? ticks - now.QuadPart : 0;
    } else if (ticks < 0) {
        /* Avoid signed overflow for the most negative interval. */
        return ((uint64_t)(-(ticks + 1)) + 1u + 9999u) / 10000u;
    }
    return ((uint64_t)ticks + 9999u) / 10000u;
}

/* Called only under s_lock. An incomplete WaitAll consumes nothing. */
static int try_wait(DispatcherWaiter *w)
{
    ULONG i;
    if (w->wait_all) {
        HANDLE hosts[MAXIMUM_WAIT_OBJECTS];
        ULONG host_indices[MAXIMUM_WAIT_OBJECTS], n_hosts = 0;
        DWORD status = WAIT_OBJECT_0;
        for (i = 0; i < w->count; i++) {
            if (w->items[i].guest) {
                if (!acquire_object(w->items[i].guest, 0)) return 0;
            } else {
                host_indices[n_hosts] = i;
                hosts[n_hosts++] = w->items[i].host;
            }
        }
        if (n_hosts) status = WaitForMultipleObjectsEx(n_hosts, hosts, TRUE, 0, w->alertable);
        if (status == WAIT_OBJECT_0 || (status >= WAIT_ABANDONED_0 && status < WAIT_ABANDONED_0+n_hosts)) {
            for (i = 0; i < w->count; i++)
                if (w->items[i].guest) acquire_object(w->items[i].guest, 1);
            w->result = status == WAIT_OBJECT_0 ? STATUS_SUCCESS
                      : (NTSTATUS)(0x80u + host_indices[status - WAIT_ABANDONED_0]);
            return 1;
        }
        if (status == WAIT_FAILED || status == WAIT_IO_COMPLETION) {
            w->result = status == WAIT_FAILED ? STATUS_INVALID_HANDLE : (NTSTATUS)0xC0;
            return 1;
        }
    } else {
        for (i = 0; i < w->count; i++) {
            DWORD status = WAIT_TIMEOUT;
            int ready;
            if (w->items[i].guest) ready = acquire_object(w->items[i].guest, 1);
            else {
                status = WaitForSingleObjectEx(w->items[i].host, 0, w->alertable);
                ready = status == WAIT_OBJECT_0 || status == WAIT_ABANDONED;
            }
            if (ready) {
                w->result = (NTSTATUS)((status == WAIT_ABANDONED ? 0x80u : 0u) + i);
                return 1;
            }
            if (status == WAIT_FAILED || status == WAIT_IO_COMPLETION) {
                w->result = status == WAIT_FAILED ? STATUS_INVALID_HANDLE : (NTSTATUS)0xC0;
                return 1;
            }
        }
    }
    return 0;
}

/* Called only under s_lock. Native mutants must be acquired on the waiting
 * thread; mixed native/guest waits retain the existing periodic path. */
static void complete_guest_waiters(void *header)
{
    DispatcherWaiter *w;
    for (w = s_waiters; w; w = w->next) {
        ULONG i;
        int has_host = 0;
        if (w->completed) continue;
        for (i = 0; i < w->count; i++)
            if (!w->items[i].guest) has_host = 1;
        if (has_host) continue;
        for (i = 0; i < w->count; i++)
            if (w->items[i].guest == header) break;
        if (i < w->count && try_wait(w)) {
            w->completed = 1;
            if(xbox_MemoryTraceWatched((uintptr_t)header))
                xbox_MemoryTrace(XBOX_TRACE_COMPLETE,(uintptr_t)header,w->result,1,(uintptr_t)w);
        }
        if (*signal_state(header) == 0) break;
    }
}
LONG xbox_DispatcherPulseEvent(void *header)
{
    LONG previous;
    AcquireSRWLockExclusive(&s_lock);
    previous = *signal_state(header);
    *signal_state(header) = 1;
    /* Only waits registered at the pulse can complete. No generation token
     * survives to satisfy a later wait or an incomplete WaitAll. */
    complete_guest_waiters(header);
    *signal_state(header) = 0;
    WakeAllConditionVariable(&s_changed);
    ReleaseSRWLockExclusive(&s_lock);
    return previous;
}

NTSTATUS xbox_DispatcherWait(ULONG count, const XboxDispatcherWaitItem *items,
                            int wait_all, BOOLEAN alertable, const LARGE_INTEGER *timeout)
{
    ULONGLONG start = xbox_HostMilliseconds(), duration = timeout_ms(timeout);
    ULONG i, j;
    DispatcherWaiter w = {items, count, wait_all, 0, alertable, STATUS_TIMEOUT, NULL};
    DispatcherWaiter **link;
    if (!count || count > MAXIMUM_WAIT_OBJECTS || !items)
        return STATUS_INVALID_PARAMETER;
    for (i = 0; i < count; i++) {
        if (!items[i].guest && !items[i].host) return STATUS_INVALID_HANDLE;
        for (j = 0; j < i; j++)
            if ((items[i].guest && items[i].guest == items[j].guest)
             || (!items[i].guest && !items[j].guest && items[i].host == items[j].host))
                return STATUS_INVALID_PARAMETER;
    }
    AcquireSRWLockExclusive(&s_lock);
    /* Append to preserve waiter order for synchronization-event pulses. */
    for (link = &s_waiters; *link; link = &(*link)->next) {}
    *link = &w;
    for(i=0;i<count;i++)if(xbox_MemoryTraceWatched((uintptr_t)items[i].guest))
        xbox_MemoryTrace(XBOX_TRACE_WAIT,(uintptr_t)items[i].guest,count,wait_all,(uintptr_t)&w);
    for (;;) {
        if (w.completed || try_wait(&w)) break;
        {
            ULONGLONG elapsed = xbox_HostMilliseconds() - start;
            DWORD slice = 4;
            if (elapsed >= duration) { w.result = STATUS_TIMEOUT; break; }
            if (duration - elapsed < slice) slice = (DWORD)(duration - elapsed);
            int memory=xbox_MemorySuspend();
            SleepConditionVariableSRW(&s_changed, &s_lock, slice, 0);
            if(memory) {
                /* Reacquire memory before the dispatcher mutex. A GPU fence
                 * must never wait for a waiter holding this mutex while that
                 * waiter waits for the GPU's memory transaction to end. */
                ReleaseSRWLockExclusive(&s_lock);
                xbox_MemoryResume(memory);
                AcquireSRWLockExclusive(&s_lock);
            }
        }
    }
    for (link = &s_waiters; *link != &w; link = &(*link)->next) {}
    *link = w.next;
    for(i=0;i<count;i++)if(xbox_MemoryTraceWatched((uintptr_t)items[i].guest))
        xbox_MemoryTrace(XBOX_TRACE_RETURN,(uintptr_t)items[i].guest,w.result,w.completed,(uintptr_t)&w);
    ReleaseSRWLockExclusive(&s_lock);
    return w.result;
}
