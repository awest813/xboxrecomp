/* Raw NV2A combiners for the decoded draw path. The register/word encoding
 * follows nv2a_combiner.c and xemu's pgraph/glsl/psh.c. D3D8's legacy token
 * abstraction does not carry all of this state (including zero stages, final
 * flags and homogeneous texture coordinates), so retain the raw words. */
#include "nv2a_render.h"
#include <stdarg.h>
#include <stdio.h>

#define CONSTANTS \
    "cbuffer State:register(b0) { float4 c0[8]; float4 c1[8];" \
    " float4 fc0; float4 fc1; float4 surface; float4 texinfo[4]; uint4 control; };\n"
#define VARYINGS \
    "struct V { float4 p:SV_POSITION; float4 d:COLOR0; float4 s:COLOR1;" \
    " float4 f:COLOR2; float4 t0:TEXCOORD0; float4 t1:TEXCOORD1;" \
    " float4 t2:TEXCOORD2; float4 t3:TEXCOORD3; noperspective float z:TEXCOORD4; };\n"

const char nv2a_render_vertex_shader[] = CONSTANTS VARYINGS
    "struct I { float4 p:POSITION; float4 d:COLOR0; float4 s:COLOR1;"
    " float4 f:COLOR2; float4 t0:TEXCOORD0; float4 t1:TEXCOORD1;"
    " float4 t2:TEXCOORD2; float4 t3:TEXCOORD3; };"
    "V main(I i) { V o; float w=1.0/i.p.w;"
    " o.p=float4((i.p.x/surface.x*2-1)*w,(1-i.p.y/surface.y*2)*w,"
    " i.p.z*surface.w*w,w); o.d=i.d; o.s=i.s; o.f=i.f;"
    " o.t0=i.t0; o.t1=i.t1; o.t2=i.t2; o.t3=i.t3; o.z=i.p.z; return o; }";

static const char helpers[] =
    "float mapping(float x,uint b) { float p=max(x,0); switch((b>>5)&7) {"
    " case 0:return p; case 1:return 1-saturate(x); case 2:return 2*p-1;"
    " case 3:return 1-2*p; case 4:return p-0.5; case 5:return 0.5-p;"
    " case 6:return x; default:return -x; } }\n"
    "float3 rgb(float4 R[16],uint b) { float4 r=R[b&15];"
    " if(b&16)r.rgb=r.aaa; return float3(mapping(r.r,b),mapping(r.g,b),mapping(r.b,b)); }\n"
    "float alpha(float4 R[16],uint b) { return mapping((b&16)?R[b&15].a:R[b&15].b,b); }\n"
    "float outputmap(float x,uint m) { switch(m) { case 8:return x-0.5;"
    " case 16:return x*2; case 24:return (x-0.5)*2; case 32:return x*4;"
    " case 48:return x*0.5; default:return x; } }\n"
    "void stage(inout float4 R[16],uint icw,uint ocw,uint aicw,uint aocw,uint flags) {"
    " float3 A=rgb(R,icw>>24),B=rgb(R,icw>>16),C=rgb(R,icw>>8),D=rgb(R,icw);"
    " float aA=alpha(R,aicw>>24),aB=alpha(R,aicw>>16),aC=alpha(R,aicw>>8),aD=alpha(R,aicw);"
    " uint fl=ocw>>12,afl=aocw>>12;"
    " bool mux=(flags&1)?R[12].a>=0.5:((int)(R[12].a*255)&1)!=0;"
    " float3 ab=(fl&2)?dot(A,B).xxx:A*B,cd=(fl&1)?dot(C,D).xxx:C*D;"
    " float3 sum=(fl&4)?(mux?cd:ab):ab+cd;"
    " float aab=aA*aB,acd=aC*aD,ams=(afl&4)?(mux?acd:aab):aab+acd;"
    " [unroll]for(uint j=0;j<3;j++) { ab[j]=clamp(outputmap(ab[j],fl&56),-1,1);"
    " cd[j]=clamp(outputmap(cd[j],fl&56),-1,1);sum[j]=clamp(outputmap(sum[j],fl&56),-1,1); }"
    " aab=clamp(outputmap(aab,afl&56),-1,1); acd=clamp(outputmap(acd,afl&56),-1,1);"
    " ams=clamp(outputmap(ams,afl&56),-1,1);"
    " if((ocw>>4)&15){R[(ocw>>4)&15].rgb=ab;if(fl&128)R[(ocw>>4)&15].a=ab.b;}"
    " if(ocw&15){R[ocw&15].rgb=cd;if(fl&64)R[ocw&15].a=cd.b;}"
    " if((ocw>>8)&15)R[(ocw>>8)&15].rgb=sum;"
    " if((aocw>>4)&15)R[(aocw>>4)&15].a=aab;"
    " if(aocw&15)R[aocw&15].a=acd; if((aocw>>8)&15)R[(aocw>>8)&15].a=ams; R[0]=0; }\n"
    "struct P { float4 color:SV_TARGET; float depth:SV_Depth; };\n";

typedef struct { char *p; size_t used, size; int failed; } Writer;
static void emit(Writer *w, const char *format, ...)
{
    va_list args;
    int n;
    if(w->failed)return;
    va_start(args,format);
    n=vsnprintf(w->p+w->used,w->size-w->used,format,args);
    va_end(args);
    if(n<0 || (size_t)n>=w->size-w->used) { w->failed=1;return; }
    w->used+=(size_t)n;
}

