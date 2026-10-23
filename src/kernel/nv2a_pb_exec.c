/**
 * Execute the parts of the title's pushbuffer that produce visible pixels.
 *
 * The title builds NV2A commands in guest RAM and advances DMA_PUT; without
 * something consuming them the framebuffer stays whatever it was, which is how
 * a fully booted title renders a black screen. This walks the same command
 * stream nv2a_pb_scan.c surveys and carries out the subset that decides what is
 * on screen: which surface is being drawn into, and clearing it.
 *
 * Geometry uses a CPU vertex-program interpreter or the existing screen-space
 * UI path, followed by textured software rasterization. VP textures use
 * perspective interpolation, four 2D texture stages and register combiners.
 * Integer pitched Z16/Z24S8 surfaces support depth tests, writes and clears.
 * Stencil operations, floating/swizzled depth, cube/volume/bump sampling,
 * fog evaluation, near-plane clipping and
 * fixed-function model transforms still need a renderer/backend upgrade.
 *
 * Everything this does not handle is counted and ranked by
 * nv2a_pb_exec_report(), so what remains is a list rather than a guess.
 *
 * Enabled with RECOMP_PB_EXEC. RECOMP_RASTER_TEST draws one known triangle
 * after every clear, which separates "the pixel path is broken" from "the title
 * has not given us any vertices". RECOMP_FB_DUMP=<prefix> writes the surface to
 * <prefix>NNN.bmp, so the result can be looked at without a display.
 */
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "kernel.h"   /* XBOX_CONTIG_BASE / XBOX_CONTIG_SIZE */
#include "xbox_memory_layout.h"   /* xbox_Nv2aFrameCounterFlip */
#include "nv2a_vertex_program.h"
#include "nv2a_combiner.h"
#include "nv2a_render.h"
#include "nv2a_trap.h"
#include "nv2a_ack.h"
/* The swizzle decoder the D3D8 layer already uses -- one implementation of
 * Morton order, not a second one that can disagree with it. */
#include "../d3d/d3d8_swizzle.h"

extern ptrdiff_t xbox_GetMemoryOffset(void);
extern void xbox_FramebufferWindowSet(uint32_t fb_va, uint32_t pitch);
extern void xbox_FramebufferWindowPresent(uint32_t fb_va, uint32_t pitch);
extern void xbox_FramebufferWindowPresentPixels(const uint8_t *pixels, uint32_t pitch);
extern int xbox_FramebufferWindowActive(uint32_t *width, uint32_t *height);
extern void xbox_FramebufferWindowStart(void);
extern uint32_t g_xbox_image_lo, g_xbox_image_hi;

/* Startup diagnostics must not scan the CRT environment for every command
 * parameter. Inline vertices make this check substantially hotter than draws.
 * The DMA worker owns this executor, so its first use initializes the cache. */
static int exec_verbose(void)
{
    static int enabled = -1;
    if (enabled < 0)
        enabled = getenv("RECOMP_PB_EXEC_VERBOSE") != NULL;
    return enabled;
}

/* Would writing this surface land on the title's own image?
 *
 * NV097_SET_SURFACE_COLOR_OFFSET is an offset within the colour DMA object,
 * not a guest virtual address, and this executor has always used it as one.
 * That is harmless while the two happen to agree and catastrophic when they do
 * not: the Xbox Dashboard names surface 0x00088000 at 1280x960x4, so clearing
 * it wrote 4.9 MB of opaque black from 0x00088000 to 0x00538000 -- straight
 * over its own code, its D3D context at 0x000BBFC0 and the register-block
 * pointer at 0x000BE2C4. The symptom was a title that submitted one perfect
 * frame and then spun forever in a pushbuffer-full loop, three layers away,
 * with every D3D global reading 0xFF000000: the clear colour.
 *
 * So refuse, and say so. Getting the address right needs the DMA object base
 * this ignores (NV097_SET_CONTEXT_DMA_COLOR); until that exists, writing
 * nothing is strictly better than writing over the guest, and a title that
 * cannot draw is easier to debug than one that has been overwritten.
 */
static int surface_hits_image(uint32_t base, uint32_t bytes)
{
    if (!g_xbox_image_hi || !bytes)
        return 0;
    return base < g_xbox_image_hi && base + bytes > g_xbox_image_lo;
}

/* Where a DMA-object offset actually lives.
 *
 * NV097_SET_SURFACE_COLOR_OFFSET is an offset inside the colour DMA object,
 * and for a framebuffer that object covers physical memory -- so the offset
 * is a physical address, not a guest VA. Those are the same number in this
 * runtime, which is why treating it as a VA works until it does not: on
 * hardware the image is mapped at VA 0x00010000 from arbitrary physical
 * pages, so a framebuffer at physical 0x84000 does not overlap it. Here it
 * would.
 *
 * The title tells us which it is by where it allocated. Half-Life 2's
 * framebuffer comes from MmAllocateContiguousMemory, which this runtime
 * serves from the window at XBOX_CONTIG_BASE, so physical P is visible at
 * XBOX_CONTIG_BASE + P -- clear of the image, and the same bytes the title's
 * own writes and the framebuffer window reach.
 *
 * So: use the offset as a VA when that is credible, and fall back to the
 * physical mirror exactly when it is not. Titles whose surfaces already sit
 * in ordinary RAM (Wreckless renders to the tiled alias of physical
 * 0x01954000) keep the first path and are unaffected.
 */
static uint32_t dma_resolve(uint32_t offset)
{
    extern uint32_t xbox_ContiguousAllocatedBytes(void);

    /* Did this runtime hand the offset out as contiguous memory? Then the
     * bytes live in the window, and that is not a guess: the arena is a bump
     * allocator from XBOX_CONTIG_BASE, so everything below its high-water
     * mark is memory some MmAllocateContiguousMemory call returned. The
     * title's own writes go through the window, so the executor's must too.
     *
     * Checking this BEFORE the image test is the whole point. The image test
     * only catches an offset that would land on the title's code, and whether
     * it does is an accident of where the image happens to end: Half-Life 2's
     * colour surface is physical 0x00A6C000, which clears the image by 700 KB.
     * So it looked like an ordinary VA, and the executor cleared 1.2 MB of
     * black straight through the guest heap -- which faulted the title three
     * frames later on a pointer that had been overwritten, while the real
     * framebuffer at 0x80A6C000 stayed untouched and the screen stayed black. */
    if (offset < xbox_ContiguousAllocatedBytes())
        return XBOX_CONTIG_BASE + offset;
    if (!surface_hits_image(offset, 1))
        return offset;
    if ((uint64_t)offset < XBOX_CONTIG_SIZE)
        return XBOX_CONTIG_BASE + offset;
    return offset;                         /* nothing better to offer */
}

static int surface_write_refused(uint32_t base, uint32_t bytes, const char *what)
{
    static int said;

    if (!surface_hits_image(base, bytes))
        return 0;
    if (!said) {
        said = 1;
        fprintf(stderr,
                "  [GPU] REFUSING to %s surface 0x%08X..0x%08X: that overlaps "
                "the loaded image (0x%08X..0x%08X).\n"
                "  [GPU]   SET_SURFACE_COLOR_OFFSET is a DMA-object offset, not "
                "a guest VA, and this executor treats it as one. Writing here "
                "would destroy the title's own code and globals.\n",
                what, base, base + bytes, g_xbox_image_lo, g_xbox_image_hi);
        fflush(stderr);
    }
    return 1;
}

/* NV097 methods this executor acts on. */
#define NV097_SET_SURFACE_CLIP_HORIZONTAL 0x0200
#define NV097_SET_SURFACE_CLIP_VERTICAL   0x0204
#define NV097_SET_SURFACE_FORMAT          0x0208
#define NV097_SET_SURFACE_PITCH           0x020C
#define NV097_SET_SURFACE_COLOR_OFFSET    0x0210
#define NV097_SET_SURFACE_ZETA_OFFSET     0x0214
#define NV097_SET_CONTROL0                0x0290
#define NV097_SET_DEPTH_TEST_ENABLE       0x030C
#define NV097_SET_DEPTH_FUNC              0x0354
#define NV097_SET_DEPTH_MASK              0x035C
#define NV097_SET_ZSTENCIL_CLEAR_VALUE    0x1D8C
#define NV097_SET_CLEAR_RECT_HORIZONTAL   0x1D98
#define NV097_SET_CLEAR_RECT_VERTICAL     0x1D9C
#define NV097_SET_COLOR_CLEAR_VALUE       0x1D90
#define NV097_CLEAR_SURFACE               0x1D94
#define NV097_SET_BLEND_ENABLE            0x0304
#define NV097_SET_BLEND_FUNC_SFACTOR       0x0344
#define NV097_SET_BLEND_FUNC_DFACTOR       0x0348
#define NV097_SET_BLEND_COLOR              0x034C
#define NV097_SET_BLEND_EQUATION           0x0350
#define NV097_SET_TRANSFORM_EXEC_MODE      0x1E94
#define NV097_SET_VERTEX_DATA_ARRAY_OFFSET 0x1720   /* +i*4, 16 attributes */
#define NV097_SET_VERTEX_DATA_ARRAY_FORMAT 0x1760   /* +i*4 */
#define NV097_SET_BEGIN_END               0x17FC
#define NV097_SET_TEXTURE_OFFSET          0x1B00   /* +i*0x40 */
#define NV097_SET_TEXTURE_FORMAT          0x1B04
#define NV097_SET_TEXTURE_ADDRESS         0x1B08
#define NV097_SET_TEXTURE_CONTROL1        0x1B10
#define NV097_SET_TEXTURE_IMAGE_RECT      0x1B1C
#define NV097_SET_TEXTURE_PALETTE         0x1B20
/* The buffer flip. A title double-buffers by telling the GPU which buffer
 * the CRTC reads and which it draws into, advancing the write index and
 * then stalling until the flip has happened. Ignoring these means the
 * stall never clears: Half-Life 2's loader submits its initialisation,
 * asks for a flip, and waits for it in a loop that makes no kernel calls
 * and burns no dispatch, which reads as a hang with no cause.
 *
 * ponytail: the flip completes the moment it is asked for, because there is
 * no scanout to be in the middle of. That makes every frame land instantly
 * and a title that paces itself on the flip runs as fast as it can draw.
 * Pacing wants the vblank clock in the kernel, not a sleep in here. */
#define NV097_SET_FLIP_READ               0x0120
#define NV097_SET_FLIP_WRITE              0x0124
#define NV097_SET_FLIP_MODULO             0x0128
#define NV097_FLIP_INCREMENT_WRITE        0x012C
#define NV097_FLIP_STALL                  0x0130
#define NV097_ARRAY_ELEMENT16             0x1800
/* Draw a run of vertices straight out of the arrays, with no index list:
 * bits 0..23 are the first vertex, bits 24..31 the count minus one. It may
 * appear several times inside one BEGIN_END to draw a longer run. */
#define NV097_DRAW_ARRAYS                 0x1810
#define NV097_INLINE_ARRAY                0x1818
/* Immediate-mode vertices. SET_VERTEX3F/4F carry the position, and writing
 * its last component completes a vertex using whatever the SET_VERTEX_DATA*
 * registers currently hold for the other attributes. This is how Half-Life
 * 2's Xbox loader and the game's own 2D drawing submit every quad -- neither
 * uses INLINE_ARRAY -- so without these the executor saw SET_BEGIN_END pairs
 * with nothing attached and reported `draws 0` while a million and a half
 * textured quads a minute went past it. */
#define NV097_SET_VERTEX3F                0x1500   /* +0..0x08, 3 floats */
#define NV097_SET_VERTEX4F                0x1518   /* +0..0x0C, 4 floats */
#define NV097_SET_VERTEX_DATA2F_M         0x1880   /* + attr*8,  2 floats */
#define NV097_SET_VERTEX_DATA4F_M         0x1A00   /* + attr*16, 4 floats */
#define NV097_SET_VERTEX_DATA4UB          0x1940   /* + attr*4,  D3DCOLOR */
#define NV097_SET_VERTEX_DATA2S           0x1900
#define NV097_SET_VERTEX_DATA4S_M         0x1980

/* One immediate vertex, as this file packs it for the shared draw path:
 * position float4, diffuse D3DCOLOR, texcoord0 float2. */
#define IMM_VERTEX_DWORDS (NV_VERTEX_ATTRS * 4)

#define NV097_CLEAR_COLOR_MASK            0xF0   /* R,G,B,A bits */

/* One vertex attribute stream, as the title describes it. Attribute 0 is
 * position; the rest are colours, texture coordinates and so on. */
typedef struct {
    uint32_t offset;      /* guest address of element 0 */
    uint32_t type;        /* NV097 data type nibble */
    uint32_t size;        /* components per element */
    uint32_t stride;      /* bytes between elements */
} VertexAttr;

#define NV_VERTEX_ATTRS 16
#define NV_MAX_INDICES  4096
#define NV_MAX_INLINE   65536           /* includes all immediate attributes */

/* One of four independent texture stages, decoded from the NV097 methods. */
typedef struct {
    uint32_t offset;                    /* guest address of texel (0,0)  */
    uint32_t width, height;             /* from IMAGE_RECT               */
    uint32_t pitch;                     /* bytes per row, from CONTROL1  */
    uint32_t color;                     /* NV097 colour-format code      */
    uint32_t palette_offset, palette_entries; /* P8 ARGB lookup table      */
    uint32_t addr_u, addr_v;            /* wrap mode per axis            */
    int      valid;
} Texture;

static struct {
    VertexAttr attr[NV_VERTEX_ATTRS];
    uint32_t   prim;                    /* SET_BEGIN_END parameter, 0 = ended */
    uint16_t   idx[NV_MAX_INDICES];
    uint32_t   idx_count;
    /* INLINE_ARRAY payload: vertices written straight into the pushbuffer
     * instead of into a buffer the title points at. Same vertex format, a
     * different place to read them from. */
    uint32_t   inline_buf[NV_MAX_INLINE];
    uint32_t   inline_count;
    /* Current values of the immediate-mode attributes, and how many complete
     * vertices they have produced in this batch. */
    float      imm_attr[NV_VERTEX_ATTRS][4];
    uint16_t   imm_used;
    uint32_t   imm_count;
    int        inline_active;
    uint32_t   draws, verts, nonzero_draws;
    float      min_x, max_x, min_y, max_y;
    uint32_t color_offset, color_base, pitch, format;
    /* The surface the last batch actually drew into. A double-buffered title
     * has already pointed color_offset at the next buffer and cleared it by
     * the time the flip arrives, so dumping the current one dumps the frame
     * that has not been drawn yet -- which is how a correctly rendered
     * sequence came out as 12 black BMPs. */
    uint32_t drawn_offset, drawn_pitch;
    uint64_t pixels;
    uint32_t pixel_max;   /* brightest value any pixel write carried */
    uint32_t clip_x, clip_w, clip_y, clip_h;
    uint32_t clear_color;
    uint32_t blend_enable, blend_sfactor, blend_dfactor, blend_equation;
    uint32_t xform_mode;
    uint32_t clears, unhandled_total;
    uint32_t flip_read, flip_write, flip_modulo, flips;
    uint32_t tris_drawn, tris_skipped_offscreen, batches_untransformed;
    /* Why a batch came out flat. "Untextured" has two causes that look
     * identical on screen and want opposite fixes: the batch carried no
     * texture coordinates, or it did and the stage was not usable. */
    uint32_t batches_textured, batches_no_uv, batches_no_tex;
    Texture  tex;
    Texture  tex_extra[3];
    Nv2aCombiner rc;
    int rc_seen;
    uint32_t shader_clip_plane;
    uint32_t alpha_test, alpha_func, alpha_ref;
    uint32_t blend_color;
    uint32_t zeta_pitch;
    uint32_t zeta_offset, control0, depth_test, depth_func, depth_mask;
    uint32_t zstencil_clear, clear_rect_h, clear_rect_v, clear_rect_seen;
} s_gpu;
static Nv2aVertexProgram s_vp;
static uint32_t s_vp_batch;
static uint32_t s_vp_stamps[65536];
static int s_frame_trace_active;

static Texture *stage_texture(unsigned stage)
{
    return stage ? &s_gpu.tex_extra[stage - 1] : &s_gpu.tex;
}

typedef struct {
    float diffuse[4], specular[4], fog[4], tc[4][4];
} PixelVertex;

/* Unhandled methods, ranked. The interesting output is not that something was
 * skipped but which things dominate, because that is the order to implement
 * them in. */
#define PB_EXEC_MAX_UNHANDLED 2048
typedef struct { uint32_t method, count, last_param; } PbUnhandled;
static PbUnhandled s_unhandled[PB_EXEC_MAX_UNHANDLED];
static int s_unhandled_count;

/* Raw texture registers retained for draw-time diagnostics. */
#define NV_TEX_FIRST 0x1B00
#define NV_TEX_LAST  0x1BFC
static uint32_t s_tex_reg[(NV_TEX_LAST - NV_TEX_FIRST) / 4 + 1];
static uint8_t  s_tex_set[(NV_TEX_LAST - NV_TEX_FIRST) / 4 + 1];
/* SET_TEXTURE_FILTER / _BORDER_COLOR per stage. Kept beside, not inside,
 * Texture: s_gpu's layout is read by offline diagnostics. */
static uint32_t s_tex_filter[4], s_tex_border[4], s_tex_levels[4];

/* Formats whose dimensions come from the format word and whose coordinates
 * arrive normalised, rather than from a pitch and SET_TEXTURE_IMAGE_RECT with
 * coordinates in texels. Swizzled and block-compressed are both in this group,
 * and every place that used to test only for swizzled needs the pair. */
static int tex_size_from_format(uint32_t fmt)
{
    return d3d8_format_is_swizzled(fmt) || d3d8_format_dxt_block_bytes(fmt);
}

static void record_tex_reg(uint32_t method, uint32_t param)
{
    s_tex_reg[(method - NV_TEX_FIRST) / 4] = param;
    s_tex_set[(method - NV_TEX_FIRST) / 4] = 1;
    /* A pitch is a linear texture's property. A swizzled one has no rows and
     * so no pitch, and requiring one here refused every swizzled texture --
     * which is nearly all of them, since swizzled is the Xbox default. That
     * left the title's own textures unsampled and every textured quad drawn in
     * flat vertex colour. */
    {
        Texture *t = stage_texture((method - NV_TEX_FIRST) / 0x40);
        t->valid = t->offset && t->width && t->height
                && (tex_size_from_format(t->color) || t->pitch);
    }
}

/* Every distinct texture a batch was drawn with, and how many batches used it.
 *
 * The per-draw verbose print shows the first few draws of the first frame,
 * which is enough to see that texturing works at all and not enough to answer
 * "is a font page ever bound". This is the same shape as the unhandled-method
 * table below it: a small set, ranked, printed with the rest of the report. */
#define PB_EXEC_MAX_TEXTURES 64
typedef struct {
    uint32_t offset, color, width, height, batches;
} PbTexUse;
static PbTexUse s_tex_use[PB_EXEC_MAX_TEXTURES];
static int s_tex_use_count;

/* Defined below, next to the sampler it goes through. */
static void dump_texture_bmp(uint32_t seq);

static void note_texture_use(void)
{
    int i;

    if (!s_gpu.tex.valid)
        return;
    for (i = 0; i < s_tex_use_count; i++) {
        if (s_tex_use[i].offset == s_gpu.tex.offset
         && s_tex_use[i].color  == s_gpu.tex.color) {
            s_tex_use[i].batches++;
            return;
        }
    }
    if (s_tex_use_count < PB_EXEC_MAX_TEXTURES) {
        s_tex_use[s_tex_use_count].offset  = s_gpu.tex.offset;
        s_tex_use[s_tex_use_count].color   = s_gpu.tex.color;
        s_tex_use[s_tex_use_count].width   = s_gpu.tex.width;
        s_tex_use[s_tex_use_count].height  = s_gpu.tex.height;
        s_tex_use[s_tex_use_count].batches = 1;
        s_tex_use_count++;
        dump_texture_bmp((uint32_t)s_tex_use_count - 1);
    }
}

