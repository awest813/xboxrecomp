/* Check the actual presenter without opening a window or using game data. */
#undef NDEBUG
#include <assert.h>
#include <stddef.h>
#include "../../src/video/fb_present.c"

static uint8_t arena[256];
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)arena; }
void xbox_InputSetKeyState(unsigned key, int down) { (void)key; (void)down; }
void xbox_InputClearKeys(void) {}

int main(void)
{
    uint32_t pixels[4];
    uint32_t *source = (uint32_t *)(arena + 16);
    uint16_t *rgb565 = (uint16_t *)(arena + 64);
    _putenv_s("RECOMP_FB_VA", "");
    s_fb_width = s_fb_height = 2;
    s_fb_running = 1;
    source[0] = 0xFFFF0000; source[1] = 0xFF00FF00;
    source[2] = 0xFF0000FF; source[3] = 0xFFFFFFFF;
    xbox_FramebufferWindowPresent(16, 8);
    memset(source, 0, 16);  /* Guest reuses the render target after the flip. */
    assert(fb_copy_presented(pixels));
    assert(pixels[0] == 0xFFFF0000 && pixels[3] == 0xFFFFFFFF);
    rgb565[0] = 0xF800; rgb565[1] = 0x07E0;
    rgb565[2] = 0x001F; rgb565[3] = 0xFFFF;
    xbox_FramebufferWindowPresent(64, 4);
    assert(fb_copy_presented(pixels));
    assert(pixels[0] == 0xFF0000 && pixels[1] == 0x00FF00);
    assert(pixels[2] == 0x0000FF && pixels[3] == 0xFFFFFF);
    _putenv_s("RECOMP_FB_VA", "16");
    assert(!fb_copy_presented(pixels)); /* Debug pin follows live memory. */
    _putenv_s("RECOMP_FB_VA", "");
    free(s_present);
    puts("Completed-frame snapshot survives target reuse; RGB565 and debug pin passed");
    return 0;
}
