/* Match the XDK/NFSU2 software-method handshake without signalling a title's
 * event directly. FIFO=1 alone is ambiguous: a handler which began before
 * publication can write it without seeing the trap, and the original DPC
 * can loop on W1C status before the GPU thread gets scheduled again. Projects
 * wrapping the original handler acknowledge under the publication lock. */
#include "nv2a_trap.h"
#include "nv2a_ack.h"
#include "platform/xbox_winnt.h"
#include <stdio.h>

#define REG(offset) (*(volatile LONG *)((volatile uint8_t *)registers+(offset)))
static SRWLOCK s_handler_lock=SRWLOCK_INIT;
static SRWLOCK s_delivery_lock=SRWLOCK_INIT;
static volatile LONG s_hooked, s_posted, s_completed;
static INIT_ONCE s_event_once=INIT_ONCE_STATIC_INIT;
static HANDLE s_taken_event;

static BOOL CALLBACK create_taken_event(PINIT_ONCE once,PVOID param,PVOID *context)
{
    (void)once;(void)param;(void)context;
    s_taken_event=CreateEvent(NULL,FALSE,FALSE,NULL);
    return s_taken_event!=NULL;
}

void nv2a_trap_enable_handler_hook(void) { InterlockedExchange(&s_hooked,1); }
void nv2a_trap_handler_lock(void) { AcquireSRWLockExclusive(&s_handler_lock); }
void nv2a_trap_handler_unlock(void) { ReleaseSRWLockExclusive(&s_handler_lock); }

static void update_bits(volatile void *registers,unsigned offset,uint32_t clear,uint32_t set)
{
    volatile LONG *r=&REG(offset);
    LONG old=*r, seen;
    do {
        LONG value=(LONG)(((uint32_t)old & ~clear)|set);
        seen=InterlockedCompareExchange(r,value,old);
        if(seen==old)break;
        old=seen;
    } while(1);
}

static void set_source(volatile void *registers,int asserted)
{
    update_bits(registers,0x100,0x1000,asserted ? 0x1000 : 0);
}

void nv2a_trap_handler_taken(volatile void *registers)
{
    /* The original handler has already read these status/data words. Clear
     * precisely this source before its caller can inspect PMC again. */
    InterlockedExchange(&REG(0x400108),0);
    update_bits(registers,0x400100,1,0);
    set_source(registers,0);
    InterlockedExchange(&s_completed,InterlockedCompareExchange(&s_posted,0,0));
    if(s_taken_event)SetEvent(s_taken_event);
}

int nv2a_trap_dispatch(volatile void *registers, uint32_t parameter,
                       Nv2aRequestInterrupt request, void *context)
{
    ULONGLONG report=GetTickCount64()+2000;
    static unsigned handled;
    int complete=0;
    LONG ticket;
    int hooked;
    if(!registers || !parameter || !request)return 0;
    AcquireSRWLockExclusive(&s_delivery_lock);
    if(nv2a_ack_stopping()) { ReleaseSRWLockExclusive(&s_delivery_lock);return 0; }
    hooked=InterlockedCompareExchange(&s_hooked,0,0)!=0;
    if(hooked)InitOnceExecuteOnce(&s_event_once,create_taken_event,NULL,NULL);
    nv2a_ack_hold_pgraph(1);
    nv2a_trap_handler_lock();
    ticket=InterlockedIncrement(&s_posted);
    REG(0x400704)=0x100;           /* trapped method, subchannel zero */
    REG(0x400708)=(LONG)parameter;
    REG(0x400108)=1;               /* NSOURCE: software method */
    REG(0x400720)=2;               /* sentinel; driver writes 0 on entry */
    REG(0x400100)=1;
    set_source(registers,1);
    nv2a_trap_handler_unlock();
    request(context);
    while((hooked ? InterlockedCompareExchange(&s_completed,0,0)!=ticket
                  : REG(0x400720)==2) && !nv2a_ack_stopping()) {
        ULONGLONG now=GetTickCount64();
        if(now>=report) {
            fprintf(stderr,"  [PB] NOP(%u) trap pending (PMC %08X EN %08X)\n",
                parameter,(unsigned)REG(0x100),(unsigned)REG(0x140));
            fflush(stderr);report=now+2000;
        }
        /* Keep the source pending until acknowledged, even when the ISR is masked.
         * DPC execution is on the timer thread, independent of this wait. */
        if(hooked && s_taken_event)WaitForSingleObject(s_taken_event,1);
        else Sleep(0);
    }
    /* Older projects can still use the FIFO sentinel. Hooked projects use
     * the original handler's explicit completion instead. */
    if(hooked)complete=InterlockedCompareExchange(&s_completed,0,0)==ticket;
    else {
        set_source(registers,0);
        while(REG(0x400720)!=1 && !nv2a_ack_stopping())Sleep(0);
        complete=REG(0x400720)==1;
    }
    nv2a_trap_handler_lock();
    set_source(registers,0);
    update_bits(registers,0x400100,1,0);
    REG(0x400108)=0;
    nv2a_trap_handler_unlock();
    nv2a_ack_hold_pgraph(0);
    if(complete && (++handled<=8 || (handled & 1023)==0)) {
        fprintf(stderr,"  [PB] NOP(%u) trap handled, %u total\n",parameter,handled);
        fflush(stderr);
    }
    ReleaseSRWLockExclusive(&s_delivery_lock);
    return complete;
}
