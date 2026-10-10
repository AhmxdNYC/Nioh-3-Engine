// scenemirror.cpp - SBM meshlet scene-buffer mirror (see scenemirror.h, notes\rebar-fix-v3.md). Default off.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <intrin.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "rebar.h"
#include "scenemirror.h"

namespace scenemirror {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define SLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

int g_mode = 0;        // [GpuOpt] SceneBufMirror: 0 off, 1 auto (no-ReBAR systems), 2 force
int g_probe = 0;       // [Profiler] SceneBufProbe: census lines (+ late-write check while the mirror runs)
int g_queueMode = 1;   // [GpuOpt] SceneBufMirrorQueue: 1 own COPY queue, 0 the game's DIRECT queue
bool g_configured = false;
volatile LONG g_on = 0;       // hook installed (mode or probe)
volatile LONG g_failed = 0;
volatile LONG g_eclLive = 0;  // the osd queue hook has seen a game DIRECT submission (Waits can be inserted)
uintptr_t g_base = 0;

// ---- v2.02 (patches.json SBM1; decompiles 5D214 / 4BBEC / 86F5DC / 4DF64 / 4DE8C) ----
constexpr uint32_t kSite = 0x5D417;     // MeshletFrameBegin 5D214: call MeshletUpdateSceneBuffers (rcx = MeshletSys)
constexpr uint32_t kCallee = 0x4BBEC;   // MeshletUpdateSceneBuffers (7 args)
const uint8_t kBefore[] = {0x44, 0x88, 0x44, 0x24, 0x20, 0x45, 0x8B, 0xC6};   // 5D40F mov [rsp+20h],r8b; mov r8d,r14d
const uint8_t kAfter[] = {0x48, 0x8B, 0x06, 0x48, 0x8B, 0xCE};                 // 5D41C mov rax,[rsi]; mov rcx,rsi
constexpr uintptr_t kOffSet = 0x16A0B0;   // MeshletSys -> current set (MeshletSys + 169FB0 / 16A030, swapped per frame)
constexpr int kSlots = 13;                // set + 18h .. + 78h
// buffer object (map 86F5DC / 4DF64, unmap 4DE8C): +18h ID3D12Resource, +68h usage index (table 3BEA260: 0/1 DEFAULT staged,
// 2 UPLOAD mapped directly, 3 READBACK), +70h capacity (bytes), +80h offset in the resource, +98h map state (4 = unmapped),
// +A0h / +A8h written range of the last map (offset added to +80h, bytes)
constexpr uintptr_t kBRes = 0x18, kBUsage = 0x68, kBCap = 0x70, kBOff = 0x80, kBState = 0x98, kBWOff = 0xA0, kBWLen = 0xA8;
constexpr int kUsageUpload = 2;
constexpr uint64_t kTwinCap = 256ull << 20;   // VRAM for twins, total
constexpr int kAlloc = 8;                     // copy lists in flight (one per frame)

using UpdateFn = void (*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
UpdateFn g_orig = nullptr;
SrvFn g_srvOrig = nullptr;
bool (*g_sysVa)(uint64_t) = nullptr;

struct Buf {
    uint64_t obj = 0, res = 0, off = 0, woff = 0, wlen = 0;
    int usage = -1, state = -1;
    uint32_t cap = 0;
};
bool Rd(uintptr_t p, void* out, size_t n) {
    if (p < 0x10000) return false;
    __try {
        memcpy(out, (const void*)p, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool ReadSet(uint64_t sys, uint64_t* set, Buf* b) {
    *set = 0;
    if (!Rd((uintptr_t)(sys + kOffSet), set, 8) || !*set) return false;
    for (int i = 0; i < kSlots; ++i) {
        b[i] = Buf{};
        uint64_t o = 0;
        if (!Rd((uintptr_t)(*set + 0x18 + 8 * i), &o, 8) || !o) continue;
        b[i].obj = o;
        Rd((uintptr_t)(o + kBRes), &b[i].res, 8);
        Rd((uintptr_t)(o + kBUsage), &b[i].usage, 4);
        Rd((uintptr_t)(o + kBCap), &b[i].cap, 4);
        Rd((uintptr_t)(o + kBOff), &b[i].off, 8);
        Rd((uintptr_t)(o + kBState), &b[i].state, 4);
        Rd((uintptr_t)(o + kBWOff), &b[i].woff, 8);
        Rd((uintptr_t)(o + kBWLen), &b[i].wlen, 8);
        if (b[i].usage < 0 || b[i].usage > 3) b[i].usage = -1;
    }
    return true;
}

// ---- SRV bookkeeping ----
struct SrvRec {
    SIZE_T h;
    D3D12_SHADER_RESOURCE_VIEW_DESC d;
    uint64_t lo, hi;   // byte range in the resource (0, 0 = unknown element size: never redirected)
    bool redir;
};
struct Range {
    uint64_t lo, hi;
    bool copied;
};
struct Res {
    uint64_t va = 0, width = 0;
    std::vector<SrvRec> srv;
    std::vector<Range> scene;
    ID3D12Resource* twin = nullptr;
    bool held = false;   // we AddRef'd the resource (twin exists)
    int heap = -1;       // D3D12_HEAP_TYPE (logging), -2 placed in a system-memory heap
};
SRWLOCK g_lock = SRWLOCK_INIT;
std::unordered_map<ID3D12Resource*, Res> g_res;
std::unordered_map<SIZE_T, ID3D12Resource*> g_owner;
uint64_t g_twinBytes = 0;

uint32_t FmtBytes(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS: case DXGI_FORMAT_R32G32B32A32_FLOAT: case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT: return 16;
        case DXGI_FORMAT_R32G32B32_TYPELESS: case DXGI_FORMAT_R32G32B32_FLOAT: case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT: return 12;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT: case DXGI_FORMAT_R16G16B16A16_SNORM: case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS: case DXGI_FORMAT_R32G32_FLOAT: case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT: return 8;
        case DXGI_FORMAT_R32_TYPELESS: case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R32_UINT: case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM: case DXGI_FORMAT_R8G8B8A8_SINT: case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT: case DXGI_FORMAT_R16G16_UNORM: case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM: case DXGI_FORMAT_R16G16_SINT: case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT: case DXGI_FORMAT_R11G11B10_FLOAT: return 4;
        case DXGI_FORMAT_R16_TYPELESS: case DXGI_FORMAT_R16_FLOAT: case DXGI_FORMAT_R16_UNORM: case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM: case DXGI_FORMAT_R16_SINT: case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM: case DXGI_FORMAT_R8G8_SINT: return 2;
        case DXGI_FORMAT_R8_TYPELESS: case DXGI_FORMAT_R8_UNORM: case DXGI_FORMAT_R8_UINT: case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT: return 1;
        default: return 0;
    }
}
void SrvRange(const D3D12_SHADER_RESOURCE_VIEW_DESC& d, uint64_t* lo, uint64_t* hi) {
    *lo = *hi = 0;
    if (d.ViewDimension != D3D12_SRV_DIMENSION_BUFFER) return;
    const uint32_t es = d.Buffer.StructureByteStride ? d.Buffer.StructureByteStride
                        : (d.Buffer.Flags & D3D12_BUFFER_SRV_FLAG_RAW) ? 4u
                                                                       : FmtBytes(d.Format);
    if (!es || !d.Buffer.NumElements) return;
    *lo = d.Buffer.FirstElement * es;
    *hi = *lo + (uint64_t)d.Buffer.NumElements * es;
}
// UPLOAD heap, CPU-visible CUSTOM L0 (VSForceSysUpload), or placed in a system-memory heap
int UploadKind(ID3D12Resource* r) {
    D3D12_HEAP_PROPERTIES hp{};
    D3D12_HEAP_FLAGS hf{};
    if (SUCCEEDED(r->GetHeapProperties(&hp, &hf))) {
        if (hp.Type == D3D12_HEAP_TYPE_UPLOAD) return D3D12_HEAP_TYPE_UPLOAD;
        if (hp.Type == D3D12_HEAP_TYPE_CUSTOM && hp.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE &&
            hp.CPUPageProperty != D3D12_CPU_PAGE_PROPERTY_UNKNOWN && hp.MemoryPoolPreference == D3D12_MEMORY_POOL_L0)
            return D3D12_HEAP_TYPE_CUSTOM;
        return 0;
    }
    return g_sysVa && g_sysVa(r->GetGPUVirtualAddress()) ? -2 : 0;
}
// census: the heap type (1 DEFAULT, 2 UPLOAD, 3 READBACK, 4 CUSTOM, 5 GPU_UPLOAD; +10 = CUSTOM in system memory L0), or
// -2 / -3 when the runtime does not say (system-memory / other VA)
int CensusKind(ID3D12Resource* r) {
    D3D12_HEAP_PROPERTIES hp{};
    D3D12_HEAP_FLAGS hf{};
    if (SUCCEEDED(r->GetHeapProperties(&hp, &hf)))
        return (int)hp.Type + (hp.Type == D3D12_HEAP_TYPE_CUSTOM && hp.MemoryPoolPreference == D3D12_MEMORY_POOL_L0 ? 10 : 0);
    return g_sysVa && g_sysVa(r->GetGPUVirtualAddress()) ? -2 : -3;
}
bool InCopied(const Res& x, uint64_t lo, uint64_t hi) {
    if (hi <= lo) return false;
    for (const Range& r : x.scene)
        if (r.copied && lo >= r.lo && hi <= r.hi) return true;
    return false;
}

// ---- copy infrastructure ----
ID3D12Device* g_dev = nullptr;      // native device (GetDevice of a resource): queues, lists, twins
ID3D12Device* g_srvDev = nullptr;   // the device object the SRV hook sees (may be a proxy): the unhooked SRV call needs it
ID3D12CommandQueue* g_cq = nullptr;       // own COPY queue (mode 1)
ID3D12CommandQueue* g_directQ = nullptr;  // the game's DIRECT queue (mode 0 copies; AddRef'd)
ID3D12CommandAllocator* g_alloc[kAlloc] = {};
uint64_t g_allocFence[kAlloc] = {};
ID3D12GraphicsCommandList* g_list = nullptr;
ID3D12Fence* g_fence = nullptr;
volatile LONG64 g_v = 0;   // last signalled copy value (visible only after the Signal was queued)
int g_next = 0;
bool g_infraTried = false, g_infraOk = false;
ID3D12QueryHeap* g_qh = nullptr;
ID3D12Resource* g_tsBuf = nullptr;
uint64_t* g_tsMap = nullptr;
uint64_t g_tsFreq = 0;
bool g_tsPending[kAlloc] = {};
uint64_t g_slotBytes[kAlloc] = {};

struct QState {
    ID3D12CommandQueue* q;
    int kind;   // 1 wait (DIRECT / COMPUTE), 0 skip
    uint64_t waited;
};
QState g_qs[16];
int g_nq = 0;
SRWLOCK g_qlock = SRWLOCK_INIT;

// counters
volatile LONG64 c_calls = 0, c_frames = 0, c_copies = 0, c_bytes = 0, c_rewrite = 0, c_atCreate = 0, c_waits = 0, c_pend = 0,
                c_cpuWaits = 0, c_capSkip = 0, c_noSrv = 0, c_tsN = 0, c_tsTicks = 0, c_tsBytes = 0, c_late = 0, c_checks = 0,
                c_retired = 0, c_srvSeen = 0;

bool EnsureInfra(ID3D12Resource* anyRes) {
    if (g_infraTried) return g_infraOk;
    g_infraTried = true;
    if (FAILED(anyRes->GetDevice(IID_PPV_ARGS(&g_dev))) || !g_dev) return false;
    const D3D12_COMMAND_LIST_TYPE t = g_queueMode == 1 ? D3D12_COMMAND_LIST_TYPE_COPY : D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (g_queueMode == 1) {
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        if (FAILED(g_dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_cq)))) return false;
        g_cq->SetName(L"Nioh3EngineFix SceneBufMirror copy queue");
    }
    for (auto& a : g_alloc)
        if (FAILED(g_dev->CreateCommandAllocator(t, IID_PPV_ARGS(&a)))) return false;
    if (FAILED(g_dev->CreateCommandList(0, t, g_alloc[0], nullptr, IID_PPV_ARGS(&g_list)))) return false;
    g_list->Close();
    if (FAILED(g_dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)))) return false;
    // GPU time of the copies (copy queues need OPTIONS3.CopyQueueTimestampQueriesSupported)
    bool ts = true;
    if (g_queueMode == 1) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS3 o3{};
        ts = SUCCEEDED(g_dev->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &o3, sizeof o3)) && o3.CopyQueueTimestampQueriesSupported;
    }
    if (ts) {
        D3D12_QUERY_HEAP_DESC qd{};
        qd.Type = g_queueMode == 1 ? D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP : D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        qd.Count = 2 * kAlloc;
        D3D12_HEAP_PROPERTIES hp{};
        hp.Type = D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC rd{};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = 16ull * kAlloc;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12CommandQueue* fq = g_queueMode == 1 ? g_cq : g_directQ;
        if (fq && SUCCEEDED(g_dev->CreateQueryHeap(&qd, IID_PPV_ARGS(&g_qh))) &&
            SUCCEEDED(g_dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                     IID_PPV_ARGS(&g_tsBuf))) &&
            SUCCEEDED(g_tsBuf->Map(0, nullptr, reinterpret_cast<void**>(&g_tsMap))) && SUCCEEDED(fq->GetTimestampFrequency(&g_tsFreq)) &&
            g_tsFreq) {
        } else {
            g_tsMap = nullptr;
        }
    }
    g_infraOk = true;
    SLOG("SBM: copy path ready: %s, %d lists in flight, GPU copy timing %s", g_queueMode == 1 ? "own COPY queue (DMA)" : "game DIRECT queue (CP DMA)",
         kAlloc, g_tsMap ? "on" : "off (no copy-queue timestamps)");
    return true;
}

