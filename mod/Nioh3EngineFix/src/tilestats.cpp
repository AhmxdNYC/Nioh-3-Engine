// tilestats.cpp - TS: D3D12 tile-mapping / queue-sync census + optional UpdateTileMappings merge (see tilestats.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <intrin.h>

#include <d3d12.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "tilestats.h"

#pragma intrinsic(_ReturnAddress)

namespace tilestats {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define TLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

int g_stats = 0;          // [Profiler] TileStats 0/1/2
volatile LONG g_batch = 0;  // [GpuOpt] TileBatch 0/1/2
bool g_installed = false;
int64_t g_qpcFreq = 1, g_qpcStart = 0;

// ---- vtable slots (ID3D12CommandQueue) -------------------------------------------------------------------------------
constexpr int kSlotUtm = 8, kSlotCtm = 9, kSlotEcl = 10, kSlotSignal = 14, kSlotWait = 15;
using UtmFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, ID3D12Resource*, UINT, const D3D12_TILED_RESOURCE_COORDINATE*,
                                       const D3D12_TILE_REGION_SIZE*, ID3D12Heap*, UINT, const D3D12_TILE_RANGE_FLAGS*,
                                       const UINT*, const UINT*, D3D12_TILE_MAPPING_FLAGS);
using CtmFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, ID3D12Resource*, const D3D12_TILED_RESOURCE_COORDINATE*,
                                       ID3D12Resource*, const D3D12_TILED_RESOURCE_COORDINATE*,
                                       const D3D12_TILE_REGION_SIZE*, D3D12_TILE_MAPPING_FLAGS);
using EclFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
using SyncFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, ID3D12Fence*, UINT64);
UtmFn g_origUtm = nullptr;
CtmFn g_origCtm = nullptr;
EclFn g_origEcl = nullptr;
SyncFn g_origSignal = nullptr, g_origWait = nullptr;

thread_local int t_depth = 0;  // re-entrancy guard: the runtime calling its own vtable passes straight through

int64_t Qpc() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

// ---- counters ---------------------------------------------------------------------------------------------------------
// bucket 0 DIRECT, 1 COMPUTE, 2 COPY, 3 other queue type, 4 = calls from other modules (any queue type)
enum { B_DIRECT, B_COMPUTE, B_COPY, B_OTHERTYPE, B_OTHERMOD, B_N };
enum {
    C_UTM, C_REG, C_RNG, C_TILES, C_UTM_TICKS, C_CTM, C_ECL, C_ECL_LISTS, C_SIG, C_WAIT, C_N
};
volatile LONG64 g_c[B_N][C_N] = {};
volatile LONG64 g_utmMaxTicks = 0;  // max per issued call, reset per log window
volatile LONG64 g_frames = 0;
volatile LONG64 g_batchIn = 0, g_batchOut = 0, g_batchMerged = 0, g_wouldMerge = 0, g_redundant = 0;
volatile LONG64 g_flushBy[6] = {};  // 0 Signal, 1 Wait, 2 ECL, 3 CTM, 4 UTM that can't merge, 5 Present/test
const char* const kFlushNames[6] = {"Signal", "Wait", "ECL", "CTM", "UTM", "Present"};

void Add(int b, int c, LONG64 v) { InterlockedAdd64(&g_c[b][c], v); }
void MaxTicks(LONG64 v) {
    LONG64 cur = g_utmMaxTicks;
    while (v > cur) {
        LONG64 prev = InterlockedCompareExchange64(&g_utmMaxTicks, v, cur);
        if (prev == cur) break;
        cur = prev;
    }
}

// ---- caller filter ----------------------------------------------------------------------------------------------------
uintptr_t g_exeLo = 0, g_exeHi = 0;
bool CallerIsGame(void* ra) {
    const uintptr_t a = (uintptr_t)ra;
    return a >= g_exeLo && a < g_exeHi;
}

