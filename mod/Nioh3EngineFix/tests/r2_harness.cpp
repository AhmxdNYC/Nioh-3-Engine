// r2_harness.cpp - gpuopt barrier rewrites R2a / R2b / R1 (1.7.10) on a real D3D12 device of this PC, no game.
// The real gpuopt.cpp + gpuopt_thunks.asm hook the runtime's command-list vtable. A tiny "engine" records the barrier
// pattern the rewrites target and submits IDENTICAL work for every switch combination:
//   * per frame (a ring of 3 lists, one ExecuteCommandLists, then a fence wait = the ECL boundary where buffers decay to COMMON)
//     the tracker of each buffer is reset to COMMON like SynTransitionResource's list stamp, so the first use is COMMON -> X:
//       A  (buffer, no UAV flag): COMMON -> COPY_DEST (previous state NPSR: R2a read case), upload copy, -> NPSR, left in NPSR
//       B  (buffer, no UAV flag): COMMON -> NPSR (previous state COPY_DEST: R2a write case = 2-struct pair), left in COPY_DEST
//       U  (buffer, ALLOW_UNORDERED_ACCESS): COMMON -> UAV (R2b), two RMW dispatches with a UAV barrier between
//       S  (texture) NPSR -> NPSR|PSR and A NPSR -> NPSR|PSR before a pixel-shader draw into RT (R1 supersets)
//   * U (after every frame) and RT (64x64, after every frame) are copied to readback buffers.
// Checks: every combination yields BIT-IDENTICAL buffers and render target; the counters of an enabled switch move, those of a
// disabled one stay 0; R1 drops exactly the supersets; the trace CSV carries res_dim / res_flags; the device is not removed.
// No D3D12 debug layer: the rewritten before-states are for vkd3d-proton (the layer validates against the D3D12 state).
// Build + run: py -I tests/run_r2_harness.py
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/gpuopt.h"
#include "../src/gpuprof.h"
#include "../src/memstats.h"

namespace memstats {  // VS bind census is outside this rewrite test.
bool BindsWanted() { return false; }
void OnBind(int, uintptr_t, uintptr_t, uintptr_t, uint16_t, uint32_t) {}
}  // namespace memstats
#include "../src/psoswap.h"

static void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("  [log] %s\n", b);
}

static int g_fails = 0;
static void check(bool c, const char* m) {
    printf("%s %s\n", c ? "OK  " : "FAIL", m);
    g_fails += !c;
}

constexpr UINT kN = 64 * 1024;          // buffer bytes
constexpr int kFrames = 64;
constexpr int kRing = 3;
constexpr UINT kNPSR = 0x40, kPSR = 0x80, kCD = 0x400, kCS = 0x800, kUAV = 0x8, kRT = 0x4;

static ID3D12Device* dev;
static ID3D12CommandQueue* q;
static ID3D12Fence* fence;
static HANDLE ev;
static UINT64 fv;

struct Trk {
    ID3D12Resource* r;
    UINT state;
};

static ID3D12Resource* MakeBuf(UINT bytes, D3D12_HEAP_TYPE heap, D3D12_RESOURCE_FLAGS fl, D3D12_RESOURCE_STATES st) {
    D3D12_HEAP_PROPERTIES hp = {heap};
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = bytes;
    rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags = fl;
    ID3D12Resource* r = nullptr;
    dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, st, nullptr, __uuidof(ID3D12Resource), (void**)&r);
    return r;
}

static ID3D12Resource* MakeTex(D3D12_RESOURCE_FLAGS fl, D3D12_RESOURCE_STATES st) {
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rd.Width = 64;
    rd.Height = 64;
    rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rd.SampleDesc.Count = 1;
    rd.Flags = fl;
    D3D12_CLEAR_VALUE cv = {};
    cv.Format = rd.Format;
    ID3D12Resource* r = nullptr;
    dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, st, (fl & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) ? &cv : nullptr,
                                 __uuidof(ID3D12Resource), (void**)&r);
    return r;
}

