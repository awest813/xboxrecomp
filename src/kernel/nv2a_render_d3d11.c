/* D3D11 rasterization of decoded NV2A triangles. Ordinary draws detect CPU
 * surface edits and commit results synchronously. Explicit scopes coalesce
 * readbacks only while their caller excludes other access to target RAM.
 * No second command parser, register model or presentation window is needed. */
#include "nv2a_render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint64_t accepted, fallback;
static uint64_t shader_compiles, texture_uploads, readback_bytes;
static uint64_t surface_uploads, surface_upload_bytes;
static uint64_t texture_cache_bytes;
static uint64_t packed_readbacks;
/* Single-consumer diagnostics: count actual pending resolves by their caller,
 * without per-draw logging or changing the transaction's barriers. */
static uint64_t flush_total,flush_scope_end,flush_external,flush_memory_alias;
static uint64_t flush_target,flush_texture_alias,flush_unscoped,flush_fallback;
static uint64_t observer_calls[NV2A_OBSERVER_COUNT],observer_resolves[NV2A_OBSERVER_COUNT];
static uint64_t observer_bytes[NV2A_OBSERVER_COUNT];
/* MAP-visible switch for same-process control/candidate comparisons. No
 * pointer, allocation or RAM-publication contract depends on this choice. */
static volatile uint32_t s_triangle_bounds=1;
static uint64_t bounds_excluded_triangles;
void nv2a_render_observer_stats(Nv2aRenderObserver observer,
                                uint64_t *calls,uint64_t *resolves,uint64_t *bytes)
{
    unsigned i=(unsigned)observer;
    if(i>=NV2A_OBSERVER_COUNT)i=NV2A_OBSERVER_OTHER;
    if(calls)*calls=observer_calls[i];
    if(resolves)*resolves=observer_resolves[i];
    if(bytes)*bytes=observer_bytes[i];
}
void nv2a_render_stats(uint64_t *draws,uint64_t *misses)
{ if(draws)*draws=accepted;if(misses)*misses=fallback; }
void nv2a_render_get_stats(Nv2aRenderStats *stats)
{
    stats->draws=accepted;stats->fallbacks=fallback;stats->shader_compiles=shader_compiles;
    stats->texture_uploads=texture_uploads;stats->readback_bytes=readback_bytes;
    stats->surface_uploads=surface_uploads;stats->surface_upload_bytes=surface_upload_bytes;
    stats->texture_cache_bytes=texture_cache_bytes;
    stats->packed_readbacks=packed_readbacks;
}

#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include "../d3d/d3d8_swizzle.h"

/* Opt-in wall-clock diagnostics, read through the matching executable MAP.
 * Renderer calls have one consumer. Nested draw/flush totals overlap; the
 * submit/map/commit counters partition readback work. No logging or GPU waits
 * are added, and the default path does not call QueryPerformanceCounter. */
static uint64_t timing_frequency,timing_draw,timing_prepare,timing_surface;
static uint64_t timing_texture,timing_compare,timing_dispatch,timing_flush;
static uint64_t timing_submit,timing_map,timing_commit;
static uint64_t timing_tex_lookup,timing_tex_compare,timing_tex_decode,timing_tex_upload;
static uint64_t tex_hits,tex_changes,tex_misses,tex_compare_requested_bytes,tex_decoded_pixels;
static uint64_t tex_pixels_by_format[128],tex_uploads_by_format[128];
/* End, external observer, RAM alias, target, texture alias, unscoped,
 * fallback, and direct API flush. Counts include successful commits only. */
static uint64_t readback_calls_by_reason[8],readback_pixels_by_reason[8];
static uint64_t readback_map_by_reason[8],readback_ticks_by_reason[8];
static uint64_t readback_area_bins[32];
static unsigned readback_reason=7;
static int timing_enabled;
static uint64_t timing_now(void)
{
    LARGE_INTEGER t;
    if(!timing_enabled)return 0;
    QueryPerformanceCounter(&t);return (uint64_t)t.QuadPart;
}
static void timing_add(uint64_t *counter,uint64_t start)
{ if(start)*counter+=timing_now()-start; }

#define RELEASE(p) do { if(p){(p)->lpVtbl->Release(p);(p)=NULL;} } while(0)
#define SHADER_SLOTS 128
#define TEXTURE_SLOTS 128
#define TEXTURE_BUDGET (128u*1024u*1024u)
#define VERTEX_RING_BYTES (16u*1024u*1024u)
typedef struct {
    Nv2aCombiner key;
    int has_combiner, valid;
    uint64_t age;
    ID3D11PixelShader *shader;
} Shader;
typedef struct {
    Nv2aRenderTexture key;
    uint8_t *shadow, *palette;
    uint32_t watch_lo, watch_hi;    /* protected source pages while watched */
    int watched, dirty;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *view;
    uint64_t age;
    size_t bytes;
} Texture;
/* One render target: GPU surfaces, the shadows that record what RAM held
 * when GPU and RAM last agreed, and its residency. The bound target is
 * gpu.s; resident mode parks others so switching targets needs no readback. */
typedef struct {
    ID3D11Texture2D *color, *color_read;
    ID3D11RenderTargetView *rtv;
    ID3D11Texture2D *depth, *depth_upload, *depth_read;
    ID3D11DepthStencilView *dsv;
    ID3D11ShaderResourceView *color_view,*depth_view;
    ID3D11Buffer *pack_result,*pack_read;
    ID3D11UnorderedAccessView *pack_uav;
    uint32_t width,height,depth_bpp;
    uint8_t *depth_pixels;
    uint8_t *color_shadow, *depth_shadow;
    const uint8_t *color_source, *depth_source;
    uint32_t depth_pitch;
    int color_valid, depth_valid;
    /* Residency (RECOMP_GPU_RESIDENT=1): committed pixels newer than guest
     * RAM. res_box/res_batch describe what a readback must still copy. */
    int res_valid, res_depth;
    D3D11_BOX res_box;
    Nv2aRenderBatch res_batch;
    /* Tracked page ranges [lo,hi) of the color (0) and depth (1) surfaces;
     * empty when untracked. A page is tracked by at most one set. */
    uint32_t rng_lo[2], rng_hi[2];
    uint64_t age;
    int used;
    /* RAM layout: bytes per pixel and Morton order. 0 means not yet set
     * (plain B8G8R8A8 rows). Others are converted on upload/readback; the
     * GPU copy is always linear B8G8R8A8. */
    uint32_t cbpp, cswz, cformat;
} SurfaceSet;
#define CONVERTED(t) ((t)->cbpp && ((t)->cbpp!=4 || (t)->cswz))
static struct {
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11DeviceContext1 *context1;   /* ClearView, when available */
    ID3D11VertexShader *vs;
    ID3D11InputLayout *layout;
    ID3D11Buffer *vertices, *constants;
    size_t vertex_bytes, vertex_used;
    ID3D11RasterizerState *rasterizer;
    ID3D11SamplerState *samplers[4];
    ID3D11BlendState *blend;
    D3D11_BLEND_DESC blend_desc;
    ID3D11DepthStencilState *ds;
    D3D11_DEPTH_STENCIL_DESC ds_desc;
    SurfaceSet s;                      /* the bound target */
    ID3D11ComputeShader *pack_shader;
    ID3D11Buffer *pack_constants;
    int pack_requested,pack_failed;
    int scope, pending, pending_depth;
    Nv2aRenderBatch pending_batch;
    D3D11_BOX pending_box;
    Shader shaders[SHADER_SLOTS];
    Texture textures[TEXTURE_SLOTS];
    uint64_t age;
    int enabled, initialized;
} gpu;

/* ── GPU residency ────────────────────────────────────────────
 *
 * The synchronous design copies every committed batch back to guest RAM, so
 * each fence, semaphore, scope end and unscoped draw waits for the GPU to
 * drain (Map) and every next batch memcmps whole surfaces to find CPU edits.
 * In MVP 06's pitch scene that was ~65 readbacks and ~65 full-surface
 * compares per flip, and the command worker spent most of its time there.
 *
 * Residency keeps committed pixels on the GPU and lets the MMU say when
 * anyone looks: the current target's pages are PAGE_NOACCESS while the GPU
 * copy is newer, PAGE_READONLY while RAM, shadow and GPU agree. A read fault
 * materializes (reads back the dirty box); a write fault also marks the
 * surface CPU-edited so the next batch compares and uploads as before. Every
 * observer -- lifted code, the presenter, software rasterization, DMA --
 * is covered without being enumerated. Host-kernel I/O does not fault, so
 * NtReadFile/NtWriteFile announce their buffers (xbox_MemoryHostAccess).
 *
 * Surfaces live in the contiguous window, which the tiled aperture aliases;
 * protection is per view, so every change is applied to both. Batches that
 * target memory outside the window keep the synchronous path. */
#define RES_PAGE 4096u
#define RES_WINDOW (64u*1024u*1024u)
#define RES_PAGES (RES_WINDOW/RES_PAGE)
enum { RES_RW, RES_RO, RES_NA };
static int resident_mode; static int filtered_sampling;
static CRITICAL_SECTION render_cs;
static uint8_t *res_contig, *res_tiled;
static size_t res_tiled_bytes;
static uint8_t page_cur[RES_PAGES];
/* env_* bounds pages that may be non-RW in any surface set. */
static uint32_t env_lo, env_hi, surf_env_lo, surf_env_hi;
static int res_writing;
static uint16_t page_watch[RES_PAGES];
static uint32_t watch_lo, watch_hi;
static int texture_watching;
static uint64_t watch_hits, watch_dirtied;

static uint64_t res_commits, res_materializes, res_read_faults, res_write_faults;
static uint64_t res_protects, res_host_access, res_untracked_compares, res_clears, res_adopts;
static uint64_t cache_hits, cache_misses, cache_evictions, direct_textures, depth_transfers, snapshot_copies, depth_texels;
static uint64_t res_synced_clears;
/* Diagnostics: why pixels were materialized, who faulted, which targets
 * alternate. Fixed tables, no allocation; printed with the periodic line. */
enum { MR_FAULT, MR_MEMREAD, MR_TEXTURE, MR_RETARGET, MR_RELEASE, MR_FAIL, MR_HOST, MR_OTHER, MR_COUNT };
static unsigned mat_reason=MR_OTHER;
static struct { uint32_t source, width, height, pitch, format, set_width, set_height, why; uint64_t count; } texture_misses[16];
static struct { unsigned line; uint64_t count; } fail_lines[16];
static void note_fail(unsigned line)
{
    unsigned i;
    for(i=0;i<16;i++) {
        if(fail_lines[i].count && fail_lines[i].line==line){fail_lines[i].count++;return;}
        if(!fail_lines[i].count){fail_lines[i].line=line;fail_lines[i].count=1;return;}
    }
}
#define DRAW_FAIL do { note_fail(__LINE__);goto fail; } while(0)
static uint64_t mat_count[MR_COUNT], mat_pixels[MR_COUNT];
static struct { uint64_t rip, count; } fault_sites[24];
static struct { uint32_t color, depth, width, height; uint64_t count; } switch_targets[24];
static void note_site(uint64_t rip)
{
    unsigned i;
    for(i=0;i<24;i++) {
        if(fault_sites[i].rip==rip){fault_sites[i].count++;return;}
        if(!fault_sites[i].count){fault_sites[i].rip=rip;fault_sites[i].count=1;return;}
    }
}
static void note_target(uint32_t color,uint32_t depth,uint32_t width,uint32_t height)
{
    unsigned i;
    for(i=0;i<24;i++) {
        if(switch_targets[i].count && switch_targets[i].color==color && switch_targets[i].depth==depth
           && switch_targets[i].width==width && switch_targets[i].height==height) { switch_targets[i].count++;return; }
        if(!switch_targets[i].count) {
            switch_targets[i].color=color;switch_targets[i].depth=depth;
            switch_targets[i].width=width;switch_targets[i].height=height;switch_targets[i].count=1;return;
        }
    }
}
static void resident_report(void)
{
    static const char *const names[MR_COUNT]={"fault","memread","texture","retarget","release","fail","host","other"};
    unsigned i;
    fprintf(stderr,"[D3D11] surface cache hits=%llu misses=%llu evictions=%llu depth-transfers=%llu\n",
        (unsigned long long)cache_hits,(unsigned long long)cache_misses,(unsigned long long)cache_evictions,
        (unsigned long long)depth_transfers);
    fprintf(stderr,"[D3D11] depth textures=%llu watched-texture hits=%llu dirtied=%llu\n",
        (unsigned long long)depth_texels,(unsigned long long)watch_hits,(unsigned long long)watch_dirtied);
    fprintf(stderr,"[D3D11] direct render-target textures=%llu snapshots=%llu\n[D3D11] texture materializes (src fmt WxH/pitch set why:n):",
        (unsigned long long)direct_textures,(unsigned long long)snapshot_copies);
    for(i=0;i<16 && texture_misses[i].count;i++)fprintf(stderr," %x %x %ux%u/%u %ux%u %u:%llu",
        texture_misses[i].source,texture_misses[i].format,texture_misses[i].width,texture_misses[i].height,
        texture_misses[i].pitch,texture_misses[i].set_width,texture_misses[i].set_height,texture_misses[i].why,
        (unsigned long long)texture_misses[i].count);
    fprintf(stderr,"\n[D3D11] draw failures by line:");
    for(i=0;i<16 && fail_lines[i].count;i++)fprintf(stderr," %u:%llu",fail_lines[i].line,(unsigned long long)fail_lines[i].count);
    fprintf(stderr,"\n");
    fprintf(stderr,"[D3D11] materialize by reason:");
    for(i=0;i<MR_COUNT;i++)if(mat_count[i])fprintf(stderr," %s=%llu/%lluKpx",names[i],
        (unsigned long long)mat_count[i],(unsigned long long)(mat_pixels[i]>>10));
    fprintf(stderr,"\n[D3D11] fault sites (exe+offset):");
    for(i=0;i<24 && fault_sites[i].count;i++)fprintf(stderr," %llx:%llu",
        (unsigned long long)fault_sites[i].rip,(unsigned long long)fault_sites[i].count);
    fprintf(stderr,"\n[D3D11] switched-to targets (color/depth offsets):");
    for(i=0;i<24 && switch_targets[i].count;i++)fprintf(stderr," %x/%x %ux%u:%llu",
        switch_targets[i].color,switch_targets[i].depth,switch_targets[i].width,switch_targets[i].height,
        (unsigned long long)switch_targets[i].count);
    fprintf(stderr,"\n");
}

static int contig_offset(const void *p,size_t n,uint32_t *off)
{
    uintptr_t a=(uintptr_t)p,base=(uintptr_t)res_contig,tiled=(uintptr_t)res_tiled;
    if(!res_contig || !p)return 0;
    if(a>=base && a-base<RES_WINDOW && n<=RES_WINDOW-(a-base)) { *off=(uint32_t)(a-base);return 1; }
    if(res_tiled_bytes && a>=tiled && a-tiled<res_tiled_bytes && n<=res_tiled_bytes-(a-tiled)) {
        *off=(uint32_t)(a-tiled);return 1;
    }
    return 0;
}
static int page_range(const void *p,size_t n,uint32_t *lo,uint32_t *hi)
{
    uint32_t off;
    if(!n || !contig_offset(p,n,&off))return 0;
    *lo=off/RES_PAGE;*hi=(uint32_t)(((uint64_t)off+n+RES_PAGE-1)/RES_PAGE);
    return 1;
}
/* Parked targets (resident mode). The set being read back opens its own
 * pages through res_writing, which names it. */
#define SURFACE_SLOTS 12
static SurfaceSet parked[SURFACE_SLOTS];
static SurfaceSet *set_at(unsigned i) { return i?&parked[i-1]:&gpu.s; }
static int has_range(const SurfaceSet *t,unsigned which) { return t->rng_hi[which]>t->rng_lo[which]; }
static void protect_run(uint32_t first,uint32_t end,unsigned state)
{
    static const DWORD prot[3]={PAGE_READWRITE,PAGE_READONLY,PAGE_NOACCESS};
    size_t at=(size_t)first*RES_PAGE,bytes=(size_t)(end-first)*RES_PAGE;
    DWORD old;
    if(!VirtualProtect(res_contig+at,bytes,prot[state],&old)) {
        static unsigned shown;
        if(shown++<8)fprintf(stderr,"[D3D11] resident protect %u failed: %lu\n",state,GetLastError());
        return;
    }
    if(at<res_tiled_bytes) {
        size_t alias=bytes<res_tiled_bytes-at?bytes:res_tiled_bytes-at;
        VirtualProtect(res_tiled+at,alias,prot[state],&old);
    }
    memset(page_cur+first,(int)state,end-first);
    res_protects++;
}
/* Bring every page that may be protected to its required state, in runs.
 * Required states are painted range by range (each set contributes its two
 * ranges once), then compared with the current protection over the span. */
