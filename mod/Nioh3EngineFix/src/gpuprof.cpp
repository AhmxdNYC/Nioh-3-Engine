// gpuprof.cpp - GP: in-process GPU pass profiler (see gpuprof.h). [Profiler] GPUProf = 0 by default.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "gamerva.h"
#include <d3d12.h>
#include <algorithm>
#include <cctype>
#include <intrin.h>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include "gpuprof.h"
#include "memstats.h"   // 1.7.48 VS: root-descriptor / VB / IB binds that point into system memory
#include "mlprobe.h"
#include "mpccheck.h"
#include "psoswap.h"

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

// GP2 (GPUProf = 2): list events that open / close a per-PSO interval
constexpr int kSlotSetPso = 25;
constexpr int kDrawSlots[] = {12, 13, 14, 59, 79};                 // DrawInstanced, DrawIndexedInstanced, Dispatch, ExecuteIndirect, DispatchMesh
constexpr int kCopySlots[] = {15, 16, 17, 19, 64, 47, 48, 49, 50};  // Copy*, Resolve*, Clear DSV / RTV / UAV x2
constexpr uint32_t kBkNoPso = 0xFFFFFFFFu, kBkCopy = 0xFFFFFFFEu;  // buckets besides PSO shader-set ids (1..) and 0x80000000 | ptr bits
constexpr uint16_t kNoMarker = 0xFFFF;

constexpr int kRing = 8;        // frame slots of the query heap; a slot is read 6 frames after it was current (the
                                // dispatcher records behind the Present thread, and the GPU queues frames)
constexpr int kChunk = 64;      // queries a list takes at a time (resolved as one range on that list)
constexpr int kMaxNames = 2048;
constexpr int kMaxDepth = 64;
constexpr int kMaxTid = 8;

struct Cfg {
    int prof = 0, markers = 0, maxMarkers = 2048, top = 30;
    int maxStamps = 8192, top2 = 40, ab = 1;   // GPUProf = 2: PSO timestamps per frame, rows in the GP2 log, overhead A/B windows
    int sampleEvery = 8, minRun = 4;           // GPUProf = 2 (1.7.8): stamp 1 frame in N; an interval holds >= minRun draws before it is cut
    int gq = 0, gqTop = 5;                     // 1.7.40 GQ: graphics-queue gap census (needs GPUProf = 2), rows per table
    // 1.7.47 MLP (notes\meshlet-frontend.md): meshlet front-end probe (needs GPUProf = 2): per-dispatch / per-barrier stamps inside the
    // meshlet markers, CS invocations per dispatch from pipeline-statistics queries in alternate sampled frames, raw event dump
    int mlp = 0, mlpTop = 40, mlpStats = 1, mlpDump = 0;
    int mlpBarriers = 1;                       // MeshletProbe_Barriers: 1 stamps before + after every barrier call, 0 dispatch stamps only
    int mpcCheck = 0;                          // MpcCheck (mpccheck.cpp): MPC tail-skip self-check, chains to verify (needs GPUProf = 2)
};
// GQ: further list calls that can be a list's first GPU command (besides ResourceBarrier, BeginEvent and the GP2 slots):
// ExecuteBundle, DiscardResource, WriteBufferImmediate, BeginRenderPass, Barrier (enhanced)
constexpr int kGqHeadSlots[] = {27, 51, 66, 68, 82};
Cfg g_cfg;
void (*g_log)(const char*, ...) = nullptr;
uint8_t* g_base = nullptr;
bool g_codeOk = false;
std::string g_wine;

