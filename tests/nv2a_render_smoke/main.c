/* Exercise the actual decoded D3D11 path, including resource mutation and
 * coherent color/depth readback. Raw combiner pixels use the portable
 * evaluator as an independent oracle; no title or window is involved. */
#undef NDEBUG
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#undef near
#undef far
#include "../../src/kernel/nv2a_render.h"

static uint32_t random_state=0x49e807;
static uint32_t next(void)
{
    random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;
    return random_state;
}
static float signed_value(void) { return (float)(next()&65535)/32767.5f-1; }
static int decode(void *opaque,uint32_t *out)
{
    const Nv2aRenderTexture *t=(const Nv2aRenderTexture*)opaque;
    memcpy(out,t->source,(size_t)t->width*t->height*4);return 1;
}
static void setup(Nv2aRenderBatch *b,Nv2aRenderVertex v[3],uint32_t *pixel,uint32_t texels[4])
{
    const float positions[3][4]={{-1,-1,0,1},{3,-1,0,1},{-1,3,0,1}};
    unsigned i,j;
    memset(b,0,sizeof *b);memset(v,0,3*sizeof *v);
    b->color=(uint8_t*)pixel;b->pitch=4;b->color_bpp=4;b->height=1;
    b->clip_w=b->clip_h=1;b->vertices=v;b->count=3;b->has_combiner=1;
    for(i=0;i<3;i++) {
        memcpy(v[i].position,positions[i],sizeof positions[i]);
        for(j=0;j<4;j++){v[i].tc[j][0]=v[i].tc[j][1]=0.5f;v[i].tc[j][3]=1;}
    }
    for(i=0;i<4;i++) {
        Nv2aRenderTexture *t=&b->textures[i];
        t->source=&texels[i];t->source_bytes=4;t->width=t->height=1;t->pitch=4;
        t->format=0x12;t->valid=t->normalized=1;t->addr_u=t->addr_v=3;
        t->decode=decode;t->opaque=t;
    }
}
static unsigned check_pixel(uint32_t got,const float expected[4],unsigned family,unsigned sample)
{
    static const unsigned shifts[4]={16,8,0,24};
    unsigned i,bad=0;
    for(i=0;i<4;i++) {
        int want=(int)floorf(expected[i]*255+0.5f),value=(got>>shifts[i])&255;
        /* UNORM stores may choose either adjacent value at a half tie. */
        if(abs(want-value)>1) {
            if(bad==0)fprintf(stderr,"FAIL family=%u sample=%u expected=%g,%g,%g,%g got=%08X\n",
                family,sample,expected[0],expected[1],expected[2],expected[3],got);
            bad++;
        }
    }
    return bad;
}
static void coherent_regions(void)
{
    Nv2aRenderBatch b={0};Nv2aRenderVertex v[3]={0};
    uint32_t color[16*16],depth[16*16];
    const float xy[3][2]={{2,2},{6,2},{2,6}};
    Nv2aRenderStats before,after;
    unsigned i;
    for(i=0;i<256;i++){color[i]=0xFF010203;depth[i]=0xC00000A5;}
    for(i=0;i<3;i++) {
        v[i].position[0]=xy[i][0];v[i].position[1]=xy[i][1];
        v[i].position[2]=8388608;v[i].position[3]=1;
        v[i].diffuse[0]=v[i].diffuse[3]=1;
    }
    b.color=(uint8_t*)color;b.pitch=64;b.height=16;b.color_bpp=4;
    b.depth=(uint8_t*)depth;b.depth_pitch=64;b.depth_bpp=4;
    b.depth_test=b.depth_mask=1;b.depth_func=0x201;
    b.clip_w=b.clip_h=16;b.vertices=v;b.count=3;
    assert(nv2a_render_draw(&b));
    assert(color[3*16+3]==0xFFFF0000 && depth[3*16+3]==0x800000A5);
    assert(color[15*16+15]==0xFF010203 && depth[15*16+15]==0xC00000A5);
    nv2a_render_get_stats(&before);
    for(i=0;i<3;i++){v[i].position[2]=8388609;v[i].diffuse[0]=0;v[i].diffuse[2]=1;}
    assert(nv2a_render_draw(&b));
    nv2a_render_get_stats(&after);
    assert(after.surface_uploads==before.surface_uploads); /* resident data reused */
    assert(after.readback_bytes-before.readback_bytes<sizeof color+sizeof depth);
    assert(color[3*16+3]==0xFFFF0000 && depth[3*16+3]==0x800000A5);
    /* External CPU/software writes both inside and outside the next box must
     * survive. The changed depth also changes whether that next draw passes. */
    color[3*16+3]=0xFFFF00FF;depth[3*16+3]=0xFFFFFF5A;
    color[15*16+15]=0x12345678;depth[15*16+15]=0x11223366;
    nv2a_render_get_stats(&before);
    assert(nv2a_render_draw(&b));
    nv2a_render_get_stats(&after);
    assert(after.surface_upload_bytes-before.surface_upload_bytes<sizeof color+sizeof depth);
    assert(color[3*16+3]==0xFF0000FF && depth[3*16+3]==0x8000015A);
    assert(color[15*16+15]==0x12345678 && depth[15*16+15]==0x11223366);
    nv2a_render_get_stats(&before);b.depth_mask=0;
    for(i=0;i<3;i++)v[i].position[2]=8388607;
    assert(nv2a_render_draw(&b));
    nv2a_render_get_stats(&after);
    assert(after.surface_uploads==before.surface_uploads);
    assert(after.readback_bytes-before.readback_bytes<sizeof color);
    assert(depth[3*16+3]==0x8000015A);
    /* Sampling a previously drawn render target uses its committed RAM. */
    b.depth_test=0;
    b.textures[0].source=color;b.textures[0].source_bytes=sizeof color;
    b.textures[0].width=b.textures[0].height=16;b.textures[0].pitch=64;
    b.textures[0].format=0x12;b.textures[0].valid=b.textures[0].normalized=1;
    b.textures[0].decode=decode;b.textures[0].opaque=&b.textures[0];
    for(i=0;i<3;i++) {
        v[i].position[0]+=7;v[i].position[1]+=7;
        v[i].tc[0][0]=v[i].tc[0][1]=3.5f/16;v[i].tc[0][3]=1;
        v[i].diffuse[0]=v[i].diffuse[1]=v[i].diffuse[2]=v[i].diffuse[3]=1;
    }
    assert(nv2a_render_draw(&b));assert(color[10*16+10]==0xFF0000FF);
}
static void offscreen_batch_bounds(void)
{
    static const float xy[9][2]={{2,2},{6,2},{2,6},
        {-100,-100},{-90,-100},{-100,-90},{100,100},{110,100},{100,110}};
    Nv2aRenderBatch b={0};Nv2aRenderVertex v[9]={0};
    Nv2aRenderStats before,after;
    uint32_t color[256];unsigned i,scoped;
    b.color=(uint8_t*)color;b.pitch=64;b.height=b.clip_w=b.clip_h=16;
    b.color_bpp=4;b.vertices=v;b.count=9;
    for(i=0;i<9;i++) {
        v[i].position[0]=xy[i][0];v[i].position[1]=xy[i][1];v[i].position[3]=1;
        v[i].diffuse[0]=v[i].diffuse[3]=1;
    }
    for(scoped=0;scoped<2;scoped++) {
        for(i=0;i<256;i++)color[i]=0xFF010203;
        if(scoped)assert(nv2a_render_begin());
        nv2a_render_get_stats(&before);
        assert(nv2a_render_draw(&b));
        if(scoped) {
            assert(color[3*16+3]==0xFF010203);
            assert(nv2a_render_end());
        }
        nv2a_render_get_stats(&after);
        printf("Offscreen bounds scoped=%u: %llu readback bytes (limit144)\n",scoped,
            (unsigned long long)(after.readback_bytes-before.readback_bytes));
        fflush(stdout);
        assert(after.readback_bytes-before.readback_bytes<=6*6*4);
        for(i=0;i<256;i++) {
            unsigned x=i%16,y=i/16;
            assert(color[i]==(x>=2 && y>=2 && x+y<7?0xFFFF0000:0xFF010203));
        }
    }
    /* Two disconnected triangles straddling the whole screen cannot touch
     * any pixel. They must neither resolve nor extend prior queued work. */
    b.vertices=v+3;b.count=6;
    nv2a_render_get_stats(&before);
    assert(nv2a_render_draw(&b));
    nv2a_render_get_stats(&after);
    assert(after.readback_bytes==before.readback_bytes);
    assert(nv2a_render_begin());
    b.vertices=v;b.count=3;assert(nv2a_render_draw(&b));
    b.vertices=v+3;b.count=6;assert(nv2a_render_draw(&b));
    assert(nv2a_render_end());
    nv2a_render_get_stats(&after);
    assert(after.readback_bytes-before.readback_bytes<=6*6*4);
    /* Even ignored geometry must retain the original rejection checks. */
    v[8].position[0]=NAN;
    assert(!nv2a_render_draw(&b));
    puts("Offscreen triangles preserve exact pixels, bounded readback, pending scopes and invalid-vertex rejection");
}
/* HUD draws disable depth without changing the color target. Retaining the
 * allocated depth surface must not force color reuploads, lose CPU edits or
 * merge scopes across a real depth-state/target transition. */
