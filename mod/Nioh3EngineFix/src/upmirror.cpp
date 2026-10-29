// upmirror.cpp - CM: per-draw constant buffers mirrored from the system-memory UPLOAD ring into VRAM (see upmirror.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <intrin.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "upmirror.h"

#pragma intrinsic(_ReturnAddress)

namespace upmirror {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define ULOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

int g_mode = 0;           // 0 off, 1 auto, 2 force
bool g_allSites = false;  // CbvMirrorAllSites: every CBV in the ring, not only the per-draw recorder site
int g_sites = 1;          // 1.7.55 CbvMirrorSites (bitmask, ignored with AllSites): 1 per-draw recorder 118F89, 2 compute sites
uint64_t g_ringBytes = 128ull << 20;
uintptr_t g_base = 0;
constexpr uint32_t kRvaRecorderCbv = 0x118F89;  // v2.02: return address of the per-draw CreateConstantBufferView (118E1E)
// 1.7.55 (notes/rdna2-meshlet-slowdown.md): the compute dispatch constants. Both sites memcpy (B94250) the constants into
// a fresh ring slot right before CreateConstantBufferView (device vtbl+88h), the same "bytes final at create" rule as 118F89.
constexpr uint32_t kRvaComputeCbvTable = 0x495B2E;  // CmdListUploadComputeCbvTable 4959EC <- SynFlushComputeState 58BE90 (~95 %)
constexpr uint32_t kRvaComputeCbvSyn = 0x117F20;    // SynFlushComputeDescriptors 117DE8 <- SynFlushComputeStateEx 119C08
volatile LONG g_active = 0;  // the redirect runs (mode 2, or mode 1 after the auto decision)
volatile LONG g_failed = 0;
bool g_wine = false;

// the engine's UPLOAD ring heap (the biggest UPLOAD heap; 700 MB in v2.02) and an alias buffer over all of it
ID3D12Heap* g_upHeap = nullptr;
ID3D12Resource* g_alias = nullptr;
uint64_t g_upBase = 0, g_upSize = 0;
uint64_t g_sysHeapBytes = 0;  // all UPLOAD / READBACK heap bytes seen (auto decision)

// the VRAM mirror ring
ID3D12Resource* g_mirror = nullptr;
uint64_t g_mirrorVa = 0;
SRWLOCK g_lock = SRWLOCK_INIT;
uint64_t g_head = 0;          // monotonically increasing byte position
uint64_t g_oldest = 0;        // start of the oldest batch the GPU may still read
struct Batch { ID3D12Fence* fence; uint64_t value; uint64_t end; int64_t qpc; };
int64_t g_qpf = 0;
std::vector<Batch> g_inFlight;
struct Copy { uint64_t src, dst, size, abs = 0; };   // abs: absolute ring position (slot reuse check)
std::vector<Copy> g_pending;
std::unordered_map<uint64_t, uint64_t> g_dedupe;   // (src VA, size) -> mirror offset, this epoch

// our COPY queue (DMA): copies never queue behind the game's graphics work; every game DIRECT / COMPUTE queue waits on
// the copy fence before its lists (cheap: the copy queue is idle apart from these few hundred KB per submission)
constexpr int kAllocs = 64;
ID3D12CommandQueue* g_cq = nullptr;
ID3D12CommandAllocator* g_ca[kAllocs] = {};
uint64_t g_caFence[kAllocs] = {};
int g_caNext = 0;
ID3D12GraphicsCommandList* g_cl = nullptr;
ID3D12Fence* g_cf = nullptr;
uint64_t g_cv = 0;
struct GameQ {
    ID3D12CommandQueue* q;
    uint64_t waited;
    // 1.7.54 inline mode: per game queue, our prologue list of the queue's type and a fence signalled after the game's lists
    D3D12_COMMAND_LIST_TYPE type;
    ID3D12CommandAllocator* alloc[kAllocs];
    uint64_t allocFence[kAllocs];
    int next;
    ID3D12GraphicsCommandList* list;
    ID3D12Fence* fence;
    uint64_t value;
    uint64_t cursor;   // next g_log sequence number this queue has not copied yet
    bool ready, tried;
};
int g_queueMode = 0;        // [GpuOpt] CbvMirrorQueue: 0 inline prologue on the game's own queue (1.7.54), 1 own COPY queue (1.7.52)
bool g_computeCopy = true;  // [GpuOpt] CbvMirrorComputeCopy: COMPUTE queues copy the ranges their lists may read too
std::vector<Copy> g_clog;    // every redirected range (inline mode), sequence g_logSeq0 + index
uint64_t g_logSeq0 = 0;
GameQ g_gq[16];
int g_ngq = 0;
bool g_copyReady = false, g_copyTried = false;
ID3D12Device* g_dev = nullptr;

// counters
volatile LONG64 c_redir = 0, c_dedup = 0, c_full = 0, c_outside = 0, c_site = 0, c_flush = 0, c_copies = 0, c_bytes = 0,
                c_waits = 0, c_cpuWaits = 0;

bool IsWine() {
    HMODULE n = GetModuleHandleW(L"ntdll.dll");
    return n && GetProcAddress(n, "wine_get_version");
}

bool CreateMirror(ID3D12Device* dev) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = g_ringBytes;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                            IID_PPV_ARGS(&g_mirror))) || !g_mirror)
        return false;
    g_mirror->SetName(L"Nioh3EngineFix CbvMirror ring");
    g_mirrorVa = g_mirror->GetGPUVirtualAddress();
    return true;
}