static uint8_t page_req[RES_PAGES];
static void apply(void)
{
    uint32_t lo=env_lo,hi=env_hi,p,run=0;
    unsigned run_state=0,i,r;
    int open=0;
    for(i=0;i<=SURFACE_SLOTS;i++)for(r=0;r<2;r++) {
        const SurfaceSet *t=set_at(i);
        if((i && !t->used) || !has_range(t,r))continue;
        if(hi<=lo){lo=t->rng_lo[r];hi=t->rng_hi[r];}
        else { if(t->rng_lo[r]<lo)lo=t->rng_lo[r];if(t->rng_hi[r]>hi)hi=t->rng_hi[r]; }
    }
    if(watch_hi>watch_lo) {
        if(hi<=lo){lo=watch_lo;hi=watch_hi;}
        else { if(watch_lo<lo)lo=watch_lo;if(watch_hi>hi)hi=watch_hi; }
    }
    if(hi<=lo)return;
    memset(page_req+lo,RES_RW,hi-lo);
    for(p=watch_lo;p<watch_hi;p++)if(page_watch[p])page_req[p]=RES_RO;
    for(i=0;i<=SURFACE_SLOTS;i++) {
        const SurfaceSet *t=set_at(i);
        if((i && !t->used) || (res_writing && t==&gpu.s))continue;
        for(r=0;r<2;r++) {
            unsigned state=r?(t->res_valid && t->res_depth?RES_NA:RES_RO):(t->res_valid?RES_NA:RES_RO);
            if(!has_range(t,r))continue;
            for(p=t->rng_lo[r];p<t->rng_hi[r];p++)if(page_req[p]<state)page_req[p]=(uint8_t)state;
        }
    }
    for(p=lo;p<=hi;p++) {
        unsigned want=p<hi?page_req[p]:0;
        int change=p<hi && want!=page_cur[p];
        if(open && (!change || want!=run_state)) { protect_run(run,p,run_state);open=0; }
        if(change && !open) { run=p;run_state=want;open=1; }
    }
    env_lo=env_hi=0;
    for(i=0;i<=SURFACE_SLOTS;i++)for(r=0;r<2;r++) {
        const SurfaceSet *t=set_at(i);
        if((i && !t->used) || !has_range(t,r))continue;
        if(env_hi<=env_lo){env_lo=t->rng_lo[r];env_hi=t->rng_hi[r];}
        else { if(t->rng_lo[r]<env_lo)env_lo=t->rng_lo[r];if(t->rng_hi[r]>env_hi)env_hi=t->rng_hi[r]; }
    }
    /* Vertex/command reads only need surface pages (see memory_read). */
    surf_env_lo=env_lo;surf_env_hi=env_hi;
    /* Pages released by unwatched textures fall inside the watch envelope,
     * which only grows, so they are always revisited. */
    if(watch_hi>watch_lo) {
        if(env_hi<=env_lo){env_lo=watch_lo;env_hi=watch_hi;}
        else { if(watch_lo<env_lo)env_lo=watch_lo;if(watch_hi>env_hi)env_hi=watch_hi; }
    }
}
static void untrack(unsigned which) { gpu.s.rng_lo[which]=gpu.s.rng_hi[which]=0; }
/* Texture write watching (resident mode). Every draw used to memcmp each
 * bound texture's whole source against its shadow -- tens of MB a frame.
 * A watched texture's source pages are PAGE_READONLY (painted by apply()
 * alongside surface states), so a write faults and marks it dirty; a clean,
 * watched texture is current without reading it. Pages tracked by a surface
 * set are never watched, and claiming one dirties overlapping textures. */
static void resident_initialize(void);
static void resident_release(void);

static int error(HRESULT hr,const char *where)
{
    static unsigned shown;
    if(SUCCEEDED(hr))return 0;
    if(shown++<8)fprintf(stderr,"[D3D11] %s failed: 0x%08lX; software fallback\n",where,(unsigned long)hr);
    return 1;
}
static ID3DBlob *compile(const char *source,const char *profile)
{
    ID3DBlob *code=NULL,*diagnostics=NULL;
    HRESULT hr=D3DCompile(source,strlen(source),"nv2a decoded draw",NULL,NULL,
        "main",profile,D3DCOMPILE_IEEE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&diagnostics);
    if(FAILED(hr)) {
        static unsigned shown;
        if(shown++<4)fprintf(stderr,"[D3D11] shader: %.1500s\n",diagnostics?
            (const char*)ID3D10Blob_GetBufferPointer(diagnostics):"compile failed");
    }
    RELEASE(diagnostics);
    return code;
}
static int initialize(void)
{
    static const D3D11_INPUT_ELEMENT_DESC elements[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,32,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",2,DXGI_FORMAT_R32G32B32A32_FLOAT,0,48,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,64,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32B32A32_FLOAT,0,80,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",2,DXGI_FORMAT_R32G32B32A32_FLOAT,0,96,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",3,DXGI_FORMAT_R32G32B32A32_FLOAT,0,112,D3D11_INPUT_PER_VERTEX_DATA,0}
    };
    const char *mode;
    D3D_DRIVER_TYPE driver;
    D3D_FEATURE_LEVEL level, requested=D3D_FEATURE_LEVEL_11_0;
    D3D11_BUFFER_DESC bd={0};
    D3D11_RASTERIZER_DESC rd={0};
    ID3DBlob *code;
    HRESULT hr;
    unsigned i;
    if(gpu.initialized)return gpu.enabled;
    gpu.initialized=1;
    mode=getenv("RECOMP_GPU_TIMING");
    timing_enabled=mode && *mode=='1';
    if(timing_enabled) {
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);timing_frequency=(uint64_t)frequency.QuadPart;
    }
    mode=getenv("RECOMP_RENDERER");
    if(!mode || (strcmp(mode,"d3d11") && strcmp(mode,"warp")))return 0;
    driver=!strcmp(mode,"warp")?D3D_DRIVER_TYPE_WARP:D3D_DRIVER_TYPE_HARDWARE;
    hr=D3D11CreateDevice(NULL,driver,NULL,0,&requested,1,D3D11_SDK_VERSION,
        &gpu.device,&level,&gpu.context);
    if(error(hr,"create device"))return 0;
    code=compile(nv2a_render_vertex_shader,"vs_5_0");
    if(!code)return 0;
    hr=ID3D11Device_CreateVertexShader(gpu.device,ID3D10Blob_GetBufferPointer(code),
        ID3D10Blob_GetBufferSize(code),NULL,&gpu.vs);
    if(!error(hr,"vertex shader"))hr=ID3D11Device_CreateInputLayout(gpu.device,elements,8,
        ID3D10Blob_GetBufferPointer(code),ID3D10Blob_GetBufferSize(code),&gpu.layout);
    RELEASE(code);
    if(error(hr,"input layout"))return 0;
    bd.ByteWidth=sizeof(Nv2aRenderConstants);bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.constants),"constant buffer"))return 0;
    rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;
    rd.ScissorEnable=TRUE;rd.DepthClipEnable=FALSE;
    if(error(ID3D11Device_CreateRasterizerState(gpu.device,&rd,&gpu.rasterizer),"rasterizer"))return 0;
    for(i=0;i<4;i++) {
        D3D11_SAMPLER_DESC sd={0};
        sd.Filter=D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU=(i&1)?D3D11_TEXTURE_ADDRESS_WRAP:D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressV=(i&2)?D3D11_TEXTURE_ADDRESS_WRAP:D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        if(error(ID3D11Device_CreateSamplerState(gpu.device,&sd,&gpu.samplers[i]),"sampler"))return 0;
    }
    {
        /* IID_ID3D11DeviceContext1, spelled out to avoid linking dxguid. */
        static const GUID iid={0xbb2c6faa,0xb5fb,0x4082,{0x8e,0x6b,0x38,0x8b,0x8c,0xfa,0x90,0xe1}};
        if(FAILED(ID3D11DeviceContext_QueryInterface(gpu.context,&iid,(void**)&gpu.context1)))
            gpu.context1=NULL;
    }
    mode=getenv("RECOMP_TEXTURE_FILTER");
    filtered_sampling=!mode || strcmp(mode,"0");
    InitializeCriticalSection(&render_cs);
    gpu.s.used=1;
    gpu.enabled=1;
    resident_initialize();
    mode=getenv("RECOMP_PACKED_READBACK");
    gpu.pack_requested=mode && !strcmp(mode,"1");
    mode=getenv("RECOMP_RENDERER");
    fprintf(stderr,"[D3D11] decoded rasterizer active (%s), coherent per-batch readback\n",mode);
    if(gpu.pack_requested)fprintf(stderr,"[D3D11] packed color/depth readback requested\n");
    return 1;
}
int nv2a_render_enabled(void) { return initialize(); }

static void release_surfaces(void)
{
    RELEASE(gpu.s.pack_uav);RELEASE(gpu.s.pack_result);RELEASE(gpu.s.pack_read);
    RELEASE(gpu.s.color_view);RELEASE(gpu.s.depth_view);
    RELEASE(gpu.s.rtv);RELEASE(gpu.s.color);RELEASE(gpu.s.color_read);
    RELEASE(gpu.s.dsv);RELEASE(gpu.s.depth);RELEASE(gpu.s.depth_upload);RELEASE(gpu.s.depth_read);
    free(gpu.s.depth_pixels);gpu.s.depth_pixels=NULL;
    free(gpu.s.color_shadow);gpu.s.color_shadow=NULL;
    free(gpu.s.depth_shadow);gpu.s.depth_shadow=NULL;
    gpu.s.color_valid=gpu.s.depth_valid=0;
    gpu.s.color_source=gpu.s.depth_source=NULL;gpu.s.depth_pitch=0;
    gpu.s.width=gpu.s.height=gpu.s.depth_bpp=0;
}
static int surfaces(uint32_t width,uint32_t height,uint32_t depth_bpp)
{
    D3D11_TEXTURE2D_DESC td={0};
    D3D11_DEPTH_STENCIL_VIEW_DESC dd={0};
    /* Disabling depth changes the draw state, not the color allocation.
     * Keep a compatible depth allocation for the next depth-enabled draw;
     * that draw still revalidates RAM through depth_valid/changed_bounds. */
    /* Height is the batch's clip extent, not a surface property: MVP 06
     * draws the same 256-wide targets as 256 and 512 rows. In resident
     * mode one allocation, as tall as any use, serves them all. */
    if(gpu.s.width==width && (gpu.s.height==height || (resident_mode && gpu.s.height>height))
       && (!depth_bpp || gpu.s.depth_bpp==depth_bpp))return 1;
    if(resident_mode && gpu.s.width==width && gpu.s.height>height)height=gpu.s.height;
    /* Resident pixels exist only in the surfaces about to be released. */
    resident_release();
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    release_surfaces();
    td.Width=width;td.Height=height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.s.color),"color surface"))return 0;
    if(error(ID3D11Device_CreateRenderTargetView(gpu.device,(ID3D11Resource*)gpu.s.color,NULL,&gpu.s.rtv),"color view"))return 0;
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.s.color_read),"color readback"))return 0;
    gpu.s.color_shadow=(uint8_t*)malloc((size_t)width*height*4);
    if(!gpu.s.color_shadow)return 0;
    if(depth_bpp) {
        /* Store integer guest Z as an exactly representable power-of-two
         * fraction. Host D24 UNORM conversion/division can shift a high Z24
         * integer by one or two, changing EQUAL/LESS at adjacent values.
         * D32_FLOAT preserves all 24 guest bits. Stencil stays in guest RAM. */
        td.Format=DXGI_FORMAT_R32_TYPELESS;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.s.depth_read),"depth readback"))return 0;
        td.Usage=D3D11_USAGE_DEFAULT;td.CPUAccessFlags=0;
        td.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.s.depth),"depth surface"))return 0;
        dd.Format=DXGI_FORMAT_D32_FLOAT;
        dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        if(error(ID3D11Device_CreateDepthStencilView(gpu.device,(ID3D11Resource*)gpu.s.depth,&dd,&gpu.s.dsv),"depth view"))return 0;
        td.BindFlags=0;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.s.depth_upload),"depth upload"))return 0;
        gpu.s.depth_pixels=(uint8_t*)malloc((size_t)width*height*4);
        gpu.s.depth_shadow=(uint8_t*)malloc((size_t)width*height*depth_bpp);
        if(!gpu.s.depth_pixels || !gpu.s.depth_shadow)return 0;
    }
    gpu.s.width=width;gpu.s.height=height;gpu.s.depth_bpp=depth_bpp;
    return 1;
}

/* A conservative screen-space union. Positive clip W keeps a clipped
 * triangle inside its projected vertex bounds. Include a pixel on each side
 * for fixed-point raster snapping. Unusual/nonfinite W uses software. */
static int draw_bounds(const Nv2aRenderBatch *b,uint32_t width,uint32_t dwidth,
                       D3D11_BOX *box)
{
    float left=INFINITY,top=INFINITY,right=-INFINITY,bottom=-INFINITY;
    float clip_right=fminf((float)width,(float)b->clip_x+(float)b->clip_w);
    float clip_bottom=fminf((float)b->height,(float)b->clip_y+(float)b->clip_h);
    unsigned i,j;
    if(dwidth && clip_right>(float)dwidth)clip_right=(float)dwidth;
    if(!s_triangle_bounds)for(i=0;i<b->count;i++) {
        const float *p=b->vertices[i].position;
        if(!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])
            || !isfinite(p[3]) || p[3]<=0)return -1;
        left=fminf(left,p[0]);top=fminf(top,p[1]);
        right=fmaxf(right,p[0]);bottom=fmaxf(bottom,p[1]);
    }
    else for(i=0;i<b->count;i+=3) {
        float l=INFINITY,t=INFINITY,r=-INFINITY,bottom_y=-INFINITY;
        for(j=0;j<3;j++) {
            const float *p=b->vertices[i+j].position;
            if(!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2])
               || !isfinite(p[3]) || p[3]<=0)return -1;
            l=fminf(l,p[0]);t=fminf(t,p[1]);
            r=fmaxf(r,p[0]);bottom_y=fmaxf(bottom_y,p[1]);
        }
        /* Disconnected triangles outside opposite viewport edges must not
         * inflate the readback union across untouched RAM. Retain the same
         * one-pixel snapping margin, and validate even discarded vertices. */
        if(r<(float)b->clip_x-1 || l>clip_right+1
            || bottom_y<(float)b->clip_y-1 || t>clip_bottom+1) {
            bounds_excluded_triangles++;continue;
        }
        left=fminf(left,l);top=fminf(top,t);
        right=fmaxf(right,r);bottom=fmaxf(bottom,bottom_y);
    }
    left=fmaxf((float)b->clip_x,floorf(left)-1);
    top=fmaxf((float)b->clip_y,floorf(top)-1);
    right=fminf(clip_right,ceilf(right)+1);
    bottom=fminf(clip_bottom,ceilf(bottom)+1);
    if(left>=right || top>=bottom)return 0;
    box->left=(UINT)left;box->top=(UINT)top;box->right=(UINT)right;
    box->bottom=(UINT)bottom;box->front=0;box->back=1;
    return 1;
}
/* A CPU/software edit between GPU batches need not re-upload a whole target.
 * Compare every row, then find changed pixel bounds in differing rows. */
#include "nv2a_surface_diff.h"
static int changed_bounds(const uint8_t *ram,uint32_t pitch,const uint8_t *shadow,
                          uint32_t shadow_pitch,uint32_t width,uint32_t height,
                          uint32_t bpp,int force,D3D11_BOX *box)
{
    uint32_t x,y,right;
    box->left=0;box->top=0;box->right=width;box->bottom=height;
    box->front=0;box->back=1;
    if(force)return 1;
    box->left=width;box->top=height;box->right=box->bottom=0;
    for(y=0;y<height;y++) {
        const uint8_t *a=ram+(size_t)y*pitch,*b=shadow+(size_t)y*shadow_pitch;
        if(!memcmp(a,b,(size_t)width*bpp))continue;
        nv2a_surface_row_bounds(a,b,width,bpp,&x,&right);
        if(x<box->left)box->left=x;
        if(right>box->right)box->right=right;
        if(y<box->top)box->top=y;
        box->bottom=y+1;
    }
    return box->top<box->bottom;
}
static ID3D11PixelShader *shader(const Nv2aRenderBatch *b)
{
    Nv2aCombiner key=b->combiner;
    unsigned i,slot=0;
    uint64_t oldest=UINT64_MAX;
    Shader *s;
    char source[32768];
    ID3DBlob *code;
    memset(key.factor0,0,sizeof key.factor0);memset(key.factor1,0,sizeof key.factor1);
    key.final_c0=key.final_c1=0;
    for(i=0;i<SHADER_SLOTS;i++) {
        s=&gpu.shaders[i];
        if(s->valid && s->has_combiner==b->has_combiner && !memcmp(&key,&s->key,sizeof key)) {
            s->age=gpu.age;return s->shader;
        }
        if(!s->valid){slot=i;oldest=0;}else if(oldest && s->age<oldest){slot=i;oldest=s->age;}
    }
    if(nv2a_render_pixel_shader(&key,b->has_combiner,source,sizeof source)<0)return NULL;
    code=compile(source,"ps_5_0");
    if(!code)return NULL;
    s=&gpu.shaders[slot];RELEASE(s->shader);s->valid=0;
    if(error(ID3D11Device_CreatePixelShader(gpu.device,ID3D10Blob_GetBufferPointer(code),
        ID3D10Blob_GetBufferSize(code),NULL,&s->shader),"pixel shader")){RELEASE(code);return NULL;}
    RELEASE(code);s->key=key;s->has_combiner=b->has_combiner;s->age=gpu.age;s->valid=1;shader_compiles++;
    return s->shader;
}
static void unwatch_texture(Texture *t)
{
    uint32_t p;
    if(!t->watched)return;
    for(p=t->watch_lo;p<t->watch_hi;p++)if(page_watch[p])page_watch[p]--;
    t->watched=0;
}
static int surface_pages(uint32_t lo,uint32_t hi)
{
    unsigned i,r;
    for(i=0;i<=SURFACE_SLOTS;i++)for(r=0;r<2;r++) {
        const SurfaceSet *t=set_at(i);
        if((i && !t->used) || !has_range(t,r))continue;
        if(lo<t->rng_hi[r] && t->rng_lo[r]<hi)return 1;
    }
    return 0;
}
/* Protect the source first, then compare or upload: a write after this
 * faults, so nothing written after the comparison can be missed. */
