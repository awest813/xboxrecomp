/* The MMIO busy/idle handshake is independent of command consumption. */
#ifndef XBOXRECOMP_NV2A_ACK_H
#define XBOXRECOMP_NV2A_ACK_H
#include <stdint.h>
#define NV2A_ACK_VBLANK_HANDOFF 1

void nv2a_ack_tick(volatile void *registers);
/* Windows: spins while flags are raised, then backs off to 250 us
 * high-resolution waits after ~2,000 idle polls (RECOMP_ACK_BACKOFF=0 spins). */
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
/* RECOMP_FLIP_PACING=1: PGRAPH_INCREMENT (READ_3D) writes by the title's
 * vblank handler, counted since the last call. */
uint32_t nv2a_ack_take_flip_increments(void);
int nv2a_ack_flip_pacing(void);
int nv2a_ack_stopping(void);

#endif
