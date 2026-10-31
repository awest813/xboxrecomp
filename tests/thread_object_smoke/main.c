/* Exercise XAPI's actual thread-exit polling fields against real host threads. */
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/kernel/kernel_thread_object.h"

static uint8_t arena[1024 * 1024];
static uint32_t cursor = 16, allocs, frees;
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)arena; }
uint32_t xbox_HeapAlloc(uint32_t bytes, uint32_t alignment)
{
    uint32_t va = (cursor + alignment - 1) & ~(alignment - 1);
    assert(va + bytes <= sizeof(arena));
    cursor = va + bytes;
    ++allocs;
    return va;
}
void xbox_HeapFree(uint32_t va) { assert(va); ++frees; }
static uint32_t field(uint32_t va, unsigned offset)
{
    return *(uint32_t *)(arena + va + offset);
}
static DWORD WINAPI worker(LPVOID gate)
{
    assert(WaitForSingleObject(gate, 5000) == WAIT_OBJECT_0);
    return 0x1234;
}
int main(void)
{
    HANDLE gate = CreateEvent(NULL, TRUE, FALSE, NULL);
    HANDLE thread = CreateThread(NULL, 0, worker, gate, 0, NULL);
    HANDLE host;
    uint32_t va = 0, again, invalid = 123;
    unsigned i;
    assert(gate && thread);
    assert(xbox_ThreadObjectReference(NULL, &invalid) < 0 && invalid == 0);
    assert(xbox_ThreadObjectReference(thread, NULL) < 0);
    assert(xbox_ThreadObjectReference(gate, &invalid) < 0 && invalid == 0);
    assert(xbox_ThreadObjectReference(thread, &va) == 0 && va);
    assert(field(va, 4) == 0 && field(va, 0x120) == STILL_ACTIVE);
    for (i = 0; i < 1000; ++i) {
        assert(xbox_ThreadObjectReference(thread, &again) == 0 && again == va);
        xbox_ThreadObjectDereference(again);
    }
    assert(allocs == 1);
    SetEvent(gate);
    assert(WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    assert(xbox_ThreadObjectReference(thread, &again) == 0 && again == va);
    assert(field(va, 4) == 1 && field(va, 0x120) == 0x1234);
    xbox_ThreadObjectDereference(again);
    xbox_ThreadObjectAddReference(va);
    xbox_ThreadObjectHandleClosed(thread);
    CloseHandle(thread);
    host = xbox_ThreadObjectHost(va);
    assert(host && WaitForSingleObject(host, 0) == WAIT_OBJECT_0);
    assert(frees == 0); /* The first object reference still owns the view. */
    xbox_ThreadObjectDereference(va);
    assert(frees == 0 && xbox_ThreadObjectHost(va));
    xbox_ThreadObjectDereference(va);
    assert(frees == 1 && !xbox_ThreadObjectHost(va));
    /* A thread which has already exited before its FIRST reference. */
    thread = CreateThread(NULL, 0, worker, gate, 0, NULL);
    assert(thread && WaitForSingleObject(thread, 5000) == WAIT_OBJECT_0);
    assert(xbox_ThreadObjectReference(thread, &va) == 0 && va);
    assert(field(va, 4) == 1 && field(va, 0x120) == 0x1234);
    xbox_ThreadObjectDereference(va);
    xbox_ThreadObjectHandleClosed(thread);
    CloseHandle(thread);
    CloseHandle(gate);
    assert(allocs == frees);
    puts("PASS: live/exited thread polling, stable views, invalid handles, close/ref lifetime");
    return 0;
}
