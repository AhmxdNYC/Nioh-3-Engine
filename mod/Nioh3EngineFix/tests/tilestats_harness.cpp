// tilestats_harness.cpp - TS (tilestats.cpp) on a real D3D12 device of this PC, no game. Build + run:
//   py -I tests/run_tilestats_harness.py
// The real src/tilestats.cpp hooks the runtime's queue vtable. A reserved 1024x1024 R32_UINT texture (8x8 tiles of
// 128x128) gets an engine-like sequence of UpdateTileMappings calls (single-tile regions, one tile per range, map and
// unmap, two heaps, a different resource, an overlap, REUSE_SINGLE_TILE, a null counts array, a 2x1 box that cannot
// merge, a CopyTileMappings) on a COPY queue, then Signal; the DIRECT queue Waits and copies the texture out. Every heap
// page holds a constant (heap 1 page i = 1000+i, heap 2 page i = 2000+i), NULL tiles read 0 (tiled tier >= 2).
// Checks: hooks in, counters, the read-back mapping equals the model for TileBatch 0, 1 and 2 (bit-exact), TileBatch 1
// merges (fewer issued calls), 2 counts would-merges, a held call is issued by OnFrame, no debug-layer errors.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "../src/tilestats.h"

static int g_fail = 0;
static void Log(const char* fmt, ...) {
    char b[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    printf("  log| %s\n", b);
}
#define CHECK(c, ...)                                   \
    do {                                                \
        if (!(c)) {                                     \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);                        \
            printf("\n");                               \
            ++g_fail;                                   \
        }                                               \
    } while (0)
#define HR(x)                                                                         \
    do {                                                                              \
        HRESULT hr_ = (x);                                                            \
        if (FAILED(hr_)) {                                                            \
            printf("FATAL %s failed hr %08lX (line %d)\n", #x, (unsigned long)hr_, __LINE__); \
            exit(2);                                                                  \
        }                                                                             \
    } while (0)

static ID3D12Device* dev;
static ID3D12CommandQueue *qCopy, *qDirect;
static ID3D12Fence* fence;
static UINT64 fenceVal = 0;
static HANDLE ev;
static ID3D12Heap *heap1, *heap2;
static ID3D12CommandAllocator* alloc;
static ID3D12GraphicsCommandList* list;
constexpr UINT kTex = 1024, kTileTx = 128, kTilesX = kTex / kTileTx;  // 8x8 tiles

static void WaitIdle(ID3D12CommandQueue* q) {
    HR(q->Signal(fence, ++fenceVal));
    HR(fence->SetEventOnCompletion(fenceVal, ev));
    WaitForSingleObject(ev, 10000);
}
static ID3D12Resource* MakeReserved() {
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d.Width = kTex;
    d.Height = kTex;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = DXGI_FORMAT_R32_UINT;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
    ID3D12Resource* r = nullptr;
    HR(dev->CreateReservedResource(&d, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&r)));
    return r;
}
static ID3D12Resource* MakeBuffer(D3D12_HEAP_TYPE t, UINT64 size, D3D12_RESOURCE_STATES st) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = t;
    D3D12_RESOURCE_DESC d{};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = size;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* r = nullptr;
    HR(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &d, st, nullptr, IID_PPV_ARGS(&r)));
    return r;
}
static D3D12_TILED_RESOURCE_COORDINATE Tc(UINT tile) {
    D3D12_TILED_RESOURCE_COORDINATE c{};
    c.X = tile % kTilesX;
    c.Y = tile / kTilesX;
    return c;
}
// engine-style call: n single-tile regions (1x1x1 box), one range per tile
static void MapTiles(ID3D12Resource* r, const std::vector<UINT>& tiles, ID3D12Heap* h, const std::vector<UINT>& pages,
                     bool unmap = false) {
    std::vector<D3D12_TILED_RESOURCE_COORDINATE> c;
    std::vector<D3D12_TILE_REGION_SIZE> s;
    std::vector<D3D12_TILE_RANGE_FLAGS> f;
    std::vector<UINT> st, cnt;
    for (size_t i = 0; i < tiles.size(); ++i) {
        c.push_back(Tc(tiles[i]));
        D3D12_TILE_REGION_SIZE z{1, TRUE, 1, 1, 1};
        s.push_back(z);
        f.push_back(unmap ? D3D12_TILE_RANGE_FLAG_NULL : D3D12_TILE_RANGE_FLAG_NONE);
        st.push_back(unmap ? 0 : pages[i]);
        cnt.push_back(1);
    }
    qCopy->UpdateTileMappings(r, (UINT)c.size(), c.data(), s.data(), h, (UINT)f.size(), f.data(), st.data(), cnt.data(),
                              D3D12_TILE_MAPPING_FLAG_NONE);
}

