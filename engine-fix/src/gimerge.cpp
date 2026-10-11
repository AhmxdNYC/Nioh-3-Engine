// gimerge.cpp - GI gather light-group probe (A0) + merge (A1). See gimerge.h and notes\gi-highest-audit.md s2 / s6.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <intrin.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>

#include "gimerge.h"

namespace gimerge {
namespace {

// ---- constants ---------------------------------------------------------------------------------------------------------
// ID3D12GraphicsCommandList vtable slots (same numbering as gpuopt kSlotInfo)
constexpr int kSlotClose = 9, kSlotReset = 10, kSlotDraw = 12, kSlotDrawIdx = 13, kSlotDispatch = 14;
constexpr int kSlotCopyFirst = 15, kSlotCopyLast = 19;  // CopyBufferRegion .. ResolveSubresource
constexpr int kSlotSetPso = 25, kSlotSetCRootSig = 29, kSlotSetCTable = 31, kSlotSetCRootCbv = 37;
constexpr int kSlotClearFirst = 47, kSlotClearLast = 51;  // ClearDSV .. DiscardResource
constexpr int kSlotExecIndirect = 59, kSlotRB = 26;
// ID3D12Device / ID3D12Resource vtable slots
constexpr int kDevCreateGfx = 10, kDevCreateCs = 11, kDevCreateCbv = 17;
constexpr int kResMap = 8, kResUnmap = 9;

constexpr int kRoot = 32;        // root parameter indices tracked (higher ones: a rebind while holding replays at once)
constexpr int kMaxCb = 768;      // largest target cbuffer (544 rounded up to 256)
constexpr int kMaxTargets = 12;
constexpr int kPsoTab = 1 << 16; // open-addressing PSO -> target table (power of two)

const GUID kIID_Device = {0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};

// ---- targets: every shader of the Ally/PC dump whose $Globals holds gv_mask_param (shaderdump_172\asm) ----------------
// container digest (psoswap / GP2 key) + DXIL HASH-part digest; $Globals size, gv_mask_param and gv_info_out offsets.
// Only #121 and #414 have the exactness proof of notes\gi-highest-audit.md s2 (every UAV store after the test,
// gv_mask_param.x/.y used only by the test, no read of what the other group writes); the rest are probe-only.
struct TargetDef { const char* name; const char* dig; const char* hash; uint16_t cbSize, maskOff, infoOff; bool mergeable; };
const TargetDef kTargetDefs[] = {
    {"rp121", "4b4c3cbe6724e06841b7ed69d7472a46", "dcbbd6ebfcf76369fa89bbcebfcf2285", 512, 464, 144, true},
    {"rp414", "6ae59bd0ddf1220dc8907954b1f215b4", "89cf7520b494ac7af097b45f5d6449d4", 544, 480, 176, true},
    {"b1bdb2aa", "b1bdb2aa4d1f6d53dd0ff3d56b1b34dc", "db381fe41737be441af527f822f3922d", 512, 464, 144, false},
    {"a84f31ad", "a84f31ad33d5866d604864c8b0bbfb67", "cd11a954bd5cb283211708b13549aa6c", 544, 480, 176, false},
    {"fd38d1f8", "fd38d1f8feaeb80d4fd79d999e845684", "82200f998228c894527c7e903b6cffb0", 544, 480, 176, false},
    {"82be364d", "82be364d6fa4a1932f29c676798c8c7d", "70c661d76186a2924564ce489ab60d3e", 512, 448, 160, false},
    {"94b88d81", "94b88d81072682b8380ecfeaa11325a1", "508d678bc42e39b52bb3885e94982793", 512, 448, 160, false},
    {"e7a03d2c", "e7a03d2c5b55c5035b01fff9b1e30d97", "eec4b08892e94fa8cea2c5f789586e51", 512, 448, 160, false},
    {"b01973eb", "b01973eb624643185a6815daadef988d", "48b569f42689e7f30172ffc4ba7caa88", 368, 304, 0, false},
};
constexpr int kNumTargets = (int)(sizeof kTargetDefs / sizeof kTargetDefs[0]);
static_assert(kNumTargets <= kMaxTargets, "kMaxTargets");
uint8_t g_tdig[kMaxTargets][16], g_thash[kMaxTargets][16];

// ---- config / logging ---------------------------------------------------------------------------------------------------
void (*g_log)(const char*, ...) = nullptr;
std::string g_wine;
struct Cfg {
    int probe = 0;         // [Debug] GIProbe
    int probeEvery = 300;  // [Debug] GIProbeEvery (frames)
    int probeFrames = 30;  // [Debug] GIProbeFrames (detail frames per session)
    int merge = 0;         // [Patches] GIMerge: 0 off, 1 merge, 2 observe only
    int qualify = 60;      // [Patches] GIMergeQualify: clean pairs in a row before merging
} g_cfg;

#define LOG(...)                                  \
    do {                                          \
        if (g_log) {                              \
            char _b[900];                         \
            snprintf(_b, sizeof _b, __VA_ARGS__); \
            g_log("GIMERGE: %s", _b);             \
        }                                         \
    } while (0)

// ---- counters -----------------------------------------------------------------------------------------------------------
struct MaskSeen { volatile LONG64 n; uint32_t x, y; };
struct TargetCnt {
    volatile LONG64 dispatches, groups, noCbv, unreadable;
    MaskSeen masks[4];
};
struct Counters {
    TargetCnt t[kMaxTargets];
    volatile LONG64 pairsOk, pairsBad, held, merged, mergedGroups, replayed, replayUav, conflicts, rewriteFail;
    volatile LONG64 why[10];  // pair failure reasons (kWhy*)
    volatile LONG64 psoTargets, psoAll, maps, cbvs;
};
Counters g_c = {};
void Add(volatile LONG64& v, LONG64 n = 1) { InterlockedAdd64(&v, n); }
enum { kWhyPso, kWhyGrid, kWhyCbv, kWhyRoots, kWhyCb, kWhyMaskVal, kWhyUnion, kWhyBlocked, kWhySameCbv, kWhyCount };
const char* const kWhyName[kWhyCount] = {"pso", "grid", "no-cbv", "roots", "cbuffer", "mask-value", "union", "barrier/work", "same-cbv"};

volatile LONG g_frame = 0;
volatile LONG g_armed[kMaxTargets] = {};
volatile LONG g_clean[kMaxTargets] = {};
volatile LONG g_disabled = 0;  // a reuse conflict was seen: never merge again this session
volatile LONG g_hooksOn = 0;
volatile LONG g_devHooked = 0, g_resHooked = 0;

// probe text, flushed by LogLine (nothing is written to the log from the render threads)
SRWLOCK g_txtLock = SRWLOCK_INIT;
std::string g_txt;
volatile LONG g_detailFrames = 0;
volatile LONG g_detailFrame = -1;

void Txt(const char* fmt, ...) {
    char b[700];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    AcquireSRWLockExclusive(&g_txtLock);
    if (g_txt.size() < 60000) {
        g_txt += b;
        g_txt += '\n';
    }
    ReleaseSRWLockExclusive(&g_txtLock);
}

// ---- hex ------------------------------------------------------------------------------------------------------------------
bool Hex16(const char* s, uint8_t out[16]) {
    for (int i = 0; i < 16; ++i) {
        auto nib = [](char c) -> int {
            return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
        };
        const int h = nib(s[2 * i]), l = nib(s[2 * i + 1]);
        if (h < 0 || l < 0) return false;
        out[i] = (uint8_t)(h * 16 + l);
    }
    return true;
}

// ---- PSO -> target ------------------------------------------------------------------------------------------------------
struct PsoEnt { void* volatile key; volatile LONG target; };  // target: -1 = known non-target
PsoEnt* g_pso = nullptr;
SRWLOCK g_psoLock = SRWLOCK_INIT;
volatile LONG g_psoFull = 0;

size_t PsoHash(const void* p) {
    uint64_t v = (uint64_t)(uintptr_t)p;
    v ^= v >> 33;
    v *= 0xff51afd7ed558ccdull;
    v ^= v >> 33;
    return (size_t)v & (kPsoTab - 1);
}
int PsoTarget(const void* pso) {
    if (!g_pso || !pso) return -1;
    for (size_t i = PsoHash(pso), n = 0; n < 64; ++n, i = (i + 1) & (kPsoTab - 1)) {
        void* k = g_pso[i].key;
        if (!k) return -1;
        if (k == pso) return g_pso[i].target;
    }
    return -1;
}
// target >= 0: insert/overwrite; target == -1: overwrite only when present (a freed target PSO's address reused)
void PsoSet(void* pso, int target) {
    if (!g_pso || !pso) return;
    AcquireSRWLockExclusive(&g_psoLock);
    size_t i = PsoHash(pso);
    for (size_t n = 0; n < 64; ++n, i = (i + 1) & (kPsoTab - 1)) {
        if (g_pso[i].key == pso) {
            InterlockedExchange(&g_pso[i].target, target);
            break;
        }
        if (!g_pso[i].key) {
            if (target >= 0) {
                g_pso[i].target = target;
                MemoryBarrier();
                InterlockedExchangePointer(&g_pso[i].key, pso);
            }
            break;
        }
        if (n == 63 && target >= 0 && !InterlockedExchange(&g_psoFull, 1)) LOG("PSO table probe limit reached - a target PSO was not recorded");
    }
    ReleaseSRWLockExclusive(&g_psoLock);
}

// digest match: container digest (bytes 4..19) or the HASH part (DXIL)
int TargetOfBytecode(const void* code, size_t len) {
    if (!code || len < 32) return -1;
    const uint8_t* b = static_cast<const uint8_t*>(code);
    if (memcmp(b, "DXBC", 4) != 0) return -1;
    for (int t = 0; t < kNumTargets; ++t)
        if (!memcmp(b + 4, g_tdig[t], 16)) return t;
    uint32_t parts = 0;
    memcpy(&parts, b + 28, 4);
    if (parts > 64 || 32 + (size_t)parts * 4 > len) return -1;
    for (uint32_t p = 0; p < parts; ++p) {
        uint32_t off = 0;
        memcpy(&off, b + 32 + p * 4, 4);
        if ((size_t)off + 8 + 20 > len) continue;
        if (memcmp(b + off, "HASH", 4) != 0) continue;
        for (int t = 0; t < kNumTargets; ++t)
            if (!memcmp(b + off + 12, g_thash[t], 16)) return t;  // fourcc, size, flags, digest[16]
        return -1;
    }
    return -1;
}

// ---- upload-buffer VA -> CPU pointer (ID3D12Resource::Map / Unmap) -----------------------------------------------------
struct MapEnt { uint64_t end; uint8_t* cpu; void* res; LONG refs; };
SRWLOCK g_mapLock = SRWLOCK_INIT;
std::map<uint64_t, MapEnt>* g_maps = nullptr;
std::unordered_map<void*, uint64_t>* g_mapByRes = nullptr;

void MapInsert(void* res, uint64_t va, uint64_t size, uint8_t* cpu) {
    AcquireSRWLockExclusive(&g_mapLock);
    if (g_maps) {
        auto r = g_mapByRes->find(res);
        if (r != g_mapByRes->end() && r->second == va) {
            auto it = g_maps->find(va);
            if (it != g_maps->end() && it->second.res == res) {
                ++it->second.refs;
                it->second.cpu = cpu;
                ReleaseSRWLockExclusive(&g_mapLock);
                return;
            }
        }
        auto old = g_maps->find(va);  // stale entry of a released resource at the same VA
        if (old != g_maps->end()) {
            g_mapByRes->erase(old->second.res);
            g_maps->erase(old);
        }
        (*g_maps)[va] = MapEnt{va + size, cpu, res, 1};
        (*g_mapByRes)[res] = va;
    }
    ReleaseSRWLockExclusive(&g_mapLock);
}
void MapRemove(void* res) {
    AcquireSRWLockExclusive(&g_mapLock);
    if (g_maps) {
        auto r = g_mapByRes->find(res);
        if (r != g_mapByRes->end()) {
            auto it = g_maps->find(r->second);
            if (it != g_maps->end() && it->second.res == res && --it->second.refs <= 0) {
                g_maps->erase(it);
                g_mapByRes->erase(r);
            }
        }
    }
    ReleaseSRWLockExclusive(&g_mapLock);
}
uint8_t* MapLookup(uint64_t va, uint64_t size) {
    uint8_t* p = nullptr;
    AcquireSRWLockShared(&g_mapLock);
    if (g_maps && !g_maps->empty()) {
        auto it = g_maps->upper_bound(va);
        if (it != g_maps->begin()) {
            --it;
            if (va >= it->first && va + size <= it->second.end) p = it->second.cpu + (va - it->first);
        }
    }
    ReleaseSRWLockShared(&g_mapLock);
    return p;
}
bool SafeRead(void* dst, const void* src, size_t n) {
    __try {
        memcpy(dst, src, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool SafeWrite8(void* dst, const void* src) {
    __try {
        memcpy(dst, src, 8);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- per-thread recording state (the dispatcher thread records the gathers and creates their CBVs) ---------------------
struct Snap {
    void* list = nullptr;
    void* pso = nullptr;
    int target = -1;
    UINT x = 0, y = 0, z = 0;
    uint64_t tab[kRoot] = {}, rcbv[kRoot] = {};
    uint32_t tabValid = 0, rcbvValid = 0;
    uint64_t cbVA = 0;
    bool cbOk = false;
    bool blocked = false;  // a non-UAV barrier or other work since (observation)
    uint8_t cb[kMaxCb] = {};
};
struct Cbv { uint64_t va; uint32_t size; };
struct ThreadState {
    void* list = nullptr;
    void* pso = nullptr;
    int target = -1;
    uint64_t tab[kRoot] = {}, rcbv[kRoot] = {};
    uint32_t tabValid = 0, rcbvValid = 0;
    Cbv cbv[4] = {};
    uint32_t cbvSeq = 0, cbvSeqAtWork = 0;
    uint32_t rcbvSetMask = 0;  // root CBVs set since the last work call
    Snap prev;
    bool prevValid = false;
    // reuse check after a merged (or would-merge) second dispatch
    void* reuseList = nullptr;
    int reuseIdx = -1;
    bool reuseRoot = false;
    uint64_t reuseVal = 0;
    bool reuseMerged = false;
};
thread_local ThreadState* t_st = nullptr;
ThreadState& T() {
    if (!t_st) t_st = new ThreadState();  // one per recording thread, lives as long as the thread (never freed: tiny)
    return *t_st;
}
void ResetList(ThreadState& t, void* list) {
    t.list = list;
    t.pso = nullptr;
    t.target = -1;
    t.tabValid = t.rcbvValid = 0;
    t.prevValid = false;
}

// ---- the hold (one list at a time) ----------------------------------------------------------------------------------------
using DispatchFn = void(STDMETHODCALLTYPE*)(void*, UINT, UINT, UINT);
using SetTableFn = void(STDMETHODCALLTYPE*)(void*, UINT, uint64_t);
using BarrierFn = void(STDMETHODCALLTYPE*)(void*, UINT, const D3D12_RESOURCE_BARRIER*);
void* const* g_orig[2] = {};
void* volatile* g_holdSlot = nullptr;
volatile uint8_t* g_mask = nullptr;

struct Hold {
    bool on = false;
    int set = 0;
    Snap g1;
    uint64_t curTab[kRoot] = {}, curRcbv[kRoot] = {};
    uint32_t chTab = 0, chRcbv = 0;
    bool heldUav = false;
};
SRWLOCK g_holdLock = SRWLOCK_INIT;
Hold g_hold;

void ReplayLocked(const char* why) {
    Hold& h = g_hold;
    if (!h.on) return;
    void* const* o = g_orig[h.set] ? g_orig[h.set] : g_orig[0];
    void* list = h.g1.list;
    auto setTab = reinterpret_cast<SetTableFn>(o[kSlotSetCTable]);
    auto setCbv = reinterpret_cast<SetTableFn>(o[kSlotSetCRootCbv]);
    for (int i = 0; i < kRoot; ++i) {
        if (h.chTab & (1u << i)) setTab(list, (UINT)i, h.g1.tab[i]);
        if (h.chRcbv & (1u << i)) setCbv(list, (UINT)i, h.g1.rcbv[i]);
    }
    reinterpret_cast<DispatchFn>(o[kSlotDispatch])(list, h.g1.x, h.g1.y, h.g1.z);
    for (int i = 0; i < kRoot; ++i) {
        if (h.chTab & (1u << i)) setTab(list, (UINT)i, h.curTab[i]);
        if (h.chRcbv & (1u << i)) setCbv(list, (UINT)i, h.curRcbv[i]);
    }
    if (h.heldUav) {  // the UAV barrier(s) that came after the first dispatch must still separate it from later work
        D3D12_RESOURCE_BARRIER b = {};
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        b.UAV.pResource = nullptr;
        reinterpret_cast<BarrierFn>(o[kSlotRB])(list, 1, &b);
        Add(g_c.replayUav);
    }
    Add(g_c.replayed);
    if (g_cfg.probe && g_detailFrame == g_frame) Txt("GIPROBE f%ld   replay %s (%s)", g_frame, kTargetDefs[h.g1.target].name, why);
    h.on = false;
    h.chTab = h.chRcbv = 0;
    h.heldUav = false;
    if (g_holdSlot) InterlockedExchangePointer(g_holdSlot, nullptr);
}

// ---- pair check -----------------------------------------------------------------------------------------------------------
bool IntFloat(float f, uint32_t& u) {
    if (!(f >= 0.0f) || f >= 16777216.0f || f != std::floor(f)) return false;
    u = (uint32_t)f;
    return true;
}
float Fl(const uint8_t* cb, int off) {
    float f;
    memcpy(&f, cb + off, 4);
    return f;
}

struct PairResult { bool ok = false; int why = -1; uint32_t xo = 0, yo = 0; int idx = -1; bool root = false; };
PairResult CheckPair(const Snap& a, const Snap& b) {
    PairResult r;
    const TargetDef& td = kTargetDefs[b.target];
    if (a.pso != b.pso || a.target != b.target) return r.why = kWhyPso, r;
    if (a.blocked) return r.why = kWhyBlocked, r;
    if (a.x != b.x || a.y != b.y || a.z != b.z) return r.why = kWhyGrid, r;
    if (!a.cbOk || !b.cbOk) return r.why = kWhyCbv, r;
    if (a.cbVA == b.cbVA) return r.why = kWhySameCbv, r;
    if (a.tabValid != b.tabValid || a.rcbvValid != b.rcbvValid) return r.why = kWhyRoots, r;
    int changed = 0;
    for (int i = 0; i < kRoot; ++i) {
        if ((a.tabValid & (1u << i)) && a.tab[i] != b.tab[i]) ++changed, r.idx = i, r.root = false;
        if ((a.rcbvValid & (1u << i)) && a.rcbv[i] != b.rcbv[i]) ++changed, r.idx = i, r.root = true;
    }
    if (changed != 1) return r.why = kWhyRoots, r;
    if (memcmp(a.cb, b.cb, td.maskOff) != 0 || memcmp(a.cb + td.maskOff + 8, b.cb + td.maskOff + 8, td.cbSize - td.maskOff - 8) != 0)
        return r.why = kWhyCb, r;
    uint32_t x1, y1, x2, y2;
    if (!IntFloat(Fl(a.cb, td.maskOff), x1) || !IntFloat(Fl(a.cb, td.maskOff + 4), y1) || !IntFloat(Fl(b.cb, td.maskOff), x2) ||
        !IntFloat(Fl(b.cb, td.maskOff + 4), y2))
        return r.why = kWhyMaskVal, r;
    if (!MaskUnion(x1, y1, x2, y2, r.xo, r.yo)) return r.why = kWhyUnion, r;
    r.ok = true;
    return r;
}

// ---- snapshot of a target dispatch ------------------------------------------------------------------------------------------
void Capture(ThreadState& t, void* list, UINT x, UINT y, UINT z, Snap& s) {
    const TargetDef& td = kTargetDefs[t.target];
    s.list = list;
    s.pso = t.pso;
    s.target = t.target;
    s.x = x, s.y = y, s.z = z;
    memcpy(s.tab, t.tab, sizeof s.tab);
    memcpy(s.rcbv, t.rcbv, sizeof s.rcbv);
    s.tabValid = t.tabValid;
    s.rcbvValid = t.rcbvValid;
    s.blocked = false;
    s.cbOk = false;
    s.cbVA = 0;
    // its $Globals: the newest CBV of the right size created on this thread since the last work call, else a root CBV
    const uint32_t lo = td.cbSize, hi = (td.cbSize + 255u) & ~255u;
    const uint32_t fresh = t.cbvSeq - t.cbvSeqAtWork;
    for (uint32_t k = 0; k < fresh && k < 4; ++k) {
        const Cbv& c = t.cbv[(t.cbvSeq - 1 - k) & 3];
        if (c.size >= lo && c.size <= hi) {
            s.cbVA = c.va;
            break;
        }
    }
    if (!s.cbVA && t.rcbvSetMask)
        for (int i = 0; i < kRoot; ++i)
            if (t.rcbvSetMask & (1u << i)) s.cbVA = t.rcbv[i];
    if (!s.cbVA) {
        Add(g_c.t[t.target].noCbv);
        return;
    }
    if (uint8_t* p = MapLookup(s.cbVA, td.cbSize)) s.cbOk = SafeRead(s.cb, p, td.cbSize);
    if (!s.cbOk) Add(g_c.t[t.target].unreadable);
}

void CountMask(int target, const Snap& s) {
    if (!s.cbOk) return;
    const TargetDef& td = kTargetDefs[target];
    uint32_t x, y;
    if (!IntFloat(Fl(s.cb, td.maskOff), x) || !IntFloat(Fl(s.cb, td.maskOff + 4), y)) x = y = 0xFFFFFFFFu;
    TargetCnt& c = g_c.t[target];
    for (auto& m : c.masks) {
        if (m.n && m.x == x && m.y == y) return Add(m.n), void();
    }
    for (auto& m : c.masks) {
        if (!m.n) {  // racy first-fill is fine for a probe (one recording thread)
            m.x = x, m.y = y;
            Add(m.n);
            return;
        }
    }
}

void DetailLine(const Snap& s, const Snap* prevSame, const PairResult* pr) {
    const TargetDef& td = kTargetDefs[s.target];
    char rows[200] = "-", roots[120] = "-";
    if (prevSame && prevSame->cbOk && s.cbOk) {
        int n = 0, w = 0;
        for (int r = 0; r * 16 < td.cbSize && w < (int)sizeof rows - 8; ++r)
            if (memcmp(prevSame->cb + r * 16, s.cb + r * 16, 16)) w += snprintf(rows + w, sizeof rows - w, "%s%d", n++ ? "," : "", r * 16);
        if (!n) strcpy_s(rows, "none");
    }
    if (prevSame) {
        int n = 0, w = 0;
        for (int i = 0; i < kRoot && w < (int)sizeof roots - 8; ++i) {
            if (((s.tabValid ^ prevSame->tabValid) & (1u << i)) || ((s.tabValid & (1u << i)) && s.tab[i] != prevSame->tab[i]))
                w += snprintf(roots + w, sizeof roots - w, "%st%d", n++ ? "," : "", i);
            if (((s.rcbvValid ^ prevSame->rcbvValid) & (1u << i)) || ((s.rcbvValid & (1u << i)) && s.rcbv[i] != prevSame->rcbv[i]))
                w += snprintf(roots + w, sizeof roots - w, "%sc%d", n++ ? "," : "", i);
        }
        if (!n) strcpy_s(roots, "none");
    }
    char pair[80] = "";
    if (pr) {
        if (pr->ok) snprintf(pair, sizeof pair, " | PAIR ok -> x'=%u y'=%u (root %s%d)", pr->xo, pr->yo, pr->root ? "c" : "t", pr->idx);
        else snprintf(pair, sizeof pair, " | PAIR no: %s", kWhyName[pr->why]);
    }
    const float* m = s.cbOk ? reinterpret_cast<const float*>(s.cb + td.maskOff) : nullptr;
    const float* io = s.cbOk ? reinterpret_cast<const float*>(s.cb + td.infoOff) : nullptr;
    Txt("GIPROBE f%ld %s list %p grid %ux%ux%u (%u groups) mask %s%g %g %g %g cbv %llx%s info_out %g %g | rows != prev: %s | "
        "roots != prev: %s%s",
        g_frame, td.name, s.list, s.x, s.y, s.z, s.x * s.y * s.z, m ? "" : "?", m ? m[0] : 0.f, m ? m[1] : 0.f, m ? m[2] : 0.f,
        m ? m[3] : 0.f, (unsigned long long)s.cbVA, s.cbOk ? "" : (s.cbVA ? " (not mapped)" : " (none)"), io ? io[0] : 0.f,
        io ? io[1] : 0.f, rows, roots, pair);
}

// qualification bookkeeping for one evaluated pair
void NotePair(int target, const PairResult& pr) {
    if (pr.ok) {
        Add(g_c.pairsOk);
        if (kTargetDefs[target].mergeable && g_cfg.merge && !g_disabled) {
            const LONG c = InterlockedIncrement(&g_clean[target]);
            if (c == g_cfg.qualify && g_cfg.merge == 1 && !InterlockedExchange(&g_armed[target], 1))
                Txt("GIMERGE: %s armed after %d clean pairs (frame %ld)", kTargetDefs[target].name, c, g_frame);
        }
    } else {
        Add(g_c.pairsBad);
        if (pr.why >= 0) Add(g_c.why[pr.why]);
        InterlockedExchange(&g_clean[target], 0);
        if (InterlockedExchange(&g_armed[target], 0)) Txt("GIMERGE: %s disarmed (pair failed: %s, frame %ld) - observing again",
                                                          kTargetDefs[target].name, kWhyName[pr.why], g_frame);
    }
}

void ArmReuseCheck(ThreadState& t, void* list, const PairResult& pr, bool merged) {
    t.reuseList = list;
    t.reuseIdx = pr.idx;
    t.reuseRoot = pr.root;
    t.reuseVal = pr.root ? t.rcbv[pr.idx] : t.tab[pr.idx];
    t.reuseMerged = merged;
}

// the first dispatch after a (would-be) merged second one: does it still read the second one's cbuffer?
void CheckReuse(ThreadState& t, void* list) {
    if (t.reuseList != list || t.reuseIdx < 0) return;
    const int i = t.reuseIdx;
    const bool same = t.reuseRoot ? ((t.rcbvValid & (1u << i)) && t.rcbv[i] == t.reuseVal)
                                  : ((t.tabValid & (1u << i)) && t.tab[i] == t.reuseVal);
    t.reuseList = nullptr;
    t.reuseIdx = -1;
    if (!same) return;
    Add(g_c.conflicts);
    if (!InterlockedExchange(&g_disabled, 1)) {
        for (auto& a : g_armed) InterlockedExchange(&a, 0);
        Txt("GIMERGE: the dispatch after a %s second gather still binds its CBV table (frame %ld) - merge DISABLED for this "
            "session", t.reuseMerged ? "merged" : "would-merge", g_frame);
    }
}

// a target (or any) dispatch, with no hold on this list (or after the hold was resolved)
bool OnDispatch(ThreadState& t, void* list, int set, UINT x, UINT y, UINT z) {
    CheckReuse(t, list);
    bool skip = false;
    if (t.target >= 0) {
        const int tg = t.target;
        static thread_local Snap* cur = nullptr;  // large: kept off the stack
        if (!cur) cur = new Snap();
        Capture(t, list, x, y, z, *cur);
        Add(g_c.t[tg].dispatches);
        Add(g_c.t[tg].groups, (LONG64)x * y * z);
        CountMask(tg, *cur);
        PairResult pr;
        const bool pair = t.prevValid && t.prev.list == list && t.prev.target == tg;
        if (pair) {
            pr = CheckPair(t.prev, *cur);
            NotePair(tg, pr);
            if (pr.ok) ArmReuseCheck(t, list, pr, false);
        }
        if (g_cfg.probe && g_detailFrame == g_frame) DetailLine(*cur, t.prevValid && t.prev.target == tg ? &t.prev : nullptr, pair ? &pr : nullptr);
        // hold this one as a possible first dispatch of a pair (not when it just completed a pair)
        if (g_cfg.merge == 1 && !g_disabled && g_armed[tg] && kTargetDefs[tg].mergeable && cur->cbOk && !(pair && pr.ok) && g_holdSlot &&
            g_orig[set ? 1 : 0]) {
            AcquireSRWLockExclusive(&g_holdLock);
            if (!g_hold.on) {
                g_hold.on = true;
                g_hold.set = set;
                g_hold.g1 = *cur;
                memcpy(g_hold.curTab, t.tab, sizeof g_hold.curTab);
                memcpy(g_hold.curRcbv, t.rcbv, sizeof g_hold.curRcbv);
                g_hold.chTab = g_hold.chRcbv = 0;
                g_hold.heldUav = false;
                InterlockedExchangePointer(g_holdSlot, list);
                skip = true;
                Add(g_c.held);
            }
            ReleaseSRWLockExclusive(&g_holdLock);
        }
        t.prev = *cur;
        t.prevValid = true;
    } else {
        t.prevValid = false;
    }
    t.cbvSeqAtWork = t.cbvSeq;
    t.rcbvSetMask = 0;
    return skip;
}

// second dispatch while holding: merge or replay. Returns true when merged (the second dispatch then proceeds).
bool TryMergeLocked(ThreadState& t, void* list, UINT x, UINT y, UINT z) {
    Hold& h = g_hold;
    if (t.target != h.g1.target || t.pso != h.g1.pso) return false;
    static thread_local Snap* cur = nullptr;
    if (!cur) cur = new Snap();
    Capture(t, list, x, y, z, *cur);
    // the snapshot's root state is the thread's; the hold tracked the same calls (one recording thread)
    const PairResult pr = CheckPair(h.g1, *cur);
    const int tg = h.g1.target;
    const TargetDef& td = kTargetDefs[tg];
    NotePair(tg, pr);
    if (g_cfg.probe && g_detailFrame == g_frame) DetailLine(*cur, &h.g1, &pr);
    t.prevValid = false;  // this pair is decided here; the caller replays the first dispatch, then runs this one normally
    if (!pr.ok) return false;
    // rewrite (x', y') into the second dispatch's own upload-ring cbuffer: the GPU has not read it yet (list not executed)
    uint8_t* p = MapLookup(cur->cbVA, td.cbSize);
    float nv[2] = {(float)pr.xo, (float)pr.yo};
    uint8_t back[8] = {};
    if (!p || !SafeWrite8(p + td.maskOff, nv) || !SafeRead(back, p + td.maskOff, 8) || memcmp(back, nv, 8)) {
        Add(g_c.rewriteFail);
        if (p) SafeWrite8(p + td.maskOff, cur->cb + td.maskOff);  // put the original back
        InterlockedExchange(&g_clean[tg], 0);
        InterlockedExchange(&g_armed[tg], 0);
        return false;
    }
    Add(g_c.merged);
    Add(g_c.mergedGroups, (LONG64)h.g1.x * h.g1.y * h.g1.z);
    if (g_cfg.probe && g_detailFrame == g_frame) Txt("GIPROBE f%ld   merged %s: first dispatch dropped, mask -> %u %u", g_frame, td.name, pr.xo, pr.yo);
    h.on = false;
    h.chTab = h.chRcbv = 0;
    h.heldUav = false;
    InterlockedExchangePointer(g_holdSlot, nullptr);
    // the merged dispatch completes this pair: it is not a new first dispatch
    ArmReuseCheck(t, list, pr, true);
    t.prev = *cur;
    t.prevValid = true;
    Add(g_c.t[tg].dispatches);
    Add(g_c.t[tg].groups, (LONG64)x * y * z);
    CountMask(tg, *cur);
    t.cbvSeqAtWork = t.cbvSeq;
    t.rcbvSetMask = 0;
    return true;
}

void TrackRoot(ThreadState& t, int slot, uintptr_t a1, uintptr_t a2) {
    const UINT i = (UINT)a1;
    if (i >= (UINT)kRoot) return;
    if (slot == kSlotSetCTable) {
        t.tab[i] = (uint64_t)a2;
        t.tabValid |= 1u << i;
    } else {
        t.rcbv[i] = (uint64_t)a2;
        t.rcbvValid |= 1u << i;
        t.rcbvSetMask |= 1u << i;
    }
}

// ---- device / resource hooks --------------------------------------------------------------------------------------------
using CreateDeviceFn = HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**);
using CreateGfxFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_GRAPHICS_PIPELINE_STATE_DESC*, REFIID, void**);
using CreateCsFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_COMPUTE_PIPELINE_STATE_DESC*, REFIID, void**);
using CreateCbvFn = void(STDMETHODCALLTYPE*)(ID3D12Device*, const D3D12_CONSTANT_BUFFER_VIEW_DESC*, D3D12_CPU_DESCRIPTOR_HANDLE);
using MapFn = HRESULT(STDMETHODCALLTYPE*)(ID3D12Resource*, UINT, const D3D12_RANGE*, void**);
using UnmapFn = void(STDMETHODCALLTYPE*)(ID3D12Resource*, UINT, const D3D12_RANGE*);

struct DevSet { void** vt; CreateGfxFn gfx; CreateCsFn cs; CreateCbvFn cbv; };
DevSet g_dev[2] = {};
volatile LONG g_devN = 0;
struct ResSet { void** vt; MapFn map; UnmapFn unmap; };
ResSet g_res[2] = {};
volatile LONG g_resN = 0;

template <int S>
HRESULT STDMETHODCALLTYPE HkGfx(ID3D12Device* self, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    const HRESULT hr = g_dev[S].gfx(self, d, riid, pp);
    if (SUCCEEDED(hr) && pp && *pp) PsoSet(*pp, -1);
    return hr;
}
template <int S>
HRESULT STDMETHODCALLTYPE HkCs(ID3D12Device* self, const D3D12_COMPUTE_PIPELINE_STATE_DESC* d, REFIID riid, void** pp) {
    const HRESULT hr = g_dev[S].cs(self, d, riid, pp);
    if (SUCCEEDED(hr) && pp && *pp) {
        int t = -1;
        __try {
            if (d) t = TargetOfBytecode(d->CS.pShaderBytecode, d->CS.BytecodeLength);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            t = -1;
        }
        PsoSet(*pp, t);
        Add(g_c.psoAll);
        if (t >= 0) Add(g_c.psoTargets);
    }
    return hr;
}
template <int S>
void STDMETHODCALLTYPE HkCbv(ID3D12Device* self, const D3D12_CONSTANT_BUFFER_VIEW_DESC* d, D3D12_CPU_DESCRIPTOR_HANDLE h) {
    if (d) {
        ThreadState& t = T();
        t.cbv[t.cbvSeq & 3] = Cbv{d->BufferLocation, d->SizeInBytes};
        ++t.cbvSeq;
    }
    g_dev[S].cbv(self, d, h);
}
void NoteMap(ID3D12Resource* r, void* cpu) {
    __try {
        const D3D12_RESOURCE_DESC desc = r->GetDesc();
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) return;
        D3D12_HEAP_PROPERTIES hp = {};
        D3D12_HEAP_FLAGS hf = D3D12_HEAP_FLAG_NONE;
        if (FAILED(r->GetHeapProperties(&hp, &hf))) return;
        if (hp.Type != D3D12_HEAP_TYPE_UPLOAD && hp.Type != D3D12_HEAP_TYPE_CUSTOM) return;
        const uint64_t va = r->GetGPUVirtualAddress();
        if (!va) return;
        MapInsert(r, va, desc.Width, static_cast<uint8_t*>(cpu));
        Add(g_c.maps);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}
template <int S>
HRESULT STDMETHODCALLTYPE HkMap(ID3D12Resource* self, UINT sub, const D3D12_RANGE* rr, void** pp) {
    const HRESULT hr = g_res[S].map(self, sub, rr, pp);
    if (SUCCEEDED(hr) && sub == 0 && pp && *pp) NoteMap(self, *pp);
    return hr;
}
template <int S>
void STDMETHODCALLTYPE HkUnmap(ID3D12Resource* self, UINT sub, const D3D12_RANGE* wr) {
    if (sub == 0) MapRemove(self);
    g_res[S].unmap(self, sub, wr);
}

bool Patch(void** slot, void* hook, void** orig) {
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = *slot;
    MemoryBarrier();
    InterlockedExchangePointer(slot, hook);
    DWORD t;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return true;
}

void HookResourceVt(void** vt, const char* what) {
    for (LONG i = 0; i < g_resN; ++i)
        if (g_res[i].vt == vt) return;
    if (g_resN >= 2) return;
    const LONG s = g_resN;
    g_res[s].vt = vt;
    bool ok = s == 0 ? Patch(&vt[kResMap], (void*)&HkMap<0>, (void**)&g_res[0].map) && Patch(&vt[kResUnmap], (void*)&HkUnmap<0>, (void**)&g_res[0].unmap)
                     : Patch(&vt[kResMap], (void*)&HkMap<1>, (void**)&g_res[1].map) && Patch(&vt[kResUnmap], (void*)&HkUnmap<1>, (void**)&g_res[1].unmap);
    if (ok) {
        g_resN = s + 1;
        InterlockedExchange(&g_resHooked, 1);
        LOG("ID3D12Resource::Map/Unmap hooked (%s, vtable %p)", what, (void*)vt);
    }
}

void HookDevice(IUnknown* obj) {
    ID3D12Device* dev = nullptr;
    if (FAILED(obj->QueryInterface(kIID_Device, reinterpret_cast<void**>(&dev))) || !dev) return;
    void** vt = *reinterpret_cast<void***>(dev);
    bool known = false;
    for (LONG i = 0; i < g_devN; ++i) known |= g_dev[i].vt == vt;
    if (!known && g_devN < 2) {
        const LONG s = g_devN;
        g_dev[s].vt = vt;
        bool ok;
        if (s == 0)
            ok = Patch(&vt[kDevCreateGfx], (void*)&HkGfx<0>, (void**)&g_dev[0].gfx) && Patch(&vt[kDevCreateCs], (void*)&HkCs<0>, (void**)&g_dev[0].cs) &&
                 Patch(&vt[kDevCreateCbv], (void*)&HkCbv<0>, (void**)&g_dev[0].cbv);
        else
            ok = Patch(&vt[kDevCreateGfx], (void*)&HkGfx<1>, (void**)&g_dev[1].gfx) && Patch(&vt[kDevCreateCs], (void*)&HkCs<1>, (void**)&g_dev[1].cs) &&
                 Patch(&vt[kDevCreateCbv], (void*)&HkCbv<1>, (void**)&g_dev[1].cbv);
        if (ok) {
            g_devN = s + 1;
            InterlockedExchange(&g_devHooked, 1);
            LOG("device vtable %p hooked: CreateGraphicsPipelineState, CreateComputePipelineState, CreateConstantBufferView", (void*)vt);
        }
    }
    // the resource vtable(s): a committed and a placed upload buffer, created and released at once (first device only)
    static volatile LONG s_resProbed = 0;
    if (!InterlockedExchange(&s_resProbed, 1)) {
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = 65536;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ID3D12Resource* r = nullptr;
        if (SUCCEEDED(dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                   __uuidof(ID3D12Resource), reinterpret_cast<void**>(&r))) && r) {
            HookResourceVt(*reinterpret_cast<void***>(r), "committed upload buffer");
            r->Release();
        } else {
            LOG("could not create a probe upload buffer - cbuffer reads unavailable (probe counts only, no merge)");
        }
        D3D12_HEAP_DESC hd = {};
        hd.SizeInBytes = 65536;
        hd.Properties = hp;
        hd.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
        ID3D12Heap* heap = nullptr;
        if (SUCCEEDED(dev->CreateHeap(&hd, __uuidof(ID3D12Heap), reinterpret_cast<void**>(&heap))) && heap) {
            ID3D12Resource* pr = nullptr;
            if (SUCCEEDED(dev->CreatePlacedResource(heap, 0, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
                                                    reinterpret_cast<void**>(&pr))) && pr) {
                HookResourceVt(*reinterpret_cast<void***>(pr), "placed upload buffer");
                pr->Release();
            }
            heap->Release();
        }
    }
    dev->Release();
}
void HookDeviceSafe(IUnknown* obj) {
    __try {
        HookDevice(obj);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        LOG("exception 0x%08lX while hooking the device - probe/merge inactive", GetExceptionCode());
    }
}

CreateDeviceFn g_origCreate[2] = {};
template <int I>
HRESULT WINAPI HkCreateDevice(IUnknown* adapter, D3D_FEATURE_LEVEL fl, REFIID riid, void** pp) {
    const HRESULT hr = g_origCreate[I](adapter, fl, riid, pp);
    if (SUCCEEDED(hr) && pp && *pp) HookDeviceSafe(static_cast<IUnknown*>(*pp));
    return hr;
}

bool InSelf(const void* p) {
    HMODULE self = nullptr, m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&InSelf), &self))
        return false;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(p), &m))
        return false;
    return m == self;
}

