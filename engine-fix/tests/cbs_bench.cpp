// cbs_bench.cpp - CBS micro-benchmark on this PC: memcpy vs CbuStreamCopy into a mapped D3D12 UPLOAD buffer (BAR VRAM with
// ReBAR / SAM, system memory without), in the replay thread's pattern: consecutive 256-aligned ring blocks, sizes like the
// game's cbuffers. Not a game test. Build + run: py -I tests/run_cbs_bench.py
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include <emmintrin.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

#define CBU_HARNESS 1
#include "../src/cbvupload.h"

int main() {
    IDXGIFactory6* f = nullptr;
    if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&f)))) return 2;
    IDXGIAdapter1* ad = nullptr;
    f->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&ad));
    DXGI_ADAPTER_DESC1 ds{};
    ad->GetDesc1(&ds);
    ID3D12Device* dev = nullptr;
    if (FAILED(D3D12CreateDevice(ad, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&dev)))) return 3;
    printf("adapter %ls\n", ds.Description);
    const uint64_t kSize = 64ull << 20;
    D3D12_HEAP_PROPERTIES hp{D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = kSize;
    rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* buf = nullptr;
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buf)))) return 4;
    uint8_t* map = nullptr;
    D3D12_RANGE none{0, 0};
    buf->Map(0, &none, (void**)&map);
    // shadow source: 112 slots x 64 KB of cached memory; sizes ~ the game's (avg ~2.9 KB, multiples of 16)
    std::vector<uint8_t> shadow(112 * 65536);
    std::mt19937 rng(5);
    for (auto& b : shadow) b = (uint8_t)rng();
    std::vector<uint32_t> sizes(1875), slots(1875);
    for (size_t i = 0; i < sizes.size(); ++i) {
        const uint32_t k = rng() % 100;
        sizes[i] = 16 * (k < 50 ? 4 + rng() % 60 : k < 90 ? 64 + rng() % 192 : 256 + rng() % 768);
        slots[i] = rng() % 112;
    }
    uint64_t total = 0;
    for (auto s : sizes) total += s;
    printf("pattern: %zu uploads per 'frame', %.2f MB (avg %.0f B), 256-aligned consecutive ring blocks\n", sizes.size(), total / 1048576.0,
           (double)total / sizes.size());
    LARGE_INTEGER fq;
    QueryPerformanceFrequency(&fq);
    // mode: 0 memcpy, 1 memcpy + sfence per upload (the CBV create right after each copy drains the WC buffers if the
    // driver serialises), 2 CbuStreamCopy, 3 CbuStreamCopy + sfence per upload
    auto run = [&](int mode, int frames) {
        cbvupload::TestSetup(nullptr, mode >= 2 ? 1 : 0, 0, 0, 0);
        uint64_t off = 0;
        LARGE_INTEGER a, b;
        QueryPerformanceCounter(&a);
        for (int fr = 0; fr < frames; ++fr)
            for (size_t i = 0; i < sizes.size(); ++i) {
                const uint32_t blk = (sizes[i] + 255) & ~255u;
                if (off + blk > kSize) off = 0;
                CbuStreamCopy(map + off, &shadow[(size_t)slots[i] * 65536], sizes[i]);
                if (mode & 1) _mm_sfence();
                off += blk;
            }
        _mm_sfence();
        QueryPerformanceCounter(&b);
        return (double)(b.QuadPart - a.QuadPart) * 1000.0 / fq.QuadPart / frames;
    };
    for (int m = 0; m < 4; ++m) run(m, 20);
    const char* names[4] = {"memcpy", "memcpy+sfence", "stream", "stream+sfence"};
    double sum[4] = {};
    for (int rep = 0; rep < 8; ++rep) {
        printf("  rep %d:", rep);
        for (int m = 0; m < 4; ++m) {
            const double t = run(m, 100);
            sum[m] += t;
            printf("  %s %.3f", names[m], t);
        }
        printf(" ms/frame\n");
    }
    printf("  mean ms/frame (ns/upload, GB/s):");
    for (int m = 0; m < 4; ++m) printf("  %s %.3f (%.0f, %.1f)", names[m], sum[m] / 8, sum[m] / 8 * 1e6 / sizes.size(), total / (sum[m] / 8) / 1e6);
    printf("\n");
    // the same into cached system memory, for reference
    std::vector<uint8_t> sys(kSize + 256);
    uint8_t* sm = (uint8_t*)(((uintptr_t)sys.data() + 255) & ~(uintptr_t)255);
    uint8_t* keep = map;
    map = sm;
    const double m = run(0, 200), s = run(2, 200);
    printf("  (cached RAM) memcpy %.3f ms/frame | CbuStreamCopy %.3f ms/frame\n", m, s);
    map = keep;
    buf->Unmap(0, nullptr);
    return 0;
}