#define GLOG(fmt, ...) do { if (g_log) g_log("GP: " fmt, __VA_ARGS__); } while (0)
#define GLOG2(fmt, ...) do { if (g_log) g_log("GP2: " fmt, __VA_ARGS__); } while (0)
#define GQLOG(fmt, ...) do { if (g_log) g_log("GQ: " fmt, __VA_ARGS__); } while (0)

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
volatile LONG g_stampOn = 1;    // GP2: this frame is sampled (1.7.8: 1 frame in SampleEvery; with SampleEvery = 1 the old A/B windows switch it)
volatile uint8_t* g_hookMask = nullptr;   // gpuopt_hook: the GP2 slots are cleared in unsampled frames, so those frames pay nothing
volatile LONG64 g_wallTicksS = 0, g_wallFramesS = 0, g_wallTicksU = 0, g_wallFramesU = 0, g_sampledFrames = 0, g_stampsTaken = 0;
volatile LONG g_stampsUsed[8] = {};   // per frame slot: PSO timestamps taken (cap = g_cfg.maxStamps)
volatile LONG64 g_stampDrop = 0, g_rec2Full = 0, g_hookTicks = 0, g_hookCalls = 0, g_wallTicks = 0, g_wallFrames = 0;

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
struct Rec2 {   // GP2: GPU time between two timestamps of one list, billed to the bucket that was bound
    int32_t b, e;
    uint32_t bucket;   // PSO shader-set id | 0x80000000|ptr bits (PSO not created through the hooks) | kBkNoPso | kBkCopy
    uint16_t marker;   // enclosing engine marker (name id) or kNoMarker
    uint16_t draws;    // draws + dispatches recorded in the interval
    uint8_t ctype;     // 0 DIRECT, 1 COMPUTE
};
struct GqRec {   // GQ: one DIRECT list Reset in a sampled frame (filed in the frame slot of its head stamp)
    int32_t qh, qe;      // head (before its first GPU command) / end (at Close) query indices
    int32_t qw, qc;      // first / last GP2 interval stamp on the list (GP2 coverage), -1 = none
    uint16_t firstMk, lastMk;   // first / last engine marker begun on the list (name id), kNoMarker = none
    uint32_t sub;        // 1-based submission number among all DIRECT-queue ECL lists (0 = no ECL seen)
    uint32_t eclRa0, eclRa1;    // ECL call site: return RVA into Nioh3.exe and the next game frame (0 = none, 1 = not the exe)
    uint16_t eclN, eclIdx;      // lists in that ECL, position of this one
};
// 1.7.47 MLP: one stamp inside a meshlet region (kind: MlpKind) and one record per region instance (notes\meshlet-frontend.md)
struct MlpEv {
    int32_t q;          // timestamp query (timing frames), -1 = none
    int32_t qs;         // pipeline-statistics query (stats frames; dispatches only), -1 = none
    uint32_t rid;       // region instance
    uint32_t key;       // dispatches: first 32 bits of the CS container digest (psoswap key), 0 = unknown
    uint32_t dims;      // Dispatch: x*y*z groups; ExecuteIndirect: MaxCommandCount; barriers: structs in the call
    uint16_t seq;       // order inside the region
    uint16_t marker;    // innermost engine marker at the event (LOD levels "0".."13")
    uint8_t kind;
    uint8_t uav;        // barriers: UAV-type structs in the call
};
struct MlpReg {
    uint32_t rid;
    uint32_t tag;       // caller RVA of the recorder call (mlprobe), 0 = unknown
    uint16_t parent;    // marker enclosing the region marker
    uint8_t reg;        // index into kMlpRegions
    uint8_t ord;        // region ordinal of that kind in the frame (0-based, 15 = 15 or more)
    uint8_t stats;      // 1: stats frame (CS invocations, no timing)
    uint8_t exact;      // tag matched on the first UAV barrier resource
};
struct Slot {
    SRWLOCK lock;
    volatile LONG used;  // queries handed out (in chunks)
    std::vector<Rec> recs;
    std::vector<Rec2> recs2;
    std::vector<GqRec> gq;
    uint32_t gen;        // GQ: +1 per ProcessSlot (a late ECL never writes into a reused slot)
    std::vector<MlpEv> mlp;      // MLP: events of the meshlet regions (timing and stats frames)
    std::vector<MlpReg> mlpReg;  // MLP: one per region instance
    volatile LONG mlpStats;             // MLP: pipeline-statistics queries handed out in this slot
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
    // GP2
    int ctype;          // 0 DIRECT, 1 COMPUTE
    int ivQ;            // query index that opened the current interval, -1 = none
    uint32_t bucket;    // what the current interval is billed to
    uint32_t bound;     // bucket of the PSO currently bound
    uintptr_t boundPtr;
    uint32_t draws;     // draws + dispatches since ivQ
    // 1.7.8 coarse stamps: an interval is cut only once it holds >= minRun draws (copies count as minRun); it is billed to its longest run
    uint32_t runBucket; // bucket of the run in progress
    uint32_t runW, bestW, ivW;   // weight (draws) of the run in progress / of the longest run so far / of the whole interval
    // 1.7.40 GQ: 0 not measured, 1 Reset in a sampled frame (head stamp pending), 2 head stamped
    int gqState, gqHead, gqWork, gqCov;
    uint16_t gqFirstMk, gqLastMk;
    uint32_t gqGen;     // generation of the head's frame slot when the head was stamped
    int mlpStatsQ;      // MLP: open pipeline-statistics query (absolute index), -1 = none
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
    s.ctype = 0;
    s.ivQ = -1;
    s.bucket = s.bound = kBkNoPso;
    s.boundPtr = 0;
    s.draws = 0;
    s.runBucket = kBkNoPso;
    s.runW = s.bestW = s.ivW = 0;
    s.gqState = 0;
    s.gqHead = s.gqWork = s.gqCov = -1;
    s.gqFirstMk = s.gqLastMk = kNoMarker;
    s.mlpStatsQ = -1;
    auto cl = (ID3D12GraphicsCommandList*)list;
    const D3D12_COMMAND_LIST_TYPE t = reinterpret_cast<GetTypeFn>(Orig(set, kSlotGetType))(cl);
    s.ctype = t == D3D12_COMMAND_LIST_TYPE_COMPUTE ? 1 : 0;
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

// a timestamp on `list` now (g_lsLock held); returns its query index or -1 (not sampled). pso = a GP2 PSO-level stamp: capped
int StampLocked(ListState& s, void* list, int set, bool pso) {
    const int cur = (int)(g_frame % kRing);
    if (pso && InterlockedIncrement(&g_stampsUsed[cur]) > g_cfg.maxStamps) {
        InterlockedIncrement64(&g_stampDrop);
        return -1;
    }
    int idx = -1;
    if (s.base >= 0 && (s.slot != cur || s.used >= kChunk)) ResolveChunk(s);
    if (s.base < 0) {
        const LONG off = InterlockedExchangeAdd(&g_slot[cur].used, kChunk);
        if (off + kChunk <= g_qPerSlot) {
            s.base = cur * g_qPerSlot + off;
            s.slot = cur;
            s.used = 0;
        } else {
            InterlockedIncrement64(&g_full);
        }
    }
    if (s.base >= 0) {
        idx = s.base + s.used++;
        InterlockedIncrement64(&g_stampsTaken);
        reinterpret_cast<EndQueryFn>(Orig(set, kSlotEndQuery))((ID3D12GraphicsCommandList*)list, g_heap,
                                                              D3D12_QUERY_TYPE_TIMESTAMP, (UINT)idx);
    }
    return idx;
}

// GP2: the interval opened at s.ivQ ends at qNow; file it under s.bucket, start the next one at qNow (g_lsLock held)
void IvClose(ListState& s, int qNow, uint16_t marker) {
    if (s.ivQ >= 0 && qNow >= 0) {
        Slot& S = g_slot[qNow / g_qPerSlot];
        AcquireSRWLockExclusive(&S.lock);
        if (S.recs2.size() < S.recs2.capacity())
            S.recs2.push_back(Rec2{s.ivQ, qNow, s.bucket, marker, (uint16_t)(s.draws > 65535 ? 65535 : s.draws), (uint8_t)s.ctype});
        else
            InterlockedIncrement64(&g_rec2Full);
        ReleaseSRWLockExclusive(&S.lock);
        if (s.gqState == 2) s.gqCov = qNow;   // GQ: last stamp of GP2 coverage on this list
    }
    if (s.ivQ < 0 && qNow >= 0 && s.gqState == 2 && s.gqWork < 0) s.gqWork = qNow;   // GQ: GP2 coverage starts here
    s.ivQ = qNow;
    s.draws = 0;
}

// the interval is about to be cut for a reason other than a PSO change (marker edge, list end): label it with its longest run
void Finalize(ListState& s) {
    if (s.runW > s.bestW) { s.bestW = s.runW; s.bucket = s.runBucket; }
}
void Restart(ListState& s) {   // after a forced cut the run in progress goes on in a fresh interval
    s.bucket = s.runBucket;
    s.runW = s.bestW = s.ivW = 0;
}

// GQ: the list's first GPU command is about to be recorded: stamp its head (g_lsLock held)
void GqHeadLocked(ListState& s, void* list, int set) {
    if (s.gqState != 1 || !s.ok) return;
    s.gqHead = StampLocked(s, list, set, false);
    s.gqState = s.gqHead >= 0 ? 2 : 0;
    if (s.gqHead >= 0) s.gqGen = *(volatile uint32_t*)&g_slot[s.gqHead / g_qPerSlot].gen;
}

// closeMarker: -2 = markers only (GPUProf = 1); otherwise GP2: the marker name id the ending interval belongs to
int Stamp(void* list, int set, int closeMarker = -2) {
    if (closeMarker != -2 && !g_stampOn) return -1;   // GP2, unsampled frame: no query at all
    AcquireSRWLockExclusive(&g_lsLock);
    int idx = -1;
    ListState* s = FindList(list, set, true);
    if (s && s->ok) {
        idx = StampLocked(*s, list, set, false);
        if (closeMarker != -2) {
            if (g_stampOn) {
                Finalize(*s);
                IvClose(*s, idx, (uint16_t)closeMarker);
                Restart(*s);
            } else {
                s->ivQ = -1;
                s->bucket = s->runBucket = kBkNoPso;
                s->runW = s->bestW = s->ivW = 0;
            }
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
uint16_t CurMarker() {
    const int d = t_depth;
    return d <= 0 || d > kMaxDepth ? kNoMarker : (uint16_t)t_stack[d - 1].name;
}

// ---- GQ (1.7.40): graphics-queue gap census -------------------------------------------------------------------------------
// A list Reset in a sampled frame is armed (gqState 1); its first GPU command stamps the head (2); Close stamps the end (also
// in an unsampled frame, so the list is complete) and files a GqRec in the head's frame slot. The ECL hook fills in the
// submission number and call site. Lock order: g_lsLock -> Slot.lock, g_lsLock -> g_gqLock; OnEcl takes them one at a time.
struct GqPend { void* list; int slot; uint32_t idx, gen; };
constexpr int kGqPend = 128;
GqPend g_gqPend[kGqPend];
SRWLOCK g_gqLock = SRWLOCK_INIT;
volatile LONG64 g_gqSub = 0;   // lists submitted on DIRECT queues (all callers): the submission numbers
volatile LONG64 g_gqArmed = 0, g_gqNoHead = 0, g_gqFull = 0, g_gqPendFull = 0, g_gqLate = 0, g_gqBad = 0;
uintptr_t g_exeLo = 0, g_exeHi = 0;

uint32_t ExeRva(const void* p) {   // 0 = null, 1 = outside Nioh3.exe, else the RVA
    const uintptr_t a = (uintptr_t)p;
    if (!a) return 0;
    return (a >= g_exeLo && a < g_exeHi) ? (uint32_t)(a - g_exeLo) : 1u;
}

void GqFileLocked(ListState& s, int qe) {   // g_lsLock held; s.gqState == 2
    Slot& S = g_slot[s.gqHead / g_qPerSlot];
    uint32_t idx = ~0u, gen = 0;
    AcquireSRWLockExclusive(&S.lock);
    if (S.gen != s.gqGen) {
        InterlockedIncrement64(&g_gqLate);   // open longer than the read-back delay: its head slot was read already
    } else if (S.gq.size() < S.gq.capacity()) {
        idx = (uint32_t)S.gq.size();
        gen = S.gen;
        S.gq.push_back(GqRec{s.gqHead, qe, s.gqWork, s.gqCov, s.gqFirstMk, s.gqLastMk, 0, 0, 0, 0, 0});
    } else {
        InterlockedIncrement64(&g_gqFull);
    }
    ReleaseSRWLockExclusive(&S.lock);
    if (idx == ~0u) return;
    AcquireSRWLockExclusive(&g_gqLock);
    GqPend* freeE = nullptr;
    for (GqPend& p : g_gqPend) {
        if (p.list == s.list) { freeE = &p; break; }   // closed again before an ECL: the newer record wins
        if (!p.list && !freeE) freeE = &p;
    }
    if (freeE) *freeE = GqPend{s.list, s.gqHead / g_qPerSlot, idx, gen};
    else InterlockedIncrement64(&g_gqPendFull);
    ReleaseSRWLockExclusive(&g_gqLock);
}

// head events outside Gp2Event (g_lsLock not held). marker >= 0: a BeginEvent with that name id
void GqHead(void* list, int set, int marker) {
    AcquireSRWLockExclusive(&g_lsLock);
    if (ListState* s = FindList(list, set, false)) {
        if (g_stampOn) GqHeadLocked(*s, list, set);
        if (marker >= 0 && s->gqState == 2) {
            if (s->gqFirstMk == kNoMarker) s->gqFirstMk = (uint16_t)marker;
            s->gqLastMk = (uint16_t)marker;
        }
    }
    ReleaseSRWLockExclusive(&g_lsLock);
}

// ---- GP2 list events -------------------------------------------------------------------------------------------------------
bool IsIn(const int* a, size_t n, int v) {
    for (size_t i = 0; i < n; ++i)
        if (a[i] == v) return true;
    return false;
}
void Gp2Event(void* list, int set, int slot, uintptr_t a1) {
    const bool setPso = slot == kSlotSetPso;
    const bool copy = !setPso && IsIn(kCopySlots, sizeof kCopySlots / sizeof(int), slot);
    AcquireSRWLockExclusive(&g_lsLock);
    ListState* s = FindList(list, set, true);
    if (s && s->ok) {
        if (setPso) {
            if (a1 != s->boundPtr) {
                s->boundPtr = a1;
                const uint32_t id = psoswap::PsoSetId((const void*)a1);
                s->bound = id ? id : (0x80000000u | (uint32_t)((a1 >> 4) & 0x7FFFFFFFu));
            }
        } else if (!g_stampOn) {
            s->ivQ = -1;
            s->bucket = s->runBucket = kBkNoPso;
            s->runW = s->bestW = s->ivW = 0;
        } else {
            GqHeadLocked(*s, list, set);   // GQ: before the list's first draw / dispatch / copy / clear stamp
            const uint32_t want = copy ? kBkCopy : s->bound;
            const uint32_t w = copy ? (uint32_t)g_cfg.minRun : 1u;
            if (s->ivQ < 0) {   // no interval open: this stamp opens one
                const int q = StampLocked(*s, list, set, true);
                IvClose(*s, q, CurMarker());
                s->bucket = s->runBucket = want;
                s->runW = s->bestW = s->ivW = 0;
            } else if (want != s->runBucket) {   // PSO change: cut only when the interval holds enough draws, else merge the runs
                if (s->runW > s->bestW) { s->bestW = s->runW; s->bucket = s->runBucket; }
                if (s->ivW >= (uint32_t)g_cfg.minRun) {
                    const int q = StampLocked(*s, list, set, true);
                    IvClose(*s, q, CurMarker());
                    s->bucket = want;
                    s->bestW = s->ivW = 0;
                }
                s->runBucket = want;
                s->runW = 0;
            }
            s->runW += w;
            s->ivW += w;
            if (!copy) ++s->draws;
        }
    }
    ReleaseSRWLockExclusive(&g_lsLock);
}

// ---- 1.7.47 MLP: meshlet front-end probe, GPU half (notes\meshlet-frontend.md) ------------------------------------------------------
// Inside the meshlet markers (kMlpRegions) of a sampled frame, every Dispatch / ExecuteIndirect gets a timestamp before it and every
// ResourceBarrier call one before and one after it (gpuopt's pass-through path), so the region's GPU time splits exactly into
// dispatch (pre-dispatch stamp -> next stamp), barrier (pre -> post) and gap (post-barrier / region start -> next command) time.
// Alternate sampled frames (MeshletProbe_Stats = 1) take no timestamps but wrap each dispatch in a pipeline-statistics query
// (CSInvocations = threads launched: 0 = empty dispatch, <= 64 = at most one wave) - the timing frames stay unperturbed.
// Regions are labelled with their ordinal in the frame and, for LOD / PRE / MIC, the recorder's caller from mlprobe.cpp.
enum MlpKind : uint8_t { kMkRB, kMkD, kMkEI, kMkBB, kMkBE, kMkRE, kMkLE };
const char* const kMlpRegions[] = {"MeshletResolveLOD", "MeshletPreculling", "MeshletInstanceCulling", "MeshletCheckRequestLoad",
                                   "MeshletRequestLoad", "GenerateSceneHiZ", "GenerateShadowMapHiZ"};
const char* const kMlpShort[] = {"LOD", "PRE", "MIC", "MCLRC", "MRL", "HiZ", "SHiZ"};
constexpr int kNumMlpReg = 7;
constexpr int kRegPre = 1, kRegMic = 2, kRegMclrc = 3;
constexpr int kMlpStatsPer = 512;          // pipeline-statistics queries per frame slot
constexpr int kStatsWords = 11;            // D3D12_QUERY_DATA_PIPELINE_STATISTICS: 11 x UINT64, CSInvocations = [10]
volatile int8_t g_mlpRegOf[kMaxNames];     // 0 not classified yet, -1 not a region, 1..7 = region index + 1
ID3D12QueryHeap* g_mlpHeap = nullptr;
ID3D12Resource* g_mlpRb = nullptr;
volatile uint64_t* g_mlpRbMap = nullptr;
volatile LONG g_mlpOn = 0;                 // MLP or MpcCheck active (region tracking in the hooks)
volatile LONG g_mlpStatsFrame = 0;         // the current sampled frame: 1 = CS invocations (+ MpcCheck), 0 = timestamps
volatile LONG g_mlpRid = 0;
volatile LONG64 g_mlpStatsFull = 0, g_mlpXFrame = 0, g_mlpXList = 0, g_mlpRegions = 0;
int g_mlpOrdN[kNumMlpReg];
LONG64 g_mlpOrdFrame = -1, g_mlpMclrcFrame = -1000;   // ordinals restart at MeshletCheckRequestLoad (frame begin), else on a new frame
SRWLOCK g_mlpOrdLock = SRWLOCK_INIT;
std::wstring g_mlpDumpPath;
int g_mlpDumped = 0;

int MlpRegionOf(int name) {
    if (name <= 0 || name >= kMaxNames) return -1;
    int8_t v = g_mlpRegOf[name];
    if (!v) {
        v = -1;
        for (int i = 0; i < kNumMlpReg; ++i)
            if (!strcmp(g_names[name].s, kMlpRegions[i])) {
                v = (int8_t)(i + 1);
                break;
            }
        g_mlpRegOf[name] = v;
    }
    return v > 0 ? v - 1 : -1;
}

struct TReg {
    bool open, stats, check, bad;
    int reg, depth, slot;
    uint32_t rid;
    uint16_t seq, parent;
    uint8_t ord;
    void* list;
    const void* firstRes;   // first UAV barrier resource (recorder tag join, mlprobe::CallerOf)
};
thread_local TReg t_reg;

std::unordered_map<uint32_t, uint32_t> g_keyCache;   // PSO shader-set id -> first 32 bits of the CS digest
SRWLOCK g_keyLock = SRWLOCK_INIT;
uint32_t KeyOfSet(uint32_t id) {
    if (!id || (id & 0x80000000u) || id == kBkNoPso || id == kBkCopy) return 0;
    AcquireSRWLockShared(&g_keyLock);
    auto it = g_keyCache.find(id);
    const bool hit = it != g_keyCache.end();
    const uint32_t k = hit ? it->second : 0;
    ReleaseSRWLockShared(&g_keyLock);
    if (hit) return k;
    char d[360];
    uint32_t key = 0;
    if (psoswap::DescribeSet(id, d, sizeof d))
        if (const char* p = strstr(d, "cs=")) {
            char h[9] = {};
            memcpy(h, p + 3, 8);
            key = (uint32_t)strtoul(h, nullptr, 16);
        }
    AcquireSRWLockExclusive(&g_keyLock);
    g_keyCache[id] = key;
    ReleaseSRWLockExclusive(&g_keyLock);
    return key;
}

using BeginQueryFn = void(STDMETHODCALLTYPE*)(ID3D12GraphicsCommandList*, ID3D12QueryHeap*, D3D12_QUERY_TYPE, UINT);
constexpr int kSlotBeginQuery = 52;

// g_lsLock held: close the open pipeline-statistics query of this list and resolve it into the readback buffer
void MlpEndStatsLocked(ListState& s) {
    if (s.mlpStatsQ < 0) return;
    auto cl = (ID3D12GraphicsCommandList*)s.list;
    reinterpret_cast<EndQueryFn>(Orig(s.set, kSlotEndQuery))(cl, g_mlpHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, (UINT)s.mlpStatsQ);
    reinterpret_cast<ResolveFn>(Orig(s.set, kSlotResolve))(cl, g_mlpHeap, D3D12_QUERY_TYPE_PIPELINE_STATISTICS, (UINT)s.mlpStatsQ, 1, g_mlpRb,
                                                          (UINT64)s.mlpStatsQ * kStatsWords * 8);
    s.mlpStatsQ = -1;
}

// recording thread, t_reg.open: one event of the open region (timestamp in timing frames; stats query around dispatches otherwise)
void MlpEvent(void* list, int set, uint8_t kind, uint32_t key, uint32_t dims, uint8_t uav) {
    AcquireSRWLockExclusive(&g_lsLock);
    ListState* s = FindList(list, set, true);
    if (s && s->ok) {
        if (s->mlpStatsQ >= 0) MlpEndStatsLocked(*s);   // the previous dispatch on this list has ended
        if (t_reg.list && t_reg.list != list && kind != kMkRB) InterlockedIncrement64(&g_mlpXList);
        t_reg.list = list;
        int q = -1, qs = -1;
        if (!t_reg.stats) {
            q = StampLocked(*s, list, set, false);
            if (q < 0 || q / g_qPerSlot != t_reg.slot) t_reg.bad = true;   // no query, or the region straddles a frame slot
        } else if ((kind == kMkD || kind == kMkEI) && g_mlpHeap) {
            const LONG k = InterlockedIncrement(&g_slot[t_reg.slot].mlpStats) - 1;
            if (k < kMlpStatsPer) {
                qs = t_reg.slot * kMlpStatsPer + k;
                reinterpret_cast<BeginQueryFn>(Orig(set, kSlotBeginQuery))((ID3D12GraphicsCommandList*)list, g_mlpHeap,
                                                                          D3D12_QUERY_TYPE_PIPELINE_STATISTICS, (UINT)qs);
                s->mlpStatsQ = qs;
            } else {
                InterlockedIncrement64(&g_mlpStatsFull);
            }
        }
        Slot& S = g_slot[t_reg.slot];
        AcquireSRWLockExclusive(&S.lock);
        if (S.mlp.size() < S.mlp.capacity())
            S.mlp.push_back(MlpEv{q, qs, t_reg.rid, key, dims, t_reg.seq, CurMarker(), kind, uav});
        else
            t_reg.bad = true;
        ReleaseSRWLockExclusive(&S.lock);
        t_reg.seq++;
    }
    ReleaseSRWLockExclusive(&g_lsLock);
}

// BeginEvent (before the marker is pushed): opens a region when the marker is a meshlet region marker in a sampled frame
void MlpBegin(void* list, int set, int name) {
    if (t_reg.open || !g_stampOn) return;
    const int reg = MlpRegionOf(name);
    if (reg < 0) return;
    t_reg = TReg{};
    t_reg.open = true;
    t_reg.reg = reg;
    t_reg.stats = !g_cfg.mlp || g_mlpStatsFrame != 0;
    t_reg.check = t_reg.stats && reg == kRegPre && g_cfg.mpcCheck && mpcck::Wanted();
    t_reg.depth = t_depth;
    t_reg.parent = (t_depth > 0 && t_depth <= kMaxDepth) ? (uint16_t)t_stack[t_depth - 1].name : kNoMarker;
    t_reg.slot = (int)(g_frame % kRing);
    t_reg.rid = (uint32_t)InterlockedIncrement(&g_mlpRid);
    AcquireSRWLockExclusive(&g_mlpOrdLock);
    const LONG64 fr = g_frame;
    if (reg == kRegMclrc) {   // MeshletCheckRequestLoad runs once, at frame begin (P0)
        memset(g_mlpOrdN, 0, sizeof g_mlpOrdN);
        g_mlpMclrcFrame = fr;
    } else if (fr - g_mlpMclrcFrame > 2 && fr != g_mlpOrdFrame) {   // no frame-begin region seen lately: restart per presented frame
        memset(g_mlpOrdN, 0, sizeof g_mlpOrdN);
    }
    g_mlpOrdFrame = fr;
    const int o = g_mlpOrdN[reg]++;
    ReleaseSRWLockExclusive(&g_mlpOrdLock);
    t_reg.ord = (uint8_t)(o > 15 ? 15 : o);
    if (g_cfg.mlp) MlpEvent(list, set, kMkRB, 0, 0, 0);
    if (t_reg.check) mpcck::OnRegion(list, set, true);
}

// EndEvent of the region marker (t_depth already back at the region's depth)
void MlpEnd(void* list, int set) {
    if (g_cfg.mlp) MlpEvent(list, set, kMkRE, 0, 0, 0);
    if (t_reg.check) mpcck::OnRegion(list, set, false);
    if (g_cfg.mlp) {
        uint32_t tag = 0;
        bool exact = false;
        if (t_reg.reg <= kRegMic && mlprobe::Active())
            tag = mlprobe::CallerOf((mlprobe::Fn)t_reg.reg, t_reg.ord, t_reg.firstRes, &exact);
        Slot& S = g_slot[t_reg.slot];
        AcquireSRWLockExclusive(&S.lock);
        if (t_reg.bad) InterlockedIncrement64(&g_mlpXFrame);
        else if (S.mlpReg.size() < S.mlpReg.capacity())
            S.mlpReg.push_back(MlpReg{t_reg.rid, tag, t_reg.parent, (uint8_t)t_reg.reg, t_reg.ord, (uint8_t)(t_reg.stats ? 1 : 0), (uint8_t)(exact ? 1 : 0)});
        ReleaseSRWLockExclusive(&S.lock);
        InterlockedIncrement64(&g_mlpRegions);
    }
    t_reg.open = false;
}

void MlpDispatch(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    uint32_t key = 0;
    AcquireSRWLockExclusive(&g_lsLock);
    if (ListState* s = FindList(list, set, true))
        if (s->ok) key = KeyOfSet(s->bound);
    ReleaseSRWLockExclusive(&g_lsLock);
    const bool ei = slot == 59;
    if (t_reg.check) mpcck::OnDispatch(list, set, key, ei ? 0 : (uint32_t)a1, ei ? 0 : (uint32_t)a2, ei ? 0 : (uint32_t)a3, ei);
    if (g_cfg.mlp) {
        const uint64_t g = ei ? (uint64_t)(uint32_t)a2 : (uint64_t)(uint32_t)a1 * (uint32_t)a2 * (uint32_t)a3;
        MlpEvent(list, set, ei ? kMkEI : kMkD, key, (uint32_t)std::min<uint64_t>(g, 0xFFFFFFFFull), 0);
    }
}

// ---- MLP aggregation (Present thread, ProcessSlot) and 30 s log --------------------------------------------------------------------
struct MlpRegA {
    double tot = 0, disp = 0, bar = 0, gap = 0;
    uint64_t n = 0, nd = 0, ne = 0, nbc = 0, nbs = 0, nbu = 0, exact = 0, ns = 0;
};
struct MlpOpA {
    double t = 0;             // timing frames: dispatch time (ticks)
    uint64_t n = 0;           // timed calls
    double inv = 0;           // stats frames: CS invocations
    uint64_t ns = 0, zero = 0, tiny = 0;
    double dims = 0;
    uint64_t rkey = 0;
    uint32_t key = 0;
    uint16_t marker = 0, op = 0;
    uint8_t kind = 0;
};
struct MlpWin {
    uint64_t framesT = 0, framesS = 0, bad = 0;
    std::map<uint64_t, MlpRegA> reg;          // reg | ord << 8 | parent << 16 | tag << 32
    std::unordered_map<uint64_t, MlpOpA> ops; // FNV(rkey, op, key, marker)
};
MlpWin g_mlpWin;

uint64_t MlpRKey(const MlpReg& r) { return (uint64_t)r.reg | (uint64_t)r.ord << 8 | (uint64_t)r.parent << 16 | (uint64_t)r.tag << 32; }
uint64_t MlpOKey(uint64_t rkey, uint16_t op, uint32_t key, uint16_t marker) {
    uint64_t h = 1469598103934665603ull;
    for (uint64_t v : {rkey, (uint64_t)op, (uint64_t)key, (uint64_t)marker}) h = (h ^ v) * 1099511628211ull;
    return h;
}
const char* MkName(uint16_t mk);

// S.lock and g_aggLock held
void MlpProcess(Slot& S, int r, double maxTicks) {
    if (S.mlpReg.empty()) return;
    std::sort(S.mlp.begin(), S.mlp.end(), [](const MlpEv& a, const MlpEv& b) { return a.rid != b.rid ? a.rid < b.rid : a.seq < b.seq; });
    MlpWin& W = g_mlpWin;
    bool anyT = false, anyS = false;
    std::string dump;
    const bool doDump = g_cfg.mlpDump > 0 && g_mlpDumped < g_cfg.mlpDump && !g_mlpDumpPath.empty();
    for (const MlpReg& R : S.mlpReg) {
        auto lo = std::lower_bound(S.mlp.begin(), S.mlp.end(), R.rid, [](const MlpEv& e, uint32_t rid) { return e.rid < rid; });
        auto hi = lo;
        while (hi != S.mlp.end() && hi->rid == R.rid) ++hi;
        if (lo == hi) continue;
        const uint64_t rk = MlpRKey(R);
        if (R.stats) {
            anyS = true;
            uint16_t op = 0;
            for (auto e = lo; e != hi; ++e) {
                if (e->kind != kMkD && e->kind != kMkEI) continue;
                const uint16_t thisOp = op++;
                if (e->qs < 0 || e->qs / kMlpStatsPer != r || !g_mlpRbMap) continue;
                const uint64_t inv = g_mlpRbMap[(size_t)e->qs * kStatsWords + 10];
                MlpOpA& a = W.ops[MlpOKey(rk, thisOp, e->key, e->marker)];
                a.rkey = rk; a.key = e->key; a.marker = e->marker; a.op = thisOp; a.kind = e->kind;
                a.inv += (double)inv;
                a.ns++;
                if (inv == 0) a.zero++;
                if (inv <= 64) a.tiny++;
                if (doDump) {
                    char b[200];
                    _snprintf_s(b, sizeof b, _TRUNCATE, "%d,S,%u,%s,%u,%X,%u,%u,%s,%08X,%u,,%llu\n", g_mlpDumped, R.rid, kMlpShort[R.reg], R.ord, R.tag,
                                e->seq, e->kind, MkName(e->marker), e->key, e->dims, (unsigned long long)inv);
                    dump += b;
                }
            }
            W.reg[rk].ns++;
            continue;
        }
        // timing region: every event stamped, monotonic, < 1 s
        bool ok = true;
        uint64_t prev = 0;
        for (auto e = lo; e != hi && ok; ++e) {
            const uint64_t t = e->q >= 0 ? g_rbMap[e->q] : 0;
            if (!t || t < prev) ok = false;
            prev = t;
        }
        if (!ok || (double)(prev - g_rbMap[lo->q]) > maxTicks) {
            W.bad++;
            continue;
        }
        anyT = true;
        MlpRegA& A = W.reg[rk];
        A.n++;
        A.exact += R.exact;
        const uint64_t t0 = g_rbMap[lo->q];
        A.tot += (double)(prev - t0);
        uint16_t op = 0;
        for (auto e = lo; e != hi; ++e) {
            auto nx = e + 1;
            const double dt = nx != hi ? (double)(g_rbMap[nx->q] - g_rbMap[e->q]) : 0.0;
            switch (e->kind) {
                case kMkD:
                case kMkEI: {
                    A.disp += dt;
                    (e->kind == kMkD ? A.nd : A.ne)++;
                    MlpOpA& a = W.ops[MlpOKey(rk, op, e->key, e->marker)];
                    a.rkey = rk; a.key = e->key; a.marker = e->marker; a.op = op; a.kind = e->kind;
                    a.t += dt;
                    a.n++;
                    a.dims += e->dims;
                    ++op;
                    break;
                }
                case kMkBB:
                    A.bar += dt;
                    A.nbc++;
                    A.nbs += e->dims;
                    A.nbu += e->uav;
                    break;
                default:   // region begin, post-barrier, list end: state setup / idle until the next command
                    A.gap += dt;
                    break;
            }
            if (doDump) {
                char b[200];
                _snprintf_s(b, sizeof b, _TRUNCATE, "%d,T,%u,%s,%u,%X,%u,%u,%s,%08X,%u,%.2f,\n", g_mlpDumped, R.rid, kMlpShort[R.reg], R.ord, R.tag,
                            e->seq, e->kind, MkName(e->marker), e->key, e->dims, (double)(g_rbMap[e->q] - t0) / g_ticksPerMs * 1000.0);
                dump += b;
            }
        }
    }
    if (anyT) W.framesT++;
    if (anyS) W.framesS++;
    if (doDump && !dump.empty()) {
        HANDLE f = CreateFileW(g_mlpDumpPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) {
            DWORD w;
            if (g_mlpDumped == 0 && GetFileSize(f, nullptr) == 0) {
                const char* h = "frame,mode,rid,region,ord,caller,seq,kind(0RB 1D 2EI 3BB 4BE 5RE 6LE),marker,cs,groups_or_structs,t_us,cs_invocations\n";
                WriteFile(f, h, (DWORD)strlen(h), &w, nullptr);
            }
            WriteFile(f, dump.data(), (DWORD)dump.size(), &w, nullptr);
            CloseHandle(f);
        }
        g_mlpDumped++;
    }
}

void MlpTagStr(uint32_t tag, char* b, size_t n) {
    if (!tag) _snprintf_s(b, n, _TRUNCATE, "?");
    else if (tag == 1) _snprintf_s(b, n, _TRUNCATE, "ext");
    else _snprintf_s(b, n, _TRUNCATE, "%X", tag);
}

void LogMlp();

// ---- init (first BeginEvent, recording thread) ----------------------------------------------------------------------------------
bool Init(void* list, int set) {
    auto cl = (ID3D12GraphicsCommandList*)list;
    ID3D12Device* dev = nullptr;
    if (FAILED(reinterpret_cast<GetDeviceFn>(Orig(set, kSlotGetDevice))(cl, __uuidof(ID3D12Device), (void**)&dev)) || !dev) {
        GLOG("%s", "GetDevice failed - off");
        return false;
    }
    g_qPerSlot = g_cfg.maxMarkers * 2;
    if (g_cfg.prof >= 2) g_qPerSlot += g_cfg.maxStamps + 256 * kChunk;   // PSO stamps + chunk slack of up to 256 lists
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
    if (g_cfg.mlp && g_cfg.mlpStats) {   // 1.7.47 MLP: pipeline-statistics queries for the stats frames (CS invocations per dispatch)
        D3D12_QUERY_HEAP_DESC sd = {};
        sd.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
        sd.Count = (UINT)(kMlpStatsPer * kRing);
        D3D12_RESOURCE_DESC sr = rd;
        sr.Width = (UINT64)sd.Count * kStatsWords * 8;
        void* sp = nullptr;
        D3D12_RANGE sall = {0, (SIZE_T)sr.Width};
        if (SUCCEEDED(dev->CreateQueryHeap(&sd, __uuidof(ID3D12QueryHeap), (void**)&g_mlpHeap)) && g_mlpHeap &&
            SUCCEEDED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &sr, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource),
                                                   (void**)&g_mlpRb)) && g_mlpRb && SUCCEEDED(g_mlpRb->Map(0, &sall, &sp)) && sp) {
            g_mlpRbMap = (volatile uint64_t*)sp;
            memset(sp, 0, (size_t)sr.Width);
        } else {
            g_mlpHeap = nullptr;   // (leaked on failure: once per process)
            GLOG("%s", "MLP: pipeline-statistics query heap not available - CS invocation counts off, timing only");
            g_cfg.mlpStats = 0;
        }
    }
    for (Slot& s : g_slot) {
        if (g_cfg.mlp || g_cfg.mpcCheck) {
            s.mlp.reserve(8192);
            s.mlpReg.reserve(512);
        }
        s.recs.reserve((size_t)g_cfg.maxMarkers);
        if (g_cfg.prof >= 2) s.recs2.reserve((size_t)(g_cfg.maxStamps + g_cfg.maxMarkers * 2 + 256));
        if (g_cfg.gq) s.gq.reserve(1024);   // GQ: DIRECT lists per frame slot (the game records ~21-28)
    }
    GLOG("timestamp queries ready: %u queries (%d frame slots x %d markers%s), readback %u KB, GPU clock %llu Hz, list %p "
         "(type %d)",
         total, kRing, g_cfg.maxMarkers, g_cfg.prof >= 2 ? " + PSO stamps (GPUProf 2)" : "", total * 8 / 1024, (unsigned long long)freq, list,
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
            if (g_cfg.gq) GqHead(list, set, name);   // GQ: a marker can be a list's first command; remember first / last marker
            const int q = Stamp(list, set, g_cfg.prof >= 2 ? (int)CurMarker() : -2);  // GP2: the interval ending here is the outer marker's
            if (g_mlpOn) MlpBegin(list, set, name);   // 1.7.47 MLP / MpcCheck: a meshlet region marker opens a region
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
            if (t_reg.open && t_depth == t_reg.depth) MlpEnd(list, set);   // 1.7.47 MLP: the region marker ends
            if (t_depth >= kMaxDepth) break;  // inside an overflowed region: not sampled
            const Open o = t_stack[t_depth];
            if (o.q < 0) break;
            const int e = Stamp(list, set, g_cfg.prof >= 2 ? (int)o.name : -2);  // GP2: the interval ending here is this marker's
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
            if (t_reg.open && t_reg.list == list && g_cfg.mlp) MlpEvent(list, set, kMkLE, 0, 0, 0);   // MLP: region continues on the next list
            if (t_reg.check && t_reg.list == list) mpcck::OnClose(list, set);
            AcquireSRWLockExclusive(&g_lsLock);
            if (ListState* s = FindList(list, set, false)) {
                if (s->ok && g_cfg.prof >= 2 && g_stampOn && s->ivQ >= 0) {  // GP2: list end
                    Finalize(*s);
                    const int q = StampLocked(*s, list, set, true);
                    IvClose(*s, q, CurMarker());
                }
                if (s->ok && s->gqState == 2) {   // GQ: list end (also in an unsampled frame: the armed list completes)
                    const int qe = StampLocked(*s, list, set, false);
                    if (qe >= 0) GqFileLocked(*s, qe);
                } else if (s->gqState == 1) {
                    InterlockedIncrement64(&g_gqNoHead);   // armed, but no hooked GPU command while sampled
                }
                if (s->ok && s->mlpStatsQ >= 0) MlpEndStatsLocked(*s);   // MLP: a stats query still open at the list's end
                if (s->ok) ResolveChunk(*s);  // last commands of the list: every query of the chunk was ended on it
                s->list = nullptr;
            }
            ReleaseSRWLockExclusive(&g_lsLock);
            break;
        }
        case kSlotReset: {
            AcquireSRWLockExclusive(&g_lsLock);
            if (ListState* s = FindList(list, set, false)) s->list = nullptr;  // reused without our Close: forget it
            if (g_cfg.gq && g_inited == 1 && g_stampOn) {   // GQ: arm the list (sampled frame); FindList checks DIRECT + our device
                ListState* s = FindList(list, set, true);
                if (s && s->ok && s->ctype == 0) {
                    s->gqState = 1;
                    InterlockedIncrement64(&g_gqArmed);
                }
            }
            ReleaseSRWLockExclusive(&g_lsLock);
            break;
        }
        default:
            if (g_cfg.prof >= 2 && g_inited == 1 &&
                (slot == kSlotSetPso || IsIn(kDrawSlots, sizeof kDrawSlots / sizeof(int), slot) ||
                 IsIn(kCopySlots, sizeof kCopySlots / sizeof(int), slot))) {
                Gp2Event(list, set, slot, a1);
                if (t_reg.open && (slot == 14 || slot == 59)) MlpDispatch(list, set, slot, a1, a2, a3);   // 1.7.47 MLP / MpcCheck
            }
            else if (g_cfg.gq && g_inited == 1 && IsIn(kGqHeadSlots, sizeof kGqHeadSlots / sizeof(int), slot))
                GqHead(list, set, -1);
            else if (slot >= 37 && slot <= 44) {   // 1.7.48 VS: root CBV/SRV/UAV (37-42), IASetIndexBuffer (43), IASetVertexBuffers (44)
                uint32_t pso = 0;   // 1.7.51: the bound PSO set, known in sampled frames only (SetPipelineState is hooked then)
                if (g_stampOn && g_cfg.prof >= 2 && g_inited == 1) {
                    AcquireSRWLockExclusive(&g_lsLock);
                    if (ListState* ls = FindList(list, set, false)) pso = ls->ok ? ls->bound : 0;
                    ReleaseSRWLockExclusive(&g_lsLock);
                }
                memstats::OnBind(slot, a1, a2, a3, CurMarker(), pso);
            }
            break;
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
struct A2 {
    double ticks = 0;
    uint64_t n = 0, draws = 0;
};
struct Win2 {   // GP2 30 s window
    std::unordered_map<uint64_t, A2> m;   // bucket | marker << 32 | ctype << 48
    double covered = 0, span = 0, async = 0;
    uint64_t frames = 0, intervals = 0, draws = 0, bad = 0;
};
Win2 g_win2;
SRWLOCK g_aggLock = SRWLOCK_INIT;
char g_status[256] = "";
SRWLOCK g_statusLock = SRWLOCK_INIT;

// ---- GQ window (Present thread fills it in ProcessSlot, the stats thread logs it) ----------------------------------------------
struct GqEdge { double ticks = 0, maxT = 0; uint64_t n = 0; uint32_t ra0 = 0, ra1 = 0; };
struct GqGap {   // one idle stretch of the DIRECT queues between two consecutive measured lists
    double t;
    uint16_t pMk, nMk;        // previous list's last marker, next list's first marker
    uint32_t pSub, nSub;      // submission numbers (0 = no ECL seen)
    uint32_t pRa, nRa, nRa1;  // ECL call sites (RVA; 1 = not the exe)
    uint16_t pPos, nPos, nLists;
    uint8_t unknown;          // 1: other (unmeasured) submissions lie between the two lists
};
struct GqWin {
    uint64_t frames = 0, lists = 0, listsEcl = 0, listsNoCov = 0, gaps = 0, gapsUnk = 0;
    double span = 0, busy = 0, gapT = 0, gapUnkT = 0, heads = 0, tails = 0, noCovT = 0;
    std::vector<float> perFrameGapMs;
    std::unordered_map<uint32_t, GqEdge> edges;   // prev last marker << 16 | next first marker (known gaps only)
    std::vector<GqGap> worst;                     // the gqTop largest single gaps, largest first
};
GqWin g_gqWin;
double g_gp2UncovMs = -1;   // GP2 span - covered of the last logged window (for the GQ line)

void GqKeepWorst(std::vector<GqGap>& v, const GqGap& g, size_t n) {
    if (v.size() >= n && g.t <= v.back().t) return;
    auto it = std::upper_bound(v.begin(), v.end(), g, [](const GqGap& a, const GqGap& b) { return a.t > b.t; });
    v.insert(it, g);
    if (v.size() > n) v.pop_back();
}

// S.lock and g_aggLock held
void GqProcess(Slot& S, double maxTicks) {
    if (S.gq.empty()) return;
    struct L { uint64_t h, e, w, c; const GqRec* r; };
    static std::vector<L> v;
    v.clear();
    for (const GqRec& rc : S.gq) {
        const uint64_t h = g_rbMap[rc.qh], e = g_rbMap[rc.qe];
        if (!(h && e && e >= h && (double)(e - h) < maxTicks)) {
            InterlockedIncrement64(&g_gqBad);
            continue;
        }
        const uint64_t w = rc.qw >= 0 ? g_rbMap[rc.qw] : 0, c = rc.qc >= 0 ? g_rbMap[rc.qc] : 0;
        v.push_back(L{h, e, (w >= h && w <= e) ? w : 0, (c >= h && c <= e) ? c : 0, &rc});
    }
    if (v.empty()) return;
    std::sort(v.begin(), v.end(), [](const L& a, const L& b) { return a.h < b.h; });
    GqWin& W = g_gqWin;
    const size_t n = v.size();
    double frameGap = 0, frameUnk = 0;
    uint64_t curEnd = v[0].e;
    size_t owner = 0;   // the list that ends last so far
    for (size_t i = 0; i < n; ++i) {
        const L& x = v[i];
        W.lists++;
        if (x.r->sub) W.listsEcl++;
        if (x.w) W.heads += (double)(x.w - x.h);
        else { W.listsNoCov++; W.noCovT += (double)(x.e - x.h); }
        if (x.c) W.tails += (double)(x.e - x.c);
        if (i == 0) continue;
        if (x.h > curEnd) {
            const double gt = (double)(x.h - curEnd);
            const GqRec& p = *v[owner].r;
            const GqRec& q = *x.r;
            const bool unk = p.sub && q.sub && q.sub != p.sub + 1;
            GqGap g{gt, p.lastMk, q.firstMk, p.sub, q.sub, p.eclRa0, q.eclRa0, q.eclRa1, (uint16_t)std::min<size_t>(owner, 65535),
                    (uint16_t)std::min<size_t>(i, 65535), (uint16_t)std::min<size_t>(n, 65535), (uint8_t)(unk ? 1 : 0)};
            if (unk) {
                frameUnk += gt;
                W.gapsUnk++;
            } else {
                frameGap += gt;
                W.gaps++;
                GqEdge& ed = W.edges[((uint32_t)p.lastMk << 16) | q.firstMk];
                ed.ticks += gt;
                ed.n++;
                if (gt > ed.maxT) { ed.maxT = gt; ed.ra0 = q.eclRa0; ed.ra1 = q.eclRa1; }
            }
            GqKeepWorst(W.worst, g, (size_t)g_cfg.gqTop);
        }
        if (x.e > curEnd) { curEnd = x.e; owner = i; }
    }
    const double span = (double)(curEnd - v[0].h);
    W.frames++;
    W.span += span;
    W.gapT += frameGap;
    W.gapUnkT += frameUnk;
    W.busy += span - frameGap - frameUnk;
    if (W.perFrameGapMs.size() < 65536) W.perFrameGapMs.push_back((float)(frameGap / g_ticksPerMs));
}

void ProcessSlot(int r) {
    Slot& S = g_slot[r];
    static double childSum[kMaxTid][kMaxDepth + 2];
    memset(childSum, 0, sizeof(childSum));
    double top = 0;
    uint64_t lo = ~0ull, hi = 0;
    // GP2 sampling: only frames that were stamped count. GQ: the end stamp of an armed list may land in an unsampled frame's
    // slot, so with GQ on a slot counts when it holds GP2 PSO stamps or marker records
    const bool had = g_cfg.prof < 2 || (g_cfg.gq ? (g_stampsUsed[r] > 0 || !S.recs.empty()) : S.used > 0);
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
    if (had)
        for (Agg* a : {&g_win, &g_short}) {
            a->top += top;
            a->span += hi > lo ? (double)(hi - lo) : 0.0;
            a->frames++;
        }
    if (g_cfg.prof >= 2) {   // GP2 intervals: per (bucket, marker) time; DIRECT-queue intervals make up "covered"
        uint64_t l2 = ~0ull, h2 = 0;
        Win2& w = g_win2;
        for (const Rec2& rc : S.recs2) {
            const uint64_t b = g_rbMap[rc.b], e = g_rbMap[rc.e];
            if (!(b && e && e >= b && (double)(e - b) < maxTicks)) {
                ++w.bad;
                continue;
            }
            const double dt = (double)(e - b);
            A2& a = w.m[(uint64_t)rc.bucket | ((uint64_t)rc.marker << 32) | ((uint64_t)rc.ctype << 48)];
            a.ticks += dt;
            a.n++;
            a.draws += rc.draws;
            w.intervals++;
            w.draws += rc.draws;
            if (rc.ctype == 0) {
                w.covered += dt;
                l2 = std::min(l2, b);
                h2 = std::max(h2, e);
            } else {
                w.async += dt;
            }
        }
        w.span += h2 > l2 ? (double)(h2 - l2) : 0.0;
        if (had) w.frames++;
    }
    if (g_cfg.gq) GqProcess(S, maxTicks);
    if (g_cfg.mlp) MlpProcess(S, r, maxTicks);
    ReleaseSRWLockExclusive(&g_aggLock);
    const LONG usedNow = S.used;
    const LONG used = std::min<LONG>(usedNow, (LONG)g_qPerSlot);
    if (used > 0) memset((void*)(g_rbMap + (size_t)r * g_qPerSlot), 0, (size_t)used * 8);
    S.recs.clear();
    S.recs2.clear();
    S.gq.clear();
    S.mlp.clear();
    S.mlpReg.clear();
    if (g_mlpRbMap) {
        const LONG ns = std::min<LONG>((LONG)S.mlpStats, (LONG)kMlpStatsPer);
        if (ns > 0) memset((void*)(g_mlpRbMap + (size_t)r * kMlpStatsPer * kStatsWords), 0, (size_t)ns * kStatsWords * 8);
    }
    InterlockedExchange(&S.mlpStats, 0);
    S.gen++;
    InterlockedExchange(&g_stampsUsed[r], 0);
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
        uint8_t* g = *reinterpret_cast<uint8_t* volatile*>(g_base + Grva(kRvaG));
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
                        const void* iface = *reinterpret_cast<void* volatile*>(g_base + Grva(kRvaIface));
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


void LogMlp() {
    MlpWin* w = new (std::nothrow) MlpWin();
    if (!w) return;
    AcquireSRWLockExclusive(&g_aggLock);
    std::swap(*w, g_mlpWin);
    ReleaseSRWLockExclusive(&g_aggLock);
    const double ms = g_ticksPerMs, fT = w->framesT ? (double)w->framesT : 1.0;
    // floor: median us/call of the dispatch rows that launch at most one wave (stats frames) = the fixed cost of a dependent stage
    std::vector<double> tinyUs;
    double tinyMs = 0, zeroMs = 0, tinyN = 0, zeroN = 0;
    for (const auto& kv : w->ops) {
        const MlpOpA& a = kv.second;
        if (!a.n || !a.ns) continue;
        const double us = a.t / (double)a.n / ms * 1000.0;
        const double ft = (double)a.tiny / (double)a.ns, fz = (double)a.zero / (double)a.ns;
        if (ft >= 0.9) tinyUs.push_back(us);
        tinyMs += a.t / fT / ms * ft;
        zeroMs += a.t / fT / ms * fz;
        tinyN += (double)a.n / fT * ft;
        zeroN += (double)a.n / fT * fz;
    }
    std::sort(tinyUs.begin(), tinyUs.end());
    const double floorUs = tinyUs.empty() ? 0.0 : tinyUs[tinyUs.size() / 2];
    MlpRegA T;
    for (const auto& kv : w->reg) {
        const MlpRegA& a = kv.second;
        T.tot += a.tot; T.disp += a.disp; T.bar += a.bar; T.gap += a.gap; T.nd += a.nd; T.ne += a.ne; T.nbc += a.nbc; T.nbs += a.nbs; T.nbu += a.nbu; T.n += a.n;
    }
    // structural share: barriers + gaps + the per-dispatch floor; the rest of the dispatch time is work above the floor
    double workMs = 0;
    for (const auto& kv : w->ops) {
        const MlpOpA& a = kv.second;
        if (!a.n) continue;
        const double per = a.t / (double)a.n / ms * 1000.0;
        workMs += std::max(0.0, per - floorUs) * (double)a.n / fT / 1000.0;
    }
    const double totMs = T.tot / fT / ms;
    GLOG("MLP: %llu timing + %llu stats frames; meshlet front-end %.3f ms/frame = dispatch %.3f + barrier %.3f + gap %.3f | %.1f regions, %.1f Dispatch + "
         "%.1f ExecuteIndirect, %.1f barrier calls (%.1f structs, %.1f UAV)/frame | <= 1 wave (<= 64 threads): %.1f dispatches %.3f ms/frame, empty (0 threads): "
         "%.1f dispatches %.3f ms/frame | floor (median us of <= 1-wave dispatches) %.1f us -> structural (barrier + gap + floor x dispatches) %.3f ms, work above "
         "the floor %.3f ms | dropped: straddling/unstamped regions %lld, bad %llu, stats queries full %lld, list switches inside regions %lld",
         (unsigned long long)w->framesT, (unsigned long long)w->framesS, totMs, T.disp / fT / ms, T.bar / fT / ms, T.gap / fT / ms, (double)T.n / fT,
         (double)T.nd / fT, (double)T.ne / fT, (double)T.nbc / fT, (double)T.nbs / fT, (double)T.nbu / fT, tinyN, tinyMs, zeroN, zeroMs, floorUs,
         std::max(0.0, totMs - workMs), workMs, (long long)g_mlpXFrame, (unsigned long long)w->bad, (long long)g_mlpStatsFull, (long long)g_mlpXList);
    GLOG("%s", "MLP: regions: region#ordinal @caller [parent marker] | per frame | ms/frame total | dispatch | barrier | gap | us/instance | Dispatch+EI | "
               "barrier calls | caller match on the first UAV barrier");
    struct RR { uint64_t k; const MlpRegA* a; };
    std::vector<RR> rr;
    for (const auto& kv : w->reg) rr.push_back(RR{kv.first, &kv.second});
    std::sort(rr.begin(), rr.end(), [](const RR& x, const RR& y) { return x.a->tot > y.a->tot; });
    int rank = 0;
    for (const RR& x : rr) {
        if (++rank > 30) break;
        const MlpRegA& a = *x.a;
        const int reg = (int)(x.k & 0xFF), ord = (int)((x.k >> 8) & 0xFF);
        const uint16_t par = (uint16_t)((x.k >> 16) & 0xFFFF);
        char tg[16];
        MlpTagStr((uint32_t)(x.k >> 32), tg, sizeof tg);
        GLOG("MLP:   %s#%d @%s [%s] | %.2f | %.3f | %.3f | %.3f | %.3f | %.1f | %.1f+%.1f | %.1f | %.0f%%", kMlpShort[reg < kNumMlpReg ? reg : 0], ord, tg,
             MkName(par), (double)a.n / fT, a.tot / fT / ms, a.disp / fT / ms, a.bar / fT / ms, a.gap / fT / ms,
             a.n ? a.tot / (double)a.n / ms * 1000.0 : 0.0, a.n ? (double)a.nd / (double)a.n : 0.0, a.n ? (double)a.ne / (double)a.n : 0.0,
             a.n ? (double)a.nbc / (double)a.n : 0.0, a.n ? 100.0 * (double)a.exact / (double)a.n : 0.0);
    }
    struct OR { const MlpOpA* a; };
    std::vector<OR> ov;
    for (const auto& kv : w->ops) ov.push_back(OR{&kv.second});
    std::sort(ov.begin(), ov.end(), [](const OR& x, const OR& y) { return x.a->t > y.a->t; });
    GLOG("MLP: top %d dispatches: region#ordinal @caller op | cs | [marker] | ms/frame | us/call | calls/frame | Dispatch groups or EI max | avg threads | "
         "<= 1 wave %% | empty %%", g_cfg.mlpTop);
    rank = 0;
    for (const OR& x : ov) {
        if (++rank > g_cfg.mlpTop) break;
        const MlpOpA& a = *x.a;
        const int reg = (int)(a.rkey & 0xFF), ord = (int)((a.rkey >> 8) & 0xFF);
        char tg[16];
        MlpTagStr((uint32_t)(a.rkey >> 32), tg, sizeof tg);
        char inv[64] = "-";
        if (a.ns) _snprintf_s(inv, sizeof inv, _TRUNCATE, "%.0f | %.0f%% | %.0f%%", a.inv / (double)a.ns, 100.0 * (double)a.tiny / (double)a.ns,
                              100.0 * (double)a.zero / (double)a.ns);
        GLOG("MLP:   %s#%d @%s op%u | %s %08x | [%s] | %.3f | %.1f | %.2f | %.0f | %s", kMlpShort[reg < kNumMlpReg ? reg : 0], ord, tg, a.op,
             a.kind == kMkEI ? "EI" : "D", a.key, MkName(a.marker), a.t / fT / ms, a.n ? a.t / (double)a.n / ms * 1000.0 : 0.0, (double)a.n / fT,
             a.n ? a.dims / (double)a.n : 0.0, inv);
    }
    delete w;
}

// GP2 30 s log (stats thread)
void LogGp2() {
    static int window = 0;
    static double wallOn = 0, wallOff = 0, hookOn = 0;
    static int nOn = 0, nOff = 0;
    static LONG64 pDrop = 0, pRec = 0, pCalls = 0, pTicks = 0, pFull = 0;
    const bool sampling = g_cfg.sampleEvery > 1;
    const bool wasOn = sampling || g_stampOn != 0;
    static LONG64 pSF = 0, pStamps = 0;
    const LONG64 wtS = InterlockedExchange64(&g_wallTicksS, 0), wfS = InterlockedExchange64(&g_wallFramesS, 0);
    const LONG64 wtU = InterlockedExchange64(&g_wallTicksU, 0), wfU = InterlockedExchange64(&g_wallFramesU, 0);
    const LONG64 sf = g_sampledFrames - pSF, stampsNow = g_stampsTaken - pStamps;
    pSF = g_sampledFrames; pStamps = g_stampsTaken;
    LARGE_INTEGER qf;
    QueryPerformanceFrequency(&qf);
    const LONG64 wt = InterlockedExchange64(&g_wallTicks, 0), wf = InterlockedExchange64(&g_wallFrames, 0);
    const double wallMs = wf ? (double)wt / (double)qf.QuadPart * 1000.0 / (double)wf : 0.0;
    Win2* w = new (std::nothrow) Win2();
    if (!w) return;
    AcquireSRWLockExclusive(&g_aggLock);
    std::swap(*w, g_win2);
    ReleaseSRWLockExclusive(&g_aggLock);
    const double fr = w->frames ? (double)w->frames : 1.0, ms = g_ticksPerMs;
    g_gp2UncovMs = w->frames ? (w->span - w->covered) / fr / ms : -1;   // GQ compares its decomposition with this
    const LONG64 calls = g_hookCalls - pCalls, ticks = g_hookTicks - pTicks;
    const double hookMsFrame = wf ? (double)ticks / (double)qf.QuadPart * 1000.0 / (double)wf : 0.0;
    const double callsFrame = wf ? (double)calls / (double)wf : 0.0;
    const double nsCall = calls ? (double)ticks / (double)qf.QuadPart * 1e9 / (double)calls : 0.0;
    pCalls = g_hookCalls; pTicks = g_hookTicks;
    if (sampling) {
    } else if (wasOn) {
        wallOn += wallMs; ++nOn; hookOn = hookMsFrame;
    } else {
        wallOff += wallMs; ++nOff;
    }
    GLOG2("frame %.2f ms GPU span (wall %.2f ms); covered %.2f ms (%.0f %% of span); async-compute %.2f ms; %.0f intervals/frame, "
         "%.0f draws+dispatches/frame, PSO stamps %s | dropped: cap %lld, rec %lld, unresolved %lld, heap full %lld | sampled %lld frames (1 in %d), min run %d draws",
         w->span / fr / ms, wallMs, w->covered / fr / ms, w->span > 0 ? 100.0 * w->covered / w->span : 0.0, w->async / fr / ms,
         (double)w->intervals / fr, (double)w->draws / fr, wasOn ? "ON" : "OFF (A/B overhead window)", g_stampDrop - pDrop,
         g_rec2Full - pRec, (long long)w->bad, g_full - pFull, (long long)w->frames, g_cfg.sampleEvery, g_cfg.minRun);
    pDrop = g_stampDrop; pRec = g_rec2Full; pFull = g_full;
    if (wasOn) {
        struct Row { uint64_t key; A2 a; };
        std::vector<Row> rows;
        rows.reserve(w->m.size());
        for (const auto& kv : w->m) rows.push_back(Row{kv.first, kv.second});
        std::sort(rows.begin(), rows.end(), [](const Row& x, const Row& y) { return x.a.ticks > y.a.ticks; });
        GLOG2("top %d of %zu (PSO shader set, marker) buckets - hash stage(s) | ms/frame | draws+dispatches/frame | intervals/frame | [marker]",
             g_cfg.top2, rows.size());
        int rank = 0;
        for (const Row& r : rows) {
            if (++rank > g_cfg.top2) break;
            const uint32_t bk = (uint32_t)(r.key & 0xFFFFFFFFu);
            const uint16_t mk = (uint16_t)((r.key >> 32) & 0xFFFFu);
            const int ct = (int)((r.key >> 48) & 0xFFu);
            char desc[360];
            if (bk == kBkNoPso) strcpy_s(desc, "(no PSO bound yet)");
            else if (bk == kBkCopy) strcpy_s(desc, "(copy / resolve / clear)");
            else if (bk & 0x80000000u) _snprintf_s(desc, sizeof desc, _TRUNCATE, "(PSO not created through the hooks @%07x)", bk & 0x7FFFFFFFu);
            else if (!psoswap::DescribeSet(bk, desc, sizeof desc)) strcpy_s(desc, "(unnamed set)");
            GLOG2("  %2d %s%s | %7.3f ms | %7.1f | %7.1f | [%s]", rank, desc, ct ? " (async compute)" : "", r.a.ticks / fr / ms,
                 (double)r.a.draws / fr, (double)r.a.n / fr, mk == kNoMarker ? "-" : NameOf(mk));
        }
    }
    if (sampling) {
        const double fq = (double)qf.QuadPart / 1000.0;
        const double mS = wfS ? (double)wtS / fq / (double)wfS : 0.0, mU = wfU ? (double)wtU / fq / (double)wfU : 0.0;
        const double d = (wfS && wfU) ? mS - mU : 0.0, amort = d / (double)g_cfg.sampleEvery;
        const double hookPerS = sf ? (double)ticks / fq / (double)sf : 0.0;
        GLOG2("overhead: wall frame, sampled %.2f ms (%lld) vs unsampled %.2f ms (%lld) -> %+.2f ms on a sampled frame = %+.2f ms/frame averaged "
             "(%+.1f %%; the cost can land one present late); stamps %.0f/sampled frame for %.0f draws; hook CPU %.3f ms/sampled frame "
             "(%.0f calls, ~%.0f ns/call, sampled) = %.3f ms/frame averaged; the rest of a sampled frame's cost is GPU-side (timestamp writes)",
             mS, (long long)wfS, mU, (long long)wfU, d, amort, mU > 0 ? 100.0 * amort / mU : 0.0, sf ? (double)stampsNow / (double)sf : 0.0,
             (double)w->draws / fr, hookPerS, sf ? (double)calls / (double)sf : 0.0, nsCall, hookMsFrame);
    } else if (nOn && nOff) {
        const double on = wallOn / nOn, off = wallOff / nOff;
        GLOG2("overhead: wall frame with PSO stamps ON %.2f ms (%d windows) vs OFF %.2f ms (%d windows) -> %+.2f ms/frame (%+.1f %%; scene "
             "changes between windows add noise); hook CPU %.3f ms/frame (%.0f calls/frame, ~%.0f ns/call, sampled); mod GPU cost "
             "= the extra timestamp writes (%.0f/frame)",
             on, nOn, off, nOff, on - off, off > 0 ? 100.0 * (on - off) / off : 0.0, hookOn, callsFrame, nsCall,
             (double)w->intervals / fr);
    } else {
        GLOG2("overhead: hook CPU %.3f ms/frame (%.0f calls/frame, ~%.0f ns/call, sampled), wall frame %.2f ms; A/B windows follow",
             hookMsFrame, callsFrame, nsCall, wallMs);
    }
    delete w;
    ++window;   // A/B (SampleEvery = 1 only): windows 2 and 4 (index 1 and 3) run with the PSO stamps off
    if (!sampling) InterlockedExchange(&g_stampOn, (g_cfg.ab && (window == 1 || window == 3)) ? 0 : 1);
}

// GQ 30 s log (stats thread), after the GP2 block. Format: notes\gq-jt-probes.md
void FmtRa(char* b, size_t n, uint32_t ra) {
    if (ra == 0) _snprintf_s(b, n, _TRUNCATE, "-");
    else if (ra == 1) _snprintf_s(b, n, _TRUNCATE, "ext");
    else _snprintf_s(b, n, _TRUNCATE, "%X", ra);
}
const char* MkName(uint16_t mk) { return mk == kNoMarker ? "-" : NameOf(mk); }

void LogGq() {
    static LONG64 pArmed = 0, pNoHead = 0, pFull = 0, pPend = 0, pLate = 0, pBad = 0;
    GqWin* w = new (std::nothrow) GqWin();
    if (!w) return;
    AcquireSRWLockExclusive(&g_aggLock);
    std::swap(*w, g_gqWin);
    ReleaseSRWLockExclusive(&g_aggLock);
    const double fr = w->frames ? (double)w->frames : 1.0, ms = g_ticksPerMs;
    std::vector<float>& pf = w->perFrameGapMs;
    std::sort(pf.begin(), pf.end());
    auto pct = [&](double p) { return pf.empty() ? 0.0 : (double)pf[std::min(pf.size() - 1, (size_t)(p * (double)(pf.size() - 1) + 0.5))]; };
    const double gapMs = w->gapT / fr / ms, headMs = w->heads / fr / ms, tailMs = w->tails / fr / ms, noCovMs = w->noCovT / fr / ms;
    const double unkMs = w->gapUnkT / fr / ms;
    char uncov[96];
    if (g_gp2UncovMs >= 0)
        _snprintf_s(uncov, sizeof uncov, _TRUNCATE, "%.3f ms (gaps + heads + no-cov = %.3f)", g_gp2UncovMs, gapMs + unkMs + headMs + noCovMs);
    else
        strcpy_s(uncov, "n/a");
    GQLOG("frame: %.0f DIRECT lists/frame (%.0f %% with ECL info) over %llu sampled frames; span %.3f ms, lists busy %.3f ms, "
          "GAPS %.3f ms/frame (%.1f gaps/frame; per frame p50 %.3f p90 %.3f max %.3f ms), gaps around unmeasured submissions %.3f ms "
          "(%.1f/frame, excluded), list HEADS %.3f ms (start -> first GP2 stamp), tails %.3f ms, lists without GP2 coverage %.3f ms | "
          "GP2 uncovered (span - covered) %s | armed %lld, no head %lld, dropped: rec cap %lld, ECL table %lld, late %lld, bad ts %lld",
          (double)w->lists / fr, w->lists ? 100.0 * (double)w->listsEcl / (double)w->lists : 0.0, (unsigned long long)w->frames,
          w->span / fr / ms, w->busy / fr / ms, gapMs, (double)w->gaps / fr, pct(0.5), pct(0.9), pf.empty() ? 0.0 : (double)pf.back(),
          unkMs, (double)w->gapsUnk / fr, headMs, tailMs, noCovMs, uncov, g_gqArmed - pArmed, g_gqNoHead - pNoHead, g_gqFull - pFull,
          g_gqPendFull - pPend, g_gqLate - pLate, g_gqBad - pBad);
    pArmed = g_gqArmed; pNoHead = g_gqNoHead; pFull = g_gqFull; pPend = g_gqPendFull; pLate = g_gqLate; pBad = g_gqBad;
    struct E { uint32_t key; GqEdge e; };
    std::vector<E> es;
    es.reserve(w->edges.size());
    for (const auto& kv : w->edges) es.push_back(E{kv.first, kv.second});
    std::sort(es.begin(), es.end(), [](const E& a, const E& b) { return a.e.ticks > b.e.ticks; });
    GQLOG("top %d of %zu gap edges [previous list's last marker -> next list's first marker] | ms/frame | gaps/frame | mean us | max us | "
          "ECL call site of the next list at the max (return RVA < next game frame)", g_cfg.gqTop, es.size());
    int rank = 0;
    for (const E& x : es) {
        if (++rank > g_cfg.gqTop) break;
        char r0[16], r1[16];
        FmtRa(r0, sizeof r0, x.e.ra0);
        FmtRa(r1, sizeof r1, x.e.ra1);
        GQLOG("  %d [%s -> %s] | %.3f ms | %.2f | %.1f | %.1f | %s < %s", rank, MkName((uint16_t)(x.key >> 16)), MkName((uint16_t)(x.key & 0xFFFF)),
              x.e.ticks / fr / ms, (double)x.e.n / fr, x.e.n ? x.e.ticks / (double)x.e.n / ms * 1000.0 : 0.0, x.e.maxT / ms * 1000.0, r0, r1);
    }
    GQLOG("largest %zu single gaps: us | list k-1 -> k of n (GPU order in the frame) | [last marker] -> [first marker] | submissions | "
          "ECL call sites prev -> next (< next game frame)", w->worst.size());
    rank = 0;
    for (const GqGap& g : w->worst) {
        char a[16], b[16], c[16];
        FmtRa(a, sizeof a, g.pRa);
        FmtRa(b, sizeof b, g.nRa);
        FmtRa(c, sizeof c, g.nRa1);
        GQLOG("  %d %.1f us | %u -> %u of %u | [%s] -> [%s] | %u -> %u%s | %s -> %s < %s", ++rank, g.t / ms * 1000.0, g.pPos, g.nPos, g.nLists,
              MkName(g.pMk), MkName(g.nMk), g.pSub, g.nSub, g.unknown ? " (UNMEASURED submissions between)" : "", a, b, c);
    }
    delete w;
}

}  // namespace

volatile long g_gqHeads = 0;

void OnListHead(void* list, int set) {
    if (!g_gqHeads || !g_stampOn || !g_on || g_inited != 1) return;
    __try {
        GqHead(list, set, -1);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_gqHeads, 0);
        InterlockedExchange(&g_on, 0);
        InterlockedExchange(&g_failed, 1);
        GLOG("exception 0x%08lX in the GQ barrier hook - GP switched off", GetExceptionCode());
    }
}

volatile long g_mlpBarriers = 0;
volatile long g_fcCap = 0;
volatile long g_fcEcl = 0;
#include "fcap.h"   // FC frame capture (namespace gp::fc)
void FcListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t* regs) {
    if (!g_fcCap) return;
    __try {
        fc::ListCall(list, set, slot, a1, a2, a3, regs);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_fcCap, 0);
        InterlockedExchange(&fc::s_capOn, 0);
        GLOG("FC: exception 0x%08lX in the command-list hook (slot %d) - capture aborted", GetExceptionCode(), slot);
    }
}

