// cbvstage.cpp - CBF (F8): system-memory staging for the per-draw CB ring + one batched copy per submission into a VRAM
// ring that the CBVs point at. See cbvstage.h and notes\renderer\cbv-upload.md section 7.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef CBU_HARNESS
#include <d3d12.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "cbvstage.h"

namespace cbvstage {
namespace {

constexpr size_t kRing = 0xB28;    // SynCtx CB ring: u64 [0] resource, [1] size, [2] CPU base, [3] GPU base
constexpr size_t kList = 0x580;    // SynCtx open command list
constexpr size_t kDev = 0x8E0;     // SynCtx ID3D12Device (CreateConstantBufferView)
constexpr uint64_t kGap = 4096;    // runs closer than this are copied as one (the gap bytes are harmless, see cbvstage.h)
constexpr int kMaxRuns = 4096;

void (*g_log)(const char*, ...) = nullptr;
int g_mode = 0;
volatile uint32_t g_on = 0, g_copyq = 0;

struct Run {
    uint64_t off, end;
};
SRWLOCK g_runLock = SRWLOCK_INIT;
Run g_runs[kMaxRuns];
int g_nRuns = 0;

uint8_t* volatile g_sys = nullptr;  // mapped staging (system memory)
uint64_t g_size = 0;
uint64_t g_vramGpu = 0;
uint8_t* g_c = nullptr;              // the graphics SynCtx (first table call)
uint64_t g_origCpu = 0, g_origGpu = 0;
bool g_swapped = false, g_ready = false, g_failed = false;
void* volatile g_recent[4] = {};
volatile LONG g_recentIdx = 0;
ID3D12CommandQueue* volatile g_q = nullptr;

volatile LONG64 c_notes = 0, c_flush = 0, c_runs = 0, c_bytes = 0, c_waits = 0, c_other = 0, c_overflow = 0, c_swaps = 0;
volatile LONG64 g_notesAtTake = 0;   // c_notes when the runs were last taken by a submission (watchdog in OnTable)
constexpr LONG64 kNoFlushNotes = 100000;  // ~0.3 s of the game's uploads with no submission taking them: CBF off

bool Recent(const void* l) {
    for (void* r : g_recent)
        if (r && r == l) return true;
    return false;
}

void TakeRuns(Run* out, int& n) {
    AcquireSRWLockExclusive(&g_runLock);
    n = g_nRuns;
    memcpy(out, g_runs, sizeof(Run) * (size_t)n);
    g_nRuns = 0;
    g_notesAtTake = c_notes;
    ReleaseSRWLockExclusive(&g_runLock);
}

#ifndef CBU_HARNESS
// ---- D3D12 side ----
ID3D12Device* g_dev = nullptr;
ID3D12Resource* g_sysRes = nullptr;
ID3D12Resource* g_vramRes = nullptr;
constexpr int kAllocs = 64;
struct Sub {
    ID3D12CommandAllocator* alloc[kAllocs] = {};
    uint64_t allocFence[kAllocs] = {};
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    uint64_t value = 0;
    int next = 0;
    HANDLE ev = nullptr;
    bool ready = false, tried = false;
};
Sub g_direct, g_copy;
ID3D12CommandQueue* g_copyQ = nullptr;
SRWLOCK g_subLock = SRWLOCK_INIT;

bool MakeSub(Sub& s, D3D12_COMMAND_LIST_TYPE t) {
    if (s.tried) return s.ready;
    s.tried = true;
    bool ok = true;
    for (auto& a : s.alloc) ok = ok && SUCCEEDED(g_dev->CreateCommandAllocator(t, IID_PPV_ARGS(&a)));
    ok = ok && SUCCEEDED(g_dev->CreateCommandList(0, t, s.alloc[0], nullptr, IID_PPV_ARGS(&s.list)));
    if (ok) s.list->Close();
    ok = ok && SUCCEEDED(g_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&s.fence)));
    s.ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    s.ready = ok && s.ev;
    return s.ready;
}

