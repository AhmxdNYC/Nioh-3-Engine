// sbm_harness.cpp - SBM scene-buffer mirror (src/scenemirror.cpp) on a real D3D12 device of this PC, no game.
// A fake MeshletSys points (+16A0B0) at fake scene-buffer sets whose +58h slot holds a fake engine buffer object over one
// real UPLOAD resource U (set X = U[0, 256K), set Y = U[512K, 768K)); a third SRV covers U[256K, 512K) (not a scene buffer).
// Per frame: the "CPU" writes a pattern into the current set's range, the post-update step runs (copy to the twin + SRV
// rewrite), the "game" queue submits a list (after the queue hook) that copies the twin into READBACK.
// Checks: the scene SRVs (and only those) are re-created on the twin, in the frame of their set; the game list sees this
// frame's bytes in the twin (Wait inserted, copy done before the read); the other set's twin range keeps its own bytes; an
// SRV created later inside a copied range is redirected at creation; mode 0 (game DIRECT queue) gives the same; with the
// probe, a CPU write into the mirrored range after its copy is caught (LATE) and every SRV is put back on U.
// The D3D12 debug layer (when installed) must report no errors.  Build + run: py -I tests/run_sbm_harness.py
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../src/scenemirror.h"

namespace rebar {
int UploadInSystemMemory() { return 1; }   // auto mode: "no ReBAR"
}

static std::vector<std::string> g_lines;
static void Log(const char* fmt, ...) {
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("  log: %s\n", b);
    g_lines.push_back(b);
}

struct SrvCall {
    SIZE_T h;
    ID3D12Resource* r;
};
static std::vector<SrvCall> g_calls;
static void __stdcall RealSrv(ID3D12Device* d, ID3D12Resource* r, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE h) {
    d->CreateShaderResourceView(r, desc, h);
    g_calls.push_back({h.ptr, r});
}

