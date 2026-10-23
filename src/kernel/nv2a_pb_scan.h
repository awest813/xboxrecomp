#ifndef XBOX_NV2A_PB_SCAN_H
#define XBOX_NV2A_PB_SCAN_H
#include <stdint.h>
void nv2a_pb_resync(uint32_t get_phys);
uint32_t nv2a_pb_scan(uint32_t put_phys);
/* Preserve packet/CALL state across bounded slices. Stop before a software
 * trap parameter so the caller can commit and release RAM before its ISR. */
uint32_t nv2a_pb_scan_slice(uint32_t put_phys, uint32_t max_words,
                          uint32_t max_ms, int stop_software, int *software);
void nv2a_pb_scan_report(void);
#endif
