/* Verify APU DMA helpers against distinct low-RAM and contiguous pages. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include "../../src/apu/apu_shim.h"

uint8_t *g_apu_ram_ptr;
int xbox_ContiguousIsPhysical(uint32_t p) { return p < 0x30000; }
int main(void)
{
    uint8_t *low = VirtualAlloc((void *)0x10000, 0x40000,
                               MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t *high = VirtualAlloc((void *)(uintptr_t)0x80010000u, 0x40000,
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(low == (void *)0x10000 && high == (void *)(uintptr_t)0x80010000u);
    g_apu_ram_ptr = low;
    *(uint32_t *)(low + 0x20000) = 0xCCCCCCCC;
    *(uint32_t *)(high + 0x20000) = 0x12345678;
    assert(ldl_le_phys(NULL, 0x20000) == 0x12345678);
    assert(ldl_le_phys(NULL, 0x80020000) == 0x12345678);
    assert(lduw_le_phys(NULL, 0x20000) == 0x5678);
    assert(ldub_phys(NULL, 0x20000) == 0x78);
    stl_le_phys(NULL, 0x20000, 0xAABBCCDD);
    stw_le_phys(NULL, 0x20000, 0x9988);
    stb_phys(NULL, 0x20000, 0x77);
    assert(*(uint32_t *)(high + 0x20000) == 0xAABB9977);
    assert(*(uint32_t *)(low + 0x20000) == 0xCCCCCCCC);
    *(uint32_t *)(low + 0x31000) = 0x11223344;
    assert(ldl_le_phys(NULL, 0x31000) == 0x11223344);
    stl_le_phys(NULL, 0x31000, 0x55667788);
    assert(*(uint32_t *)(low + 0x31000) == 0x55667788);
    assert(apu_phys(0x20000) == high + 0x20000);
    VirtualFree(high, 0, MEM_RELEASE); VirtualFree(low, 0, MEM_RELEASE);
    puts("APU voice/sample DMA, byte/word/dword access and low-RAM fallback passed");
    return 0;
}
