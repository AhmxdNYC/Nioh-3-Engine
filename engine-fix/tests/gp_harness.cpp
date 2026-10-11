// gp_harness.cpp - GP (GPU pass profiler) on a real D3D12 device of this PC, no game: the real gpuopt.cpp +
// gpuopt_thunks.asm + gpuprof.cpp hook the runtime's command-list vtable, a fake engine block provides the two marker
// bytes, and a render loop records nested BeginEvent/EndEvent around known GPU work (UAV clears of different sizes)
// with 2-3 frames in flight. Checks: hooks installed, markers enabled, every frame's queries resolved, names
// aggregated with inclusive >= self, nested self = inclusive - children, larger work -> more GPU time, no device
// removal, the D3D12 debug layer (when installed) reports no errors.  Build + run: py -I tests/run_gp_harness.py
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/gpuopt.h"
#include "../src/gpuprof.h"
#include "../src/memstats.h"
#include "../src/psoswap.h"

namespace memstats {  // VS bind census is outside this GPU profiler test (same stubs as hot_harness).
bool BindsWanted() { return false; }
void OnBind(int, uintptr_t, uintptr_t, uintptr_t, uint16_t, uint32_t) {}
}  // namespace memstats

static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("%s\n", b);
    g_lines.push_back(b);
}

