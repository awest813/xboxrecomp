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
#include <d3dcompiler.h>

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
typedef struct {
    Nv2aCombiner key;
    int has_combiner, valid;
    uint64_t age;
    ID3D11PixelShader *shader;
} Shader;
typedef struct {
    Nv2aRenderTexture key;
    uint8_t *shadow, *palette;
    ID3D11Texture2D *texture;
    ID3D11ShaderResourceView *view;
    uint64_t age;
    size_t bytes;
} Texture;
static struct {
    ID3D11Device *device;
    ID3D11DeviceContext *context;
    ID3D11VertexShader *vs;
    ID3D11InputLayout *layout;
    ID3D11Buffer *vertices, *constants;
    size_t vertex_bytes;
    ID3D11RasterizerState *rasterizer;
    ID3D11SamplerState *samplers[4];
    ID3D11BlendState *blend;
    D3D11_BLEND_DESC blend_desc;
    ID3D11DepthStencilState *ds;
    D3D11_DEPTH_STENCIL_DESC ds_desc;
    ID3D11Texture2D *color, *color_read;
    ID3D11RenderTargetView *rtv;
    ID3D11Texture2D *depth, *depth_upload, *depth_read;
    ID3D11DepthStencilView *dsv;
    ID3D11ShaderResourceView *color_view,*depth_view;
    ID3D11ComputeShader *pack_shader;
    ID3D11Buffer *pack_constants,*pack_result,*pack_read;
    ID3D11UnorderedAccessView *pack_uav;
    int pack_requested,pack_failed;
    uint32_t width,height,depth_bpp;
    uint8_t *depth_pixels;
    uint8_t *color_shadow, *depth_shadow;
    const uint8_t *color_source, *depth_source;
    uint32_t depth_pitch;
    int color_valid, depth_valid;
    int scope, pending, pending_depth;
    Nv2aRenderBatch pending_batch;
    D3D11_BOX pending_box;
    Shader shaders[SHADER_SLOTS];
    Texture textures[TEXTURE_SLOTS];
    uint64_t age;
    int enabled, initialized;
} gpu;

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
    gpu.enabled=1;
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
    RELEASE(gpu.pack_uav);RELEASE(gpu.pack_result);RELEASE(gpu.pack_read);
    RELEASE(gpu.color_view);RELEASE(gpu.depth_view);
    RELEASE(gpu.rtv);RELEASE(gpu.color);RELEASE(gpu.color_read);
    RELEASE(gpu.dsv);RELEASE(gpu.depth);RELEASE(gpu.depth_upload);RELEASE(gpu.depth_read);
    free(gpu.depth_pixels);gpu.depth_pixels=NULL;
    free(gpu.color_shadow);gpu.color_shadow=NULL;
    free(gpu.depth_shadow);gpu.depth_shadow=NULL;
    gpu.color_valid=gpu.depth_valid=0;
    gpu.color_source=gpu.depth_source=NULL;gpu.depth_pitch=0;
    gpu.width=gpu.height=gpu.depth_bpp=0;
}
static int surfaces(uint32_t width,uint32_t height,uint32_t depth_bpp)
{
    D3D11_TEXTURE2D_DESC td={0};
    D3D11_DEPTH_STENCIL_VIEW_DESC dd={0};
    /* Disabling depth changes the draw state, not the color allocation.
     * Keep a compatible depth allocation for the next depth-enabled draw;
     * that draw still revalidates RAM through depth_valid/changed_bounds. */
    if(gpu.width==width && gpu.height==height
       && (!depth_bpp || gpu.depth_bpp==depth_bpp))return 1;
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    release_surfaces();
    td.Width=width;td.Height=height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.color),"color surface"))return 0;
    if(error(ID3D11Device_CreateRenderTargetView(gpu.device,(ID3D11Resource*)gpu.color,NULL,&gpu.rtv),"color view"))return 0;
    td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.color_read),"color readback"))return 0;
    gpu.color_shadow=(uint8_t*)malloc((size_t)width*height*4);
    if(!gpu.color_shadow)return 0;
    if(depth_bpp) {
        /* Store integer guest Z as an exactly representable power-of-two
         * fraction. Host D24 UNORM conversion/division can shift a high Z24
         * integer by one or two, changing EQUAL/LESS at adjacent values.
         * D32_FLOAT preserves all 24 guest bits. Stencil stays in guest RAM. */
        td.Format=DXGI_FORMAT_R32_TYPELESS;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.depth_read),"depth readback"))return 0;
        td.Usage=D3D11_USAGE_DEFAULT;td.CPUAccessFlags=0;
        td.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.depth),"depth surface"))return 0;
        dd.Format=DXGI_FORMAT_D32_FLOAT;
        dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        if(error(ID3D11Device_CreateDepthStencilView(gpu.device,(ID3D11Resource*)gpu.depth,&dd,&gpu.dsv),"depth view"))return 0;
        td.BindFlags=0;
        if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&gpu.depth_upload),"depth upload"))return 0;
        gpu.depth_pixels=(uint8_t*)malloc((size_t)width*height*4);
        gpu.depth_shadow=(uint8_t*)malloc((size_t)width*height*depth_bpp);
        if(!gpu.depth_pixels || !gpu.depth_shadow)return 0;
    }
    gpu.width=width;gpu.height=height;gpu.depth_bpp=depth_bpp;
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
static void release_texture(Texture *t)
{
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
    required=in->source_bytes+in->palette_bytes+(size_t)in->width*in->height*4;
    if(required>TEXTURE_BUDGET)return NULL;
    tick=timing_now();
    for(i=0;i<TEXTURE_SLOTS;i++) {
        Texture *candidate=&gpu.textures[i];
        const Nv2aRenderTexture *k=&candidate->key;
        if(candidate->view && k->source==in->source && k->source_bytes==in->source_bytes
           && k->palette==in->palette && k->palette_bytes==in->palette_bytes
           && k->width==in->width && k->height==in->height && k->format==in->format && k->pitch==in->pitch) {
            t=candidate;break;
        }
        if(!candidate->view){slot=i;oldest=0;}
        else if(oldest && candidate->age<oldest){slot=i;oldest=candidate->age;}
    }
    timing_add(&timing_tex_lookup,tick);tick=timing_now();
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
    memcpy(t->shadow,in->source,in->source_bytes);
    if(in->palette_bytes)memcpy(t->palette,in->palette,in->palette_bytes);
    td.Width=in->width;td.Height=in->height;td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
    td.Format=DXGI_FORMAT_B8G8R8A8_UNORM;td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    if(error(ID3D11Device_CreateTexture2D(gpu.device,&td,NULL,&t->texture),"texture")
       || error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)t->texture,NULL,&t->view),"texture view")) {
        free(decoded);release_texture(t);return NULL;
    }
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)t->texture,0,NULL,decoded,in->width*4,0);
    free(decoded);t->key=*in;t->age=gpu.age;t->bytes=required;
    texture_cache_bytes+=required;texture_uploads++;
    timing_add(&timing_tex_upload,tick);return t->view;
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
    if(gpu.pack_uav)return 1;
    if(!gpu.pack_shader) {
        code=compile(source,"cs_5_0");if(!code)goto fail;
        hr=ID3D11Device_CreateComputeShader(gpu.device,ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code),NULL,&gpu.pack_shader);
        RELEASE(code);
        if(error(hr,"pack shader"))goto fail;
        bd.ByteWidth=32;bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.pack_constants),"pack constants"))goto fail;
    }
    if(error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)gpu.color,NULL,&gpu.color_view),"pack color view"))goto fail;
    sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels=1;
    if(error(ID3D11Device_CreateShaderResourceView(gpu.device,(ID3D11Resource*)gpu.depth,&sd,&gpu.depth_view),"pack depth view"))goto fail;
    memset(&bd,0,sizeof bd);
    bd.ByteWidth=gpu.width*gpu.height*8;
    bd.BindFlags=D3D11_BIND_UNORDERED_ACCESS;bd.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.pack_result),"pack output"))goto fail;
    ud.Format=DXGI_FORMAT_R32_TYPELESS;ud.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;
    ud.Buffer.NumElements=bd.ByteWidth/4;ud.Buffer.Flags=D3D11_BUFFER_UAV_FLAG_RAW;
    if(error(ID3D11Device_CreateUnorderedAccessView(gpu.device,(ID3D11Resource*)gpu.pack_result,&ud,&gpu.pack_uav),"pack output view"))goto fail;
    bd.BindFlags=bd.MiscFlags=0;bd.Usage=D3D11_USAGE_STAGING;bd.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.pack_read),"pack staging"))goto fail;
    return 1;
