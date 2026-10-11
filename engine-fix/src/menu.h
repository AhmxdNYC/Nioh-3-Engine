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
                   kExpDQ, kExpP1E7,                // main-loop retry quantum, texture-loop prefetch (1.7.32 caves; rows 1.7.39)
                   kExpMA,                          // main-thread locked atomic loads -> plain loads (1.7.39, code bytes)
                   kExpBPO,                         // BoundsFromPartObbs quaternion memo / ready loads / copy (1.7.40, live)
                   kExpW1B, kExpRPA,                // glyph padding clear stores, RenderParamsAssign range copy (1.7.42, live)
                   kExpAS,                          // engine async-compute slots (1.7.44, boot slots + live producer switch)
                   kExpPJH,                         // PSO hand-over from the PJ prewarm (2026-10-09, live switch; psojournal.h)
                   kExpTR,                          // timer resolution held once (1.7.45, IAT hooks + live switch)
                   kExpVSMA, kExpLPVR, kExpSKYR, kExpFOGT, kExpCLDR,  // same-look package (1.7.49, samelook.cpp, live switches)
                   kExpRTP,                         // render threads at a higher scheduling priority (1.7.58, thrsched.cpp, live)
                   kExpPCM,                         // graphics-PSO lookup memo on the replay thread (1.7.58, psomemo.cpp, live)
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
    // 1.7.44 FSR FG under Proton ([Menu] FrameGenToggle = 3, notesg-proton-fix.md)
    volatile uint32_t* fgPause = nullptr;         // caves.asm fgp_pause (1 = FG paused by the mod); nullptr = FGP not installed
    bool fgcOn = false;                           // FGC copy-job null guard written this session
};

// dllmain: copies the host description and reads [Menu]. Cheap; installs nothing.
void Configure(const Host& h);
// Proton only (FG crash-loop guard, see menu.cpp). Early: called by dllmain's worker before the game reads config.xml;
// restores FRAME_GENERATION OFF when the last session left the mod's FG marker. Exit: called from the ExitProcess /
// TerminateProcess import hooks (or DLL detach); restores OFF when the mod switched FG on this session.
// fgToggle = [Menu] FrameGenToggle. 3 ("Proton safe FG"): a launch with FG on in config.xml arms the marker before the
// game reads it (a crash in that session -> OFF at the next start), and a clean exit keeps the FG value the user chose.
void ProtonFgEarly(void (*log)(const char* fmt, ...), const wchar_t* markerPath, int fgToggle);
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

// ---- remote control (vpad.cpp pipe thread, 1.7.37): get (set = 0) or set one item by key, executed on the Present thread
// exactly like a menu press (same 'MENU: <label> -> <value>' log line). Keys: the item's tech id without parentheses / spaces
// (GF W6 MQ PP B VRR W3 R1 L1 F1 WK GPU R2a AutoContinueLoad PauseHint WorkerCap ...), the [Patches] ini key of an experimental
// item (WakeStats), or FPS / FrameTime / Speed. Bool items take 0/1, R1 0-16, maxlatency 0-3, PauseHint 0-2. Frame generation
// and Save are not remote. Returns 0 ok (reply = "<label> = <value>"), < 0 error (reply says why).
int Remote(const char* key, int set, int value, char* reply, size_t n, unsigned timeoutMs);
// vpad.cpp: filter for the game's 256-byte DirectInput keyboard reads, called before the menu looks at them (true = report
// success to the game). Set before osd::Start.
void SetKeyFilter(bool (*filter)(uint8_t* keys, bool readOk));

// ---- W3 hookPeek (game's message thread): swallow the menu key (keeps Windows from entering the F10 system-menu
// mode) and, while the menu is open, the navigation keys. Only WM_KEY*/WM_SYSKEY* are looked at.
bool SwallowMessage(const tagMSG* m);

}  // namespace menu