// Retire mirror space (caller holds g_lock). Our fence only says the COPY ran; the game's lists that read a batch run
// right after it or a few submissions later. A batch is free once a batch flushed >= 80 ms later has completed (by then
// every list recorded with those descriptors, one frame, has executed at >= 12.5 fps), or 250 ms after its own copy
// completed when nothing newer was flushed.
void RetireLocked() {
    const size_t n = g_inFlight.size();
    if (!n) {
        if (g_pending.empty()) g_oldest = g_head;
        return;
    }
    std::vector<char> done(n);
    for (size_t j = 0; j < n; ++j) done[j] = g_inFlight[j].fence->GetCompletedValue() >= g_inFlight[j].value;
    const int64_t win = g_qpf * 80 / 1000, idle = g_qpf / 4;
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    size_t i = 0;
    for (; i < n; ++i) {
        // free when a copy flushed >= 80 ms later completed, or (no later traffic, e.g. a loading screen) when this copy
        // completed >= 250 ms ago: its readers were submitted right after it
        bool ok = done[i] && now.QuadPart - g_inFlight[i].qpc >= idle;
        for (size_t j = i + 1; j < n && !ok; ++j) ok = done[j] && g_inFlight[j].qpc - g_inFlight[i].qpc >= win;
        if (!ok) break;
        g_oldest = g_inFlight[i].end;
    }
    if (i) g_inFlight.erase(g_inFlight.begin(), g_inFlight.begin() + (ptrdiff_t)i);
}

// mirror offset for size bytes (256-aligned), or UINT64_MAX when the ring is full (caller holds g_lock)
uint64_t AllocLocked(uint64_t size) {
    size = (size + 255) & ~255ull;
    uint64_t pos = g_head % g_ringBytes;
    uint64_t start = g_head;
    if (pos + size > g_ringBytes) start += g_ringBytes - pos;   // no wrap inside one CBV
    if (start + size - g_oldest > g_ringBytes) {
        RetireLocked();
        if (start + size - g_oldest > g_ringBytes) return UINT64_MAX;
    }
    g_head = start + size;
    return start % g_ringBytes;
}