static void watch_texture(Texture *t,const Nv2aRenderTexture *in)
{
    uint32_t lo,hi,p;
    if(!texture_watching || t->watched || in->palette_bytes)return;
    if(!page_range(in->source,in->source_bytes,&lo,&hi) || surface_pages(lo,hi))return;
    for(p=lo;p<hi;p++)page_watch[p]++;
    if(watch_hi<=watch_lo){watch_lo=lo;watch_hi=hi;}
    else { if(lo<watch_lo)watch_lo=lo;if(hi>watch_hi)watch_hi=hi; }
    t->watch_lo=lo;t->watch_hi=hi;t->watched=1;t->dirty=0;
    apply();
}
/* Any write to [lo,hi): watched textures there must be compared again. */
static void textures_written(uint32_t lo,uint32_t hi)
{
    unsigned i;
    int changed=0;
    if(watch_hi<=watch_lo || hi<=watch_lo || lo>=watch_hi)return;
    for(i=0;i<TEXTURE_SLOTS;i++) {
        Texture *t=&gpu.textures[i];
        if(t->watched && lo<t->watch_hi && t->watch_lo<hi) {
            t->dirty=1;unwatch_texture(t);watch_dirtied++;changed=1;
        }
    }
    if(changed)apply();
}
static void release_texture(Texture *t)
{
    unwatch_texture(t);
    texture_cache_bytes-=t->bytes;
    RELEASE(t->view);RELEASE(t->texture);free(t->shadow);free(t->palette);memset(t,0,sizeof *t);
}
static ID3D11ShaderResourceView *texture(const Nv2aRenderTexture *in)
{
    Texture *t=NULL;
    unsigned i,slot=0;
    uint64_t oldest=UINT64_MAX;
    D3D11_TEXTURE2D_DESC td={0};
    uint32_t *decoded;
    size_t required;
    uint64_t tick;
    int unchanged=0,decoded_ok;
    if(!in->source || !in->source_bytes || !in->decode || !in->width || !in->height
       || (uint64_t)in->width*in->height>16777216)return NULL;
    if(in->source_bytes>TEXTURE_BUDGET || in->palette_bytes>TEXTURE_BUDGET)return NULL;
    /* A full mip chain adds at most a third to the decoded size. */
    required=in->source_bytes+in->palette_bytes+(size_t)in->width*in->height*4*(in->levels>1?2:1);
    if(required>TEXTURE_BUDGET)return NULL;
    tick=timing_now();
    for(i=0;i<TEXTURE_SLOTS;i++) {
        Texture *candidate=&gpu.textures[i];
        const Nv2aRenderTexture *k=&candidate->key;
        if(candidate->view && k->source==in->source && k->source_bytes==in->source_bytes
           && k->palette==in->palette && k->palette_bytes==in->palette_bytes
           && k->width==in->width && k->height==in->height && k->format==in->format && k->pitch==in->pitch
           && k->levels==in->levels) {
            t=candidate;break;
        }
        if(!candidate->view){slot=i;oldest=0;}
        else if(oldest && candidate->age<oldest){slot=i;oldest=candidate->age;}
    }
    timing_add(&timing_tex_lookup,tick);tick=timing_now();
    if(t && t->watched && !t->dirty) {
        watch_hits++;t->age=gpu.age;
        timing_add(&timing_tex_compare,tick);return t->view;
    }
    if(t)watch_texture(t,in);
    if(t) {
        if(timing_enabled)tex_compare_requested_bytes+=in->source_bytes;
        unchanged=!memcmp(t->shadow,in->source,in->source_bytes);
        if(unchanged && in->palette_bytes) {
            if(timing_enabled)tex_compare_requested_bytes+=in->palette_bytes;
            unchanged=!memcmp(t->palette,in->palette,in->palette_bytes);
        }
    }
    timing_add(&timing_tex_compare,tick);
    if(unchanged) {
        if(timing_enabled)tex_hits++;
        t->age=gpu.age;return t->view;
    }
    if(timing_enabled) { if(t)tex_changes++;else tex_misses++; }
    tick=timing_now();
    decoded=(uint32_t*)malloc((size_t)in->width*in->height*4);
    if(!decoded)return NULL;
    decoded_ok=in->decode(in->opaque,decoded);timing_add(&timing_tex_decode,tick);
    if(!decoded_ok){free(decoded);return NULL;}
    if(timing_enabled) {
        uint64_t pixels=(uint64_t)in->width*in->height;
        tex_decoded_pixels+=pixels;
        if(in->format<128) { tex_pixels_by_format[in->format]+=pixels;tex_uploads_by_format[in->format]++; }
    }
    tick=timing_now();
    if(!t)t=&gpu.textures[slot];
    release_texture(t);
    while(texture_cache_bytes+required>TEXTURE_BUDGET) {
        Texture *victim=NULL;
        for(i=0;i<TEXTURE_SLOTS;i++)if(gpu.textures[i].view
            && (!victim || gpu.textures[i].age<victim->age))victim=&gpu.textures[i];
        if(!victim){free(decoded);return NULL;}
        release_texture(victim);
    }
    t->shadow=(uint8_t*)malloc(in->source_bytes);
    if(in->palette_bytes)t->palette=(uint8_t*)malloc(in->palette_bytes);
    if(!t->shadow || (in->palette_bytes && !t->palette)){free(decoded);release_texture(t);return NULL;}
    watch_texture(t,in);
    memcpy(t->shadow,in->source,in->source_bytes);
    if(in->palette_bytes)memcpy(t->palette,in->palette,in->palette_bytes);
    td.Width=in->width;td.Height=in->height;td.ArraySize=td.SampleDesc.Count=1;
    td.MipLevels=in->levels>1 && in->decode_level?in->levels:1;
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&t->texture),"texture")
       || error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)t->texture,NULL,&t->view),"texture view")) {
        free(decoded);release_texture(t);return NULL;
    }
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)t->texture,0,NULL,decoded,in->width*4,0);
    /* Smaller levels reuse the base buffer; a level that cannot be decoded
     * keeps the base's first texels rather than leaving undefined memory. */
    for(i=1;i<td.MipLevels;i++) {
        uint32_t w=in->width>>i,h=in->height>>i;
        if(!w)w=1;
        if(!h)h=1;
        in->decode_level(in->opaque,i,decoded);
        ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)t->texture,i,NULL,decoded,w*4,0);
    }
    free(decoded);t->key=*in;t->age=gpu.age;t->bytes=required;
    texture_cache_bytes+=required;texture_uploads++;
    timing_add(&timing_tex_upload,tick);return t->view;
}
/* The stage's own filter and address modes (NV2A TEXFILTER/ADDRESS). The
 * original renderer point-sampled everything with wrap-or-clamp, which made
 * every minified/magnified texture blocky. TENT is bilinear, BOX nearest;
 * only level 0 is uploaded, so LOD selection between mips does not apply.
 * RECOMP_TEXTURE_FILTER=0 restores point sampling. */
static int filtered_sampling=-1;
static struct { uint32_t key, border; ID3D11SamplerState *state; } sampler_cache[64];
static D3D11_TEXTURE_ADDRESS_MODE address_mode(uint32_t mode)
{
    switch(mode) {
    case 1:return D3D11_TEXTURE_ADDRESS_WRAP;
    case 2:return D3D11_TEXTURE_ADDRESS_MIRROR;
    case 4:return D3D11_TEXTURE_ADDRESS_BORDER;
    default:return D3D11_TEXTURE_ADDRESS_CLAMP;   /* 3 CLAMP_TO_EDGE, 5 CLAMP_OGL */
    }
}
static ID3D11SamplerState *sampler_for(const Nv2aRenderTexture *t)
{
    /* MIN: 1 BOX_LOD0, 2 TENT_LOD0, 3 BOX_NEARESTLOD, 4 TENT_NEARESTLOD,
     * 5 BOX_TENT_LOD, 6 TENT_TENT_LOD, 7 CONVOLUTION_2D_LOD0. MAG: 1 BOX,
     * 2 TENT, 4 CONVOLUTION. LOD bias: signed 13-bit, 8 fraction bits. */
    uint32_t min=(t->filter>>16)&0x3F,mag=(t->filter>>24)&0x0F,bias=t->filter&0x1FFF;
    int min_linear=min==2 || min==4 || min==6 || min==7,mag_linear=mag==2 || mag==4;
    int mips=min>=3 && min<=6,mip_linear=min==5 || min==6;
    uint32_t key=(uint32_t)min_linear|(uint32_t)mag_linear<<1|(t->addr_u&15)<<2|(t->addr_v&15)<<6
        |(uint32_t)mips<<10|(uint32_t)mip_linear<<11|bias<<12|1u<<31;
    uint32_t border=(t->addr_u==4 || t->addr_v==4)?t->border:0;
    static int point_format=-2;
    if(point_format==-2){const char *e=getenv("RECOMP_TEXTURE_POINT_FORMAT");point_format=e?(int)strtol(e,NULL,0):-1;}
    if((int)t->format==point_format){min_linear=mag_linear=0;key&=~3u;}  /* diagnostics: force one format to nearest */
    unsigned i;
    D3D11_SAMPLER_DESC sd={0};
    for(i=0;i<64 && sampler_cache[i].key;i++)
        if(sampler_cache[i].key==key && sampler_cache[i].border==border)return sampler_cache[i].state;
    if(i==64)return NULL;
    /* D3D11's filter enum encodes min/mag/mip linearity as bits 4/2/0. */
    sd.Filter=(D3D11_FILTER)((min_linear?0x10:0)|(mag_linear?0x04:0)|(mip_linear?0x01:0));
    sd.AddressU=address_mode(t->addr_u);sd.AddressV=address_mode(t->addr_v);
    sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=mips?D3D11_FLOAT32_MAX:0;
    sd.MipLODBias=(float)((int32_t)(bias<<19)>>19)/256.0f;
    nv2a_rc_unpack(border,sd.BorderColor);
    if(error(ID3D11Device_CreateSamplerState(gpu.device,&sd,&sampler_cache[i].state),"stage sampler"))return NULL;
    sampler_cache[i].key=key;sampler_cache[i].border=border;
    return sampler_cache[i].state;
}
static D3D11_BLEND factor(uint32_t f,int alpha)
{
    switch(f) {
    case 0:return D3D11_BLEND_ZERO;case 1:return D3D11_BLEND_ONE;
    case 0x300:return alpha?D3D11_BLEND_SRC_ALPHA:D3D11_BLEND_SRC_COLOR;
    case 0x301:return alpha?D3D11_BLEND_INV_SRC_ALPHA:D3D11_BLEND_INV_SRC_COLOR;
    case 0x302:return D3D11_BLEND_SRC_ALPHA;case 0x303:return D3D11_BLEND_INV_SRC_ALPHA;
    case 0x304:return D3D11_BLEND_DEST_ALPHA;case 0x305:return D3D11_BLEND_INV_DEST_ALPHA;
    case 0x306:return alpha?D3D11_BLEND_DEST_ALPHA:D3D11_BLEND_DEST_COLOR;
    case 0x307:return alpha?D3D11_BLEND_INV_DEST_ALPHA:D3D11_BLEND_INV_DEST_COLOR;
    case 0x308:return alpha?D3D11_BLEND_ONE:D3D11_BLEND_SRC_ALPHA_SAT;
    case 0x8001:case 0x8003:return D3D11_BLEND_BLEND_FACTOR;
    case 0x8002:case 0x8004:return D3D11_BLEND_INV_BLEND_FACTOR;
    default:return 0;
    }
}
static int render_states(const Nv2aRenderBatch *b)
{
    D3D11_BLEND_DESC bd={0};D3D11_DEPTH_STENCIL_DESC dd={0};
    D3D11_RENDER_TARGET_BLEND_DESC *r=&bd.RenderTarget[0];
    float blend[4];unsigned i;
    int ca=b->blend_src==0x8003 || b->blend_src==0x8004 || b->blend_dst==0x8003 || b->blend_dst==0x8004;
    int cc=b->blend_src==0x8001 || b->blend_src==0x8002 || b->blend_dst==0x8001 || b->blend_dst==0x8002;
    if(b->blend_enable && ca && cc)return 0; /* D3D11 has one blend constant vector. */
    nv2a_rc_unpack(b->blend_color,blend);if(ca)for(i=0;i<3;i++)blend[i]=blend[3];
    r->RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;r->BlendEnable=b->blend_enable!=0;
    r->SrcBlend=factor(b->blend_src,0);r->DestBlend=factor(b->blend_dst,0);
    r->SrcBlendAlpha=factor(b->blend_src,1);r->DestBlendAlpha=factor(b->blend_dst,1);
    if(!r->SrcBlend || !r->DestBlend || !r->SrcBlendAlpha || !r->DestBlendAlpha)return 0;
    switch(b->blend_equation) {
    case 0:case 0x8006:r->BlendOp=D3D11_BLEND_OP_ADD;break;
    case 0x8007:r->BlendOp=D3D11_BLEND_OP_MIN;break;case 0x8008:r->BlendOp=D3D11_BLEND_OP_MAX;break;
    case 0x800A:r->BlendOp=D3D11_BLEND_OP_SUBTRACT;break;case 0x800B:r->BlendOp=D3D11_BLEND_OP_REV_SUBTRACT;break;
    default:return 0;
    }
    r->BlendOpAlpha=r->BlendOp;
    if(!gpu.blend || memcmp(&bd,&gpu.blend_desc,sizeof bd)) {
        RELEASE(gpu.blend);
        if(error(ID3D11Device_CreateBlendState(gpu.device,&bd,&gpu.blend),"blend state"))return 0;
        gpu.blend_desc=bd;
    }
    dd.DepthEnable=b->depth_test && b->depth && b->depth_bpp;
    dd.DepthWriteMask=b->depth_mask?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
    dd.DepthFunc=b->depth_func>=0x200 && b->depth_func<=0x207?
        (D3D11_COMPARISON_FUNC)(b->depth_func-0x200+1):D3D11_COMPARISON_NEVER;
    if(!gpu.ds || memcmp(&dd,&gpu.ds_desc,sizeof dd)) {
        RELEASE(gpu.ds);
        if(error(ID3D11Device_CreateDepthStencilState(gpu.device,&dd,&gpu.ds),"depth state"))return 0;
        gpu.ds_desc=dd;
    }
    ID3D11DeviceContext_OMSetBlendState(gpu.context,gpu.blend,blend,~0u);
    ID3D11DeviceContext_OMSetDepthStencilState(gpu.context,gpu.ds,0);
    return 1;
}

/* A depth-writing batch otherwise needs two staging maps and a whole-depth
 * intermediate copy. Pack the already drawn pixels on the GPU and map one
 * buffer. Neither guest surface changes on failure. */
