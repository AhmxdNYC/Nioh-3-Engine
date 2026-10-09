// async_overlap_harness.cpp - what moving a latency-bound compute chain from the graphics queue to a COMPUTE queue does on
// THIS PC's real D3D12 device, with the KTGL async-slot fence pattern (notes\gpu-async-wins.md). Not a game test.
// Built and run by tests/run_async_overlap_harness.py.
//
// X = an "effect chain" like r5 GFX segments 70-80: K dependent Dispatch(1,1,1) of 64 threads with a UAV barrier between
//     each (state carried in a 256-byte UAV buffer, so step i needs step i-1).
// W = graphics work for X to hide under: full-screen draws (2560x1440 RGBA8, ALU loop per pixel, ~3 ms).
// Modes (one GFX-queue span each, measured with timestamps on the DIRECT queue; X's result and the render target read back):
//   SERIAL  = the stock path: X then W on the DIRECT queue.
//   ASYNC   = the engine's async-slot pattern: DIRECT signals (KtglCtxGetFence at the producer), the COMPUTE queue waits on it,
//             runs X, signals; DIRECT runs W, then waits on the compute fence before the consumer (AsyncSlotAddFence ->
//             GfxWaitAsyncFence), then the consumer list.
//   NOWINDOW= the same queues but the consumer wait right after the producer (no graphics work in between): what a
//             CPU-bound frame or a join placed at the producer gives (why AS0 read "neutral" in the 1.7.16 CPU-bound run).
// Checks: X's 64 words are bit-identical in all modes and equal a CPU model of the chain; the render target hash is
// identical in all modes. Report: median GFX span per mode over interleaved iterations, X alone, W alone, the saving.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static ID3D12Device* dev = nullptr;
static ID3D12CommandQueue *qG = nullptr, *qC = nullptr;
static ID3D12Fence *fG = nullptr, *fC = nullptr, *fDone = nullptr;
static UINT64 vG = 0, vC = 0, vDone = 0;
static HANDLE ev = nullptr;
static int g_fail = 0;
static void check(bool ok, const char* what) {
    printf("%s %s\n", ok ? "  ok  " : "  FAIL", what);
    if (!ok) ++g_fail;
}

static ID3DBlob* Compile(const char* src, const char* target) {
    ID3DBlob *b = nullptr, *e = nullptr;
    if (FAILED(D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, "main", target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &b, &e))) {
        printf("shader error: %s\n", e ? (const char*)e->GetBufferPointer() : "?");
        return nullptr;
    }
    return b;
}
static ID3D12Resource* Buf(UINT64 n, D3D12_HEAP_TYPE h, D3D12_RESOURCE_FLAGS f, D3D12_RESOURCE_STATES s) {
    D3D12_HEAP_PROPERTIES hp = {h};
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = n;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d.Flags = f;
    ID3D12Resource* r = nullptr;
    dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, s, nullptr, __uuidof(ID3D12Resource), (void**)&r);
    return r;
}
static void WaitDone(ID3D12CommandQueue* q) {
    q->Signal(fDone, ++vDone);
    fDone->SetEventOnCompletion(vDone, ev);
    WaitForSingleObject(ev, 20000);
}
static void Uav(ID3D12GraphicsCommandList* cl, ID3D12Resource* r) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    b.UAV.pResource = r;
    cl->ResourceBarrier(1, &b);
}
static void Trans(ID3D12GraphicsCommandList* cl, ID3D12Resource* r, D3D12_RESOURCE_STATES a, D3D12_RESOURCE_STATES b2) {
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = r;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    b.Transition.StateBefore = a;
    b.Transition.StateAfter = b2;
    cl->ResourceBarrier(1, &b);
}