static void note_unhandled(uint32_t method, uint32_t param)
{
    int i;

    s_gpu.unhandled_total++;
    for (i = 0; i < s_unhandled_count; i++) {
        if (s_unhandled[i].method == method) {
            s_unhandled[i].count++;
            s_unhandled[i].last_param = param;
            return;
        }
    }
    if (s_unhandled_count < PB_EXEC_MAX_UNHANDLED) {
        s_unhandled[s_unhandled_count].method = method;
        s_unhandled[s_unhandled_count].count = 1;
        s_unhandled[s_unhandled_count].last_param = param;
        s_unhandled_count++;
    }
}

/* Read attribute `a` of vertex `index` as floats, decoding color, float,
 * normalized bytes, signed shorts and packed signed normals. Unsupported
 * types return 0; inline reads must stay inside the submitted payload. */
static int fetch_attr(const VertexAttr *a, uint32_t index, float out[4])
{
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    const uint8_t *p;
    uint32_t i, count, bytes;

    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
    if (!a->size || !a->stride)
        return 0;
    count=a->size<4?a->size:4;
    switch(a->type) {
    case 0: case 6: bytes=4; break;
    case 2: bytes=4*count; break;
    case 4: bytes=count; break;
    case 5: bytes=2*count; break;
    default: return 0;
    }
    if (s_gpu.inline_active) {
        /* The batch arrived as INLINE_ARRAY, so `offset` is a byte offset into
         * the buffered payload rather than a guest address -- and 0 is a legal
         * one there, which is why the offset test is on the other side of this
         * branch. */
        size_t at = (size_t)a->offset + (size_t)index * a->stride;
        if (at + bytes > (size_t)s_gpu.inline_count * 4)
            return 0;
        p = (const uint8_t *)s_gpu.inline_buf + at;
    } else {
        if (!a->offset)
            return 0;
        p = mem + a->offset + (size_t)index * a->stride;
        nv2a_render_memory_read(p,bytes);
    }

    switch (a->type) {
    case 0:                                  /* D3DCOLOR */
        /* A DWORD 0xAARRGGBB, so little-endian bytes are B,G,R,A -- not the
         * component order of every other format here. Returned as R,G,B,A so
         * callers need not know which format the title chose. */
        out[0] = (float)p[2] / 255.0f;
        out[1] = (float)p[1] / 255.0f;
        out[2] = (float)p[0] / 255.0f;
        out[3] = (float)p[3] / 255.0f;
        return 1;
    case 2:                                  /* float */
        for (i = 0; i < a->size && i < 4; i++)
            out[i] = ((const float *)p)[i];
        return 1;
    case 4:                                  /* unsigned byte, normalised */
        for (i = 0; i < a->size && i < 4; i++)
            out[i] = (float)p[i] / 255.0f;
        return 1;
    case 5:                                  /* S32K: signed short, unscaled */
        for(i=0;i<count;i++) {
            int16_t value;
            memcpy(&value,p+i*2,2);
            out[i]=(float)value;
        }
        return 1;
    case 6: {                                /* CMP: signed normalized 11/11/10 */
        uint32_t packed;
        int32_t x,y,z;
        memcpy(&packed,p,4);
        x=(int32_t)(packed&0x7FF); if(x&0x400)x-=0x800;
        y=(int32_t)((packed>>11)&0x7FF); if(y&0x400)y-=0x800;
        z=(int32_t)((packed>>22)&0x3FF); if(z&0x200)z-=0x400;
        out[0]=fmaxf(-1.0f,(float)x/1023.0f);
        out[1]=fmaxf(-1.0f,(float)y/1023.0f);
        out[2]=fmaxf(-1.0f,(float)z/511.0f);
        return 1;
    }
    default:
        return 0;
    }
}

static int batch_is_vp(void)
{
    static int off = -1;
    if (off < 0) {
        const char *e = getenv("RECOMP_VP");
        off = e && *e == '0';
    }
    return !off && (s_gpu.xform_mode & 3u) == 2u;
}

/* Cache within one submitted draw: strips/indexed meshes repeatedly name the
 * same vertices, while the guest can change their buffers between draws. */
static Nv2aVpOutput s_vp_cache[65536];
static void vp_compute(uint32_t index, Nv2aVpOutput *o)
{
    float v[16][4];
    uint32_t a;
    for (a = 0; a < 16; a++) fetch_attr(&s_gpu.attr[a], index, v[a]);
    nv2a_vp_run(&s_vp, v, o);
}
/* Transform a list of vertices into their cache slots, NV2A_VP_LANES at a
 * time through the block interpreter (instruction dispatch amortized over
 * the lanes). Only for prepared programs (nv2a_vp_prepare returned 1);
 * RECOMP_VP_BLOCK=0 keeps one vertex at a time. */
static void vp_compute_list(const uint16_t *list, LONG count)
{
    static int block = -1;
    float v[NV2A_VP_LANES][16][4];
    Nv2aVpOutput o[NV2A_VP_LANES];
    LONG i;
    unsigned n, l, a;
    uint32_t inputs = nv2a_vp_inputs();
    if (block < 0) { const char *e = getenv("RECOMP_VP_BLOCK"); block = !(e && *e == '0'); }
    if (!block) {
        for (i = 0; i < count; i++) vp_compute(list[i], &s_vp_cache[list[i]]);
        return;
    }
    for (i = 0; i < count; i += NV2A_VP_LANES) {
        n = count - i < NV2A_VP_LANES ? (unsigned)(count - i) : NV2A_VP_LANES;
        /* Inputs the program never reads are left unfetched: each fetch
         * is a guest-memory read check. */
        for (l = 0; l < n; l++)
            for (a = 0; a < 16; a++)
                if (inputs & (1u << a)) fetch_attr(&s_gpu.attr[a], list[i + l], v[l][a]);
        for (; l < NV2A_VP_LANES; l++) memset(v[l], 0, sizeof v[l]);
        nv2a_vp_run_block(&s_vp, (const float (*)[16][4])v, o, n);
        for (l = 0; l < n; l++) s_vp_cache[list[i + l]] = o[l];
    }
}
static const Nv2aVpOutput *vp_vertex(uint32_t index)
{
    static Nv2aVpOutput uncached;
    Nv2aVpOutput *o;
    if (index < 65536 && s_vp_stamps[index] == s_vp_batch && s_vp_batch)
        return &s_vp_cache[index];
    o = index < 65536 ? &s_vp_cache[index] : &uncached;
    vp_compute(index, o);
    if (index < 65536) s_vp_stamps[index] = s_vp_batch;
    return o;
}

/* Vertex programs run on the CPU, once per vertex, and were the largest
 * single cost of MVP 06's frame while the other cores idled. A batch's
 * vertices are independent (fetch reads guest memory and fixed state; each
 * writes only its own cache slot), so large batches are transformed up
 * front by a small pool, the command worker taking chunks too. Programs that
 * write constants stay serial. RECOMP_VP_THREADS=0 disables the pool. */
#define VP_POOL_MAX 8
#define VP_POOL_CHUNK 32
#define VP_POOL_MIN 128
static struct {
    SRWLOCK lock;
    CONDITION_VARIABLE wake;
    volatile LONG generation, next, done;
    volatile LONG count;
    const uint16_t *volatile list;
    int threads;
    LONG sleepers;          /* guarded by lock: workers parked on wake */
} s_vpp = { SRWLOCK_INIT, CONDITION_VARIABLE_INIT };
static uint64_t s_vpp_batches, s_vpp_vertices;
static void vp_pool_work(void)
{
    for (;;) {
        LONG count = s_vpp.count, i = InterlockedAdd(&s_vpp.next, VP_POOL_CHUNK) - VP_POOL_CHUNK, end;
        const uint16_t *list = s_vpp.list;
        if (i >= count) return;
        end = i + VP_POOL_CHUNK < count ? i + VP_POOL_CHUNK : count;
        vp_compute_list(list + i, end - i);
        InterlockedAdd(&s_vpp.done, end - i);
    }
}
static DWORD WINAPI vp_pool_thread(LPVOID unused)
{
    LONG seen = 0;
    (void)unused;
    for (;;) {
        int spin;
        for (spin = 0; spin < 2000 && s_vpp.generation == seen; spin++) YieldProcessor();
        if (s_vpp.generation == seen) {
            AcquireSRWLockExclusive(&s_vpp.lock);
            s_vpp.sleepers++;
            while (s_vpp.generation == seen)
                SleepConditionVariableSRW(&s_vpp.wake, &s_vpp.lock, INFINITE, 0);
            s_vpp.sleepers--;
            ReleaseSRWLockExclusive(&s_vpp.lock);
        }
        seen = s_vpp.generation;
        vp_pool_work();
    }
}
static int vp_pool_start(void)
{
    static int started = -1;
    if (started < 0) {
        const char *e = getenv("RECOMP_VP_THREADS");
        SYSTEM_INFO si;
        int i, want;
        GetSystemInfo(&si);
        want = e ? atoi(e) : (int)si.dwNumberOfProcessors / 2 - 1;
        if (want > VP_POOL_MAX) want = VP_POOL_MAX;
        started = 0;
        for (i = 0; i < want; i++) {
            HANDLE h = CreateThread(NULL, 0, vp_pool_thread, NULL, 0, NULL);
            if (!h) break;
            CloseHandle(h);
            s_vpp.threads++;
        }
        started = s_vpp.threads > 0;
        if (started)
            fprintf(stderr, "  [GPU] vertex program pool: %d threads\n", s_vpp.threads);
    }
    return started;
}
static void vp_prepare_batch(void)
{
    static uint16_t list[NV_MAX_INDICES];
    LONG n = 0;
    uint32_t i;
    static long pool_min = -1;
    if (pool_min < 0) { const char *e = getenv("RECOMP_VP_POOL_MIN"); pool_min = e ? atol(e) : VP_POOL_MIN; }
    if ((long)s_gpu.idx_count < pool_min || !batch_is_vp() || !nv2a_vp_prepare(&s_vp) || !vp_pool_start())
        return;
    for (i = 0; i < s_gpu.idx_count; i++) {
        uint16_t k = s_gpu.idx[i];
        if (s_vp_stamps[k] == s_vp_batch) continue;
        s_vp_stamps[k] = s_vp_batch;
        list[n++] = k;
    }
    if (n < pool_min) {
        /* Not worth a dispatch: compute here (stamps are already set). */
        vp_compute_list(list, n);
        return;
    }
    s_vpp.list = list;
    InterlockedExchange(&s_vpp.count, n);
    InterlockedExchange(&s_vpp.done, 0);
    InterlockedExchange(&s_vpp.next, 0);
    {
        /* Spinning workers see the generation change; only parked ones
         * need the (system-call) wake. */
        LONG sleepers;
        AcquireSRWLockExclusive(&s_vpp.lock);
        InterlockedIncrement(&s_vpp.generation);
        sleepers = s_vpp.sleepers;
        ReleaseSRWLockExclusive(&s_vpp.lock);
        if (sleepers) WakeAllConditionVariable(&s_vpp.wake);
    }
    vp_pool_work();
    while (InterlockedCompareExchange(&s_vpp.done, 0, 0) < n) YieldProcessor();
    s_vpp_batches++; s_vpp_vertices += (uint64_t)n;
}

static int fetch_position(uint32_t index, float p[4])
{
    if (batch_is_vp()) {
        const Nv2aVpOutput *o = vp_vertex(index);
        /* Near-plane clipping is still needed for triangles crossing W=0.
         * Until then, reject those rather than filling with wild coordinates. */
        if (!o->ok || o->pos[3] <= 0.0f) return 0;
        memcpy(p, o->pos, 4 * sizeof(float));
        p[3] = 1.0f / o->pos[3];
        return 1;
    }
    return fetch_attr(&s_gpu.attr[0], index, p);
}

static uint32_t surface_bpp(void)
{
    /* Clip width describes a rectangle, not the allocated surface width.
     * A shadow-atlas quadrant has half-width clipping and full-width pitch;
     * dividing those reports eight bytes per pixel and discards every draw.
     * NV097's low nibble supplies storage width independently of layout/zeta.
     * Channel packing outside ARGB8888/RGB565 remains partial. */
    static const uint8_t bytes[11] = {0,2,2,2,4,4,4,4,4,1,2};
    unsigned color = s_gpu.format & 15;
    return color < sizeof bytes ? bytes[color] : 0;
}


/* Write the current surface out as a 24-bit BMP.
 *
 * A framebuffer window needs someone watching it. A file does not, which makes
 * this the only way to check what a title actually rendered on a machine you
 * are not sitting at -- and the only way to put a picture in a bug report.
 *
 * ponytail: bottom-up 24bpp BMP, no palette, no compression. That is the one
 * format every viewer reads and it is 30 lines; PNG would need a dependency.
 */
static void dump_surface_bmp(void)
{
    const char *prefix = getenv("RECOMP_FB_DUMP");
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    uint32_t bpp = surface_bpp();
    static int seq;
    char path[512];
    uint32_t w = s_gpu.clip_w, h = s_gpu.clip_h, y, x;
    uint32_t row_bytes, pad, filesz;
    uint8_t hdr[54];
    FILE *f;

    if (!prefix || !w || !h || (bpp != 2 && bpp != 4) || !s_gpu.color_offset)
        return;
    nv2a_render_require_flush();

    row_bytes = w * 3;
    pad = (4 - (row_bytes & 3)) & 3;
    filesz = 54 + (row_bytes + pad) * h;

    snprintf(path, sizeof path, "%s%03d.bmp", prefix, seq++);
    f = fopen(path, "wb");
    if (!f)
        return;

    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2, &filesz, 4);
    hdr[10] = 54;
    hdr[14] = 40;
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    hdr[26] = 1;
    hdr[28] = 24;
    fwrite(hdr, 1, sizeof hdr, f);

    /* BMP rows run bottom-up. */
    for (y = h; y-- > 0; ) {
        const uint8_t *row = mem + dma_resolve(s_gpu.drawn_offset
                                                ? s_gpu.drawn_offset
                                                : s_gpu.color_offset)
                           + (size_t)(s_gpu.clip_y + y) * s_gpu.pitch;
        for (x = 0; x < w; x++) {
            uint8_t bgr[3];
            if (bpp == 4) {
                uint32_t v = ((const uint32_t *)row)[s_gpu.clip_x + x];
                bgr[0] = (uint8_t)(v);
                bgr[1] = (uint8_t)(v >> 8);
                bgr[2] = (uint8_t)(v >> 16);
            } else {
                uint16_t v = ((const uint16_t *)row)[s_gpu.clip_x + x];
                bgr[0] = (uint8_t)(( v        & 0x1F) << 3);
                bgr[1] = (uint8_t)(((v >>  5) & 0x3F) << 2);
                bgr[2] = (uint8_t)(((v >> 11) & 0x1F) << 3);
            }
            fwrite(bgr, 1, 3, f);
        }
        if (pad) {
            static const uint8_t zero[3] = {0, 0, 0};
            fwrite(zero, 1, pad, f);
        }
    }
    fclose(f);
    if (seq == 1)
        fprintf(stderr, "  [GPU] framebuffer dump: %s (%ux%u from 0x%08X %ubpp)\n",
                path, w, h, s_gpu.color_offset, bpp);
}

/* Defined below, next to the rest of the rasteriser; the clear path uses it
 * for RECOMP_RASTER_TEST. */
static void raster_triangle(const float a[3], const float b[3],
                            const float c[3], uint32_t argb,
                            const float uv[3][2], const float rhw[3],
                            const PixelVertex inputs[3]);

/* Clear rectangles have inclusive maxima and are independent of draw clipping.
 * Without explicit rectangle methods retain the legacy full-clip clear. */
static void clear_bounds(uint32_t bpp, uint32_t pitch, uint32_t r[4])
{
    r[0]=s_gpu.clip_x; r[1]=s_gpu.clip_y;
    r[2]=s_gpu.clip_x+s_gpu.clip_w; r[3]=s_gpu.clip_y+s_gpu.clip_h;
    if (s_gpu.clear_rect_seen & 1) {
        r[0]=s_gpu.clear_rect_h & 0xFFF;
        r[2]=((s_gpu.clear_rect_h>>16)&0xFFF)+1;
    }
    if (s_gpu.clear_rect_seen & 2) {
        r[1]=s_gpu.clear_rect_v & 0xFFF;
        r[3]=((s_gpu.clear_rect_v>>16)&0xFFF)+1;
    }
    if (r[2]>pitch/bpp) r[2]=pitch/bpp;
    if (r[3]>s_gpu.clip_y+s_gpu.clip_h) r[3]=s_gpu.clip_y+s_gpu.clip_h;
}

typedef struct { uint8_t *base; uint32_t pitch, bpp; } DepthSurface;

static DepthSurface depth_surface(uint8_t *mem)
{
    DepthSurface d={0};
    unsigned fmt=(s_gpu.format>>4)&15;
    uint32_t base;
    uint64_t bytes=(uint64_t)(s_gpu.clip_y+s_gpu.clip_h)*s_gpu.zeta_pitch;
    /* Other layouts/encodings need their own addressing and conversion. */
    if (((s_gpu.format>>8)&15)!=1 || (s_gpu.control0 & (1u<<12))
        || (fmt!=1 && fmt!=2) || !s_gpu.zeta_offset || !s_gpu.zeta_pitch)
        return d;
    d.bpp=fmt==1?2:4;
    if (s_gpu.zeta_pitch%d.bpp || bytes>UINT32_MAX) { d.bpp=0; return d; }
    base=dma_resolve(s_gpu.zeta_offset);
    if ((uint64_t)base+bytes>0x100000000ULL
        || surface_write_refused(base,(uint32_t)bytes,"access depth")) {
        d.bpp=0; return d;
    }
    d.base=mem+base; d.pitch=s_gpu.zeta_pitch;
    return d;
}

static void clear_depth(uint8_t *mem, uint32_t param)
{
    DepthSurface d=depth_surface(mem);
    uint32_t r[4],x,y;
    if (!d.base || !(param&3)) return;
    clear_bounds(d.bpp,d.pitch,r);
    for(y=r[1];y<r[3];y++)for(x=r[0];x<r[2];x++) {
        uint8_t *p=d.base+(size_t)y*d.pitch+x*d.bpp;
        if(d.bpp==2) {
            if(param&1) *(uint16_t*)p=(uint16_t)s_gpu.zstencil_clear;
        } else {
            uint32_t mask=((param&1)?0xFFFFFF00u:0)|((param&2)?255u:0);
            *(uint32_t*)p=(*(uint32_t*)p & ~mask)|(s_gpu.zstencil_clear & mask);
        }
    }
}

static int depth_pass(uint32_t src, uint32_t dst)
{
    switch(s_gpu.depth_func) {
    case 0x200: return 0;
    case 0x201: return src<dst;
    case 0x202: return src==dst;
    case 0x203: return src<=dst;
    case 0x204: return src>dst;
    case 0x205: return src!=dst;
    case 0x206: return src>=dst;
    case 0x207: return 1;
    default: return 0;
    }
}

static void depth_write(const DepthSurface *d, uint8_t *p, uint32_t z)
{
    if (!p || !s_gpu.depth_mask) return;
    if (d->bpp==2) *(uint16_t*)p=(uint16_t)z;
    else *(uint32_t*)p=(z<<8)|(*(uint32_t*)p & 255);
}

