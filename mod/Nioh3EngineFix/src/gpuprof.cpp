// gpuprof.cpp - GP: in-process GPU pass profiler (see gpuprof.h). [Profiler] GPUProf = 0 by default.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gpuprof.h"

extern "C" {
extern void* gpuopt_orig0[96];  // gpuopt_thunks.asm: the original command-list methods per vtable set
extern void* gpuopt_orig1[96];
}

namespace gp {
namespace {

// ---- v2.02 engine layout (verified by dllmain: GP check sites, deps, token table) -------------------------------------
constexpr uint32_t kRvaG = 0x45BA588;       // KtglContext* g
constexpr uint32_t kOffRecord = 0x2A62;     // record flag (GfxBeginMarker 1D3AD8 / GfxEndMarker 113CA4)
constexpr uint32_t kOffReplay = 0x552;      // replay flag = SynCtx (g+10h) + 542h (SynBeginEvent / SynTokEndEvent)
constexpr uint32_t kRvaIface = 0x4BD09C0;   // optional marker interface (when set, BeginEvent is not called)

// command-list vtable slots
constexpr int kSlotGetDevice = 7, kSlotGetType = 8, kSlotClose = 9, kSlotReset = 10, kSlotEndQuery = 53,
              kSlotResolve = 54, kSlotBeginEvent = 57, kSlotEndEvent = 58;

constexpr int kRing = 8;        // frame slots of the query heap; a slot is read 6 frames after it was current (the
                                // dispatcher records behind the Present thread, and the GPU queues frames)
constexpr int kChunk = 64;      // queries a list takes at a time (resolved as one range on that list)
constexpr int kMaxNames = 2048;
constexpr int kMaxDepth = 64;
constexpr int kMaxTid = 8;

struct Cfg {
    int prof = 0, markers = 0, maxMarkers = 2048, top = 30;
};
Cfg g_cfg;
void (*g_log)(const char*, ...) = nullptr;
uint8_t* g_base = nullptr;
bool g_codeOk = false;
std::string g_wine;

#define GLOG(fmt, ...) do { if (g_log) g_log("GP: " fmt, __VA_ARGS__); } while (0)

volatile LONG g_on = 0;         // sampling (queries) active
volatile LONG g_failed = 0;
volatile LONG g_inited = 0;     // 0 not yet, 1 resources ready, -1 failed
SRWLOCK g_initLock = SRWLOCK_INIT;
ID3D12Device* g_dev = nullptr;
ID3D12QueryHeap* g_heap = nullptr;
ID3D12Resource* g_rb = nullptr;
volatile uint64_t* g_rbMap = nullptr;
double g_ticksPerMs = 0;
int g_qPerSlot = 0;
volatile LONG64 g_frame = 0;

using EndQueryFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT);
using ResolveFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT, UINT,
                                          ID3D12Resource*, UINT64);
using GetTypeFn = D3D12_COMMAND_LIST_TYPE(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*);
using GetDeviceFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, REFIID, void**);

void* Orig(int set, int slot) { return (set ? gpuopt_orig1 : gpuopt_orig0)[slot]; }

// ---- counters ------------------------------------------------------------------------------------------------------------
volatile LONG64 g_begins = 0, g_ends = 0, g_full = 0, g_unmatched = 0, g_badTs = 0, g_skippedLists = 0, g_recFull = 0;

// ---- names -----------------------------------------------------------------------------------------------------------------
struct Name {
    char s[64];
    uint32_t hash;
};
Name g_names[kMaxNames];
volatile LONG g_nameCount = 1;  // 0 = "(other)"
SRWLOCK g_nameLock = SRWLOCK_INIT;
int16_t g_nameIdx[4096];       // open addressing: hash -> name id (0 = empty)

uint32_t Fnv(const char* s) {
    uint32_t h = 0x811C9DC5u;
    for (; *s; ++s) h = (h ^ (uint8_t)*s) * 0x01000193u;
    return h;
}