int nv2a_render_pixel_shader(const Nv2aCombiner *rc, int has_combiner,
                              char *out, size_t bytes)
{
    Writer w={out,0,bytes,0};
    unsigned i,n=rc->control&255,flags=rc->control>>8;
    if(!bytes)return -1;
    if(n>8)n=8;
    emit(&w,"%s%s%s",CONSTANTS,VARYINGS,helpers);
    for(i=0;i<4;i++) emit(&w,"Texture2D tex%u:register(t%u); SamplerState samp%u:register(s%u);\n",i,i,i,i);
    emit(&w,"P main(V input) { float4 R[16]; [unroll]for(uint k=0;k<16;k++)R[k]=0;"
            " R[3]=input.f;R[4]=input.d;R[5]=input.s;\n");
    for(i=0;i<4;i++) {
        unsigned mode=has_combiner?(rc->stage_program>>(5*i))&31:(i?0:1);
        emit(&w,"R[%u]=float4(0,0,0,1);\n",8+i);
        if(mode==1) {
            emit(&w,"if(texinfo[%u].w && abs(input.t%u.w)>=1e-20) { float2 uv=input.t%u.xy/input.t%u.w;"
                    " if(!texinfo[%u].z)uv/=texinfo[%u].xy;R[%u]=tex%u.Sample(samp%u,uv);"
                    " if(texinfo[%u].w>1.5)R[%u].a=1; }\n",
                    i,i,i,i,i,i,8+i,i,i,i,8+i);
        } else if(i && (mode==NV2A_STAGE_DEPENDENT_AR || mode==NV2A_STAGE_DEPENDENT_GB)) {
            /* An earlier stage's colour is the (normalized) coordinate. */
            emit(&w,"if(texinfo[%u].w) { R[%u]=tex%u.Sample(samp%u,R[%u].%s);"
                    " if(texinfo[%u].w>1.5)R[%u].a=1; }\n",
                    i,8+i,i,i,8+nv2a_rc_stage_source(rc,i),
                    mode==NV2A_STAGE_DEPENDENT_AR?"ar":"gb",i,8+i);
        } else if(mode==4) emit(&w,"R[%u]=saturate(input.t%u);\n",8+i,i);
        else if(mode==5) {
            emit(&w,"[unroll]for(uint j=0;j<4;j++) { bool ge=(control.w>>(%u+j))&1;"
                    " if(ge?input.t%u[j]>=0:input.t%u[j]<0)discard; }\n",i*4,i,i);
        }
    }
    if(has_combiner) {
        emit(&w,"R[12].a=%s;\n",(rc->stage_program&31)?"R[8].a":"1");
        for(i=0;i<n;i++) {
            emit(&w,"R[1]=c0[%u];R[2]=c1[%u];stage(R,%uu,%uu,%uu,%uu,%uu);\n",
                    (flags&16)?i:0,(flags&256)?i:0,rc->color_icw[i],rc->color_ocw[i],
                    rc->alpha_icw[i],rc->alpha_ocw[i],flags);
        }
        if(rc->final0 || rc->final1) {
            unsigned f=rc->final1&255;
            emit(&w,"R[1]=fc0;R[2]=fc1;R[14].rgb=%s+%s;R[14].a=0;\n",
                    (f&64)?"(1-R[5].rgb)":"R[5].rgb",(f&32)?"(1-R[12].rgb)":"R[12].rgb");
            if(f&128)emit(&w,"R[14].rgb=saturate(R[14].rgb);\n");
            emit(&w,"R[15].rgb=rgb(R,%uu)*rgb(R,%uu);R[15].a=0;"
                    " float3 A=rgb(R,%uu),B=rgb(R,%uu),C=rgb(R,%uu),D=rgb(R,%uu);"
                    " float4 result=float4(D+C*(1-A)+B*A,alpha(R,%uu));\n",
                    rc->final1>>24,rc->final1>>16,rc->final0>>24,rc->final0>>16,
                    rc->final0>>8,rc->final0,rc->final1>>8);
        } else emit(&w,"float4 result=R[12];\n");
    } else {
        emit(&w,"float4 result=input.d;if(texinfo[0].w)result=floor(R[8]*input.d*255+0.5)/255;\n");
    }
    emit(&w,"result=saturate(result); if(control.z) { uint a=(uint)floor(result.a*255+0.5),r=control.y;"
            " bool alpha_ok=false;switch(control.x) { case 513:alpha_ok=a<r;break;case 514:alpha_ok=a==r;break;"
            " case 515:alpha_ok=a<=r;break;case 516:alpha_ok=a>r;break;case 517:alpha_ok=a!=r;break;"
            " case 518:alpha_ok=a>=r;break;case 519:alpha_ok=true;break;}if(!alpha_ok)discard;}"
            /* At Z24 magnitudes, adding 0.5 in float rounds odd integers to
             * the next even value. Test the fractional part before adding 1. */
            " float z=clamp(input.z,0,surface.z);z=floor(z)+(frac(z)>=0.5?1:0);"
            " P o;o.color=result;o.depth=z*surface.w;return o;}\n");
    return w.failed?-1:(int)w.used;
}
