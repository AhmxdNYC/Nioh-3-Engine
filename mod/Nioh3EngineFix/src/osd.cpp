// osd.cpp - in-game overlay for Nioh3EngineFix (see osd.h and notes\osd-design.md).
//
// How it works
//   1. A background thread waits until the engine runs frames (known build: frame counter G+0x610 advancing), then
//      creates a throw-away D3D12 device/queue/swapchain on a hidden window with the SYSTEM d3d12.dll / dxgi.dll
//      (Windows: the real DXGI; Proton: DXVK's dxgi + vkd3d-proton) to learn the swapchain class's vtable, releases
//      it again, and swaps vtable slots 8 (Present), 22 (Present1) and 38 (SetColorSpace1) for our hooks.
//      Every swapchain of that class shares the vtable, so the hook sees the game's bottom-most Present: with FSR
//      frame generation that is the FidelityFX presenter thread presenting real AND generated frames; without it,
//      the engine's present thread. Steam overlay / RTSS inline hooks stay in the chain (we call the original).
//   2. In the hook (outermost call only, never for DXGI_PRESENT_TEST) we count presents, sample the engine, and draw:
//      the swapchain's own queue (IDXGISwapChain::GetDevice(ID3D12CommandQueue)), one command list from a ring of 4
//      allocators, back buffer PRESENT -> RENDER_TARGET -> PRESENT, one instanced draw of glyph quads (font in a
//      constant buffer, shaders precompiled DXBC), ExecuteCommandLists + Signal, then the original Present.
//      No back-buffer reference is kept between frames, so the game's ResizeBuffers is never blocked.
//   3. Fail-safe: every call is checked; any failure disables drawing (the hook keeps passing through) and logs why.
//      Drawing runs under SEH. A busy lock or an unfinished GPU slot skips the overlay for that frame - never blocks.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include <intrin.h>
#pragma intrinsic(_ReturnAddress)
#include "gimerge.h"
#include "gpuopt.h"
#include "gpuprof.h"
#include "menu.h"
#include "padlat.h"
#include "pmode.h"
#include "upmirror.h"
#include "scenemirror.h"
#include "cbvstage.h"
#include "osd.h"
#include "psoswap.h"
#include "tilestats.h"
#include "samelook.h"
#include "livectl.h"
#include "osd_font.h"
#include "osd_shaders.h"

// diagnostics exported by caves.asm (VRR). vrr_scale stays 1.0 when the VRR group is not installed.
extern "C" {
extern float vrr_scale;
extern uint32_t vrr_fpsOut;
extern uint32_t vrr_tier;
extern uint32_t vrr_capCur;
}

namespace osd {
namespace {

// ---- engine addresses (Nioh 3 Steam v2.02 only; see notes\frame-timing.md, notes\settings.md) -----------------------
constexpr uint32_t kRvaFrameMgrPtr = 0x4759910;  // G = [base+4759910]
// G+610: +1 per game-update TICK (`add [rdi+610h],r12d` at 233B73, the end of the update tick 2338B0 called from 45A54;
// skipped while G+646h is 0). It is not tied to Present: when the render side presents fewer frames than the game
// updates (seen on the Ally: 54.6 ticks/s vs 35.8 presents/s with FG off), ticks > frames. So the counter shows the
// presented (rendered) rate as FPS and the tick rate only as a secondary "game Hz" when they differ.
constexpr uint32_t kOffFrameCounter = 0x610;
constexpr uint32_t kOffFpsForDt = 0x614;         // G+614: fps used for dt this frame
constexpr uint32_t kRvaRenFps = 0x45BAF54;       // REN_FPS index -> {30,60,120}
constexpr uint32_t kRvaDynamicFps = 0x45BAF58;   // DYNAMIC_FPS 0 = OFF, 1 = ON
constexpr uint32_t kRvaFrameGen = 0x45BB008;     // FRAME_GENERATION 0 OFF, 1 DLSS, 2 FSR, 3 XeSS
constexpr uint32_t kRvaUpscaler = 0x45BAFF8;     // RESOLUTION_SCALING 0 OFF, 1 DLSS, 2 FSR, 3 XESS

// ---- DXGI vtable slots (IDXGISwapChain4 single-inheritance layout) -------------------------------------------------
constexpr int kSlotPresent = 8, kSlotPresent1 = 22, kSlotSetColorSpace1 = 38;

constexpr int kRing = 4;          // command allocators / instance regions in flight
constexpr int kMaxInst = 4096;    // quads per frame (counter + popup + mod menu)
constexpr UINT kInstBytes = 24;   // float4 rect + uint glyph + uint color
constexpr UINT kFontCb = 6144;    // 95 glyphs * 16 rows * 4 B = 6080, rounded to 256
constexpr UINT kSlotBytes = kMaxInst * kInstBytes;
constexpr uint32_t kSolid = 0xFFFF;
static_assert(osdfont::kWords * 4 <= (int)kFontCb, "font does not fit the constant buffer");

using LogFn = void (*)(const char*, ...);
LogFn g_log = nullptr;
#define OLOG(...)                                \
    do {                                         \
        if (g_log) g_log("OSD: " __VA_ARGS__);   \
    } while (0)

using Settings = OverlaySettings;  // osd.h (shared with the mod menu)

struct Info {
    std::string version, wine, skippedNames;
    uint8_t* base = nullptr;
    bool known = false, vrr = false;
    int applied = 0, skipped = 0;
    State l1 = State::Off, f1 = State::Off;
};

Settings g_cfg;
Info g_info;
LARGE_INTEGER g_freq{};

// ---- hooks ----------------------------------------------------------------------------------------------------------
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT, const DXGI_PRESENT_PARAMETERS*);
using SetCs1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE);
PresentFn g_origPresent = nullptr;
Present1Fn g_origPresent1 = nullptr;
SetCs1Fn g_origSetCs1 = nullptr;
// Fallback queue source: some swapchain proxies (Streamline's sl.interposer on DXVK/vkd3d-proton under Proton) refuse
// IDXGISwapChain::GetDevice(ID3D12CommandQueue). Then the presenting queue is taken from the game's own submissions:
// the last DIRECT queue that called ExecuteCommandLists on the presenting thread (else on any thread). Pointers only,
// no references - BuildCtx AddRefs the one it picks, right after it submitted (it is alive then).
using EclFn = void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*, UINT, ID3D12CommandList* const*);
constexpr int kSlotExecuteCommandLists = 10;
EclFn g_origEcl = nullptr;
thread_local ID3D12CommandQueue* t_lastQ = nullptr;        // last queue seen on this thread (any type)
thread_local ID3D12CommandQueue* t_lastDirectQ = nullptr;  // last DIRECT queue that submitted on this thread
ID3D12CommandQueue* volatile g_lastDirectQ = nullptr;
thread_local int t_depth = 0;
volatile LONG g_disabled = 0;
volatile LONG g_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
SRWLOCK g_lock = SRWLOCK_INIT;

// ---- small helpers --------------------------------------------------------------------------------------------------
int64_t Qpc() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
double Secs(int64_t ticks) { return (double)ticks / (double)g_freq.QuadPart; }

template <class T>
bool Rd(const void* p, T& out) {
    if (!p) return false;
    __try {
        out = *static_cast<const volatile T*>(p);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint8_t* EngineG() {
    if (!g_info.known || !g_info.base) return nullptr;
    uint64_t g = 0;
    if (!Rd(g_info.base + kRvaFrameMgrPtr, g) || g < 0x10000) return nullptr;
    return reinterpret_cast<uint8_t*>((uintptr_t)g);
}

int ReadCfg(uint32_t rva) {
    int v = -1;
    if (g_info.known && g_info.base) Rd(g_info.base + rva, v);
    return v;
}

std::string ModuleOf(const void* p) {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)p, &m))
        return "?";
    wchar_t w[MAX_PATH] = {};
    GetModuleFileNameW(m, w, MAX_PATH);
    char a[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, w, -1, a, (int)sizeof(a), nullptr, nullptr);
    char buf[MAX_PATH * 2 + 32];
    snprintf(buf, sizeof(buf), "%s+%llX", a, (unsigned long long)((const uint8_t*)p - (const uint8_t*)m));
    return buf;
}

std::string ThreadName() {
    using GetDescFn = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    static GetDescFn fn = []() -> GetDescFn {
        HMODULE k = GetModuleHandleW(L"kernelbase.dll");
        if (!k) k = GetModuleHandleW(L"kernel32.dll");
        return k ? reinterpret_cast<GetDescFn>(GetProcAddress(k, "GetThreadDescription")) : nullptr;
    }();
    std::string out;
    PWSTR w = nullptr;
    if (fn && SUCCEEDED(fn(GetCurrentThread(), &w)) && w) {
        char a[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, w, -1, a, (int)sizeof(a), nullptr, nullptr);
        out = a;
        LocalFree(w);
    }
    return out;
}

const char* StateStr(State s) {
    switch (s) {
        case State::On: return "on";
        case State::WineSkipped: return "off (Wine)";
        case State::Failed: return "failed";
        default: return "off";
    }
}

// ---- statistics (all touched under g_lock from the present hook) ---------------------------------------------------
// Presents per swapchain, counted for EVERY hooked outermost Present (any thread, before the overlay lock). With FSR
// frame generation behind Streamline's interposer two swapchain objects present through the same vtable: the game
// thread presents its rendered frames to the FG proxy chain (our drawing chain), the "AMD FSR Presenter Thread" presents
// rendered + generated frames to the inner chain. Displayed fps = the busiest chain; rendered fps = the quietest active
// chain when two are active.
struct ScCount {
    IDXGISwapChain* volatile sc;
    volatile LONG64 n;
    volatile LONG64 last;
};
ScCount g_sc[4] = {};
void (*volatile g_rateSink)(double, double) = nullptr;  // TG governor (dllmain), set before Start
volatile DWORD g_fsrTid = 0;          // the FidelityFX presenter thread (by name), 0 = never seen
volatile LONG64 g_lastFsrPresent = 0;
volatile LONG g_fsrSeen = 0;
volatile LONG64 g_nGame = 0, g_nFsr = 0;  // presents per THREAD role (1.7.13): the game/render thread vs the FSR presenter thread
thread_local int t_isFsr = 0;        // this thread is the FidelityFX presenter (set by NotePresentThread)
thread_local int t_named = 0;        // this thread's presents were looked at (name logged / FSR presenter checked)

