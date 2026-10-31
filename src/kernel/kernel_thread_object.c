/*
 * Xbox XAPI GetExitCodeThread inspects KTHREAD.SignalState (+4) and
 * ETHREAD.ExitStatus (+0x120) after ObReferenceObjectByHandle. A native
 * HANDLE or NULL cannot stand in for that guest-addressable structure.
 * Refresh these fields from the host on each reference, including threads
 * which exited before their first reference. Keep one view per open handle;
 * outstanding references survive NtClose through a duplicated host handle.
 */
#include "kernel_thread_object.h"
#include "xbox_memory_layout.h"
#include <string.h>

#define THREAD_OBJECT_SIZE 0x140u
#define THREAD_OBJECT_MAX 256

static struct {
    HANDLE owner, host;
    uint32_t va, refs;
} s_objects[THREAD_OBJECT_MAX];
static SRWLOCK s_lock = SRWLOCK_INIT;

static void destroy_object(unsigned i)
{
    CloseHandle(s_objects[i].host);
    xbox_HeapFree(s_objects[i].va);
    memset(&s_objects[i], 0, sizeof(s_objects[i]));
}

NTSTATUS xbox_ThreadObjectReference(HANDLE handle, uint32_t *object_va)
{
    DWORD status;
    DWORD signaled;
    unsigned i, slot = THREAD_OBJECT_MAX;
    uint8_t *guest;
    if (!object_va) return (NTSTATUS)0xC000000Du;
    *object_va = 0;
    if (!handle || handle == INVALID_HANDLE_VALUE ||
        !GetExitCodeThread(handle, &status))
        return (NTSTATUS)0xC0000008u;
    signaled = WaitForSingleObject(handle, 0);
    if (signaled == WAIT_FAILED) return (NTSTATUS)0xC0000008u;

    AcquireSRWLockExclusive(&s_lock);
    for (i = 0; i < THREAD_OBJECT_MAX; ++i) {
        if (s_objects[i].host && s_objects[i].owner == handle) {
            slot = i;
            break;
        }
        if (!s_objects[i].host && slot == THREAD_OBJECT_MAX) slot = i;
    }
    if (slot == THREAD_OBJECT_MAX) {
        ReleaseSRWLockExclusive(&s_lock);
        return (NTSTATUS)0xC000009Au;
    }
    if (!s_objects[slot].host) {
        HANDLE dup = NULL;
        uint32_t va = xbox_HeapAlloc(THREAD_OBJECT_SIZE, 16);
        if (!va || !DuplicateHandle(GetCurrentProcess(), handle,
                GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            if (va) xbox_HeapFree(va);
            ReleaseSRWLockExclusive(&s_lock);
            return (NTSTATUS)0xC000009Au;
        }
        s_objects[slot].owner = handle;
        s_objects[slot].host = dup;
        s_objects[slot].va = va;
        memset((void *)(xbox_GetMemoryOffset() + va), 0, THREAD_OBJECT_SIZE);
        /* Sparse ThreadObject view; unmodeled KTHREAD fields remain zero. */
        guest = (uint8_t *)(xbox_GetMemoryOffset() + va);
        guest[0] = 6;
    }
    guest = (uint8_t *)(xbox_GetMemoryOffset() + s_objects[slot].va);
    *(volatile uint32_t *)(guest + 0x120) = status;
    *(volatile uint32_t *)(guest + 4) = signaled == WAIT_OBJECT_0;
    ++s_objects[slot].refs;
    *object_va = s_objects[slot].va;
    ReleaseSRWLockExclusive(&s_lock);
    return 0;
}

void xbox_ThreadObjectDereference(uint32_t va)
{
    unsigned i;
    AcquireSRWLockExclusive(&s_lock);
    for (i = 0; i < THREAD_OBJECT_MAX; ++i)
        if (va && s_objects[i].va == va) {
            if (s_objects[i].refs) --s_objects[i].refs;
            if (!s_objects[i].refs && !s_objects[i].owner) destroy_object(i);
            break;
        }
    ReleaseSRWLockExclusive(&s_lock);
}

void xbox_ThreadObjectAddReference(uint32_t va)
{
    unsigned i;
    AcquireSRWLockExclusive(&s_lock);
    for (i = 0; i < THREAD_OBJECT_MAX; ++i)
        if (va && s_objects[i].va == va) {
            ++s_objects[i].refs;
            break;
        }
    ReleaseSRWLockExclusive(&s_lock);
}

void xbox_ThreadObjectHandleClosed(HANDLE handle)
{
    unsigned i;
    AcquireSRWLockExclusive(&s_lock);
    for (i = 0; i < THREAD_OBJECT_MAX; ++i)
        if (s_objects[i].host && s_objects[i].owner == handle) {
            s_objects[i].owner = NULL;
            if (!s_objects[i].refs) destroy_object(i);
            break;
        }
    ReleaseSRWLockExclusive(&s_lock);
}

HANDLE xbox_ThreadObjectHost(uint32_t va)
{
    unsigned i;
    HANDLE h = NULL;
    AcquireSRWLockShared(&s_lock);
    for (i = 0; i < THREAD_OBJECT_MAX; ++i)
        if (va && s_objects[i].va == va) {
            h = s_objects[i].host;
            break;
        }
    ReleaseSRWLockShared(&s_lock);
    return h;
}