// 1.7.47 MLP / MpcCheck: gpuopt's pass-through ResourceBarrier path, before (post = false) and after (post = true) the original call
static void OnBarrierImpl(void* list, int set, unsigned n, const void* bp, bool post) {
    const D3D12_RESOURCE_BARRIER* b = (const D3D12_RESOURCE_BARRIER*)bp;
    if (!post) {
        uint8_t uav = 0;
        for (unsigned i = 0; i < n && b; ++i)
            if (b[i].Type == D3D12_RESOURCE_BARRIER_TYPE_UAV) {
                if (uav < 255) ++uav;
                if (!t_reg.firstRes && b[i].UAV.pResource) t_reg.firstRes = b[i].UAV.pResource;
            }
        if (g_cfg.mlp && g_cfg.mlpBarriers) MlpEvent(list, set, kMkBB, 0, n, uav);
    } else {
        if (g_cfg.mlp && g_cfg.mlpBarriers) MlpEvent(list, set, kMkBE, 0, 0, 0);
        if (t_reg.check) mpcck::OnBarrier(list, set, n, b);
    }
}
void OnBarrier(void* list, int set, unsigned n, const void* barriers, bool post, void* ra) {
    if (g_fcCap && !post) {
        __try {
            fc::Barrier(list, set, n, barriers, ra);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&g_fcCap, 0);
            GLOG("FC: exception 0x%08lX in the barrier hook - capture aborted", GetExceptionCode());
        }
    }
    if (!t_reg.open || !g_on || g_inited != 1) return;
    __try {
        OnBarrierImpl(list, set, n, barriers, post);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_mlpBarriers, 0);
        InterlockedExchange(&g_on, 0);
        InterlockedExchange(&g_failed, 1);
        GLOG("exception 0x%08lX in the MLP barrier hook - GP switched off", GetExceptionCode());
    }
}

