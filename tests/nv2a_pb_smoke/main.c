/* Exercise the real executor without a game image, optionally through D3D11. */
#undef NDEBUG
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../src/kernel/nv2a_pb_exec.c"

static uint8_t arena[8u*1024u*1024u];
uint32_t g_xbox_image_lo, g_xbox_image_hi;
ptrdiff_t xbox_GetMemoryOffset(void) { return (ptrdiff_t)arena; }
uint32_t xbox_ContiguousAllocatedBytes(void) { return 0; }
void xbox_FramebufferWindowSet(uint32_t va, uint32_t pitch) { (void)va; (void)pitch; }
static uint32_t presented_va, presented_pitch;
void xbox_FramebufferWindowPresent(uint32_t va, uint32_t pitch)
{ presented_va = va; presented_pitch = pitch; }
void xbox_FramebufferWindowStart(void) {}
void xbox_Nv2aFrameCounterFlip(void) {}
static uint32_t pattern_color, pattern_writes, released_fence, fence_writes, fence_order_errors;
static uint32_t trap_parameter, trap_calls, trap_seen_pixel;
void xbox_Nv2aSoftwareMethod(uint32_t value)
{
    trap_parameter=value;trap_calls++;
    trap_seen_pixel=((uint32_t *)(arena+0x1000))[7*16+7];
}
void xbox_Nv2aPatternColor(uint32_t value) { pattern_color=value;pattern_writes++; }
void xbox_Nv2aSemaphoreRelease(uint32_t value)
{
    released_fence=value;fence_writes++;
    if((pattern_color^(value<<2))&0x7C)fence_order_errors++;
}