static int pack_resources(void)
{
    static const char source[]=
        "Texture2D<float4> color_in:register(t0);\n"
        "Texture2D<float> depth_in:register(t1);\n"
        "RWByteAddressBuffer result:register(u0);\n"
        "cbuffer Params:register(b0){uint4 area;uint4 settings;};\n"
        "[numthreads(16,16,1)] void main(uint3 id:SV_DispatchThreadID){\n"
        "if(id.x>=area.z||id.y>=area.w)return;\n"
        "int3 p=int3(area.xy+id.xy,0);\n"
        "uint4 c=(uint4)round(saturate(color_in.Load(p))*255.0);\n"
        "uint argb=(c.a<<24)|(c.r<<16)|(c.g<<8)|c.b;\n"
        "uint z=(uint)(depth_in.Load(p)*(float)settings.x);\n"
        "result.Store2((id.y*area.z+id.x)*8,uint2(argb,z));}\n";
    D3D11_BUFFER_DESC bd={0};
    D3D11_SHADER_RESOURCE_VIEW_DESC sd={0};
    D3D11_UNORDERED_ACCESS_VIEW_DESC ud={0};
    ID3DBlob *code;
    HRESULT hr;
    if(!gpu.pack_requested || gpu.pack_failed)return 0;
    if(gpu.s.pack_uav)return 1;
    if(!gpu.pack_shader) {
        code=compile(source,"cs_5_0");if(!code)goto fail;
        hr=ID3D11Device_CreateComputeShader(gpu.device,ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code),NULL,&gpu.pack_shader);
        RELEASE(code);
        if(error(hr,"pack shader"))goto fail;
        bd.ByteWidth=32;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.pack_constants),"pack constants"))goto fail;
    }
    if(!gpu.s.color_view && error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)gpu.s.color,NULL,&gpu.s.color_view),"pack color view"))goto fail;
    sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels=1;
    if(error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)gpu.s.depth,&sd,&gpu.s.depth_view),"pack depth view"))goto fail;
    memset(&bd,0,sizeof bd);
    bd.ByteWidth=gpu.s.width*gpu.s.height*8;
    bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.s.pack_result),"pack output"))goto fail;
    ud.Format=DXGI_FORMAT_R32_TYPELESS;ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;
    ud.Buffer.NumElements=bd.ByteWidth/4;ud.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_RAW;
    if(error(ID3D11Device_CreateUnorderedAccessView(gpu.device,(ID3D11Resource*)gpu.s.pack_result,&ud,&gpu.s.pack_uav),"pack output view"))goto fail;
    bd.BindFlags=bd.MiscFlags=0;bd.Usage=D3D11_USAGE_STAGING;bd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.s.pack_read),"pack staging"))goto fail;
    return 1;
fail:
    gpu.pack_failed=1;
    RELEASE(gpu.s.pack_uav);RELEASE(gpu.s.pack_result);RELEASE(gpu.s.pack_read);
    RELEASE(gpu.s.color_view);RELEASE(gpu.s.depth_view);
    return 0;
}

static int packed_readback(const Nv2aRenderBatch *b,const D3D11_BOX *box,unsigned dbpp)
{
    ID3D11ShaderResourceView *views[2]={gpu.s.color_view,gpu.s.depth_view};
    ID3D11ShaderResourceView *none[2]={0};
    ID3D11UnorderedAccessView *no_uav=NULL;
    D3D11_MAPPED_SUBRESOURCE mapped={0};
    uint32_t width=box->right-box->left,height=box->bottom-box->top;
    uint32_t constants[8]={box->left,box->top,width,height,dbpp==2?65536:16777216,0,0,0};
    D3D11_BOX bytes={0,0,0,width*height*8,1,1};
    const uint32_t *data;
    uint64_t tick;
    unsigned x,y;
    if(!pack_resources())return 0;
    tick=timing_now();
    views[0]=gpu.s.color_view;views[1]=gpu.s.depth_view;
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.pack_constants,0,NULL,constants,0,0);
    ID3D11DeviceContext_CSSetShader(gpu.context,gpu.pack_shader,NULL,0);
    ID3D11DeviceContext_CSSetConstantBuffers(gpu.context,0,1,&gpu.pack_constants);
    ID3D11DeviceContext_CSSetShaderResources(gpu.context,0,2,views);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(gpu.context,0,1,&gpu.s.pack_uav,NULL);
    ID3D11DeviceContext_Dispatch(gpu.context,(width+15)/16,(height+15)/16,1);
    ID3D11DeviceContext_CSSetShaderResources(gpu.context,0,2,none);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(gpu.context,0,1,&no_uav,NULL);
    ID3D11DeviceContext_CopySubresourceRegion(gpu.context,(ID3D11Resource*)gpu.s.pack_read,0,0,0,0,
        (ID3D11Resource*)gpu.s.pack_result,0,&bytes);
    timing_add(&timing_submit,tick);tick=timing_now();
    if(error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.s.pack_read,0,D3D11_MAP_READ,0,&mapped),"map packed result")) {
        timing_add(&timing_map,tick);
        gpu.pack_failed=1;return 0;
    }
    timing_add(&timing_map,tick);tick=timing_now();
    data=(const uint32_t*)mapped.pData;
    for(y=0;y<height;y++)for(x=0;x<width;x++) {
        size_t at=((size_t)y*width+x)*2;
        size_t color=(size_t)(box->top+y)*b->pitch+(box->left+x)*4;
        size_t depth=(size_t)(box->top+y)*b->depth_pitch+(box->left+x)*dbpp;
        size_t shadow=((size_t)(box->top+y)*gpu.s.width+box->left+x)*dbpp;
        uint32_t z=data[at+1];
        memcpy(b->color+color,data+at,4);memcpy(gpu.s.color_shadow+color,data+at,4);
        if(dbpp==2) { uint16_t v=(uint16_t)z;memcpy(b->depth+depth,&v,2); }
        else { uint32_t v;memcpy(&v,b->depth+depth,4);v=(z<<8)|(v&255);memcpy(b->depth+depth,&v,4); }
        memcpy(gpu.s.depth_shadow+shadow,b->depth+depth,dbpp);
    }
    ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.s.pack_read,0);
    timing_add(&timing_commit,tick);
    packed_readbacks++;
    return 1;
}

/* ── Converted (16-bit / swizzled) colour surfaces ───────────────────
 * MVP 06 renders a 256x256 swizzled R5G6B5 target and samples it back as a
 * swizzled texture; the software rasterizer wrote it pitch-linear, so the
 * texture read scrambled texels, and it cost ~6% of the command worker.
 * The GPU renders it as linear B8G8R8A8 (blending at 8-bit, which is no
 * coarser than the hardware); RAM keeps the title's layout. */
static uint32_t conv_col[4096],conv_row[4096],conv_w,conv_h;
static uint32_t *conv_buf;
static size_t conv_buf_px;
static void conv_tables(uint32_t width,uint32_t height)
{
    uint32_t mx,my,i;
    if(conv_w==width && conv_h==height)return;
    xbox_swizzle_masks(width,height,&mx,&my);
    for(i=0;i<width && i<4096;i++)conv_col[i]=swizzle_deposit(i,mx);
    for(i=0;i<height && i<4096;i++)conv_row[i]=swizzle_deposit(i,my);
    conv_w=width;conv_h=height;
}
static size_t conv_offset(const SurfaceSet *t,uint32_t pitch,uint32_t x,uint32_t y)
{
    return t->cswz?(size_t)(conv_col[x]|conv_row[y])*t->cbpp:(size_t)y*pitch+(size_t)x*t->cbpp;
}
static uint32_t conv_expand(uint32_t v,uint32_t bits) { return (v<<(8-bits))|(v>>(2*bits-8)); }
static uint32_t conv_load(const SurfaceSet *t,const uint8_t *p)
{
    uint32_t v;
    if(t->cbpp==4){memcpy(&v,p,4);return v;}
    v=(uint32_t)p[0]|((uint32_t)p[1]<<8);
    if(t->cformat==3)                                          /* R5G6B5 */
        return 0xFF000000u|(conv_expand((v>>11)&31,5)<<16)|(conv_expand((v>>5)&63,6)<<8)|conv_expand(v&31,5);
    return 0xFF000000u|(conv_expand((v>>10)&31,5)<<16)|(conv_expand((v>>5)&31,5)<<8)|conv_expand(v&31,5);
}
static void conv_store(const SurfaceSet *t,uint8_t *p,uint32_t argb)
{
    uint32_t r=(argb>>16)&255,g=(argb>>8)&255,b=argb&255,v;
    if(t->cbpp==4){memcpy(p,&argb,4);return;}
    if(t->cformat==3)v=((r*31+127)/255)<<11|((g*63+127)/255)<<5|(b*31+127)/255;
    else v=((r*31+127)/255)<<10|((g*31+127)/255)<<5|(b*31+127)/255;
    p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);
}
/* Upload the whole surface when RAM differs from the shadow (or always). */
static int conv_upload(const uint8_t *ram,uint32_t pitch,uint32_t width,uint32_t height,int force)
{
    size_t bytes=(size_t)pitch*height,need=(size_t)width*height;
    uint32_t x,y;
    if(!force && !memcmp(ram,gpu.s.color_shadow,bytes))return 0;
    if(need>conv_buf_px) {
        free(conv_buf);conv_buf=(uint32_t*)malloc(need*4);
        conv_buf_px=conv_buf?need:0;
        if(!conv_buf)return 0;
    }
    if(gpu.s.cswz)conv_tables(width,height);
    for(y=0;y<height;y++)for(x=0;x<width;x++)
        conv_buf[(size_t)y*width+x]=conv_load(&gpu.s,ram+conv_offset(&gpu.s,pitch,x,y));
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.s.color,0,NULL,conv_buf,width*4,0);
    memcpy(gpu.s.color_shadow,ram,bytes);
    surface_uploads++;surface_upload_bytes+=(uint64_t)need*4;
    return 1;
}
/* Copy a committed box of the GPU surfaces into guest RAM and the shadows. */
static int readback(const Nv2aRenderBatch *b,D3D11_BOX box,int write_depth)
{
    D3D11_MAPPED_SUBRESOURCE cr={0},dr={0};
    unsigned x,y,dbpp=gpu.s.depth_bpp,width=gpu.s.width;
    uint64_t start,tick,map_before;
    start=timing_now();
    map_before=timing_map;
    if(write_depth && !CONVERTED(&gpu.s) && packed_readback(b,&box,dbpp))goto committed;
    tick=timing_now();
    ID3D11DeviceContext_CopySubresourceRegion(gpu.context,(ID3D11Resource*)gpu.s.color_read,0,0,0,0,
        (ID3D11Resource*)gpu.s.color,0,&box);
    if(write_depth) {
        /* Depth-stencil sources require a whole-resource copy first. */
        ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.s.depth_upload,(ID3D11Resource*)gpu.s.depth);
        ID3D11DeviceContext_CopySubresourceRegion(gpu.context,(ID3D11Resource*)gpu.s.depth_read,0,0,0,0,
            (ID3D11Resource*)gpu.s.depth_upload,0,&box);
    }
    timing_add(&timing_submit,tick);tick=timing_now();
    if(error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.s.color_read,0,D3D11_MAP_READ,0,&cr),"map color result"))goto failed;
    if(write_depth && error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.s.depth_read,0,D3D11_MAP_READ,0,&dr),"map depth result")) {
        ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.s.color_read,0);goto failed;
    }
    timing_add(&timing_map,tick);tick=timing_now();
    /* Commit only after both mappings succeed. */
    if(CONVERTED(&gpu.s)) {
        if(gpu.s.cswz)conv_tables(gpu.s.width,gpu.s.height);
        for(y=box.top;y<box.bottom;y++) {
            const uint32_t *src=(const uint32_t*)((uint8_t*)cr.pData+(size_t)(y-box.top)*cr.RowPitch);
            for(x=box.left;x<box.right;x++) {
                size_t at=conv_offset(&gpu.s,b->pitch,x,y);
                conv_store(&gpu.s,b->color+at,src[x-box.left]);
                conv_store(&gpu.s,gpu.s.color_shadow+at,src[x-box.left]);
            }
        }
    } else for(y=box.top;y<box.bottom;y++) {
        size_t dst=(size_t)y*b->pitch+box.left*4,row=(size_t)(box.right-box.left)*4;
        const uint8_t *src=(uint8_t*)cr.pData+(size_t)(y-box.top)*cr.RowPitch;
        memcpy(b->color+dst,src,row);memcpy(gpu.s.color_shadow+dst,src,row);
    }
    if(write_depth) {
        for(y=box.top;y<box.bottom;y++)for(x=box.left;x<box.right;x++) {
            uint8_t *p=b->depth+(size_t)y*b->depth_pitch+x*dbpp;
            const uint8_t *q=(uint8_t*)dr.pData+(size_t)(y-box.top)*dr.RowPitch+(x-box.left)*4;
            float f;uint32_t z;
            memcpy(&f,q,4);z=(uint32_t)(f*(dbpp==2?65536.0f:16777216.0f));
            if(dbpp==2){uint16_t v=(uint16_t)z;memcpy(p,&v,2);}
            else {uint32_t v;memcpy(&v,p,4);v=(z<<8)|(v&255);memcpy(p,&v,4);}
            memcpy(gpu.s.depth_shadow+((size_t)y*width+x)*dbpp,p,dbpp);
        }
        ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.s.depth_read,0);
    }
    ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.s.color_read,0);
    timing_add(&timing_commit,tick);
committed:
    if(timing_enabled) {
        uint64_t pixels=(uint64_t)(box.right-box.left)*(box.bottom-box.top),area=pixels;
        unsigned bin=0;
        while(area>1){area>>=1;bin++;}
        readback_calls_by_reason[readback_reason]++;
        readback_pixels_by_reason[readback_reason]+=pixels;
        readback_map_by_reason[readback_reason]+=timing_map-map_before;
        readback_ticks_by_reason[readback_reason]+=timing_now()-start;
        readback_area_bins[bin]++;
    }
    flush_total++;
    readback_bytes+=(uint64_t)(box.right-box.left)*(box.bottom-box.top)*(write_depth?8:4);
    timing_add(&timing_flush,start);
    return 1;
failed:
    timing_add(&timing_map,tick);timing_add(&timing_flush,start);
    return 0;
}

/* Copy resident pixels to RAM. Pages are opened for the copy, then left
 * read-only so a later CPU edit is still seen. Failure is fatal: RAM would
 * otherwise silently stay stale behind an unprotected page. */
static void materialize(void)
{
    int ok;
    if(!gpu.s.res_valid)return;
    res_writing=1;apply();
    ok=readback(&gpu.s.res_batch,gpu.s.res_box,gpu.s.res_depth);
    res_writing=0;
    if(!ok) {
        fprintf(stderr,"[D3D11] fatal: cannot materialize resident pixels\n");
        abort();
    }
    mat_count[mat_reason]++;
    mat_pixels[mat_reason]+=(uint64_t)(gpu.s.res_box.right-gpu.s.res_box.left)*(gpu.s.res_box.bottom-gpu.s.res_box.top);
    gpu.s.res_valid=gpu.s.res_depth=0;res_materializes++;
    apply();
}
/* RAM becomes the only copy: materialize and stop tracking the target. */
static void resident_release(void)
{
    if(!resident_mode)return;
    mat_reason=MR_RELEASE;materialize();mat_reason=MR_OTHER;untrack(0);untrack(1);apply();
}

/* ── Surface cache ────────────────────────────────────────────
 *
 * MVP 06 alternates ~8 targets a frame (three back buffers sharing one
 * depth buffer, three 256-wide render-to-texture pairs, a 336x240 pass).
 * With one bound set every switch read the old target back and uploaded the
 * new one. Parked sets keep their GPU pixels and their page protection, so
 * a switch is a swap. Invariant: a page is tracked by at most one set, so a
 * GPU copy can never go stale behind another set's materialize. */
static uint64_t surface_age;

static void swap_set(SurfaceSet *t) { SurfaceSet x=gpu.s;gpu.s=*t;*t=x; }
static void materialize_set(SurfaceSet *t)
{
    if(!t->res_valid)return;
    if(t==&gpu.s) { materialize();return; }
    swap_set(t);materialize();swap_set(t);
}
/* Release a parked set's GPU objects; its pixels reach RAM first. */
static void drop_set(SurfaceSet *t)
{
    unsigned reason=mat_reason;
    mat_reason=MR_RELEASE;materialize_set(t);mat_reason=reason;
    swap_set(t);release_surfaces();swap_set(t);
    memset(t,0,sizeof *t);
    cache_evictions++;
}
static int ranges_overlap(const SurfaceSet *t,unsigned which,uint32_t lo,uint32_t hi)
{ return has_range(t,which) && lo<t->rng_hi[which] && t->rng_lo[which]<hi; }
/* Give pages [lo,hi) to owner's range `which`. Any other set tracking them
 * materializes and stops tracking; its own next use compares against its
 * shadow and uploads what changed. */
static int claim(SurfaceSet *owner,unsigned which,uint32_t lo,uint32_t hi)
{
    unsigned i,r;
    int changed=owner->rng_lo[which]!=lo || owner->rng_hi[which]!=hi;
    for(i=0;i<=SURFACE_SLOTS;i++) {
        SurfaceSet *t=set_at(i);
        if(t==owner || (i && !t->used))continue;
        for(r=0;r<2;r++)if(ranges_overlap(t,r,lo,hi)) {
            materialize_set(t);t->rng_lo[r]=t->rng_hi[r]=0;changed=1;
        }
    }
    owner->rng_lo[which]=lo;owner->rng_hi[which]=hi;
    textures_written(lo,hi);
    return changed;
}
/* Three back buffers share one depth buffer. Claiming it from the set that
 * last drew read that whole set back. Instead move the depth surface on the
 * GPU: same allocation shape, so CopyResource carries the pixels; tracking,
 * shadow and residency move with it. The receiving color must be current
 * (tracked) because a shared residency box also reads its color back. */