// ---- per-queue state ----------------------------------------------------------------------------------------------------
struct Pending {
    bool active = false;
    ID3D12Resource* res = nullptr;
    ID3D12Heap* heap = nullptr;  // AddRef'd while held (TileBatch 1)
    bool heapUsed = false;       // some range maps heap pages (NONE / REUSE_SINGLE_TILE)
    D3D12_TILE_MAPPING_FLAGS flags = D3D12_TILE_MAPPING_FLAG_NONE;
    std::vector<D3D12_TILED_RESOURCE_COORDINATE> coords;
    std::vector<D3D12_TILE_REGION_SIZE> sizes;
    std::vector<D3D12_TILE_RANGE_FLAGS> rflags;
    std::vector<UINT> starts, counts;
    std::unordered_set<uint64_t> tiles;
    int calls = 0;
};
struct QEntry {
    ID3D12CommandQueue* volatile q = nullptr;
    int bucket = B_OTHERTYPE;
    SRWLOCK lock = SRWLOCK_INIT;
    Pending* p = nullptr;  // created on the first game UpdateTileMappings of this queue (TileBatch != 0)
};
constexpr int kMaxQ = 96;
QEntry g_q[kMaxQ];
volatile LONG g_qn = 0;
SRWLOCK g_qAdd = SRWLOCK_INIT;
volatile LONG g_held = 0;  // queues with an active held call (fast path: 0 -> nothing to issue)

QEntry* FindQ(ID3D12CommandQueue* q) {
    const LONG n = g_qn;
    for (LONG i = 0; i < n; ++i)
        if (g_q[i].q == q) return &g_q[i];
    AcquireSRWLockExclusive(&g_qAdd);
    QEntry* e = nullptr;
    for (LONG i = 0; i < g_qn; ++i)
        if (g_q[i].q == q) e = &g_q[i];
    if (!e && g_qn < kMaxQ) {
        e = &g_q[g_qn];
        const D3D12_COMMAND_LIST_TYPE t = q->GetDesc().Type;
        e->bucket = t == D3D12_COMMAND_LIST_TYPE_DIRECT ? B_DIRECT
                    : t == D3D12_COMMAND_LIST_TYPE_COMPUTE ? B_COMPUTE
                    : t == D3D12_COMMAND_LIST_TYPE_COPY  ? B_COPY
                                                          : B_OTHERTYPE;
        e->q = q;
        MemoryBarrier();
        InterlockedIncrement(&g_qn);
    }
    ReleaseSRWLockExclusive(&g_qAdd);
    return e;  // null only when the table is full (stats as "other type", no batching)
}

// ---- UpdateTileMappings normal form ----------------------------------------------------------------------------------
// A call qualifies for merging when every region is exactly one tile and its ranges cover exactly those tiles.
struct Norm {
    bool ok = false;
    bool heapUsed = false;
    UINT tiles = 0;
    std::vector<D3D12_TILE_RANGE_FLAGS> rflags;
    std::vector<UINT> starts, counts;
    std::vector<uint64_t> keys;
};
bool TileKey(const D3D12_TILED_RESOURCE_COORDINATE& c, uint64_t* k) {
    if (c.X >= 0x10000 || c.Y >= 0x10000 || c.Z >= 0x10000 || c.Subresource >= 0x10000) return false;
    *k = (uint64_t)c.Subresource << 48 | (uint64_t)c.Z << 32 | (uint64_t)c.Y << 16 | c.X;
    return true;
}
void Normalize(UINT nReg, const D3D12_TILED_RESOURCE_COORDINATE* coords, const D3D12_TILE_REGION_SIZE* sizes,
               ID3D12Heap* heap, UINT nRng, const D3D12_TILE_RANGE_FLAGS* rf, const UINT* starts, const UINT* counts,
               Norm& n) {
    n.ok = false;
    if (!nReg || !coords || !sizes || !nRng || nReg > 4096 || nRng > 4096) return;
    n.keys.resize(nReg);
    for (UINT i = 0; i < nReg; ++i) {
        const D3D12_TILE_REGION_SIZE& s = sizes[i];
        if (s.NumTiles != 1) return;
        if (s.UseBox && (s.Width != 1 || s.Height != 1 || s.Depth != 1)) return;
        if (!TileKey(coords[i], &n.keys[i])) return;
    }
    n.rflags.resize(nRng);
    n.starts.resize(nRng);
    n.counts.resize(nRng);
    uint64_t sum = 0;
    n.heapUsed = false;
    for (UINT i = 0; i < nRng; ++i) {
        const D3D12_TILE_RANGE_FLAGS f = rf ? rf[i] : D3D12_TILE_RANGE_FLAG_NONE;
        if (f != D3D12_TILE_RANGE_FLAG_NONE && f != D3D12_TILE_RANGE_FLAG_NULL && f != D3D12_TILE_RANGE_FLAG_SKIP &&
            f != D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE)
            return;
        const bool usesHeap = f == D3D12_TILE_RANGE_FLAG_NONE || f == D3D12_TILE_RANGE_FLAG_REUSE_SINGLE_TILE;
        if (usesHeap && (!heap || !starts)) return;
        n.heapUsed |= usesHeap;
        n.rflags[i] = f;
        n.starts[i] = starts ? starts[i] : 0;
        UINT c;
        if (counts) c = counts[i];
        else if (nRng == 1) c = nReg;  // a single range with no counts covers every region tile
        else return;
        n.counts[i] = c;
        sum += c;
    }
    if (sum != nReg) return;  // ranges must cover the regions exactly (else tiles of the next call would shift)
    n.tiles = nReg;
    n.ok = true;
}