void CountPresent(IDXGISwapChain* sc, int64_t now) {
    for (ScCount& e : g_sc)
        if (e.sc == sc) {
            InterlockedIncrement64(&e.n);
            e.last = now;
            return;
        }
    for (ScCount& e : g_sc) {  // a free slot, or one whose chain went quiet (> 2 s)
        IDXGISwapChain* cur = e.sc;
        if (cur && Secs(now - e.last) <= 2.0) continue;
        if (InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&e.sc), sc, cur) == cur) {
            e.n = 1;
            e.last = now;
            return;
        }
    }
}

volatile DWORD g_gamePresentTid = 0;  // 1.7.58: the thread that presents the game's real frames (thrsched RTP / TSTATE)

void NotePresentThread(int64_t now) {
    const DWORD tid = GetCurrentThreadId();
    if (tid == g_fsrTid) g_lastFsrPresent = now;
    if (t_named) {
        if (!t_isFsr) g_gamePresentTid = tid;
        InterlockedIncrement64(t_isFsr ? &g_nFsr : &g_nGame);
        return;
    }
    t_named = 1;
    std::string name = ThreadName();
    if (name.find("FSR") != std::string::npos || name.find("FrameInterpolation") != std::string::npos) {
        g_fsrTid = tid;
        g_lastFsrPresent = now;
        InterlockedExchange(&g_fsrSeen, 1);
        t_isFsr = 1;
    }
    if (!t_isFsr) g_gamePresentTid = tid;
    InterlockedIncrement64(t_isFsr ? &g_nFsr : &g_nGame);
    OLOG("Present called from thread %lu \"%s\"", tid, name.c_str());
}

struct Stats {
    int64_t winStart = 0;
    uint32_t frame0 = 0;
    IDXGISwapChain* snapSc[4] = {};
    LONG64 snapN[4] = {};
    LONG64 snapGame = 0, snapFsr = 0;
    double dtSum = 0;
    int dtN = 0;
    bool haveRates = false, haveReal = false, fgActive = false;
    double realFps = 0, presentFps = 0, tickFps = 0, frameMs = 0, speed = 0, ratio = 0;
    int chains = 0;             // swapchains that presented in the last window
    double gameThreadFps = 0, presenterFps = 0;  // per-thread present rates of the last window
    int64_t lastLog = 0;
    uint64_t draws = 0, skips = 0;
};
Stats g_st;

// not reset with the swapchain: FG-setting change log and the menu's "switching" window
int g_fgLogged = -2, g_upLogged = -2;
int64_t g_fgSwitchAt = 0;

void SnapCounts(Stats& s) {
    for (int i = 0; i < 4; ++i) {
        s.snapSc[i] = g_sc[i].sc;
        s.snapN[i] = g_sc[i].n;
    }
    s.snapGame = g_nGame;
    s.snapFsr = g_nFsr;
}

void UpdateStats(int64_t now) {
    uint8_t* G = EngineG();
    if (G) {
        int fps = 0;
        if (Rd(G + kOffFpsForDt, fps) && fps > 0 && fps < 100000) {
            g_st.dtSum += (double)vrr_scale / (double)fps;  // game seconds advanced per engine frame
            ++g_st.dtN;
        }
    }
    if (g_st.winStart == 0) {
        g_st.winStart = now;
        SnapCounts(g_st);
        if (G) Rd(G + kOffFrameCounter, g_st.frame0);
        g_st.lastLog = now;
        return;
    }
    const double el = Secs(now - g_st.winStart);
    if (el < 0.5) return;
    uint32_t fr = 0;
    g_st.haveReal = G && Rd(G + kOffFrameCounter, fr);
    g_st.tickFps = g_st.haveReal ? (double)(uint32_t)(fr - g_st.frame0) / el : 0.0;
    g_st.frame0 = fr;
    // per-swapchain present rates over the window (a slot reassigned mid-window is ignored for this window)
    double hi = 0, lo = 0;
    int chains = 0;
    for (int i = 0; i < 4; ++i) {
        IDXGISwapChain* sc = g_sc[i].sc;
        if (!sc || sc != g_st.snapSc[i]) continue;
        const double r = (double)(g_sc[i].n - g_st.snapN[i]) / el;
        if (r < 1.0) continue;
        if (!chains || r > hi) hi = r;
        if (!chains || r < lo) lo = r;
        ++chains;
    }
    // per THREAD (1.7.13): on the PC the FSR presenter presents into the SAME swapchain, so per-chain counting sees one chain.
    // The game/render thread's presents are the real frames, the presenter thread's the displayed ones (real + generated).
    const double gameR = (double)(g_nGame - g_st.snapGame) / el, fsrR = (double)(g_nFsr - g_st.snapFsr) / el;
    SnapCounts(g_st);
    g_st.chains = chains;
    g_st.gameThreadFps = gameR;
    g_st.presenterFps = fsrR;
    g_st.presentFps = hi;  // displayed: the busiest chain (incl. generated frames)
    // rendered: the game's own chain when the FG presenter has its own one; with a single chain that carries
    // generated frames too (presented well above the tick rate), the engine tick rate is the best estimate
    const bool presenterRecent = g_lastFsrPresent && Secs(now - g_lastFsrPresent) < 1.0;
    double rendered = chains >= 2 ? lo : hi;
    if (chains < 2 && g_st.tickFps > 5 && hi > 1.4 * g_st.tickFps) rendered = g_st.tickFps;
    g_st.realFps = rendered;
    g_st.frameMs = rendered > 0.5 ? 1000.0 / rendered : 0.0;
    g_st.speed = g_st.dtN ? g_st.tickFps * g_st.dtSum / g_st.dtN : 0.0;  // game seconds per real second
    g_st.ratio = rendered > 0.5 ? hi / rendered : 0.0;
    // FG runs when the displayed rate is well above the rendered one; a present from the FSR presenter thread counts
    // as FG running unless no extra frames reach the screen (menus/cutscenes: the presenter passes frames through)
    const bool threadMode = fsrR >= 5.0 && gameR >= 5.0;
    if (threadMode) {  // both thread roles present: real = game thread, displayed = presenter thread
        rendered = gameR;
        g_st.realFps = gameR;
        g_st.presentFps = fsrR;
        g_st.frameMs = 1000.0 / gameR;
        g_st.ratio = fsrR / gameR;
        g_st.fgActive = g_st.ratio > (g_st.fgActive ? 1.08 : 1.15);
    } else if (presenterRecent && rendered > 5 && g_st.ratio >= 1.15) g_st.fgActive = true;
    else if (!g_st.fgActive && rendered > 5 && g_st.ratio > 1.4) g_st.fgActive = true;
    else if (g_st.fgActive && (g_st.ratio < 1.2 || rendered <= 5)) g_st.fgActive = false;
    g_st.haveRates = true;
    if (g_rateSink) g_rateSink(g_st.realFps, g_st.tickFps);  // TG governor (dllmain)
    if (g_info.known) {  // the game's FG / upscaler settings: logged at the first window and whenever they change
        const int fg = ReadCfg(kRvaFrameGen), up = ReadCfg(kRvaUpscaler);
        if (fg != g_fgLogged || up != g_upLogged) {
            const char* a = FgTypeName(fg);
            const char* b = FgTypeName(up);
            OLOG("game settings: FRAME_GENERATION %s (%d), RESOLUTION_SCALING %s (%d)%s", a ? a : "?", fg, b ? b : "?", up,
                 g_fgLogged == -2 ? "" : " - changed");
            g_fgLogged = fg;
            g_upLogged = up;
        }
    }
    {   // FG state transitions with the reason
        static int lastState = -1, lastReason = -1;
        const FgStatus fs = FrameGen();
        if (fs.state != lastState || (fs.state == kFgPaused && fs.pauseReason != lastReason)) {
            char txt[96];
            FgStateText(fs, txt, sizeof(txt));
            OLOG("FG state: %s [setting %d, game thread %.1f/s, presenter thread %.1f/s, ratio %.2f, pause reason %d]", txt, fs.setting,
                 g_st.gameThreadFps, g_st.presenterFps, g_st.ratio, fs.pauseReason);
            lastState = fs.state;
            lastReason = fs.pauseReason;
        }
    }
    g_st.winStart = now;
    g_st.dtSum = 0;
    g_st.dtN = 0;
    if (Secs(now - g_st.lastLog) >= 30.0) {
        g_st.lastLog = now;
        OLOG("stats: rendered %.1f fps, displayed %.1f fps (x%.2f, FG %s, %d chain(s)%s, threads game %.1f/s presenter %.1f/s), game ticks %.1f/s, speed %.2fx, "
             "vrr tier %u cap %u armed %u, draws %llu, skipped %llu",
             g_st.realFps, g_st.presentFps, g_st.ratio, g_st.fgActive ? "active" : "off", g_st.chains,
             presenterRecent ? ", FSR presenter" : "", g_st.gameThreadFps, g_st.presenterFps, g_st.tickFps, g_st.speed, vrr_tier, vrr_capCur, vrr_fpsOut,
             (unsigned long long)g_st.draws, (unsigned long long)g_st.skips);
    }
}

const char* FgName(int setting) {
    switch (setting) {
        case 1: return "DLSS";
        case 2: return "FSR";
        case 3: return "XeSS";
        default: return nullptr;
    }
}

const char* FgActiveName() {
    if (g_fsrSeen) return "FSR";
    if (const char* n = FgName(ReadCfg(kRvaFrameGen))) return n;
    if (GetModuleHandleW(L"libxess_fg.dll")) return "XeSS";
    if (GetModuleHandleW(L"sl.dlss_g.dll")) return "DLSS";
    return "external";
}