void Create(uint8_t* c) {
    g_failed = true;  // until everything below worked
    const uint64_t* ring = reinterpret_cast<const uint64_t*>(c + kRing);
    g_dev = *reinterpret_cast<ID3D12Device**>(c + kDev);
    const uint64_t size = ring[1];
    if (!g_dev || size < 65536 || size > (1ull << 31)) {
        if (g_log) g_log("CBU: CBF not started (ring size %llu, device %p)", (unsigned long long)size, (void*)g_dev);
        return;
    }
    D3D12_COMMAND_QUEUE_DESC qd = g_q->GetDesc();
    if (qd.Type != D3D12_COMMAND_LIST_TYPE_DIRECT) {
        if (g_log) g_log("CBU: CBF not started (the SynCtx lists go to a queue of type %d, not DIRECT)", (int)qd.Type);
        return;
    }
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = size;
    rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES sys{};  // what UPLOAD is on a discrete adapter, pinned to system memory (VSForceSysUpload)
    sys.Type = D3D12_HEAP_TYPE_CUSTOM;
    sys.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE;
    sys.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    D3D12_HEAP_PROPERTIES vram{};
    vram.Type = D3D12_HEAP_TYPE_DEFAULT;
    if (FAILED(g_dev->CreateCommittedResource(&sys, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&g_sysRes))) ||
        FAILED(g_dev->CreateCommittedResource(&vram, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&g_vramRes)))) {
        if (g_log) g_log("CBU: CBF not started (staging / VRAM ring of %llu MB not created)", (unsigned long long)(size >> 20));
        return;
    }
    void* p = nullptr;
    D3D12_RANGE none{0, 0};
    if (FAILED(g_sysRes->Map(0, &none, &p)) || !p) {
        if (g_log) g_log("CBU: CBF not started (staging Map failed)");
        return;
    }
    if (!MakeSub(g_direct, D3D12_COMMAND_LIST_TYPE_DIRECT)) {
        if (g_log) g_log("CBU: CBF not started (prologue list / fence not created)");
        return;
    }
    g_size = size;
    g_vramGpu = g_vramRes->GetGPUVirtualAddress();
    g_sys = static_cast<uint8_t*>(p);
    g_failed = false;
    g_ready = true;
    if (g_log)
        g_log("CBU: CBF ready: CB ring %llu MB -> staging %p (system memory, write-combined) + VRAM ring GPU %llX; queue %p", (unsigned long long)(size >> 20),
              p, (unsigned long long)g_vramGpu, (void*)g_q);
}

// one list: every run, system -> VRAM (+ COPY_DEST -> CB barrier on DIRECT); false on failure (caller turns CBF off)
bool Record(Sub& s, const Run* r, int n, bool direct, uint64_t& bytes) {
    const int k = s.next;
    s.next = (s.next + 1) % kAllocs;
    if (s.allocFence[k] && s.fence->GetCompletedValue() < s.allocFence[k]) {
        InterlockedIncrement64(&c_waits);
        if (SUCCEEDED(s.fence->SetEventOnCompletion(s.allocFence[k], s.ev))) WaitForSingleObject(s.ev, 2000);
    }
    if (FAILED(s.alloc[k]->Reset()) || FAILED(s.list->Reset(s.alloc[k], nullptr))) return false;
    bytes = 0;
    for (int i = 0; i < n; ++i) {
        const uint64_t off = r[i].off, end = r[i].end < g_size ? r[i].end : g_size;
        if (end <= off) continue;
        s.list->CopyBufferRegion(g_vramRes, off, g_sysRes, off, end - off);
        bytes += end - off;
    }
    if (direct) {
        D3D12_RESOURCE_BARRIER b{};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = g_vramRes;
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        s.list->ResourceBarrier(1, &b);
    }
    return SUCCEEDED(s.list->Close());
}
#endif

}  // namespace

void Configure(void (*log)(const char*, ...), int mode, int copyQueue) {
    g_log = log;
    g_mode = mode;
    g_on = mode == 1 ? 1u : 0u;
    g_copyq = copyQueue ? 1u : 0u;
}
bool Installed() { return g_mode != 0; }
bool WantsEclHook() { return g_mode != 0; }
volatile uint32_t* Switch() { return g_mode ? &g_on : nullptr; }
volatile uint32_t* QueueSwitch() { return g_mode ? &g_copyq : nullptr; }