// the engine's SynTransitionResource: barrier only when the tracked state differs, one ResourceBarrier(1) call each
static void Tr(ID3D12GraphicsCommandList* cl, Trk& t, UINT x) {
    if (t.state == x) return;
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = t.r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = (D3D12_RESOURCE_STATES)t.state;
    b.Transition.StateAfter = (D3D12_RESOURCE_STATES)x;
    cl->ResourceBarrier(1, &b);
    t.state = x;
}
static void UavBar(ID3D12GraphicsCommandList* cl, ID3D12Resource* r) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = r;
    cl->ResourceBarrier(1, &b);
}

static void Wait() {
    q->Signal(fence, ++fv);
    fence->SetEventOnCompletion(fv, ev);
    WaitForSingleObject(ev, 10000);
}

static ID3DBlob* Compile(const char* src, const char* target, const char* entry = "main") {
    ID3DBlob *b = nullptr, *e = nullptr;
    if (FAILED(D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, 0, 0, &b, &e))) {
        printf("shader error: %s\n", e ? (const char*)e->GetBufferPointer() : "?");
        return nullptr;
    }
    return b;
}

static void Fill(ID3D12Resource* up, uint32_t seed) {
    uint32_t* p = nullptr;
    up->Map(0, nullptr, (void**)&p);
    uint32_t h = seed * 2654435761u + 12345u;
    for (UINT i = 0; i < kN / 4; ++i) {
        h ^= h << 13; h ^= h >> 17; h ^= h << 5;
        p[i] = h;
    }
    up->Unmap(0, nullptr);
}

struct Out {
    std::vector<uint8_t> u, rt;
    gpuopt::Stats d;   // counter deltas of the run
};