bool CreateCopyQueue() {
    g_copyTried = true;
    D3D12_COMMAND_QUEUE_DESC qd{};
    qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    if (FAILED(g_dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_cq)))) return false;
    for (auto& a : g_ca)
        if (FAILED(g_dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&a)))) return false;
    if (FAILED(g_dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, g_ca[0], nullptr, IID_PPV_ARGS(&g_cl)))) return false;
    g_cl->Close();
    if (FAILED(g_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_cf)))) return false;
    g_cq->SetName(L"Nioh3EngineFix CbvMirror copy queue");
    ULOG("CM: own COPY queue %p created (%d allocators)", (void*)g_cq, kAllocs);
    g_copyReady = true;
    return true;
}
GameQ* GameQueue(ID3D12CommandQueue* q) {
    for (int i = 0; i < g_ngq; ++i)
        if (g_gq[i].q == q) return &g_gq[i];
    const D3D12_COMMAND_QUEUE_DESC qd = q->GetDesc();
    if (qd.Type != D3D12_COMMAND_LIST_TYPE_DIRECT && qd.Type != D3D12_COMMAND_LIST_TYPE_COMPUTE) return nullptr;
    if (g_ngq >= 16) return nullptr;
    g_gq[g_ngq] = GameQ{};
    g_gq[g_ngq].q = q;
    g_gq[g_ngq].type = qd.Type;
    g_gq[g_ngq].cursor = g_logSeq0 + g_clog.size();
    ULOG("CM: game queue %p (%s) waits on the copy fence", (void*)q, qd.Type == D3D12_COMMAND_LIST_TYPE_DIRECT ? "DIRECT" : "COMPUTE");
    return &g_gq[g_ngq++];
}

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!info.iniPath) return false;
    g_mode = (int)GetPrivateProfileIntW(L"GpuOpt", L"CbvMirror", 0, info.iniPath);
    g_allSites = GetPrivateProfileIntW(L"GpuOpt", L"CbvMirrorAllSites", 0, info.iniPath) != 0;
    g_sites = (int)GetPrivateProfileIntW(L"GpuOpt", L"CbvMirrorSites", 1, info.iniPath) & 3;
    if (!g_sites) g_sites = 1;
    g_queueMode = (int)GetPrivateProfileIntW(L"GpuOpt", L"CbvMirrorQueue", 0, info.iniPath) == 1 ? 1 : 0;
    g_computeCopy = GetPrivateProfileIntW(L"GpuOpt", L"CbvMirrorComputeCopy", 1, info.iniPath) != 0;
    int mb = (int)GetPrivateProfileIntW(L"GpuOpt", L"CbvMirrorMB", 128, info.iniPath);
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_qpf = f.QuadPart;
    mb = mb < 8 ? 8 : mb > 512 ? 512 : mb;
    g_ringBytes = (uint64_t)mb << 20;
    g_wine = IsWine();
    if (g_mode < 0 || g_mode > 2) g_mode = 0;
    if (g_mode && g_wine) {
        ULOG("CM: CbvMirror = %d refused under Wine / vkd3d-proton (no PCIe upload heaps to fix there) - off", g_mode);
        g_mode = 0;
    }
    if (g_mode)
        ULOG("CM: CbvMirror = %d (%s), mirror ring %d MB in VRAM, sites %s", g_mode,
             g_mode == 2 ? "FORCE - test mode" : "auto: only when the UPLOAD heaps live in system memory",
             mb, g_allSites ? "all CBVs in the UPLOAD ring"
                 : g_sites == 1 ? "per-draw recorder 118F89"
                 : g_sites == 2 ? "compute dispatch constants 495B2E + 117F20 (CbvMirrorSites = 2)"
                                : "per-draw 118F89 + compute 495B2E / 117F20 (CbvMirrorSites = 3)");
    if (g_mode == 2) InterlockedExchange(&g_active, 1);
    return g_mode != 0;
}

bool Wanted() { return g_mode != 0; }
bool WantsPresentHook() { return g_mode != 0; }

void OnHeapType(int type) {
    if (type == 5 && g_mode && !g_failed) {   // D3D12_HEAP_TYPE_GPU_UPLOAD: the game itself uses ReBAR memory - stand down
        InterlockedExchange(&g_failed, 1);
        ULOG("CM: the game created a GPU_UPLOAD heap (ReBAR path) - CbvMirror off");
    }
}