static void method(uint32_t m, uint32_t v) { nv2a_pb_exec_method(0, m, v); }
static void reset(void)
{
    memset(arena, 0, sizeof(arena));
    memset(&s_gpu, 0, sizeof(s_gpu));
    memset(&s_vp, 0, sizeof(s_vp));
    s_drawn_dumps = FB_DUMP_AFTER_DRAW;
    method(NV097_SET_SURFACE_CLIP_HORIZONTAL, 16u << 16);
    method(NV097_SET_SURFACE_CLIP_VERTICAL, 16u << 16);
    method(NV097_SET_SURFACE_FORMAT, 6);
    method(NV097_SET_SURFACE_PITCH, 64);
    method(NV097_SET_SURFACE_COLOR_OFFSET, 0x1000);
}
static uint32_t pixel(unsigned x, unsigned y)
{
    return ((uint32_t *)(arena + 0x1000))[y * 16 + x];
}
static int test_pattern_progress(void)
{
    unsigned i, failures=0;
    reset();pattern_color=0xDEADBEEF;pattern_writes=0;
    released_fence=0xBADF00D;fence_writes=fence_order_errors=0;
    for(i=0;i<128;i++) {
        uint32_t value=((i&31)<<2)|0xA5B60080;
        nv2a_pb_exec_method(1,0x310,value);
        if(pattern_color!=value || pattern_writes!=i+1) failures++;
        /* The same method number in the 3D class is unrelated state. */
        method(0x310,0xFF00FF00);
        if(pattern_color!=value || pattern_writes!=i+1) failures++;
        nv2a_pb_exec_method(2,0x1D70,0xBADF00D); /* unrelated subchannel */
        method(0x1D70,i);
        if(released_fence!=i || fence_writes!=i+1 || fence_order_errors)failures++;
    }
    printf("Pattern/semaphore progress: %u failures in 128 wrapped fences; subchannels isolated\n",failures);
    return failures!=0;
}
static int test_software_method(void)
{
    unsigned failures=0;
    reset();trap_calls=0;trap_parameter=0;
    method(0x100,0);
    nv2a_pb_exec_method(1,0x100,5);
    if(trap_calls)failures++;
    method(0x100,5);
    if(trap_calls!=1 || trap_parameter!=5)failures++;
    method(0x100,0x8000000Eu);
    if(trap_calls!=2 || trap_parameter!=0x8000000Eu)failures++;
    printf("Software-method routing: %u failures; zero NOP and non-3D isolated\n",failures);
    return failures!=0;
}
typedef struct { float x, y, z; uint32_t color; } TestVertex;
static void draw(uint32_t primitive, const TestVertex *v, unsigned n)
{
    unsigned i;
    memcpy(arena + 0x300, v, n * sizeof(*v));
    method(NV097_SET_VERTEX_DATA_ARRAY_OFFSET, 0x300);
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT, 2 | (3 << 4) | (16 << 8));
    method(NV097_SET_VERTEX_DATA_ARRAY_OFFSET + 3 * 4, 0x30C);
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT + 3 * 4, (4 << 4) | (16 << 8));
    method(NV097_SET_BEGIN_END, primitive);
    for (i = 0; i < n; i += 2)
        method(NV097_ARRAY_ELEMENT16, i | ((i + 1) << 16));
    method(NV097_SET_BEGIN_END, 0);
}
static void test_scoped_observers(void)
{
    const TestVertex q[]={{2,2,0,~0u},{14,2,0,~0u},{14,14,0,~0u},{2,14,0,~0u}};
    unsigned kind;
    const Nv2aRenderObserver observers[]={NV2A_OBSERVER_PATTERN,NV2A_OBSERVER_SEMAPHORE,
        NV2A_OBSERVER_METHOD,NV2A_OBSERVER_FLIP,NV2A_OBSERVER_CLEAR};
    if(!nv2a_render_enabled())return;
    for(kind=0;kind<6;kind++) {
        uint64_t calls=0,resolves=0,bytes=0,after_calls,after_resolves,after_bytes;
        if(kind<5)nv2a_render_observer_stats(observers[kind],&calls,&resolves,&bytes);
        reset();assert(nv2a_render_begin());draw(8,q,4);assert(!pixel(7,7));
        switch(kind) {
        case 0:nv2a_pb_exec_method(1,0x310,123);break;
        case 1:method(0x1D70,123);break;
        case 2:method(0x100,5);break;
        case 3:method(NV097_FLIP_STALL,0);break;
        case 4:method(NV097_CLEAR_SURFACE,0);break;
        case 5: {
            VertexAttr attr={0};float out[4];
            attr.offset=0x1000+7*64+7*4;attr.type=0;attr.size=4;attr.stride=4;
            assert(fetch_attr(&attr,0,out) && out[0]==1 && out[3]==1);break;
        }
        }
        assert(pixel(7,7)==~0u);assert(nv2a_render_end());
        if(kind<5) {
            nv2a_render_observer_stats(observers[kind],&after_calls,&after_resolves,&after_bytes);
            assert(after_calls==calls+1 && after_resolves==resolves+1 && after_bytes>bytes);
        }
    }
    puts("Scoped pushbuffer pattern, semaphore, software trap, flip, clear and vertex-alias observers passed");
    {
        Nv2aRenderStats before,after;
        unsigned saved_traps=trap_calls;
        reset();assert(nv2a_render_begin());draw(8,q,4);assert(!pixel(7,7));
        nv2a_render_get_stats(&before);
        method(0x100,0);
        nv2a_render_get_stats(&after);
        if(pixel(7,7) || after.readback_bytes!=before.readback_bytes || trap_calls!=saved_traps) {
            fprintf(stderr,"Zero NOP unexpectedly resolves queued GPU pixels or raises a software trap\n");
            exit(2);
        }
        method(0x100,5);
        assert(pixel(7,7)==~0u && trap_seen_pixel==~0u
            && trap_calls==saved_traps+1 && trap_parameter==5);
        assert(nv2a_render_end());
        puts("Zero NOP preserves queued work; nonzero NOP commits before its software trap");
    }
}
static void test_primitives(void)
{
    const TestVertex triangles[] = {
        {0,0,0,~0u},{4,0,0,~0u},{0,4,0,~0u},
        {11,11,0,~0u},{15,11,0,~0u},{11,15,0,~0u}
    };
    const TestVertex quads[] = {
        {0,0,0,~0u},{4,0,0,~0u},{4,15,0,~0u},{0,15,0,~0u},
        {11,0,0,~0u},{15,0,0,~0u},{15,15,0,~0u},{11,15,0,~0u}
    };
    reset(); draw(5, triangles, 6);
    assert(pixel(1,1) == ~0u && pixel(12,12) == ~0u);
    assert(pixel(7,7) == 0 && s_gpu.tris_drawn == 2);
    reset(); draw(8, quads, 8);
    assert(pixel(1,7) == ~0u && pixel(12,7) == ~0u);
    assert(pixel(7,7) == 0 && s_gpu.tris_drawn == 4);
}
static void test_shared_edges(void)
{
    const TestVertex corners[]={{2,2,0,0x80FFFFFF},{14,2,0,0x80FFFFFF},
        {14,14,0,0x80FFFFFF},{2,14,0,0x80FFFFFF}};
    const unsigned order[][6]={{0,1,2,0,2,3},{2,1,0,3,2,0},
        {0,1,3,2},{1,0,2,3},{0,1,2,3},{0,3,2,1}};
    unsigned kind,half,x,y;
    for(half=0;half<2;half++)for(kind=0;kind<6;kind++) {
        TestVertex vertices[6];unsigned i,n=kind<2?6:4;
        reset();
        method(NV097_SET_BLEND_ENABLE,1);
        method(NV097_SET_BLEND_FUNC_SFACTOR,0x302);
        method(NV097_SET_BLEND_FUNC_DFACTOR,0x303);
        for(i=0;i<n;i++) {
            vertices[i]=corners[order[kind][i]];
            vertices[i].x+=half*0.5f;vertices[i].y+=half*0.5f;
        }
        draw(kind<2?5:kind<4?6:8,vertices,n);
        for(y=0;y<16;y++)for(x=0;x<16;x++) {
            uint32_t want=x>=2 && x<14 && y>=2 && y<14?0x40808080:0;
            if(pixel(x,y)!=want) {
                fprintf(stderr,"Shared edge kind=%u half=%u pixel=(%u,%u) got=%08X want=%08X\n",
                    kind,half,x,y,pixel(x,y),want);
                exit(2);
            }
        }
    }
    puts("Translucent shared edges: lists, strips and quads cover each pixel once in both windings, integer/half-pixel bounds");
}
static void indexed_quad(void)
{
    method(NV097_SET_BEGIN_END,8);
    method(NV097_ARRAY_ELEMENT16,0|(1<<16));
    method(NV097_ARRAY_ELEMENT16,2|(3<<16));
    method(NV097_SET_BEGIN_END,0);
}
static void test_packed_vertex_formats(void)
{
    struct ShortVertex { int16_t x,y,z,pad; uint32_t color; };
    const struct ShortVertex q[]={{2,2,0,0,0xFF00FF00},{14,2,0,0,0xFF00FF00},
                                  {14,14,0,0,0xFF00FF00},{2,14,0,0,0xFF00FF00}};
    VertexAttr attr={0x200,5,2,4}; float out[4];
    const int16_t edge[]={-32768,32767};
    unsigned i;
    reset(); memcpy(arena+0x200,edge,sizeof edge);
    assert(fetch_attr(&attr,0,out));
    assert(out[0]==-32768 && out[1]==32767 && out[2]==0 && out[3]==1);
    memcpy(arena+0x300,q,sizeof q);
    method(NV097_SET_VERTEX_DATA_ARRAY_OFFSET,0x300);
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT,5|(3<<4)|(12<<8));
    method(NV097_SET_VERTEX_DATA_ARRAY_OFFSET+3*4,0x308);
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+3*4,(4<<4)|(12<<8));
    indexed_quad();
    assert(pixel(7,7)==0xFF00FF00 && pixel(0,0)==0 && pixel(15,15)==0);
    /* CMP is one DWORD regardless of the declared component count; decode
     * signed 11/11/10 and saturate each negative minimum to -1. */
    reset(); attr.type=6;attr.size=1;
    *(uint32_t*)(arena+0x200)=1023u|(0x400u<<11)|(0x200u<<22);
    assert(fetch_attr(&attr,0,out));
    assert(out[0]==1 && out[1]==-1 && out[2]==-1 && out[3]==1);
    *(uint32_t*)(arena+0x200)=0x400u|(1023u<<11)|(511u<<22);
    assert(fetch_attr(&attr,0,out));
    assert(out[0]==-1 && out[1]==1 && out[2]==1 && out[3]==1);
    /* Real vertex-program input: the compressed normal stream drives red. */
    { const TestVertex v[]={{2,2,0,1023},{14,2,0,1023},{14,14,0,1023},{2,14,0,1023}};
      memcpy(arena+0x300,v,sizeof v); }
    method(NV097_SET_VERTEX_DATA_ARRAY_OFFSET,0x300);
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT,2|(3<<4)|(16<<8));
    method(NV097_SET_VERTEX_DATA_ARRAY_OFFSET+3*4,0x30C);
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+3*4,6|(1<<4)|(16<<8));
    /* Raw MOV oPos,v0; MOV oD0,v3 (FINAL). */
    method(0x1E94,6);method(0x1E9C,0);
    { const uint32_t t[2][4]={{0,(1u<<21)|0x1B,2u<<26,(15u<<12)|(1u<<11)},
       {0,(1u<<21)|(3u<<9)|0x1B,2u<<26,(15u<<12)|(1u<<11)|(3u<<3)|1u}};
      for(i=0;i<8;i++) method(0xB00+(i%4)*4,t[i/4][i%4]); }
    indexed_quad(); assert(pixel(7,7)==0xFFFF0000 && pixel(0,0)==0);
    /* Truncated inline elements are rejected at their actual storage size. */
    reset(); s_gpu.inline_active=1;s_gpu.inline_count=1;
    s_gpu.inline_buf[0]=0x80007FFF;attr.offset=2;attr.type=5;attr.size=1;
    assert(fetch_attr(&attr,0,out) && out[0]==-32768 && out[3]==1);
    attr.size=2;assert(!fetch_attr(&attr,0,out));
    attr.type=6;attr.size=1;assert(!fetch_attr(&attr,0,out));
}
static void inline_quad(const void *data, unsigned bytes)
{
    const uint8_t *p=(const uint8_t*)data;
    unsigned i;
    assert(bytes%4==0);
    method(NV097_SET_BEGIN_END,8);
    for(i=0;i<bytes;i+=4) { uint32_t word;memcpy(&word,p+i,4);method(NV097_INLINE_ARRAY,word); }
    method(NV097_SET_BEGIN_END,0);
}
static void test_packed_inline_layout(void)
{
    const int16_t xy[4][2]={{2,2},{14,2},{14,14},{2,14}};
    uint8_t data[48]; unsigned i;
    /* Adjacent one-short attributes share a DWORD; each contributes two
     * bytes. Wrong layout used to count fewer than four quad vertices. */
    reset();
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT,5|(2<<4));
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+4,5|(1<<4));
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+8,5|(1<<4));
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+12,4<<4);
    for(i=0;i<4;i++) {
        uint32_t color=0xFF00FF00;int16_t aux[2]={17,-23};
        memcpy(data+i*12,xy[i],4);memcpy(data+i*12+4,aux,4);
        memcpy(data+i*12+8,&color,4);
    }
    inline_quad(data,48);
    assert(pixel(7,7)==0xFF00FF00 && pixel(0,0)==0 && s_gpu.tris_drawn==2);
    /* Byte RGB has no invented DWORD padding at the end of each vertex. */
    reset();
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT,5|(2<<4));
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+12,4|(3<<4));
    for(i=0;i<4;i++) {
        memcpy(data+i*7,xy[i],4);data[i*7+4]=0;data[i*7+5]=255;data[i*7+6]=0;
    }
    inline_quad(data,28);
    assert(pixel(7,7)==0xFF00FF00 && pixel(0,0)==0 && s_gpu.tris_drawn==2);
    /* A following float attribute aligns itself, even after a one-short
     * field. Its shader output makes a wrong offset visible as a red pixel. */
    reset();memset(data,0,sizeof data);
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT,5|(2<<4));
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+4,5|(1<<4));
    method(NV097_SET_VERTEX_DATA_ARRAY_FORMAT+12,2|(1<<4));
    for(i=0;i<4;i++) {
        float red=1;int16_t aux=-32768;
        memcpy(data+i*12,xy[i],4);memcpy(data+i*12+4,&aux,2);
        memcpy(data+i*12+8,&red,4);
    }
    method(0x1E94,6);method(0x1E9C,0);
    { const uint32_t t[2][4]={{0,(1u<<21)|0x1B,2u<<26,(15u<<12)|(1u<<11)},
       {0,(1u<<21)|(3u<<9)|0x1B,2u<<26,(15u<<12)|(1u<<11)|(3u<<3)|1u}};
      for(i=0;i<8;i++) method(0xB00+(i%4)*4,t[i/4][i%4]); }
    inline_quad(data,48);
    assert(pixel(7,7)==0xFFFF0000 && pixel(0,0)==0);
}
static void test_blend_modes(void)
{
    static const struct { uint32_t factor, channels; } factors[] = {
        {0,0}, {1,0xFFFFFFFF}, {0x300,0x80402010}, {0x301,0x7FBFDFEF},
        {0x302,0x80808080}, {0x303,0x7F7F7F7F}, {0x304,0xC8C8C8C8},
        {0x305,0x37373737}, {0x306,0xC8102030}, {0x307,0x37EFDFCF},
        {0x308,0xFF373737}, {0x8001,0x408060A0}, {0x8002,0xBF7F9F5F},
        {0x8003,0x40404040}, {0x8004,0xBFBFBFBF}
    };
    unsigned i, shift;
    reset(); s_surface = arena + 0x1000;
    method(0x34C, 0x408060A0);
    for(i=0;i<sizeof factors/sizeof factors[0];i++) {
        uint32_t packed = 0;
        for(shift=0;shift<32;shift+=8)
            packed |= blend_factor(factors[i].factor,0x80402010,0xC8102030,shift)<<shift;
        assert(packed == factors[i].channels);
    }
    method(NV097_SET_BLEND_ENABLE, 1);
    method(NV097_SET_BLEND_EQUATION, 0x8006);
    /* Actual stadium states: additive SRC_ALPHA/ONE and ONE/CONSTANT_COLOR.
     * Both must retain a destination contribution instead of replacing it. */
    method(NV097_SET_BLEND_FUNC_SFACTOR, 0x302);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 1);
    *(uint32_t *)(arena + 0x1000) = 0xFF102030;
    put_pixel(arena,4,0,0,0x80402010);
    assert(pixel(0,0) == 0xFF303038);
    method(NV097_SET_BLEND_FUNC_SFACTOR, 1);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 0x8001);
    method(0x34C, 0x80402010);
    *(uint32_t *)(arena + 0x1000) = 0xFF102030;
    put_pixel(arena,4,0,0,0x80402010);
    assert(pixel(0,0) == 0xFF442413);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 0);
    put_pixel(arena,4,0,0,0x00402010);
    assert(pixel(0,0) == 0x00402010); /* ONE still writes when source alpha=0 */
    /* MIN/MAX ignore factors; subtraction clamps each channel separately. */
    method(NV097_SET_BLEND_FUNC_SFACTOR, 0);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 0);
    method(NV097_SET_BLEND_EQUATION, 0x8007);
    *(uint32_t *)(arena + 0x1000) = 0xFF102030;
    put_pixel(arena,4,0,0,0x80402010);
    assert(pixel(0,0) == 0x80102010);
    method(NV097_SET_BLEND_EQUATION, 0x8008);
    *(uint32_t *)(arena + 0x1000) = 0xFF102030;
    put_pixel(arena,4,0,0,0x80402010);
    assert(pixel(0,0) == 0xFF402030);
    method(NV097_SET_BLEND_FUNC_SFACTOR, 1);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 1);
    method(NV097_SET_BLEND_EQUATION, 0x800A);
    *(uint32_t *)(arena + 0x1000) = 0xFF102030;
    put_pixel(arena,4,0,0,0x80402010);
    assert(pixel(0,0) == 0x00300000);
    method(NV097_SET_BLEND_EQUATION, 0x800B);
    *(uint32_t *)(arena + 0x1000) = 0xFF102030;
    put_pixel(arena,4,0,0,0x80402010);
    assert(pixel(0,0) == 0x7F000020);
    /* Destination-alpha saturation uses ONE for the source alpha channel. */
    method(NV097_SET_BLEND_EQUATION, 0x8006);
    method(NV097_SET_BLEND_FUNC_SFACTOR, 0x308);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 0);
    *(uint32_t *)(arena + 0x1000) = 0xFF102030;
    put_pixel(arena,4,0,0,0x80402010);
    assert(pixel(0,0) == 0x80000000);
    /* RGB565 destinations are expanded before the same blend operation. */
    method(NV097_SET_BLEND_FUNC_SFACTOR, 0x302);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 1);
    *(uint16_t *)(arena + 0x1000) = 0x001F;
    put_pixel(arena,2,0,0,0x80FF0000);
    assert(*(uint16_t *)(arena + 0x1000) == 0x801F);
}
static void test_partial_surface_clip(void)
{
    TestVertex quad[] = {
        {16,0,0,0xFF00FF00},{32,0,0,0xFF00FF00},
        {32,16,0,0xFF00FF00},{16,16,0,0xFF00FF00}
    };
    uint32_t *rt=(uint32_t*)(arena+0x1000);
    unsigned i;
    reset();
    /* Live shadow atlas: 32-bit format0x128, a half-width clip and a full
     * surface pitch. Pitch/clip_width would incorrectly say eight bpp. */
    method(NV097_SET_SURFACE_FORMAT,0x128);
    method(NV097_SET_SURFACE_PITCH,128);
    method(NV097_SET_SURFACE_CLIP_HORIZONTAL,(16u<<16)|16);
    for(i=0;i<32*16;i++)rt[i]=0x12345678;
    method(NV097_SET_COLOR_CLEAR_VALUE,0xFFFF0000);
    method(NV097_CLEAR_SURFACE,NV097_CLEAR_COLOR_MASK);
    assert(rt[7*32+15]==0x12345678 && rt[7*32+16]==0xFFFF0000);
    assert(rt[7*32+31]==0xFFFF0000 && rt[8*32]==0x12345678);
    draw(8,quad,4);
    assert(rt[7*32+23]==0xFF00FF00 && rt[7*32+15]==0x12345678);
}
static void test_yuv(void)
{
    uint32_t t;
    reset();
    s_gpu.tex.offset = 0x200; s_gpu.tex.width = 2; s_gpu.tex.height = 1;
    s_gpu.tex.pitch = 4; s_gpu.tex.valid = 1; s_gpu.tex.color = 0x24;
    memcpy(arena + 0x200, "\x10\x80\xEB\x80", 4);
    assert(sample_texture(0,0,&t) && t == 0xFF000000u);
    assert(sample_texture(1,0,&t) && t == 0xFFFFFFFFu);
    memcpy(arena + 0x200, "\x51\x5A\x51\xF0", 4);
    assert(sample_texture(0,0,&t) && t == 0xFFFF0000u);
    s_gpu.tex.color = 0x25;
    memcpy(arena + 0x200, "\x80\x10\x80\xEB", 4);
    assert(sample_texture(0,0,&t) && t == 0xFF000000u);
    assert(sample_texture(1,0,&t) && t == 0xFFFFFFFFu);
}
static void test_negative_texture_coordinates(void)
{
    const TestVertex quad[] = {
        {0,0,0,~0u},{8,0,0,~0u},{8,8,0,~0u},{0,8,0,~0u}
    };
    const float uv[] = {0.5f,-0.25f, 0.5f,-0.25f,
                        0.5f,-0.25f, 0.5f,-0.25f};
    float tc[4] = {0.146655f,-0.689499f,0,1}, out[4];
    unsigned i;
    reset();
    s_gpu.tex.offset=0x200; s_gpu.tex.width=s_gpu.tex.height=4;
    s_gpu.tex.pitch=16; s_gpu.tex.valid=1; s_gpu.tex.color=0x06;
    s_gpu.tex.addr_u=s_gpu.tex.addr_v=1;
    for (i=0;i<16;i++) ((uint32_t *)(arena+0x200))[i]=0xFF000000u | (i+1);
    s_gpu.rc.stage_program=1;
    /* Live stadium UVs: floor(-0.689499*4)=-3 wraps to row 1.
     * In a swizzled 4x4 page (0,1) is texel index 2. */
    assert(combiner_texture(0,tc,out) && out[2] == 3/255.0f);
    tc[0]=-0.0625f; tc[1]=0;
    assert(combiner_texture(0,tc,out) && out[2] == 6/255.0f); /* x=-1 -> 3 */
    tc[0]=0.5f; tc[1]=-0.5f; tc[3]=2;
    assert(combiner_texture(0,tc,out) && out[2] == 12/255.0f); /* (1,3) */
    {
        const struct { float u; unsigned blue; } edges[] = {
            {-0.00000001f,6}, {-1.0f,1}, {-1.0625f,6}, {1.0f,1},
            {2.3125f,2}, {1099511627776.0f,1}, {-1099511627776.0f,1}
        };
        tc[1]=0; tc[3]=1;
        for (i=0;i<sizeof edges/sizeof edges[0];i++) {
            tc[0]=edges[i].u;
            assert(combiner_texture(0,tc,out) && out[2] == edges[i].blue/255.0f);
        }
    }
    /* Linear UI pages use texel coordinates; preserve negative fractions
     * until wrapping, including the fixed-function raster path. */
    s_gpu.tex.color=0x12;
    memcpy(arena+0x800,uv,sizeof uv);
    s_gpu.attr[9].offset=0x800; s_gpu.attr[9].type=2;
    s_gpu.attr[9].size=2; s_gpu.attr[9].stride=8;
    draw(8,quad,4);
    assert(pixel(3,3) == 0xFF00000Du); /* floor(v)=-1 -> last row */
    s_gpu.tex.addr_u=s_gpu.tex.addr_v=3;
    memset(arena+0x1000,0,1024); draw(8,quad,4);
    assert(pixel(3,3) == 0xFF000001u); /* clamp still uses the first row */
}
static void test_linear_argb_decode(void)
{
    static const unsigned sizes[][2]={{1,1},{3,7},{257,17},{640,480},{640,1},{1,480}};
    static uint32_t decoded[640*480+2];
    Texture t={0};unsigned n,pass,x,y;uint64_t checked=0;
    t.offset=0x20004;t.color=0x12;t.valid=1;
    for(n=0;n<sizeof sizes/sizeof sizes[0];n++)for(pass=0;pass<2;pass++) {
        size_t pixels;
        t.width=sizes[n][0];t.height=sizes[n][1];
        t.pitch=t.width*4+(pass?28:0);
        t.addr_u=pass?1:3;t.addr_v=pass?3:1;
        pixels=(size_t)t.width*t.height;
        memset(arena+t.offset,0xC3,(size_t)t.pitch*t.height);
        for(y=0;y<t.height;y++)for(x=0;x<t.width;x++) {
            uint32_t value=(y*0x91E10DA5u)^(x*0x9E3779B9u)^(pass*0x76543210u);
            if(!(x%7))value&=0x00FFFFFF; /* Preserve zero alpha, too. */
            memcpy(arena+t.offset+(size_t)y*t.pitch+x*4,&value,4);
        }
        decoded[0]=decoded[pixels+1]=0xD15C0DED;
        memset(decoded+1,0xBA,pixels*4);
        assert(render_texture_decode(&t,decoded+1));
        for(y=0;y<t.height;y++)for(x=0;x<t.width;x++) {
            uint32_t expected;
            assert(sample_texture_for(&t,x,y,&expected));
            assert(decoded[1+(size_t)y*t.width+x]==expected);checked++;
        }
        assert(decoded[0]==0xD15C0DED && decoded[pixels+1]==0xD15C0DED);
        /* Decode again after a CPU edit, without changing the descriptor. */
        {
            uint32_t changed=0x00123456;
            memcpy(arena+t.offset+(size_t)(t.height-1)*t.pitch+(t.width-1)*4,&changed,4);
            assert(render_texture_decode(&t,decoded+1));
            assert(decoded[pixels]==changed);
            assert(decoded[0]==0xD15C0DED && decoded[pixels+1]==0xD15C0DED);
        }
    }
    t.valid=0;assert(!render_texture_decode(&t,decoded+1));
    t.valid=1;t.color=0x7F;assert(!render_texture_decode(&t,decoded+1));
    printf("Linear ARGB bulk decode: %llu exact scalar texels, tight/padded rows, alpha, guards and CPU edits passed\n",
        (unsigned long long)checked);
}
static void test_fade(void)
{
    TestVertex quad[] = {
        {0,0,0,0x00FFFFFFu},{15,0,0,0x00FFFFFFu},
        {15,15,0,0x00FFFFFFu},{0,15,0,0x00FFFFFFu}
    };
    unsigned i;
    reset();
    for (i = 0; i < 256; i++) ((uint32_t *)(arena + 0x1000))[i] = 0xFF0000FFu;
    method(NV097_SET_BLEND_ENABLE, 1);
    method(NV097_SET_BLEND_FUNC_SFACTOR, 0x302);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 0x303);
    method(NV097_SET_BLEND_EQUATION, 0x8006);
    s_surface = arena + 0x1000;
    put_pixel(arena, 4, 0, 0, 0x80FF0000u);
    assert(pixel(0,0) == 0xBF80007Fu);
    /* A transparent diffuse value must modulate an opaque texture's alpha. */
    s_gpu.tex.offset = 0x200; s_gpu.tex.width = s_gpu.tex.height = 1;
    s_gpu.tex.pitch = 4; s_gpu.tex.valid = 1; s_gpu.tex.color = 0x12;
    *(uint32_t *)(arena + 0x200) = 0xFFFFFFFFu;
    s_gpu.attr[9].offset = 0x800; s_gpu.attr[9].type = 2;
    s_gpu.attr[9].size = 2; s_gpu.attr[9].stride = 8;
    draw(8, quad, 4);
    assert(pixel(7,7) == 0xFF0000FFu);
    for (i = 0; i < 4; i++) quad[i].color = 0xFFFFFFFFu;
    draw(8, quad, 4);
    assert(pixel(7,7) == 0xFFFFFFFFu);
}