// ---- redundant-region estimate (TileStats 2) ------------------------------------------------------------------------
SRWLOCK g_shadowLock = SRWLOCK_INIT;
std::unordered_map<ID3D12Resource*, std::unordered_map<uint64_t, uint64_t>>* g_shadow = nullptr;
size_t g_shadowN = 0;
constexpr uint64_t kNullTarget = ~0ull;
void ShadowUpdate(ID3D12Resource* res, ID3D12Heap* heap, const Norm* n) {
    AcquireSRWLockExclusive(&g_shadowLock);
    if (!g_shadow) g_shadow = new std::unordered_map<ID3D12Resource*, std::unordered_map<uint64_t, uint64_t>>();
    if (!n || !n->ok) {
        auto it = g_shadow->find(res);
        if (it != g_shadow->end()) {
            g_shadowN -= it->second.size();
            g_shadow->erase(it);
        }
    } else {
        if (g_shadowN > 2000000) {
            g_shadow->clear();
            g_shadowN = 0;
        }
        auto& m = (*g_shadow)[res];
        UINT r = 0, inR = 0;
        for (size_t i = 0; i < n->keys.size(); ++i) {
            while (r < n->counts.size() && inR >= n->counts[r]) { ++r; inR = 0; }
            if (r >= n->counts.size()) break;
            const D3D12_TILE_RANGE_FLAGS f = n->rflags[r];
            uint64_t tgt;
            if (f == D3D12_TILE_RANGE_FLAG_SKIP) { ++inR; continue; }
            if (f == D3D12_TILE_RANGE_FLAG_NULL) tgt = kNullTarget;
            else tgt = ((uint64_t)(uintptr_t)heap << 20) ^ (uint64_t)(n->starts[r] + (f == D3D12_TILE_RANGE_FLAG_NONE ? inR : 0));
            ++inR;
            auto ins = m.emplace(n->keys[i], tgt);
            if (ins.second) ++g_shadowN;
            else if (ins.first->second == tgt) InterlockedIncrement64(&g_redundant);
            else ins.first->second = tgt;
        }
    }
    ReleaseSRWLockExclusive(&g_shadowLock);
}

// ---- trace (TileStats 2) ----------------------------------------------------------------------------------------------
struct TraceRec { int64_t qpc; int bucket; UINT regions, ranges, tiles; int64_t ticks; void* res; };
constexpr int kTraceN = 256;
TraceRec g_trace[kTraceN];
volatile LONG g_traceN = 0;
volatile LONG g_traceDumped = 0;
void TraceAdd(int64_t t0, int bucket, UINT reg, UINT rng, UINT tiles, int64_t ticks, void* res) {
    if (g_stats < 2 || t0 - g_qpcStart < 60 * g_qpcFreq) return;
    const LONG i = InterlockedIncrement(&g_traceN) - 1;
    if (i < kTraceN) g_trace[i] = TraceRec{t0, bucket, reg, rng, tiles, ticks, res};
}

