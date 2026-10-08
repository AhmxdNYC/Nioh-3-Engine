// menu.h - in-game mod menu of Nioh3EngineFix (keyboard F10 / controller hold-Start), drawn by the OSD (osd.cpp).
//
// Threads:
//   * dllmain's Worker calls Configure() once, after the patches are applied.
//   * osd's install thread calls InstallInputHooks() once the Present hook is in place.
//   * the game's input thread runs our XInputGetState IAT hook and the DirectInput GetDeviceState hook: they only
//     record button edges, detect the open gesture and blank the game's view of the pad / navigation keys while the
//     menu is open (plus until the buttons are released after closing).
//   * the Present hook thread (osd.cpp, under the osd lock) runs OnPresent() + Draw(): all menu state changes and
//     every runtime switch write happen there.
// Fail-safe: any exception in a hook disables the menu (the hooks keep passing through), and the input hooks
// stop blanking by themselves when the overlay has not presented for 1.5 s.
// See notes\osd-design.md section 10.
#pragma once
#include <cstddef>
#include <cstdint>

struct tagMSG;

namespace menu {

enum class FixState : uint8_t { Installed, NotInstalled, WineSkipped };

// experimental CPU-power patches (ini default 0), in menu order
enum ExpId : int { kExpW6, kExpB, kExpPP, kExpFPC, kExpAABB, kExpM1, kExpX1, kExpE1, kExpM2, kExpJ1, kExpS1,
                   kExpIK, kExpTS,  // input latency (1.2.0)
                   kExpWS, kExpED,  // worker wakes (1.3.0)
                   kExpGF,          // GPU-fence waits (1.4.0)
                   kExpWL,          // waitable-handle leak fix (1.5.0)
                   kExpWG, kExpTG, kExpDS,          // wake gate, tick governor, retry-spin sleep (1.6.0)
                   kExpLK, kExpCP, kExpSPF,         // worker jobs: locked loads, collision / stage prefetch (1.6.0)
                   kExpMQ, kExpPT1, kExpDL,         // matrix build SSE, texture pool lock, dead collision lookup (1.7.12)
                   kNumExp };
struct ExpFix {
    volatile uint32_t* enable = nullptr;  // live switch; nullptr = not installed this session or code-only (next launch)
    int ini = 0;                          // ini value at launch
    bool applied = false;                 // written this session
};

struct Host {
    const char* version = "";
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;             // copied
    uint8_t* base = nullptr;                      // Nioh3.exe
    bool knownBuild = false;                      // v2.02: engine reads (pause flags) only then
    bool wine = false;
    // live switches (nullptr = the fix is not installed this session)
    volatile uint32_t* vrrEnable = nullptr;       // caves.asm vrr_enable
    volatile long* w3Enable = nullptr;            // g_w3Enable
    volatile long* w3UseSleep = nullptr;          // g_w3UseSleep (Wine sleep mode)
    volatile uint32_t* r1Timeout = nullptr;       // near page r1Timeout (ms, 0 = stock poll)
    uint32_t r1Default = 2;                       // [Tuning] R1_TimeoutMs at launch
    volatile long* l1Enable = nullptr;            // g_l1Enable
    volatile long* fgEnable = nullptr;            // g_fgEnable
    volatile uint32_t* wkEnable = nullptr;        // caves.asm wk_enable
    FixState l1 = FixState::NotInstalled, f1 = FixState::NotInstalled;
    // counters (stats line)
    volatile long* w3Count = nullptr;
    volatile long* l1Count = nullptr;
    volatile long* fgCount = nullptr;
    volatile uint32_t* wkKept = nullptr;
    volatile uint32_t* wkSkipped = nullptr;
    // boot-time values the game was launched with ("applies next launch" items)
    int workerCap = 0, p12 = 0, l1OnWine = 0, fgOnWine = 0;
    // experimental patches + their counters (stats line)
    ExpFix exp[kNumExp];
    volatile uint32_t *ppRan = nullptr, *ppSkipped = nullptr, *m1Refresh = nullptr, *x1Skip = nullptr;
    volatile uint64_t *fpcHits = nullptr, *fpcMisses = nullptr, *e1Upd = nullptr, *e1Skip = nullptr;
    volatile long *w6Waits = nullptr, *w6Sleeps = nullptr;
    volatile long long* w6Ticks = nullptr;
    volatile long* tsInDispUs = nullptr;  // TS: input -> display median (us), -1 = no data yet
    const char* (*gpuStatus)(char* buf, size_t n) = nullptr;  // cached gpuopt status line (dllmain refreshes it)
    volatile long *gfWaits = nullptr, *gfFallbacks = nullptr;  // GF counters
    volatile long long* gfTicks = nullptr;
    volatile long long* wgSkipped = nullptr;      // WG skipped wakes
    volatile uint32_t* tgCap = nullptr;           // TG limiter cap (0 = stock)
    volatile long* tgState = nullptr;             // TG state (0 off, 1 governing, 2 refused: L1, 3 VRR not armed, 4 idle)
    volatile long* dsWaits = nullptr;             // DS timer sleeps
    const char* (*gpStatus)(char* buf, size_t n) = nullptr;  // GP profiler: frame ms + top 5 passes (empty = off)
    volatile uint8_t* autoContinue = nullptr;     // autocont.cpp live switch (1 = skip the post-load tip page's press); nullptr = not installed
    long (*autoContinueCount)() = nullptr;        // times the tip page's confirm check was answered by the switch
};

// dllmain: copies the host description and reads [Menu]. Cheap; installs nothing.
void Configure(const Host& h);
// Proton only (FG crash-loop guard, see menu.cpp). Early: called by dllmain's worker before the game reads config.xml;
// restores FRAME_GENERATION OFF when the last session left the mod's FG marker. Exit: called from the ExitProcess /
// TerminateProcess import hooks (or DLL detach); restores OFF when the mod switched FG on this session.
void ProtonFgEarly(void (*log)(const char* fmt, ...), const wchar_t* markerPath);
void ProtonFgExit(const char* how);
// true when [Menu] Enabled=1 (the OSD then installs its Present hook even with the counter and popup off)
bool Enabled();
// osd install thread, after the Present hook is installed: hooks XInputGetState (game IAT) and DirectInput
// GetDeviceState (shared vtable); falls back to own polling for whatever could not be hooked.
void InstallInputHooks();
// osd: the overlay could not be installed - the menu stays off
void Disable(const char* why);

// ---- Present-hook thread only (under the osd lock) ----
// input processing, gesture handling, runtime switch writes; once per hooked Present that is about to draw
void OnPresent();
// appends the menu / pause hint / hold indicator quads through osd::DrawRect/DrawText. Returns the text scale the
// quads were laid out for (0 = nothing drawn); osd draws them with that scale.
float Draw(unsigned W, unsigned H);
bool IsOpen();
// why the game is paused right now (any thread; set by the Present thread): 0 none, 1 a game menu is open (the engine's own
// "menu open" getters 0x5A37D4 / 0xD63C0), 2 game time stopped (stop flags 58B / 58C), 3 loading (58A)
int GamePauseReason();
// "F10 / Esc x3 / Start x3": all ways to open the menu (startup popup, pause hint)
const char* OpenHint();

// ---- W3 hookPeek (game's message thread): swallow the menu key (keeps Windows from entering the F10 system-menu
// mode) and, while the menu is open, the navigation keys. Only WM_KEY*/WM_SYSKEY* are looked at.
bool SwallowMessage(const tagMSG* m);

}  // namespace menu
