/* Guest RAM exclusion for bounded GPU transactions. This is separate from
 * the single guest CPU: passive kernel bridges and native DMA also use RAM. */
#ifndef XBOX_GUEST_MEMORY_H
#define XBOX_GUEST_MEMORY_H
#include <stddef.h>
#include <stdint.h>
/* Optional in-memory ordering history (RECOMP_MEMORY_TRACE=1). */
enum { XBOX_TRACE_CALL=1, XBOX_TRACE_CLEAR, XBOX_TRACE_PUBLISH,
       XBOX_TRACE_METHOD, XBOX_TRACE_METHOD_DONE, XBOX_TRACE_SET,
       XBOX_TRACE_RESET, XBOX_TRACE_WAIT, XBOX_TRACE_COMPLETE,
       XBOX_TRACE_RETURN, XBOX_TRACE_SCOPE, XBOX_TRACE_SCOPE_END, XBOX_TRACE_RESYNC,
       XBOX_TRACE_GET };
void xbox_MemoryTrace(unsigned kind,uint64_t a,uint64_t b,uint64_t c,uint64_t d);
void xbox_MemoryTraceWatch(uintptr_t object);
int xbox_MemoryTraceWatched(uintptr_t object);
int xbox_MemoryGateEnabled(void);
void xbox_MemoryEnter(void);
void xbox_MemoryLeave(void);
/* Retain a reader across lifted preemption during command publication. Actual
 * host waits can still suspend every nested reference, allowing GPU progress. */
void xbox_MemoryCall(void (*fn)(void));
int xbox_MemorySuspend(void);
void xbox_MemoryResume(int depth);
/* Never wait to upgrade a reader or to exclude a polling guest CPU. A failed
 * try means the GPU must retain its ordinary synchronous execution path. */
int xbox_MemoryTryGpu(void);
void xbox_MemoryEndGpu(void);
int xbox_MemoryGpuOwned(void);
/* Advisory safe-point request. A publication/bridge reader must remain held
 * across ordinary preemption; only the single lifted-CPU reader may yield. */
extern volatile int32_t g_guest_gpu_contended;
int xbox_MemoryGpuYieldAllowed(void);
/* Host-kernel access to guest RAM (ReadFile/WriteFile buffers) does not raise
 * user-mode faults on protected pages; it fails the system call instead.
 * Callers announce such ranges first so a page-protection owner (the GPU
 * residency tracker) can make them accessible. No-op when nothing registered. */
typedef void (*xbox_HostAccessFn)(void *p, size_t bytes, int write);
void xbox_MemorySetHostAccessHook(xbox_HostAccessFn fn);
void xbox_MemoryHostAccess(void *p, size_t bytes, int write);
#endif