static void clear_surface(uint32_t param)
{
    uint8_t *mem = (uint8_t *)xbox_GetMemoryOffset();
    uint32_t bpp = surface_bpp();
    uint32_t y, x, r[4];

    nv2a_render_require_flush();
    /* The resident GPU target clears itself; RAM catches up on demand.
     * Same bounds and refusals as the RAM clear below. */
    {
        Nv2aRenderClear c={0};
        DepthSurface d=depth_surface(mem);
        uint32_t base=s_gpu.color_offset?dma_resolve(s_gpu.color_offset):0;
        c.flags=param & (NV097_CLEAR_COLOR_MASK|3u);
        if(!d.base)c.flags&=~3u;
        if(base && bpp==4 && s_gpu.pitch && s_gpu.clip_h
           && !surface_write_refused(base,(s_gpu.clip_y+s_gpu.clip_h)*s_gpu.pitch,"clear")) {
            c.color=mem+base;c.pitch=s_gpu.pitch;c.height=s_gpu.clip_y+s_gpu.clip_h;
            clear_bounds(4,s_gpu.pitch,c.rect);
            c.depth=d.base;c.depth_pitch=d.pitch;c.depth_bpp=d.bpp;
            if(d.base)clear_bounds(d.bpp,d.pitch,c.depth_rect);
            c.color_value=s_gpu.clear_color;c.zstencil=s_gpu.zstencil_clear;
            if(c.flags && nv2a_render_clear(&c)) {
                s_gpu.color_base=base;s_gpu.clears++;
                return;
            }
        }
    }
    clear_depth(mem,param);

    if (!(param & NV097_CLEAR_COLOR_MASK))
        return;
    if (!s_gpu.color_offset || !s_gpu.pitch || !s_gpu.clip_h || bpp == 0)
        return;
    {
        uint32_t base = dma_resolve(s_gpu.color_offset);
        if (surface_write_refused(base,
                                  (s_gpu.clip_y + s_gpu.clip_h) * s_gpu.pitch,
                                  "clear"))
            return;
        s_gpu.color_base = base;
    }

    clear_bounds(bpp,s_gpu.pitch,r);
    for (y = r[1]; y < r[3]; y++) {
        uint8_t *row = mem + s_gpu.color_base
                     + (size_t)y * s_gpu.pitch;
        if (bpp == 4) {
            /* CLEAR_SURFACE R/G/B/A are bits 4..7: clear only those
             * channels, as the hardware does for a partial colour mask. */
            uint32_t *p = (uint32_t *)row;
            uint32_t mask = ((param & 0x10) ? 0x00FF0000u : 0) | ((param & 0x20) ? 0x0000FF00u : 0)
                          | ((param & 0x40) ? 0x000000FFu : 0) | ((param & 0x80) ? 0xFF000000u : 0);
            for (x = r[0]; x < r[2]; x++)
                p[x] = (p[x] & ~mask) | (s_gpu.clear_color & mask);
        } else if (bpp == 2) {
            /* The clear value is always given as A8R8G8B8; a 16-bit surface
             * takes the same colour reduced to 5:6:5. */
            uint16_t v = (uint16_t)(((s_gpu.clear_color >> 8) & 0xF800)
                                  | ((s_gpu.clear_color >> 5) & 0x07E0)
                                  | ((s_gpu.clear_color >> 3) & 0x001F));
            uint16_t *p = (uint16_t *)row;
            for (x = r[0]; x < r[2]; x++)
                p[x] = v;
        }
    }
    s_gpu.clears++;
    /* Progress markers, interleaved with everything else in the log. The
     * summary says drawing stopped; only a marker next to the surrounding
     * activity says what the title was doing when it stopped. */
    if ((s_gpu.clears % 100) == 0)
        fprintf(stderr, "  [GPU] clear #%u\n", s_gpu.clears);
    /* Distinct clear colours actually used. "Cleared to black" and "the clear
     * never ran" look identical in the framebuffer, and only one of them is a
     * bug -- so record what was asked for, not just how often. */
    {
        static uint32_t seen[8];
        static int n;
        int i;
        for (i = 0; i < n; i++)
            if (seen[i] == s_gpu.clear_color) break;
        if (i == n && n < 8) {
            seen[n++] = s_gpu.clear_color;
            fprintf(stderr, "  [GPU] clear colour 0x%08X -> surface 0x%08X"
                            " (%ubpp)\n",
                    s_gpu.clear_color, s_gpu.color_offset, surface_bpp());
        }
    }

    /* Prove the pixel path end to end, independent of whether the title has
     * given us any geometry yet.
     *
     * "Nothing on screen" has three very different causes -- the surface
     * address or pitch is wrong, the rasteriser is broken, or the title's
     * vertex buffers are empty -- and they are indistinguishable from a black
     * window. RECOMP_RASTER_TEST draws one known triangle into the surface
     * just cleared, so a visible triangle rules out the first two and leaves
     * only the third. On Wreckless it is the third: attribute 0 decodes
     * correctly (float, size 2, stride 16) and the buffer it points at stays
     * zero.
     *
     * ponytail: bring-up aid, not a feature. It costs one branch per clear. */
    if (getenv("RECOMP_RASTER_TEST")) {
        static int announced;
        /* Every clear, not once: the title clears each frame and double-buffers,
         * so a triangle drawn a single time is erased before anyone sees it. */
        if (s_gpu.clip_w && s_gpu.clip_h) {
            float a[3]={0}, b[3]={0}, c[3]={0};
            a[0] = s_gpu.clip_w * 0.5f; a[1] = s_gpu.clip_h * 0.15f;
            b[0] = s_gpu.clip_w * 0.85f; b[1] = s_gpu.clip_h * 0.85f;
            c[0] = s_gpu.clip_w * 0.15f; c[1] = s_gpu.clip_h * 0.85f;
            raster_triangle(a, b, c, 0xFFFF00FFu, NULL, NULL, NULL);
            if (announced++ == 0)
            fprintf(stderr, "  [GPU] raster self-test: triangle (%.0f,%.0f)"
                            " (%.0f,%.0f) (%.0f,%.0f) into 0x%08X %ubpp\n",
                    a[0], a[1], b[0], b[1], c[0], c[1],
                    s_gpu.color_offset, surface_bpp());
        }
    }

    /* Show the surface actually being drawn into. A title that double-buffers
     * renders into the back buffer, so following AvSetDisplayMode's address
     * would show the one nothing is writing. */
    /* The window has to read where the pixels actually are, which is the
     * resolved address rather than the DMA-object offset. */
    xbox_FramebufferWindowSet(dma_resolve(s_gpu.color_offset), s_gpu.pitch);

    /* And open the window, rather than waiting for AvSetDisplayMode to do it.
     *
     * That was the only caller, so a title which draws before setting a display
     * mode -- or never sets one at all -- got no window however much it
     * rendered. The Xbox Dashboard clears a 1280x960 surface at 0x00088000 on
     * its first frame and had not called AvSetDisplayMode by then, so
     * RECOMP_FB_WINDOW=1 was set, the executor knew the address and the pitch,
     * and nothing appeared.
     *
     * Here is the better trigger anyway: this runs when a surface address is
     * known to be real, because a clear just used it. Idempotent and gated on
     * RECOMP_FB_WINDOW, so the cost is one interlocked compare per clear. */
    xbox_FramebufferWindowStart();
}


/* ── Rasteriser ──────────────────────────────────────────────────────────
 *
 * Fills triangles straight into the guest framebuffer, the same memory
 * clear_surface() writes and the framebuffer window already shows. That is the
 * whole reason it is done on the CPU rather than through D3D: nothing new has
 * to be plumbed for the result to be visible.
 *
 * Vertex programs produce screen-space positions and perspective attributes;
 * the legacy path accepts pretransformed UI. Register combiners consume the
 * interpolated colors and four texture results. Depth, clipping, fog and
 * additional sampler modes remain incomplete. A native graphics backend is
 * still needed for game-speed rendering.
 */

/* One texel, in the title's own format.
 *
 * The codes are the NV097 colour field, which is the Xbox D3DFMT_ enum --
 * src/d3d/d3d8_xbox.h is the table, and it is the table to check against
 * rather than recollection: 0x1E is LIN_X8R8G8B8 and not, as this first read
 * it, a byte-reversed BGRA. Getting that one wrong turned an opaque black
 * render target into a screen of pure blue, which is the kind of wrong that
 * looks like content.
 *
 * Linear, Morton-swizzled, DXT, P8 and packed YUV formats share this sampler.
 * Unsupported formats return failure instead of inventing a texel.
 *
 * ponytail: nearest texel, no filtering, whatever SET_TEXTURE_FILTER asked
 * for. Bilinear when a title's output actually depends on it.
 */
/* Off the edge of the texture, the way the title asked for.
 *
 * Refusing to sample instead is not neutral: it hands the caller back the
 * vertex colour, so a pass whose coordinates reach the last texel by half a
 * texel gets a bright line down the edge of the screen. The dashboard's
 * resolve does exactly that -- its last column and last row, 1119 pixels of
 * white on a black frame, from a rounding step at the boundary.
 */
static uint32_t wrap_coord(uint32_t c, uint32_t size, uint32_t mode)
{
    if (!size)
        return 0;
    if (mode == 1)                         /* wrap */
        return c % size;
    return c >= size ? size - 1 : c;       /* clamp, and everything else */
}

static uint32_t expand(uint32_t v, uint32_t bits)
{
    return d3d8_expand_channel(v, bits);
}

/* The linear format that decodes the same texels as a swizzled one.
 *
 * Swizzling changes where a texel lives, not what it says: A8R8G8B8 (0x06) and
 * LIN_A8R8G8B8 (0x12) are the same four bytes in the same order. So the whole
 * difference is the address calculation, and one of those lets every format
 * below serve both. Pairs read off the table in d3d8_xbox.h rather than
 * recalled -- the comment above this one is about getting exactly that wrong. */
static uint32_t linear_twin(uint32_t fmt)
{
    switch (fmt) {
    case 0x00: return 0x13;                /* L8        -> LIN_L8        */
    case 0x02: return 0x10;                /* A1R5G5B5  -> LIN_A1R5G5B5  */
    case 0x03: return 0x1C;                /* X1R5G5B5  -> LIN_X1R5G5B5  */
    case 0x04: return 0x1D;                /* A4R4G4B4  -> LIN_A4R4G4B4  */
    case 0x05: return 0x11;                /* R5G6B5    -> LIN_R5G6B5    */
    case 0x06: return 0x12;                /* A8R8G8B8  -> LIN_A8R8G8B8  */
    case 0x07: return 0x1E;                /* X8R8G8B8  -> LIN_X8R8G8B8  */
    case 0x19: return 0x1F;                /* A8        -> LIN_A8        */
    default:   return fmt;                 /* already linear, or unhandled */
    }
}

static int sample_texture_for(const Texture *tex, uint32_t u, uint32_t v, uint32_t *argb)
{
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    const uint8_t *p;
    uint32_t fmt;

    if (!tex->valid)
        return 0;
    u = wrap_coord(u, tex->width,  tex->addr_u);
    v = wrap_coord(v, tex->height, tex->addr_v);

    fmt = tex->color;
    if (d3d8_format_dxt_block_bytes(fmt))
        return d3d8_dxt_decode_texel(mem + tex->offset, fmt, u, v,
                                     tex->width, argb);
    if (d3d8_format_is_swizzled(fmt)) {
        /* Morton order: a texel's index is interleaved from x and y instead of
         * v*pitch + u, so index from the base of the image. The switch below
         * casts to each format's own width, which makes that index a texel
         * index for every one of them. */
        fmt = linear_twin(fmt);
        p = mem + tex->offset;
        u = swizzle_offset(u, v, tex->width, tex->height);
    } else {
        p = mem + tex->offset + (size_t)v * tex->pitch;
    }

    switch (fmt) {

    /* P8 texels are indices, including alpha from the ARGB palette. Font
     * pages use transparent entries; treating an unsupported page as vertex
     * colour turns each glyph quad into a solid rectangle. */
    case 0x0B: {
        uint32_t index = p[u];
        if (index >= tex->palette_entries)
            return 0;
        *argb = ((const uint32_t *)(mem + tex->palette_offset))[index];
        return 1;
    }

    /* 32-bit, alpha-red-green-blue in the dword. */
    case 0x12:                                      /* LIN_A8R8G8B8 */
        *argb = ((const uint32_t *)p)[u];
        return 1;
    case 0x1E:                                      /* LIN_X8R8G8B8 */
        *argb = ((const uint32_t *)p)[u] | 0xFF000000u;
        return 1;

    /* 32-bit, other channel orders. The name gives the byte order from the
     * top of the dword down, so each is a permutation of the same four. */
    case 0x3F: {                                    /* LIN_A8B8G8R8 */
        uint32_t t = ((const uint32_t *)p)[u];
        *argb = (t & 0xFF00FF00u) | ((t & 0xFF) << 16) | ((t >> 16) & 0xFF);
        return 1;
    }
    case 0x40: {                                    /* LIN_B8G8R8A8 */
        uint32_t t = ((const uint32_t *)p)[u];
        *argb = ((t & 0xFFu) << 24)                 /* A, from the bottom */
              | (((t >>  8) & 0xFFu) << 16)         /* R */
              | (((t >> 16) & 0xFFu) <<  8)         /* G */
              |  ((t >> 24) & 0xFFu);               /* B, from the top */
        return 1;
    }
    case 0x41: {                                    /* LIN_R8G8B8A8 */
        uint32_t t = ((const uint32_t *)p)[u];
        *argb = ((t & 0xFFu) << 24) | (t >> 8);
        return 1;
    }

    /* 16-bit. */
    case 0x10: {                                    /* LIN_A1R5G5B5 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = ((t & 0x8000u) ? 0xFF000000u : 0u)
              | (expand((t >> 10) & 0x1F, 5) << 16)
              | (expand((t >>  5) & 0x1F, 5) <<  8)
              |  expand( t        & 0x1F, 5);
        return 1;
    }
    case 0x1C: {                                    /* LIN_X1R5G5B5 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = 0xFF000000u
              | (expand((t >> 10) & 0x1F, 5) << 16)
              | (expand((t >>  5) & 0x1F, 5) <<  8)
              |  expand( t        & 0x1F, 5);
        return 1;
    }
    case 0x11: {                                    /* LIN_R5G6B5 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = 0xFF000000u
              | (expand((t >> 11) & 0x1F, 5) << 16)
              | (expand((t >>  5) & 0x3F, 6) <<  8)
              |  expand( t        & 0x1F, 5);
        return 1;
    }
    case 0x1D: {                                    /* LIN_A4R4G4B4 */
        uint32_t t = ((const uint16_t *)p)[u];
        *argb = (expand((t >> 12) & 0x0F, 4) << 24)
              | (expand((t >>  8) & 0x0F, 4) << 16)
              | (expand((t >>  4) & 0x0F, 4) <<  8)
              |  expand( t        & 0x0F, 4);
        return 1;
    }

    /* 8-bit. */
    case 0x13: {                                    /* LIN_L8 */
        uint32_t t = p[u];
        *argb = 0xFF000000u | (t << 16) | (t << 8) | t;
        return 1;
    }
    case 0x1F:                                      /* LIN_A8 */
        *argb = ((uint32_t)p[u] << 24) | 0x00FFFFFFu;
        return 1;

    /* Packed 4:2:2 video: adjacent pixels share U and V, with separate Y.
     * Falling back to vertex colour here hides otherwise decoded movies.
     * BT.601 limited-range conversion, matching the D3D8 upload path. */
    case 0x24:                                      /* YUY2 */
    case 0x25: {                                    /* UYVY */
        uint32_t yoff = fmt == 0x24 ? 0u : 1u;
        const uint8_t *g = p + (size_t)(u & ~1u) * 2;
        int c = (int)g[(u & 1u) * 2 + yoff] - 16;
        int cu = (int)g[1 - yoff] - 128;
        int cv = (int)g[3 - yoff] - 128;
        int r = (298 * c + 409 * cv + 128) >> 8;
        int gg = (298 * c - 100 * cu - 208 * cv + 128) >> 8;
        int b = (298 * c + 516 * cu + 128) >> 8;
        if (r < 0) r = 0;
        if (r > 255) r = 255;
        if (gg < 0) gg = 0;
        if (gg > 255) gg = 255;
        if (b < 0) b = 0;
        if (b > 255) b = 255;
        *argb = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)gg << 8)
              | (uint32_t)b;
        return 1;
    }

    default:
        return 0;
    }
}


static int sample_texture(uint32_t u, uint32_t v, uint32_t *argb)
{
    return sample_texture_for(&s_gpu.tex, u, v, argb);
}

/* Nearest sampling floors signed texel coordinates before addressing. A
 * negative fractional UV is the previous texel, not zero: clamping it before
 * REPEAT smears an edge across the stadium. Reduce out-of-range repeats in
 * float space so even large finite coordinates never overflow an integer. */
static uint32_t texture_coord(float c, uint32_t size, uint32_t mode)
{
    if (c >= 0 && c < (float)size) return (uint32_t)c;
    if (mode == 1) {
        float wrapped = fmodf(floorf(c), (float)size);
        if (wrapped < 0) wrapped += (float)size;
        return (uint32_t)wrapped;
    }
    return c < 0 ? 0 : size - 1;
}

static int sample_texture_coords(const Texture *tex, float u, float v, uint32_t *argb)
{
    if (!tex->valid || !tex->width || !tex->height || !isfinite(u) || !isfinite(v))
        return 0;
    return sample_texture_for(tex, texture_coord(u,tex->width,tex->addr_u),
                              texture_coord(v,tex->height,tex->addr_v), argb);
}

/* The ordinary 2D/projective stages used by the frontend. Keep coordinates
 * per stage: a second texture need not share either the first UVs or size.
 * Cube/volume/bump modes still need a sampler extension. */
static int combiner_texture(unsigned stage, const float tc[4], float out[4])
{
    unsigned mode = (s_gpu.rc.stage_program >> (stage * 5)) & 31, i;
    const Texture *t = stage_texture(stage);
    uint32_t texel;
    float u, v, q = tc[3];
    out[0] = out[1] = out[2] = 0.0f; out[3] = 1.0f;
    if (!mode) return 1;
    if (mode == 4) {
        for (i = 0; i < 4; i++) out[i] = fminf(1, fmaxf(0, tc[i]));
        return 1;
    }
    if (mode == 5) {
        for (i = 0; i < 4; i++) {
            int ge = (s_gpu.shader_clip_plane >> (stage * 4 + i)) & 1;
            if (ge ? tc[i] >= 0 : tc[i] < 0) return 0;
        }
        return 1;
    }
    if (mode != 1 || !isfinite(q) || fabsf(q) < 1e-20f) return 1;
    u = tc[0] / q; v = tc[1] / q;
    if (tex_size_from_format(t->color)) { u *= t->width; v *= t->height; }
    if (!isfinite(u) || !isfinite(v)) return 1;
    if (sample_texture_coords(t, u, v, &texel))
        nv2a_rc_unpack(texel, out);
    return 1;
}

static int alpha_pass(float a)
{
    unsigned value = (unsigned)(fminf(1, fmaxf(0, a)) * 255.0f + 0.5f);
    unsigned ref = s_gpu.alpha_ref & 255;
    if (!s_gpu.alpha_test) return 1;
    switch (s_gpu.alpha_func) {
    case 0x200: return 0;
    case 0x201: return value < ref;
    case 0x202: return value == ref;
    case 0x203: return value <= ref;
    case 0x204: return value > ref;
    case 0x205: return value != ref;
    case 0x206: return value >= ref;
    default: return 1;
    }
}

/* Write a bound texture out as a BMP, through the sampler rather than around it.
 *
 * "Which texture is this" is not answerable from an address and a format, and
 * it is the question behind most of the ones that matter -- is that a font
 * page or an icon atlas, did the swizzle decode, is the alpha inverted. Going
 * through sample_texture means the file shows exactly what the rasteriser
 * sees, so a decode bug appears here rather than only as a wrong-looking
 * triangle.
 *
 * ponytail: RGB only, alpha dropped. A glyph page is alpha and would come out
 * black, so alpha is composited onto mid-grey to stay legible; that is a
 * viewing choice, not a decode. One file per distinct texture, first use only.
 */
static void dump_texture_bmp(uint32_t seq)
{
    const char *prefix = getenv("RECOMP_TEX_DUMP");
    uint32_t w = s_gpu.tex.width, h = s_gpu.tex.height, x, y;
    uint32_t row_bytes, pad, filesz;
    uint8_t hdr[54];
    char path[512];
    FILE *f;

    if (!prefix || !w || !h || w > 4096 || h > 4096)
        return;
    nv2a_render_require_flush();
    row_bytes = w * 3;
    pad = (4 - (row_bytes & 3)) & 3;
    filesz = 54 + (row_bytes + pad) * h;

    snprintf(path, sizeof path, "%s%02u_%08X_fmt%02X.bmp",
             prefix, seq, s_gpu.tex.offset, s_gpu.tex.color);
    f = fopen(path, "wb");
    if (!f)
        return;
    memset(hdr, 0, sizeof hdr);
    hdr[0] = 'B'; hdr[1] = 'M';
    memcpy(hdr + 2, &filesz, 4);
    hdr[10] = 54; hdr[14] = 40;
    memcpy(hdr + 18, &w, 4);
    memcpy(hdr + 22, &h, 4);
    hdr[26] = 1; hdr[28] = 24;
    fwrite(hdr, 1, sizeof hdr, f);

    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            uint32_t argb = 0, a;
            uint8_t px[3];
            if (!sample_texture(x, h - 1 - y, &argb))
                argb = 0;
            a = (argb >> 24) & 0xFFu;
            /* over mid-grey, so an alpha-only page is visible either way */
            px[0] = (uint8_t)(((argb & 0xFFu) * a + 128u * (255u - a)) / 255u);
            px[1] = (uint8_t)((((argb >> 8) & 0xFFu) * a + 128u * (255u - a)) / 255u);
            px[2] = (uint8_t)((((argb >> 16) & 0xFFu) * a + 128u * (255u - a)) / 255u);
            fwrite(px, 1, 3, f);
        }
        if (pad) {
            static const uint8_t zero[3] = {0, 0, 0};
            fwrite(zero, 1, pad, f);
        }
    }
    fclose(f);
    fprintf(stderr, "  [TEXDUMP] %s (%ux%u fmt 0x%02X)\n",
            path, w, h, s_gpu.tex.color);
    fflush(stderr);
}