static void test_palette(void)
{
    uint32_t t, i;
    const float uv[] = {0,0, 2,0, 2,2, 0,2};
    const TestVertex quad[] = {
        {0,0,0,~0u},{8,0,0,~0u},{8,8,0,~0u},{0,8,0,~0u}
    };
    reset();
    s_gpu.tex.offset = 0x200; s_gpu.tex.width = s_gpu.tex.height = 2;
    s_gpu.tex.valid = 1; s_gpu.tex.color = 0x0B;
    arena[0x200] = 0; arena[0x201] = 1;
    arena[0x202] = 17; arena[0x203] = 255;
    ((uint32_t *)(arena + 0x500))[0] = 0x00FFFFFFu;
    ((uint32_t *)(arena + 0x500))[1] = 0xFFFF0000u;
    ((uint32_t *)(arena + 0x500))[17] = 0xFF00FF00u;
    ((uint32_t *)(arena + 0x500))[255] = 0x800000FFu;
    method(NV097_SET_TEXTURE_PALETTE, 0x500 | 1u);
    assert(s_gpu.tex.palette_offset == 0x500);
    assert(sample_texture(0,0,&t) && t == 0x00FFFFFFu);
    assert(sample_texture(1,0,&t) && t == 0xFFFF0000u);
    assert(sample_texture(0,1,&t) && t == 0xFF00FF00u);
    assert(sample_texture(1,1,&t) && t == 0x800000FFu);
    method(NV097_SET_TEXTURE_PALETTE, 0x500 | (3u << 2));
    assert(s_gpu.tex.palette_entries == 32);
    assert(!sample_texture(1,1,&t));
    method(NV097_SET_TEXTURE_PALETTE, 0x500);
    memcpy(arena + 0x800, uv, sizeof(uv));
    s_gpu.attr[9].offset = 0x800; s_gpu.attr[9].type = 2;
    s_gpu.attr[9].size = 2; s_gpu.attr[9].stride = 8;
    for (i = 0; i < 256; i++) ((uint32_t *)(arena + 0x1000))[i] = 0xFF0000FFu;
    method(NV097_SET_BLEND_ENABLE, 1);
    method(NV097_SET_BLEND_FUNC_SFACTOR, 0x302);
    method(NV097_SET_BLEND_FUNC_DFACTOR, 0x303);
    method(NV097_SET_BLEND_EQUATION, 0x8006);
    draw(8, quad, 4);
    assert(pixel(1,1) == 0xFF0000FFu); /* transparent palette entry */
    assert(pixel(6,1) == 0xFFFF0000u);
    assert(pixel(1,6) == 0xFF00FF00u);
}
static void test_flip_target(void)
{
    const TestVertex quad[] = {
        {0,0,0,~0u},{15,0,0,~0u},{15,15,0,~0u},{0,15,0,~0u}
    };
    reset(); draw(8, quad, 4);
    /* The next frame's clear must not replace the completed frame, including
     * its pitch, before presentation. */
    method(NV097_SET_SURFACE_COLOR_OFFSET, 0x2000);
    method(NV097_SET_SURFACE_PITCH, 128);
    method(NV097_CLEAR_SURFACE, NV097_CLEAR_COLOR_MASK);
    method(NV097_FLIP_STALL, 0);
    assert(presented_va == 0x1000 && presented_pitch == 64);
}