void OnHeap(ID3D12Device* dev, ID3D12Heap* heap, const D3D12_HEAP_DESC& d) {
    OnHeapType((int)d.Properties.Type);
    if (!g_mode || !heap) return;
    const bool up = d.Properties.Type == D3D12_HEAP_TYPE_UPLOAD ||
                    (d.Properties.Type == D3D12_HEAP_TYPE_CUSTOM && d.Properties.MemoryPoolPreference == D3D12_MEMORY_POOL_L0);
    if (up || d.Properties.Type == D3D12_HEAP_TYPE_READBACK) g_sysHeapBytes += d.SizeInBytes;
    if (!up || d.SizeInBytes < (256ull << 20) || g_alias) return;   // the ring: the first big UPLOAD heap
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = d.SizeInBytes;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    if (FAILED(dev->CreatePlacedResource(heap, 0, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_alias))) ||
        !g_alias) {
        ULOG("CM: alias buffer over the %llu MB UPLOAD heap failed - CbvMirror off", (unsigned long long)(d.SizeInBytes >> 20));
        InterlockedExchange(&g_failed, 1);
        return;
    }
    if (!CreateMirror(dev)) {
        ULOG("CM: %llu MB DEFAULT mirror ring not created - CbvMirror off", (unsigned long long)(g_ringBytes >> 20));
        g_alias->Release();
        g_alias = nullptr;
        InterlockedExchange(&g_failed, 1);
        return;
    }
    heap->AddRef();
    g_upHeap = heap;
    g_upBase = g_alias->GetGPUVirtualAddress();
    g_upSize = d.SizeInBytes;
    g_dev = dev;
    ULOG("CM: UPLOAD ring heap %p, %llu MB at GPU VA %llX; mirror ring %llu MB at %llX", (void*)heap,
         (unsigned long long)(g_upSize >> 20), (unsigned long long)g_upBase, (unsigned long long)(g_ringBytes >> 20),
         (unsigned long long)g_mirrorVa);
}

void SetAuto(double nonLocalUsedMB, bool uma, bool gpuUploadHeap) {
    if (g_mode != 1 || g_failed || !g_alias) return;
    static int stable = 0;
    const double sysMB = (double)g_sysHeapBytes / 1048576.0;
    const bool want = !uma && !gpuUploadHeap && sysMB >= 256.0 && nonLocalUsedMB >= 0.75 * sysMB;
    if (want == (g_active != 0)) { stable = 0; return; }
    if (++stable < 3) return;   // 3 s in a row
    stable = 0;
    if (want) {   // only ever switches ON (switching off mid-frame would leave descriptors on the mirror without copies)
        InterlockedExchange(&g_active, 1);
        ULOG("CM: auto -> ON: UPLOAD/READBACK heaps %.0f MB, DXGI non-local usage %.0f MB (system memory, no ReBAR), UMA %s",
             sysMB, nonLocalUsedMB, uma ? "yes" : "no");
    }
}

bool RedirectCbv(ID3D12Device* dev, const D3D12_CONSTANT_BUFFER_VIEW_DESC* in, D3D12_CONSTANT_BUFFER_VIEW_DESC* out, void* ra) {
    if (!g_active || g_failed || !in || !g_alias || dev != g_dev) return false;
    const uint64_t va = in->BufferLocation, size = in->SizeInBytes;
    if (va < g_upBase || va + size > g_upBase + g_upSize || !size) { InterlockedIncrement64(&c_outside); return false; }
    if (!g_allSites) {
        const uintptr_t r = reinterpret_cast<uintptr_t>(ra);
        const uintptr_t rva = g_base ? r - g_base : 0;
        const bool ok = g_base && (((g_sites & 1) && rva == kRvaRecorderCbv) ||
                                   ((g_sites & 2) && (rva == kRvaComputeCbvTable || rva == kRvaComputeCbvSyn)));
        if (!ok) { InterlockedIncrement64(&c_site); return false; }
    }
    const uint64_t key = va ^ (size << 44);
    uint64_t off;
    AcquireSRWLockExclusive(&g_lock);
    auto it = g_dedupe.find(key);
    if (it != g_dedupe.end()) {
        off = it->second;
        ReleaseSRWLockExclusive(&g_lock);
        InterlockedIncrement64(&c_dedup);
    } else {
        off = AllocLocked(size);
        if (off == UINT64_MAX) {
            ReleaseSRWLockExclusive(&g_lock);
            InterlockedIncrement64(&c_full);
            return false;
        }
        g_dedupe.emplace(key, off);
        uint64_t cs = (size + 255) & ~255ull;
        if (va - g_upBase + cs > g_upSize) cs = g_upSize - (va - g_upBase);
        g_pending.push_back(Copy{va - g_upBase, off, cs});
        if (g_queueMode == 0) g_clog.push_back(Copy{va - g_upBase, off, cs, g_head - ((size + 255) & ~255ull)});
        ReleaseSRWLockExclusive(&g_lock);
    }
    *out = *in;
    out->BufferLocation = g_mirrorVa + off;
    InterlockedIncrement64(&c_redir);
    return true;
}