static int transfer_depth(SurfaceSet *from,uint32_t lo,uint32_t hi)
{
    SurfaceSet *to=&gpu.s;
    if(!from->depth || !to->depth || !from->depth_valid || from->rng_lo[1]!=lo || from->rng_hi[1]!=hi
       || from->width!=to->width || from->height!=to->height || from->depth_bpp!=to->depth_bpp
       || !has_range(to,0) || !to->color_valid)return 0;
    ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)to->depth,(ID3D11Resource*)from->depth);
    memcpy(to->depth_shadow,from->depth_shadow,(size_t)to->width*to->height*to->depth_bpp);
    to->depth_source=from->depth_source;to->depth_pitch=from->depth_pitch;to->depth_valid=1;
    to->rng_lo[1]=lo;to->rng_hi[1]=hi;
    if(from->res_valid && from->res_depth) {
        const D3D11_BOX *f=&from->res_box;
        if(to->res_valid) {
            D3D11_BOX *b=&to->res_box;
            if(f->left<b->left)b->left=f->left;
            if(f->top<b->top)b->top=f->top;
            if(f->right>b->right)b->right=f->right;
            if(f->bottom>b->bottom)b->bottom=f->bottom;
        } else {
            to->res_box=*f;to->res_valid=1;
            to->res_batch=from->res_batch;
            to->res_batch.color=(uint8_t*)to->color_source;to->res_batch.pitch=to->width*4;
        }
        to->res_batch.depth=from->res_batch.depth;to->res_batch.depth_pitch=from->res_batch.depth_pitch;
        to->res_depth=1;
        from->res_depth=0;   /* its color stays resident */
    }
    from->rng_lo[1]=from->rng_hi[1]=0;from->depth_valid=0;
    depth_transfers++;
    return 1;
}
/* Move the bound set into a slot (evicting the least recently used). */
static void park(void)
{
    unsigned i,slot=0;
    uint64_t oldest=UINT64_MAX;
    if(!gpu.s.color)return;
    for(i=0;i<SURFACE_SLOTS;i++) {
        if(!parked[i].used){slot=i;oldest=0;break;}
        if(parked[i].age<oldest){slot=i;oldest=parked[i].age;}
    }
    if(parked[slot].used)drop_set(&parked[slot]);
    gpu.s.age=++surface_age;gpu.s.used=1;
    parked[slot]=gpu.s;
    memset(&gpu.s,0,sizeof gpu.s);gpu.s.used=1;
}
/* Bind the set for a target, parking the current one. A set whose color
 * or depth pages overlap the target's is reconciled by claim. */
static uint32_t bind_pitch;   /* colour bytes per row of the target being bound */
static void bind_target(const uint8_t *color,uint32_t width,uint32_t height,
                        const uint8_t *depth,uint32_t depth_pitch,uint32_t dbpp)
{
    uint32_t lo,hi;
    unsigned i;
    int changed=0;
    if(!gpu.s.color || gpu.s.color_source!=color || gpu.s.width!=width) {
        note_target((uint32_t)(color-res_contig),depth?(uint32_t)(depth-res_contig):0,width,height);
        park();
        for(i=0;i<SURFACE_SLOTS;i++) {
            SurfaceSet *t=&parked[i];
            if(t->used && t->color_source==color && t->width==width) {
                gpu.s=*t;memset(t,0,sizeof *t);gpu.s.used=1;cache_hits++;
                break;
            }
        }
        if(!gpu.s.color && !gpu.s.color_valid)cache_misses++;
    }
    /* The bound depth must be this target's depth. */
    if(dbpp && gpu.s.depth_valid && (gpu.s.depth_source!=depth || gpu.s.depth_pitch!=depth_pitch
                                     || gpu.s.depth_bpp!=dbpp)) {
        mat_reason=MR_RETARGET;materialize();mat_reason=MR_OTHER;
        untrack(1);gpu.s.depth_valid=0;changed=1;
    }
    /* Pixels another set holds for these pages must reach RAM before this
     * set compares or uploads them. */
    if(gpu.s.color && gpu.s.height>height)height=gpu.s.height;
    if(page_range(color,(size_t)(bind_pitch?bind_pitch:width*4)*height,&lo,&hi) && !ranges_overlap(&gpu.s,0,lo,hi)) {
        mat_reason=MR_RETARGET;changed|=claim(&gpu.s,0,lo,hi);gpu.s.rng_lo[0]=gpu.s.rng_hi[0]=0;mat_reason=MR_OTHER;
    }
    if(dbpp && page_range(depth,(size_t)depth_pitch*height,&lo,&hi) && !ranges_overlap(&gpu.s,1,lo,hi)) {
        for(i=0;i<SURFACE_SLOTS;i++)
            if(parked[i].used && ranges_overlap(&parked[i],1,lo,hi) && transfer_depth(&parked[i],lo,hi)) {
                changed=1;break;
            }
    }
    if(dbpp && page_range(depth,(size_t)depth_pitch*height,&lo,&hi) && !ranges_overlap(&gpu.s,1,lo,hi)) {
        mat_reason=MR_RETARGET;changed|=claim(&gpu.s,1,lo,hi);gpu.s.rng_lo[1]=gpu.s.rng_hi[1]=0;mat_reason=MR_OTHER;
    }
    if(changed)apply();
}
static int batch_depth(const Nv2aRenderBatch *b) { return b->depth_test && b->depth && b->depth_bpp; }
static int resident_target(const Nv2aRenderBatch *b)
{
    uint32_t lo,hi;
    if(!resident_mode || !page_range(b->color,(size_t)b->pitch*b->height,&lo,&hi))return 0;
    return !batch_depth(b) || page_range(b->depth,(size_t)b->depth_pitch*b->height,&lo,&hi);
}
/* Queued pixels become resident instead of being read back. */
static void commit(void)
{
    const Nv2aRenderBatch *b=&gpu.pending_batch;
    if(gpu.s.res_valid) {
        D3D11_BOX *r=&gpu.s.res_box;const D3D11_BOX *p=&gpu.pending_box;
        if(p->left<r->left)r->left=p->left;
        if(p->top<r->top)r->top=p->top;
        if(p->right>r->right)r->right=p->right;
        if(p->bottom>r->bottom)r->bottom=p->bottom;
    } else gpu.s.res_box=gpu.pending_box;
    if(batch_depth(b) || !gpu.s.res_valid || !gpu.s.res_depth) {
        gpu.s.res_batch=*b;
    } else {
        /* A depthless HUD group after a depth-writing scene keeps the
         * resident depth surface it did not touch. */
        uint8_t *depth=gpu.s.res_batch.depth;uint32_t pitch=gpu.s.res_batch.depth_pitch;
        gpu.s.res_batch=*b;gpu.s.res_batch.depth=depth;gpu.s.res_batch.depth_pitch=pitch;
    }
    /* Protection changes only when residency starts, depth joins it, or a
     * range moves; most commits extend a box already guarded. */
    {
        uint32_t lo,hi;
        int changed=!gpu.s.res_valid || (gpu.pending_depth && !gpu.s.res_depth);
        gpu.s.res_depth|=gpu.pending_depth;gpu.s.res_valid=1;
        gpu.pending=gpu.pending_depth=0;
        uint32_t rows=gpu.s.height>b->height?gpu.s.height:b->height;
        if(page_range(b->color,(size_t)b->pitch*rows,&lo,&hi))changed|=claim(&gpu.s,0,lo,hi);
        if(batch_depth(b) && page_range(b->depth,(size_t)b->depth_pitch*rows,&lo,&hi))
            changed|=claim(&gpu.s,1,lo,hi);
        res_commits++;
        if(changed)apply();
    }
}
static int flush_locked(void)
{
    int ok;
    if(!gpu.pending)return 1;
    if(resident_target(&gpu.pending_batch)) { commit();return 1; }
    materialize();
    ok=readback(&gpu.pending_batch,gpu.pending_box,gpu.pending_depth);
    if(ok)gpu.pending=gpu.pending_depth=0;
    return ok;
}
int nv2a_render_flush(void)
{
    int ok;
    if(!gpu.enabled)return 1;
    EnterCriticalSection(&render_cs);ok=flush_locked();LeaveCriticalSection(&render_cs);
    return ok;
}
static int counted_flush(uint64_t *reason)
{
    int ok;
    unsigned previous=readback_reason;
    if(gpu.pending)(*reason)++;
    if(timing_enabled) {
        readback_reason=reason==&flush_scope_end?0:reason==&flush_external?1:
            reason==&flush_memory_alias?2:reason==&flush_target?3:
            reason==&flush_texture_alias?4:reason==&flush_unscoped?5:
            reason==&flush_fallback?6:7;
    }
    ok=flush_locked();readback_reason=previous;return ok;
}

/* Any thread touching a tracked page. Runs under the renderer lock, which
 * the command worker holds only inside renderer calls (never across guest
 * waits), so a faulting guest thread cannot deadlock against it. */
static void touch_pages(uint32_t lo,uint32_t hi,int write)
{
    unsigned i,r;
    int changed=0;
    for(i=0;i<=SURFACE_SLOTS;i++) {
        SurfaceSet *t=set_at(i);
        if(i && !t->used)continue;
        if(!ranges_overlap(t,0,lo,hi) && !ranges_overlap(t,1,lo,hi))continue;
        materialize_set(t);
        if(write)for(r=0;r<2;r++)if(ranges_overlap(t,r,lo,hi)){t->rng_lo[r]=t->rng_hi[r]=0;changed=1;}
    }
    if(changed)apply();
}
static LONG CALLBACK resident_fault(PEXCEPTION_POINTERS ep)
{
    const EXCEPTION_RECORD *r=ep->ExceptionRecord;
    MEMORY_BASIC_INFORMATION mbi;
    uint32_t off,page;
    int write;
    LONG result=EXCEPTION_CONTINUE_SEARCH;
    if(r->ExceptionCode!=EXCEPTION_ACCESS_VIOLATION || r->NumberParameters<2
       || r->ExceptionInformation[0]==8)return EXCEPTION_CONTINUE_SEARCH;
    if(!contig_offset((const void*)r->ExceptionInformation[1],1,&off))return EXCEPTION_CONTINUE_SEARCH;
    write=r->ExceptionInformation[0]==1;page=off/RES_PAGE;
    EnterCriticalSection(&render_cs);
    if(write && page_watch[page])textures_written(page,page+1);
    if(page_cur[page]!=RES_RW) {
        if(write)res_write_faults++;else res_read_faults++;
        if(gpu.s.res_valid)note_site((uint64_t)(ep->ContextRecord->Rip-(uintptr_t)GetModuleHandleA(NULL)));
        mat_reason=MR_FAULT;touch_pages(page,page+1,write);mat_reason=MR_OTHER;
        /* A page left read-only by a released watch: recompute states. */
        if(page_cur[page]!=RES_RW && !page_watch[page] && !surface_pages(page,page+1))apply();
    }
    /* Retry only once the page actually permits the access; another
     * thread may have resolved it first. Anything else is not ours. */
    if(VirtualQuery((const void*)r->ExceptionInformation[1],&mbi,sizeof mbi)
       && (mbi.Protect&(write?(PAGE_READWRITE|PAGE_EXECUTE_READWRITE)
                            :(PAGE_READWRITE|PAGE_READONLY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE))))
        result=EXCEPTION_CONTINUE_EXECUTION;
    LeaveCriticalSection(&render_cs);
    return result;
}
void nv2a_render_host_access(void *p,size_t bytes,int write)
{
    uint32_t lo,hi;
    if(!resident_mode || !page_range(p,bytes,&lo,&hi))return;
    EnterCriticalSection(&render_cs);
    res_host_access++;mat_reason=MR_HOST;touch_pages(lo,hi,write);mat_reason=MR_OTHER;
    if(write)textures_written(lo,hi);
    LeaveCriticalSection(&render_cs);
}
void nv2a_render_set_window(void *contig,void *tiled,size_t tiled_bytes)
{
    res_contig=(uint8_t*)contig;res_tiled=(uint8_t*)tiled;
    res_tiled_bytes=tiled?tiled_bytes:0;
}
static void resident_initialize(void)
{
    const char *mode=getenv("RECOMP_GPU_RESIDENT");
    if(!mode || strcmp(mode,"1") || !res_contig)return;
    resident_mode=1;
    mode=getenv("RECOMP_TEXTURE_WATCH");
    texture_watching=!mode || strcmp(mode,"0");
    AddVectoredExceptionHandler(1,resident_fault);
    fprintf(stderr,"[D3D11] resident surfaces: page-protected, tiled alias %zu MB\n",res_tiled_bytes>>20);
}
int nv2a_render_begin(void)
{
    if(!initialize() || gpu.scope || gpu.pending)return 0;
    gpu.scope=1;return 1;
}
int nv2a_render_end(void)
{
    int ok;
    EnterCriticalSection(&render_cs);ok=counted_flush(&flush_scope_end);LeaveCriticalSection(&render_cs);
    if(!ok)return 0;
    gpu.scope=0;return 1;
}
static int overlaps(const void *a,size_t an,const void *b,size_t bn)
{
    uintptr_t av=(uintptr_t)a,bv=(uintptr_t)b;
    return a && b && an && bn && (av<=bv ? bv-av<an : av-bv<bn);
}
int nv2a_render_scoped(void) { return gpu.scope; }
static void require_flush(uint64_t *reason)
{
    if(!counted_flush(reason)) {
        fprintf(stderr,"[D3D11] fatal: cannot commit GPU transaction to guest RAM\n");
        abort();
    }
}
void nv2a_render_observe(Nv2aRenderObserver observer)
{
    unsigned i=(unsigned)observer;
    uint64_t before,resolves;
    if(!gpu.enabled)return;
    if(i>=NV2A_OBSERVER_COUNT)i=NV2A_OBSERVER_OTHER;
    EnterCriticalSection(&render_cs);
    before=readback_bytes;resolves=flush_total;
    observer_calls[i]++;
    require_flush(&flush_external);
    observer_resolves[i]+=flush_total-resolves;
    observer_bytes[i]+=readback_bytes-before;
    LeaveCriticalSection(&render_cs);
}
void nv2a_render_require_flush(void) { nv2a_render_observe(NV2A_OBSERVER_OTHER); }
void nv2a_render_memory_read(const void *source,size_t bytes)
{
    const Nv2aRenderBatch *b=&gpu.pending_batch;
    uint32_t lo,hi;
    /* Unlocked fast path: only this consumer queues work, and a stale
     * resident flag is still caught by the page protection. */
    {
        uint32_t off;
        if(!gpu.pending && (surf_env_hi<=surf_env_lo || !contig_offset(source,1,&off)
                            || off/RES_PAGE<surf_env_lo || off/RES_PAGE>=surf_env_hi))return;
    }
    EnterCriticalSection(&render_cs);
    if(gpu.pending && (overlaps(source,bytes,b->color,(size_t)b->pitch*b->height)
        || (gpu.pending_depth && overlaps(source,bytes,b->depth,(size_t)b->depth_pitch*b->height))))
        require_flush(&flush_memory_alias);
    /* Materializing here is cheaper than taking the fault. */
    if(page_range(source,bytes,&lo,&hi)) {
        mat_reason=MR_MEMREAD;touch_pages(lo,hi,0);mat_reason=MR_OTHER;
    }
    LeaveCriticalSection(&render_cs);
}
static int pending_texture(const Nv2aRenderTexture *t)
{
    const Nv2aRenderBatch *b=&gpu.pending_batch;
    size_t color=(size_t)b->pitch*b->height,depth=(size_t)b->depth_pitch*b->height;
    return overlaps(t->source,t->source_bytes,b->color,color)
        || overlaps(t->palette,t->palette_bytes,b->color,color)
        || (gpu.pending_depth && (overlaps(t->source,t->source_bytes,b->depth,depth)
            || overlaps(t->palette,t->palette_bytes,b->depth,depth)));
}

static int draw_locked(const Nv2aRenderBatch *b);
int nv2a_render_draw(const Nv2aRenderBatch *b)
{
    int ok;
    if(!initialize())return 0;
    EnterCriticalSection(&render_cs);ok=draw_locked(b);LeaveCriticalSection(&render_cs);
    return ok;
}
/* A resident target must reach RAM before the single GPU surface is reused
 * for another target or reallocated. */
