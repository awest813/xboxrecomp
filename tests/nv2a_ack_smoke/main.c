/* Rendering can block. A hardware flush acknowledgment must still progress,
 * without falsely publishing consumed DMA commands. No title files needed. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "platform/xbox_winnt.h"
#include "../../src/kernel/nv2a_ack.h"
#include "../../src/kernel/nv2a_trap.h"
#include "../../src/kernel/nv2a_vblank_clock.h"

#define APERTURE (16u*1024u*1024u)
#define FLUSH 0x100410
#define PUT 0x800040
#define GET 0x800044
static HANDLE entered, release_executor;
static HANDLE trap_published;
static int trap_result;

static volatile LONG *reg(volatile void *memory, unsigned offset)
{ return (volatile LONG *)((volatile uint8_t *)memory+offset); }
static DWORD WINAPI combined_worker(LPVOID memory)
{
    nv2a_ack_tick(memory);
    SetEvent(entered);
    assert(WaitForSingleObject(release_executor,15000)==WAIT_OBJECT_0);
    return 0;
}
static void masked_registers(void *memory)
{
    static const unsigned pending[]={0x100,0x1100,0x2100,0x400100,0x600100};
    unsigned i;
    *reg(memory,FLUSH)=(LONG)0x80010002u;
    *reg(memory,0x2400)=2; *reg(memory,0x3214)=4;
    *reg(memory,0x3220)=(LONG)0xBADF00Du;
    *reg(memory,PUT)=0x12340; *reg(memory,GET)=0x12000;
    for(i=0;i<sizeof(pending)/sizeof(pending[0]);i++) *reg(memory,pending[i])=-1;
    nv2a_ack_tick(memory);
    assert((uint32_t)*reg(memory,FLUSH)==0x80000002u);
    assert(*reg(memory,0x2400)==0x12 && *reg(memory,0x3214)==0x14);
    assert(*reg(memory,0x3220)==0xBADF00D);
    assert(*reg(memory,PUT)==0x12340 && *reg(memory,GET)==0x12000);
    for(i=0;i<sizeof(pending)/sizeof(pending[0]);i++) assert(!*reg(memory,pending[i]));

    nv2a_ack_hold_pgraph(1);nv2a_ack_hold_pgraph(1);
    *reg(memory,0x100)=0x1001000;*reg(memory,0x400100)=0x1001;
    *reg(memory,FLUSH)=0x10000;*reg(memory,0x600100)=1;
    nv2a_ack_tick(memory);
    assert(*reg(memory,0x100)==0x1000 && *reg(memory,0x400100)==1);
    assert(!*reg(memory,FLUSH) && !*reg(memory,0x600100));
    nv2a_ack_hold_pgraph(0);nv2a_ack_tick(memory);
    assert(*reg(memory,0x100)==0x1000 && *reg(memory,0x400100)==1);
    nv2a_ack_hold_pgraph(0);nv2a_ack_tick(memory);
    assert(!*reg(memory,0x100) && !*reg(memory,0x400100));
}

static void vblank_handoff(void *memory)
{
    unsigned i;
#ifdef NV2A_ACK_VBLANK_HANDOFF
    nv2a_ack_enable_vblank_hook();
    assert(nv2a_ack_vblank_publish(memory));
#else
    *reg(memory,0x100)=0x1000000;*reg(memory,0x600100)=1;
#endif
    for(i=0;i<128;i++)nv2a_ack_tick(memory);
    assert((*reg(memory,0x100)&0x1000000) && (*reg(memory,0x600100)&1));
#ifdef NV2A_ACK_VBLANK_HANDOFF
    assert(!nv2a_ack_vblank_publish(memory)); /* One pending hardware level. */
    nv2a_ack_hold_pgraph(1);
    InterlockedExchange(reg(memory,0x100),0x1001000);
    InterlockedExchange(reg(memory,0x600100),3);
    nv2a_ack_vblank_write(memory,2); /* An unrelated W1C bit cannot take vblank. */
    assert((*reg(memory,0x100)&0x1000000) && *reg(memory,0x600100)==1);
    nv2a_ack_vblank_write(memory,1);
    assert(*reg(memory,0x100)==0x1000 && !*reg(memory,0x600100));
    nv2a_ack_hold_pgraph(0);nv2a_ack_tick(memory);
    assert(!*reg(memory,0x100));
    assert(nv2a_ack_vblank_publish(memory));
    nv2a_ack_vblank_drop(memory); /* No callable ISR/queued DPC. */
    assert(!*reg(memory,0x100) && !*reg(memory,0x600100));
    for(i=0;i<10000;i++) {
        assert(nv2a_ack_vblank_publish(memory));
        nv2a_ack_tick(memory); /* DPC delayed behind the guest CPU. */
        assert((*reg(memory,0x100)&0x1000000) && (*reg(memory,0x600100)&1));
        nv2a_ack_vblank_write(memory,1);
        assert(!*reg(memory,0x100) && !*reg(memory,0x600100));
    }
#endif
    puts("Vblank: retained queued status, W1C masks, source isolation, coalescing and 10000 handoffs passed");
}