// ---- issuing --------------------------------------------------------------------------------------------------------
UINT SumCounts(UINT nReg, UINT nRng, const UINT* counts) {
    if (!counts) return nRng == 1 ? nReg : nRng;
    uint64_t s = 0;
    for (UINT i = 0; i < nRng; ++i) s += counts[i];
    return (UINT)(s > 0xFFFFFFFFull ? 0xFFFFFFFFull : s);
}

void CallUtm(int bucket, ID3D12CommandQueue* q, ID3D12Resource* res, UINT nReg, const D3D12_TILED_RESOURCE_COORDINATE* c,
             const D3D12_TILE_REGION_SIZE* s, ID3D12Heap* heap, UINT nRng, const D3D12_TILE_RANGE_FLAGS* rf,
             const UINT* st, const UINT* cnt, D3D12_TILE_MAPPING_FLAGS fl) {
    const int64_t t0 = Qpc();
    ++t_depth;
    g_origUtm(q, res, nReg, c, s, heap, nRng, rf, st, cnt, fl);
    --t_depth;
    const int64_t dt = Qpc() - t0;
    Add(bucket, C_UTM_TICKS, dt);
    MaxTicks(dt);
    TraceAdd(t0, bucket, nReg, nRng, SumCounts(nReg, nRng, cnt), dt, res);
}

void ReleaseHeld(Pending& p, ID3D12CommandQueue* q) {
    if (p.res) p.res->Release();
    if (p.heap) p.heap->Release();
    if (q) q->Release();
    p.res = nullptr;
    p.heap = nullptr;
}

// Issues the held call of e (lock held by the caller). why = index into g_flushBy.
void IssueHeldLocked(QEntry* e, int why) {
    Pending* p = e->p;
    if (!p || !p->active) return;
    if (g_batch == 1) {
        CallUtm(e->bucket, e->q, p->res, (UINT)p->coords.size(), p->coords.data(), p->sizes.data(), p->heap,
                (UINT)p->counts.size(), p->rflags.data(), p->starts.data(), p->counts.data(), p->flags);
        InterlockedIncrement64(&g_batchOut);
        if (p->calls > 1) InterlockedAdd64(&g_batchMerged, p->calls - 1);
        InterlockedIncrement64(&g_flushBy[why]);
        ReleaseHeld(*p, e->q);
        InterlockedDecrement(&g_held);
    }
    p->active = false;
    p->calls = 0;
    p->coords.clear();
    p->sizes.clear();
    p->rflags.clear();
    p->starts.clear();
    p->counts.clear();
    p->tiles.clear();
}

void IssueHeld(QEntry* e, int why) {
    if (!e || !e->p) return;
    AcquireSRWLockExclusive(&e->lock);
    IssueHeldLocked(e, why);
    ReleaseSRWLockExclusive(&e->lock);
}

// Before any non-UpdateTileMappings operation on q: issue its held call (TileBatch 1) / end its dry-run run (2).
void BeforeQueueOp(QEntry* e, int why) {
    if (!e || !e->p || !g_batch) return;
    if (g_batch == 1 && !g_held) return;
    IssueHeld(e, why);
}

void IssueAllHeld(int why) {
    if (!g_held && g_batch != 2) return;
    const LONG n = g_qn;
    for (LONG i = 0; i < n; ++i) IssueHeld(&g_q[i], why);
}

