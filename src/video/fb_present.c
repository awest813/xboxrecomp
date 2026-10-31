/**
 * Show the guest framebuffer in a window.
 *
 * The title renders into its own framebuffer in guest RAM and tells the kernel
 * where it is through AvSetDisplayMode; on hardware the CRTC scans that memory
 * out. Nothing here scans anything out, so however much of the GPU is
 * implemented, none of it is observable. This is the other half: a window that
 * reads that memory and puts it on screen.
 *
 * Deliberately plain GDI rather than the D3D8 layer. The point is to display
 * whatever the guest actually wrote, so the fewer stages between guest memory
 * and the screen the better -- and it must keep working while the D3D8 layer
 * is busy with something else, such as the FMV player's own window.
 *
 * Off unless RECOMP_FB_WINDOW is set.
 */
#include <stdint.h>

#if defined(_WIN32)
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../input/xinput_xbox.h"
#include "platform/guest_memory.h"

extern ptrdiff_t xbox_GetMemoryOffset(void);

static volatile LONG s_fb_running;
static uint32_t      s_fb_va, s_fb_pitch, s_fb_width = 640, s_fb_height = 480;
static uint32_t     *s_rgb;           /* converted 32-bit copy for GDI */
/* Keep the completed frame separate from guest render targets. Following
 * clears showed offscreen UI surfaces and half-drawn back buffers. The
 * executor publishes at FLIP_STALL, as in the reference presentation path. */
static SRWLOCK      s_present_lock = SRWLOCK_INIT;
static uint32_t     *s_present;
static int          s_present_valid;

void xbox_FramebufferWindowSet(uint32_t fb_va, uint32_t pitch)
{
    /* RECOMP_FB_VA pins the window to one guest address instead of following
     * whichever surface is being drawn into. A black window cannot distinguish
     * "the read path is broken" from "the title rendered black", and pointing
     * it at memory known to have content settles that. */
    const char *pin = getenv("RECOMP_FB_VA");

    s_fb_va = pin ? (uint32_t)strtoul(pin, NULL, 0) : fb_va;
    if (pitch)
        s_fb_pitch = pitch;
}

