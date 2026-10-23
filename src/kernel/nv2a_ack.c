/* D3D's write-combine flush must not wait behind synchronous rendering.
 * Match nfsu2-sw's separate hardware-flag worker. These registers describe
 * the MMIO handshake; DMA_GET and completed render/fence state stay with
 * the command executor. A pending software-method interrupt is retained
 * until its guest handler consumes it. */
#include "nv2a_ack.h"
#include "platform/xbox_winnt.h"
#include <stdint.h>
#include <stdlib.h>

static HANDLE s_thread;
static volatile LONG s_stop;
#ifdef _WIN32
/* Experimental bounded idle polling. Zero preserves the established loop;
 * a high-resolution timer avoids turning a short delay into Sleep(1)'s tick.
 * The MMIO masks, publication locks and acknowledgment operation are identical. */
static volatile uint32_t s_poll_us;
static volatile LONG s_poll_timer_ready;
#endif
static SRWLOCK s_irq_lock=SRWLOCK_INIT;
static unsigned s_pgraph_holds;
static int s_vblank_hooked, s_vblank_pending;
static void update_bits(volatile LONG *word,uint32_t clear,uint32_t set)
{
    LONG old=*word,seen;
    do {
        LONG value=(LONG)(((uint32_t)old & ~clear)|set);
        seen=InterlockedCompareExchange(word,value,old);
        if(seen==old)return;
        old=seen;
    } while(1);
}
static const struct { uint32_t offset, clear, set; } flags[] = {
    { 0x100410, 0x00010000u, 0 }, /* PFB write-combine flush request */
    { 0x000100, 0xFFFFFFFFu, 0 }, /* PMC_INTR_0 */
    { 0x001100, 0xFFFFFFFFu, 0 }, /* PBUS_INTR_0 */
    { 0x002100, 0xFFFFFFFFu, 0 }, /* PFIFO_INTR_0 */
    { 0x400100, 0xFFFFFFFFu, 0 }, /* PGRAPH_INTR */
    { 0x600100, 0xFFFFFFFFu, 0 }, /* PCRTC_INTR_0 */
    { 0x002400, 0, 0x00000010u }, /* PFIFO_RUNOUT_STATUS LOW_MARK */
    { 0x003214, 0, 0x00000010u }, /* PFIFO_CACHE1_STATUS LOW_MARK */
};

void nv2a_ack_tick(volatile void *registers)
{
    size_t i;
    if (!registers) return;
    for (i=0; i<sizeof(flags)/sizeof(flags[0]); i++) {
        volatile LONG *r=(volatile LONG *)((volatile uint8_t *)registers+flags[i].offset);
        /* Aligned MMIO words use the same volatile loads as the title. Avoid
         * locked writes to unchanged status words on every idle poll. */
        LONG old=*r;
        uint32_t clear=flags[i].clear;
        int irq=(i==1 || i==4 || i==5) && old!=0;
        if(irq) {
            /* Hold publication waits for any in-flight clear, closing the
             * old read-not-held/clear-after-publication race. */
            AcquireSRWLockShared(&s_irq_lock);
            if(s_pgraph_holds && (i==1 || i==4))clear &= ~(i==1 ? 0x1000u : 1u);
            if(s_vblank_pending && (i==1 || i==5))clear &= ~(i==1 ? 0x1000000u : 1u);
        }
        for (;;) {
            LONG value=(LONG)(((uint32_t)old & ~clear) | flags[i].set);
            LONG observed;
            if (value==old) break;
            /* Full barrier before acknowledging the write-combine flush;
             * preserve unrelated bits if the title writes concurrently. */
            observed=InterlockedCompareExchange(r,value,old);
            if (observed==old) break;
            old=observed;
        }
        if(irq)ReleaseSRWLockShared(&s_irq_lock);
    }
}

void nv2a_ack_enable_vblank_hook(void)
{
    AcquireSRWLockExclusive(&s_irq_lock);
    s_vblank_hooked=1;
    ReleaseSRWLockExclusive(&s_irq_lock);
}