/* Raw NV2A words: these tests upload through the real method dispatcher rather
 * than installing interpreter state directly. Attribute/constant muxes and
 * destination masks are deliberately different in the paired-issue case. */
static void vp_instruction(uint32_t d1, uint32_t d2, uint32_t d3)
{
    method(0xB00, 0); method(0xB04, d1); method(0xB08, d2); method(0xB0C, d3);
}
static void vp_constant(const float c[4])
{
    unsigned i;
    for (i = 0; i < 4; i++) {
        uint32_t bits; memcpy(&bits, c + i, 4); method(0xB80 + i*4, bits);
    }
}
static void test_vertex_program(void)
{
    const float scale[4] = {12,12,1,1}, offset[4] = {2,2,0,0};
    const TestVertex quad[] = {
        {0,0,0,0xFF0000FFu},{1,0,0,0xFF0000FFu},
        {1,1,0,0xFF0000FFu},{0,1,0,0xFF0000FFu}
    };
    float v[16][4] = {{0}};
    Nv2aVpOutput o;
    reset();
    method(0x1E94, 6); method(0x1E9C, 0); method(0x1EA0, 0); method(0x1EA4, 0);
    vp_constant(scale); vp_constant(offset);
    /* MUL R0,v0,c0; ADD oPos,R0,c1; MOV oD0,v3; MOV oT0,v9 (FINAL). */
    vp_instruction((2u<<21)|0x1B, (2u<<26)|(3u<<11)|(0x1Bu<<17), 15u<<24);
    vp_instruction((3u<<21)|(1u<<13)|0x1B, (1u<<26)|(0x1Bu<<2),
                   (3u<<28)|(15u<<12)|(1u<<11));
    vp_instruction((1u<<21)|(3u<<9)|0x1B, 2u<<26, (15u<<12)|(1u<<11)|(3u<<3));
    vp_instruction((1u<<21)|(9u<<9)|0x1B, 2u<<26, (15u<<12)|(1u<<11)|(9u<<3)|1u);
    draw(8, quad, 4);
    /* The input spans one model unit; the old screen-space heuristic skipped
     * it. The program expands/translates it to a 12-pixel blue rectangle. */
    assert(pixel(3,3) == 0xFF0000FFu && pixel(13,13) == 0xFF0000FFu);
    assert(pixel(0,0) == 0 && pixel(15,15) == 0 && s_gpu.tris_drawn == 2);
    /* Reusing indices in the next draw must fetch the new vertex buffer. */
    method(0x1EA4, 1); { const float shifted[4] = {5,5,0,0}; vp_constant(shifted); }
    memset(arena + 0x1000, 0, 1024); draw(8, quad, 4);
    assert(pixel(3,3) == 0 && pixel(6,6) == 0xFF0000FFu);

    reset(); method(0x1E9C, 0);
    v[0][0]=2; v[0][1]=3; v[0][2]=4; v[0][3]=1;
    /* MOV R0,v0. MUL R0,v0,v0 + RCP R1,R0.x, both reading old R0.
     * MOV oPos,R0; MOV oD0,R1. */
    vp_instruction((1u<<21)|0x1B, 2u<<26, 15u<<24);
    vp_instruction((2u<<21)|(2u<<25)|0x1B,
                   (2u<<26)|(2u<<11)|(0x1Bu<<17), (1u<<28)|(15u<<24)|(15u<<16));
    vp_instruction((1u<<21)|0x1B, 1u<<26, (15u<<12)|(1u<<11));
    vp_instruction((1u<<21)|0x1B, (1u<<26)|(1u<<28),
                   (15u<<12)|(1u<<11)|(3u<<3)|1u);
    nv2a_vp_run(&s_vp, v, &o);
    assert(o.ok && o.pos[0]==4 && o.pos[1]==9 && o.pos[2]==16 && o.pos[3]==1);
    assert(o.d0[0]==0.5f && o.d0[3]==0.5f);

    /* Swizzle/negate an input and preserve unmasked oPos components. */
    method(0x1E9C, 0);
    vp_instruction((1u<<21)|0x1B, 2u<<26, (15u<<12)|(1u<<11));
    vp_instruction((1u<<21)|(1u<<8)|0xE4, 2u<<26, (10u<<12)|(1u<<11)|1u);
    nv2a_vp_run(&s_vp, v, &o);
    assert(o.ok && o.pos[0]==-1 && o.pos[1]==3 && o.pos[2]==-3 && o.pos[3]==1);

    /* Relative constant access uses ARL's address, including negative input. */
    method(0x1E9C, 0); method(0x1EA4, 0); vp_constant(scale); vp_constant(offset);
    v[0][0]=-0.25f;
    vp_instruction((13u<<21), 2u<<26, 0);
    vp_instruction((1u<<21)|(2u<<13)|0x1B, 3u<<26,
                   (15u<<12)|(1u<<11)|2u|1u);
    nv2a_vp_run(&s_vp, v, &o);
    /* offset.w=0 is deliberately invalid as a projected position, but data
     * still has to equal c[2 + floor(-0.25)] = c[1]. */
    assert(!o.ok && o.pos[0]==2 && o.pos[1]==2 && o.pos[3]==0);
    method(0x1EA0, NV2A_VP_SLOTS); nv2a_vp_run(&s_vp, v, &o); assert(!o.ok);
    method(0x1E9C, NV2A_VP_SLOTS); vp_instruction(~0u,~0u,~0u);
    method(0x1EA4, NV2A_VP_CONSTS); vp_constant(scale); /* bounded uploads */
}
static void test_vertex_program_viewport(void)
{
    const float scale[4] = {6,-6,1,0}, offset[4] = {8,8,0,0};
    const TestVertex quad[] = {
        {-1,-1,0,0xFF0000FFu},{1,-1,0,0xFF0000FFu},
        {1,1,0,0xFF0000FFu},{-1,1,0,0xFF0000FFu}
    };
    unsigned i;
    reset(); method(0x1E94, 6); method(0x1EA4, 123);
    for (i = 0; i < 4; i++) {
        uint32_t bits;
        memcpy(&bits, scale + i, 4); method(0xAF0 + 4*i, bits);
        memcpy(&bits, offset + i, 4); method(0xA20 + 4*i, bits);
    }
    assert(s_vp.const_load == 123); /* viewport writes do not advance uploads */
    /* The XDK viewport tail: MUL oPos.xyz,R12,c58 + RCC R1.x,R12.w;
     * MAD oPos.xyz,R12,R1.x,c59. Preserve the original clip W. */
    vp_instruction((1u<<21)|0x1B, 2u<<26, (15u<<12)|(1u<<11));
    vp_instruction(0x0647401B, 0xC4361BFF, 0x1078E800);
    vp_instruction(0x0087601B, 0xC400286C, 0x3070E800);
    vp_instruction((1u<<21)|(3u<<9)|0x1B, 2u<<26,
                   (15u<<12)|(1u<<11)|(3u<<3)|1u);
    draw(8, quad, 4);
    assert(pixel(3,3) == 0xFF0000FFu && pixel(13,13) == 0xFF0000FFu);
    assert(pixel(0,0) == 0 && pixel(15,15) == 0 && s_gpu.tris_drawn == 2);
    /* SET_TRANSFORM_CONSTANT aliases the same storage in the other direction. */
    method(0x1EA4, 59); { const float shifted[4] = {11,11,0,0}; vp_constant(shifted); }
    memset(arena + 0x1000, 0, 1024); draw(8, quad, 4);
    assert(pixel(3,3) == 0 && pixel(6,6) == 0xFF0000FFu);
}