// UTF-16 (the engine) or ANSI (PIX metadata 1) -> ASCII; two or more numbers ("Tile 3 5") -> '#' so tiles aggregate
int InternName(UINT meta, const void* data, UINT size) {
    char buf[64];
    int n = 0;
    if (data && size) {
        const uint8_t* p = (const uint8_t*)data;
        const bool wide = meta != 1 && size >= 2 && p[1] == 0;
        if (wide) {
            const wchar_t* w = (const wchar_t*)data;
            for (UINT i = 0; i < size / 2 && n < 63 && w[i]; ++i) buf[n++] = (w[i] >= 32 && w[i] < 127) ? (char)w[i] : '?';
        } else {
            const char* c = (const char*)data;
            for (UINT i = 0; i < size && n < 63 && c[i]; ++i) buf[n++] = (c[i] >= 32 && c[i] < 127) ? c[i] : '?';
        }
    }
    if (!n) { strcpy_s(buf, "?"); n = 1; }
    buf[n] = 0;
    int runs = 0;
    for (int i = 0; i < n; ++i)
        if (buf[i] >= '0' && buf[i] <= '9' && (i == 0 || buf[i - 1] < '0' || buf[i - 1] > '9')) ++runs;
    if (runs >= 2) {
        char o[64];
        int m = 0;
        for (int i = 0; i < n && m < 63; ++i) {
            const bool d = buf[i] >= '0' && buf[i] <= '9';
            if (!d) o[m++] = buf[i];
            else if (i == 0 || buf[i - 1] < '0' || buf[i - 1] > '9') o[m++] = '#';
        }
        o[m] = 0;
        memcpy(buf, o, (size_t)m + 1);
    }
    const uint32_t h = Fnv(buf);
    AcquireSRWLockShared(&g_nameLock);
    for (uint32_t k = 0; k < 4096; ++k) {
        const int id = g_nameIdx[(h + k) & 4095];
        if (!id) break;
        if (g_names[id].hash == h && !strcmp(g_names[id].s, buf)) {
            ReleaseSRWLockShared(&g_nameLock);
            return id;
        }
    }
    ReleaseSRWLockShared(&g_nameLock);
    AcquireSRWLockExclusive(&g_nameLock);
    int found = 0;
    uint32_t k = 0;
    for (; k < 4096; ++k) {
        const int id = g_nameIdx[(h + k) & 4095];
        if (!id) break;
        if (g_names[id].hash == h && !strcmp(g_names[id].s, buf)) { found = id; break; }
    }
    if (!found && g_nameCount < kMaxNames && k < 4096) {
        found = g_nameCount;
        strcpy_s(g_names[found].s, buf);
        g_names[found].hash = h;
        g_nameIdx[(h + k) & 4095] = (int16_t)found;
        InterlockedIncrement(&g_nameCount);
    }
    ReleaseSRWLockExclusive(&g_nameLock);
    return found;  // 0 = "(other)" when the table is full
}

// ---- frame slots and records ------------------------------------------------------------------------------------------------
struct Rec {
    int32_t b, e;     // query indices (absolute)
    int16_t name;
    uint8_t depth, tid;
};
struct Slot {
    SRWLOCK lock;
    volatile LONG used;  // queries handed out (in chunks)
    std::vector<Rec> recs;
};
Slot g_slot[kRing];

// ---- per-list query chunks ----------------------------------------------------------------------------------------------
struct ListState {
    void* list;
    int set;
    int slot;   // frame slot of the current chunk
    int base;   // first query index of the chunk, -1 = none
    int used;   // queries ended in the chunk
    int ok;     // 1 DIRECT/COMPUTE list of our device, 0 skip
};
constexpr int kLists = 64;
ListState g_ls[kLists];
SRWLOCK g_lsLock = SRWLOCK_INIT;