ID3D12Resource* MakeTwin(uint64_t width) {
    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = width;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ID3D12Resource* t = nullptr;
    if (FAILED(g_dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&t))))
        return nullptr;
    t->SetName(L"Nioh3EngineFix SceneBufMirror twin");
    return t;
}

// every redirected SRV back onto its original resource (failure path); twins are kept (in-flight lists may read them)
void RestoreAllLocked(const char* why) {
    int n = 0;
    for (auto& kv : g_res)
        for (SrvRec& s : kv.second.srv)
            if (s.redir && g_srvOrig && g_srvDev) {
                D3D12_CPU_DESCRIPTOR_HANDLE h{s.h};
                g_srvOrig(g_srvDev, kv.first, &s.d, h);
                s.redir = false;
                ++n;
            }
    SLOG("SBM: OFF for the rest of the session (%s) - %d SRVs put back on the original resources", why, n);
}
void Fail(const char* why) {
    if (InterlockedExchange(&g_failed, 1)) return;
    AcquireSRWLockExclusive(&g_lock);
    RestoreAllLocked(why);
    ReleaseSRWLockExclusive(&g_lock);
}

bool Active() {
    if (g_failed || !g_mode) return false;
    if (g_mode == 2) return true;
    return rebar::UploadInSystemMemory() == 1;
}