// ---- hooks ----------------------------------------------------------------------------------------------------------
void STDMETHODCALLTYPE HkUtm(ID3D12CommandQueue* q, ID3D12Resource* res, UINT nReg, const D3D12_TILED_RESOURCE_COORDINATE* c,
                             const D3D12_TILE_REGION_SIZE* s, ID3D12Heap* heap, UINT nRng, const D3D12_TILE_RANGE_FLAGS* rf,
                             const UINT* st, const UINT* cnt, D3D12_TILE_MAPPING_FLAGS fl) {
    if (t_depth) { g_origUtm(q, res, nReg, c, s, heap, nRng, rf, st, cnt, fl); return; }
    const bool game = CallerIsGame(_ReturnAddress());
    QEntry* e = FindQ(q);
    const int bucket = !game ? B_OTHERMOD : e ? e->bucket : B_OTHERTYPE;
    Add(bucket, C_UTM, 1);
    Add(bucket, C_REG, nReg);
    Add(bucket, C_RNG, nRng);
    Add(bucket, C_TILES, SumCounts(nReg, nRng, cnt));

    const LONG mode = g_batch;
    Norm n;
    if (g_stats >= 2 || (mode && game && e)) Normalize(nReg, c, s, heap, nRng, rf, st, cnt, n);
    if (g_stats >= 2 && res) ShadowUpdate(res, heap, &n);

    if (!mode || !game || !e || !res) {
        if (e && mode) BeforeQueueOp(e, 4);  // a foreign / unusable call: issue ours first
        CallUtm(bucket, q, res, nReg, c, s, heap, nRng, rf, st, cnt, fl);
        return;
    }
    AcquireSRWLockExclusive(&e->lock);
    if (!e->p) e->p = new Pending();
    Pending& p = *e->p;
    InterlockedIncrement64(&g_batchIn);
    bool merge = false;
    if (p.active && n.ok && p.res == res && p.flags == fl && p.coords.size() + nReg <= 4096) {
        const bool heapOk = !(p.heapUsed && n.heapUsed) || p.heap == heap;
        bool overlap = false;
        if (heapOk)
            for (uint64_t k : n.keys)
                if (p.tiles.count(k)) { overlap = true; break; }
        merge = heapOk && !overlap;
    }
    if (mode == 2) {  // dry run: count, keep the run's footprint, change nothing
        if (merge) InterlockedIncrement64(&g_wouldMerge);
        else { IssueHeldLocked(e, 4); }
        if (n.ok) {
            if (!p.active) { p.active = true; p.res = res; p.flags = fl; p.heap = heap; p.heapUsed = false; }
            if (n.heapUsed) { p.heap = heap; p.heapUsed = true; }
            p.tiles.insert(n.keys.begin(), n.keys.end());
            p.coords.resize(p.coords.size() + nReg);  // only the size is used (cap)
            ++p.calls;
        }
        ReleaseSRWLockExclusive(&e->lock);
        CallUtm(bucket, q, res, nReg, c, s, heap, nRng, rf, st, cnt, fl);
        return;
    }
    if (!merge) IssueHeldLocked(e, 4);
    if (!n.ok) {  // not mergeable: pass through right away (the held call, if any, went first)
        ReleaseSRWLockExclusive(&e->lock);
        CallUtm(bucket, q, res, nReg, c, s, heap, nRng, rf, st, cnt, fl);
        InterlockedIncrement64(&g_batchOut);
        return;
    }
    if (!p.active) {
        p.active = true;
        p.res = res;
        res->AddRef();
        q->AddRef();
        p.flags = fl;
        p.heap = nullptr;
        p.heapUsed = false;
        InterlockedIncrement(&g_held);
    }
    if (heap && (n.heapUsed || !p.heap) && heap != p.heap) {  // the merged call's heap: the one some range uses
        if (p.heap) p.heap->Release();
        p.heap = heap;
        heap->AddRef();
    }
    p.heapUsed |= n.heapUsed;
    p.coords.insert(p.coords.end(), c, c + nReg);
    p.sizes.insert(p.sizes.end(), s, s + nReg);
    p.rflags.insert(p.rflags.end(), n.rflags.begin(), n.rflags.end());
    p.starts.insert(p.starts.end(), n.starts.begin(), n.starts.end());
    p.counts.insert(p.counts.end(), n.counts.begin(), n.counts.end());
    p.tiles.insert(n.keys.begin(), n.keys.end());
    ++p.calls;
    ReleaseSRWLockExclusive(&e->lock);
}