static void depth_toggle_residency(void)
{
    Nv2aRenderBatch b={0};Nv2aRenderVertex v[3]={0};
    Nv2aRenderStats before,after;
    uint32_t color[16*16],depth[16*16];
    const float xy[3][2]={{2,2},{6,2},{2,6}};
    unsigned format,scoped,clipped,i;
    for(format=0;format<2;format++)for(scoped=0;scoped<2;scoped++)for(clipped=0;clipped<2;clipped++) {
        unsigned bpp=format?4:2;
        memset(&b,0,sizeof b);memset(v,0,sizeof v);
        for(i=0;i<256;i++){color[i]=0xFF010203;depth[i]=0xFFFFFFFF;}
        for(i=0;i<3;i++) {
            v[i].position[0]=xy[i][0];v[i].position[1]=xy[i][1];
            v[i].position[2]=format?8388608:32768;v[i].position[3]=1;
            v[i].diffuse[0]=v[i].diffuse[3]=1;
        }
        b.color=(uint8_t*)color;b.pitch=64;b.height=16;b.color_bpp=4;
        b.depth=(uint8_t*)depth;b.depth_pitch=16*bpp;b.depth_bpp=bpp;
        b.depth_test=b.depth_mask=1;b.depth_func=0x201;
        b.clip_w=b.clip_h=16;b.vertices=v;b.count=3;
        assert(nv2a_render_draw(&b));
        nv2a_render_get_stats(&before);
        b.depth_test=0;b.height=b.clip_h=clipped?8:16;
        if(scoped)assert(nv2a_render_begin());
        for(i=0;i<3;i++){v[i].diffuse[0]=0;v[i].diffuse[1]=1;}
        assert(nv2a_render_draw(&b));assert(nv2a_render_draw(&b));
        if(scoped)assert(nv2a_render_end());
        assert(color[3*16+3]==0xFF00FF00);
        b.depth_test=1;b.height=b.clip_h=16;
        for(i=0;i<3;i++){v[i].position[2]-=1;v[i].diffuse[1]=0;v[i].diffuse[2]=1;}
        assert(nv2a_render_draw(&b));
        assert(color[3*16+3]==0xFF0000FF);
        nv2a_render_get_stats(&after);
        /* Same-height HUDs only revalidate depth. Keep exact-height
         * allocation changes: capacity reuse regressed live performance. */
        if(after.surface_uploads-before.surface_uploads!=(clipped?3u:1u)) {
            fprintf(stderr,"depth toggle format%u scope%u clipped%u: %llu uploads, expected%u\n",
                bpp,scoped,clipped,(unsigned long long)(after.surface_uploads-before.surface_uploads),clipped?3u:1u);
            abort();
        }
        assert(after.readback_bytes-before.readback_bytes==(scoped?12u:16u)*36u);
        /* CPU edits while depth is disabled must be seen when enabled again. */
        b.depth_test=0;assert(nv2a_render_draw(&b));
        color[15*16+15]=0x12345678;
        if(format)depth[3*16+3]=0x000000A5;
        else ((uint16_t*)depth)[3*16+3]=0;
        b.depth_test=1;v[0].diffuse[0]=v[1].diffuse[0]=v[2].diffuse[0]=1;
        assert(nv2a_render_draw(&b));
        assert(color[3*16+3]==0xFF0000FF && color[15*16+15]==0x12345678);
    }
}
/* A short draw's full CPU edit is still partial relative to the retained
 * allocation. Preserve GPU depth below it before copying the upload surface. */
