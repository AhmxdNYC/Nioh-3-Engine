// replayprof.cpp - RPF replay-thread profiler. See replayprof.h and notes\renderer\replay-profile.md. MEASUREMENT ONLY.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <intrin.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "replayprof.h"
#include "replayprof_detail.h"
#include "replayprof_names.h"

// ---- asm side (replayprof.asm) ------------------------------------------------------------------------------------------
extern "C" {
extern volatile uint32_t rp_tid;
extern volatile uint32_t rp_curOp;
extern volatile uint8_t rp_busy;
extern volatile uint8_t rp_on;
extern void* rp_orig[];
extern uint64_t rp_id[];
extern uint64_t rp_mat[];
extern uint8_t rp_grp[];
extern uint8_t rp_pk[];
extern uint64_t rp_psoRed[];
extern void* rp_thunks[];
extern void* rp_tokthunks[];
void RpTok(void* c, uint8_t** cur, uint32_t op);
}

namespace replayprof {
namespace detail {

const char* const kGroupNames[kGroups] = {
    "none",       "IA/RS/OM state", "root sig/args", "PSO/heaps",  "barrier",     "draw/dispatch", "copy/clear/resolve",
    "query/marker", "list close/reset", "descr copy", "view create", "device other", "ExecuteCmdLists", "queue sync/tiles",
    "fence",      "Map/Unmap",      "resource other", "COM",       "wait (WFSO)", "sleep/yield",   "list other",
    "-",          "-",              "-"};

int GroupOf(int kind, const char* n) {
    auto is = [&](const char* s) { return strcmp(n, s) == 0; };
    auto pre = [&](const char* s) { return strncmp(n, s, strlen(s)) == 0; };
    if (is("QueryInterface") || is("AddRef") || is("Release") || is("GetPrivateData") || is("SetPrivateData") ||
        is("SetPrivateDataInterface") || is("SetName") || is("GetDevice"))
        return kGCom;
    switch (kind) {
        case kKindList:
            if (is("ResourceBarrier") || is("Barrier")) return kGBarrier;
            if (pre("Draw") || pre("Dispatch") || is("ExecuteIndirect")) return kGDraw;
            if (is("ResolveQueryData") || is("BeginQuery") || is("EndQuery") || is("SetPredication") || is("SetMarker") ||
                is("BeginEvent") || is("EndEvent"))
                return kGQuery;
            if (is("Close") || is("Reset") || is("ClearState")) return kGLife;
            if (pre("Copy") || pre("Clear") || pre("Resolve") || is("DiscardResource") || is("WriteBufferImmediate") ||
                pre("AtomicCopy"))
                return kGCopy;
            if (pre("SetPipelineState") || is("SetDescriptorHeaps") || is("SetProgram")) return kGPso;
            if (strstr(n, "RootSignature") || pre("SetGraphicsRoot") || pre("SetComputeRoot")) return kGRoot;
            if (pre("IA") || pre("RS") || pre("OM") || is("SetSamplePositions") || is("SetViewInstanceMask")) return kGState;
            return kGListOther;
        case kKindDevice:
            if (is("CopyDescriptors") || is("CopyDescriptorsSimple")) return kGDescCopy;
            if ((pre("Create") && strstr(n, "View")) || pre("CreateSampler")) return kGViewCreate;
            return kGDevOther;
        case kKindQueue:
            return is("ExecuteCommandLists") ? kGEcl : kGQueueSync;
        case kKindFence:
            return kGFence;
        case kKindResource:
            return (is("Map") || is("Unmap")) ? kGMap : kGResOther;
    }
    return kGNone;
}

const char* const* KindNames(int kind, int& n) {
    switch (kind) {
        case kKindList: n = kListNamesN; return kListNames;
        case kKindDevice: n = kDeviceNamesN; return kDeviceNames;
        case kKindQueue: n = kQueueNamesN; return kQueueNames;
        case kKindFence: n = kFenceNamesN; return kFenceNames;
        case kKindResource: n = kResourceNamesN; return kResourceNames;
    }
    n = 0;
    return nullptr;
}

}  // namespace detail

namespace {
using namespace detail;

// ---- v2.02 layout (notes\renderer\backend.md; decompile 1A7C3E0, disassembly 1A7C3E0..1A7C4C8, 115750, 495EFC) ------------
constexpr uintptr_t kRvaTable = 0x3C02220;      // KtglTokenHandlerTable, 185 x u64
constexpr uintptr_t kRvaLoopLea = 0x1A7C455;    // lea r8,[table]; lea rdx,[rbp-30h]; mov rcx,rdi; call [r8+rax*8]
constexpr uintptr_t kRvaLoopRet = 0x1A7C467;    //   -> return address of the dispatcher loop's handler call
constexpr uintptr_t kRvaWaitCall = 0x1A7C3FC;   // call [__imp_WaitForSingleObject] (dispatcher semaphore, INFINITE)
constexpr uintptr_t kRvaWaitRet = 0x1A7C402;
constexpr uintptr_t kRvaSubLea = 0x11575D;      // lea rdi,[table] (sub-range replay 115750)
constexpr uintptr_t kRvaRepLea = 0x495F26;      // lea r8,[table] (495EFC)
constexpr uintptr_t kRvaTextLo = 0x1000, kRvaTextHi = 0x1000 + 0x38E028B;
constexpr uintptr_t kRvaGfx = 0x45BA588;        // GfxDevice* (SynCtx = g + 10h)
constexpr uintptr_t kSynList = 0x10 + 0x580, kSynDev0 = 0x10 + 0x8E0, kSynDev1 = 0x10 + 0x8E8, kSynFence = 0x10 + 0x6C8,
                    kGfxQueue = 0x4270;
constexpr uint32_t kOpFrame = 0x95;             // once per frame (PagePoolFrameUpdate token, RenderFrameBegin)

struct Cfg {
    int mode = 0, sec = 10, delay = 45, top = 30, api = 1, topApi = 25;
} g_cfg;

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
bool g_known = false;
bool g_testMode = false;

// ---- replay-thread accounting (written by the replay thread only; the reporter reads aligned u64s) -------------------------
using Handler = void (*)(void*, uint8_t**);
Handler g_tokOrig[kTokens];
uintptr_t g_loopRet = 0, g_waitRet = 0;
struct HStat {
    uint64_t n, incl, child;
};
HStat g_h[kOps];
uint64_t g_outer = 0, g_childRun = 0;
uint64_t g_idleTicks = 0, g_idleN = 0;
volatile LONG64 g_otherN[kTokens];

// imports
using WfsoFn = DWORD(WINAPI*)(HANDLE, DWORD);
using WfsoExFn = DWORD(WINAPI*)(HANDLE, DWORD, BOOL);
using SleepFn = void(WINAPI*)(DWORD);
using SleepExFn = DWORD(WINAPI*)(DWORD, BOOL);
using YieldFn = BOOL(WINAPI*)();
WfsoFn g_oWfso = nullptr;
WfsoExFn g_oWfsoEx = nullptr;
SleepFn g_oSleep = nullptr;
SleepExFn g_oSleepEx = nullptr;
YieldFn g_oYield = nullptr;

inline uint32_t Tid() { return (uint32_t)__readgsdword(0x48); }
inline bool OnReplay() { return Tid() == rp_tid && rp_on; }
inline void MatAdd(uint32_t op, int g, uint64_t dt) {
    uint64_t* m = &rp_mat[((size_t)op * kGroups + g) * 2];
    m[0] += dt;
    m[1] += 1;
}

DWORD WINAPI HkWfso(HANDLE h, DWORD ms) {
    if (!OnReplay()) return g_oWfso(h, ms);
    const uintptr_t ra = (uintptr_t)_ReturnAddress();
    const uint64_t t0 = __rdtsc();
    const DWORD r = g_oWfso(h, ms);
    const uint64_t dt = __rdtsc() - t0;
    if (ra == g_waitRet && rp_curOp == kOpNone) {
        g_idleTicks += dt;
        ++g_idleN;
    } else {
        MatAdd(rp_curOp, kGWait, dt);
    }
    return r;
}
DWORD WINAPI HkWfsoEx(HANDLE h, DWORD ms, BOOL a) {
    if (!OnReplay()) return g_oWfsoEx(h, ms, a);
    const uint64_t t0 = __rdtsc();
    const DWORD r = g_oWfsoEx(h, ms, a);
    MatAdd(rp_curOp, kGWait, __rdtsc() - t0);
    return r;
}
void WINAPI HkSleep(DWORD ms) {
    if (!OnReplay()) return g_oSleep(ms);
    const uint64_t t0 = __rdtsc();
    g_oSleep(ms);
    MatAdd(rp_curOp, kGSleep, __rdtsc() - t0);
}
DWORD WINAPI HkSleepEx(DWORD ms, BOOL a) {
    if (!OnReplay()) return g_oSleepEx(ms, a);
    const uint64_t t0 = __rdtsc();
    const DWORD r = g_oSleepEx(ms, a);
    MatAdd(rp_curOp, kGSleep, __rdtsc() - t0);
    return r;
}
BOOL WINAPI HkYield() {
    if (!OnReplay()) return g_oYield();
    const uint64_t t0 = __rdtsc();
    const BOOL r = g_oYield();
    MatAdd(rp_curOp, kGSleep, __rdtsc() - t0);
    return r;
}

// ---- hook bookkeeping --------------------------------------------------------------------------------------------------
struct VtSet {
    const char* label = nullptr;
    int kind = 0;
    void** vt = nullptr;
    int n = 0;
};
VtSet g_set[kSets];
int g_nSets = 0;

struct Imp {
    const char* name;
    void* hook;
    void** orig;
    void** slot;
};
Imp g_imp[] = {
    {"WaitForSingleObject", (void*)&HkWfso, (void**)&g_oWfso, nullptr},
    {"WaitForSingleObjectEx", (void*)&HkWfsoEx, (void**)&g_oWfsoEx, nullptr},
    {"Sleep", (void*)&HkSleep, (void**)&g_oSleep, nullptr},
    {"SleepEx", (void*)&HkSleepEx, (void**)&g_oSleepEx, nullptr},
    {"SwitchToThread", (void*)&HkYield, (void**)&g_oYield, nullptr},
};
constexpr int kImps = (int)(sizeof g_imp / sizeof g_imp[0]);

uintptr_t g_table = 0;
uintptr_t g_selfLo = 0, g_selfHi = 0;
CRITICAL_SECTION g_cs;
volatile LONG g_installed = 0;  // hooks currently in place
volatile LONG g_ready = 0;      // layout verified, Install may run
volatile LONG g_gen = 0;        // bumped on every switch: the reporter restarts its window
volatile LONG g_want = 0;       // live switch

bool WritePtr(void** at, void* v) {
    DWORD old, t;
    if (!VirtualProtect(at, sizeof(void*), PAGE_READWRITE, &old)) return false;
    InterlockedExchangePointer(at, v);
    VirtualProtect(at, sizeof(void*), old, &t);
    return true;
}

bool InSelf(uintptr_t a) { return a >= g_selfLo && a < g_selfHi; }
bool InText(uintptr_t a) { return a >= g_base + kRvaTextLo && a < g_base + kRvaTextHi; }

void ModRange(const void* addr, uintptr_t& lo, uintptr_t& hi) {
    HMODULE m = nullptr;
    lo = hi = 0;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)addr, &m) ||
        !m)
        return;
    const auto* dos = (const IMAGE_DOS_HEADER*)m;
    const auto* nt = (const IMAGE_NT_HEADERS64*)((const uint8_t*)m + dos->e_lfanew);
    lo = (uintptr_t)m;
    hi = lo + nt->OptionalHeader.SizeOfImage;
}