void STDMETHODCALLTYPE HkCtm(ID3D12CommandQueue* q, ID3D12Resource* dst, const D3D12_TILED_RESOURCE_COORDINATE* dc,
                             ID3D12Resource* src, const D3D12_TILED_RESOURCE_COORDINATE* sc,
                             const D3D12_TILE_REGION_SIZE* sz, D3D12_TILE_MAPPING_FLAGS fl) {
    if (t_depth) { g_origCtm(q, dst, dc, src, sc, sz, fl); return; }
    const bool game = CallerIsGame(_ReturnAddress());
    QEntry* e = FindQ(q);
    Add(!game ? B_OTHERMOD : e ? e->bucket : B_OTHERTYPE, C_CTM, 1);
    if (g_batch) BeforeQueueOp(e, 3);
    if (g_stats >= 2 && dst) ShadowUpdate(dst, nullptr, nullptr);
    ++t_depth;
    g_origCtm(q, dst, dc, src, sc, sz, fl);
    --t_depth;
}

HRESULT STDMETHODCALLTYPE HkSignal(ID3D12CommandQueue* q, ID3D12Fence* f, UINT64 v) {
    if (t_depth) return g_origSignal(q, f, v);
    const bool game = CallerIsGame(_ReturnAddress());
    QEntry* e = (game || g_held) ? FindQ(q) : nullptr;
    Add(!game ? B_OTHERMOD : e ? e->bucket : B_OTHERTYPE, C_SIG, 1);
    if (g_batch) BeforeQueueOp(e, 0);
    ++t_depth;
    const HRESULT hr = g_origSignal(q, f, v);
    --t_depth;
    return hr;
}

HRESULT STDMETHODCALLTYPE HkWait(ID3D12CommandQueue* q, ID3D12Fence* f, UINT64 v) {
    if (t_depth) return g_origWait(q, f, v);
    const bool game = CallerIsGame(_ReturnAddress());
    QEntry* e = (game || g_held) ? FindQ(q) : nullptr;
    Add(!game ? B_OTHERMOD : e ? e->bucket : B_OTHERTYPE, C_WAIT, 1);
    if (g_batch) BeforeQueueOp(e, 1);
    ++t_depth;
    const HRESULT hr = g_origWait(q, f, v);
    --t_depth;
    return hr;
}

void EclCommon(ID3D12CommandQueue* q, UINT n, bool game) {
    QEntry* e = (game || g_held) ? FindQ(q) : nullptr;
    const int b = !game ? B_OTHERMOD : e ? e->bucket : B_OTHERTYPE;
    Add(b, C_ECL, 1);
    Add(b, C_ECL_LISTS, n);
    if (g_batch) BeforeQueueOp(e, 2);
}

void STDMETHODCALLTYPE HkEcl(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* lists) {
    if (!t_depth) EclCommon(q, n, CallerIsGame(_ReturnAddress()));
    g_origEcl(q, n, lists);
}

bool PatchSlot(void** slot, void* hook, void** orig, const char* name) {
    if (*slot == hook) return true;
    DWORD old = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
        TLOG("TS: VirtualProtect(queue vtable %s) failed (%lu)", name, GetLastError());
        return false;
    }
    *orig = *slot;  // published before the slot so the hook never sees a null original
    MemoryBarrier();
    InterlockedExchangePointer(slot, hook);
    DWORD t = 0;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return true;
}

// window bookkeeping for LogLine
LONG64 g_last[B_N][C_N] = {};
LONG64 g_lastFrames = 0, g_lastIn = 0, g_lastOut = 0, g_lastMerged = 0, g_lastWould = 0, g_lastRed = 0;
int64_t g_lastQpc = 0;

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpcFreq = f.QuadPart ? f.QuadPart : 1;
    g_qpcStart = g_lastQpc = Qpc();
    if (info.iniPath) {
        g_stats = (int)GetPrivateProfileIntW(L"Profiler", L"TileStats", 0, info.iniPath);
        g_batch = (LONG)GetPrivateProfileIntW(L"GpuOpt", L"TileBatch", 0, info.iniPath);
    }
    if (g_stats < 0 || g_stats > 2) g_stats = 1;
    if (g_batch < 0 || g_batch > 2) g_batch = 0;
    if (HMODULE m = GetModuleHandleW(nullptr)) {
        auto* dos = (IMAGE_DOS_HEADER*)m;
        auto* nt = (IMAGE_NT_HEADERS*)((uint8_t*)m + dos->e_lfanew);
        g_exeLo = (uintptr_t)m;
        g_exeHi = g_exeLo + nt->OptionalHeader.SizeOfImage;
    }
    if (WantsHooks()) TLOG("TS: TileStats %d, TileBatch %ld - queue hooks go in with the overlay's", g_stats, g_batch);
    return WantsHooks();
}