int nv2a_ack_vblank_publish(volatile void *registers)
{
    int posted=0;
    volatile uint8_t *nv=(volatile uint8_t *)registers;
    AcquireSRWLockExclusive(&s_irq_lock);
    if(!s_vblank_pending) {
        s_vblank_pending=s_vblank_hooked;
        update_bits((volatile LONG *)(nv+0x600100),0,1);
        update_bits((volatile LONG *)(nv+0x100),0,0x1000000);
        posted=1;
    }
    ReleaseSRWLockExclusive(&s_irq_lock);
    return posted;
}

void nv2a_ack_vblank_write(volatile void *registers,uint32_t value)
{
    volatile uint8_t *nv=(volatile uint8_t *)registers;
    AcquireSRWLockExclusive(&s_irq_lock);
    update_bits((volatile LONG *)(nv+0x600100),value,0);
    if(value&1) {
        s_vblank_pending=0;
        update_bits((volatile LONG *)(nv+0x100),0x1000000,0);
    }
    ReleaseSRWLockExclusive(&s_irq_lock);
}

void nv2a_ack_vblank_drop(volatile void *registers)
{
    nv2a_ack_vblank_write(registers,1);
}

void nv2a_ack_hold_pgraph(int hold)
{
    AcquireSRWLockExclusive(&s_irq_lock);
    if(hold)s_pgraph_holds++;
    else if(s_pgraph_holds)s_pgraph_holds--;
    ReleaseSRWLockExclusive(&s_irq_lock);
}

void nv2a_ack_request_stop(void) { InterlockedExchange(&s_stop,1); }
int nv2a_ack_stopping(void) { return InterlockedCompareExchange(&s_stop,0,0)!=0; }

static DWORD WINAPI flag_thread(LPVOID registers)
{
#ifdef _WIN32
    HANDLE timer=NULL;
    int attempted=0;
    LARGE_INTEGER frequency={0};
    uint64_t active_until=0;
#endif
    while (!InterlockedCompareExchange(&s_stop,0,0)) {
#ifdef _WIN32
        uint32_t us=s_poll_us;
        int requested=us && (*(volatile uint32_t *)((volatile uint8_t *)registers+0x100410)&0x10000);
#endif
        nv2a_ack_tick(registers);
#ifdef _WIN32
        {
            if(us && us<=1000) {
                LARGE_INTEGER due,now;
                if(!attempted) {
                    QueryPerformanceFrequency(&frequency);
                    timer=CreateWaitableTimerExW(NULL,NULL,CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                        TIMER_MODIFY_STATE|SYNCHRONIZE);
                    attempted=1;
                    InterlockedExchange(&s_poll_timer_ready,timer!=NULL);
                }
                QueryPerformanceCounter(&now);
                /* Preserve fast consecutive write-combine handshakes. The
                 * idle timer applies only after two milliseconds without a
                 * request, not between every acknowledgment in a burst. */
                if(requested)active_until=(uint64_t)now.QuadPart+(uint64_t)frequency.QuadPart/500;
                if((uint64_t)now.QuadPart<active_until) { Sleep(0);continue; }
                due.QuadPart=-(LONGLONG)us*10;
                if(timer && SetWaitableTimer(timer,&due,0,NULL,NULL,FALSE)
                    && WaitForSingleObject(timer,10)==WAIT_OBJECT_0)continue;
            }
        }
#endif
        Sleep(0);
    }
#ifdef _WIN32
    if(timer)CloseHandle(timer);
    InterlockedExchange(&s_poll_timer_ready,0);
#endif
    return 0;
}

int nv2a_ack_start(volatile void *registers)
{
    if (!registers || s_thread) return 0;
#ifdef _WIN32
    {
        const char *e=getenv("RECOMP_NV2A_ACK_POLL_US");char *end=NULL;
        unsigned long us=e && *e?strtoul(e,&end,10):100;
        s_poll_us=(end && *end) || us>1000?0:(uint32_t)us;
    }
#endif
    InterlockedExchange(&s_stop,0);
    s_thread=CreateThread(NULL,0,flag_thread,(LPVOID)registers,0,NULL);
    return s_thread!=NULL;
}

void nv2a_ack_stop(void)
{
    if (!s_thread) return;
    nv2a_ack_request_stop();
    WaitForSingleObject(s_thread,INFINITE);
    CloseHandle(s_thread);
    s_thread=NULL;
}