void** ImportSlotOf(uintptr_t base, const char* fn) {
    __try {
        const auto* dos = (const IMAGE_DOS_HEADER*)base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
        const auto* nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
        const auto& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dd.VirtualAddress) return nullptr;
        for (auto d = (const IMAGE_IMPORT_DESCRIPTOR*)(base + dd.VirtualAddress); d->Name; ++d) {
            if (_stricmp((const char*)(base + d->Name), "KERNEL32.dll") != 0 || !d->OriginalFirstThunk) continue;
            auto oft = (const IMAGE_THUNK_DATA64*)(base + d->OriginalFirstThunk);
            auto ft = (IMAGE_THUNK_DATA64*)(base + d->FirstThunk);
            for (; oft->u1.AddressOfData; ++oft, ++ft)
                if (!IMAGE_SNAP_BY_ORDINAL64(oft->u1.Ordinal) &&
                    !strcmp(((const IMAGE_IMPORT_BY_NAME*)(base + oft->u1.AddressOfData))->Name, fn))
                    return (void**)&ft->u1.Function;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

bool RdPtr(uintptr_t a, uintptr_t& v) {
    if (a < 0x10000) return false;
    __try {
        v = *(const volatile uintptr_t*)a;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// rel32 operand at `at` (instruction ends at `end`) -> absolute target
uintptr_t Rel(uintptr_t at, uintptr_t end) { return end + (intptr_t) * (const int32_t*)at; }

}  // namespace

// ---- token wrapper (asm thunk: mov r8d, op; jmp RpTok) -----------------------------------------------------------------
}  // namespace replayprof

extern "C" bool RpForwards(const void* slotValue, const void* want) { return replayprof::Forwards(slotValue, want); }

extern "C" void RpTok(void* c, uint8_t** cur, uint32_t op) {
    using namespace replayprof;
    const Handler fn = g_tokOrig[op];
    const uint32_t tid = Tid();
    if (tid != rp_tid) {
        if (rp_tid == 0 && (uintptr_t)_ReturnAddress() == g_loopRet) {
            rp_tid = tid;  // the dispatcher loop's own call site: this is SynergisticDispatcherTask
        } else {
            InterlockedIncrement64(&g_otherN[op]);
            fn(c, cur);
            return;
        }
    }
    const uint32_t prev = rp_curOp;
    rp_curOp = op;
    const uint64_t child0 = g_childRun;
    const uint64_t t0 = __rdtsc();
    fn(c, cur);
    const uint64_t dt = __rdtsc() - t0;
    HStat& h = g_h[op];
    ++h.n;
    h.incl += dt;
    h.child += g_childRun - child0;
    g_childRun = child0 + dt;
    rp_curOp = prev;
    if (prev == kOpNone) g_outer += dt;
}

namespace replayprof {
namespace detail {

int HookVtable(const char* label, int kind, void** vt, int slots) {
    if (!vt || slots <= 0) return -1;
    for (int s = 0; s < g_nSets; ++s)
        if (g_set[s].vt == vt) return s;  // the same class through another object
    if (g_nSets >= kSets) return -1;
    int nn = 0;
    const char* const* names = KindNames(kind, nn);
    const int n = std::min({slots, nn, kSlots});
    const int s = g_nSets;
    DWORD old, t;
    if (!VirtualProtect(vt, n * sizeof(void*), PAGE_READWRITE, &old)) {
        if (g_log) g_log("RPF: VirtualProtect(vtable %p) failed (%lu) - %s not hooked", (void*)vt, GetLastError(), label);
        return -1;
    }
    for (int i = 0; i < n; ++i) {
        const int id = s * kSlots + i;
        rp_grp[id] = (uint8_t)GroupOf(kind, names[i]);
        rp_pk[id] = kind != kKindList ? 0
                    : !strcmp(names[i], "SetPipelineState")                            ? 1
                    : (!strcmp(names[i], "Reset") || !strcmp(names[i], "ClearState")) ? 2
                                                                                      : 0;
        rp_orig[id] = vt[i];
    }
    MemoryBarrier();
    for (int i = 0; i < n; ++i) InterlockedExchangePointer(&vt[i], rp_thunks[s * kSlots + i]);
    VirtualProtect(vt, n * sizeof(void*), old, &t);
    g_set[s] = {label, kind, vt, n};
    g_nSets = s + 1;
    return s;
}

void RehookVtables(bool on) {
    for (int s = 0; s < g_nSets; ++s) {
        VtSet& v = g_set[s];
        DWORD old, t;
        if (!VirtualProtect(v.vt, v.n * sizeof(void*), PAGE_READWRITE, &old)) continue;
        for (int i = 0; i < v.n; ++i) {
            const int id = s * kSlots + i;
            if (on) {
                if (v.vt[i] != rp_thunks[id]) {
                    rp_orig[id] = v.vt[i];
                    MemoryBarrier();
                    InterlockedExchangePointer(&v.vt[i], rp_thunks[id]);
                }
            } else if (v.vt[i] == rp_thunks[id]) {
                InterlockedExchangePointer(&v.vt[i], rp_orig[id]);
            }
        }
        VirtualProtect(v.vt, v.n * sizeof(void*), old, &t);
    }
}

int TableSwap(bool on) {
    if (!g_table) return 0;
    void** tab = (void**)g_table;
    DWORD old, t;
    if (!VirtualProtect(tab, kTokens * sizeof(void*), PAGE_READWRITE, &old)) return -1;
    int n = 0;
    for (int i = 0; i < kTokens; ++i) {
        if (on) {
            if (tab[i] != rp_tokthunks[i]) {
                g_tokOrig[i] = (Handler)tab[i];
                MemoryBarrier();
                InterlockedExchangePointer(&tab[i], rp_tokthunks[i]);
            }
            ++n;
        } else if (tab[i] == rp_tokthunks[i]) {
            InterlockedExchangePointer(&tab[i], (void*)g_tokOrig[i]);
            ++n;
        }
    }
    VirtualProtect(tab, kTokens * sizeof(void*), old, &t);
    return n;
}

int ImportSwap(bool on) {
    int n = 0;
    for (Imp& m : g_imp) {
        if (!m.slot) continue;
        if (on) {
            if (*m.slot != m.hook) {
                *m.orig = *m.slot;
                MemoryBarrier();
                if (WritePtr(m.slot, m.hook)) ++n;
            }
        } else if (*m.slot == m.hook) {
            if (WritePtr(m.slot, *m.orig)) ++n;
        }
    }
    return n;
}

void Switch(bool on) {
    EnterCriticalSection(&g_cs);
    if (on && !g_installed) {
        if (g_cfg.api) ImportSwap(true);
        if (g_cfg.api) RehookVtables(true);
        TableSwap(true);
        rp_on = 1;
        g_installed = 1;
    } else if (!on && g_installed) {
        TableSwap(false);
        RehookVtables(false);
        ImportSwap(false);
        rp_on = 0;
        g_installed = 0;
    }
    InterlockedIncrement(&g_gen);
    LeaveCriticalSection(&g_cs);
}

void TestSetup(uintptr_t table, uintptr_t loopRet, uintptr_t waitRet, void** wfsoSlot, int api) {
    static bool csInit = false;
    if (!csInit) {
        InitializeCriticalSection(&g_cs);
        csInit = true;
    }
    g_testMode = true;
    g_table = table;
    g_loopRet = loopRet;
    g_waitRet = waitRet;
    g_imp[0].slot = wfsoSlot;
    g_cfg.api = api;
    g_ready = 1;
}

Counters Read() {
    Counters c;
    for (int i = 0; i < kOps; ++i) {
        c.n[i] = g_h[i].n;
        c.incl[i] = g_h[i].incl;
        c.child[i] = g_h[i].child;
    }
    for (int i = 0; i < kTokens; ++i) c.other[i] = (uint64_t)g_otherN[i];
    memcpy(c.id, rp_id, sizeof c.id);
    memcpy(c.mat, rp_mat, sizeof c.mat);
    c.outer = g_outer;
    c.idle = g_idleTicks;
    c.idleN = g_idleN;
    c.tid = rp_tid;
    c.psoRedT = rp_psoRed[0];
    c.psoRedN = rp_psoRed[1];
    return c;
}

const char* IdName(int id, char* b, size_t n) {
    const int s = id / kSlots, i = id % kSlots;
    if (s >= g_nSets) {
        snprintf(b, n, "?%d", id);
        return b;
    }
    int nn = 0;
    const char* const* names = KindNames(g_set[s].kind, nn);
    snprintf(b, n, "%s.%s", g_set[s].label, i < nn ? names[i] : "?");
    return b;
}

// Report of one window (a = start, b = end; ms per TSC tick; seconds). Returns the number of lines logged.
int Report(const Counters& a, const Counters& b, double msPerTick, double sec, double pairTicks, int top, int topApi,
           void (*log)(const char*, ...)) {
    const uint64_t frames = b.n[kOpFrame] - a.n[kOpFrame];
    if (!frames) {
        log("RPF: %.1f s, no frames (token 95 not replayed: loading / menu?)%s", sec, b.tid ? "" : " - replay thread not seen yet");
        return 1;
    }
    const double F = (double)frames, k = msPerTick / F;
    auto D = [](uint64_t x, uint64_t y) { return (double)(y - x); };
    auto M = [&](const Counters& c, int op, int g, int w) { return c.mat[((size_t)op * kGroups + g) * 2 + w]; };
    auto isWait = [](int g) { return g == kGWait || g == kGSleep; };
    double d3dIn = 0, waitIn = 0, d3dOut = 0, waitOut = 0, tokN = 0, callN = 0, waitN = 0;
    double gT[kGroups] = {}, gN[kGroups] = {};
    for (int op = 0; op < kOps; ++op)
        for (int g = 0; g < kGroups; ++g) {
            const double t = D(M(a, op, g, 0), M(b, op, g, 0)), n = D(M(a, op, g, 1), M(b, op, g, 1));
            gT[g] += t;
            gN[g] += n;
            if (isWait(g)) {
                (op == kOpNone ? waitOut : waitIn) += t;
                waitN += n;
            } else {
                (op == kOpNone ? d3dOut : d3dIn) += t;
                callN += n;
            }
        }
    for (int op = 0; op < kTokens; ++op) tokN += D(a.n[op], b.n[op]);
    const double wall = sec * 1000.0 / F, handlers = D(a.outer, b.outer) * k, idle = D(a.idle, b.idle) * k;
    const double events = tokN + callN + waitN + D(a.idleN, b.idleN);
    log("RPF: %.1f s, %llu frames (%.1f/s by token 95); replay thread ms/frame: wall %.2f = handlers %.2f [self CPU %.2f, "
        "D3D12 %.2f, waits %.2f] + outside handlers %.2f [D3D12 %.2f, waits %.2f] + idle (waiting for kicks) %.2f in %.1f "
        "waits; %.0f tokens/f, %.0f D3D12 calls/f; est. probe cost %.2f ms/f",
        sec, (unsigned long long)frames, F / sec, wall, handlers, handlers - (d3dIn + waitIn) * k, d3dIn * k, waitIn * k,
        wall - handlers - idle, d3dOut * k, waitOut * k, idle, D(a.idleN, b.idleN) / F, tokN / F, callN / F,
        events * pairTicks * k);
    int lines = 1;
    // handlers by self time (incl - nested handlers)
    int ord[kTokens];
    double self[kTokens];
    for (int i = 0; i < kTokens; ++i) {
        ord[i] = i;
        self[i] = D(a.incl[i], b.incl[i]) - D(a.child[i], b.child[i]);
    }
    std::sort(ord, ord + kTokens, [&](int x, int y) { return self[x] > self[y]; });
    double totSelf = 0;
    for (int i = 0; i < kTokens; ++i) totSelf += self[i];
    for (int r = 0; r < top && r < kTokens; ++r) {
        const int op = ord[r];
        const double n = D(a.n[op], b.n[op]);
        if (n <= 0) break;
        double d3d = 0, w = 0;
        int g1 = -1, g2 = -1;
        double t1 = 0, t2 = 0;
        for (int g = 0; g < kGroups; ++g) {
            const double t = D(M(a, op, g, 0), M(b, op, g, 0));
            (isWait(g) ? w : d3d) += t;
            if (t > t1) {
                g2 = g1, t2 = t1;
                g1 = g, t1 = t;
            } else if (t > t2) {
                g2 = g, t2 = t;
            }
        }
        char gs[160] = "";
        if (g1 >= 0) {
            const int c1 = (int)snprintf(gs, sizeof gs, " [%s %.3f x%.0f", kGroupNames[g1], t1 * k,
                                         D(M(a, op, g1, 1), M(b, op, g1, 1)) / F);
            if (g2 >= 0 && c1 > 0)
                snprintf(gs + c1, sizeof gs - c1, ", %s %.3f x%.0f]", kGroupNames[g2], t2 * k, D(M(a, op, g2, 1), M(b, op, g2, 1)) / F);
            else if (c1 > 0)
                snprintf(gs + c1, sizeof gs - c1, "]");
        }
        log("RPF tok %2d %02X %-34s n/f %7.1f self %6.3f (%4.1f %%) incl %6.3f cpu %6.3f d3d %6.3f wait %6.3f ms/f, %5.0f ns/call%s",
            r + 1, op, kTokNames[op].name, n / F, self[op] * k, totSelf > 0 ? 100.0 * self[op] / totSelf : 0.0,
            D(a.incl[op], b.incl[op]) * k, (self[op] - d3d - w) * k, d3d * k, w * k, self[op] * msPerTick * 1e6 / n, gs);
        ++lines;
    }
    // call groups
    {
        char buf[1400];
        int p = snprintf(buf, sizeof buf, "RPF groups ms/f (calls/f):");
        int gord[kGroups];
        for (int g = 0; g < kGroups; ++g) gord[g] = g;
        std::sort(gord, gord + kGroups, [&](int x, int y) { return gT[x] > gT[y]; });
        for (int i = 0; i < kGroups && p > 0 && p < (int)sizeof buf - 64; ++i) {
            const int g = gord[i];
            if (gN[g] <= 0) continue;
            p += snprintf(buf + p, sizeof buf - p, " %s %.3f (%.0f);", kGroupNames[g], gT[g] * k, gN[g] / F);
        }
        log("%s", buf);
        ++lines;
    }
    {
        double psoN = 0, psoT = 0;
        for (int s = 0; s < g_nSets; ++s)
            for (int i = 0; i < g_set[s].n; ++i)
                if (rp_pk[s * kSlots + i] == 1) {
                    psoT += D(a.id[(s * kSlots + i) * 2], b.id[(s * kSlots + i) * 2]);
                    psoN += D(a.id[(s * kSlots + i) * 2 + 1], b.id[(s * kSlots + i) * 2 + 1]);
                }
        const double rn = D(a.psoRedN, b.psoRedN), rt = D(a.psoRedT, b.psoRedT);
        log("RPF pso: SetPipelineState %.0f/f %.3f ms/f, redundant (same list, same PSO, no Reset / ClearState between) %.0f/f "
            "(%.1f %%) %.3f ms/f",
            psoN / F, psoT * k, rn / F, psoN > 0 ? 100.0 * rn / psoN : 0.0, rt * k);
        ++lines;
    }
    // top methods
    {
        static int iord[kIds];
        static double it[kIds];
        for (int i = 0; i < kIds; ++i) {
            iord[i] = i;
            it[i] = D(a.id[i * 2], b.id[i * 2]);
        }
        std::sort(iord, iord + kIds, [&](int x, int y) { return it[x] > it[y]; });
        for (int r = 0; r < topApi; ++r) {
            const int id = iord[r];
            const double n = D(a.id[id * 2 + 1], b.id[id * 2 + 1]);
            if (n <= 0) break;
            char nm[96];
            log("RPF api %2d %-44s %6.3f ms/f  n/f %7.1f  %5.0f ns/call", r + 1, IdName(id, nm, sizeof nm), it[id] * k, n / F,
                it[id] * msPerTick * 1e6 / n);
            ++lines;
        }
    }
    // tokens replayed on other threads (not timed)
    {
        double on = 0;
        int best = -1;
        double bn = 0;
        for (int i = 0; i < kTokens; ++i) {
            const double n = D(a.other[i], b.other[i]);
            on += n;
            if (n > bn) bn = n, best = i;
        }
        if (on > 0) {
            log("RPF: %.1f tokens/f replayed on other threads (not timed); most %02X %s %.1f/f", on / F, best,
                kTokNames[best].name, bn / F);
            ++lines;
        }
    }
    return lines;
}

}  // namespace detail

namespace {

double PairTicks() {  // TSC ticks of one rdtsc pair + an add (the probe's minimum per event), median of 5 runs
    double r[5];
    for (int k = 0; k < 5; ++k) {
        volatile uint64_t acc = 0;
        const uint64_t t0 = __rdtsc();
        for (int i = 0; i < 20000; ++i) {
            const uint64_t a = __rdtsc();
            acc = acc + (__rdtsc() - a);
        }
        r[k] = (double)(__rdtsc() - t0) / 20000.0;
    }
    std::sort(r, r + 5);
    return r[2];
}

bool VerifyLayout() {
    const uintptr_t b = g_base;
    __try {
        const uint8_t* p = (const uint8_t*)(b + kRvaLoopLea);
        static const uint8_t kTail[] = {0x48, 0x8D, 0x55, 0xD0, 0x48, 0x8B, 0xCF, 0x41, 0xFF, 0x14, 0xC0};
        if (p[0] != 0x4C || p[1] != 0x8D || p[2] != 0x05 || Rel(b + kRvaLoopLea + 3, b + kRvaLoopLea + 7) != b + kRvaTable ||
            memcmp(p + 7, kTail, sizeof kTail) != 0 || b + kRvaLoopLea + 7 + sizeof kTail != b + kRvaLoopRet) {
            g_log("RPF: layout check FAILED (dispatcher loop 1A7C455)");
            return false;
        }
        const uint8_t* q = (const uint8_t*)(b + kRvaSubLea);
        const uint8_t* r = (const uint8_t*)(b + kRvaRepLea);
        if (q[0] != 0x48 || q[1] != 0x8D || q[2] != 0x3D || Rel(b + kRvaSubLea + 3, b + kRvaSubLea + 7) != b + kRvaTable ||
            r[0] != 0x4C || r[1] != 0x8D || r[2] != 0x05 || Rel(b + kRvaRepLea + 3, b + kRvaRepLea + 7) != b + kRvaTable) {
            g_log("RPF: layout check FAILED (sub-range replays 11575D / 495F26)");
            return false;
        }
        const uint8_t* w = (const uint8_t*)(b + kRvaWaitCall);
        void** wfso = ImportSlotOf(b, "WaitForSingleObject");
        if (w[0] != 0xFF || w[1] != 0x15 || !wfso || Rel(b + kRvaWaitCall + 2, b + kRvaWaitRet) != (uintptr_t)wfso) {
            g_log("RPF: layout check FAILED (dispatcher wait 1A7C3FC is not call [WaitForSingleObject import])");
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_log("RPF: layout check FAULTED");
        return false;
    }
    // table: every entry the engine's handler or a wrapper in this module (FO / GP wrap some)
    int eng = 0, own = 0, bad = 0;
    for (int i = 0; i < kTokens; ++i) {
        uintptr_t v = 0;
        RdPtr(b + kRvaTable + i * 8, v);
        if (v == b + kTokNames[i].rva) ++eng;
        else if (InSelf(v)) ++own;
        else if (!InText(v)) ++bad;
        else ++eng;  // in .text but not the documented handler: still the engine's code
    }
    if (bad) {
        g_log("RPF: token table has %d entries outside Nioh3.exe .text and this mod - not installed", bad);
        return false;
    }
    g_log("RPF: layout verified (loop 1A7C455, wait 1A7C3FC, sub-replays 11575D / 495F26; table: %d engine, %d wrapped by this mod)",
          eng, own);
    return true;
}

template <class T>
int SlotCountQi(IUnknown* o, const T& tab) {
    for (auto& e : tab) {
        IUnknown* q = nullptr;
        if (SUCCEEDED(o->QueryInterface(*e.iid, (void**)&q)) && q) {
            const bool same = *(void***)q == *(void***)o;
            q->Release();
            if (same) return e.slots;
        }
    }
    return 0;
}
struct IidSlots {
    const IID* iid;
    int slots;
};
const IidSlots kListIids[] = {{&__uuidof(ID3D12GraphicsCommandList7), 81}, {&__uuidof(ID3D12GraphicsCommandList6), 80},
                              {&__uuidof(ID3D12GraphicsCommandList5), 79}, {&__uuidof(ID3D12GraphicsCommandList4), 77},
                              {&__uuidof(ID3D12GraphicsCommandList3), 68}, {&__uuidof(ID3D12GraphicsCommandList2), 67},
                              {&__uuidof(ID3D12GraphicsCommandList1), 66}, {&__uuidof(ID3D12GraphicsCommandList), 60}};
const IidSlots kDevIids[] = {{&__uuidof(ID3D12Device10), 79}, {&__uuidof(ID3D12Device9), 76}, {&__uuidof(ID3D12Device8), 73},
                             {&__uuidof(ID3D12Device7), 68}, {&__uuidof(ID3D12Device6), 66}, {&__uuidof(ID3D12Device5), 65},
                             {&__uuidof(ID3D12Device4), 57}, {&__uuidof(ID3D12Device3), 53}, {&__uuidof(ID3D12Device2), 50},
                             {&__uuidof(ID3D12Device1), 47}, {&__uuidof(ID3D12Device), 44}};
const IidSlots kQueueIids[] = {{&__uuidof(ID3D12CommandQueue), 19}};
const IidSlots kFenceIids[] = {{&__uuidof(ID3D12Fence1), 12}, {&__uuidof(ID3D12Fence), 11}};
const IidSlots kResIids[] = {{&__uuidof(ID3D12Resource2), 17}, {&__uuidof(ID3D12Resource1), 16}, {&__uuidof(ID3D12Resource), 15}};

const char* ModName(const void* p, char* b, size_t n) {
    HMODULE m = nullptr;
    b[0] = 0;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)p, &m) &&
        m) {
        char path[MAX_PATH];
        if (GetModuleFileNameA(m, path, MAX_PATH)) {
            const char* s = strrchr(path, '\\');
            snprintf(b, n, "%s", s ? s + 1 : path);
        }
    }
    if (!b[0]) snprintf(b, n, "?");
    return b;
}

template <class T>
void HookObject(const char* label, int kind, IUnknown* o, const T& iids) {
    if (!o) return;
    void** vt = *(void***)o;
    const int n = SlotCountQi(o, iids);
    char mn[64];
    if (n <= 0) {
        g_log("RPF: %s %p: no known interface on vtable %p (%s) - not hooked", label, (void*)o, (void*)vt, ModName(vt, mn, sizeof mn));
        return;
    }
    const int before = g_nSets;
    const int s = HookVtable(label, kind, vt, n);
    if (s < 0) return;
    if (s == before)
        g_log("RPF: hooked %s vtable %p (%s), %d slots", label, (void*)vt, ModName(vt, mn, sizeof mn), g_set[s].n);
    else
        g_log("RPF: %s uses the %s vtable (already hooked)", label, g_set[s].label);
}

void HookD3D() {
    wchar_t sys[MAX_PATH] = {};
    const UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    HMODULE d3d12 = (n && n < MAX_PATH - 16) ? LoadLibraryW((std::wstring(sys) + L"\\d3d12.dll").c_str()) : nullptr;
    auto create = d3d12 ? (PFN_D3D12_CREATE_DEVICE)GetProcAddress(d3d12, "D3D12CreateDevice") : nullptr;
    ID3D12Device* dev = nullptr;
    if (!create || FAILED(create(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), (void**)&dev)) || !dev) {
        g_log("RPF: no dummy D3D12 device - D3D12 calls not timed");
        return;
    }
    // runtime classes through dummy objects (released before patching: none of their calls go through the hooks)
    ID3D12CommandAllocator* al[2] = {};
    ID3D12GraphicsCommandList* li[2] = {};
    const D3D12_COMMAND_LIST_TYPE ty[2] = {D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_TYPE_COMPUTE};
    for (int t = 0; t < 2; ++t)
        if (SUCCEEDED(dev->CreateCommandAllocator(ty[t], __uuidof(ID3D12CommandAllocator), (void**)&al[t])))
            if (SUCCEEDED(dev->CreateCommandList(0, ty[t], al[t], nullptr, __uuidof(ID3D12GraphicsCommandList), (void**)&li[t])))
                li[t]->Close();
    ID3D12CommandQueue* q = nullptr;
    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    dev->CreateCommandQueue(&qd, __uuidof(ID3D12CommandQueue), (void**)&q);
    ID3D12Fence* f = nullptr;
    dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&f);
    ID3D12Resource* res = nullptr;
    {
        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = 65536;
        rd.Height = rd.DepthOrArraySize = rd.MipLevels = 1;
        rd.SampleDesc.Count = 1;
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                     __uuidof(ID3D12Resource), (void**)&res);
    }
    struct Pend {
        const char* label;
        int kind;
        void** vt;
        int n;
    } pend[8];
    int np = 0;
    auto add = [&](const char* l, int kind, IUnknown* o, int cnt) {
        if (o && cnt > 0) pend[np++] = {l, kind, *(void***)o, cnt};
    };
    add("list", kKindList, li[0], SlotCountQi(li[0], kListIids));
    add("clist", kKindList, li[1], li[1] ? SlotCountQi(li[1], kListIids) : 0);
    add("dev", kKindDevice, dev, SlotCountQi(dev, kDevIids));
    add("queue", kKindQueue, q, q ? SlotCountQi(q, kQueueIids) : 0);
    add("fence", kKindFence, f, f ? SlotCountQi(f, kFenceIids) : 0);
    add("res", kKindResource, res, res ? SlotCountQi(res, kResIids) : 0);
    for (int t = 0; t < 2; ++t) {
        if (li[t]) li[t]->Release();
        if (al[t]) al[t]->Release();
    }
    if (q) q->Release();
    if (f) f->Release();
    if (res) res->Release();
    dev->Release();
    char mn[64];
    for (int i = 0; i < np; ++i) {
        const int before = g_nSets;
        const int s = HookVtable(pend[i].label, pend[i].kind, pend[i].vt, pend[i].n);
        if (s == before)
            g_log("RPF: hooked runtime %s vtable %p (%s), %d slots", pend[i].label, (void*)pend[i].vt,
                  ModName(pend[i].vt, mn, sizeof mn), g_set[s].n);
    }
    // the engine's own objects (a Streamline proxy has its own vtable; the same class is skipped)
    uintptr_t g = 0, o = 0;
    if (!RdPtr(g_base + kRvaGfx, g) || !g) {
        g_log("RPF: no GfxDevice yet - engine objects not checked");
        return;
    }
    const uintptr_t synOff[4] = {kSynList, kSynDev0, kSynDev1, kSynFence};
    const char* lab[4] = {"eng.list", "eng.dev", "eng.dev2", "eng.fence"};
    const int kinds[4] = {kKindList, kKindDevice, kKindDevice, kKindFence};
    for (int i = 0; i < 4; ++i) {
        if (!RdPtr(g + synOff[i], o) || !o) {
            g_log("RPF: SynCtx+%llXh is null", (unsigned long long)(synOff[i] - 0x10));
            continue;
        }
        if (kinds[i] == kKindList) HookObject(lab[i], kinds[i], (IUnknown*)o, kListIids);
        else if (kinds[i] == kKindDevice) HookObject(lab[i], kinds[i], (IUnknown*)o, kDevIids);
        else HookObject(lab[i], kinds[i], (IUnknown*)o, kFenceIids);
    }
    if (RdPtr(g + kGfxQueue, o) && o) HookObject("eng.queue", kKindQueue, (IUnknown*)o, kQueueIids);
}

