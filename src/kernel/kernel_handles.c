#include "kernel_handles.h"
#include <stdio.h>
#include <stdlib.h>

#define HANDLE_TAG 0x48000000u
#define HANDLE_MASK 0x00ffffffu
#define HANDLE_MAX 16384
static HANDLE s_handle_table[HANDLE_MAX];
/* Kernel bridges release the guest CPU lock before host work. A worker's
 * file/event creation can therefore overlap another thread's handle publish
 * or NtClose. Reserve, resolve and remove entries under this separate lock;
 * otherwise two handles receive the same token and either close steals both.
 */
static SRWLOCK s_handle_lock = SRWLOCK_INIT;
static int s_trace = -1;
static void trace_handle(const char *operation, uint32_t token, HANDLE h)
{
    /* Called with the table lock held; initialization and log order agree
     * with entry publication/removal. Resolve polls stay silent. */
    if (s_trace < 0) {
        const char *e = getenv("RECOMP_HANDLE_TRACE");
        s_trace = e && *e && *e != '0';
    }
    if (s_trace) fprintf(stderr, "  [HANDLE] %s tid=%lu token=%08X native=%p\n",
                         operation, (unsigned long)GetCurrentThreadId(), token, h);
}

uint32_t xbox_HandleToken(HANDLE h)
{
    unsigned i;
    uint32_t token = 0;
    if (!h || h == INVALID_HANDLE_VALUE) return 0;
    AcquireSRWLockExclusive(&s_handle_lock);
    for (i = 1; i < HANDLE_MAX; ++i)
        if (s_handle_table[i] == h) { token = HANDLE_TAG | i; goto done; }
    for (i = 1; i < HANDLE_MAX; ++i)
        if (!s_handle_table[i]) {
            s_handle_table[i] = h;
            token = HANDLE_TAG | i;
            trace_handle("publish", token, h);
            goto done;
        }
done:
    ReleaseSRWLockExclusive(&s_handle_lock);
    if (!token) fprintf(stderr, "  [BRIDGE] handle table full\n");
    return token;
}

HANDLE xbox_HandleResolve(uint32_t token)
{
    if ((token & 0xff000000u) == HANDLE_TAG) {
        unsigned i = token & HANDLE_MASK;
        HANDLE h = NULL;
        AcquireSRWLockShared(&s_handle_lock);
        if (i && i < HANDLE_MAX) h = s_handle_table[i];
        ReleaseSRWLockShared(&s_handle_lock);
        return h;
    }
    return (HANDLE)(uintptr_t)token;
}

HANDLE xbox_HandleTake(uint32_t token)
{
    if ((token & 0xff000000u) == HANDLE_TAG) {
        unsigned i = token & HANDLE_MASK;
        if (i && i < HANDLE_MAX) {
            HANDLE h;
            AcquireSRWLockExclusive(&s_handle_lock);
            h = s_handle_table[i];
            s_handle_table[i] = NULL;
            trace_handle("take", token, h);
            ReleaseSRWLockExclusive(&s_handle_lock);
            return h;
        }
    }
    return NULL;
}
