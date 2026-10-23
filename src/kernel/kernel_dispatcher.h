#ifndef XBOX_KERNEL_DISPATCHER_H
#define XBOX_KERNEL_DISPATCHER_H
#include "kernel.h"

/* For inline-initialized XDK events/semaphores the guest header is the state.
 * A cached host event cannot see KeClearEvent's direct SignalState store. */
typedef struct XboxDispatcherWaitItem {
    void *guest;
    HANDLE host;
} XboxDispatcherWaitItem;

int xbox_DispatcherIsGuestObject(const void *header);
LONG xbox_DispatcherSetEvent(void *header);
LONG xbox_DispatcherResetEvent(void *header);
LONG xbox_DispatcherPulseEvent(void *header);
NTSTATUS xbox_DispatcherReleaseSemaphore(void *header, LONG amount, LONG *previous);
NTSTATUS xbox_DispatcherWait(ULONG count, const XboxDispatcherWaitItem *items,
                            int wait_all, BOOLEAN alertable, const LARGE_INTEGER *timeout);
#endif