int main() {
    printf("=== gpuopt R2a / R2b / R1 on a real device ===\n");
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"r2_harness.ini");
    DeleteFileW(ini);
    wchar_t csv[MAX_PATH];
    GetTempPathW(MAX_PATH, csv);
    wcscat_s(csv, L"Nioh3EngineFix.gpuopt-trace.csv");
    DeleteFileW(csv);
    WritePrivateProfileStringW(L"GpuOpt", L"Barriers", L"1", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"AnyCaller", L"1", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"DeferThreads", L"*", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"InstallDelaySec", L"600", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"TraceRecords", L"3000", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"TraceDelaySec", L"0", ini);
    WritePrivateProfileStringW(L"ShaderSwap", L"Enabled", L"0", ini);
    gpuopt::StartInfo gi;
    gi.log = &Log;
    gi.iniPath = ini;
    gi.wine = "harness";   // the rewrites only run when a Wine version is given
    gpuopt::Start(gi);
    check(gpuopt::InstallNow(), "gpuopt hooks installed (Barriers = 1)");
    check(gpuopt::Batching(), "batching on");

    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev))) return 2;
    D3D12_COMMAND_QUEUE_DESC qd = {};
    dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void**)&q);
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fence);
    ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    ID3D12CommandAllocator* al[kRing];
    ID3D12GraphicsCommandList* cls[kRing];
    for (int i = 0; i < kRing; ++i) {
        dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&al[i]);
        dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al[i], nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&cls[i]);
        cls[i]->Close();
    }

    // resources
    Trk A{MakeBuf(kN, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON), 0};
    Trk B{MakeBuf(kN, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COMMON), 0};
    Trk U{MakeBuf(kN, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON), 0};
    Trk S{MakeTex(D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST), kCD};
    Trk RT{MakeTex(D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET, D3D12_RESOURCE_STATE_RENDER_TARGET), kRT};
    ID3D12Resource* upA = MakeBuf(kN, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
    ID3D12Resource* upB = MakeBuf(kN, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
    ID3D12Resource* upS = MakeBuf(64 * 64 * 4, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
    ID3D12Resource* rbU = MakeBuf(kN * kFrames, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
    ID3D12Resource* rbRT = MakeBuf(64 * 64 * 4 * kFrames, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
    if (!A.r || !B.r || !U.r || !S.r || !RT.r || !upA || !upB || !upS || !rbU || !rbRT) return 2;
    {
        uint32_t* p = nullptr;
        upS->Map(0, nullptr, (void**)&p);
        for (UINT i = 0; i < 64 * 64; ++i) p[i] = 0xFF000000u | (i * 2654435761u >> 8);
        upS->Unmap(0, nullptr);
    }

    // descriptors / pipelines
    D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    ID3D12DescriptorHeap* heap = nullptr;
    dev->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), (void**)&heap);
    D3D12_SHADER_RESOURCE_VIEW_DESC sv = {};
    sv.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    sv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    sv.Texture2D.MipLevels = 1;
    dev->CreateShaderResourceView(S.r, &sv, heap->GetCPUDescriptorHandleForHeapStart());
    D3D12_DESCRIPTOR_HEAP_DESC rhd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    dev->CreateDescriptorHeap(&rhd, __uuidof(ID3D12DescriptorHeap), (void**)&rtvHeap);
    dev->CreateRenderTargetView(RT.r, nullptr, rtvHeap->GetCPUDescriptorHandleForHeapStart());

    ID3D12RootSignature *rsC = nullptr, *rsG = nullptr;
    {
        D3D12_ROOT_PARAMETER p[4] = {};
        p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[0].Descriptor = {0, 0};
        p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; p[1].Descriptor = {1, 0};
        p[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[2].Descriptor = {0, 0};
        p[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p[3].Constants = {0, 0, 1};
        D3D12_ROOT_SIGNATURE_DESC d = {4, p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ID3DBlob *sb = nullptr, *eb = nullptr;
        D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1, &sb, &eb);
        dev->CreateRootSignature(0, sb->GetBufferPointer(), sb->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&rsC);
        D3D12_DESCRIPTOR_RANGE rg = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, 0};
        D3D12_ROOT_PARAMETER g[2] = {};
        g[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; g[0].DescriptorTable = {1, &rg};
        g[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        g[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; g[1].Descriptor = {1, 0};
        g[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC gd = {2, g, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        D3D12SerializeRootSignature(&gd, D3D_ROOT_SIGNATURE_VERSION_1, &sb, &eb);
        dev->CreateRootSignature(0, sb->GetBufferPointer(), sb->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&rsG);
    }
    ID3D12PipelineState *psoC = nullptr, *psoG = nullptr;
    {
        ID3DBlob* cs = Compile("ByteAddressBuffer A : register(t0); ByteAddressBuffer B : register(t1); RWByteAddressBuffer U : register(u0);\n"
                               "cbuffer c : register(b0) { uint f; };\n"
                               "[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { uint o = t.x * 4; uint a = A.Load(o), b = B.Load(o);\n"
                               "  uint u = U.Load(o); U.Store(o, u * 1664525u + (a ^ (b * 3u)) + f); }\n", "cs_5_0");
        ID3DBlob* vs = Compile("float4 main(uint id : SV_VertexID) : SV_Position { float2 p = float2((id << 1) & 2, id & 2);\n"
                               "  return float4(p * 2 - 1, 0, 1); }\n", "vs_5_0");
        ID3DBlob* ps = Compile("Texture2D<float4> S : register(t0); ByteAddressBuffer A : register(t1);\n"
                               "float4 main(float4 p : SV_Position) : SV_Target { uint2 q = uint2(p.xy); uint a = A.Load(((q.y * 64 + q.x) & 0x3FFF) * 4);\n"
                               "  float4 s = S.Load(int3(q, 0)); return float4(s.rgb * 0.5 + float3((a & 255) / 255.0, ((a >> 8) & 255) / 255.0, ((a >> 16) & 255) / 255.0) * 0.5, 1); }\n", "ps_5_0");
        if (!cs || !vs || !ps) return 2;
        D3D12_COMPUTE_PIPELINE_STATE_DESC cd = {};
        cd.pRootSignature = rsC;
        cd.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
        dev->CreateComputePipelineState(&cd, __uuidof(ID3D12PipelineState), (void**)&psoC);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC gd = {};
        gd.pRootSignature = rsG;
        gd.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        gd.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        gd.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        gd.SampleMask = UINT_MAX;
        gd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        gd.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        gd.RasterizerState.DepthClipEnable = TRUE;
        gd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        gd.NumRenderTargets = 1;
        gd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        gd.SampleDesc.Count = 1;
        dev->CreateGraphicsPipelineState(&gd, __uuidof(ID3D12PipelineState), (void**)&psoG);
    }
    check(psoC && psoG, "compute + graphics pipelines created");

    // setup list (switches off): initial contents of A, B, U (zeros) and the texture S
    {
        Fill(upA, 1);
        Fill(upB, 2);
        ID3D12GraphicsCommandList* cl = cls[0];
        al[0]->Reset();
        cl->Reset(al[0], nullptr);
        A.state = B.state = U.state = 0;
        Tr(cl, A, kCD); cl->CopyBufferRegion(A.r, 0, upA, 0, kN); Tr(cl, A, kNPSR);
        Tr(cl, B, kCD); cl->CopyBufferRegion(B.r, 0, upB, 0, kN);
        Tr(cl, U, kCD);
        D3D12_TEXTURE_COPY_LOCATION dst = {}, src = {};
        dst.pResource = S.r; dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.pResource = upS; src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, 64, 64, 1, 256};
        cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        Tr(cl, S, kNPSR);
        cl->Close();
        ID3D12CommandList* l[] = {cl};
        q->ExecuteCommandLists(1, l);
        Wait();
    }
    // zero U with a compute pass that stores 0 (U was only moved to COPY_DEST above, never read)
    {
        ID3DBlob* z = Compile("RWByteAddressBuffer U : register(u0); [numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { U.Store(t.x * 4, 0); }\n", "cs_5_0");
        D3D12_ROOT_PARAMETER p = {};
        p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        D3D12_ROOT_SIGNATURE_DESC d = {1, &p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ID3DBlob *sb = nullptr, *eb = nullptr;
        ID3D12RootSignature* rs = nullptr;
        D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1, &sb, &eb);
        dev->CreateRootSignature(0, sb->GetBufferPointer(), sb->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&rs);
        D3D12_COMPUTE_PIPELINE_STATE_DESC cd = {};
        cd.pRootSignature = rs;
        cd.CS = {z->GetBufferPointer(), z->GetBufferSize()};
        ID3D12PipelineState* pz = nullptr;
        dev->CreateComputePipelineState(&cd, __uuidof(ID3D12PipelineState), (void**)&pz);
        ID3D12GraphicsCommandList* cl = cls[0];
        al[0]->Reset();
        cl->Reset(al[0], nullptr);
        Tr(cl, U, kUAV);
        cl->SetComputeRootSignature(rs);
        cl->SetPipelineState(pz);
        cl->SetComputeRootUnorderedAccessView(0, U.r->GetGPUVirtualAddress());
        cl->Dispatch(kN / 4 / 64, 1, 1);
        cl->Close();
        ID3D12CommandList* l[] = {cl};
        q->ExecuteCommandLists(1, l);
        Wait();
        pz->Release();
        rs->Release();
    }

    // one run = kFrames frames; the final state of A/B/U/S/RT is re-created by the next run (same pattern), so runs are comparable
    auto reset = [&]() {
        // A back to NPSR with frame-0 data, B to COPY_DEST with frame-0 data, U zeroed, S/RT as at the start
        gpuopt::SetRewrite(0, false); gpuopt::SetRewrite(1, false); gpuopt::SetRewrite(2, false);
        Fill(upA, 1);
        Fill(upB, 2);
        ID3D12GraphicsCommandList* cl = cls[0];
        al[0]->Reset();
        cl->Reset(al[0], nullptr);
        Tr(cl, A, kCD); cl->CopyBufferRegion(A.r, 0, upA, 0, kN); Tr(cl, A, kNPSR);
        Tr(cl, B, kCD); cl->CopyBufferRegion(B.r, 0, upB, 0, kN);
        cl->Close();
        ID3D12CommandList* l[] = {cl};
        q->ExecuteCommandLists(1, l);
        Wait();
    };
    ID3D12Resource* zeros = MakeBuf(kN, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
    {
        uint32_t* p = nullptr;
        zeros->Map(0, nullptr, (void**)&p);
        memset(p, 0, kN);
        zeros->Unmap(0, nullptr);
    }

    auto run = [&](bool a, bool b, bool r1) -> Out {
        reset();
        // zero U: UAV -> COPY_DEST -> copy zeros -> UAV (switches still off)
        {
            ID3D12GraphicsCommandList* cl = cls[1];
            al[1]->Reset();
            cl->Reset(al[1], nullptr);
            U.state = 0;
            Tr(cl, U, kCD); cl->CopyBufferRegion(U.r, 0, zeros, 0, kN); Tr(cl, U, kUAV);
            cl->Close();
            ID3D12CommandList* l[] = {cl};
            q->ExecuteCommandLists(1, l);
            Wait();
        }
        gpuopt::SetRewrite(0, a);
        gpuopt::SetRewrite(1, b);
        gpuopt::SetRewrite(2, r1);
        gpuopt::Stats s0;
        gpuopt::GetStats(s0);
        for (int f = 0; f < kFrames; ++f) {
            const int k = f % kRing;
            ID3D12GraphicsCommandList* cl = cls[k];
            al[k]->Reset();
            cl->Reset(al[k], nullptr);
            A.state = B.state = U.state = 0;   // list stamp changed: buffers start at COMMON (SynTransitionResource 115E55..115E67)
            Fill(upA, 100 + f);
            Fill(upB, 200 + f);
            cl->SetDescriptorHeaps(1, &heap);
            Tr(cl, A, kCD);
            cl->CopyBufferRegion(A.r, 0, upA, 0, kN);
            Tr(cl, A, kNPSR);
            Tr(cl, B, kNPSR);                  // first use: COMMON -> NPSR, previous state COPY_DEST (the 2-struct pair)
            Tr(cl, U, kUAV);                   // first use: COMMON -> UAV, previous UAV (R2b -> UAV barrier)
            cl->SetComputeRootSignature(rsC);
            cl->SetPipelineState(psoC);
            cl->SetComputeRootShaderResourceView(0, A.r->GetGPUVirtualAddress());
            cl->SetComputeRootShaderResourceView(1, B.r->GetGPUVirtualAddress());
            cl->SetComputeRootUnorderedAccessView(2, U.r->GetGPUVirtualAddress());
            cl->SetComputeRoot32BitConstant(3, (UINT)f, 0);
            cl->Dispatch(kN / 4 / 64, 1, 1);
            UavBar(cl, U.r);
            cl->SetComputeRoot32BitConstant(3, (UINT)f + 7u, 0);
            cl->Dispatch(kN / 4 / 64, 1, 1);
            Tr(cl, B, kCD);
            cl->CopyBufferRegion(B.r, 0, upB, 0, kN);       // B is left in COPY_DEST
            Tr(cl, A, kNPSR | kPSR);                         // R1: buffer superset
            Tr(cl, S, kNPSR | kPSR);                         // R1: texture superset
            Tr(cl, RT, kRT);
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            const float col[4] = {0.f, 0.f, 0.f, 1.f};
            cl->ClearRenderTargetView(rtv, col, 0, nullptr);
            cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            D3D12_VIEWPORT vp = {0, 0, 64, 64, 0, 1};
            D3D12_RECT sc = {0, 0, 64, 64};
            cl->RSSetViewports(1, &vp);
            cl->RSSetScissorRects(1, &sc);
            cl->SetGraphicsRootSignature(rsG);
            cl->SetPipelineState(psoG);
            cl->SetGraphicsRootDescriptorTable(0, heap->GetGPUDescriptorHandleForHeapStart());
            cl->SetGraphicsRootShaderResourceView(1, A.r->GetGPUVirtualAddress());
            cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cl->DrawInstanced(3, 1, 0, 0);
            Tr(cl, RT, kCS);
            D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
            src.pResource = RT.r; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.pResource = rbRT; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            dst.PlacedFootprint.Offset = (UINT64)f * 64 * 64 * 4;
            dst.PlacedFootprint.Footprint = {DXGI_FORMAT_R8G8B8A8_UNORM, 64, 64, 1, 256};
            cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            Tr(cl, RT, kRT);
            Tr(cl, A, kNPSR);                                // subset: kept by R1
            Tr(cl, S, kNPSR);
            Tr(cl, U, kCS);
            cl->CopyBufferRegion(rbU, (UINT64)f * kN, U.r, 0, kN);
            Tr(cl, U, kUAV);
            if (FAILED(cl->Close())) { check(false, "Close"); return {}; }
            ID3D12CommandList* l[] = {cl};
            q->ExecuteCommandLists(1, l);
            Wait();
        }
        Out o;
        gpuopt::Stats s1;
        gpuopt::GetStats(s1);
        o.d = s1;
        o.d.r2a -= s0.r2a; o.d.r2b -= s0.r2b; o.d.r1Dropped -= s0.r1Dropped; o.d.keptTexture -= s0.keptTexture;
        o.d.keptUav -= s0.keptUav; o.d.keptUnknown -= s0.keptUnknown; o.d.keptForeign -= s0.keptForeign;
        o.d.barriers -= s0.barriers; o.d.emitCalls -= s0.emitCalls;
        void* p = nullptr;
        D3D12_RANGE rr = {0, (SIZE_T)kN * kFrames};
        rbU->Map(0, &rr, &p);
        o.u.assign((uint8_t*)p, (uint8_t*)p + (size_t)kN * kFrames);
        rbU->Unmap(0, nullptr);
        D3D12_RANGE rr2 = {0, (SIZE_T)64 * 64 * 4 * kFrames};
        rbRT->Map(0, &rr2, &p);
        o.rt.assign((uint8_t*)p, (uint8_t*)p + (size_t)64 * 64 * 4 * kFrames);
        rbRT->Unmap(0, nullptr);
        return o;
    };

    struct Cfg { const char* name; bool a, b, r1; };
    const Cfg cfgs[] = {{"all off", 0, 0, 0}, {"all off (repeat)", 0, 0, 0}, {"R2a", 1, 0, 0}, {"R1", 0, 0, 1},
                        {"R2a + R1", 1, 0, 1}, {"R2b (experiment)", 0, 1, 0}, {"R2a + R2b + R1", 1, 1, 1}};
    std::vector<Out> outs;
    for (const Cfg& c : cfgs) {
        outs.push_back(run(c.a, c.b, c.r1));
        const Out& o = outs.back();
        printf("[%s] r2a %llu r2b %llu r1 %llu | kept tex %llu uav %llu unknown %llu foreign %llu | barriers %llu -> emits %llu\n",
               c.name, (unsigned long long)o.d.r2a, (unsigned long long)o.d.r2b, (unsigned long long)o.d.r1Dropped,
               (unsigned long long)o.d.keptTexture, (unsigned long long)o.d.keptUav, (unsigned long long)o.d.keptUnknown,
               (unsigned long long)o.d.keptForeign, (unsigned long long)o.d.barriers, (unsigned long long)o.d.emitCalls);
    }
    gpuopt::SetRewrite(0, false); gpuopt::SetRewrite(1, false); gpuopt::SetRewrite(2, false);
    bool nonTrivial = false;
    for (size_t i = 0; i < outs[0].u.size(); i += 4099) nonTrivial |= outs[0].u[i] != 0;
    check(nonTrivial, "baseline output is not all zero");
    check(outs[0].u.size() == (size_t)kN * kFrames && outs[0].rt.size() == (size_t)64 * 64 * 4 * kFrames, "readback sizes");
    check(outs[0].u == outs[1].u && outs[0].rt == outs[1].rt, "determinism: two runs with every switch off are bit-identical");
    for (size_t i = 2; i < outs.size(); ++i) {
        char m[160];
        snprintf(m, sizeof m, "%s: buffers (U every frame) and render target bit-identical to switches off", cfgs[i].name);
        check(outs[i].u == outs[0].u && outs[i].rt == outs[0].rt, m);
    }
    const uint64_t F = kFrames;
    const gpuopt::Stats& off = outs[0].d;
    check(off.r2a == 0 && off.r2b == 0 && off.r1Dropped == 0 && off.keptTexture == 0 && off.keptUav == 0 && off.keptUnknown == 0 &&
              off.keptForeign == 0, "all off: every rewrite counter stays 0");
    check(outs[2].d.r2a >= 2 * (F - 3) && outs[2].d.r2b == 0 && outs[2].d.r1Dropped == 0, "R2a: A (read P) and B (write P) rewritten every frame, nothing else");
    check(outs[2].d.keptUav >= F - 3, "R2a: the UAV buffer U is kept (counted kept_uav)");
    check(outs[2].d.barriers == off.barriers && outs[2].d.emitCalls <= off.emitCalls + 1, "R2a: same number of engine barriers, pair stays inside the batch");
    check(outs[3].d.r1Dropped == 2 * F && outs[3].d.r2a == 0, "R1: exactly the 2 supersets per frame (buffer A, texture S) dropped");
    check(outs[4].d.r2a >= 2 * (F - 3) && outs[4].d.r1Dropped == 2 * F, "R2a + R1: both fire");
    check(outs[5].d.r2b >= F - 3 && outs[5].d.r2a == 0, "R2b: the UAV buffer is rewritten every frame");
    check(outs[6].d.r2a >= 2 * (F - 3) && outs[6].d.r2b >= F - 3 && outs[6].d.r1Dropped == 2 * F, "all three on: all fire");
    check(outs[2].d.keptForeign == 0, "no 'same list / other thread' rejection with a 3-list ring");
    check(dev->GetDeviceRemovedReason() == S_OK, "device not removed");

    // trace CSV (TraceRecords = 3000 started at install): header + res_dim / res_flags columns
    bool haveCsv = false;
    for (int i = 0; i < 100 && !haveCsv; ++i) {
        DWORD a = GetFileAttributesW(csv);
        haveCsv = a != INVALID_FILE_ATTRIBUTES;
        if (!haveCsv) Sleep(100);
    }
    Sleep(300);
    check(haveCsv, "trace CSV written");
    if (haveCsv) {
        FILE* f = nullptr;
        _wfopen_s(&f, csv, L"rb");
        std::string txt;
        char buf[65536];
        size_t n;
        while (f && (n = fread(buf, 1, sizeof buf, f)) > 0) txt.append(buf, n);
        if (f) fclose(f);
        check(txt.rfind("seq,us,tid,list,set,slot,method,action,caller_rva,a1,a2,a3,caller2_rva,res_dim,res_flags", 0) == 0,
              "trace header names caller2_rva, res_dim, res_flags");
        // row = ...,<caller2_rva>,<dim>,<flags>\n
        check(txt.find(",1,0x4\r\n") != std::string::npos || txt.find(",1,0x4\n") != std::string::npos, "trace: UAV buffer row (dim 1, flags 0x4)");
        check(txt.find(",1,0x0\r\n") != std::string::npos || txt.find(",1,0x0\n") != std::string::npos, "trace: plain buffer row (dim 1, flags 0x0)");
        check(txt.find(",3,0x1\r\n") != std::string::npos || txt.find(",3,0x1\n") != std::string::npos, "trace: render-target texture row (dim 3, flags 0x1)");
        check(txt.find(",3,0x0\r\n") != std::string::npos || txt.find(",3,0x0\n") != std::string::npos, "trace: plain texture row (dim 3, flags 0x0)");
    }
    char st[512];
    gpuopt::StatusLine(st, sizeof st);
    printf("stats/s gpuopt: %s\n", st);
    check(strstr(st, "R2a") && strstr(st, "R1"), "stats line carries the R2a / R2b / R1 counters");
    printf(g_fails ? "FAILED (%d)\n" : "ALL PASSED\n", g_fails);
    return g_fails;
}