// X: one chain step. Reads its own and its neighbour's word (all reads before the group barrier), mixes, writes.
static const char* kXs =
    "RWByteAddressBuffer U : register(u0); cbuffer C : register(b0) { uint step; };\n"
    "[numthreads(64,1,1)] void main(uint t : SV_GroupThreadID) {\n"
    "  uint a = U.Load(t * 4); uint b = U.Load(((t + 1) & 63) * 4);\n"
    "  uint v = a * 1664525u + b + step; v ^= v >> 13; v *= 0x5bd1e995u; v ^= v >> 15;\n"
    "  [loop] for (uint i = 0; i < 48; i++) { v = v * 1103515245u + 12345u + i; v ^= v >> 7; }\n"
    "  GroupMemoryBarrierWithGroupSync();\n"
    "  U.Store(t * 4, v); }\n";
static void XModel(uint32_t* u, int k) {
    for (int s = 0; s < k; ++s) {
        uint32_t n[64];
        for (int t = 0; t < 64; ++t) {
            uint32_t a = u[t], b = u[(t + 1) & 63];
            uint32_t v = a * 1664525u + b + (uint32_t)s;
            v ^= v >> 13; v *= 0x5bd1e995u; v ^= v >> 15;
            for (uint32_t i = 0; i < 48; ++i) { v = v * 1103515245u + 12345u + i; v ^= v >> 7; }
            n[t] = v;
        }
        memcpy(u, n, sizeof n);
    }
}
static const char* kVs = "float4 main(uint id : SV_VertexID) : SV_Position { float2 p = float2((id << 1) & 2, id & 2);\n"
                         "  return float4(p * float2(2, -2) + float2(-1, 1), 0, 1); }\n";
static const char* kPs = "cbuffer C : register(b0) { uint iters; };\n"
                         "float4 main(float4 p : SV_Position) : SV_Target { uint h = uint(p.x) * 73856093u ^ uint(p.y) * 19349663u;\n"
                         "  [loop] for (uint i = 0; i < iters; i++) { h = h * 1664525u + 1013904223u; h ^= h >> 11; }\n"
                         "  return float4((h & 255) / 255.0, ((h >> 8) & 255) / 255.0, ((h >> 16) & 255) / 255.0, 1); }\n";

