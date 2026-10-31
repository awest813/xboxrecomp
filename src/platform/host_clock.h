/* Host monotonic clock for the guest's time sources.
 *
 * GetTickCount/GetTickCount64 advance in ~15.6 ms steps on Windows, and
 * timeBeginPeriod does not change that. Every guest clock built on them moved
 * in those steps: KeTickCount, KeInterruptTime, timed dispatcher waits and the
 * kernel timer table. A 16 ms periodic timer re-armed from such a clock cannot
 * fire until two steps have passed, so a 60 Hz alarm ran at about 32 Hz. EA's
 * titles pace game timers from exactly such alarms (SSX 3 and NFSU2's ports hit
 * the same coarse tick). The performance counter has sub-microsecond resolution
 * and, like GetTickCount64, counts from boot, so values stay comparable. */
#ifndef XBOXRECOMP_HOST_CLOCK_H
#define XBOXRECOMP_HOST_CLOCK_H

#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

static inline uint64_t xbox_HostMicroseconds(void)
{
#ifdef _WIN32
    static volatile LONGLONG frequency;
    LARGE_INTEGER count;
    uint64_t n, f;
    if (!frequency) {
        LARGE_INTEGER q;
        QueryPerformanceFrequency(&q);
        frequency = q.QuadPart ? q.QuadPart : 1;
    }
    QueryPerformanceCounter(&count);
    n = (uint64_t)count.QuadPart; f = (uint64_t)frequency;
    return (n / f) * 1000000u + (n % f) * 1000000u / f;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u;
#endif
}

static inline uint64_t xbox_HostMilliseconds(void)
{
    return xbox_HostMicroseconds() / 1000u;
}

#endif