/* The surface, resolved once per batch.
 *
 * dma_resolve consults the contiguous arena's high-water mark and
 * surface_hits_image walks the image range; both were being done per pixel
 * -- dma_resolve twice -- which cost more than the rasterisation they
 * guarded. Neither answer can change inside a batch, because the colour
 * offset arrives as a method and a method cannot arrive mid-triangle.
 *
 * This is not a micro-optimisation for its own sake: the loader's video
 * paces on frames actually presented, so the rasteriser's throughput is the
 * playback rate. */
static uint8_t *s_surface;          /* host address of surface row 0 */

static int surface_begin_batch(const uint8_t *mem)
{
    uint32_t base = dma_resolve(s_gpu.color_offset);

    if (surface_hits_image(base, (s_gpu.clip_y + s_gpu.clip_h) * s_gpu.pitch))
        return 0;
    s_surface = (uint8_t *)mem + base;
    return 1;
}

/* NV097 factors have the GL enum values. Work in integer channel units so
 * the ordinary eight-bit fade retains its existing round-to-nearest result. */
static uint32_t blend_factor(uint32_t factor, uint32_t src, uint32_t dst,
                             unsigned shift)
{
    uint32_t s = (src >> shift) & 255, d = (dst >> shift) & 255;
    uint32_t sa = src >> 24, da = dst >> 24;
    uint32_t constant = (s_gpu.blend_color >> shift) & 255;
    switch (factor) {
    case 0: return 0;
    case 1: return 255;
    case 0x300: return s;
    case 0x301: return 255 - s;
    case 0x302: return sa;
    case 0x303: return 255 - sa;
    case 0x304: return da;
    case 0x305: return 255 - da;
    case 0x306: return d;
    case 0x307: return 255 - d;
    case 0x308: return shift == 24 ? 255 : (sa < 255-da ? sa : 255-da);
    case 0x8001: return constant;
    case 0x8002: return 255 - constant;
    case 0x8003: return s_gpu.blend_color >> 24;
    case 0x8004: return 255 - (s_gpu.blend_color >> 24);
    default: return 255;
    }
}

static uint32_t blend_argb(uint32_t src, uint32_t dst)
{
    uint32_t result = 0, shift;
    for (shift = 0; shift < 32; shift += 8) {
        int s = (src >> shift) & 255, d = (dst >> shift) & 255, value;
        if (s_gpu.blend_equation == 0x8007) value = s < d ? s : d;
        else if (s_gpu.blend_equation == 0x8008) value = s > d ? s : d;
        else {
            int a = s * blend_factor(s_gpu.blend_sfactor, src, dst, shift);
            int b = d * blend_factor(s_gpu.blend_dfactor, src, dst, shift);
            switch (s_gpu.blend_equation) {
            case 0: case 0x8006: value = a + b; break;
            case 0x800A: value = a - b; break;
            case 0x800B: value = b - a; break;
            /* Signed equations need signed-target handling as well. */
            default: return src;
            }
            value = value > 0 ? (value + 127) / 255 : 0;
            if (value > 255) value = 255;
        }
        result |= (uint32_t)value << shift;
    }
    return result;
}

static void put_pixel(uint8_t *mem, uint32_t bpp, int x, int y, uint32_t argb)
{
    uint8_t *row;

    (void)mem;
    if (x < (int)s_gpu.clip_x || x >= (int)(s_gpu.clip_x + s_gpu.clip_w))
        return;
    if (y < (int)s_gpu.clip_y || y >= (int)(s_gpu.clip_y + s_gpu.clip_h))
        return;
    s_gpu.pixels++;
    if ((argb & 0x00FFFFFFu) > (s_gpu.pixel_max & 0x00FFFFFFu))
        s_gpu.pixel_max = argb;
    row = s_surface + (size_t)y * s_gpu.pitch;
    /* Keep the common opaque movie/UI fade cheap. Stadium light and resolve
     * passes also use additive and constant-color factors; replacing their
     * destination discarded the existing scene. */
    int standard_fade = s_gpu.blend_sfactor == 0x0302
                     && s_gpu.blend_dfactor == 0x0303
                     && (!s_gpu.blend_equation || s_gpu.blend_equation == 0x8006);
    if (s_gpu.blend_enable && !(standard_fade && (argb >> 24) == 255)) {
        uint32_t alpha = argb >> 24, dst, mixed = 0, shift;
        if (standard_fade && !alpha) return;
        if (bpp == 4) {
            dst = ((const uint32_t *)row)[x];
        } else if (bpp == 2) {
            uint32_t t = ((const uint16_t *)row)[x];
            dst = 0xFF000000u | (expand((t >> 11) & 31, 5) << 16)
                  | (expand((t >> 5) & 63, 6) << 8) | expand(t & 31, 5);
        } else return;
        if (standard_fade) {
            for (shift = 0; shift < 32; shift += 8) {
                uint32_t channel = (((argb >> shift) & 255) * alpha
                    + ((dst >> shift) & 255) * (255 - alpha) + 127) / 255;
                mixed |= channel << shift;
            }
            argb = mixed;
        } else {
            argb = blend_argb(argb, dst);
        }
    }
    if (bpp == 4) {
        ((uint32_t *)row)[x] = argb;
    } else if (bpp == 2) {
        ((uint16_t *)row)[x] = (uint16_t)(((argb >> 8) & 0xF800)
                                        | ((argb >> 5) & 0x07E0)
                                        | ((argb >> 3) & 0x001F));
    }
}

/* Half-space fill. Barycentric edge functions rather than scanline slopes:
 * the same test decides both windings, so a title that emits clockwise
 * triangles does not silently render nothing. */