// ---- late-write check (probe + mirror): hash the copied ranges right after the copy; the same ranges again at the next
// hook (the other set is current then, nobody should write this one) ----
struct CheckRange {
    ID3D12Resource* r;
    uint64_t off, len;
};
std::vector<CheckRange> g_chk;
uint64_t g_chkHash = 0;
uint64_t g_chkSet = 0;
bool g_chkArmed = false;
LONG64 g_chkNextFrame = 0;
uint64_t HashMem(const uint8_t* p, uint64_t n, uint64_t h) {   // FNV-1a over 8-byte words
    __try {
        for (uint64_t i = 0; i + 8 <= n; i += 8) {
            uint64_t v;
            memcpy(&v, p + i, 8);
            h = (h ^ v) * 0x100000001B3ull;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        h ^= 0xDEADu;
    }
    return h;
}
uint64_t HashRanges(const std::vector<CheckRange>& v) {
    uint64_t h = 0xCBF29CE484222325ull;
    for (const CheckRange& c : v) {
        uint8_t* p = nullptr;
        if (FAILED(c.r->Map(0, nullptr, reinterpret_cast<void**>(&p))) || !p) return 0;
        h = HashMem(p + c.off, c.len, h);
        D3D12_RANGE none{0, 0};
        c.r->Unmap(0, &none);
    }
    return h;
}

struct Op {
    ID3D12Resource *dst, *src;
    uint64_t off, len;
};

void Mirror(uint64_t set, const Buf* b) {
    if (!g_eclLive) return;   // the queue hook must be inserting Waits before any SRV points at a twin
    ID3D12Resource* first = nullptr;
    for (int i = 0; i < kSlots && !first; ++i)
        if (b[i].usage == kUsageUpload && b[i].res && b[i].cap) first = (ID3D12Resource*)b[i].res;
    if (!first) return;
    if (!EnsureInfra(first)) {
        Fail("copy path not created");
        return;
    }
    ID3D12CommandQueue* q = g_queueMode == 1 ? g_cq : g_directQ;
    if (!q) return;
    // late-write check of the previous checked set (probe mode, ~every 30 s)
    if (g_chkArmed && set != g_chkSet) {   // the same set again (a second update in one frame) is rewritten legitimately
        g_chkArmed = false;
        const uint64_t h = HashRanges(g_chk);
        InterlockedIncrement64(&c_checks);
        if (h != g_chkHash) {
            InterlockedIncrement64(&c_late);
            SLOG("SBM: LATE CPU WRITE - a mirrored scene buffer changed after its copy (%zu ranges); the mirror would read stale data",
                 g_chk.size());
            Fail("late CPU write into a mirrored buffer");
            return;
        }
    }
    std::vector<Op> ops;
    std::vector<std::pair<ID3D12Resource*, Range>> cur;
    AcquireSRWLockExclusive(&g_lock);
    for (int i = 0; i < kSlots; ++i) {
        const Buf& s = b[i];
        if (s.usage != kUsageUpload || !s.res || !s.cap) continue;
        ID3D12Resource* r = (ID3D12Resource*)s.res;
        Res& x = g_res[r];
        if (!x.width) {
            const D3D12_RESOURCE_DESC rd = r->GetDesc();
            if (rd.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) continue;
            x.width = rd.Width;
            x.va = r->GetGPUVirtualAddress();
            x.heap = UploadKind(r);
        }
        const uint64_t lo = s.off, hi = std::min<uint64_t>(s.off + s.cap, x.width);
        if (hi <= lo) continue;
        // the last map's range [woff, woff + wlen); a partial last map (woff != 0) -> the whole capacity
        uint64_t len = s.wlen && !s.woff ? std::min<uint64_t>(s.wlen, s.cap) : s.cap;
        len = std::min<uint64_t>(len, x.width - lo);
        bool have = false;
        for (Range& rg : x.scene)
            if (rg.lo == lo && rg.hi == hi) have = true;
        if (!have) x.scene.push_back(Range{lo, hi, false});
        if (!x.twin) {
            if (g_twinBytes + x.width > kTwinCap) {
                InterlockedIncrement64(&c_capSkip);
                continue;
            }
            x.twin = MakeTwin(x.width);
            if (!x.twin) {
                ReleaseSRWLockExclusive(&g_lock);
                Fail("twin buffer not created");
                return;
            }
            r->AddRef();
            x.held = true;
            g_twinBytes += x.width;
        }
        ops.push_back(Op{x.twin, r, lo, len});
        cur.push_back({r, Range{lo, hi, true}});
    }
    ReleaseSRWLockExclusive(&g_lock);
    if (ops.empty()) return;
    std::sort(ops.begin(), ops.end(), [](const Op& a, const Op& c) { return a.dst != c.dst ? a.dst < c.dst : a.off < c.off; });
    // record + submit one list
    const int k = g_next;
    g_next = (g_next + 1) % kAlloc;
    if (g_allocFence[k] && g_fence->GetCompletedValue() < g_allocFence[k]) {
        InterlockedIncrement64(&c_cpuWaits);
        HANDLE e = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (e && SUCCEEDED(g_fence->SetEventOnCompletion(g_allocFence[k], e))) WaitForSingleObject(e, 2000);
        if (e) CloseHandle(e);
    }
    if (g_tsPending[k] && g_tsMap && g_fence->GetCompletedValue() >= g_allocFence[k]) {
        const uint64_t t0 = g_tsMap[2 * k], t1 = g_tsMap[2 * k + 1];
        if (t1 > t0) {
            InterlockedIncrement64(&c_tsN);
            InterlockedAdd64(&c_tsTicks, (LONG64)(t1 - t0));
            InterlockedAdd64(&c_tsBytes, (LONG64)g_slotBytes[k]);
        }
        g_tsPending[k] = false;
    }
    if (FAILED(g_alloc[k]->Reset()) || FAILED(g_list->Reset(g_alloc[k], nullptr))) {
        Fail("copy list reset failed");
        return;
    }
    if (g_tsMap) g_list->EndQuery(g_qh, D3D12_QUERY_TYPE_TIMESTAMP, 2 * k);
    uint64_t bytes = 0;
    int n = 0;
    for (size_t i = 0; i < ops.size();) {
        Op o = ops[i++];
        while (i < ops.size() && ops[i].dst == o.dst && ops[i].off <= o.off + o.len) {   // merge touching / overlapping ranges
            o.len = std::max(o.len, ops[i].off + ops[i].len - o.off);
            ++i;
        }
        g_list->CopyBufferRegion(o.dst, o.off, o.src, o.off, o.len);
        bytes += o.len;
        ++n;
    }
    if (g_tsMap) {
        g_list->EndQuery(g_qh, D3D12_QUERY_TYPE_TIMESTAMP, 2 * k + 1);
        g_list->ResolveQueryData(g_qh, D3D12_QUERY_TYPE_TIMESTAMP, 2 * k, 2, g_tsBuf, 16ull * k);
    }
    if (FAILED(g_list->Close())) {
        Fail("copy list record failed");
        return;
    }
    ID3D12CommandList* l = g_list;
    q->ExecuteCommandLists(1, &l);
    const uint64_t v = (uint64_t)g_v + 1;
    if (FAILED(q->Signal(g_fence, v))) {
        Fail("fence signal failed");
        return;
    }
    g_allocFence[k] = v;
    g_slotBytes[k] = bytes;
    g_tsPending[k] = g_tsMap != nullptr;
    InterlockedExchange64(&g_v, (LONG64)v);   // from here on the queue hook makes the game queues wait for this copy
    InterlockedIncrement64(&c_frames);
    InterlockedAdd64(&c_copies, n);
    InterlockedAdd64(&c_bytes, (LONG64)bytes);
    // redirect this set's SRVs (their twin ranges were just copied; their readers are recorded after this point)
    AcquireSRWLockExclusive(&g_lock);
    for (auto& c : cur) {
        auto it = g_res.find(c.first);
        if (it == g_res.end()) continue;
        Res& x = it->second;
        for (Range& rg : x.scene)
            if (rg.lo == c.second.lo && rg.hi == c.second.hi) rg.copied = true;
        bool any = false;
        for (SrvRec& s : x.srv) {
            if (s.hi > s.lo && s.lo >= c.second.lo && s.hi <= c.second.hi) any = true;
            if (s.redir || s.hi <= s.lo || s.lo < c.second.lo || s.hi > c.second.hi) continue;
            D3D12_CPU_DESCRIPTOR_HANDLE h{s.h};
            g_srvOrig(g_srvDev, x.twin, &s.d, h);
            s.redir = true;
            InterlockedIncrement64(&c_rewrite);
        }
        if (!any) InterlockedIncrement64(&c_noSrv);
    }
    ReleaseSRWLockExclusive(&g_lock);
    // arm the late-write check for this set (probe mode, every ~30 s of frames)
    if (g_probe && c_frames >= g_chkNextFrame) {
        g_chk.clear();
        for (const Op& o : ops) g_chk.push_back(CheckRange{o.src, o.off, o.len});
        g_chkHash = HashRanges(g_chk);
        g_chkSet = set;
        g_chkArmed = true;
        g_chkNextFrame = c_frames + 900;
    }
}

// probe snapshot for the log (last two sets seen)
struct Snap {
    uint64_t sys, set;
    Buf b[kSlots];
};
Snap g_snap[2];
int g_snapN = 0;
SRWLOCK g_snapLock = SRWLOCK_INIT;
std::unordered_map<uint64_t, int> g_kind;   // census: resource -> heap kind, read while the engine holds it (right after its map)

void AfterUpdate(uint64_t sys) {
    InterlockedIncrement64(&c_calls);
    uint64_t set = 0;
    Buf b[kSlots];
    if (!ReadSet(sys, &set, b)) return;
    if (g_probe) {
        AcquireSRWLockExclusive(&g_snapLock);
        int i = 0;
        for (; i < g_snapN; ++i)
            if (g_snap[i].set == set) break;
        if (i == g_snapN) i = g_snapN < 2 ? g_snapN++ : (int)(c_calls & 1);
        g_snap[i].sys = sys;
        g_snap[i].set = set;
        memcpy(g_snap[i].b, b, sizeof b);
        for (int k = 0; k < kSlots; ++k)
            if (b[k].res && !g_kind.count(b[k].res) && g_kind.size() < 4096) g_kind[b[k].res] = CensusKind((ID3D12Resource*)b[k].res);
        ReleaseSRWLockExclusive(&g_snapLock);
    }
    if (Active() && g_srvOrig) Mirror(set, b);
}

void HkUpdate(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7) {
    g_orig(a1, a2, a3, a4, a5, a6, a7);
    AfterUpdate(a1);
}

// ---- patching (autocont.cpp scheme) ----
uint8_t* AllocIn(uintptr_t from, uintptr_t to, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + size <= re && c + size <= to)
                if (void* p = VirtualAlloc((void*)c, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}
// the 5 bytes at 5D417 sit inside one 16-byte window (5D410..5D41F): one 16-byte compare-exchange
bool PatchAtomic16(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w16 = a & ~uintptr_t(15);
    if (a + len > w16 + 16) return false;
    DWORD old = 0;
    if (!VirtualProtect((void*)w16, 16, PAGE_EXECUTE_READWRITE, &old)) return false;
    volatile LONG64* q = (volatile LONG64*)w16;
    alignas(16) LONG64 cur[2] = {q[0], q[1]};
    LONG64 nw[2] = {cur[0], cur[1]};
    memcpy((uint8_t*)nw + (a - w16), bytes, len);
    const bool ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    VirtualProtect((void*)w16, 16, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)w16, 16);
    return ok;
}

const char* HeapName(int h) {   // CensusKind values
    switch (h) {
        case 1: return "DEFAULT";
        case 2: return "UPLOAD";
        case 3: return "READBACK";
        case 4: return "CUSTOM";
        case 14: return "CUSTOM-L0(sys)";
        case 5: return "GPU_UPLOAD";
        case -2: return "sys-va";
        case -3: return "va?";
        default: return "-";
    }
}

}  // namespace

