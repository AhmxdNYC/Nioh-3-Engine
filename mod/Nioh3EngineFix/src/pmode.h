// pmode.h - present mode log + [Latency] VrrPresent (1.7.44, notes\vrr-present.md).
//
// The game's Present SyncInterval is engine state (GfxDevice+424Ch) with three writers (ctor default 1, the display
// settings watcher = VSync slot, a setter reached through a pointer) plus a per-frame override (GfxDevicePresent params
// bit 0 -> 0). Which one wins differed per launch and mid-run, and SyncInterval 1 turns the PC's "independent flip"
// (VRR, tearing allowed) into "composed flip": ~+7 ms display latency and 8-20 % presented frames never shown.
//
// A  [Debug] PresentModeLog (default 1): from the bottom-most Present hook (osd.cpp, real DXGI), one log line per
//    change of what each caller presents with (SyncInterval, flags, what we sent), and one line per change of the
//    engine's SyncInterval with the writer that set it (PM group caves: watcher 1350210 / setter 2EEBB50 + its caller;
//    ctor 2EDFBBD by inference). 2 = also a 30 s count summary. Measurement only.
// B  [Latency] VrrPresent (default 0): 1 = present with SyncInterval 0 + DXGI_PRESENT_ALLOW_TEARING whenever the
//    swapchain was created with ALLOW_TEARING and is not exclusive fullscreen (the exact call the stock run made);
//    -1 = auto: only while the game's own VSync setting is OFF (then the user already chose tearing-allowed presents and
//    a stale engine SyncInterval 1 is a bug, not a choice). 0 = off. Windows only (refused under Proton/Wine).
//    The FG presenter's timing is not touched: only the two arguments of the Present call it already makes.
#pragma once
#include <cstdint>

struct IDXGISwapChain;

namespace pmode {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uint8_t* base = nullptr;   // Nioh3.exe image base
    bool knownBuild = false;   // v2.02: engine fields are read only then
    const char* wine = nullptr;
};

void Start(const StartInfo& si);          // reads the ini; call before osd::Start
void SetWriterProbes(bool installed);     // PM group (dllmain) applied
bool WantsPresentHook();                  // osd installs its Present hook for us too

// Outermost hooked Present/Present1 (not DXGI_PRESENT_TEST). May rewrite sync/flags; returns true if it did.
// role: 1 = the FidelityFX presenter thread, 0 = another thread, -1 = not known yet. caller = _ReturnAddress().
bool Filter(IDXGISwapChain* sc, unsigned& sync, unsigned& flags, const void* caller, int role);
// The rewritten call failed (hr): the swapchain is excluded from now on; the caller presents the original arguments.
void Refused(IDXGISwapChain* sc, long hr);

}  // namespace pmode