static void raster_triangle(const float a[3], const float b[3],
                            const float c[3], uint32_t argb,
                            const float uv[3][2], const float rhw[3],
                            const PixelVertex inputs[3])
{
    uint8_t *mem = (uint8_t *)xbox_GetMemoryOffset();
    uint32_t bpp = surface_bpp();
    float area;
    int minx, maxx, miny, maxy, x, y;
    int textured = uv && s_gpu.tex.valid;
    DepthSurface depth={0};

    if (bpp != 4 && bpp != 2)
        return;

    area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (area == 0.0f)
        return;                            /* degenerate */

    /* Where this batch writes. The same check the per-pixel path made, made
     * once: a surface address landing on the title's own image is no safer
     * one pixel at a time than 4.9 MB at once. */
    if (!surface_begin_batch(mem))
        return;
    if (s_gpu.depth_test) depth=depth_surface(mem);

    minx = (int)floorf(fminf(a[0], fminf(b[0], c[0])));
    maxx = (int)ceilf (fmaxf(a[0], fmaxf(b[0], c[0])));
    miny = (int)floorf(fminf(a[1], fminf(b[1], c[1])));
    maxy = (int)ceilf (fmaxf(a[1], fmaxf(b[1], c[1])));

    if (minx < (int)s_gpu.clip_x) minx = (int)s_gpu.clip_x;
    if (miny < (int)s_gpu.clip_y) miny = (int)s_gpu.clip_y;
    if (maxx > (int)(s_gpu.clip_x + s_gpu.clip_w)) maxx = (int)(s_gpu.clip_x + s_gpu.clip_w);
    if (maxy > (int)(s_gpu.clip_y + s_gpu.clip_h)) maxy = (int)(s_gpu.clip_y + s_gpu.clip_h);
    if (depth.base && maxx>(int)(depth.pitch/depth.bpp)) maxx=(int)(depth.pitch/depth.bpp);
    if (minx >= maxx || miny >= maxy) {
        s_gpu.tris_skipped_offscreen++;
        return;
    }

    for (y = miny; y < maxy; y++) {
        for (x = minx; x < maxx; x++) {
            uint8_t *zp=NULL;
            uint32_t z=0;
            float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float w0 = (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
            float w1 = (c[0] - b[0]) * (py - b[1]) - (c[1] - b[1]) * (px - b[0]);
            float w2 = (a[0] - c[0]) * (py - c[1]) - (a[1] - c[1]) * (px - c[0]);
            if (!((w0 >= 0 && w1 >= 0 && w2 >= 0)
               || (w0 <= 0 && w1 <= 0 && w2 <= 0)))
                continue;
            if (depth.base) {
                /* Vertex programs already output viewport Z (e.g. 16.5M for
                 * integer Z24), so interpolate screen Z affinely, separately
                 * from the perspective-correct colors/texture coordinates. */
                double screen_z=((double)w1*a[2]+(double)w2*b[2]+(double)w0*c[2])/area;
                double limit=depth.bpp==2?65535.0:16777215.0;
                uint32_t old;
                if (!isfinite(screen_z)) continue;
                if (screen_z<0) screen_z=0;
                if (screen_z>limit) screen_z=limit;
                z=(uint32_t)floor(screen_z+0.5);
                zp=depth.base+(size_t)y*depth.pitch+(size_t)x*depth.bpp;
                old=depth.bpp==2?*(uint16_t*)zp:*(uint32_t*)zp>>8;
                if (!depth_pass(z,old)) continue;
            }
            if (s_gpu.rc_seen && inputs) {
                PixelVertex p;
                float weights[3] = {w1, w2, w0}, sum = area;
                float tex[4][4], result[4];
                unsigned i, j, k;
                uint32_t packed = 0;
                int alive = 1;
                if (rhw) {
                    for (i = 0; i < 3; i++) weights[i] *= rhw[i];
                    sum = weights[0] + weights[1] + weights[2];
                }
                if (fabsf(sum) < 1e-20f) continue;
                memset(&p, 0, sizeof p);
                /* All fields are floats; interpolate explicitly to keep
                 * struct/array aliasing out of the pixel path. */
                for (i = 0; i < 3; i++) {
                    float w = weights[i] / sum;
                    for (j = 0; j < 4; j++) {
                        p.diffuse[j] += inputs[i].diffuse[j] * w;
                        p.specular[j] += inputs[i].specular[j] * w;
                        p.fog[j] += inputs[i].fog[j] * w;
                        for (k = 0; k < 4; k++) p.tc[k][j] += inputs[i].tc[k][j] * w;
                    }
                }
                for (i = 0; i < 4; i++)
                    if (!combiner_texture(i, p.tc[i], tex[i])) alive = 0;
                if (!alive) continue;
                nv2a_rc_eval(&s_gpu.rc, p.diffuse, p.specular, p.fog, tex, result);
                if (!alpha_pass(result[3])) continue;
                packed = ((uint32_t)(result[3]*255 + 0.5f) << 24)
                       | ((uint32_t)(result[0]*255 + 0.5f) << 16)
                       | ((uint32_t)(result[1]*255 + 0.5f) << 8)
                       | (uint32_t)(result[2]*255 + 0.5f);
                put_pixel(mem, bpp, x, y, packed);
                depth_write(&depth,zp,z);
                continue;
            }
            if (textured) {
                /* Barycentric, straight from the edge functions already
                 * computed: w1 is the area opposite a, w2 opposite b, w0
                 * opposite c, and the three sum to the whole triangle.
                 * VP batches retain reciprocal clip W for perspective-correct
                 * interpolation; pretransformed UI keeps its affine path. */
                uint32_t texel;
                float su = (w1 * uv[0][0] + w2 * uv[1][0] + w0 * uv[2][0]) / area;
                float sv = (w1 * uv[0][1] + w2 * uv[1][1] + w0 * uv[2][1]) / area;
                if (rhw) {
                    float q = w1*rhw[0] + w2*rhw[1] + w0*rhw[2];
                    if (fabsf(q) <= 1e-20f) continue;
                    su = (w1*uv[0][0]*rhw[0] + w2*uv[1][0]*rhw[1] + w0*uv[2][0]*rhw[2]) / q;
                    sv = (w1*uv[0][1]*rhw[0] + w2*uv[1][1]*rhw[1] + w0*uv[2][1]*rhw[2]) / q;
                }
                if (sample_texture_coords(&s_gpu.tex, su, sv, &texel)) {
                    /* Fixed-function UI batches use diffuse colour to fade
                     * a texture. Ignoring diffuse alpha keeps the previous
                     * legal screen opaque after its fade has finished. */
                    if (argb != 0xFFFFFFFFu) {
                        uint32_t modulated = 0, shift;
                        for (shift = 0; shift < 32; shift += 8)
                            modulated |= ((((texel >> shift) & 255)
                                * ((argb >> shift) & 255) + 127) / 255) << shift;
                        texel = modulated;
                    }
                    if (!alpha_pass((float)(texel>>24)/255.0f)) continue;
                    put_pixel(mem, bpp, x, y, texel);
                    depth_write(&depth,zp,z);
                    continue;
                }
            }
            if (!alpha_pass((float)(argb>>24)/255.0f)) continue;
            put_pixel(mem, bpp, x, y, argb);
            depth_write(&depth,zp,z);
        }
    }
    s_gpu.tris_drawn++;
    s_gpu.drawn_offset = s_gpu.color_offset;
    s_gpu.drawn_pitch = s_gpu.pitch;
}

/* Attribute 3 is diffuse colour in every NV2A layout that sets one. Absent it,
 * white -- a visible wrong colour beats an invisible correct one during
 * bring-up. */
/* Which attribute carries the colour.
 *
 * Slot 3 is diffuse by convention and titles that follow it are read straight
 * from there. Half-Life 2 does not: its vertex is position, colour, texcoord
 * at stride 24, and the colour arrives in slot 5. So fall back to the format
 * rather than the slot number -- D3DCOLOR is the one attribute type that is
 * only ever a colour, which makes it a stronger signal than the convention. */
static const VertexAttr *color_attr(void)
{
    uint32_t a;

    if (s_gpu.attr[3].offset && s_gpu.attr[3].stride)
        return &s_gpu.attr[3];
    for (a = 0; a < NV_VERTEX_ATTRS; a++)
        if (s_gpu.attr[a].type == 0 && s_gpu.attr[a].size == 4
                && s_gpu.attr[a].offset && s_gpu.attr[a].stride)
            return &s_gpu.attr[a];
    return &s_gpu.attr[3];
}

/* Attribute 9 is texture coordinate 0 in the NV2A vertex layout, the same way
 * 0 is position and 3 is diffuse -- for a title that follows the convention.
 *
 * Half-Life 2 does not, in either place. Its menu and HUD vertex is position,
 * colour, texcoord at stride 24, with the colour in slot 5 and the texcoords
 * in slot 7, so reading slot 9 found nothing and every batch drew untextured.
 * That is invisible rather than wrong-looking: the menu paints a full-screen
 * quad and then draws its text over it, and with no sampling both come out
 * white, so the screen is blank white and nothing suggests the text was ever
 * drawn.
 *
 * Falling back to the format works because the three attributes of such a
 * vertex are distinguishable: position is float3, colour is D3DCOLOR, and a
 * float2 is a texture coordinate and nothing else.
 *
 * This heuristic is retained only for legacy stage 0; stages 1–3 use their
 * declared slots 10–12, and program-mode draws use oT0–oT3. */
static const VertexAttr *texcoord_attr(void)
{
    uint32_t a;

    if (s_gpu.attr[9].offset && s_gpu.attr[9].stride)
        return &s_gpu.attr[9];
    for (a = 0; a < NV_VERTEX_ATTRS; a++)
        if (s_gpu.attr[a].type == 2 && s_gpu.attr[a].size == 2
                && s_gpu.attr[a].offset && s_gpu.attr[a].stride)
            return &s_gpu.attr[a];
    return &s_gpu.attr[9];
}

/* Texel coordinates, whichever convention the title used.
 *
 * The two are not interchangeable and the format decides which is in force: a
 * swizzled texture is addressed in [0,1], a linear one in texels. Both are
 * scaled to texels here so that everything downstream -- the barycentric
 * interpolation and the sampler -- works in one unit.
 *
 * This mattered the moment swizzled formats became samplable. Normalised
 * coordinates truncated to a texel index land on texel 0 for any coordinate
 * below 1.0, so a whole quad sampled a single texel and came out flat: the
 * background painted one near-black colour, which looks like a texture that
 * decoded wrong rather than one that was never indexed. */
static int fetch_texcoord(uint32_t index, float out[2])
{
    float t[4];

    if (batch_is_vp()) memcpy(t, vp_vertex(index)->t0, sizeof t);
    else if (!fetch_attr(texcoord_attr(), index, t)) return 0;
    out[0] = t[0];
    out[1] = t[1];
    if (tex_size_from_format(s_gpu.tex.color)) {
        out[0] *= (float)s_gpu.tex.width;
        out[1] *= (float)s_gpu.tex.height;
    }
    return 1;
}

static uint32_t vertex_color(uint32_t index)
{
    float c[4];

    if (batch_is_vp()) {
        unsigned i;
        memcpy(c, vp_vertex(index)->d0, sizeof c);
        for (i = 0; i < 4; i++) c[i] = fminf(1.0f, fmaxf(0.0f, c[i]));
    } else if (!fetch_attr(color_attr(), index, c)) return 0xFFFFFFFFu;
    return ((uint32_t)(c[3] * 255.0f) << 24)
         | ((uint32_t)(c[0] * 255.0f) << 16)
         | ((uint32_t)(c[1] * 255.0f) <<  8)
         |  (uint32_t)(c[2] * 255.0f);
}

/* Fog. The combiner's FOG register is (fog colour RGB, fog factor), the
 * factor 1 meaning unfogged. Per vertex, interpolated, as xemu forms it
 * (and the JSRF port's nv2a_fog_factor): the distance is the program's oFog.x,
 * or for fixed function the clamped specular alpha (GEN_MODE SPEC_ALPHA);
 * other fixed-function generation modes keep the unfogged factor. */
static uint32_t s_fog_enable, s_fog_mode, s_fog_gen, s_fog_color;
static float s_fog_param[3];
static float fog_factor(float d)
{
    float f, p0 = s_fog_param[0], p1 = s_fog_param[1];
    uint32_t mode = s_fog_mode;
    if (mode == 0x804 || mode == 0x802 || mode == 0x803) d = fabsf(d);
    if (!isfinite(d)) d = 0;
    switch (mode) {
    case 0x2601: case 0x804: f = p0 + d * p1 - 1.0f; break;
    case 0x800: case 0x802: f = p0 + exp2f(d * p1 * 16.0f) - 1.5f; break;
    case 0x801: case 0x803: f = p0 + exp2f(-d * d * p1 * p1 * 32.0f) - 1.5f; break;
    default: return 1.0f;
    }
    return f > 0.0f ? (f < 1.0f ? f : 1.0f) : 0.0f;
}
static void fog_fill(PixelVertex *v, int have_distance, float d)
{
    v->fog[0] = (float)(s_fog_color & 255) / 255.0f;          /* parameter is ABGR */
    v->fog[1] = (float)((s_fog_color >> 8) & 255) / 255.0f;
    v->fog[2] = (float)((s_fog_color >> 16) & 255) / 255.0f;
    v->fog[3] = s_fog_enable && have_distance ? fog_factor(d) : 1.0f;
}
static void pixel_vertex(uint32_t index, PixelVertex *v)
{
    unsigned st;
    memset(v, 0, sizeof *v);
    v->fog[3] = 1.0f;
    if (batch_is_vp()) {
        const Nv2aVpOutput *o = vp_vertex(index);
        fog_fill(v, 1, o->fog[0]);
        memcpy(v->diffuse, o->d0, sizeof v->diffuse);
        memcpy(v->specular, o->d1, sizeof v->specular);
        memcpy(v->tc[0], o->t0, sizeof o->t0);
        memcpy(v->tc[1], o->t1, sizeof o->t1);
        memcpy(v->tc[2], o->t2, sizeof o->t2);
        memcpy(v->tc[3], o->t3, sizeof o->t3);
    } else {
        if (!fetch_attr(color_attr(), index, v->diffuse))
            v->diffuse[0] = v->diffuse[1] = v->diffuse[2] = v->diffuse[3] = 1;
        fetch_attr(&s_gpu.attr[4], index, v->specular);
        for (st = 0; st < 4; st++)
            fetch_attr(st ? &s_gpu.attr[9 + st] : texcoord_attr(), index, v->tc[st]);
        fog_fill(v, s_fog_gen == 0, fminf(1.0f, fmaxf(0.0f, v->specular[3])));
    }
}

/* An untransformed batch drawn as if it were screen space smears a few pixels
 * into the corner, so the batch has to be classified before it is rasterised.
 *
 * This used to demand that every vertex land inside the surface, which is a
 * different question and the wrong one: geometry that extends past the
 * viewport is ordinary, and clipping it is raster_triangle's job (it clamps
 * its span to the clip rect). The dashboard is exactly the case that exposed
 * it -- a full-screen pass drawn as one oversized triangle, vertices at
 * (-0.5,-0.5), (2*w,-0.5), (-0.5,2*h), all correct and all rejected.
 *
 * What actually separates the two is scale. Object-space positions are model
 * units, a handful either side of the origin; screen-space ones are measured
 * in pixels of a surface hundreds of pixels wide. So: the batch has to be able
 * to touch the surface at all, and it has to be bigger than object space.
 *
 * ponytail: a genuinely tiny screen-space sprite reads as object space and is
 * skipped on the legacy fixed-function path. Program-mode batches use the VP
 * output directly and do not need this scale heuristic.
 */
#define OBJECT_SPACE_SPAN 8.0f

static int batch_is_screen_space(void)
{
    float p[4], lo_x, hi_x, lo_y, hi_y;
    uint32_t i;

    if (!s_gpu.clip_w || !s_gpu.clip_h || !s_gpu.idx_count)
        return 0;
    if ((s_gpu.xform_mode & 3u) == 2u) return batch_is_vp();
    if (!fetch_attr(&s_gpu.attr[0], s_gpu.idx[0], p))
        return 0;
    lo_x = hi_x = p[0];
    lo_y = hi_y = p[1];
    for (i = 1; i < s_gpu.idx_count; i++) {
        if (!fetch_attr(&s_gpu.attr[0], s_gpu.idx[i], p))
            return 0;
        if (p[0] < lo_x) lo_x = p[0];
        if (p[0] > hi_x) hi_x = p[0];
        if (p[1] < lo_y) lo_y = p[1];
        if (p[1] > hi_y) hi_y = p[1];
    }

    /* Entirely off the surface: nothing to draw under either reading. */
    if (hi_x < (float)s_gpu.clip_x
     || lo_x > (float)(s_gpu.clip_x + s_gpu.clip_w)
     || hi_y < (float)s_gpu.clip_y
     || lo_y > (float)(s_gpu.clip_y + s_gpu.clip_h))
        return 0;

    /* Small enough to be model units rather than pixels. */
    if (hi_x - lo_x < OBJECT_SPACE_SPAN && hi_y - lo_y < OBJECT_SPACE_SPAN)
        return 0;

    return 1;
}

/* NV097 primitive types that are triangles under some winding. */
/* NV097_SET_BEGIN_END codes (src/nv2a/nv2a_regs.h), not D3D primitive codes.
 * An off-by-one treated triangle lists as strips and connected unrelated
 * triangles across the movie/legal quads. */
#define NV_PRIM_TRIANGLES      5
#define NV_PRIM_TRIANGLE_STRIP 6
#define NV_PRIM_TRIANGLE_FAN   7
#define NV_PRIM_QUADS          8
#define NV_PRIM_QUAD_STRIP     9
#define NV_PRIM_POLYGON        10

/* How many post-draw captures to keep: enough to see whether the geometry
 * is stable from frame to frame, few enough not to fill a directory. */
#define FB_DUMP_AFTER_DRAW 8
static int s_drawn_dumps;

static void dump_surface_bmp(void);

/* One triangle by vertex index: gather position and, if the batch has one,
 * texture coordinate 0. A vertex whose position cannot be read is not drawn;
 * a batch whose texcoords cannot be read is drawn untextured rather than not
 * at all, so a missing coordinate stream costs the colour and not the shape.
 */
static void raster_indexed(uint32_t i0, uint32_t i1, uint32_t i2, uint32_t argb)
{
    float p[3][4], uv[3][2];
    float rhw[3];
    PixelVertex inputs[3];
    int textured;

    if (!fetch_position(i0, p[0])
     || !fetch_position(i1, p[1])
     || !fetch_position(i2, p[2]))
        return;

    textured = fetch_texcoord(i0, uv[0])
            && fetch_texcoord(i1, uv[1])
            && fetch_texcoord(i2, uv[2]);

    rhw[0] = p[0][3]; rhw[1] = p[1][3]; rhw[2] = p[2][3];
    if (s_gpu.rc_seen) {
        pixel_vertex(i0, &inputs[0]); pixel_vertex(i1, &inputs[1]); pixel_vertex(i2, &inputs[2]);
    }
    raster_triangle(p[0], p[1], p[2], argb,
                    textured ? (const float (*)[2])uv : NULL,
                    batch_is_vp() ? rhw : NULL, s_gpu.rc_seen ? inputs : NULL);
}

static int render_texture_decode(void *opaque,uint32_t *out)
{
    const Texture *t=(const Texture*)opaque;
    uint32_t x,y;
    /* Linear ARGB already has the renderer's exact byte layout. Copy only
     * the visible texels in each pitched row; the scalar sampler's wrapping,
     * format dispatch and address calculation are unnecessary in bounds.
     * Keep cache validation and target/palette alias barriers unchanged. */
    if(t->valid && t->color==0x12) {
        const uint8_t *source=(const uint8_t*)xbox_GetMemoryOffset()+t->offset;
        for(y=0;y<t->height;y++)
            memcpy(out+(size_t)y*t->width,source+(size_t)y*t->pitch,(size_t)t->width*4);
        return 1;
    }
    /* Swizzled 16/32-bit: sample_texture_for rebuilt the Morton masks and
     * deposited the bits again for every texel (~5 ms for one 256x256
     * R5G6B5 page a frame). Deposit each column and row once; the texel
     * index is their OR, and the conversions are the sampler's own. */
    if(t->valid && d3d8_format_is_swizzled(t->color) && t->width<=4096 && t->height<=4096) {
        uint32_t fmt=linear_twin(t->color),mask_x,mask_y;
        if(fmt==0x10 || fmt==0x1C || fmt==0x11 || fmt==0x1D || fmt==0x12 || fmt==0x1E) {
            static uint32_t col[4096],row[4096];
            const uint8_t *base=(const uint8_t*)xbox_GetMemoryOffset()+t->offset;
            xbox_swizzle_masks(t->width,t->height,&mask_x,&mask_y);
            for(x=0;x<t->width;x++)col[x]=swizzle_deposit(x,mask_x);
            for(y=0;y<t->height;y++)row[y]=swizzle_deposit(y,mask_y);
            for(y=0;y<t->height;y++) {
                uint32_t *dst=out+(size_t)y*t->width,r=row[y];
                if(fmt==0x12 || fmt==0x1E) {
                    const uint32_t *p=(const uint32_t*)base;
                    uint32_t alpha=fmt==0x1E?0xFF000000u:0;
                    for(x=0;x<t->width;x++)dst[x]=p[col[x]|r]|alpha;
                    continue;
                }
                for(x=0;x<t->width;x++) {
                    uint32_t v=((const uint16_t*)base)[col[x]|r];
                    switch(fmt) {
                    case 0x10:
                        dst[x]=((v&0x8000u)?0xFF000000u:0u)|(expand((v>>10)&0x1F,5)<<16)
                              |(expand((v>>5)&0x1F,5)<<8)|expand(v&0x1F,5);
                        break;
                    case 0x1C:
                        dst[x]=0xFF000000u|(expand((v>>10)&0x1F,5)<<16)
                              |(expand((v>>5)&0x1F,5)<<8)|expand(v&0x1F,5);
                        break;
                    case 0x11:
                        dst[x]=0xFF000000u|(expand((v>>11)&0x1F,5)<<16)
                              |(expand((v>>5)&0x3F,6)<<8)|expand(v&0x1F,5);
                        break;
                    default: /* 0x1D */
                        dst[x]=(expand((v>>12)&0x0F,4)<<24)|(expand((v>>8)&0x0F,4)<<16)
                              |(expand((v>>4)&0x0F,4)<<8)|expand(v&0x0F,4);
                        break;
                    }
                }
            }
            /* RECOMP_TEXTURE_DECODE_CHECK=1: compare with the scalar sampler. */
            {
                static int check=-1;
                static unsigned long long checked,mismatched,calls;
                if(check<0){const char *e=getenv("RECOMP_TEXTURE_DECODE_CHECK");check=e && *e=='1';}
                if(check) {
                    for(y=0;y<t->height;y++)for(x=0;x<t->width;x++) {
                        uint32_t ref;
                        if(sample_texture_for(t,x,y,&ref) && ref!=out[(size_t)y*t->width+x])mismatched++;
                        checked++;
                    }
                    if(!(++calls%64))
                        fprintf(stderr,"[TEXCHECK] fmt=%02X texels=%llu mismatches=%llu\n",
                                t->color,checked,mismatched);
                }
            }
            return 1;
        }
    }
    for(y=0;y<t->height;y++)for(x=0;x<t->width;x++)
        if(!sample_texture_for(t,x,y,&out[(size_t)y*t->width+x]))return 0;
    return 1;
}

/* Mip level geometry. Swizzled and compressed images store their levels
 * consecutively after the base, each max(1, size >> 1); DXT rounds every
 * level up to whole 4x4 blocks. Linear images have no mips. */
static uint64_t texture_level_bytes(const Texture *t,uint32_t bpp,uint32_t block,uint32_t level)
{
    uint32_t w=t->width>>level,h=t->height>>level;
    if(!w)w=1;
    if(!h)h=1;
    return block?(uint64_t)((w+3)/4)*((h+3)/4)*block:(uint64_t)w*h*bpp;
}
static uint32_t texture_bpp(uint32_t color)
{
    uint32_t fmt=d3d8_format_is_swizzled(color)?linear_twin(color):color;
    switch(fmt) {
    case 0x0B:case 0x13:case 0x1F:return 1;
    case 0x10:case 0x1C:case 0x11:case 0x1D:case 0x24:case 0x25:return 2;
    case 0x12:case 0x1E:case 0x3F:case 0x40:case 0x41:return 4;
    default:return 0;
    }
}
static int render_texture_decode(void *opaque,uint32_t *out);
static int render_texture_decode_level(void *opaque,uint32_t level,uint32_t *out)
{
    const Texture *t=(const Texture*)opaque;
    Texture at=*t;
    uint32_t block=d3d8_format_dxt_block_bytes(t->color),bpp=texture_bpp(t->color),i;
    for(i=0;i<level;i++)at.offset+=(uint32_t)texture_level_bytes(t,bpp,block,i);
    at.width=t->width>>level;at.height=t->height>>level;
    if(!at.width)at.width=1;
    if(!at.height)at.height=1;
    return render_texture_decode(&at,out);
}
static int render_texture(const Texture *t,Nv2aRenderTexture *out)
{
    const uint8_t *mem=(const uint8_t*)xbox_GetMemoryOffset();
    uint32_t block=d3d8_format_dxt_block_bytes(t->color),bpp=0;
    uint32_t fmt=d3d8_format_is_swizzled(t->color)?linear_twin(t->color):t->color;
    uint64_t bytes;
    memset(out,0,sizeof *out);
    if(!t->valid)return 1;
    out->width=t->width;out->height=t->height;out->pitch=t->pitch;out->format=t->color;
    out->normalized=tex_size_from_format(t->color);out->addr_u=t->addr_u;out->addr_v=t->addr_v;
    {
        unsigned stage=t==&s_gpu.tex?0:(unsigned)(t-s_gpu.tex_extra)+1;
        out->filter=s_tex_filter[stage&3];out->border=s_tex_border[stage&3];
    }
    switch(fmt) {
    case 0x0B:case 0x13:case 0x1F:bpp=1;break;
    case 0x10:case 0x1C:case 0x11:case 0x1D:case 0x24:case 0x25:bpp=2;break;
    case 0x12:case 0x1E:case 0x3F:case 0x40:case 0x41:bpp=4;break;
    default:if(!block)return 0;
    }
    bytes=block?(uint64_t)((t->width+3)/4)*((t->height+3)/4)*block:
        d3d8_format_is_swizzled(t->color)?(uint64_t)t->width*t->height*bpp:(uint64_t)t->pitch*t->height;
    if(!bytes || bytes>UINT32_MAX || (uint64_t)t->offset+bytes>0x100000000ULL)return 0;
    out->source=mem+t->offset;out->source_bytes=(size_t)bytes;
    if(t->color==0x0B) {
        out->palette=mem+t->palette_offset;out->palette_bytes=(size_t)t->palette_entries*4;
    }
    out->valid=1;out->opaque=(void*)t;out->decode=render_texture_decode;
    out->levels=1;
    if(tex_size_from_format(t->color)) {
        unsigned stage=t==&s_gpu.tex?0:(unsigned)(t-s_gpu.tex_extra)+1;
        uint32_t want=s_tex_levels[stage&3],max=1,size=t->width>t->height?t->width:t->height,i;
        while(size>1){size>>=1;max++;}
        if(want>max)want=max;
        if(want>1) {
            uint64_t all=0;
            for(i=0;i<want;i++)all+=texture_level_bytes(t,bpp,block,i);
            if((uint64_t)t->offset+all<=0x100000000ULL && all<=UINT32_MAX) {
                out->levels=want;out->source_bytes=(size_t)all;out->decode_level=render_texture_decode_level;
            }
        }
    }
    return 1;
}

/* Post-transform cache for one batch. Strips and indexed meshes reference
 * each vertex from up to three triangles, and every reference re-ran the
 * CPU vertex program (~14 ms of each MVP 06 frame). Within a batch the
 * inputs and all state are fixed, so a vertex's result depends only on its
 * index. render_batch advances the serial, invalidating every entry. */
static uint32_t s_vc_serial;
static uint32_t s_vc_stamp[65536];
static struct { float position[4]; PixelVertex p; int ok; } s_vc[65536];
static void vertex_cache_reset(void)
{
    if(++s_vc_serial==0) { memset(s_vc_stamp,0,sizeof s_vc_stamp);s_vc_serial=1; }
}
static int render_triangle(Nv2aRenderBatch *batch,Nv2aRenderVertex *vertices,
                           uint32_t i0,uint32_t i1,uint32_t i2)
{
    const uint32_t index[3]={i0,i1,i2};
    Nv2aRenderVertex v[3];
    uint32_t j;
    float flat[4],uv[2];
    if(!batch->has_combiner)nv2a_rc_unpack(vertex_color(i0),flat);
    for(j=0;j<3;j++) {
        const PixelVertex *pv;
        uint32_t k=index[j]&0xFFFF;
        if(s_vc_stamp[k]!=s_vc_serial || index[j]>0xFFFF) {
            s_vc[k].ok=fetch_position(index[j],s_vc[k].position);
            if(s_vc[k].ok) {
                if(!batch_is_vp())s_vc[k].position[3]=1;
                pixel_vertex(index[j],&s_vc[k].p);
            }
            s_vc_stamp[k]=index[j]>0xFFFF?0:s_vc_serial;
        }
        if(!s_vc[k].ok)return 1; /* same rejected triangle as software */
        pv=&s_vc[k].p;
        memcpy(v[j].position,s_vc[k].position,sizeof v[j].position);
        memcpy(v[j].diffuse,pv->diffuse,sizeof pv->diffuse);
        memcpy(v[j].specular,pv->specular,sizeof pv->specular);
        memcpy(v[j].fog,pv->fog,sizeof pv->fog);memcpy(v[j].tc,pv->tc,sizeof pv->tc);
        if(!batch->has_combiner) {
            memcpy(v[j].diffuse,flat,sizeof flat);
            if(batch->textures[0].valid && !fetch_texcoord(index[j],uv))return 0;
        }
    }
    if(batch->count+3>NV_MAX_INDICES*3)return 0;
    memcpy(vertices+batch->count,v,sizeof v);batch->count+=3;
    return 1;
}

/* Synchronous GPU readback has a fixed cost. Native six-stage/four-texture
 * measurements put 16x16 at ~0.28 ms in software versus ~0.64 ms on D3D11,
 * while 32x32 favors D3D11. Count clipped triangle bounding-box work, the
 * same loops the CPU rasterizer would visit; keep tiny batches on software.
 * RAM snapshots in the backend detect those software writes before reuse. */
static int render_small_batch(const Nv2aRenderBatch *b)
{
    static uint32_t cutoff=UINT32_MAX;
    static uint64_t small;
    double pixels=0;
    uint32_t i;
    if(cutoff==UINT32_MAX) {
        const char *e=getenv("RECOMP_GPU_MIN_PIXELS");
        unsigned long value=e?strtoul(e,NULL,10):2048;
        cutoff=value>16777216?16777216:(uint32_t)value;
    }
    if(!cutoff || nv2a_render_scoped())return 0;
    for(i=0;i+2<b->count;i+=3) {
        const float *a=b->vertices[i].position,*v=b->vertices[i+1].position,
                    *c=b->vertices[i+2].position;
        float left,right,top,bottom,area=(v[0]-a[0])*(c[1]-a[1])-(v[1]-a[1])*(c[0]-a[0]);
        if(!isfinite(area))return 0;
        if(area==0)continue;
        left=fmaxf((float)b->clip_x,floorf(fminf(a[0],fminf(v[0],c[0]))));
        right=fminf((float)b->clip_x+(float)b->clip_w,ceilf(fmaxf(a[0],fmaxf(v[0],c[0]))));
        top=fmaxf((float)b->clip_y,floorf(fminf(a[1],fminf(v[1],c[1]))));
        bottom=fminf((float)b->clip_y+(float)b->clip_h,ceilf(fmaxf(a[1],fmaxf(v[1],c[1]))));
        if(left<right && top<bottom)pixels+=(double)(right-left)*(bottom-top);
        if(pixels>=cutoff)return 0;
    }
    small++;
    if(small==1 || !(small%10000))fprintf(stderr,"[D3D11] small software batches=%llu cutoff=%u\n",
        (unsigned long long)small,cutoff);
    return 1;
}
/* RECOMP_EXEC_TIMING=1: per-flip wall milliseconds of the command worker's
 * phases, printed every 300 flips. Off by default (no clock reads). */
static int exec_timing=-1;
static uint64_t exec_build, exec_draw, exec_flip_ticks, exec_last_flip, exec_freq, exec_stall;
static uint64_t exec_now(void)
{
    LARGE_INTEGER t;
    if(exec_timing<0) {
        const char *e=getenv("RECOMP_EXEC_TIMING");
        exec_timing=e && *e=='1';
        if(exec_timing){QueryPerformanceFrequency(&t);exec_freq=(uint64_t)t.QuadPart;}
    }
    if(!exec_timing)return 0;
    QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;
}
static void exec_flip_report(void)
{
    static uint64_t flips, last_trap_count, last_trap_wait, base_build, base_draw, base_ticks, base_stall;
    static uint64_t base_pool, base_idle;
    extern volatile uint64_t g_nv2a_idle_ticks;
    uint64_t now=exec_now(),traps,wait;
    if(!now)return;
    if(exec_last_flip)exec_flip_ticks+=now-exec_last_flip;
    exec_last_flip=now;
    if(++flips%300)return;
    nv2a_trap_stats(&traps,&wait);
    {
        double ms=1000.0/(double)exec_freq/300.0;
        fprintf(stderr,"[EXEC] per flip ms: frame=%.2f build=%.2f draw=%.2f trap=%.2f stall=%.2f idle=%.2f traps=%.2f pooled=%.0f\n",
            (double)(exec_flip_ticks-base_ticks)*ms,(double)(exec_build-base_build)*ms,
            (double)(exec_draw-base_draw)*ms,(double)(wait-last_trap_wait)*ms,
            (double)(exec_stall-base_stall)*ms,(double)(g_nv2a_idle_ticks-base_idle)*ms,
            (double)(traps-last_trap_count)/300.0,
            (double)(s_vpp_vertices-base_pool)/300.0);
    }
    base_build=exec_build;base_draw=exec_draw;base_ticks=exec_flip_ticks;base_stall=exec_stall;
    base_pool=s_vpp_vertices;base_idle=g_nv2a_idle_ticks;
    last_trap_count=traps;last_trap_wait=wait;
}
static int render_batch(void)
{
    static Nv2aRenderVertex vertices[NV_MAX_INDICES*3];
    Nv2aRenderBatch b={0};
    uint8_t *mem=(uint8_t*)xbox_GetMemoryOffset();
    DepthSurface d;
    uint32_t i;
    uint64_t t0=exec_now(),t1;
    int drawn;
    if(!nv2a_render_enabled() || !surface_begin_batch(mem))return 0;
    vertex_cache_reset();
    vp_prepare_batch();
    b.color=s_surface;b.pitch=s_gpu.pitch;b.height=s_gpu.clip_y+s_gpu.clip_h;b.color_bpp=surface_bpp();
    b.color_format=s_gpu.format&15;b.swizzled=((s_gpu.format>>8)&15)==2;
    if(b.swizzled) {
        /* A swizzled surface has no row pitch: its extent is the format's
         * log2 width/height, and RAM holds width*height texels in Morton
         * order (the layout the title then samples it with). */
        uint32_t lw=(s_gpu.format>>16)&0xFF,lh=(s_gpu.format>>24)&0xFF;
        if(lw>12 || lh>12 || !b.color_bpp)return 0;
        b.pitch=(1u<<lw)*b.color_bpp;
        if(b.height<(1u<<lh))b.height=1u<<lh;
    }
    b.clip_x=s_gpu.clip_x;b.clip_y=s_gpu.clip_y;b.clip_w=s_gpu.clip_w;b.clip_h=s_gpu.clip_h;
    d=depth_surface(mem);b.depth=d.base;b.depth_pitch=d.pitch;b.depth_bpp=d.bpp;
    b.depth_test=s_gpu.depth_test;b.depth_func=s_gpu.depth_func;b.depth_mask=s_gpu.depth_mask;
    b.blend_enable=s_gpu.blend_enable;b.blend_src=s_gpu.blend_sfactor;b.blend_dst=s_gpu.blend_dfactor;
    b.blend_equation=s_gpu.blend_equation;b.blend_color=s_gpu.blend_color;
    b.alpha_test=s_gpu.alpha_test;b.alpha_func=s_gpu.alpha_func;b.alpha_ref=s_gpu.alpha_ref;
    b.shader_clip_plane=s_gpu.shader_clip_plane;b.combiner=s_gpu.rc;b.has_combiner=s_gpu.rc_seen;
    for(i=0;i<4;i++) {
        unsigned mode=b.has_combiner?(b.combiner.stage_program>>(5*i))&31:(i?0:1);
        if(mode==1 && !render_texture(stage_texture(i),&b.textures[i]))return 0;
    }
#define TRI(a,b_,c) do { if(!render_triangle(&b,vertices,(a),(b_),(c)))return 0; } while(0)
    switch(s_gpu.prim) {
    case NV_PRIM_TRIANGLES:
        for(i=0;i+2<s_gpu.idx_count;i+=3)TRI(s_gpu.idx[i],s_gpu.idx[i+1],s_gpu.idx[i+2]);break;
    case NV_PRIM_TRIANGLE_STRIP:
        for(i=0;i+2<s_gpu.idx_count;i++)TRI(s_gpu.idx[i],s_gpu.idx[i+1],s_gpu.idx[i+2]);break;
    case NV_PRIM_TRIANGLE_FAN:case NV_PRIM_POLYGON:
        for(i=1;i+1<s_gpu.idx_count;i++)TRI(s_gpu.idx[0],s_gpu.idx[i],s_gpu.idx[i+1]);break;
    case NV_PRIM_QUADS:
        for(i=0;i+3<s_gpu.idx_count;i+=4) {
            TRI(s_gpu.idx[i],s_gpu.idx[i+1],s_gpu.idx[i+2]);TRI(s_gpu.idx[i],s_gpu.idx[i+2],s_gpu.idx[i+3]);
        }break;
    case NV_PRIM_QUAD_STRIP:
        for(i=0;i+3<s_gpu.idx_count;i+=2) {
            TRI(s_gpu.idx[i],s_gpu.idx[i+1],s_gpu.idx[i+3]);TRI(s_gpu.idx[i],s_gpu.idx[i+3],s_gpu.idx[i+2]);
        }break;
    default:return 0;
    }
#undef TRI
    b.vertices=vertices;
    /* RECOMP_DRAW_LOG_FLIP=N: describe every batch of flip N (diagnostics). */
    {
        static long log_flip=-2;
        static const char *trigger;
        static uint32_t checked;
        if(log_flip==-2){const char *e=getenv("RECOMP_DRAW_LOG_FLIP");log_flip=e?strtol(e,NULL,0):-1;
                         trigger=getenv("RECOMP_DRAW_LOG_TRIGGER");}
        /* RECOMP_DRAW_LOG_TRIGGER=<path>: log the flip after that file appears
         * (polled every 30 flips, then deleted), for scenes reached live. */
        if(trigger && s_gpu.flips!=checked && !(s_gpu.flips%30)) {
            checked=s_gpu.flips;
            if(GetFileAttributesA(trigger)!=INVALID_FILE_ATTRIBUTES){DeleteFileA(trigger);log_flip=(long)s_gpu.flips+1;}
        }
        if(log_flip>=0 && s_gpu.flips==(uint32_t)log_flip && b.count) {
            uint8_t *base=(uint8_t*)xbox_GetMemoryOffset();
            fprintf(stderr,"[DRAW] tgt=%08X %ux%u z=%08X zt=%u zw=%u blend=%u %X/%X verts=%u pos0=(%.2f,%.2f,%.4f,%.3f) pos1=(%.2f,%.2f)\n",
                (unsigned)(b.color-base),b.pitch/4,b.height,b.depth?(unsigned)(b.depth-base):0,b.depth_test,b.depth_mask,
                b.blend_enable,b.blend_src,b.blend_dst,b.count,
                b.vertices[0].position[0],b.vertices[0].position[1],b.vertices[0].position[2],b.vertices[0].position[3],
                b.vertices[1].position[0],b.vertices[1].position[1]);
            for(i=0;i<4;i++)if(b.textures[i].valid)
                fprintf(stderr,"[DRAW]   t%u src=%08X fmt=%02X %ux%u pitch=%u norm=%u addr=%u/%u filt=%08X tc0=(%.3f,%.3f) tc1=(%.3f,%.3f)\n",
                    i,(unsigned)((const uint8_t*)b.textures[i].source-base),b.textures[i].format,b.textures[i].width,
                    b.textures[i].height,b.textures[i].pitch,b.textures[i].normalized,b.textures[i].addr_u,b.textures[i].addr_v,
                    b.textures[i].filter,b.vertices[0].tc[i][0],b.vertices[0].tc[i][1],b.vertices[1].tc[i][0],b.vertices[1].tc[i][1]);
        }
    }
    t1=exec_now();
    if(t1)exec_build+=t1-t0;
    drawn=b.count && !render_small_batch(&b) && nv2a_render_draw(&b);
    if(t1)exec_draw+=exec_now()-t1;
    if(!drawn)return 0;
    s_gpu.tris_drawn+=b.count/3;s_gpu.drawn_offset=s_gpu.color_offset;s_gpu.drawn_pitch=s_gpu.pitch;
    return 1;
}

static void raster_batch(void)
{
    uint32_t i;
    uint32_t before = s_gpu.tris_drawn;

    if (s_gpu.idx_count < 3)
        return;
    if (!batch_is_screen_space()) {
        s_gpu.batches_untransformed++;
        return;
    }

    /* Count why, once per batch: the texture stage cannot change inside one. */
    {
        const VertexAttr *tc = texcoord_attr();

        if (!(tc->offset && tc->stride))
            s_gpu.batches_no_uv++;
        else if (!s_gpu.tex.valid)
            s_gpu.batches_no_tex++;
        else {
            s_gpu.batches_textured++;
            note_texture_use();
        }
    }

    if(render_batch())goto capture;
    nv2a_render_require_flush();
    switch (s_gpu.prim) {
    case NV_PRIM_TRIANGLES:
        for (i = 0; i + 2 < s_gpu.idx_count; i += 3)
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1], s_gpu.idx[i+2],
                           vertex_color(s_gpu.idx[i]));
        break;
    case NV_PRIM_TRIANGLE_STRIP:
        for (i = 0; i + 2 < s_gpu.idx_count; i++)
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1], s_gpu.idx[i+2],
                           vertex_color(s_gpu.idx[i]));
        break;
    case NV_PRIM_TRIANGLE_FAN:
    case NV_PRIM_POLYGON:
        for (i = 1; i + 1 < s_gpu.idx_count; i++)
            raster_indexed(s_gpu.idx[0], s_gpu.idx[i], s_gpu.idx[i+1],
                           vertex_color(s_gpu.idx[0]));
        break;
    case NV_PRIM_QUADS:
        for (i = 0; i + 3 < s_gpu.idx_count; i += 4) {
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1], s_gpu.idx[i+2],
                           vertex_color(s_gpu.idx[i]));
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+2], s_gpu.idx[i+3],
                           vertex_color(s_gpu.idx[i]));
        }
        break;
    case NV_PRIM_QUAD_STRIP:
        for (i = 0; i + 3 < s_gpu.idx_count; i += 2) {
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+1], s_gpu.idx[i+3],
                           vertex_color(s_gpu.idx[i]));
            raster_indexed(s_gpu.idx[i], s_gpu.idx[i+3], s_gpu.idx[i+2],
                           vertex_color(s_gpu.idx[i]));
        }
        break;
    default:
        break;                             /* points and lines: not yet */
    }