void NoteWrite(const void* dst, size_t n) {
    uint8_t* s = g_sys;
    const uint8_t* p = static_cast<const uint8_t*>(dst);
    if (!s || p < s || p >= s + g_size || !n) return;
    const uint64_t off = (uint64_t)(p - s), end = off + n;
    InterlockedIncrement64(&c_notes);
    AcquireSRWLockExclusive(&g_runLock);
    Run* last = g_nRuns ? &g_runs[g_nRuns - 1] : nullptr;
    if (last && off >= last->off && off <= last->end + kGap) {
        if (end > last->end) last->end = end;
    } else if (g_nRuns < kMaxRuns) {
        g_runs[g_nRuns++] = Run{off, end};
    } else {  // full: widen the last run (copies more, never less)
        if (off < last->off) last->off = off;
        if (end > last->end) last->end = end;
        InterlockedIncrement64(&c_overflow);
    }
    ReleaseSRWLockExclusive(&g_runLock);
}

void OnTable(uint8_t* c) {
    if (!g_mode) return;
    if (!g_c) g_c = c;
    if (c != g_c) return;
    g_recent[InterlockedIncrement(&g_recentIdx) & 3] = *reinterpret_cast<void**>(c + kList);
    uint64_t* ring = reinterpret_cast<uint64_t*>(c + kRing);
    if (g_swapped && g_on && c_notes - g_notesAtTake > kNoFlushNotes) {
        // the noted ranges are not reaching any submission (a hook in front of ours, a queue we do not see): the VRAM ring
        // would serve stale bytes -> back to the UPLOAD ring for the rest of the session
        g_on = 0;
        g_failed = true;
        if (g_log) g_log("CBU: CBF OFF for this session - %lld ring writes and no game submission took them (watchdog)", (long long)(c_notes - g_notesAtTake));
    }
    if (g_on && !g_swapped) {
#ifndef CBU_HARNESS
        if (!g_ready && !g_failed && g_q) Create(c);
#endif
        if (g_ready && !g_failed && ring[1] <= g_size) {
            g_origCpu = ring[2];
            g_origGpu = ring[3];
            ring[2] = (uint64_t)(uintptr_t)g_sys;
            ring[3] = g_vramGpu;
            g_swapped = true;
            InterlockedIncrement64(&c_swaps);
        }
    } else if (!g_on && g_swapped) {
        ring[2] = g_origCpu;
        ring[3] = g_origGpu;
        g_swapped = false;
        InterlockedIncrement64(&c_swaps);
    }
}