void OnEclCopyQueue(ID3D12CommandQueue* q, EclFn orig) {
    if (!g_mode || !g_alias || g_failed || !orig) return;
    AcquireSRWLockExclusive(&g_lock);
    GameQ* gq = GameQueue(q);
    if (!gq) {   // a COPY queue of the game (streaming): its lists never read constant buffers
        ReleaseSRWLockExclusive(&g_lock);
        return;
    }
    if (!g_copyReady && (g_copyTried || !CreateCopyQueue())) {
        if (!g_failed) ULOG("CM: own COPY queue not created - CbvMirror off");
        InterlockedExchange(&g_failed, 1);
        ReleaseSRWLockExclusive(&g_lock);
        return;   // nothing was redirected without a flush path: redirects stop with g_failed (descriptors so far: see below)
    }
    if (!g_pending.empty()) {
        std::vector<Copy> cp;
        cp.swap(g_pending);
        g_dedupe.clear();
        std::sort(cp.begin(), cp.end(), [](const Copy& a, const Copy& b) { return a.dst < b.dst; });
        std::vector<Copy> m;   // merge ranges contiguous in both the source and the mirror
        m.reserve(cp.size());
        for (const Copy& x : cp) {
            if (!m.empty() && m.back().dst + m.back().size == x.dst && m.back().src + m.back().size == x.src) m.back().size += x.size;
            else m.push_back(x);
        }
        const int k = g_caNext;
        g_caNext = (g_caNext + 1) % kAllocs;
        if (g_caFence[k] && g_cf->GetCompletedValue() < g_caFence[k]) {   // the copy queue is 64 submissions behind: wait
            InterlockedIncrement64(&c_cpuWaits);
            HANDLE e = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            if (e && SUCCEEDED(g_cf->SetEventOnCompletion(g_caFence[k], e))) WaitForSingleObject(e, 2000);
            if (e) CloseHandle(e);
        }
        bool ok = SUCCEEDED(g_ca[k]->Reset()) && SUCCEEDED(g_cl->Reset(g_ca[k], nullptr));
        uint64_t bytes = 0;
        if (ok) {
            for (const Copy& x : m) {
                g_cl->CopyBufferRegion(g_mirror, x.dst, g_alias, x.src, x.size);
                bytes += x.size;
            }
            ok = SUCCEEDED(g_cl->Close());
        }
        if (!ok) {
            InterlockedExchange(&g_failed, 1);
            ReleaseSRWLockExclusive(&g_lock);
            ULOG("CM: copy list record failed - CbvMirror off for the rest of the session");
            return;
        }
        ID3D12CommandList* l = g_cl;
        orig(g_cq, 1, &l);
        g_cq->Signal(g_cf, ++g_cv);
        g_caFence[k] = g_cv;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        g_inFlight.push_back(Batch{g_cf, g_cv, g_head, now.QuadPart});
        InterlockedIncrement64(&c_flush);
        InterlockedAdd64(&c_copies, (LONG64)m.size());
        InterlockedAdd64(&c_bytes, (LONG64)bytes);
    }
    // this queue's lists may read anything copied so far (also copies flushed for another queue's submission)
    if (g_cv > gq->waited) {
        if (g_cf->GetCompletedValue() < g_cv) {
            q->Wait(g_cf, g_cv);
            InterlockedIncrement64(&c_waits);
        }
        gq->waited = g_cv;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

// 1.7.54 inline mode: one prologue list on the game's OWN queue, submitted in the SAME ExecuteCommandLists call ahead of the
// game's lists: copies (implicit promotion COMMON -> COPY_DEST) + one barrier COPY_DEST -> VERTEX_AND_CONSTANT_BUFFER. No
// cross-queue waits. A DIRECT queue copies every range redirected since its last submission; a COMPUTE queue (async) copies the
// same ranges again into the same mirror slots (identical bytes; buffers are simultaneous-access) unless CbvMirrorComputeCopy = 0.
// Returns true when it submitted the game's lists itself.
bool OnEclInline(ID3D12CommandQueue* q, unsigned n, ID3D12CommandList* const* lists, EclFn orig) {
    if (!g_mode || !g_alias || g_failed || !orig || n > 62) return false;
    AcquireSRWLockExclusive(&g_lock);
    GameQ* gq = GameQueue(q);
    if (!gq) {
        ReleaseSRWLockExclusive(&g_lock);
        return false;
    }
    if (!gq->ready) {
        if (gq->tried) {
            ReleaseSRWLockExclusive(&g_lock);
            return false;
        }
        gq->tried = true;
        bool ok = true;
        for (auto& a : gq->alloc) ok = ok && SUCCEEDED(g_dev->CreateCommandAllocator(gq->type, IID_PPV_ARGS(&a)));
        ok = ok && SUCCEEDED(g_dev->CreateCommandList(0, gq->type, gq->alloc[0], nullptr, IID_PPV_ARGS(&gq->list)));
        if (ok) gq->list->Close();
        ok = ok && SUCCEEDED(g_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gq->fence)));
        if (!ok) {
            InterlockedExchange(&g_failed, 1);
            ReleaseSRWLockExclusive(&g_lock);
            ULOG("CM: prologue list for queue %p not created - CbvMirror off", (void*)q);
            return false;
        }
        gq->ready = true;
    }
    const bool direct = gq->type == D3D12_COMMAND_LIST_TYPE_DIRECT;
    const uint64_t end = g_logSeq0 + g_clog.size();
    if (gq->cursor < g_logSeq0) gq->cursor = g_logSeq0;   // trimmed past this (idle) queue
    if (!direct && !g_computeCopy) gq->cursor = end;
    if (gq->cursor >= end) {
        ReleaseSRWLockExclusive(&g_lock);
        return false;
    }
    std::vector<Copy> cp;
    RetireLocked();
    for (auto it = g_clog.begin() + (ptrdiff_t)(gq->cursor - g_logSeq0); it != g_clog.end(); ++it)
        if (direct || it->abs >= g_oldest) cp.push_back(*it);   // a lagging COMPUTE queue never re-copies into a reused slot
    gq->cursor = end;
    if (cp.empty()) {
        ReleaseSRWLockExclusive(&g_lock);
        return false;
    }
    if (direct) {   // the DIRECT queue defines the epoch: dedupe restarts, pending (not yet copied by DIRECT) is cleared
        g_pending.clear();
        g_dedupe.clear();
    }
    uint64_t minCur = end;   // trim the log to the slowest active cursor
    for (int i = 0; i < g_ngq; ++i)
        if (g_gq[i].ready && (g_gq[i].type == D3D12_COMMAND_LIST_TYPE_DIRECT || g_computeCopy)) minCur = std::min(minCur, g_gq[i].cursor);
    if (g_clog.size() > 200000 && minCur + 100000 < end) minCur = end - 100000;   // a queue that stopped submitting
    if (minCur > g_logSeq0) {
        g_clog.erase(g_clog.begin(), g_clog.begin() + (ptrdiff_t)(minCur - g_logSeq0));
        g_logSeq0 = minCur;
    }
    std::sort(cp.begin(), cp.end(), [](const Copy& a, const Copy& b) { return a.dst < b.dst; });
    std::vector<Copy> m;
    m.reserve(cp.size());
    for (const Copy& x : cp) {
        if (!m.empty() && m.back().dst + m.back().size == x.dst && m.back().src + m.back().size == x.src) m.back().size += x.size;
        else if (m.empty() || x.dst >= m.back().dst + m.back().size) m.push_back(x);   // drops exact re-copies of one slot
    }
    const int k = gq->next;
    gq->next = (gq->next + 1) % kAllocs;
    if (gq->allocFence[k] && gq->fence->GetCompletedValue() < gq->allocFence[k]) {
        InterlockedIncrement64(&c_cpuWaits);
        HANDLE e = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (e && SUCCEEDED(gq->fence->SetEventOnCompletion(gq->allocFence[k], e))) WaitForSingleObject(e, 2000);
        if (e) CloseHandle(e);
    }
    bool ok = SUCCEEDED(gq->alloc[k]->Reset()) && SUCCEEDED(gq->list->Reset(gq->alloc[k], nullptr));
    uint64_t bytes = 0;
    if (ok) {
        for (const Copy& x : m) {
            gq->list->CopyBufferRegion(g_mirror, x.dst, g_alias, x.src, x.size);
            bytes += x.size;
        }
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = g_mirror;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        gq->list->ResourceBarrier(1, &b);
        ok = SUCCEEDED(gq->list->Close());
    }
    if (!ok) {
        InterlockedExchange(&g_failed, 1);
        ReleaseSRWLockExclusive(&g_lock);
        ULOG("CM: prologue record failed - CbvMirror off for the rest of the session");
        return false;
    }
    ID3D12CommandList* arr[64];
    arr[0] = gq->list;
    for (unsigned i = 0; i < n; ++i) arr[i + 1] = lists[i];
    orig(q, n + 1, arr);
    q->Signal(gq->fence, ++gq->value);   // after the game's lists of this submission: retire tracks the readers too
    gq->allocFence[k] = gq->value;
    if (direct) {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        g_inFlight.push_back(Batch{gq->fence, gq->value, g_head, now.QuadPart});
    }
    InterlockedIncrement64(&c_flush);
    InterlockedAdd64(&c_copies, (LONG64)m.size());
    InterlockedAdd64(&c_bytes, (LONG64)bytes);
    if (!direct) InterlockedIncrement64(&c_waits);   // inline mode: counts COMPUTE prologues (no waits exist)
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

bool OnEcl(ID3D12CommandQueue* q, unsigned n, ID3D12CommandList* const* lists, EclFn orig) {
    if (g_queueMode == 0) return OnEclInline(q, n, lists, orig);
    OnEclCopyQueue(q, orig);
    return false;
}

void LogLine() {
    if (!g_mode) return;
    static LONG64 p[10] = {};
    const LONG64 v[10] = {c_redir, c_dedup, c_full, c_outside, c_site, c_flush, c_copies, c_bytes, c_waits, c_cpuWaits};
    ULOG("stats/s CM: %s | redirected %lld/s (dedupe %lld), ring full %lld, outside ring %lld, other site %lld | flushes %lld/s, "
         "copies %lld/s, %.2f MB/s | %s %lld, CPU allocator waits %lld",
         g_failed ? "FAILED (off)" : g_active ? "ON" : g_alias ? "armed (auto: UPLOAD heaps are in VRAM / not decided)" : "no ring heap yet",
         (v[0] - p[0]) / 30, (v[1] - p[1]) / 30, v[2] - p[2], (v[3] - p[3]) / 30, (v[4] - p[4]) / 30, (v[5] - p[5]) / 30,
         (v[6] - p[6]) / 30, (double)(v[7] - p[7]) / 30.0 / 1048576.0,
         g_queueMode == 0 ? "inline prologues (compute)" : "GPU waits on the copy fence", v[8] - p[8], v[9] - p[9]);
    for (int i = 0; i < 10; ++i) p[i] = v[i];
}

}  // namespace upmirror
