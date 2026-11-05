#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include "../src/vrs_selective.h"
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
bool CompileAndCheckShader(const char* source) {
    ID3DBlob *b=nullptr,*e=nullptr,*t=nullptr;
    HRESULT hr=D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&b,&e);
    if (e) e->Release(); if (FAILED(hr)||!b) abort();
    hr=D3DDisassemble(b->GetBufferPointer(),b->GetBufferSize(),0,nullptr,&t); b->Release();
    if (FAILED(hr)||!t) abort();
    bool safe=vrsselect::PixelTextSafe((const char*)t->GetBufferPointer()); t->Release(); return safe;
}
int main() {
    using namespace vrsselect;
    CHECK(CompileAndCheckShader("float4 main(float4 p:SV_Position):SV_Target {return p*0.01;}"));
    CHECK(!CompileAndCheckShader("float4 main(float4 p:SV_Position):SV_Target {clip(p.x-1);return p;}"));
    CHECK(!CompileAndCheckShader("float main(float4 p:SV_Position):SV_Depth {return p.z;}"));
    CHECK(!PixelTextSafe(nullptr)); CHECK(!PixelTextSafe("ps_6_0"));
    CHECK(!PixelTextSafe("ps_5_0\ndcl_uav_typed u0"));
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.NumRenderTargets=7; d.PS={(void*)1,1}; d.SampleDesc.Count=1;
    d.DSVFormat=DXGI_FORMAT_D32_FLOAT_S8X24_UINT; d.DepthStencilState.DepthEnable=TRUE;
    d.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;
    for (auto& f:d.RTVFormats) f=DXGI_FORMAT_R8G8B8A8_UNORM;
    auto m=Describe(d,true); CHECK(m.reject==0 && RejectDraw(m,true,7,true)==0);
    CHECK(RejectDraw(m,false,7,true)&Binding); CHECK(RejectDraw(m,true,4,true)&Binding);
    d.RTVFormats[6]=DXGI_FORMAT_R16G16_FLOAT; m=Describe(d,true);
    CHECK(m.motion && RejectDraw(m,true,7,true)==Motion && RejectDraw(m,true,7,false)==0);
    d.RTVFormats[5]=DXGI_FORMAT_R16G16B16A16_FLOAT; CHECK(Describe(d,true).reject&Hdr);
    d.RTVFormats[5]=DXGI_FORMAT_R11G11B10_FLOAT; CHECK(Describe(d,true).reject&Hdr);
    d.RTVFormats[5]=DXGI_FORMAT_R8G8B8A8_UNORM;
    d.BlendState.RenderTarget[0].BlendEnable=TRUE; CHECK(Describe(d,true).reject&Blend);
    d.BlendState.RenderTarget[0].BlendEnable=FALSE; d.BlendState.IndependentBlendEnable=TRUE;
    d.BlendState.RenderTarget[5].LogicOpEnable=TRUE; CHECK(Describe(d,true).reject&Blend);
    d.BlendState.RenderTarget[5].LogicOpEnable=FALSE;
    CHECK(Describe(d,false).reject&Shader);
    d.SampleDesc.Count=4; CHECK(Describe(d,true).reject&Samples); d.SampleDesc.Count=1;
    d.BlendState.AlphaToCoverageEnable=TRUE; CHECK(Describe(d,true).reject&Samples);
    d.BlendState.AlphaToCoverageEnable=FALSE;
    d.DepthStencilState.DepthEnable=FALSE; CHECK(Describe(d,true).reject&NonGBuffer);
    d.DepthStencilState.DepthEnable=TRUE; d.NumRenderTargets=1; CHECK(Describe(d,true).reject&NonGBuffer);
    d.NumRenderTargets=0; CHECK(Describe(d,true).reject&DepthOnly);
    CHECK(Meta{}.reject==Unknown);
    puts("VRS selective: real compiled alpha-discard/depth shaders excluded; RT/blend/motion/history-shape/unknown gates PASS");
}