fail:
    gpu.pack_failed=1;
    RELEASE(gpu.pack_uav);RELEASE(gpu.pack_result);RELEASE(gpu.pack_read);
    RELEASE(gpu.color_view);RELEASE(gpu.depth_view);
    return 0;
}

static int packed_readback(const Nv2aRenderBatch *b,const D3D11_BOX *box,unsigned dbpp)
{
    ID3D11ShaderResourceView *views[2]={gpu.color_view,gpu.depth_view};
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
    views[0]=gpu.color_view;views[1]=gpu.depth_view;
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.pack_constants,0,NULL,constants,0,0);
    ID3D11DeviceContext_CSSetShader(gpu.context,gpu.pack_shader,NULL,0);
    ID3D11DeviceContext_CSSetConstantBuffers(gpu.context,0,1,&gpu.pack_constants);
    ID3D11DeviceContext_CSSetShaderResources(gpu.context,0,2,views);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(gpu.context,0,1,&gpu.pack_uav,NULL);
    ID3D11DeviceContext_Dispatch(gpu.context,(width+15)/16,(height+15)/16,1);
    ID3D11DeviceContext_CSSetShaderResources(gpu.context,0,2,none);
    ID3D11DeviceContext_CSSetUnorderedAccessViews(gpu.context,0,1,&no_uav,NULL);
    ID3D11DeviceContext_CopySubresourceRegion(gpu.context,(ID3D11Resource*)gpu.pack_read,0,0,0,0,
        (ID3D11Resource*)gpu.pack_result,0,&bytes);
    timing_add(&timing_submit,tick);tick=timing_now();
    if(error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.pack_read,0,D3D11_MAP_READ,0,&mapped),"map packed result")) {
        timing_add(&timing_map,tick);
        gpu.pack_failed=1;return 0;
    }
    timing_add(&timing_map,tick);tick=timing_now();
    data=(const uint32_t*)mapped.pData;
    for(y=0;y<height;y++)for(x=0;x<width;x++) {
        size_t at=((size_t)y*width+x)*2;
        size_t color=(size_t)(box->top+y)*b->pitch+(box->left+x)*4;
        size_t depth=(size_t)(box->top+y)*b->depth_pitch+(box->left+x)*dbpp;
        size_t shadow=((size_t)(box->top+y)*gpu.width+box->left+x)*dbpp;
        uint32_t z=data[at+1];
        memcpy(b->color+color,data+at,4);memcpy(gpu.color_shadow+color,data+at,4);
        if(dbpp==2) { uint16_t v=(uint16_t)z;memcpy(b->depth+depth,&v,2); }
        else { uint32_t v;memcpy(&v,b->depth+depth,4);v=(z<<8)|(v&255);memcpy(b->depth+depth,&v,4); }
        memcpy(gpu.depth_shadow+shadow,b->depth+depth,dbpp);
    }
    ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.pack_read,0);
    timing_add(&timing_commit,tick);
    packed_readbacks++;
    return 1;
}