bool WantsHooks() { return g_stats != 0 || g_batch != 0; }

bool InstallOnQueueVtable(void** qvt, bool eclViaOsd) {
    if (!WantsHooks() || !qvt) return false;
    if (g_installed) return true;
    bool ok = PatchSlot(&qvt[kSlotUtm], (void*)&HkUtm, (void**)&g_origUtm, "UpdateTileMappings") &&
              PatchSlot(&qvt[kSlotCtm], (void*)&HkCtm, (void**)&g_origCtm, "CopyTileMappings") &&
              PatchSlot(&qvt[kSlotSignal], (void*)&HkSignal, (void**)&g_origSignal, "Signal") &&
              PatchSlot(&qvt[kSlotWait], (void*)&HkWait, (void**)&g_origWait, "Wait");
    if (ok && !eclViaOsd) ok = PatchSlot(&qvt[kSlotEcl], (void*)&HkEcl, (void**)&g_origEcl, "ExecuteCommandLists");
    if (!ok) {
        TLOG("TS: queue hooks NOT complete - TileBatch forced off (counters may be partial)");
        g_batch = 0;
    }
    g_installed = true;
    TLOG("TS: queue vtable %p hooked (UTM/CTM/Signal/Wait%s), TileStats %d, TileBatch %ld", (void*)qvt,
         eclViaOsd ? ", ECL via the OSD" : ", ECL", g_stats, g_batch);
    return ok;
}

void OnEcl(ID3D12CommandQueue* q, unsigned numLists, bool gameCaller) {
    if (!g_installed || t_depth) return;
    EclCommon(q, numLists, gameCaller);
}

void OnFrame() {
    if (!g_installed) return;
    InterlockedIncrement64(&g_frames);
    if (g_batch == 1 && g_held) IssueAllHeld(5);
}

void SetBatchModeForTest(int mode) {
    IssueAllHeld(5);
    g_batch = mode;
}

Counters Totals() {
    Counters c{};
    for (int b = 0; b < B_N; ++b) {
        if (b == B_OTHERMOD) continue;
        c.utmCalls += g_c[b][C_UTM];
        c.utmRegions += g_c[b][C_REG];
        c.utmRanges += g_c[b][C_RNG];
        c.utmTiles += g_c[b][C_TILES];
        c.ctmCalls += g_c[b][C_CTM];
        c.signals += g_c[b][C_SIG];
        c.waits += g_c[b][C_WAIT];
        c.ecls += g_c[b][C_ECL];
    }
    c.batchIn = g_batchIn;
    c.batchOut = g_batchOut;
    c.batchMerged = g_batchMerged;
    c.wouldMerge = g_wouldMerge;
    c.redundantRegions = g_redundant;
    return c;
}

