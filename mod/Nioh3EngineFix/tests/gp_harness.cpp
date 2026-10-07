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
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/gpuopt.h"
#include "../src/gpuprof.h"

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

int main() {
    int fails = 0;
    auto check = [&](bool c, const char* m) { printf("%s %s\n", c ? "OK  " : "FAIL", m); fails += !c; };
    // fake engine image: g = [base+45BA588] -> block with the record (+2A62h) and replay (+552h) bytes
    uint8_t* base = (uint8_t*)VirtualAlloc(nullptr, 0x4C00000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t* g = (uint8_t*)VirtualAlloc(nullptr, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    *(uint8_t**)(base + 0x45BA588) = g;
    wchar_t ini[MAX_PATH];
    GetTempPathW(MAX_PATH, ini);
    wcscat_s(ini, L"gp_harness.ini");
    WritePrivateProfileStringW(L"Profiler", L"GPUProf", L"1", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"Barriers", L"0", ini);
    WritePrivateProfileStringW(L"GpuOpt", L"InstallDelaySec", L"600", ini);

    ID3D12Debug* dbg = nullptr;
    const bool debug = SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&dbg)) && dbg;
    if (debug) dbg->EnableDebugLayer();
    printf("D3D12 debug layer: %s\n", debug ? "on" : "not installed");

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
    auto ev16 = [&](const wchar_t* s) { cl->BeginEvent(0xFF336699u, s, (UINT)(wcslen(s) * 2)); };
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
        if (FAILED(cl->Close())) { ++fails; printf("FAIL Close in frame %d\n", f); }
        ID3D12CommandList* lists[] = {cl};
        q->ExecuteCommandLists(1, lists);
        q->Signal(fence, ++fv);
        done[k] = fv;
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
