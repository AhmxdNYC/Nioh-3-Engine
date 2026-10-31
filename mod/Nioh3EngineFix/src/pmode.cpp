// pmode.cpp - present mode log ([Debug] PresentModeLog) + [Latency] VrrPresent. See pmode.h and notes\vrr-present.md.
//
// Runs inside osd.cpp's bottom-most Present / Present1 hook (vtable of the REAL dxgi.dll swapchain class: below
// Streamline's interposer, OptiScaler's dxgi.dll proxy and the game's statically linked FidelityFX presenter), so what
// it sees and sends is exactly what DXGI (and PresentMon's SyncInterval / PresentMode columns) gets.
// Engine fields (v2.02, byte-verified by the PM group deps in patches.json):
//   g_GfxDevice [Nioh3+45BA588]; +424Ch SyncInterval state; +3F80h present-thread mode (0 = inline Present);
//   +7738h SyncInterval handed to the present thread this frame (GfxDevicePresent 3D6545); VSync slot Nioh3+45BAF3C
//   (0 OFF, 1 ON); FRAME_GENERATION slot Nioh3+45BB008.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "gamerva.h"
#include <dxgi.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pmode.h"

// PM group caves (pmode.asm): sequence / value / device of the last watcher and setter store, the setter's caller
extern "C" {
extern volatile uint64_t pm_wSeq, pm_wDev, pm_sSeq, pm_sDev, pm_sRet;
extern volatile uint32_t pm_wVal, pm_sVal;
}

