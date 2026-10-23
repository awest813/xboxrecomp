/* Rational 60 Hz deadlines, independent of host sleep granularity. Keep the
 * phase across ordinary scheduling delay; after a missed period restart
 * without a burst of artificial vblanks. */
#ifndef XBOXRECOMP_NV2A_VBLANK_CLOCK_H
#define XBOXRECOMP_NV2A_VBLANK_CLOCK_H
#include <stdint.h>
typedef struct { uint64_t next_us; unsigned remainder; int started; } Nv2aVblankClock;
static int nv2a_vblank_due(Nv2aVblankClock *clock,uint64_t now_us)
{
    if(clock->started && now_us<clock->next_us)return 0;
    if(!clock->started || now_us-clock->next_us>=16667) {
        clock->started=1;clock->next_us=now_us;clock->remainder=0;
    }
    clock->next_us+=16666;
    clock->remainder+=40;
    if(clock->remainder>=60) { clock->next_us++;clock->remainder-=60; }
    return 1;
}
static unsigned nv2a_vblank_wait_ms(const Nv2aVblankClock *clock,uint64_t now_us)
{
    uint64_t ms;
    if(!clock->started || clock->next_us<=now_us)return 1;
    ms=(clock->next_us-now_us+999)/1000;
    return (unsigned)(ms>10 ? 10 : ms);
}
#endif
