// vpad.h - VirtualPad + KeepRunningUnfocused ([Debug], both 0 by default, 1.7.37). Test-harness input without OS-level input:
// see notes\virtual-input.md.
//   * VirtualPad = 1: the game's XInputGetState import (IAT, like the mod menu / PadLatency) answers slot 0 with a scripted pad
//     and slots 1-3 with "not connected" (the user's real pads are hidden from the game for the session). VirtualPad = 2: merge -
//     real pads keep working, the virtual buttons / sticks / triggers are mixed into slot 0. Commands come over the named pipe
//     \\.\pipe\nioh3vpad (local clients only), one text line each, one reply line each ("ok ..." / "err ..."): press / hold /
//     release / stick / trigger / key / menu get|set (mod menu items set directly on the Present thread, no navigation) / state.
//     Virtual keys go through the mod menu's DirectInput keyboard hook (needs [Menu] Enabled = 1); while the game is not the
//     foreground window the real keyboard state is hidden from it (typing into another window never reaches the game).
//   * KeepRunningUnfocused = 1 (v2.02): the game ignores pads while its window is inactive (WM_ACTIVATEAPP -> GameMain+1ABh,
//     IsAppActive 436CF8) and opens its pause menu on focus loss when its 'pause in background' option is set. Three call sites
//     of IsAppActive (pad update 436758, pad slot read 436A20, focus-loss pause 89B2FD) are retargeted to a thunk that answers
//     "active" while the switch is on (al + flags only, like the original). The mouse / keyboard / cursor callers keep the real
//     answer: an unfocused game never reads the user's mouse buttons or moves the cursor. Also opts the process out of Windows
//     power throttling (EcoQoS + timer-resolution coarsening of occluded windows) so unfocused timing equals focused timing.
#pragma once
#include <cstddef>
#include <cstdint>

namespace vpad {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uint8_t* base = nullptr;   // Nioh3.exe image base
    bool knownBuild = false;
    const char* version = "";
    // mod menu: get (set = 0) or set an item by its key; returns 0 ok, < 0 error; reply = one line (menu.cpp menu::Remote)
    int (*menuRemote)(const char* key, int set, int value, char* reply, size_t n, unsigned timeoutMs) = nullptr;
    int (*pauseReason)() = nullptr;  // menu::GamePauseReason (0 none, 1 game menu, 2 stopped, 3 loading)
    const wchar_t* pipeName = nullptr;  // tests only (default \\.\pipe\nioh3vpad)
};

// dllmain worker, before padlat::Start / osd::Start (our IAT hook then sits innermost: the mod menu and PadLatency see the
// virtual pad like a real one). Reads [Debug] VirtualPad / KeepRunningUnfocused; installs nothing when both are 0.
void Start(const StartInfo& info);
// true when VirtualPad is on (dllmain then hands KeyFilter to the menu's DirectInput keyboard hook)
bool KeysWanted();
// menu.cpp DirectInput keyboard hook (256-byte reads, game input thread): hides the real keys while the game is not
// foreground (or the read failed), ORs the virtual keys in. Returns true = report success to the game.
bool KeyFilter(uint8_t* keys, bool readOk);

// ---- tests (tests/vpad_harness.cpp) ----
bool TestHookSlot(void** slot, uint8_t* hostBase, int mode);   // hook a fake IAT slot with VirtualPad = mode (no pipe)
int TestExec(const char* line, char* reply, size_t n);         // run one command line synchronously (0 ok, 1 err)
bool TestStartPipe(const wchar_t* name);                      // start the pipe server under a test name
uint8_t* TestBuildThunk(void* isActive);                      // KeepRunningUnfocused thunk on a fresh page (+28h switch, +2Ch count)
void TestSetHost(const StartInfo& info);                      // log / menuRemote / pauseReason without Start
}  // namespace vpad
