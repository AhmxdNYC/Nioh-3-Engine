// cbf_bench.cpp - CBF copy cost on this PC (no game): GPU time of the system-memory -> VRAM copies CBF issues, per "frame"
// of the game's measured pattern (~11 MB of constants in 16 submissions), on the DIRECT queue (prologue) and on a COPY queue,
// plus the CbvMirror-style pattern (one 3 KB copy per CBV) for comparison. GPU timestamps; no game, no other queue load.
// Build + run: py -I tests/run_cbf_bench.py [MB per frame] [submissions per frame]
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CK(x)                                                                    \
    do {                                                                         \
        HRESULT hr_ = (x);                                                       \
        if (FAILED(hr_)) {                                                       \
            printf("FAILED %s: %08lX (line %d)\n", #x, (unsigned long)hr_, __LINE__); \
            return 1;                                                            \
        }                                                                        \
    } while (0)

struct Q {
    ID3D12CommandQueue* q = nullptr;
    ID3D12CommandAllocator* a = nullptr;
    ID3D12GraphicsCommandList* l = nullptr;
    ID3D12Fence* f = nullptr;
    uint64_t v = 0;
    ID3D12QueryHeap* qh = nullptr;
    ID3D12Resource* rb = nullptr;
    uint64_t freq = 0;
};

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    const double mbPerFrame = argc > 1 ? atof(argv[1]) : 11.0;
    const int subs = argc > 2 ? atoi(argv[2]) : 16;
    IDXGIFactory6* fac = nullptr;
    CK(CreateDXGIFactory2(0, IID_PPV_ARGS(&fac)));
    IDXGIAdapter1* ad = nullptr;
    CK(fac->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&ad)));
    DXGI_ADAPTER_DESC1 ds{};
    ad->GetDesc1(&ds);
    ID3D12Device* dev = nullptr;
    CK(D3D12CreateDevice(ad, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dev)));
    D3D12_FEATURE_DATA_D3D12_OPTIONS3 o3{};
    dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &o3, sizeof o3);
    printf("adapter %ls; copy-queue timestamps %d; pattern %.1f MB/frame in %d submissions\n", ds.Description, o3.CopyQueueTimestampQueriesSupported,
           mbPerFrame, subs);
    const uint64_t size = 64ull << 20;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES sys{};
    sys.Type = D3D12_HEAP_TYPE_CUSTOM;
    sys.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
    sys.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    D3D12_HEAP_PROPERTIES vram{D3D12_HEAP_TYPE_DEFAULT};
    ID3D12Resource *src = nullptr, *dst = nullptr;
    CK(dev->CreateCommittedResource(&sys, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&src)));
    CK(dev->CreateCommittedResource(&vram, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&dst)));
    uint8_t* map = nullptr;
    D3D12_RANGE none{0, 0};
    CK(src->Map(0, &none, (void**)&map));
    for (uint64_t i = 0; i < size; i += 4) *(uint32_t*)(map + i) = (uint32_t)(i * 2654435761u);
    auto make = [&](Q& x, D3D12_COMMAND_LIST_TYPE t) -> bool {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = t;
        if (FAILED(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&x.q))) || FAILED(dev->CreateCommandAllocator(t, IID_PPV_ARGS(&x.a))) ||
            FAILED(dev->CreateCommandList(0, t, x.a, nullptr, IID_PPV_ARGS(&x.l))) || FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&x.f))))
            return false;
        x.l->Close();
        D3D12_QUERY_HEAP_DESC hd{};
        hd.Type = t == D3D12_COMMAND_LIST_TYPE_COPY ? D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP : D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        hd.Count = 2;
        if (t == D3D12_COMMAND_LIST_TYPE_COPY && !o3.CopyQueueTimestampQueriesSupported) return true;
        if (FAILED(dev->CreateQueryHeap(&hd, IID_PPV_ARGS(&x.qh)))) return false;
        D3D12_HEAP_PROPERTIES rbp{D3D12_HEAP_TYPE_READBACK};
        D3D12_RESOURCE_DESC rr = rd;
        rr.Width = 16;
        if (FAILED(dev->CreateCommittedResource(&rbp, D3D12_HEAP_FLAG_NONE, &rr, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&x.rb)))) return false;
        x.q->GetTimestampFrequency(&x.freq);
        return true;
    };
    Q direct, copy;
    if (!make(direct, D3D12_COMMAND_LIST_TYPE_DIRECT) || !make(copy, D3D12_COMMAND_LIST_TYPE_COPY)) {
        printf("queue setup failed\n");
        return 1;
    }
    HANDLE ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    // one submission: copies of `chunk` bytes covering `bytes`, timed; returns GPU microseconds
    auto run = [&](Q& x, bool isDirect, uint64_t off, uint64_t bytes, uint64_t chunk) -> double {
        x.a->Reset();
        x.l->Reset(x.a, nullptr);
        if (x.qh) x.l->EndQuery(x.qh, D3D12_QUERY_TYPE_TIMESTAMP, 0);
        for (uint64_t o = 0; o < bytes; o += chunk) {
            const uint64_t n = bytes - o < chunk ? bytes - o : chunk;
            x.l->CopyBufferRegion(dst, off + o, src, off + o, n);
        }
        if (isDirect) {
            D3D12_RESOURCE_BARRIER b{};
            b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            b.Transition.pResource = dst;
            b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            b.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            x.l->ResourceBarrier(1, &b);
        }
        if (x.qh) {
            x.l->EndQuery(x.qh, D3D12_QUERY_TYPE_TIMESTAMP, 1);
            x.l->ResolveQueryData(x.qh, D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, x.rb, 0);
        }
        x.l->Close();
        ID3D12CommandList* l = x.l;
        x.q->ExecuteCommandLists(1, &l);
        x.q->Signal(x.f, ++x.v);
        x.f->SetEventOnCompletion(x.v, ev);
        WaitForSingleObject(ev, 5000);
        if (!x.qh) return -1;
        uint64_t* t = nullptr;
        D3D12_RANGE r{0, 16};
        x.rb->Map(0, &r, (void**)&t);
        const double us = (double)(t[1] - t[0]) * 1e6 / (double)x.freq;
        x.rb->Unmap(0, &none);
        return us;
    };
    // CPU cost of CreateConstantBufferView on the three ring kinds (cbu2pc: 27 ns/call on the game's UPLOAD ring, 245 on the
    // CBF VRAM ring): 3 KB views walking each buffer, into a CPU-only CBV heap
    {
        D3D12_HEAP_PROPERTIES up{D3D12_HEAP_TYPE_UPLOAD};
        ID3D12Resource* upl = nullptr;
        CK(dev->CreateCommittedResource(&up, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upl)));
        D3D12_DESCRIPTOR_HEAP_DESC hd{D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};
        ID3D12DescriptorHeap* dh = nullptr;
        CK(dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&dh)));
        const UINT inc = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        const D3D12_CPU_DESCRIPTOR_HANDLE h0 = dh->GetCPUDescriptorHandleForHeapStart();
        struct {
            const char* name;
            ID3D12Resource* r;
        } kinds[] = {{"UPLOAD", upl}, {"CUSTOM WC L0 (CBF staging)", src}, {"DEFAULT (CBF VRAM ring)", dst}};
        LARGE_INTEGER f, a, b;
        QueryPerformanceFrequency(&f);
        // a game-like population: thousands of other resources, and the CBF rings made LAST (as in the game)
        const int extra = argc > 4 ? atoi(argv[4]) : 0;
        D3D12_RESOURCE_DESC sd = rd;
        sd.Width = 65536;
        for (int i = 0; i < extra; ++i) {
            ID3D12Resource* x = nullptr;
            D3D12_HEAP_PROPERTIES hp{i & 1 ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT};
            dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &sd, i & 1 ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON, nullptr,
                                         IID_PPV_ARGS(&x));
        }
        ID3D12Resource *lateD = nullptr, *lateS = nullptr;
        CK(dev->CreateCommittedResource(&vram, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&lateD)));
        CK(dev->CreateCommittedResource(&sys, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&lateS)));
        if (extra) printf("  (%d other resources created, then a DEFAULT + CUSTOM ring)\n", extra);
        kinds[1].r = lateS, kinds[2].r = lateD;
        for (auto& k : kinds) {
            const uint64_t va = k.r->GetGPUVirtualAddress();
            constexpr int kN = 200000;
            QueryPerformanceCounter(&a);
            for (int i = 0; i < kN; ++i) {
                D3D12_CONSTANT_BUFFER_VIEW_DESC cd{va + ((uint64_t)i * 3328) % (size - 4096), 3328};
                D3D12_CPU_DESCRIPTOR_HANDLE h{h0.ptr + (SIZE_T)(i & 1023) * inc};
                dev->CreateConstantBufferView(&cd, h);
            }
            QueryPerformanceCounter(&b);
            printf("  CreateConstantBufferView on %-28s %.0f ns/call\n", k.name, (double)(b.QuadPart - a.QuadPart) * 1e9 / (double)f.QuadPart / kN);
        }
    }
    if (argc > 3) return 0;  // 3rd argument (any): CPU-only CBV timing, no GPU copies (safe while another app uses the GPU)
    const uint64_t per = (uint64_t)(mbPerFrame * 1048576.0 / subs) & ~255ull;
    struct Case {
        const char* name;
        Q* q;
        bool direct;
        uint64_t chunk;
    } cases[] = {{"DIRECT prologue, one copy per submission", &direct, true, ~0ull},
                 {"COPY queue, one copy per submission", &copy, false, ~0ull},
                 {"DIRECT, one 3 KB copy per CBV (CbvMirror pattern)", &direct, true, 3072},
                 {"COPY queue, one 3 KB copy per CBV", &copy, false, 3072}};
    for (auto& c : cases) {
        double tot = 0;
        int nOk = 0;
        uint64_t off = 0;
        for (int frame = 0; frame < 12; ++frame) {  // 2 warm-up frames
            double fr = 0;
            for (int s = 0; s < subs; ++s) {
                if (off + per > size) off = 0;
                const double us = run(*c.q, c.direct, off, per, c.chunk == ~0ull ? per : c.chunk);
                off += per;
                fr += us;
            }
            if (frame >= 2 && fr >= 0) tot += fr, ++nOk;
        }
        if (!nOk || tot < 0) {
            printf("  %-52s no timestamps on this queue\n", c.name);
            continue;
        }
        const double ms = tot / nOk / 1000.0;
        printf("  %-52s GPU %.3f ms/frame for %.1f MB = %.1f GB/s\n", c.name, ms, per * subs / 1048576.0, per * subs / (ms / 1000.0) / 1e9);
    }
    return 0;
}
