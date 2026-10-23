/* The MMIO busy/idle handshake is independent of command consumption. */
#ifndef XBOXRECOMP_NV2A_ACK_H
#define XBOXRECOMP_NV2A_ACK_H
#include <stdint.h>
#define NV2A_ACK_VBLANK_HANDOFF 1

void nv2a_ack_tick(volatile void *registers);
/* Windows: RECOMP_NV2A_ACK_POLL_US=1..1000 requests an idle
 * high-resolution timer (default 100); zero/invalid values retain Sleep(0). Requests
 * keep a two-millisecond fast polling burst. Timer failure falls back. */
int nv2a_ack_start(volatile void *registers);
void nv2a_ack_stop(void);
/* Retain the software-method source while its ISR/DPC reads plain RAM MMIO.
 * Other interrupt/flush acknowledgments continue. Holds may nest. */
void nv2a_ack_hold_pgraph(int hold);
/* Projects which service original PCRTC W1C stores can retain a pending
 * vblank until that store, including while its DPC waits for the guest CPU. */
void nv2a_ack_enable_vblank_hook(void);
int nv2a_ack_vblank_publish(volatile void *registers);
void nv2a_ack_vblank_write(volatile void *registers,uint32_t value);
void nv2a_ack_vblank_drop(volatile void *registers);
void nv2a_ack_request_stop(void);
int nv2a_ack_stopping(void);

#endif