static void OnEclImpl(void* queue, unsigned n, void* const* lists, void* retAddr) {
    static thread_local void* t_q = nullptr;
    static thread_local bool t_direct = false;
    if (queue != t_q) {   // GetDesc only when this thread switches queues
        t_q = queue;
        t_direct = ((ID3D12CommandQueue*)queue)->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    if (!t_direct) return;
    const LONG64 base = InterlockedExchangeAdd64(&g_gqSub, (LONG64)n);
    GqPend found[16];
    int fi[16];
    int nf = 0;
    AcquireSRWLockExclusive(&g_gqLock);
    for (unsigned i = 0; i < n && nf < 16; ++i)
        for (GqPend& p : g_gqPend)
            if (p.list && p.list == lists[i]) {
                found[nf] = p;
                fi[nf++] = (int)i;
                p.list = nullptr;
                break;
            }
    ReleaseSRWLockExclusive(&g_gqLock);
    if (!nf) return;
    uint32_t ra0 = ExeRva(retAddr), ra1 = 0;
    void* fr[12];
    const USHORT nfr = RtlCaptureStackBackTrace(0, 12, fr, nullptr);
    bool seen = false;
    for (USHORT k = 0; k < nfr; ++k) {   // the game frame above the ECL call site
        if (fr[k] == retAddr) { seen = true; continue; }
        if (seen) { ra1 = ExeRva(fr[k]); break; }
    }
    for (int k = 0; k < nf; ++k) {
        Slot& S = g_slot[found[k].slot];
        AcquireSRWLockExclusive(&S.lock);
        if (S.gen == found[k].gen && found[k].idx < S.gq.size()) {
            GqRec& r = S.gq[found[k].idx];
            r.sub = (uint32_t)(base + fi[k] + 1);
            r.eclRa0 = ra0;
            r.eclRa1 = ra1;
            r.eclN = (uint16_t)std::min<unsigned>(n, 65535);
            r.eclIdx = (uint16_t)fi[k];
        } else {
            InterlockedIncrement64(&g_gqLate);
        }
        ReleaseSRWLockExclusive(&S.lock);
    }
}

void OnEcl(void* queue, unsigned n, void* const* lists, void* retAddr, bool gameCaller) {
    if (g_fcEcl && queue) {
        __try {
            fc::Ecl(queue, n, lists, retAddr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            InterlockedExchange(&g_fcEcl, 0);
            GLOG("FC: exception 0x%08lX in the ECL hook - FC queue rows off", GetExceptionCode());
        }
    }
    (void)gameCaller;   // our own overlay's lists count too: they occupy the queue like the game's
    if (!g_cfg.gq || g_inited != 1 || !g_on || !queue || !n || !lists) return;
    __try {
        OnEclImpl(queue, n, lists, retAddr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_gqHeads, 0);
        GLOG("exception 0x%08lX in the GQ ECL hook - GQ ECL info off", GetExceptionCode());
        g_cfg.gq = 0;
    }
}

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
    g_cfg.prof = I(L"GPUProf", 0, 0, 2);
    g_cfg.markers = I(L"GPUProf_Markers", 0, 0, 1);
    g_cfg.maxMarkers = I(L"GPUProf_MaxMarkers", 2048, 256, 8192);
    g_cfg.top = I(L"GPUProf_Top", 30, 5, 60);
    g_cfg.maxStamps = I(L"GPUProf2_MaxStamps", 8192, 1024, 65536);
    g_cfg.top2 = I(L"GPUProf2_Top", 40, 5, 100);
    g_cfg.ab = I(L"GPUProf2_AB", 1, 0, 1);
    g_cfg.sampleEvery = I(L"GPUProf2_SampleEvery", 8, 1, 64);
    g_cfg.minRun = I(L"GPUProf2_MinRun", 4, 1, 64);
    g_cfg.gq = I(L"GQ", 0, 0, 1);
    g_cfg.gqTop = I(L"GQ_Top", 5, 1, 20);
    g_cfg.mlp = I(L"MeshletProbe", 0, 0, 1);
    g_cfg.mlpTop = I(L"MeshletProbe_Top", 40, 5, 200);
    g_cfg.mlpStats = I(L"MeshletProbe_Stats", 1, 0, 1);
    g_cfg.mlpDump = I(L"MeshletProbe_Dump", 0, 0, 2000);
    g_cfg.mlpBarriers = I(L"MeshletProbe_Barriers", 1, 0, 1);
    g_cfg.mpcCheck = I(L"MpcCheck", 0, 0, 1000000);
    fc::Start(ini, g_cfg.prof);   // FC: markers on (pass names in the capture), hooks via WantsHooks
    if (fc::c.on && g_codeOk) g_cfg.markers = 1;
    if (fc::c.on && !g_codeOk)
        GLOG("%s", "FC: no command-list hooks - the GP code sites were not verified (set [Profiler] GPUProf_Markers = 1 with FrameCap); "
                   "captures will hold queue rows (ECL / Signal / Wait) only");
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
    if (g_cfg.prof >= 2)
        GLOG2("per-PSO attribution on - stamps in 1 frame of %d, intervals >= %d draws, max %d PSO timestamps/frame, top %d rows, overhead %s; PSO->shader map %s",
             g_cfg.sampleEvery, g_cfg.minRun, g_cfg.maxStamps, g_cfg.top2,
             g_cfg.sampleEvery > 1 ? "measured live (sampled vs unsampled frames)" : g_cfg.ab ? "A/B windows 2 and 4 run with PSO stamps off" : "not measured",
             psoswap::Tracking() ? "from the psoswap creation hooks" : "NOT available yet (psoswap starts earlier; check the PSOSWAP lines)");
    if (g_cfg.gq && g_cfg.prof < 2) {
        GQLOG("%s", "GQ needs GPUProf = 2 (it reads the GP2 stamps) - GQ off");
        g_cfg.gq = 0;
    }
    if ((g_cfg.mlp || g_cfg.mpcCheck) && g_cfg.prof < 2) {
        GLOG("%s", "MLP: [Profiler] MeshletProbe / MpcCheck need GPUProf = 2 (they ride on the GP2 command-list hooks) - GPU half off");
        g_cfg.mlp = g_cfg.mpcCheck = 0;
    }
    if (g_cfg.mpcCheck) {
        mpcck::StartInfo ci;
        ci.log = g_log;
        ci.iniPath = ini;
        ci.chains = g_cfg.mpcCheck;
        if (!mpcck::Start(ci)) g_cfg.mpcCheck = 0;
    }
    if (g_cfg.mlp || g_cfg.mpcCheck) {
        InterlockedExchange(&g_mlpOn, 1);
        InterlockedExchange(&g_mlpBarriers, 1);
        if (g_cfg.mlpDump) {
            std::wstring d(ini);
            const size_t sl = d.find_last_of(L"\\/");
            g_mlpDumpPath = (sl == std::wstring::npos ? std::wstring() : d.substr(0, sl + 1)) + L"Nioh3EngineFix_mlp.csv";
        }
        if (g_cfg.mlp)
            GLOG("MLP: meshlet front-end probe on - in sampled frames (1 in %d), inside MeshletResolveLOD / MeshletPreculling / MeshletInstanceCulling / "
                 "MeshletCheckRequestLoad / MeshletRequestLoad / GenerateSceneHiZ / GenerateShadowMapHiZ: a timestamp before every Dispatch / "
                 "ExecuteIndirect and before + after every ResourceBarrier call%s; top %d dispatch rows%s. Needs [GpuOpt] Barriers = 0 (pass-through "
                 "barriers) for the barrier split",
                 g_cfg.sampleEvery, g_cfg.mlpStats ? "; every other sampled frame CS invocations per dispatch (pipeline statistics) instead" : "",
                 g_cfg.mlpTop, g_cfg.mlpDump ? "; raw events of the first sampled frames -> Nioh3EngineFix_mlp.csv" : "");
    }
    if (g_cfg.gq) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_base);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE) {
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(g_base + dos->e_lfanew);
            g_exeLo = (uintptr_t)g_base;
            g_exeHi = g_exeLo + nt->OptionalHeader.SizeOfImage;
        }
        InterlockedExchange(&g_gqHeads, 1);
        GQLOG("graphics-queue gap census on - DIRECT lists Reset in a sampled frame (1 in %d) get a head stamp before their first GPU "
              "command and an end stamp at Close; idle time between consecutive lists per frame, top %d edges / gaps per window; "
              "ECL order and call sites from the overlay's ExecuteCommandLists hook",
              g_cfg.sampleEvery, g_cfg.gqTop);
    }
    if (HANDLE h = CreateThread(nullptr, 0, MarkerThread, nullptr, 0, nullptr)) CloseHandle(h);
}