// ---- text layout ----------------------------------------------------------------------------------------------------
struct Inst {
    float x, y, w, h;
    uint32_t glyph, color;
};
static_assert(sizeof(Inst) == kInstBytes, "instance layout");
Inst g_inst[kMaxInst];
int g_n = 0;
int g_nMain = 0;          // quads [0, g_nMain) use the counter scale, [g_nMain, g_n) the menu scale
float g_menuScale = 0;    // text scale of the menu quads (0 = none this frame)

constexpr uint32_t kWhite = 0xFFFFFFFF, kGray = 0xFFB4B4B4, kGreen = 0xFF6CFF6C, kYellow = 0xFF40E6FF,
                   kOrange = 0xFF3C8CFF, kCyan = 0xFFFFE08C, kRed = 0xFF5050FF, kPanel = 0xA0000000;

uint32_t Fade(uint32_t c, float a) {
    uint32_t al = (uint32_t)std::clamp((float)(c >> 24) * a, 0.0f, 255.0f);
    return (c & 0x00FFFFFFu) | (al << 24);
}

struct Seg {
    char t[112];
    uint32_t c;
};
struct Line {
    Seg s[6];
    int n = 0;
    void add(uint32_t c, const char* fmt, ...) {
        if (n >= 6) return;
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(s[n].t, sizeof(s[n].t), fmt, ap);
        va_end(ap);
        s[n++].c = c;
    }
    size_t chars() const {
        size_t k = 0;
        for (int i = 0; i < n; ++i) k += strlen(s[i].t);
        return k;
    }
};

void AddQuad(float x, float y, float w, float h, uint32_t glyph, uint32_t color) {
    if (g_n >= kMaxInst) return;
    g_inst[g_n++] = {x, y, w, h, glyph, color};
}

float AddText(float x, float y, const char* t, uint32_t color, float sc) {
    for (; *t; ++t) {
        unsigned ch = (unsigned char)*t;
        if (ch < (unsigned)osdfont::kFirst || ch > (unsigned)osdfont::kLast) ch = '?';
        if (ch != ' ')
            AddQuad(x - sc, y - sc, (osdfont::kW + 2) * sc, (osdfont::kH + 2) * sc, ch - osdfont::kFirst, color);
        x += osdfont::kW * sc;
    }
    return x;
}

// draws a block of lines inside a translucent panel; returns the block height
float DrawBlock(const Line* lines, int count, float x, float y, bool alignRight, float sc, float alpha) {
    if (count <= 0) return 0;
    const float pad = 4 * sc, lineH = (osdfont::kH + 2) * sc;
    size_t maxChars = 0;
    for (int i = 0; i < count; ++i) maxChars = std::max(maxChars, lines[i].chars());
    const float w = (float)maxChars * osdfont::kW * sc + 2 * pad, h = count * lineH + 2 * pad - 2 * sc;
    if (alignRight) x -= w;
    AddQuad(x, y, w, h, kSolid, Fade(kPanel, alpha));
    for (int i = 0; i < count; ++i) {
        float cx = x + pad;
        for (int k = 0; k < lines[i].n; ++k) cx = AddText(cx, y + pad + i * lineH, lines[i].s[k].t, Fade(lines[i].s[k].c, alpha), sc);
    }
    return h;
}

int64_t g_firstDraw = 0;

// 1.7.56 one-time notice toast (rebar.cpp). The strings are written before the state goes 0 -> 1 (interlocked = barrier).
std::string g_noticeText[4];
int g_noticeLines = 0;
float g_noticeSecs = 25.0f;
volatile LONG g_noticeState = 0;   // 0 none, 1 pending (waits for the startup popup), 2 on screen, 3 done
int64_t g_noticeStart = 0;

float CounterScaleImpl(UINT H) {
    const float autoMul = (float)std::max(1u, (H + 540) / 1080);  // 1080p/1440p: 1, 4K: 2
    return std::clamp(g_cfg.scale * autoMul, 0.5f, 8.0f);
}

void BuildOverlay(UINT W, UINT H, int64_t now) {
    g_n = 0;
    g_nMain = 0;
    g_menuScale = 0;
    const float sc = CounterScaleImpl(H);
    const float k1080 = (float)H / 1080.0f;
    const float margin = 8 * sc;
    const float offX = (float)OffsetX() * k1080, offY = (float)OffsetY() * k1080;
    const bool right = g_cfg.pos == 1 || g_cfg.pos == 3, bottom = g_cfg.pos >= 2;

    Line counter;
    if (g_cfg.fpsCounter) {
        if (!g_st.haveRates) {
            counter.add(kWhite, "FPS: --");
        } else if (!g_st.haveReal) {
            counter.add(kWhite, "FPS: %.0f", g_st.presentFps);  // unknown build: presented frames only
        } else {
            // FPS = frames the game rendered and presented; FG FPS = frames shown incl. generated ones; "game N Hz" =
            // engine update ticks, shown only when they differ from the rendered rate (ticks without a Present)
            char tick[32] = "";
            if (g_st.tickFps > 0 && std::fabs(g_st.tickFps - g_st.realFps) > std::max(2.0, 0.1 * g_st.realFps))
                snprintf(tick, sizeof(tick), " (game %.0f Hz)", g_st.tickFps);
            counter.add(kWhite, "FPS: %.0f", g_st.realFps);
            if (g_st.fgActive) counter.add(kCyan, " FG");
            if (g_st.fgActive) counter.add(kCyan, " | FG FPS: %.0f", g_st.presentFps);
            else counter.add(kGray, " | FG: off%s", tick);
            if (g_st.fgActive && tick[0]) counter.add(kGray, "%s", tick);
            if (g_cfg.frameTime && g_st.frameMs > 0) counter.add(kWhite, " | %.1f ms", g_st.frameMs);
            if (g_cfg.speed && g_st.speed > 0) {
                const bool ok = g_st.speed > 0.97 && g_st.speed < 1.03;
                counter.add(ok ? kGreen : kYellow, " | %.2fx", g_st.speed);
            }
        }
    }

    Line pop[10];
    int np = 0;
    float popAlpha = 0;
    if (g_cfg.popup && g_firstDraw && !menu::IsOpen()) {
        const double t = Secs(now - g_firstDraw) - g_cfg.popupDelay;
        if (t >= 0 && t < g_cfg.popupSeconds) {
            popAlpha = (float)std::min({1.0, t / 0.3, (g_cfg.popupSeconds - t) / 1.5});
            const int total = g_info.applied + g_info.skipped;
            Line& h = pop[np++];
            h.add(g_info.skipped ? kYellow : kGreen, "Nioh3EngineFix v%s loaded - %d/%d fixes active", g_info.version.c_str(),
                  g_info.applied, total);
            if (g_info.skipped && !g_info.skippedNames.empty()) h.add(kYellow, " (skipped: %s)", g_info.skippedNames.c_str());
            if (!g_info.known) pop[np++].add(kYellow, "Unknown game build - engine readouts off");
            const FgStatus fs = FrameGen();
            Line& fg = pop[np++];
            fg.add(kWhite, "Frame generation: ");
            if (fs.setting == 0) {
                fg.add(kGray, "off in game settings");
            } else if (fs.active) {
                fg.add(kCyan, "%s active", FgActiveName());
                fg.add(kWhite, " (%.0f -> %.0f fps)", fs.realFps, fs.presentFps);
            } else if (FgName(fs.setting)) {
                if (fs.measured) fg.add(kYellow, "%s on in settings but NOT running", FgName(fs.setting));
                else fg.add(kWhite, "%s on in settings, measuring...", FgName(fs.setting));
            } else {
                fg.add(kWhite, !g_st.haveRates ? "measuring..." : !g_st.haveReal ? "unknown" : "off");
            }
            if (const char* up = fs.upscaler >= 0 ? FgTypeName(fs.upscaler) : nullptr) fg.add(kGray, " | upscaler %s", up);
            if (const char* why = FgReason(fs)) pop[np++].add(kOrange, "  %s", why);
            const int renIdx = ReadCfg(kRvaRenFps);
            static const int kTiers[3] = {30, 60, 120};
            const int cap = renIdx >= 0 && renIdx <= 2 ? kTiers[renIdx] : 0;
            Line& v = pop[np++];
            if (g_info.vrr) {
                v.add(kWhite, "VRR timing: ");
                v.add(kGreen, "on");
                if (cap) v.add(kWhite, " - %d fps cap (limiter %u)", cap, vrr_capCur);
                if (g_st.haveRates && g_st.speed > 0)
                    v.add(g_st.speed > 0.97 && g_st.speed < 1.03 ? kGreen : kYellow, ", game speed %.2fx", g_st.speed);
            } else {
                v.add(kWhite, "VRR timing: ");
                v.add(kGray, "off");
                if (cap) v.add(kWhite, " - %d fps cap (stock: slow-down below it)", cap);
            }
            if (ReadCfg(kRvaDynamicFps) == 1)
                pop[np++].add(kOrange, "WARNING: 'FPS dynamic adjustment' is ON - set it OFF");
            if (menu::Enabled()) pop[np++].add(kWhite, "Mod menu: %s", menu::OpenHint());
            Line& p = pop[np++];
            if (!g_info.wine.empty())
                p.add(kWhite, "Proton/Wine %s detected - L1 %s, F1 %s", g_info.wine.c_str(), StateStr(g_info.l1), StateStr(g_info.f1));
            else
                p.add(kWhite, "Windows - L1 %s, F1 %s", StateStr(g_info.l1), StateStr(g_info.f1));
        }
    }

    Line note[4];
    int nn = 0;
    float noteAlpha = 0;
    const LONG nst = g_noticeState;
    if (nst == 1 && g_firstDraw && !menu::IsOpen()) {
        const bool popupBusy = g_cfg.popup && Secs(now - g_firstDraw) < (double)g_cfg.popupDelay + g_cfg.popupSeconds;
        if (!popupBusy) {
            g_noticeStart = now;
            InterlockedExchange(&g_noticeState, 2);
        }
    } else if (nst == 2) {
        const double t = Secs(now - g_noticeStart);
        if (menu::IsOpen() || t >= g_noticeSecs) {
            InterlockedExchange(&g_noticeState, 3);
        } else {
            noteAlpha = (float)std::min({1.0, t / 0.3, ((double)g_noticeSecs - t) / 1.5});
            for (int i = 0; i < g_noticeLines && nn < 4; ++i)
                note[nn++].add(i == 0 ? kOrange : i == 1 ? kWhite : kGray, "%s", g_noticeText[i].c_str());
        }
    }

    // stack: counter first, popup next to it (below for top positions, above for bottom positions)
    const float x = right ? (float)W - margin - offX : margin + offX;
    const float gap = 4 * sc, lineH = (osdfont::kH + 2) * sc, pad = 4 * sc;
    float y = bottom ? (float)H - margin - offY : margin + offY;
    auto place = [&](const Line* l, int n, float a) {
        if (n <= 0) return;
        const float h = n * lineH + 2 * pad - 2 * sc;
        if (bottom) {
            y -= h;
            DrawBlock(l, n, x, y, right, sc, a);
            y -= gap;
        } else {
            DrawBlock(l, n, x, y, right, sc, a);
            y += h + gap;
        }
    };
    if (counter.n) place(&counter, 1, 1.0f);
    if (np && popAlpha > 0) place(pop, np, popAlpha);
    if (nn && noteAlpha > 0) place(note, nn, noteAlpha);
    g_nMain = g_n;
    g_menuScale = menu::Draw(W, H);  // mod menu / pause hint / hold indicator (menu.cpp), drawn at its own scale
    if (g_menuScale <= 0) g_n = g_nMain;
}