int ArmImportHooks() {
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    int n = 0;
    for (auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); imp->Name && n < 2; ++imp) {
        const char* dll = reinterpret_cast<const char*>(base + imp->Name);
        const bool sl = !_stricmp(dll, "sl.interposer.dll"), d12 = !_stricmp(dll, "d3d12.dll");
        if (!sl && !d12) continue;
        auto* names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto* slots = reinterpret_cast<void**>(base + imp->FirstThunk);
        HMODULE dm = GetModuleHandleA(dll);
        void* bound = dm ? (void*)GetProcAddress(dm, "D3D12CreateDevice") : nullptr;
        for (int i = 0; names[i].u1.AddressOfData; ++i) {
            if (IMAGE_SNAP_BY_ORDINAL64(names[i].u1.Ordinal)) {
                // ordinal import (d3d12.lib links D3D12CreateDevice as ordinal 101): the slot holds the bound export, or
                // psoswap's hook (inside this module) when psoswap armed first on the same slot
                if (!slots[i] || (slots[i] != bound && !InSelf(slots[i]))) continue;
            } else {
                auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names[i].u1.AddressOfData);
                if (strcmp(reinterpret_cast<const char*>(ibn->Name), "D3D12CreateDevice") != 0) continue;
            }
            if (!slots[i]) break;
            const bool ok = n == 0 ? Patch(&slots[i], (void*)&HkCreateDevice<0>, (void**)&g_origCreate[0])
                                   : Patch(&slots[i], (void*)&HkCreateDevice<1>, (void**)&g_origCreate[1]);
            if (ok) {
                LOG("IAT hook on %s!D3D12CreateDevice armed (slot RVA 0x%llx)", sl ? "sl.interposer.dll" : "d3d12.dll",
                    (unsigned long long)(reinterpret_cast<uint8_t*>(&slots[i]) - base));
                ++n;
            }
            break;
        }
    }
    return n;
}
int ArmImportHooksSafe() {
    __try {
        return ArmImportHooks();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

int IniInt(const wchar_t* sec, const wchar_t* key, int def, int lo, int hi, const wchar_t* ini) {
    const int v = (int)GetPrivateProfileIntW(sec, key, def, ini);
    return v < lo ? lo : v > hi ? hi : v;
}

void InitTables() {
    for (int t = 0; t < kNumTargets; ++t) {
        Hex16(kTargetDefs[t].dig, g_tdig[t]);
        Hex16(kTargetDefs[t].hash, g_thash[t]);
    }
    if (!g_pso) g_pso = static_cast<PsoEnt*>(VirtualAlloc(nullptr, sizeof(PsoEnt) * kPsoTab, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!g_maps) {
        g_maps = new std::map<uint64_t, MapEnt>();
        g_mapByRes = new std::unordered_map<void*, uint64_t>();
    }
}

LONG64 Rd(volatile LONG64& v) { return InterlockedCompareExchange64(&v, 0, 0); }

}  // namespace

// ---- public ---------------------------------------------------------------------------------------------------------------
bool MaskUnion(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, uint32_t& xo, uint32_t& yo) {
    // S(x, y) = { s : (s & y) == x } is a cube: the bits of y are fixed to x, the others free; empty when x has a bit
    // outside y. The union of two cubes is a cube iff one contains the other, or both fix the same bits and differ in
    // exactly one of them (then that bit becomes free).
    const bool e1 = (x1 & ~y1) != 0, e2 = (x2 & ~y2) != 0;
    if (e1 && e2) return false;
    if (e1) return xo = x2, yo = y2, true;
    if (e2) return xo = x1, yo = y1, true;
    if ((y2 & ~y1) == 0 && (x1 & y2) == x2) return xo = x2, yo = y2, true;  // S1 within S2
    if ((y1 & ~y2) == 0 && (x2 & y1) == x1) return xo = x1, yo = y1, true;  // S2 within S1
    if (y1 == y2) {
        const uint32_t d = x1 ^ x2;
        if (d && !(d & (d - 1))) {
            yo = y1 & ~d;
            xo = x1 & yo;
            return true;
        }
    }
    return false;
}

void Start(const StartInfo& info) {
    g_log = info.log;
    g_wine = info.wine ? info.wine : "";
    const wchar_t* ini = info.iniPath ? info.iniPath : L"";
    g_cfg.probe = IniInt(L"Debug", L"GIProbe", 0, 0, 1, ini);
    g_cfg.probeEvery = IniInt(L"Debug", L"GIProbeEvery", 300, 10, 100000, ini);
    g_cfg.probeFrames = IniInt(L"Debug", L"GIProbeFrames", 30, 0, 10000, ini);
    g_cfg.merge = IniInt(L"Patches", L"GIMerge", 0, 0, 2, ini);
    g_cfg.qualify = IniInt(L"Patches", L"GIMergeQualify", 60, 1, 100000, ini);
    if (!g_cfg.probe && !g_cfg.merge) return;
    InitTables();
    if (!g_pso) {
        LOG("out of memory - off");
        g_cfg.probe = g_cfg.merge = 0;
        return;
    }
    const int n = ArmImportHooksSafe();
    LOG("GIProbe=%d (every %d frames, %d detail frames) GIMerge=%d (qualify %d pairs)%s%s: %d D3D12CreateDevice import(s) hooked",
        g_cfg.probe, g_cfg.probeEvery, g_cfg.probeFrames, g_cfg.merge, g_cfg.qualify, g_wine.empty() ? "" : ", wine ", g_wine.c_str(), n);
    if (!n) LOG("no D3D12CreateDevice import - the game's device will not be hooked: no PSO/cbuffer data (counts stay 0)");
}

bool WantsHooks() { return g_cfg.probe || g_cfg.merge; }

void EnableHooks(volatile uint8_t* mask, int slots, void* const* orig0, void* const* orig1, void* volatile* hold) {
    if (!WantsHooks() || !mask || slots <= kSlotExecIndirect) return;
    g_orig[0] = orig0;
    g_orig[1] = orig1 && orig1[kSlotDispatch] ? orig1 : nullptr;
    g_holdSlot = hold;
    const int s[] = {kSlotClose, kSlotReset, kSlotDraw, kSlotDrawIdx, kSlotDispatch, kSlotSetPso, kSlotSetCRootSig, kSlotSetCTable,
                     kSlotSetCRootCbv, kSlotExecIndirect};
    for (int v : s) mask[v] = 1;
    for (int v = kSlotCopyFirst; v <= kSlotCopyLast; ++v) mask[v] = 1;
    for (int v = kSlotClearFirst; v <= kSlotClearLast; ++v) mask[v] = 1;
    g_mask = mask;
    InterlockedExchange(&g_hooksOn, 1);
    LOG("command-list hooks on (gpuopt thunks): Dispatch, SetPipelineState, compute root tables/CBVs, draws, copies, clears%s",
        g_cfg.merge == 1 ? "; merge after qualification" : g_cfg.merge == 2 ? "; merge observe-only" : "");
}

bool OnListCall(void* list, int set, int slot, uintptr_t a1, uintptr_t a2, uintptr_t a3) {
    if (!g_hooksOn) return false;
    ThreadState& t = T();
    if (t.list != list && slot != kSlotClose) ResetList(t, list);
    // ---- a call on the held list ----
    if (g_holdSlot && *g_holdSlot == list) {
        AcquireSRWLockExclusive(&g_holdLock);
        if (g_hold.on && g_hold.g1.list == list) {
            Hold& h = g_hold;
            bool resolved = false;
            if ((slot == kSlotSetCTable || slot == kSlotSetCRootCbv) && (UINT)a1 < (UINT)kRoot) {
                const UINT i = (UINT)a1;
                const bool tab = slot == kSlotSetCTable;
                if ((tab ? h.g1.tabValid : h.g1.rcbvValid) & (1u << i)) {
                    if (tab) h.curTab[i] = (uint64_t)a2, h.chTab |= 1u << i;
                    else h.curRcbv[i] = (uint64_t)a2, h.chRcbv |= 1u << i;
                    resolved = true;  // keep holding: a rebind we can undo for a replay
                }
            } else if (slot == kSlotDispatch && t.list == list) {
                if (TryMergeLocked(t, list, (UINT)a1, (UINT)a2, (UINT)a3)) {
                    ReleaseSRWLockExclusive(&g_holdLock);
                    return false;  // the merged second dispatch runs
                }
            }
            if (!resolved) ReplayLocked(slot == kSlotDispatch ? "pair mismatch" : "other call");
        }
        ReleaseSRWLockExclusive(&g_holdLock);
    }
    // ---- normal tracking ----
    switch (slot) {
        case kSlotReset:
            ResetList(t, list);
            t.pso = reinterpret_cast<void*>(a2);
            t.target = PsoTarget(t.pso);
            t.reuseList = nullptr;
            break;
        case kSlotClose:
            if (t.list == list) t.prevValid = false;
            if (t.reuseList == list) t.reuseList = nullptr;
            break;
        case kSlotSetPso:
            t.pso = reinterpret_cast<void*>(a1);
            t.target = PsoTarget(t.pso);
            break;
        case kSlotSetCRootSig:
            t.tabValid = t.rcbvValid = 0;
            break;
        case kSlotSetCTable:
        case kSlotSetCRootCbv:
            TrackRoot(t, slot, a1, a2);
            break;
        case kSlotDispatch:
            return OnDispatch(t, list, set, (UINT)a1, (UINT)a2, (UINT)a3);
        case kSlotExecIndirect:
            CheckReuse(t, list);
            t.prevValid = false;
            t.cbvSeqAtWork = t.cbvSeq;
            t.rcbvSetMask = 0;
            break;
        default:  // draws, copies, clears: work between two dispatches breaks a pair
            if ((slot >= kSlotDraw && slot <= kSlotDrawIdx) || (slot >= kSlotCopyFirst && slot <= kSlotCopyLast) ||
                (slot >= kSlotClearFirst && slot <= kSlotClearLast)) {
                if (t.prevValid && t.prev.list == list) t.prev.blocked = true;
                if (slot <= kSlotDrawIdx) t.cbvSeqAtWork = t.cbvSeq;
            }
            break;
    }
    return false;
}

void OnBarrier(void* list, int, unsigned n, const void* barriers) {
    if (!g_hooksOn) return;
    const auto* b = static_cast<const D3D12_RESOURCE_BARRIER*>(barriers);
    bool allUav = b != nullptr;
    for (unsigned k = 0; k < n && b; ++k) allUav &= b[k].Type == D3D12_RESOURCE_BARRIER_TYPE_UAV;
    if (g_holdSlot && *g_holdSlot == list) {
        AcquireSRWLockExclusive(&g_holdLock);
        if (g_hold.on && g_hold.g1.list == list) {
            if (allUav) g_hold.heldUav = true;
            else ReplayLocked("transition barrier");
        }
        ReleaseSRWLockExclusive(&g_holdLock);
    }
    if (!allUav && t_st && t_st->prevValid && t_st->prev.list == list) t_st->prev.blocked = true;
}

void OnFrame() {
    const LONG f = InterlockedIncrement(&g_frame);
    if (g_cfg.probe && g_detailFrames < g_cfg.probeFrames && f % g_cfg.probeEvery == 0) {
        InterlockedIncrement(&g_detailFrames);
        InterlockedExchange(&g_detailFrame, f);
    }
}

const char* StatusLine(char* buf, size_t n) {
    if (!n) return buf;
    buf[0] = 0;
    if (!WantsHooks()) return buf;
    int w = snprintf(buf, n, "%s, merged %lld, replays %lld", g_disabled ? "DISABLED (reuse)" : g_armed[0] || g_armed[1] ? "armed" : "observing",
                     Rd(g_c.merged), Rd(g_c.replayed));
    (void)w;
    return buf;
}

void LogLine() {
    if (!WantsHooks() || !g_log) return;
    static LONG s_lastFrame = 0;
    static LONG64 s_last[kMaxTargets][2] = {}, s_merged = 0, s_mgroups = 0, s_replayed = 0, s_ok = 0, s_bad = 0;
    const LONG fr = g_frame;
    const LONG df = fr - s_lastFrame;
    s_lastFrame = fr;
    const double per = df > 0 ? 1.0 / df : 1.0 / 30.0;  // per frame, or per second when OnFrame never runs
    const char* unit = df > 0 ? "/f" : "/s";
    char line[1000];
    int w = snprintf(line, sizeof line, "frames %ld | device %s, Map %s, PSOs %lld target / %lld cs, maps %lld |", df,
                     g_devHooked ? "hooked" : "NOT hooked", g_resHooked ? "hooked" : "NOT hooked", Rd(g_c.psoTargets), Rd(g_c.psoAll), Rd(g_c.maps));
    for (int t = 0; t < kNumTargets && w < (int)sizeof line - 160; ++t) {
        const LONG64 d = Rd(g_c.t[t].dispatches), g = Rd(g_c.t[t].groups);
        if (!d) continue;
        w += snprintf(line + w, sizeof line - w, " %s %.2f%s %.0f groups%s masks", kTargetDefs[t].name, (d - s_last[t][0]) * per, unit,
                      (g - s_last[t][1]) * per, unit);
        for (auto& m : g_c.t[t].masks)
            if (m.n) w += snprintf(line + w, sizeof line - w, " (%u,%u)x%lld", m.x, m.y, Rd(m.n));
        if (g_c.t[t].noCbv || g_c.t[t].unreadable)
            w += snprintf(line + w, sizeof line - w, " nocbv %lld unread %lld", Rd(g_c.t[t].noCbv), Rd(g_c.t[t].unreadable));
        w += snprintf(line + w, sizeof line - w, " |");
        s_last[t][0] = d, s_last[t][1] = g;
    }
    g_log("stats/s giprobe: %s", line);
    const LONG64 ok = Rd(g_c.pairsOk), bad = Rd(g_c.pairsBad), mg = Rd(g_c.merged), mgg = Rd(g_c.mergedGroups), rp = Rd(g_c.replayed);
    w = snprintf(line, sizeof line, "pairs ok %lld bad %lld (window +%lld/+%lld) why:", ok, bad, ok - s_ok, bad - s_bad);
    for (int k = 0; k < kWhyCount; ++k)
        if (g_c.why[k]) w += snprintf(line + w, sizeof line - w, " %s %lld", kWhyName[k], Rd(g_c.why[k]));
    w += snprintf(line + w, sizeof line - w, " | merge %s: armed", g_cfg.merge == 1 ? "on" : g_cfg.merge == 2 ? "observe" : "off");
    for (int t = 0; t < kNumTargets; ++t)
        if (kTargetDefs[t].mergeable) w += snprintf(line + w, sizeof line - w, " %s=%ld(clean %ld)", kTargetDefs[t].name, g_armed[t], g_clean[t]);
    snprintf(line + w, sizeof line - w, ", merged %.2f%s (%.0f groups skipped%s), replays %.2f%s, uav-replays %lld, held %lld, conflicts %lld, "
             "rewrite-fail %lld%s",
             (mg - s_merged) * per, unit, (mgg - s_mgroups) * per, unit, (rp - s_replayed) * per, unit, Rd(g_c.replayUav), Rd(g_c.held),
             Rd(g_c.conflicts), Rd(g_c.rewriteFail), g_disabled ? " DISABLED" : "");
    g_log("stats/s gimerge: %s", line);
    s_ok = ok, s_bad = bad, s_merged = mg, s_mgroups = mgg, s_replayed = rp;
    // buffered probe lines
    std::string txt;
    AcquireSRWLockExclusive(&g_txtLock);
    txt.swap(g_txt);
    ReleaseSRWLockExclusive(&g_txtLock);
    size_t p = 0;
    while (p < txt.size()) {
        size_t e = txt.find('\n', p);
        if (e == std::string::npos) e = txt.size();
        g_log("%.*s", (int)(e - p), txt.c_str() + p);
        p = e + 1;
    }
}

// ---- test seam ------------------------------------------------------------------------------------------------------------
namespace {
void TRegisterPso(void* pso, const uint8_t dig[16]) {
    InitTables();
    uint8_t blob[64] = {'D', 'X', 'B', 'C'};
    memcpy(blob + 4, dig, 16);
    PsoSet(pso, TargetOfBytecode(blob, sizeof blob));
}
void TCreateCbv(uint64_t va, uint32_t size) {
    ThreadState& t = T();
    t.cbv[t.cbvSeq & 3] = Cbv{va, size};
    ++t.cbvSeq;
}
void TMapRange(uint64_t va, void* cpu, uint64_t size) {
    InitTables();
    MapInsert(reinterpret_cast<void*>(va), va, size, static_cast<uint8_t*>(cpu));
}
void TSetMode(int probe, int merge, int qualify) {
    InitTables();
    g_cfg.probe = probe, g_cfg.merge = merge, g_cfg.qualify = qualify;
    g_disabled = 0;
    for (auto& a : g_armed) a = 0;
    for (auto& c : g_clean) c = 0;
}
uint64_t TCounter(int w) {
    switch (w) {
        case 0: return (uint64_t)Rd(g_c.merged);
        case 1: return (uint64_t)Rd(g_c.replayed);
        case 2: return (uint64_t)Rd(g_c.held);
        case 3: return (uint64_t)Rd(g_c.pairsOk);
        case 4: return (uint64_t)Rd(g_c.pairsBad);
        case 5: return (uint64_t)Rd(g_c.conflicts);
        case 6: return (uint64_t)(g_armed[0] + 2 * g_armed[1]);
        case 7: return (uint64_t)g_disabled;
    }
    return 0;
}
const TestApi g_test = {&TRegisterPso, &TCreateCbv, &TMapRange, &TSetMode, &TCounter};
}  // namespace
const TestApi& Test() { return g_test; }

}  // namespace gimerge