ListState* FindList(void* list, int set, bool create) {
    ListState* freeE = nullptr;
    for (ListState& s : g_ls) {
        if (s.list == list) return &s;
        if (!s.list && !freeE) freeE = &s;
    }
    if (!create || !freeE) return nullptr;
    ListState& s = *freeE;
    s.list = list;
    s.set = set;
    s.slot = -1;
    s.base = -1;
    s.used = 0;
    s.ok = 0;
    auto cl = (ID3D12GraphicsCommandList*)list;
    const D3D12_COMMAND_LIST_TYPE t = reinterpret_cast<GetTypeFn>(Orig(set, kSlotGetType))(cl);
    if (t == D3D12_COMMAND_LIST_TYPE_DIRECT || t == D3D12_COMMAND_LIST_TYPE_COMPUTE) {
        ID3D12Device* d = nullptr;
        if (SUCCEEDED(reinterpret_cast<GetDeviceFn>(Orig(set, kSlotGetDevice))(cl, __uuidof(ID3D12Device), (void**)&d)) && d) {
            s.ok = d == g_dev ? 1 : 0;
            d->Release();
        }
    }
    if (!s.ok) InterlockedIncrement64(&g_skippedLists);
    return &s;
}

void ResolveChunk(ListState& s) {
    if (s.base >= 0 && s.used > 0)
        reinterpret_cast<ResolveFn>(Orig(s.set, kSlotResolve))((ID3D12GraphicsCommandList*)s.list, g_heap,
                                                              D3D12_QUERY_TYPE_TIMESTAMP, (UINT)s.base, (UINT)s.used, g_rb,
                                                              (UINT64)s.base * 8);
    s.base = -1;
    s.used = 0;
}

// a timestamp on `list` now; returns its query index or -1 (not sampled)
int Stamp(void* list, int set) {
    AcquireSRWLockExclusive(&g_lsLock);
    int idx = -1;
    ListState* s = FindList(list, set, true);
    if (s && s->ok) {
        const int cur = (int)(g_frame % kRing);
        if (s->base >= 0 && (s->slot != cur || s->used >= kChunk)) ResolveChunk(*s);
        if (s->base < 0) {
            const LONG off = InterlockedExchangeAdd(&g_slot[cur].used, kChunk);
            if (off + kChunk <= g_qPerSlot) {
                s->base = cur * g_qPerSlot + off;
                s->slot = cur;
                s->used = 0;
            } else {
                InterlockedIncrement64(&g_full);
            }
        }
        if (s->base >= 0) {
            idx = s->base + s->used++;
            reinterpret_cast<EndQueryFn>(Orig(set, kSlotEndQuery))((ID3D12GraphicsCommandList*)list, g_heap,
                                                                  D3D12_QUERY_TYPE_TIMESTAMP, (UINT)idx);
        }
    }
    ReleaseSRWLockExclusive(&g_lsLock);
    return idx;
}

// ---- per-thread marker stack -------------------------------------------------------------------------------------------------
struct Open {
    int q;
    int16_t name;
};
thread_local Open t_stack[kMaxDepth];
thread_local int t_depth = 0;
thread_local int t_tid = -1;
volatile LONG g_tidNext = 0;