static void frontend_combiner(void)
{
    /* Captured from MVP's actual team-screen draw, not a synthetic shortcut. */
    method(0xAC0, 0xC4C80000); method(0x1E40, 0x000100C0);
    method(0x260, 0xD4D81010); method(0xAA0, 0x000100C0);
    method(0x264, 0xD4D81010); method(0xAA4, 0x000000C0);
    method(0x288, 0x0000000C); method(0x28C, 0x00001C80);
    method(0x1E60, 0x00011102); method(0x1E70, 1);
}
static void float_method(uint32_t m, float f)
{
    uint32_t bits; memcpy(&bits, &f, 4); method(m, bits);
}
static void test_frontend_combiner(void)
{
    TestVertex quad[] = {
        {0,0,0,0x807F7F7Fu},{15,0,0,0x807F7F7Fu},
        {15,15,0,0x807F7F7Fu},{0,15,0,0x807F7F7Fu}
    };
    reset(); frontend_combiner();
    method(0x1B00, 0x200); method(0x1B04, 0x12 << 8);
    method(0x1B10, 4 << 16); method(0x1B1C, (1 << 16) | 1);
    *(uint32_t *)(arena+0x200) = 0xFFFFFFFF;
    s_gpu.attr[9].offset=0x800; s_gpu.attr[9].type=2;
    s_gpu.attr[9].size=2; s_gpu.attr[9].stride=8;
    draw(8, quad, 4);
    assert(pixel(3,7) == 0x80FEFEFEu); /* RGB x2, alpha remains x1 */
    method(0x300, 1); method(0x33C, 0x204); method(0x340, 200);
    memset(arena+0x1000, 0, 1024); draw(8, quad, 4);
    assert(pixel(3,7) == 0); /* alpha test happens before blending/write */
    method(0x33C, 0x202); method(0x340, 128);
    assert(alpha_pass(0.501f)); /* quantize alpha to the eight-bit comparator */
    assert(!alpha_pass(0.49f));
    method(0x300, 0);
    quad[0].color=quad[3].color=0xFF000000;
    quad[1].color=quad[2].color=0xFF7F7F7F;
    draw(8, quad, 4);
    assert((pixel(1,7)&255) < 40 && (pixel(13,7)&255) > 220);
}
static uint32_t depth24(unsigned x, unsigned y)
{
    return ((uint32_t *)(arena+0x4000))[y*32+x];
}
static void depth_setup(uint32_t format)
{
    method(NV097_SET_SURFACE_FORMAT,format);
    /* Deliberately different color/depth pitches. */
    method(NV097_SET_SURFACE_PITCH,64u | (128u<<16));
    method(0x214,0x4000);
    method(0x290,0x00100001); /* observed integer-Z stadium state */
    method(0x30C,1); method(0x354,0x203); method(0x35C,1);
}
static void depth_quad(float z, uint32_t color)
{
    const TestVertex q[]={{0,0,z,color},{15,0,z,color},
                          {15,15,z,color},{0,15,z,color}};
    draw(8,q,4);
}
static void test_depth_surfaces(void)
{
    static const unsigned pass[8][3]={
        {0,0,0},{1,0,0},{0,1,0},{1,1,0},
        {0,0,1},{1,0,1},{0,1,1},{1,1,1}
    };
    unsigned f,i;
    reset(); depth_setup(0x128);
    memset(arena+0x4000,0xA5,2048);
    method(0x1D8C,0xFFFFFF3C);
    method(NV097_CLEAR_SURFACE,1); /* depth-only, preserve stencil */
    assert(depth24(7,7)==0xFFFFFFA5 && pixel(7,7)==0);
    assert(depth24(16,7)==0xA5A5A5A5); /* row padding stays intact */
    depth_quad(16500000,0xFFFF0000);
    depth_quad(16600000,0xFF00FF00); /* farther draw cannot hide the near one */
    assert(pixel(7,7)==0xFFFF0000 && depth24(7,7)==(16500000u<<8|0xA5));
    depth_quad(16400000,0xFF0000FF);
    assert(pixel(7,7)==0xFF0000FF && depth24(7,7)==(16400000u<<8|0xA5));
    method(0x35C,0); depth_quad(16300000,0xFF00FF00);
    assert(pixel(7,7)==0xFF00FF00 && depth24(7,7)==(16400000u<<8|0xA5));
    method(0x35C,1); method(0x30C,0); depth_quad(16600000,0xFFFFFFFF);
    assert(pixel(7,7)==0xFFFFFFFF && depth24(7,7)==(16400000u<<8|0xA5));
    method(NV097_CLEAR_SURFACE,2); /* stencil-only, preserve depth */
    assert(depth24(7,7)==(16400000u<<8|0x3C));
    /* Every comparison, including equality and unsigned 24-bit values. */
    for(f=0;f<8;f++)for(i=0;i<3;i++) {
        reset(); depth_setup(0x128);
        method(0x1D8C,16500000u<<8|0x79); method(NV097_CLEAR_SURFACE,3);
        method(0x354,0x200+f); depth_quad(16499999.0f+(float)i,0xFF123456);
        if(pixel(7,7)!=(pass[f][i]?0xFF123456:0))fprintf(stderr,
            "Depth compare func=%X index=%u color=%08X depth=%08X expected_pass=%u\n",
            0x200+f,i,pixel(7,7),depth24(7,7),pass[f][i]);
        assert(pixel(7,7)==(pass[f][i]?0xFF123456:0));
        assert(depth24(7,7)==((pass[f][i]?16499999u+i:16500000u)<<8|0x79));
    }
    /* The accelerated backend must preserve every guest bit at range and
     * float-precision boundaries, including the maximum integer depth. */
    {
        const uint32_t z24[]={0,1,8388607,8388608,16777214,16777215};
        const uint16_t z16[]={0,1,32767,32768,65534,65535};
        reset();depth_setup(0x128);method(0x354,0x207);
        method(0x1D8C,0xFFFFFF79);method(NV097_CLEAR_SURFACE,3);
        for(i=0;i<sizeof z24/sizeof z24[0];i++) {
            depth_quad((float)z24[i],0xFF123456);
            assert(depth24(7,7)==(z24[i]<<8|0x79));
        }
        depth_quad(123.5f,0xFF123456);
        assert(depth24(7,7)==(124u<<8|0x79));
        reset();depth_setup(0x118);method(0x354,0x207);
        for(i=0;i<sizeof z16/sizeof z16[0];i++) {
            depth_quad((float)z16[i],0xFF123456);
            assert(((uint16_t*)(arena+0x4000))[7*64+7]==z16[i]);
        }
        depth_quad(60000.5f,0xFF123456);
        assert(((uint16_t*)(arena+0x4000))[7*64+7]==60001);
    }
    /* Inclusive clear rectangle, separate from the draw clip; no mask/test
     * state may suppress a requested clear. */
    reset(); depth_setup(0x128); memset(arena+0x4000,0xA5,2048);
    method(0x35C,0); method(0x30C,0);
    method(0x1D98,(9u<<16)|6); method(0x1D9C,(7u<<16)|5);
    method(0x1D8C,0x12345678); method(NV097_CLEAR_SURFACE,3);
    assert(depth24(6,5)==0x12345678 && depth24(9,7)==0x12345678);
    assert(depth24(5,5)==0xA5A5A5A5 && depth24(10,7)==0xA5A5A5A5);
    assert(depth24(6,4)==0xA5A5A5A5 && depth24(9,8)==0xA5A5A5A5);
    /* Z16 uses the low clear halfword and has no stencil lane. */
    reset(); depth_setup(0x118); memset(arena+0x4000,0xA5,2048);
    method(0x1D8C,0x1234FFFF); method(NV097_CLEAR_SURFACE,1);
    assert(((uint16_t*)(arena+0x4000))[7*64+7]==0xFFFF);
    depth_quad(60000,0xFFFF0000); depth_quad(61000,0xFF00FF00);
    assert(pixel(7,7)==0xFFFF0000);
    assert(((uint16_t*)(arena+0x4000))[7*64+7]==60000);
    method(NV097_CLEAR_SURFACE,2);
    assert(((uint16_t*)(arena+0x4000))[7*64+7]==60000);
    /* Alpha discard must leave depth available for the later opaque draw. */
    reset(); depth_setup(0x128);
    method(0x1D8C,0xFFFFFF66); method(NV097_CLEAR_SURFACE,3);
    frontend_combiner(); method(0x300,1); method(0x33C,0x204); method(0x340,200);
    method(0x1B00,0x200); method(0x1B04,0x12<<8);
    method(0x1B10,4<<16); method(0x1B1C,(1<<16)|1);
    *(uint32_t*)(arena+0x200)=0xFFFFFFFF;
    depth_quad(16000000,0x807F7F7F);
    assert(pixel(7,7)==0 && depth24(7,7)==0xFFFFFF66);
    depth_quad(16500000,0xFF7F7F7F);
    assert(pixel(7,7)==0xFFFEFEFE && depth24(7,7)==(16500000u<<8|0x66));
    /* A real VP batch with unequal clip W: screen-space Z stays affine. */
    reset(); depth_setup(0x128);
    method(0x1D8C,0xFFFFFF55); method(NV097_CLEAR_SURFACE,3);
    method(0x1E94,6); method(0x1E9C,0);
    vp_instruction((1u<<21)|0x1B,2u<<26,(15u<<12)|(1u<<11));
    vp_instruction((1u<<21)|(3u<<9)|0x1B,2u<<26,
                   (15u<<12)|(1u<<11)|(3u<<3)|1u);
    method(0x194C,0xFFFFFFFF);
    method(0x17FC,5);
    { const float p[3][4]={{0,0,10,1},{14,0,50,4},{0,14,90,8}};
      for(i=0;i<3;i++)for(f=0;f<4;f++) float_method(0x1518+f*4,p[i][f]); }
    method(0x17FC,0);
    assert(pixel(3,3)==0xFFFFFFFF && depth24(3,3)==(40u<<8|0x55));
    /* Clears happen again each frame and zeta changes select distinct RAM. */
    method(0x214,0x6000); method(0x1D8C,0x98765432); method(NV097_CLEAR_SURFACE,3);
    assert(*(uint32_t*)(arena+0x6000+3*128+3*4)==0x98765432);
    assert(depth24(3,3)==(40u<<8|0x55));
    method(0x214,0x4000); method(0x1D8C,0xFFFFFF00); method(NV097_CLEAR_SURFACE,3);
    assert(depth24(3,3)==0xFFFFFF00);
}
static void test_four_texture_immediate(void)
{
    const uint32_t texels[4][2] = {
        {0xFFFFFFFF,0xFFFFFFFF}, {0xFFFF0000,0xFFFFFFFF},
        {0xFFFFFFFF,0xFF00FF00}, {0xFF0000FF,0xFFFF0000}
    };
    const float xy[4][2] = {{0,0},{15,0},{15,15},{0,15}};
    const unsigned uv[4] = {0,1,0,1};
    unsigned st, i;
    reset();
    for (st=0; st<4; st++) {
        unsigned base=0x1B00+st*0x40;
        memcpy(arena+0x200+st*16, texels[st], 8);
        method(base,0x200+st*16); method(base+4,0x12<<8);
        method(base+16,8<<16); method(base+28,(2<<16)|1);
        float_method(0x1880+(9+st)*8,(float)uv[st]);
        float_method(0x1884+(9+st)*8,0);
    }
    /* Product of four independent stages; their UV choices give red.
     * Reusing t0 coordinates gives black; ignoring other stages gives white. */
    method(0xAC0,0x08090000); method(0xAC4,0x0C0A0000); method(0xAC8,0x0C0B0000);
    method(0x260,0x18190000); method(0x264,0x1C1A0000); method(0x268,0x1C1B0000);
    for (i=0;i<3;i++) { method(0x1E40+i*4,0xC0); method(0xAA0+i*4,0xC0); }
    method(0x288,0xC); method(0x28C,0x1C80);
    method(0x1E60,3); method(0x1E70,0x8421);
    method(0x1940+3*4,0xFFFFFFFF);
    method(0x17FC,8);
    for (i=0;i<4;i++) {
        float_method(0x1518,xy[i][0]); float_method(0x151C,xy[i][1]);
        float_method(0x1520,0); float_method(0x1524,1);
    }
    method(0x17FC,0);
    assert(pixel(7,7) == 0xFFFF0000u && s_gpu.tris_drawn == 2);
    /* The same four-stage draw through actual oT0..oT3 program outputs. */
    method(0x1E94,6); method(0x1E9C,0);
    vp_instruction((1u<<21)|0x1B,2u<<26,(15u<<12)|(1u<<11));
    vp_instruction((1u<<21)|(3u<<9)|0x1B,2u<<26,(15u<<12)|(1u<<11)|(3u<<3));
    for(st=0;st<4;st++)
        vp_instruction((1u<<21)|((9u+st)<<9)|0x1B,2u<<26,
                       (15u<<12)|(1u<<11)|((9u+st)<<3)|(st==3));
    memset(arena+0x1000,0,1024); method(0x17FC,8);
    for(i=0;i<4;i++) {
        float_method(0x1518,xy[i][0]); float_method(0x151C,xy[i][1]);
        float_method(0x1520,0); float_method(0x1524,1);
    }
    method(0x17FC,0);
    assert(pixel(7,7) == 0xFFFF0000u && s_gpu.tris_drawn == 4);
    /* A shader may write only texture XY. The unwritten projective Q must
     * retain 1 for every output, including the three secondary stages. */
    method(0x1E9C,0);
    vp_instruction((1u<<21)|0x1B,2u<<26,(15u<<12)|(1u<<11));
    vp_instruction((1u<<21)|(3u<<9)|0x1B,2u<<26,(15u<<12)|(1u<<11)|(3u<<3));
    for(st=0;st<4;st++)
        vp_instruction((1u<<21)|((9u+st)<<9)|0x1B,2u<<26,
                       (12u<<12)|(1u<<11)|((9u+st)<<3)|(st==3));
    memset(arena+0x1000,0,1024); method(0x17FC,8);
    for(i=0;i<4;i++) {
        float_method(0x1518,xy[i][0]); float_method(0x151C,xy[i][1]);
        float_method(0x1520,0); float_method(0x1524,1);
    }
    method(0x17FC,0);
    assert(pixel(7,7) == 0xFFFF0000u && s_gpu.tris_drawn == 6);
    /* 4UB is register-order RGBA, unlike a D3DCOLOR vertex-array dword. */
    assert(imm_vertex_method(0x194C,0x80402010));
    assert(s_gpu.imm_attr[3][0] == 16/255.0f && s_gpu.imm_attr[3][2] == 64/255.0f);
}
static void test_combiner_dependencies(void)
{
    Nv2aCombiner rc = {0};
    const float v0[4]={0.25f,0.5f,0.75f,0.5f}, v1[4]={0.5f,0.25f,1,1};
    const float fog[4]={0,0,0,1};
    float t[4][4]={{1,0,0,1/255.0f},{0,0,1,1},{0},{0}}, out[4];
    rc.stage_program=1; rc.control=1; rc.final0=0xC; rc.final1=0x1C80;
    rc.color_icw[0]=0x08200920; rc.color_ocw[0]=0x4C00;
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[0]==0 && out[2]==1); /* LSB=1 selects CD (blue) */
    rc.control=0x101;
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[0]==1 && out[2]==0); /* same alpha's MSB=0 selects AB (red) */
    t[0][3]=128/255.0f;
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[0]==0 && out[2]==1);
    rc.control=1;
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[0]==1 && out[2]==0);

    rc.color_icw[0]=0x04200000; rc.color_ocw[0]=0xC0;
    rc.alpha_icw[0]=0x0C200000; rc.alpha_ocw[0]=0xC0;
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[2]==0.75f && out[3]==0); /* alpha reads old R0.b */

    memset(&rc,0,sizeof rc); rc.final0=0xF;
    rc.final1=(4u<<24)|(5u<<16)|(0x14u<<8);
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[0]==0.125f && out[1]==0.125f && out[2]==0.75f && out[3]==0.5f);

    memset(&rc,0,sizeof rc); rc.control=2; rc.final0=0xC; rc.final1=0x1C80;
    rc.factor0[0]=0xFFFF0000; rc.factor0[1]=0xFF00FF00;
    rc.color_icw[1]=0x01200000; rc.color_ocw[1]=0xC0;
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[0]==1 && out[1]==0); /* shared constant */
    rc.control=0x1002;
    nv2a_rc_eval(&rc,v0,v1,fog,t,out);
    assert(out[0]==0 && out[1]==1); /* per-stage constant */
}
#ifdef _WIN32
static double benchmark_seconds(void)
{
    LARGE_INTEGER now,frequency;
    QueryPerformanceCounter(&now);QueryPerformanceFrequency(&frequency);
    return (double)now.QuadPart/(double)frequency.QuadPart;
}
static void benchmark_texture_decode(void)
{
    Texture t={0};static uint32_t decoded[640*480];
    int (*volatile decode_fn)(void*,uint32_t*)=render_texture_decode;
    unsigned i;uint32_t checksum=0;double elapsed;
    t.offset=0x20004;t.width=640;t.height=480;t.pitch=640*4+32;
    t.color=0x12;t.valid=1;t.addr_u=t.addr_v=1;
    for(i=0;i<t.pitch*t.height/4;i++)((uint32_t*)(arena+t.offset))[i]=i*0x9E3779B9u;
    assert(decode_fn(&t,decoded));elapsed=benchmark_seconds();
    for(i=0;i<512;i++) {
        ((uint32_t*)(arena+t.offset))[0]^=i+1;
        assert(decode_fn(&t,decoded));checksum^=decoded[0];
    }
    elapsed=benchmark_seconds()-elapsed;
    printf("Linear ARGB 640x480 padded decode: 512 iterations, %.6f seconds, %.6f ms/texture, checksum=%08X\n",
        elapsed,elapsed*1000/512,checksum);
}
static void benchmark_draw_sizes(unsigned width,unsigned height,int edit_depth,unsigned group)
{
    unsigned gpu_mode,n,i,j;
    const unsigned sizes[]={4,8,16,32,64,128};
    _putenv_s("RECOMP_GPU_MIN_PIXELS","0"); /* measure the full hardware cost */
    for(gpu_mode=0;gpu_mode<2;gpu_mode++) {
        nv2a_render_shutdown();_putenv_s("RECOMP_RENDERER",gpu_mode?"d3d11":"software");
        reset();depth_setup(0x128);method(0x354,0x207);
        method(NV097_SET_SURFACE_CLIP_HORIZONTAL,width<<16);
        method(NV097_SET_SURFACE_CLIP_VERTICAL,height<<16);
        method(NV097_SET_SURFACE_PITCH,width*4|((width*4)<<16));method(0x214,0x200000);
        method(0x1E94,6);method(0x1E9C,0);method(0x1EA0,0);
        vp_instruction((1u<<21)|0x1B,2u<<26,(15u<<12)|(1u<<11));
        vp_instruction((1u<<21)|(3u<<9)|0x1B,2u<<26,(15u<<12)|(1u<<11)|(3u<<3)|1u);
        frontend_combiner();method(0x1E60,0x00011106);
        method(0x1E70,1|(1<<5)|(1<<10)|(1<<15));
        for(i=0;i<6;i++) {
            method(0xAC0+i*4,i?0x0C200000:0x04200000);method(0x1E40+i*4,0xC0);
            method(0x260+i*4,i?0x1C200000:0x14200000);method(0xAA0+i*4,0xC0);
        }
        for(i=0;i<4;i++) {
            method(0x1B00+i*64,0x400000);method(0x1B04+i*64,0x12<<8);
            method(0x1B10+i*64,16<<16);method(0x1B1C+i*64,(4<<16)|4);
        }
        memset(arena+0x400000,0xFF,64);
        for(n=0;n<sizeof sizes/sizeof sizes[0];n++) {
            float size=(float)sizes[n];unsigned repeats=gpu_mode?512:(sizes[n]<64?128:32);
            TestVertex q[]={{0,0,16500000,~0u},{size,0,16500000,~0u},
                {size,size,16500000,~0u},{0,size,16500000,~0u}};
            double start,elapsed;
            draw(8,q,4);start=benchmark_seconds();
            for(j=0;j<repeats;j++) {
                if(gpu_mode && group && !(j%group))assert(nv2a_render_begin());
                if(edit_depth)((uint32_t*)(arena+0x200000))[width*height-1]=
                    (j&1?0x43210000u:0x54320000u)|0x5A;
                draw(8,q,4);
                if(gpu_mode && group && ((j+1)%group==0 || j+1==repeats))assert(nv2a_render_end());
            }
            elapsed=benchmark_seconds()-start;
            assert(((uint32_t*)(arena+0x1000))[(sizes[n]/2)*width+sizes[n]/2]==0xFFFFFFFF);
            assert(((uint32_t*)(arena+0x200000))[(sizes[n]/2)*width+sizes[n]/2]==16500000u<<8);
            if(width>128)assert(((uint32_t*)(arena+0x1000))[width*height-1]==0);
            if(edit_depth)assert(((uint32_t*)(arena+0x200000))[width*height-1]==
                ((repeats-1)&1?0x4321005Au:0x5432005Au));
            printf("%s %ux%u six-stage/four-texture depth draw (%ux%u surface%s, group %u): %.3f ms\n",
                gpu_mode?"AMD D3D11":"software",sizes[n],sizes[n],width,height,
                edit_depth?", partial CPU depth edit":"",gpu_mode?group:0,elapsed*1000/repeats);
            fflush(stdout);
        }
    }
    nv2a_render_shutdown();
}
static void test_hybrid_selection(void)
{
    const TestVertex q[]={{0,0,0,~0u},{15,0,0,~0u},{15,15,0,~0u},{0,15,0,~0u}};
    TestVertex many[160],blue[4];uint64_t before,after;unsigned i;
    reset();nv2a_render_stats(&before,NULL);
    draw(8,q,4);nv2a_render_stats(&after,NULL);
    assert(after==before && pixel(7,7)==0xFFFFFFFF); /* small CPU batch */
    /* Enough overdraw to remain a GPU batch at every tested cutoff, while
     * the single quad exercises the ordinary CPU path. */
    for(i=0;i<40;i++)memcpy(many+i*4,q,sizeof q);
    draw(8,many,160);nv2a_render_stats(&after,NULL);
    assert(after==before+1 && pixel(7,7)==0xFFFFFFFF); /* GPU overdraw batch */
    memcpy(blue,q,sizeof blue);for(i=0;i<4;i++)blue[i].color=0xFF0000FF;
    draw(8,blue,4);assert(pixel(7,7)==0xFF0000FF); /* CPU edits resident GPU color */
    for(i=0;i<160;i++)many[i].color=0;
    method(NV097_SET_BLEND_ENABLE,1);
    method(NV097_SET_BLEND_FUNC_SFACTOR,0x302);method(NV097_SET_BLEND_FUNC_DFACTOR,0x303);
    draw(8,many,160);nv2a_render_stats(&after,NULL);
    assert(after==before+2 && pixel(7,7)==0xFF0000FF);
    puts("Hybrid CPU/GPU selection and coherent transition passed");
}
#endif
int main(int argc, char **argv)
{
    if(argc==2 && !strcmp(argv[1],"--software-method"))return test_software_method();
    if(argc==2 && !strcmp(argv[1],"--pattern-progress"))return test_pattern_progress();
#ifdef _WIN32
    if(argc==2 && !strcmp(argv[1],"--benchmark-texture-decode")){benchmark_texture_decode();return 0;}
    if(argc==2 && !strcmp(argv[1],"--benchmark")){benchmark_draw_sizes(128,128,0,0);return 0;}
    if(argc==2 && !strcmp(argv[1],"--benchmark-large")){benchmark_draw_sizes(640,480,0,0);return 0;}
    if(argc==2 && !strcmp(argv[1],"--benchmark-large-edits")){benchmark_draw_sizes(640,480,1,0);return 0;}
    if(argc==2 && !strcmp(argv[1],"--benchmark-scopes")){benchmark_draw_sizes(640,480,0,32);return 0;}
#endif
    int custom_cutoff=argc==3 && !strcmp(argv[1],"--hybrid-cutoff");
    int hybrid=(argc==2 && !strcmp(argv[1],"--hybrid")) || custom_cutoff;
    int hardware=hybrid || (argc==2 && (!strcmp(argv[1],"--d3d11") || !strcmp(argv[1],"--hardware")));
    if(hardware) {
        _putenv_s("RECOMP_RENDERER",!strcmp(argv[1],"--d3d11")?"warp":"d3d11");
        if(custom_cutoff)assert(!strcmp(argv[2],"4096") || !strcmp(argv[2],"8192") || !strcmp(argv[2],"16384"));
        _putenv_s("RECOMP_GPU_MIN_PIXELS",custom_cutoff?argv[2]:hybrid?"2048":"0");
    }
    if(argc==2 && strcmp(argv[1],"--gpu-layout")==0) {
        printf("inline_active=%zu flips=%zu draws=%zu\n",
            (size_t)((char*)&s_gpu.inline_active-(char*)&s_gpu),
            (size_t)((char*)&s_gpu.flips-(char*)&s_gpu),
            (size_t)((char*)&s_gpu.draws-(char*)&s_gpu));
        return 0;
    }
    if(argc==3 && strcmp(argv[1],"--trace-rearm")==0) {
        const TestVertex q[]={{2,2,0,~0u},{14,2,0,~0u},{14,14,0,~0u},{2,14,0,~0u}};
        unsigned i;
        _putenv_s("RECOMP_FRAME_TRACE",argv[2]);reset();
        for(i=0;i<2;i++) {
            FILE *f=fopen(argv[2],"wb");assert(f);fclose(f);
            draw(8,q,4);
            assert(s_frame_trace_active==1);
            f=fopen(argv[2],"rb");assert(!f);
            method(NV097_FLIP_STALL,0);assert(s_frame_trace_active==0);
        }
        puts("Two independent frame trace requests passed");return 0;
    }
    assert(!test_pattern_progress());
    assert(!test_software_method());
    test_primitives(); test_yuv(); test_linear_argb_decode(); test_fade(); test_palette(); test_flip_target();
    test_shared_edges();
    test_packed_vertex_formats();
    test_packed_inline_layout();
    test_blend_modes();
    test_partial_surface_clip();
    test_negative_texture_coordinates();
    test_vertex_program();
    test_vertex_program_viewport();
    test_frontend_combiner(); test_four_texture_immediate();
    test_depth_surfaces();
    test_combiner_dependencies();
    test_scoped_observers();
#ifdef _WIN32
    if(hybrid)test_hybrid_selection();
#endif
    if(hardware) {
        uint64_t draws=0,fallbacks=0;
        nv2a_render_stats(&draws,&fallbacks);
        printf("D3D11 draws=%llu fallback=%llu\n",(unsigned long long)draws,(unsigned long long)fallbacks);
        assert(draws>=(hybrid?1:20));nv2a_render_shutdown();
    }
    puts("NV2A primitives, packed vertices, video, fades, blending, P8, flips, VP, combiners, four-stage draws and integer depth surfaces passed");
    return 0;
}