static void retarget(const Nv2aRenderBatch *b,uint32_t width,uint32_t dbpp)
{
    bind_pitch=b->pitch;
    bind_target(b->color,width,b->height,dbpp?b->depth:NULL,dbpp?b->depth_pitch:0,dbpp);
    bind_pitch=0;
}
/* A texture that is a parked render target is sampled where it already is,
 * instead of reading it back, decoding RAM and uploading it again. Same
 * bytes: LIN_A8R8G8B8 decodes as a copy into the B8G8R8A8 layout both use,
 * and linear textures address in texels against the same dimensions. The
 * parked copy must be current: tracked (resident or equal to RAM, since a
 * CPU write would have faulted and untracked it). The bound target is never
 * sampled this way (render-target feedback keeps the RAM path). */
static void note_texture_miss(const Nv2aRenderTexture *in,const SurfaceSet *t,uint32_t why)
{
    unsigned i;
    uint32_t src=(uint32_t)((const uint8_t*)in->source-res_contig);
    for(i=0;i<16;i++) {
        if(texture_misses[i].count && texture_misses[i].source==src && texture_misses[i].width==in->width
           && texture_misses[i].height==in->height && texture_misses[i].why==why){texture_misses[i].count++;return;}
        if(!texture_misses[i].count) {
            texture_misses[i].source=src;texture_misses[i].width=in->width;texture_misses[i].height=in->height;
            texture_misses[i].pitch=in->pitch;texture_misses[i].format=in->format;texture_misses[i].why=why;
            texture_misses[i].set_width=t?t->width:0;texture_misses[i].set_height=t?t->height:0;
            texture_misses[i].count=1;return;
        }
    }
}
/* Depth sampled as LIN_A8R8G8B8 (MVP 06 reads its scene depth this way).
 * RAM holds each D24S8 word as (z24<<8)|stencil, so the texel the decoder
 * would produce is B=stencil, G=z[7:0], R=z[15:8], A=z[23:16]. A compute
 * pass builds exactly that from the resident depth -- converted with the
 * readback's own (uint)(f*2^24) -- and the RAM-owned stencil plane, taken
 * from the depth shadow (RAM as of the last sync; a CPU write would have
 * faulted and untracked the depth). */
static const char depth_texel_source[]=
    "Texture2D<float> depth_in:register(t0);\n"
    "Texture2D<uint> stencil_in:register(t1);\n"
    "RWTexture2D<unorm float4> texel_out:register(u0);\n"
    "[numthreads(16,16,1)] void main(uint3 id:SV_DispatchThreadID){\n"
    "uint w,h;texel_out.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;\n"
    "uint z=(uint)(depth_in.Load(int3(id.xy,0))*16777216.0);\n"
    "uint s=stencil_in.Load(int3(id.xy,0));\n"
    "texel_out[id.xy]=float4((z>>8)&255,z&255,s,(z>>16)&255)/255.0;}\n";
static ID3D11ComputeShader *depth_texel_shader;
static ID3D11Texture2D *depth_texel, *depth_stencil_plane;
static ID3D11ShaderResourceView *depth_texel_view, *depth_stencil_view;
static ID3D11UnorderedAccessView *depth_texel_uav;
static uint32_t depth_texel_width, depth_texel_height;
static uint8_t *stencil_plane;
static uint64_t depth_texel_age;
static const void *depth_texel_source_ptr;
static int depth_texel_failed;
static ID3D11ShaderResourceView *depth_texture(SurfaceSet *t)
{
    ID3D11ShaderResourceView *views[2],*none[2]={0};
    ID3D11UnorderedAccessView *no_uav=NULL;
    D3D11_SHADER_RESOURCE_VIEW_DESC sd={0};
    size_t i,n=(size_t)t->width*t->height;
    if(depth_texel_failed)return NULL;
    if(depth_texel_age==gpu.age && depth_texel_source_ptr==t->depth_source)return depth_texel_view;
    if(!depth_texel_shader) {
        ID3DBlob *code=compile(depth_texel_source,"cs_5_0");
        HRESULT hr;
        if(!code){depth_texel_failed=1;return NULL;}
        hr=ID3D11Device_CreateComputeShader(gpu.device,ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code),NULL,&depth_texel_shader);
        RELEASE(code);
        if(error(hr,"depth texel shader")){depth_texel_failed=1;return NULL;}
    }
    if(depth_texel_width!=t->width || depth_texel_height!=t->height) {
        D3D11_TEXTURE2D_DESC td={0};
        RELEASE(depth_texel_uav);RELEASE(depth_texel_view);RELEASE(depth_texel);
        RELEASE(depth_stencil_view);RELEASE(depth_stencil_plane);free(stencil_plane);stencil_plane=NULL;
        depth_texel_width=depth_texel_height=0;
        td.Width=t->width;td.Height=t->height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
        td.Format=DXGI_FORMAT_R8G8B8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&depth_texel),"depth texel")
           || error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)depth_texel,NULL,&depth_texel_view),"depth texel view")
           || error(ID3D11Device_CreateUnorderedAccessView(gpu.device,(ID3D11Resource*)depth_texel,NULL,&depth_texel_uav),"depth texel uav"))
            return NULL;
        td.Format=DXGI_FORMAT_R8_UINT;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&depth_stencil_plane),"stencil plane")
           || error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)depth_stencil_plane,NULL,&depth_stencil_view),"stencil view"))
            return NULL;
        stencil_plane=(uint8_t*)malloc(n);
        if(!stencil_plane)return NULL;
        depth_texel_width=t->width;depth_texel_height=t->height;
    }
    if(!t->depth_view) {
        sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;
        if(error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)t->depth,&sd,&t->depth_view),"depth texel source"))
            return NULL;
    }
    for(i=0;i<n;i++)stencil_plane[i]=t->depth_shadow[i*4];
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)depth_stencil_plane,0,NULL,stencil_plane,t->width,0);
    /* The depth surface may be the bound DSV; reading it needs it unbound. */
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    views[0]=t->depth_view;views[1]=depth_stencil_view;
    ID3D11DeviceContext_CSSetShader(gpu.context,depth_texel_shader,NULL,0);
    ID3D11DeviceContext_CSSetShaderResources(gpu.context,0,2,views);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(gpu.context,0,1,&depth_texel_uav,NULL);
    ID3D11DeviceContext_Dispatch(gpu.context,(t->width+15)/16,(t->height+15)/16,1);
    ID3D11DeviceContext_CSSetShaderResources(gpu.context,0,2,none);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(gpu.context,0,1,&no_uav,NULL);
    depth_texel_age=gpu.age;depth_texel_source_ptr=t->depth_source;depth_texels++;
    return depth_texel_view;
}
static ID3D11Texture2D *snapshot;
static ID3D11ShaderResourceView *snapshot_view;
static uint32_t snapshot_width, snapshot_height;
static uint64_t snapshot_age;
static ID3D11ShaderResourceView *surface_texture(const Nv2aRenderTexture *in,uint32_t *width,uint32_t *height,
                                                 int *force_alpha)
{
    unsigned i;
    *force_alpha=0;
    /* Converted render targets sampled in their own format: 0x05/0x11 are
     * swizzled/linear R5G6B5, 0x06/0x07 swizzled A8R8G8B8/X8R8G8B8. */
    if(resident_mode && !in->palette_bytes
       && (in->format==0x05 || in->format==0x11 || in->format==0x06 || in->format==0x07)) {
        uint32_t want_bpp=in->format==0x06 || in->format==0x07?4:2,want_swz=in->format!=0x11;
        for(i=1;i<=SURFACE_SLOTS;i++) {
            SurfaceSet *t=set_at(i);
            if(!t->used || !t->color || !t->color_valid || t->color_source!=in->source || !has_range(t,0)
               || !CONVERTED(t) || t->cbpp!=want_bpp || t->cswz!=want_swz || (want_bpp==2 && t->cformat!=3))continue;
            if(in->width!=t->width || in->height!=t->height)continue;
            if(!t->color_view && error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)t->color,
                                       NULL,&t->color_view),"converted texture view"))return NULL;
            *width=t->width;*height=t->height;*force_alpha=in->format!=0x06;
            t->age=++surface_age;direct_textures++;
            return t->color_view;
        }
        return NULL;
    }
    if(!resident_mode || in->format!=0x12 || in->normalized || in->palette_bytes)return NULL;
    for(i=0;i<=SURFACE_SLOTS;i++) {
        SurfaceSet *t=set_at(i);
        if((i && !t->used) || !t->color || !t->color_valid || t->color_source!=in->source || !has_range(t,0))continue;
        /* A smaller view of the same rows (the game samples its 336-wide
         * target as 321 texels) addresses the same texels; only an edge
         * clamp would differ, so wrapped views of a sub-rect stay on RAM. */
        if(in->pitch!=t->width*4 || in->width>t->width || in->height>t->height)continue;
        if((in->width!=t->width && in->addr_u==1) || (in->height!=t->height && in->addr_v==1))continue;
        *width=t->width;*height=t->height;
        if(!i) {
            /* Sampling the bound target: on RAM this read the pixels as of
             * the draw. A GPU copy of the surface is the same snapshot. */
            if(!snapshot_view || snapshot_width!=t->width || snapshot_height!=t->height) {
                D3D11_TEXTURE2D_DESC td={0};
                RELEASE(snapshot_view);RELEASE(snapshot);
                td.Width=t->width;td.Height=t->height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
                td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
                if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&snapshot),"snapshot")
                   || error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)snapshot,NULL,&snapshot_view),
                            "snapshot view")) { RELEASE(snapshot);return NULL; }
                snapshot_width=t->width;snapshot_height=t->height;snapshot_age=0;
            }
            if(snapshot_age!=gpu.age) {
                ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)snapshot,(ID3D11Resource*)t->color);
                snapshot_age=gpu.age;snapshot_copies++;
            }
            return snapshot_view;
        }
        if(!t->color_view && error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)t->color,
                                   NULL,&t->color_view),"surface texture view"))return NULL;
        t->age=++surface_age;direct_textures++;
        return t->color_view;
    }
    for(i=0;i<=SURFACE_SLOTS;i++) {
        SurfaceSet *t=set_at(i);
        if((i && !t->used) || !t->depth || !t->depth_valid || t->depth_source!=in->source || t->depth_bpp!=4
           || !has_range(t,1) || !t->depth_shadow)continue;
        if(in->pitch!=t->depth_pitch || t->depth_pitch!=t->width*4 || in->width>t->width || in->height>t->height)continue;
        if((in->width!=t->width && in->addr_u==1) || (in->height!=t->height && in->addr_v==1))continue;
        *width=t->width;*height=t->height;
        return depth_texture(t);
    }
    return NULL;
}
/* Resident textures sampled from the target need RAM to be current. */
static void resident_texture(const Nv2aRenderTexture *t)
{
    uint32_t lo,hi;
    mat_reason=MR_TEXTURE;
    if(page_range(t->source,t->source_bytes,&lo,&hi)) {
        uint64_t before=res_materializes;
        touch_pages(lo,hi,0);
        if(res_materializes!=before)note_texture_miss(t,NULL,9);
    }
    if(gpu.s.res_valid && page_range(t->palette,t->palette_bytes,&lo,&hi))touch_pages(lo,hi,0);
    mat_reason=MR_OTHER;
}
static int draw_locked(const Nv2aRenderBatch *b)
{
    Nv2aRenderConstants constants={0};
    ID3D11PixelShader *ps;
    ID3D11ShaderResourceView *views[4]={0};
    ID3D11SamplerState *samplers[4];
    D3D11_MAPPED_SUBRESOURCE vb={0};
    D3D11_VIEWPORT viewport={0};D3D11_RECT rect;
    D3D11_BOX box,dirty={0};
    uint32_t width,dbpp,dwidth=0;
    int bounds,color_changed,depth_changed=0,write_depth;
    unsigned i,x,y;UINT stride=sizeof(Nv2aRenderVertex),offset=0;
    size_t bytes=(size_t)b->count*sizeof(Nv2aRenderVertex);
    uint64_t start,tick,part;
    if(!initialize())return 0;
    start=timing_now();
    gpu.age++;
    if(!(b->color_bpp==4 || (b->color_bpp==2 && b->color_format>=1 && b->color_format<=3))
       || !b->pitch || b->pitch%b->color_bpp || !b->height || !b->color
       || !b->vertices || !b->count || b->count%3 || (b->swizzled && !resident_mode)) {
        note_fail(100000+b->color_bpp*100+(b->count%3)*10+(!b->color)+(!b->pitch)*2);
        goto fail;
    }
    width=b->pitch/b->color_bpp;
    if(width>16384 || b->height>16384)DRAW_FAIL;
    /* Converted layouts need residency's whole-surface synchronisation. */
    if((b->color_bpp!=4 || b->swizzled) && !resident_mode)DRAW_FAIL;
    dbpp=b->depth_test && b->depth?b->depth_bpp:0;
    if(dbpp && dbpp!=2 && dbpp!=4)DRAW_FAIL;
    if(dbpp){if(!b->depth_pitch || b->depth_pitch%dbpp)DRAW_FAIL;dwidth=b->depth_pitch/dbpp;}
    bounds=draw_bounds(b,width,dwidth,&box);
    if(bounds<0)DRAW_FAIL;
    if(!bounds)goto success;
    write_depth=dbpp && b->depth_mask;
    if(gpu.pending && (width!=gpu.s.width || b->height!=gpu.pending_batch.height
        /* Allocation depth can outlive a disabled interval. Compare the
         * pending draw's active depth state so HUD draws still coalesce. */
        || dbpp!=(gpu.pending_batch.depth_test && gpu.pending_batch.depth?
                   gpu.pending_batch.depth_bpp:0)
        || b->color!=gpu.s.color_source || b->depth!=gpu.s.depth_source || b->depth_pitch!=gpu.s.depth_pitch
        /* Overlapping color/depth RAM requires the ordinary commit/upload
         * ordering between draws, including read-only depth aliasing color. */
        || (dbpp && overlaps(b->color,(size_t)b->pitch*b->height,
                             b->depth,(size_t)b->depth_pitch*b->height))))
        if(!counted_flush(&flush_target))goto failed_resolve;
    for(i=0;gpu.pending && i<4;i++) {
        unsigned mode=b->has_combiner?(b->combiner.stage_program>>(5*i))&31:(i?0:1);
        if(b->textures[i].valid && nv2a_rc_stage_samples(mode) && pending_texture(&b->textures[i]))
            if(!counted_flush(&flush_texture_alias))goto failed_resolve;
    }
    if(resident_mode) {
        retarget(b,width,dbpp);
        for(i=0;i<4;i++) {
            unsigned mode=b->has_combiner?(b->combiner.stage_program>>(5*i))&31:(i?0:1);
            uint32_t w,h;
            int fa;
            if(b->textures[i].valid && nv2a_rc_stage_samples(mode) && !surface_texture(&b->textures[i],&w,&h,&fa))
                resident_texture(&b->textures[i]);
        }
    }
    tick=timing_now();
    if(!surfaces(width,b->height,dbpp) || !render_states(b))DRAW_FAIL;
    if(gpu.s.cbpp!=b->color_bpp || gpu.s.cswz!=b->swizzled || (b->color_bpp==2 && gpu.s.cformat!=b->color_format)) {
        /* Same memory drawn with another layout: RAM is authoritative. */
        if(gpu.s.cbpp)resident_release();
        gpu.s.cbpp=b->color_bpp;gpu.s.cswz=b->swizzled;gpu.s.cformat=b->color_format;
        gpu.s.color_valid=0;
    }
    ps=shader(b);if(!ps)DRAW_FAIL;
    for(i=0;i<4;i++) {
        const Nv2aRenderTexture *t=&b->textures[i];
        unsigned mode=b->has_combiner?(b->combiner.stage_program>>(5*i))&31:(i?0:1);
        uint32_t view_width=t->width,view_height=t->height;
        int force_alpha=0;
        if(t->valid && nv2a_rc_stage_samples(mode)) {
            part=timing_now();
            view_width=t->width;view_height=t->height;
            views[i]=surface_texture(t,&view_width,&view_height,&force_alpha);
            if(!views[i])views[i]=texture(t);
            timing_add(&timing_texture,part);
            if(!views[i])DRAW_FAIL;
        }
        samplers[i]=t->valid && nv2a_rc_stage_samples(mode) && filtered_sampling?sampler_for(t):
            gpu.samplers[(t->addr_u==1?1:0)|(t->addr_v==1?2:0)];
        if(!samplers[i])samplers[i]=gpu.samplers[(t->addr_u==1?1:0)|(t->addr_v==1?2:0)];
        /* Unnormalized coordinates divide by the bound view's own size. */
        constants.texture[i][0]=(float)(views[i]?view_width:t->width);
        constants.texture[i][1]=(float)(views[i]?view_height:t->height);
        constants.texture[i][2]=(float)t->normalized;constants.texture[i][3]=views[i]?(force_alpha?2.0f:1.0f):0.0f;
        nv2a_rc_unpack(b->combiner.factor0[i],constants.c0[i]);
        nv2a_rc_unpack(b->combiner.factor1[i],constants.c1[i]);
        nv2a_rc_unpack(b->combiner.factor0[i+4],constants.c0[i+4]);
        nv2a_rc_unpack(b->combiner.factor1[i+4],constants.c1[i+4]);
    }
    nv2a_rc_unpack(b->combiner.final_c0,constants.final_c0);
    nv2a_rc_unpack(b->combiner.final_c1,constants.final_c1);
    constants.surface[0]=(float)width;constants.surface[1]=(float)b->height;
    constants.surface[2]=dbpp==2?65535.0f:16777215.0f;
    constants.surface[3]=dbpp==2?1.0f/65536.0f:1.0f/16777216.0f;
    constants.control[0]=b->alpha_func;constants.control[1]=b->alpha_ref&255;
    constants.control[2]=b->alpha_test;constants.control[3]=b->shader_clip_plane;
    /* A ring: each draw appends with NO_OVERWRITE and only a wrap discards.
     * Discarding the whole (largest-draw-sized) buffer on every one of
     * ~460 draws a frame exhausted the driver's renaming and made it wait
     * for the GPU. */
    if(bytes>gpu.vertex_bytes) {
        D3D11_BUFFER_DESC bd={0};
        size_t size=bytes<VERTEX_RING_BYTES?VERTEX_RING_BYTES:bytes;
        RELEASE(gpu.vertices);gpu.vertex_bytes=gpu.vertex_used=0;
        if(size>UINT32_MAX)DRAW_FAIL;
        bd.ByteWidth=(UINT)size;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        bd.Usage=D3D11_USAGE_DYNAMIC;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.vertices),"vertex buffer"))DRAW_FAIL;
        gpu.vertex_bytes=size;gpu.vertex_used=size;   /* first map discards */
    }
    {
        D3D11_MAP kind=D3D11_MAP_WRITE_NO_OVERWRITE;
        if(gpu.vertex_used+bytes>gpu.vertex_bytes){kind=D3D11_MAP_WRITE_DISCARD;gpu.vertex_used=0;}
        if(error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.vertices,0,kind,0,&vb),"map vertices"))DRAW_FAIL;
        memcpy((uint8_t*)vb.pData+gpu.vertex_used,b->vertices,bytes);
        ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.vertices,0);
        offset=(UINT)gpu.vertex_used;
        gpu.vertex_used+=(bytes+255)&~(size_t)255;
    }
    timing_add(&timing_prepare,tick);tick=timing_now();
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    part=timing_now();
    /* A tracked page range proves RAM was not written since GPU and shadow
     * last agreed (a write would have faulted and untracked it). */
    if(CONVERTED(&gpu.s)) {
        if(!gpu.pending && !(gpu.s.rng_hi[0]>gpu.s.rng_lo[0] && gpu.s.color_valid && gpu.s.color_source==b->color))
            conv_upload(b->color,b->pitch,width,gpu.s.height,!gpu.s.color_valid || gpu.s.color_source!=b->color);
        color_changed=0;
    } else
    color_changed=!gpu.pending && !(resident_mode && gpu.s.rng_hi[0]>gpu.s.rng_lo[0] && gpu.s.color_valid
                                    && gpu.s.color_source==b->color)
        && changed_bounds(b->color,b->pitch,gpu.s.color_shadow,b->pitch,width,gpu.s.height,4,
        !gpu.s.color_valid || gpu.s.color_source!=b->color,&dirty);
    timing_add(&timing_compare,part);
    if(color_changed) {
        ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.s.color,0,&dirty,
            b->color+(size_t)dirty.top*b->pitch+dirty.left*4,b->pitch,0);
        for(y=dirty.top;y<dirty.bottom;y++) {
            size_t at=(size_t)y*b->pitch+dirty.left*4;
            memcpy(gpu.s.color_shadow+at,b->color+at,(size_t)(dirty.right-dirty.left)*4);
        }
        surface_uploads++;surface_upload_bytes+=(uint64_t)(dirty.right-dirty.left)*(dirty.bottom-dirty.top)*4;
    }
    if(dbpp) {
        part=timing_now();
        depth_changed=!gpu.pending && !(resident_mode && gpu.s.rng_hi[1]>gpu.s.rng_lo[1] && gpu.s.depth_valid
                                        && gpu.s.depth_source==b->depth && gpu.s.depth_pitch==b->depth_pitch)
            && changed_bounds(b->depth,b->depth_pitch,gpu.s.depth_shadow,width*dbpp,
            width<dwidth?width:dwidth,gpu.s.height,dbpp,
            !gpu.s.depth_valid || gpu.s.depth_source!=b->depth || gpu.s.depth_pitch!=b->depth_pitch,&dirty);
        timing_add(&timing_compare,part);
        if(depth_changed) {
            for(y=dirty.top;y<dirty.bottom;y++) {
                memcpy(gpu.s.depth_shadow+((size_t)y*width+dirty.left)*dbpp,
                    b->depth+(size_t)y*b->depth_pitch+dirty.left*dbpp,(size_t)(dirty.right-dirty.left)*dbpp);
                for(x=dirty.left;x<dirty.right;x++) {
                    const uint8_t *p=b->depth+(size_t)y*b->depth_pitch+x*dbpp;
                    float z;
                    if(dbpp==2){uint16_t v;memcpy(&v,p,2);z=(float)v;}
                    else {uint32_t v;memcpy(&v,p,4);z=(float)(v>>8);}
                    z*=constants.surface[3];
                    memcpy(gpu.s.depth_pixels+((size_t)y*width+x)*4,&z,4);
                }
            }
            /* CopyResource back into the depth-stencil target replaces the
             * entire surface. Packed readback does not refresh depth_upload,
             * so seed its untouched pixels from the current GPU depth before
             * applying a partial CPU edit. Whole-surface edits need no copy. */
            if(dirty.left || dirty.top || dirty.right!=width || dirty.bottom!=gpu.s.height)
                ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.s.depth_upload,(ID3D11Resource*)gpu.s.depth);
            ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.s.depth_upload,0,&dirty,
                gpu.s.depth_pixels+((size_t)dirty.top*width+dirty.left)*4,width*4,0);
            ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.s.depth,(ID3D11Resource*)gpu.s.depth_upload);
            surface_uploads++;surface_upload_bytes+=(uint64_t)(dirty.right-dirty.left)*(dirty.bottom-dirty.top)*4;
        }
    }
    timing_add(&timing_surface,tick);tick=timing_now();
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.constants,0,NULL,&constants,0,0);
    viewport.Width=(float)width;viewport.Height=(float)b->height;viewport.MaxDepth=1;
    rect.left=(LONG)box.left;rect.top=(LONG)box.top;
    rect.right=(LONG)box.right;rect.bottom=(LONG)box.bottom;
    ID3D11DeviceContext_RSSetState(gpu.context,gpu.rasterizer);
    ID3D11DeviceContext_RSSetViewports(gpu.context,1,&viewport);
    ID3D11DeviceContext_RSSetScissorRects(gpu.context,1,&rect);
    ID3D11DeviceContext_IASetInputLayout(gpu.context,gpu.layout);
    ID3D11DeviceContext_IASetVertexBuffers(gpu.context,0,1,&gpu.vertices,&stride,&offset);
    ID3D11DeviceContext_IASetPrimitiveTopology(gpu.context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(gpu.context,gpu.vs,NULL,0);
    ID3D11DeviceContext_VSSetConstantBuffers(gpu.context,0,1,&gpu.constants);
    ID3D11DeviceContext_PSSetShader(gpu.context,ps,NULL,0);
    ID3D11DeviceContext_PSSetConstantBuffers(gpu.context,0,1,&gpu.constants);
    ID3D11DeviceContext_PSSetShaderResources(gpu.context,0,4,views);
    ID3D11DeviceContext_PSSetSamplers(gpu.context,0,4,samplers);
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,1,&gpu.s.rtv,gpu.s.dsv);
    ID3D11DeviceContext_Draw(gpu.context,b->count,0);
    timing_add(&timing_dispatch,tick);
    if(gpu.pending) {
        if(box.left<gpu.pending_box.left)gpu.pending_box.left=box.left;
        if(box.top<gpu.pending_box.top)gpu.pending_box.top=box.top;
        if(box.right>gpu.pending_box.right)gpu.pending_box.right=box.right;
        if(box.bottom>gpu.pending_box.bottom)gpu.pending_box.bottom=box.bottom;
    } else { gpu.pending_box=box;gpu.pending_batch=*b; }
    gpu.pending=1;gpu.pending_depth|=write_depth;
    gpu.s.color_source=b->color;gpu.s.color_valid=1;
    /* Resident mode keeps an untouched depth surface valid across depthless
     * draws; its tracked pages still catch CPU edits. */
    if(dbpp || !resident_mode) {
        gpu.s.depth_source=b->depth;gpu.s.depth_pitch=b->depth_pitch;gpu.s.depth_valid=dbpp!=0;
    }
    if(!gpu.scope && !counted_flush(&flush_unscoped))goto failed_resolve;