// ---- D3D12 drawing context (one per swapchain; rebuilt when the presenting swapchain changes) -----------------------
template <class T>
void SafeRelease(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

struct Ctx {
    IDXGISwapChain* sc = nullptr;    // identity only - NO reference held (would block the game's swapchain teardown)
    IDXGISwapChain3* sc3 = nullptr;  // same object, no reference held; only used inside its own Present
    ID3D12CommandQueue* q = nullptr;
    ID3D12Device* dev = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE ev = nullptr;
    uint64_t fenceVal = 0, slotVal[kRing] = {};
    ID3D12CommandAllocator* alloc[kRing] = {};
    ID3D12GraphicsCommandList* cl = nullptr;
    ID3D12DescriptorHeap* rtv = nullptr;
    UINT rtvInc = 0;
    ID3D12Resource* upload = nullptr;
    uint8_t* mapped = nullptr;
    ID3D12RootSignature* rs = nullptr;
    ID3D12PipelineState* pso = nullptr;
    DXGI_FORMAT psoFmt = DXGI_FORMAT_UNKNOWN;
    uint64_t frame = 0;
    int64_t lastPresent = 0;
    bool qFallback = false;  // q came from ExecuteCommandLists, not from the swapchain
    bool ok = false;
};
Ctx g_ctx;
IDXGISwapChain* g_bad[8] = {};
int g_badN = 0;
int g_ctxBuilds = 0;
bool g_needQueue = false;  // last BuildCtx failed only because no game DIRECT queue was captured yet
int g_queueWaits = 0;

// Only submissions made by the game itself (Nioh3.exe) or through Streamline's interposer count: the queues that FSR's
// frame-interpolation library (and other middleware) creates internally are DIRECT too, die with their swapchain, and
// crashed the game ~2 s after FG ON -> OFF under Proton (v9 log) when one of them was picked as the overlay queue.
bool CallerIsGame(const void* ra) {
    static uintptr_t exeLo = 0, exeHi = 0, slLo = 0, slHi = 0;
    static volatile LONG tries = 0;
    auto range = [](HMODULE m, uintptr_t& lo, uintptr_t& hi) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(m);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const uint8_t*>(m) + dos->e_lfanew);
        lo = (uintptr_t)m;
        hi = lo + nt->OptionalHeader.SizeOfImage;
    };
    if (!exeHi) {
        if (HMODULE m = GetModuleHandleW(nullptr)) range(m, exeLo, exeHi);
    }
    if (!slHi && (InterlockedIncrement(&tries) & 63) == 1) {  // sl.interposer may load late - look now and then
        if (HMODULE m = GetModuleHandleW(L"sl.interposer.dll")) range(m, slLo, slHi);
    }
    const uintptr_t a = (uintptr_t)ra;
    return (a >= exeLo && a < exeHi) || (slHi && a >= slLo && a < slHi);
}

void STDMETHODCALLTYPE HkExecuteCommandLists(ID3D12CommandQueue* q, UINT n, ID3D12CommandList* const* lists) {
    static thread_local bool lastIsDirect = false;
    tilestats::OnEcl(q, n, CallerIsGame(_ReturnAddress()));  // before g_origEcl for EVERY caller (TileBatch correctness)
    if (gp::g_gqHeads || gp::g_fcEcl) gp::OnEcl(q, n, (void* const*)lists, _ReturnAddress(), CallerIsGame(_ReturnAddress()));  // 1.7.40 GQ
    if (!CallerIsGame(_ReturnAddress())) {
        // 1.7.60-cbu2 CBF: the game's own submissions arrive here too when another hook sits in front of this one (RPF's
        // ExecuteCommandLists thunk: the return address is in the ASI); copying staged constants early is always safe
        if (cbvstage::OnEcl(q, n, lists, reinterpret_cast<cbvstage::EclFn>(g_origEcl))) return;
        g_origEcl(q, n, lists);
        return;
    }
    if (q != t_lastQ) {  // GetDesc only when this thread switches queues (a few times per frame at most)
        t_lastQ = q;
        lastIsDirect = q->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT;
    }
    if (lastIsDirect) {
        t_lastDirectQ = q;
        g_lastDirectQ = q;
    }
    scenemirror::OnEcl(q);  // 1.7.57 SBM: this queue waits for the latest scene-buffer copy (once per copy)
    // 1.7.60-cbu2 CBF: staged constants copied to the VRAM ring first (prologue mode submits the game lists itself)
    if (cbvstage::OnEcl(q, n, lists, reinterpret_cast<cbvstage::EclFn>(g_origEcl))) return;
    // 1.7.52/54 CM: mirror copies first; the inline mode submits a prologue list + the game's lists itself (true)
    if (upmirror::OnEcl(q, n, lists, reinterpret_cast<upmirror::EclFn>(g_origEcl))) return;
    g_origEcl(q, n, lists);
}

// The game's presenting queue when the swapchain won't say: the DIRECT queue that last submitted on this (the
// presenting) thread, else on any thread, accepted only if it belongs to the device that owns the back buffers.
// Returns an AddRef'd queue, or nullptr.
ID3D12CommandQueue* CapturedQueue(IDXGISwapChain* sc) {
    if (!g_origEcl) return nullptr;
    ID3D12CommandQueue* q = t_lastDirectQ ? t_lastDirectQ : g_lastDirectQ;
    if (!q) return nullptr;
    q->AddRef();
    ID3D12Device *qDev = nullptr, *bufDev = nullptr;
    ID3D12Resource* buf = nullptr;
    q->GetDevice(IID_PPV_ARGS(&qDev));
    if (SUCCEEDED(sc->GetBuffer(0, IID_PPV_ARGS(&buf))) && buf) buf->GetDevice(IID_PPV_ARGS(&bufDev));
    const bool match = qDev && (!bufDev || bufDev == qDev);
    OLOG("captured DIRECT queue %p (%s, %s thread), device %p (%s); back buffer device %p (%s) -> %s", (void*)q,
         ModuleOf(*(void**)q).c_str(), q == t_lastDirectQ ? "presenting" : "other", (void*)qDev,
         qDev ? ModuleOf(*(void**)qDev).c_str() : "-", (void*)bufDev, bufDev ? ModuleOf(*(void**)bufDev).c_str() : "-",
         match ? (bufDev ? "same device, using it" : "back buffer device unknown, using it") : "DIFFERENT device, not used");
    if (bufDev) bufDev->Release();
    if (buf) buf->Release();
    if (qDev) qDev->Release();
    if (!match) {
        q->Release();
        return nullptr;
    }
    return q;
}

bool IsBad(IDXGISwapChain* sc) {
    for (int i = 0; i < g_badN; ++i)
        if (g_bad[i] == sc) return true;
    return false;
}
void MarkBad(IDXGISwapChain* sc) {
    static int next = 0;
    g_bad[next] = sc;
    next = (next + 1) % 8;
    g_badN = std::min(g_badN + 1, 8);
}

void WaitIdle(Ctx& c, DWORD ms) {
    if (!c.fence || !c.ev || c.fence->GetCompletedValue() >= c.fenceVal) return;
    if (SUCCEEDED(c.fence->SetEventOnCompletion(c.fenceVal, c.ev))) WaitForSingleObject(c.ev, ms);
}

void DestroyCtx(Ctx& c) {
    WaitIdle(c, 1500);  // the old context's last submission must be done before anything it uses is released
    if (c.upload && c.mapped) c.upload->Unmap(0, nullptr);
    SafeRelease(c.pso);
    SafeRelease(c.rs);
    SafeRelease(c.upload);
    SafeRelease(c.rtv);
    SafeRelease(c.cl);
    for (auto& a : c.alloc) SafeRelease(a);
    SafeRelease(c.fence);
    if (c.ev) CloseHandle(c.ev);
    SafeRelease(c.dev);
    SafeRelease(c.q);
    c = Ctx{};
}

#define CHECK(expr, what)                                                                     \
    do {                                                                                      \
        HRESULT hr_ = (expr);                                                                 \
        if (FAILED(hr_)) {                                                                    \
            OLOG("%s failed (hr %08lX) - overlay off for this swapchain", what, (unsigned long)hr_); \
            return false;                                                             \
        }                                                                                     \
    } while (0)