static LRESULT CALLBACK fb_wndproc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    if (m == WM_KEYDOWN || m == WM_SYSKEYDOWN)
        xbox_InputSetKeyState((unsigned)w, 1);
    else if (m == WM_KEYUP || m == WM_SYSKEYUP)
        xbox_InputSetKeyState((unsigned)w, 0);
    else if (m == WM_KILLFOCUS)
        xbox_InputClearKeys();
    if (m == WM_CLOSE || m == WM_DESTROY) {
        xbox_InputClearKeys();
        InterlockedExchange(&s_fb_running, 0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* The display formats an Xbox front buffer is actually set to. The pitch says
 * how wide a row is in bytes, so pitch/width gives the pixel size; the exact
 * component layout only matters for 16-bit, where 5:6:5 and 1:5:5:5 differ. */
static void fb_convert(const uint8_t *src, uint32_t pitch, uint32_t *pixels)
{
    uint32_t x, y;
    uint32_t bpp = pitch / s_fb_width;

    for (y = 0; y < s_fb_height; y++) {
        const uint8_t *row = src + (size_t)y * pitch;
        uint32_t *dst = pixels + (size_t)y * s_fb_width;

        if (bpp == 4) {
            memcpy(dst, row, (size_t)s_fb_width * 4);
        } else if (bpp == 2) {
            const uint16_t *p = (const uint16_t *)row;
            for (x = 0; x < s_fb_width; x++) {
                uint16_t v = p[x];
                uint32_t r = (uint32_t)((v >> 11) & 0x1F) * 255u / 31u;
                uint32_t g = (uint32_t)((v >>  5) & 0x3F) * 255u / 63u;
                uint32_t b = (uint32_t)( v        & 0x1F) * 255u / 31u;
                dst[x] = (r << 16) | (g << 8) | b;
            }
        } else {
            memset(dst, 0, (size_t)s_fb_width * 4);
        }
    }
}

void xbox_FramebufferWindowPresent(uint32_t fb_va, uint32_t pitch)
{
    if (!s_fb_running || !fb_va || !pitch || getenv("RECOMP_FB_VA"))
        return;
    xbox_MemoryEnter();
    AcquireSRWLockExclusive(&s_present_lock);
    if (!s_present)
        s_present = (uint32_t *)calloc((size_t)s_fb_width * s_fb_height, 4);
    if (s_present) {
        const uint8_t *src =
            (const uint8_t *)((uintptr_t)fb_va + xbox_GetMemoryOffset());
        fb_convert(src, pitch, s_present);
        s_present_valid = 1;
    }
    ReleaseSRWLockExclusive(&s_present_lock);
    xbox_MemoryLeave();
}

/* Publish a completed frame the renderer produced itself (a GPU copy of the
 * surface), in the same format the guest surface would have. No guest RAM
 * is read, so no RAM reader is taken. */
void xbox_FramebufferWindowPresentPixels(const uint8_t *pixels, uint32_t pitch)
{
    if (!s_fb_running || !pixels || !pitch || getenv("RECOMP_FB_VA"))
        return;
    AcquireSRWLockExclusive(&s_present_lock);
    if (!s_present)
        s_present = (uint32_t *)calloc((size_t)s_fb_width * s_fb_height, 4);
    if (s_present) {
        fb_convert(pixels, pitch, s_present);
        s_present_valid = 1;
    }
    ReleaseSRWLockExclusive(&s_present_lock);
}

/* Whether a window is showing frames, and its size in pixels. */
int xbox_FramebufferWindowActive(uint32_t *width, uint32_t *height)
{
    if (width) *width = s_fb_width;
    if (height) *height = s_fb_height;
    return s_fb_running && !getenv("RECOMP_FB_VA");
}

/* Copy under the same lock as publication, then release it before GDI or
 * file I/O. Even several flips during a refresh cannot overwrite its pixels. */
static int fb_copy_presented(uint32_t *pixels)
{
    int valid;
    if (getenv("RECOMP_FB_VA"))
        return 0;
    AcquireSRWLockShared(&s_present_lock);
    valid = s_present_valid;
    if (valid)
        memcpy(pixels, s_present, (size_t)s_fb_width * s_fb_height * 4);
    ReleaseSRWLockShared(&s_present_lock);
    return valid;
}

/* Write what the window is currently showing to a 24-bit BMP.
 *
 * A black window is ambiguous: it means either that the read path is wrong or
 * that the title really did render black. Dumping the same converted pixels
 * the window draws settles which, and does it without a screenshot. */
int xbox_FramebufferDumpBmp(const char *path)
{
    FILE *f;
    uint32_t row = ((s_fb_width * 3u) + 3u) & ~3u;
    uint32_t img = row * s_fb_height, total = 54u + img, y, x;
    uint8_t hdr[54], *line;

    if (!s_rgb || !s_fb_va)
        return -1;
    f = fopen(path, "wb");
    if (!f)
        return -1;
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2, &total, 4);
    hdr[10] = 54; hdr[14] = 40;
    memcpy(hdr + 18, &s_fb_width, 4);
    memcpy(hdr + 22, &s_fb_height, 4);
    hdr[26] = 1; hdr[28] = 24;
    memcpy(hdr + 34, &img, 4);
    fwrite(hdr, 1, sizeof(hdr), f);

    line = (uint8_t *)calloc(1, row);
    for (y = 0; y < s_fb_height; y++) {
        const uint32_t *src = s_rgb + (size_t)(s_fb_height - 1 - y) * s_fb_width;
        for (x = 0; x < s_fb_width; x++) {
            line[x * 3 + 0] = (uint8_t)(src[x] & 0xFF);
            line[x * 3 + 1] = (uint8_t)((src[x] >> 8) & 0xFF);
            line[x * 3 + 2] = (uint8_t)((src[x] >> 16) & 0xFF);
        }
        fwrite(line, 1, row, f);
    }
    free(line);
    fclose(f);
    fprintf(stderr, "  [FBWIN] wrote %s (%ux%u from 0x%08X)\n",
            path, s_fb_width, s_fb_height, s_fb_va);
    return 0;
}

static DWORD WINAPI fb_thread(LPVOID unused)
{
    HWND hwnd;
    HDC hdc;
    BITMAPINFO bi;
    RECT r;
    const char *keyboard = getenv("RECOMP_KEYBOARD");
    const char *title = keyboard && *keyboard && *keyboard != '0'
        ? "Xbox Recomp - Framebuffer (Enter: Start, Z: A, arrows: D-pad)"
        : "Xbox Recomp - Framebuffer";

    (void)unused;

    {
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc   = fb_wndproc;
        wc.hInstance     = GetModuleHandleA(NULL);
        wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
        wc.lpszClassName = "XboxRecompFramebuffer";
        RegisterClassA(&wc);
    }
    r.left = 0; r.top = 0; r.right = (LONG)s_fb_width; r.bottom = (LONG)s_fb_height;
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    hwnd = CreateWindowExA(0, "XboxRecompFramebuffer", title,
                           WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           r.right - r.left, r.bottom - r.top,
                           NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!hwnd) {
        InterlockedExchange(&s_fb_running, 0);
        return 0;
    }
    hdc = GetDC(hwnd);

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize        = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth       = (LONG)s_fb_width;
    bi.bmiHeader.biHeight      = -(LONG)s_fb_height;   /* top-down */
    bi.bmiHeader.biPlanes      = 1;
    bi.bmiHeader.biBitCount    = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    s_rgb = (uint32_t *)calloc((size_t)s_fb_width * s_fb_height, 4);

    fprintf(stderr, "  [FBWIN] framebuffer window open (%ux%u)\n",
            s_fb_width, s_fb_height);

    while (InterlockedCompareExchange(&s_fb_running, 1, 1)) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        if (s_fb_va && s_fb_pitch && s_rgb) {
            const uint8_t *src =
                (const uint8_t *)((uintptr_t)s_fb_va + xbox_GetMemoryOffset());
            if (!fb_copy_presented(s_rgb)) {
                xbox_MemoryEnter();
                fb_convert(src, s_fb_pitch, s_rgb);
                xbox_MemoryLeave();
            }
            StretchDIBits(hdc, 0, 0, (int)s_fb_width, (int)s_fb_height,
                          0, 0, (int)s_fb_width, (int)s_fb_height,
                          s_rgb, &bi, DIB_RGB_COLORS, SRCCOPY);
        }
        {
            /* One dump by default. Slow guest startup can outlast that sample;
             * an optional interval captures the window throughout an intro. */
            const char *dump = getenv("RECOMP_FB_DUMP");
            const char *every = getenv("RECOMP_FB_WINDOW_DUMP_EVERY");
            static unsigned frames, dumps;
            int period = every ? atoi(every) : 0;
            frames++;
            if (dump && period > 0 && frames % (unsigned)period == 0) {
                char path[1024];
                snprintf(path, sizeof(path), "%s.window%03u.bmp", dump, dumps++);
                xbox_FramebufferDumpBmp(path);
            } else if (dump && period <= 0 && frames == 600)
                xbox_FramebufferDumpBmp(dump);
        }
        Sleep(16);
    }

    ReleaseDC(hwnd, hdc);
    DestroyWindow(hwnd);
    free(s_rgb);
    s_rgb = NULL;
    return 0;
}

void xbox_FramebufferWindowStart(void)
{
    HANDLE th;

    if (!getenv("RECOMP_FB_WINDOW"))
        return;
    if (InterlockedCompareExchange(&s_fb_running, 1, 0) != 0)
        return;
    th = CreateThread(NULL, 0, fb_thread, NULL, 0, NULL);
    if (th)
        CloseHandle(th);
    else
        InterlockedExchange(&s_fb_running, 0);
}

#else
void xbox_FramebufferWindowSet(uint32_t fb_va, uint32_t pitch) { (void)fb_va; (void)pitch; }
void xbox_FramebufferWindowPresent(uint32_t fb_va, uint32_t pitch) { (void)fb_va; (void)pitch; }
void xbox_FramebufferWindowPresentPixels(const uint8_t *pixels, uint32_t pitch) { (void)pixels; (void)pitch; }
int xbox_FramebufferWindowActive(uint32_t *width, uint32_t *height) { (void)width; (void)height; return 0; }
void xbox_FramebufferWindowStart(void) {}
#endif
