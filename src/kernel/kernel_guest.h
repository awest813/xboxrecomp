#ifndef XBOX_KERNEL_GUEST_H
#define XBOX_KERNEL_GUEST_H
#include "kernel.h"

/* One lifted CPU at a time. Opt-in RAM scopes separately exclude native DMA
 * and passive bridge access while a GPU transaction has deferred readbacks. */
void xbox_GuestEnter(void);
void xbox_GuestLeave(void);
int xbox_GuestSuspend(void);
void xbox_GuestResume(int depth);
void xbox_GuestCall(void (*fn)(void));
void recomp_preempt(void);
extern volatile LONG g_guest_contended;
KIRQL xbox_CurrentIrql(void);
void xbox_GuestWaitResume(void);
#endif