int main(int argc, char** argv) {
    const int queueMode = argc > 1 ? atoi(argv[1]) : 1;     // 1 own COPY queue, 0 game DIRECT queue
    const bool late = argc > 2 && !strcmp(argv[2], "late");  // late-write scenario (probe on)
    printf("=== SBM harness: queue %s%s ===\n", queueMode ? "COPY" : "DIRECT", late ? ", late-write check" : "");
    int fails = 0;
    auto check = [&](bool c, const char* m) {
        printf("%s %s\n", c ? "OK  " : "FAIL", m);
        fails += !c;
    };
    ID3D12Debug* dbg = nullptr;
    const bool debug = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)));
    if (debug) dbg->EnableDebugLayer();
    ID3D12Device* dev = nullptr;
    if (FAILED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)))) {
        printf("FAIL no D3D12 device\n");
        return 1;
    }
    ID3D12InfoQueue* iq = nullptr;
    if (debug) dev->QueryInterface(IID_PPV_ARGS(&iq));
    printf("debug layer: %s\n", iq ? "on" : "not installed");
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ID3D12CommandQueue* gameQ = nullptr;
    dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&gameQ));
    ID3D12CommandAllocator* ga = nullptr;
    dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&ga));
    ID3D12GraphicsCommandList* gl = nullptr;
    dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, ga, nullptr, IID_PPV_ARGS(&gl));
    gl->Close();
    ID3D12Fence* gf = nullptr;
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gf));
    uint64_t gv = 0;
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);

    auto buffer = [&](D3D12_HEAP_TYPE t, uint64_t w, D3D12_RESOURCE_STATES st) {
        D3D12_HEAP_PROPERTIES hp{};
        hp.Type = t;
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = w;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource* r = nullptr;
        dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, st, nullptr, IID_PPV_ARGS(&r));
        return r;
    };
    constexpr uint64_t kMB = 1 << 20, kCap = 256 << 10;
    ID3D12Resource* U = buffer(D3D12_HEAP_TYPE_UPLOAD, kMB, D3D12_RESOURCE_STATE_GENERIC_READ);
    ID3D12Resource* RB = buffer(D3D12_HEAP_TYPE_READBACK, kMB, D3D12_RESOURCE_STATE_COPY_DEST);
    uint8_t* up = nullptr;
    U->Map(0, nullptr, (void**)&up);
    uint8_t* rb = nullptr;
    RB->Map(0, nullptr, (void**)&rb);

    scenemirror::TestSetup(&Log, 2, queueMode, late ? 1 : 0);
    scenemirror::SetSrvOriginal(&RealSrv);
    scenemirror::SetSysVaTest(nullptr);

    // CPU descriptor heap with the engine's views
    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    hd.NumDescriptors = 16;
    ID3D12DescriptorHeap* heap = nullptr;
    dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&heap));
    const UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto handle = [&](int i) {
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += (SIZE_T)i * inc;
        return h;
    };
    auto srv = [&](int slot, uint64_t first, uint32_t n) {   // the engine creating a view (memstats HkSrv order)
        D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        d.Format = DXGI_FORMAT_UNKNOWN;
        d.Buffer.FirstElement = first;
        d.Buffer.NumElements = n;
        d.Buffer.StructureByteStride = 8;
        ID3D12Resource* use = U;
        scenemirror::OnSrv(dev, U, &d, handle(slot), &use);
        dev->CreateShaderResourceView(use, &d, handle(slot));
        return use;
    };
    srv(0, 0, (uint32_t)(kCap / 8));                  // set X's buffer (U[0, 256K))
    srv(1, (512 << 10) / 8, (uint32_t)(kCap / 8));    // set Y's buffer (U[512K, 768K))
    srv(2, (256 << 10) / 8, (uint32_t)(kCap / 8));    // another buffer in U, not a scene buffer
    check(g_calls.empty(), "no rewrite before the first update");

    // fake engine objects
    uint8_t* sys = (uint8_t*)VirtualAlloc(nullptr, 0x170000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t* setX = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t* setY = setX + 0x800;
    uint8_t* bufA = (uint8_t*)VirtualAlloc(nullptr, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    uint8_t* bufB = bufA + 0x800;
    auto mkbuf = [&](uint8_t* b, uint64_t off, uint64_t wlen) {
        *(ID3D12Resource**)(b + 0x18) = U;
        *(int*)(b + 0x68) = 2;
        *(uint32_t*)(b + 0x70) = (uint32_t)kCap;
        *(uint64_t*)(b + 0x80) = off;
        *(int*)(b + 0x98) = 4;
        *(uint64_t*)(b + 0xA0) = 0;
        *(uint64_t*)(b + 0xA8) = wlen;
    };
    mkbuf(bufA, 0, 192 << 10);
    mkbuf(bufB, 512 << 10, 128 << 10);
    *(uint8_t**)(setX + 0x58) = bufA;
    *(uint8_t**)(setY + 0x58) = bufB;

    scenemirror::OnEcl(gameQ);   // the queue hook goes live (first game DIRECT submission)
    // one frame: CPU write of the current set, post-update step, game list (after the queue hook) copies U's twin regions
    // the engine's views point at into READBACK. Returns the twin (from the rewrite calls).
    ID3D12Resource* twin = nullptr;
    auto frame = [&](uint8_t* set, uint64_t off, uint64_t len, uint8_t pat) {
        *(uint8_t**)(sys + 0x16A0B0) = set;
        memset(up + off, pat, len);
        scenemirror::TestAfterUpdate((uint64_t)sys);
        for (const SrvCall& c : g_calls)
            if (c.r != U) twin = c.r;
        scenemirror::OnEcl(gameQ);   // must insert the Wait for this copy
        if (!twin) return;
        ga->Reset();
        gl->Reset(ga, nullptr);
        gl->CopyBufferRegion(RB, 0, twin, 0, kMB);
        gl->Close();
        ID3D12CommandList* l = gl;
        gameQ->ExecuteCommandLists(1, &l);
        gameQ->Signal(gf, ++gv);
        gf->SetEventOnCompletion(gv, ev);
        WaitForSingleObject(ev, 5000);
    };
    auto all = [&](uint64_t off, uint64_t len, uint8_t pat) {
        for (uint64_t i = 0; i < len; ++i)
            if (rb[off + i] != pat) return false;
        return true;
    };

    if (argc > 2 && !strcmp(argv[2], "bw")) {
        // copy throughput from SYSTEM memory (what a PC without ReBAR has): a 16 MB CUSTOM L0 buffer (write-back, or
        // write-combine with "wc" - a ReBAR driver may keep WC L0 in BAR VRAM), 4 MB written + mirrored per frame, 60 frames
        const bool wc = argc > 3 && !strcmp(argv[3], "wc");
        D3D12_HEAP_PROPERTIES hp{};
        hp.Type = D3D12_HEAP_TYPE_CUSTOM;
        hp.CPUPageProperty = wc ? D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE : D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
        hp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = 16 * kMB;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource* S = nullptr;
        if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&S)))) {
            printf("FAIL CUSTOM L0 buffer not created\n");
            return 1;
        }
        uint8_t* sp = nullptr;
        S->Map(0, nullptr, (void**)&sp);
        for (int i = 0; i < 2; ++i) {
            D3D12_SHADER_RESOURCE_VIEW_DESC d{};
            d.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            d.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            d.Buffer.FirstElement = (uint64_t)i * (8 * kMB) / 8;
            d.Buffer.NumElements = (uint32_t)(8 * kMB / 8);
            d.Buffer.StructureByteStride = 8;
            ID3D12Resource* use = S;
            scenemirror::OnSrv(dev, S, &d, handle(8 + i), &use);
            dev->CreateShaderResourceView(use, &d, handle(8 + i));
        }
        for (int i = 0; i < 2; ++i) {
            uint8_t* b = i ? bufB : bufA;
            *(ID3D12Resource**)(b + 0x18) = S;
            *(uint32_t*)(b + 0x70) = (uint32_t)(8 * kMB);
            *(uint64_t*)(b + 0x80) = (uint64_t)i * 8 * kMB;
            *(uint64_t*)(b + 0xA8) = 4 * kMB;
        }
        LARGE_INTEGER f0, f1, qf;
        QueryPerformanceFrequency(&qf);
        QueryPerformanceCounter(&f0);
        for (int f = 0; f < 60; ++f) {
            *(uint8_t**)(sys + 0x16A0B0) = (f & 1) ? setY : setX;
            memset(sp + (uint64_t)(f & 1) * 8 * kMB, f, 4 * kMB);
            scenemirror::TestAfterUpdate((uint64_t)sys);
            scenemirror::OnEcl(gameQ);
            gameQ->Signal(gf, ++gv);   // the "game" frame = the wait for the copy
            gf->SetEventOnCompletion(gv, ev);
            WaitForSingleObject(ev, 5000);
        }
        QueryPerformanceCounter(&f1);
        printf("bw: %s L0, 60 frames x 4 MB in %.1f ms wall (copy + wait, serial)\n", wc ? "WRITE_COMBINE" : "WRITE_BACK",
               (double)(f1.QuadPart - f0.QuadPart) * 1000.0 / (double)qf.QuadPart);
        scenemirror::LogLine();
        check(dev->GetDeviceRemovedReason() == S_OK, "device not removed");
        printf("%s (%d failure(s))\n", fails ? "SBM HARNESS FAILED" : "SBM HARNESS OK", fails);
        return fails ? 1 : 0;
    }
    if (!late) {
        frame(setX, 0, 192 << 10, 0x11);
        bool hA = false, other = false;
        for (const SrvCall& c : g_calls) {
            if (c.h == handle(0).ptr && c.r != U) hA = true;
            if (c.h != handle(0).ptr) other = true;
        }
        check(twin != nullptr && hA, "frame 1: set X's SRV re-created on a VRAM twin");
        check(!other, "frame 1: set Y's SRV and the non-scene SRV untouched");
        check(twin && all(0, 192 << 10, 0x11), "frame 1: the game list reads this frame's bytes from the twin");
        const size_t n1 = g_calls.size();
        frame(setY, 512 << 10, 128 << 10, 0x22);
        bool hB = false, hF = false;
        for (size_t i = n1; i < g_calls.size(); ++i) {
            if (g_calls[i].h == handle(1).ptr && g_calls[i].r == twin) hB = true;
            if (g_calls[i].h == handle(2).ptr) hF = true;
        }
        check(hB && !hF, "frame 2: set Y's SRV re-created on the twin, the non-scene SRV never");
        check(all(512 << 10, 128 << 10, 0x22), "frame 2: set Y's range in the twin = this frame's bytes");
        check(all(0, 192 << 10, 0x11), "frame 2: set X's range in the twin kept frame 1's bytes");
        frame(setX, 0, 192 << 10, 0x33);
        check(all(0, 192 << 10, 0x33) && all(512 << 10, 128 << 10, 0x22), "frame 3: set X updated, set Y kept");
        ID3D12Resource* u = srv(3, 1024, 2048);   // a new engine view inside set X's (copied) range
        check(u == twin, "a view created later inside a copied scene range is redirected at creation");
        ID3D12Resource* u2 = srv(4, (256 << 10) / 8, 16);
        check(u2 == U, "a view created later outside the scene ranges stays on the original");
        for (int f = 0; f < 40; ++f) frame((f & 1) ? setX : setY, (f & 1) ? 0 : 512 << 10, 64 << 10, (uint8_t)(0x40 + f));
        check(all(512 << 10, 64 << 10, 0x40 + 38) && all(0, 64 << 10, 0x40 + 39), "40 more frames (allocator ring wrap): last bytes of both sets");
    } else {
        frame(setX, 0, 192 << 10, 0x11);   // copy + arm the late-write check (probe)
        memset(up, 0x99, 16);              // the CPU writes set X AFTER its copy
        frame(setY, 512 << 10, 128 << 10, 0x22);
        bool late1 = false;
        for (const std::string& s : g_lines)
            if (s.find("LATE CPU WRITE") != std::string::npos) late1 = true;
        bool back = false;
        for (const SrvCall& c : g_calls)
            if (c.h == handle(0).ptr && c.r == U) back = true;
        check(late1, "a CPU write after the copy is reported (LATE CPU WRITE)");
        check(back, "and set X's SRV is put back on the original resource");
    }
    scenemirror::LogLine();
    if (iq) {
        const UINT64 n = iq->GetNumStoredMessages();
        int errs = 0;
        for (UINT64 i = 0; i < n; ++i) {
            SIZE_T sz = 0;
            iq->GetMessage(i, nullptr, &sz);
            std::vector<char> m(sz);
            auto* msg = (D3D12_MESSAGE*)m.data();
            iq->GetMessage(i, msg, &sz);
            if (msg->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                ++errs;
                printf("  d3d12: %s\n", msg->pDescription);
            }
        }
        check(errs == 0, "D3D12 debug layer: no errors");
    }
    check(dev->GetDeviceRemovedReason() == S_OK, "device not removed");
    printf("%s (%d failure(s))\n", fails ? "SBM HARNESS FAILED" : "SBM HARNESS OK", fails);
    return fails ? 1 : 0;
}