// fills every page of both heaps with its constant through a fill texture mapped 1:1 onto the pages
static void FillPages() {
    ID3D12Resource* fill = MakeReserved();
    std::vector<UINT> t1, p1, t2;
    for (UINT i = 0; i < 16; ++i) { t1.push_back(i); p1.push_back(i); t2.push_back(16 + i); }
    MapTiles(fill, t1, heap1, p1);
    MapTiles(fill, t2, heap2, p1);
    WaitIdle(qCopy);
    const UINT64 tileBytes = 128 * 128 * 4;
    ID3D12Resource* up = MakeBuffer(D3D12_HEAP_TYPE_UPLOAD, tileBytes * 32, D3D12_RESOURCE_STATE_GENERIC_READ);
    uint32_t* m = nullptr;
    HR(up->Map(0, nullptr, (void**)&m));
    for (UINT t = 0; t < 32; ++t)
        for (UINT i = 0; i < 128 * 128; ++i) m[t * 128 * 128 + i] = t < 16 ? 1000 + t : 2000 + (t - 16);
    up->Unmap(0, nullptr);
    HR(alloc->Reset());
    HR(list->Reset(alloc, nullptr));
    for (UINT t = 0; t < 32; ++t) {
        D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
        dst.pResource = fill;
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        src.pResource = up;
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint.Offset = t * tileBytes;
        src.PlacedFootprint.Footprint = {DXGI_FORMAT_R32_UINT, 128, 128, 1, 128 * 4};
        list->CopyTextureRegion(&dst, (t % kTilesX) * 128, (t / kTilesX) * 128, 0, &src, nullptr);
    }
    HR(list->Close());
    ID3D12CommandList* l[] = {list};
    qDirect->ExecuteCommandLists(1, l);
    WaitIdle(qDirect);
    up->Release();
    fill->Release();
}

