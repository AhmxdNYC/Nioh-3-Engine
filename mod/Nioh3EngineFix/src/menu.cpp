// menu.cpp - in-game mod menu of Nioh3EngineFix (see menu.h and notes\osd-design.md section 10).
//
// Input
//   * Controller: the game reads its pads through XInputGetState in its import table (XINPUT1_4 ordinal 2, IAT slot
//     0x38E2C50 on v2.02, called by the [App]InputUpdateWorker thread every logic frame - notes\input-thread.md).
//     We swap that IAT slot for HkXInputGetState, which calls the previous pointer (so the Steam overlay's inline hook
//     inside xinput stays in the chain), records button edges for the menu and detects the open gesture (default:
//     hold Start 5 s). While the menu is open, and afterwards until every button is released, the game gets a neutral
//     pad. A short Start press reaches the game untouched.
//   * Keyboard: the game reads it with IDirectInputDevice8::GetDeviceState(256). We patch slot 9 of the shared
//     DirectInput device vtables (A and W) of the system dinput8.dll: the menu key (default F10) is always hidden from
//     the game, the navigation keys only while the menu is open (and until released after closing). The W3 message
//     hook additionally drops WM_KEYDOWN/WM_SYSKEYDOWN of the menu key, so Windows never enters the F10 menu mode.
//   * Fallback: whatever could not be hooked is polled from the Present hook (xinput1_4 / 1_3 / 9_1_0 loaded
//     dynamically, GetAsyncKeyState); the game then also sees the presses (logged).
// Drawing and every state change run on the Present-hook thread (osd.cpp calls OnPresent + Draw under its lock).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <xinput.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "extraperf.h"
#include "gpuopt.h"
#include "menu.h"
#include "osd.h"

namespace menu {
namespace {

using LogFn = void (*)(const char*, ...);
LogFn g_log = nullptr;
#define MLOG(...)                               \
    do {                                        \
        if (g_log) g_log("MENU: " __VA_ARGS__); \
    } while (0)

// ---- engine (v2.02 only): stop flags G+0x588..0x58D (FrameDtSetup zeroes game time when 0x58B or 0x58C is set) ----
constexpr uint32_t kRvaFrameMgrPtr = 0x4759910;  // G
constexpr uint32_t kOffStopFlags = 0x588;        // 6 latched bytes, from request counters G+0x590+4*i
constexpr uint32_t kOffFrameCounter = 0x610;
// "a game menu is open" (1.7.13): the two getters ApplyGraphicsSettings (0x39A39C) ORs into the FG pause flag S+0x2055.
// 0x5A37D4: [g_45C84A8+0x170] screen object, vfunc 0x100 && vfunc 0x48; 0xD63C0: tail-jumps to vfunc 8 of [0x4758BA0].
constexpr uint32_t kRvaMenuA = 0x5A37D4, kRvaMenuB = 0xD63C0;
const uint8_t kMenuABytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0x05, 0xC7, 0x4C, 0x02, 0x04, 0x48, 0x8B, 0x98, 0x70, 0x01,
                               0x00, 0x00, 0x48, 0x85, 0xDB, 0x74, 0x1D, 0x48, 0x8B, 0x03, 0x48, 0x8B, 0xCB, 0xFF, 0x90, 0x00, 0x01, 0x00,
                               0x00, 0x84, 0xC0, 0x74, 0x0D, 0x48, 0x8B, 0x03, 0x48, 0x8B, 0xCB, 0xFF, 0x50, 0x48, 0x84, 0xC0, 0x75, 0x08,
                               0x32, 0xC0, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3, 0xB0, 0x01, 0xEB, 0xF6};
const uint8_t kMenuBBytes[] = {0x48, 0x8B, 0x0D, 0xD9, 0x27, 0x68, 0x04, 0x48, 0x85, 0xC9, 0x74, 0x07,
                               0x48, 0x8B, 0x01, 0x48, 0xFF, 0x60, 0x08, 0x32, 0xC0, 0xC3};
constexpr uint32_t kRvaXInputIat = 0x38E2C50;    // XINPUT1_4 ordinal 2 (XInputGetState) on v2.02

// ---- frame generation (v2.02 only; notes\frame-generation.md) ---------------------------------------------------------
// The game's options menu changes FG through SetOptionValue(0x126, v) (0x12A2368): FrameGenSettingWrite(v) + the
// config-dirty byte. Nothing else: the engine's per-frame display watcher (0x1350210) sees the new slot value, posts
// window message 0x405, and its display reset (0x226D140) rebuilds the swapchain with the chosen FG backend after a
// 5-tick countdown. The menu item below does exactly those two steps (the game's own function, bytes verified).
constexpr uint32_t kRvaFrameGen = 0x45BB008;      // FRAME_GENERATION slot: 0 OFF, 1 DLSS, 2 FSR, 3 XeSS
constexpr uint32_t kRvaFgWrite = 0x1FDDFCC;       // FrameGenSettingWrite(int): gate 0x1FE3FA4, then slot (+VSYNC/Reflex/XeLL)
constexpr uint32_t kRvaFgAvail = 0x1FE3FA4;       // IsFrameGenTypeAvailable(int): DLSS = Streamline caps+F0, XeSS = 45C3EAA, FSR = 1
constexpr uint32_t kRvaDirtyStore = 0x12A46AD;    // SetOptionValue: mov byte [g_ConfigDirty],1
constexpr uint32_t kRvaConfigDirty = 0x45C6F80;   // u8: SaveConfigIfDirty (0x283892C) writes config.xml and clears it
constexpr uint32_t kRvaXessFgAvail = 0x45C3EAA;   // u8 g_XessFgAvailable
constexpr uint32_t kRvaSlCaps = 0x46D7690;        // Streamline feature caps (static returned by 0x6B8E88); +F0 = DLSS-G
constexpr uint32_t kRvaConfigDesc = 0x45C8570;    // 60 x 0x30 descriptors: +0 name*, +8 values**, +0x24 count
constexpr int kIdxFrameGen = 54;
constexpr uint32_t kRvaConfigPathW = 0x4B736F0;   // wchar_t[]: the game's full config.xml path
const uint8_t kFgWriteBytes[] = {  // 0x1FDDFCC, 93 bytes (rel32 / RIP disp32 only - position independent)
    0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x8B, 0xD9, 0xE8, 0xCB, 0x5F, 0x00, 0x00, 0x33, 0xC9, 0x84, 0xC0, 0x74, 0x3E,
    0x89, 0x1D, 0x23, 0xD0, 0x5D, 0x02, 0x83, 0xEB, 0x01, 0x74, 0x20, 0x83, 0xFB, 0x02, 0x74, 0x09, 0x48, 0x89, 0x0D,
    0x1A, 0xD0, 0x5D, 0x02, 0xEB, 0x2B, 0xC7, 0x05, 0x12, 0xD0, 0x5D, 0x02, 0x01, 0x00, 0x00, 0x00, 0x89, 0x0D, 0x08,
    0xD0, 0x5D, 0x02, 0xEB, 0x19, 0x48, 0xC7, 0x05, 0xFB, 0xCF, 0x5D, 0x02, 0x01, 0x00, 0x00, 0x00, 0x89, 0x0D, 0x21,
    0xCF, 0x5D, 0x02, 0xEB, 0x06, 0x89, 0x0D, 0xE5, 0xCF, 0x5D, 0x02, 0x48, 0x83, 0xC4, 0x20, 0x5B, 0xC3};
const uint8_t kFgAvailBytes[] = {  // 0x1FE3FA4, 47 bytes
    0x48, 0x83, 0xEC, 0x28, 0x83, 0xE9, 0x01, 0x74, 0x16, 0x83, 0xE9, 0x01, 0x74, 0x0D, 0x83, 0xF9, 0x01, 0x75,
    0x08, 0x8A, 0x05, 0xED, 0xFE, 0x5D, 0x02, 0xEB, 0x0F, 0xB0, 0x01, 0xEB, 0x0B, 0xE8, 0xC0, 0x4E, 0x6D, 0xFE,
    0x8A, 0x80, 0xF0, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3};
const uint8_t kDirtyStoreBytes[] = {0xC6, 0x05, 0xCC, 0x28, 0x32, 0x03, 0x01};  // -> 0x45C6F80

// ---- configuration ([Menu]) ------------------------------------------------------------------------------------------
struct KeyDef {
    const char* name;
    UINT vk;
    uint8_t dik;
};
const KeyDef kKeys[] = {
    {"F1", VK_F1, 0x3B},  {"F2", VK_F2, 0x3C},   {"F3", VK_F3, 0x3D},         {"F4", VK_F4, 0x3E},
    {"F5", VK_F5, 0x3F},  {"F6", VK_F6, 0x40},   {"F7", VK_F7, 0x41},         {"F8", VK_F8, 0x42},
    {"F9", VK_F9, 0x43},  {"F10", VK_F10, 0x44}, {"F11", VK_F11, 0x57},       {"F12", VK_F12, 0x58},
    {"Insert", VK_INSERT, 0xD2}, {"Home", VK_HOME, 0xC7}, {"End", VK_END, 0xCF}, {"PageUp", VK_PRIOR, 0xC9},
    {"PageDown", VK_NEXT, 0xD1}, {"Pause", VK_PAUSE, 0xC5}, {"ScrollLock", VK_SCROLL, 0x46},
};

struct Cfg {
    int enabled = 1;
    UINT vk = VK_F10;
    uint8_t dik = 0x44;
    char keyName[16] = "F10";
    uint32_t holdMask = XINPUT_GAMEPAD_START;
    char holdName[48] = "Start";
    int holdMs = 5000;  // 0 = press the button combo (no hold)
    int gesture = 1;    // [Menu] Gesture: 1 TripleTap (default), 0 Hold
    int tapWindowMs = 900;
    int tapGapMs = 300;
    int swallowTaps = 1;
    float scale = 0;    // 0 = auto
    int pauseHint = 2;  // [Menu] PauseHint: 2 always, 1 while a game menu is open, 0 off
    uint8_t pauseMask = 0x18;  // bit i = G+0x588+i; default 58B + 58C (the two flags that stop game time)
    int fgToggle = 1;          // [Menu] FrameGenToggle: 0 status only, 1 switch on Windows, 2 also under Proton (risky)
    int pauseSrc = 2;          // [Menu] PauseHintSource: 0 Menu (the game's own "menu open" signal), 1 StopFlags (G+58B/58C), 2 Both
    uint8_t escDik = 0x01;     // [Menu] KeyTripleTap = Escape: press Escape 3x to open the menu (0 = off)
};
Cfg g_cfg;
Host g_host;
std::wstring g_ini;
bool g_configured = false;
volatile LONG g_disabled = 0;

// ---- shared input state (input thread <-> Present thread) -------------------------------------------------------------
enum : uint32_t { N_UP = 1, N_DOWN = 2, N_LEFT = 4, N_RIGHT = 8, N_OK = 16, N_BACK = 32 };
// pad raw bits: wButtons | stick directions | triggers
constexpr uint32_t B_SU = 1u << 16, B_SD = 1u << 17, B_SL = 1u << 18, B_SR = 1u << 19, B_LT = 1u << 20,
                   B_RT = 1u << 21, B_STICKS = B_SU | B_SD | B_SL | B_SR;
// keyboard bits
enum : uint32_t { K_TOGGLE = 1, K_UP = 2, K_DOWN = 4, K_LEFT = 8, K_RIGHT = 16, K_ENTER = 32, K_NPENTER = 64, K_ESC = 128, K_BACKSP = 256 };
constexpr uint32_t K_NAV = K_UP | K_DOWN | K_LEFT | K_RIGHT | K_ENTER | K_NPENTER | K_ESC | K_BACKSP;
struct NavKey {
    uint32_t bit;
    UINT vk;
    uint8_t dik;
};
const NavKey kNavKeys[] = {{K_UP, VK_UP, 0xC8},       {K_DOWN, VK_DOWN, 0xD0},     {K_LEFT, VK_LEFT, 0xCB},
                           {K_RIGHT, VK_RIGHT, 0xCD}, {K_ENTER, VK_RETURN, 0x1C},  {K_NPENTER, VK_RETURN, 0x9C},
                           {K_ESC, VK_ESCAPE, 0x01},  {K_BACKSP, VK_BACK, 0x0E}};

volatile LONG g_open = 0;
volatile LONG g_pressed = 0;          // N_* edges collected while open, consumed by OnPresent
volatile LONG g_toggleReq = 0;
volatile LONG64 g_toggleAt = 0;
volatile LONG g_toggleSrc = 0;        // 1 key, 2 pad
volatile LONG64 g_lastPresent = 0;    // last OnPresent (GetTickCount64): the overlay is alive
volatile LONG64 g_lastPadFeed = 0;    // the game called XInputGetState (any slot)
volatile LONG64 g_lastKeyFeed = 0;    // the game read the keyboard successfully
volatile LONG g_padHeld[4] = {};
volatile LONG64 g_padHeldAt[4] = {};
volatile LONG g_keyHeld = 0;
volatile LONG64 g_keyHeldAt = 0;
volatile LONG g_keyLatch = 0;         // nav keys kept hidden from the game until released
volatile LONG g_pauseReason = 0;      // 0 none, 1 game menu open, 2 game paused (stop flags), 3 loading (58A)
volatile LONG64 g_holdSince = 0;      // the open gesture is being held since (0 = not)
volatile LONG64 g_holdSeen = 0;

struct PadSlot {
    uint32_t prev = 0;
    ULONGLONG holdStart = 0;
    bool holdFired = false;
    // triple-tap gesture
    int nTaps = 0;
    ULONGLONG tap0 = 0, lastTap = 0, synthUntil = 0;
    bool passHold = false;  // the press turned out to be a hold: the game sees it from now on
    volatile bool latch = false;  // keep the pad neutral for the game until every button is released
};
PadSlot g_pad[4];
uint32_t g_keyPrev = 0;
// Escape triple-tap (same logic as the pad's Start gesture; DirectInput keyboard state is the only path the game reads Escape through)
int g_escTaps = 0;
ULONGLONG g_escTap0 = 0, g_escLast = 0, g_escSynth = 0;
ULONGLONG g_escMuteUntil = 0;  // after Escape closed the menu: the rest of a triple tap stays with the mod (never reaches the game)
bool g_escPass = false, g_escPrevDown = false;

ULONGLONG Now() { return GetTickCount64(); }
bool Alive(ULONGLONG t, ULONGLONG ms) { return t - (ULONGLONG)g_lastPresent < ms; }

uint32_t PadNav(uint32_t raw) {
    uint32_t n = 0;
    if (raw & (XINPUT_GAMEPAD_DPAD_UP | B_SU)) n |= N_UP;
    if (raw & (XINPUT_GAMEPAD_DPAD_DOWN | B_SD)) n |= N_DOWN;
    if (raw & (XINPUT_GAMEPAD_DPAD_LEFT | B_SL)) n |= N_LEFT;
    if (raw & (XINPUT_GAMEPAD_DPAD_RIGHT | B_SR)) n |= N_RIGHT;
    if (raw & XINPUT_GAMEPAD_A) n |= N_OK;
    if (raw & XINPUT_GAMEPAD_B) n |= N_BACK;
    return n;
}
uint32_t KeyNav(uint32_t k) {
    uint32_t n = 0;
    if (k & K_UP) n |= N_UP;
    if (k & K_DOWN) n |= N_DOWN;
    if (k & K_LEFT) n |= N_LEFT;
    if (k & K_RIGHT) n |= N_RIGHT;
    if (k & (K_ENTER | K_NPENTER)) n |= N_OK;
    if (k & (K_ESC | K_BACKSP)) n |= N_BACK;
    return n;
}

void Request(ULONGLONG t, LONG src) {
    for (PadSlot& s : g_pad) s.latch = true;
    g_toggleAt = (LONG64)t;
    g_toggleSrc = src;
    InterlockedExchange(&g_toggleReq, 1);
}

void ForceCloseIfDead(ULONGLONG t) {
    if (g_open && !Alive(t, 1500) && InterlockedExchange(&g_open, 0))
        MLOG("closed: the overlay stopped presenting for 1.5 s - game input passes through again");
}

// ---- pad feed (game input thread via the IAT hook; Present thread for own polling) ------------------------------------
// No C++ objects with destructors here: called inside __try.
void PadFeed(DWORD idx, XINPUT_STATE* st, bool own) {
    const ULONGLONG t = Now();
    PadSlot& s = g_pad[idx];
    XINPUT_GAMEPAD& g = st->Gamepad;
    const uint32_t prev = s.prev;
    uint32_t raw = g.wButtons;
    if (g.sThumbLY > ((prev & B_SU) ? 12000 : 20000)) raw |= B_SU;  // hysteresis: no edge flicker at the threshold
    if (g.sThumbLY < -((prev & B_SD) ? 12000 : 20000)) raw |= B_SD;
    if (g.sThumbLX < -((prev & B_SL) ? 12000 : 20000)) raw |= B_SL;
    if (g.sThumbLX > ((prev & B_SR) ? 12000 : 20000)) raw |= B_SR;
    if (g.bLeftTrigger > 30) raw |= B_LT;
    if (g.bRightTrigger > 30) raw |= B_RT;
    s.prev = raw;
    const uint32_t pressed = raw & ~prev;
    g_padHeld[idx] = (LONG)raw;
    g_padHeldAt[idx] = (LONG64)t;
    if (g_disabled) return;
    ForceCloseIfDead(t);
    const bool alive = Alive(t, 1000);
    // open/close gesture
    const uint32_t hm = g_cfg.holdMask;
    bool tapModified = false;
    if (hm && g_cfg.gesture == 1) {
        const bool down = (raw & hm) == hm, wasDown = (prev & hm) == hm;
        if (s.nTaps && t - s.lastTap >= (ULONGLONG)g_cfg.tapGapMs) {  // no further press in time: decide what the game sees
            if (down && wasDown && s.nTaps == 1) s.passHold = true;       // still held: a real hold, the game gets it now
            else if (!g_open && g_cfg.swallowTaps) s.synthUntil = t + 120;  // finished tap(s): replay one tap
            s.nTaps = 0;
        }
        if (!down) s.passHold = false;
        if (down && !wasDown) {
            if (s.nTaps && t - s.tap0 > (ULONGLONG)g_cfg.tapWindowMs) s.nTaps = 0;
            if (s.nTaps == 0) s.tap0 = t;
            ++s.nTaps;
            s.lastTap = t;
            s.synthUntil = 0;
            if (s.nTaps >= 3) {
                s.nTaps = 0;
                if (alive) Request(t, 2);
                else s.passHold = true;
            }
        }
        if (!own && g_cfg.swallowTaps && down && !s.passHold && s.nTaps) {
            g.wButtons &= (WORD)~hm;
            tapModified = true;
        } else if (!own && t < s.synthUntil && !down) {
            g.wButtons |= (WORD)hm;
            tapModified = true;
        }
    } else if (hm) {
        if ((raw & hm) == hm) {
            if (g_cfg.holdMs <= 0) {
                if ((pressed & hm) && alive) Request(t, 2);
            } else {
                if (!s.holdStart) {
                    s.holdStart = t;
                    s.holdFired = false;
                }
                if (!s.holdFired && alive) {
                    g_holdSince = (LONG64)s.holdStart;
                    g_holdSeen = (LONG64)t;
                    if (t - s.holdStart >= (ULONGLONG)g_cfg.holdMs) {
                        s.holdFired = true;
                        g_holdSince = 0;
                        Request(t, 2);
                    }
                }
            }
        } else {
            if (s.holdStart && !s.holdFired) g_holdSince = 0;
            s.holdStart = 0;
            s.holdFired = false;
        }
    }
    if (g_open) InterlockedOr(&g_pressed, (LONG)PadNav(pressed));
    if (own) return;
    if (tapModified) st->dwPacketNumber += 0x40000000u;
    bool blank = g_open != 0;
    if (s.latch) {
        if ((raw & ~B_STICKS) == 0) s.latch = false;
        else blank = true;
    }
    if (blank) {
        st->dwPacketNumber += 0x40000000u;  // the neutral state gets its own packet numbers (games may cache by packet)
        memset(&g, 0, sizeof(g));
    }
}

void PadFeedSafe(DWORD idx, XINPUT_STATE* st, bool own) {
    __try {
        PadFeed(idx, st, own);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_disabled, 1);
        InterlockedExchange(&g_open, 0);
        MLOG("exception %08lX in the pad hook - mod menu disabled for this session (input passes through)", GetExceptionCode());
    }
}