int main(int argc, char** argv) {
    const int K = argc > 1 ? atoi(argv[1]) : 170;     // chain length (r5 segments 73-76: 168 / 190 / 190 dispatches)
    const int iters = argc > 2 ? atoi(argv[2]) : 60;  // interleaved iterations per mode
    printf("=== async overlap on a real D3D12 device: chain of %d x Dispatch(1,1,1) vs full-screen graphics ===\n", K);
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev))) return 2;
    {
        IDXGIFactory4* f = nullptr;
        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)&f))) {
            IDXGIAdapter1* a = nullptr;
            DXGI_ADAPTER_DESC1 ad = {};
            if (SUCCEEDED(f->EnumAdapterByLuid(dev->GetAdapterLuid(), __uuidof(IDXGIAdapter1), (void**)&a)) && SUCCEEDED(a->GetDesc1(&ad)))
                printf("adapter: %ls (vendor %04X)\n", ad.Description, ad.VendorId);
            if (a) a->Release();
            f->Release();
        }
    }
    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void**)&qG);
    qd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void**)&qC);
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fG);
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fC);
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fDone);
    ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    UINT64 tsFreq = 0;
    qG->GetTimestampFrequency(&tsFreq);
    UINT64 tsFreqC = 0;
    qC->GetTimestampFrequency(&tsFreqC);

    // root signatures + PSOs
    ID3D12RootSignature *rsC = nullptr, *rsG = nullptr;
    {
        D3D12_ROOT_PARAMETER p[2] = {};
        p[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; p[0].Descriptor = {0, 0};
        p[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; p[1].Constants = {0, 0, 1};
        D3D12_ROOT_SIGNATURE_DESC d = {2, p, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ID3DBlob *sb = nullptr, *eb = nullptr;
        D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1, &sb, &eb);
        dev->CreateRootSignature(0, sb->GetBufferPointer(), sb->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&rsC);
        D3D12_ROOT_PARAMETER g[1] = {};
        g[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; g[0].Constants = {0, 0, 1};
        g[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        D3D12_ROOT_SIGNATURE_DESC gd = {1, g, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT};
        D3D12SerializeRootSignature(&gd, D3D_ROOT_SIGNATURE_VERSION_1, &sb, &eb);
        dev->CreateRootSignature(0, sb->GetBufferPointer(), sb->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&rsG);
    }
    ID3DBlob *xs = Compile(kXs, "cs_5_0"), *vs = Compile(kVs, "vs_5_0"), *ps = Compile(kPs, "ps_5_0");
    if (!xs || !vs || !ps || !rsC || !rsG) return 2;
    ID3D12PipelineState *psoX = nullptr, *psoW = nullptr;
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC d = {};
        d.pRootSignature = rsC;
        d.CS = {xs->GetBufferPointer(), xs->GetBufferSize()};
        dev->CreateComputePipelineState(&d, __uuidof(ID3D12PipelineState), (void**)&psoX);
        D3D12_GRAPHICS_PIPELINE_STATE_DESC g = {};
        g.pRootSignature = rsG;
        g.VS = {vs->GetBufferPointer(), vs->GetBufferSize()};
        g.PS = {ps->GetBufferPointer(), ps->GetBufferSize()};
        g.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        g.SampleMask = 0xFFFFFFFFu;
        g.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        g.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        g.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        g.NumRenderTargets = 1;
        g.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        g.SampleDesc.Count = 1;
        dev->CreateGraphicsPipelineState(&g, __uuidof(ID3D12PipelineState), (void**)&psoW);
    }
    if (!psoX || !psoW) return 2;

    // resources
    const UINT kW = 2560, kH = 1440;
    ID3D12Resource* U = Buf(256, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
    ID3D12Resource* U0 = Buf(256, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ);
    ID3D12Resource* rbU = Buf(256, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
    uint32_t init[64];
    for (int i = 0; i < 64; ++i) init[i] = 0x9E3779B9u * (uint32_t)(i + 1);
    {
        void* p = nullptr;
        U0->Map(0, nullptr, &p);
        memcpy(p, init, 256);
        U0->Unmap(0, nullptr);
    }
    ID3D12Resource* RT = nullptr;
    {
        D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
        D3D12_RESOURCE_DESC d = {};
        d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        d.Width = kW;
        d.Height = kH;
        d.DepthOrArraySize = 1;
        d.MipLevels = 1;
        d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, __uuidof(ID3D12Resource), (void**)&RT);
    }
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp = {};
    UINT64 rtBytes = 0;
    {
        D3D12_RESOURCE_DESC d = RT->GetDesc();
        dev->GetCopyableFootprints(&d, 0, 1, 0, &fp, nullptr, nullptr, &rtBytes);
    }
    ID3D12Resource* rbRT = Buf(rtBytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    {
        D3D12_DESCRIPTOR_HEAP_DESC hd = {D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
        dev->CreateDescriptorHeap(&hd, __uuidof(ID3D12DescriptorHeap), (void**)&rtvHeap);
        dev->CreateRenderTargetView(RT, nullptr, rtvHeap->GetCPUDescriptorHandleForHeapStart());
    }
    ID3D12QueryHeap* qh = nullptr;
    {
        D3D12_QUERY_HEAP_DESC d = {D3D12_QUERY_HEAP_TYPE_TIMESTAMP, 2, 0};
        dev->CreateQueryHeap(&d, __uuidof(ID3D12QueryHeap), (void**)&qh);
    }
    ID3D12Resource* rbTs = Buf(16, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST);
    if (!U || !U0 || !rbU || !RT || !rbRT || !rtvHeap || !qh || !rbTs) return 2;

    // command lists (recorded once, executed many times)
    auto NewList = [](D3D12_COMMAND_LIST_TYPE t, ID3D12CommandAllocator** al) {
        ID3D12GraphicsCommandList* cl = nullptr;
        dev->CreateCommandAllocator(t, __uuidof(ID3D12CommandAllocator), (void**)al);
        dev->CreateCommandList(0, t, *al, nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&cl);
        return cl;
    };
    std::vector<ID3D12CommandAllocator*> als;
    auto L = [&](D3D12_COMMAND_LIST_TYPE t) { ID3D12CommandAllocator* a = nullptr; auto* cl = NewList(t, &a); als.push_back(a); return cl; };
    // reset: U <- init (DIRECT; U decays to COMMON at the end of every ExecuteCommandLists)
    auto* clReset = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
    clReset->CopyBufferRegion(U, 0, U0, 0, 256);
    clReset->Close();
    auto RecordX = [&](ID3D12GraphicsCommandList* cl) {
        cl->SetComputeRootSignature(rsC);
        cl->SetPipelineState(psoX);
        cl->SetComputeRootUnorderedAccessView(0, U->GetGPUVirtualAddress());
        for (int s = 0; s < K; ++s) {
            cl->SetComputeRoot32BitConstant(1, (UINT)s, 0);
            cl->Dispatch(1, 1, 1);
            Uav(cl, U);
        }
    };
    UINT wIters = 64;
    const int kDraws = 4;
    auto RecordW = [&](ID3D12GraphicsCommandList* cl) {
        D3D12_VIEWPORT vp = {0, 0, (float)kW, (float)kH, 0, 1};
        D3D12_RECT sc = {0, 0, (LONG)kW, (LONG)kH};
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cl->RSSetViewports(1, &vp);
        cl->RSSetScissorRects(1, &sc);
        cl->SetGraphicsRootSignature(rsG);
        cl->SetPipelineState(psoW);
        cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cl->SetGraphicsRoot32BitConstant(0, wIters, 0);
        for (int i = 0; i < kDraws; ++i) cl->DrawInstanced(3, 1, 0, 0);
    };
    auto TsBegin = [&](ID3D12GraphicsCommandList* cl) { cl->EndQuery(qh, D3D12_QUERY_TYPE_TIMESTAMP, 0); };
    auto TsEnd = [&](ID3D12GraphicsCommandList* cl) {
        cl->EndQuery(qh, D3D12_QUERY_TYPE_TIMESTAMP, 1);
        cl->ResolveQueryData(qh, D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, rbTs, 0);
    };
    auto ReadTs = [&](UINT64 freq) {
        uint64_t* p = nullptr;
        D3D12_RANGE r = {0, 16};
        rbTs->Map(0, &r, (void**)&p);
        const double ms = (double)(p[1] - p[0]) * 1000.0 / (double)freq;
        rbTs->Unmap(0, nullptr);
        return ms;
    };
    // the consumer: read X's result and the render target back (also part of every mode, identical)
    auto RecordConsumer = [&](ID3D12GraphicsCommandList* cl) {
        cl->CopyBufferRegion(rbU, 0, U, 0, 256);
        Trans(cl, RT, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION dst = {rbRT, D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT};
        dst.PlacedFootprint = fp;
        D3D12_TEXTURE_COPY_LOCATION src = {RT, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
        src.SubresourceIndex = 0;
        cl->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        Trans(cl, RT, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
    };

    ID3D12GraphicsCommandList *sSerial = nullptr, *aPre = nullptr, *aX = nullptr, *aW = nullptr, *aPost = nullptr, *xOnly = nullptr, *wOnly = nullptr, *xComp = nullptr, *xPlain = nullptr,
                              *clRead = nullptr;
    auto RecordAll = [&]() {
        for (auto* a : als) a->Release();
        als.clear();
        clReset = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        clReset->CopyBufferRegion(U, 0, U0, 0, 256);
        clReset->Close();
        sSerial = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        TsBegin(sSerial); RecordX(sSerial); RecordW(sSerial); TsEnd(sSerial); sSerial->Close();
        aPre = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        TsBegin(aPre); aPre->Close();
        aX = L(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        RecordX(aX); aX->Close();
        aW = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        RecordW(aW); aW->Close();
        aPost = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        TsEnd(aPost); aPost->Close();  // the consumer side of the wait (the readback runs after the timed span)
        clRead = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        RecordConsumer(clRead); clRead->Close();
        xOnly = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        TsBegin(xOnly); RecordX(xOnly); TsEnd(xOnly); xOnly->Close();
        wOnly = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        TsBegin(wOnly); RecordW(wOnly); TsEnd(wOnly); wOnly->Close();
        xPlain = L(D3D12_COMMAND_LIST_TYPE_DIRECT);
        RecordX(xPlain); xPlain->Close();
        xComp = L(D3D12_COMMAND_LIST_TYPE_COMPUTE);
        TsBegin(xComp); RecordX(xComp); TsEnd(xComp); xComp->Close();
    };
    auto Exec = [](ID3D12CommandQueue* q, ID3D12GraphicsCommandList* cl) {
        ID3D12CommandList* l[] = {cl};
        q->ExecuteCommandLists(1, l);
    };
    auto Reset = [&]() { Exec(qG, clReset); WaitDone(qG); };
    // calibrate W to ~3 ms (the host window: r5 fab20-fab26 graphics, here simply a busy graphics queue)
    for (int c = 0; c < 12; ++c) {
        RecordAll();
        Exec(qG, wOnly);
        WaitDone(qG);
        Exec(qG, wOnly);
        WaitDone(qG);
        const double ms = ReadTs(tsFreq);
        if (ms >= 2.5 || wIters >= (1u << 20)) break;
        wIters = (UINT)(wIters * std::max(1.3, std::min(8.0, 3.0 / std::max(ms, 0.05))));
    }
    RecordAll();

    uint32_t model[64];
    memcpy(model, init, sizeof model);
    XModel(model, K);
    auto ReadU = [&](uint32_t* out) {
        void* p = nullptr;
        D3D12_RANGE r = {0, 256};
        rbU->Map(0, &r, &p);
        memcpy(out, p, 256);
        rbU->Unmap(0, nullptr);
    };
    auto HashRT = [&]() {
        uint8_t* p = nullptr;
        D3D12_RANGE r = {0, (SIZE_T)rtBytes};
        rbRT->Map(0, &r, (void**)&p);
        uint64_t h = 1469598103934665603ull;
        for (UINT y = 0; y < kH; ++y) {
            const uint8_t* row = p + fp.Offset + (UINT64)y * fp.Footprint.RowPitch;
            for (UINT x = 0; x < kW * 4; ++x) h = (h ^ row[x]) * 1099511628211ull;
        }
        rbRT->Unmap(0, nullptr);
        return h;
    };

    enum { SERIAL, ASYNC, NOWINDOW, XONLY, WONLY, XCOMP, SPLIT, NM };
    static const char* kName[NM] = {"SERIAL (stock: X then W on DIRECT)", "ASYNC (engine slot pattern, consumer wait after W)",
                                    "NOWINDOW (consumer wait right after the producer)", "X alone (DIRECT)", "W alone (DIRECT)", "X alone (COMPUTE queue)",
                                    "SPLIT (X then W on DIRECT, same 4 ExecuteCommandLists as ASYNC)"};
    std::vector<double> t[NM];
    uint32_t got[NM][64] = {};
    uint64_t rth[NM] = {};
    auto RunMode = [&](int m) {
        Reset();
        if (m == SERIAL) {
            Exec(qG, sSerial);
        } else if (m == ASYNC) {
            Exec(qG, aPre);
            qG->Signal(fG, ++vG);   // KtglCtxGetFence at the producer: graphics work so far
            qC->Wait(fG, vG);       // the async context waits on it (3D2724)
            Exec(qC, aX);
            qC->Signal(fC, ++vC);   // KtglCtxGetFence on the async context -> AsyncSlotAddFence
            Exec(qG, aW);           // the graphics work between producer and consumer
            qG->Wait(fC, vC);       // GfxWaitAsyncFence at the consumer
            Exec(qG, aPost);
        } else if (m == NOWINDOW) {
            Exec(qG, aPre);
            qG->Signal(fG, ++vG);
            qC->Wait(fG, vG);
            Exec(qC, aX);
            qC->Signal(fC, ++vC);
            qG->Wait(fC, vC);       // consumer wait placed right after the producer
            Exec(qG, aW);
            Exec(qG, aPost);
        } else if (m == XONLY) {
            Exec(qG, xOnly);
        } else if (m == SPLIT) {
            Exec(qG, aPre);
            qG->Signal(fG, ++vG);
            Exec(qG, xPlain);
            Exec(qG, aW);
            Exec(qG, aPost);
        } else if (m == WONLY) {
            Exec(qG, wOnly);
        } else {
            Exec(qC, xComp);
        }
        WaitDone(qG);
        WaitDone(qC);
        t[m].push_back(ReadTs(m == XCOMP ? tsFreqC : tsFreq));
        if (m <= NOWINDOW) {
            Exec(qG, clRead);  // U decayed to COMMON at the end of the timed ExecuteCommandLists: the copy promotes it
            WaitDone(qG);
            ReadU(got[m]);
            rth[m] = HashRT();
        }
    };
    for (int w = 0; w < 3; ++w)
        for (int m = 0; m < NM; ++m) RunMode(m);  // warm-up
    for (auto& v : t) v.clear();
    for (int i = 0; i < iters; ++i)
        for (int m = 0; m < NM; ++m) RunMode((m + i) % NM);  // rotate the order: drift cancels
    auto Med = [](std::vector<double> v) { std::sort(v.begin(), v.end()); return v.empty() ? 0.0 : v[v.size() / 2]; };
    double med[NM];
    printf("W = %d draws x %ux%u, %u ALU iterations per pixel; %d iterations per mode (median GFX-queue span, ms):\n", kDraws, kW, kH, wIters, iters);
    for (int m = 0; m < NM; ++m) {
        med[m] = Med(t[m]);
        printf("  %-55s %7.3f\n", kName[m], med[m]);
    }
    const double save = med[SERIAL] - med[ASYNC];
    printf("list splits alone (SPLIT - SERIAL) %+.3f ms; cross-queue sync (NOWINDOW - SPLIT) %+.3f ms\n", med[SPLIT] - med[SERIAL],
           med[NOWINDOW] - med[SPLIT]);
    printf("X alone %.3f ms; saving SERIAL -> ASYNC %.3f ms (%.0f %% of X); NOWINDOW - SERIAL %+.3f ms\n", med[XONLY], save,
           med[XONLY] > 0 ? 100.0 * save / med[XONLY] : 0.0, med[NOWINDOW] - med[SERIAL]);
    check(memcmp(got[SERIAL], model, 256) == 0, "X result (SERIAL) == CPU model of the chain");
    check(memcmp(got[ASYNC], got[SERIAL], 256) == 0 && memcmp(got[NOWINDOW], got[SERIAL], 256) == 0,
          "X result bit-identical: SERIAL == ASYNC == NOWINDOW");
    check(rth[SERIAL] && rth[ASYNC] == rth[SERIAL] && rth[NOWINDOW] == rth[SERIAL], "render target bit-identical in all three modes");
    check(med[XONLY] > 0.0 && med[WONLY] > 0.0, "timestamps valid");
    if (g_fail) {
        printf("FAILED (%d)\n", g_fail);
        return 1;
    }
    printf("ALL PASSED\n");
    return 0;
}