static void short_height_depth_preservation(void)
{
    Nv2aRenderBatch b={0};Nv2aRenderVertex v[3]={0};
    uint32_t color[16*16],depth[16*16];
    const float xy[3][2]={{9,9},{13,9},{9,13}};
    unsigned format,i;
    for(format=0;format<2;format++) {
        unsigned bpp=format?4:2,z=format?0x800000:0x8000;
        for(i=0;i<256;i++) {
            color[i]=0xFF010203;
            if(format)depth[i]=0xC00000A5;else ((uint16_t*)depth)[i]=0xC000;
        }
        for(i=0;i<3;i++) {
            memset(&v[i],0,sizeof v[i]);
            v[i].position[0]=xy[i][0];v[i].position[1]=xy[i][1];
            v[i].position[2]=(float)z;v[i].position[3]=1;
            v[i].diffuse[0]=v[i].diffuse[3]=1;
        }
        b.color=(uint8_t*)color;b.pitch=64;b.height=16;b.color_bpp=4;
        b.depth=(uint8_t*)depth;b.depth_pitch=16*bpp;b.depth_bpp=bpp;
        b.depth_test=b.depth_mask=1;b.depth_func=0x201;
        b.clip_w=b.clip_h=16;b.vertices=v;b.count=3;
        assert(nv2a_render_draw(&b));assert(color[10*16+10]==0xFFFF0000);
        b.height=b.clip_h=8;
        for(i=0;i<16*8;i++) {
            if(format)depth[i]=0xE000005A;else ((uint16_t*)depth)[i]=0xE000;
        }
        for(i=0;i<3;i++){v[i].position[0]-=7;v[i].position[1]-=7;}
        assert(nv2a_render_draw(&b));
        b.height=b.clip_h=16;b.depth_func=0x202;b.depth_mask=0;
        for(i=0;i<3;i++) {
            v[i].position[0]+=7;v[i].position[1]+=7;
            v[i].diffuse[0]=0;v[i].diffuse[2]=1;
        }
        assert(nv2a_render_draw(&b));
        assert(color[10*16+10]==0xFF0000FF);
        if(format)assert(depth[10*16+10]==0x800000A5);
        else assert(((uint16_t*)depth)[10*16+10]==0x8000);
    }
}
static void texture_residency(void)
{
    Nv2aRenderBatch b;Nv2aRenderVertex v[3];
    Nv2aRenderStats before,after;
    uint32_t pixel=0,texels[4]={0},pages[40];unsigned pass,i;
    setup(&b,v,&pixel,texels);
    b.combiner.final0=8;b.combiner.final1=0x1800;b.combiner.stage_program=1;
    for(i=0;i<40;i++)pages[i]=0xFF000000|(i*137319);
    for(pass=0;pass<2;pass++) {
        nv2a_render_get_stats(&before);
        for(i=0;i<40;i++) {
            b.textures[0].source=&pages[i];
            assert(nv2a_render_draw(&b));assert(pixel==pages[i]);
        }
        nv2a_render_get_stats(&after);
        if(pass)assert(after.texture_uploads==before.texture_uploads);
    }
}
static unsigned partial_depth_upload_preserves_drawn_z(void)
{
    Nv2aRenderBatch b={0};Nv2aRenderVertex v[3]={0};
    uint32_t color[16*16],depth[16*16];
    const float xy[3][2]={{2,2},{6,2},{2,6}};
    unsigned format,i;
    for(format=0;format<2;format++) {
        unsigned bpp=format?4:2,near=3*16+3,far=15*16+15;
        uint32_t z=format?0x800000:0x8000;
        for(i=0;i<256;i++) {
            color[i]=0xFF010203;
            if(format)depth[i]=0xC00000A5;
            else ((uint16_t*)depth)[i]=0xC000;
        }
        for(i=0;i<3;i++) {
            v[i].position[0]=xy[i][0];v[i].position[1]=xy[i][1];
            v[i].position[2]=(float)z;v[i].position[3]=1;
            v[i].diffuse[0]=v[i].diffuse[3]=1;v[i].diffuse[2]=0;
        }
        b.color=(uint8_t*)color;b.pitch=64;b.height=16;b.color_bpp=4;
        b.depth=(uint8_t*)depth;b.depth_pitch=16*bpp;b.depth_bpp=bpp;
        b.depth_test=b.depth_mask=1;b.depth_func=0x201;
        b.clip_w=b.clip_h=16;b.vertices=v;b.count=3;
        assert(nv2a_render_draw(&b));assert(color[near]==0xFFFF0000);
        /* A CPU edit outside the draw must not restore the upload resource's
         * older Z values over the GPU-written region. The farther blue
         * triangle must still fail LESS against the first red triangle. */
        if(format)depth[far]=0x11223366;
        else ((uint16_t*)depth)[far]=0x1122;
        for(i=0;i<3;i++) {
            v[i].position[2]=(float)(z+1);v[i].diffuse[0]=0;v[i].diffuse[2]=1;
        }
        assert(nv2a_render_draw(&b));
        if(color[near]!=0xFFFF0000) {
            fprintf(stderr,"FAIL partial Z%u upload replaced GPU-written depth outside CPU edit\n",bpp*8);
            return 1;
        }
        assert((format?depth[near]:((uint16_t*)depth)[near])==(format?0x800000A5:0x8000));
        assert((format?depth[far]:((uint16_t*)depth)[far])==(format?0x11223366:0x1122));
        /* An actual edit inside the draw must still take effect. */
        if(format)depth[near]=0xFFFFFF5A;
        else ((uint16_t*)depth)[near]=0xFFFF;
        assert(nv2a_render_draw(&b));assert(color[near]==0xFF0000FF);
        assert((format?depth[near]:((uint16_t*)depth)[near])==(format?0x8000015A:0x8001));
    }
    puts("Partial CPU Z16/Z24 uploads preserve prior GPU depth outside edited bounds");
    return 0;
}
static void packed_exact_pixels(void)
{
    Nv2aRenderBatch b;Nv2aRenderVertex v[3];
    Nv2aRenderStats before,after;
    uint32_t color=0,depth,texels[4]={0};
    unsigned i,j;
    setup(&b,v,&color,texels);
    b.combiner.final0=8;b.combiner.final1=0x1800;b.combiner.stage_program=1;
    b.depth=(uint8_t*)&depth;b.depth_test=b.depth_mask=1;b.depth_func=0x207;
    nv2a_render_get_stats(&before);
    for(i=0;i<512;i++) {
        uint32_t z=next() & (i<256?65535:16777215);
        uint32_t want;
        texels[0]=next();
        b.depth_bpp=i<256?2:4;b.depth_pitch=b.depth_bpp;
        depth=i<256?0xCAFE1234:0x123456A5;
        want=i<256 ? 0xCAFE0000|z : (z<<8)|0xA5;
        for(j=0;j<3;j++)v[j].position[2]=(float)z;
        assert(nv2a_render_draw(&b));
        assert(color==texels[0] && depth==want);
    }
    nv2a_render_get_stats(&after);
    if(getenv("RECOMP_PACKED_READBACK") && !strcmp(getenv("RECOMP_PACKED_READBACK"),"1"))
        assert(after.packed_readbacks-before.packed_readbacks==512);
    else assert(after.packed_readbacks==before.packed_readbacks);
    puts("512 exact ARGB/Z16/Z24 pixels, stencil/guard preservation and synchronous readback passed");
}
static void scoped_draw(Nv2aRenderBatch *b,Nv2aRenderVertex v[3],unsigned n)
{
    unsigned i;
    const float xy[3][2]={{1,1},{13,2},{2,13}};
    for(i=0;i<3;i++) {
        v[i].position[0]=xy[i][0]+(float)(n%3);
        v[i].position[1]=xy[i][1];v[i].position[3]=1;
        v[i].position[2]=(float)((b->depth_bpp==2?60000:15000000)-n*100);
        v[i].diffuse[0]=(float)(n%5)/4;v[i].diffuse[1]=(float)(n%7)/6;
        v[i].diffuse[2]=(float)(n%3)/2;v[i].diffuse[3]=0.5f;
    }
    b->depth_mask=!(n&1);
    assert(nv2a_render_draw(b));
}
static int decode_palette(void *opaque,uint32_t *out)
{
    const Nv2aRenderTexture *t=(const Nv2aRenderTexture*)opaque;
    memcpy(out,t->palette,4);return 1;
}
static void coherent_scopes(void)
{
    Nv2aRenderBatch b={0};Nv2aRenderVertex v[3]={0};
    uint32_t reference[16*16],color[16*16],other[16*16],dummy=0;
    uint8_t reference_z[20*16*4],depth[20*16*4];
    Nv2aRenderStats before,after;
    unsigned dbpp,i,n,pass;
    b.pitch=64;b.height=16;b.color_bpp=4;b.clip_w=b.clip_h=16;
    b.vertices=v;b.count=3;b.depth_test=1;b.depth_func=0x201;
    b.blend_enable=1;b.blend_src=0x302;b.blend_dst=0x303;b.blend_equation=0x8006;
    for(dbpp=2;dbpp<=4;dbpp+=2) {
        b.depth_bpp=dbpp;b.depth_pitch=20*dbpp;
        for(pass=0;pass<2;pass++) {
            uint32_t *c=pass?color:reference;uint8_t *z=pass?depth:reference_z;
            for(i=0;i<256;i++)c[i]=0xFF123456;
            memset(z,0xFF,sizeof depth);b.color=(uint8_t*)c;b.depth=z;
            nv2a_render_get_stats(&before);
            if(pass){assert(nv2a_render_begin());assert(!nv2a_render_begin());}
            for(n=0;n<32;n++)scoped_draw(&b,v,n);
            if(pass) {
                /* The explicit scope has not exposed a partial frame. */
                for(i=0;i<256;i++)assert(c[i]==0xFF123456);
                assert(nv2a_render_end());assert(nv2a_render_flush());
            }
            nv2a_render_get_stats(&after);
            assert(after.draws-before.draws==32);
            if(pass) {
                assert(!memcmp(color,reference,sizeof color));
                assert(!memcmp(depth,reference_z,sizeof depth));
                assert(after.readback_bytes-before.readback_bytes<4096);
                if(getenv("RECOMP_PACKED_READBACK") && !strcmp(getenv("RECOMP_PACKED_READBACK"),"1"))
                    assert(after.packed_readbacks-before.packed_readbacks==1);
            }
        }
    }
    /* Switching targets commits the old target before reusing its resources. */
    b.blend_enable=0;b.color=(uint8_t*)color;b.depth_test=0;
    memset(color,0,sizeof color);memset(other,0,sizeof other);
    assert(nv2a_render_begin());scoped_draw(&b,v,4);
    assert(!color[3*16+3]);b.color=(uint8_t*)other;
    scoped_draw(&b,v,4);assert(color[3*16+3]);assert(!other[3*16+3]);
    assert(nv2a_render_end());assert(!memcmp(color,other,sizeof color));
    /* Sampling the pending target, including an interior source or a palette,
     * must resolve before the decoder sees guest RAM. */
    for(pass=0;pass<2;pass++) {
        uint32_t expected;
        memset(color,0,sizeof color);b.color=(uint8_t*)color;
        memset(b.textures,0,sizeof b.textures);
        assert(nv2a_render_begin());scoped_draw(&b,v,4);
        {
            Nv2aRenderTexture *t=&b.textures[0];
            t->source=pass?&dummy:color+3*16+3;t->source_bytes=4;
            t->palette=pass?color+3*16+3:NULL;t->palette_bytes=pass?4:0;
            t->width=t->height=1;t->pitch=4;t->format=0x12;
            t->valid=t->normalized=1;t->addr_u=t->addr_v=3;
            t->decode=pass?decode_palette:decode;t->opaque=t;
        }
        for(i=0;i<3;i++) {
            v[i].diffuse[0]=v[i].diffuse[1]=v[i].diffuse[2]=v[i].diffuse[3]=1;
            v[i].position[0]+=1;v[i].tc[0][0]=v[i].tc[0][1]=0.5f;v[i].tc[0][3]=1;
        }
        assert(nv2a_render_draw(&b));expected=color[3*16+3];assert(expected);
        assert(nv2a_render_end());assert(color[4*16+4]==expected);
    }
    /* Depth can also feed a later texture. Keep depth testing enabled so the
     * surface layout itself does not provide an accidental target barrier. */
    memset(b.textures,0,sizeof b.textures);memset(depth,0xFF,sizeof depth);
    b.depth=(uint8_t*)depth;b.depth_bpp=4;b.depth_pitch=80;b.depth_test=1;b.depth_func=0x207;
    assert(nv2a_render_begin());scoped_draw(&b,v,4);
    {
        Nv2aRenderTexture *t=&b.textures[0];uint32_t expected;
        t->source=depth+(3*20+3)*4;t->source_bytes=4;
        t->width=t->height=1;t->pitch=4;t->format=0x12;
        t->valid=t->normalized=1;t->addr_u=t->addr_v=3;t->decode=decode;t->opaque=t;
        b.depth_mask=0;
        for(i=0;i<3;i++) {
            v[i].diffuse[0]=v[i].diffuse[1]=v[i].diffuse[2]=v[i].diffuse[3]=1;
            v[i].position[0]+=1;v[i].tc[0][0]=v[i].tc[0][1]=0.5f;v[i].tc[0][3]=1;
        }
        assert(nv2a_render_draw(&b));memcpy(&expected,t->source,4);
        assert(expected==((15000000u-400)<<8|255));
        assert(nv2a_render_end());assert(color[4*16+4]==expected);
    }
    /* A real CPU edit is legal after a flush, even with the scope open. The
     * next draw must upload it while preserving earlier GPU depth elsewhere. */
    memset(b.textures,0,sizeof b.textures);b.depth_func=0x201;
    assert(nv2a_render_begin());scoped_draw(&b,v,4);assert(nv2a_render_flush());
    color[15*16+15]=0x13579BDF;((uint32_t*)depth)[15*20+15]=0x1234565A;
    scoped_draw(&b,v,5);assert(nv2a_render_end());
    assert(color[15*16+15]==0x13579BDF && ((uint32_t*)depth)[15*20+15]==0x1234565A);
    /* Command words and vertex attributes can alias pending target storage.
     * An unrelated read retains the batch; an overlapping read commits it. */
    memset(color,0,sizeof color);b.depth_test=0;
    assert(nv2a_render_begin());assert(nv2a_render_scoped());scoped_draw(&b,v,4);
    nv2a_render_memory_read(other,sizeof other);assert(!color[3*16+3]);
    nv2a_render_memory_read(color+3*16+3,4);assert(color[3*16+3]);
    assert(nv2a_render_end());assert(!nv2a_render_scoped());
    /* Rejecting a later batch still resolves earlier accepted work. */
    memset(b.textures,0,sizeof b.textures);memset(color,0,sizeof color);
    b.depth_test=0;
    assert(nv2a_render_begin());scoped_draw(&b,v,4);b.color_bpp=2;
    assert(!nv2a_render_draw(&b));assert(color[3*16+3]);assert(nv2a_render_end());
    /* Color and Z can share guest storage. Those draws must retain the same
     * per-draw RAM ordering as the synchronous reference, including Z reads. */
    b.color_bpp=4;b.depth_test=1;b.depth_func=0x207;b.depth_bpp=4;b.depth_pitch=64;
    for(pass=0;pass<2;pass++) {
        uint32_t *c=pass?color:reference;
        for(i=0;i<256;i++)c[i]=0xFFFFFFFF;
        b.color=(uint8_t*)c;b.depth=(uint8_t*)c;
        if(pass)assert(nv2a_render_begin());
        for(n=0;n<4;n++)scoped_draw(&b,v,n);
        if(pass){assert(nv2a_render_end());assert(!memcmp(color,reference,sizeof color));}
    }
    puts("Scoped draws match synchronous Z16/Z24/blend pixels; target, color/depth source, palette, CPU-edit, overlapping targets and fallback barriers pass");
}
static int benchmark_depth_toggle(int clipped)
{
    Nv2aRenderBatch b={0};Nv2aRenderVertex v[3]={0};
    Nv2aRenderStats before,after;
    uint32_t *color=(uint32_t*)calloc(640*480,4);
    uint32_t *depth=(uint32_t*)malloc(640*480*4);
    LARGE_INTEGER frequency,start,end;
    unsigned i,pass;
    assert(color && depth);memset(depth,0xFF,640*480*4);
    for(i=0;i<3;i++) {
        v[i].position[0]=i==1?40:8;v[i].position[1]=i==2?40:8;
        v[i].position[2]=8388608;v[i].position[3]=1;
        v[i].diffuse[0]=v[i].diffuse[3]=1;
    }
    b.color=(uint8_t*)color;b.pitch=640*4;b.height=480;b.color_bpp=4;
    b.depth=(uint8_t*)depth;b.depth_pitch=b.pitch;b.depth_bpp=4;
    b.depth_test=b.depth_mask=1;b.depth_func=0x207;
    b.clip_w=640;b.clip_h=480;b.vertices=v;b.count=3;
    assert(nv2a_render_draw(&b));
    QueryPerformanceFrequency(&frequency);
    for(pass=0;pass<3;pass++) {
        nv2a_render_get_stats(&before);QueryPerformanceCounter(&start);
        for(i=0;i<100;i++) {
            /* A HUD phase, then resumed depth, with independent CPU edits. */
            b.depth_test=0;b.height=b.clip_h=clipped?64:480;assert(nv2a_render_draw(&b));
            color[479*640+639]=0x12345678;
            depth[479*640+639]=0x765432A5;
            b.depth_test=1;b.height=b.clip_h=480;assert(nv2a_render_draw(&b));
            assert(color[10*640+10]==0xFFFF0000);
            assert(color[479*640+639]==0x12345678 && depth[479*640+639]==0x765432A5);
        }
        QueryPerformanceCounter(&end);nv2a_render_get_stats(&after);
        printf("depth-toggle pass%u: %.6f ms/pair, %llu uploads, %llu bytes\n",pass,
            1000.0*(end.QuadPart-start.QuadPart)/frequency.QuadPart/100,
            (unsigned long long)(after.surface_uploads-before.surface_uploads),
            (unsigned long long)(after.surface_upload_bytes-before.surface_upload_bytes));
    }
    free(color);free(depth);nv2a_render_shutdown();return 0;
}
int main(int argc,char **argv)
{
    static const unsigned mappings[6]={0,8,16,24,32,48};
    Nv2aRenderBatch b;Nv2aRenderVertex v[3];
    uint32_t pixel=0,texels[4]={0};
    Nv2aRenderStats before,after;
    unsigned family,sample,i,failures=0;
    int bounds_only=argc==2 && !strcmp(argv[1],"--bounds-only");
    int clipped=argc==2 && !strcmp(argv[1],"--benchmark-depth-toggle-clipped");
    int bench=clipped || (argc==2 && !strcmp(argv[1],"--benchmark-depth-toggle"));
    _putenv_s("RECOMP_RENDERER",bounds_only || bench || (argc==2 && !strcmp(argv[1],"--hardware"))?"d3d11":"warp");
    if(bench)return benchmark_depth_toggle(clipped);
    if(bounds_only) {
        assert(nv2a_render_enabled());offscreen_batch_bounds();nv2a_render_shutdown();return 0;
    }
    setup(&b,v,&pixel,texels);
    assert(nv2a_render_enabled());
    for(family=0;family<56;family++) {
        Nv2aCombiner *rc=&b.combiner;
        memset(rc,0,sizeof *rc);
        rc->control=1;rc->stage_program=1|(1<<5)|(1<<10)|(1<<15);
        rc->color_icw[0]=((0x04|(family%8)<<5)<<24)|0x00200522;
        rc->alpha_icw[0]=((0x14|(family%8)<<5)<<24)|0x00201522;
        rc->color_ocw[0]=0xCD0|((family<48?mappings[family/8]:0)<<12);
        rc->alpha_ocw[0]=rc->color_ocw[0];
        rc->final0=0xC;rc->final1=0x1C00;
        if(family>=48) {
            rc->color_icw[0]=0x04200522;rc->alpha_icw[0]=0x1C200000;
            rc->color_ocw[0]=rc->alpha_ocw[0]=0xC0;
            switch(family) {
            case 48:break; /* alpha reads old R0.b while RGB writes it */
            case 49:rc->color_ocw[0]|=0x80000;rc->alpha_ocw[0]=0;break; /* blue-to-alpha */
            case 50:rc->color_ocw[0]=0x4C00;break; /* alpha LSB mux */
            case 51:rc->color_ocw[0]=0x4C00;rc->control|=0x100;break; /* MSB mux */
            case 52:rc->color_ocw[0]|=0x2000;break; /* AB dot */
            case 53:rc->color_ocw[0]=0x100C;break; /* CD dot */
            case 54:rc->final0=0x040F000E;rc->final1=0x040514E0;break;
            case 55:
                rc->control=0x11002;rc->color_icw[1]=0x01200220;rc->color_ocw[1]=0xC00;
                rc->alpha_icw[1]=0x11201220;rc->alpha_ocw[1]=0xC00;break;
            }
        }
        nv2a_render_get_stats(&before);
        for(sample=0;sample<32;sample++) {
            float textures[4][4],expected[4];
            for(i=0;i<4;i++) {
                v[0].diffuse[i]=signed_value();v[0].specular[i]=signed_value();v[0].fog[i]=signed_value();
                texels[i]=next();nv2a_rc_unpack(texels[i],textures[i]);
            }
            for(i=0;i<8;i++){rc->factor0[i]=next();rc->factor1[i]=next();}
            rc->final_c0=next();rc->final_c1=next();
            for(i=1;i<3;i++) {
                memcpy(v[i].diffuse,v[0].diffuse,sizeof v[0].diffuse);
                memcpy(v[i].specular,v[0].specular,sizeof v[0].specular);
                memcpy(v[i].fog,v[0].fog,sizeof v[0].fog);
            }
            nv2a_rc_eval(rc,v[0].diffuse,v[0].specular,v[0].fog,textures,expected);
            pixel=0xAABBCCDD;assert(nv2a_render_draw(&b));
            failures+=check_pixel(pixel,expected,family,sample);
        }
        nv2a_render_get_stats(&after);
        /* Constants changed every draw, while the shader's structure did not. */
        assert(after.shader_compiles-before.shader_compiles<=1);
    }
    /* Projective Q stays independent for each stage, and in-place CPU texture
     * edits invalidate the decoded cache even when its address is unchanged. */
    setup(&b,v,&pixel,texels);
    b.combiner.final0=8;b.combiner.final1=0x1800;b.combiner.stage_program=1;
    {
        uint32_t page[4]={0xFFFF0000,0xFF00FF00,0xFF0000FF,0xFFFFFFFF};
        b.textures[0].source=page;b.textures[0].source_bytes=16;
        b.textures[0].width=b.textures[0].height=2;b.textures[0].pitch=8;
        for(i=0;i<3;i++){v[i].tc[0][0]=1.2f;v[i].tc[0][1]=0.3f;v[i].tc[0][3]=2;}
        assert(nv2a_render_draw(&b));assert(pixel==0xFF00FF00);
        page[1]=0xFF123456;assert(nv2a_render_draw(&b));assert(pixel==0xFF123456);
        pixel=0xAABBCCDD;b.alpha_test=1;b.alpha_func=0x201;b.alpha_ref=0;
        assert(nv2a_render_draw(&b));assert(pixel==0xAABBCCDD);
    }
    /* Failed batches must leave guest RAM unchanged for software fallback. */
    pixel=0x13579BDF;b.color_bpp=2;assert(!nv2a_render_draw(&b));assert(pixel==0x13579BDF);
    coherent_regions();
    offscreen_batch_bounds();
    depth_toggle_residency();
    short_height_depth_preservation();
    texture_residency();
    failures+=partial_depth_upload_preserves_drawn_z();
    packed_exact_pixels();
    coherent_scopes();
    nv2a_render_get_stats(&after);
    printf("Decoded D3D11: %u failures, %llu GPU draws, %llu shader compiles, %llu texture uploads, %llu readback bytes, %llu surface uploads\n",
        failures,(unsigned long long)after.draws,(unsigned long long)after.shader_compiles,
        (unsigned long long)after.texture_uploads,(unsigned long long)after.readback_bytes,
        (unsigned long long)after.surface_uploads);
    nv2a_render_shutdown();
    return failures?1:0;
}