capture:
    if (s_gpu.tris_drawn && (s_gpu.tris_drawn % 50000) == 0)
        fprintf(stderr, "  [GPU] %u triangles rasterised\n", s_gpu.tris_drawn);

    /* Capture the surface while the geometry is still on it.
     *
     * The periodic report dumps too, but a title clears every frame and draws
     * in only some of them, so a report almost always lands on a surface that
     * was wiped a moment ago -- which reads as "nothing was drawn" when the
     * triangles went down correctly just before it. A few frames that actually
     * contain geometry are worth more than any number of clears. */
    if (s_gpu.tris_drawn != before && s_drawn_dumps < FB_DUMP_AFTER_DRAW) {
        s_drawn_dumps++;
        dump_surface_bmp();
    }
}

/* What a batch actually contains. Before anything can be rasterised, the
 * question is what space attribute 0 arrives in: a title running a vertex
 * program hands over object-space positions that mean nothing without running
 * it, while pre-transformed screen-space coordinates can be drawn directly. */

/* Like Burnout 3's one-frame trace: create the named flag after navigating
 * to the screen of interest. Capturing state at draws avoids mistaking boot
 * defaults or a later batch's state for the geometry which disappeared. */
static void trace_current_batch(void)
{
    static const char *flag;
    static int initialized;
    uint32_t i;
    if (!initialized) { flag = getenv("RECOMP_FRAME_TRACE"); initialized = 1; }
    if (!flag) return;
    if (!s_frame_trace_active) {
        FILE *f = fopen(flag, "rb");
        if (!f) return;
        fclose(f);
        /* Consume each request. A new file can request a later frame without
         * restarting the title or changing its live diagnostic flags. */
        if (remove(flag)!=0) return;
        s_frame_trace_active = 1;
        fprintf(stderr, "[FTRACE] begin\n");
    }
    fprintf(stderr, "[FTRACE] draw=%u prim=%u n=%u rt=%08X mode=%X blend=%u:%X/%X rc=%08X final=%08X/%08X stageprog=%08X\n",
            s_gpu.draws, s_gpu.prim, s_gpu.idx_count, s_gpu.color_offset,
            s_gpu.xform_mode, s_gpu.blend_enable, s_gpu.blend_sfactor, s_gpu.blend_dfactor,
            s_gpu.rc.control, s_gpu.rc.final0, s_gpu.rc.final1,
            s_gpu.rc.stage_program);
    fprintf(stderr, "[FTRACE] state fmt=%08X pitch=%u clip=%u,%u,%u,%u alpha=%u:%X/%u finalc=%08X/%08X vp=%u/%u start=%u\n",
            s_gpu.format, s_gpu.pitch, s_gpu.clip_x, s_gpu.clip_y,
            s_gpu.clip_w, s_gpu.clip_h, s_gpu.alpha_test, s_gpu.alpha_func,
            s_gpu.alpha_ref, s_gpu.rc.final_c0, s_gpu.rc.final_c1,
            s_vp.prog_gen, s_vp.const_gen, s_vp.prog_start);
    fprintf(stderr, "[FTRACE] blend equation=%X constant=%08X\n",
            s_gpu.blend_equation, s_gpu.blend_color);
    /* Capture the actual draw-time depth state alongside remaining backend
     * gaps, including floating/swizzled formats and stencil operations. */
    fprintf(stderr, "[FTRACE] depth test=%X func=%X mask=%X zeta=%08X pitch=%u clear=%08X control=%08X\n",
            s_gpu.depth_test, s_gpu.depth_func, s_gpu.depth_mask,
            s_gpu.zeta_offset, s_gpu.zeta_pitch, s_gpu.zstencil_clear,
            s_gpu.control0);
    fprintf(stderr,"[FTRACE] clearrect h=%08X v=%08X seen=%u\n",
            s_gpu.clear_rect_h,s_gpu.clear_rect_v,s_gpu.clear_rect_seen);
    fprintf(stderr,"[FTRACE] arrays inline=%d immediate=%d bytes=%u\n",
            s_gpu.inline_active,s_gpu.imm_count!=0,s_gpu.inline_count*4);
    for(i=0;i<NV_VERTEX_ATTRS;i++) if(s_gpu.attr[i].size)
        fprintf(stderr,"[FTRACE] attr%u off=%08X type=%u count=%u stride=%u\n",
                i,s_gpu.attr[i].offset,s_gpu.attr[i].type,s_gpu.attr[i].size,s_gpu.attr[i].stride);
    for (i = 0; i < (s_gpu.rc.control & 255) && i < 8; i++)
        fprintf(stderr, "[FTRACE] rc%u color=%08X/%08X alpha=%08X/%08X factors=%08X/%08X\n", i,
                s_gpu.rc.color_icw[i], s_gpu.rc.color_ocw[i],
                s_gpu.rc.alpha_icw[i], s_gpu.rc.alpha_ocw[i],
                s_gpu.rc.factor0[i], s_gpu.rc.factor1[i]);
    for (i = 0; i < 4; i++) {
        unsigned base = i * 16;
        const Texture *t = stage_texture(i);
        fprintf(stderr, "[FTRACE] tex%u off=%08X fmt=%08X ctrl=%08X pitch=%08X rect=%08X\n", i,
                s_tex_reg[base], s_tex_reg[base+1], s_tex_reg[base+3],
                s_tex_reg[base+4], s_tex_reg[base+7]);
        fprintf(stderr, "[FTRACE] tex%u decoded va=%08X size=%ux%u pitch=%u color=%X valid=%d wrap=%u/%u palette=%08X/%u\n", i,
                t->offset, t->width, t->height, t->pitch, t->color,
                t->valid, t->addr_u, t->addr_v, t->palette_offset, t->palette_entries);
    }
    for (i = 0; i < s_gpu.idx_count && i < 3; i++) {
        float p[4] = {0}, t[2] = {0};
        PixelVertex pv;
        unsigned stage;
        int ok = fetch_position(s_gpu.idx[i], p);
        fetch_texcoord(s_gpu.idx[i], t);
        pixel_vertex(s_gpu.idx[i], &pv);
        fprintf(stderr, "[FTRACE] v%u ok=%d pos=%g,%g,%g,%g uv=%g,%g diffuse=%08X\n", i, ok,
                p[0],p[1],p[2],p[3],t[0],t[1],vertex_color(s_gpu.idx[i]));
        fprintf(stderr, "[FTRACE] v%u colors d=%g,%g,%g,%g s=%g,%g,%g,%g fog=%g,%g,%g,%g\n", i,
                pv.diffuse[0],pv.diffuse[1],pv.diffuse[2],pv.diffuse[3],
                pv.specular[0],pv.specular[1],pv.specular[2],pv.specular[3],
                pv.fog[0],pv.fog[1],pv.fog[2],pv.fog[3]);
        for (stage = 0; stage < 4; stage++)
            fprintf(stderr, "[FTRACE] v%u t%u=%g,%g,%g,%g\n", i, stage,
                    pv.tc[stage][0],pv.tc[stage][1],pv.tc[stage][2],pv.tc[stage][3]);
    }
}

static void draw_primitive(void)
{
    float v[4];
    uint32_t i;

    if (!s_gpu.prim || !s_gpu.idx_count)
        return;
    s_gpu.draws++;
    if (++s_vp_batch == 0) memset(s_vp_stamps, 0, sizeof s_vp_stamps);
    if ((s_gpu.draws % 20000) == 0)   /* progress marker; stderr is unbuffered */
        fprintf(stderr, "  [GPU] draw #%u\n", s_gpu.draws);
    s_gpu.verts += s_gpu.idx_count;

    /* How many batches carry coordinates at all, and what range they span.
     * A pipeline that decodes perfectly and draws nothing is indistinguishable
     * from one that never ran, unless the vertices themselves are measured. */
    {
        float p[4];
        if (fetch_attr(&s_gpu.attr[0], s_gpu.idx[0], p)) {
            if (p[0] != 0.0f || p[1] != 0.0f || p[2] != 0.0f) {
                s_gpu.nonzero_draws++;
                if (p[0] < s_gpu.min_x) s_gpu.min_x = p[0];
                if (p[0] > s_gpu.max_x) s_gpu.max_x = p[0];
                if (p[1] < s_gpu.min_y) s_gpu.min_y = p[1];
                if (p[1] > s_gpu.max_y) s_gpu.max_y = p[1];
            }
        }
    }

    trace_current_batch();
    raster_batch();

    if (exec_verbose()) {
        static int shown;
        const char *limit = getenv("RECOMP_PB_DRAW_TRACE_LIMIT");
        if (shown++ < (limit ? atoi(limit) : 6)) {
            fprintf(stderr, "  [GPU] prim %u, %u indices, pos attr:"
                            " off 0x%08X type %u size %u stride %u\n",
                    s_gpu.prim, s_gpu.idx_count, s_gpu.attr[0].offset,
                    s_gpu.attr[0].type, s_gpu.attr[0].size, s_gpu.attr[0].stride);
            /* The texture stage, for either kind of batch. This used to print
             * only for inline batches, which meant a title drawing through
             * vertex arrays -- Half-Life 2's menu, for one -- showed no
             * texture state at all, and the reason a quad sampled flat was
             * invisible. */
            fprintf(stderr, "  [GPU]   tex: off 0x%08X %ux%u pitch %u"
                            " colour 0x%02X swizzled %d valid %d\n",
                    s_gpu.tex.offset, s_gpu.tex.width, s_gpu.tex.height,
                    s_gpu.tex.pitch, s_gpu.tex.color,
                    d3d8_format_is_swizzled(s_gpu.tex.color), s_gpu.tex.valid);
            fprintf(stderr, "  [GPU]   diffuse %08X blend %u %04X/%04X eq %04X\n",
                    vertex_color(s_gpu.idx[0]), s_gpu.blend_enable,
                    s_gpu.blend_sfactor, s_gpu.blend_dfactor, s_gpu.blend_equation);
            {
                uint32_t k;
                for (k = 0; k < s_gpu.idx_count && k < 3; k++) {
                    float t[2];
                    if (fetch_texcoord(s_gpu.idx[k], t))
                        fprintf(stderr, "  [GPU]   uv[%u] = %.3f %.3f\n",
                                k, t[0], t[1]);
                }
            }
            /* An inline batch has no guest buffer to go and look at -- the
             * vertices are the payload -- so print the payload too. */
            if (s_gpu.inline_active) {
                uint32_t k;
                fprintf(stderr, "  [GPU]   inline %u dwords:", s_gpu.inline_count);
                for (k = 0; k < s_gpu.inline_count && k < 16; k++)
                    fprintf(stderr, " %08X", s_gpu.inline_buf[k]);
                fprintf(stderr, "\n");
                for (k = 0; k < s_gpu.idx_count && k < 4; k++) {
                    float t[2];
                    if (fetch_texcoord(s_gpu.idx[k], t))
                        fprintf(stderr, "  [GPU]   uv[%u] = %.3f %.3f\n",
                                k, t[0], t[1]);
                }
                fprintf(stderr, "  [GPU]   tex: off 0x%08X %ux%u pitch %u"
                                " colour 0x%02X valid %d\n",
                        s_gpu.tex.offset, s_gpu.tex.width, s_gpu.tex.height,
                        s_gpu.tex.pitch, s_gpu.tex.color, s_gpu.tex.valid);
            }
            {
                /* Every attribute the batch has, not just position. If the
                 * other streams carry data and position does not, the problem
                 * is one buffer rather than the whole vertex path. */
                const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
                uint32_t a, k;
                for (a = 0; a < NV_VERTEX_ATTRS; a++) {
                    const VertexAttr *at = &s_gpu.attr[a];
                    uint32_t nz = 0;
                    if (!at->offset || !at->size)
                        continue;
                    for (k = 0; k < 64; k++)
                        if (mem[at->offset + k]) nz++;
                    fprintf(stderr, "  [GPU]   attr%-2u off 0x%08X type %u"
                                    " size %u stride %-3u  %u/64 bytes set\n",
                            a, at->offset, at->type, at->size, at->stride, nz);
                }
            }
            {
                /* Raw bytes at the array, in case the values read as zero:
                 * that looks the same whether the offset is wrong or the
                 * buffer genuinely has not been filled yet. */
                const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
                uint32_t k;
                fprintf(stderr, "  [GPU]   bytes @0x%08X:", s_gpu.attr[0].offset);
                for (k = 0; k < 32; k++)
                    fprintf(stderr, " %02X", mem[s_gpu.attr[0].offset + k]);
                fprintf(stderr, "\n");
                fprintf(stderr, "  [GPU]   indices:");
                for (k = 0; k < s_gpu.idx_count && k < 8; k++)
                    fprintf(stderr, " %u", s_gpu.idx[k]);
                fprintf(stderr, "\n");
            }
            for (i = 0; i < s_gpu.idx_count && i < 3; i++) {
                if (fetch_attr(&s_gpu.attr[0], s_gpu.idx[i], v))
                    fprintf(stderr, "  [GPU]   v[%u] = %.3f %.3f %.3f %.3f\n",
                            s_gpu.idx[i], v[0], v[1], v[2], v[3]);
            }
        }
    }
}

