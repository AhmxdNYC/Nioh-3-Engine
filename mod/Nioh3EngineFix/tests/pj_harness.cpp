// pj_harness.cpp - PJ (psojournal.cpp) on a real D3D12 device (WARP: CPU only, the GPU is not touched). Not a game test.
// Run by tests/run_pj_harness.py, one process per "launch":
//   pj_harness rec   <dir>  launch 1: the "engine" creates a root signature, a graphics PSO (input layout; twice, with
//                           different garbage in the padding), a compute PSO and a stream PSO -> 3 records journaled.
//                           A spy hooked BELOW PJ writes an independent deep description of every create to spy_rec.txt.
//   pj_harness warm  <dir>  launch 2: the engine creates the graphics PSO only; the prewarm must create the other two;
//                           the spy's descriptions of this launch (engine + prewarm) must equal launch 1's exactly.
//   pj_harness again <dir>  launch 3: nothing new is journaled, the file is unchanged.
//   pj_harness trunc <dir>  the file's last chunk was cut by the runner: tail dropped, 2 records, prewarm still works.
//   pj_harness badsh <dir>  a shader chunk was corrupted by the runner: its PSOs are reported missing, nothing crashes.
//   pj_harness build <dir>  the header's exe stamp was changed by the runner: a new journal is started.
//   pj_harness hand  <dir>  PJH = 1: the prewarm keeps its 3 PSOs; the engine's creates of the same descs get them back
//                           (no create reaches the spy below PJ), and each handed object works (SetPipelineState + Close).
//   pj_harness handoff <dir> PJH = 2 (installed, switch off): kept but not handed; the engine's own creates run.
// Journal v2: every launch first creates a "boot list" PSO (made in every launch, never journaled) and a boot-only PSO
// before the boot gate (quiet 200 ms); only creates after the gate are journaled.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "../src/psojournal.h"
#include "../src/psoswap.h"

static int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            printf("FAIL %s: ", #c);          \
            printf(__VA_ARGS__);              \
            printf("\n");                     \
            ++g_fail;                         \
        }                                     \
    } while (0)

static std::string g_logText;
static void Log(const char* fmt, ...) {
    char b[1024];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    printf("  [mod] %s\n", b);
    g_logText += b;
    g_logText += "\n";
}

// ---- spy (installed below PJ): independent deep description of each successful create ------------------------------------
using GfxFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);
using CsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_COMPUTE_PIPELINE_STATE_DESC*, REFIID, void**);
using RsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, UINT, const void*, SIZE_T, REFIID, void**);
using StreamFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_PIPELINE_STATE_STREAM_DESC*, REFIID, void**);
static GfxFn s_gfx;
static CsFn s_cs;
static RsFn s_rs;
static StreamFn s_stream;
static SRWLOCK s_lock = SRWLOCK_INIT;
static std::set<std::string> s_seen;
static volatile LONG s_spyCreates = 0;  // PSO creates that reached the spy (below PJ)
static std::vector<std::pair<const void*, unsigned long long>> s_rsMap;