bool WantsHooks() { return (g_cfg.prof != 0 || fc::c.on != 0) && g_codeOk; }
bool Active() { return g_on != 0 && g_inited == 1; }

void EnableHooks(volatile uint8_t* mask, int slots) {
    if (!WantsHooks() || !mask || slots <= kSlotEndEvent) return;
    if (fc::c.on && slots >= 86) fc::s_mask = mask;   // FC arms every list slot in its capture windows only
    mask[kSlotClose] = 1;
    mask[kSlotReset] = 1;
    mask[kSlotBeginEvent] = 1;
    mask[kSlotEndEvent] = 1;
    if (g_cfg.prof >= 2 && slots > 79) {
        mask[kSlotSetPso] = 1;
        for (int v : kDrawSlots) mask[v] = 1;
        for (int v : kCopySlots) mask[v] = 1;
        if (g_cfg.gq)
            for (int v : kGqHeadSlots) mask[v] = 1;
        if (memstats::BindsWanted()) {   // 1.7.48 VS (MemStats = 2): every frame, not only sampled ones
            for (int v = 37; v <= 44; ++v) mask[v] = 1;
            GLOG("%s", "command-list hooks: + VS root CBV/SRV/UAV, IASetIndexBuffer, IASetVertexBuffers (system-memory read census)");
        }
        g_hookMask = mask;   // OnFrame clears these slots in unsampled frames
        GLOG("%s%s", "command-list hooks: BeginEvent, EndEvent, Close, Reset + GP2: SetPipelineState, Draw*, Dispatch, ExecuteIndirect, DispatchMesh, Copy*, Resolve*, Clear*",
             g_cfg.gq ? " + GQ: ExecuteBundle, DiscardResource, WriteBufferImmediate, BeginRenderPass, Barrier (and ResourceBarrier in gpuopt)" : "");
        return;
    }
    GLOG("%s", "command-list hooks: BeginEvent, EndEvent, Close, Reset");
}

void OnListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    if (!g_on) return;
    __try {
        if (g_cfg.prof >= 2) {   // GP2 overhead: time 1 call in 16 (QPC), count calls in batches of 1024
            static thread_local uint32_t t_n = 0;
            const uint32_t n = ++t_n;
            if (!(n & 1023)) InterlockedAdd64(&g_hookCalls, 1024);
            if (!(n & 15)) {
                LARGE_INTEGER t0, t1;
                QueryPerformanceCounter(&t0);
                OnListCallImpl(list, set, slot, a1, a2, a3);
                QueryPerformanceCounter(&t1);
                InterlockedAdd64(&g_hookTicks, (t1.QuadPart - t0.QuadPart) * 16);
            } else {
                OnListCallImpl(list, set, slot, a1, a2, a3);
            }
        } else {
            OnListCallImpl(list, set, slot, a1, a2, a3);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_on, 0);
        InterlockedExchange(&g_failed, 1);
        GLOG("exception 0x%08lX in the command-list hook (slot %d) - GP switched off", GetExceptionCode(), slot);
    }
}

// GP2 sampling: frame f is stamped when f % SampleEvery == 0. The GP2 list slots are cleared in the hook mask for the other frames,
// so their draws never leave the asm thunk. Lists that straddle a switch forget their open interval (the stale query would pair
// with a query of another frame slot).
void SetSampled(bool on) {
    InterlockedExchange(&g_stampOn, on ? 1 : 0);
    if (g_hookMask) {
        g_hookMask[kSlotSetPso] = on;
        for (int v : kDrawSlots) g_hookMask[v] = on;
        for (int v : kCopySlots) g_hookMask[v] = on;
        if (g_cfg.gq)
            for (int v : kGqHeadSlots) g_hookMask[v] = on;
    }
    AcquireSRWLockExclusive(&g_lsLock);
    for (ListState& l : g_ls) {
        l.ivQ = -1;
        l.bucket = l.runBucket = kBkNoPso;
        l.runW = l.bestW = l.ivW = 0;
        l.bound = kBkNoPso;
        l.boundPtr = 0;   // the mask was off: SetPipelineState calls were missed, re-learn the bound PSO on the next one
    }
    ReleaseSRWLockExclusive(&g_lsLock);
}