DWORD WINAPI Thread(LPVOID) {
    Sleep((DWORD)g_cfg.delay * 1000);
    // wait for the renderer (GfxDevice + the open command list), up to 10 minutes
    for (int i = 0; i < 600; ++i) {
        uintptr_t g = 0, l = 0;
        if (GetModuleHandleW(L"d3d12.dll") && RdPtr(g_base + kRvaGfx, g) && g && RdPtr(g + kSynList, l) && l) break;
        Sleep(1000);
    }
    if (!VerifyLayout()) return 0;
    g_table = g_base + kRvaTable;
    g_loopRet = g_base + kRvaLoopRet;
    g_waitRet = g_base + kRvaWaitRet;
    for (Imp& m : g_imp) m.slot = ImportSlotOf(g_base, m.name);
    if (g_cfg.api) {
        EnterCriticalSection(&g_cs);
        HookD3D();  // installs the vtable thunks (pass-through until rp_on and the replay thread is known)
        LeaveCriticalSection(&g_cs);
    }
    const double pair = PairTicks();
    g_ready = 1;
    Switch(g_want != 0);
    g_log("RPF: replay profiler ready: %s, %d D3D12 vtables, %d imports, report every %d s, probe pair %.0f TSC ticks (live: "
          "Tuning.RPF = 0/1)",
          g_installed ? "ON" : "installed OFF", g_nSets, (int)std::count_if(g_imp, g_imp + kImps, [](const Imp& m) { return m.slot; }),
          g_cfg.sec, pair);
    static Counters c0, c1;
    LARGE_INTEGER qf, q0 = {}, q1 = {};
    QueryPerformanceFrequency(&qf);
    LONG gen = -1;
    uint64_t t0 = 0;
    for (;;) {
        if (gen != g_gen) {  // (re)start the window after a switch
            gen = g_gen;
            c0 = Read();
            QueryPerformanceCounter(&q0);
            t0 = __rdtsc();
        }
        Sleep(100);
        QueryPerformanceCounter(&q1);
        const double sec = (double)(q1.QuadPart - q0.QuadPart) / (double)qf.QuadPart;
        if (sec < g_cfg.sec || gen != g_gen) continue;
        const uint64_t t1 = __rdtsc();
        c1 = Read();
        if (g_installed) {
            const double msPerTick = sec * 1000.0 / (double)(t1 - t0);
            Report(c0, c1, msPerTick, sec, pair, g_cfg.top, g_cfg.topApi, g_log);
        }
        c0 = c1;
        q0 = q1;
        t0 = t1;
    }
}

}  // namespace