void Configure(const wchar_t* iniPath, void (*log)(const char*, ...)) {
    if (g_configured || !iniPath) return;
    g_configured = true;
    if (log) g_log = log;
    g_mode = (int)GetPrivateProfileIntW(L"GpuOpt", L"SceneBufMirror", 0, iniPath);
    if (g_mode < 0 || g_mode > 2) g_mode = 0;
    g_queueMode = GetPrivateProfileIntW(L"GpuOpt", L"SceneBufMirrorQueue", 1, iniPath) == 0 ? 0 : 1;
    g_probe = GetPrivateProfileIntW(L"Profiler", L"SceneBufProbe", 0, iniPath) != 0 ? 1 : 0;
    HMODULE n = GetModuleHandleW(L"ntdll.dll");
    if (g_mode && n && GetProcAddress(n, "wine_get_version")) {
        SLOG("SBM: SceneBufMirror = %d refused under Wine / vkd3d-proton - off", g_mode);
        g_mode = 0;
    }
}

bool Wanted() { return g_mode != 0 || g_probe != 0; }

bool Start(const StartInfo& info) {
    if (info.log) g_log = info.log;
    Configure(info.iniPath, info.log);
    g_base = info.base;
    if (!Wanted() || !g_base) return false;
    if (!info.knownBuild) {
        SLOG("SBM: not installed (unknown build; MeshletSys offsets and the 5D417 site are v2.02)");
        g_mode = g_probe = 0;
        return false;
    }
    uint8_t* site = (uint8_t*)(g_base + kSite);
    int32_t rel = 0;
    memcpy(&rel, site + 1, 4);
    if (site[0] != 0xE8 || (uintptr_t)(site + 5 + rel) != g_base + kCallee || memcmp(site - sizeof kBefore, kBefore, sizeof kBefore) != 0 ||
        memcmp(site + 5, kAfter, sizeof kAfter) != 0) {
        SLOG("SBM: not installed (MeshletFrameBegin call 5D417 bytes differ - another patch or build)");
        g_mode = g_probe = 0;
        return false;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    uint8_t* page = AllocNear(g_base, g_base + nt->OptionalHeader.SizeOfImage, 4096);
    if (!page) {
        SLOG("SBM: not installed (no executable page within 2 GB of the image)");
        g_mode = g_probe = 0;
        return false;
    }
    g_orig = (UpdateFn)(g_base + kCallee);
    // relay: mov rax, imm64 (HkUpdate); jmp rax  (rax is not an argument; the callee clobbers it anyway)
    page[0] = 0x48;
    page[1] = 0xB8;
    const uint64_t hk = (uint64_t)(uintptr_t)&HkUpdate;
    memcpy(page + 2, &hk, 8);
    page[10] = 0xFF;
    page[11] = 0xE0;
    FlushInstructionCache(GetCurrentProcess(), page, 16);
    const intptr_t d = (intptr_t)page - (intptr_t)(site + 5);
    if (d < INT32_MIN || d > INT32_MAX) {
        SLOG("SBM: not installed (relay out of rel32 reach)");
        g_mode = g_probe = 0;
        return false;
    }
    uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
    const int32_t nrel = (int32_t)d;
    memcpy(nb + 1, &nrel, 4);
    if (!PatchAtomic16(site, nb, 5)) {
        SLOG("SBM: not installed (site write failed)");
        g_mode = g_probe = 0;
        return false;
    }
    InterlockedExchange(&g_on, 1);
    SLOG("SBM: hook on MeshletFrameBegin -> MeshletUpdateSceneBuffers (5D417) installed; SceneBufMirror = %d (%s), queue %s, "
         "SceneBufProbe = %d",
         g_mode, g_mode == 0 ? "off - census only" : g_mode == 1 ? "auto: only when the UPLOAD heaps are in system memory" : "FORCE - test",
         g_queueMode == 1 ? "own COPY queue" : "game DIRECT queue", g_probe);
    return true;
}

void OnSrv(ID3D12Device* dev, ID3D12Resource* res, const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE h,
           ID3D12Resource** use) {
    *use = res;
    if (!g_on) return;
    if (!g_srvDev && dev) g_srvDev = dev;
    bool owned = false;
    AcquireSRWLockShared(&g_lock);
    owned = !g_owner.empty() && g_owner.count(h.ptr) != 0;
    ReleaseSRWLockShared(&g_lock);
    const bool isBuf = res && desc && desc->ViewDimension == D3D12_SRV_DIMENSION_BUFFER;
    if (!isBuf && !owned) return;
    const int kind = isBuf ? UploadKind(res) : 0;
    if (!kind && !owned) return;
    AcquireSRWLockExclusive(&g_lock);
    if (owned) {   // the engine writes a new view into a handle we recorded: forget the old record
        auto it = g_owner.find(h.ptr);
        if (it != g_owner.end()) {
            auto r = g_res.find(it->second);
            if (r != g_res.end()) {
                auto& v = r->second.srv;
                v.erase(std::remove_if(v.begin(), v.end(), [&](const SrvRec& s) { return s.h == h.ptr; }), v.end());
            }
            g_owner.erase(it);
        }
    }
    if (kind) {
        Res& x = g_res[res];
        const uint64_t va = res->GetGPUVirtualAddress();
        if (x.va && x.va != va && !x.held) x = Res{};   // a new resource at a recycled address
        if (!x.width) {
            x.width = res->GetDesc().Width;
            x.va = va;
            x.heap = kind;
        }
        if (x.srv.size() < 4096) {
            SrvRec s{h.ptr, *desc, 0, 0, false};
            SrvRange(*desc, &s.lo, &s.hi);
            if (Active() && x.twin && InCopied(x, s.lo, s.hi)) {
                *use = x.twin;
                s.redir = true;
                InterlockedIncrement64(&c_atCreate);
            }
            x.srv.push_back(s);
            g_owner[h.ptr] = res;
            InterlockedIncrement64(&c_srvSeen);
        }
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void TestSetup(void (*log)(const char*, ...), int mode, int queueMode, int probe) {
    g_log = log;
    g_configured = true;
    g_mode = mode;
    g_queueMode = queueMode ? 1 : 0;
    g_probe = probe;
    InterlockedExchange(&g_on, 1);
}
void TestAfterUpdate(uint64_t meshletSys) { AfterUpdate(meshletSys); }

void SetSrvOriginal(SrvFn fn) { g_srvOrig = fn; }
void SetSysVaTest(bool (*fn)(uint64_t)) { g_sysVa = fn; }

void OnEcl(ID3D12CommandQueue* q) {
    if (!g_on || !q) return;
    if (!g_eclLive) {
        if (q->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT) {
            AcquireSRWLockExclusive(&g_qlock);
            if (!g_directQ) {
                q->AddRef();
                g_directQ = q;
            }
            ReleaseSRWLockExclusive(&g_qlock);
            InterlockedExchange(&g_eclLive, 1);
        }
    }
    const uint64_t v = (uint64_t)g_v;
    if (!v || q == g_cq) return;
    if (g_queueMode == 0 && q == g_directQ) return;   // our copies run on this queue: in order already (a Wait could deadlock)
    AcquireSRWLockExclusive(&g_qlock);
    QState* e = nullptr;
    for (int i = 0; i < g_nq; ++i)
        if (g_qs[i].q == q) e = &g_qs[i];
    if (!e && g_nq < 16) {
        const D3D12_COMMAND_LIST_TYPE t = q->GetDesc().Type;
        g_qs[g_nq] = QState{q, (t == D3D12_COMMAND_LIST_TYPE_DIRECT || t == D3D12_COMMAND_LIST_TYPE_COMPUTE) ? 1 : 0, 0};
        e = &g_qs[g_nq++];
    }
    if (e && e->kind && e->waited < v) {
        q->Wait(g_fence, v);
        e->waited = v;
        InterlockedIncrement64(&c_waits);
        if (g_fence->GetCompletedValue() < v) InterlockedIncrement64(&c_pend);
    }
    ReleaseSRWLockExclusive(&g_qlock);
}

void LogLine() {
    if (!g_on) return;
    static LONG64 p[12] = {};
    const LONG64 v[12] = {c_calls, c_frames, c_copies, c_bytes, c_waits, c_pend, c_tsN, c_tsTicks, c_tsBytes, c_rewrite, c_atCreate, c_cpuWaits};
    LONG64 d[12];
    for (int i = 0; i < 12; ++i) d[i] = v[i] - p[i], p[i] = v[i];
    // retire twins whose resource only we still hold (the engine released the buffer)
    int nTwin = 0, nRes = 0;
    AcquireSRWLockExclusive(&g_lock);
    for (auto it = g_res.begin(); it != g_res.end();) {
        Res& x = it->second;
        if (x.held && x.twin) {
            const ULONG c = it->first->AddRef();
            it->first->Release();
            if (c <= 2) {
                for (const SrvRec& s : x.srv) g_owner.erase(s.h);
                g_twinBytes -= std::min(g_twinBytes, x.width);
                x.twin->Release();
                it->first->Release();
                it = g_res.erase(it);
                InterlockedIncrement64(&c_retired);
                continue;
            }
            ++nTwin;
        }
        ++nRes;
        ++it;
    }
    const double twinMB = (double)g_twinBytes / 1048576.0;
    ReleaseSRWLockExclusive(&g_lock);
    const double gpuMs = d[6] && g_tsFreq ? (double)d[7] * 1000.0 / (double)g_tsFreq / (double)d[6] : -1.0;
    const double gbps = d[7] && g_tsFreq ? (double)d[8] / ((double)d[7] / (double)g_tsFreq) / 1e9 : -1.0;
    SLOG("stats/s SBM: %s | hook calls %.1f/s, copy lists %.1f/s, copies %.1f/frame, %.3f MB/frame (%.2f MB/s) | GPU copy %.3f ms/frame "
         "(%lld samples) = %.2f GB/s | queue waits %.1f/s (copy not done yet at insert %.0f %%), CPU allocator waits %lld | twins %d "
         "(%.1f MB), resources %d, SRVs seen %lld, redirected %lld (+%lld at creation), sets without a redirectable SRV %lld, "
         "cap skips %lld, retired %lld | late-write checks %lld, LATE %lld",
         g_failed ? "FAILED (off)" : Active() ? "ON" : g_mode ? "armed (auto: UPLOAD heaps in VRAM / not decided)" : "census only",
         d[0] / 30.0, d[1] / 30.0, d[1] ? (double)d[2] / d[1] : 0.0, d[1] ? (double)d[3] / d[1] / 1048576.0 : 0.0,
         (double)d[3] / 30.0 / 1048576.0, gpuMs, (long long)d[6], gbps, d[4] / 30.0, d[4] ? 100.0 * d[5] / d[4] : 0.0, (long long)d[11],
         nTwin, twinMB, nRes, (long long)c_srvSeen, (long long)c_rewrite, (long long)c_atCreate, (long long)c_noSrv,
         (long long)c_capSkip, (long long)c_retired, (long long)c_checks, (long long)c_late);
    if (!g_probe) return;
    Snap s[2];
    int n;
    AcquireSRWLockShared(&g_snapLock);
    n = g_snapN;
    memcpy(s, g_snap, sizeof s);
    ReleaseSRWLockShared(&g_snapLock);
    for (int k = 0; k < n; ++k) {
        char line[2600];
        size_t o = (size_t)_snprintf_s(line, sizeof line, _TRUNCATE, "SBM set %llX (MeshletSys %llX): slot usage res off cap written heap srv(in/redir):",
                                       (unsigned long long)s[k].set, (unsigned long long)s[k].sys);
        for (int i = 0; i < kSlots && o < sizeof line - 160; ++i) {
            const Buf& b = s[k].b[i];
            if (!b.obj) continue;
            int heap = -1, srv = 0, in = 0, red = 0;
            AcquireSRWLockShared(&g_snapLock);
            if (auto kt = g_kind.find(b.res); kt != g_kind.end()) heap = kt->second;
            ReleaseSRWLockShared(&g_snapLock);
            AcquireSRWLockShared(&g_lock);
            auto it = g_res.find((ID3D12Resource*)b.res);
            if (it != g_res.end()) {
                if (heap < 0) heap = it->second.heap;
                for (const SrvRec& r : it->second.srv) {
                    ++srv;
                    if (r.hi > r.lo && r.lo >= b.off && r.hi <= b.off + b.cap) {
                        ++in;
                        red += r.redir;
                    }
                }
            }
            ReleaseSRWLockShared(&g_lock);
            o += (size_t)_snprintf_s(line + o, sizeof line - o, _TRUNCATE, " +%X u%d %llX %llX %u %llu %s %d(%d/%d);", 0x18 + 8 * i, b.usage,
                                     (unsigned long long)b.res, (unsigned long long)b.off, b.cap, (unsigned long long)b.wlen,
                                     HeapName(heap), srv, in, red);
        }
        SLOG("%s", line);
    }
}

}  // namespace scenemirror