bool BuildCtx(Ctx& c, IDXGISwapChain* sc) {
    c.sc = sc;
    ++g_ctxBuilds;
    IDXGISwapChain3* sc3 = nullptr;
    CHECK(sc->QueryInterface(IID_PPV_ARGS(&sc3)), "QueryInterface(IDXGISwapChain3)");
    c.sc3 = sc3;
    sc3->Release();  // keep the raw pointer only
    g_needQueue = false;
    HRESULT qhr = sc->GetDevice(IID_PPV_ARGS(&c.q));
    if (FAILED(qhr) || !c.q) {
        c.q = CapturedQueue(sc);
        if (!c.q) {
            g_needQueue = (g_origEcl != nullptr);
            if (!g_needQueue || g_queueWaits == 0)
                OLOG("IDXGISwapChain::GetDevice(ID3D12CommandQueue) failed (hr %08lX) and %s", (unsigned long)qhr,
                     g_origEcl ? "no usable game queue captured yet - retrying for up to 600 frames"
                               : "ExecuteCommandLists isn't hooked - overlay off for this swapchain");
            return false;
        }
        c.qFallback = true;
        OLOG("IDXGISwapChain::GetDevice(ID3D12CommandQueue) failed (hr %08lX, swapchain %s) - drawing on the game's "
             "captured DIRECT queue instead", (unsigned long)qhr, ModuleOf(*(void**)sc).c_str());
    }
    const D3D12_COMMAND_QUEUE_DESC qd = c.q->GetDesc();
    if (qd.Type != D3D12_COMMAND_LIST_TYPE_DIRECT) {
        OLOG("swapchain queue is type %d, not DIRECT - overlay off for this swapchain", (int)qd.Type);
        return false;
    }
    CHECK(c.q->GetDevice(IID_PPV_ARGS(&c.dev)), "ID3D12CommandQueue::GetDevice");
    DXGI_SWAP_CHAIN_DESC sd{};
    sc->GetDesc(&sd);
    OLOG("swapchain %p: %ux%u fmt %d, %u buffers, swap effect %d, flags %X; queue %p (%s), device %p", (void*)sc,
         sd.BufferDesc.Width, sd.BufferDesc.Height, (int)sd.BufferDesc.Format, sd.BufferCount, (int)sd.SwapEffect,
         sd.Flags, (void*)c.q, ModuleOf(*(void**)c.q).c_str(), (void*)c.dev);

    CHECK(c.dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&c.fence)), "CreateFence");
    c.ev = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!c.ev) { OLOG("CreateEvent failed"); return false; }
    for (auto& a : c.alloc) CHECK(c.dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&a)), "CreateCommandAllocator");
    CHECK(c.dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, c.alloc[0], nullptr, IID_PPV_ARGS(&c.cl)), "CreateCommandList");
    CHECK(c.cl->Close(), "CommandList::Close");

    D3D12_DESCRIPTOR_HEAP_DESC hd{};
    hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    hd.NumDescriptors = kRing;
    CHECK(c.dev->CreateDescriptorHeap(&hd, IID_PPV_ARGS(&c.rtv)), "CreateDescriptorHeap(RTV)");
    c.rtvInc = c.dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_HEAP_PROPERTIES hp{};
    hp.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC rd{};
    rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    rd.Width = kFontCb + (UINT64)kRing * kSlotBytes;
    rd.Height = 1;
    rd.DepthOrArraySize = 1;
    rd.MipLevels = 1;
    rd.SampleDesc.Count = 1;
    rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    CHECK(c.dev->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                         IID_PPV_ARGS(&c.upload)),
          "CreateCommittedResource(upload)");
    D3D12_RANGE none{0, 0};
    void* m = nullptr;
    CHECK(c.upload->Map(0, &none, &m), "Map(upload)");
    c.mapped = (uint8_t*)m;
    memset(c.mapped, 0, kFontCb);
    memcpy(c.mapped, osdfont::kBits, osdfont::kWords * 4);

    using SerializeFn = HRESULT(WINAPI*)(const D3D12_ROOT_SIGNATURE_DESC*, D3D_ROOT_SIGNATURE_VERSION, ID3DBlob**, ID3DBlob**);
    HMODULE d3d12 = GetModuleHandleW(L"d3d12.dll");
    auto serialize = d3d12 ? reinterpret_cast<SerializeFn>(GetProcAddress(d3d12, "D3D12SerializeRootSignature")) : nullptr;
    if (!serialize) { OLOG("D3D12SerializeRootSignature not found"); return false; }
    D3D12_ROOT_PARAMETER rp[2]{};
    rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rp[0].Descriptor.ShaderRegister = 0;
    rp[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rp[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rp[1].Constants.ShaderRegister = 1;
    rp[1].Constants.Num32BitValues = 4;
    rp[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    D3D12_ROOT_SIGNATURE_DESC rsd{};
    rsd.NumParameters = 2;
    rsd.pParameters = rp;
    rsd.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob* blob = nullptr;
    ID3DBlob* err = nullptr;
    HRESULT hr = serialize(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &err);
    if (FAILED(hr)) {
        OLOG("D3D12SerializeRootSignature failed (hr %08lX) %s", (unsigned long)hr,
             err ? (const char*)err->GetBufferPointer() : "");
        SafeRelease(err);
        return false;
    }
    SafeRelease(err);
    hr = c.dev->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&c.rs));
    SafeRelease(blob);
    CHECK(hr, "CreateRootSignature");
    c.ok = true;
    OLOG("drawing context ready (build #%d)", g_ctxBuilds);
    return true;
}