// ---- keyboard feed (DirectInput hook / own polling). keys = 256-byte DIK state -----------------------------------------
void KeyFeed(uint8_t* keys, bool own) {
    const ULONGLONG t = Now();
    uint32_t raw = 0;
    if (g_cfg.dik && (keys[g_cfg.dik] & 0x80)) raw |= K_TOGGLE;
    for (const NavKey& k : kNavKeys)
        if (keys[k.dik] & 0x80) raw |= k.bit;
    const uint32_t pressed = raw & ~g_keyPrev;
    g_keyPrev = raw;
    g_keyHeld = (LONG)raw;
    g_keyHeldAt = (LONG64)t;
    if (g_disabled) return;
    ForceCloseIfDead(t);
    if ((pressed & K_TOGGLE) && Alive(t, 1000)) Request(t, 1);
    if (g_open) InterlockedOr(&g_pressed, (LONG)KeyNav(pressed));
    InterlockedAnd(&g_keyLatch, (LONG)raw);  // released keys leave the latch
    // Escape x3 opens the menu. The first presses are held back from the game for TapGapMs; a lone press (or two) is replayed
    // to it afterwards as one short press; a press that is still held after the gap is a real hold and passes at once.
    bool escHide = false, escSynth = false;
    if (g_cfg.escDik) {
        const bool down = (keys[g_cfg.escDik] & 0x80) != 0, wasDown = g_escPrevDown;
        g_escPrevDown = down;
        if (g_open && down && !wasDown) g_escMuteUntil = t + (ULONGLONG)g_cfg.tapWindowMs;  // Esc x3 toggles: 1st closes
        if (g_open || (g_keyLatch & K_ESC) || t < g_escMuteUntil) {  // the open menu (or the press that just opened/closed it) owns Escape
            if (!g_open && down) escHide = true;
            g_escTaps = 0;
            g_escPass = false;
            g_escSynth = 0;
        } else {
            if (g_escTaps && t - g_escLast >= (ULONGLONG)g_cfg.tapGapMs) {
                if (down && wasDown && g_escTaps == 1) g_escPass = true;
                else if (g_cfg.swallowTaps) g_escSynth = t + 120;
                g_escTaps = 0;
            }
            if (!down) g_escPass = false;
            if (down && !wasDown) {
                if (g_escTaps && t - g_escTap0 > (ULONGLONG)g_cfg.tapWindowMs) g_escTaps = 0;
                if (g_escTaps == 0) g_escTap0 = t;
                ++g_escTaps;
                g_escLast = t;
                g_escSynth = 0;
                if (g_escTaps >= 3) {
                    g_escTaps = 0;
                    if (Alive(t, 1000)) {
                        Request(t, 3);
                        InterlockedOr(&g_keyLatch, (LONG)K_ESC);  // the third press stays hidden until released
                        escHide = true;
                    } else {
                        g_escPass = true;
                    }
                }
            }
            if (g_cfg.swallowTaps && down && !g_escPass && g_escTaps) escHide = true;
            else if (t < g_escSynth && !down) escSynth = true;
        }
    }
    if (own) return;
    if (escHide) keys[g_cfg.escDik] = 0;
    else if (escSynth) keys[g_cfg.escDik] = 0x80;
    if (g_cfg.dik) keys[g_cfg.dik] = 0;  // the menu key belongs to the mod
    const uint32_t hide = (g_open ? K_NAV : 0u) | (uint32_t)g_keyLatch;
    if (hide)
        for (const NavKey& k : kNavKeys)
            if (hide & k.bit) keys[k.dik] = 0;
}

void KeyFeedSafe(uint8_t* keys, bool own) {
    __try {
        KeyFeed(keys, own);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_disabled, 1);
        InterlockedExchange(&g_open, 0);
        MLOG("exception %08lX in the keyboard hook - mod menu disabled for this session (input passes through)", GetExceptionCode());
    }
}

// ---- hooks ---------------------------------------------------------------------------------------------------------
using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetState_t g_xiOrig = nullptr;  // the game's previous IAT target (xinput1_4, possibly Steam-overlay-hooked)
XInputGetState_t g_xiOwn = nullptr;   // dynamically loaded, for own polling
bool g_padHooked = false, g_keyHooked = false;

DWORD WINAPI HkXInputGetState(DWORD idx, XINPUT_STATE* st) {
    const DWORD r = g_xiOrig(idx, st);
    g_lastPadFeed = (LONG64)Now();
    if (r == ERROR_SUCCESS && st && idx < 4 && !g_disabled) PadFeedSafe(idx, st, false);
    return r;
}

using GetDeviceState_t = HRESULT(STDMETHODCALLTYPE*)(void*, DWORD, void*);
GetDeviceState_t g_gdsW = nullptr, g_gdsA = nullptr;
thread_local int t_diDepth = 0;
bool (*volatile g_keyFilter)(uint8_t*, bool) = nullptr;  // vpad.cpp (VirtualPad): virtual keys, real keys hidden while unfocused

HRESULT DiHooked(GetDeviceState_t orig, void* self, DWORD cb, void* data) {
    ++t_diDepth;  // Wine's A method calls the W one: only the outermost call looks at the data
    HRESULT hr = orig(self, cb, data);
    --t_diDepth;
    if (t_diDepth == 0 && cb == 256 && data)
        if (auto f = g_keyFilter)
            if (f(static_cast<uint8_t*>(data), SUCCEEDED(hr))) hr = DI_OK;
    if (t_diDepth == 0 && SUCCEEDED(hr) && cb == 256 && data && !g_disabled) {  // 256 bytes = keyboard state
        g_lastKeyFeed = (LONG64)Now();
        KeyFeedSafe(static_cast<uint8_t*>(data), false);
    }
    return hr;
}
HRESULT STDMETHODCALLTYPE HkGdsW(void* self, DWORD cb, void* data) { return DiHooked(g_gdsW, self, cb, data); }
HRESULT STDMETHODCALLTYPE HkGdsA(void* self, DWORD cb, void* data) { return DiHooked(g_gdsA, self, cb, data); }

std::string ModuleOf(const void* p) {
    HMODULE m = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCWSTR)p, &m))
        return "?";
    wchar_t w[MAX_PATH] = {};
    GetModuleFileNameW(m, w, MAX_PATH);
    const wchar_t* b = wcsrchr(w, L'\\');
    char a[MAX_PATH * 2] = {};
    WideCharToMultiByte(CP_UTF8, 0, b ? b + 1 : w, -1, a, (int)sizeof(a), nullptr, nullptr);
    char buf[MAX_PATH * 2 + 32];
    snprintf(buf, sizeof(buf), "%s+%llX", a, (unsigned long long)((const uint8_t*)p - (const uint8_t*)m));
    return buf;
}

bool SwapPointer(void** slot, void* hook, void** orig) {
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
    *orig = *slot;  // published before the slot so the hook never sees a null original
    MemoryBarrier();
    InterlockedExchangePointer(slot, hook);
    DWORD t;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return true;
}

// the game's IAT slot for XInputGetState (by import name / ordinal 2 of xinput1_x; known-build RVA as a fallback)
void** FindXInputSlotRaw(uint8_t* base, bool known) {
    __try {
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (dd.VirtualAddress) {
            for (auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dd.VirtualAddress); d->Name; ++d) {
                const char* dll = reinterpret_cast<const char*>(base + d->Name);
                if (_strnicmp(dll, "xinput", 6) != 0 || !d->OriginalFirstThunk || !d->FirstThunk) continue;
                const bool ord2 = _strnicmp(dll, "xinput1_", 8) == 0;  // ordinal 2 = XInputGetState in xinput1_3/1_4
                auto names = reinterpret_cast<IMAGE_THUNK_DATA64*>(base + d->OriginalFirstThunk);
                auto iat = reinterpret_cast<void**>(base + d->FirstThunk);
                for (int i = 0; names[i].u1.AddressOfData; ++i) {
                    const ULONGLONG e = names[i].u1.AddressOfData;
                    if (IMAGE_SNAP_BY_ORDINAL64(e)) {
                        if (ord2 && IMAGE_ORDINAL64(e) == 2) return &iat[i];
                    } else if (!strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + e)->Name, "XInputGetState")) {
                        return &iat[i];
                    }
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return known ? reinterpret_cast<void**>(base + kRvaXInputIat) : nullptr;
}

bool InstallPadHook() {
    if (!g_host.base) return false;
    void** slot = FindXInputSlotRaw(g_host.base, g_host.knownBuild);
    if (!slot) {
        MLOG("no XInputGetState import found in Nioh3.exe");
        return false;
    }
    void* cur = *slot;
    HMODULE m = nullptr;
    if (!cur || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                    (LPCWSTR)cur, &m) ||
        (uint8_t*)m == g_host.base) {
        MLOG("XInputGetState IAT slot %p holds %p, which is not inside a loaded DLL - not hooked", (void*)slot, cur);
        return false;
    }
    if (!SwapPointer(slot, (void*)&HkXInputGetState, (void**)&g_xiOrig)) {
        MLOG("VirtualProtect on the XInputGetState IAT slot failed (%lu)", GetLastError());
        return false;
    }
    MLOG("hooked the game's XInputGetState: IAT slot Nioh3.exe+%llX, chained to %s",
         (unsigned long long)((uint8_t*)slot - g_host.base), ModuleOf((void*)g_xiOrig).c_str());
    return true;
}