int nv2a_render_flush(void)
{
    const Nv2aRenderBatch *b=&gpu.pending_batch;
    D3D11_BOX box=gpu.pending_box;
    D3D11_MAPPED_SUBRESOURCE cr={0},dr={0};
    unsigned x,y,dbpp=gpu.depth_bpp,width=gpu.width;
    int write_depth=gpu.pending_depth;
    uint64_t start,tick,map_before;
    if(!gpu.pending)return 1;
    start=timing_now();
    map_before=timing_map;
    if(write_depth && packed_readback(b,&box,dbpp))goto committed;
    tick=timing_now();
    ID3D11DeviceContext_CopySubresourceRegion(gpu.context,(ID3D11Resource*)gpu.color_read,0,0,0,0,
        (ID3D11Resource*)gpu.color,0,&box);
    if(write_depth) {
        /* Depth-stencil sources require a whole-resource copy first. */
        ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.depth_upload,(ID3D11Resource*)gpu.depth);
        ID3D11DeviceContext_CopySubresourceRegion(gpu.context,(ID3D11Resource*)gpu.depth_read,0,0,0,0,
            (ID3D11Resource*)gpu.depth_upload,0,&box);
    }
    timing_add(&timing_submit,tick);tick=timing_now();
    if(error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.color_read,0,D3D11_MAP_READ,0,&cr),"map color result"))goto failed;
    if(write_depth && error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.depth_read,0,D3D11_MAP_READ,0,&dr),"map depth result")) {
        ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.color_read,0);goto failed;
    }
    timing_add(&timing_map,tick);tick=timing_now();
    /* Commit only after both mappings succeed. */
    for(y=box.top;y<box.bottom;y++) {
        size_t dst=(size_t)y*b->pitch+box.left*4,row=(size_t)(box.right-box.left)*4;
        const uint8_t *src=(uint8_t*)cr.pData+(size_t)(y-box.top)*cr.RowPitch;
        memcpy(b->color+dst,src,row);memcpy(gpu.color_shadow+dst,src,row);
    }
    if(write_depth) {
        for(y=box.top;y<box.bottom;y++)for(x=box.left;x<box.right;x++) {
            uint8_t *p=b->depth+(size_t)y*b->depth_pitch+x*dbpp;
            const uint8_t *q=(uint8_t*)dr.pData+(size_t)(y-box.top)*dr.RowPitch+(x-box.left)*4;
            float f;uint32_t z;
            memcpy(&f,q,4);z=(uint32_t)(f*(dbpp==2?65536.0f:16777216.0f));
            if(dbpp==2){uint16_t v=(uint16_t)z;memcpy(p,&v,2);}
            else {uint32_t v;memcpy(&v,p,4);v=(z<<8)|(v&255);memcpy(p,&v,4);}
            memcpy(gpu.depth_shadow+((size_t)y*width+x)*dbpp,p,dbpp);
        }
        ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.depth_read,0);
    }
    ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.color_read,0);
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
    gpu.pending=gpu.pending_depth=0;
    readback_bytes+=(uint64_t)(box.right-box.left)*(box.bottom-box.top)*(write_depth?8:4);
    timing_add(&timing_flush,start);
    return 1;
