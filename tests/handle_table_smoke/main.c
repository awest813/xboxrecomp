/* Reproduce kernel bridges allocating and closing handles outside the GIL. */
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../src/kernel/kernel_handles.h"

#define THREADS 32
#define ROUNDS 1000
#define BATCH 4
static HANDLE gate;
static volatile LONG failures;

static DWORD WINAPI worker(LPVOID arg)
{
    uintptr_t id = (uintptr_t)arg;
    unsigned round, j;
    assert(WaitForSingleObject(gate, 5000) == WAIT_OBJECT_0);
    for (round = 0; round < ROUNDS; ++round) {
        HANDLE handles[BATCH];
        uint32_t tokens[BATCH];
        for (j = 0; j < BATCH; ++j) {
            handles[j] = (HANDLE)(uintptr_t)(0x100000u +
                (id * ROUNDS + round) * BATCH + j);
            tokens[j] = xbox_HandleToken(handles[j]);
        }
        SwitchToThread();
        for (j = 0; j < BATCH; ++j) {
            if (!tokens[j] || xbox_HandleResolve(tokens[j]) != handles[j])
                InterlockedIncrement(&failures);
            if (xbox_HandleTake(tokens[j]) != handles[j])
                InterlockedIncrement(&failures);
        }
    }
    return 0;
}

int main(void)
{
    HANDLE threads[THREADS], h = (HANDLE)(uintptr_t)0x123456789u;
    uint32_t token;
    unsigned i;
    assert(!xbox_HandleToken(NULL) && !xbox_HandleToken(INVALID_HANDLE_VALUE));
    token = xbox_HandleToken(h);
    assert(token && xbox_HandleToken(h) == token);
    assert(xbox_HandleResolve(token) == h && xbox_HandleTake(token) == h);
    assert(!xbox_HandleResolve(token) && !xbox_HandleTake(token));
    assert(!xbox_HandleResolve(0x48000000) && !xbox_HandleResolve(0x48ffffff));
    assert(xbox_HandleResolve(0xdead0001) == (HANDLE)(uintptr_t)0xdead0001);
    assert(!xbox_HandleTake(0xdead0001));
    gate = CreateEvent(NULL, TRUE, FALSE, NULL); assert(gate);
    for (i = 0; i < THREADS; ++i) {
        threads[i] = CreateThread(NULL, 0, worker, (LPVOID)(uintptr_t)i, 0, NULL);
        assert(threads[i]);
    }
    SetEvent(gate);
    for (i = 0; i < THREADS; ++i) {
        assert(WaitForSingleObject(threads[i], 30000) == WAIT_OBJECT_0);
        CloseHandle(threads[i]);
    }
    CloseHandle(gate);
    if (failures) {
        printf("FAIL: %ld stolen/closed handle observations in %u parallel allocations\n",
               failures, THREADS * ROUNDS * BATCH);
        return 1;
    }
    puts("PASS: token identity, invalid/synthetic handles, 128000 concurrent allocate/resolve/take operations");
    return 0;
}