static unsigned long long Fnv(const void* p, size_t n) {
    unsigned long long h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 1099511628211ull;
    return h;
}
static std::string F(const char* fmt, ...) {
    char b[512];
    va_list a;
    va_start(a, fmt);
    vsnprintf(b, sizeof b, fmt, a);
    va_end(a);
    return b;
}
static std::string Rs(const void* p) {
    if (!p) return "rs=none ";
    AcquireSRWLockShared(&s_lock);
    unsigned long long h = 0;
    for (auto& e : s_rsMap)
        if (e.first == p) h = e.second;
    ReleaseSRWLockShared(&s_lock);
    return F("rs=%016llx ", h);
}
static std::string Sh(const char* n, const D3D12_SHADER_BYTECODE& b) {
    return b.pShaderBytecode ? F("%s=%016llx/%zu ", n, Fnv(b.pShaderBytecode, b.BytecodeLength), b.BytecodeLength) : "";
}
static std::string Blend(const D3D12_BLEND_DESC& b) {
    std::string s = F("blend %d %d ", b.AlphaToCoverageEnable, b.IndependentBlendEnable);
    for (const auto& r : b.RenderTarget)
        s += F("[%d %d %d %d %d %d %d %d %d %u]", r.BlendEnable, r.LogicOpEnable, r.SrcBlend, r.DestBlend, r.BlendOp, r.SrcBlendAlpha,
               r.DestBlendAlpha, r.BlendOpAlpha, r.LogicOp, r.RenderTargetWriteMask);
    return s + " ";
}
static std::string Ras(const D3D12_RASTERIZER_DESC& r) {
    return F("ras %d %d %d %d %g %g %d %d %d %u %d ", r.FillMode, r.CullMode, r.FrontCounterClockwise, r.DepthBias,
             r.DepthBiasClamp, r.SlopeScaledDepthBias, r.DepthClipEnable, r.MultisampleEnable, r.AntialiasedLineEnable,
             r.ForcedSampleCount, r.ConservativeRaster);
}
static std::string Ds(const D3D12_DEPTH_STENCIL_DESC& d) {
    auto op = [](const D3D12_DEPTH_STENCILOP_DESC& o) { return F("%d%d%d%d", o.StencilFailOp, o.StencilDepthFailOp, o.StencilPassOp, o.StencilFunc); };
    return F("ds %d %d %d %d %u %u ", d.DepthEnable, d.DepthWriteMask, d.DepthFunc, d.StencilEnable, d.StencilReadMask,
             d.StencilWriteMask) + op(d.FrontFace) + " " + op(d.BackFace) + " ";
}
static std::string Il(const D3D12_INPUT_LAYOUT_DESC& il) {
    std::string s = F("il %u ", il.NumElements);
    for (UINT i = 0; i < il.NumElements; ++i) {
        const auto& e = il.pInputElementDescs[i];
        s += F("[%s %u %d %u %u %d %u]", e.SemanticName, e.SemanticIndex, e.Format, e.InputSlot, e.AlignedByteOffset,
               e.InputSlotClass, e.InstanceDataStepRate);
    }
    return s + " ";
}
static std::string Rtv(UINT n, const DXGI_FORMAT* f) {
    std::string s = F("rt %u", n);
    for (UINT i = 0; i < 8; ++i) s += F(" %d", f[i]);
    return s + " ";
}
static void Seen(std::string s) {
    AcquireSRWLockExclusive(&s_lock);
    s_seen.insert(std::move(s));
    ReleaseSRWLockExclusive(&s_lock);
}
static HRESULT STDMETHODCALLTYPE SpyGfx(ID3D12Device* d, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* p, REFIID r, void** pp) {
    InterlockedIncrement(&s_spyCreates);
    const HRESULT hr = s_gfx(d, p, r, pp);
    if (SUCCEEDED(hr) && p)
        Seen("GFX " + Rs(p->pRootSignature) + Sh("vs", p->VS) + Sh("ps", p->PS) + Sh("ds", p->DS) + Sh("hs", p->HS) +
             Sh("gs", p->GS) + Blend(p->BlendState) + F("mask %08x ", p->SampleMask) + Ras(p->RasterizerState) +
             Ds(p->DepthStencilState) + Il(p->InputLayout) + F("cut %d topo %d ", p->IBStripCutValue, p->PrimitiveTopologyType) +
             Rtv(p->NumRenderTargets, p->RTVFormats) + F("dsv %d sd %u/%u node %u flags %d cached %zu", p->DSVFormat,
                                                         p->SampleDesc.Count, p->SampleDesc.Quality, p->NodeMask, p->Flags,
                                                         p->CachedPSO.CachedBlobSizeInBytes));
    return hr;
}
static HRESULT STDMETHODCALLTYPE SpyCs(ID3D12Device* d, const D3D12_COMPUTE_PIPELINE_STATE_DESC* p, REFIID r, void** pp) {
    InterlockedIncrement(&s_spyCreates);
    const HRESULT hr = s_cs(d, p, r, pp);
    if (SUCCEEDED(hr) && p) Seen("CS " + Rs(p->pRootSignature) + Sh("cs", p->CS) + F("node %u flags %d", p->NodeMask, p->Flags));
    return hr;
}
static HRESULT STDMETHODCALLTYPE SpyRs(ID3D12Device* d, UINT n, const void* b, SIZE_T len, REFIID r, void** pp) {
    const HRESULT hr = s_rs(d, n, b, len, r, pp);
    if (SUCCEEDED(hr) && pp && *pp) {
        AcquireSRWLockExclusive(&s_lock);
        s_rsMap.push_back({*pp, Fnv(b, len)});
        ReleaseSRWLockExclusive(&s_lock);
    }
    return hr;
}
static HRESULT STDMETHODCALLTYPE SpyStream(ID3D12Device* d, const D3D12_PIPELINE_STATE_STREAM_DESC* p, REFIID r, void** pp) {
    InterlockedIncrement(&s_spyCreates);
    const HRESULT hr = s_stream(d, p, r, pp);
    if (SUCCEEDED(hr) && p) {
        std::string s = "STREAM ";
        const unsigned char* b = static_cast<const unsigned char*>(p->pPipelineStateSubobjectStream);
        size_t pos = 0;
        while (pos + 4 <= p->SizeInBytes) {
            UINT t;
            memcpy(&t, b + pos, 4);
            const unsigned char* in = b + pos + 8;  // every type the harness uses has its inner data at +8 (pointer-aligned)
            size_t sz = 8;
            switch (t) {
                case 0: { void* q; memcpy(&q, in, 8); s += Rs(q); break; }
                case 1: case 2: { D3D12_SHADER_BYTECODE c; memcpy(&c, in, sizeof c); s += Sh(t == 1 ? "vs" : "ps", c); sz = 16; break; }
                case 12: { D3D12_INPUT_LAYOUT_DESC c; memcpy(&c, in, sizeof c); s += Il(c); sz = 16; break; }
                case 14: { UINT v; memcpy(&v, b + pos + 4, 4); s += F("topo %u ", v); sz = 0; break; }
                case 15: { D3D12_RT_FORMAT_ARRAY a; memcpy(&a, b + pos + 4, sizeof a); s += Rtv(a.NumRenderTargets, a.RTFormats); sz = 32; break; }
                case 8: { D3D12_BLEND_DESC c; memcpy(&c, b + pos + 4, sizeof c); s += Blend(c); sz = sizeof c - 4; break; }
                default: s += F("type%u ", t); break;
            }
            pos = (pos + 8 + sz + 7) & ~size_t(7);
        }
        Seen(s);
    }
    return hr;
}
static void PatchSlot(void** vt, int i, void* hook, void** orig) {
    DWORD o, t;
    VirtualProtect(&vt[i], 8, PAGE_READWRITE, &o);
    *orig = vt[i];
    vt[i] = hook;
    VirtualProtect(&vt[i], 8, o, &t);
}