// ---- init (first BeginEvent, recording thread) ----------------------------------------------------------------------------------
bool Init(void* list, int set) {
    auto cl = (ID3D12GraphicsCommandList*)list;
    ID3D12Device* dev = nullptr;
    if (FAILED(reinterpret_cast<GetDeviceFn>(Orig(set, kSlotGetDevice))(cl, __uuidof(ID3D12Device), (void**)&dev)) || !dev) {
        GLOG("%s", "GetDevice failed - off");
        return false;
    }
    g_qPerSlot = g_cfg.maxMarkers * 2;
    const UINT total = (UINT)(g_qPerSlot * kRing);
    D3D12_QUERY_HEAP_DESC qd = {};
    qd.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    qd.Count = total;
    HRESULT hr = dev->CreateQueryHeap(&qd, __uuidof(ID3D12QueryHeap), (void**)&g_heap);
    if (FAILED(hr) || !g_heap) {
        GLOG("CreateQueryHeap(%u timestamps) failed (hr %08lX) - off", total, (unsigned long)hr);
        dev->Release();
        return false;
    }
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC rd = {};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = (UINT64)total * 8;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    hr = dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                      __uuidof(ID3D12Resource), (void**)&g_rb);
    if (FAILED(hr) || !g_rb) {
        GLOG("readback buffer (%u KB) failed (hr %08lX) - off", total * 8 / 1024, (unsigned long)hr);
        dev->Release();
        return false;
    }
    void* p = nullptr;
    D3D12_RANGE all = {0, (SIZE_T)total * 8};
    hr = g_rb->Map(0, &all, &p);
    if (FAILED(hr) || !p) {
        GLOG("Map(readback) failed (hr %08lX) - off", (unsigned long)hr);
        dev->Release();
        return false;
    }
    g_rbMap = (volatile uint64_t*)p;
    memset(p, 0, (size_t)total * 8);
    // timestamp frequency: from a DIRECT queue of the same device (all direct queues share the GPU clock)
    ID3D12CommandQueue* q = nullptr;
    D3D12_COMMAND_QUEUE_DESC cd = {};
    cd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    UINT64 freq = 0;
    hr = dev->CreateCommandQueue(&cd, __uuidof(ID3D12CommandQueue), (void**)&q);
    if (SUCCEEDED(hr) && q) {
        if (FAILED(q->GetTimestampFrequency(&freq))) freq = 0;
        q->Release();
    }
    if (!freq) {
        GLOG("%s", "GetTimestampFrequency failed - off");
        dev->Release();
        return false;
    }
    g_ticksPerMs = (double)freq / 1000.0;
    g_dev = dev;  // keep the reference: the heap and buffer belong to it
    for (Slot& s : g_slot) s.recs.reserve((size_t)g_cfg.maxMarkers);
    GLOG("timestamp queries ready: %u queries (%d frame slots x %d markers), readback %u KB, GPU clock %llu Hz, list %p "
         "(type %d)",
         total, kRing, g_cfg.maxMarkers, total * 8 / 1024, (unsigned long long)freq, list,
         (int)reinterpret_cast<GetTypeFn>(Orig(set, kSlotGetType))(cl));
    return true;
}

void OnListCallImpl(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    if (slot == kSlotBeginEvent || slot == kSlotEndEvent) {
        if (g_inited == 0) {
            AcquireSRWLockExclusive(&g_initLock);
            if (g_inited == 0) {
                const bool ok = Init(list, set);
                InterlockedExchange(&g_inited, ok ? 1 : -1);
                if (!ok) {
                    InterlockedExchange(&g_on, 0);
                    InterlockedExchange(&g_failed, 1);
                }
            }
            ReleaseSRWLockExclusive(&g_initLock);
        }
        if (g_inited != 1) return;
    }
    switch (slot) {
        case kSlotBeginEvent: {
            InterlockedIncrement64(&g_begins);
            const int name = InternName((UINT)a1, (const void*)a2, (UINT)a3);
            const int q = Stamp(list, set);
            if (t_depth < kMaxDepth) t_stack[t_depth] = Open{q, (int16_t)name};
            ++t_depth;
            break;
        }
        case kSlotEndEvent: {
            InterlockedIncrement64(&g_ends);
            if (t_depth <= 0) {
                InterlockedIncrement64(&g_unmatched);
                break;
            }
            --t_depth;
            if (t_depth >= kMaxDepth) break;  // inside an overflowed region: not sampled
            const Open o = t_stack[t_depth];
            if (o.q < 0) break;
            const int e = Stamp(list, set);
            if (e < 0) break;
            if (t_tid < 0) {
                const LONG id = InterlockedIncrement(&g_tidNext) - 1;
                t_tid = id < kMaxTid ? (int)id : kMaxTid - 1;
            }
            Slot& s = g_slot[e / g_qPerSlot];
            AcquireSRWLockExclusive(&s.lock);
            if (s.recs.size() < s.recs.capacity()) s.recs.push_back(Rec{o.q, e, o.name, (uint8_t)t_depth, (uint8_t)t_tid});
            else InterlockedIncrement64(&g_recFull);
            ReleaseSRWLockExclusive(&s.lock);
            break;
        }
        case kSlotClose: {
            if (g_inited != 1) break;
            AcquireSRWLockExclusive(&g_lsLock);
            if (ListState* s = FindList(list, set, false)) {
                if (s->ok) ResolveChunk(*s);  // last commands of the list: every query of the chunk was ended on it
                s->list = nullptr;
            }
            ReleaseSRWLockExclusive(&g_lsLock);
            break;
        }
        case kSlotReset: {
            AcquireSRWLockExclusive(&g_lsLock);
            if (ListState* s = FindList(list, set, false)) s->list = nullptr;  // reused without our Close: forget it
            ReleaseSRWLockExclusive(&g_lsLock);
            break;
        }
        default: break;
    }
}