void OnFrame() {
    const LONG64 f = InterlockedIncrement64(&g_frame);
    fc::OnFrame();        // FC capture windows (no-op when off)
    mlprobe::OnFrame();   // 1.7.47 MLP CPU half: frame boundary of the recorder call sequences (no-op when off)
    if (!g_on || g_inited != 1) return;
    if (g_cfg.prof >= 2) {   // wall time between presents (the game is GPU-bound, so this tracks GPU frame time)
        static LARGE_INTEGER last = {};
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        if (last.QuadPart) {
            const LONG64 dt = now.QuadPart - last.QuadPart;
            InterlockedAdd64(&g_wallTicks, dt);
            InterlockedIncrement64(&g_wallFrames);
            if (g_cfg.sampleEvery > 1) {   // the interval that just ended belongs to the frame whose state is still current
                InterlockedAdd64(g_stampOn ? &g_wallTicksS : &g_wallTicksU, dt);
                InterlockedIncrement64(g_stampOn ? &g_wallFramesS : &g_wallFramesU);
            }
        }
        last = now;
        if (g_cfg.sampleEvery > 1) {
            const bool want = f % g_cfg.sampleEvery == 0;
            if (want != (g_stampOn != 0)) SetSampled(want);
            if (want) InterlockedIncrement64(&g_sampledFrames);
        }
        if (g_mlpOn) {   // 1.7.47 MLP: sampled frames alternate timestamps / CS-invocation stats (+ MpcCheck); MpcCheck alone: all stats
            const LONG64 k = g_cfg.sampleEvery > 1 ? g_sampledFrames : f;
            InterlockedExchange(&g_mlpStatsFrame, !g_cfg.mlp ? 1 : (g_cfg.mlpStats ? (LONG)(k & 1) : 0));
        }
    }
    if (g_cfg.mpcCheck) mpcck::OnFrame();
    __try {
        ProcessSlot((int)((f + 2) % kRing));  // the slot that was current 6 frames ago; current again 2 frames from now
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_on, 0);
        InterlockedExchange(&g_failed, 1);
        GLOG("exception 0x%08lX while reading the timestamps - GP switched off", GetExceptionCode());
    }
}