bool EnsurePso(Ctx& c, DXGI_FORMAT fmt) {
    if (c.pso && c.psoFmt == fmt) return true;
    switch (fmt) {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: break;
        default: OLOG("back buffer format %d not supported - overlay off for this swapchain", (int)fmt); return false;
    }
    if (c.pso) {
        WaitIdle(c, 500);  // in-flight lists still reference the old PSO
        SafeRelease(c.pso);
    }
    static const D3D12_INPUT_ELEMENT_DESC il[3] = {
        {"RECT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"GLYPH", 0, DXGI_FORMAT_R32_UINT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
        {"COLOR", 0, DXGI_FORMAT_R32_UINT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1},
    };
    D3D12_GRAPHICS_PIPELINE_STATE_DESC d{};
    d.pRootSignature = c.rs;
    d.VS = {kOsdVS, sizeof(kOsdVS)};
    d.PS = {kOsdPS, sizeof(kOsdPS)};
    D3D12_RENDER_TARGET_BLEND_DESC& b = d.BlendState.RenderTarget[0];
    b.BlendEnable = TRUE;
    b.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    b.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    b.BlendOp = D3D12_BLEND_OP_ADD;
    b.SrcBlendAlpha = D3D12_BLEND_ZERO;
    b.DestBlendAlpha = D3D12_BLEND_ONE;
    b.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    b.LogicOp = D3D12_LOGIC_OP_NOOP;
    b.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    d.SampleMask = UINT_MAX;
    d.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    d.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    d.RasterizerState.DepthClipEnable = TRUE;
    d.DepthStencilState.DepthEnable = FALSE;
    d.DepthStencilState.StencilEnable = FALSE;
    d.InputLayout = {il, 3};
    d.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    d.NumRenderTargets = 1;
    d.RTVFormats[0] = fmt;
    d.SampleDesc.Count = 1;
    CHECK(c.dev->CreateGraphicsPipelineState(&d, IID_PPV_ARGS(&c.pso)), "CreateGraphicsPipelineState");
    c.psoFmt = fmt;
    OLOG("pipeline created for back buffer format %d", (int)fmt);
    return true;
}

// returns true when something was drawn
bool DrawFrame(Ctx& c, int64_t now) {
    const UINT idx = c.sc3->GetCurrentBackBufferIndex();
    ID3D12Resource* bb = nullptr;
    if (FAILED(c.sc->GetBuffer(idx, IID_PPV_ARGS(&bb))) || !bb) {
        OLOG("GetBuffer(%u) failed - overlay off for this swapchain", idx);
        c.ok = false;
        return false;
    }
    const D3D12_RESOURCE_DESC bd = bb->GetDesc();
    const UINT W = (UINT)bd.Width, H = bd.Height;
    bool drawn = false;
    do {
        if (W < 64 || H < 64) break;
        if (!EnsurePso(c, bd.Format)) { c.ok = false; break; }
        BuildOverlay(W, H, now);
        if (g_n == 0) break;
        const int slot = (int)(c.frame % kRing);
        if (c.fence->GetCompletedValue() < c.slotVal[slot]) {
            if (FAILED(c.fence->SetEventOnCompletion(c.slotVal[slot], c.ev)) ||
                WaitForSingleObject(c.ev, 50) != WAIT_OBJECT_0) {
                ++g_st.skips;  // GPU far behind: skip the overlay this frame rather than stall the presenter
                break;
            }
        }
        const UINT64 off = kFontCb + (UINT64)slot * kSlotBytes;
        memcpy(c.mapped + off, g_inst, (size_t)g_n * kInstBytes);
        if (FAILED(c.alloc[slot]->Reset()) || FAILED(c.cl->Reset(c.alloc[slot], c.pso))) {
            OLOG("command allocator/list reset failed - overlay off");
            c.ok = false;
            break;
        }
        D3D12_RESOURCE_BARRIER br{};
        br.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        br.Transition.pResource = bb;
        br.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        br.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        br.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        c.cl->ResourceBarrier(1, &br);
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = c.rtv->GetCPUDescriptorHandleForHeapStart();
        rtv.ptr += (SIZE_T)slot * c.rtvInc;
        c.dev->CreateRenderTargetView(bb, nullptr, rtv);
        c.cl->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        D3D12_VIEWPORT vp{0, 0, (float)W, (float)H, 0, 1};
        D3D12_RECT sr{0, 0, (LONG)W, (LONG)H};
        c.cl->RSSetViewports(1, &vp);
        c.cl->RSSetScissorRects(1, &sr);
        c.cl->SetGraphicsRootSignature(c.rs);
        const D3D12_GPU_VIRTUAL_ADDRESS va = c.upload->GetGPUVirtualAddress();
        c.cl->SetGraphicsRootConstantBufferView(0, va);
        float bright = 1.0f;
        if (bd.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) bright = 2.5f;  // scRGB: 1.0 = 80 nits
        else if (bd.Format == DXGI_FORMAT_R10G10B10A2_UNORM && g_colorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
            bright = 0.55f;  // HDR10 PQ: keep text near 150-200 nits instead of 10000
        // the pixel shader derives font texels from one scale constant, so the counter and the menu are two draws
        float consts[4] = {1.0f / W, 1.0f / H, CounterScaleImpl(H), bright * g_cfg.brightness};
        c.cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        D3D12_VERTEX_BUFFER_VIEW vbv{va + off, (UINT)g_n * kInstBytes, kInstBytes};
        c.cl->IASetVertexBuffers(0, 1, &vbv);
        if (g_nMain > 0) {
            c.cl->SetGraphicsRoot32BitConstants(1, 4, consts, 0);
            c.cl->DrawInstanced(4, (UINT)g_nMain, 0, 0);
        }
        if (g_n > g_nMain) {
            consts[2] = g_menuScale;
            c.cl->SetGraphicsRoot32BitConstants(1, 4, consts, 0);
            c.cl->DrawInstanced(4, (UINT)(g_n - g_nMain), 0, (UINT)g_nMain);
        }
        std::swap(br.Transition.StateBefore, br.Transition.StateAfter);
        c.cl->ResourceBarrier(1, &br);
        if (FAILED(c.cl->Close())) {
            OLOG("command list Close failed - overlay off");
            c.ok = false;
            break;
        }
        ID3D12CommandList* lists[1] = {c.cl};
        c.q->ExecuteCommandLists(1, lists);
        if (FAILED(c.q->Signal(c.fence, ++c.fenceVal))) {
            OLOG("queue Signal failed (device removed?) - overlay off");
            c.ok = false;
            break;
        }
        c.slotVal[slot] = c.fenceVal;
        ++c.frame;
        drawn = true;
    } while (false);
    bb->Release();
    return drawn;
}

// ---- [Latency] MaxFrameLatency: IDXGISwapChain2::SetMaximumFrameLatency on every presenting swapchain that was created
// with DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT (the game's FG chain: flags 0x842). Re-checked every 2 s
// and on a setting change, so swapchain rebuilds (FG toggle, resize) get it again; 0 restores the game's own value.
struct LatEntry {
    IDXGISwapChain* sc = nullptr;
    int64_t checked = 0;
    int want = -1, game = 0, cur = 0, applied = 0, state = 0;
};
LatEntry g_lat[4];
IDXGISwapChain* g_latLast = nullptr;  // the swapchain the menu reports

void ApplyLatency(IDXGISwapChain* sc, int64_t now) {
    LatEntry* e = nullptr;
    for (LatEntry& x : g_lat)
        if (x.sc == sc) e = &x;
    const bool fresh = e == nullptr;
    if (fresh) {
        e = &g_lat[0];
        for (LatEntry& x : g_lat)
            if (x.checked < e->checked) e = &x;  // reuse the least recently checked entry
        *e = LatEntry{};
        e->sc = sc;
    }
    g_latLast = sc;
    const int want = g_cfg.maxLatency;
    if (!fresh && e->want == want && Secs(now - e->checked) < 2.0) return;
    const bool wantChanged = e->want != want;
    e->checked = now;
    e->want = want;
    const bool wasNa = e->state == 2;  // not applicable last time: re-checked quietly (the address may be a new chain)
    IDXGISwapChain2* s2 = nullptr;
    if (FAILED(sc->QueryInterface(IID_PPV_ARGS(&s2))) || !s2) {
        if (want && !wasNa) OLOG("MaxFrameLatency: swapchain %p has no IDXGISwapChain2 - not applicable", (void*)sc);
        e->state = 2;
        return;
    }
    DXGI_SWAP_CHAIN_DESC1 d{};
    if (FAILED(s2->GetDesc1(&d)) || !(d.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT)) {
        if (!wasNa) OLOG("MaxFrameLatency: swapchain %p (flags %X) was not created with FRAME_LATENCY_WAITABLE_OBJECT - not applicable%s",
             (void*)sc, d.Flags, want ? "" : " (setting is 0 anyway)");
        e->state = 2;
        s2->Release();
        return;
    }
    UINT cur = 0;
    if (FAILED(s2->GetMaximumFrameLatency(&cur))) cur = 0;
    if (fresh || wasNa || (int)cur != e->applied) {  // first look, or the game (or a rebuilt chain) changed it: that is its value
        if (!fresh) OLOG("MaxFrameLatency: swapchain %p value is now %u (set by the game)", (void*)sc, cur);
        else OLOG("MaxFrameLatency: swapchain %p (flags %X) - the game's maximum frame latency is %u", (void*)sc, d.Flags, cur);
        e->game = (int)cur;
    }
    const int target = want ? want : e->game;
    if (target > 0 && (int)cur != target) {
        const HRESULT hr = s2->SetMaximumFrameLatency((UINT)target);
        OLOG("MaxFrameLatency: swapchain %p %u -> %d (%s)%s", (void*)sc, cur, target, want ? "setting" : "game's value restored",
             SUCCEEDED(hr) ? "" : " FAILED");
        if (SUCCEEDED(hr)) cur = (UINT)target;
    } else if (wantChanged && !fresh) {
        OLOG("MaxFrameLatency: swapchain %p stays %u", (void*)sc, cur);
    }
    e->applied = (int)cur;
    e->cur = (int)cur;
    e->state = 1;
    s2->Release();
}

void OnPresent(IDXGISwapChain* sc) {
    const int64_t now = Qpc();
    if (sc != g_ctx.sc) {
        if (IsBad(sc)) return;
        // another swapchain is presenting: switch only when the current one went quiet (e.g. FG toggled -> new chain)
        if (g_ctx.sc && Secs(now - g_ctx.lastPresent) < 1.0) return;
        if (g_ctx.sc) OLOG("presenting swapchain changed %p -> %p", (void*)g_ctx.sc, (void*)sc);
        DestroyCtx(g_ctx);
        g_st = Stats{};
        if (g_ctxBuilds >= 16) {
            OLOG("too many swapchain switches - overlay disabled");
            InterlockedExchange(&g_disabled, 1);
            return;
        }
        if (!BuildCtx(g_ctx, sc)) {
            DestroyCtx(g_ctx);
            if (g_needQueue && ++g_queueWaits < 600) {
                --g_ctxBuilds;  // no game submission seen yet - try again next frame (not a failure)
                return;
            }
            MarkBad(sc);
            return;
        }
    }
    if (!g_ctx.ok) return;
    // the same address can be a NEW swapchain (old one released, e.g. frame generation toggled): its queue tells.
    // Not checkable when the queue came from ExecuteCommandLists (several DIRECT queues may take turns there).
    ID3D12CommandQueue* q = g_ctx.q;
    if (!g_ctx.qFallback) {
        q = nullptr;
        if (FAILED(sc->GetDevice(IID_PPV_ARGS(&q))) || !q) return;
        q->Release();
    }
    if (q != g_ctx.q) {
        OLOG("swapchain %p now presents on another queue (%p -> %p) - rebuilding", (void*)sc, (void*)g_ctx.q, (void*)q);
        DestroyCtx(g_ctx);
        g_st = Stats{};
        if (g_ctxBuilds >= 16 || !BuildCtx(g_ctx, sc)) {
            DestroyCtx(g_ctx);
            MarkBad(sc);
            return;
        }
    }
    g_ctx.lastPresent = now;
    padlat::OnPresent(g_ctx.q, now);  // 1.7.35 PadLatency: present stamp + GPU-done fence (no-op when off)
    gpuopt::OnFrame();  // once per frame of the drawing (game) swapchain
    gimerge::OnFrame(); // 1.7.29 GIProbe / GIMerge frame counter
    tilestats::OnFrame();
    samelook::OnPresent();  // 1.7.49 VSMA: calls per frame in the stats line
    livectl::OnFrame();     // 1.7.53 fast testing: the A/B windows' frame clock (no-op unless [Debug] LiveCtl = 1)
    gp::OnFrame();      // GP profiler: reads the timestamps of 6 frames ago
    if (!g_firstDraw) g_firstDraw = now;  // popup clock starts at the first hooked game frame
    UpdateStats(now);
    menu::OnPresent();  // menu input + runtime switches (needs a working drawing context, checked above)
    static bool loggedFirst = false;
    if (DrawFrame(g_ctx, now)) {
        if (!loggedFirst) {
            loggedFirst = true;
            OLOG("first overlay frame drawn (%d quads)", g_n);
        }
        ++g_st.draws;
    }
}

void OnPresentSafe(IDXGISwapChain* sc) {
    __try {
        OnPresent(sc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_disabled, 1);
        if (g_log) g_log("OSD: exception %08lX while drawing - overlay disabled for this session", GetExceptionCode());
    }
}

void Hooked(IDXGISwapChain* sc, UINT flags) {
    if (t_depth != 1 || (flags & DXGI_PRESENT_TEST) || g_disabled) return;
    const int64_t now = Qpc();
    CountPresent(sc, now);  // every swapchain, every presenting thread (FG presenter included), lock-free
    NotePresentThread(now);
    if (!TryAcquireSRWLockExclusive(&g_lock)) return;  // another thread is drawing: just present
    __try {
        ApplyLatency(sc, now);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    OnPresentSafe(sc);
    ReleaseSRWLockExclusive(&g_lock);
}

// 1.7.44 pmode (present mode log + [Latency] VrrPresent): outermost call only, never DXGI_PRESENT_TEST. Thread role from
// NotePresentThread (1 = FidelityFX presenter, 0 = other, -1 = not looked at yet).
bool PresentArgs(IDXGISwapChain* sc, UINT& sync, UINT& flags, const void* caller) {
    if (t_depth != 1 || (flags & DXGI_PRESENT_TEST)) return false;
    return pmode::Filter(sc, sync, flags, caller, t_named ? t_isFsr : -1);
}

HRESULT STDMETHODCALLTYPE HkPresent(IDXGISwapChain* sc, UINT sync, UINT flags) {
    ++t_depth;
    Hooked(sc, flags);
    UINT s2 = sync, f2 = flags;
    const bool rw = PresentArgs(sc, s2, f2, _ReturnAddress());
    HRESULT hr = g_origPresent(sc, s2, f2);
    if (rw && hr == DXGI_ERROR_INVALID_CALL) {  // validation refused the rewrite: nothing was presented - present as the game asked
        pmode::Refused(sc, hr);
        hr = g_origPresent(sc, sync, flags);
    }
    --t_depth;
    return hr;
}

HRESULT STDMETHODCALLTYPE HkPresent1(IDXGISwapChain1* sc, UINT sync, UINT flags, const DXGI_PRESENT_PARAMETERS* p) {
    ++t_depth;
    Hooked(sc, flags);
    UINT s2 = sync, f2 = flags;
    const bool rw = PresentArgs(sc, s2, f2, _ReturnAddress());
    HRESULT hr = g_origPresent1(sc, s2, f2, p);
    if (rw && hr == DXGI_ERROR_INVALID_CALL) {
        pmode::Refused(sc, hr);
        hr = g_origPresent1(sc, sync, flags, p);
    }
    --t_depth;
    return hr;
}

HRESULT STDMETHODCALLTYPE HkSetColorSpace1(IDXGISwapChain3* sc, DXGI_COLOR_SPACE_TYPE cs) {
    HRESULT hr = g_origSetCs1(sc, cs);
    if (SUCCEEDED(hr)) {
        if (g_colorSpace != (LONG)cs) OLOG("swapchain %p color space -> %d", (void*)sc, (int)cs);
        InterlockedExchange(&g_colorSpace, (LONG)cs);
    }
    return hr;
}

// ---- hook installation ---------------------------------------------------------------------------------------------
bool PatchSlot(void** slot, void* hook, void** orig, const char* name) {
    if (*slot == hook) return true;
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
        OLOG("VirtualProtect(vtable %s) failed (%lu)", name, GetLastError());
        return false;
    }
    *orig = *slot;  // published before the slot so the hook never sees a null original
    MemoryBarrier();
    InterlockedExchangePointer(slot, hook);
    DWORD t;
    VirtualProtect(slot, sizeof(void*), old, &t);
    OLOG("hooked %s: vtable slot %p, original %s", name, (void*)slot, ModuleOf(*orig).c_str());
    return true;
}

LRESULT CALLBACK DummyWndProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }

bool InstallHooks() {
    wchar_t sys[MAX_PATH] = {};
    UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (!n || n > MAX_PATH - 16) { OLOG("GetSystemDirectory failed"); return false; }
    const std::wstring d3d12Path = std::wstring(sys) + L"\\d3d12.dll", dxgiPath = std::wstring(sys) + L"\\dxgi.dll";
    // full paths: the bottom-most (system) DXGI, even if a wrapper dxgi.dll (OptiScaler, ReShade...) sits next to the exe
    HMODULE d3d12 = LoadLibraryW(d3d12Path.c_str());
    HMODULE dxgi = LoadLibraryW(dxgiPath.c_str());
    if (!d3d12 || !dxgi) { OLOG("loading system d3d12.dll/dxgi.dll failed (%lu)", GetLastError()); return false; }
    OLOG("system d3d12 %s, dxgi %s", ModuleOf(d3d12).c_str(), ModuleOf(dxgi).c_str());
    auto createDevice = reinterpret_cast<PFN_D3D12_CREATE_DEVICE>(GetProcAddress(d3d12, "D3D12CreateDevice"));
    using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
    auto createFactory = reinterpret_cast<CreateFactoryFn>(GetProcAddress(dxgi, "CreateDXGIFactory1"));
    if (!createDevice || !createFactory) { OLOG("D3D12CreateDevice/CreateDXGIFactory1 not exported"); return false; }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DummyWndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"Nioh3EngineFixOsdDummy";
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) { OLOG("dummy window creation failed (%lu)", GetLastError()); return false; }

    bool ok = false;
    IDXGIFactory2* factory = nullptr;
    ID3D12Device* dev = nullptr;
    ID3D12CommandQueue* q = nullptr;
    IDXGISwapChain1* sc = nullptr;
    do {
        HRESULT hr = createFactory(IID_PPV_ARGS(&factory));
        if (FAILED(hr)) { OLOG("CreateDXGIFactory1 failed (hr %08lX)", (unsigned long)hr); break; }
        // D3D12 devices are per-adapter singletons: this returns the game's device (or vkd3d-proton's cached one)
        hr = createDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dev));
        if (FAILED(hr)) { OLOG("D3D12CreateDevice failed (hr %08lX)", (unsigned long)hr); break; }
        D3D12_COMMAND_QUEUE_DESC qd{};
        qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        hr = dev->CreateCommandQueue(&qd, IID_PPV_ARGS(&q));
        if (FAILED(hr)) { OLOG("CreateCommandQueue failed (hr %08lX)", (unsigned long)hr); break; }
        DXGI_SWAP_CHAIN_DESC1 sd{};
        sd.Width = 64;
        sd.Height = 64;
        sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.Scaling = DXGI_SCALING_STRETCH;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        hr = factory->CreateSwapChainForHwnd(q, hwnd, &sd, nullptr, nullptr, &sc);
        if (FAILED(hr)) { OLOG("dummy CreateSwapChainForHwnd failed (hr %08lX)", (unsigned long)hr); break; }
        void** vt = *reinterpret_cast<void***>(sc);
        void** qvt = *reinterpret_cast<void***>(q);
        OLOG("dummy swapchain %p, vtable %s; dummy queue vtable %s", (void*)sc, ModuleOf(vt).c_str(), ModuleOf(qvt).c_str());
        // release the dummy objects before patching, so none of their calls go through our hooks
        sc->Release();
        sc = nullptr;
        // queue first: by the time Present is hooked the game's queue is being captured (fallback for proxy swapchains)
        if (!PatchSlot(&qvt[kSlotExecuteCommandLists], (void*)&HkExecuteCommandLists, (void**)&g_origEcl, "ExecuteCommandLists"))
            g_origEcl = nullptr;
        if (tilestats::WantsHooks()) tilestats::InstallOnQueueVtable(qvt, /*eclViaOsd=*/g_origEcl != nullptr);
        bool a = PatchSlot(&vt[kSlotPresent], (void*)&HkPresent, (void**)&g_origPresent, "Present");
        bool b = PatchSlot(&vt[kSlotPresent1], (void*)&HkPresent1, (void**)&g_origPresent1, "Present1");
        if (!PatchSlot(&vt[kSlotSetColorSpace1], (void*)&HkSetColorSpace1, (void**)&g_origSetCs1, "SetColorSpace1"))
            OLOG("SetColorSpace1 not hooked - HDR10 text brightness is not adjusted");
        ok = a || b;
    } while (false);
    SafeRelease(sc);
    SafeRelease(q);
    SafeRelease(dev);
    SafeRelease(factory);
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return ok;
}