/* Draw the vertices the title wrote straight into the pushbuffer.
 *
 * INLINE_ARRAY carries no offsets and no indices: the dwords between BEGIN and
 * END *are* the vertex buffer, packed in attribute order using the same
 * SET_VERTEX_DATA_ARRAY_FORMAT registers an ordinary array would use. So the
 * whole batch is describable as a vertex array whose base happens to be that
 * payload, which means synthesising the layout and handing it to the existing
 * path -- rather than a second copy of the topology and rasterisation code.
 *
 * The title's own attribute table is saved and put back: these offsets and
 * strides are ours, and it has not stopped using its.
 *
 * Each attribute aligns to its component width. Byte/short inputs can share
 * a DWORD; float and CMP inputs align to four bytes.
 */
static void draw_inline_array(void)
{
    VertexAttr saved[NV_VERTEX_ATTRS];
    uint32_t off = 0, a, i, vsize, count;

    memcpy(saved, s_gpu.attr, sizeof saved);

    for (a = 0; a < NV_VERTEX_ATTRS; a++) {
        uint32_t bytes, alignment;
        if (!s_gpu.attr[a].size)
            continue;
        switch (s_gpu.attr[a].type) {
        case 0:  bytes = 4; alignment = 1; break;  /* D3DCOLOR */
        case 1:
        case 5:  bytes = 2 * s_gpu.attr[a].size; alignment = 2; break;
        case 2:  bytes = 4 * s_gpu.attr[a].size; alignment = 4; break;
        case 4:  bytes = s_gpu.attr[a].size; alignment = 1; break;
        case 6:  bytes = 4; alignment = 4; break;  /* packed 11/11/10 */
        default: goto out;
        }
        /* Inline attributes align to their component width. Counting shorts
         * as floats or padding every byte/short attribute to a DWORD shifts
         * subsequent inputs and can drop a complete quad as only two vertices.
         * Match the layouts used by xemu and the Mercenaries NV2A backend. */
        off = (off + alignment - 1u) & ~(alignment - 1u);
        s_gpu.attr[a].offset = off;
        off += bytes;
    }
    vsize = off;
    if (!vsize)
        goto out;

    count = (s_gpu.inline_count * 4) / vsize;
    if (count < 3 || count > NV_MAX_INDICES)
        goto out;
    for (a = 0; a < NV_VERTEX_ATTRS; a++)
        if (s_gpu.attr[a].size)
            s_gpu.attr[a].stride = vsize;

    for (i = 0; i < count; i++)
        s_gpu.idx[i] = (uint16_t)i;
    s_gpu.idx_count = count;

    s_gpu.inline_active = 1;
    draw_primitive();
    s_gpu.inline_active = 0;

out:
    memcpy(s_gpu.attr, saved, sizeof saved);
    s_gpu.idx_count = 0;
}

/* Draw the vertices SET_VERTEX3F/4F completed.
 *
 * Same trick as draw_inline_array: rather than a second copy of the topology
 * and rasterisation code, describe what was accumulated as an ordinary vertex
 * array and hand it to the existing path. The layout is ours and fixed, so
 * the attribute table is written out here rather than derived from the
 * title's format registers.
 *
 * The title's own table is saved and put back -- it has not stopped using it.
 *
 * All sixteen attributes retain their standing values, including each UV set.
 */
static void draw_immediate(void)
{
    VertexAttr saved[NV_VERTEX_ATTRS];
    uint32_t i;

    if (s_gpu.imm_count < 3)
        return;

    memcpy(saved, s_gpu.attr, sizeof saved);
    memset(s_gpu.attr, 0, sizeof s_gpu.attr);
    /* Offsets are byte offsets into inline_buf here, not guest addresses --
     * fetch_attr reads them that way while inline_active is set, which is
     * also why 0 is a legal offset for position. Every attribute is a float4;
     * the ones the batch never set carry their standing values, as on the
     * GPU. */
    for (i = 0; i < NV_VERTEX_ATTRS; i++) {
        s_gpu.attr[i].type = 2;
        s_gpu.attr[i].size = 4;
        s_gpu.attr[i].offset = i * 16;
        s_gpu.attr[i].stride = IMM_VERTEX_DWORDS * 4;
    }

    for (i = 0; i < s_gpu.imm_count && i < NV_MAX_INDICES; i++)
        s_gpu.idx[i] = (uint16_t)i;
    s_gpu.idx_count = i;

    /* fetch_attr bounds-checks against inline_count dwords. */
    s_gpu.inline_count = s_gpu.imm_count * IMM_VERTEX_DWORDS;
    s_gpu.inline_active = 1;
    draw_primitive();
    s_gpu.inline_active = 0;

    memcpy(s_gpu.attr, saved, sizeof saved);
    s_gpu.idx_count = 0;
    s_gpu.inline_count = 0;
}

/* A vertex is complete: append all 16 attributes in the layout
 * draw_immediate describes. */
static void imm_emit_vertex(void)
{
    uint32_t at = s_gpu.imm_count * IMM_VERTEX_DWORDS;

    if (!s_gpu.prim || at + IMM_VERTEX_DWORDS > NV_MAX_INLINE)
        return;
    memcpy(&s_gpu.inline_buf[at], s_gpu.imm_attr, sizeof s_gpu.imm_attr);
    s_gpu.imm_count++;
}

/* The immediate-mode writes. Returns 1 if `method` was one of them.
 *
 * Split out because it is a range test against five separate bases, and that
 * reads better than five more cases in an already long switch.
 */
/* The immediate-mode attribute methods (xemu pgraph.c SET_VERTEX_DATA*).
 * Each sets an attribute's current value; completing attribute 0 -- the
 * position -- emits a vertex carrying every attribute as it stands. This used
 * to keep position, diffuse and texcoord 0 only, which dropped any quad with
 * a second texture coordinate: Burnout 3 composites its whole 3D scene into
 * the frame with exactly such a quad, and the frame never got the scene. */
static int imm_vertex_method(uint32_t method, uint32_t param)
{
    union { uint32_t u; float f; } v;
    uint32_t attr, c;
    float *a;
    v.u = param;

    if (method >= NV097_SET_VERTEX4F && method < NV097_SET_VERTEX4F + 16) {
        c = (method - NV097_SET_VERTEX4F) / 4;
        s_gpu.imm_attr[0][c] = v.f;
        s_gpu.imm_used |= 1;
        if (c == 3)
            imm_emit_vertex();
        return 1;
    }
    if (method >= NV097_SET_VERTEX3F && method < NV097_SET_VERTEX3F + 12) {
        c = (method - NV097_SET_VERTEX3F) / 4;
        s_gpu.imm_attr[0][c] = v.f;
        s_gpu.imm_used |= 1;
        if (c == 2) {
            s_gpu.imm_attr[0][3] = 1.0f;
            imm_emit_vertex();
        }
        return 1;
    }
    if (method >= NV097_SET_VERTEX_DATA2F_M
            && method < NV097_SET_VERTEX_DATA2F_M + NV_VERTEX_ATTRS * 8) {
        attr = (method - NV097_SET_VERTEX_DATA2F_M) / 8;
        c = ((method - NV097_SET_VERTEX_DATA2F_M) % 8) / 4;
        a = s_gpu.imm_attr[attr];
        a[c] = v.f;
        if (c == 1) {
            a[2] = 0.0f; a[3] = 1.0f;
        }
        s_gpu.imm_used |= (uint16_t)(1u << attr);
        if (attr == 0 && c == 1)
            imm_emit_vertex();
        return 1;
    }
    if (method >= NV097_SET_VERTEX_DATA4F_M
            && method < NV097_SET_VERTEX_DATA4F_M + NV_VERTEX_ATTRS * 16) {
        attr = (method - NV097_SET_VERTEX_DATA4F_M) / 16;
        c = ((method - NV097_SET_VERTEX_DATA4F_M) % 16) / 4;
        s_gpu.imm_attr[attr][c] = v.f;
        s_gpu.imm_used |= (uint16_t)(1u << attr);
        if (attr == 0 && c == 3)
            imm_emit_vertex();
        return 1;
    }
    if (method >= NV097_SET_VERTEX_DATA2S
            && method < NV097_SET_VERTEX_DATA2S + NV_VERTEX_ATTRS * 4) {
        attr = (method - NV097_SET_VERTEX_DATA2S) / 4;
        a = s_gpu.imm_attr[attr];
        a[0] = (float)(int16_t)(param & 0xFFFF);
        a[1] = (float)(int16_t)(param >> 16);
        a[2] = 0.0f; a[3] = 1.0f;
        s_gpu.imm_used |= (uint16_t)(1u << attr);
        if (attr == 0)
            imm_emit_vertex();
        return 1;
    }
    if (method >= NV097_SET_VERTEX_DATA4UB
            && method < NV097_SET_VERTEX_DATA4UB + NV_VERTEX_ATTRS * 4) {
        /* Bytes in register order, x from the low byte (xemu). */
        attr = (method - NV097_SET_VERTEX_DATA4UB) / 4;
        a = s_gpu.imm_attr[attr];
        a[0] = (float)( param        & 0xFF) / 255.0f;
        a[1] = (float)((param >>  8) & 0xFF) / 255.0f;
        a[2] = (float)((param >> 16) & 0xFF) / 255.0f;
        a[3] = (float)( param >> 24        ) / 255.0f;
        s_gpu.imm_used |= (uint16_t)(1u << attr);
        if (attr == 0)
            imm_emit_vertex();
        return 1;
    }
    if (method >= NV097_SET_VERTEX_DATA4S_M
            && method < NV097_SET_VERTEX_DATA4S_M + NV_VERTEX_ATTRS * 8) {
        attr = (method - NV097_SET_VERTEX_DATA4S_M) / 8;
        c = ((method - NV097_SET_VERTEX_DATA4S_M) % 8) / 4;
        a = s_gpu.imm_attr[attr];
        a[c * 2]     = (float)(int16_t)(param & 0xFFFF);
        a[c * 2 + 1] = (float)(int16_t)(param >> 16);
        s_gpu.imm_used |= (uint16_t)(1u << attr);
        if (attr == 0 && c == 1)
            imm_emit_vertex();
        return 1;
    }
    return 0;
}
static void texture_method(uint32_t method, uint32_t param)
{
    Texture *t = stage_texture((method - NV_TEX_FIRST) / 0x40);
    switch ((method - NV_TEX_FIRST) & 0x3F) {
    case 0x14: s_tex_filter[(method - NV_TEX_FIRST) / 0x40] = param; break;
    case 0x24: s_tex_border[(method - NV_TEX_FIRST) / 0x40] = param; break;
    case 0: t->offset = dma_resolve(param); break;
    case 4:
        t->color = (param >> 8) & 255;
        s_tex_levels[(method - NV_TEX_FIRST) / 0x40] = (param >> 16) & 15;
        if (tex_size_from_format(t->color)) {
            t->width = 1u << ((param >> 20) & 15);
            t->height = 1u << ((param >> 24) & 15);
        }
        break;
    case 8: t->addr_u = param & 15; t->addr_v = (param >> 8) & 15; break;
    case 0x10: t->pitch = param >> 16; break;
    case 0x1C: t->width = param >> 16; t->height = param & 65535; break;
    case 0x20:
        t->palette_offset = dma_resolve(param & 0xFFFFFFC0u);
        t->palette_entries = 256u >> ((param >> 2) & 3u);
        break;
    default: break;
    }
    record_tex_reg(method, param);
}

static int combiner_method(uint32_t method, uint32_t param)
{
    uint32_t *array = NULL;
    unsigned base = 0;
    if (method >= 0x260 && method < 0x280) { array=s_gpu.rc.alpha_icw; base=0x260; }
    else if (method >= 0xAC0 && method < 0xAE0) { array=s_gpu.rc.color_icw; base=0xAC0; }
    else if (method >= 0x1E40 && method < 0x1E60) { array=s_gpu.rc.color_ocw; base=0x1E40; }
    else if (method >= 0xAA0 && method < 0xAC0) { array=s_gpu.rc.alpha_ocw; base=0xAA0; }
    else if (method >= 0xA60 && method < 0xA80) { array=s_gpu.rc.factor0; base=0xA60; }
    else if (method >= 0xA80 && method < 0xAA0) { array=s_gpu.rc.factor1; base=0xA80; }
    if (array) { array[(method-base)/4] = param; return 1; }
    switch (method) {
    case 0x288: s_gpu.rc.final0=param; break;
    case 0x28C: s_gpu.rc.final1=param; break;
    case 0x1E20: s_gpu.rc.final_c0=param; break;
    case 0x1E24: s_gpu.rc.final_c1=param; break;
    case 0x1E60: s_gpu.rc.control=param; s_gpu.rc_seen=1; break;
    case 0x1E70: s_gpu.rc.stage_program=param; break;
    case 0x17F8: s_gpu.shader_clip_plane=param; break;
    case 0x300: s_gpu.alpha_test=param; break;
    case 0x33C: s_gpu.alpha_func=param; break;
    case 0x340: s_gpu.alpha_ref=param; break;
    default: return 0;
    }
    return 1;
}

