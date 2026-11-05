#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstdlib>
#include "../src/vrs_selective.h"
#include "../src/vrs_dxc.h"
#define CHECK(x) do { if(!(x)) { printf("FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
bool CompileAndCheckShader(const char* source) {
    ID3DBlob *b=nullptr,*e=nullptr,*t=nullptr;
    HRESULT hr=D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&b,&e);
    if (e) e->Release(); if (FAILED(hr)||!b) abort();
    hr=D3DDisassemble(b->GetBufferPointer(),b->GetBufferSize(),0,nullptr,&t); b->Release();
    if (FAILED(hr)||!t) abort();
    bool safe=vrsselect::PixelTextSafe((const char*)t->GetBufferPointer()); t->Release(); return safe;
}
bool CompileDxil(vrsdxc::Compiler* c,const char* source) {
    vrsdxc::Buffer input{source,strlen(source),65001};
    const wchar_t* args[]={L"-E",L"main",L"-T",L"ps_6_0",L"-O3",L"-Vd"};
    vrsdxc::Result* r=nullptr;
    HRESULT hr=c->Compile(&input,args,(UINT)(sizeof args/sizeof args[0]),nullptr,vrsdxc::resultIid,reinterpret_cast<void**>(&r));
    if (FAILED(hr) || !r) abort();
    HRESULT status=E_FAIL; ID3DBlob* b=nullptr;
    r->Status(&status); if (FAILED(status) || FAILED(r->Output(&b)) || !b) abort();
    std::string text; bool ok=vrsdxc::Text(c,b->GetBufferPointer(),b->GetBufferSize(),text);
    b->Release(); r->Release(); if (!ok) abort();
    return vrsselect::PixelDxilTextSafe(text.c_str());
}
int main() {
    using namespace vrsselect;
    CHECK(CompileAndCheckShader("float4 main(float4 p:SV_Position):SV_Target {return p*0.01;}"));
    CHECK(!CompileAndCheckShader("float4 main(float4 p:SV_Position):SV_Target {clip(p.x-1);return p;}"));
    CHECK(!CompileAndCheckShader("float main(float4 p:SV_Position):SV_Depth {return p.z;}"));
    CHECK(!PixelTextSafe(nullptr)); CHECK(!PixelTextSafe("ps_6_0"));
    CHECK(!PixelTextSafe("ps_5_0\ndcl_uav_typed u0"));
    wchar_t dll[32768];
    if (GetEnvironmentVariableW(L"N3_VRS_TEST_DXC",dll,32768)) {
        HMODULE module=vrsdxc::Load(dll); auto* c=vrsdxc::New(module); CHECK(c);
        CHECK(CompileDxil(c,"float4 main(float4 p:SV_Position):SV_Target {return p*0.01;}"));
        CHECK(!CompileDxil(c,"float4 main(float4 p:SV_Position):SV_Target {clip(p.x-1);return p;}"));
        CHECK(!CompileDxil(c,"float main(float4 p:SV_Position):SV_Depth {return p.z;}"));
        CHECK(!CompileDxil(c,"float4 main(float4 p:SV_Position,uint s:SV_SampleIndex):SV_Target {return p+s;}"));
        CHECK(!CompileDxil(c,"RWBuffer<uint> u:register(u0); float4 main(float4 p:SV_Position):SV_Target {u[0]=(uint)p.x;return p;}"));
        std::string junk; CHECK(!vrsdxc::Text(c,"bad",3,junk));
        c->Release(); FreeLibrary(module);
        puts("DXIL: actual DXC compile/disassemble opaque PASS; discard/depth/sample/UAV and invalid blob rejected");
    } else puts("SKIP DXIL compiler integration: set N3_VRS_TEST_DXC to dxcompiler.dll");
    CHECK(!PixelDxilTextSafe(nullptr)); CHECK(!PixelDxilTextSafe(""));
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
    d={}; d.NumRenderTargets=8; d.PS={(void*)1,1}; d.SampleDesc.Count=1;
    d.DSVFormat=DXGI_FORMAT_D32_FLOAT_S8X24_UINT; d.DepthStencilState.DepthEnable=TRUE;
    d.RasterizerState.CullMode=D3D12_CULL_MODE_BACK;
    const unsigned captured[8]={91,11,87,87,26,26,87,34};
    for (unsigned i=0;i<8;++i) d.RTVFormats[i]=(DXGI_FORMAT)captured[i];
    CHECK(CapturedGBuffer8(d)); m=Describe(d,true);
    CHECK(m.reject==0 && m.motion && RejectDraw(m,true,8,false)==0);
    CHECK(RejectDraw(m,true,8,true)==Motion);
    CHECK(RejectDraw(m,true,7,false)==Binding);
    CHECK(Describe(d,false).reject==0 && Describe(d,false).shaderPolicy==2); // requested v3 shape policy
    for (unsigned i=0;i<8;++i) { auto copy=d; copy.RTVFormats[i]=DXGI_FORMAT_R8_UNORM;
        CHECK(!CapturedGBuffer8(copy) && (Describe(copy,true).reject&NonGBuffer)); }
    d.BlendState.IndependentBlendEnable=TRUE;
    for (unsigned i=0;i<8;++i) { auto copy=d; copy.BlendState.RenderTarget[i].BlendEnable=TRUE; CHECK(Describe(copy,true).reject&Blend); }
    auto copy=d; copy.DepthStencilState.DepthEnable=FALSE; CHECK(Describe(copy,false).reject==0 && (Describe(copy,false).legacyShape&DepthDisabled));
    copy=d; copy.RasterizerState.CullMode=D3D12_CULL_MODE_NONE; CHECK(Describe(copy,false).reject==0 && (Describe(copy,false).legacyShape&CullNone));
    copy=d; copy.NumRenderTargets=1; CHECK(Describe(copy,true).reject&NonGBuffer);
    CHECK(LayoutMask(-1)==7 && LayoutMask(8)==7 && LayoutMask(0)==0);
    CHECK(RateOption(-1)==0 && RateOption(3)==0 && RateOption(2)==2);
    CHECK(SelectiveRate(0)==D3D12_SHADING_RATE_2X2 && SelectiveRate(1)==D3D12_SHADING_RATE_1X2 && SelectiveRate(2)==D3D12_SHADING_RATE_2X1);
    auto reader=[](void* dst,const void* src,size_t size) { memcpy(dst,src,size); return true; };
    D3D12_INPUT_ELEMENT_DESC input[2]{};
    input[0].SemanticName="blendweights"; input[0].SemanticIndex=1;
    input[1].SemanticName="BLENDINDICES";
    Meta skin=Describe(d,false); DescribeInput(skin,{input,2},reader);
    CHECK(skin.skin==3 && skin.inputKnown && skin.inputCount==2);
    CHECK(RejectDraw(skin,true,8,false,7,true)==Skinned);
    CHECK(RejectDraw(skin,true,8,false,7,false)==0);
    input[0].SemanticName="TEXCOORD"; input[1].InputSlotClass=D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
    DescribeInput(skin,{input,2},reader); CHECK(skin.skin==0 && skin.inputKnown);
    input[1].InputSlotClass=D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    input[1].SemanticName="BLENDINDICES_SUFFIX";
    DescribeInput(skin,{input,2},reader); CHECK(skin.skin==0 && skin.inputKnown);
    input[1].SemanticName="blendweight";
    DescribeInput(skin,{input,2},reader); CHECK(skin.skin==1 && skin.inputKnown);
    DescribeInput(skin,{nullptr,2},reader); CHECK(!skin.inputKnown && !skin.skin);
    DescribeInput(skin,{nullptr,0},reader); CHECK(skin.inputKnown && !skin.skin);
    DescribeInput(skin,{input,33},reader); CHECK(!skin.inputKnown);
    DescribeInput(skin,{input,2},[](void*,const void*,size_t) { return false; }); CHECK(!skin.inputKnown);
    const unsigned layouts[3][8]={{91,11,87,26,87,34},{91,11,87,26,87,26,34},{91,11,87,87,26,26,87,34}};
    for(unsigned n=6;n<=8;++n) {
        d.NumRenderTargets=n;
        for(unsigned i=0;i<8;++i) d.RTVFormats[i]=(DXGI_FORMAT)layouts[n-6][i];
        m=Describe(d,false); CHECK(m.reject==0);
        for(unsigned mask=0;mask<8;++mask) {
            CHECK((RejectDraw(m,true,n,false,mask)==0)==((mask&LayoutBit(n))!=0));
            auto blend=m; blend.reject|=Blend;
            CHECK(RejectDraw(blend,true,n,false,mask)&Blend);
        }
    }
    CHECK(IsExecuteIndirect(59) && !IsExecuteIndirect(27) && !IsExecuteIndirect(12) && !IsExecuteIndirect(13));
    unsigned worldKey[4]{}; const unsigned char bytes[16]={0x15,0x59,0x71,0x4c,0xa4,0x96,0xff,0xfe,0x6d,0xdd,0x3c,0x82,0x7e,0x1d,0x96,0x2e};
    memcpy(worldKey,bytes,16); CHECK(KnownWorldVs(worldKey));
    const unsigned char skinVs[16]={0xe3,0xd0,0x6c,0x93,0x3a,0xdc,0x6a,0x26,0x55,0x62,0x87,0xed,0x3e,0xdb,0x7a,0x62}; unsigned skinKey[4]{}; memcpy(skinKey,skinVs,16); CHECK(!KnownWorldVs(skinKey)); // captured meshlet variant with bSkin
    worldKey[3]^=1; CHECK(!KnownWorldVs(worldKey)); // full digest, never prefix match
    m=Describe(d,false); m.worldVs=1; m.inputKnown=1;
    CHECK(RejectDraw(m,true,8,false,7,true,true,true)==0);
    CHECK(RejectDraw(m,true,8,false,7,true,true,false)==Character);
    m.inputKnown=0; CHECK(RejectDraw(m,true,8,false,7,true,true,true)==Character);
    m.inputKnown=1; m.worldVs=0; CHECK(RejectDraw(m,true,8,false,7,true,true,true)==Character);
    puts("V4: 8 layout masks x 3 captured layouts; 3 rate mappings; explicit skin semantics, generic/instance inputs and guarded reads PASS");
    puts("Captured surface-layout admission PASS; changed formats, blended outputs and motion guard excluded; old depth/cull checks diagnostic only");
    puts("VRS selective: real compiled alpha-discard/depth shaders excluded; RT/blend/motion/history-shape/unknown gates PASS");
}