static void vblank_clock(void)
{
    Nv2aVblankClock clock={0};
    unsigned i;
    assert(nv2a_vblank_due(&clock,0));
    for(i=1;i<=60000;i++) {
        uint64_t deadline=(uint64_t)i*1000000/60;
        assert(clock.next_us==deadline);
        assert(!nv2a_vblank_due(&clock,deadline-1));
        assert(nv2a_vblank_due(&clock,deadline));
    }
    assert(nv2a_vblank_due(&clock,clock.next_us+100000));
    assert(nv2a_vblank_wait_ms(&clock,clock.next_us-100000)==10);
    assert(nv2a_vblank_wait_ms(&clock,clock.next_us-1001)==2);
    assert(nv2a_vblank_wait_ms(&clock,clock.next_us)==1);
    puts("60 Hz clock: 60000 phase-correct periods, late restart and bounded waits passed");
}

static void request_interrupt(void *unused)
{ (void)unused;SetEvent(trap_published); }
static DWORD WINAPI trap_worker(LPVOID memory)
{
    trap_result=nv2a_trap_dispatch(memory,5,request_interrupt,NULL);
    return 0;
}
static void wait_register(void *memory,unsigned offset,LONG value)
{
    ULONGLONG deadline=GetTickCount64()+1000;
    while(*reg(memory,offset)!=value && GetTickCount64()<deadline)Sleep(0);
    assert(*reg(memory,offset)==value);
}
static void explicit_trap_handoff(void *memory)
{
    HANDLE worker;
    unsigned i, calls=0;
#ifdef NV2A_TRAP_EXPLICIT_HANDOFF
    nv2a_trap_enable_handler_hook();
#endif
    for(i=0;i<64;i++) {
        unsigned turn;
#ifdef NV2A_ACK_VBLANK_HANDOFF
        assert(nv2a_ack_vblank_publish(memory));
#endif
        ResetEvent(trap_published);trap_result=-1;
        worker=CreateThread(NULL,0,trap_worker,memory,0,NULL);assert(worker);
        assert(WaitForSingleObject(trap_published,1000)==WAIT_OBJECT_0);
#ifdef NV2A_TRAP_EXPLICIT_HANDOFF
        nv2a_trap_handler_lock();
#endif
        /* FIFO=1 precedes the guest handler's return. It is also written by
         * calls which found no trap. Neither write proves this trap was taken. */
        *reg(memory,0x400720)=1;
        assert(WaitForSingleObject(worker,20)==WAIT_TIMEOUT);
        assert(*reg(memory,0x400108)==1);
        calls++;
#ifdef NV2A_TRAP_EXPLICIT_HANDOFF
        nv2a_trap_handler_taken(memory);
        nv2a_trap_handler_unlock();
#endif
        /* The original DPC immediately loops while PMC has a source. The
         * return path must clear W1C status before it can enter a second time. */
        for(turn=0;turn<100;turn++) {
            if(*reg(memory,0x100)&0x1000)calls++;
        }
        assert(calls==i+1);
        assert(WaitForSingleObject(worker,1000)==WAIT_OBJECT_0 && trap_result==1);
        assert(!*reg(memory,0x400108) && !*reg(memory,0x400100));
#ifdef NV2A_ACK_VBLANK_HANDOFF
        assert(*reg(memory,0x100)==0x1000000 && *reg(memory,0x600100)==1);
        nv2a_ack_vblank_write(memory,1);
#endif
        assert(*reg(memory,PUT)==0x12340 && *reg(memory,GET)==0x12000);
        CloseHandle(worker);
    }
#ifdef NV2A_TRAP_EXPLICIT_HANDOFF
    /* An empty handler already in progress cannot acknowledge a newly posted
     * trap on its way out. Publication waits for that same handler lock. */
    nv2a_trap_handler_lock();
    ResetEvent(trap_published);
    worker=CreateThread(NULL,0,trap_worker,memory,0,NULL);assert(worker);
    assert(WaitForSingleObject(trap_published,20)==WAIT_TIMEOUT);
    *reg(memory,0x400720)=1;
    nv2a_trap_handler_unlock();
    assert(WaitForSingleObject(trap_published,1000)==WAIT_OBJECT_0);
    assert(WaitForSingleObject(worker,20)==WAIT_TIMEOUT);
    nv2a_trap_handler_lock();
    nv2a_trap_handler_taken(memory);
    nv2a_trap_handler_unlock();
    assert(WaitForSingleObject(worker,1000)==WAIT_OBJECT_0 && trap_result==1);
    CloseHandle(worker);
#endif
    puts("64 explicit trap handoffs: FIFO race, duplicate DPC loop and overlapping publication passed");
}
static void software_traps(void *memory)
{
    HANDLE worker;
    unsigned i;
    trap_published=CreateEvent(NULL,TRUE,FALSE,NULL);assert(trap_published);
    for(i=0;i<32;i++) {
        ResetEvent(trap_published);trap_result=-1;
        worker=CreateThread(NULL,0,trap_worker,memory,0,NULL);assert(worker);
        assert(WaitForSingleObject(trap_published,1000)==WAIT_OBJECT_0);
        Sleep(10); /* Raised IRQL/no callable ISR: never consume the command. */
        assert(WaitForSingleObject(worker,0)==WAIT_TIMEOUT);
        assert(*reg(memory,0x400704)==0x100 && *reg(memory,0x400708)==5);
        assert(*reg(memory,0x400108)==1 && *reg(memory,0x400720)==2);
        assert(*reg(memory,0x400100)==1 && (*reg(memory,0x100)&0x1000));
        InterlockedExchange(reg(memory,FLUSH),0x10000);
        wait_register(memory,FLUSH,0); /* Flush progresses during the trap. */
        *reg(memory,0x400720)=0; /* Original DPC enters: source retires now. */
        wait_register(memory,0x100,0);
        Sleep(2);
        assert(*reg(memory,0x400100)==1 && *reg(memory,0x400108)==1);
        assert(WaitForSingleObject(worker,0)==WAIT_TIMEOUT);
        *reg(memory,0x400720)=1; /* Handler has read the trap and exited. */
        assert(WaitForSingleObject(worker,1000)==WAIT_OBJECT_0);
        assert(trap_result==1 && !*reg(memory,0x400100) && !*reg(memory,0x400108));
        assert(*reg(memory,PUT)==0x12340 && *reg(memory,GET)==0x12000);
        CloseHandle(worker);
    }
    explicit_trap_handoff(memory);
    ResetEvent(trap_published);
    worker=CreateThread(NULL,0,trap_worker,memory,0,NULL);assert(worker);
    assert(WaitForSingleObject(trap_published,1000)==WAIT_OBJECT_0);
    nv2a_ack_request_stop();
    assert(WaitForSingleObject(worker,1000)==WAIT_OBJECT_0);
    assert(trap_result==0 && *reg(memory,0x400720)==2); /* no fake completion */
    assert(!*reg(memory,0x100) && !*reg(memory,0x400100));
    CloseHandle(worker);CloseHandle(trap_published);
    puts("32 delayed software traps: protected source, DPC entry/exit, flush progress and cancellation passed");
}
int main(void)
{
    void *memory=VirtualAlloc(NULL,APERTURE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    HANDLE executor;
    unsigned i;
    ULONGLONG begin;
    assert(memory);
    masked_registers(memory);
    vblank_handoff(memory);
    vblank_clock();
    entered=CreateEvent(NULL,TRUE,FALSE,NULL);
    release_executor=CreateEvent(NULL,TRUE,FALSE,NULL);
    assert(entered && release_executor);
    executor=CreateThread(NULL,0,combined_worker,memory,0,NULL); assert(executor);
    assert(WaitForSingleObject(entered,1000)==WAIT_OBJECT_0);
    *reg(memory,FLUSH)=0x10000;
    Sleep(20);
    assert(*reg(memory,FLUSH)==0x10000);
    puts("Combined-worker negative control: blocked rendering leaves flush busy");
    assert(nv2a_ack_start(memory));
    assert(!nv2a_ack_start(memory)); /* no duplicate worker */
    vblank_handoff(memory); /* Repeat against the actual concurrent flag worker. */
    begin=GetTickCount64();
    for(i=0;i<128;i++) {
        ULONGLONG deadline=GetTickCount64()+1000;
        InterlockedExchange(reg(memory,FLUSH),0x10000);
        while(InterlockedCompareExchange(reg(memory,FLUSH),0,0) && GetTickCount64()<deadline)
            Sleep(0);
        assert(!*reg(memory,FLUSH));
        assert(*reg(memory,PUT)==0x12340 && *reg(memory,GET)==0x12000);
        assert(WaitForSingleObject(executor,0)==WAIT_TIMEOUT);
    }
    printf("Independent acknowledgments: 128 requests in %llu ms; DMA_GET retained\n",
           (unsigned long long)(GetTickCount64()-begin));
    software_traps(memory);
    nv2a_ack_stop(); nv2a_ack_stop();
    InterlockedExchange(reg(memory,FLUSH),0x10000);
    Sleep(20); assert(*reg(memory,FLUSH)==0x10000);
    SetEvent(release_executor);
    assert(WaitForSingleObject(executor,1000)==WAIT_OBJECT_0);
    CloseHandle(executor);CloseHandle(entered);CloseHandle(release_executor);
    assert(VirtualFree(memory,0,MEM_RELEASE));

    /* Restart only after the old aperture is gone; no stale polling pointer. */
    memory=VirtualAlloc(NULL,APERTURE,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(memory && nv2a_ack_start(memory));
    InterlockedExchange(reg(memory,FLUSH),0x10000);
    begin=GetTickCount64();
    while(*reg(memory,FLUSH) && GetTickCount64()-begin<1000) Sleep(0);
    assert(!*reg(memory,FLUSH));
    nv2a_ack_stop(); assert(VirtualFree(memory,0,MEM_RELEASE));
    puts("NV2A masked flags, blocked-render progress, DMA ownership and worker lifetime passed");
    return 0;
}