void nv2a_pb_exec_method(uint32_t subch, uint32_t method, uint32_t param)
{
    static int inited;
    if (!inited) {
        inited = 1;
        s_gpu.min_x = s_gpu.min_y = 1e30f;
        s_gpu.max_x = s_gpu.max_y = -1e30f;
        { unsigned i; for (i = 0; i < NV_VERTEX_ATTRS; i++) s_gpu.imm_attr[i][3] = 1; }
    }
    /* Bring-up: the first parameters each surface method carries. A wrong
     * pitch or clip is indistinguishable from a method never arriving unless
     * the values are visible. */
    if (exec_verbose()) {
        static int shown[8];
        int slot = -1;
        switch (method) {
        case NV097_SET_SURFACE_CLIP_HORIZONTAL: slot = 0; break;
        case NV097_SET_SURFACE_CLIP_VERTICAL:   slot = 1; break;
        case NV097_SET_SURFACE_FORMAT:          slot = 2; break;
        case NV097_SET_SURFACE_PITCH:           slot = 3; break;
        case NV097_SET_SURFACE_COLOR_OFFSET:    slot = 4; break;
        case NV097_SET_COLOR_CLEAR_VALUE:       slot = 5; break;
        case NV097_CLEAR_SURFACE:               slot = 6; break;
        default: break;
        }
        if (slot >= 0 && shown[slot]++ < 4)
            fprintf(stderr, "  [GPU] subch %u method 0x%04X param 0x%08X\n",
                    subch, method, param);
    }

    /* XDK uses the pattern object's color as a progress marker before its
     * semaphore release. Latch the consumed command, not a submitted fence
     * value: D3D polls PGRAPH_PATT_COLOR0 against the semaphore's low bits.
     * The same method on our 3D subchannel has unrelated state semantics. */
    if (subch != 0 && method == 0x0310) {
        nv2a_render_require_flush();
        xbox_Nv2aPatternColor(param);
        return;
    }
    if (subch == 0 && method == 0x1D70) {
        nv2a_render_require_flush();
        xbox_Nv2aSemaphoreRelease(param);
        return;
    }
    /* A nonzero NO_OPERATION is an actual software-method interrupt.
     * XDK D3D uses NOP(5) after WAIT_FOR_IDLE and waits on its handler's event.
     * Zero is padding, with no RAM observer or completion to publish. Keep
     * queued draws together until an actual barrier or scope end. */
    if (subch == 0 && method == 0x0100) {
        if (param) {
            nv2a_render_require_flush();
            xbox_Nv2aSoftwareMethod(param);
        }
        return;
    }
    if (subch != 0) {                      /* 3D class lives on subchannel 0 */
        note_unhandled(method, param);
        return;
    }
    if (method >= NV_TEX_FIRST && method <= NV_TEX_LAST) {
        texture_method(method, param);
        return;
    }
    if (combiner_method(method, param)) return;
    if (nv2a_vp_method(&s_vp, method, param)) return;
    switch (method) {
    case NV097_SET_TRANSFORM_EXEC_MODE:
        s_gpu.xform_mode = param;
        break;
    case NV097_SET_SURFACE_CLIP_HORIZONTAL:
        s_gpu.clip_x = param & 0xFFFF;
        s_gpu.clip_w = (param >> 16) & 0xFFFF;
        break;
    case NV097_SET_SURFACE_CLIP_VERTICAL:
        s_gpu.clip_y = param & 0xFFFF;
        s_gpu.clip_h = (param >> 16) & 0xFFFF;
        break;
    case NV097_SET_SURFACE_FORMAT:
        s_gpu.format = param;
        break;
    case NV097_SET_SURFACE_PITCH:
        s_gpu.pitch = param & 0xFFFF;      /* colour pitch; zeta is the top half */
        s_gpu.zeta_pitch = param >> 16;
        break;
    case NV097_SET_SURFACE_COLOR_OFFSET:
        s_gpu.color_offset = param;
        break;
    case NV097_SET_SURFACE_ZETA_OFFSET: s_gpu.zeta_offset=param; break;
    case NV097_SET_CONTROL0: s_gpu.control0=param; break;
    case NV097_SET_DEPTH_TEST_ENABLE: s_gpu.depth_test=param; break;
    case NV097_SET_DEPTH_FUNC: s_gpu.depth_func=param; break;
    case NV097_SET_DEPTH_MASK: s_gpu.depth_mask=param; break;
    case NV097_SET_ZSTENCIL_CLEAR_VALUE: s_gpu.zstencil_clear=param; break;
    case NV097_SET_CLEAR_RECT_HORIZONTAL:
        s_gpu.clear_rect_h=param; s_gpu.clear_rect_seen|=1; break;
    case NV097_SET_CLEAR_RECT_VERTICAL:
        s_gpu.clear_rect_v=param; s_gpu.clear_rect_seen|=2; break;
    case NV097_SET_COLOR_CLEAR_VALUE:
        s_gpu.clear_color = param;
        break;
    case NV097_CLEAR_SURFACE:
        clear_surface(param);
        break;

    case NV097_SET_BLEND_ENABLE:
        s_gpu.blend_enable = param;
        break;
    case NV097_SET_BLEND_FUNC_SFACTOR:
        s_gpu.blend_sfactor = param;
        break;
    case NV097_SET_BLEND_FUNC_DFACTOR:
        s_gpu.blend_dfactor = param;
        break;
    case NV097_SET_BLEND_COLOR:
        s_gpu.blend_color = param;
        break;
    case NV097_SET_BLEND_EQUATION:
        s_gpu.blend_equation = param;
        break;

    case NV097_SET_BEGIN_END:
        if (param) {
            s_gpu.prim = param;
            s_gpu.idx_count = 0;
            s_gpu.inline_count = 0;
            s_gpu.imm_count = 0;
        } else {
            /* Three ways a batch can have arrived, and only one is in use at
             * a time: vertices completed by SET_VERTEX4F, a payload written
             * with INLINE_ARRAY, or indices into the title's own arrays. */
            if (s_gpu.imm_count)
                draw_immediate();
            else if (s_gpu.inline_count)
                draw_inline_array();
            else
                draw_primitive();
            s_gpu.prim = 0;
            s_gpu.inline_count = 0;
            s_gpu.imm_count = 0;
        }
        break;

    case NV097_INLINE_ARRAY:
        /* Vertex data, not a pointer to it. Buffered rather than decoded here
         * because the format is only fully known at END. */
        if (s_gpu.prim && s_gpu.inline_count < NV_MAX_INLINE)
            s_gpu.inline_buf[s_gpu.inline_count++] = param;
        break;

    case NV097_DRAW_ARRAYS: {
        /* The method this title actually draws with, and the reason the
         * executor reported zero draws while geometry was being submitted the
         * whole time: BEGIN_END arrived, END arrived, and in between came a
         * run description rather than the index list the draw path wanted, so
         * every batch ended with idx_count == 0 and was dropped in silence.
         *
         * Expanded into indices because that is what the rasteriser consumes,
         * and an implicit run is just the indices start..start+count-1. */
        uint32_t start = param & 0x00FFFFFFu;
        uint32_t count = ((param >> 24) & 0xFFu) + 1u;
        uint32_t i;

        if (!s_gpu.prim)
            break;
        for (i = 0; i < count && s_gpu.idx_count < NV_MAX_INDICES; i++)
            s_gpu.idx[s_gpu.idx_count++] = (uint16_t)(start + i);
        break;
    }

    case NV097_ARRAY_ELEMENT16:
        /* Two 16-bit indices per parameter word. */
        if (s_gpu.prim && s_gpu.idx_count + 2 <= NV_MAX_INDICES) {
            s_gpu.idx[s_gpu.idx_count++] = (uint16_t)(param & 0xFFFF);
            s_gpu.idx[s_gpu.idx_count++] = (uint16_t)(param >> 16);
        }
        break;

    case 0x029C: s_fog_mode = param; return;        /* SET_FOG_MODE */
    case 0x02A0: s_fog_gen = param; return;         /* SET_FOG_GEN_MODE */
    case 0x02A4: s_fog_enable = param; return;      /* SET_FOG_ENABLE */
    case 0x02A8: s_fog_color = param; return;       /* SET_FOG_COLOR */
    case 0x09C0: case 0x09C4: case 0x09C8:          /* SET_FOG_PARAMS */
        memcpy(&s_fog_param[(method - 0x09C0) / 4], &param, 4);
        return;
    case NV097_SET_FLIP_READ:
        s_gpu.flip_read = param;
        return;

    case NV097_SET_FLIP_WRITE:
        s_gpu.flip_write = param;
        return;

    case NV097_SET_FLIP_MODULO:
        s_gpu.flip_modulo = param;
        return;

    case NV097_FLIP_INCREMENT_WRITE:
        s_gpu.flip_write = s_gpu.flip_modulo
                         ? (s_gpu.flip_write + 1) % s_gpu.flip_modulo
                         : s_gpu.flip_write + 1;
        s_gpu.flips++;
        return;

    case NV097_FLIP_STALL:
        nv2a_render_require_flush();
        if (s_frame_trace_active) {
            fprintf(stderr, "[FTRACE] end at flip %u\n", s_gpu.flips);
            fflush(stderr);
            s_frame_trace_active = 0;
        }
        /* Hardware stalls while READ_3D == WRITE_3D: the buffer about to be
         * drawn is still on screen. The title's vblank handler advances READ
         * through PGRAPH_INCREMENT, so waiting here is what holds it to its
         * presentation interval. Without pacing (or if no increment arrives
         * within 100 ms, which would otherwise be a hang) the stall ends now,
         * as there is no scanout. */
        if (nv2a_ack_flip_pacing()) {
            ULONGLONG deadline = GetTickCount64() + 100;
            uint64_t stall_start = exec_now();
            for (;;) {
                uint32_t n = nv2a_ack_take_flip_increments();
                while (n--)
                    s_gpu.flip_read = s_gpu.flip_modulo
                                    ? (s_gpu.flip_read + 1) % s_gpu.flip_modulo
                                    : s_gpu.flip_read + 1;
                if (s_gpu.flip_read != s_gpu.flip_write) break;
                if (GetTickCount64() >= deadline) {
                    static unsigned timeouts;
                    if (timeouts++ < 8)
                        fprintf(stderr, "  [GPU] flip stall timed out (read=%u write=%u modulo=%u)\n",
                                s_gpu.flip_read, s_gpu.flip_write, s_gpu.flip_modulo);
                    s_gpu.flip_read = s_gpu.flip_write;
                    break;
                }
                nv2a_ack_tick((volatile void *)((uintptr_t)0xFD000000u + (uintptr_t)xbox_GetMemoryOffset()));
                Sleep(0);
            }
            if (stall_start) exec_stall += exec_now() - stall_start;
        } else
            s_gpu.flip_read = s_gpu.flip_write;
        /* And this is a completed swap, which is what a title's own swap
         * counter counts -- see xbox_Nv2aFrameCounterFlip. */
        xbox_Nv2aFrameCounterFlip();
        exec_flip_report();
        /* color_offset may already name the next target. Copy the last
         * rendered surface before any later clear can overwrite it. */
        if (s_gpu.drawn_offset || s_gpu.color_offset) {
            uint32_t done = s_gpu.drawn_offset ? s_gpu.drawn_offset
                                              : s_gpu.color_offset;
            uint32_t pitch = s_gpu.drawn_offset ? s_gpu.drawn_pitch : s_gpu.pitch;
            /* A resident surface is shown from its GPU copy; RAM would cost
             * a readback of the whole target every flip. */
            static uint8_t *frame;
            static size_t frame_bytes;
            uint32_t fw, fh;
            int shown = 0;
            if (xbox_FramebufferWindowActive(&fw, &fh) && pitch == fw * 4) {
                if (frame_bytes < (size_t)pitch * fh) {
                    free(frame);
                    frame = (uint8_t *)malloc((size_t)pitch * fh);
                    frame_bytes = frame ? (size_t)pitch * fh : 0;
                }
                if (frame && nv2a_render_present((const uint8_t *)xbox_GetMemoryOffset()
                                                 + dma_resolve(done), pitch, fh, frame)) {
                    xbox_FramebufferWindowPresentPixels(frame, pitch);
                    shown = 1;
                }
            }
            if (!shown)
                xbox_FramebufferWindowPresent(dma_resolve(done), pitch);
        }
        if (exec_verbose()) {
            static unsigned n;
            if (n++ < 8) {
                fprintf(stderr, "  [GPU] flip %u: read=%u write=%u\n",
                        s_gpu.flips, s_gpu.flip_read, s_gpu.flip_write);
                fflush(stderr);
            }
        }
        return;


    default:
        if (method >= NV_TEX_FIRST && method <= NV_TEX_LAST)
            record_tex_reg(method, param);
        if (method >= NV097_SET_VERTEX_DATA_ARRAY_OFFSET
                && method < NV097_SET_VERTEX_DATA_ARRAY_OFFSET + NV_VERTEX_ATTRS * 4) {
            /* Resolved here, once, so every consumer -- the rasteriser's
             * attribute reads and the diagnostics alike -- sees the same
             * address. A vertex array offset is a DMA-object offset exactly
             * like a surface offset: physical, and addressable only through
             * the window when it names contiguous memory. */
            s_gpu.attr[(method - NV097_SET_VERTEX_DATA_ARRAY_OFFSET) / 4].offset =
                dma_resolve(param);
        } else if (method >= NV097_SET_VERTEX_DATA_ARRAY_FORMAT
                && method < NV097_SET_VERTEX_DATA_ARRAY_FORMAT + NV_VERTEX_ATTRS * 4) {
            VertexAttr *a = &s_gpu.attr[(method - NV097_SET_VERTEX_DATA_ARRAY_FORMAT) / 4];
            a->type   =  param        & 0x0F;
            a->size   = (param >> 4)  & 0x0F;
            a->stride = (param >> 8)  & 0xFF;
        } else if (!imm_vertex_method(method, param)) {
            note_unhandled(method, param);
        }
        break;
    }
}

/* Find where the title actually wrote its quad.
 *
 * The GPU is pointed at a buffer that stays zero, which says the data went
 * somewhere else -- and the only way to find somewhere else is to look for the
 * data. A screen-space quad for a 640x480 target contains 640.0f and 480.0f as
 * floats, which is a distinctive enough pair to search guest RAM for. Whatever
 * address that turns up is where the title's writes are landing, and the
 * difference from the programmed offset is the bug. */
static void find_quad_vertices(void)
{
    const uint32_t W = 0x44200000u;   /* 640.0f */
    const uint32_t H = 0x43F00000u;   /* 480.0f */
    const uint32_t *ram = (const uint32_t *)xbox_GetMemoryOffset();
    uint32_t i, hits = 0;

    fprintf(stderr, "[GPU] searching guest RAM for 640.0f/480.0f pairs...\n");
    for (i = 0x1000 / 4; i < (0x04000000u / 4) - 8 && hits < 12; i++) {
        if (ram[i] != W && ram[i] != H)
            continue;
        /* Both values within a few words of each other: a lone 640.0f is
         * common, the pair much less so. */
        {
            int has_w = 0, has_h = 0;
            uint32_t k;
            for (k = 0; k < 8; k++) {
                if (ram[i + k] == W) has_w = 1;
                if (ram[i + k] == H) has_h = 1;
            }
            if (!has_w || !has_h)
                continue;
        }
        hits++;
        fprintf(stderr, "  [GPU]   0x%08X:", i * 4);
        {
            uint32_t k;
            for (k = 0; k < 8; k++)
                fprintf(stderr, " %08X", ram[i + k]);
        }
        fprintf(stderr, "\n");
        i += 8;
    }
    if (!hits)
        fprintf(stderr, "  [GPU]   none found -- the quad is not in RAM in"
                        " that form\n");
    fflush(stderr);
}

/* Locate NaN-filled transform matrices in guest RAM.
 *
 * A matrix arriving as NaN says the maths went wrong somewhere upstream, and
 * the only way to find where is to find the matrix and watch who writes it.
 * Three consecutive real-indefinite values is a distinctive enough signature:
 * ordinary data does not contain runs of 0xFFC00000. */
static void find_nan_matrices(void)
{
    const uint32_t NAN_NEG = 0xFFC00000u;
    const uint32_t *ram = (const uint32_t *)xbox_GetMemoryOffset();
    uint32_t i, hits = 0;

    fprintf(stderr, "[GPU] searching guest RAM for NaN matrices...\n");
    for (i = 0x1000 / 4; i < (0x04000000u / 4) - 20 && hits < 10; i++) {
        if (ram[i] != NAN_NEG || ram[i + 1] != NAN_NEG || ram[i + 2] != NAN_NEG)
            continue;
        hits++;
        fprintf(stderr, "  [GPU]   0x%08X:", i * 4);
        {
            uint32_t k;
            for (k = 0; k < 16; k++)
                fprintf(stderr, " %08X", ram[i + k]);
        }
        fprintf(stderr, "\n");
        i += 16;
    }
    if (!hits)
        fprintf(stderr, "  [GPU]   none in RAM -- the NaNs are computed into"
                        " registers, not stored\n");
    fflush(stderr);
}

/* Print guest dwords named by RECOMP_PEEK, as hex and as float.
 *
 * Chasing a value backwards means reading it, and a value that is only wrong
 * for one frame in a thousand cannot be caught by stopping. Both
 * interpretations are printed because the question is usually "is this a
 * pointer or a number", and guessing wrong costs a run. */
static void peek_addresses(void)
{
    const char *spec = getenv("RECOMP_PEEK");
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    char buf[256];
    char *tok, *ctx = NULL;

    if (!spec)
        return;
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    for (tok = strtok_s(buf, ",", &ctx); tok; tok = strtok_s(NULL, ",", &ctx)) {
        uint32_t va = (uint32_t)strtoul(tok, NULL, 0);
        uint32_t v;
        float f;
        if (va < 0x1000u || va >= 0x04000000u)
            continue;
        v = *(const uint32_t *)(mem + va);
        memcpy(&f, &v, 4);
        fprintf(stderr, "  [PEEK] 0x%08X = %08X  (%g)\n", va, v, f);
    }
    fflush(stderr);
}

/* Walk a pointer chain and print every step.
 *
 * RECOMP_PEEK_CHAIN="0x1315A8,8,0x10,0" starts at that address, and for each
 * offset dereferences the current pointer and adds it. Following a chain by
 * hand costs one run per level; this costs one run for the whole chain, and
 * prints where it goes wrong when a level is null.
 */
static void peek_chain(void)
{
    const char *spec = getenv("RECOMP_PEEK_CHAIN");
    const uint8_t *mem = (const uint8_t *)xbox_GetMemoryOffset();
    char buf[256], *tok, *ctx = NULL;
    uint32_t cur = 0;
    int step = 0;

    if (!spec)
        return;
    strncpy(buf, spec, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    for (tok = strtok_s(buf, ",", &ctx); tok; tok = strtok_s(NULL, ",", &ctx)) {
        uint32_t off = (uint32_t)strtoul(tok, NULL, 0);
        if (step == 0) {
            cur = off;
            fprintf(stderr, "  [CHAIN] start 0x%08X\n", cur);
        } else {
            if (cur < 0x1000u || cur + 4 >= 0x04000000u) {
                fprintf(stderr, "  [CHAIN] step %d: 0x%08X is not a guest"
                                " pointer -- chain ends\n", step, cur);
                return;
            }
            cur = *(const uint32_t *)(mem + cur) + off;
            fprintf(stderr, "  [CHAIN] step %d: deref +0x%X -> 0x%08X\n",
                    step, off, cur);
        }
        step++;
    }
    if (cur >= 0x1000u && cur + 4 < 0x04000000u)
        fprintf(stderr, "  [CHAIN] final value at 0x%08X = 0x%08X\n",
                cur, *(const uint32_t *)(mem + cur));
    fflush(stderr);
}

void nv2a_pb_exec_report(void)
{
    peek_addresses();
    peek_chain();
    if (getenv("RECOMP_FIND_NAN")) {
        /* Every report, not once: the matrix is fine early on and only turns
         * to NaN later, so a single scan at startup finds nothing and says
         * nothing. */
        find_nan_matrices();
    }
    if (getenv("RECOMP_FIND_QUAD")) {
        static int done;
        if (!done) { done = 1; find_quad_vertices(); }
    }
    int i, j;

    fprintf(stderr, "[GPU] surface 0x%08X pitch %u clip %ux%u+%u+%u"
                    " clears %u | %u unhandled methods (%d distinct)\n",
            s_gpu.color_offset, s_gpu.pitch, s_gpu.clip_w, s_gpu.clip_h,
            s_gpu.clip_x, s_gpu.clip_y, s_gpu.clears,
            s_gpu.unhandled_total, s_unhandled_count);
    fprintf(stderr, "[GPU] draws %u (%u with coordinates), %u indices;"
                    " x %.1f..%.1f  y %.1f..%.1f\n",
            s_gpu.draws, s_gpu.nonzero_draws, s_gpu.verts,
            s_gpu.min_x, s_gpu.max_x, s_gpu.min_y, s_gpu.max_y);
    /* One picture per report rather than per clear: a title clears hundreds of
     * times a second and nobody wants that many files. */
    dump_surface_bmp();

    /* Drawn and skipped separately: "nothing appeared" and "every batch needed
     * a vertex program we do not run" look identical on screen, and only one
     * of them means the rasteriser is broken. */
    fprintf(stderr, "[GPU] brightest pixel written 0x%08X\n", s_gpu.pixel_max);
    fprintf(stderr, "[GPU] %llu pixels written; draw surface 0x%08X"
                    " -> 0x%08X, clear surface 0x%08X -> 0x%08X\n",
            (unsigned long long)s_gpu.pixels, s_gpu.drawn_offset,
            dma_resolve(s_gpu.drawn_offset), s_gpu.color_offset,
            dma_resolve(s_gpu.color_offset));
    fprintf(stderr, "[GPU] rasterised %u triangles; %u batches skipped as not"
                    " screen-space, %u triangles fully off-surface\n",
            s_gpu.tris_drawn, s_gpu.batches_untransformed,
            s_gpu.tris_skipped_offscreen);

    /* And of the batches that did rasterise, how many sampled anything. A menu
     * that draws its background from one texture and its text from another
     * shows both as flat colour if either half is missing, so the split is
     * what says which half. */
    fprintf(stderr, "[GPU] batches: %u textured, %u with no texcoords,"
                    " %u with texcoords but no usable stage\n",
            s_gpu.batches_textured, s_gpu.batches_no_uv, s_gpu.batches_no_tex);
    for (i = 0; i < s_tex_use_count; i++)
        fprintf(stderr, "  [TEXUSE] 0x%08X %ux%u fmt 0x%02X%s: %u batches\n",
                s_tex_use[i].offset, s_tex_use[i].width, s_tex_use[i].height,
                s_tex_use[i].color,
                d3d8_format_dxt_block_bytes(s_tex_use[i].color) ? " dxt"
                    : d3d8_format_is_swizzled(s_tex_use[i].color) ? " swz" : " lin",
                s_tex_use[i].batches);

    if (getenv("RECOMP_TEX_STATE")) {
        uint32_t k;
        for (k = 0; k < sizeof s_tex_set / sizeof s_tex_set[0]; k++)
            if (s_tex_set[k])
                fprintf(stderr, "  [TEX] 0x%04X = 0x%08X\n",
                        (unsigned)(NV_TEX_FIRST + k * 4), s_tex_reg[k]);
    }

    /* Top ten by frequency: selection sort over a small table, once every few
     * seconds, is not worth a better algorithm.
     *
     * RECOMP_PB_UNHANDLED_ALL lists every one instead. Ten is the right
     * default -- the tail is a long list of state registers nobody needs to
     * read -- but when a title stops and the question is which method it
     * stopped on, the answer is as likely to be the one seen twice as the
     * one seen a thousand times, and ten hides it. */
    {
        int shown = getenv("RECOMP_PB_UNHANDLED_ALL") ? s_unhandled_count : 10;
    for (i = 0; i < shown && i < s_unhandled_count; i++) {
        int best = i;
        for (j = i + 1; j < s_unhandled_count; j++)
            if (s_unhandled[j].count > s_unhandled[best].count)
                best = j;
        if (best != i) {
            PbUnhandled t = s_unhandled[i];
            s_unhandled[i] = s_unhandled[best];
            s_unhandled[best] = t;
        }
        {
            /* The value as well as the count. A method nobody decoded is
             * a guess until you see what it carried: screen coordinates,
             * a 0..1 texcoord and a packed colour are told apart at a
             * glance, and that is what says which vertex encoding a title
             * is using. */
            union { uint32_t u; float f; } v;
            v.u = s_unhandled[i].last_param;
            fprintf(stderr, "  [GPU]   0x%04X x%-8u last=0x%08X (%.4f)\n",
                    s_unhandled[i].method, s_unhandled[i].count,
                    v.u, v.f);
        }
    }
    }    fflush(stderr);
}
