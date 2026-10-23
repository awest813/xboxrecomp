/* Real OHCI control transfer across separate low-RAM and contiguous backing. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <windows.h>
static void test_tick(DWORD delay);
static ULONGLONG test_clock(void);
#define Sleep test_tick
#define GetTickCount64 test_clock
#include "../../src/usb/ohci.c"
#undef Sleep
#undef GetTickCount64

OHCI_TLS uint32_t g_eax, g_ecx, g_edx, g_esp, g_ebx, g_esi, g_edi, g_fs_base;
uint32_t g_xbox_image_lo = 0x10000, g_xbox_image_hi = 0x20000;
ptrdiff_t xbox_GetMemoryOffset(void) { return 0x10000; }
size_t xbox_GetMappedSize(void) { return 0x40000; }
uint32_t xbox_ContiguousAllocatedBytes(void) { return 0x40000; }
static int interrupt_test;
static unsigned irq_deliveries;
static unsigned masked_ticks;
static unsigned plugged_test_port;
static ULONGLONG clock_ms;
static ULONGLONG test_clock(void) { return clock_ms; }
void xbox_log(int level, const char *subsystem, const char *fmt, ...)
{ (void)level; (void)subsystem; (void)fmt; assert(!"Unexpected IRQL warning"); }
static void test_isr(void)
{
    KIRQL old = xbox_KfRaiseIrql(26);
    assert(old == 26);
    xbox_KfLowerIrql(old);
    if (interrupt_test == 1 || interrupt_test == 3)
        ohci_write(&s_hc[0], HcInterruptStatus, INTR_WDH, 4);
    if (interrupt_test == 4)
        ohci_write(&s_hc[0], HcInterruptStatus, INTR_RHSC, 4);
    irq_deliveries++;
    g_eax = 1;
}
static void test_tick(DWORD delay)
{
    clock_ms += delay;
    if (interrupt_test == 4) {
        masked_ticks++;
        if (masked_ticks == 1) {
            s_hc[0].reg[HcControl / 4] = 0x80;
        } else if (masked_ticks == 2) {
            for (unsigned port = 0; port < 4; port++) {
                uint32_t ps = ohci_read(&s_hc[0], HcRhPortStatus1 + port * 4, 4);
                assert(ps == (PORT_PPS | (port == plugged_test_port ? PORT_CCS | PORT_CSC : 0)));
            }
            assert(irq_deliveries == 1);
            s_hc[0].reg[HcControl / 4] = 0;
        }
        return;
    }
    if (interrupt_test == 3) {
        masked_ticks++;
        if (masked_ticks == 1) {
            s_hc[0].reg[HcControl / 4] = 0x90;
        } else if (masked_ticks == 2) {
            /* DMA and the shared frame clock progressed with MIE clear. */
            assert(s_hc[0].reg[HcFmNumber / 4] == OHCI_TICK_MS);
            assert(rd32(0x34080) == OHCI_TICK_MS);
            assert(rd32(0x34084) == 0x30060);
            assert((rd32(0x30050) >> 28) == TD_CC_NOERROR);
            assert(irq_deliveries == 0);
            s_hc[0].reg[HcInterruptEnable / 4] = INTR_MIE | INTR_WDH;
        } else {
            s_hc[0].reg[HcControl / 4] = 0;
        }
        return;
    }
    /* One new completion each frame, acknowledged by the fake guest ISR.
     * Stop operational service after 250 deliveries so the real thread loop
     * exits through its existing startup timeout without a wall-clock wait. */
    s_hc[0].reg[HcControl / 4] = irq_deliveries < 250 ? 0x80 : 0;
    s_hc[0].reg[HcInterruptStatus / 4] |= INTR_WDH;
}
recomp_func_t recomp_lookup(uint32_t va)
{ return interrupt_test && va == 0x12345 ? test_isr : NULL; }
int xbox_worker_stack_alloc(void) { return interrupt_test ? 0 : -1; }
void xbox_worker_stack_free(int slot) { (void)slot; }
uint32_t xbox_GetConnectedInterrupt(uint32_t vector)
{ assert(vector == 0x31); return interrupt_test ? 0x13000 : 0; }
uint32_t xbox_AllocThreadTib(void) { return interrupt_test ? 0x12000 : 0; }