failed:
    timing_add(&timing_map,tick);timing_add(&timing_flush,start);
    return 0;
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
    ok=nv2a_render_flush();readback_reason=previous;return ok;
}
int nv2a_render_begin(void)
{
    if(!initialize() || gpu.scope || gpu.pending)return 0;
    gpu.scope=1;return 1;
}
int nv2a_render_end(void)
{
    if(!counted_flush(&flush_scope_end))return 0;
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
    uint64_t before=readback_bytes,resolves=flush_total;
    if(i>=NV2A_OBSERVER_COUNT)i=NV2A_OBSERVER_OTHER;
    observer_calls[i]++;
    require_flush(&flush_external);
    observer_resolves[i]+=flush_total-resolves;
    observer_bytes[i]+=readback_bytes-before;
}
void nv2a_render_require_flush(void) { nv2a_render_observe(NV2A_OBSERVER_OTHER); }
void nv2a_render_memory_read(const void *source,size_t bytes)
{
    const Nv2aRenderBatch *b=&gpu.pending_batch;
    if(gpu.pending && (overlaps(source,bytes,b->color,(size_t)b->pitch*b->height)
        || (gpu.pending_depth && overlaps(source,bytes,b->depth,(size_t)b->depth_pitch*b->height))))
        require_flush(&flush_memory_alias);
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

int nv2a_render_draw(const Nv2aRenderBatch *b)
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
    if(b->color_bpp!=4 || !b->pitch || b->pitch%4 || !b->height || !b->color
       || !b->vertices || !b->count || b->count%3)goto fail;
    width=b->pitch/4;
    if(width>16384 || b->height>16384)goto fail;
    dbpp=b->depth_test && b->depth?b->depth_bpp:0;
    if(dbpp && dbpp!=2 && dbpp!=4)goto fail;
    if(dbpp){if(!b->depth_pitch || b->depth_pitch%dbpp)goto fail;dwidth=b->depth_pitch/dbpp;}
    bounds=draw_bounds(b,width,dwidth,&box);
    if(bounds<0)goto fail;
    if(!bounds)goto success;
    write_depth=dbpp && b->depth_mask;
    if(gpu.pending && (width!=gpu.width || b->height!=gpu.height
        /* Allocation depth can outlive a disabled interval. Compare the
         * pending draw's active depth state so HUD draws still coalesce. */
        || dbpp!=(gpu.pending_batch.depth_test && gpu.pending_batch.depth?
                   gpu.pending_batch.depth_bpp:0)
        || b->color!=gpu.color_source || b->depth!=gpu.depth_source || b->depth_pitch!=gpu.depth_pitch
        /* Overlapping color/depth RAM requires the ordinary commit/upload
         * ordering between draws, including read-only depth aliasing color. */
        || (dbpp && overlaps(b->color,(size_t)b->pitch*b->height,
                             b->depth,(size_t)b->depth_pitch*b->height))))
        if(!counted_flush(&flush_target))goto failed_resolve;
    for(i=0;gpu.pending && i<4;i++) {
        unsigned mode=b->has_combiner?(b->combiner.stage_program>>(5*i))&31:(i?0:1);
        if(b->textures[i].valid && mode==1 && pending_texture(&b->textures[i]))
            if(!counted_flush(&flush_texture_alias))goto failed_resolve;
    }
    tick=timing_now();
    if(!surfaces(width,b->height,dbpp) || !render_states(b))goto fail;
    ps=shader(b);if(!ps)goto fail;
    for(i=0;i<4;i++) {
        const Nv2aRenderTexture *t=&b->textures[i];
        unsigned mode=b->has_combiner?(b->combiner.stage_program>>(5*i))&31:(i?0:1);
        if(t->valid && mode==1) {
            part=timing_now();views[i]=texture(t);timing_add(&timing_texture,part);
            if(!views[i])goto fail;
        }
        samplers[i]=gpu.samplers[(t->addr_u==1?1:0)|(t->addr_v==1?2:0)];
        constants.texture[i][0]=(float)t->width;constants.texture[i][1]=(float)t->height;
        constants.texture[i][2]=(float)t->normalized;constants.texture[i][3]=views[i]?1.0f:0.0f;
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
    if(bytes>gpu.vertex_bytes) {
        D3D11_BUFFER_DESC bd={0};
        RELEASE(gpu.vertices);gpu.vertex_bytes=0;
        if(bytes>UINT32_MAX)goto fail;
        bd.ByteWidth=(UINT)bytes;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;
        bd.Usage=D3D11_USAGE_DYNAMIC;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if(error(ID3D11Device_CreateBuffer(gpu.device,&bd,NULL,&gpu.vertices),"vertex buffer"))goto fail;
        gpu.vertex_bytes=bytes;
    }
    if(error(ID3D11DeviceContext_Map(gpu.context,(ID3D11Resource*)gpu.vertices,0,D3D11_MAP_WRITE_DISCARD,0,&vb),"map vertices"))goto fail;
    memcpy(vb.pData,b->vertices,bytes);ID3D11DeviceContext_Unmap(gpu.context,(ID3D11Resource*)gpu.vertices,0);
    timing_add(&timing_prepare,tick);tick=timing_now();
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,0,NULL,NULL);
    part=timing_now();
    color_changed=!gpu.pending && changed_bounds(b->color,b->pitch,gpu.color_shadow,b->pitch,width,b->height,4,
        !gpu.color_valid || gpu.color_source!=b->color,&dirty);
    timing_add(&timing_compare,part);
    if(color_changed) {
        ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.color,0,&dirty,
            b->color+(size_t)dirty.top*b->pitch+dirty.left*4,b->pitch,0);
        for(y=dirty.top;y<dirty.bottom;y++) {
            size_t at=(size_t)y*b->pitch+dirty.left*4;
            memcpy(gpu.color_shadow+at,b->color+at,(size_t)(dirty.right-dirty.left)*4);
        }
        surface_uploads++;surface_upload_bytes+=(uint64_t)(dirty.right-dirty.left)*(dirty.bottom-dirty.top)*4;
    }
    if(dbpp) {
        part=timing_now();
        depth_changed=!gpu.pending && changed_bounds(b->depth,b->depth_pitch,gpu.depth_shadow,width*dbpp,
            width<dwidth?width:dwidth,b->height,dbpp,
            !gpu.depth_valid || gpu.depth_source!=b->depth || gpu.depth_pitch!=b->depth_pitch,&dirty);
        timing_add(&timing_compare,part);
        if(depth_changed) {
            for(y=dirty.top;y<dirty.bottom;y++) {
                memcpy(gpu.depth_shadow+((size_t)y*width+dirty.left)*dbpp,
                    b->depth+(size_t)y*b->depth_pitch+dirty.left*dbpp,(size_t)(dirty.right-dirty.left)*dbpp);
                for(x=dirty.left;x<dirty.right;x++) {
                    const uint8_t *p=b->depth+(size_t)y*b->depth_pitch+x*dbpp;
                    float z;
                    if(dbpp==2){uint16_t v;memcpy(&v,p,2);z=(float)v;}
                    else {uint32_t v;memcpy(&v,p,4);z=(float)(v>>8);}
                    z*=constants.surface[3];
                    memcpy(gpu.depth_pixels+((size_t)y*width+x)*4,&z,4);
                }
            }
            /* CopyResource back into the depth-stencil target replaces the
             * entire surface. Packed readback does not refresh depth_upload,
             * so seed its untouched pixels from the current GPU depth before
             * applying a partial CPU edit. Whole-surface edits need no copy. */
            if(dirty.left || dirty.top || dirty.right!=width || dirty.bottom!=b->height)
                ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.depth_upload,(ID3D11Resource*)gpu.depth);
            ID3D11DeviceContext_UpdateSubresource(gpu.context,(ID3D11Resource*)gpu.depth_upload,0,&dirty,
                gpu.depth_pixels+((size_t)dirty.top*width+dirty.left)*4,width*4,0);
            ID3D11DeviceContext_CopyResource(gpu.context,(ID3D11Resource*)gpu.depth,(ID3D11Resource*)gpu.depth_upload);
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
    ID3D11DeviceContext_OMSetRenderTargets(gpu.context,1,&gpu.rtv,gpu.dsv);
    ID3D11DeviceContext_Draw(gpu.context,b->count,0);
    timing_add(&timing_dispatch,tick);
    if(gpu.pending) {
        if(box.left<gpu.pending_box.left)gpu.pending_box.left=box.left;
        if(box.top<gpu.pending_box.top)gpu.pending_box.top=box.top;
        if(box.right>gpu.pending_box.right)gpu.pending_box.right=box.right;
        if(box.bottom>gpu.pending_box.bottom)gpu.pending_box.bottom=box.bottom;
    } else { gpu.pending_box=box;gpu.pending_batch=*b; }
    gpu.pending=1;gpu.pending_depth|=write_depth;
    gpu.color_source=b->color;gpu.color_valid=1;
    gpu.depth_source=b->depth;gpu.depth_pitch=b->depth_pitch;gpu.depth_valid=dbpp!=0;
    if(!gpu.scope && !counted_flush(&flush_unscoped))goto failed_resolve;
success:
    accepted++;
    if(accepted==1 || !(accepted%1000))fprintf(stderr,"[D3D11] draws=%llu fallback=%llu upload=%llu readback=%lluMB tex=%llu shaders=%llu cache=%lluMB\n",
        (unsigned long long)accepted,(unsigned long long)fallback,
        (unsigned long long)surface_uploads,(unsigned long long)(readback_bytes>>20),
        (unsigned long long)texture_uploads,(unsigned long long)shader_compiles,
        (unsigned long long)(texture_cache_bytes>>20));
    timing_add(&timing_draw,start);
    return 1;
fail:
    /* Earlier queued draws must retire before the caller's software fallback.
     * A failed resolve remains pending and must be treated as fatal by a scope
     * owner; it cannot masquerade as successful completion. */
    if(!counted_flush(&flush_fallback))goto failed_resolve;
failed_resolve:
    gpu.color_valid=gpu.depth_valid=0;
    /* The ordinary synchronous caller can redraw this one failed draw in
     * software. A scoped caller owns earlier accepted draws and must stop. */
    if(!gpu.scope)gpu.pending=gpu.pending_depth=0;
    fallback++;timing_add(&timing_draw,start);return 0;
}
void nv2a_render_shutdown(void)
{
    unsigned i;
    if(gpu.context)ID3D11DeviceContext_ClearState(gpu.context);
    release_surfaces();
    for(i=0;i<SHADER_SLOTS;i++)RELEASE(gpu.shaders[i].shader);
    for(i=0;i<TEXTURE_SLOTS;i++)release_texture(&gpu.textures[i]);
    for(i=0;i<4;i++)RELEASE(gpu.samplers[i]);
    RELEASE(gpu.blend);RELEASE(gpu.ds);RELEASE(gpu.rasterizer);RELEASE(gpu.vertices);
    RELEASE(gpu.pack_constants);RELEASE(gpu.pack_shader);
    RELEASE(gpu.constants);RELEASE(gpu.layout);RELEASE(gpu.vs);RELEASE(gpu.context);RELEASE(gpu.device);
    memset(&gpu,0,sizeof gpu);
}
#else
int nv2a_render_enabled(void) { return 0; }
int nv2a_render_draw(const Nv2aRenderBatch *batch) { (void)batch;return 0; }
int nv2a_render_begin(void) { return 0; }
int nv2a_render_flush(void) { return 1; }
int nv2a_render_end(void) { return 1; }
int nv2a_render_scoped(void) { return 0; }
void nv2a_render_require_flush(void) {}
void nv2a_render_observe(Nv2aRenderObserver observer) { (void)observer; }
void nv2a_render_memory_read(const void *source,size_t bytes) { (void)source;(void)bytes; }
void nv2a_render_shutdown(void) {}
#endif