bool InstallGpuOptSafe() {
    __try {
        return gpuopt::InstallNow();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        OLOG("gpuopt: exception %08lX in InstallNow - gpuopt off for this session", GetExceptionCode());
        return false;
    }
}

DWORD WINAPI InstallThread(LPVOID) {
    const DWORD t0 = GetTickCount();
    if (g_info.known) {
        // wait until the engine renders: G exists and its frame counter advances (device + swapchain exist by then)
        uint32_t first = 0;
        bool haveFirst = false, running = false;
        while (GetTickCount() - t0 < 300000) {
            uint8_t* G = EngineG();
            uint32_t fc = 0;
            if (G && Rd(G + kOffFrameCounter, fc)) {
                if (!haveFirst) first = fc, haveFirst = true;
                else if (fc - first >= 30) { running = true; break; }
            }
            Sleep(100);
        }
        if (running) OLOG("engine is rendering (%lu ms after start) - installing the Present hook", GetTickCount() - t0);
        else OLOG("engine frame counter did not advance within %lu ms - installing the Present hook anyway", GetTickCount() - t0);
    } else {
        while (GetTickCount() - t0 < 120000 && !(GetModuleHandleW(L"d3d12.dll") && GetModuleHandleW(L"dxgi.dll"))) Sleep(250);
        Sleep(10000);
        OLOG("unknown build: installing the Present hook %lu ms after start (no engine readouts)", GetTickCount() - t0);
    }
    if (!InstallHooks()) {
        OLOG("Present hook NOT installed - no overlay this session (the patches are unaffected)");
        menu::Disable("the overlay's Present hook could not be installed");
    } else if (menu::Enabled()) {
        menu::InstallInputHooks();
    }
    // GPU barrier batching (gpuopt.cpp): install its command-list hooks now that the engine renders (it installs by
    // itself later otherwise). A failure only disables gpuopt; the overlay and the patches go on.
    if (gpuopt::GetState() != gpuopt::State::Off && !InstallGpuOptSafe())
        OLOG("gpuopt: hooks not installed (see the GPUOPT: lines) - the overlay and the patches are unaffected");
    // PSO bytecode swap / dump: late path, only does something when the IAT hook missed the engine's device
    if (psoswap::GetState() != psoswap::State::Off) psoswap::InstallNow();
    return 0;
}

float IniFloat(const wchar_t* ini, const wchar_t* sec, const wchar_t* key, float def) {
    wchar_t b[64] = {};
    GetPrivateProfileStringW(sec, key, L"", b, 64, ini);
    if (!b[0]) return def;
    wchar_t* e = nullptr;
    double v = wcstod(b, &e);
    return e == b ? def : (float)v;
}

}  // namespace

OverlaySettings& Overlay() { return g_cfg; }

void SetRateSink(void (*fn)(double rendered, double ticks)) { g_rateSink = fn; }

unsigned long GamePresentTid() { return g_gamePresentTid; }

int64_t LastPresentQpc() {
    int64_t last = 0;
    for (const ScCount& e : g_sc) {
        const int64_t t = e.last;
        if (e.sc && t > last) last = t;
    }
    return last;
}

