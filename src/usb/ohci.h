/*
 * ohci.h -- the Xbox USB host controllers, enough of them to be found.
 *
 * A title reaches its gamepad through XAPI, which is statically linked into
 * the image and drives the OHCI controller registers directly rather than
 * going through anything this runtime can shim. Half-Life 2 registers a 4 KB
 * block at 0xFED00000 and takes an interrupt vector for it, then reads
 * HcRevision out of an aperture backed by zeroed RAM, concludes there is a
 * host controller with no root hub ports, and enumerates nothing.
 *
 * The model supplies operational registers, root-hub connection/reset,
 * control/bulk/periodic descriptor walks, HCCA completion queues and interrupt
 * delivery. Queue bus addresses resolve into contiguous DMA backing, while
 * static-image control reply buffers retain their virtual addresses. Completed
 * lists remain driver-owned until WDH acknowledgment, and repeated acknowledged
 * reports do not trip the stuck-interrupt guard.
 *
 * Written from the OHCI 1.0a specification and this title's own code.
 */
#ifndef XBOX_OHCI_H
#define XBOX_OHCI_H

#include <stdint.h>

/* The two MCPX host controllers, as the XDK addresses them. */
#define XBOX_OHCI0_BASE   0xFED00000u
#define XBOX_OHCI1_BASE   0xFED08000u
#define XBOX_OHCI_SIZE    0x00001000u   /* 4 KB, the length XAPI registers */

/* Bring the models up. Safe to call more than once; does nothing unless
 * RECOMP_USB is set, so a title that was working without a controller keeps
 * behaving exactly as it did. */
void xbox_OhciInit(void);

/* 1 if the address is inside a controller this model owns. */
int  xbox_OhciOwnsAddress(uint32_t xbox_va);

/* Service a trapped access. Returns 1 if the faulting instruction was decoded
 * and stepped over. `ctx` is a PCONTEXT; void * keeps windows.h out of here. */
int  xbox_OhciHandleMmio(void *ctx, uint32_t xbox_va);

/* Report counts at exit, so a run says whether the driver ever looked. */
void xbox_OhciReport(void);

#endif /* XBOX_OHCI_H */