bool OnEcl(ID3D12CommandQueue* q, unsigned n, ID3D12CommandList* const* lists, EclFn orig) {
#ifdef CBU_HARNESS
    (void)q, (void)n, (void)lists, (void)orig;
    return false;
#else
    if (!g_mode || !q) return false;
    if (!g_q) {
        for (unsigned i = 0; i < n; ++i)
            if (Recent(lists[i])) {
                g_q = q;
                break;
            }
        return false;  // nothing staged before the queue is known
    }
    if (q != g_q) {
        if (g_nRuns)
            for (unsigned i = 0; i < n; ++i)
                if (Recent(lists[i])) {
                    InterlockedIncrement64(&c_other);  // a SynCtx list on another queue while ranges are pending (never seen)
                    break;
                }
        return false;
    }
    if (!g_ready || !g_nRuns) return false;
    static Run runs[kMaxRuns];
    AcquireSRWLockExclusive(&g_subLock);
    int nr = 0;
    TakeRuns(runs, nr);
    if (!nr) {
        ReleaseSRWLockExclusive(&g_subLock);
        return false;
    }
    uint64_t bytes = 0;
    bool handled = false, ok = true;
    if (g_copyq) {
        if (!g_copyQ) {
            D3D12_COMMAND_QUEUE_DESC d{};
            d.Type = D3D12_COMMAND_LIST_TYPE_COPY;
            ok = SUCCEEDED(g_dev->CreateCommandQueue(&d, IID_PPV_ARGS(&g_copyQ))) && MakeSub(g_copy, D3D12_COMMAND_LIST_TYPE_COPY);
        }
        ok = ok && g_copy.ready && Record(g_copy, runs, nr, false, bytes);
        if (ok) {
            ID3D12CommandList* l = g_copy.list;
            g_copyQ->ExecuteCommandLists(1, &l);
            g_copyQ->Signal(g_copy.fence, ++g_copy.value);
            g_copy.allocFence[(g_copy.next + kAllocs - 1) % kAllocs] = g_copy.value;
            q->Wait(g_copy.fence, g_copy.value);  // the game's lists of this submission start after the copy
        }
    } else {
        ok = Record(g_direct, runs, nr, true, bytes);
        if (ok) {
            ID3D12CommandList* arr[64];
            ID3D12CommandList* one[1] = {g_direct.list};
            if (n < 64) {
                arr[0] = g_direct.list;
                for (unsigned i = 0; i < n; ++i) arr[i + 1] = lists[i];
                orig(q, n + 1, arr);
            } else {  // more than 63 lists: the prologue as its own submission first (same queue, in order)
                orig(q, 1, one);
                orig(q, n, lists);
            }
            q->Signal(g_direct.fence, ++g_direct.value);
            g_direct.allocFence[(g_direct.next + kAllocs - 1) % kAllocs] = g_direct.value;
            handled = true;
        }
    }
    if (!ok) {
        // never leave CBVs on a VRAM ring that is not filled: stop staging at once (the replay thread restores the bases
        // at its next table call; until then its writes still go to staging and are copied by the next good flush - none:
        // so copy synchronously is impossible here; the safe answer is the game's own lists unchanged + CBF off + log)
        g_on = 0;
        g_failed = true;
        if (g_log) g_log("CBU: CBF copy record failed - CBF off for the rest of the session");
    } else {
        InterlockedIncrement64(&c_flush);
        InterlockedAdd64(&c_runs, nr);
        InterlockedAdd64(&c_bytes, (LONG64)bytes);
    }
    ReleaseSRWLockExclusive(&g_subLock);
    return handled;
#endif
}

void StatsLine(char* buf, size_t n, double s) {
    if (!n) return;
    buf[0] = 0;
    if (!g_mode) return;
    static LONG64 p[8] = {};
    const LONG64 v[8] = {c_notes, c_flush, c_runs, c_bytes, c_waits, c_other, c_overflow, c_swaps};
    snprintf(buf, n, "CBF %s%s (%s) | writes %.0f/s, flushes %.0f/s (%.1f runs each), copied %.2f MB/s | allocator waits %lld, other-queue %lld, "
             "run overflow %lld, ring swaps %lld",
             g_failed ? "FAILED " : "", g_on ? (g_swapped ? "ON" : "armed") : "off", g_copyq ? "COPY queue + wait" : "DIRECT prologue",
             (v[0] - p[0]) / s, (v[1] - p[1]) / s, v[1] - p[1] ? (double)(v[2] - p[2]) / (double)(v[1] - p[1]) : 0.0, (v[3] - p[3]) / s / 1048576.0,
             v[4] - p[4], v[5] - p[5], v[6] - p[6], v[7] - p[7]);
    for (int i = 0; i < 8; ++i) p[i] = v[i];
}

#ifdef CBU_HARNESS
void TestSetup(uint8_t* sys, uint8_t* vram, uint64_t size) {
    g_mode = 1;
    g_on = 1;
    g_sys = sys;
    g_size = size;
    g_vramGpu = (uint64_t)(uintptr_t)vram;
    g_ready = true;
    g_failed = false;
    g_q = reinterpret_cast<ID3D12CommandQueue*>(1);
}
void TestFlush(void (*copy)(uint64_t off, uint64_t len)) {
    static Run runs[kMaxRuns];
    int nr = 0;
    TakeRuns(runs, nr);
    for (int i = 0; i < nr; ++i) {
        const uint64_t end = runs[i].end < g_size ? runs[i].end : g_size;
        if (end > runs[i].off) copy(runs[i].off, end - runs[i].off);
    }
    InterlockedIncrement64(&c_flush);
    InterlockedAdd64(&c_runs, nr);
}
void TestReset() {
    g_mode = 0;
    g_on = 0;
    g_sys = nullptr;
    g_size = 0;
    g_c = nullptr;
    g_swapped = g_ready = g_failed = false;
    g_nRuns = 0;
    g_q = nullptr;
}
#endif

}  // namespace cbvstage
