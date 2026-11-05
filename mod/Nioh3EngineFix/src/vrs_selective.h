// Conservative selective VRS metadata; shared by PSO capture, draw hooks and tests.
#pragma once
#include <d3d12.h>
#include <cstring>
namespace vrsselect {
inline const GUID kMeta = {0xeaa17a30,0x6068,0x4f33,{0x91,0x36,0x4d,0xb1,0x82,0x3a,0xc8,0x09}};
enum Reject : unsigned { Unknown=1, DepthOnly=2, Blend=4, Shader=8, Hdr=16, Motion=32, NonGBuffer=64, Samples=128, Binding=256, Layout=512, Skinned=1024, Character=2048 };
enum LegacyShape : unsigned { RtLayout=1, Dsv=2, DepthDisabled=4, CullNone=8, Geometry=16 };
struct Meta { unsigned version=4, count=0, formats[8]{}, reject=Unknown; bool motion=false;
    unsigned legacyShape=0, dsv=0, depth=0, cull=0, geometry=0, blendMask=0, shaderPolicy=0;
    unsigned skin=0, inputCount=0, inputKnown=0, vsHash[4]{}, worldVs=0; };
inline unsigned LayoutBit(unsigned count) { return count>=6 && count<=8 ? 1u<<(count-6) : 0; }
inline unsigned LayoutMask(int value) { return value>=0 && value<=7 ? (unsigned)value : 7u; }
inline int RateOption(int value) { return value>=0 && value<=2 ? value : 0; }
inline D3D12_SHADING_RATE SelectiveRate(int value) {
    return value==1 ? D3D12_SHADING_RATE_1X2 : value==2 ? D3D12_SHADING_RATE_2X1 : D3D12_SHADING_RATE_2X2;
}
inline const char* RateName(D3D12_SHADING_RATE rate) {
    return rate==D3D12_SHADING_RATE_1X2 ? "1x2" : rate==D3D12_SHADING_RATE_2X1 ? "2x1" :
           rate==D3D12_SHADING_RATE_4X4 ? "4x4" : rate==D3D12_SHADING_RATE_2X2 ? "2x2" : "1x1";
}
// Creation-time only, guarded reader supplied by PSO capture. Explicit skin
// semantics are positive evidence; TEXCOORD/empty layouts are NOT proof of static geometry.
template<class Read> inline void DescribeInput(Meta& m,const D3D12_INPUT_LAYOUT_DESC& input,Read read) {
    m.inputCount=input.NumElements; m.skin=0; m.inputKnown=0;
    if(input.NumElements>32 || (input.NumElements && !input.pInputElementDescs)) return;
    for(unsigned i=0;i<input.NumElements;++i) {
        D3D12_INPUT_ELEMENT_DESC e{};
        if(!read(&e,input.pInputElementDescs+i,sizeof e) || !e.SemanticName) return;
        char name[64]{}; bool ended=false;
        for(unsigned j=0;j<sizeof name;++j) {
            if(!read(name+j,e.SemanticName+j,1)) return;
            if(!name[j]) { ended=true; break; }
            if(name[j]>='a' && name[j]<='z') name[j]-='a'-'A';
        }
        if(!ended) return;
        if(e.InputSlotClass!=D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA) continue;
        if(!strcmp(name,"BLENDWEIGHT") || !strcmp(name,"BLENDWEIGHTS")) m.skin|=1;
        if(!strcmp(name,"BLENDINDICES")) m.skin|=2;
    }
    m.inputKnown=1;
}
// Narrow audited meshlet VS family, original container digests only. Evidence:
// notes/asset-audit.md, notes/material-pb2-next-opportunities.md and
// gpu-shaders/meshvs/out/msv_build_hot.csv. Unknown families stay full rate.
// Require empty, readable IA layout AND an indirect draw at the call site too.
inline bool KnownWorldVs(const unsigned hash[4]) {
    static const unsigned char keys[3][16]={
        {0x15,0x59,0x71,0x4c,0xa4,0x96,0xff,0xfe,0x6d,0xdd,0x3c,0x82,0x7e,0x1d,0x96,0x2e},
        {0x70,0xd0,0xee,0x9f,0xee,0xe5,0x90,0xf9,0xc4,0x8c,0x40,0xb8,0x08,0x85,0x8e,0x27},
        {0xf3,0x11,0xc8,0xe2,0x9e,0x98,0x22,0xee,0x8c,0x34,0xd3,0x8a,0xba,0x8b,0x59,0x66}
    };
    for(const auto& key:keys) if(!memcmp(hash,key,16)) return true;
    return false;
}
inline bool IsExecuteIndirect(unsigned slot) { return slot==59; } // ExecuteBundle(27) is not a draw with the outer PSO
inline bool KeepCharacterFine(const Meta& m,bool indirect) {
    return m.skin || !m.inputKnown || m.inputCount || !m.worldVs || !indirect;
}
inline bool MotionFormat(unsigned f) {
    return f == DXGI_FORMAT_R16G16_FLOAT || f == DXGI_FORMAT_R16G16_SNORM ||
           f == DXGI_FORMAT_R32G32_FLOAT || f == DXGI_FORMAT_R16G16_UNORM;
}
inline bool MaterialFormat(unsigned f) {
    return f == DXGI_FORMAT_R8G8B8A8_UNORM || f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ||
           f == DXGI_FORMAT_R8G8B8A8_SNORM || f == DXGI_FORMAT_R10G10B10A2_UNORM ||
           f == DXGI_FORMAT_R16G16B16A16_UNORM || f == DXGI_FORMAT_R16G16B16A16_SNORM ||
           f == DXGI_FORMAT_R8_UNORM || f == DXGI_FORMAT_R8G8_UNORM;
}
inline bool CapturedGBuffer8(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    // pc2 #69517: exact ordered PSO layout, not permission for arbitrary HDR MRTs.
    const unsigned formats[8]={91,11,87,87,26,26,87,34};
    if (d.NumRenderTargets != 8) return false;
    for (unsigned i=0;i<8;++i) if ((unsigned)d.RTVFormats[i] != formats[i]) return false;
    return true;
}
inline bool CapturedGBuffer(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d) {
    if (CapturedGBuffer8(d)) return true;
    const unsigned rt7[7]={91,11,87,26,87,26,34}, rt6[6]={91,11,87,26,87,34};
    const unsigned* rt=d.NumRenderTargets==7 ? rt7 : d.NumRenderTargets==6 ? rt6 : nullptr;
    if (!rt) return false;
    for(unsigned i=0;i<d.NumRenderTargets;++i) if ((unsigned)d.RTVFormats[i]!=rt[i]) return false;
    return true;
}
inline Meta Describe(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& d, bool pixelVerified) {
    Meta m; m.reject=0; m.count=d.NumRenderTargets;
    if (m.count > 8) { m.reject=Unknown; return m; }
    if (!d.NumRenderTargets || !d.PS.pShaderBytecode || !d.PS.BytecodeLength) m.reject |= DepthOnly;
    const bool captured=CapturedGBuffer(d);
    // v3: known opaque surface layouts are admitted by PSO blend state, not
    // speculative depth/culling conventions or a shader disassembly allowlist.
    // Separate emissive/particle/post layouts and alpha blending stay excluded.
    if (!pixelVerified && !captured) m.reject |= Shader;
    m.shaderPolicy=captured ? 2u : pixelVerified ? 1u : 0u; // 2=shape policy, 1=verified, 0=not verified
    if (d.SampleDesc.Count != 1 || d.RasterizerState.ForcedSampleCount || d.BlendState.AlphaToCoverageEnable) m.reject |= Samples;
    m.dsv=d.DSVFormat; m.depth=d.DepthStencilState.DepthEnable; m.cull=d.RasterizerState.CullMode; m.geometry=d.GS.BytecodeLength!=0;
    m.legacyShape= ((!CapturedGBuffer8(d) && m.count!=7) ? RtLayout : 0) |
        (d.DSVFormat!=DXGI_FORMAT_D32_FLOAT_S8X24_UINT ? Dsv : 0) |
        (!m.depth ? DepthDisabled : 0) | (m.cull==D3D12_CULL_MODE_NONE ? CullNone : 0) | (m.geometry ? Geometry : 0);
    if (!captured && m.legacyShape) m.reject |= NonGBuffer;
    for (unsigned i=0;i<m.count;++i) {
        m.formats[i]=d.RTVFormats[i];
        const auto& b=d.BlendState.RenderTarget[d.BlendState.IndependentBlendEnable ? i : 0];
        if (b.BlendEnable || b.LogicOpEnable) { m.reject |= Blend; m.blendMask |= 1u<<i; }
        if (MotionFormat(m.formats[i])) m.motion=true;
        else if (!captured && !MaterialFormat(m.formats[i])) m.reject |= Hdr;
    }
    return m;
}
inline unsigned RejectDraw(const Meta& m, bool bindingKnown, unsigned boundCount, bool guardMotion,
                           unsigned layouts=7, bool skipSkinned=true, bool keepCharacters=false, bool indirect=false) {
    return m.reject | ((!bindingKnown || boundCount != m.count) ? Binding : 0) | ((guardMotion && m.motion) ? Motion : 0) |
        ((layouts & LayoutBit(m.count)) ? 0 : Layout) | ((skipSkinned && m.skin) ? Skinned : 0) |
        ((keepCharacters && KeepCharacterFine(m,indirect)) ? Character : 0);
}
// D3DDisassemble text: no discard/alpha test, pixel UAV effects, depth/coverage
// output or sample-frequency inputs. Missing/unsupported bytecode fails closed.
inline bool PixelTextSafe(const char* p) {
    return p && (strstr(p,"ps_5_0") || strstr(p,"ps_5_1")) &&
        !strstr(p,"discard") && !strstr(p,"dcl_uav") && !strstr(p,"oDepth") &&
        !strstr(p,"oMask") && !strstr(p,"oStencilRef") && !strstr(p,"sample_index") &&
        !strstr(p,"coverage") && !strstr(p,"linear sample") && !strstr(p,"noperspective sample");
}
inline bool PixelDxilTextSafe(const char* p) {
    // DXC disassembly includes signatures, PSV and the complete LLVM module.
    // Unsupported text fails closed; no substring search of bit-packed DXIL.
    return p && strstr(p,"Pixel Shader") && strstr(p,"!dx.shaderModel") &&
        strstr(p,"!\"ps\", i32 6,") && strstr(p,"SampleFrequency=0") &&
        !strstr(p,"dx.op.discard") && !strstr(p,"UAV") &&
        !strstr(p,"dx.op.atomic") && !strstr(p,"dx.op.textureStore") &&
        !strstr(p,"dx.op.bufferStore") && !strstr(p,"dx.op.rawBufferStore") &&
        !strstr(p,"SV_Depth") && !strstr(p,"SV_Coverage") && !strstr(p,"SV_StencilRef") &&
        !strstr(p,"SV_SampleIndex") && !strstr(p,"dx.op.sampleIndex") &&
        !strstr(p,"dx.op.coverage") && !strstr(p,"dx.op.innerCoverage") &&
        !strstr(p,"dx.op.evalSampleIndex");
}
}