void ShowNotice(const char* const* lines, int n, float seconds) {
    if (g_noticeState != 0 || !lines || n <= 0) return;
    n = std::min(n, 4);
    for (int i = 0; i < n; ++i) g_noticeText[i] = lines[i] ? lines[i] : "";
    g_noticeLines = n;
    g_noticeSecs = std::clamp(seconds, 3.0f, 120.0f);
    InterlockedCompareExchange(&g_noticeState, 1, 0);
}

bool NoticeDone() { return g_noticeState == 3; }
LatencyStatus Latency() {
    LatencyStatus s;
    s.want = g_cfg.maxLatency;
    // prefer the drawing chain (the game's own), else the swapchain that presented last
    const LatEntry* e = nullptr;
    for (const LatEntry& x : g_lat)
        if (x.sc && x.sc == g_ctx.sc && x.state) e = &x;
    if (!e)
        for (const LatEntry& x : g_lat)
            if (x.sc && x.sc == g_latLast) e = &x;
    if (!e) return s;
    // a waitable chain anywhere counts as applicable (with FG the presenter's inner chain may not be waitable)
    for (const LatEntry& x : g_lat)
        if (x.sc && x.state == 1 && e->state != 1) e = &x;
    s.game = e->game;
    s.cur = e->cur;
    s.state = e->state;
    return s;
}
int DefaultOffsetY(int pos) { return pos == 0 ? 160 : 0; }
int OffsetX() { return g_cfg.offUser ? g_cfg.offX : 0; }
int OffsetY() { return g_cfg.offUser ? g_cfg.offY : DefaultOffsetY(g_cfg.pos); }

const char* PositionName(int pos) {
    static const char* const k[4] = {"TopLeft", "TopRight", "BottomLeft", "BottomRight"};
    return k[pos & 3];
}
float CounterScale(unsigned H) { return CounterScaleImpl(H); }
void DrawRect(float x, float y, float w, float h, uint32_t abgr) { AddQuad(x, y, w, h, kSolid, abgr); }
float DrawText(float x, float y, const char* text, uint32_t abgr, float scale) { return AddText(x, y, text, abgr, scale); }

const char* FgTypeName(int v) {
    switch (v) {
        case 0: return "OFF";
        case 1: return "DLSS";
        case 2: return "FSR";
        case 3: return "XeSS";
        default: return nullptr;
    }
}

FgStatus FrameGen() {
    FgStatus s;
    const int64_t now = Qpc();
    if (g_info.known) {
        s.setting = ReadCfg(kRvaFrameGen);
        s.upscaler = ReadCfg(kRvaUpscaler);
        if (!FgTypeName(s.setting)) s.setting = -1;
        if (!FgTypeName(s.upscaler)) s.upscaler = -1;
    }
    s.measured = g_st.haveRates && g_st.haveReal;
    s.active = g_st.fgActive;
    s.presenter = g_fsrTid && g_lastFsrPresent && Secs(now - g_lastFsrPresent) < 2.0;
    s.realFps = g_st.realFps;
    s.presentFps = g_st.presentFps;
    s.gameThreadFps = g_st.gameThreadFps;
    s.presenterFps = g_st.presenterFps;
    s.pauseReason = menu::GamePauseReason();
    s.tickFps = g_st.tickFps;
    if (g_fgSwitchAt) s.sinceSwitch = Secs(now - g_fgSwitchAt);
    if (s.setting < 0) s.state = kFgUnknown;
    else if (s.setting == 0) s.state = kFgOff;
    else if (s.active) s.state = kFgRunning;
    else if (!s.measured || s.sinceSwitch < 12.0) s.state = kFgStarting;
    else if (s.pauseReason || s.presenter) s.state = kFgPaused;  // presenter alive, no extra frames: menu / loading / cutscene
    else s.state = kFgNotRunning;
    return s;
}

int FgStateText(const FgStatus& s, char* buf, size_t n) {
    switch (s.state) {
        case kFgOff: snprintf(buf, n, "OFF"); return 0;
        case kFgRunning: snprintf(buf, n, "ON - running (%.0f -> %.0f fps)", s.realFps, s.presentFps); return 1;
        case kFgPaused:
            snprintf(buf, n, "ON - paused (%s)", s.pauseReason == 1 ? "game menu open" : s.pauseReason == 2 ? "game paused"
                                                : s.pauseReason == 3 ? "loading" : "menu/loading/cutscene");
            return 2;
        case kFgNotRunning: snprintf(buf, n, "ON - NOT running (no presenter thread)"); return 3;
        case kFgStarting: snprintf(buf, n, "ON - %s", s.measured ? "switching..." : "measuring..."); return 3;
        default: snprintf(buf, n, "unknown build"); return 0;
    }
}

// The setting is only the request: the engine builds the FG swapchain on its display reset (notes\frame-generation.md).
const char* FgReason(const FgStatus& s) {
    if (s.setting <= 0) return nullptr;
    if (s.sinceSwitch < 12.0 && !s.active) return "switching - the game is rebuilding its swapchain (a few seconds)";
    if (!s.measured || s.active) return nullptr;
    if (s.presenter) return "FSR presenter runs but adds no frames (menu, cutscene or loading screen?)";
    switch (s.setting) {
        case 2:
            return g_info.wine.empty() ? "no FSR presenter thread: the FG swapchain was not created - see the log"
                                       : "no FSR presenter thread: FG swapchain not created (Proton/vkd3d?) - see the log";
        case 1: return "DLSS FG selected but no generated frames reach the screen - try FSR";
        default: return "XeSS FG selected but no generated frames reach the screen - try FSR";
    }
}

void NoteFgSwitch() { g_fgSwitchAt = Qpc(); }

void Start(const StartupInfo& si) {
    g_log = si.log;
    QueryPerformanceFrequency(&g_freq);
    g_info.version = si.version ? si.version : "";
    g_info.wine = si.wine ? si.wine : "";
    g_info.skippedNames = si.skippedNames ? si.skippedNames : "";
    while (!g_info.skippedNames.empty() && g_info.skippedNames.back() == ' ') g_info.skippedNames.pop_back();
    g_info.base = si.base;
    g_info.known = si.knownBuild;
    g_info.vrr = si.vrr;
    g_info.applied = si.applied;
    g_info.skipped = si.skipped;
    g_info.l1 = si.l1;
    g_info.f1 = si.f1;
    if (si.iniPath) {
        const wchar_t* f = si.iniPath;
        auto I = [&](const wchar_t* sec, const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, f); };
        g_cfg.fpsCounter = I(L"Overlay", L"FpsCounter", g_cfg.fpsCounter);
        g_cfg.frameTime = I(L"Overlay", L"FrameTime", g_cfg.frameTime);
        g_cfg.speed = I(L"Overlay", L"SpeedFactor", g_cfg.speed);
        g_cfg.popup = I(L"General", L"StartupPopup", g_cfg.popup);
        g_cfg.scale = std::clamp(IniFloat(f, L"Overlay", L"Scale", g_cfg.scale), 0.5f, 6.0f);
        g_cfg.brightness = std::clamp(IniFloat(f, L"Overlay", L"Brightness", g_cfg.brightness), 0.1f, 4.0f);
        g_cfg.popupSeconds = std::clamp(IniFloat(f, L"General", L"StartupPopupSeconds", g_cfg.popupSeconds), 2.0f, 60.0f);
        g_cfg.popupDelay = std::clamp(IniFloat(f, L"General", L"StartupPopupDelay", g_cfg.popupDelay), 0.0f, 60.0f);
        wchar_t pos[32] = {};
        GetPrivateProfileStringW(L"Overlay", L"Position", L"TopLeft", pos, 32, f);
        g_cfg.pos = !_wcsicmp(pos, L"TopRight") ? 1 : !_wcsicmp(pos, L"BottomLeft") ? 2 : !_wcsicmp(pos, L"BottomRight") ? 3 : 0;
        {  // -1 = auto by OS: 1 under Wine/Proton (lowest latency, explicit under vkd3d), 0 on native Windows (the game's 1 is already lowest)
            int ml = I(L"Latency", L"MaxFrameLatency", 0);
            g_cfg.maxLatencyAuto = ml < 0;
            if (ml < 0) {
                HMODULE nt = GetModuleHandleW(L"ntdll.dll");
                const bool wn = nt && GetProcAddress(nt, "wine_get_version");
                ml = wn ? 1 : 0;
                OLOG("auto MaxFrameLatency = %d (%s)", ml, wn ? "Proton/Wine" : "native Windows");
            }
            g_cfg.maxLatency = std::clamp(ml, 0, 16);
        }
        const int ox = I(L"Overlay", L"OffsetX", -99999), oy = I(L"Overlay", L"OffsetY", -99999);
        if (ox != -99999 || oy != -99999) {  // pixels at 1080p; a missing key keeps the corner default
            g_cfg.offUser = true;
            g_cfg.offX = std::clamp(ox == -99999 ? 0 : ox, 0, 1900);
            g_cfg.offY = std::clamp(oy == -99999 ? DefaultOffsetY(g_cfg.pos) : oy, 0, 1060);
        }
    }
    if (g_cfg.maxLatency) OLOG("MaxFrameLatency=%d (applied to waitable swapchains as they present)", g_cfg.maxLatency);
    OLOG("FpsCounter=%d FrameTime=%d SpeedFactor=%d StartupPopup=%d Position=%d Scale=%.2f Offset=%d,%d (px at 1080p)", g_cfg.fpsCounter,
         g_cfg.frameTime, g_cfg.speed, g_cfg.popup, g_cfg.pos, g_cfg.scale, OffsetX(), OffsetY());
    if (!g_cfg.fpsCounter && !g_cfg.popup && !menu::Enabled() && !padlat::WantsPresentHook() && !pmode::WantsPresentHook() &&
        !upmirror::WantsPresentHook() && !scenemirror::Wanted() && !cbvstage::WantsEclHook()) {
        OLOG("overlay off in ini ([Overlay] FpsCounter=0, [General] StartupPopup=0, [Menu] Enabled=0, [Debug] PresentModeLog=0, "
             "[Latency] VrrPresent=0) - no Present hook");
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr)) CloseHandle(h);
    else OLOG("CreateThread failed (%lu)", GetLastError());
}

}  // namespace osd