// ---- the "engine" ----------------------------------------------------------------------------------------------------------
static ID3DBlob* Compile(const char* src, const char* target) {
    ID3DBlob* b = nullptr;
    ID3DBlob* e = nullptr;
    if (FAILED(D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, "main", target, 0, 0, &b, &e))) {
        printf("compile %s failed: %s\n", target, e ? (const char*)e->GetBufferPointer() : "?");
        exit(2);
    }
    return b;
}
static const char* kVs = "struct I{float3 p:POSITION; float2 t:TEXCOORD3;}; struct O{float4 p:SV_Position; float2 t:TEXCOORD0;};"
                         "cbuffer C:register(b0){float4x4 m;}; O main(I i){O o; o.p=mul(m,float4(i.p,1)); o.t=i.t; return o;}";
static const char* kPs = "Texture2D t:register(t0); SamplerState s:register(s0);"
                         "float4 main(float4 p:SV_Position, float2 c:TEXCOORD0):SV_Target{return t.Sample(s,c);}";
static const char* kCs = "RWStructuredBuffer<uint> u:register(u0); [numthreads(64,1,1)] void main(uint3 i:SV_DispatchThreadID){u[i.x]=i.x*7u;}";

struct Engine {
    ID3D12Device* dev = nullptr;
    ID3D12Device2* dev2 = nullptr;
    ID3D12RootSignature* rs = nullptr;
    ID3DBlob *vs = nullptr, *ps = nullptr, *cs = nullptr;
    D3D12_INPUT_ELEMENT_DESC il[2] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 3, DXGI_FORMAT_R16G16_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    void Init() {
        IDXGIFactory4* f = nullptr;
        CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&f);
        f->EnumWarpAdapter(__uuidof(IDXGIAdapter), (void**)&warp);
        if (FAILED(D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev))) {
            printf("no WARP D3D12 device\n");
            exit(2);
        }
        f->Release();
        dev->QueryInterface(__uuidof(ID3D12Device2), (void**)&dev2);
        vs = Compile(kVs, "vs_5_0");
        ps = Compile(kPs, "ps_5_0");
        cs = Compile(kCs, "cs_5_0");
    }
    IDXGIAdapter* warp = nullptr;
    // the engine's own D3D12CreateDevice: through the exe's IAT slot (d3d12.dll import), where psoswap's hook sits
    ID3D12Device* CreateAgain() {
        ID3D12Device* d = nullptr;
        D3D12CreateDevice(warp, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&d);
        return d;
    }
    void CreateRs() {
        D3D12_DESCRIPTOR_RANGE r[2] = {{D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0}, {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, 1}};
        D3D12_ROOT_PARAMETER p[2] = {};
        p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        p[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        p[1].DescriptorTable = {2, r};
        p[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        D3D12_STATIC_SAMPLER_DESC ss = {};
        ss.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        ss.AddressU = ss.AddressV = ss.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        ss.MaxLOD = D3D12_FLOAT32_MAX;
        ss.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC d = {2, p, 1, &ss, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        ID3DBlob *b = nullptr, *e = nullptr;
        D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1, &b, &e);
        // through the vtable like the engine (the hooks sit there)
        HRESULT hr = dev->CreateRootSignature(0, b->GetBufferPointer(), b->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&rs);
        if (FAILED(hr)) { printf("CreateRootSignature failed %08lx\n", (unsigned long)hr); exit(2); }
        b->Release();
    }
    int used = 0, useFail = 0;  // handed objects exercised by Use()
    // the returned PSO must be a working object of this device: SetPipelineState on a fresh list + Close
    void Use(ID3D12PipelineState* p, bool compute) {
        ID3D12Device* d = nullptr;
        if (FAILED(p->GetDevice(__uuidof(ID3D12Device), (void**)&d)) || d != dev) ++useFail;
        if (d) d->Release();
        const D3D12_COMMAND_LIST_TYPE t = compute ? D3D12_COMMAND_LIST_TYPE_COMPUTE : D3D12_COMMAND_LIST_TYPE_DIRECT;
        ID3D12CommandAllocator* a = nullptr;
        ID3D12GraphicsCommandList* l = nullptr;
        if (FAILED(dev->CreateCommandAllocator(t, __uuidof(ID3D12CommandAllocator), (void**)&a)) ||
            FAILED(dev->CreateCommandList(0, t, a, p, __uuidof(ID3D12GraphicsCommandList), (void**)&l))) {
            ++useFail;
        } else {
            l->SetPipelineState(p);
            if (FAILED(l->Close())) ++useFail;
        }
        if (l) l->Release();
        if (a) a->Release();
        ++used;
    }
    bool use = false;
    DXGI_FORMAT rt1 = DXGI_FORMAT_R8G8B8A8_UNORM;  // the boot-list PSO differs in this format only
    // garbage = the byte the desc memory holds before the fields are set (padding stays garbage, as on the engine's stack)
    HRESULT Gfx(unsigned char garbage) {
        static D3D12_GRAPHICS_PIPELINE_STATE_DESC d;
        memset(&d, garbage, sizeof d);
        d.pRootSignature = rs;
        d.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        d.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        d.DS = d.HS = d.GS = {nullptr, 0};
        d.StreamOutput.pSODeclaration = nullptr;
        d.StreamOutput.NumEntries = 0;
        d.StreamOutput.pBufferStrides = nullptr;
        d.StreamOutput.NumStrides = 0;
        d.StreamOutput.RasterizedStream = 0;
        d.BlendState.AlphaToCoverageEnable = FALSE;
        d.BlendState.IndependentBlendEnable = FALSE;
        for (auto& r : d.BlendState.RenderTarget) {
            r.BlendEnable = TRUE; r.LogicOpEnable = FALSE;
            r.SrcBlend = D3D12_BLEND_SRC_ALPHA; r.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; r.BlendOp = D3D12_BLEND_OP_ADD;
            r.SrcBlendAlpha = D3D12_BLEND_ONE; r.DestBlendAlpha = D3D12_BLEND_ZERO; r.BlendOpAlpha = D3D12_BLEND_OP_ADD;
            r.LogicOp = D3D12_LOGIC_OP_NOOP; r.RenderTargetWriteMask = 0x7;
        }
        d.SampleMask = 0xFFFFFFFF;
        d.RasterizerState = {D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0, 0.f, 0.f, TRUE, FALSE, FALSE, 0,
                             D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF};
        auto& z = d.DepthStencilState;
        z.DepthEnable = TRUE; z.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; z.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        z.StencilEnable = FALSE; z.StencilReadMask = 0xFF; z.StencilWriteMask = 0xFF;
        z.FrontFace = z.BackFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
        d.InputLayout = {il, 2};
        d.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        d.NumRenderTargets = 2;
        for (auto& f : d.RTVFormats) f = DXGI_FORMAT_UNKNOWN;
        d.RTVFormats[0] = DXGI_FORMAT_R11G11B10_FLOAT;
        d.RTVFormats[1] = rt1;
        d.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        d.SampleDesc = {1, 0};
        d.NodeMask = 0;
        d.CachedPSO = {nullptr, 0};
        d.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ID3D12PipelineState* p = nullptr;
        const HRESULT hr = dev->CreateGraphicsPipelineState(&d, __uuidof(ID3D12PipelineState), (void**)&p);
        if (p && use) Use(p, false);
        if (p) p->Release();
        return hr;
    }
    HRESULT Cs() {
        D3D12_COMPUTE_PIPELINE_STATE_DESC d;
        memset(&d, 0xEE, sizeof d);
        d.pRootSignature = rs;
        d.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
        d.NodeMask = 0;
        d.CachedPSO = {nullptr, 0};
        d.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        ID3D12PipelineState* p = nullptr;
        const HRESULT hr = dev->CreateComputePipelineState(&d, __uuidof(ID3D12PipelineState), (void**)&p);
        if (p && use) Use(p, true);
        if (p) p->Release();
        return hr;
    }
    HRESULT Stream() {
        // RS, VS, PS, input layout, topology, RT formats (single-sample, no depth): CD3DX12-style 8-aligned subobjects
        struct alignas(8) SoPtr { UINT t; void* p; };
        struct alignas(8) SoSh { UINT t; D3D12_SHADER_BYTECODE b; };
        struct alignas(8) SoIl { UINT t; D3D12_INPUT_LAYOUT_DESC d; };
        struct alignas(8) SoU { UINT t; UINT v; };
        struct alignas(8) SoRt { UINT t; D3D12_RT_FORMAT_ARRAY a; };
        struct alignas(8) Stream {
            SoPtr rs; SoSh vs; SoSh ps; SoIl il; SoU topo; SoRt rt;
        } s;
        memset(&s, 0xA5, sizeof s);  // garbage in every padding byte
        s.rs = {0, rs};
        s.vs.t = 1; s.vs.b = {vs->GetBufferPointer(), vs->GetBufferSize()};
        s.ps.t = 2; s.ps.b = {ps->GetBufferPointer(), ps->GetBufferSize()};
        s.il.t = 12; s.il.d = {il, 2};
        s.topo = {14, D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE};
        s.rt.t = 15;
        for (auto& f : s.rt.a.RTFormats) f = DXGI_FORMAT_UNKNOWN;
        s.rt.a.RTFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
        s.rt.a.NumRenderTargets = 1;
        const D3D12_PIPELINE_STATE_STREAM_DESC sd = {sizeof s, &s};
        ID3D12PipelineState* p = nullptr;
        const HRESULT hr = dev2->CreatePipelineState(&sd, __uuidof(ID3D12PipelineState), (void**)&p);
        if (p && use) Use(p, false);
        if (p) p->Release();
        return hr;
    }
};

static std::wstring W(const char* s) {
    wchar_t b[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, s, -1, b, MAX_PATH);
    return b;
}

// ---- PL (boot pipeline library) launches: every create before the boot gate is a boot-list create --------------------------
//   plrec     PL = 1, no file: gfx + cs + stream are compiled and stored; the library is written at the gate
//   plwarm    the 3 are loaded from the library (no create reaches the spy below PJ; each object works); one new desc is
//             compiled + stored -> written to .new (the current file is mapped)
//   plwarm2   .new promoted: all 4 loaded, nothing written
//   plbad     blob byte flipped (runner): checksum refuses it -> new library, 3 stored, file rewritten
//   plstale   header driver version changed (runner): refused -> new library
//   plgarb    blob replaced by garbage with a matching checksum (runner): D3D12 refuses it -> new library, nothing crashes
//   plpj      PJ = 1 + PL = 1: boot creates loaded; a create after the gate is journaled by PJ and not touched by PL
//   pl2       PL = 2: timing only, no library, no file
static int PlMain(const std::string& mode, const std::wstring& dir) {
    const std::wstring ini = dir + L"\\test.ini";
    WritePrivateProfileStringW(L"Patches", L"PJ", mode == "plpj" ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Patches", L"PJH", L"0", ini.c_str());
    WritePrivateProfileStringW(L"Patches", L"PL", mode == "pl2" ? L"2" : L"1", ini.c_str());
    Engine e;
    e.Init();
    void** vt = *reinterpret_cast<void***>(e.dev);
    PatchSlot(vt, 10, (void*)&SpyGfx, (void**)&s_gfx);
    PatchSlot(vt, 11, (void*)&SpyCs, (void**)&s_cs);
    PatchSlot(vt, 16, (void*)&SpyRs, (void**)&s_rs);
    PatchSlot(vt, 47, (void*)&SpyStream, (void**)&s_stream);
    pj::StartInfo si;
    si.log = &Log;
    si.iniPath = ini.c_str();
    si.folder = dir.c_str();
    si.quietMs = 300;
    pj::Start(si);
    psoswap::StartInfo pi;
    pi.log = &Log;
    pi.iniPath = ini.c_str();
    pi.onDevice = &pj::OnDevice;
    pi.observeOnly = pj::Wanted(ini.c_str());
    psoswap::Start(pi);
    ID3D12Device* again = e.CreateAgain();
    if (again) again->Release();
    CHECK(pj::WaitPlReadyForTest(10000), "PL ready");
    pj::Stats st;
    pj::GetStats(st);
    const bool warm = mode == "plwarm" || mode == "plwarm2" || mode == "plpj";
    if (mode == "pl2") CHECK(st.pl == 2 && st.plPhase == -1, "PL %d phase %d (want 2/-1)", st.pl, st.plPhase);
    else CHECK(st.pl == 1 && st.plPhase == 1 && st.plFromFile == warm, "PL %d phase %d fromFile %d (want 1/1/%d)", st.pl, st.plPhase,
               st.plFromFile, warm);
    e.CreateRs();
    const LONG before = s_spyCreates;
    e.use = true;
    e.rt1 = DXGI_FORMAT_R8G8B8A8_UNORM;
    CHECK(SUCCEEDED(e.Gfx(0x21)), "boot gfx");
    CHECK(SUCCEEDED(e.Cs()), "boot cs");
    CHECK(SUCCEEDED(e.Stream()), "boot stream");
    if (mode == "plwarm" || mode == "plwarm2") {
        e.rt1 = DXGI_FORMAT_R16G16B16A16_FLOAT;  // new in plwarm, from the library in plwarm2
        CHECK(SUCCEEDED(e.Gfx(0x63)), "boot gfx (other RT format)");
        e.rt1 = DXGI_FORMAT_R8G8B8A8_UNORM;
    }
    const LONG reached = s_spyCreates - before;
    CHECK(pj::WaitBootForTest(10000), "boot gate opened");
    LONG after = 0;
    if (mode == "plpj") {
        const LONG b2 = s_spyCreates;
        CHECK(SUCCEEDED(e.Gfx(0x44)), "runtime gfx after the gate");
        after = s_spyCreates - b2;
        CHECK(pj::WaitIdleForTest(10000), "writer idle");
    }
    e.use = false;
    if (mode != "pl2") CHECK(pj::WaitPlDoneForTest(20000), "PL closed");
    else Sleep(300);
    pj::GetStats(st);
    CHECK(e.useFail == 0, "loaded / created objects failed to work: %d of %d", e.useFail, e.used);
    CHECK(st.plErrors == 0 && st.plEarly == 0, "PL errors %llu early %llu", (unsigned long long)st.plErrors, (unsigned long long)st.plEarly);
    WIN32_FIND_DATAW fd;
    auto exists = [&](const wchar_t* pat) {
        HANDLE h = FindFirstFileW((dir + pat).c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) return false;
        FindClose(h);
        return true;
    };
    if (mode == "plrec" || mode == "plbad" || mode == "plstale" || mode == "plgarb") {
        CHECK(st.plLoaded == 0 && st.plStored == 3, "loaded %llu stored %llu (want 0/3)", (unsigned long long)st.plLoaded,
              (unsigned long long)st.plStored);
        CHECK(reached == 3, "%ld creates reached the driver (want 3)", reached);
        CHECK(st.plRebuilt == (mode != "plrec"), "rebuilt %d", st.plRebuilt);
        CHECK(exists(L"\\pl_*.bin") && !exists(L"\\pl_*.new"), "library written in place");
        CHECK(st.bootN == 3 && st.bootThreads == 1, "boot creates %llu threads %u (want 3/1)", (unsigned long long)st.bootN, st.bootThreads);
    } else if (mode == "plwarm") {
        CHECK(st.plLoaded == 3 && st.plMiss == 1 && st.plStored == 1, "loaded %llu miss %llu stored %llu (want 3/1/1)",
              (unsigned long long)st.plLoaded, (unsigned long long)st.plMiss, (unsigned long long)st.plStored);
        CHECK(reached == 1, "%ld creates reached the driver (want 1: the new desc only)", reached);
        CHECK(exists(L"\\pl_*.new"), "grown library written to .new (current file mapped)");
    } else if (mode == "plwarm2") {
        CHECK(st.plLoaded == 4 && st.plMiss == 0 && st.plStored == 0, "loaded %llu miss %llu stored %llu (want 4/0/0)",
              (unsigned long long)st.plLoaded, (unsigned long long)st.plMiss, (unsigned long long)st.plStored);
        CHECK(st.plFileEntries == 4, "file entries %u (want 4)", st.plFileEntries);
        CHECK(reached == 0, "%ld creates reached the driver (want 0)", reached);
        CHECK(!exists(L"\\pl_*.new"), ".new promoted, nothing new written");
    } else if (mode == "plpj") {
        CHECK(st.plLoaded == 3 && st.plStored == 0, "loaded %llu stored %llu (want 3/0)", (unsigned long long)st.plLoaded,
              (unsigned long long)st.plStored);
        CHECK(reached == 0 && after == 1, "driver creates boot %ld after gate %ld (want 0/1)", reached, after);
        CHECK(st.newRecs == 1 && st.bootCreates == 3, "PJ journaled %u boot %llu (want 1/3)", st.newRecs, (unsigned long long)st.bootCreates);
    } else if (mode == "pl2") {
        CHECK(st.plLoaded == 0 && st.plStored == 0 && reached == 3, "loaded %llu stored %llu reached %ld (want 0/0/3)",
              (unsigned long long)st.plLoaded, (unsigned long long)st.plStored, reached);
        CHECK(!exists(L"\\pl_*"), "no library file with PL = 2");
        CHECK(st.bootN == 3, "boot creates %llu (want 3)", (unsigned long long)st.bootN);
        CHECK(g_logText.find("boot PSO list: 3 engine creates") != std::string::npos, "boot timing line");
    } else {
        return 2;
    }
    if (mode == "plgarb") CHECK(g_logText.find("D3D12 refused the blob") != std::string::npos, "D3D12 refusal logged");
    if (mode == "plbad") CHECK(g_logText.find("checksum mismatch") != std::string::npos, "checksum refusal logged");
    if (mode == "plstale") CHECK(g_logText.find("other GPU / driver version") != std::string::npos, "stale refusal logged");
    char line[900];
    printf("  status: %s\n", pj::StatusLine(line, sizeof line));
    printf(g_fail ? "%s: %d FAILED\n" : "%s: OK\n", mode.c_str(), g_fail);
    fflush(stdout);
    ExitProcess(g_fail ? 1 : 0);
}

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    const std::string mode = argv[1];
    const std::wstring dir = W(argv[2]);
    if (mode.compare(0, 2, "pl") == 0) return PlMain(mode, dir);
    const std::wstring ini = dir + L"\\test.ini";
    WritePrivateProfileStringW(L"Patches", L"PJ", L"1", ini.c_str());
    WritePrivateProfileStringW(L"Patches", L"PJ_MaxMB", L"8", ini.c_str());
    WritePrivateProfileStringW(L"Patches", L"PJH", mode == "hand" ? L"1" : mode == "handoff" ? L"2" : L"0", ini.c_str());

    Engine e;
    e.Init();
    // spy first (below PJ), then PJ (outermost), as psoswap + PJ are ordered in the mod
    void** vt = *reinterpret_cast<void***>(e.dev);
    PatchSlot(vt, 10, (void*)&SpyGfx, (void**)&s_gfx);
    PatchSlot(vt, 11, (void*)&SpyCs, (void**)&s_cs);
    PatchSlot(vt, 16, (void*)&SpyRs, (void**)&s_rs);
    PatchSlot(vt, 47, (void*)&SpyStream, (void**)&s_stream);

    pj::StartInfo si;
    si.log = &Log;
    si.iniPath = ini.c_str();
    si.folder = dir.c_str();
    si.quietMs = 200;
    pj::Start(si);
    // as dllmain: psoswap armed observe-only ([ShaderSwap] Enabled = 0, no GPUProf) reports the device to PJ
    psoswap::StartInfo pi;
    pi.log = &Log;
    pi.iniPath = ini.c_str();
    pi.onDevice = &pj::OnDevice;
    pi.observeOnly = pj::Wanted(ini.c_str());
    psoswap::Start(pi);
    CHECK(g_logText.find("IAT hook on d3d12.dll!D3D12CreateDevice armed") != std::string::npos, "psoswap IAT hook armed");
    CHECK(psoswap::GetState() == psoswap::State::Off, "psoswap stays off in observe-only mode");
    ID3D12Device* again = e.CreateAgain();  // the runtime returns the same device for the same adapter
    CHECK(again == e.dev, "same device");
    if (again) again->Release();
    pj::OnDevice(e.dev);  // a second report of the same device hooks nothing twice
    CHECK(g_logText.find("PSOSWAP: hooked device") == std::string::npos, "psoswap hooked nothing itself");
    pj::Stats st;
    e.CreateRs();  // every launch, as the engine does (the D3D12 runtime returns the same object for the same blob)
    // the boot PSO list: created in every launch before the title, never journaled
    e.rt1 = DXGI_FORMAT_R16G16B16A16_FLOAT;
    CHECK(SUCCEEDED(e.Gfx(0x77)), "boot-list gfx");
    e.rt1 = DXGI_FORMAT_R8G8B8A8_UNORM;
    // the runtime PSO the engine already created in this launch before the gate (the prewarm must skip it)
    if (mode == "warm" || mode == "again") CHECK(SUCCEEDED(e.Gfx(0x11)), "gfx (engine, same desc, before the prewarm)");
    if (mode == "trunc" || mode == "badsh") CHECK(SUCCEEDED(e.Cs()), "cs (engine, before the prewarm)");
    CHECK(pj::WaitBootForTest(10000), "boot gate opened");

    if (mode == "rec") {
        CHECK(SUCCEEDED(e.Gfx(0xCD)), "gfx");
        CHECK(SUCCEEDED(e.Gfx(0x5A)), "gfx again with other padding garbage");
        CHECK(SUCCEEDED(e.Cs()), "cs");
        CHECK(SUCCEEDED(e.Stream()), "stream");
        CHECK(pj::WaitIdleForTest(10000), "writer idle");
        CHECK(pj::WaitPrewarmForTest(10000), "prewarm finished (nothing to do on the first launch)");
        pj::GetStats(st);
        CHECK(st.hooked, "hooked via psoswap's D3D12CreateDevice hook");
        CHECK(st.seen == 5 && st.bootCreates == 1, "engine creates seen %llu boot %llu (want 5/1)", (unsigned long long)st.seen,
              (unsigned long long)st.bootCreates);
        CHECK(st.fileRecs == 0 && st.newRecs == 3, "records file %u new %u (want 0/3: padding garbage must not split a desc)",
              st.fileRecs, st.newRecs);
        CHECK(st.shaders == 3 && st.rootSigs == 1, "shaders %u root sigs %u (want 3/1)", st.shaders, st.rootSigs);
        CHECK(st.skipped == 0 && st.dropped == 0, "skipped %llu dropped %llu", (unsigned long long)st.skipped,
              (unsigned long long)st.dropped);
        CHECK(st.pwOk == 0, "no prewarm on the first launch");
        FILE* f = nullptr;
        _wfopen_s(&f, (dir + L"\\spy_rec.txt").c_str(), L"w");
        for (const auto& s : s_seen)
            if (f) fprintf(f, "%s\n", s.c_str());
        if (f) fclose(f);
        CHECK(s_seen.size() == 4, "spy saw %zu distinct descs (want 4: 3 journaled + the boot-list one)", s_seen.size());
    } else if (mode == "warm" || mode == "again") {
        CHECK(pj::WaitPrewarmForTest(20000), "prewarm finished");
        CHECK(pj::WaitIdleForTest(10000), "writer idle");
        pj::GetStats(st);
        CHECK(st.fileRecs == 3 && st.newRecs == 0, "records file %u new %u (want 3/0)", st.fileRecs, st.newRecs);
        CHECK(st.pwCandidates == 3 && st.pwAlready == 1 && st.pwOk == 2 && st.pwFailed == 0 && st.pwMissing == 0,
              "prewarm cand %u already %u ok %u failed %u missing %u (want 3/1/2/0/0)", st.pwCandidates, st.pwAlready, st.pwOk,
              st.pwFailed, st.pwMissing);
        std::set<std::string> rec;
        char line[8192];
        FILE* f = nullptr;
        _wfopen_s(&f, (dir + L"\\spy_rec.txt").c_str(), L"r");
        while (f && fgets(line, sizeof line, f)) {
            line[strcspn(line, "\n")] = 0;
            rec.insert(line);
        }
        if (f) fclose(f);
        CHECK(rec.size() == 4, "spy_rec.txt has %zu descs", rec.size());
        CHECK(s_seen == rec, "the prewarm's descs (seen below PJ) differ from the engine's (launch 1)");
        if (s_seen != rec)
            for (const auto& s : s_seen)
                if (!rec.count(s)) printf("  only now: %s\n", s.c_str());
    } else if (mode == "trunc") {
        CHECK(pj::WaitPrewarmForTest(20000), "prewarm finished");
        pj::GetStats(st);
        CHECK(st.fileRecs == 2, "records %u (want 2: the cut stream record is dropped)", st.fileRecs);
        CHECK(st.pwAlready == 1 && st.pwOk == 1 && st.pwMissing == 0, "already %u ok %u missing %u (want 1/1/0)", st.pwAlready,
              st.pwOk, st.pwMissing);
    } else if (mode == "badsh") {
        CHECK(pj::WaitPrewarmForTest(20000), "prewarm finished");
        pj::GetStats(st);
        CHECK(st.fileRecs == 3 && st.pwAlready == 1 && st.pwOk == 0 && st.pwMissing == 2 && st.pwFailed == 0,
              "records %u already %u ok %u missing %u failed %u (want 3/1/0/2/0: VS chunk corrupt)", st.fileRecs, st.pwAlready,
              st.pwOk, st.pwMissing, st.pwFailed);
    } else if (mode == "build") {
        CHECK(SUCCEEDED(e.Gfx(0)), "gfx");
        CHECK(pj::WaitIdleForTest(10000), "writer idle");
        CHECK(pj::WaitPrewarmForTest(10000), "prewarm finished");
        pj::GetStats(st);
        CHECK(st.fileRecs == 0 && st.newRecs == 1 && st.pwOk == 0, "records file %u new %u ok %u (want 0/1/0: new journal)",
              st.fileRecs, st.newRecs, st.pwOk);
    } else if (mode == "hand" || mode == "handoff") {
        const bool on = mode == "hand";
        CHECK(pj::WaitPrewarmForTest(20000), "prewarm finished");
        pj::GetStats(st);
        CHECK(st.handover == (on ? 1 : 2) && (pj::HandoverSwitch() != nullptr), "PJH %d, switch installed", st.handover);
        CHECK(st.pwOk == 3 && st.kept == 3 && st.keepRefused == 0, "prewarm ok %u kept %llu refused %llu (want 3/3/0)", st.pwOk,
              (unsigned long long)st.kept, (unsigned long long)st.keepRefused);
        const LONG before = s_spyCreates;
        e.use = true;
        CHECK(SUCCEEDED(e.Gfx(0x33)), "gfx (engine)");
        CHECK(SUCCEEDED(e.Cs()), "cs (engine)");
        CHECK(SUCCEEDED(e.Stream()), "stream (engine)");
        CHECK(SUCCEEDED(e.Gfx(0x44)), "gfx again (engine: a second create of a handed desc compiles as usual)");
        e.use = false;
        const LONG reached = s_spyCreates - before;
        pj::GetStats(st);
        if (on) {
            CHECK(st.handed == 3 && st.kept == 0, "handed %llu waiting %llu (want 3/0)", (unsigned long long)st.handed,
                  (unsigned long long)st.kept);
            CHECK(reached == 1, "%ld creates reached the driver (want 1: only the repeated gfx)", reached);
            CHECK(st.handMiss == 1, "runtime creates without a prewarmed PSO %llu (want 1)", (unsigned long long)st.handMiss);
        } else {
            CHECK(st.handed == 0 && st.kept == 3, "handed %llu waiting %llu (want 0/3: switch off)", (unsigned long long)st.handed,
                  (unsigned long long)st.kept);
            CHECK(reached == 4, "%ld creates reached the driver (want 4)", reached);
        }
        CHECK(e.used == 4 && e.useFail == 0, "handed / created objects used %d, failed %d (want 4/0)", e.used, e.useFail);
        CHECK(st.newRecs == 0, "nothing new journaled (%u)", st.newRecs);
    } else {
        return 2;
    }
    char line[900];
    printf("  status: %s\n", pj::StatusLine(line, sizeof line));
    printf(g_fail ? "%s: %d FAILED\n" : "%s: OK\n", mode.c_str(), g_fail);
    fflush(stdout);
    ExitProcess(g_fail ? 1 : 0);  // background threads of the module are still running (as in the game)
}