success:
    accepted++;
    if(accepted==1 || !(accepted%20000))fprintf(stderr,"[D3D11] draws=%llu fallback=%llu upload=%llu readback=%lluMB tex=%llu shaders=%llu cache=%lluMB\n",
        (unsigned long long)accepted,(unsigned long long)fallback,
        (unsigned long long)surface_uploads,(unsigned long long)(readback_bytes>>20),
        (unsigned long long)texture_uploads,(unsigned long long)shader_compiles,
        (unsigned long long)(texture_cache_bytes>>20));
    if(resident_mode && (accepted==1 || !(accepted%20000)))fprintf(stderr,"[D3D11] resident commits=%llu materialize=%llu faults=%llu/%llu protects=%llu host=%llu clears=%llu adopts=%llu\n",
        (unsigned long long)res_commits,(unsigned long long)res_materializes,
        (unsigned long long)res_read_faults,(unsigned long long)res_write_faults,
        (unsigned long long)res_protects,(unsigned long long)res_host_access,(unsigned long long)res_clears,(unsigned long long)res_adopts);
    if(resident_mode && accepted && !(accepted%20000))resident_report();
    timing_add(&timing_draw,start);
    return 1;
fail:
    /* Earlier queued draws must retire before the caller's software fallback.
     * A failed resolve remains pending and must be treated as fatal by a scope
     * owner; it cannot masquerade as successful completion. */
    if(!counted_flush(&flush_fallback))goto failed_resolve;
failed_resolve:
    /* Resident surfaces need no invalidation: a software redraw into a
     * tracked target faults, materializes and untracks it, and the next GPU
     * draw compares and uploads what changed. Otherwise RAM is the only copy
     * once invalidated, so it must be current. */
    if(resident_mode && !gpu.pending) { fallback++;timing_add(&timing_draw,start);return 0; }
    if(!gpu.pending){mat_reason=MR_FAIL;materialize();mat_reason=MR_OTHER;untrack(0);untrack(1);apply();}
    gpu.s.color_valid=gpu.s.depth_valid=0;
    /* The ordinary synchronous caller can redraw this one failed draw in
     * software. A scoped caller owns earlier accepted draws and must stop. */
    if(!gpu.scope)gpu.pending=gpu.pending_depth=0;
    fallback++;timing_add(&timing_draw,start);return 0;
}
/* Clear the current resident target on the GPU. RAM is not written: the
 * cleared rect joins the resident box, so any observer sees it through the
 * ordinary materialize. Stencil is RAM-owned (the GPU depth has none), so
 * its bytes are written directly; readback preserves them. Anything outside
 * the cases below keeps the CPU clear, which the page protection orders. */
/* Bind a target for a clear the way a draw would: unpark its cached set
 * (current when tracked) or upload what RAM holds, then track it. */