static uint32_t *physical(uint32_t p)
{
    return (uint32_t *)(uintptr_t)(0x80010000u + p);
}
static void td(uint32_t p, uint32_t dp, uint32_t cbp, uint32_t be, uint32_t next)
{
    uint32_t *t = physical(p);
    t[0] = 0xF0000000u | (dp << 19);
    t[1] = cbp; t[2] = next; t[3] = be;
}
int main(void)
{
    void *low = VirtualAlloc((void *)0x10000, 0x40000,
                            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    void *high = VirtualAlloc((void *)(uintptr_t)0x80010000u, 0x40000,
                             MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    OhciController hc = {0};
    UsbSetup setup = {0x80, 6, 0x100, 0, 8};
    uint32_t done = 0;
    assert(low == (void *)0x10000 && high == (void *)(uintptr_t)0x80010000u);
    /* All four physical root ports exist; P1/P2 use ports 3/4. Exercise the
     * actual register requests, especially simultaneous old-change ACK/reset. */
    ohci_reset(&hc, XBOX_OHCI0_BASE, 0);
    assert((ohci_read(&hc, HcRhDescriptorA, 4) & 0xFF) == 4);
    for (unsigned port = 0; port < 4; port++) {
        uint32_t reg = HcRhPortStatus1 + port * 4;
        assert(ohci_read(&hc, reg, 4) == PORT_PPS);
        hc.reg[reg / 4] |= PORT_CCS | PORT_CSC | PORT_PRSC;
        ohci_write(&hc, reg, PORT_PRSC | PORT_W_PRS_SET_RESET, 4);
        assert(ohci_read(&hc, reg, 4) == (PORT_PPS | PORT_CCS | PORT_CSC | PORT_PES | PORT_PRSC));
        ohci_write(&hc, reg, PORT_CSC | PORT_PRSC | PORT_W_CCS_CLEAR_ENABLE, 4);
        assert(ohci_read(&hc, reg, 4) == (PORT_PPS | PORT_CCS));
        ohci_write(&hc, reg, PORT_W_CLEAR_POWER, 4);
        assert(ohci_read(&hc, reg, 4) == PORT_CCS);
        ohci_write(&hc, reg, PORT_W_PPS_SET_POWER | PORT_W_PES_SET_ENABLE, 4);
        assert(ohci_read(&hc, reg, 4) == (PORT_PPS | PORT_CCS | PORT_PES));
    }
    memset(&hc, 0, sizeof hc);
    /* The control reply is a static .data VA, the queue is physical DMA. */
    memset((void *)physical(0x11000), 0xAA, 8);
    memcpy(physical(0x30100), &setup, sizeof(setup));
    physical(0x30000)[0] = 8u << 16;
    physical(0x30000)[1] = 0x30070;
    physical(0x30000)[2] = 0x30040;
    td(0x30040, TD_DP_SETUP, 0x30100, 0x30107, 0x30050);
    td(0x30050, TD_DP_IN, 0x11000, 0x11007, 0x30060);
    td(0x30060, TD_DP_OUT, 0, 0, 0x30070);
    assert(ohci_walk_eds(&hc, 0x30000, &done) == 3);
    assert(physical(0x30000)[2] == 0x30070);
    assert(*(uint8_t *)0x21000 == 18 && *(uint8_t *)0x21001 == 1);
    assert(*(uint8_t *)physical(0x11000) == 0xAA);
    assert(*(uint32_t *)0x40000 == 0); /* distinct low-RAM copy stayed untouched */
    assert((physical(0x30050)[0] >> 28) == TD_CC_NOERROR);
    hc.reg[HcHCCA / 4] = 0x34000;
    ohci_publish_done(&hc, done);
    assert(physical(0x34000 + HCCA_DONE_HEAD)[0] == 0x30060);
    assert(hc.reg[HcInterruptStatus / 4] & INTR_WDH);
    assert(hc.reg[HcDoneHead / 4] == 0);
    /* A second batch must not replace a done list owned by the driver. */
    physical(0x30300)[2] = 0;
    ohci_publish_done(&hc, 0x30300);
    if (physical(0x34000 + HCCA_DONE_HEAD)[0] != 0x30060) {
        fputs("FAIL: an unacknowledged HCCA done list was overwritten\n", stderr);
        return 7;
    }
    assert(hc.reg[HcDoneHead / 4] == 0x30300);
    physical(0x30310)[2] = 0;
    ohci_publish_done(&hc, 0x30310);
    assert(physical(0x34000 + HCCA_DONE_HEAD)[0] == 0x30060);
    assert(hc.reg[HcDoneHead / 4] == 0x30310);
    assert(physical(0x30310)[2] == 0x30300);
    assert(physical(0x30300)[2] == 0);
    ohci_write(&hc, HcInterruptStatus, INTR_WDH, 4);
    assert(physical(0x34000 + HCCA_DONE_HEAD)[0] == 0);
    assert(!(hc.reg[HcInterruptStatus / 4] & INTR_WDH));
    assert(hc.reg[HcDoneHead / 4] == 0x30310);
    ohci_flush_done(&hc);
    assert(physical(0x34000 + HCCA_DONE_HEAD)[0] == 0x30310);
    assert(hc.reg[HcDoneHead / 4] == 0);
    assert(hc.reg[HcInterruptStatus / 4] & INTR_WDH);
    assert(physical(0x30310)[2] == 0x30300);
    ohci_write(&hc, HcInterruptStatus, INTR_WDH, 4);
    assert(physical(0x34000 + HCCA_DONE_HEAD)[0] == 0);
    assert(!guest_ptr(0xCCCCCCCC, 16) && !guest_ptr(0, 4));
    /* XAPI's class GET_REPORT/SET_REPORT must not stall endpoint zero after
     * enumeration. Exercise the actual TD walker, not just the descriptor. */
    setup = (UsbSetup){0xA1, 1, 0x100, 0, 20};
    memcpy(physical(0x30100), &setup, sizeof(setup));
    physical(0x30000)[2] = 0x30040;
    td(0x30040, TD_DP_SETUP, 0x30100, 0x30107, 0x30050);
    td(0x30050, TD_DP_IN, 0x11000, 0x11013, 0x30060);
    td(0x30060, TD_DP_OUT, 0, 0, 0x30070);
    done = 0;
    assert(ohci_walk_eds(&hc, 0x30000, &done) == 3);
    assert(*(uint8_t *)0x21000 == 0 && *(uint8_t *)0x21001 == 20);
    assert((physical(0x30050)[0] >> 28) == TD_CC_NOERROR);
    setup = (UsbSetup){0x21, 9, 0x200, 0, 6};
    memcpy(physical(0x30100), &setup, sizeof(setup));
    physical(0x30000)[2] = 0x30040;
    td(0x30040, TD_DP_SETUP, 0x30100, 0x30107, 0x30050);
    td(0x30050, TD_DP_OUT, 0x30200, 0x30205, 0x30060);
    td(0x30060, TD_DP_IN, 0, 0, 0x30070);
    done = 0;
    assert(ohci_walk_eds(&hc, 0x30000, &done) == 3);
    assert((physical(0x30060)[0] >> 28) == TD_CC_NOERROR);
    /* A healthy pad repeatedly raises the same WDH source. Exercise the real
     * controller loop past its stuck-source threshold with actual ACK writes. */
    {
        uintptr_t stack_base = (0x10000u + XBOX_WORKER_STACK_TOP(0)) & ~(uintptr_t)0xFFFF;
        void *stack = VirtualAlloc((void *)stack_base, 0x10000,
                                  MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        assert(stack == (void *)stack_base);
        *(uint32_t *)0x23000 = 0x12345; /* KINTERRUPT routine/context */
        *(uint32_t *)0x23004 = 0;
        *(uint8_t *)0x2300C = 26;
        ohci_reset(&s_hc[0], XBOX_OHCI0_BASE, 0);
        s_hc[0].reg[HcInterruptEnable / 4] = INTR_MIE | INTR_WDH;
        interrupt_test = 1;
        ohci_thread(NULL);
        if (irq_deliveries != 250) {
            fprintf(stderr, "FAIL: acknowledged WDH delivery stopped at %u/250\n", irq_deliveries);
            return 8;
        }
        ohci_reset(&s_hc[0], XBOX_OHCI0_BASE, 0);
        s_hc[0].reg[HcInterruptEnable / 4] = INTR_MIE | INTR_WDH;
        irq_deliveries = 0;
        interrupt_test = 2; /* The ISR now leaves the source unacknowledged. */
        ohci_thread(NULL);
        assert(irq_deliveries == 201);
        for (unsigned port = 0; port < 4; port++) {
            char choice[2] = {(char)('0' + port), 0};
            _putenv_s("RECOMP_USB_PORT", choice);
            ohci_reset(&s_hc[0], XBOX_OHCI0_BASE, 0);
            s_hc[0].reg[HcInterruptEnable / 4] = INTR_MIE | INTR_RHSC;
            interrupt_test = 4; masked_ticks = 0; irq_deliveries = 0;
            plugged_test_port = port;
            ohci_thread(NULL);
            assert(irq_deliveries == 1);
        }
        _putenv_s("RECOMP_USB_PORT", "");
        ohci_reset(&s_hc[0], XBOX_OHCI0_BASE, 0);
        s_hc[0].reg[HcHCCA / 4] = 0x34000;
        s_hc[0].reg[HcControlHeadED / 4] = 0x30000;
        setup = (UsbSetup){0x80, 6, 0x100, 0, 8};
        memcpy(physical(0x30100), &setup, sizeof setup);
        physical(0x30000)[2] = 0x30040;
        td(0x30040, TD_DP_SETUP, 0x30100, 0x30107, 0x30050);
        td(0x30050, TD_DP_IN, 0x11000, 0x11007, 0x30060);
        td(0x30060, TD_DP_OUT, 0, 0, 0x30070);
        interrupt_test = 3; masked_ticks = 0; irq_deliveries = 0;
        ohci_thread(NULL);
        assert(irq_deliveries == 1);
        s_hc[0].reg[HcFmNumber / 4] = 0xFFFE;
        ohci_advance_frame(&s_hc[0], 5);
        assert(rd32(0x34080) == 3);
        assert(s_hc[0].reg[HcInterruptStatus / 4] & INTR_FNO);
        for (unsigned i = 0; i < 100; i++)
            assert(ohci_read(&s_hc[0], HcFmNumber, 4) == 3);
        interrupt_test = 0;
        VirtualFree(stack, 0, MEM_RELEASE);
    }
    VirtualFree(high, 0, MEM_RELEASE); VirtualFree(low, 0, MEM_RELEASE);
    puts("OHCI four-port root hub, DMA, done ownership, IRQ continuity, stuck-source guard and XID reports passed");
    return 0;
}