int main(int argc, char** argv) {
    // 1 = marker profiler, 2 = GP2 (every frame, every PSO run), 3 = GP2 sampled 1 in 4, min run 2,
    // 4 = GP2 + GQ gap census, sampled 1 in 2: a second list per frame is submitted after the GPU went idle + 3 ms
    // 5 = GP2 + MLP meshlet probe + MpcCheck (1.7.47): a fake "MeshletPreculling" region (K23, 2x K28, K24, K26, K27 compute PSOs whose
    //     digests are mapped to MPC kernels through [Profiler] MpcCheckKeys); K28 is a no-op except in "bad" frames (f % 16 >= 8),
    //     where it changes one dword -> MpcCheck must count equal AND mismatching chains, #26 (rewrites K23's value) always equal
    const int mode = argc > 1 ? atoi(argv[1]) : 1;
    const bool ctl = argc > 2 && !strcmp(argv[2], "ctl");   // mode 5 control run: the same region, MLP and MpcCheck off (probe overhead)
    const bool light = argc > 2 && !strcmp(argv[2], "light");   // mode 5 with MeshletProbe_Barriers = 0 (dispatch stamps only)
    printf("=== GPUProf = %d ===\n", mode);
    int fails = 0;
    auto check = [&](bool c, const char* m) { printf("%s %s\n", c ? "OK  " : "FAIL", m); fails += !c; };
    // fake engine image: g = [base+45BA588] -> block with the record (+2A62h) and replay (+552h) bytes
    uint8_t* base = (uint8_t*)VirtualAlloc(nullptr, 0x4C00000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t* g = (uint8_t*)VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    *(uint8_t**)(base + 0x45BA588) = g;
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"gp_harness.ini");
    WritePrivateProfileStringW(L"Profiler", L"GPUProf", mode >= 2 ? L"2" : L"1", ini);
    WritePrivateProfileStringW(L"ShaderSwap", L"Enabled", L"0", ini);   // GP2 alone: psoswap only maps PSOs to shader hashes
    WritePrivateProfileStringW(L"Profiler", L"GPUProf2_AB", L"0", ini);
    WritePrivateProfileStringW(L"Profiler", L"GPUProf2_SampleEvery", mode == 3 ? L"4" : mode == 4 ? L"2" : L"1", ini);
    WritePrivateProfileStringW(L"Profiler", L"GQ", mode == 4 ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"Profiler", L"GPUProf2_MinRun", mode == 3 ? L"2" : L"1", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"Barriers", L"0", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"InstallDelaySec", L"600", ini);
    WritePrivateProfileStringW(L"Profiler", L"MeshletProbe", mode == 5 && !ctl ? L"1" : L"0", ini);
    WritePrivateProfileStringW(L"Profiler", L"MpcCheck", mode == 5 && !ctl ? L"100000" : L"0", ini);
    WritePrivateProfileStringW(L"Profiler", L"MpcCheckKeys", L"", ini);
    WritePrivateProfileStringW(L"Profiler", L"MeshletProbe_Barriers", light ? L"0" : L"1", ini);
    if (mode == 5) WritePrivateProfileStringW(L"Profiler", L"GPUProf2_SampleEvery", L"2", ini);
    // mode 5: the fake MPC kernels (compiled before gp::Start: MpcCheck reads their digests from MpcCheckKeys)
    const char* const kMpcSrc[6] = {
        "RWByteAddressBuffer u : register(u0);\n[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { u.Store(t.x * 4, t.x + 1000u); }\n",
        "RWByteAddressBuffer u : register(u0);\n[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { if (u.Load(0) == 0xFFFFFFFFu) u.Store(4, 7u); }\n",
        "RWByteAddressBuffer u : register(u0);\n[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { if (t.x == 0) u.Store(32, u.Load(32) + 1u); }\n",
        "RWByteAddressBuffer u : register(u0);\n[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { u.Store(4096 + t.x * 4, t.x); }\n",
        "RWByteAddressBuffer u : register(u0);\n[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { if (t.x == 0) u.Store(4, 1001u); }\n",
        "RWByteAddressBuffer u : register(u0);\n[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) { u.Store(8192 + t.x * 4, t.x * 3u); }\n"};
    ID3DBlob* mpcCs[6] = {};
    if (mode == 5) {
        wchar_t keys[256] = L"";
        const int kk[6] = {23, 28, 28, 24, 26, 27};
        for (int i = 0; i < 6; ++i) {
            ID3DBlob* er = nullptr;
            D3DCompile(kMpcSrc[i], strlen(kMpcSrc[i]), nullptr, nullptr, nullptr, "main", "cs_5_0", 0, 0, &mpcCs[i], &er);
            if (!mpcCs[i]) return 3;
            const uint8_t* d = (const uint8_t*)mpcCs[i]->GetBufferPointer();
            const uint32_t key = (uint32_t)d[4] << 24 | (uint32_t)d[5] << 16 | (uint32_t)d[6] << 8 | d[7];
            wchar_t e[32];
            swprintf_s(e, L"%s%d=%08x", i ? L"," : L"", kk[i], key);
            wcscat_s(keys, e);
        }
        WritePrivateProfileStringW(L"Profiler", L"MpcCheckKeys", keys, ini);
    }

    ID3D12Debug* dbg = nullptr;
    const bool debug = SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&dbg)) && dbg;
    if (debug) dbg->EnableDebugLayer();
    printf("D3D12 debug layer: %s\n", debug ? "on" : "not installed");

    if (mode >= 2) {
        psoswap::StartInfo ps;
        ps.log = &Log;
        ps.iniPath = ini;
        psoswap::Start(ps);   // IAT hook on this exe's D3D12CreateDevice
    }
    gp::StartInfo si;
    si.log = &Log;
    si.iniPath = ini;
    si.base = base;
    si.codeOk = true;
    gp::Start(si);
    gpuopt::StartInfo gi;
    gi.log = &Log;
    gi.iniPath = ini;
    gi.gpuProf = gp::WantsHooks();
    gpuopt::Start(gi);
    check(gpuopt::InstallNow(), "gpuopt hooks installed for GP (Barriers = 0)");
    Sleep(400);
    check(g[0x552] == 1 && g[0x2A62] == 1, "marker bytes set (replay g+552h, record g+2A62h)");

    ID3D12Device* dev = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev))) return 2;
    check(mode < 2 || psoswap::Tracking(), "psoswap PSO tracking on");
    ID3D12InfoQueue* iq = nullptr;
    if (debug) dev->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&iq);
    D3D12_COMMAND_QUEUE_DESC qd = {};
    ID3D12CommandQueue* q = nullptr;
    dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void**)&q);
    const int kInFlight = 3;
    ID3D12CommandAllocator* al[kInFlight];
    for (auto& a : al) dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&a);
    ID3D12GraphicsCommandList* cl = nullptr;
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, al[0], nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&cl);
    cl->Close();
    ID3D12Fence* fence = nullptr;
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&fence);
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // UAV buffer + descriptor heaps for ClearUnorderedAccessViewUint (GPU work of known relative size)
    const UINT64 kBig = 64ull << 20;
    D3D12_HEAP_PROPERTIES hp = {D3D12_HEAP_TYPE_DEFAULT};
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = kBig;
    rd.Height = 1; rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    ID3D12Resource* buf = nullptr;
    dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
                                 __uuidof(ID3D12Resource), (void**)&buf);
    D3D12_DESCRIPTOR_HEAP_DESC hdGpu = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE};
    D3D12_DESCRIPTOR_HEAP_DESC hdCpu = {D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2, D3D12_DESCRIPTOR_HEAP_FLAG_NONE};
    ID3D12DescriptorHeap *hGpu = nullptr, *hCpu = nullptr;
    dev->CreateDescriptorHeap(&hdGpu, __uuidof(ID3D12DescriptorHeap), (void**)&hGpu);
    dev->CreateDescriptorHeap(&hdCpu, __uuidof(ID3D12DescriptorHeap), (void**)&hCpu);
    const UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto mkUav = [&](UINT i, UINT elems) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC u = {};
        u.Format = DXGI_FORMAT_R32_UINT;
        u.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        u.Buffer.NumElements = elems;
        D3D12_CPU_DESCRIPTOR_HANDLE c = hCpu->GetCPUDescriptorHandleForHeapStart();
        c.ptr += (SIZE_T)i * inc;
        D3D12_CPU_DESCRIPTOR_HANDLE gc = hGpu->GetCPUDescriptorHandleForHeapStart();
        gc.ptr += (SIZE_T)i * inc;
        dev->CreateUnorderedAccessView(buf, nullptr, &u, c);
        dev->CreateUnorderedAccessView(buf, nullptr, &u, gc);
    };
    mkUav(0, (UINT)(kBig / 4));        // "Big pass": 64 MB
    mkUav(1, (UINT)(kBig / 4 / 16));   // "Cascade 0": 4 MB (nested)
    auto clear = [&](UINT i) {
        D3D12_GPU_DESCRIPTOR_HANDLE gg = hGpu->GetGPUDescriptorHandleForHeapStart();
        gg.ptr += (UINT64)i * inc;
        D3D12_CPU_DESCRIPTOR_HANDLE c = hCpu->GetCPUDescriptorHandleForHeapStart();
        c.ptr += (SIZE_T)i * inc;
        const UINT v[4] = {i, i, i, i};
        cl->ClearUnorderedAccessViewUint(gg, c, buf, v, 0, nullptr);
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.UAV.pResource = buf;
        cl->ResourceBarrier(1, &b);
    };
    // GP2: two compute PSOs, one ~20x heavier (a dependent integer loop), a root UAV on the buffer
    ID3D12RootSignature* rs = nullptr;
    ID3D12PipelineState *psoHeavy = nullptr, *psoLight = nullptr;
    if (mode >= 2) {
        D3D12_ROOT_PARAMETER rp = {};
        rp.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        D3D12_ROOT_SIGNATURE_DESC rsd = {1, &rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE};
        ID3DBlob *sb = nullptr, *eb = nullptr;
        D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &sb, &eb);
        dev->CreateRootSignature(0, sb->GetBufferPointer(), sb->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&rs);
        auto mk = [&](int n) {
            char src[512];
            snprintf(src, sizeof src,
                     "RWByteAddressBuffer u : register(u0);\n[numthreads(64,1,1)] void main(uint3 t : SV_DispatchThreadID) {\n"
                     "uint a = t.x; [loop] for (uint i = 0; i < %d; ++i) a = a * 1664525u + 1013904223u + (a >> 3);\n"
                     "u.Store((t.x & 0xFFFFu) * 4, a); }\n", n);
            ID3DBlob *cs = nullptr, *er = nullptr;
            D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, "main", "cs_5_0", 0, 0, &cs, &er);
            ID3D12PipelineState* p = nullptr;
            if (cs) {
                D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
                pd.pRootSignature = rs;
                pd.CS = {cs->GetBufferPointer(), cs->GetBufferSize()};
                dev->CreateComputePipelineState(&pd, __uuidof(ID3D12PipelineState), (void**)&p);
            }
            return p;
        };
        psoHeavy = mk(3000);
        psoLight = mk(150);
        check(psoHeavy && psoLight, "two compute PSOs created");
    }
    auto ev16 = [&](const wchar_t* s) { cl->BeginEvent(0xFF336699u, s, (UINT)(wcslen(s) * 2)); };
    ID3D12PipelineState* mpcPso[6] = {};
    ID3D12Resource* bufM = nullptr;
    if (mode == 5 && rs) {
        for (int i = 0; i < 6; ++i) {
            D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
            pd.pRootSignature = rs;
            pd.CS = {mpcCs[i]->GetBufferPointer(), mpcCs[i]->GetBufferSize()};
            dev->CreateComputePipelineState(&pd, __uuidof(ID3D12PipelineState), (void**)&mpcPso[i]);
        }
        D3D12_RESOURCE_DESC rm = rd;
        rm.Width = 64 * 1024;
        dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rm, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, __uuidof(ID3D12Resource),
                                     (void**)&bufM);
        check(mpcPso[0] && mpcPso[1] && mpcPso[2] && mpcPso[3] && mpcPso[4] && mpcPso[5] && bufM, "MLP: six fake MPC PSOs + 64 KB buffer");
    }
    // GQ (mode 4): list C, recorded and submitted after list A finished on the GPU and 3 ms of CPU sleep -> a >= 3 ms queue gap
    ID3D12CommandAllocator* alC[kInFlight] = {};
    ID3D12GraphicsCommandList* clC = nullptr;
    if (mode == 4) {
        for (auto& a : alC) dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&a);
        dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alC[0], nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&clC);
        clC->Close();
    }
    void* const fakeRa = (void*)&Log;   // not inside the fake image: reported as "ext"
    UINT64 fv = 0;
    UINT64 done[kInFlight] = {};
    const int kFrames = 400;
    for (int f = 0; f < kFrames; ++f) {
        const int k = f % kInFlight;
        if (fence->GetCompletedValue() < done[k]) {
            fence->SetEventOnCompletion(done[k], ev);
            WaitForSingleObject(ev, 5000);
        }
        al[k]->Reset();
        cl->Reset(al[k], nullptr);
        cl->SetDescriptorHeaps(1, &hGpu);
        ev16(L"Big pass");
        clear(0);
        ev16(L"Cascade 0");
        clear(1);
        cl->EndEvent();
        cl->EndEvent();
        ev16(L"Tile 3 5");   // two numbers -> aggregated as "Tile # #"
        clear(1);
        cl->EndEvent();
        ev16(L"Tile 4 7");
        clear(1);
        cl->EndEvent();
        if (mode >= 2 && psoHeavy && psoLight) {
            cl->SetComputeRootSignature(rs);
            cl->SetComputeRootUnorderedAccessView(0, buf->GetGPUVirtualAddress());
            ev16(L"HeavyPass");
            cl->SetPipelineState(psoHeavy);
            cl->Dispatch(1024, 1, 1);
            cl->Dispatch(1024, 1, 1);
            cl->EndEvent();
            ev16(L"LightPass");
            cl->SetPipelineState(psoLight);
            cl->Dispatch(1024, 1, 1);
            cl->EndEvent();
        }
        if (mode == 5 && bufM) {   // fake MeshletPreculling region: #23, #28 x2, #24, #26, #27 with a UAV barrier after each
            const bool bad = (f % 16) >= 8;
            D3D12_RESOURCE_BARRIER ub = {};
            ub.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            ub.UAV.pResource = bufM;
            cl->SetComputeRootSignature(rs);
            cl->SetComputeRootUnorderedAccessView(0, bufM->GetGPUVirtualAddress());
            ev16(L"MeshletPreculling");
            const int seq[6] = {0, bad ? 2 : 1, bad ? 2 : 1, 3, 4, 5};
            for (int i = 0; i < 6; ++i) {
                cl->SetPipelineState(mpcPso[seq[i]]);
                cl->Dispatch(1, 1, 1);
                cl->ResourceBarrier(1, &ub);
            }
            cl->EndEvent();
        }
        if (FAILED(cl->Close())) { ++fails; printf("FAIL Close in frame %d\n", f); }
        ID3D12CommandList* lists[] = {cl};
        if (mode == 4) gp::OnEcl(q, 1, (void* const*)lists, fakeRa, true);   // what osd's ExecuteCommandLists hook does
        q->ExecuteCommandLists(1, lists);
        q->Signal(fence, ++fv);
        done[k] = fv;
        if (mode == 4) {
            fence->SetEventOnCompletion(fv, ev);   // the queue drains ...
            WaitForSingleObject(ev, 5000);
            Sleep(3);                              // ... and stays idle >= 3 ms before list C
            alC[k]->Reset();
            clC->Reset(alC[k], nullptr);
            D3D12_RESOURCE_BARRIER ub = {};
            ub.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            ub.UAV.pResource = buf;
            clC->ResourceBarrier(1, &ub);          // the list's first GPU command: GQ's head stamp goes before it
            clC->SetDescriptorHeaps(1, &hGpu);
            clC->BeginEvent(0xFF336699u, L"ListC", 10);
            D3D12_GPU_DESCRIPTOR_HANDLE gg = hGpu->GetGPUDescriptorHandleForHeapStart();
            gg.ptr += (UINT64)1 * inc;
            D3D12_CPU_DESCRIPTOR_HANDLE c1 = hCpu->GetCPUDescriptorHandleForHeapStart();
            c1.ptr += (SIZE_T)1 * inc;
            const UINT v1[4] = {7, 7, 7, 7};
            clC->ClearUnorderedAccessViewUint(gg, c1, buf, v1, 0, nullptr);
            clC->EndEvent();
            if (FAILED(clC->Close())) { ++fails; printf("FAIL Close (list C) in frame %d\n", f); }
            ID3D12CommandList* lc[] = {clC};
            gp::OnEcl(q, 1, (void* const*)lc, fakeRa, true);
            q->ExecuteCommandLists(1, lc);
            q->Signal(fence, ++fv);
            done[k] = fv;
        }
        gp::OnFrame();
        if (f == kFrames / 2) gp::LogLine();  // reset the window: the second half is measured clean
    }
    fence->SetEventOnCompletion(fv, ev);
    WaitForSingleObject(ev, 5000);
    for (int i = 0; i < 4; ++i) gp::OnFrame();
    check(dev->GetDeviceRemovedReason() == S_OK, "device not removed");
    char st[256];
    gp::StatusLine(st, sizeof st);
    printf("menu row: GP: %s\n", st);
    g_lines.clear();
    gp::LogLine();
    double big = -1, bigSelf = -1, casc = -1, tile = -1, frameMs = -1, tilePerFrame = -1;
    long long unmatched = -1, unresolved = -1;
    for (const std::string& l : g_lines) {
        double a, b, c;
        const size_t pi = l.find(" incl ");
        int rank = 0, used = 0;
        if (pi != std::string::npos && sscanf_s(l.c_str(), "GP: %d %n", &rank, &used) == 1 && used > 0 &&
            sscanf_s(l.c_str() + pi, " incl %lf ms self %lf ms %lf/frame", &a, &b, &c) == 3) {
            std::string n = l.substr((size_t)used, pi - (size_t)used);
            while (!n.empty() && n.back() == ' ') n.pop_back();
            if (n == "Big pass") big = a, bigSelf = b;
            if (n == "Cascade 0") casc = a;
            if (n == "Tile # #") tile = a, tilePerFrame = c;
            if (n == "MeshletPreculling") printf("parsed: GP marker MeshletPreculling incl %.4f ms/frame (all frames; %s)\n", a,
                                                 ctl ? "control: no MLP stamps" : "1 in 4 frames carries MLP stamps, 1 in 4 stats queries + MpcCheck copies");
        }
        if (sscanf_s(l.c_str(), "GP: frame %lf ms", &frameMs) == 1) {
            const char* u = strstr(l.c_str(), "unmatched ends ");
            if (u) unmatched = _atoi64(u + 15);
            const char* r = strstr(l.c_str(), "unresolved ");
            if (r) unresolved = _atoi64(r + 11);
        }
    }
    printf("parsed: frame %.3f ms, Big pass incl %.3f self %.3f, Cascade 0 incl %.3f, Tile # # %.3f (%.1f/frame), "
           "unmatched %lld, unresolved %lld\n", frameMs, big, bigSelf, casc, tile, tilePerFrame, unmatched, unresolved);
    check(big > 0 && casc > 0 && tile > 0, "all three names timed");
    check(big >= casc && big >= bigSelf && bigSelf >= 0, "inclusive >= self, parent >= child");
    check(bigSelf > casc, "Big pass self (64 MB clear) > Cascade 0 (4 MB clear)");
    check(tilePerFrame > 1.9 && tilePerFrame < 2.1, "\"Tile 3 5\" + \"Tile 4 7\" aggregated as \"Tile # #\", 2 per frame");
    check(unmatched == 0, "every EndEvent matched a BeginEvent");
    check(unresolved == 0, "no unresolved / bad timestamps in the measured window");
    check(frameMs > 0 && frameMs >= big - 1e-6, "frame total covers the top-level pass");
    if (mode >= 2) {
        // GP2: rows "GP2:   <rank> <desc> | <ms> ms | <draws/frame> | <intervals/frame> | [marker]"
        double heavy = -1, light = -1, hd = -1, copyMs = -1, covered = -1, span = -1;
        bool csNamed = false;
        for (const std::string& l : g_lines) {
            const size_t bar = l.find(" | ");
            if (l.rfind("GP2: frame ", 0) == 0) {
                sscanf_s(l.c_str(), "GP2: frame %lf ms GPU span (wall %*lf ms); covered %lf ms", &span, &covered);
            }
            if (l.rfind("GP2:  ", 0) != 0 || bar == std::string::npos) continue;
            double ms = 0, d = 0;
            if (sscanf_s(l.c_str() + bar, " | %lf ms | %lf |", &ms, &d) != 2) continue;
            const bool hv = l.find("[HeavyPass]") != std::string::npos, lt = l.find("[LightPass]") != std::string::npos;
            if (l.find(" cs=") != std::string::npos) csNamed = true;
            if (hv && l.find(" cs=") != std::string::npos) heavy = ms, hd = d;
            if (lt && l.find(" cs=") != std::string::npos) light = ms;
            if (l.find("(copy / resolve / clear)") != std::string::npos) copyMs += ms + (copyMs < 0 ? 1 : 0);
        }
        printf("parsed GP2: span %.3f covered %.3f, HeavyPass cs %.3f ms (%.1f dispatches/frame), LightPass cs %.3f ms, copy/clear %.3f\n",
               span, covered, heavy, hd, light, copyMs);
        check(csNamed, "GP2: compute buckets carry their cs= shader hash");
        check(heavy > 0 && light >= 0 && heavy > 3 * light, "GP2: heavy PSO (3000 iterations) billed much more than the light one (150)");
        check(hd > 1.9 && hd < 2.1, "GP2: 2 dispatches/frame counted for the heavy PSO");
        check(copyMs > 0, "GP2: clears are billed to the copy/clear bucket");
        check(covered > 0 && span > 0 && covered <= span * 1.001, "GP2: covered <= span");
    }
    if (mode == 4) {
        // "GQ: frame: L DIRECT lists/frame (P % with ECL info) over F sampled frames; span S ms, lists busy B ms, GAPS G ms/frame
        //  (N gaps/frame; per frame p50 a p90 b max c ms), ... list HEADS H ms ..., lists without GP2 coverage C ms | GP2 uncovered (span
        //  - covered) U ms (gaps + heads + no-cov = V) | ..."; edge rows "GQ:   1 [prev -> next] | ms | gaps/frame | mean us | max us | ..."
        double lists = -1, eclPct = -1, gaps = -1, p50 = -1, heads = -1, noCov = -1, unc = -1, sum = -1, edgeMs = -1, edgeMean = -1;
        unsigned long long frames = 0;
        bool edgeTop = false, worstC = false;
        int gapRow = 0;
        for (const std::string& l : g_lines) {
            if (l.rfind("GQ: frame: ", 0) == 0) {
                sscanf_s(l.c_str(), "GQ: frame: %lf DIRECT lists/frame (%lf %% with ECL info) over %llu", &lists, &eclPct, &frames);
                if (const char* g = strstr(l.c_str(), "GAPS ")) sscanf_s(g, "GAPS %lf ms/frame", &gaps);
                if (const char* g = strstr(l.c_str(), "p50 ")) sscanf_s(g, "p50 %lf", &p50);
                if (const char* g = strstr(l.c_str(), "list HEADS ")) sscanf_s(g, "list HEADS %lf", &heads);
                if (const char* g = strstr(l.c_str(), "without GP2 coverage ")) sscanf_s(g, "without GP2 coverage %lf", &noCov);
                if (const char* g = strstr(l.c_str(), "(span - covered) ")) sscanf_s(g, "(span - covered) %lf ms (gaps + heads + no-cov = %lf", &unc, &sum);
            }
            if (l.rfind("GQ:   1 [", 0) == 0) {
                edgeTop = l.find("[LightPass -> ListC]") != std::string::npos;
                const size_t bar = l.find(" | ");
                if (bar != std::string::npos) sscanf_s(l.c_str() + bar, " | %lf ms | %*f | %lf", &edgeMs, &edgeMean);
            }
            if (l.rfind("GQ:   ", 0) == 0 && l.find(" us | ") != std::string::npos && ++gapRow == 1)
                worstC = l.find("[LightPass] -> [ListC]") != std::string::npos && l.find("ext -> ext") != std::string::npos;
        }
        printf("parsed GQ: %.1f lists/frame (%.0f %% ECL) over %llu frames, gaps %.3f ms/frame (p50 %.3f), heads %.3f, no-cov %.3f, "
               "GP2 uncovered %.3f vs gaps+heads+no-cov %.3f, top edge %s %.3f ms/frame mean %.1f us\n", lists, eclPct, frames, gaps, p50,
               heads, noCov, unc, sum, edgeTop ? "[LightPass -> ListC]" : "(other)", edgeMs, edgeMean);
        check(frames >= 90 && frames <= 110, "GQ: about half of the 200 measured frames sampled (SampleEvery 2)");
        check(lists > 1.99 && lists < 2.01 && eclPct > 99.9, "GQ: 2 DIRECT lists per sampled frame, every one with its ECL submission number");
        check(gaps >= 2.9 && p50 >= 2.9, "GQ: the >= 3 ms idle stretch before list C is measured as a queue gap (mean and p50)");
        check(edgeTop && edgeMean >= 2900, "GQ: the top gap edge is [LightPass -> ListC] with a mean >= 2.9 ms");
        check(worstC, "GQ: the largest single gap names both lists' markers and ECL call sites");
        check(heads >= 0 && heads < 0.5 && noCov >= 0 && noCov < 0.01, "GQ: list heads small (barrier / marker first), every list covered by GP2");
        check(unc > 0 && sum > 0 && fabs(unc - sum) < 0.1 + 0.05 * unc, "GQ: GP2 uncovered (span - covered) == gaps + heads + no-cov (within 0.1 ms + 5 %)");
    }
    if (mode == 5 && !ctl) {
        double mlpRegions = -1, mlpD = -1, mlpBc = -1, tinyN = -1, tot = -1, disp = -1, bar = -1;
        long long eq = -1, mm = -1, eq26 = -1, mm26 = -1;
        bool threads64 = false;
        for (const std::string& l : g_lines) {
            const char* h = strstr(l.c_str(), "GP: MLP: ");
            if (h) {
                if (const char* fe = strstr(h, "front-end "))
                    sscanf_s(fe, "front-end %lf ms/frame = dispatch %lf + barrier %lf", &tot, &disp, &bar);
                if (strstr(h, " regions, "))
                    if (const char* r = strstr(h, " + gap ")) sscanf_s(r, " + gap %*f | %lf regions, %lf Dispatch", &mlpRegions, &mlpD);
                if (const char* b = strstr(h, "ExecuteIndirect, ")) sscanf_s(b, "ExecuteIndirect, %lf barrier calls", &mlpBc);
                if (const char* w = strstr(h, "threads): ")) sscanf_s(w, "threads): %lf dispatches", &tinyN);
                if (strstr(h, " | D ") && strstr(h, "| 64 | 100% |")) threads64 = true;
            }
            if (const char* t = strstr(l.c_str(), "TOTAL: #28-#31 equal "))
                sscanf_s(t, "TOTAL: #28-#31 equal %lld / MISMATCH %lld, #26 equal %lld / MISMATCH %lld", &eq, &mm, &eq26, &mm26);
        }
        printf("parsed MLP: front-end %.3f ms (dispatch %.3f, barrier %.3f), %.2f regions, %.2f Dispatch, %.2f barrier calls, %.2f one-wave "
               "dispatches /frame; MPCK #28-#31 equal %lld mismatch %lld, #26 equal %lld mismatch %lld\n", tot, disp, bar, mlpRegions, mlpD, mlpBc,
               tinyN, eq, mm, eq26, mm26);
        check(tot > 0 && disp > 0 && bar >= 0 && tot >= disp + bar - 1e-3, "MLP: region time split into dispatch + barrier + gap");
        check(mlpRegions > 0.99 && mlpRegions < 1.01, "MLP: one MeshletPreculling region per timing frame");
        check(mlpD > 5.99 && mlpD < 6.01, "MLP: 6 dispatches per region");
        check(light ? mlpBc < 0.01 : (mlpBc > 5.99 && mlpBc < 6.01), "MLP: 6 barrier calls per region (pre + post stamped; 0 with Barriers = 0)");
        check(tinyN > 5.5 && tinyN < 6.5 && threads64, "MLP: stats frames count 64 CS threads per dispatch (one wave)");
        check(eq > 0 && mm > 0, "MPCK: no-op K28 chains compare equal, chains with the changing K28 are reported as MISMATCH");
        check(eq26 > 0 && mm26 == 0, "MPCK: #26 (rewrites K23's value) always equal");
    }
    if (iq) {
        const UINT64 n = iq->GetNumStoredMessages();
        int errors = 0;
        for (UINT64 i = 0; i < n; ++i) {
            SIZE_T len = 0;
            iq->GetMessage(i, nullptr, &len);
            std::vector<char> m(len);
            auto* msg = (D3D12_MESSAGE*)m.data();
            iq->GetMessage(i, msg, &len);
            if (msg->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                ++errors;
                if (errors <= 5) printf("  debug layer: %s\n", msg->pDescription);
            }
        }
        check(errors == 0, "D3D12 debug layer: no errors or corruption messages");
    }
    printf(fails ? "FAILED (%d)\n" : "ALL PASSED\n", fails);
    return fails;
}