static int sync_target(const uint8_t *color,uint32_t pitch,uint32_t height,
                       const uint8_t *depth,uint32_t depth_pitch,uint32_t dbpp)
{
    uint32_t width=pitch/4,x,y,lo,hi;
    D3D11_BOX dirty={0};
    if(!width || pitch%4 || width>16384 || !height || height>16384)return 0;
    if(!page_range(color,(size_t)pitch*height,&lo,&hi))return 0;
    if(dbpp && (!depth || (dbpp!=2 && dbpp!=4) || depth_pitch%dbpp
                || !page_range(depth,(size_t)depth_pitch*height,&lo,&hi)))return 0;
    bind_target(color,width,height,dbpp?depth:NULL,dbpp?depth_pitch:0,dbpp);
    if(!surfaces(width,height,dbpp))return 0;
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    if(!(gpu.s.rng_hi[0]>gpu.s.rng_lo[0] && gpu.s.color_valid && gpu.s.color_source==color)
       && changed_bounds(color,pitch,gpu.s.color_shadow,pitch,width,gpu.s.height,4,
                         !gpu.s.color_valid || gpu.s.color_source!=color,&dirty)) {
        ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.s.color,0,&dirty,
            color+(size_t)dirty.top*pitch+dirty.left*4,pitch,0);
        for(y=dirty.top;y<dirty.bottom;y++) {
            size_t at=(size_t)y*pitch+dirty.left*4;
            memcpy(gpu.s.color_shadow+at,color+at,(size_t)(dirty.right-dirty.left)*4);
        }
        surface_uploads++;
    }
    gpu.s.color_source=color;gpu.s.color_valid=1;
    if(dbpp) {
        uint32_t dwidth=depth_pitch/dbpp;
        float scale=dbpp==2?1.0f/65536.0f:1.0f/16777216.0f;
        if(!(gpu.s.rng_hi[1]>gpu.s.rng_lo[1] && gpu.s.depth_valid && gpu.s.depth_source==depth
             && gpu.s.depth_pitch==depth_pitch)
           && changed_bounds(depth,depth_pitch,gpu.s.depth_shadow,width*dbpp,width<dwidth?width:dwidth,
                             gpu.s.height,dbpp,!gpu.s.depth_valid || gpu.s.depth_source!=depth
                             || gpu.s.depth_pitch!=depth_pitch,&dirty)) {
            for(y=dirty.top;y<dirty.bottom;y++) {
                memcpy(gpu.s.depth_shadow+((size_t)y*width+dirty.left)*dbpp,
                    depth+(size_t)y*depth_pitch+dirty.left*dbpp,(size_t)(dirty.right-dirty.left)*dbpp);
                for(x=dirty.left;x<dirty.right;x++) {
                    const uint8_t *q=depth+(size_t)y*depth_pitch+x*dbpp;
                    float z;
                    if(dbpp==2){uint16_t v;memcpy(&v,q,2);z=(float)v;}
                    else {uint32_t v;memcpy(&v,q,4);z=(float)(v>>8);}
                    z*=scale;
                    memcpy(gpu.s.depth_pixels+((size_t)y*width+x)*4,&z,4);
                }
            }
            if(dirty.left || dirty.top || dirty.right!=width || dirty.bottom!=gpu.s.height)
                ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.s.depth_upload,(ID3D11Resource*)gpu.s.depth);
            ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.s.depth_upload,0,&dirty,
                gpu.s.depth_pixels+((size_t)dirty.top*width+dirty.left)*4,width*4,0);
            ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.s.depth,(ID3D11Resource*)gpu.s.depth_upload);
            surface_uploads++;
        }
        gpu.s.depth_source=depth;gpu.s.depth_pitch=depth_pitch;gpu.s.depth_valid=1;
        page_range(depth,(size_t)depth_pitch*gpu.s.height,&lo,&hi);claim(&gpu.s,1,lo,hi);
    }
    page_range(color,(size_t)pitch*gpu.s.height,&lo,&hi);claim(&gpu.s,0,lo,hi);
    apply();
    return 1;
}
/* A Z clear of a rect: ClearDepthStencilView has none, so draw it. */
static ID3D11VertexShader *zclear_vs;
static ID3D11PixelShader *zclear_ps;
static ID3D11Buffer *zclear_constants;
static ID3D11DepthStencilState *zclear_ds;
static int zclear_failed;
static int clear_depth_rect(const uint32_t r[4],float depth)
{
    static const char vs[]="float4 main(uint id:SV_VertexID):SV_POSITION{"
        "float2 p=float2((id<<1)&2,id&2);return float4(p*float2(2,-2)+float2(-1,1),0,1);}";
    static const char ps[]="cbuffer C:register(b0){float4 z;};float main():SV_Depth{return z.x;}";
    float constants[4]={depth,0,0,0};
    D3D11_VIEWPORT viewport={0};
    D3D11_RECT rect={(LONG)r[0],(LONG)r[1],(LONG)r[2],(LONG)r[3]};
    if(zclear_failed)return 0;
    if(!zclear_vs) {
        ID3DBlob *code=compile(vs,"vs_5_0");
        D3D11_BUFFER_DESC bd={0};
        D3D11_DEPTH_STENCIL_DESC dd={0};
        if(!code || error(ID3D11Device_CreateVertexShader(gpu.device,ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code),NULL,&zclear_vs),"z clear vs")){RELEASE(code);zclear_failed=1;return 0;}
        RELEASE(code);
        code=compile(ps,"ps_5_0");
        if(!code || error(ID3D11Device_CreatePixelShader(gpu.device,ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code),NULL,&zclear_ps),"z clear ps")){RELEASE(code);zclear_failed=1;return 0;}
        RELEASE(code);
        bd.ByteWidth=16;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        dd.DepthEnable=TRUE;dd.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;dd.DepthFunc=D3D11_COMPARISON_ALWAYS;
        if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&zclear_constants),"z clear constants")
           || error(ID3D11Device_CreateDepthStencilState(gpu.device,&dd,&zclear_ds),"z clear state"))
            {zclear_failed=1;return 0;}
    }
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)zclear_constants,0,NULL,constants,0,0);
    viewport.Width=(float)gpu.s.width;viewport.Height=(float)gpu.s.height;viewport.MaxDepth=1;
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,gpu.s.dsv);
    ID3D11DeviceContext_OMSetDepthStencilState(gpu.context,zclear_ds,0);
    ID3D11DeviceContext_RSSetState(gpu.context,gpu.rasterizer);
    ID3D11DeviceContext_RSSetViewports(gpu.context,1,&viewport);
    ID3D11DeviceContext_RSSetScissorRects(gpu.context,1,&rect);
    ID3D11DeviceContext_IASetInputLayout(gpu.context,NULL);
    ID3D11DeviceContext_IASetPrimitiveTopology(gpu.context,D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ID3D11DeviceContext_VSSetShader(gpu.context,zclear_vs,NULL,0);
    ID3D11DeviceContext_PSSetShader(gpu.context,zclear_ps,NULL,0);
    ID3D11DeviceContext_PSSetConstantBuffers(gpu.context,0,1,&zclear_constants);
    ID3D11DeviceContext_Draw(gpu.context,3,0);
    /* The draw path rebinds every state it uses; drop the cached depth state
     * so render_states recreates the binding rather than assuming it. */
    ID3D11DeviceContext_OMSetDepthStencilState(gpu.context,gpu.ds,0);
    return 1;
}
/* Diagnostics: why a clear stayed on the CPU (each costs a readback). */
static void clear_decline(const Nv2aRenderClear *c,unsigned line)
{
    static unsigned shown;
    note_fail(300000+line);
    if(shown++<12)fprintf(stderr,"[D3D11] clear declined at %u: flags=%02X rect=%u,%u,%u,%u zrect=%u,%u,%u,%u %ux%u pitch=%u bound=%d dbpp=%u/%u\n",
        line,c->flags,c->rect[0],c->rect[1],c->rect[2],c->rect[3],c->depth_rect[0],c->depth_rect[1],
        c->depth_rect[2],c->depth_rect[3],c->pitch/4,c->height,c->pitch,gpu.s.color_source==c->color,
        c->depth_bpp,gpu.s.depth_bpp);
}
static int clear_locked(const Nv2aRenderClear *c)
{
    int color=(c->flags&0xF0)!=0,z=(c->flags&1)!=0,stencil=(c->flags&2)!=0,partial_z=0;
    uint32_t r[4],x,y;
    D3D11_BOX box;
    if(!flush_locked() || !gpu.context1){clear_decline(c,__LINE__);return 0;}
    /* ClearView writes whole pixels; per-channel masks stay on the CPU. */
    if(color && (c->flags&0xF0)!=0xF0){clear_decline(c,__LINE__);return 0;}
    if(!gpu.s.color_valid || gpu.s.color_source!=c->color || c->pitch!=gpu.s.width*4
       || c->height>gpu.s.height || gpu.s.rng_hi[0]<=gpu.s.rng_lo[0]) {
        /* A different target cleared in full (the next back buffer) is
         * adopted: every pixel is defined by the clear, so nothing needs
         * uploading. Its depth is adopted only when Z is cleared in full. */
        uint32_t width=c->pitch/4,dbpp=c->depth?c->depth_bpp:0,lo,hi;
        int adopt_depth=z && dbpp && !c->depth_rect[0] && !c->depth_rect[1]
            && c->depth_rect[2]>=width && c->depth_rect[3]>=c->height && c->depth_pitch/dbpp>=width;
        int full=color && !(c->pitch%4) && width && width<=16384 && c->height && c->height<=16384
            && !c->rect[0] && !c->rect[1] && c->rect[2]>=width && c->rect[3]>=c->height
            && page_range(c->color,(size_t)c->pitch*c->height,&lo,&hi)
            && (!(z || stencil) || adopt_depth);
        if(!full) {
            /* A partial or depth-only clear needs the target's other pixels:
             * bind it as a draw would, then clear in place below. */
            if(!sync_target(c->color,c->pitch,c->height,(z || stencil)?c->depth:NULL,
                            (z || stencil)?c->depth_pitch:0,(z || stencil)?dbpp:0))
                {clear_decline(c,__LINE__);return 0;}
            res_synced_clears++;
            goto bound;
        }
        if(adopt_depth && !page_range(c->depth,(size_t)c->depth_pitch*c->height,&lo,&hi)){clear_decline(c,__LINE__);return 0;}
        bind_target(c->color,width,c->height,adopt_depth?c->depth:NULL,adopt_depth?c->depth_pitch:0,
                    adopt_depth?dbpp:0);
        if(!surfaces(width,c->height,adopt_depth?dbpp:0)){clear_decline(c,__LINE__);return 0;}
        gpu.s.color_source=c->color;gpu.s.color_valid=1;
        if(adopt_depth) {
            gpu.s.depth_source=c->depth;gpu.s.depth_pitch=c->depth_pitch;gpu.s.depth_valid=1;
            page_range(c->depth,(size_t)c->depth_pitch*c->height,&lo,&hi);claim(&gpu.s,1,lo,hi);
        } else { untrack(1);gpu.s.depth_valid=0; }
        page_range(c->color,(size_t)c->pitch*c->height,&lo,&hi);claim(&gpu.s,0,lo,hi);
        res_adopts++;
    }
bound:
    if(z || stencil) {
        if(!c->depth || c->depth!=gpu.s.depth_source || !gpu.s.depth_valid || c->depth_pitch!=gpu.s.depth_pitch
           || c->depth_bpp!=gpu.s.depth_bpp || gpu.s.rng_hi[1]<=gpu.s.rng_lo[1] || !gpu.s.dsv){clear_decline(c,__LINE__);return 0;}
        if(stencil && c->depth_bpp!=4)stencil=0;
        /* ClearDepthStencilView has no rect; a partial Z clear is drawn. */
        partial_z=z && (c->depth_rect[0] || c->depth_rect[1] || c->depth_rect[2]<gpu.s.width
                        || c->depth_rect[3]<gpu.s.height || c->depth_pitch/c->depth_bpp<gpu.s.width);
    }
    box.left=gpu.s.width;box.top=gpu.s.height;box.right=box.bottom=0;box.front=0;box.back=1;
    if(color) {
        memcpy(r,c->rect,sizeof r);
        if(r[2]>gpu.s.width)r[2]=gpu.s.width;
        if(r[3]>gpu.s.height)r[3]=gpu.s.height;
        if(r[0]<r[2] && r[1]<r[3]) {
            D3D11_RECT rect={(LONG)r[0],(LONG)r[1],(LONG)r[2],(LONG)r[3]};
            float v[4]={(float)((c->color_value>>16)&255)/255.0f,(float)((c->color_value>>8)&255)/255.0f,
                        (float)(c->color_value&255)/255.0f,(float)(c->color_value>>24)/255.0f};
            ID3D11DeviceContext1_ClearView(gpu.context1,(ID3D11View*)gpu.s.rtv,v,&rect,1);
            box.left=r[0];box.top=r[1];box.right=r[2];box.bottom=r[3];
        }
    }
    if(stencil) {
        memcpy(r,c->depth_rect,sizeof r);
        if(r[2]>gpu.s.width)r[2]=gpu.s.width;
        if(r[3]>gpu.s.height)r[3]=gpu.s.height;
        /* The shadow is RAM as of the last sync, and only this path or a
         * (faulting) CPU write changes stencil; usually it is already clear. */
        {
            int same=1;
            uint8_t want=(uint8_t)c->zstencil;
            for(y=r[1];y<r[3] && same;y++) {
                const uint8_t *sh=gpu.s.depth_shadow+((size_t)y*gpu.s.width+r[0])*4;
                for(x=r[0];x<r[2];x++,sh+=4)if(*sh!=want){same=0;break;}
            }
            if(!same) {
                res_writing=1;apply();
                for(y=r[1];y<r[3];y++)for(x=r[0];x<r[2];x++) {
                    size_t at=(size_t)y*c->depth_pitch+x*4;
                    c->depth[at]=want;
                    gpu.s.depth_shadow[((size_t)y*gpu.s.width+x)*4]=want;
                }
                res_writing=0;
            }
        }
    }
    if(z) {
        float depth=c->depth_bpp==2?(float)(c->zstencil&0xFFFF)*(1.0f/65536.0f)
                                   :(float)(c->zstencil>>8)*(1.0f/16777216.0f);
        if(partial_z) {
            uint32_t zr[4];
            memcpy(zr,c->depth_rect,sizeof zr);
            if(zr[2]>gpu.s.width)zr[2]=gpu.s.width;
            if(zr[3]>gpu.s.height)zr[3]=gpu.s.height;
            if(zr[0]<zr[2] && zr[1]<zr[3]) {
                if(!clear_depth_rect(zr,depth)){clear_decline(c,__LINE__);return 0;}
                if(zr[0]<box.left)box.left=zr[0];
                if(zr[1]<box.top)box.top=zr[1];
                if(zr[2]>box.right)box.right=zr[2];
                if(zr[3]>box.bottom)box.bottom=zr[3];
            }
        } else {
            ID3D11DeviceContext_ClearDepthStencilView(gpu.context,gpu.s.dsv,D3D11_CLEAR_DEPTH,depth,0);
            box.left=box.top=0;box.right=gpu.s.width;box.bottom=gpu.s.height;
        }
    }
    if(box.left<box.right && box.top<box.bottom) {
        if(gpu.s.res_valid) {
            D3D11_BOX *b=&gpu.s.res_box;
            if(box.left<b->left)b->left=box.left;
            if(box.top<b->top)b->top=box.top;
            if(box.right>b->right)b->right=box.right;
            if(box.bottom>b->bottom)b->bottom=box.bottom;
        } else {
            memset(&gpu.s.res_batch,0,sizeof gpu.s.res_batch);
            gpu.s.res_batch.color=(uint8_t*)gpu.s.color_source;gpu.s.res_batch.pitch=gpu.s.width*4;
            gpu.s.res_batch.height=gpu.s.height;gpu.s.res_box=box;gpu.s.res_valid=1;gpu.s.res_depth=0;
        }
        if(z) {
            gpu.s.res_batch.depth=(uint8_t*)gpu.s.depth_source;gpu.s.res_batch.depth_pitch=gpu.s.depth_pitch;
            gpu.s.res_depth=1;
        }
    }
    res_clears++;
    apply();
    return 1;
}
/* Display copy of a finished resident surface, one frame behind. Reading
 * the surface from RAM forced a full color+depth materialize each flip and
 * drained the GPU; a GPU copy into a staging ring is read back a flip later,
 * when it has long completed, and the surface stays resident. */
#define PRESENT_RING 2
static ID3D11Texture2D *present_ring[PRESENT_RING];
static uint32_t present_width, present_height;
static uint64_t present_frames;
static int present_locked(const uint8_t *color,uint32_t pitch,uint32_t height,uint8_t *out)
{
    D3D11_MAPPED_SUBRESOURCE m={0};
    D3D11_BOX box={0,0,0,0,0,1};
    SurfaceSet *t=NULL;
    unsigned i,k,y;
    if(!flush_locked())return 0;
    for(i=0;i<=SURFACE_SLOTS && !t;i++) {
        SurfaceSet *c=set_at(i);
        if((!i || c->used) && c->color && c->color_valid && c->color_source==color
           && c->width*4==pitch && c->height>=height && has_range(c,0))t=c;
    }
    if(!t)return 0;
    if(present_width!=t->width || present_height!=height) {
        D3D11_TEXTURE2D_DESC td={0};
        for(k=0;k<PRESENT_RING;k++)RELEASE(present_ring[k]);
        td.Width=t->width;td.Height=height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
        td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        for(k=0;k<PRESENT_RING;k++)
            if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&present_ring[k]),"present staging")) {
                for(k=0;k<PRESENT_RING;k++)RELEASE(present_ring[k]);
                present_width=present_height=0;return 0;
            }
        present_width=t->width;present_height=height;present_frames=0;
    }
    k=(unsigned)(present_frames%PRESENT_RING);
    box.right=t->width;box.bottom=height;
    ID3D11DeviceContext_CopySubresourceRegion(gpu.context,(ID3D11Resource*)present_ring[k],0,0,0,0,
        (ID3D11Resource*)t->color,0,&box);
    /* Show the previous copy; the very first flip waits for its own. */
    if(present_frames)k=(k+PRESENT_RING-1)%PRESENT_RING;
    present_frames++;
    if(error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)present_ring[k],0,D3D11_MAP_READ,0,&m),"map present"))
        return 0;
    for(y=0;y<height;y++)memcpy(out+(size_t)y*pitch,(const uint8_t*)m.pData+(size_t)y*m.RowPitch,pitch);
    ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)present_ring[k],0);
    return 1;
}
int nv2a_render_present(const uint8_t *color,uint32_t pitch,uint32_t height,uint8_t *out)
{
    int ok;
    if(!resident_mode || !gpu.enabled)return 0;
    EnterCriticalSection(&render_cs);ok=present_locked(color,pitch,height,out);LeaveCriticalSection(&render_cs);
    return ok;
}
int nv2a_render_clear(const Nv2aRenderClear *c)
{
    int ok;
    if(!resident_mode || !gpu.enabled)return 0;
    EnterCriticalSection(&render_cs);ok=clear_locked(c);LeaveCriticalSection(&render_cs);
    return ok;
}
void nv2a_render_shutdown(void)
{
    unsigned i;
    if(gpu.enabled) {
        EnterCriticalSection(&render_cs);
        flush_locked();materialize();untrack(0);untrack(1);
        for(i=0;i<SURFACE_SLOTS;i++)if(parked[i].used)drop_set(&parked[i]);
        apply();
        LeaveCriticalSection(&render_cs);
    }
    if(gpu.context)ID3D11DeviceContext_ClearState(gpu.context);
    release_surfaces();
    for(i=0;i<SHADER_SLOTS;i++)RELEASE(gpu.shaders[i].shader);
    for(i=0;i<TEXTURE_SLOTS;i++)release_texture(&gpu.textures[i]);
    for(i=0;i<4;i++)RELEASE(gpu.samplers[i]);
    for(i=0;i<64;i++){RELEASE(sampler_cache[i].state);sampler_cache[i].key=0;}
    RELEASE(gpu.blend);RELEASE(gpu.ds);RELEASE(gpu.rasterizer);RELEASE(gpu.vertices);
    RELEASE(gpu.pack_constants);RELEASE(gpu.pack_shader);
    RELEASE(snapshot_view);RELEASE(snapshot);
    RELEASE(zclear_vs);RELEASE(zclear_ps);RELEASE(zclear_constants);RELEASE(zclear_ds);
    RELEASE(depth_texel_uav);RELEASE(depth_texel_view);RELEASE(depth_texel);
    RELEASE(depth_stencil_view);RELEASE(depth_stencil_plane);RELEASE(depth_texel_shader);
    free(stencil_plane);stencil_plane=NULL;
    for(i=0;i<PRESENT_RING;i++)RELEASE(present_ring[i]);
    RELEASE(gpu.context1);
    RELEASE(gpu.constants);RELEASE(gpu.layout);RELEASE(gpu.vs);RELEASE(gpu.context);RELEASE(gpu.device);
    memset(&gpu,0,sizeof gpu);
}
#else
int nv2a_render_enabled(void) { return 0; }
int nv2a_render_draw(const Nv2aRenderBatch *batch) { (void)batch;return 0; }
int nv2a_render_begin(void) { return 0; }
int nv2a_render_flush(void) { return 1; }
int nv2a_render_end(void) { return 1; }
int nv2a_render_clear(const Nv2aRenderClear *clear) { (void)clear;return 0; }
int nv2a_render_present(const uint8_t *color,uint32_t pitch,uint32_t height,uint8_t *out)
{ (void)color;(void)pitch;(void)height;(void)out;return 0; }
int nv2a_render_scoped(void) { return 0; }
void nv2a_render_require_flush(void) {}
void nv2a_render_observe(Nv2aRenderObserver observer) { (void)observer; }
void nv2a_render_memory_read(const void *source,size_t bytes) { (void)source;(void)bytes; }
void nv2a_render_shutdown(void) {}
void nv2a_render_set_window(void *contig,void *tiled,size_t tiled_bytes) { (void)contig;(void)tiled;(void)tiled_bytes; }
void nv2a_render_host_access(void *p,size_t bytes,int write) { (void)p;(void)bytes;(void)write; }
#endif
