/* Synthetic DMA commands exercise the actual walker without game code. */
#undef NDEBUG
#include <assert.h>
#include "../../src/kernel/nv2a_pb_scan.c"
static uint32_t arena[4096];
static struct { uint32_t subch, method, value, get; } calls[32];
static unsigned call_count;
static uint32_t published_get;
ptrdiff_t xbox_GetMemoryOffset(void)
{ return (ptrdiff_t)((uintptr_t)arena - 0x80000000u); }
void xbox_Nv2aDmaGet(uint32_t get) { published_get = get; }
void nv2a_pb_exec_report(void) {}
void nv2a_render_memory_read(const void *p,size_t n) { (void)p;(void)n; }
void nv2a_pb_exec_method(uint32_t subch, uint32_t method, uint32_t value)
{
    assert(call_count < 32);
    calls[call_count].subch = subch;
    calls[call_count].method = method;
    calls[call_count].value = value;
    calls[call_count++].get = published_get;
}
static uint32_t header(unsigned count, unsigned subch, unsigned method)
{ return count << 18 | subch << 13 | method; }
static void reset(uint32_t get)
{
    memset(arena, 0, sizeof(arena));
    call_count = 0;
    s_exec_enabled = 1;
    nv2a_pb_resync(get);
}
static void packet_split(void)
{
    reset(0x100);
    arena[0x100/4] = header(3, 2, 0x200);
    arena[0x104/4] = 11; arena[0x108/4] = 22; arena[0x10C/4] = 33;
    assert(nv2a_pb_scan(0x108) == 0x108 && call_count == 1);
    assert(calls[0].method == 0x200 && calls[0].get == 0x104);
    assert(published_get == 0x108);
    assert(nv2a_pb_scan(0x110) == 0x110 && call_count == 3);
    assert(calls[1].method == 0x204 && calls[2].value == 33);
    assert(calls[2].subch == 2 && calls[2].get == 0x10C);
    assert(nv2a_pb_scan(0x110) == 0x110 && call_count == 3);
}
static void call_and_wrap(void)
{
    reset(0x300);
    arena[0x300/4] = 0x502; /* CALL */
    arena[0x500/4] = 0x40000000 | header(2, 1, 0x1800);
    arena[0x504/4] = 111; arena[0x508/4] = 222;
    arena[0x50C/4] = 0x20000; /* RETURN */
    arena[0x304/4] = 0x101; /* JUMP backwards to ring start */
    arena[0x100/4] = header(1, 0, 0x130);
    arena[0x104/4] = 333;
    assert(nv2a_pb_scan(0x108) == 0x108 && call_count == 3);
    assert(calls[0].value == 111 && calls[1].value == 222);
    assert(calls[0].method == 0x1800 && calls[1].method == 0x1800);
    assert(calls[2].method == 0x130 && calls[2].value == 333);
    reset(0x300);
    arena[0x300/4] = 0x20000100; /* old-style jump */
    arena[0x100/4] = header(1, 0, 0x200); arena[0x104/4] = 444;
    nv2a_pb_scan(0x108);
    assert(call_count == 1 && calls[0].value == 444);
}
static void reset_and_invalid(void)
{
    reset(0x100);
    arena[0x100/4] = header(4, 0, 0x200);
    nv2a_pb_scan(0x104); /* Pending parameters are discarded by a ring reset. */
    arena[0x200/4] = header(1, 0, 0x400); arena[0x204/4] = 555;
    nv2a_pb_resync(0x200); nv2a_pb_scan(0x208);
    assert(call_count == 1 && calls[0].method == 0x400);
    reset(0x100); arena[0x100/4] = 0x04000001; /* Beyond physical RAM */
    assert(nv2a_pb_scan(0x104) == 0x104 && !call_count);
    reset(0x100); arena[0x100/4] = 0x20000; /* Unmatched RETURN */
    assert(nv2a_pb_scan(0x104) == 0x104 && !call_count);
}
static void survey_only(void)
{
    reset(0x100);
#if defined(_WIN32)
    _putenv_s("RECOMP_PB_SCAN", "1");
#else
    setenv("RECOMP_PB_SCAN", "1", 1);
#endif
    s_exec_enabled = 0;
    published_get = 0;
    arena[0x100/4] = header(1, 0, 0x200); arena[0x104/4] = 666;
    assert(nv2a_pb_scan(0x108) == 0x108 && !call_count);
    assert(published_get == 0); /* The survey itself has no bus writes. */
}
static void bounded_and_trap(void)
{
    int software;
    reset(0x100);
    arena[0x100/4]=header(3,0,0xFC);
    arena[0x104/4]=7;arena[0x108/4]=5;arena[0x10C/4]=9;
    assert(nv2a_pb_scan_slice(0x110,1,0,1,&software)==0x104 && !software && !call_count);
    assert(nv2a_pb_scan_slice(0x110,4096,2,1,&software)==0x108 && software && call_count==1);
    assert(published_get==0x108 && calls[0].method==0xFC);
    assert(nv2a_pb_scan_slice(0x110,1,0,0,NULL)==0x10C && call_count==2);
    assert(calls[1].method==0x100 && calls[1].get==0x108);
    assert(nv2a_pb_scan_slice(0x110,1,0,1,&software)==0x110 && !software && call_count==3);
    reset(0x300);
    arena[0x300/4]=0x502;arena[0x500/4]=header(1,0,0x200);
    arena[0x504/4]=21;arena[0x508/4]=0x20000;
    arena[0x304/4]=header(1,0,0x204);arena[0x308/4]=22;
    while(published_get!=0x30C)nv2a_pb_scan_slice(0x30C,1,0,1,&software);
    assert(call_count==2 && calls[0].value==21 && calls[1].value==22);
    /* Bounded continuation must retain runaway/bad-stream protection too. */
    reset(0x100);arena[0x100/4]=0x101;
    while(published_get!=0x104)nv2a_pb_scan_slice(0x104,4096,0,1,&software);
    assert(!call_count && s_walk_words==0);
    reset(0x100);
    memset(arena+0x100/4,0xFF,65*4);
    while(published_get!=0x204)nv2a_pb_scan_slice(0x204,1,0,1,&software);
    assert(!call_count && !s_unknown_run);
}
static void continuous_producer(void)
{
    unsigned i;
    reset(0x100);
    arena[0x10C/4]=0x101;
    /* Keep two words submitted while consuming one. Every endpoint is
     * reachable, but GET never catches a submission before PUT advances.
     * A busy, valid ring must not exhaust a lifetime runaway allowance. */
    for(i=0;i<0x400004u;i++) {
        uint32_t next=s_get==0x10C ? 0x100 : s_get+4;
        uint32_t put=next==0x10C ? 0x100 : next+4;
        uint32_t actual=nv2a_pb_scan_slice(put,1,0,0,NULL);
        if(actual!=next || published_get!=next) {
            fprintf(stderr,"continuous producer skipped command after %u words: "
                "expected GET %X, actual %X, PUT %X\n",i,next,actual,put);
            exit(2);
        }
    }
    assert(s_get==0x100 && !call_count);
    arena[0x100/4]=header(1,0,0x1D70);arena[0x104/4]=0x6495;
    arena[0x108/4]=header(1,0,0x100);arena[0x10C/4]=5;
    assert(nv2a_pb_scan(0x110)==0x110 && call_count==2);
    assert(calls[0].method==0x1D70 && calls[0].value==0x6495);
    assert(calls[1].method==0x100 && calls[1].value==5);
    puts("Continuous producer exceeds four million words and retires fence/NOP without skipping");
}
static void advisory_request_expiry(void)
{
    reset(0x100);arena[0x100/4]=header(1,0,0x200);arena[0x104/4]=42;
    InterlockedExchange((volatile LONG*)&g_guest_gpu_contended,1);
    assert(nv2a_pb_scan_slice(0x108,1,0,0,NULL)==0x104);
    assert(g_guest_gpu_contended==1); /* partial work still wants exclusion */
    assert(nv2a_pb_scan_slice(0x8000010Bu,1,0,0,NULL)==0x108);
    assert(g_guest_gpu_contended==0 && call_count==1 && calls[0].value==42);
    puts("Completed submission expires its advisory GPU request; partial slices retain it");
}
int main(void)
{
    packet_split(); call_and_wrap(); reset_and_invalid(); bounded_and_trap();
    continuous_producer(); advisory_request_expiry(); survey_only();
    puts("DMA packet continuation, calls/returns, ring wraps, reset and GET ownership passed");
    return 0;
}