const GUID kIID_DI8W = {0xBF798031, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
const GUID kIID_DI8A = {0xBF798030, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
const GUID kGUID_SysKeyboard = {0x6F1D2B61, 0xD5A0, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

// vtable of a keyboard device created through the given IDirectInput8 flavour (objects are released again)
void** KeyboardVtable(HMODULE di8, const GUID& iid) {
    using Create_t = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
    auto create = reinterpret_cast<Create_t>(GetProcAddress(di8, "DirectInput8Create"));
    if (!create) return nullptr;
    IDirectInput8W* di = nullptr;  // the A interface has the same layout up to CreateDevice
    HRESULT hr = create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, iid, (void**)&di, nullptr);
    if (FAILED(hr) || !di) {
        MLOG("DirectInput8Create failed (hr %08lX)", (unsigned long)hr);
        return nullptr;
    }
    IDirectInputDevice8W* dev = nullptr;
    hr = di->CreateDevice(kGUID_SysKeyboard, &dev, nullptr);
    void** vt = nullptr;
    if (SUCCEEDED(hr) && dev) {
        vt = *reinterpret_cast<void***>(dev);
        dev->Release();
    } else {
        MLOG("CreateDevice(keyboard) failed (hr %08lX)", (unsigned long)hr);
    }
    di->Release();
    return vt;
}

bool InstallKeyHook() {
    wchar_t sys[MAX_PATH] = {};
    const UINT n = GetSystemDirectoryW(sys, MAX_PATH);
    if (!n || n > MAX_PATH - 16) return false;
    // the system dinput8.dll (an ASI loader's dinput8.dll proxy forwards the game's calls to the same module)
    HMODULE di8 = LoadLibraryW((std::wstring(sys) + L"\\dinput8.dll").c_str());
    if (!di8) {
        MLOG("loading the system dinput8.dll failed (%lu)", GetLastError());
        return false;
    }
    void** vtW = KeyboardVtable(di8, kIID_DI8W);
    void** vtA = KeyboardVtable(di8, kIID_DI8A);
    constexpr int kSlotGetDeviceState = 9;
    bool any = false;
    if (vtW && SwapPointer(&vtW[kSlotGetDeviceState], (void*)&HkGdsW, (void**)&g_gdsW)) {
        MLOG("hooked DirectInput GetDeviceState (W): vtable %s, original %s", ModuleOf(vtW).c_str(), ModuleOf((void*)g_gdsW).c_str());
        any = true;
    }
    if (vtA && vtA != vtW && SwapPointer(&vtA[kSlotGetDeviceState], (void*)&HkGdsA, (void**)&g_gdsA)) {
        MLOG("hooked DirectInput GetDeviceState (A): vtable %s, original %s", ModuleOf(vtA).c_str(), ModuleOf((void*)g_gdsA).c_str());
        any = true;
    }
    return any;
}

void LoadOwnXInput() {
    const wchar_t* names[] = {L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"};
    for (const wchar_t* nm : names) {
        HMODULE m = GetModuleHandleW(nm);
        if (!m) m = LoadLibraryW(nm);
        if (!m) continue;
        if (auto f = reinterpret_cast<XInputGetState_t>(GetProcAddress(m, "XInputGetState"))) {
            g_xiOwn = f;
            MLOG("own XInput for fallback polling: %s", ModuleOf((void*)f).c_str());
            return;
        }
    }
    MLOG("no XInput DLL could be loaded - controller fallback polling unavailable");
}

// ---- engine reads -------------------------------------------------------------------------------------------------------
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

// ---- frame generation through the game's own setter --------------------------------------------------------------------
bool g_fgOk = false;         // setter, gate and dirty store verified, and [Menu] FrameGenToggle allows it (1 / 2 on Proton)
bool g_fgChanged = false;    // changed from this menu this session: "Save to ini" also writes config.xml (Proton: OFF only)
bool g_fgNextLaunch = false;  // Proton: FG ON -> OFF written to config.xml, applies at the next launch
int g_fgLastOn = 2;          // A/Enter turns FG on with this type (FSR unless another one was picked)
ULONGLONG g_fgChangedAt = 0;
const int kFgCycle[] = {0, 2, 3, 1};  // OFF, FSR, XeSS, DLSS (unavailable types are skipped)

bool BytesAt(uint32_t rva, const uint8_t* b, size_t n) {
    __try {
        return memcmp(g_host.base + rva, b, n) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

int FgSetting() {
    int v = -1;
    if (g_host.knownBuild && g_host.base) Rd(g_host.base + kRvaFrameGen, v);
    return v >= 0 && v <= 3 ? v : -1;
}

// mirrors IsFrameGenTypeAvailable (0x1FE3FA4) without calling it: FSR always, DLSS / XeSS only when the game found them
bool FgTypeAvailable(int v) {
    uint8_t b = 0;
    switch (v) {
        case 0: case 2: return true;
        case 1: return Rd(g_host.base + kRvaSlCaps + 0xF0, b) && b;
        case 3: return Rd(g_host.base + kRvaXessFgAvail, b) && b;
        default: return false;
    }
}

// FrameGenSettingWrite(v) - the game's own setter (it re-checks availability and writes 0 if the type is unavailable).
// Returns the slot value afterwards (-2 on an exception).
int CallFgWrite(int v) {
    using Fn = void (*)(int);
    __try {
        reinterpret_cast<Fn>(g_host.base + kRvaFgWrite)(v);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -2;
    }
    return FgSetting();
}

void SetConfigDirty() {
    __try {
        *reinterpret_cast<volatile uint8_t*>(g_host.base + kRvaConfigDirty) = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

bool CStrEq(const char* p, const char* s) {
    __try {
        return p && s && strcmp(p, s) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// the game's own descriptor for FRAME_GENERATION still says {OFF, DLSS, FSR, XeSS} (the strings config.xml uses)
bool FgDescMatches() {
    const uint8_t* d = g_host.base + kRvaConfigDesc + kIdxFrameGen * 0x30;
    uint64_t name = 0, vals = 0;
    int cnt = 0;
    if (!Rd(d, name) || !Rd(d + 8, vals) || !Rd(d + 0x24, cnt) || cnt != 4) return false;
    if (!CStrEq(reinterpret_cast<const char*>((uintptr_t)name), "FRAME_GENERATION")) return false;
    for (int i = 0; i < 4; ++i) {
        uint64_t p = 0;
        if (!Rd(reinterpret_cast<const uint8_t*>((uintptr_t)vals) + 8 * i, p) ||
            !CStrEq(reinterpret_cast<const char*>((uintptr_t)p), osd::FgTypeName(i)))
            return false;
    }
    return true;
}

bool ReadWStr(const wchar_t* p, wchar_t* out, size_t n) {
    __try {
        size_t i = 0;
        for (; i + 1 < n && p[i]; ++i) out[i] = p[i];
        out[i] = 0;
        return i > 0 && i + 1 < n;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// the config.xml the game loaded (its own path string); %LOCALAPPDATA%\KoeiTecmo\NIOH3\config.xml if that looks wrong
std::wstring GameConfigPath() {
    static const wchar_t kTail[] = L"\\KoeiTecmo\\NIOH3\\config.xml";
    const size_t tail = wcslen(kTail);
    wchar_t b[MAX_PATH] = {};
    if (ReadWStr(reinterpret_cast<const wchar_t*>(g_host.base + kRvaConfigPathW), b, MAX_PATH)) {
        const size_t n = wcslen(b);
        if (n > tail + 2 && !_wcsicmp(b + n - tail, kTail)) return b;
    }
    wchar_t e[MAX_PATH] = {};
    const DWORD n = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\KoeiTecmo\\NIOH3\\config.xml", e, MAX_PATH);
    MLOG("config.xml: the game's path string is not readable - using %ls", n && n <= MAX_PATH ? e : L"(none)");
    return n && n <= MAX_PATH && e[0] != L'%' ? e : L"";
}

// Rewrites ONLY the FRAME_GENERATION value of the game's config.xml (every other byte stays), after copying the file
// to config.xml.Nioh3EngineFix.bak. Returns 1 written, 2 already that value, 3 failed (logged). Save thread.
int WriteGameConfigFg(const std::wstring& path, const std::string& value) {
    if (path.empty()) return 3;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        MLOG("config.xml: cannot open %ls (%lu) - not changed", path.c_str(), GetLastError());
        return 3;
    }
    std::string data;
    LARGE_INTEGER sz{};
    bool ok = GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < (1 << 20);
    if (ok) {
        data.resize((size_t)sz.QuadPart);
        DWORD rd = 0;
        ok = ReadFile(h, &data[0], (DWORD)data.size(), &rd, nullptr) && rd == (DWORD)data.size();
    }
    CloseHandle(h);
    if (!ok) {
        MLOG("config.xml: reading %ls failed - not changed", path.c_str());
        return 3;
    }
    static const char kOpen[] = "<FRAME_GENERATION>", kClose[] = "</FRAME_GENERATION>";
    const size_t a = data.find(kOpen);
    if (a == std::string::npos || data.find(kOpen, a + 1) != std::string::npos) {
        MLOG("config.xml: <FRAME_GENERATION> not found exactly once - not changed");
        return 3;
    }
    const size_t v0 = a + strlen(kOpen), b = data.find(kClose, v0);
    const std::string cur = b == std::string::npos ? std::string() : data.substr(v0, b - v0);
    bool known = false;
    for (int i = 0; i < 4; ++i) known |= cur == osd::FgTypeName(i);
    if (!known) {
        MLOG("config.xml: unexpected FRAME_GENERATION value \"%.16s\" - not changed", cur.c_str());
        return 3;
    }
    if (cur == value) {
        MLOG("config.xml: FRAME_GENERATION is already %s - not changed", value.c_str());
        return 2;
    }
    const std::wstring bak = path + L".Nioh3EngineFix.bak", tmp = path + L".Nioh3EngineFix.tmp";
    if (!CopyFileW(path.c_str(), bak.c_str(), FALSE)) {
        MLOG("config.xml: backup to %ls failed (%lu) - not changed", bak.c_str(), GetLastError());
        return 3;
    }
    const std::string out = data.substr(0, v0) + value + data.substr(b);
    h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    DWORD wr = 0;
    ok = h != INVALID_HANDLE_VALUE && WriteFile(h, out.data(), (DWORD)out.size(), &wr, nullptr) && wr == (DWORD)out.size() &&
         FlushFileBuffers(h);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        MLOG("config.xml: writing %ls failed (%lu) - not changed", path.c_str(), GetLastError());
        DeleteFileW(tmp.c_str());
        return 3;
    }
    MLOG("config.xml: FRAME_GENERATION %s -> %s in %ls (previous file: %ls)", cur.c_str(), value.c_str(), path.c_str(), bak.c_str());
    return 1;
}

// ---- Proton: never leave FRAME_GENERATION on in config.xml because of the mod ---------------------------------------------
// FG on under Proton is reported to crash the game, and a config.xml with FG on then crash-loops it. The mod never saves
// FG on there itself, but the game does as soon as the user leaves its own Options/pause menu. So when the mod switched
// FG on this session and config.xml had FG OFF at startup, it drops a marker file next to the mod and puts OFF back:
//   * at exit (dllmain hooks the game's ExitProcess / TerminateProcess imports and calls ProtonFgExit; DLL detach as a
//     last resort), and
//   * at the next start, before the game reads its config, if the marker is still there (crash) - ProtonFgEarly.
std::wstring g_fgMarker;        // Nioh3EngineFix.fgmarker
std::wstring g_cfgPathEarly;    // %LOCALAPPDATA%\KoeiTecmo\NIOH3\config.xml
std::string g_fgAtStart;        // FRAME_GENERATION in config.xml when the mod started ("" = unknown)
volatile LONG g_fgMarkerSet = 0;
volatile LONG g_fgExitDone = 0;
// 1.7.44 [Menu] FrameGenToggle = 3 ("Proton safe FG", notes\fg-proton-fix.md)
int g_fgToggleEarly = 1;        // [Menu] FrameGenToggle as read by dllmain before Configure (ProtonFgEarly)
std::string g_fgExitWant;       // FRAME_GENERATION config.xml must hold at a clean exit ("" = OFF, the FrameGenToggle 2 rule)
bool g_fgSoft = false;          // FSR FG paused by the mod (FGP): the setting stays FSR, no frames are interpolated

std::string ReadGameConfigFg(const std::wstring& path) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return "";
    std::string data;
    LARGE_INTEGER sz{};
    DWORD rd = 0;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart < (1 << 20)) {
        data.resize((size_t)sz.QuadPart);
        if (!ReadFile(h, &data[0], (DWORD)data.size(), &rd, nullptr) || rd != (DWORD)data.size()) data.clear();
    }
    CloseHandle(h);
    static const char kOpen[] = "<FRAME_GENERATION>", kClose[] = "</FRAME_GENERATION>";
    const size_t a = data.find(kOpen);
    if (a == std::string::npos) return "";
    const size_t v0 = a + strlen(kOpen), b = data.find(kClose, v0);
    return b == std::string::npos || b - v0 > 8 ? "" : data.substr(v0, b - v0);
}

void SetFgMarker() {
    if (g_fgMarker.empty() || InterlockedExchange(&g_fgMarkerSet, 1)) return;
    HANDLE h = CreateFileW(g_fgMarker.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    static const char kText[] = "Frame generation is on in this Proton session (switched on by the mod, or launched with it).\r\n"
                                "If this file is still here at the next start, the session did not end cleanly and\r\n"
                                "Nioh3EngineFix puts FRAME_GENERATION OFF back into config.xml before the game reads it.\r\n";
    DWORD wr = 0;
    const bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, kText, (DWORD)sizeof(kText) - 1, &wr, nullptr) && FlushFileBuffers(h);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    MLOG("Proton: FG on this session - marker %ls %s (a crash puts FRAME_GENERATION OFF back at the next start; clean exit: %s)",
         g_fgMarker.c_str(), ok ? "written" : "COULD NOT be written", g_fgExitWant.empty() ? "OFF" : g_fgExitWant.c_str());
}

// 1.7.44 FrameGenToggle = 3: a launch with FG on in config.xml (and no crash marker left, or after ProtonFgEarly put OFF
// back) arms the marker before the game reads config.xml; a clean exit keeps the value, a crash reverts it to OFF.
void ProtonFgArmAtLaunch() {
    if (g_fgToggleEarly != 3 || g_fgAtStart.empty() || g_fgAtStart == "OFF") return;
    g_fgExitWant = g_fgAtStart;
    SetFgMarker();
    MLOG("Proton safe FG ([Menu] FrameGenToggle = 3): launching with FRAME_GENERATION %s - crash guard armed before the game "
         "reads config.xml", g_fgAtStart.c_str());
}

// called from ChangeFg on the Present thread after a switch
void NoteFgSwitchProton(int want) {
    if (!g_host.wine || !want) return;
    if (g_fgAtStart == "OFF") SetFgMarker();
    else MLOG("Proton: config.xml had FRAME_GENERATION %s at startup - no OFF restore at exit", g_fgAtStart.empty() ? "?" : g_fgAtStart.c_str());
}

// ---- items ----------------------------------------------------------------------------------------------------------
enum Id { kVrr, kW3, kW3Mode, kR1, kL1, kF1, kWk, kLat,
          kXW6, kXB, kXPP, kXFPC, kXAABB, kXM1, kXX1, kXE1, kXM2, kXJ1, kXS1, kXIK, kXTS, kXWS, kXED, kXGF, kXWL,  // order = menu::ExpId
          kXWG, kXTG, kXDS, kXLK, kXCP, kXSPF, kXMQ, kXPT1, kXDL, kXDQ, kXP1E7, kXMA, kXBPO, kXW1B, kXRPA, kXAS, kXPJH, kXTR,
          kXVSMA, kXLPVR, kXSKYR, kXFOGT, kXCLDR, kXRTP, kXPCM,
          kGpu, kGR2a, kGR2b, kGR1,
          kEpPre, kEp0, kEp1, kEp2, kEp3, kEp4, kEp5, kEp6, kEp7, kEp8, kEp9,  // 1.7.59 Extra performance (extraperf.h order)
          kFps, kFrameTime, kSpeed, kPos, kScale, kFg, kFgState, kUpscaler, kOffX, kOffY, kPerfNote, kHint, kAutoCont,
          kWorkerCap, kP12, kL1Wine, kFgWine, kSave, kClose, kCount };
static_assert(kXPCM - kXW6 + 1 == kNumExp, "experimental items must match menu::ExpId");
static_assert(kEp9 - kEp0 + 1 == extraperf::kNumOpts, "Extra performance rows must match extraperf::Opt");
bool IsExp(int id) { return id >= kXW6 && id <= kXPCM; }
bool IsEp(int id) { return id >= kEp0 && id <= kEp9; }  // one Extra performance option each (level row)
const wchar_t* const kExpIniKey[kNumExp] = {L"W6", L"B", L"PP", L"FPC", L"AABB", L"M1", L"X1", L"E1", L"M2", L"J1", L"S1", L"IK", L"TS", L"WakeStats", L"ED", L"GF", L"WL",
                                             L"WG", L"TG", L"DS", L"LK", L"CP", L"SPF", L"MQ", L"PT1", L"DL",
                                             L"DQ", L"P1E7", L"MA", L"BPO", L"W1B", L"RPA", L"AS", L"PJH", L"TR",
                                             L"VSMA", L"LPVR", L"SKYR", L"FOGT", L"CLDR", L"RTP", L"PCM"};
struct ItemDef {
    const char* plain;   // plain-language name
    const char* tech;    // technical id, drawn small and dim after the name ("" = none)
    const char* what;    // one line: what it does
    const char* effect;  // one line: picture / behaviour, when it applies, measured effect where known
};
#define E_CPU "Picture unchanged. Applies now. CPU only: no fps change on the Ally."
#define E_CPU_NEXT "Picture unchanged. Applies at the next launch (save to ini). CPU only: no fps change on the Ally."
const ItemDef kItems[kCount] = {
    {"Smoother frame pacing", "(VRR)", "Game speed stays real below the fps cap instead of the whole game slowing down.", "Changes timing, not the picture. Applies now. Auto-suspended while an online session is detected (VRROnline)."},
    {"Message thread sleeps, not spins", "(W3)", "The window-message thread sleeps (<=1 ms) instead of spinning a CPU core.", E_CPU},
    {"Message wait style (Proton)", "(W3 mode)", "Proton only. Sleep 1 ms = few wake-ups; MsgWait = Windows wait (~24k wakes/s on Wine).", "Picture unchanged. Applies now."},
    {"Render wait timeout", "(R1)", "The game thread waits for the GPU with a timeout per round. 0 = stock busy poll.", "Picture unchanged. Applies now. CPU only: no fps change on the Ally."},
    {"Frame limiter sleeps, not spins", "(L1)", "The frame limiter sleeps on a high-resolution timer instead of spinning.", E_CPU},
    {"Frame-gen presenter sleeps", "(F1)", "The FSR frame-gen presenter sleeps instead of spinning (only matters with frame gen ON).", E_CPU},
    {"Fewer worker wake-ups", "(WK)", "Skips waking a job worker when no queue holds work.", E_CPU},
    {"Frames queued ahead", "(max latency)", "How many frames the game queues (game: 1). Fewer = lower input delay.", "Picture unchanged. Applies now (Proton: after a swapchain rebuild or restart)."},
    {"CPU waits sleep instead of spinning", "(W6)", "The main thread sleeps while Present blocks, instead of spinning a core.", E_CPU},
    {"Faster draw-list search", "(B)", "Draw-list group length found by galloping, not a full binary search.", "Picture unchanged. Applies now. About +5 % main-thread headroom on PC."},
    {"Page-pool sweep less often", "(PP)", "Streaming page-pool usage sweep runs every N-th frame (Tuning PP_Period, default 4).", E_CPU},
    {"Cache material lookups", "(FPC)", "Per-frame cache of material parameter lookups on the render thread.", E_CPU},
    {"Faster bounding boxes", "(AABB)", "Character bounding boxes computed in SSE, bit-identical results.", E_CPU},
    {"Meshlet pool sum cached", "(M1)", "Meshlet workers stop re-summing the page-pool capacity per chunk.", E_CPU},
    {"Skip empty pad slots", "(X1)", "The active-pad scan skips XInput slots the game knows are empty.", E_CPU},
    {"Hidden effects update less", "(E1)", "Effects nobody can see update every N-th pass with N x the time step (E1_N=2).", "Visible effects untouched. Applies now. CPU only: no fps change on the Ally."},
    {"Meshlet plain loads", "(M2)", "Meshlet workers read bitmaps with plain loads instead of locked ones.", E_CPU_NEXT},
    {"Job-queue plain loads", "(J1)", "Idle job workers poll queues with plain loads instead of locked ones.", E_CPU_NEXT},
    {"Pause in end-of-frame spin", "(S1)", "A PAUSE instruction in the main thread's end-of-frame spin.", E_CPU_NEXT},
    {"Read input earlier", "(IK)", "The pad is read at the start of the logic frame, not the end.", "Lower input delay (-4..9 ms), picture unchanged. Applies now: toggle while idle."},
    {"Input-to-screen timing", "(TS)", "Measures button press to frame shown (median; add half a refresh).", "Measurement only. Applies now."},
    {"Count worker wakes", "(WakeStats)", "Counts job-worker wakes and sleeps (log line every 30 s).", "Measurement only. Applies now."},
    {"Skip empty effect lists", "(ED)", "Effect workers with nothing to update are not woken.", "Same output. Applies now. CPU only: no fps change on the Ally."},
    {"GPU wait fix", "(GF)", "Main thread and workers sleep until the GPU fence fires instead of spinning.", E_CPU},
    {"Close leaked handles", "(WL)", "Closes the per-frame frame-latency handle the game leaks (Windows; no-op on Proton).", "Picture unchanged. Applies now. Prevents a slow handle leak."},
    {"Wake gate", "(WG)", "No second wake-one while a woken worker of the pool is still on its way.", E_CPU},
    {"Logic ticks follow render rate", "(TG)", "EXPERIMENT: logic ticks follow rendered fps (floor 30). Needs the frame-limiter fix (L1).", "Changes timing behaviour. Applies now. Judge by feel; Ally effect not proven."},
    {"Retry-spin sleeps", "(DS)", "The main loop's 1 ms no-new-frame spin sleeps on a timer. Needs L1.", E_CPU},
    {"Plain loads in workers", "(LK)", "18 locked 'atomic loads' in worker code become plain loads.", E_CPU_NEXT},
    {"Collision prefetch", "(CP)", "Collision workers prefetch the next colliders (same results, less memory stall).", E_CPU},
    {"Stage prefetch", "(SPF)", "The stage-update worker prefetches placeables 4 ahead (same results).", E_CPU},
    {"Faster matrix math", "(MQ)", "Skeleton matrix build in SSE, bit-identical (about 1 % of each scheduler thread).", E_CPU},
    {"Texture pool lock once", "(PT1)", "Texture tile requests take the pool lock once per texture (same state, fewer lock pairs).", E_CPU},
    {"Skip dead collision lookup", "(DL)", "Collision update skips a part-map search whose result is thrown away.", E_CPU},
    {"Shorter no-frame retry", "(DQ)", "The main loop re-checks for a new frame every 200 us instead of a blind 1 ms spin.", E_CPU},
    {"Texture-loop prefetch", "(P1E7)", "The main thread prefetches the next texture's streaming data (same results).", E_CPU},
    {"Plain loads in main thread", "(MA)", "25 locked 'atomic loads' in the main thread's meshlet bookkeeping become plain loads.", E_CPU_NEXT},
    {"Faster part bounds", "(BPO)", "Per-part bounds reuse each part's rotation instead of rebuilding it every frame (bit-identical).", E_CPU},
    {"Glyph clear plain stores", "(W1B)", "The text atlas padding clear writes its zeros with plain stores (same bytes, no rep stosd per row).", E_CPU},
    {"Range copy render params", "(RPA)", "RenderParamsAssign copies its 21 field ranges in 16-byte moves instead of 29.6 KB of code (same bytes).", E_CPU},
    {"Async compute slots", "(AS)", "EXPERIMENT. Effects / grass / tree wind compute run on the game's own async queues (same shaders, game's fences).",
     "Picture unchanged. Applies now. GPU-bound only: watch fps / GPU ms. Off = stock queues."},
    {"Pipelines ready before first use", "(PJH)", "Pipelines the game built in earlier sessions are built at the title screen and handed over when first drawn.",
     "Picture unchanged. Applies now. Fewer first-sight hitches from the 2nd launch on. Off = the game compiles them itself."},
    {"Steady timer resolution", "(TR)", "Holds the 1 ms Windows timer for the session instead of the pad-reader threads switching it ~250x/s.",
     "Picture unchanged. Applies now. Windows only (no effect on Proton). Steadier Sleep timing; the log shows the call rate."},
    {"Fewer shadow tile redraws", "(VSMA)", "Shadow-map tiles still waiting for their turn are redrawn on 1 visit in VSMA_K, not every visit.",
     "Same shadows (unmarked tiles are never shown). Applies now. Cuts shadow redraw bursts while moving."},
    {"LPV update rate", "(LPVR)", "EXPERIMENT. Writes LPVUpdateFrequency = LPVR_N into the frame's render config (game: 1).",
     "Judge with screenshots. Applies now."},
    {"Sky ambient update rate", "(SKYR)", "EXPERIMENT. Writes SkylightAmbientUpdateFreq = SKYR_V into the render config (game: 1).",
     "Judge with screenshots. Applies now."},
    {"Fog temporal factor", "(FOGT)", "EXPERIMENT. Writes VolumetricFogTemporalFactor = FOGT_V (game: 0 = no history blend).",
     "Judge with screenshots. Applies now. Fog lags moving lights."},
    {"Cloud reprojection weight", "(CLDR)", "EXPERIMENT. Writes VolumetricCloudReprojectionBlendWeightCurrentFrame = CLDR_V (game: 0).",
     "Judge with screenshots. Applies now."},
    {"Render threads first", "(RTP)", "Draw prep, D3D12 submit and Present threads get a higher Windows priority than the logic workers.",
     "Picture unchanged. Applies now. For CPUs that are fully busy (6 cores and less); the logic ticks on the CPU time left."},
    {"Faster pipeline lookup", "(PCM)", "The D3D12 submit thread remembers the last pipelines it found instead of searching the cache each draw.",
     "Picture unchanged (same pipelines). Applies now. CPU-bound PCs: shorter submit tail per frame."},
    {"Batch GPU barriers", "(GPU)", "Batches the game's D3D12 resource barriers into fewer calls.", "Picture unchanged. Applies now. About 0 on the Ally, 2-3 % on PC."},
    {"Cheaper barriers A (Proton)", "(R2a)", "Buffer COMMON barriers get their real read state (cheaper wait on RADV).", "Picture unchanged. Applies now. A/B against the GPU ms line."},
    {"Cheaper barriers B (Proton)", "(R2b)", "EXPERIMENT. Same for UAV buffers. May flicker or show stale data.", "Judge with screenshots. Applies now."},
    {"Drop read->read barriers", "(R1 gpu)", "EXPERIMENT. Drops read->read superset barriers. Needs an image compare.", "Judge with screenshots. Applies now."},
    // 1.7.59 Extra performance (changes the look): extraperf.h; gains = notes\extra-performance-settings.md (pc2 RX 6650 XT 1080p / Ally)
    {"Preset", "(XPRE)", "Game = all rows off. Performance = cuts that are hard to see (clouds, GI, LOD, shadow distance; fog untouched). Potato = + coarser fog, AO off, 1-tap shadows.",
     "Changes the look. Applies now. Left/Right picks the next preset. Gains: see each row."},
    {"Cloud quality", "(XCLD)", "Fewer ray-march steps for the volumetric clouds (game: up to 256 per pixel).",
     "Blockier / noisier cloud edges. Applies now. PC RX 9070 (GPU-bound): no gain; Ally: untested."},
    {"Volumetric fog detail", "(XFOG)", "Coarser fog light grid than the game's lowest (LIGHT = 80x32); 'off' = VOLUMETRIC LIGHTEST.",
     "Blurrier god rays and fog lighting; off = no volumetric fog. Applies now. PC +1.5 % (48x16) / +2.8 % (off: glow gone, big look change)."},
    {"Global illumination resolution", "(XGI)", "The bounce-light / reflection gathers run at this share of the GI setting's resolution.",
     "Softer, laggier indirect light and reflections. Applies now. PC +3.6 % (x0.35)."},
    {"Shadow distance", "(XSHD)", "Sun shadows fade out and stop drawing casters closer to the camera (game: 250-500 m).",
     "Distant objects lose their shadows. Applies now. PC: no gain at the spawn tested."},
    {"Shadow filtering", "(XSHF)", "Fewer soft-shadow filter taps (game: 4).",
     "Harder, more jagged shadow edges. Applies now. PC: no gain measured."},
    {"Geometry detail", "(XLOD)", "Meshes switch to their simpler versions sooner (game MESHLET_QUALITY LIGHT = x2).",
     "Visible pop-in and simpler distant models. Applies now. PC: GPU time unchanged (CPU-side)."},
    {"Terrain detail", "(XTER)", "Shorter terrain tessellation distance and rate than LANDSCAPE LIGHTEST.",
     "Flatter, blockier ground in the distance. Applies now. PC: not measured."},
    {"GPU particles", "(XPRT)", "Share of GPU particle groups simulated and drawn (the engine's own per-view fraction).",
     "Thinner sparks, embers, mist; off hides GPU particles (attack tells may be effects). Applies now. PC: no gain measured."},
    {"Ambient occlusion off", "(XAO)", "Same values as the game's AMBIENT_OCCLUSION OFF, here so presets and A/B can switch it.",
     "Flatter contact shadows. Applies now. PC +1.8 %."},
    {"Camera effects off", "(XPOST)", "1 = motion blur + depth of field off, 2 = also bloom off (the game's own OFF values).",
     "No blur / no glow around lights. Applies now. PC GPU -2.8 % (fps noisy)."},
    {"Show FPS counter", "", "Real fps (the game's frames) and, with frame gen, the displayed fps incl. generated frames.", "Overlay only. Applies now."},
    {"Show frame time", "", "Adds the real frame time (ms) to the counter.", "Overlay only. Applies now."},
    {"Show game speed", "", "Adds the game speed: 1.00x = real time, below 1 = the game runs slow.", "Overlay only. Applies now."},
    {"Move FPS counter", "(corner)", "Which screen corner the counter sits in. Top-left starts below the game's own health bars.", "Overlay only. Applies now. Save to ini keeps it."},
    {"FPS counter text size", "", "1.0 = 8x16 px letters (x2 at 4K). Whole numbers look sharpest.", "Overlay only. Applies now."},
    {"Turn frame gen ON", "(FG)", "Same as the game's Options > Frame Generation. Left/Right picks the type. The screen blinks while it switches.", "Changes the picture (extra frames). Applies now. Proton: ON->OFF applies at the next launch."},
    {"Frame gen:", "", "Real fps (game thread) vs displayed fps (FSR presenter thread): running when displayed > 1.15x real.", "Status only."},
    {"Upscaler (game setting)", "", "The game's upscaler setting. FSR frame gen does not need the FSR upscaler.", "Status only."},
    {"Nudge counter right", "(offset X)", "Moves the counter away from its corner's left/right screen edge (pixels at 1080p).", "Overlay only. Applies now. A resets to 0."},
    {"Nudge counter down", "(offset Y)", "Moves the counter away from its corner's top/bottom edge (pixels at 1080p).", "Overlay only. Applies now. A resets to the corner default."},
    {"Game's own performance display", "", "Nioh 3's built-in fps overlay is compiled out of the retail game: no setting exists to toggle.", "Nothing to switch. The counter above is the mod's own."},
    {"Mod menu reminder (top-right)", "(PauseHint)", "The small 'Mod menu: F10 / Esc x3 / Start x3' note. Always, only while a game menu is open, or off.", "Overlay only. Applies now. Save to ini keeps it."},
    {"Skip loading tip screen", "(AutoContinueLoad)", "Presses Continue for you when a load finishes.", "Off by default. Applies now; saved to the ini at once."},
    {"Job-thread limit (handhelds)", "(WorkerCap)", "Caps the job thread pools at N (try 10, then 8). Off on desktops.", "Picture unchanged. Applies at the next launch. Lowers CPU use on handhelds."},
    {"Keep 3 more pool threads", "(P12)", "Keeps 3 more pool threads on CPUs with <= 12 threads. Experimental.", "Picture unchanged. Applies at the next launch."},
    {"Sleeping limiter on Proton", "(L1)", "Install L1 under Wine/Proton too (Wine may not honour the timer).", "Picture unchanged. Applies at the next launch."},
    {"Sleeping FG presenter on Proton", "(F1)", "Install F1 under Wine/Proton too (same timer caveat as L1).", "Picture unchanged. Applies at the next launch."},
    {"Save to ini", "", "Saves these choices to Nioh3EngineFix.ini; a frame-gen change made here also goes to the game's config.xml.", "Applies at the next launch."},
    {"Close menu", "", "Also: B / Esc / the menu key / the open gesture.", ""},
};

struct OrderEnt {
    int id;
    const char* section;  // header drawn before this item (nullptr = none)
};
const OrderEnt kOrder[] = {
    {kFps, "Display & overlay"}, {kFrameTime, nullptr}, {kSpeed, nullptr}, {kPos, nullptr}, {kOffX, nullptr}, {kOffY, nullptr},
    {kScale, nullptr}, {kHint, nullptr}, {kPerfNote, nullptr},
    {kFgState, "Frame generation"}, {kFg, nullptr}, {kUpscaler, nullptr},
    {kAutoCont, "Gameplay"},
    {kEpPre, "Extra performance (changes the look)"}, {kEp0, nullptr}, {kEp1, nullptr}, {kEp2, nullptr}, {kEp3, nullptr},
    {kEp4, nullptr}, {kEp5, nullptr}, {kEp6, nullptr}, {kEp7, nullptr}, {kEp8, nullptr}, {kEp9, nullptr},
    {kVrr, "Performance fixes (safe)"}, {kW3, nullptr}, {kW3Mode, nullptr}, {kR1, nullptr}, {kL1, nullptr}, {kF1, nullptr}, {kWk, nullptr},
    {kLat, nullptr}, {kGpu, nullptr},
    {kXW6, "Experimental (CPU, A/B live)"}, {kXGF, nullptr}, {kXIK, nullptr}, {kXED, nullptr}, {kXWL, nullptr}, {kXB, nullptr}, {kXPP, nullptr}, {kXFPC, nullptr}, {kXAABB, nullptr}, {kXM1, nullptr},
    {kXX1, nullptr}, {kXE1, nullptr}, {kXM2, nullptr}, {kXJ1, nullptr}, {kXS1, nullptr}, {kXWG, nullptr}, {kXTG, nullptr},
    {kXDS, nullptr}, {kXLK, nullptr}, {kXCP, nullptr}, {kXSPF, nullptr}, {kXMQ, nullptr}, {kXPT1, nullptr}, {kXDL, nullptr},
    {kXDQ, nullptr}, {kXP1E7, nullptr}, {kXMA, nullptr}, {kXBPO, nullptr}, {kXW1B, nullptr}, {kXRPA, nullptr}, {kXTR, nullptr},
    {kXAS, "Experimental (GPU, A/B live)"}, {kXPJH, nullptr}, {kXVSMA, nullptr}, {kXLPVR, nullptr}, {kXSKYR, nullptr},
    {kXFOGT, nullptr}, {kXCLDR, nullptr}, {kXRTP, "Experimental (CPU scheduling, A/B live)"}, {kXPCM, nullptr},
    {kGR2a, "Experimental (GPU, Proton)"}, {kGR2b, nullptr}, {kGR1, nullptr},
    {kXTS, "Diagnostics / logging"}, {kXWS, nullptr},
    {kWorkerCap, "Next launch (saved to the ini)"}, {kP12, nullptr}, {kL1Wine, nullptr}, {kFgWine, nullptr},
    {kSave, "Save & exit"}, {kClose, nullptr},
};
constexpr int kOrderN = (int)(sizeof(kOrder) / sizeof(kOrder[0]));
static_assert(kOrderN == (int)kCount, "every menu item must be in kOrder exactly once");
int OrderPos(int id) {
    for (int i = 0; i < kOrderN; ++i)
        if (kOrder[i].id == id) return i;
    return 0;
}

int g_sel = kFps;  // first row of the menu (kOrder[0]); Up from it wraps to 'Close menu'
bool g_dirty = false;
int g_bootWorkerCap = 0, g_bootP12 = 0, g_bootL1Wine = 0, g_bootFgWine = 0;
int g_expIni0[kNumExp] = {};  // value at launch
bool g_expAuto[kNumExp] = {};  // ini held -1 (auto by OS) at launch
int g_latBoot = 0;
int g_expIni[kNumExp] = {};  // what Save writes for each experimental patch (live state when installed)

bool Visible(int id) {
    switch (id) {
        case kW3Mode: return g_host.wine && g_host.w3Enable && g_host.w3UseSleep;
        case kL1Wine:
        case kFgWine: return g_host.wine;
        case kFg: case kFgState: case kUpscaler: return g_host.knownBuild && g_host.base;  // engine slots: v2.02 only
        case kGpu: return gpuopt::GetState() != gpuopt::State::Off;
        case kGR2a: case kGR2b: case kGR1: return g_host.wine && gpuopt::GetState() != gpuopt::State::Off;
        default: return true;
    }
}

// status rows: shown, never selected
bool ReadOnly(int id) { return id == kFgState || id == kUpscaler; }

bool Available(int id) {
    switch (id) {
        case kVrr: return g_host.vrrEnable != nullptr;
        case kW3: return g_host.w3Enable != nullptr;
        case kW3Mode: return g_host.w3Enable && g_host.w3UseSleep;
        case kR1: return g_host.r1Timeout != nullptr;
        case kL1: return g_host.l1Enable != nullptr;
        case kF1: return g_host.fgEnable != nullptr;
        case kWk: return g_host.wkEnable != nullptr;
        case kAutoCont: return g_host.autoContinue != nullptr;
        case kEpPre: return extraperf::AnyAvailable();
        case kFg: return g_fgOk;
        case kGpu:
        case kGR2a: case kGR2b: case kGR1:
            return gpuopt::GetState() == gpuopt::State::Active || gpuopt::GetState() == gpuopt::State::Paused;
        default:
            if (IsEp(id)) return extraperf::Available(id - kEp0);
            return true;
    }
}

constexpr uint32_t cWhite = 0xFFFFFFFF, cGray = 0xFFB4B4B4, cDim = 0xFF8C8C8C, cGreen = 0xFF6CFF6C, cYellow = 0xFF40E6FF,
                   cOrange = 0xFF3C8CFF, cCyan = 0xFFFFE08C, cGold = 0xFF5AC8F0, cPanel = 0xA0000000, cMenuPanel = 0xD8000000,
                   cHighlight = 0xE0703818, cRule = 0xC05AC8F0;

const char* NaText(int id) {
    if (id == kL1 && g_host.l1 == FixState::WineSkipped) return "off (Proton)";
    if (id == kF1 && g_host.f1 == FixState::WineSkipped) return "off (Proton)";
    return "not installed";
}

bool LiveBool(int id) {
    switch (id) {
        case kVrr: return *g_host.vrrEnable != 0;
        case kW3: return *g_host.w3Enable != 0;
        case kW3Mode: return *g_host.w3UseSleep != 0;
        case kL1: return *g_host.l1Enable != 0;
        case kF1: return *g_host.fgEnable != 0;
        case kWk: return *g_host.wkEnable != 0;
        default:
            if (IsExp(id) && g_host.exp[id - kXW6].enable) return *g_host.exp[id - kXW6].enable != 0;
            return false;
    }
}

// experimental rows: ON/OFF live when installed with a switch, otherwise the ini value for the next launch
void ExpValue(int id, char* buf, size_t n, uint32_t& color, uint32_t cGreenC, uint32_t cGrayC, uint32_t cYellowC, uint32_t cDimC) {
    const int e = id - kXW6;
    const ExpFix& x = g_host.exp[e];
    if (x.enable) {
        const bool on = *x.enable != 0;
        if (e == kExpTS && on) {  // the measurement itself: median input -> display
            const long us = g_host.tsInDispUs ? *g_host.tsInDispUs : -1;
            if (us >= 0) snprintf(buf, n, "ON  %.1f ms", us / 1000.0);
            else snprintf(buf, n, "ON  measuring...");
            color = cGreenC;
            return;
        }
        if (e == kExpTG && on) {  // governor state: the cap it publishes, or why it does not engage
            const long st = g_host.tgState ? *g_host.tgState : 0;
            const uint32_t cap = g_host.tgCap ? *g_host.tgCap : 0;
            if (st == 1 && cap) snprintf(buf, n, "ON  ticks <= %u", cap);
            else if (st == 2) snprintf(buf, n, "ON  REFUSED: L1 off");
            else if (st == 3) snprintf(buf, n, "ON  (VRR not armed)");
            else snprintf(buf, n, "ON  (idle)");
            color = st == 2 ? cYellowC : cGreenC;
            return;
        }
        snprintf(buf, n, "%s", on ? "ON" : "OFF");
        color = on ? cGreenC : cGrayC;
        return;
    }
    const bool changed = g_expIni[e] != x.ini;
    if (x.ini && !x.applied && !changed) {
        snprintf(buf, n, "%s", g_host.knownBuild ? "failed - see the log" : "v2.02 only");
        color = cDimC;
        return;
    }
    snprintf(buf, n, "%s%s%s", g_expIni[e] ? "ON" : "OFF", changed ? "  * restart" : (x.applied ? "" : "  (ini)"),
             g_expAuto[e] && g_expIni[e] == g_expIni0[e] ? "  (auto)" : "");
    color = changed ? cYellowC : (g_expIni[e] ? cGreenC : cGrayC);
}

// frame-generation rows (status from osd::FrameGen; also shown when the switch itself is not available)
void FgValue(int id, char* buf, size_t n, uint32_t& color) {
    const osd::FgStatus s = osd::FrameGen();
    const char* name = osd::FgTypeName(s.setting);
    if (id == kUpscaler) {
        const char* u = osd::FgTypeName(s.upscaler);
        snprintf(buf, n, "%s", !u ? "?" : s.upscaler == 0 ? "OFF (native)" : u);
        color = u ? cWhite : cDim;
        return;
    }
    if (id == kFgState) {  // 'ON - running (R -> D fps)' green / 'ON - paused (...)' orange / 'OFF' grey
        static const uint32_t kCol[4] = {cGray, cGreen, cOrange, cYellow};
        const int cls = osd::FgStateText(s, buf, n);
        color = kCol[cls & 3];
        if (g_fgNextLaunch && s.setting != 0) {
            snprintf(buf, n, "ON - turns OFF at next launch");
            color = cOrange;
        }
        if (g_fgSoft && s.setting != 0) {
            snprintf(buf, n, "OFF - paused by the mod (no rebuild)");
            color = cGray;
        }
        return;
    }
    // kFg
    const char* ro = Available(kFg) ? "" : "  (read-only)";
    if (!name) {
        snprintf(buf, n, "?%s", ro);
        color = cDim;
    } else if (g_fgNextLaunch && s.setting != 0) {
        snprintf(buf, n, "FG off at next launch");
        color = cOrange;
    } else if (g_fgSoft && s.setting != 0) {
        snprintf(buf, n, "type: FSR (paused)%s", ro);
        color = cGray;
    } else if (s.setting == 0) {
        snprintf(buf, n, "type: FSR%s", ro);  // what A/Enter would switch on
        color = cGray;
    } else {
        snprintf(buf, n, "type: %s%s", name, ro);
        color = s.active ? cGreen : cYellow;
    }
}

void Value(int id, char* buf, size_t n, uint32_t& color) {
    buf[0] = 0;
    color = cWhite;
    if (id == kFg || id == kFgState || id == kUpscaler) {
        FgValue(id, buf, n, color);
        return;
    }
    if (IsExp(id)) {
        ExpValue(id, buf, n, color, cGreen, cGray, cYellow, cDim);
        return;
    }
    if (id == kGR2a || id == kGR2b || id == kGR1) {
        const bool on = gpuopt::Rewrite(id - kGR2a);
        snprintf(buf, n, "%s", on ? "ON" : "off");
        color = on ? (id == kGR2a ? cGreen : cYellow) : cGray;
        return;
    }
    if (id == kGpu) {
        const gpuopt::State s = gpuopt::GetState();
        snprintf(buf, n, "%s", s == gpuopt::State::Active ? "ON" : s == gpuopt::State::Paused ? "OFF (hooked)"
                                   : s == gpuopt::State::Waiting ? "waiting for the device" : "failed - see the log");
        color = s == gpuopt::State::Active ? cGreen : s == gpuopt::State::Paused ? cGray : cDim;
        return;
    }
    if (id == kLat) {
        const osd::LatencyStatus ls = osd::Latency();
        char game[16];
        if (ls.game > 0) snprintf(game, sizeof(game), "%d", ls.game);
        else snprintf(game, sizeof(game), "?");
        if (ls.state == 2) snprintf(buf, n, "%s (n/a: no waitable swapchain)", ls.want ? std::to_string(ls.want).c_str() : "game");
        else if (!ls.want) snprintf(buf, n, "game (%s)", game);
        else snprintf(buf, n, "%d (game %s)", ls.want, game);
        color = ls.state == 2 ? cDim : ls.want ? cGreen : cGray;
        return;
    }
    if (!Available(id)) {
        snprintf(buf, n, "%s", NaText(id));
        color = cDim;
        return;
    }
    if (IsEp(id)) {  // 1.7.59 Extra performance level rows
        const int e = id - kEp0, l = extraperf::Level(e);
        snprintf(buf, n, "%s", extraperf::LevelName(e, l));
        color = l ? cCyan : cGray;
        return;
    }
    if (id == kEpPre) {
        const int m = extraperf::MatchPreset();
        snprintf(buf, n, "%s", extraperf::PresetName(m));
        color = m == 0 ? cGray : m > 0 ? cGreen : cCyan;
        return;
    }
    osd::OverlaySettings& o = osd::Overlay();
    auto onoff = [&](bool v) {
        snprintf(buf, n, "%s", v ? "ON" : "OFF");
        color = v ? cGreen : cGray;
    };
    auto boot = [&](bool v, bool changed) {
        snprintf(buf, n, "%s%s", v ? "ON" : "OFF", changed ? "  * restart" : "");
        color = changed ? cYellow : (v ? cGreen : cGray);
    };
    switch (id) {
        case kVrr: case kW3: case kL1: case kF1: case kWk: onoff(LiveBool(id)); break;
        case kW3Mode:
            snprintf(buf, n, "%s", *g_host.w3UseSleep ? "Sleep 1 ms" : "MsgWait");
            color = *g_host.w3UseSleep ? cGreen : cYellow;
            break;
        case kR1: {
            const uint32_t v = *g_host.r1Timeout;
            if (v) snprintf(buf, n, "%u ms", v);
            else snprintf(buf, n, "0 = stock poll");
            color = v ? cGreen : cGray;
            break;
        }
        case kFps: onoff(o.fpsCounter != 0); break;
        case kAutoCont: {
            const bool on = *g_host.autoContinue != 0;
            const long used = g_host.autoContinueCount ? g_host.autoContinueCount() : 0;
            if (on && used > 0) snprintf(buf, n, "ON  (used)");
            else snprintf(buf, n, "%s", on ? "ON" : "OFF");
            color = on ? cGreen : cGray;
            break;
        }
        case kHint:
            snprintf(buf, n, "%s", g_cfg.pauseHint == 2 ? "Always" : g_cfg.pauseHint ? "In game menus" : "OFF");
            color = g_cfg.pauseHint ? cGreen : cGray;
            break;
        case kFrameTime: onoff(o.frameTime != 0); break;
        case kSpeed: onoff(o.speed != 0); break;
        case kPos: snprintf(buf, n, "%s", osd::PositionName(o.pos)); break;
        case kScale: snprintf(buf, n, "%.1f", o.scale); break;
        case kOffX: snprintf(buf, n, "%d px", osd::OffsetX()); break;
        case kOffY: snprintf(buf, n, "%d px%s", osd::OffsetY(), o.offUser ? "" : " (default)"); break;
        case kPerfNote: snprintf(buf, n, "not in this game build"); color = cDim; break;
        case kWorkerCap: {
            const bool ch = g_bootWorkerCap != g_host.workerCap;
            if (g_bootWorkerCap) snprintf(buf, n, "%d threads%s", g_bootWorkerCap, ch ? "  * restart" : "");
            else snprintf(buf, n, "off%s", ch ? "  * restart" : "");
            color = ch ? cYellow : (g_bootWorkerCap ? cGreen : cGray);
            break;
        }
        case kP12: boot(g_bootP12 != 0, g_bootP12 != g_host.p12); break;
        case kL1Wine: boot(g_bootL1Wine != 0, g_bootL1Wine != g_host.l1OnWine); break;
        case kFgWine: boot(g_bootFgWine != 0, g_bootFgWine != g_host.fgOnWine); break;
        default: break;
    }
}

// ---- save (background thread: WritePrivateProfileString rewrites the file per key) ------------------------------------
struct SaveJob {
    std::wstring ini;
    std::vector<std::wstring> entries;  // section, key, value triples
    std::wstring gameCfg;               // config.xml to update (empty = leave it alone)
    std::string fgValue;                // FRAME_GENERATION value for it ("OFF", "FSR", ...)
};
volatile LONG g_saveState = 0;  // 0 idle, 1 running, 2 ok, 3 failed
volatile LONG g_saveErr = 0;
volatile LONG g_cfgXmlState = 0;  // last save: 0 config.xml not touched, 1 written, 2 already that value, 3 failed
ULONGLONG g_saveShownAt = 0;

DWORD WINAPI SaveThread(LPVOID p) {
    SaveJob* job = static_cast<SaveJob*>(p);
    DWORD err = 0;
    size_t ok = 0;
    for (size_t i = 0; i + 2 < job->entries.size(); i += 3) {
        if (WritePrivateProfileStringW(job->entries[i].c_str(), job->entries[i + 1].c_str(), job->entries[i + 2].c_str(),
                                       job->ini.c_str()))
            ++ok;
        else if (!err)
            err = GetLastError();
    }
    WritePrivateProfileStringW(nullptr, nullptr, nullptr, job->ini.c_str());  // flush the profile cache
    if (err) MLOG("save: %zu of %zu keys written, first error %lu", ok, job->entries.size() / 3, err);
    else MLOG("save: %zu keys written to the ini", ok);
    const LONG xml = job->fgValue.empty() ? 0 : (LONG)WriteGameConfigFg(job->gameCfg, job->fgValue);
    InterlockedExchange(&g_cfgXmlState, xml);
    g_saveErr = (LONG)err;
    InterlockedExchange(&g_saveState, err || xml == 3 ? 3 : 2);
    delete job;
    return 0;
}

void StartSave() {
    if (g_saveState == 1) return;
    SaveJob* job = new (std::nothrow) SaveJob;
    if (!job) return;
    job->ini = g_ini;
    auto add = [&](const wchar_t* sec, const wchar_t* key, const std::wstring& val) {
        job->entries.push_back(sec);
        job->entries.push_back(key);
        job->entries.push_back(val);
    };
    auto b = [](bool v) { return std::wstring(v ? L"1" : L"0"); };
    if (Available(kVrr)) add(L"Patches", L"VRR", b(LiveBool(kVrr)));
    if (Available(kW3)) add(L"Patches", L"W3", b(LiveBool(kW3)));
    if (Visible(kW3Mode) && Available(kW3Mode)) add(L"Patches", L"W3WineSleep", b(LiveBool(kW3Mode)));
    if (Available(kR1)) add(L"Tuning", L"R1_TimeoutMs", std::to_wstring(*g_host.r1Timeout));
    if (Available(kL1)) add(L"Patches", L"L1", b(LiveBool(kL1)));
    if (Available(kF1)) add(L"Patches", L"FG", b(LiveBool(kF1)));
    if (Available(kWk)) add(L"Patches", L"WK", b(LiveBool(kWk)));
    for (int e = 0; e < kNumExp; ++e) {
        int v = g_expIni[e];
        // ED / WG / DS / CP / SPF installed with the switch off stay installed (2) so the live A/B keeps working next
        // launch (TG needs no install: its 0 already keeps the live switch)
        if ((e == kExpED || e == kExpWG || e == kExpDS || e == kExpCP || e == kExpSPF || e == kExpMQ || e == kExpPT1 || e == kExpDL ||
             e == kExpDQ || e == kExpP1E7 || e == kExpBPO || e == kExpAABB || e == kExpW1B || e == kExpRPA || e == kExpAS || e == kExpPJH || e == kExpTR ||
             e == kExpVSMA || e == kExpLPVR || e == kExpSKYR || e == kExpFOGT || e == kExpCLDR || e == kExpRTP || e == kExpPCM) && g_host.exp[e].enable &&
            !*g_host.exp[e].enable && v == 0)
            v = 2;
        add(L"Patches", kExpIniKey[e], g_expAuto[e] && v == g_expIni0[e] ? std::wstring(L"-1") : std::to_wstring(v));  // unchanged auto stays auto
    }
    if (extraperf::AnyAvailable()) {  // 1.7.59 Extra performance: every level explicitly; Preset 0 so the levels are what loads
        for (int e = 0; e < extraperf::kNumOpts; ++e) {
            wchar_t k[16];
            swprintf(k, 16, L"%hs", extraperf::Key(e));
            add(L"ExtraPerf", k, std::to_wstring(extraperf::Level(e)));
        }
        add(L"ExtraPerf", L"Preset", L"0");
    }
    if (Available(kGpu))  // hooks installed only for the GP profiler (Barriers 0 at launch) stay 0 unless switched on here
        add(L"GpuOpt", L"Barriers", gpuopt::Batching() ? L"1" : gpuopt::BarriersIni() ? L"2" : L"0");
    if (Available(kGpu) && g_host.wine) {
        add(L"GpuOpt", L"R2a", gpuopt::Rewrite(0) ? L"1" : L"0");
        add(L"GpuOpt", L"R2b", gpuopt::Rewrite(1) ? L"1" : L"0");
        add(L"GpuOpt", L"R1", gpuopt::Rewrite(2) ? L"1" : L"0");
    }
    add(L"Latency", L"MaxFrameLatency", osd::Overlay().maxLatencyAuto && osd::Overlay().maxLatency == g_latBoot ? std::wstring(L"-1") : std::to_wstring(osd::Overlay().maxLatency));
    add(L"Patches", L"WorkerCap", std::to_wstring(g_bootWorkerCap));
    add(L"Patches", L"P12", b(g_bootP12 != 0));
    if (g_host.wine) {
        add(L"Patches", L"L1OnWine", b(g_bootL1Wine != 0));
        add(L"Patches", L"FGOnWine", b(g_bootFgWine != 0));
    }
    osd::OverlaySettings& o = osd::Overlay();
    if (o.offUser) {
        add(L"Overlay", L"OffsetX", std::to_wstring(o.offX));
        add(L"Overlay", L"OffsetY", std::to_wstring(o.offY));
    }
    add(L"Overlay", L"FpsCounter", b(o.fpsCounter != 0));
    if (Available(kAutoCont)) add(L"General", L"AutoContinueLoad", b(*g_host.autoContinue != 0));
    add(L"Menu", L"PauseHint", std::to_wstring(g_cfg.pauseHint));
    add(L"Overlay", L"FrameTime", b(o.frameTime != 0));
    add(L"Overlay", L"SpeedFactor", b(o.speed != 0));
    {
        wchar_t w[32];
        swprintf(w, 32, L"%hs", osd::PositionName(o.pos));
        add(L"Overlay", L"Position", w);
        swprintf(w, 32, L"%.1f", o.scale);
        add(L"Overlay", L"Scale", w);
    }
    // frame generation changed from this menu: also into the game's config.xml (only that value; backup first)
    if (g_fgChanged && Available(kFg)) {
        const char* v = osd::FgTypeName(FgSetting());
        if (!v) MLOG("save: FRAME_GENERATION slot unreadable - config.xml not changed");
        else if (g_host.wine && strcmp(v, "OFF") != 0)
            MLOG("save: Proton - frame generation %s is kept for this session only, config.xml not changed", v);
        else if (!FgDescMatches()) MLOG("save: the game's FRAME_GENERATION descriptor differs from OFF/DLSS/FSR/XeSS - config.xml not changed");
        else {
            job->gameCfg = GameConfigPath();
            job->fgValue = v;
        }
    }
    InterlockedExchange(&g_cfgXmlState, 0);
    InterlockedExchange(&g_saveState, 1);
    g_saveShownAt = 0;
    MLOG("save: writing %zu keys", job->entries.size() / 3);
    if (HANDLE h = CreateThread(nullptr, 0, SaveThread, job, 0, nullptr)) {
        CloseHandle(h);
        g_dirty = false;
    } else {
        MLOG("save: CreateThread failed (%lu)", GetLastError());
        g_saveErr = (LONG)GetLastError();
        InterlockedExchange(&g_saveState, 3);
        delete job;
    }
}

// ---- open / close / change ---------------------------------------------------------------------------------------------
const char* SrcName(LONG src) { return src == 1 ? g_cfg.keyName : src == 2 ? "controller gesture" : src == 3 ? "Esc x3" : "?"; }

void Open(const char* how) {
    InterlockedExchange(&g_pressed, 0);
    for (PadSlot& s : g_pad) s.latch = true;
    InterlockedExchange(&g_open, 1);
    MessageBeep(MB_OK);  // audible confirmation of the hold gesture, even if the overlay can't draw
    if (!Visible(g_sel)) g_sel = kOrder[0].id;
    MLOG("opened (%s) - game pad%s input is held neutral while open", how, g_keyHooked ? " and navigation-key" : "");
}

void Close(const char* how) {
    for (PadSlot& s : g_pad) s.latch = true;
    InterlockedOr(&g_keyLatch, (LONG)K_NAV);
    InterlockedExchange(&g_open, 0);
    MessageBeep(MB_ICONASTERISK);
    MLOG("closed (%s)%s", how, g_dirty ? " - unsaved changes are active until the game exits" : "");
}

// AutoContinueLoad: live switch + the one ini key written at once on a background thread (the rest waits for 'Save to ini')
DWORD WINAPI SaveAutoContinueThread(void* p) {
    const bool on = p != nullptr;
    if (!WritePrivateProfileStringW(L"General", L"AutoContinueLoad", on ? L"1" : L"0", g_ini.c_str()))
        MLOG("save: [General] AutoContinueLoad could not be written (%lu)", GetLastError());
    else
        MLOG("save: [General] AutoContinueLoad = %d written to the ini", on ? 1 : 0);
    return 0;
}
void SetAutoContinue(bool on) {
    *g_host.autoContinue = on ? 1 : 0;
    if (HANDLE h = CreateThread(nullptr, 0, SaveAutoContinueThread, on ? (void*)1 : nullptr, 0, nullptr)) CloseHandle(h);
}

void SetLiveBool(int id, bool v) {
    switch (id) {
        case kVrr: *g_host.vrrEnable = v ? 1u : 0u; break;
        case kW3: InterlockedExchange(g_host.w3Enable, v ? 1 : 0); break;
        case kW3Mode: InterlockedExchange(g_host.w3UseSleep, v ? 1 : 0); break;
        case kL1: InterlockedExchange(g_host.l1Enable, v ? 1 : 0); break;
        case kF1: InterlockedExchange(g_host.fgEnable, v ? 1 : 0); break;
        case kWk: *g_host.wkEnable = v ? 1u : 0u; break;
        default: break;
    }
}

// 1.7.44 [Menu] FrameGenToggle = 3 under Proton ("Proton safe FG", notes\fg-proton-fix.md). OFF <-> FSR only.
//   * on: from a paused FG (FGP) = resume, no swapchain rebuild. From OFF = the game's setter (the live OFF -> FSR rebuild
//     ran 3/3 on the Ally). Then the crash marker is armed FIRST and config.xml gets FSR, so the next launch starts with
//     FG, and a crash in this or a later FG session puts OFF back at the next start (ProtonFgEarly).
//   * off: never the game's FSR -> OFF rebuild (it crashed the game ~2 s later on the Ally, v9 log). FGP pauses FSR FG at
//     once (the engine's own menu pause: real frames only); config.xml gets OFF, so the next launch has no FG swapchain.
//     Without FGP: config.xml OFF only, FG stays until the next launch (the FrameGenToggle 2 behaviour).
void ChangeFgSafe(ULONGLONG t) {
    const int cur = FgSetting();
    if (cur < 0) return;
    const int eff = (g_fgSoft || g_fgNextLaunch) ? 0 : cur;
    const std::wstring path = GameConfigPath();
    g_fgChangedAt = t;
    if (eff != 0) {  // -> OFF
        if (g_host.fgPause) {
            *g_host.fgPause = 1;
            g_fgSoft = true;
        }
        g_fgExitWant = "OFF";
        SetFgMarker();  // the clean-exit OFF rule needs the marker (no-op when already armed)
        const int r = path.empty() ? 3 : WriteGameConfigFg(path, "OFF");
        g_fgNextLaunch = !g_host.fgPause;
        g_dirty = true;
        MLOG("Frame generation %s -> OFF (Proton safe FG): %s; config.xml %s", osd::FgTypeName(cur),
             g_host.fgPause ? "FSR FG paused now through the engine's own FG pause (FGP) - no swapchain rebuild"
                            : "FGP not installed - FG keeps running until the next launch",
             r == 3 ? "could NOT be written (OFF is still forced at a clean exit)" : "set to OFF for the next launch");
        return;
    }
    // -> FSR
    if (cur == 2 && (g_fgSoft || g_fgNextLaunch)) {
        if (g_fgSoft) *g_host.fgPause = 0;
        MLOG("Frame generation back ON (Proton safe FG): %s - no swapchain rebuild",
             g_fgSoft ? "FGP pause cleared" : "the 'off at next launch' is cancelled");
        g_fgSoft = false;
    } else if (cur == 0) {
        const int got = CallFgWrite(2);
        if (got != 2) {
            MLOG("Frame generation OFF -> FSR (Proton safe FG): the game's setter left it at %s", osd::FgTypeName(got) ? osd::FgTypeName(got) : "?");
            return;
        }
        osd::NoteFgSwitch();
        MLOG("Frame generation OFF -> FSR (Proton safe FG) via the game's FrameGenSettingWrite; the game rebuilds its swapchain "
             "in a few frames%s", g_host.fgcOn ? " (FGC copy-job guard installed)" : " - WARNING: FGC guard not installed");
    } else {
        MLOG("Frame generation: Proton safe FG switches only OFF <-> FSR (the setting is %s) - nothing changed", osd::FgTypeName(cur));
        return;
    }
    g_fgLastOn = 2;
    g_fgNextLaunch = false;
    g_fgExitWant = "FSR";
    SetFgMarker();  // armed BEFORE config.xml holds FSR (a crash from here on puts OFF back at the next start)
    const int r = path.empty() ? 3 : WriteGameConfigFg(path, "FSR");
    g_dirty = true;
    MLOG("Proton safe FG: config.xml FRAME_GENERATION %s", r == 3 ? "could NOT be set to FSR (FG is on for this session only)"
                                                                : "= FSR - the next launch starts with FG (crash guard: marker)");
    if (r == 3) g_fgExitWant.clear();
}

// Frame generation: A/Enter = OFF <-> last type (FSR), Left/Right = cycle OFF / FSR / XeSS / DLSS (available ones).
// One switch per 2 s: every change makes the game rebuild its swapchain.
void ChangeFg(int dir) {
    if (!Available(kFg)) return;
    const ULONGLONG t = Now();
    if (g_fgChangedAt && t - g_fgChangedAt < 2000) return;
    if (g_host.wine && g_cfg.fgToggle == 3) {
        ChangeFgSafe(t);
        return;
    }
    const int cur = FgSetting();
    if (cur < 0) return;
    int want = cur;
    if (dir == 0) {
        want = cur ? 0 : (FgTypeAvailable(g_fgLastOn) ? g_fgLastOn : 2);
    } else {
        int i = 0;
        while (i < 3 && kFgCycle[i] != cur) ++i;
        for (int k = 0; k < 4; ++k) {
            i = (i + (dir > 0 ? 1 : 3)) % 4;
            if (FgTypeAvailable(kFgCycle[i])) break;
        }
        want = kFgCycle[i];
    }
    if (want == cur) return;
    if (g_host.wine && cur != 0 && want == 0) {
        // Proton: rebuilding the swapchain away from FSR frame generation crashed the game (v9 log). So FG ON -> OFF is
        // applied at the next launch: config.xml gets FRAME_GENERATION OFF now, the game keeps running as it is.
        g_fgChangedAt = t;
        const std::wstring path = GameConfigPath();
        const int r = path.empty() ? 3 : WriteGameConfigFg(path, "OFF");
        if (r != 3) {
            if (g_fgAtStart == "OFF") SetFgMarker();
            g_fgNextLaunch = true;
            g_dirty = true;
        }
        MLOG("Frame generation %s -> OFF on Proton: %s (live swapchain rebuild skipped - it crashed the game)",
             osd::FgTypeName(cur), r != 3 ? "config.xml set to OFF, takes effect at the next launch" : "config.xml could NOT be written");
        return;
    }
    const int got = CallFgWrite(want);
    g_fgChangedAt = t;
    const char* a = osd::FgTypeName(cur);
    const char* b = osd::FgTypeName(want);
    if (got != want) {
        const char* g = osd::FgTypeName(got);
        MLOG("Frame generation %s -> %s: the game's setter left it at %s (type not available here)", a, b, g ? g : "?");
        return;
    }
    // as the game's SetOptionValue does: its next SaveConfigIfDirty writes config.xml. Not for FG on under Proton:
    // if the FG swapchain crashes the game there, the next launch must still start with the old (working) config.
    const bool persist = !(g_host.wine && want != 0);
    if (persist) SetConfigDirty();
    osd::NoteFgSwitch();
    g_fgChanged = true;
    g_dirty = true;
    if (want) g_fgLastOn = want;
    NoteFgSwitchProton(want);
    MLOG("Frame generation %s -> %s via the game's FrameGenSettingWrite (%s); the game rebuilds its swapchain in a few "
         "frames", a, b, persist ? "config marked dirty" : "Proton: this session only, never saved");
}

// dir: -1 left, +1 right, 0 = A / Enter
void Change(int id, int dir) {
    if (id == kFg) {
        ChangeFg(dir);
        return;
    }
    if (ReadOnly(id)) return;
    if (id == kSave) {
        if (dir == 0) StartSave();
        return;
    }
    if (id == kClose) {
        if (dir == 0) Close("Close menu item");
        return;
    }
    if (!Available(id)) return;
    osd::OverlaySettings& o = osd::Overlay();
    if (IsExp(id)) {
        const int e = id - kXW6;
        if (volatile uint32_t* sw = g_host.exp[e].enable) {
            *sw = *sw ? 0u : 1u;
            g_expIni[e] = *sw ? 1 : 0;
        } else {
            g_expIni[e] = !g_expIni[e];
        }
    } else if (IsEp(id)) {  // 1.7.59: Left/Right step the level, A cycles (wraps to the game's value)
        const int e = id - kEp0, nl = extraperf::Levels(e), l = extraperf::Level(e);
        extraperf::SetLevel(e, dir == 0 ? (l + 1) % nl : std::clamp(l + dir, 0, nl - 1));
    } else if (id == kEpPre) {  // next / previous preset, applied at once
        int m = extraperf::MatchPreset();
        if (m < 0) m = 0;
        extraperf::ApplyPreset((m + (dir < 0 ? extraperf::kNumPresets - 1 : 1)) % extraperf::kNumPresets);
    } else if (id == kGpu) {
        gpuopt::SetBatching(!gpuopt::Batching());
    } else if (id == kGR2a || id == kGR2b || id == kGR1) {
        gpuopt::SetRewrite(id - kGR2a, !gpuopt::Rewrite(id - kGR2a));
    } else
    switch (id) {
        case kLat: {
            int v = o.maxLatency;
            if (dir == 0) v = v ? 0 : 1;
            else v = (v + (dir > 0 ? 1 : 3)) % 4;
            o.maxLatency = v;
            break;
        }
        case kVrr: case kW3: case kW3Mode: case kL1: case kF1: case kWk: SetLiveBool(id, !LiveBool(id)); break;
        case kR1: {
            int v = (int)*g_host.r1Timeout;
            if (dir == 0) v = v ? 0 : (int)(g_host.r1Default ? g_host.r1Default : 2);
            else v = std::clamp(v + dir, 0, 16);
            *g_host.r1Timeout = (uint32_t)v;
            break;
        }
        case kFps: o.fpsCounter = !o.fpsCounter; break;
        case kAutoCont: SetAutoContinue(*g_host.autoContinue == 0); break;
        case kHint: g_cfg.pauseHint = (g_cfg.pauseHint + (dir < 0 ? 2 : 1)) % 3; break;  // Always -> Off -> In game menus
        case kFrameTime: o.frameTime = !o.frameTime; break;
        case kSpeed: o.speed = !o.speed; break;
        case kPos: o.pos = (o.pos + (dir < 0 ? 3 : 1)) & 3; break;
        case kOffX: case kOffY: {
            int& v = id == kOffX ? o.offX : o.offY;
            if (!o.offUser) {  // first nudge: take over the current effective values
                o.offX = osd::OffsetX();
                o.offY = osd::OffsetY();
                o.offUser = true;
            }
            if (dir == 0) v = id == kOffX ? 0 : osd::DefaultOffsetY(o.pos);
            else v = std::clamp(v + (dir < 0 ? -20 : 20), 0, id == kOffX ? 1900 : 1060);
            break;
        }
        case kPerfNote: return;
        case kScale: {
            float v = o.scale + (dir < 0 ? -0.5f : 0.5f);
            if (dir == 0 && v > 6.0f) v = 0.5f;
            o.scale = std::clamp(std::round(v * 2.0f) / 2.0f, 0.5f, 6.0f);
            break;
        }
        case kWorkerCap: {
            int v = g_bootWorkerCap;
            if (dir == 0) v = v ? 0 : 10;
            else if (dir > 0) v = v ? std::min(v + 1, 31) : 6;
            else v = v > 6 ? v - 1 : 0;
            g_bootWorkerCap = v;
            break;
        }
        case kP12: g_bootP12 = !g_bootP12; break;
        case kL1Wine: g_bootL1Wine = !g_bootL1Wine; break;
        case kFgWine: g_bootFgWine = !g_bootFgWine; break;
        default: return;
    }
    g_dirty = true;
    char v[64];
    uint32_t c;
    Value(id, v, sizeof(v), c);
    MLOG("%s%s%s -> %s", kItems[id].plain, kItems[id].tech[0] ? " " : "", kItems[id].tech, v);
}

void Move(int dir) {
    int pos = OrderPos(g_sel);
    for (int k = 0; k < kOrderN; ++k) {
        pos = (pos + dir + kOrderN) % kOrderN;
        const int id = kOrder[pos].id;
        if (Visible(id) && !ReadOnly(id)) {
            g_sel = id;
            return;
        }
    }
}

// ---- remote control (vpad pipe -> Present thread) ---------------------------------------------------------------------------
// One request at a time: the caller posts it (state 1), the Present thread takes it (2), runs it and marks it done (3).
struct RemoteReq {
    char key[48];
    int set, value, rc;
    char reply[200];
};
RemoteReq g_rem;
SRWLOCK g_remLock = SRWLOCK_INIT;  // serialises Remote() callers
volatile LONG g_remState = 0;

void NormKey(const char* s, char* out, size_t n) {  // drop parentheses / spaces, upper case
    size_t k = 0;
    for (; *s && k + 1 < n; ++s)
        if (*s != '(' && *s != ')' && *s != ' ') out[k++] = (char)toupper((unsigned char)*s);
    out[k] = 0;
}
int FindItem(const char* key) {
    char want[48], have[48];
    NormKey(key, want, sizeof(want));
    if (!strcmp(want, "FPS")) return kFps;
    if (!strcmp(want, "FRAMETIME")) return kFrameTime;
    if (!strcmp(want, "SPEED")) return kSpeed;
    for (int id = 0; id < kCount; ++id) {  // enum order: kL1 / kF1 come before their Proton next-launch twins
        if (!kItems[id].tech[0]) continue;
        NormKey(kItems[id].tech, have, sizeof(have));
        if (!strcmp(want, have)) return id;
    }
    for (int e = 0; e < kNumExp; ++e) {
        char k[48];
        WideCharToMultiByte(CP_UTF8, 0, kExpIniKey[e], -1, k, (int)sizeof(k), nullptr, nullptr);
        NormKey(k, have, sizeof(have));
        if (!strcmp(want, have)) return kXW6 + e;
    }
    return -1;
}
// current value of a remotely settable item; false = not settable remotely
bool RemoteValue(int id, int& v) {
    osd::OverlaySettings& o = osd::Overlay();
    if (IsExp(id)) {
        const ExpFix& x = g_host.exp[id - kXW6];
        v = x.enable ? (*x.enable != 0) : (g_expIni[id - kXW6] != 0);
        return true;
    }
    if (IsEp(id)) {
        v = extraperf::Level(id - kEp0);
        return true;
    }
    if (id == kEpPre) {
        v = extraperf::MatchPreset();  // -1 = custom
        return true;
    }
    switch (id) {
        case kVrr: case kW3: case kW3Mode: case kL1: case kF1: case kWk: v = LiveBool(id) ? 1 : 0; return true;
        case kR1: v = (int)*g_host.r1Timeout; return true;
        case kLat: v = o.maxLatency; return true;
        case kGpu: v = gpuopt::Batching() ? 1 : 0; return true;
        case kGR2a: case kGR2b: case kGR1: v = gpuopt::Rewrite(id - kGR2a) ? 1 : 0; return true;
        case kFps: v = o.fpsCounter ? 1 : 0; return true;
        case kFrameTime: v = o.frameTime ? 1 : 0; return true;
        case kSpeed: v = o.speed ? 1 : 0; return true;
        case kAutoCont: v = *g_host.autoContinue ? 1 : 0; return true;
        case kHint: v = g_cfg.pauseHint; return true;
        case kWorkerCap: v = g_bootWorkerCap; return true;
        case kP12: v = g_bootP12; return true;
        case kL1Wine: v = g_bootL1Wine; return true;
        case kFgWine: v = g_bootFgWine; return true;
        default: return false;
    }
}
void LogItem(int id) {
    char v[64];
    uint32_t c;
    Value(id, v, sizeof(v), c);
    MLOG("%s%s%s -> %s", kItems[id].plain, kItems[id].tech[0] ? " " : "", kItems[id].tech, v);
}
void RunRemote(RemoteReq& q) {
    q.rc = -1;
    const int id = FindItem(q.key);
    if (id < 0) {
        snprintf(q.reply, sizeof(q.reply), "no menu item '%s'", q.key);
        return;
    }
    int cur = 0;
    if (!Visible(id) || ReadOnly(id) || !Available(id) || !RemoteValue(id, cur)) {
        snprintf(q.reply, sizeof(q.reply), "'%s' (%s) is %s", q.key, kItems[id].plain,
                 !Visible(id) || !Available(id) ? "not installed / not shown this session" : "not settable remotely");
        return;
    }
    if (q.set) {
        osd::OverlaySettings& o = osd::Overlay();
        switch (id) {
            case kR1:
                *g_host.r1Timeout = (uint32_t)std::clamp(q.value, 0, 16);
                g_dirty = true;
                LogItem(id);
                break;
            case kLat:
                o.maxLatency = std::clamp(q.value, 0, 3);
                g_dirty = true;
                LogItem(id);
                break;
            case kHint:
                g_cfg.pauseHint = std::clamp(q.value, 0, 2);
                g_dirty = true;
                LogItem(id);
                break;
            case kWorkerCap:
                g_bootWorkerCap = q.value <= 0 ? 0 : std::clamp(q.value, 6, 31);
                g_dirty = true;
                LogItem(id);
                break;
            case kEpPre:  // 1.7.59: 0 game, 1 Performance, 2 Potato
                extraperf::ApplyPreset(std::clamp(q.value, 0, extraperf::kNumPresets - 1));
                g_dirty = true;
                LogItem(id);
                break;
            default:
                if (IsEp(id)) {  // 1.7.59 Extra performance level (0 = game)
                    extraperf::SetLevel(id - kEp0, q.value);
                    g_dirty = true;
                    LogItem(id);
                    break;
                }
                // bool items: one menu press when the state differs (logs 'MENU: <label> -> <value>')
                if ((cur != 0) != (q.value != 0)) Change(id, 0);
                break;
        }
        RemoteValue(id, cur);
    }
    char v[64];
    uint32_t c;
    Value(id, v, sizeof(v), c);
    snprintf(q.reply, sizeof(q.reply), "%s%s%s = %s (value %d)", kItems[id].plain, kItems[id].tech[0] ? " " : "", kItems[id].tech, v, cur);
    q.rc = 0;
}
void ProcessRemote() {
    if (InterlockedCompareExchange(&g_remState, 2, 1) != 1) return;
    RunRemote(g_rem);
    InterlockedExchange(&g_remState, 3);
}

// ---- per-present processing -----------------------------------------------------------------------------------------------
struct Rates {
    ULONGLONG t0 = 0;
    long w3 = 0, l1 = 0, fg = 0;
    uint32_t kk = 0, ks = 0;
    double rW3 = -1, rL1 = -1, rFg = -1, rKk = -1, rKs = -1;
    // experimental
    long w6 = 0, w6s = 0;
    long long w6t = 0;
    uint32_t pps = 0, m1r = 0, x1s = 0;
    uint64_t fh = 0, fm = 0, e1s = 0;
    double rW6 = 0, w6Us = 0, rPps = 0, fpcPct = -1, rM1 = 0, rX1 = 0, rE1 = 0;
    long gf = 0;
    long long gft = 0;
    double rGf = 0, gfUs = 0;
    long long wgs = 0;
    long ds = 0;
    double rWg = 0, rDs = 0;
};
Rates g_rates;
ULONGLONG g_rep[4] = {};  // next auto-repeat time per direction (0 = not repeating)
ULONGLONG g_lastOwnPad = 0, g_lastOwnPadScan = 0, g_lastOwnKey = 0;
bool g_ownConnected[4] = {};
bool g_loggedPadFallback = false, g_loggedKeyFallback = false;

// pause detection
uint8_t g_stopFlags = 0xFF;     // last logged value
int g_stopLogs = 0;
uint32_t g_lastFrame = 0;
ULONGLONG g_frameAdvAt = 0, g_pauseCandSince = 0;
bool g_paused = false;

template <class T>
T ReadPtr(volatile T* p) { return p ? *p : T(); }

void SampleRates(ULONGLONG t) {
    Rates& r = g_rates;
    const long w3 = ReadPtr(g_host.w3Count), l1 = ReadPtr(g_host.l1Count), fg = ReadPtr(g_host.fgCount);
    const uint32_t kk = ReadPtr(g_host.wkKept), ks = ReadPtr(g_host.wkSkipped);
    if (r.t0 && t - r.t0 >= 1000) {
        const double s = (double)(t - r.t0) / 1000.0;
        r.rW3 = (double)(w3 - r.w3) / s;
        r.rL1 = (double)(l1 - r.l1) / s;
        r.rFg = (double)(fg - r.fg) / s;
        r.rKk = (double)(uint32_t)(kk - r.kk) / s;
        r.rKs = (double)(uint32_t)(ks - r.ks) / s;
        const long w6 = ReadPtr(g_host.w6Waits);
        const long long w6t = ReadPtr(g_host.w6Ticks);
        static LARGE_INTEGER qf = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
        r.rW6 = (double)(w6 - r.w6) / s;
        r.w6Us = w6 > r.w6 ? (double)(w6t - r.w6t) * 1e6 / (double)qf.QuadPart / (double)(w6 - r.w6) : 0.0;
        r.rPps = (double)(uint32_t)(ReadPtr(g_host.ppSkipped) - r.pps) / s;
        const uint64_t fh = ReadPtr(g_host.fpcHits) - r.fh, fm = ReadPtr(g_host.fpcMisses) - r.fm;
        r.fpcPct = fh + fm ? 100.0 * (double)fh / (double)(fh + fm) : -1;
        r.rM1 = (double)(uint32_t)(ReadPtr(g_host.m1Refresh) - r.m1r) / s;
        r.rX1 = (double)(uint32_t)(ReadPtr(g_host.x1Skip) - r.x1s) / s;
        r.rE1 = (double)(ReadPtr(g_host.e1Skip) - r.e1s) / s;
        const long gfw = ReadPtr(g_host.gfWaits);
        r.rGf = (double)(gfw - r.gf) / s;
        r.gfUs = gfw > r.gf ? (double)(ReadPtr(g_host.gfTicks) - r.gft) * 1e6 / (double)qf.QuadPart / (double)(gfw - r.gf) : 0.0;
        r.rWg = (double)(ReadPtr(g_host.wgSkipped) - r.wgs) / s;
        r.rDs = (double)(ReadPtr(g_host.dsWaits) - r.ds) / s;
    } else if (r.t0) {
        return;
    }
    r.w6 = ReadPtr(g_host.w6Waits);
    r.w6t = ReadPtr(g_host.w6Ticks);
    r.pps = ReadPtr(g_host.ppSkipped);
    r.fh = ReadPtr(g_host.fpcHits);
    r.fm = ReadPtr(g_host.fpcMisses);
    r.m1r = ReadPtr(g_host.m1Refresh);
    r.x1s = ReadPtr(g_host.x1Skip);
    r.e1s = ReadPtr(g_host.e1Skip);
    r.gf = ReadPtr(g_host.gfWaits);
    r.gft = ReadPtr(g_host.gfTicks);
    r.wgs = ReadPtr(g_host.wgSkipped);
    r.ds = ReadPtr(g_host.dsWaits);
    r.t0 = t;
    r.w3 = w3;
    r.l1 = l1;
    r.fg = fg;
    r.kk = kk;
    r.ks = ks;
}

bool ForegroundIsGame() {
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    if (w) GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

void OwnPolling(ULONGLONG t) {
    // controller: only when the game's own XInputGetState calls are not reaching us
    if (g_xiOwn && t - (ULONGLONG)g_lastPadFeed > 500 && t - g_lastOwnPad >= 16) {
        if (!g_loggedPadFallback) {
            g_loggedPadFallback = true;
            MLOG("controller: %s - polling XInput ourselves (the game will also see menu button presses)",
                 g_padHooked ? "the game has not polled its pad for 0.5 s" : "the game's XInputGetState is not hooked");
        }
        g_lastOwnPad = t;
        const bool scan = t - g_lastOwnPadScan >= 2000;  // empty slots are expensive on Windows: probe them every 2 s
        if (scan) g_lastOwnPadScan = t;
        for (DWORD i = 0; i < 4; ++i) {
            if (!g_ownConnected[i] && !scan) continue;
            XINPUT_STATE st{};
            g_ownConnected[i] = g_xiOwn(i, &st) == ERROR_SUCCESS;
            if (g_ownConnected[i]) PadFeedSafe(i, &st, true);
        }
    }
    // keyboard: only when the DirectInput hook has not seen a keyboard read for 0.5 s
    if (t - (ULONGLONG)g_lastKeyFeed > 500 && t - g_lastOwnKey >= 16) {
        if (!g_loggedKeyFallback) {
            g_loggedKeyFallback = true;
            MLOG("keyboard: %s - polling GetAsyncKeyState (the game will also see menu key presses)",
                 g_keyHooked ? "no DirectInput keyboard read for 0.5 s" : "DirectInput is not hooked");
        }
        g_lastOwnKey = t;
        uint8_t keys[256] = {};
        bool any = false;
        if (g_cfg.vk && (GetAsyncKeyState((int)g_cfg.vk) & 0x8000)) keys[g_cfg.dik] = 0x80, any = true;
        if (g_open)
            for (const NavKey& k : kNavKeys)
                if (k.dik != 0x9C && (GetAsyncKeyState((int)k.vk) & 0x8000)) keys[k.dik] = 0x80, any = true;
        if (any && !ForegroundIsGame()) memset(keys, 0, sizeof(keys));
        KeyFeedSafe(keys, true);
    }
}

bool g_menuSigOk = false;     // the two "menu open" getters verified (bytes) at Configure
bool g_menuShown = false, g_stopPaused = false;
ULONGLONG g_menuSince = 0, g_menuLastOpen = 0;
int g_menuLogged = -1, g_menuLogs = 0;

// the game's own getters, called on the Present thread inside SEH (tiny virtual-call wrappers that only read UI state)
bool CallGameBool(uint32_t rva) {
    __try {
        return reinterpret_cast<unsigned char (*)()>(g_host.base + rva)() != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_menuSigOk = false;
        return false;
    }
}

void PauseDetect(ULONGLONG t) {
    if (!g_host.knownBuild || !g_host.base) return;
    uint64_t gp = 0;
    uint8_t flags = 0;
    bool haveFlags = false;
    if (Rd(g_host.base + kRvaFrameMgrPtr, gp) && gp >= 0x10000) {
        const uint8_t* G = reinterpret_cast<const uint8_t*>((uintptr_t)gp);
        uint8_t b[6] = {};
        uint32_t fc = 0;
        haveFlags = true;
        for (int i = 0; i < 6; ++i)
            if (!Rd(G + kOffStopFlags + i, b[i])) haveFlags = false;
        if (haveFlags && !Rd(G + kOffFrameCounter, fc)) haveFlags = false;
        if (haveFlags) {
            for (int i = 0; i < 6; ++i)
                if (b[i]) flags |= (uint8_t)(1u << i);
            if (flags != g_stopFlags) {  // diagnostic: tells from the first log which byte the pause menu sets
                if (g_stopLogs < 200) {
                    char list[48] = "";
                    for (int i = 0; i < 6; ++i)
                        if (flags & (1u << i)) {
                            char one[8];
                            snprintf(one, sizeof(one), " %X", (unsigned)(kOffStopFlags + i));
                            strncat_s(list, one, _TRUNCATE);
                        }
                    MLOG("engine stop flags (G+588..58D) now:%s%s", flags ? list : " none", ++g_stopLogs == 200 ? " (no further stop-flag lines)" : "");
                }
                g_stopFlags = flags;
            }
            if (fc != g_lastFrame) {
                g_lastFrame = fc;
                g_frameAdvAt = t;
            }
        }
    }
    // stop-flag path (58B / 58C stop game time; kept as the fallback signal)
    constexpr uint8_t kHard = 0x23;  // 588, 589, 58D: shutdown / system stops - never show the hint for those
    const bool cand = haveFlags && (flags & g_cfg.pauseMask) && !(flags & kHard & ~g_cfg.pauseMask) && t - g_frameAdvAt < 300;
    if (!cand) {
        g_pauseCandSince = 0;
        g_stopPaused = false;
    } else {
        if (!g_pauseCandSince) g_pauseCandSince = t;
        if (!g_stopPaused && t - g_pauseCandSince >= 250) g_stopPaused = true;
    }
    // the game's own "a menu is open" signal (the one its frame generation uses to pause itself)
    bool menuNow = false;
    if (g_menuSigOk) {
        const bool a = CallGameBool(kRvaMenuA), bb = g_menuSigOk && CallGameBool(kRvaMenuB);
        menuNow = a || bb;
        const int code = (a ? 1 : 0) | (bb ? 2 : 0);
        if (code != g_menuLogged) {
            if (g_menuLogs < 200)
                MLOG("game menu signal now %s (5A37D4=%d D63C0=%d)%s", code ? "OPEN" : "closed", a ? 1 : 0, bb ? 1 : 0,
                     ++g_menuLogs == 200 ? " (no further lines)" : "");
            g_menuLogged = code;
        }
    }
    if (menuNow) {
        if (!g_menuSince) g_menuSince = t;
        g_menuLastOpen = t;
    } else {
        g_menuSince = 0;
    }
    g_menuShown = (g_menuSince && t - g_menuSince >= 120) || (g_menuShown && t - g_menuLastOpen < 150);
    g_pauseReason = g_menuShown ? 1 : g_stopPaused ? 2 : (haveFlags && (flags & 0x04)) ? 3 : 0;
    const bool useMenu = g_cfg.pauseSrc != 1 && g_menuSigOk, useStop = g_cfg.pauseSrc != 0 || !g_menuSigOk;
    g_paused = (useMenu && g_menuShown) || (useStop && g_stopPaused);
}

void OnPresentImpl() {
    const ULONGLONG t = Now();
    g_lastPresent = (LONG64)t;
    OwnPolling(t);
    SampleRates(t);
    PauseDetect(t);  // also feeds the frame-generation status row (why FG is paused)
    ProcessRemote();  // vpad pipe: 'menu get / set' (1.7.37)
    if (InterlockedExchange(&g_toggleReq, 0) && t - (ULONGLONG)g_toggleAt < 1000) {
        const LONG src = g_toggleSrc;
        if (g_open) Close(SrcName(src));
        else Open(SrcName(src));
    }
    if (!g_open) {
        InterlockedExchange(&g_pressed, 0);
        g_rep[0] = g_rep[1] = g_rep[2] = g_rep[3] = 0;
        return;
    }
    const uint32_t pressed = (uint32_t)InterlockedExchange(&g_pressed, 0);
    uint32_t held = 0;
    for (int i = 0; i < 4; ++i)
        if (t - (ULONGLONG)g_padHeldAt[i] < 300) held |= PadNav((uint32_t)g_padHeld[i]);
    if (t - (ULONGLONG)g_keyHeldAt < 300) held |= KeyNav((uint32_t)g_keyHeld);
    static const uint32_t dirs[4] = {N_UP, N_DOWN, N_LEFT, N_RIGHT};
    for (int d = 0; d < 4; ++d) {
        bool act = false;
        if (pressed & dirs[d]) {
            act = true;
            g_rep[d] = t + 400;  // first repeat after 400 ms, then every 90 ms
        } else if ((held & dirs[d]) && g_rep[d] && t >= g_rep[d]) {
            act = true;
            g_rep[d] = t + 90;
        } else if (!(held & dirs[d])) {
            g_rep[d] = 0;
        }
        if (!act) continue;
        if (d == 0) Move(-1);
        else if (d == 1) Move(+1);
        else Change(g_sel, d == 2 ? -1 : +1);
    }
    if (pressed & N_OK) Change(g_sel, 0);
    if ((pressed & N_BACK) && g_open) {
        Close("B / Esc");
    }
}

// ---- drawing --------------------------------------------------------------------------------------------------------------
struct Row {
    char label[128];
    char tech[24];
    char value[48];
    uint32_t lc, vc;
    bool sel, rule;
};

std::string GestureText() {
    std::string s;
    if (g_cfg.vk) s = g_cfg.keyName;
    if (g_cfg.escDik) {
        if (!s.empty()) s += " / ";
        s += "Esc x3";
    }
    if (g_cfg.holdMask) {
        if (!s.empty()) s += " / ";
        char b[96];
        if (g_cfg.gesture == 1) snprintf(b, sizeof(b), "%s x3", g_cfg.holdName);
        else if (g_cfg.holdMs > 0) snprintf(b, sizeof(b), "Hold %s %gs", g_cfg.holdName, g_cfg.holdMs / 1000.0);
        else snprintf(b, sizeof(b), "%s", g_cfg.holdName);
        s += b;
    }
    return s;
}

float AutoScale(unsigned H) {
    if (g_cfg.scale > 0) return g_cfg.scale;
    return std::max(1.0f, std::round((float)H / 540.0f * 2.0f) / 2.0f);  // 800p 1.5, 1080p 2, 1440p 2.5, 4K 4
}

// one-line note in the top-right corner (below the FPS counter when that sits there too)
float DrawCornerNote(unsigned W, unsigned H, const char* text, uint32_t color) {
    const float sc = std::max(1.0f, std::round(AutoScale(H) * 0.75f * 2.0f) / 2.0f);  // 800p/1080p 1.5, 1440p 2
    const float pad = 4 * sc, lineH = (osd::kGlyphH + 2) * sc, margin = 8 * sc;
    const float w = (float)strlen(text) * osd::kGlyphW * sc + 2 * pad, h = lineH + 2 * pad - 2 * sc;
    float y = margin;
    const osd::OverlaySettings& o = osd::Overlay();
    if (o.pos == 1 && o.fpsCounter) {
        const float csc = osd::CounterScale(H);
        y += (float)osd::OffsetY() * (float)H / 1080.0f + (osd::kGlyphH + 2) * csc + 8 * csc - 2 * csc + 4 * csc;
    }
    const float x = (float)W - margin - w;
    osd::DrawRect(x, y, w, h, cPanel);
    osd::DrawText(x + pad, y + pad, text, color, sc);
    return sc;
}

float DrawImpl(unsigned W, unsigned H) {
    const ULONGLONG t = Now();
    // hold progress of the controller gesture (opening: corner note; closing: menu footer). All quads of this module
    // share one text scale, so the two are never drawn together.
    const ULONGLONG since = (ULONGLONG)g_holdSince;
    char holdNote[96] = "";
    if (since && g_cfg.holdMs > 0 && t - (ULONGLONG)g_holdSeen < 300 && t - since >= 1000 && t - since < (ULONGLONG)g_cfg.holdMs) {
        const int left = (int)std::ceil((double)(g_cfg.holdMs - (t - since)) / 1000.0);
        snprintf(holdNote, sizeof(holdNote), g_open ? "Hold %s to close... %ds" : "Hold %s for mod menu... %ds", g_cfg.holdName, left);
        if (!g_open) return DrawCornerNote(W, H, holdNote, cGold);
    }
    if (!g_open) {
        if (g_cfg.pauseHint == 2 || (g_cfg.pauseHint && g_paused)) {
            const std::string s = "Mod menu: " + GestureText();  // drawn every frame (2 = always, 1 = while a game menu is open)
            return DrawCornerNote(W, H, s.c_str(), cWhite);
        }
        return 0;
    }

    // ---- the menu panel: title, a scrolling body (section headers + items in kOrder), then the explanation of the selected
    // row, the diagnostics lines and the footer. Rows are filled by put(); kValueCol keeps the values in one column.
    Row head;
    Row body[96], tail[28];
    int nb = 0, nt = 0, selRow = -1;
    auto put = [&](Row* a, int& cnt, int cap, const char* label, const char* tech, const char* value, uint32_t lc, uint32_t vc, bool sel,
                   bool rule) {
        if (cnt >= cap) return;
        Row& r = a[cnt++];
        snprintf(r.label, sizeof(r.label), "%s", label);
        snprintf(r.tech, sizeof(r.tech), "%s", tech ? tech : "");
        snprintf(r.value, sizeof(r.value), "%s", value);
        r.lc = lc;
        r.vc = vc;
        r.sel = sel;
        r.rule = rule;
    };
    auto addT = [&](const char* label, uint32_t lc, bool rule = false) { put(tail, nt, 28, label, "", "", lc, cWhite, false, rule); };
    char title[96];
    snprintf(title, sizeof(title), "Nioh3EngineFix v%s - mod menu", g_host.version);
    {
        int c = 0;
        put(&head, c, 1, title, "", "", cGold, cYellow, false, true);
    }
    const char* pending = nullptr;
    for (int oi = 0; oi < kOrderN; ++oi) {
        const int id = kOrder[oi].id;
        if (kOrder[oi].section) pending = kOrder[oi].section;
        if (!Visible(id)) continue;
        if (pending) {
            put(body, nb, 96, pending, "", "", cCyan, cWhite, false, false);
            pending = nullptr;
        }
        char v[64];
        uint32_t vc;
        Value(id, v, sizeof(v), vc);
        char lab[96];
        const char* plain = kItems[id].plain;
        if (id == kFg) {
            const osd::FgStatus fs = osd::FrameGen();
            plain = (fs.setting > 0 && !g_fgNextLaunch && !g_fgSoft) ? "Turn frame gen OFF" : "Turn frame gen ON";
        }
        snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", plain);
        if (id == g_sel) selRow = nb;
        const bool avail = Available(id) || id >= kSave || id == kPerfNote;
        put(body, nb, 96, lab, kItems[id].tech, v, id == g_sel ? cWhite : (avail ? cGray : cDim), vc, id == g_sel, false);
    }
    // explanation of the selected item: what it does, then picture / timing, when it applies and the measured effect
    char what[200], effect[200];
    snprintf(what, sizeof(what), "%s", kItems[g_sel].what);
    snprintf(effect, sizeof(effect), "%s", kItems[g_sel].effect);
    if (!Available(g_sel)) {
        if ((g_sel == kL1 && g_host.l1 == FixState::WineSkipped) || (g_sel == kF1 && g_host.f1 == FixState::WineSkipped))
            snprintf(what, sizeof(what), "Skipped under Proton - turn on '%s on Proton' below, save, restart.", g_sel == kL1 ? "L1" : "F1");
        else if (g_sel == kFg)
            snprintf(what, sizeof(what), "%s",
                     !g_cfg.fgToggle ? "Status only ([Menu] FrameGenToggle = 0): switch it in the game's Options."
                     : g_host.wine && g_cfg.fgToggle == 1 ? "Status only on Proton: FG is reported to crash there (FrameGenToggle=2)."
                                                          : "Status only: the game's FG setter did not verify on this build (see the log).");
        else
            snprintf(what, sizeof(what), "Not installed this session (off in the ini or a site was not found - see the log).");
        effect[0] = 0;
    }
    addT("", cWhite, true);
    addT(what, cWhite);
    addT(effect[0] ? effect : " ", cGray);
    if (g_sel == kFg) {  // the row stays while FG is selected, so the panel does not jump when the reason changes
        const char* why = osd::FgReason(osd::FrameGen());
        addT(why ? why : " ", cOrange);
    }
    char stats[200];
    const Rates& r = g_rates;
    if (r.rW3 < 0) snprintf(stats, sizeof(stats), "Stats/s: measuring...");
    else
        snprintf(stats, sizeof(stats), "Stats/s: W3 %.0f  L1 %.0f  F1 %.0f  WK kept %.0f skipped %.0f", r.rW3, r.rL1, r.rFg,
                 r.rKk, r.rKs);
    addT(stats, cGray);
    bool anyExp = false;
    for (const ExpFix& x : g_host.exp) anyExp |= x.applied;
    if (anyExp && r.rW3 >= 0) {
        char fpc[16];
        if (r.fpcPct >= 0) snprintf(fpc, sizeof(fpc), "%.0f%%", r.fpcPct);
        else snprintf(fpc, sizeof(fpc), "-");
        snprintf(stats, sizeof(stats), "Exp/s: W6 %.0f x %.0fus  GF %.0f x %.0fus  PP skip %.0f  FPC hit %s  M1 %.0f  X1 %.0f  E1 %.0f  WG skip %.0f  DS %.0f",
                 r.rW6, r.w6Us, r.rGf, r.gfUs, r.rPps, fpc, r.rM1, r.rX1, r.rE1, r.rWg, r.rDs);
        addT(stats, cGray);
    }
    if (gpuopt::GetState() != gpuopt::State::Off && g_host.gpuStatus) {
        char gs[120];
        g_host.gpuStatus(gs, sizeof(gs));
        char line[160];
        snprintf(line, sizeof(line), "GPU: %s", gs);
        addT(line, cGray);
    }
    if (g_host.gpStatus) {  // GP profiler (1.7.0): GPU frame time and the top 5 passes
        char gs[200];
        g_host.gpStatus(gs, sizeof(gs));
        if (gs[0]) {
            char line[240];
            snprintf(line, sizeof(line), "GP: %s", gs);
            addT(line, cCyan);
        }
    }
    // footer / save status
    const LONG ss = g_saveState;
    if (ss >= 2 && !g_saveShownAt) g_saveShownAt = t;
    if (holdNote[0]) addT(holdNote, cGold);
    else if (ss == 1) addT("Saving to Nioh3EngineFix.ini...", cYellow);
    else if (ss >= 2 && t - g_saveShownAt < 4000) {
        char b[96];
        const LONG xml = g_cfgXmlState;
        if (ss == 2) snprintf(b, sizeof(b), "Saved to Nioh3EngineFix.ini%s", xml == 1 ? " + game config.xml (FG)" : "");
        else if (xml == 3) snprintf(b, sizeof(b), "config.xml NOT changed (FG) - see the log");
        else snprintf(b, sizeof(b), "Save FAILED (error %ld) - see the log", (long)g_saveErr);
        addT(b, ss == 2 ? cGreen : cOrange);
    } else {
        addT("Up/Down: move   A/Enter: toggle   Left/Right: change   B/Esc: close", cDim);
    }

    // value column: just right of the longest "name (id)", between 30 and 48 characters
    size_t valueCol = 30;
    for (int i = 0; i < nb; ++i)
        if (body[i].value[0]) {
            const size_t w = strlen(body[i].label) + (body[i].tech[0] ? 1 + strlen(body[i].tech) : 0) + 2;
            valueCol = std::max(valueCol, std::min<size_t>(w, 48));
        }
    size_t cols = 0;
    auto widen = [&](const Row& rw) {
        const size_t lc = strlen(rw.label) + (rw.tech[0] ? 1 + strlen(rw.tech) : 0), vc = strlen(rw.value);
        cols = std::max(cols, vc ? std::max(lc + 2, valueCol) + vc : lc);
    };
    for (int i = 0; i < nb; ++i) widen(body[i]);
    for (int i = 0; i < nt; ++i) widen(tail[i]);
    widen(head);
    float sc = AutoScale(H);
    const int minBody = std::min(nb, 22);
    auto bodyRows = [&](float s2) {  // body rows that fit under this scale
        const float lineH = (osd::kGlyphH + 2) * s2, pad = 6 * s2;
        return (int)std::floor((0.94f * (float)H - 2 * pad) / lineH) - 1 - nt;
    };
    auto fitsW = [&](float s2) { return (float)cols * osd::kGlyphW * s2 + 12 * s2 <= 0.96f * (float)W; };
    while (sc > 1.0f && (!fitsW(sc) || bodyRows(sc) < minBody)) sc -= 0.5f;
    while (sc > 0.5f && (!fitsW(sc) || bodyRows(sc) < std::min(nb, 12))) sc -= 0.25f;
    const int avail = std::max(5, bodyRows(sc));
    int first = 0, shown = nb;
    char scrollNote[40] = "";
    if (nb > avail) {  // scroll window: keep the selected row visible with one row of context above and below
        static int scroll = 0;
        shown = avail;
        const int row = selRow < 0 ? 0 : selRow;
        if (row - 1 < scroll) scroll = std::max(0, row - 1);
        if (row + 2 > scroll + shown) scroll = row + 2 - shown;
        scroll = std::clamp(scroll, 0, nb - shown);
        first = scroll;
        snprintf(scrollNote, sizeof(scrollNote), "rows %d-%d of %d", first + 1, first + shown, nb);
    }
    if (scrollNote[0] || g_dirty) {
        char tv[64];
        snprintf(tv, sizeof(tv), "%s%s%s", g_dirty ? "unsaved changes" : "", g_dirty && scrollNote[0] ? "  " : "", scrollNote);
        snprintf(head.value, sizeof(head.value), "%s", tv);
    }
    const int n = 1 + shown + nt;
    const float lineH = (osd::kGlyphH + 2) * sc, pad = 6 * sc, cw = osd::kGlyphW * sc;
    const float w = (float)std::max(cols, strlen(head.label) + 2 + strlen(head.value)) * cw + 2 * pad, h = n * lineH + 2 * pad;
    const float x0 = std::floor(((float)W - w) / 2), y0 = std::floor(((float)H - h) / 2);
    osd::DrawRect(x0, y0, w, h, cMenuPanel);
    osd::DrawRect(x0, y0, w, std::max(1.0f, sc), cRule);
    osd::DrawRect(x0, y0 + h - std::max(1.0f, sc), w, std::max(1.0f, sc), cRule);
    int line = 0;
    auto drawRow = [&](const Row& rw, bool isHead) {
        const float y = y0 + pad + line * lineH;
        if (rw.sel) osd::DrawRect(x0 + pad / 2, y - sc, w - pad, lineH, cHighlight);
        if (rw.rule && line > 0) osd::DrawRect(x0 + pad, y + lineH / 2, w - 2 * pad, std::max(1.0f, sc / 2), 0x80808080);
        float lx = x0 + pad;
        if (rw.label[0]) lx = osd::DrawText(lx, y, rw.label, rw.lc, sc);
        if (rw.tech[0]) osd::DrawText(lx + cw, y, rw.tech, rw.sel ? cGray : cDim, sc);
        if (rw.value[0]) {
            const size_t lc = strlen(rw.label) + (rw.tech[0] ? 1 + strlen(rw.tech) : 0);
            const float vx = x0 + pad + (float)std::max(lc + 2, valueCol) * cw;
            osd::DrawText(isHead ? x0 + w - pad - (float)strlen(rw.value) * cw : vx, y, rw.value, rw.vc, sc);
        }
        ++line;
    };
    drawRow(head, true);
    for (int i = 0; i < shown; ++i) drawRow(body[first + i], false);
    for (int i = 0; i < nt; ++i) drawRow(tail[i], false);
    return sc;
}

void Fail(const char* where, DWORD code) {
    InterlockedExchange(&g_disabled, 1);
    InterlockedExchange(&g_open, 0);
    MLOG("exception %08lX in %s - mod menu disabled for this session (the game and the fixes are unaffected)", code, where);
}

bool ParseHold(const wchar_t* s, uint32_t& mask, char* name, size_t nameLen) {
    mask = 0;
    name[0] = 0;
    if (!_wcsicmp(s, L"None") || !s[0]) return true;
    std::wstring all(s);
    size_t p = 0;
    while (p <= all.size()) {
        size_t e = all.find(L'+', p);
        if (e == std::wstring::npos) e = all.size();
        std::wstring tok = all.substr(p, e - p);
        while (!tok.empty() && tok.front() == L' ') tok.erase(tok.begin());
        while (!tok.empty() && tok.back() == L' ') tok.pop_back();
        struct B {
            const wchar_t* n;
            uint16_t bit;
            const char* show;
        };
        static const B kB[] = {{L"Start", XINPUT_GAMEPAD_START, "Start"}, {L"Menu", XINPUT_GAMEPAD_START, "Start"},
                               {L"Options", XINPUT_GAMEPAD_START, "Start"}, {L"Back", XINPUT_GAMEPAD_BACK, "View"},
                               {L"View", XINPUT_GAMEPAD_BACK, "View"},     {L"Select", XINPUT_GAMEPAD_BACK, "View"},
                               {L"L3", XINPUT_GAMEPAD_LEFT_THUMB, "L3"},   {L"R3", XINPUT_GAMEPAD_RIGHT_THUMB, "R3"},
                               {L"LB", XINPUT_GAMEPAD_LEFT_SHOULDER, "LB"}, {L"RB", XINPUT_GAMEPAD_RIGHT_SHOULDER, "RB"}};
        bool found = false;
        for (const B& b : kB)
            if (!_wcsicmp(tok.c_str(), b.n)) {
                if (!(mask & b.bit)) {
                    if (name[0]) strncat_s(name, nameLen, "+", _TRUNCATE);
                    strncat_s(name, nameLen, b.show, _TRUNCATE);
                }
                mask |= b.bit;
                found = true;
            }
        if (!found) return false;
        p = e + 1;
    }
    return mask != 0;
}

}  // namespace

// ---- public ------------------------------------------------------------------------------------------------------------------
void ProtonFgEarly(void (*log)(const char* fmt, ...), const wchar_t* markerPath, int fgToggle) {
    if (!g_log) g_log = log;
    g_fgToggleEarly = fgToggle;
    g_fgMarker = markerPath ? markerPath : L"";
    wchar_t e[MAX_PATH] = {};
    const DWORD n = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\KoeiTecmo\\NIOH3\\config.xml", e, MAX_PATH);
    if (n && n <= MAX_PATH && e[0] != L'%') g_cfgPathEarly = e;
    g_fgAtStart = g_cfgPathEarly.empty() ? "" : ReadGameConfigFg(g_cfgPathEarly);
    const bool marker = !g_fgMarker.empty() && GetFileAttributesW(g_fgMarker.c_str()) != INVALID_FILE_ATTRIBUTES;
    MLOG("Proton: config.xml %ls: FRAME_GENERATION %s at startup%s", g_cfgPathEarly.empty() ? L"(no path)" : g_cfgPathEarly.c_str(),
         g_fgAtStart.empty() ? "?" : g_fgAtStart.c_str(), marker ? "; FG marker from the last session found" : "");
    if (!marker) {
        ProtonFgArmAtLaunch();
        return;
    }
    if (!g_fgAtStart.empty() && g_fgAtStart != "OFF") {
        MLOG("Proton: the last session ended with FG switched on by the mod (crash?) and config.xml has %s - putting OFF back "
             "before the game reads it", g_fgAtStart.c_str());
        const int r = WriteGameConfigFg(g_cfgPathEarly, "OFF");
        if (r == 1 || r == 2) g_fgAtStart = "OFF";
    }
    DeleteFileW(g_fgMarker.c_str());
    ProtonFgArmAtLaunch();
}

void ProtonFgExit(const char* how) {
    if (!g_fgMarkerSet || InterlockedExchange(&g_fgExitDone, 1)) return;
    std::wstring path = g_host.base && g_host.knownBuild ? GameConfigPath() : g_cfgPathEarly;
    if (path.empty()) path = g_cfgPathEarly;
    const std::string cur = ReadGameConfigFg(path);
    // FrameGenToggle 2: always OFF. 3 (Proton safe FG): what the user chose last (FSR stays for the next launch, OFF after
    // a mod-menu FG off - the game may have written its in-memory FSR back when its Options menu closed)
    const std::string want = g_fgExitWant.empty() ? std::string("OFF") : g_fgExitWant;
    MLOG("Proton exit (%s): config.xml FRAME_GENERATION is %s, wanted %s", how, cur.empty() ? "?" : cur.c_str(), want.c_str());
    if (cur != want) {
        const int r = WriteGameConfigFg(path, want);
        MLOG("Proton exit: FRAME_GENERATION %s %s", want.c_str(), r == 1 ? "restored" : r == 2 ? "already set" : "NOT restored - the marker stays for the next start");
        if (r == 3) return;
    }
    DeleteFileW(g_fgMarker.c_str());
}

void Configure(const Host& h) {
    g_host = h;
    g_log = h.log;
    g_ini = h.iniPath ? h.iniPath : L"";
    g_host.iniPath = nullptr;  // not kept (g_ini owns the copy)
    g_bootWorkerCap = h.workerCap;
    g_bootP12 = h.p12;
    g_bootL1Wine = h.l1OnWine;
    g_bootFgWine = h.fgOnWine;
    for (int e = 0; e < kNumExp; ++e) g_expIni[e] = h.exp[e].enable ? (*h.exp[e].enable ? 1 : 0) : h.exp[e].ini;
    for (int e = 0; e < kNumExp; ++e) {
        g_expIni0[e] = g_expIni[e];
        g_expAuto[e] = GetPrivateProfileIntW(L"Patches", kExpIniKey[e], 0, g_ini.c_str()) < 0;
    }
    g_latBoot = osd::Overlay().maxLatency;
    const wchar_t* f = g_ini.c_str();
    g_cfg.enabled = (int)GetPrivateProfileIntW(L"Menu", L"Enabled", 1, f);
    wchar_t b[96] = {};
    GetPrivateProfileStringW(L"Menu", L"Key", L"F10", b, 96, f);
    g_cfg.vk = 0;
    g_cfg.dik = 0;
    snprintf(g_cfg.keyName, sizeof(g_cfg.keyName), "None");
    if (_wcsicmp(b, L"None") != 0) {
        bool ok = false;
        for (const KeyDef& k : kKeys) {
            wchar_t wn[16];
            swprintf(wn, 16, L"%hs", k.name);
            if (!_wcsicmp(b, wn)) {
                g_cfg.vk = k.vk;
                g_cfg.dik = k.dik;
                snprintf(g_cfg.keyName, sizeof(g_cfg.keyName), "%s", k.name);
                ok = true;
            }
        }
        if (!ok) {
            MLOG("[Menu] Key=%ls is not one of F1..F12, Insert, Home, End, PageUp, PageDown, Pause, ScrollLock, None - using F10", b);
            g_cfg.vk = VK_F10;
            g_cfg.dik = 0x44;
            snprintf(g_cfg.keyName, sizeof(g_cfg.keyName), "F10");
        }
    }
    GetPrivateProfileStringW(L"Menu", L"HoldButton", L"Start", b, 96, f);
    if (!ParseHold(b, g_cfg.holdMask, g_cfg.holdName, sizeof(g_cfg.holdName))) {
        MLOG("[Menu] HoldButton=%ls not understood (use Start, Back/View, L3, R3, LB, RB, combos like L3+R3, or None) - using Start", b);
        g_cfg.holdMask = XINPUT_GAMEPAD_START;
        snprintf(g_cfg.holdName, sizeof(g_cfg.holdName), "Start");
    }
    GetPrivateProfileStringW(L"Menu", L"Gesture", L"TripleTap", b, 96, f);
    g_cfg.gesture = (_wcsicmp(b, L"Hold") == 0) ? 0 : 1;
    g_cfg.tapWindowMs = std::clamp((int)GetPrivateProfileIntW(L"Menu", L"TapWindowMs", 900, f), 300, 3000);
    g_cfg.tapGapMs = std::clamp((int)GetPrivateProfileIntW(L"Menu", L"TapGapMs", 300, f), 100, 600);
    g_cfg.swallowTaps = (int)GetPrivateProfileIntW(L"Menu", L"SwallowTaps", 1, f);
    GetPrivateProfileStringW(L"Menu", L"HoldSeconds", L"5", b, 96, f);
    g_cfg.holdMs = (int)std::lround(std::clamp(_wtof(b), 0.0, 30.0) * 1000.0);
    if (g_cfg.holdMs > 0 && g_cfg.holdMs < 500) g_cfg.holdMs = 500;
    GetPrivateProfileStringW(L"Menu", L"Scale", L"0", b, 96, f);
    g_cfg.scale = (float)std::clamp(_wtof(b), 0.0, 8.0);
    if (g_cfg.scale > 0 && g_cfg.scale < 0.5f) g_cfg.scale = 0.5f;
    g_cfg.pauseHint = (int)GetPrivateProfileIntW(L"Menu", L"PauseHint", 2, f);
    GetPrivateProfileStringW(L"Menu", L"PauseHintSource", L"Both", b, 96, f);
    g_cfg.pauseSrc = !_wcsicmp(b, L"Menu") ? 0 : !_wcsicmp(b, L"StopFlags") ? 1 : 2;
    GetPrivateProfileStringW(L"Menu", L"KeyTripleTap", L"Escape", b, 96, f);
    g_cfg.escDik = (!_wcsicmp(b, L"None") || !b[0]) ? 0 : 0x01;
    if (g_cfg.escDik && _wcsicmp(b, L"Escape") != 0 && _wcsicmp(b, L"Esc") != 0) {
        MLOG("[Menu] KeyTripleTap=%ls not understood (use Escape or None) - using Escape", b);
    }
    g_menuSigOk = g_host.knownBuild && g_host.base && BytesAt(kRvaMenuA, kMenuABytes, sizeof(kMenuABytes)) &&
                  BytesAt(kRvaMenuB, kMenuBBytes, sizeof(kMenuBBytes));
    MLOG("game menu signal (5A37D4 / D63C0): %s; pause hint source %s", g_menuSigOk ? "bytes verified" : "not available (stop flags only)",
         g_cfg.pauseSrc == 0 ? "Menu" : g_cfg.pauseSrc == 1 ? "StopFlags" : "Both");
    GetPrivateProfileStringW(L"Menu", L"PauseHintFlags", L"58B 58C", b, 96, f);
    {
        uint8_t m = 0;
        for (wchar_t* p = b; *p;) {
            wchar_t* e = nullptr;
            const unsigned long v = wcstoul(p, &e, 16);
            if (e == p) { ++p; continue; }
            if (v >= 0x588 && v <= 0x58D) m |= (uint8_t)(1u << (v - 0x588));
            p = e;
        }
        g_cfg.pauseMask = m ? m : 0x18;
    }
    if (g_cfg.gesture == 0 && g_cfg.holdMask && g_cfg.holdMask == XINPUT_GAMEPAD_START && g_cfg.holdMs == 0)
        MLOG("[Menu] HoldSeconds=0 with HoldButton=Start opens the menu on every Start press - consider a combo");
    g_cfg.fgToggle = (int)GetPrivateProfileIntW(L"Menu", L"FrameGenToggle", 1, f);
    g_fgOk = false;
    if (g_host.knownBuild && g_host.base) {
        const bool code = BytesAt(kRvaFgWrite, kFgWriteBytes, sizeof(kFgWriteBytes)) &&
                          BytesAt(kRvaFgAvail, kFgAvailBytes, sizeof(kFgAvailBytes)) &&
                          BytesAt(kRvaDirtyStore, kDirtyStoreBytes, sizeof(kDirtyStoreBytes));
        // Proton: FG is reported to crash the game on Steam Deck / Linux (vkd3d-proton issue 2845, notes\community)
        g_fgOk = code && (g_cfg.fgToggle >= 2 || (g_cfg.fgToggle == 1 && !g_host.wine));
        MLOG("frame generation: %s", !code ? "the game's FG setter bytes differ - status only"
                                     : g_fgOk ? (g_host.wine ? (g_cfg.fgToggle == 3 ? "Proton safe FG ([Menu] FrameGenToggle = 3): OFF <-> FSR; "
                                                                "off = engine FG pause (FGP), on = resume or one OFF -> FSR rebuild; FG is "
                                                                "kept in config.xml behind the crash marker"
                                                              : "live switch enabled under Proton ([Menu] FrameGenToggle = 2); FG on is "
                                                                "never saved to the game's config here")
                                                             : "live switch through the game's FrameGenSettingWrite (0x1FDDFCC) verified")
                                     : g_cfg.fgToggle ? "status only under Proton (FG is reported to crash there; FrameGenToggle = 2 "
                                                        "allows the switch)"
                                                      : "status only ([Menu] FrameGenToggle = 0)");
    }
    if (g_host.wine && g_cfg.fgToggle == 3)
        MLOG("Proton safe FG: FGC copy-job guard %s, FGP pause switch %s", g_host.fgcOn ? "installed" : "NOT installed",
             g_host.fgPause ? "installed" : "NOT installed (FG off applies at the next launch)");
    g_configured = true;
    if (!g_cfg.enabled) {
        MLOG("off in ini ([Menu] Enabled=0)");
        return;
    }
    char hold[96];
    if (!g_cfg.holdMask) snprintf(hold, sizeof(hold), "none");
    else if (g_cfg.gesture == 1) snprintf(hold, sizeof(hold), "triple-tap %s (window %d ms, gap %d ms, swallow %d)", g_cfg.holdName, g_cfg.tapWindowMs, g_cfg.tapGapMs, g_cfg.swallowTaps);
    else if (g_cfg.holdMs > 0) snprintf(hold, sizeof(hold), "hold %s %.1f s", g_cfg.holdName, g_cfg.holdMs / 1000.0);
    else snprintf(hold, sizeof(hold), "press %s", g_cfg.holdName);
    MLOG("enabled - key %s, controller %s, scale %s, pause hint %s (flags mask %02X%s)", g_cfg.keyName, hold,
         g_cfg.scale > 0 ? "fixed" : "auto", g_cfg.pauseHint ? "on" : "off", g_cfg.pauseMask,
         g_host.knownBuild ? "" : ", unknown build: no pause detection");
}

bool Enabled() { return g_configured && g_cfg.enabled && !g_disabled; }

void Disable(const char* why) {
    if (!g_configured || !g_cfg.enabled) return;
    InterlockedExchange(&g_disabled, 1);
    MLOG("disabled: %s", why);
}

void InstallInputHooks() {
    if (!Enabled()) return;
    g_padHooked = InstallPadHook();
    g_keyHooked = InstallKeyHook();
    LoadOwnXInput();
    if (!g_padHooked) MLOG("controller: game's XInputGetState NOT hooked - menu buttons are polled by the mod and ALSO reach the game");
    if (!g_keyHooked) MLOG("keyboard: DirectInput NOT hooked - the menu keys are polled with GetAsyncKeyState and ALSO reach the game");
    MLOG("ready - open with %s", GestureText().c_str());
}

void OnPresent() {
    if (!Enabled()) return;
    __try {
        OnPresentImpl();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Fail("OnPresent", GetExceptionCode());
    }
}

float Draw(unsigned W, unsigned H) {
    if (!Enabled()) return 0;
    __try {
        return DrawImpl(W, H);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Fail("Draw", GetExceptionCode());
        return 0;
    }
}

bool IsOpen() { return g_open != 0 && !g_disabled; }
int GamePauseReason() { return (int)g_pauseReason; }

void SetKeyFilter(bool (*filter)(uint8_t* keys, bool readOk)) { g_keyFilter = filter; }

int Remote(const char* key, int set, int value, char* reply, size_t n, unsigned timeoutMs) {
    if (!Enabled()) {
        snprintf(reply, n, "mod menu off ([Menu] Enabled = 0 or disabled this session)");
        return -1;
    }
    AcquireSRWLockExclusive(&g_remLock);
    snprintf(g_rem.key, sizeof(g_rem.key), "%s", key ? key : "");
    g_rem.set = set;
    g_rem.value = value;
    g_rem.rc = -1;
    g_rem.reply[0] = 0;
    InterlockedExchange(&g_remState, 1);
    const ULONGLONG t0 = Now();
    while (g_remState == 1 && Now() - t0 < timeoutMs) Sleep(2);
    int rc;
    if (InterlockedCompareExchange(&g_remState, 0, 1) == 1) {  // never taken
        snprintf(reply, n, "timeout: the overlay did not present for %u ms (game minimised / not rendering?)", timeoutMs);
        rc = -2;
    } else {
        while (g_remState != 3 && Now() - t0 < timeoutMs + 2000) Sleep(1);  // taken: RunRemote is short
        if (g_remState == 3) {
            snprintf(reply, n, "%s", g_rem.reply);
            rc = g_rem.rc;
        } else {
            snprintf(reply, n, "the Present thread did not finish the request");
            rc = -3;
        }
        InterlockedExchange(&g_remState, 0);
    }
    ReleaseSRWLockExclusive(&g_remLock);
    return rc;
}
const char* OpenHint() {
    static char buf[96];
    snprintf(buf, sizeof(buf), "%s", GestureText().c_str());
    return buf;
}

bool SwallowMessage(const tagMSG* m) {
    if (!g_configured || !g_cfg.enabled || g_disabled || !m) return false;
    const UINT msg = m->message;
    if (msg != WM_KEYDOWN && msg != WM_KEYUP && msg != WM_SYSKEYDOWN && msg != WM_SYSKEYUP) return false;
    const UINT vk = (UINT)m->wParam;
    if (g_cfg.vk && vk == g_cfg.vk) return true;
    if (g_open || g_keyLatch)
        for (const NavKey& k : kNavKeys)
            if (k.vk == vk) return true;
    return false;
}

}  // namespace menu