bool Forwards(const void* slotValue, const void* want) {  // also extern "C" RpForwards (gpuopt.cpp)
    for (int s = 0; s < g_nSets; ++s)
        for (int i = 0; i < g_set[s].n; ++i)
            if (rp_thunks[s * kSlots + i] == slotValue) return rp_orig[s * kSlots + i] == want;
    return false;
}

bool SetTuning(const char* key, const char* val, char* reply, size_t n) {
    if (_stricmp(key, "RPF") != 0) return false;
    const int v = atoi(val);
    if (v != 0 && v != 1) {
        snprintf(reply, n, "RPF must be 0 or 1 (got %s)", val);
        return true;
    }
    if (!g_ready) {
        InterlockedExchange(&g_want, v);
        snprintf(reply, n, "RPF = %d (%s)", v, g_cfg.mode ? "applied when the profiler is ready" : "[Profiler] ReplayProf is 0 - not installed");
        if (g_log) g_log("LIVE tuning: %s", reply);
        return true;
    }
    InterlockedExchange(&g_want, v);
    Switch(v != 0);
    snprintf(reply, n, "RPF = %d", g_installed ? 1 : 0);
    g_log("LIVE tuning: %s", reply);
    return true;
}

void Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_known = info.knownBuild;
    auto I = [&](const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(L"Profiler", key, def, info.iniPath); };
    g_cfg.mode = I(L"ReplayProf", 0);
    if (g_cfg.mode != 1 && g_cfg.mode != 2) return;
    g_cfg.sec = std::clamp(I(L"ReplayProfSec", 10), 2, 600);
    g_cfg.delay = std::clamp(I(L"ReplayProfDelaySec", 45), 0, 600);
    g_cfg.top = std::clamp(I(L"ReplayProfTop", 30), 0, kTokens);
    g_cfg.topApi = std::clamp(I(L"ReplayProfTopApi", 25), 0, 100);
    g_cfg.api = I(L"ReplayProfApi", 1) ? 1 : 0;
    g_want = g_cfg.mode == 1 ? 1 : 0;
    if (!g_known) {
        g_log("RPF: unknown game build - replay profiler off");
        return;
    }
    InitializeCriticalSection(&g_cs);
    ModRange((const void*)&RpTok, g_selfLo, g_selfHi);
    if (HANDLE h = CreateThread(nullptr, 0, Thread, nullptr, 0, nullptr)) {
        using SetDesc = HRESULT(WINAPI*)(HANDLE, PCWSTR);
        if (auto sd = reinterpret_cast<SetDesc>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")))
            sd(h, L"Nioh3EngineFix RPF");
        CloseHandle(h);
    }
    g_log("RPF: replay profiler armed (%s, installs %d s after start; D3D12 calls %s)", g_cfg.mode == 1 ? "ON" : "installed OFF",
          g_cfg.delay, g_cfg.api ? "timed" : "not timed");
}

}  // namespace replayprof