// 1.7.36 Tour: windows follow the tour's dwells instead of the 30 s stats period
static volatile LONG g_extWindows = 0;
static void LogLineImpl();

void SetExternalWindows(bool on) { InterlockedExchange(&g_extWindows, on ? 1 : 0); }
void LogLine() {
    if (g_extWindows) return;
    LogLineImpl();
}
void LogWindow(const char* tag) {
    if (tag) fc::TourTag(tag); else fc::TourDwell();   // FC: one capture per tour dwell
    if (!g_cfg.prof) return;
    if (!tag) {  // discard everything gathered so far (the time between two dwells)
        AcquireSRWLockExclusive(&g_aggLock);
        memset(&g_win, 0, sizeof(g_win));
        Win2 empty;
        std::swap(empty, g_win2);
        GqWin emptyGq;
        std::swap(emptyGq, g_gqWin);
        ReleaseSRWLockExclusive(&g_aggLock);
        InterlockedExchange64(&g_wallTicks, 0), InterlockedExchange64(&g_wallFrames, 0);
        InterlockedExchange64(&g_wallTicksS, 0), InterlockedExchange64(&g_wallFramesS, 0);
        InterlockedExchange64(&g_wallTicksU, 0), InterlockedExchange64(&g_wallFramesU, 0);
        return;
    }
    GLOG("window %s", tag);
    LogLineImpl();
}

static void LogLineImpl() {
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
    if (g_cfg.prof >= 2) LogGp2();
    if (g_cfg.gq) LogGq();
    if (g_cfg.mlp) LogMlp();
    if (g_cfg.mpcCheck) mpcck::LogLine();
}

// 1.7.48 VS: engine marker name for a CurMarker() id ("-" = none / unknown)
const char* MarkerName(uint16_t id) {
    if (id == kNoMarker || id >= kMaxNames || (LONG)id >= g_nameCount) return "-";
    return g_names[id].s[0] ? g_names[id].s : "(other)";
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
