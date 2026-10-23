/* Real D3D11 WARP pixels from raw NV2A words, compared with the portable
 * register-combiner evaluator. No game, window or hardware adapter needed. */
#include "d3d8_internal.h"
#include "d3d8_combiners.h"
#include "../../src/kernel/nv2a_combiner.h"
#include <d3dcompiler.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ID3D11Device *device;
static ID3D11DeviceContext *context;
static DWORD rs[512];
ID3D11Device *d3d8_GetD3D11Device(void) { return device; }
ID3D11DeviceContext *d3d8_GetD3D11Context(void) { return context; }
const DWORD *d3d8_GetRenderStates(void) { return rs; }
IDirect3DBaseTexture8 *d3d8_GetStageTexture(DWORD stage) { (void)stage; return NULL; }
D3DFORMAT d3d8_base_format(IDirect3DBaseTexture8 *texture)
{ (void)texture; return D3DFMT_UNKNOWN; }

#define REQUIRE(call) do { HRESULT hr_ = (call); if (FAILED(hr_)) { \
    fprintf(stderr, "%s: HRESULT 0x%08lx\n", #call, (unsigned long)hr_); exit(1); \
} } while (0)

static ID3DBlob *compile(const char *source, const char *profile)
{
    ID3DBlob *code = NULL, *errors = NULL;
    HRESULT hr = D3DCompile(source, strlen(source), NULL, NULL, NULL, "main",
        profile, D3DCOMPILE_IEEE_STRICTNESS, 0, &code, &errors);
    if (FAILED(hr)) {
        fprintf(stderr, "compile: %s\n", errors ?
            (const char *)ID3D10Blob_GetBufferPointer(errors) : "no diagnostics");
        exit(1);
    }
    if (errors) ID3D10Blob_Release(errors);
    return code;
}

static uint32_t rng = 0x432fbb;
static float random_unit(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (float)(rng & 0xffff) / 65535.0f;
}

