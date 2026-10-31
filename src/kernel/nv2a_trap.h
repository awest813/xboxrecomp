/* NV097 NO_OPERATION(nonzero) stalls PGRAPH until the guest driver takes it. */
#ifndef XBOXRECOMP_NV2A_TRAP_H
#define XBOXRECOMP_NV2A_TRAP_H
#include <stdint.h>
#define NV2A_TRAP_EXPLICIT_HANDOFF 1
typedef void (*Nv2aRequestInterrupt)(void *context);
int nv2a_trap_dispatch(volatile void *registers, uint32_t parameter,
                       Nv2aRequestInterrupt request, void *context);
/* Opt in before guest execution when the project wraps its original PGRAPH
 * handler. Publication and the complete handler call share this lock. Taken
 * must be called under the lock, after a call which found NSOURCE set. */
void nv2a_trap_enable_handler_hook(void);
void nv2a_trap_handler_lock(void);
void nv2a_trap_handler_taken(volatile void *registers);
void nv2a_trap_handler_unlock(void);
/* Cumulative traps and QueryPerformanceCounter ticks spent waiting for the
 * guest handler (diagnostics). */
void nv2a_trap_stats(uint64_t *count, uint64_t *wait_ticks);
#endif
