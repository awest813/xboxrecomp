/* Decoded NV2A draw boundary. All calls are synchronous on the DMA consumer.
 * Positions are surface pixels and reciprocal clip W; texture coordinates
 * retain four homogeneous components for each independent texture stage. */
#ifndef XBOXRECOMP_NV2A_RENDER_H
#define XBOXRECOMP_NV2A_RENDER_H
#include <stddef.h>
#include <stdint.h>
#include "nv2a_combiner.h"

typedef struct {
    float position[4];
    float diffuse[4], specular[4], fog[4], tc[4][4];
} Nv2aRenderVertex;

typedef struct {
    const void *source, *palette;
    size_t source_bytes, palette_bytes;
    uint32_t width, height, pitch, format, normalized, addr_u, addr_v;
    /* NV097_SET_TEXTURE_FILTER and _BORDER_COLOR, as the stage holds them. */
    uint32_t filter, border;
    int valid;
    /* Decode the base level using the executor's existing format decoder. */
    int (*decode)(void *opaque, uint32_t *argb);
    void *opaque;
    /* Mip levels stored after the base (1 = base only); source_bytes spans
     * all of them. decode_level decodes level >= 1 at max(1, size >> level). */
    uint32_t levels;
    int (*decode_level)(void *opaque, uint32_t level, uint32_t *argb);
} Nv2aRenderTexture;

typedef struct {
    uint8_t *color, *depth;
    uint32_t pitch, height, color_bpp, depth_pitch, depth_bpp;
    /* SET_SURFACE_FORMAT colour code and layout. A swizzled surface's pitch
     * is its width in bytes; its texels are Morton-ordered in RAM. */
    uint32_t color_format, swizzled;
    uint32_t clip_x, clip_y, clip_w, clip_h;
    uint32_t depth_test, depth_func, depth_mask;
    uint32_t blend_enable, blend_src, blend_dst, blend_equation, blend_color;
    uint32_t alpha_test, alpha_func, alpha_ref, shader_clip_plane;
    Nv2aCombiner combiner;
    int has_combiner;
    Nv2aRenderTexture textures[4];
    const Nv2aRenderVertex *vertices; /* triangle list */
    uint32_t count;
} Nv2aRenderBatch;

/* Shared HLSL constant layout (16-byte rows). Constants do not change shader
 * structure, so animated colors must not cause shader recompilation. */
typedef struct {
    float c0[8][4], c1[8][4], final_c0[4], final_c1[4];
    float surface[4];             /* width, height, depth maximum, binary scale */
    float texture[4][4];          /* width, height, normalized, valid */
    uint32_t control[4];          /* alpha func/ref/enable, clip-plane bits */
} Nv2aRenderConstants;

/* Ordinarily, return 1 after color/depth are committed to guest RAM. Within
 * an explicit scope, 1 means queued; flush/end must succeed before RAM access.
 * A rejected draw does not modify RAM for that draw. */
int nv2a_render_draw(const Nv2aRenderBatch *batch);
/* NV097_CLEAR_SURFACE. flags is the method parameter (Z=1, stencil=2,
 * color 0xF0). Rects are [x0,y0,x1,y1). Returns 1 when the resident GPU
 * surfaces performed the clear (guest RAM catches up on demand), 0 when the
 * caller must clear RAM itself. */
typedef struct {
    uint8_t *color, *depth;
    uint32_t pitch, height, depth_pitch, depth_bpp;
    uint32_t rect[4], depth_rect[4];
    uint32_t flags, color_value, zstencil;
} Nv2aRenderClear;
int nv2a_render_clear(const Nv2aRenderClear *clear);
/* Fill `out` (pitch*height bytes, the surface's own layout) with a display
 * copy of a finished resident surface, from the previous flip's GPU copy.
 * Returns 0 when the caller must read the surface from RAM instead. */
int nv2a_render_present(const uint8_t *color, uint32_t pitch, uint32_t height, uint8_t *out);
/* Single-consumer transaction: the caller excludes CPU/other-device access to
 * target memory until end. Flush before software draws, clears, fences, flips
 * or any other RAM observer. Target changes and texture/palette aliases flush
 * automatically. All pointers must use the caller's canonical RAM mapping;
 * distinct virtual aliases of the same storage cannot be detected here.
 * Scopes cannot nest. A failed flush is fatal to the caller's
 * transaction, never permission to consume stale RAM or signal completion. */
int nv2a_render_begin(void);
int nv2a_render_flush(void);
int nv2a_render_end(void);
int nv2a_render_scoped(void);
/* Executor observers must commit pending pixels before reading or publishing
 * completion. A device/resolve failure terminates instead of exposing stale RAM. */
void nv2a_render_require_flush(void);
/* Identify the executor observer without changing its mandatory RAM commit.
 * Counts include calls with no pending draw, to distinguish traffic from waits. */
typedef enum {
    NV2A_OBSERVER_OTHER, NV2A_OBSERVER_CLEAR, NV2A_OBSERVER_SOFTWARE,
    NV2A_OBSERVER_CAPTURE, NV2A_OBSERVER_PATTERN, NV2A_OBSERVER_SEMAPHORE,
    NV2A_OBSERVER_METHOD, NV2A_OBSERVER_FLIP, NV2A_OBSERVER_COUNT
} Nv2aRenderObserver;
void nv2a_render_observe(Nv2aRenderObserver observer);
void nv2a_render_observer_stats(Nv2aRenderObserver observer,
                                uint64_t *calls, uint64_t *resolves, uint64_t *bytes);
void nv2a_render_memory_read(const void *source, size_t bytes);
int nv2a_render_enabled(void);
/* Residency (RECOMP_GPU_RESIDENT=1) page-protects surfaces in the guest's
 * contiguous window and its tiled alias; the memory layout describes both.
 * Host-kernel I/O on guest buffers must call nv2a_render_host_access first. */
void nv2a_render_set_window(void *contig, void *tiled, size_t tiled_bytes);
void nv2a_render_host_access(void *p, size_t bytes, int write);
void nv2a_render_shutdown(void);
void nv2a_render_stats(uint64_t *draws, uint64_t *fallbacks);
typedef struct {
    uint64_t draws, fallbacks, shader_compiles, texture_uploads, readback_bytes;
    uint64_t surface_uploads, surface_upload_bytes;
    uint64_t texture_cache_bytes;
    uint64_t packed_readbacks;
} Nv2aRenderStats;
void nv2a_render_get_stats(Nv2aRenderStats *stats);

/* Pure source generation, also used by offline WARP differential tests. */
int nv2a_render_pixel_shader(const Nv2aCombiner *rc, int has_combiner,
                              char *out, size_t bytes);
extern const char nv2a_render_vertex_shader[];
#endif
