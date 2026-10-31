/* Probe default texture CPU access before changing renderer coherence. */
#undef NDEBUG
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d3d11_3.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void compare_reads(ID3D11Device *device,ID3D11Device3 *device3,
    ID3D11DeviceContext *context,ID3D11Texture2D1 *direct,ID3D11RenderTargetView *direct_rtv)
{
    D3D11_TEXTURE2D_DESC d={0};ID3D11Texture2D *legacy=NULL,*staging=NULL;
    ID3D11RenderTargetView *rtv=NULL;uint32_t *pixels=(uint32_t*)malloc(640*480*4);
    LARGE_INTEGER frequency,begin,end;unsigned size,phase,i,y;
    static const unsigned modes[4]={0,1,1,0};
    assert(pixels);QueryPerformanceFrequency(&frequency);
    d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.BindFlags=D3D11_BIND_RENDER_TARGET;
    assert(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&d,NULL,&legacy)));
    assert(SUCCEEDED(ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource*)legacy,NULL,&rtv)));
    d.BindFlags=0;d.Usage=D3D11_USAGE_STAGING;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    assert(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&d,NULL,&staging)));
    for(size=0;size<4;size++)for(phase=0;phase<4;phase++) {
        unsigned w=size==3?640:32u<<(size*1),h=size==3?480:w;D3D11_BOX box={0,0,0,w,h,1};
        QueryPerformanceCounter(&begin);
        for(i=0;i<110;i++) {
            float clear[4]={0,0,0,1};uint32_t expected;
            clear[i&1]=1;expected=i&1?0xFF00FF00:0xFFFF0000;
            if(i==10)QueryPerformanceCounter(&begin); /* warm each mode first */
            ID3D11DeviceContext_ClearRenderTargetView(context,modes[phase]?direct_rtv:rtv,clear);
            if(modes[phase]) {
                assert(SUCCEEDED(ID3D11DeviceContext_Map(context,(ID3D11Resource*)direct,0,D3D11_MAP_READ,0,NULL)));
                ID3D11Device3_ReadFromSubresource(device3,pixels,w*4,w*h*4,(ID3D11Resource*)direct,0,&box);
                ID3D11DeviceContext_Unmap(context,(ID3D11Resource*)direct,0);
            } else {
                D3D11_MAPPED_SUBRESOURCE mapped={0};
                ID3D11DeviceContext_CopySubresourceRegion(context,(ID3D11Resource*)staging,0,0,0,0,
                    (ID3D11Resource*)legacy,0,&box);
                assert(SUCCEEDED(ID3D11DeviceContext_Map(context,(ID3D11Resource*)staging,0,D3D11_MAP_READ,0,&mapped)));
                for(y=0;y<h;y++)memcpy(pixels+y*w,(uint8_t*)mapped.pData+(size_t)y*mapped.RowPitch,w*4);
                ID3D11DeviceContext_Unmap(context,(ID3D11Resource*)staging,0);
            }
            assert(pixels[0]==expected && pixels[w*h-1]==expected);
        }
        QueryPerformanceCounter(&end);
        printf("color read %ux%u phase%u direct%u: %.6f ms/clear+read\n",w,h,phase,modes[phase],
            1000.0*(end.QuadPart-begin.QuadPart)/frequency.QuadPart/100);
    }
    free(pixels);ID3D11RenderTargetView_Release(rtv);
    ID3D11Texture2D_Release(staging);ID3D11Texture2D_Release(legacy);
}
int main(void)
{
    ID3D11Device *device=NULL;ID3D11Device3 *device3=NULL;
    ID3D11DeviceContext *context=NULL;
    ID3D11Texture2D1 *texture=NULL;ID3D11RenderTargetView *rtv=NULL;
    D3D_FEATURE_LEVEL requested=D3D_FEATURE_LEVEL_11_0,actual;
    D3D11_FEATURE_DATA_D3D11_OPTIONS2 options={0};
    D3D11_TEXTURE2D_DESC1 d={0};HRESULT hr;
    uint32_t *pixels;unsigned i;
    const float clear[4]={1,0,0,1};
    hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,&requested,1,
        D3D11_SDK_VERSION,&device,&actual,&context);
    if(FAILED(hr)){printf("create device: %08lX\n",(unsigned long)hr);return 1;}
    hr=ID3D11Device_QueryInterface(device,&IID_ID3D11Device3,(void**)&device3);
    if(FAILED(hr)){printf("Device3 unavailable: %08lX\n",(unsigned long)hr);goto done;}
    hr=ID3D11Device_CheckFeatureSupport(device,D3D11_FEATURE_D3D11_OPTIONS2,&options,sizeof options);
    printf("Options2=%08lX UMA=%d MapOnDefaultTextures=%d StandardSwizzle=%d\n",
        (unsigned long)hr,options.UnifiedMemoryArchitecture,options.MapOnDefaultTextures,
        options.StandardSwizzle);
    if(FAILED(hr) || !options.MapOnDefaultTextures)goto done;
    d.Width=640;d.Height=480;d.MipLevels=d.ArraySize=d.SampleDesc.Count=1;
    d.Format=DXGI_FORMAT_B8G8R8A8_UNORM;d.Usage=D3D11_USAGE_DEFAULT;
    d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.TextureLayout=D3D11_TEXTURE_LAYOUT_UNDEFINED;
    hr=ID3D11Device3_CreateTexture2D1(device3,&d,NULL,&texture);
    printf("CPU-readable default color texture: %08lX\n",(unsigned long)hr);
    if(FAILED(hr))goto done;
    hr=ID3D11Device_CreateRenderTargetView(device,(ID3D11Resource*)texture,NULL,&rtv);
    assert(SUCCEEDED(hr));pixels=(uint32_t*)malloc(640*480*4);assert(pixels);
    ID3D11DeviceContext_ClearRenderTargetView(context,rtv,clear);
    hr=ID3D11DeviceContext_Map(context,(ID3D11Resource*)texture,0,D3D11_MAP_READ,0,NULL);
    printf("Map default color: %08lX\n",(unsigned long)hr);
    if(SUCCEEDED(hr)) {
        ID3D11Device3_ReadFromSubresource(device3,pixels,640*4,640*480*4,
            (ID3D11Resource*)texture,0,NULL);
        ID3D11DeviceContext_Unmap(context,(ID3D11Resource*)texture,0);
        for(i=0;i<640*480;i++)assert(pixels[i]==0xFFFF0000);
        puts("Exact full-surface red pixels passed without a staging texture");
    }
    free(pixels);
    if(SUCCEEDED(hr))compare_reads(device,device3,context,texture,rtv);
done:
    if(rtv)ID3D11RenderTargetView_Release(rtv);
    if(texture)ID3D11Texture2D1_Release(texture);
    if(device3)ID3D11Device3_Release(device3);
    if(context)ID3D11DeviceContext_Release(context);
    if(device)ID3D11Device_Release(device);
    return 0;
}