void LogLine() {
    if (!g_installed || !g_stats) return;
    const int64_t now = Qpc();
    const double sec = (double)(now - g_lastQpc) / (double)g_qpcFreq;
    g_lastQpc = now;
    LONG64 d[B_N][C_N];
    for (int b = 0; b < B_N; ++b)
        for (int c = 0; c < C_N; ++c) {
            const LONG64 v = g_c[b][c];
            d[b][c] = v - g_last[b][c];
            g_last[b][c] = v;
        }
    const LONG64 fr = g_frames - g_lastFrames;
    g_lastFrames = g_frames;
    const double div = fr > 0 ? (double)fr : (sec > 0 ? sec : 1.0);
    const char* per = fr > 0 ? "/f" : "/s";
    const double us = 1e6 / (double)g_qpcFreq;
    LONG64 utm = 0, reg = 0, rng = 0, tiles = 0, ticks = 0, ctm = 0;
    for (int b = 0; b < B_OTHERMOD; ++b) {
        utm += d[b][C_UTM]; reg += d[b][C_REG]; rng += d[b][C_RNG]; tiles += d[b][C_TILES];
        ticks += d[b][C_UTM_TICKS]; ctm += d[b][C_CTM];
    }
    const LONG64 issuedTicks = ticks + d[B_OTHERMOD][C_UTM_TICKS];
    const LONG64 maxT = InterlockedExchange64(&g_utmMaxTicks, 0);
    char q[640];
    snprintf(q, sizeof q,
             "ECL%s D %.1f C %.1f Cp %.1f | Signal%s D %.1f C %.1f Cp %.1f | Wait%s D %.1f C %.1f Cp %.1f | other modules%s "
             "UTM %.2f ECL %.1f Signal %.1f Wait %.1f",
             per, d[B_DIRECT][C_ECL] / div, d[B_COMPUTE][C_ECL] / div, d[B_COPY][C_ECL] / div, per,
             d[B_DIRECT][C_SIG] / div, d[B_COMPUTE][C_SIG] / div, d[B_COPY][C_SIG] / div, per,
             d[B_DIRECT][C_WAIT] / div, d[B_COMPUTE][C_WAIT] / div, d[B_COPY][C_WAIT] / div, per,
             d[B_OTHERMOD][C_UTM] / div, d[B_OTHERMOD][C_ECL] / div, d[B_OTHERMOD][C_SIG] / div,
             d[B_OTHERMOD][C_WAIT] / div);
    char bt[320] = "";
    if (g_batch) {
        const LONG64 in = g_batchIn - g_lastIn, out = g_batchOut - g_lastOut, mg = g_batchMerged - g_lastMerged,
                     wm = g_wouldMerge - g_lastWould;
        g_lastIn = g_batchIn; g_lastOut = g_batchOut; g_lastMerged = g_batchMerged; g_lastWould = g_wouldMerge;
        snprintf(bt, sizeof bt,
                 " | TileBatch %ld: in %lld out %lld merged %lld would-merge %lld; held calls issued by Signal %lld Wait "
                 "%lld ECL %lld CTM %lld UTM %lld Present %lld (since start)",
                 g_batch, in, out, mg, wm, g_flushBy[0], g_flushBy[1], g_flushBy[2], g_flushBy[3], g_flushBy[4],
                 g_flushBy[5]);
    }
    char rd[96] = "";
    if (g_stats >= 2) {
        const LONG64 r = g_redundant - g_lastRed;
        g_lastRed = g_redundant;
        snprintf(rd, sizeof rd, " | redundant regions %lld (est.)", r);
    }
    TLOG("stats tiles: %.1f s, %lld frames | UTM %.2f calls%s (D %.2f C %.2f Cp %.2f), %.1f regions, %.1f ranges, %.1f "
         "tiles%s, CPU %.1f us%s (%.1f us/call, max %.0f us) | CTM %.3f%s | %s%s%s",
         sec, fr, utm / div, per, d[B_DIRECT][C_UTM] / div, d[B_COMPUTE][C_UTM] / div, d[B_COPY][C_UTM] / div,
         reg / div, rng / div, tiles / div, per, issuedTicks * us / div, per,
         utm ? ticks * us / (double)utm : 0.0, maxT * us, ctm / div, per, q, bt, rd);
    if (g_stats >= 2 && g_traceN >= kTraceN && !InterlockedExchange(&g_traceDumped, 1)) {
        static const char* const kB[] = {"DIRECT", "COMPUTE", "COPY", "other", "foreign"};
        for (int i = 0; i < kTraceN; ++i) {
            const TraceRec& t = g_trace[i];
            TLOG("tiles trace %d: qpc %lld q %s regions %u ranges %u tiles %u us %.1f res %p", i, t.qpc, kB[t.bucket],
                 t.regions, t.ranges, t.tiles, t.ticks * us, t.res);
        }
    }
}

const char* StatusLine(char* buf, unsigned n) {
    if (!buf || !n) return "";
    buf[0] = 0;
    if (!g_installed || !g_stats) return buf;
    const Counters c = Totals();
    const double fr = g_frames > 0 ? (double)g_frames : 1.0;
    snprintf(buf, n, "TS: UTM %.2f/f %.1f tiles/f, Wait %.1f/f", c.utmCalls / fr, c.utmTiles / fr, c.waits / fr);
    return buf;
}

}  // namespace tilestats