// ---- aggregation (Present thread) -------------------------------------------------------------------------------------------
struct Agg {
    double incl[kMaxNames], self[kMaxNames];
    uint64_t count[kMaxNames];
    double top = 0, span = 0;
    uint64_t frames = 0, recs = 0;
};
Agg g_win, g_short;   // 30 s log window, ~2 s menu window
SRWLOCK g_aggLock = SRWLOCK_INIT;
char g_status[256] = "";
SRWLOCK g_statusLock = SRWLOCK_INIT;

void ProcessSlot(int r) {
    Slot& S = g_slot[r];
    static double childSum[kMaxTid][kMaxDepth + 2];
    memset(childSum, 0, sizeof(childSum));
    double top = 0;
    uint64_t lo = ~0ull, hi = 0;
    const double maxTicks = g_ticksPerMs * 1000.0;  // a pass longer than 1 s is a bad sample
    AcquireSRWLockExclusive(&S.lock);
    AcquireSRWLockExclusive(&g_aggLock);
    for (const Rec& rc : S.recs) {
        const uint64_t b = g_rbMap[rc.b], e = g_rbMap[rc.e];
        double incl = 0;
        if (b && e && e >= b && (double)(e - b) < maxTicks) incl = (double)(e - b);
        else InterlockedIncrement64(&g_badTs);
        const int d = rc.depth < kMaxDepth ? rc.depth : kMaxDepth - 1;
        double* cs = childSum[rc.tid < kMaxTid ? rc.tid : kMaxTid - 1];
        double self = incl - cs[d + 1];
        if (self < 0) self = 0;
        cs[d + 1] = 0;
        cs[d] += incl;
        for (Agg* a : {&g_win, &g_short}) {
            a->incl[rc.name] += incl;
            a->self[rc.name] += self;
            a->count[rc.name]++;
            a->recs++;
        }
        if (d == 0 && incl > 0) {
            top += incl;
            lo = std::min(lo, b);
            hi = std::max(hi, e);
        }
    }
    for (Agg* a : {&g_win, &g_short}) {
        a->top += top;
        a->span += hi > lo ? (double)(hi - lo) : 0.0;
        a->frames++;
    }
    ReleaseSRWLockExclusive(&g_aggLock);
    const LONG usedNow = S.used;
    const LONG used = std::min<LONG>(usedNow, (LONG)g_qPerSlot);
    if (used > 0) memset((void*)(g_rbMap + (size_t)r * g_qPerSlot), 0, (size_t)used * 8);
    S.recs.clear();
    InterlockedExchange(&S.used, 0);
    ReleaseSRWLockExclusive(&S.lock);
}

std::vector<int> TopNames(const Agg& a, int n) {
    std::vector<int> ids;
    for (int i = 0; i < kMaxNames; ++i)
        if (a.count[i]) ids.push_back(i);
    std::sort(ids.begin(), ids.end(), [&](int x, int y) { return a.incl[x] > a.incl[y]; });
    if ((int)ids.size() > n) ids.resize((size_t)n);
    return ids;
}

const char* NameOf(int id) { return id ? g_names[id].s : "(other names)"; }