int main(void)
{
    static const char vs_source[] =
        "cbuffer Inputs:register(b0) { float4 d; float4 s; };"
        "struct V { float4 p:SV_POSITION; float4 d:COLOR0; float4 s:COLOR1;"
        " float3 t0:TEXCOORD0; float3 t1:TEXCOORD1; float3 t2:TEXCOORD2;"
        " float3 t3:TEXCOORD3; };"
        "V main(uint id:SV_VertexID) { V o=(V)0;"
        " o.p=float4(id==2?3:-1,id==1?3:-1,0,1); o.d=d; o.s=s; return o; }";
    ID3D11Texture2D *target, *readback;
    ID3D11RenderTargetView *rtv;
    ID3D11VertexShader *vs;
    ID3D11RasterizerState *rasterizer;
    ID3D11Buffer *vs_cb, *ps_cb;
    ID3DBlob *code;
    D3D11_TEXTURE2D_DESC td = {0};
    D3D11_BUFFER_DESC bd = {0};
    D3D11_RASTERIZER_DESC rd = {0};
    D3D11_VIEWPORT viewport = {0,0,1,1,0,1};
    unsigned family, sample, i;
    int failures = 0, draws = 0;

    REQUIRE(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0, NULL, 0,
        D3D11_SDK_VERSION, &device, NULL, &context));
    code = compile(vs_source, "vs_4_0");
    REQUIRE(ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(code),
        ID3D10Blob_GetBufferSize(code), NULL, &vs));
    ID3D10Blob_Release(code);
    ID3D11DeviceContext_VSSetShader(context, vs, NULL, 0);
    ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    td.Width = td.Height = td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    REQUIRE(ID3D11Device_CreateTexture2D(device, &td, NULL, &target));
    REQUIRE(ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource *)target, NULL, &rtv));
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    REQUIRE(ID3D11Device_CreateTexture2D(device, &td, NULL, &readback));
    bd.ByteWidth = 32; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    REQUIRE(ID3D11Device_CreateBuffer(device, &bd, NULL, &vs_cb));
    bd.ByteWidth = sizeof(NV2APSConstants);
    REQUIRE(ID3D11Device_CreateBuffer(device, &bd, NULL, &ps_cb));
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE;
    REQUIRE(ID3D11Device_CreateRasterizerState(device, &rd, &rasterizer));
    ID3D11DeviceContext_RSSetState(context, rasterizer);
    ID3D11DeviceContext_RSSetViewports(context, 1, &viewport);
    ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rtv, NULL);
    ID3D11DeviceContext_VSSetConstantBuffers(context, 0, 1, &vs_cb);
    ID3D11DeviceContext_PSSetConstantBuffers(context, 0, 1, &ps_cb);

    for (family = 0; family < 5; family++) {
        Nv2aCombiner raw = {0};
        NV2ACombinerState parsed = {0};
        ID3D11PixelShader *ps;
        char source[32768];
        /* Raw hardware packing: A/B/C/D high-to-low, E/F/G in bytes 3/2/1.
         * Asymmetric inputs and dot flags expose errors that a commutative
         * one-texture multiply or AB+CD alone would accidentally conceal. */
        raw.control = 1;
        raw.final0 = 0x04050301; /* v0*v1 + (1-v0)*fog + c0 */
        raw.final1 = 0x00001400; /* G = v0.a */
        if (family == 1) raw.final1 = 0x00000400; /* G = v0.b */
        if (family == 2 || family == 3) {
            raw.color_icw[0] = 0x04010502;
            raw.alpha_icw[0] = 0x14110512;
            raw.color_ocw[0] = 0x2000 | 0xCD; /* dot AB -> r0, CD -> r1 */
            raw.alpha_ocw[0] = 0xCD;
            raw.final0 = family == 2 ? 0x0000000C : 0x0000000D;
            raw.final1 = family == 2 ? 0x00001D00 : 0x00001C00;
        }
        if (family == 4) {
            raw.final0 = 0x040F000E; /* v0 * E*F + (v1+r0) */
            raw.final1 = 0x04051400; /* E=v0, F=v1, G=v0.a */
        }
        memset(rs, 0, sizeof rs);
        rs[D3DRS_PSCOMBINERCOUNT] = raw.control;
        rs[D3DRS_PSRGBINPUTS0] = raw.color_icw[0];
        rs[D3DRS_PSALPHAINPUTS0] = raw.alpha_icw[0];
        rs[D3DRS_PSRGBOUTPUTS0] = raw.color_ocw[0];
        rs[D3DRS_PSALPHAOUTPUTS0] = raw.alpha_ocw[0];
        rs[D3DRS_PSFINALCOMBINERINPUTSABCD] = raw.final0;
        rs[D3DRS_PSFINALCOMBINERINPUTSEFG] = raw.final1;
        rs[D3DRS_PSTEXTUREMODES] = 0x3333; /* all disabled */
        d3d8_combiners_from_render_states(rs, &parsed);
        if (d3d8_combiners_generate_hlsl(&parsed, source, sizeof source) < 0) return 1;
        code = compile(source, "ps_4_0");
        REQUIRE(ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(code),
            ID3D10Blob_GetBufferSize(code), NULL, &ps));
        ID3D10Blob_Release(code);
        ID3D11DeviceContext_PSSetShader(context, ps, NULL, 0);
        for (sample = 0; sample < 64; sample++) {
            NV2APSConstants constants = {0};
            float inputs[2][4], expected[4], tex[4][4] = {{0}};
            D3D11_MAPPED_SUBRESOURCE mapped;
            const float clear[4] = {-4,-4,-4,-4};
            const float *got;
            for (i = 0; i < 8; i++) inputs[i / 4][i % 4] = random_unit();
            raw.factor0[0] = raw.final_c0 = rng;
            random_unit(); raw.factor1[0] = raw.final_c1 = rng;
            nv2a_rc_unpack(raw.factor0[0], constants.c0[0]);
            nv2a_rc_unpack(raw.factor1[0], constants.c1[0]);
            nv2a_rc_unpack(raw.final_c0, constants.final_c0);
            nv2a_rc_unpack(raw.final_c1, constants.final_c1);
            for (i = 0; i < 4; i++) constants.fog_color[i] = random_unit();
            nv2a_rc_eval(&raw, inputs[0], inputs[1], constants.fog_color, tex, expected);
            ID3D11DeviceContext_UpdateSubresource(context, (ID3D11Resource *)vs_cb, 0, NULL, inputs, 0, 0);
            ID3D11DeviceContext_UpdateSubresource(context, (ID3D11Resource *)ps_cb, 0, NULL, &constants, 0, 0);
            ID3D11DeviceContext_ClearRenderTargetView(context, rtv, clear);
            ID3D11DeviceContext_Draw(context, 3, 0);
            ID3D11DeviceContext_CopyResource(context, (ID3D11Resource *)readback, (ID3D11Resource *)target);
            REQUIRE(ID3D11DeviceContext_Map(context, (ID3D11Resource *)readback, 0, D3D11_MAP_READ, 0, &mapped));
            got = (const float *)mapped.pData;
            for (i = 0; i < 4; i++) if (!isfinite(got[i]) || fabsf(got[i] - expected[i]) > 0.00001f) {
                if (failures < 12) fprintf(stderr,
                    "FAIL family=%u sample=%u channel=%u expected=%g got=%g\n",
                    family, sample, i, expected[i], got[i]);
                failures++;
            }
            ID3D11DeviceContext_Unmap(context, (ID3D11Resource *)readback, 0);
            draws++;
        }
        ID3D11PixelShader_Release(ps);
    }
    ID3D11DeviceContext_ClearState(context);
    ID3D11Buffer_Release(vs_cb); ID3D11Buffer_Release(ps_cb);
    ID3D11RasterizerState_Release(rasterizer); ID3D11VertexShader_Release(vs);
    ID3D11RenderTargetView_Release(rtv); ID3D11Texture2D_Release(target);
    ID3D11Texture2D_Release(readback); ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    printf("d3d8_combiners: %d failures (%d WARP draws / %d channels)\n", failures, draws, draws * 4);
    return failures ? 1 : 0;
}