// the engine-like sequence; returns the value read at each tile's first texel (64 tiles)
static std::vector<uint32_t> RunSequence() {
    ID3D12Resource* T = MakeReserved();
    ID3D12Resource* T2 = MakeReserved();
    MapTiles(T, {0, 1, 2, 3, 4, 5}, heap1, {0, 1, 2, 3, 4, 5});  // a
    MapTiles(T, {6, 7}, heap1, {6, 7});                           // b  (merges with a)
    MapTiles(T, {1, 2}, nullptr, {}, true);                       // c  unmap: overlaps a -> issue, start new
    MapTiles(T, {8}, heap2, {0});                                 // d  heap2 (c used no heap -> merges)
    MapTiles(T, {9}, heap2, {1});                                 // e  merges
    MapTiles(T, {3}, heap1, {9});                                 // f  heap1 vs heap2 -> issue
    MapTiles(T, {3}, heap1, {10});                                // g  same tile as f -> issue, f then g
    {                                                             // h  unmap tile 0, heap passed but unused
        D3D12_TILED_RESOURCE_COORDINATE c = Tc(0);
        D3D12_TILE_REGION_SIZE s{1, FALSE, 0, 0, 0};
        D3D12_TILE_RANGE_FLAGS f = D3D12_TILE_RANGE_FLAG_NULL;
        UINT st = 0, n = 1;
        qCopy->UpdateTileMappings(T, 1, &c, &s, heap1, 1, &f, &st, &n, D3D12_TILE_MAPPING_FLAG_NONE);
    }
    {  // i  tiles 10..12 -> one REUSE_SINGLE_TILE range on heap1 page 11
        D3D12_TILED_RESOURCE_COORDINATE c[3] = {Tc(10), Tc(11), Tc(12)};
        D3D12_TILE_REGION_SIZE s[3] = {{1, TRUE, 1, 1, 1}, {1, TRUE, 1, 1, 1}, {1, TRUE, 1, 1, 1}};
        D3D12_TILE_RANGE_FLAGS f = D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE;
        UINT st = 11, n = 3;
        qCopy->UpdateTileMappings(T, 3, c, s, heap1, 1, &f, &st, &n, D3D12_TILE_MAPPING_FLAG_NONE);
    }
    MapTiles(T2, {0}, heap1, {12});  // j  other resource -> issue
    MapTiles(T, {13}, heap2, {2});   // k  other resource again -> issue
    {                                // k2 CopyTileMappings T 8..9 -> T 14..15 (held call issued first)
        D3D12_TILED_RESOURCE_COORDINATE dc = Tc(14), sc = Tc(8);
        D3D12_TILE_REGION_SIZE s{2, FALSE, 0, 0, 0};
        qCopy->CopyTileMappings(T, &dc, T, &sc, &s, D3D12_TILE_MAPPING_FLAG_NONE);
    }
    {  // l  tile 20, no range flags / counts arrays (one range covers the region)
        D3D12_TILED_RESOURCE_COORDINATE c = Tc(20);
        D3D12_TILE_REGION_SIZE s{1, TRUE, 1, 1, 1};
        UINT st = 13;
        qCopy->UpdateTileMappings(T, 1, &c, &s, heap1, 1, nullptr, &st, nullptr, D3D12_TILE_MAPPING_FLAG_NONE);
    }
    {  // m  a 2x1 box (tiles 20, 21) -> not mergeable, passes through after the held l
        D3D12_TILED_RESOURCE_COORDINATE c = Tc(20);
        D3D12_TILE_REGION_SIZE s{2, TRUE, 2, 1, 1};
        D3D12_TILE_RANGE_FLAGS f = D3D12_TILE_RANGE_FLAG_NONE;
        UINT st = 3, n = 2;
        qCopy->UpdateTileMappings(T, 1, &c, &s, heap2, 1, &f, &st, &n, D3D12_TILE_MAPPING_FLAG_NONE);
    }
    MapTiles(T, {30}, heap1, {14});  // n  held until the Signal below
    // engine pattern: Signal on the tile queue, the consumer queue Waits
    HR(qCopy->Signal(fence, ++fenceVal));
    HR(qDirect->Wait(fence, fenceVal));
    ID3D12Resource* rb = MakeBuffer(D3D12_HEAP_TYPE_READBACK, (UINT64)kTex * kTex * 4, D3D12_RESOURCE_STATE_COPY_DEST);
    HR(alloc->Reset());
    HR(list->Reset(alloc, nullptr));
    D3D12_TEXTURE_COPY_LOCATION dst{}, src{};
    src.pResource = T;
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = rb;
    dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dst.PlacedFootprint.Footprint = {DXGI_FORMAT_R32_UINT, kTex, kTex, 1, kTex * 4};
    list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    HR(list->Close());
    ID3D12CommandList* l[] = {list};
    qDirect->ExecuteCommandLists(1, l);
    WaitIdle(qDirect);
    std::vector<uint32_t> out(kTilesX * kTilesX);
    uint32_t* m = nullptr;
    HR(rb->Map(0, nullptr, (void**)&m));
    for (UINT t = 0; t < out.size(); ++t) out[t] = m[(t / kTilesX) * 128 * kTex + (t % kTilesX) * 128];
    rb->Unmap(0, nullptr);
    rb->Release();
    T->Release();
    T2->Release();
    return out;
}