DWORD WINAPI MarkerThread(LPVOID) {
    bool on = false;
    int resets = 0;
    LONG64 lastBegins = 0;
    ULONGLONG onAt = 0;
    bool warnedNone = false;
    for (int i = 0;; ++i) {
        uint8_t* g = *reinterpret_cast<uint8_t* volatile*>(g_base + kRvaG);
        if (g) {
            MEMORY_BASIC_INFORMATION a{}, b{};
            const bool okA = VirtualQuery(g + kOffReplay, &a, sizeof(a)) && a.State == MEM_COMMIT &&
                             (a.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE));
            const bool okB = VirtualQuery(g + kOffRecord, &b, sizeof(b)) && b.State == MEM_COMMIT &&
                             (b.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE));
            if (okA && okB) {
                volatile uint8_t* rep = g + kOffReplay;
                volatile uint8_t* rec = g + kOffRecord;
                if (!*rep || !*rec) {
                    if (on && ++resets <= 3) GLOG("engine markers were switched off by the game (record %u, replay %u) - on again", *rec, *rep);
                    *rep = 1;  // replay first: a recorded marker token is then never skipped by the dispatcher
                    Sleep(50);
                    *rec = 1;
                    if (!on) {
                        on = true;
                        onAt = GetTickCount64();
                        const void* iface = *reinterpret_cast<void* volatile*>(g_base + kRvaIface);
                        GLOG("engine debug markers ON (g = %p: record g+2A62h, replay g+552h)%s%s", g,
                             iface ? " - WARNING: the engine's marker interface [4BD09C0] is set, so BeginEvent may not be called" : "",
                             g_cfg.prof ? "" : " - markers only (GPUProf_Markers), no queries");
                    }
                }
            }
        }
        if (on && g_cfg.prof && !warnedNone && GetTickCount64() - onAt > 15000) {
            warnedNone = true;
            if (g_begins == lastBegins) GLOG("%s", "no BeginEvent reached the command-list hook in 15 s (gpuopt hooks not installed, or the marker interface is in use)");
        }
        lastBegins = g_begins;
        Sleep(i < 600 ? 100 : 2000);
    }
}

}  // namespace

void Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_codeOk = info.codeOk;
    g_wine = info.wine ? info.wine : "";
    const wchar_t* ini = info.iniPath ? info.iniPath : L"";
    auto I = [&](const wchar_t* k, int def, int lo, int hi) {
        const int v = (int)GetPrivateProfileIntW(L"Profiler", k, def, ini);
        return v < lo ? lo : v > hi ? hi : v;
    };
    g_cfg.prof = I(L"GPUProf", 0, 0, 1);
    g_cfg.markers = I(L"GPUProf_Markers", 0, 0, 1);
    g_cfg.maxMarkers = I(L"GPUProf_MaxMarkers", 2048, 256, 8192);
    g_cfg.top = I(L"GPUProf_Top", 30, 5, 60);
    if (!g_cfg.prof && !g_cfg.markers) return;
    if (!g_codeOk || !g_base) {
        GLOG("%s", "marker code not verified on this build - GP off");
        g_cfg.prof = g_cfg.markers = 0;
        return;
    }
    for (Slot& s : g_slot) InitializeSRWLock(&s.lock);
    strcpy_s(g_names[0].s, "(other names)");
    if (g_cfg.prof) InterlockedExchange(&g_on, 1);
    GLOG("GPUProf %d, GPUProf_Markers %d, max %d markers/frame, top %d%s", g_cfg.prof, g_cfg.markers, g_cfg.maxMarkers,
         g_cfg.top, g_wine.empty() ? "" : " (Proton: timestamps through vkd3d-proton)");
    if (HANDLE h = CreateThread(nullptr, 0, MarkerThread, nullptr, 0, nullptr)) CloseHandle(h);
}

bool WantsHooks() { return g_cfg.prof != 0 && g_codeOk; }
bool Active() { return g_on != 0 && g_inited == 1; }

void EnableHooks(volatile uint8_t* mask, int slots) {
    if (!WantsHooks() || !mask || slots <= kSlotEndEvent) return;
    mask[kSlotClose] = 1;
    mask[kSlotReset] = 1;
    mask[kSlotBeginEvent] = 1;
    mask[kSlotEndEvent] = 1;
    GLOG("%s", "command-list hooks: BeginEvent, EndEvent, Close, Reset");
}

void OnListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    if (!g_on) return;
    __try {
        OnListCallImpl(list, set, slot, a1, a2, a3);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_on, 0);
        InterlockedExchange(&g_failed, 1);
        GLOG("exception 0x%08lX in the command-list hook (slot %d) - GP switched off", GetExceptionCode(), slot);
    }
}

