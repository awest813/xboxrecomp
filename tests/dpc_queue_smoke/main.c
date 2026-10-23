#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../src/kernel/kernel.h"
#include "../../src/kernel/kernel_dpc_queue.h"
static uint8_t arena[512 * 1024];
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)arena; }
#define VA(i) (32u + (i) * 32u)
#define COUNT 2000u
static DWORD WINAPI producer(LPVOID context)
{
    unsigned i, base = (unsigned)(uintptr_t)context;
    for (i = 0; i < COUNT; ++i)
        while (!xbox_DpcQueueInsert(VA(base + i), base, i)) Sleep(0);
    return 0;
}
int main(void)
{
    XboxPendingDpc d;
    unsigned i, count = 0;
    uint8_t seen[COUNT * 2] = {0};
    HANDLE producers[2];
    assert(!xbox_DpcQueueInsert(0, 0, 0));
    assert(!xbox_DpcQueueContains(0) && !xbox_DpcQueueContains(VA(0)));
    assert(xbox_DpcQueueInsert(VA(0), 7, 8));
    assert(xbox_DpcQueueContains(VA(0)));
    for (i = 0; i < 1000; ++i)
        assert(!xbox_DpcQueueInsert(VA(0), 1, 2));
    assert(arena[VA(0)+2] == 1);
    assert(xbox_DpcQueuePop(&d) && d.dpc == VA(0) && d.arg1 == 7 && d.arg2 == 8);
    assert(arena[VA(0)+2] == 0 && !xbox_DpcQueuePop(&d));
    assert(!xbox_DpcQueueContains(VA(0)));
    assert(xbox_DpcQueueInsert(VA(0), 9, 10));
    assert(xbox_DpcQueueInsert(VA(1), 11, 12));
    assert(xbox_DpcQueueInsert(VA(2), 13, 14));
    assert(xbox_DpcQueueRemove(VA(1)) && !xbox_DpcQueueRemove(VA(1)));
    assert(!xbox_DpcQueueContains(VA(1)) && xbox_DpcQueueContains(VA(2)));
    assert(xbox_DpcQueuePop(&d) && d.dpc == VA(0));
    assert(xbox_DpcQueuePop(&d) && d.dpc == VA(2));
    assert(!xbox_DpcQueuePop(&d));
    for (i = 0; i < 63; ++i) assert(xbox_DpcQueueInsert(VA(i), i, 0));
    assert(!xbox_DpcQueueInsert(VA(63), 0, 0));
    assert(xbox_DpcQueueRemove(VA(31)));
    assert(xbox_DpcQueueInsert(VA(63), 63, 0));
    for (i = 0; i < 64; ++i)
        if (i != 31) assert(xbox_DpcQueuePop(&d) && d.arg1 == i);
    assert(!xbox_DpcQueuePop(&d));
    producers[0] = CreateThread(NULL, 0, producer, (LPVOID)0, 0, NULL);
    producers[1] = CreateThread(NULL, 0, producer, (LPVOID)(uintptr_t)COUNT, 0, NULL);
    assert(producers[0] && producers[1]);
    while (count < COUNT * 2) {
        if (xbox_DpcQueuePop(&d)) {
            unsigned index = (d.dpc - 32) / 32;
            assert(index < COUNT * 2 && !seen[index]);
            assert(d.arg1 + d.arg2 == index);
            seen[index] = 1;
            ++count;
        } else Sleep(0);
    }
    assert(WaitForMultipleObjects(2, producers, TRUE, 5000) == WAIT_OBJECT_0);
    CloseHandle(producers[0]); CloseHandle(producers[1]);
    assert(!xbox_DpcQueuePop(&d));
    puts("PASS: DPC duplicate rejection, cancellation, wrap/capacity, 4000 concurrent insertions");
    return 0;
}