int main() {
    ID3D12Debug* dbg = nullptr;
    bool debugLayer = SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)));
    if (debugLayer) dbg->EnableDebugLayer();
    HR(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev)));
    D3D12_FEATURE_DATA_D3D12_OPTIONS o{};
    dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &o, sizeof o);
    printf("device ok, debug layer %s, tiled resources tier %d\n", debugLayer ? "on" : "not installed",
           (int)o.TiledResourcesTier);
    if (o.TiledResourcesTier < D3D12_TILED_RESOURCES_TIER_2) { printf("SKIP: needs tiled tier 2\n"); return 0; }
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    HR(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&qCopy)));
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HR(dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&qDirect)));
    CHECK(*(void**)qCopy == *(void**)qDirect, "COPY and DIRECT queues have different vtables");
    HR(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)));
    ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    D3D12_HEAP_DESC hd{};
    hd.SizeInBytes = 16 * 65536;
    hd.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    hd.Flags = D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES;
    HR(dev->CreateHeap(&hd, IID_PPV_ARGS(&heap1)));
    HR(dev->CreateHeap(&hd, IID_PPV_ARGS(&heap2)));
    HR(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&alloc)));
    HR(dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc, nullptr, IID_PPV_ARGS(&list)));
    HR(list->Close());

    // ini: TileStats 2, TileBatch 0 (switched per run below)
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    std::wstring ini = std::wstring(tmp) + L"tilestats_harness.ini";
    WritePrivateProfileStringW(L"Profiler", L"TileStats", L"2", ini.c_str());
    WritePrivateProfileStringW(L"GpuOpt", L"TileBatch", L"0", ini.c_str());
    tilestats::StartInfo si;
    si.log = &Log;
    si.iniPath = ini.c_str();
    CHECK(tilestats::Start(si), "Start returned false");
    CHECK(tilestats::InstallOnQueueVtable(*(void***)qCopy, false), "hooks not installed");

    FillPages();
    // model of the sequence (heap1 page i = 1000+i, heap2 page i = 2000+i, NULL = 0)
    std::vector<uint32_t> want(64, 0);
    want[3] = 1010; want[4] = 1004; want[5] = 1005; want[6] = 1006; want[7] = 1007; want[8] = 2000; want[9] = 2001;
    want[10] = want[11] = want[12] = 1011; want[13] = 2002; want[14] = 2000; want[15] = 2001; want[20] = 2003;
    want[21] = 2004; want[30] = 1014;

    std::vector<uint32_t> res[3];
    tilestats::Counters before[3], after[3];
    for (int mode = 0; mode < 3; ++mode) {
        tilestats::SetBatchModeForTest(mode);
        before[mode] = tilestats::Totals();
        res[mode] = RunSequence();
        after[mode] = tilestats::Totals();
        int bad = 0;
        for (int t = 0; t < 64; ++t)
            if (res[mode][t] != want[t]) {
                if (bad++ < 6) printf("  mode %d tile %d: read %u want %u\n", mode, t, res[mode][t], want[t]);
            }
        CHECK(bad == 0, "TileBatch %d: %d tiles differ from the model", mode, bad);
        printf("TileBatch %d: UTM in %llu, issued %llu, merged %llu, would-merge %llu, redundant %llu\n", mode,
               after[mode].utmCalls - before[mode].utmCalls, after[mode].batchOut - before[mode].batchOut,
               after[mode].batchMerged - before[mode].batchMerged, after[mode].wouldMerge - before[mode].wouldMerge,
               after[mode].redundantRegions - before[mode].redundantRegions);
    }
    CHECK(res[0] == res[1] && res[0] == res[2], "read-back differs between TileBatch modes");
    const uint64_t in1 = after[1].utmCalls - before[1].utmCalls, out1 = after[1].batchOut - before[1].batchOut;
    CHECK(in1 == 14, "expected 14 UTM calls in the sequence, got %llu", in1);
    CHECK(out1 < in1 && out1 > 0, "TileBatch 1 did not merge (in %llu out %llu)", in1, out1);
    CHECK(after[1].batchMerged - before[1].batchMerged == in1 - out1, "merged != in - out");
    CHECK(after[2].wouldMerge - before[2].wouldMerge == after[1].batchMerged - before[1].batchMerged,
          "dry run would-merge (%llu) != real merges (%llu)", after[2].wouldMerge - before[2].wouldMerge,
          after[1].batchMerged - before[1].batchMerged);
    CHECK(after[0].signals - before[0].signals >= 1 && after[0].waits - before[0].waits >= 1, "Signal/Wait not counted");
    CHECK(after[0].ctmCalls - before[0].ctmCalls == 1, "CTM not counted");

    // a held call is issued by OnFrame (the Present safety net)
    tilestats::SetBatchModeForTest(1);
    ID3D12Resource* T = MakeReserved();
    const auto b0 = tilestats::Totals();
    MapTiles(T, {0}, heap1, {0});
    const auto b1 = tilestats::Totals();
    tilestats::OnFrame();
    const auto b2 = tilestats::Totals();
    CHECK(b1.batchOut == b0.batchOut && b2.batchOut == b0.batchOut + 1, "OnFrame did not issue the held call");
    WaitIdle(qCopy);
    T->Release();
    tilestats::SetBatchModeForTest(0);
    tilestats::LogLine();

    if (debugLayer) {
        ID3D12InfoQueue* iq = nullptr;
        if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&iq)))) {
            UINT64 n = iq->GetNumStoredMessages(), errs = 0;
            for (UINT64 i = 0; i < n; ++i) {
                SIZE_T len = 0;
                iq->GetMessage(i, nullptr, &len);
                std::vector<char> buf(len);
                auto* msg = (D3D12_MESSAGE*)buf.data();
                iq->GetMessage(i, msg, &len);
                if (msg->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) {
                    if (errs++ < 5) printf("  debug layer: %s\n", msg->pDescription);
                }
            }
            CHECK(errs == 0, "%llu debug-layer errors", errs);
            iq->Release();
        }
    }
    CHECK(dev->GetDeviceRemovedReason() == S_OK, "device removed");
    printf(g_fail ? "TILESTATS HARNESS: %d FAILURES\n" : "TILESTATS HARNESS: ALL OK\n", g_fail);
    return g_fail ? 1 : 0;
}