void OnFrame() {
    const LONG64 f = InterlockedIncrement64(&g_frame);
    if (!g_on || g_inited != 1) return;
    __try {
        ProcessSlot((int)((f + 2) % kRing));  // the slot that was current 6 frames ago; current again 2 frames from now
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_on, 0);
        InterlockedExchange(&g_failed, 1);
        GLOG("exception 0x%08lX while reading the timestamps - GP switched off", GetExceptionCode());
    }
}

void LogLine() {
    if (!g_cfg.prof) return;
    static LONG64 pB = 0, pE = 0, pF = 0, pU = 0, pBad = 0, pR = 0;
    if (g_failed) { GLOG("%s", "off after a failure (see above)"); return; }
    if (g_inited != 1) { GLOG("%s", "waiting for the first marker on a hooked command list"); return; }
    AcquireSRWLockExclusive(&g_aggLock);
    Agg* a = new (std::nothrow) Agg(g_win);
    memset(&g_win, 0, sizeof(g_win));
    ReleaseSRWLockExclusive(&g_aggLock);
    if (!a) return;
    const double fr = a->frames ? (double)a->frames : 1.0, ms = g_ticksPerMs;
    GLOG("frame %.2f ms GPU (sum of top-level markers; first-to-last span %.2f ms), %.0f markers/frame over %llu frames | "
         "begins %lld, ends %lld, heap full %lld, unresolved %lld, unmatched ends %lld, record cap %lld",
         a->top / fr / ms, a->span / fr / ms, (double)a->recs / fr, (unsigned long long)a->frames, g_begins - pB, g_ends - pE,
         g_full - pF, g_badTs - pBad, g_unmatched - pU, g_recFull - pR);
    pB = g_begins; pE = g_ends; pF = g_full; pU = g_unmatched; pBad = g_badTs; pR = g_recFull;
    const std::vector<int> ids = TopNames(*a, g_cfg.top);
    int rank = 0;
    for (int id : ids)
        GLOG("  %2d %-44s incl %7.3f ms  self %7.3f ms  %6.1f/frame", ++rank, NameOf(id), a->incl[id] / fr / ms,
             a->self[id] / fr / ms, (double)a->count[id] / fr);
    delete a;
}

const char* StatusLine(char* buf, size_t n) {
    if (!buf || !n) return buf;
    buf[0] = 0;
    if (!g_cfg.prof) return buf;
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    AcquireSRWLockExclusive(&g_statusLock);
    if (now - last >= 2000 && g_inited == 1 && !g_failed) {
        last = now;
        AcquireSRWLockExclusive(&g_aggLock);
        Agg* a = new (std::nothrow) Agg(g_short);
        memset(&g_short, 0, sizeof(g_short));
        ReleaseSRWLockExclusive(&g_aggLock);
        if (a && a->frames) {
            const double fr = (double)a->frames, ms = g_ticksPerMs;
            int o = _snprintf_s(g_status, sizeof(g_status), _TRUNCATE, "%.1f ms |", a->top / fr / ms);
            for (int id : TopNames(*a, 5)) {
                if (o < 0 || o >= (int)sizeof(g_status) - 1) break;
                char nm[17];
                strncpy_s(nm, NameOf(id), 16);
                const int w = _snprintf_s(g_status + o, sizeof(g_status) - (size_t)o, _TRUNCATE, " %s %.2f", nm,
                                          a->incl[id] / fr / ms);
                if (w < 0) break;
                o += w;
            }
        }
        delete a;
    }
    if (g_failed) _snprintf_s(buf, n, _TRUNCATE, "off (failed, see log)");
    else if (g_inited != 1) _snprintf_s(buf, n, _TRUNCATE, "waiting for markers...");
    else _snprintf_s(buf, n, _TRUNCATE, "%s", g_status[0] ? g_status : "measuring...");
    ReleaseSRWLockExclusive(&g_statusLock);
    return buf;
}

}  // namespace gp