namespace pmode {
namespace {

constexpr uint32_t kRvaGfxDevice = 0x45BA588;
constexpr uint32_t kOffSi = 0x424C, kOffPresentThread = 0x3F80, kOffReqSi = 0x7738;
constexpr uint32_t kRvaVsync = 0x45BAF3C, kRvaFrameGen = 0x45BB008;
constexpr unsigned kTear = 0x200;        // DXGI_PRESENT_ALLOW_TEARING
constexpr unsigned kKeepable = 0x200 | 0x4 | 0x8;  // ALLOW_TEARING | RESTART | DO_NOT_WAIT: other flags -> never rewritten
constexpr UINT kScAllowTearing = 0x800;  // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
constexpr int kMaxLines = 200;

void (*g_log)(const char*, ...) = nullptr;
uint8_t* g_base = nullptr;
bool g_known = false, g_wine = false, g_started = false;
int g_logMode = 1;  // [Debug] PresentModeLog
int g_vrr = 0;      // [Latency] VrrPresent, effective: -1 auto, 0 off, 1 on
bool g_probes = false;
SRWLOCK g_lock = SRWLOCK_INIT;
LARGE_INTEGER g_freq{};
int g_lines = 0;
uint64_t g_suppressed = 0;

int64_t Qpc() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
double Secs(int64_t d) { return g_freq.QuadPart ? (double)d / (double)g_freq.QuadPart : 0.0; }

#define PLOG(...)                     \
    do {                              \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

// change lines: capped per session (a value flipping every frame must not flood the log)
template <class... A>
void Line(const char* fmt, A... a) {
    if (!g_log) return;
    if (g_lines >= kMaxLines) {
        if (g_suppressed++ == 0) g_log("PresentMode: %d change lines written - further changes are only counted", kMaxLines);
        return;
    }
    ++g_lines;
    g_log(fmt, a...);
}

bool RdI(uintptr_t a, int& v) {
    if (a < 0x10000) return false;
    __try {
        v = *reinterpret_cast<const volatile int*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool RdP(uintptr_t a, uintptr_t& v) {
    if (a < 0x10000) return false;
    __try {
        v = *reinterpret_cast<const volatile uintptr_t*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

int Slot(uint32_t rva) {
    int v = -1;
    if (g_known && g_base) RdI((uintptr_t)g_base + Grva(rva), v);
    return v;
}
const char* OnOff(int v) { return v == 0 ? "OFF" : v == 1 ? "ON" : "?"; }

// "module+offset" (basename) of a code address
void ModOff(const void* p, char* out, size_t n) {
    HMODULE m = nullptr;
    if (!p || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  (LPCWSTR)p, &m)) {
        snprintf(out, n, "%p", p);
        return;
    }
    char path[MAX_PATH] = {};
    GetModuleFileNameA(m, path, MAX_PATH);
    const char* b = strrchr(path, '\\');
    snprintf(out, n, "%s+%llX", b ? b + 1 : path, (unsigned long long)((const uint8_t*)p - (const uint8_t*)m));
}

// ---- engine SyncInterval and its writers ---------------------------------------------------------------------------
struct Engine {
    uintptr_t dev = 0;
    int si = -99, vs = -99;
    uint64_t seenW = 0, seenS = 0, sameW = 0, sameS = 0;
    int64_t mismatchSince = 0;
    bool mismatchLogged = false;
} g_eng;

// "engine SI 1, present-thread request 1, VSync ON, FG 2" (for the caller lines)
void EngineText(char* out, size_t n) {
    if (!g_known) {
        snprintf(out, n, "engine fields n/a (unknown build)");
        return;
    }
    int pt = 0;
    const bool thread = g_eng.dev && RdI(g_eng.dev + kOffPresentThread, pt) && (pt & 0xFF);
    char req[32] = "inline";
    int r = 0;
    if (thread && RdI(g_eng.dev + kOffReqSi, r)) snprintf(req, sizeof(req), "%d", r);
    snprintf(out, n, "engine SI %d, request %s, VSync %s, FG %d", g_eng.si, req, OnOff(g_eng.vs), Slot(kRvaFrameGen));
}

void EngineSample(int64_t now) {
    if (!g_known || !g_base) return;
    uintptr_t dev = 0;
    if (!RdP((uintptr_t)g_base + Grva(kRvaGfxDevice), dev) || dev < 0x10000) return;
    int si = 0;
    if (!RdI(dev + kOffSi, si)) return;
    const int vs = Slot(kRvaVsync);
    const uint64_t ws = pm_wSeq, ss = pm_sSeq;
    const bool newDev = dev != g_eng.dev;
    if (newDev || si != g_eng.si) {
        char who[160];
        if (g_probes && ws != g_eng.seenW && (int)pm_wVal == si && (uintptr_t)pm_wDev == dev) {
            snprintf(who, sizeof(who), "DisplaySettingsWatcher 1350210 (VSync slot %s)", OnOff(vs));
        } else if (g_probes && ss != g_eng.seenS && (int)pm_sVal == si && (uintptr_t)pm_sDev == dev) {
            char c[96];
            ModOff((const void*)(uintptr_t)pm_sRet, c, sizeof(c));
            snprintf(who, sizeof(who), "setter 2EEBB50 called from %s", c);
        } else if (newDev && g_eng.dev == 0) {
            snprintf(who, sizeof(who), si == 1 ? "first look: GfxDevice ctor 2EDFBBD default 1, no later write seen"
                                               : "first look: written before the hook saw a frame");
        } else if (newDev) {
            snprintf(who, sizeof(who), si == 1 ? "a NEW GfxDevice %p (ctor 2EDFBBD default 1)" : "a NEW GfxDevice %p",
                     (void*)dev);
        } else {
            snprintf(who, sizeof(who), g_probes ? "a writer that is not probed (not the watcher, not the setter)"
                                                : "writer not known (PM probes not installed)");
        }
        if (g_eng.si == -99)
            Line("PresentMode: engine SyncInterval (GfxDevice %p +424C) = %d (%s); game VSync %s, FG %d", (void*)dev, si, who,
                 OnOff(vs), Slot(kRvaFrameGen));
        else
            Line("PresentMode: engine SyncInterval %d -> %d by %s; game VSync %s, FG %d", g_eng.si, si, who, OnOff(vs),
                 Slot(kRvaFrameGen));
        g_eng.mismatchSince = 0;
    } else {
        if (ws != g_eng.seenW) ++g_eng.sameW;  // a write of the value it already had
        if (ss != g_eng.seenS) ++g_eng.sameS;
    }
    g_eng.seenW = ws;
    g_eng.seenS = ss;
    g_eng.dev = dev;
    g_eng.si = si;
    if (vs != g_eng.vs) {
        if (g_eng.vs != -99) Line("PresentMode: game VSync setting %s -> %s", OnOff(g_eng.vs), OnOff(vs));
        g_eng.vs = vs;
        g_eng.mismatchSince = 0;
    }
    // the state the VSync setting asks for vs the engine's state (stale value: the watcher writes only when the slot
    // changes, updates its cache BEFORE its guard d+2538 != 0 && d+2544 >= 0 and never retries a skipped write)
    if (vs == 0 || vs == 1) {
        if (si != vs) {
            if (!g_eng.mismatchSince) g_eng.mismatchSince = now;
            else if (!g_eng.mismatchLogged && Secs(now - g_eng.mismatchSince) > 2.0) {
                g_eng.mismatchLogged = true;
                Line("PresentMode: engine SyncInterval %d differs from the game's VSync setting %s for > 2 s (watcher write "
                     "skipped by its guard, or a later writer)", si, OnOff(vs));
            }
        } else {
            g_eng.mismatchSince = 0;
            if (g_eng.mismatchLogged) {
                g_eng.mismatchLogged = false;
                Line("PresentMode: engine SyncInterval %d follows the VSync setting again", si);
            }
        }
    }
}

// ---- per-caller present arguments --------------------------------------------------------------------------------
struct Caller {
    const void* addr = nullptr;
    DWORD tid = 0;
    int role = -1;
    unsigned inS = 0, inF = 0, outS = 0, outF = 0;
    uint64_t n = 0, sinceChange = 0, rewritten = 0, winN = 0, winRw = 0, winSi1 = 0;
};
Caller g_callers[8];
bool g_callersFull = false;
int64_t g_lastSummary = 0;

const char* RoleName(int r) { return r == 1 ? "FSR presenter" : r == 0 ? "game/other" : "?"; }

void NoteCaller(const void* caller, int role, unsigned inS, unsigned inF, unsigned outS, unsigned outF, bool rw,
                IDXGISwapChain* sc) {
    Caller* c = nullptr;
    for (Caller& x : g_callers)
        if (x.addr == caller) c = &x;
    const bool fresh = c == nullptr;
    if (fresh) {
        for (Caller& x : g_callers)
            if (!x.addr) {
                c = &x;
                break;
            }
        if (!c) {
            if (!g_callersFull) Line("PresentMode: more than 8 Present callers - the rest is not tracked");
            g_callersFull = true;
            return;
        }
        c->addr = caller;
        c->tid = GetCurrentThreadId();
    }
    if (role >= 0) c->role = role;
    ++c->n;
    ++c->winN;
    if (rw) ++c->rewritten, ++c->winRw;
    if (inS) ++c->winSi1;
    if (fresh || c->inS != inS || c->inF != inF || c->outS != outS || c->outF != outF) {
        char where[96], eng[96], sent[64] = "";
        ModOff(caller, where, sizeof(where));
        EngineText(eng, sizeof(eng));
        if (outS != inS || outF != inF) snprintf(sent, sizeof(sent), " -> sent SI %u flags 0x%X (VrrPresent)", outS, outF);
        if (fresh)
            Line("PresentMode: %s (thread %lu, %s) presents SI %u flags 0x%X%s on swapchain %p; %s", where, c->tid,
                 RoleName(c->role), inS, inF, sent, (void*)sc, eng);
        else
            Line("PresentMode: %s (%s) SI %u flags 0x%X -> SI %u flags 0x%X%s after %llu presents; %s", where,
                 RoleName(c->role), c->inS, c->inF, inS, inF, sent, (unsigned long long)c->sinceChange, eng);
        c->inS = inS;
        c->inF = inF;
        c->outS = outS;
        c->outF = outF;
        c->sinceChange = 0;
    }
    ++c->sinceChange;
}

void Summary(int64_t now) {
    if (g_logMode < 2) return;
    if (!g_lastSummary) {
        g_lastSummary = now;
        return;
    }
    const double el = Secs(now - g_lastSummary);
    if (el < 30.0) return;
    g_lastSummary = now;
    for (Caller& c : g_callers) {
        if (!c.addr || !c.winN) continue;
        char where[96];
        ModOff(c.addr, where, sizeof(where));
        PLOG("PresentMode: %.0f s %s (%s): %.1f presents/s, SI>0 %.1f %%, rewritten %.1f %%", el, where, RoleName(c.role),
             c.winN / el, 100.0 * c.winSi1 / c.winN, 100.0 * c.winRw / c.winN);
        c.winN = c.winRw = c.winSi1 = 0;
    }
    if (g_eng.sameW || g_eng.sameS)
        PLOG("PresentMode: engine SyncInterval writes of an unchanged value: watcher %llu, setter %llu",
             (unsigned long long)g_eng.sameW, (unsigned long long)g_eng.sameS);
}

// ---- VrrPresent: per-swapchain capability ---------------------------------------------------------------------------
struct Chain {
    IDXGISwapChain* sc = nullptr;
    int64_t checked = 0, used = 0;
    UINT flags = 0;
    int can = -1;  // 1 = ALLOW_TEARING chain, not exclusive fullscreen
    bool refused = false, hzTried = false, overLogged = false;
    unsigned hz = 0;
    uint64_t winN = 0;
};
Chain g_chains[4];

Chain* ChainOf(IDXGISwapChain* sc, int64_t now) {
    Chain* e = nullptr;
    for (Chain& x : g_chains)
        if (x.sc == sc) e = &x;
    if (!e) {
        e = &g_chains[0];
        for (Chain& x : g_chains)
            if (x.used < e->used) e = &x;  // reuse the least recently used entry
        *e = Chain{};
        e->sc = sc;
    }
    e->used = now;
    ++e->winN;
    if (e->checked && Secs(now - e->checked) < 2.0) return e;
    // re-check every 2 s: the address can be a NEW chain (FG toggle / resize rebuild), fullscreen can change
    const double el = e->checked ? Secs(now - e->checked) : 0.0;
    if (el > 0 && e->hz && e->can == 1 && g_vrr && !e->overLogged && e->winN / el > e->hz * 1.03) {
        e->overLogged = true;
        PLOG("VrrPresent: swapchain %p presents %.0f/s, above the display's %u Hz: frames above the VRR range tear - keep "
             "the fps cap (in-game / [VRR] MaxFps) low enough that FG output stays below %u", (void*)sc, e->winN / el, e->hz,
             e->hz);
    }
    e->checked = now;
    e->winN = 0;
    DXGI_SWAP_CHAIN_DESC d{};
    BOOL fs = FALSE;
    const bool okDesc = SUCCEEDED(sc->GetDesc(&d));
    if (FAILED(sc->GetFullscreenState(&fs, nullptr))) fs = FALSE;
    const int can = okDesc && (d.Flags & kScAllowTearing) && !fs ? 1 : 0;
    if (!e->hzTried) {
        e->hzTried = true;
        IDXGIOutput* o = nullptr;
        if (SUCCEEDED(sc->GetContainingOutput(&o)) && o) {
            DXGI_OUTPUT_DESC od{};
            DEVMODEW dm{};
            dm.dmSize = sizeof(dm);
            if (SUCCEEDED(o->GetDesc(&od)) && EnumDisplaySettingsW(od.DeviceName, ENUM_CURRENT_SETTINGS, &dm))
                e->hz = dm.dmDisplayFrequency > 1 ? dm.dmDisplayFrequency : 0;
            o->Release();
        }
    }
    if (can != e->can || d.Flags != e->flags) {
        if (g_vrr)
            PLOG("VrrPresent: swapchain %p flags 0x%X, %s, display %u Hz: %s", (void*)sc, d.Flags,
                 fs ? "exclusive fullscreen" : "windowed/borderless", e->hz,
                 can ? "SyncInterval 0 + ALLOW_TEARING allowed"
                     : "left alone (created without ALLOW_TEARING, or exclusive fullscreen)");
        e->can = can;
        e->flags = d.Flags;
    }
    return e;
}

bool Wanted() {
    if (g_vrr == 1) return true;
    if (g_vrr == -1) return Slot(kRvaVsync) == 0;  // auto: the game's VSync is OFF -> tearing-allowed presents are the user's choice
    return false;
}

}  // namespace

void Start(const StartInfo& si) {
    g_log = si.log;
    g_base = si.base;
    g_known = si.knownBuild;
    g_wine = si.wine != nullptr;
    QueryPerformanceFrequency(&g_freq);
    if (si.iniPath) {
        g_logMode = std::clamp((int)GetPrivateProfileIntW(L"Debug", L"PresentModeLog", 1, si.iniPath), 0, 2);
        g_vrr = std::clamp((int)GetPrivateProfileIntW(L"Latency", L"VrrPresent", 0, si.iniPath), -1, 1);
    }
    const int ini = g_vrr;
    if (g_vrr && g_wine) {
        g_vrr = 0;
        PLOG("VrrPresent = %d: Proton/Wine (%s) - not applied (Windows only: vkd3d-proton turns SyncInterval 0 into an "
             "IMMEDIATE/MAILBOX Vulkan present and gamescope composites anyway; notes\\vrr-present.md)", ini, si.wine);
    } else if (g_vrr == -1 && !g_known) {
        g_vrr = 0;
        PLOG("VrrPresent = -1 (auto) needs the v2.02 VSync slot - off on this build");
    } else {
        PLOG("VrrPresent = %d (%s)", ini,
             g_vrr == 1 ? "on: SyncInterval 0 + ALLOW_TEARING on tearing-capable windowed swapchains"
             : g_vrr == -1 ? "auto: on while the game's VSync setting is OFF"
                           : "off: the game's own present arguments");
    }
    PLOG("PresentModeLog = %d%s", g_logMode, g_logMode ? " (one line per present-mode / engine SyncInterval change)" : "");
    g_started = true;
}

void SetWriterProbes(bool installed) { g_probes = installed; }

bool WantsPresentHook() { return g_started && (g_logMode != 0 || g_vrr != 0); }

bool Filter(IDXGISwapChain* sc, unsigned& sync, unsigned& flags, const void* caller, int role) {
    if (!g_started || !sc || (!g_logMode && !g_vrr)) return false;
    const unsigned inS = sync, inF = flags;
    bool rw = false;
    AcquireSRWLockExclusive(&g_lock);
    const int64_t now = Qpc();
    if (g_vrr) {
        Chain* e = ChainOf(sc, now);
        if (e->can == 1 && !e->refused && (inF & ~kKeepable) == 0 && (inS != 0 || !(inF & kTear)) && Wanted()) {
            sync = 0;
            flags = inF | kTear;
            rw = true;
        }
    }
    if (g_logMode) {
        EngineSample(now);
        NoteCaller(caller, role, inS, inF, sync, flags, rw, sc);
        Summary(now);
    }
    ReleaseSRWLockExclusive(&g_lock);
    return rw;
}

void Refused(IDXGISwapChain* sc, long hr) {
    AcquireSRWLockExclusive(&g_lock);
    for (Chain& x : g_chains)
        if (x.sc == sc && !x.refused) {
            x.refused = true;
            PLOG("VrrPresent: swapchain %p refused Present(0, ALLOW_TEARING) (hr %08lX) - it keeps the game's arguments",
                 (void*)sc, (unsigned long)hr);
        }
    ReleaseSRWLockExclusive(&g_lock);
}

}  // namespace pmode
