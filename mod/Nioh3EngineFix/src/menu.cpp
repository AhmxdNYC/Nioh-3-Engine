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
#include <atomic>
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
#include "menu_mode.h"
#include "updatecheck.h"
#include "savefiles.h"
#include "../../settings/store.h"
#include "../../menu_navigation.h"
#include "osd.h"
#include "crashreport.h"
#include "menu_style.h"
#include "padviz.h"
#include "inputhist.h"
#include "hudhide.h"
#include "../../rematch_menu_bridge.h"

namespace menu {
namespace {
#include "menu_rematch_host.h"

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
    int layout = 1;  // 1 grouped pages, 0 original flat layout (automation compatibility)
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
    int pauseSrc = 2;          // [Menu] PauseHintSource: 0 Menu (the game's own "menu open" signal), 1 StopFlags (G+58B/58C), 2 Both
    int freezeGame = 1;        // [Menu] FreezeGame: request the 58B stop flag (game time stops) while the mod menu is open
    uint8_t escDik = 0x01;     // [Menu] KeyTripleTap = Escape: press Escape 3x to open the menu (0 = off)
};
Cfg g_cfg;
Host g_host;
std::wstring g_ini;
bool g_configured = false;
volatile LONG g_disabled = 0;

// ---- shared input state (input thread <-> Present thread) -------------------------------------------------------------
enum : uint32_t { N_UP = 1, N_DOWN = 2, N_LEFT = 4, N_RIGHT = 8, N_OK = 16, N_BACK = 32, N_PAGEUP = 64, N_PAGEDOWN = 128, N_CLOSE = 256, N_HOME = 512,
                  N_TAB_PREV = 1024, N_TAB_NEXT = 2048 };  // #44232: pad bumpers switch tabs; keyboard PgUp / PgDn still jump rows
// pad raw bits: wButtons | stick directions | triggers
constexpr uint32_t B_SU = 1u << 16, B_SD = 1u << 17, B_SL = 1u << 18, B_SR = 1u << 19, B_LT = 1u << 20,
                   B_RT = 1u << 21, B_STICKS = B_SU | B_SD | B_SL | B_SR;
// keyboard bits
enum : uint32_t { K_TOGGLE = 1, K_UP = 2, K_DOWN = 4, K_LEFT = 8, K_RIGHT = 16, K_ENTER = 32, K_NPENTER = 64, K_ESC = 128, K_BACKSP = 256, K_PAGEUP = 512, K_PAGEDOWN = 1024 };
constexpr uint32_t K_NAV = K_UP | K_DOWN | K_LEFT | K_RIGHT | K_ENTER | K_NPENTER | K_ESC | K_BACKSP | K_PAGEUP | K_PAGEDOWN;
struct NavKey {
    uint32_t bit;
    UINT vk;
    uint8_t dik;
};
const NavKey kNavKeys[] = {{K_UP, VK_UP, 0xC8},       {K_DOWN, VK_DOWN, 0xD0},     {K_LEFT, VK_LEFT, 0xCB},
                           {K_RIGHT, VK_RIGHT, 0xCD}, {K_ENTER, VK_RETURN, 0x1C},  {K_NPENTER, VK_RETURN, 0x9C},
                           {K_ESC, VK_ESCAPE, 0x01},  {K_BACKSP, VK_BACK, 0x0E},
                           {K_PAGEUP, VK_PRIOR, 0xC9}, {K_PAGEDOWN, VK_NEXT, 0xD1}};

volatile LONG g_open = 0;
void SyncFreeze();  // holds / releases the 58B stop request so it follows g_open (defined with the engine reads)
volatile LONG g_pressed = 0;          // N_* edges collected while open, consumed by OnPresent
volatile LONG g_toggleReq = 0;
volatile LONG64 g_toggleAt = 0;
volatile LONG g_toggleSrc = 0;        // 1 key, 2 pad
volatile LONG64 g_lastPresent = 0;    // last OnPresent (GetTickCount64): the overlay is alive
// #9324 Nioh3EngineFix_MaskGameButtons(mask, ms): these pad buttons are hidden from the GAME (not from our menus) until the lease ends.
// Nioh3Training renews it every frame while the player stands near its Return gate (the native clear gadget's interact would open
// the game's own "This will end the mission" prompt, whose Yes exits the game outside a real mission).
volatile LONG g_gameMask = 0; volatile LONG64 g_gameMaskUntil = 0;
volatile LONG g_extCloseReq = 0;      // #13796 Nioh3EngineFix_MenuClose: an add-on asks to close the menu (native box at the Return object)
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
    WORD maskHeld = 0;            // #9909: buttons masked by MaskGameButtons that are still held: stay hidden after the lease until released
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
    if (raw & XINPUT_GAMEPAD_Y) n |= N_CLOSE;
    if (raw & XINPUT_GAMEPAD_BACK) n |= N_HOME;  // user #44181: View / Create returns to the first page; Start no longer does
    if (raw & XINPUT_GAMEPAD_LEFT_SHOULDER) n |= N_TAB_PREV;
    if (raw & XINPUT_GAMEPAD_RIGHT_SHOULDER) n |= N_TAB_NEXT;
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
    if (k & K_PAGEUP) n |= N_PAGEUP;
    if (k & K_PAGEDOWN) n |= N_PAGEDOWN;
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
    SyncFreeze();
}

// ---- pad feed (game input thread via the IAT hook; Present thread for own polling) ------------------------------------
// No C++ objects with destructors here: called inside __try.
void PadFeed(DWORD idx, XINPUT_STATE* st, bool own) {
    const ULONGLONG t = Now();
    PadSlot& s = g_pad[idx];
    XINPUT_GAMEPAD& g = st->Gamepad;
    padviz::Feed((int)idx, g.wButtons, g.sThumbLX, g.sThumbLY, g.sThumbRX, g.sThumbRY, g.bLeftTrigger, g.bRightTrigger, t);  // raw, before any masking
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
    if(g_coverToken.load()) {
        if((pressed & XINPUT_GAMEPAD_B)!=0){CancelCoverInput();if(!own)g.wButtons&=~XINPUT_GAMEPAD_B;}
        if(!own && CoverCaptures(t)){memset(&g,0,sizeof(g));st->dwPacketNumber+=0x40000000u;}
        else if(!own && t<g_coverCancelUntil.load())g.wButtons&=~XINPUT_GAMEPAD_B;
        return; // cover cancellation/staleness must not leave normal menu latches capturing
    }

    if (g_disabled) return;
    ForceCloseIfDead(t);
    const bool alive = Alive(t, 1000);
    // open/close gesture
    const uint32_t hm = g_cfg.holdMask;
    bool tapModified = false;
    if (g_open && (hm & (XINPUT_GAMEPAD_START | XINPUT_GAMEPAD_BACK))) {
        // While open, Start/Options does nothing: never the open gesture, and Home is View / Create now (#44181).
        s.nTaps = 0; s.tap0 = s.lastTap = s.synthUntil = 0;
        s.holdStart = 0; s.holdFired = false; s.passHold = false;
        g_holdSince = 0;
    } else if (hm && g_cfg.gesture == 1) {
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
    if(!own && t<g_coverCancelUntil.load())g.wButtons&=~XINPUT_GAMEPAD_B;
    if (g_open) InterlockedOr(&g_pressed, (LONG)PadNav(pressed));
    if (own) return;
    if (tapModified) st->dwPacketNumber += 0x40000000u;
    {   // #9324 lease; #9909: a masked button held when the lease ends stays hidden until released (unmasking a held A = a fresh press for the game)
        WORD mk;
        if (g_gameMask && t < (ULONGLONG)g_gameMaskUntil) { mk = (WORD)g_gameMask; s.maskHeld = (WORD)(g.wButtons & mk); }
        else { s.maskHeld = (WORD)(s.maskHeld & g.wButtons); mk = s.maskHeld; }
        if (mk && (g.wButtons & mk)) { g.wButtons &= (WORD)~mk; st->dwPacketNumber += 0x40000000u; }
    }
    bool blank = g_open != 0;
    if (s.latch) {
        if ((raw & ~B_STICKS) == 0) s.latch = false;
        else blank = true;
    }
    if (blank) {
        st->dwPacketNumber += 0x40000000u;  // the neutral state gets its own packet numbers (games may cache by packet)
        const SHORT rx = g.sThumbRX, ry = g.sThumbRY;
        memset(&g, 0, sizeof(g));
        if (g_open && !s.latch) { g.sThumbRX = rx; g.sThumbRY = ry; }  // #38155: the right stick (camera) stays live in the menu; the menu only navigates with the left stick/D-pad
    }
}

void PadFeedSafe(DWORD idx, XINPUT_STATE* st, bool own) {
    __try {
        PadFeed(idx, st, own);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_disabled, 1);
        InterlockedExchange(&g_open, 0);
        SyncFreeze();
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
    if(g_coverToken.load()) {
        if((pressed & K_ESC)!=0){CancelCoverInput();if(!own)keys[DIK_ESCAPE]=0;}
        if(!own && CoverCaptures(t))memset(keys,0,256);
        else if(!own && t<g_coverCancelUntil.load())keys[DIK_ESCAPE]=0;
        return;
    }
    if(!own && t<g_coverCancelUntil.load())keys[DIK_ESCAPE]=0;
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
        SyncFreeze();
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

// ---- freeze while the menu is open --------------------------------------------------------------------------------------
// The game's own pause raises request counters in the latched stop-flag block (notes\osd-design.md 10.5): G+0x590+4*i feeds the byte
// G+0x588+i, and FrameDtSetup zeroes the game time scale for 58B (i=3, counter G+0x59C, the same dword 0x10FD6E0 inc/decs) and 58C.
// The mod adds one request to the 58B counter while g_open && FreezeGame and takes exactly that one back, so the count it held is
// returned even when the menu is closed from another thread, a hook faults, the overlay dies, or the DLL is unloaded.
constexpr uint32_t kOffFreezeCounter = 0x590 + 4 * 3;  // request counter of stop flag 58B
volatile LONG g_freezeBusy = 0;
uintptr_t g_freezeCounter = 0;  // address of the counter we incremented; 0 = nothing held (guarded by g_freezeBusy)

int (*g_menuPauseAllowed)() = nullptr;
bool BossPauseAllowed() {
    static ULONGLONG tried = 0;
    if (!g_menuPauseAllowed && (!tried || Now() - tried > 5000)) {
        tried = Now();
        if (HMODULE m = GetModuleHandleW(L"Nioh3Training.asi"))
            g_menuPauseAllowed = reinterpret_cast<int (*)()>(GetProcAddress(m, "Nioh3Training_MenuPauseAllowed"));
    }
    // Missing/old Training or a stale sample must never freeze an empty scene.
    return g_menuPauseAllowed && g_menuPauseAllowed() > 0;
}
void SyncFreezeLocked() {
    const bool want = g_open && !g_disabled && g_cfg.freezeGame && g_host.knownBuild && g_host.base && BossPauseAllowed();
    if (want == (g_freezeCounter != 0)) return;
    __try {
        if (want) {
            uint64_t gp = 0;
            if (!Rd(g_host.base + kRvaFrameMgrPtr, gp) || gp < 0x10000) return;
            volatile LONG* c = reinterpret_cast<volatile LONG*>((uintptr_t)gp + kOffFreezeCounter);
            if (*c < 0 || *c > 1000) return;  // not a sane counter: leave the game alone
            InterlockedIncrement(c);
            g_freezeCounter = (uintptr_t)c;
            MLOG("freeze: game time stopped (58B request +1 -> %ld)", (long)*c);
        } else {
            volatile LONG* c = reinterpret_cast<volatile LONG*>(g_freezeCounter);
            g_freezeCounter = 0;
            LONG cur = *c;
            while (cur > 0) {  // the game floors at 0 too; never go negative
                const LONG seen = InterlockedCompareExchange(c, cur - 1, cur);
                if (seen == cur) break;
                cur = seen;
            }
            MLOG("freeze: game time resumed (58B request -> %ld)", (long)*c);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_freezeCounter = 0;  // the block vanished: nothing left to give back
    }
}

void SyncFreeze() {
    if (InterlockedCompareExchange(&g_freezeBusy, 1, 0) != 0) return;  // another thread is syncing; the next call re-checks
    SyncFreezeLocked();
    InterlockedExchange(&g_freezeBusy, 0);
}

bool BytesAt(uint32_t rva, const uint8_t* b, size_t n) {
    __try {
        return memcmp(g_host.base + rva, b, n) == 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// ---- items ----------------------------------------------------------------------------------------------------------
enum Id { kVrr, kW3, kW3Mode, kR1, kL1, kF1, kWk, kLat,
          kXW6, kXB, kXPP, kXFPC, kXAABB, kXM1, kXX1, kXE1, kXM2, kXJ1, kXS1, kXIK, kXTS, kXWS, kXED, kXGF, kXWL,  // order = menu::ExpId
          kXWG, kXTG, kXDS, kXLK, kXCP, kXSPF, kXMQ, kXPT1, kXDL, kXDQ, kXP1E7, kXMA, kXBPO, kXW1B, kXRPA, kXAS, kXPJH, kXTR,
          kXVSMA, kXLPVR, kXSKYR, kXFOGT, kXCLDR, kXRTP, kXPCM,
          kGpu, kGR2a, kGR2b, kGR1,
          kEpPre, kEp0, kEp1, kEp2, kEp3, kEp4, kEp5, kEp6, kEp7, kEp8, kEp9,  // 1.7.59 Extra performance (extraperf.h order)
          kFps, kFrameTime, kSpeed, kPos, kScale, kFg, kFgState, kUpscaler, kOffX, kOffY, kPerfNote, kHint, kPage, kAutoCont,
          kWorkerCap, kP12, kFreeze, kL1Wine, kFgWine, kSave, kClose, kCount };
#include "settings_catalog.h"
#include "menu_pages.h"
menu_nav::Trail<> g_pages;
ULONGLONG g_rep[4] = {};  // repeats never carry into a different page
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
    {"Preset", "(XPRE)", "Off = every look row at the game value. Optimized with effect visuals = clouds, GI, shadow distance and detail only. Optimized = those plus coarser fog, ambient occlusion off and blur off. Custom = the XCLD-style rows were set by hand.",
     "Changes the look. Applies now. Left/Right picks the next preset. The CPU / GPU / Game groups stay under this row."},
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
    {"Frame generation (game setting)", "(FG)", "Shows the game's frame-generation setting. Change it in the game's Options.", "Status only."},
    {"Frame gen:", "", "Real fps (game thread) vs displayed fps (FSR presenter thread): running when displayed > 1.15x real.", "Status only."},
    {"Upscaler (game setting)", "", "The game's upscaler setting. FSR frame gen does not need the FSR upscaler.", "Status only."},
    {"Nudge counter right", "(offset X)", "Moves the counter away from its corner's left/right screen edge (pixels at 1080p).", "Overlay only. Applies now. A resets to 0."},
    {"Nudge counter down", "(offset Y)", "Moves the counter away from its corner's top/bottom edge (pixels at 1080p).", "Overlay only. Applies now. A resets to the corner default."},
    {"Game's own performance display", "", "Nioh 3's built-in fps overlay is compiled out of the retail game: no setting exists to toggle.", "Nothing to switch. The counter above is the mod's own."},
    {"Mod menu reminder (top-right)", "(PauseHint)", "The small 'Mod menu: F10 / Esc x3 / Start x3' note. Always, only while a game menu is open, or off.", "Overlay only. Applies now. Save to ini keeps it."},
    {"Optimizations page", "", "Page 1 keeps the picture the same. Page 2 can change how the game looks. Boss Matches, Training and Display Overlays are on the first screen. Left/Right or A switches.", "Menu only."},
    {"Skip load tip screen", "", "Presses Continue for you on the tip page when a load finishes. Tips still show while the game is loading.", "Off by default. Applies now; saved to the ini at once."},
    {"Job-thread limit (handhelds)", "(WorkerCap)", "Caps the job thread pools at N (try 10, then 8). Off on desktops.", "Picture unchanged. Applies at the next launch. Lowers CPU use on handhelds."},
    {"Keep 3 more pool threads", "(P12)", "Keeps 3 more pool threads on CPUs with <= 12 threads. Experimental.", "Picture unchanged. Applies at the next launch."},
    {"Freeze game while menu is open", "(FreezeGame)", "Stops game time like the game's pause menu while any mod menu page is open; closing the menu resumes it. Overlays keep drawing.", "Picture unchanged. Applies now. Released on close, hide and unload."},
    {"Sleeping limiter on Proton", "(L1)", "Install L1 under Wine/Proton too (Wine may not honour the timer).", "Picture unchanged. Applies at the next launch."},
    {"Sleeping FG presenter on Proton", "(F1)", "Install F1 under Wine/Proton too (same timer caveat as L1).", "Picture unchanged. Applies at the next launch."},
    {"Save to ini", "", "Saves mod choices to Nioh3EngineFix.ini.", "Applies at the next launch."},
    {"Close menu", "", "Also: B / Esc / the menu key / the open gesture.", ""},
};

struct OrderEnt {
    int id;
    const char* section;  // header drawn before this item (nullptr = none)
};
const OrderEnt kOrder[] = {
    {kPage, "Menu"},
    {kFps, "Display & overlay"}, {kFrameTime, nullptr}, {kSpeed, nullptr}, {kPos, nullptr}, {kOffX, nullptr}, {kOffY, nullptr},
    {kScale, nullptr}, {kHint, nullptr}, {kPerfNote, nullptr},
    {kFgState, "Frame generation"}, {kFg, nullptr}, {kUpscaler, nullptr},
    {kAutoCont, "Gameplay"},
    {kEpPre, "Extra performance (changes the look)"}, {kEp0, nullptr}, {kEp1, nullptr}, {kEp2, nullptr}, {kEp3, nullptr},
    {kEp4, nullptr}, {kEp5, nullptr}, {kEp6, nullptr}, {kEp7, nullptr}, {kEp8, nullptr}, {kEp9, nullptr},
    {kVrr, "Performance fixes (safe)"}, {kW3, nullptr}, {kW3Mode, nullptr}, {kR1, nullptr}, {kL1, nullptr}, {kF1, nullptr}, {kWk, nullptr},
    {kLat, nullptr}, {kFreeze, nullptr}, {kGpu, nullptr},
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

int g_sel = kPage;  // two-page menu (Layout=0) opens on the page switcher; grouped opens reset this
int g_page = 0;       // 0 = same picture, 1 = may change the look
bool g_dirty = false;
int g_bootWorkerCap = 0, g_bootP12 = 0, g_bootL1Wine = 0, g_bootFgWine = 0;
int g_expIni0[kNumExp] = {};  // value at launch
bool g_expAuto[kNumExp] = {};  // ini held -1 (auto by OS) at launch
int g_latBoot = 0;
int g_expIni[kNumExp] = {};  // what Save writes for each experimental patch (live state when installed)

bool Visible(int id) {
    switch (id) {
        case kPage: return !g_cfg.layout;  // the two-page switcher is the Layout=0 menu; grouped pages hide it
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
bool ReadOnly(int id) { return id == kFg || id == kFgState || id == kUpscaler; }

// Page 2 is only the rows whose text says the picture can change. Everything else stays on page 1.
bool LookChanging(int id) {
    return id == kEpPre || IsEp(id) || id == kFg || id == kFgState || id == kUpscaler ||
           id == kXLPVR || id == kXSKYR || id == kXFOGT || id == kXCLDR || id == kGR2b || id == kGR1;
}
bool OnPage(int id) {
    return id == kPage || id == kSave || id == kClose || LookChanging(id) == (g_page == 1);
}
const char* FlatPageName() { return g_page ? "2/2 may change the look" : "1/2 same picture"; }

// #34265: these leave Settings and live on the Gameplay / Overlays page (Layout=0). Layout=1 still files them under pDisplay.
// #37433: every non-performance row leaves Settings: frame-gen and upscaler status readouts, the input-to-screen and
// worker-wake diagnostics, and the loading-screen auto-click (#37423). Settings keeps only the performance fixes and their switches.
bool OverlayRow(int id) {
    return id == kFps || id == kFrameTime || id == kSpeed || id == kPos || id == kOffX || id == kOffY || id == kScale ||
           id == kHint || id == kPerfNote || id == kAutoCont || id == kFgState || id == kUpscaler || id == kXTS || id == kXWS;
}

// PS_FRAME_METER is index 7 in Nioh3Training. Its only menu row is Display Overlays > Frame meter settings.
constexpr int kFrameMeterWhat = 7;
constexpr int kKiWindowWhat = 21;  // PS_KI_WINDOW in Nioh3Training (#34558)
int (*g_pstGet)(int) = nullptr;
int (*g_pstSet)(int, int, int) = nullptr;
char g_frameMeterNote[200] = "Boss frame meter. Needs Nioh3Training.";
char g_kiWindowNote[220] = "Whole Ki window in dark blue on the player frame meter. The perfect Ki Pulse part (teal) always shows. Needs Nioh3Training.";
void ResolveFrameMeter() {
    if (g_pstGet && g_pstSet) return;
    static ULONGLONG tried = 0;
    if (tried && Now() - tried < 5000) return;
    tried = Now();
    const HMODULE m = GetModuleHandleW(L"Nioh3Training.asi");
    if (!m) return;
    if (!g_pstGet) g_pstGet = reinterpret_cast<int (*)(int)>(GetProcAddress(m, "Nioh3Training_PracticeStatGet"));
    if (!g_pstSet) g_pstSet = reinterpret_cast<int (*)(int, int, int)>(GetProcAddress(m, "Nioh3Training_PracticeStatSet"));
}
int FrameMeterGet() {
    ResolveFrameMeter();
    return g_pstGet ? g_pstGet(kFrameMeterWhat) : -1;
}
void ChangeFrameMeter() {
    ResolveFrameMeter();
    if (!g_pstSet) {
        snprintf(g_frameMeterNote, sizeof(g_frameMeterNote), "Needs a Nioh3Training.asi with the frame meter.");
        return;
    }
    const int v = g_pstSet(kFrameMeterWhat, -1, 1);  // -1 flips; Training saves its own ini
    if (v < 0) snprintf(g_frameMeterNote, sizeof(g_frameMeterNote), "Needs a Nioh3Training.asi with the frame meter.");
    else snprintf(g_frameMeterNote, sizeof(g_frameMeterNote), v ? "Frame meter ON. Saved in Training." : "Frame meter OFF. Saved in Training.");
}
int KiWindowGet() {
    ResolveFrameMeter();
    return g_pstGet ? g_pstGet(kKiWindowWhat) : -1;
}
void ChangeKiWindow() {
    ResolveFrameMeter();
    if (!g_pstSet) {
        snprintf(g_kiWindowNote, sizeof(g_kiWindowNote), "Needs a Nioh3Training.asi with the Ki window.");
        return;
    }
    const int v = g_pstSet(kKiWindowWhat, -1, 1);
    if (v < 0) snprintf(g_kiWindowNote, sizeof(g_kiWindowNote), "Needs a Nioh3Training.asi with the Ki window.");
    else snprintf(g_kiWindowNote, sizeof(g_kiWindowNote), v ? "Ki window ON. Dark blue on the player frame meter. Saved in Training."
                                                             : "Ki window OFF. The dark blue is hidden. Saved in Training.");
}

// #38437: the rest of Training's frame-meter switches as rows of the "Frame meter settings" page (same PracticeStat flip as the
// Frame meter / Ki window rows; Training saves its own ini). kPsToggleN in menu_pages.h must match this table.
struct PsToggle { int what; const char* label; const char* note; };
const PsToggle kPsToggles[kPsToggleN] = {
    {10, "Player frame meter", "Second bar under the boss meter for your own attacks."},
    {22, "I-frame lane", "Own I-FR row under the PLAYER row showing only the i-frame cells in gold."},
    {23, "Player row", "Draw the PLAYER row. Off keeps the I-FR row."},
    {17, "Negative Ki meter", "Thin bar under a Training boss's Ki bar while its Ki is below 0. Off by default."},
    {18, "Colour key (legend)", "Show the KEY legend under the meter."},
    {19, "Readout line", "The top readout with the frame counts."},
    {20, "Ki lane", "Ki pulse / Ki debt lane on the meter."},
    {24, "Key: Startup", "Legend entry for startup frames."},
    {25, "Key: Active", "Legend entry for active frames."},
    {26, "Key: Recovery", "Legend entry for recovery frames."},
    {27, "Key: Unblockable", "Legend entry for unblockable moves."},
    {28, "Key: Grab", "Legend entry for grabs."},
    {29, "Key: Other", "Legend entries No hit / Pending / Empty."},
    {30, "Key: Timing", "Legend entries Action start / Total frames / Stun."},
    {31, "Key: Ki", "Legend entries Ki Pulse / Ki debt / Perfect."},
    {32, "Key: I-frames", "The four I-FR legend entries."},
};
char g_psNote[200] = "";
int PsGet(int what) {
    ResolveFrameMeter();
    return g_pstGet ? g_pstGet(what) : -1;
}
constexpr int kFrameMeterStyleWhat = 33; // append-only Training PracticeStat ABI
constexpr int kFrameMeterSizeWhat = 34;
void ChangeFrameMeterSize(int dir) {
    const int cur=PsGet(kFrameMeterSizeWhat);
    if(cur<0 || !g_pstSet){snprintf(g_psNote,sizeof g_psNote,"Needs Training with ring sizes.");return;}
    const int next=dir?std::clamp(cur+(dir>0?1:-1),0,2):(cur+1)%3;
    const int v=g_pstSet(kFrameMeterSizeWhat,next,1);
    snprintf(g_psNote,sizeof g_psNote,v<0?"Could not change ring size.":"Ring size saved in Training.");
}
void ChangeFrameMeterStyle(int dir) {
    const int cur = PsGet(kFrameMeterStyleWhat);
    if (cur < 0 || !g_pstSet) {
        snprintf(g_psNote, sizeof(g_psNote), "Needs Nioh3Training with Ring frame meter support.");
        return;
    }
    const int next = dir > 0 ? 1 : dir < 0 ? 0 : !cur;
    const int v = g_pstSet(kFrameMeterStyleWhat, next, 1);
    snprintf(g_psNote, sizeof(g_psNote), v < 0 ? "Could not change frame meter style." :
        v ? "Ring style. Saved in Training." : "Row style. Saved in Training.");
}
// Global Display sky settings share Training's existing persistent stat ABI (#38871).
constexpr int kTimeOfDayWhat = 12, kWeatherWhat = 13;
char g_skyNote[200] = "";
void SkyValueText(int id, int value, char* text, size_t cap) {
    if (value < 0) { snprintf(text, cap, "needs Training"); return; }
    if (!value) { snprintf(text, cap, "Stock"); return; }
    if (id == kTimeOfDayRow) {
        const int minutes = (std::min(value, 48) - 1) * 30;
        const int hour = minutes / 60;
        snprintf(text, cap, "%d:%02d %s", hour % 12 ? hour % 12 : 12, minutes % 60, hour < 12 ? "am" : "pm");
    } else {
        static const char* const names[] = {"Stock", "Clear", "Overcast", "Rain", "Storm", "Snow"};
        snprintf(text, cap, "%s", names[std::min(value, 5)]);
    }
}
void ChangeSky(int id, int dir) {
    const int what = id == kTimeOfDayRow ? kTimeOfDayWhat : kWeatherWhat;
    const int cur = PsGet(what);
    if (cur < 0 || !g_pstSet) { snprintf(g_skyNote, sizeof(g_skyNote), "Needs Nioh3Training with Display time and weather support."); return; }
    // Left/right select; A/Enter restores the area's own authored sky.
    const int next = dir ? std::clamp(cur + (dir > 0 ? 1 : -1), 0, id == kTimeOfDayRow ? 48 : 5) : 0;
    if (next == cur) return;
    const int value = g_pstSet(what, next, 1);
    if (value < 0) { snprintf(g_skyNote, sizeof(g_skyNote), "Could not change this sky setting."); return; }
    char text[32]; SkyValueText(id, value, text, sizeof(text));
    snprintf(g_skyNote, sizeof(g_skyNote), "%s: %s. Saved.", id == kTimeOfDayRow ? "Time of day" : "Weather", text);
}
void ChangePs(int index) {
    ResolveFrameMeter();
    const PsToggle& t = kPsToggles[index];
    const int v = g_pstSet ? g_pstSet(t.what, -1, 1) : -1;
    if (v < 0) snprintf(g_psNote, sizeof(g_psNote), "Needs a Nioh3Training.asi with this switch.");
    else snprintf(g_psNote, sizeof(g_psNote), "%s %s. Saved in Training.", t.label, v ? "ON" : "OFF");
}
// 'All frame rows' / 'All key indicators' masters (were Practice hub rows, human #38180): same Training settings
// as the Practice masters were. State 1 = all ON, 0 = all OFF, 2 = mixed, -1 = Training missing or too old.
constexpr int kPsRowSet[] = {7, 10, 23, 22};                       // boss meter, player meter, Player row, I-frame lane
constexpr int kPsKeySet[] = {18, 24, 25, 26, 27, 28, 29, 30, 31, 32};  // legend + every KEY entry group
int PsMasterState(bool keys) {
    const int* set = keys ? kPsKeySet : kPsRowSet;
    const int n = keys ? (int)(sizeof(kPsKeySet) / sizeof(*kPsKeySet)) : (int)(sizeof(kPsRowSet) / sizeof(*kPsRowSet));
    int on = 0, off = 0;
    for (int i = 0; i < n; ++i) { const int v = PsGet(set[i]); if (v < 0) return -1; (v ? on : off)++; }
    return !off ? 1 : !on ? 0 : 2;
}
void ChangePsMaster(bool keys, int dir) {
    const int st = PsMasterState(keys);
    if (st < 0 || !g_pstSet) { snprintf(g_psNote, sizeof(g_psNote), "Needs a newer Nioh3Training.asi."); return; }
    const int want = dir > 0 ? 1 : dir < 0 ? 0 : st == 1 ? 0 : 1;
    const int* set = keys ? kPsKeySet : kPsRowSet;
    const int n = keys ? (int)(sizeof(kPsKeySet) / sizeof(*kPsKeySet)) : (int)(sizeof(kPsRowSet) / sizeof(*kPsRowSet));
    for (int i = 0; i < n; ++i) g_pstSet(set[i], want, 1);
    snprintf(g_psNote, sizeof(g_psNote), keys ? "All key indicators %s (the KEY under the meter and every entry). Saved in Training."
                                              : "All frame rows %s (boss, player, i-frames). Saved in Training.", want ? "ON" : "OFF");
}

// #38437 / human #43461: snapshot, then open the report folder and the website.
char g_reportNote[300] = "A: save a snapshot, open its folder, and open the report website.";
void DoReport() {
    char path[520] = {};
    const bool ok = crashreport::Snapshot(path, sizeof(path));
    const bool clip = crashreport::CopyReportUrl();
    crashreport::OpenReportUrl();
    crashreport::OpenReportFolder(ok ? path : nullptr);
    if (ok) snprintf(g_reportNote, sizeof(g_reportNote), "Saved %s. Opened the folder and %s%s", path, crashreport::ReportUrl(), clip ? "." : " (clipboard failed).");
    else snprintf(g_reportNote, sizeof(g_reportNote), "Snapshot failed (crash reports off?). Opened the folder and %s.", crashreport::ReportUrl());
    MLOG("Report row: %s", g_reportNote);
}

// human #45507: 'Reverse Mods' row = 'Restore Vanilla Game.cmd' from inside the game. Renames the loader and the .asi files
// to *.mod-off (saves and ini files stay), so the NEXT start is the normal game. Windows and Wine both allow renaming a
// loaded module; this session keeps the mod until the game closes. Pressing it again while the mod is off puts it back.
namespace reversemods {
const wchar_t* const kFiles[] = {L"dinput8.dll", L"Nioh3EngineFix.asi", L"Nioh3BossRematch.asi", L"Nioh3Training.asi", L"Nioh3CamKiProbe.asi"};
inline bool Exists(const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; }
inline std::wstring GameDir() {
    wchar_t exe[MAX_PATH] = {};
    const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe, n);
    const size_t slash = dir.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : dir.substr(0, slash + 1);
}
// The mod counts as off when the loader is parked and not present.
inline bool IsOff(const std::wstring& dir) { return !Exists(dir + kFiles[0]) && Exists(dir + kFiles[0] + L".mod-off"); }
// off=true parks every present file, off=false brings every parked file back. Returns files moved, or -1 after undoing a
// partial move (a target already there, or a rename refused), so the folder is never left half on / half off.
inline int Move(const std::wstring& dir, bool off) {
    int moved = 0; const wchar_t* done[sizeof(kFiles) / sizeof(kFiles[0])] = {};
    for (const wchar_t* f : kFiles) {
        const std::wstring on = dir + f, parked = on + L".mod-off";
        const std::wstring& from = off ? on : parked; const std::wstring& to = off ? parked : on;
        if (!Exists(from)) continue;
        if (Exists(to) || !MoveFileExW(from.c_str(), to.c_str(), 0)) {
            for (int i = moved - 1; i >= 0; --i) {
                const std::wstring a = dir + done[i], b = a + L".mod-off";
                MoveFileExW(off ? b.c_str() : a.c_str(), off ? a.c_str() : b.c_str(), 0);
            }
            return -1;
        }
        done[moved++] = f;
    }
    return moved;
}
}  // namespace reversemods
char g_reverseNote[200] = "A twice: mod OFF from the next start. Saves stay.";
uint64_t g_reverseConfirmAt = 0;
std::wstring g_reverseDir;  // tests point this at a temp folder; empty = the game folder
void DoReverseMods(uint64_t now) {
    const std::wstring dir = g_reverseDir.empty() ? reversemods::GameDir() : g_reverseDir;
    const bool off = !reversemods::IsOff(dir);
    if (!g_reverseConfirmAt || now - g_reverseConfirmAt > 5000) {
        g_reverseConfirmAt = now ? now : 1;
        snprintf(g_reverseNote, sizeof(g_reverseNote), "%s", off ? "Press A again: mod OFF from the next start."
                                                                 : "Press A again: mod back ON from the next start.");
        return;
    }
    g_reverseConfirmAt = 0;
    const int moved = reversemods::Move(dir, off);
    if (moved < 0) snprintf(g_reverseNote, sizeof(g_reverseNote), "Rename failed, nothing changed. Use '%s' instead.", off ? "Restore Vanilla Game" : "Put the mod back");
    else if (off) snprintf(g_reverseNote, sizeof(g_reverseNote), "%s", moved ? "Mod is OFF from the next start. A twice here: back ON."
                                                                           : "No mod files found to turn off.");
    else snprintf(g_reverseNote, sizeof(g_reverseNote), "%s", moved ? "Mod is back ON from the next start." : "No parked mod files found.");
    MLOG("Reverse Mods row: %s (%d files)", off ? "off" : "on", moved);
}

int HideHudSettingRow() {  // #44871: Display Overlays row for [Menu] HideGameHud
    for (int i = 0; i < kConfigN; ++i) if (!strcmp(kConfigSettings[i].key, "HideGameHud")) return kSettingRow + i;
    return -1;
}
bool IsInputDisplayKey(const char* key) { return key && strncmp(key, "InputDisplay", 12) == 0; }
bool IsControllerOnlyKey(const char* key) { return key && (strncmp(key, "ControllerDisplay", 17) == 0 || strcmp(key, "ButtonStyle") == 0); }
// Settings sub-page of a performance row (layout 0): CPU / GPU / Game / Frame Gen; -1 = not a Settings row
int SetGroup(int id) {
    if (id == kF1 || id == kFgWine || id == kFg || id == kFgState || id == kUpscaler) return pFrameGen;  // #38461: frame-gen rows live only in Settings > Frame Gen
    if (id == kEpPre) return -1;  // #43269: preset is the first row of Optimizations, not a row inside GPU
    if (id == kVrr || id == kLat || id == kWorkerCap || id == kP12 || id == kFreeze) return pSGame;
    if (id == kEp9 || OverlayRow(id) || id == kPage || id == kSave || id == kClose || id == kL1Wine) return id == kL1Wine ? pSCpu : -1;
    const int sp = SettingsPage(id);
    if (id == kGpu || sp == pGpu || sp == pProton || sp == pExtra) return pSGpu;
    if (sp == pCpu || sp == pScheduling || sp == pSafe) return pSCpu;
    return -1;
}

std::string g_saveName;int g_saveConfirm=0;uint64_t g_saveConfirmAt=0;
int PageRowsBody(int* out, bool selectable) {
    int n = 0;
    auto add = [&](int id) { out[n++] = id; };
    if(g_pages.page==pSaveFiles||g_pages.page==pSaveActions){
        if(savefiles::Pending())add(kSaveCancelRow);
        else if(savefiles::Allowed()){
            if(g_pages.page==pSaveFiles){
                for(int i=0;i<savefiles::SlotCount();++i)add(kSaveNamedFirst+i);
                add(kSaveBackupRow);add(kSaveRefreshRow);add(kSaveOpenRow);
            }else if(!g_saveName.empty()){add(kSaveSwitchRow);if(savefiles::LiveAvailable())add(kSaveLiveRow);}
        }
        add(kBackRow);return n;
    }
    if (!g_cfg.layout) {
        // #38437 first screen: Boss Matches, Practice, Display Overlays, Camera settings, Settings, Close.
        const int pg = g_pages.page;
        if (pg == pSettings) {  // #43269: preset first, then the CPU / GPU / Game / Frame Gen groups
            if (Visible(kEpPre)) add(kEpPre);
            for (int p : {pSCpu, pSGpu, pSGame, pFrameGen}) add(kPageRow + p);
            add(kSave);
            add(kBackRow);
            return n;
        }
        if (pg == pSCpu || pg == pSGpu || pg == pSGame || pg == pFrameGen) {
            for (const auto& row : kOrder)
                if (SetGroup(row.id) == pg && Visible(row.id) && (!selectable || !ReadOnly(row.id))) add(row.id);
            add(kBackRow);
            return n;
        }
        if (pg == pDisplay) {
            for (int p : {pOvController, pOvFrameMeter, pOvInput, pOvFps}) add(kPageRow + p);
            add(kTimeOfDayRow); add(kWeatherRow);
            if (HideHudSettingRow() >= 0) add(kPageRow + pOvHud);  // one 'Game HUD >' row; the master + per-part rows live on that page
            if (Visible(kAutoCont)) add(kAutoCont);
            add(kBackRow);
            return n;
        }
        if (pg == pOvHud) {  // master 'Hide game HUD' on top, then the per-element rows (#44877, hudhide::kElems, catalog order)
            if (HideHudSettingRow() >= 0) add(HideHudSettingRow());
            for (int i = 0; i < kConfigN; ++i)
                if (hudhide::ElemIndex(kConfigSettings[i].key) >= 0) add(kSettingRow + i);
            add(kBackRow);
            return n;
        }
        if (pg == pOvFps) {
            for (const auto& row : kOrder) {
                if (!OverlayRow(row.id) || row.id == kAutoCont || SetGroup(row.id) == pFrameGen || !Visible(row.id)) continue;
                if (selectable && ReadOnly(row.id)) continue;
                add(row.id);
            }
            add(kBackRow);
            return n;
        }
        if (pg == pOvFrameMeter) {
            add(kFrameMeterRow);
            add(kFrameMeterStyleRow);
            add(kFrameMeterSizeRow);
            add(kPsAllRowsRow);
            add(kKiWindowRow);
            for (int i = 0; i < kPsToggleN; ++i) {
                if (i == 7) add(kPsAllKeysRow);  // before 'Key: Startup' (kPsToggles[7])
                add(kPsRowFirst + i);
            }
            add(kBackRow);
            return n;
        }
        if (pg == pOvController || pg == pOvInput) {
            for (int i = 0; i < kConfigN; ++i)
                if (pg == pOvInput ? IsInputDisplayKey(kConfigSettings[i].key) : IsControllerOnlyKey(kConfigSettings[i].key)) add(kSettingRow + i);
            add(kBackRow);
            return n;
        }
        if (pg == pCamera) {  // the camera options the mod already has: motion blur / depth of field / bloom (XPOST)
            for (int i : {kCamRight, kCamUp, kCamDistance, kCamLens, kCamReset}) add(kCamRowFirst + i);
            if (Visible(kEp9)) add(kEp9);
            add(kBackRow);
            return n;
        }
        if (g_rematchSession.load()) { add(kRematchRow); add(kPracticeRow); }
        add(kPageRow + pDisplay);
        add(kPageRow + pCamera);
        add(kPageRow + pSettings);
        add(kClose);
        return n;
    }
    const int page = g_pages.page;
    if (page == pRoot) {
        add(kPageRow + pSettings);  // user #13223: Settings / Boss Rematch / Practice (+ Close)
        if (g_rematchSession.load()) { add(kRematchRow); add(kPracticeRow); }
    }
    else if (page == pSettings) {
        for (int p : {pDisplay, pFrameGen, pGameplay, pSafe, pExtra, pExperimental, pDiagnostics, pBoot, pAll, pConfig}) {
            bool visible = p == pExperimental || p == pAll || p == pConfig;
            for (const auto& row : kOrder) visible |= PageContains(p, row.id) && Visible(row.id);
            if (visible) add(kPageRow + p);
        }
    } else if (page == pExperimental) {
        for (int p : {pCpu, pGpu, pScheduling, pProton}) {
            bool visible = false;
            for (const auto& row : kOrder) visible |= PageContains(p, row.id) && Visible(row.id);
            if (visible) add(kPageRow + p);
        }
    }
    if (page == pConfig) {
        for (int group = 0; group < kConfigGroupN; ++group) add(kPageRow + pCfgFirst + group);
    } else if (page >= pCfgFirst && page < pCount) {
        for (int i = 0; i < kConfigN; ++i)
            if (kConfigSettings[i].group == page - pCfgFirst) add(kSettingRow + i);
        add(kSave);
    }
    for (const auto& row : kOrder)
        if (PageContains(page, row.id) && Visible(row.id) && (!selectable || !ReadOnly(row.id))) add(row.id);
    if (page == pDisplay) { add(kTimeOfDayRow); add(kWeatherRow); }
    // All settings deliberately has exactly the original traversal order, so
    // keyed test sequences can enter it without acquiring an extra Back row.
    if (page != pRoot && page != pAll) add(kBackRow);
    return n;
}

// #38077/#38437: every page starts with 'Mod menu' (top-level page), 'Close' (close the menu) and 'Report', then its own rows.
int PageRows(int* out, bool selectable) {
    out[0] = kModMenuRow;
    out[1] = kCloseTopRow;
    out[2] = kReportRow;  // #38437
    int n = 3 + PageRowsBody(out + 3, selectable);
    if (g_pages.page == pRoot) { out[n] = out[n-1]; out[n-1] = kSaveFilesRow; ++n; }
    if (g_pages.page == pRoot) { out[n] = out[n-1]; out[n-1] = kUpdatePageRow; ++n; }
    if (g_pages.page == pRoot) { out[n] = out[n-1]; out[n-1] = kReverseModsRow; ++n; }
    return n;
}
int FirstOwnRow(const int* rows, int n) { return n > 3 ? rows[3] : n ? rows[0] : kBackRow; }
int TopLevelSel() { return g_cfg.layout ? kPageRow + pSettings : (g_rematchSession.load() ? kRematchRow : kPageRow + pDisplay); }

void EnterPage(int page) {
    if (!g_pages.Enter(page, g_sel)) return;
    int rows[kMenuRowCap];
    const int n = PageRows(rows, true);
    g_sel = page == pAll ? kClose : FirstOwnRow(rows, n);
    for (auto& repeat : g_rep) repeat = 0;
}

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
        case kFg: return g_host.knownBuild && g_host.base;
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
        return;
    }
    // The setting is telemetry only, including when FG is OFF.
    snprintf(buf, n, "%s  (read-only)", name ? name : "?");
    color = !name ? cDim : s.setting == 0 ? cGray : s.active ? cGreen : cYellow;
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
        case kPage:
            snprintf(buf, n, "%s", FlatPageName());
            color = g_page ? cOrange : cGreen;
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
        case kFreeze: onoff(g_cfg.freezeGame != 0); break;
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
};
volatile LONG g_saveState = 0;  // 0 idle, 1 running, 2 ok, 3 failed
volatile LONG g_saveErr = 0;
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
    g_saveErr = (LONG)err;
    InterlockedExchange(&g_saveState, err ? 3 : 2);
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
    add(L"Menu", L"FreezeGame", b(g_cfg.freezeGame != 0));
    add(L"Overlay", L"FrameTime", b(o.frameTime != 0));
    add(L"Overlay", L"SpeedFactor", b(o.speed != 0));
    {
        wchar_t w[32];
        swprintf(w, 32, L"%hs", osd::PositionName(o.pos));
        add(L"Overlay", L"Position", w);
        swprintf(w, 32, L"%.1f", o.scale);
        add(L"Overlay", L"Scale", w);
    }
    // Only explicitly edited catalog values override the existing live-control save.
    // Unchanged startup preferences, paths and comments are never rewritten here.
    g_configValues.Save(kConfigSettings, add);
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
    g_pages.Reset();
    // Layout=1 harness and host open on Settings. Layout=0 opens on the first root row (Boss Matches when a session is up).
    g_sel = g_cfg.layout ? kPageRow + pSettings : (g_rematchSession.load() ? kRematchRow : kPageRow + pDisplay);
    for (auto& repeat : g_rep) repeat = 0;
    MLOG("opened (%s) - game pad%s input is held neutral while open", how, g_keyHooked ? " and navigation-key" : "");
}

void Close(const char* how) {
    CloseRematchHostPage();
    for (PadSlot& s : g_pad) s.latch = true;
    InterlockedOr(&g_keyLatch, (LONG)K_NAV);
    InterlockedExchange(&g_open, 0);
    SyncFreeze();
    MessageBeep(MB_ICONASTERISK);
    const bool autoSave = g_dirty && !g_ini.empty();
    MLOG("closed (%s)%s", how, autoSave ? " - auto-saving the changed settings to the ini" : "");
    if (autoSave) StartSave();  // user #13197/#13223: no Save row on the first page; every change is kept when the menu closes
}

void LaunchUpdatePage() {
    if (updatecheck::RunningUnderWine()) {
        Close("showing update instructions on Wine");
        g_pressed=0; g_toggleReq=0;
        for (auto& repeat : g_rep) repeat=0;
        MLOG("update instructions: Wine uses in-game URL and path only; no external app launch");
    }
    updatecheck::OpenPage();
}

// user #13529: the open shortcut (Start x3, Esc x3, the menu key) opens the CURRENT mode's page: Practice / Training session (or summoned
// bosses) -> the Practice page, a Boss Match (Fight now) -> the Boss Matches page, otherwise the first page. Mode from Nioh3Training:
// Nioh3Training_SessionMode 1 Boss Match / 2 Practice, Nioh3Training_SessionActive bit 2 = summoned bosses alive. B on that page returns to the first page.
int TrainingMode() {
    static int (*mode)() = nullptr; static int (*active)() = nullptr; static ULONGLONG tried = 0;
    if (!mode && (!tried || Now() - tried > 5000)) {
        tried = Now();
        if (HMODULE m = GetModuleHandleW(L"Nioh3Training.asi")) {
            mode = reinterpret_cast<int (*)()>(GetProcAddress(m, "Nioh3Training_SessionMode"));
            active = reinterpret_cast<int (*)()>(GetProcAddress(m, "Nioh3Training_SessionActive"));
        }
    }
    int r = mode ? mode() : 0;
    if (!r && active && (active() & 2)) r = 2;
    return r;
}
menu_mode::Selection g_modeSelection;
int g_hostTab = 0;  // #44232: kRematchRow / kPracticeRow = the tab of the hosted page on screen (valid while g_rematchPage)
void OpenModePage(bool ready) {
    if (!ready || !g_rematchSession.load()) return;
    const int m = g_modeSelection.Reopen(TrainingMode());
    if (m != 1 && m != 2) return;
    if (PostRematchAction(m == 1 ? rmb::OpenRematch : rmb::OpenPractice) || PostRematchAction(rmb::OpenMenu)) {
        g_rematchPage = true; g_rematchOpened = false; g_hostTab = m == 1 ? kRematchRow : kPracticeRow;
        for (auto& repeat : g_rep) repeat = 0;
        MLOG("opened on the %s page (current mode, #13529)", m == 1 ? "Boss Matches" : "Practice");
    }
}

void CloseCover(const char* why) {
    g_coverTombstone=g_coverToken.load();g_coverTombstoneSession=g_coverSession.load();
    CloseRematchHostPage();
    for(PadSlot& p:g_pad)p.latch=false;
    g_keyLatch=0;g_pressed=0;g_toggleReq=0;for(auto& repeat:g_rep)repeat=0;g_open=0;g_coverEnabled=false;g_coverToken=0;g_coverPresented=0;
    snprintf(g_coverNotice,sizeof(g_coverNotice),"%s",why);g_coverNoticeUntil=Now()+8000;
    MLOG("transition cover closed: %s (native ownership unchanged)",why);
}
void Back() {
    if (!g_pages.Back(g_sel)) Close("B / Esc");
    for (auto& repeat : g_rep) repeat = 0;
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

bool RemoteValue(int id, int& value);
void SyncConfigFromLive(int id) {
    std::string value;
    int number = 0;
    auto& o = osd::Overlay();
    if (id == kPos) value = osd::PositionName(o.pos);
    else if (id == kScale) value = std::to_string(o.scale);
    else if (id == kOffX || id == kOffY) value = std::to_string(id == kOffX ? o.offX : o.offY);
    else if (Available(id) && RemoteValue(id, number)) value = std::to_string(number);
    for (int i = 0; i < kConfigN; ++i) {
        if (kConfigSettings[i].legacy != id) continue;
        auto& preference = g_configValues.values[(size_t)i];
        preference.changed = false;
        if (!value.empty()) preference.text = value;
    }
}
char g_configReply[256] = "";
int g_configReplyRow = -1;
void ChangeConfig(int index, int dir) {
    if (index < 0 || index >= kConfigN) return;
    const auto& d = kConfigSettings[index];
    if (!g_configValues.Change(kConfigSettings, (size_t)index, dir)) return;
    const auto& v = g_configValues.values[(size_t)index].text;
    g_configReplyRow = index;
    snprintf(g_configReply, sizeof(g_configReply), "Next launch: save to ini, then restart the game.");
    if (d.apply == setting_ui::Apply::OverlayLive) {
        auto& o = osd::Overlay();
        if (!strcmp(d.key, "Brightness")) o.brightness = (float)atof(v.c_str());
        else if (!strcmp(d.key, "StartupPopup")) o.popup = atoi(v.c_str());
        else if (!strcmp(d.key, "StartupPopupSeconds")) o.popupSeconds = (float)atof(v.c_str());
        else if (!strcmp(d.key, "StartupPopupDelay")) o.popupDelay = (float)atof(v.c_str());
        snprintf(g_configReply, sizeof(g_configReply), "Overlay value applies now; save to keep it.");
    } else if (d.apply == setting_ui::Apply::MenuLive) {
        if (!strcmp(d.key, "ButtonStyle")) {
            menu_style::g_btnStyle = _stricmp(v.c_str(), "PlayStation") == 0 ? 1 : 0;
            snprintf(g_configReply, sizeof(g_configReply), "Button style applies now. Save to keep it.");
        } else if (!strcmp(d.key, "ControllerDisplay")) {
            padviz::g_on = atoi(v.c_str()) ? 1 : 0;
            snprintf(g_configReply, sizeof(g_configReply), padviz::g_on ? "Controller display on (top-left). Save to keep it." : "Controller display off. Save to keep it.");
        } else if (!strcmp(d.key, "ControllerDisplaySize")) {
            padviz::g_size100 = std::clamp((int)std::lround(atof(v.c_str()) * 100.0), 50, 400);
            snprintf(g_configReply, sizeof(g_configReply), "Controller display size applies now. Save to keep it.");
        } else if (!strcmp(d.key, "ControllerDisplayX") || !strcmp(d.key, "ControllerDisplayY")) {
            const int pv = std::clamp((int)std::lround(atof(v.c_str())), -100, 100);
            (d.key[17] == 'X' ? padviz::g_posX : padviz::g_posY) = pv;
            snprintf(g_configReply, sizeof(g_configReply), "Controller display position applies now. Save to keep it.");
        } else if (!strcmp(d.key, "ControllerDisplayPressColor")) {
            padviz::g_pressRGB = padviz::PressColor(v.c_str());
            snprintf(g_configReply, sizeof(g_configReply), "Pressed-button colour applies now. Save to keep it.");
        } else if (!strcmp(d.key, "InputDisplay")) {
            inputhist::g_on = atoi(v.c_str()) ? 1 : 0;
            snprintf(g_configReply, sizeof(g_configReply), inputhist::g_on ? "Input display on. Save to keep it." : "Input display off. Save to keep it.");
        } else if (!strcmp(d.key, "InputDisplaySide")) {
            inputhist::g_side = _stricmp(v.c_str(), "Right") == 0 ? 1 : 0;
            snprintf(g_configReply, sizeof(g_configReply), "Input display side applies now. Save to keep it.");
        } else if (!strcmp(d.key, "InputDisplayRows")) {
            inputhist::g_rows = std::clamp((int)std::lround(atof(v.c_str())), inputhist::kMinRows, inputhist::kMaxRows);
            snprintf(g_configReply, sizeof(g_configReply), "Input display rows apply now. Save to keep it.");
        } else if (!strcmp(d.key, "HideGameHud")) {
            hudhide::g_want = atoi(v.c_str()) ? 1 : 0;
            snprintf(g_configReply, sizeof(g_configReply), hudhide::g_bad ? "Hide game HUD is not available on this game version." : hudhide::g_want ? "Game HUD hidden. Save to keep it." : "Game HUD shown. Save to keep it.");
        } else if (hudhide::ElemIndex(d.key) >= 0) {  // #44877
            const int on = atoi(v.c_str()) ? 1 : 0;
            hudhide::g_elemOn[hudhide::ElemIndex(d.key)] = on;
            snprintf(g_configReply, sizeof(g_configReply), hudhide::g_idsBad ? "Per-element HUD hide is not available on this game version." : on ? "%s hidden. Save to keep it." : "%s shown. Save to keep it.", d.label + (strncmp(d.label, "Hide ", 5) ? 0 : 5));
        } else if (!strcmp(d.key, "ControllerDisplayOpacity")) {
            padviz::g_opacity100 = std::clamp((int)std::lround(atof(v.c_str()) * 100.0), 20, 100);
            snprintf(g_configReply, sizeof(g_configReply), "Controller display opacity applies now. Save to keep it.");
        } else {
            g_cfg.scale = (float)atof(v.c_str());
            snprintf(g_configReply, sizeof(g_configReply), "Menu text size applies now; 0 = auto. Save to keep it.");
        }
    } else if (d.apply == setting_ui::Apply::TuningLive && g_host.tuning) {
        // Install-with-switch-off (2) means OFF now, install OFF on the next launch.
        const char* live = !strcmp(d.section, "Patches") && v == "2" ? "0" : v.c_str();
        if (!g_host.tuning(d.liveKey, live, g_configReply, sizeof(g_configReply)))
            snprintf(g_configReply, sizeof(g_configReply), "Live setter unavailable; save for the next launch.");
    }
    g_dirty = true;
    MLOG("[%s] %s requested %s - %s", d.section, d.key, v.c_str(), g_configReply);
}

// Real camera offsets are applied by CamKiProbe 1.0.21; no stick simulation.
// Nonzero distance/lens settings use calibrated references, not live per-scene defaults.
// Zero always removes the key, releasing the camera to the native game calculation.
constexpr float kCamDistDefault = 585.f, kCamDistStep = 60.f, kCamDistMin = 50.f, kCamDistMax = 4000.f;
constexpr float kCamFovDefault = 47.5f, kCamFovStep = 5.f, kCamFovMin = 20.f, kCamFovMax = 90.f;
float g_camDist = 0.f, g_camFov = 0.f;
float g_camRight = 0.f, g_camUp = 0.f;
char g_camNote[160] = "";
std::wstring CamIniPath() {
    const size_t slash = g_ini.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? L"" : g_ini.substr(0, slash + 1)) + L"Nioh3CamKiProbe.ini";
}
void CamReadIni() {
    if (g_ini.empty()) return;
    const auto p = CamIniPath();
    auto read = [&](const wchar_t* key, float lo, float hi) {
        wchar_t v[64]; GetPrivateProfileStringW(L"CamKiProbe", key, L"0", v, 64, p.c_str());
        float f = (float)_wtof(v);
        return std::isfinite(f) ? std::min(hi, std::max(lo, f)) : 0.f;
    };
    g_camRight = read(L"CamOffsetRight", -400.f, 400.f);
    g_camUp = read(L"CamOffsetUp", -400.f, 400.f);
    g_camDist = read(L"CamDist", 0.f, 4000.f);
    g_camFov = read(L"CamFovDeg", 0.f, 90.f);
}
bool CamAny() { return g_camDist > 0.f || g_camFov > 0.f || g_camRight != 0.f || g_camUp != 0.f; }
void CamWriteIni() {
    if (g_ini.empty()) return;
    const auto p = CamIniPath();
    wchar_t v[32];
    auto put = [&](const wchar_t* key, bool on, float val, const wchar_t* fmt) {
        swprintf(v, 32, fmt, val);
        WritePrivateProfileStringW(L"CamKiProbe", key, on ? v : nullptr, p.c_str());
    };
    put(L"CamDist", g_camDist > 0.f, g_camDist, L"%.0f");
    put(L"CamFovDeg", g_camFov > 0.f, g_camFov, L"%.1f");
    put(L"CamOffsetRight", g_camRight != 0.f, g_camRight, L"%.0f");
    put(L"CamOffsetUp", g_camUp != 0.f, g_camUp, L"%.0f");
}
// Always land on the unset center, including when coming from an old absolute
// INI value or an asymmetric clamped endpoint that is not a multiple of the step.
float CamStep(float value, int dir, float step, float lo, float hi) {
    if (!dir) return 0.f;
    if ((value < 0.f && dir > 0 && value >= -step) ||
        (value > 0.f && dir < 0 && value <= step)) return 0.f;
    return std::clamp(value + (dir < 0 ? -step : step), lo, hi);
}
float CamValue(int which) {
    if (which == kCamRight) return g_camRight;
    if (which == kCamUp) return g_camUp;
    if (which == kCamDistance) return g_camDist > 0.f ? g_camDist - kCamDistDefault : 0.f;
    return g_camFov > 0.f ? g_camFov - kCamFovDefault : 0.f;
}
void ChangeCamera(int which, int dir) {
    CamReadIni();
    if (which == kCamRight || which == kCamUp) {
        float& value = which == kCamRight ? g_camRight : g_camUp;
        value = CamStep(value, dir, 20.f, -400.f, 400.f);
    } else if (which == kCamDistance || which == kCamLens) {
        const bool distance = which == kCamDistance;
        const float ref = distance ? kCamDistDefault : kCamFovDefault;
        const float v = CamStep(CamValue(which), dir, distance ? kCamDistStep : kCamFovStep,
            (distance ? kCamDistMin : kCamFovMin) - ref, (distance ? kCamDistMax : kCamFovMax) - ref);
        (distance ? g_camDist : g_camFov) = v == 0.f ? 0.f : ref + v;
    } else if (which == kCamReset && dir == 0) {
        g_camDist = g_camFov = g_camRight = g_camUp = 0.f;
    } else return;
    snprintf(g_camNote, sizeof(g_camNote), "Saved. Applies within 2 s. Zero releases the selected override to the game's own camera.");
    CamWriteIni();
}

// dir: -1 left, +1 right, 0 = A / Enter
void Change(int id, int dir) {
    if(id==kSaveFilesRow){
        if(!dir&&(savefiles::Pending()||savefiles::ActionAllowed("open-menu"))){
            g_saveConfirm=0;g_saveName.clear();if(!savefiles::Pending())savefiles::RefreshSlots();EnterPage(pSaveFiles);
        }return;
    }
    if(id==kSaveCancelRow){if(!dir){savefiles::CancelPending();g_saveConfirm=0;}return;}
    if(id>=kSaveNamedFirst&&id<kSaveNamedFirst+savefiles::kMaxSlots){
        if(!dir&&!savefiles::Pending()&&savefiles::ActionAllowed("select-named-save")){
            const char* name=savefiles::SlotName(id-kSaveNamedFirst);
            if(*name&&!savefiles::IsActive(id-kSaveNamedFirst)){
                const auto now=Now();
                if(g_saveConfirm==id&&now-g_saveConfirmAt<5000){g_saveName=name;g_saveConfirm=0;if(savefiles::LiveAvailable())savefiles::QueueLive(name);else{g_saveConfirm=0;EnterPage(pSaveActions);}}
                else{g_saveConfirm=id;g_saveConfirmAt=now;}
            }
        }return;
    }
    if(id==kSaveLiveRow){if(!dir&&!savefiles::Pending()&&!g_saveName.empty())savefiles::QueueLive(g_saveName.c_str());g_saveConfirm=0;return;}
    if(id==kSaveOpenRow){if(!dir)savefiles::OpenFolder();return;}
    if(id==kSaveRefreshRow){if(!dir&&!savefiles::Pending()){savefiles::RefreshSlots();g_saveConfirm=0;}return;}
    if(id==kSaveBackupRow||id==kSaveSwitchRow){
        if(dir||savefiles::Pending()||!savefiles::ActionAllowed(id==kSaveBackupRow?"confirm-backup":"confirm-switch")){g_saveConfirm=0;return;}
        const auto now=Now();
        if(g_saveConfirm==id&&now-g_saveConfirmAt<5000){
            if(id==kSaveBackupRow)savefiles::QueueNewBackup();
            else if(!g_saveName.empty())savefiles::QueueNamed(g_saveName.c_str(),false);
            g_saveConfirm=0;
        }else{g_saveConfirm=id;g_saveConfirmAt=now;}
        return;
    }
    if (id == kRematchRow || id == kPracticeRow) {
        // user #13223: open the add-on directly on its Boss Rematch / Practice page; an older add-on refuses the action -> its own root
        if (dir == 0 && RefreshRematchHost(Now()) &&
            (PostRematchAction(id == kRematchRow ? rmb::OpenRematch : rmb::OpenPractice) || PostRematchAction(rmb::OpenMenu))) {
            g_rematchPage = true; g_rematchOpened = false; g_hostTab = id;
            g_modeSelection.Choose(id == kRematchRow ? 1 : 2, TrainingMode());
            for (auto& repeat : g_rep) repeat = 0;
        }
        return;
    }
    if (id == kModMenuRow) {  // #38077: back to the menu's top-level page (page trail reset, cursor on its first row)
        if (dir == 0) { g_pages.Reset(); g_sel = TopLevelSel(); for (auto& repeat : g_rep) repeat = 0; MLOG("Mod menu row: top-level page"); }
        return;
    }
    if (id == kCloseTopRow) { if (dir == 0) Close("Close row"); return; }
    if (id == kUpdatePageRow) { if (dir == 0) LaunchUpdatePage(); return; }
    if (id == kReverseModsRow) { if (dir == 0) DoReverseMods(Now()); return; }
    if (id == kReportRow) { if (dir == 0) DoReport(); return; }
    if (id >= kPsRowFirst && id < kPsRowFirst + kPsToggleN) { ChangePs(id - kPsRowFirst); return; }
    if (id == kPsAllRowsRow || id == kPsAllKeysRow) { ChangePsMaster(id == kPsAllKeysRow, dir); return; }
    if (id == kTimeOfDayRow || id == kWeatherRow) { ChangeSky(id, dir); return; }
    if (id >= kCamRowFirst && id < kCamRowFirst + kCamRowN) { ChangeCamera(id - kCamRowFirst, dir); return; }
    if (id == kFrameMeterRow) { ChangeFrameMeter(); return; }
    if (id == kFrameMeterStyleRow) { ChangeFrameMeterStyle(dir); return; }
    if (id == kFrameMeterSizeRow) { ChangeFrameMeterSize(dir); return; }
    if (id == kKiWindowRow) { ChangeKiWindow(); return; }
    if (id >= kSettingRow && id < kSettingRow + kConfigN) { ChangeConfig(id - kSettingRow, dir); return; }
    if (id >= kCount) {
        if (dir != 0) return;
        if (id == kBackRow) Back();
        else if (id < kBackRow) EnterPage(id - kPageRow);
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
    if (id == kPage) {
        g_page ^= 1;
        MLOG("settings page -> %s", FlatPageName());
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
        case kFreeze: g_cfg.freezeGame = !g_cfg.freezeGame; break;
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
            else v = std::clamp(v + (dir < 0 ? -20 : 20), id == kOffX ? -1900 : -1060, id == kOffX ? 1900 : 1060);
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
    SyncConfigFromLive(id);
    g_dirty = true;
    char v[64];
    uint32_t c;
    Value(id, v, sizeof(v), c);
    MLOG("%s%s%s -> %s", kItems[id].plain, kItems[id].tech[0] ? " " : "", kItems[id].tech, v);
}

void Move(int dir) {
    int rows[kMenuRowCap];
    const int n = PageRows(rows, true);
    if (!n) return;
    int pos = 0;
    for (int i = 0; i < n; ++i) if (rows[i] == g_sel) pos = i;
    g_sel = rows[menu_nav::Wrap(pos, dir, n)];
}

void MovePage(int dir) {
    int rows[kMenuRowCap];
    const int n = PageRows(rows, true);
    if (!n) return;
    int pos = 0;
    for (int i = 0; i < n; ++i) if (rows[i] == g_sel) pos = i;
    g_sel = rows[menu_nav::PageStep(pos, dir * 10, n)];
}

// user #44232: while the menu is open LB / RB step through the first page's destinations ("tabs"), in the order that page lists
// them (Boss Matches, Practice, Display Overlays, Camera settings, Settings, Save files - whatever is shown this session). From the
// first page itself RB goes to the first tab and LB to the last; the ends wrap. View / Create returns to the first page (TryPadHome).
bool IsTabRow(int id) { return id == kRematchRow || id == kPracticeRow || id == kSaveFilesRow || (id >= kPageRow && id < kBackRow); }
int TabRows(int* out) {
    const int page = g_pages.page;
    g_pages.page = pRoot;  // PageRows describes the current page; borrow the first page's list (Present thread only)
    int rows[kMenuRowCap];
    const int n = PageRows(rows, true);
    g_pages.page = page;
    int k = 0;
    for (int i = 0; i < n; ++i)
        if (IsTabRow(rows[i])) out[k++] = rows[i];
    return k;
}
// the tab the player is in: the hosted page's tab, else the first-page row the page trail started from (0 = on the first page)
int CurrentTab() {
    if (g_rematchPage) return g_hostTab;
    if (!g_pages.depth) return 0;
    const int top = g_pages.depth >= 2 ? g_pages.pages[1] : g_pages.page;
    return top == pSaveFiles || top == pSaveActions ? kSaveFilesRow : kPageRow + top;
}
const char* TabName(int id) {
    return id == kRematchRow ? "Boss Matches" : id == kPracticeRow ? "Practice" : id == kSaveFilesRow ? "Save files" : PageName(id - kPageRow);
}
// #44353: the tab bar row at the top of every page: Home (the first page) then the LB / RB order, short names
const char* TabShort(int id) {
    if (!id) return "Home";
    if (id == kRematchRow) return "Boss";
    if (id == kPracticeRow) return "Training";  // the first page names this row Training
    if (id == kSaveFilesRow) return "Saves";
    const int pg = id - kPageRow;
    return pg == pDisplay ? "Display" : pg == pCamera ? "Camera" : pg == pSettings ? "Optimize" : PageName(pg);
}
// draws the row at (x, y) within `cols` columns; the current tab is "[name]" in gold on the soft gold wash, the rest gray
void DrawTabBar(float x, float y, int cols, float cw, float lineH, float sc, float a) {
    namespace ms = menu_style;
    int tabs[kMenuRowCap + 1], lens[kMenuRowCap + 1];
    tabs[0] = 0;
    const int n = 1 + TabRows(tabs + 1), cur = CurrentTab();
    int ci = 0;
    for (int i = 0; i < n; ++i) {
        lens[i] = (int)strlen(TabShort(tabs[i]));
        if (tabs[i] == cur) ci = i;
    }
    menu_nav::TabSeg seg[kMenuRowCap + 1];
    bool moreL, moreR;
    const int k = menu_nav::TabBar(lens, n, ci, cols, seg, moreL, moreR);
    if (moreL) osd::DrawText(x, y, "<", ms::Fade(cDim, a), sc);
    for (int i = 0; i < k; ++i) {
        const bool on = seg[i].tab == ci;
        char b[64];
        snprintf(b, sizeof b, "%c%s%c", on ? '[' : ' ', TabShort(tabs[seg[i].tab]), on ? ']' : ' ');
        const float sx = x + (float)seg[i].col * cw;
        if (on) osd::DrawRect(sx, y - sc, (float)seg[i].len * cw, lineH, ms::Fade(ms::kAccentSoft, a));
        osd::DrawText(sx, y, b, ms::Fade(on ? ms::kAccent : cGray, a), sc);
    }
    if (moreR) osd::DrawText(x + (float)(cols - 1) * cw, y, ">", ms::Fade(cDim, a), sc);
}
void SwitchTab(int dir) {
    int tabs[kMenuRowCap];
    const int n = TabRows(tabs);
    if (!n) return;
    const int cur = CurrentTab();
    int pos = -1;
    for (int i = 0; i < n; ++i) if (tabs[i] == cur) pos = i;
    const int next = tabs[pos < 0 ? (dir > 0 ? 0 : n - 1) : menu_nav::Wrap(pos, dir, n)];
    if (next == cur) return;
    for (auto& repeat : g_rep) repeat = 0;
    const bool hosted = next == kRematchRow || next == kPracticeRow;
    if (hosted && g_rematchPage) {  // Boss Matches <-> Practice: the add-on switches its page in place (its OpenRematch / OpenPractice)
        if (PostRematchAction(next == kRematchRow ? rmb::OpenRematch : rmb::OpenPractice)) {
            g_rematchOpened = false; g_hostTab = next;
            g_modeSelection.Choose(next == kRematchRow ? 1 : 2, TrainingMode());
            MLOG("tab %s: %s", dir > 0 ? "RB" : "LB", TabName(next));
        }
        return;
    }
    if (g_rematchPage) CloseRematchHostPage();
    g_pages.Reset();
    g_sel = next;  // stays on the first page with this tab selected when the tab cannot open (add-on busy, save files locked)
    Change(next, 0);
    MLOG("tab %s: %s", dir > 0 ? "RB" : "LB", TabName(next));
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
    if (!strcmp(want, "AUTOCONTINUELOAD")) return kAutoCont;
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
        case kFreeze: v = g_cfg.freezeGame; return true;
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
    for (int i = 0; i < kConfigN; ++i) {  // catalog Choice rows live-applied by the menu (e.g. HideGameHud, HideHud*): 'menu set <key> 0|1'
        const auto& d = kConfigSettings[i];
        if (_stricmp(d.key, q.key) || d.apply != setting_ui::Apply::MenuLive || d.kind != setting_ui::Kind::Choice) continue;
        if (q.set && (atoi(g_configValues.values[(size_t)i].text.c_str()) != 0) != (q.value != 0)) ChangeConfig(i, 0);
        if (strcmp(d.key, "HideGameHud") && hudhide::ElemIndex(d.key) < 0)
            snprintf(q.reply, sizeof(q.reply), "%s = %s", d.key, g_configValues.values[(size_t)i].text.c_str());
        else  // #44877: HUD hide rows also report the widget-id redirect state
            snprintf(q.reply, sizeof(q.reply), "%s = %s (ids %s)", d.key, g_configValues.values[(size_t)i].text.c_str(),
                     hudhide::g_idsBad ? "unavailable" : hudhide::g_redirected ? "redirected" : "off");
        q.rc = 0;
        return;
    }
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
            case kFreeze:
                g_cfg.freezeGame = q.value ? 1 : 0;
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
        SyncConfigFromLive(id);
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

// Direct settings sliders have no modal edit session. While a value is being
// adjusted/accepted, consume Y without closing. Hosted modal editors advertise
// Editing; outstanding provider actions also retain input ownership.
bool TryPadHome(uint32_t pressed) {
    if (!g_open || !(pressed & N_HOME)) return false;
    CloseRematchHostPage();
    g_pages.Reset(); g_sel = TopLevelSel();
    for (auto& repeat : g_rep) repeat = 0;
    return true;
}

bool TryPadClose(uint32_t pressed, ULONGLONG now) {
    if(!g_open || !(pressed & N_CLOSE))return false;
    uint32_t editing=pressed & (N_LEFT|N_RIGHT|N_OK);
    for(int i=0;i<4;++i)
        if(now-(ULONGLONG)g_padHeldAt[i]<300)editing |= PadNav((uint32_t)g_padHeld[i]) & (N_LEFT|N_RIGHT|N_OK);
    if(now-(ULONGLONG)g_keyHeldAt<300)editing |= KeyNav((uint32_t)g_keyHeld) & (N_LEFT|N_RIGHT|N_OK);
    if(editing || (g_rematchPage && (g_rematchAwait || (g_rematchView.flags & rmb::Editing))))return true;
    Close("Y / Triangle");return true;
}

void OnPresentImpl() {
    hudhide::Sync();
    hudhide::SyncIds();
    savefiles::Poll();
    if(savefiles::TakeMenuCloseRequest()&&g_open)Close("live title reset");
    if((g_pages.page==pSaveFiles||g_pages.page==pSaveActions)&&!savefiles::Pending()&&!savefiles::Allowed()){g_pages.Reset();g_sel=kPageRow+pSettings;g_saveConfirm=0;}
    const ULONGLONG t = Now();
    g_rematchRendererTick.store(t); // resolver may attach only after an actual usable Present
    const bool rematchReady = RefreshRematchHost(t);
    if((g_rematchView.flags & rmb::CoverActive) &&
       !(g_rematchView.coverToken==g_coverTombstone.load() && g_rematchView.session==g_coverTombstoneSession.load())) {
        const bool captureHooks=g_padHooked && g_keyHooked && !g_disabled;
        if(g_coverToken.load()!=g_rematchView.coverToken) {
            g_coverPresented=0;g_coverCancel=false;g_coverToken=g_rematchView.coverToken;g_coverArmedAt=t;g_toggleReq=0;g_pressed=0;for(auto& repeat:g_rep)repeat=0;
        }
        g_coverSession=g_rematchView.session;g_coverWorker=g_rematchView.workerTickMs;
        g_coverDeadline=g_rematchView.coverDeadline;g_coverEnabled=captureHooks;
        if(!captureHooks || !rematchReady || !CoverCaptures(t)) {
            CloseCover("cancelled/stale/unavailable");
        } else {
            // Keep native simulation running; do not call PauseDetect/frame-advance.
            g_lastPresent=(LONG64)t;OwnPolling(t);return;
        }
    } else if(g_coverToken.load()) {
        MLOG("transition finished: %s",g_rematchView.coverReason);
        CloseCover(g_rematchView.coverReason);
    }

    g_lastPresent = (LONG64)t;
    OwnPolling(t);
    SampleRates(t);
    PauseDetect(t);  // also feeds the frame-generation status row (why FG is paused)
    ProcessRemote();  // vpad pipe: 'menu get / set' (1.7.37)
    // The add-on opened its own menu (arena Return gate page): the host draws and drives it, so open the host menu on the Rematch page.
    // Without this the add-on's menu state was 'open' but nothing was drawn and no input reached it (user: Return / Fight again did nothing).
    if (rematchReady && !g_disabled && !g_open && !g_rematchPage && !g_rematchClosePending && !g_rematchAwait && !g_coverToken.load() &&
        (g_rematchView.flags & rmb::Open) && t >= g_rematchNoAutoOpenUntil) {
        Open("Boss Rematch request");
        g_rematchPage = true; g_rematchOpened = true; g_rematchFromAddon = true;
        for (auto& repeat : g_rep) repeat = 0;
    }
    if (InterlockedExchange(&g_extCloseReq, 0) && g_open) Close("add-on request: a native box is open, the player answers it");  // #13796 safety net
    if (InterlockedExchange(&g_toggleReq, 0) && t - (ULONGLONG)g_toggleAt < 1000) {
        const LONG src = g_toggleSrc;
        if (g_open) Close(SrcName(src));
        else { Open(SrcName(src)); OpenModePage(rematchReady); }
    }
    if (!g_open) {
        InterlockedExchange(&g_pressed, 0);
        g_rep[0] = g_rep[1] = g_rep[2] = g_rep[3] = 0;
        return;
    }
    const uint32_t pressed = (uint32_t)InterlockedExchange(&g_pressed, 0);
    if (TryPadHome(pressed)) return;
    if (TryPadClose(pressed,t)) return;
    if (g_rematchPage) {
        if (!rematchReady) {
            Close("rematch worker stale/unavailable");
            return; // fail closed; ordinary neutral-release latches still apply
        }
        // #13796: a mode page opened by the shortcut (OpenModePage, g_rematchOpened false) that B closed at its base also leaves at once
        // (B = back one page = the host's first page), instead of waiting for a second B on a page that no longer draws anything.
        if ((g_rematchOpened || g_rematchLastAction == rmb::Back) && !(g_rematchView.flags & rmb::Open) && !g_rematchAwait) {
            // user #13796: a page the add-on opened by itself (the physical Return object) is the whole menu: B closes it, it never
            // falls back to the mod menu's first page (that second screen is where the player got stuck).
            const bool toHost = (g_rematchView.flags & rmb::HostMenu) != 0;  // #38077: hosted 'Mod menu' row
            const bool back = g_rematchLastAction == rmb::Back || toHost, fromAddon = g_rematchFromAddon && !toHost;
            CloseRematchHostPage();
            if (toHost) { g_pages.Reset(); g_sel = TopLevelSel(); }
            else if (!back) Close("rematch action");
            else if (fromAddon) Close("B on the Return object page");
            return;
        }
        if (g_rematchAwait || !(g_rematchView.flags & rmb::Open)) {
            if (pressed & N_BACK) { const bool fromAddon = g_rematchFromAddon; CloseRematchHostPage(); if (fromAddon) Close("B on the Return object page"); }
            return;
        }
        // One normalized edge per frame; Back wins, then paging/navigation.
        // The addon worker rechecks the displayed generation before dispatch.
        // #44232: bumpers switch tabs, except inside a value editor and on the Return object's own page (that page is the whole menu).
        const uint32_t tab = pressed & (N_TAB_PREV | N_TAB_NEXT);
        if (pressed & N_BACK) PostRematchAction(rmb::Back);
        else if (tab) { if (tab != (N_TAB_PREV | N_TAB_NEXT) && !g_rematchFromAddon && !(g_rematchView.flags & rmb::Editing)) SwitchTab(tab == N_TAB_NEXT ? 1 : -1); }
        else if (pressed & N_PAGEUP) PostRematchAction(rmb::PageUp);
        else if (pressed & N_PAGEDOWN) PostRematchAction(rmb::PageDown);
        else if (pressed & N_UP) PostRematchAction(rmb::Up);
        else if (pressed & N_DOWN) PostRematchAction(rmb::Down);
        else if (pressed & N_LEFT) PostRematchAction(rmb::Left);
        else if (pressed & N_RIGHT) PostRematchAction(rmb::Right);
        else if (pressed & N_OK) PostRematchAction(rmb::Accept);
        return;
    }
    // Back owns a mixed input frame: cancelling a confirmation/page must never
    // activate the row being left, even if A arrives in the same Present.
    if (pressed & N_BACK) {
        Back();
        return;
    }
    if (const uint32_t tab = pressed & (N_TAB_PREV | N_TAB_NEXT)) {  // #44232: one bumper = one tab; both at once = nothing
        if (tab != (N_TAB_PREV | N_TAB_NEXT)) SwitchTab(tab == N_TAB_NEXT ? 1 : -1);
        return;
    }
    if (pressed & (N_PAGEUP | N_PAGEDOWN)) {
        if ((pressed & (N_PAGEUP | N_PAGEDOWN)) == N_PAGEUP) MovePage(-1);
        else if ((pressed & (N_PAGEUP | N_PAGEDOWN)) == N_PAGEDOWN) MovePage(1);
        return;
    }
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

bool g_updateMouseDown = false, g_updateMouseEdge = false;
bool TryUpdateHintClick(bool edge, bool focused, float px, float py, float x, float y, float w, float h) {
    if (!edge || !focused || px < x || py < y || px >= x+w || py >= y+h) return false;
    LaunchUpdatePage();
    return true;
}
// Hit test in render pixels, mapped from the foreground game window's client area.
void PollUpdateHint(unsigned W, unsigned H, float x, float y, float w, float h) {
    HWND window = GetForegroundWindow(); DWORD pid = 0;
    if (!window || !GetWindowThreadProcessId(window, &pid) || pid != GetCurrentProcessId()) return;
    POINT p{}; RECT client{};
    if (!GetCursorPos(&p) || !ScreenToClient(window, &p) || !GetClientRect(window, &client) ||
        client.right <= 0 || client.bottom <= 0) return;
    TryUpdateHintClick(g_updateMouseEdge, true, p.x * (float)W / client.right,
        p.y * (float)H / client.bottom, x, y, w, h);
}
// Top-right reminder with a single clickable, controller-friendly menu instruction.
float DrawCornerNote(unsigned W, unsigned H, const char* text, uint32_t color) {
    char update[2048]{};
    const bool hasUpdate = updatecheck::CopyNotice(update, sizeof(update));
    const size_t chars = hasUpdate ? std::max(strlen(text), strlen(update)) : strlen(text);
    const float normalScale = std::max(1.0f, std::round(AutoScale(H) * 0.75f * 2.0f) / 2.0f);
    const float sc = std::min(normalScale, std::max(0.25f, ((float)W - 2 * osd::kCornerPx) / ((float)chars * osd::kGlyphW + 8.f)));
    const float pad = 4 * sc, lineH = (osd::kGlyphH + 2) * sc, margin = osd::kCornerPx;
    const float w = (float)chars * osd::kGlyphW * sc + 2 * pad, h = (hasUpdate ? 2 : 1) * lineH + 2 * pad - 4 * sc;
    float y = margin;
    const osd::OverlaySettings& o = osd::Overlay();
    if (o.pos == 1 && o.fpsCounter) {
        const float csc = osd::CounterScale(H);
        y += (float)osd::OffsetY() * (float)H / 1080.0f + (osd::kGlyphH + 2) * csc + 8 * csc - 4 * csc + 4 * csc;
    }
    const float x = (float)W - margin - w;
    menu_style::Panel(x, y, w, h, sc, menu_style::kPanelBgSide, 1.0f, false);  // rounded, no gold edge (#38505)
    osd::DrawText(x + pad, y + pad, text, color, sc);
    if (hasUpdate) {
        osd::DrawText(x + pad, y + pad + lineH, update, cGold, sc);
        if (strncmp(update, "Game folder:", 12) != 0)
            PollUpdateHint(W, H, x, y + pad + lineH, w, lineH);
    }
    return sc;
}

// bridge 12773 (user #12772, #13257): the Rematch / Practice / Training page is a compact semi-transparent side panel docked right
// (58 columns, was 44; at most ~66% of the screen height), so the boss and the arena stay visible while toggling moves / summoning.
// The worker sends plain text rows; this only re-lays them out for display (no menu logic here): a title bar (page + status), rows
// with a selection bar, values right-aligned in one column (ON / OFF / RPT pills, slider bars for the known timed values, '>' for
// submenus), wrapped notes, then the selected row's explanation, the toast and a button-prompt line. Long pages scroll (scroll bar).
namespace hostui {
struct Item {
    char label[112], value[112], note[224];
    uint32_t c;
    bool sel, sub, adj, section;
    int indent;
    float slider;  // 0..1, < 0 = none
};
void Trim(char* s) {
    size_t n = strlen(s);
    while (n && s[n - 1] == ' ') s[--n] = 0;
    size_t k = 0;
    while (s[k] == ' ') ++k;
    if (k) memmove(s, s + k, n - k + 1);
}
void Squeeze(char* s) {  // runs of spaces -> two spaces (table columns stay apart, no wide gaps)
    char* o = s;
    int run = 0;
    for (const char* p = s; *p; ++p) {
        run = *p == ' ' ? run + 1 : 0;
        if (run <= 2) *o++ = *p;
    }
    *o = 0;
}
bool Cut(char* s, const char* what) {  // removes one occurrence of `what` that ends the text or is followed by a space
    for (char* p = strstr(s, what); p; p = strstr(p + 1, what)) {
        const size_t n = strlen(what);
        if (!p[n] || p[n] == ' ') { memmove(p, p + n, strlen(p + n) + 1); return true; }
    }
    return false;
}
void Parse(const char* text, uint32_t color, bool sel, Item& o) {
    memset(&o, 0, sizeof o);
    o.c = color; o.sel = sel; o.slider = -1;
    if (!strncmp(text, "-- ", 3)) {  // user #13279: section header row (not selectable)
        snprintf(o.label, sizeof o.label, "%s", text + 3);
        const size_t n = strlen(o.label);
        if (n >= 3 && !strcmp(o.label + n - 3, " --")) o.label[n - 3] = 0;
        o.section = true;
        return;
    }
    const char* s = text;
    if (*s == '>' || *s == ' ') ++s;
    if (*s == ' ') ++s;
    int sp = 0;
    while (s[sp] == ' ') ++sp;
    o.indent = sp >= 2;
    char b[112];
    snprintf(b, sizeof b, "%s", s + sp);
    // control hints move to the prompt line; they only set flags here
    static const char* const kAdj[] = {"   A / Left fight / Right stand still", "   A / Left CPU / Right Idle", "   A / Left / Right", "   Left / Right", "  Left / Right"};
    static const char* const kSub[] = {"   A / Enter opens", "   A: open"};
    for (const char* h : kAdj) if (Cut(b, h)) { o.adj = true; break; }
    for (const char* h : kSub) if (Cut(b, h)) { o.sub = true; break; }
    Cut(b, "   B / Esc / Backspace");
    // human #44361: "left<TAB>right" = an explicit right column (Boss Rematch move rows: '1 : Anim 1.0s  1Hit3<TAB>Attack  [ON]')
    if (char* tab = strchr(b, '	')) {
        *tab = 0;
        snprintf(o.label, sizeof o.label, "%s", b);
        snprintf(o.value, sizeof o.value, "%s", tab + 1);
        Trim(o.label);
        Trim(o.value);
        return;
    }
    // move switch rows: "0x1234  ON   label"
    if (b[0] == '0' && b[1] == 'x' && strlen(b) > 8 && b[6] == ' ') {
        char st[8] = {};
        int i = 6;
        while (b[i] == ' ') ++i;
        int k = 0;
        while (b[i] && b[i] != ' ' && k < 7) st[k++] = b[i++];
        snprintf(o.value, sizeof o.value, "%s", menu_style::PillText(st));
        snprintf(o.label, sizeof o.label, "%s", b + i);
        Trim(o.label);
        snprintf(o.note, sizeof o.note, "%.6s", b);
        return;
    }
    const char* colon = o.sub ? nullptr : strstr(b, ": ");
    const char* gap = strstr(b, "   ");
    if (colon && (!gap || gap > colon) && !memchr(b, '(', (size_t)(colon - b)) && colon - b <= 40 && strncmp(b, "Arena: ", 7)) {
        snprintf(o.label, sizeof o.label, "%.*s", (int)(colon - b), b);
        char rest[112];
        snprintf(rest, sizeof rest, "%s", colon + 1);
        Trim(rest);
        const char* g2 = strstr(rest, "   ");
        snprintf(o.value, sizeof o.value, "%.*s", g2 ? (int)(g2 - rest) : (int)strlen(rest), rest);
        Trim(o.value);
        if (strcmp(menu_style::PillText(o.value), o.value)) { char v[8]; snprintf(v, sizeof v, "%s", menu_style::PillText(o.value)); snprintf(o.value, sizeof o.value, "%s", v); }
        if (g2) { snprintf(o.note, sizeof o.note, "%s", g2); Trim(o.note); }
    } else {
        const char* body = strncmp(b, "Arena: ", 7) ? b : b + 7;
        const char* g = strstr(body, "   ");
        snprintf(o.label, sizeof o.label, "%.*s", g ? (int)(g - body) : (int)strlen(body), body);
        Trim(o.label);
        if (g) {
            snprintf(o.note, sizeof o.note, "%s", g);
            Trim(o.note);
            if (o.note[0] == '-' && o.note[1] == ' ') memmove(o.note, o.note + 2, strlen(o.note + 2) + 1);
        }
    }
    Squeeze(o.note);
    // display-only ranges of the worker's timed sliders (rematch.cpp TuneStep / RepGapStep / SumPassiveStep)
    static const struct { const char* label; float lo, hi; } kRange[] = {
        {"Wait before attacking", 0.f, 30.f}, {"Gap between repeats", 0.f, 10.f}, {"Passive time", 0.f, 30.f}, {"Repeat delay", 0.f, 10.f}, {"Finisher kill window", 0.5f, 30.f}, {"HP-0 combo window", 0.2f, 5.f}};
    for (const auto& r : kRange)
        if (!strcmp(o.label, r.label) && o.value[0] >= '0' && o.value[0] <= '9') o.slider = ((float)atof(o.value) - r.lo) / (r.hi - r.lo);
}
}  // namespace hostui

float DrawRematchHost(unsigned W, unsigned H) {
    namespace ms = menu_style;
    constexpr int kCols = 58;
    const float scale = PanelScale(W, H);
    const float cw = osd::kGlyphW * scale, line = (osd::kGlyphH + 4.f) * scale, pad = 8.f * scale, a = ms::OpenFade(Now());
    const float width = std::min((float)W - 16.f, (float)kCols * cw + pad * 2.f + cw);
    const float x = std::floor((float)W - width - 12.f), y0 = 12.f, tx = x + pad + 3.f * scale;  // tx: text left (after the stripe)
    const float maxH = 0.66f * (float)H;
    if (!g_rematchFreshAt || Now() - g_rematchFreshAt >= 1500 || !(g_rematchView.flags & rmb::Open)) {
        ms::Panel(x, y0, width, line * 2.f + pad * 2.f, scale, ms::kPanelBgSide, a);
        osd::DrawText(tx, y0 + pad, "Mod menu", ms::Fade(ms::kTitleText, a), scale);
        osd::DrawText(tx, y0 + pad + line, "Waiting for the menu worker. B: back.", ms::Fade(cGray, a), scale);
        return scale;
    }
    // ---- title row "<page> (<version>) - <status>"
    char title[112] = "Mod menu", status[112] = "";
    uint32_t first = 0;
    if (g_rematchView.rowCount && !g_rematchView.rows[0].selected) {
        const char* t = g_rematchView.rows[0].text;
        const char* ver = strstr(t, " (");
        const char* dash = strstr(t, ") - ");
        if (ver && dash && dash > ver) {
            snprintf(title, sizeof title, "%.*s", (int)(ver - t), t);
            snprintf(status, sizeof status, "%s", dash + 4);
        } else snprintf(title, sizeof title, "%s", t);
        first = 1;
    }
    if (strlen(title) > (size_t)kCols) memcpy(title + kCols - 3, "...", 4);
    if (strlen(status) > (size_t)kCols) memcpy(status + kCols - 3, "...", 4);
    uint32_t statusC = cGray;
    if (!strcmp(status, "ready")) statusC = cGreen;
    else if (!strncmp(status, "active:", 7)) statusC = cYellow;
    // ---- display lines. right = this line carries the item's value column; inl = note drawn on the previous line (no advance)
    struct Ln { char t[kCols + 1]; int col; uint32_t c; int item; bool right, inl; };
    static hostui::Item items[rmb::kMaxRows];
    static Ln lines[160];
    int nItems = 0, n = 0, selFirst = -1, selLast = -1;
    auto push = [&](const char* t, size_t len, int col, uint32_t c, int item) {
        if (n >= 160) return;
        Ln& l = lines[n++];
        snprintf(l.t, sizeof l.t, "%.*s", (int)std::min(len, (size_t)(kCols - col)), t);
        l.col = col; l.c = c; l.item = item; l.right = l.inl = false;
    };
    // word wrap into lines of `cols` columns at column `col`; the first line may be shorter (`firstCols`)
    auto wrap = [&](const char* t, int col, int cols, int firstCols, uint32_t c, int item) {
        int lim = firstCols;
        do {
            size_t k = std::min(strlen(t), (size_t)std::max(1, lim));
            if (t[k] && k > 1) { size_t s = k; while (s && t[s] != ' ') --s; if (s) k = s; }
            push(t, k, col, c, item);
            t += k;
            while (*t == ' ') ++t;
            lim = cols;
        } while (*t && n < 160);
    };
    auto rightCols = [](const hostui::Item& it) {
        if (it.slider >= 0) return 12 + (int)strlen(it.value);
        if (it.value[0]) return (int)strlen(it.value) + (it.sel && it.adj ? 4 : 1);
        return it.sub ? 2 : 0;
    };
    for (uint32_t i = first; i < g_rematchView.rowCount && nItems < (int)rmb::kMaxRows; ++i) {
        const auto& row = g_rematchView.rows[i];
        hostui::Item& it = items[nItems];
        hostui::Parse(row.text, row.color | 0xFF000000u, row.selected != 0, it);
        if (strlen(it.value) > (size_t)kCols - 8 && it.slider < 0) {  // a value that long reads better as a wrapped note line
            char nb[224];
            snprintf(nb, sizeof nb, "%s%s%s", it.value, it.note[0] ? "  " : "", it.note);
            memcpy(it.note, nb, sizeof nb);
            it.value[0] = 0;
        }
        if (it.section) {  // a blank gap above every header but the first, then the gold header line
            if (n > 0 && n < 160) { push("", 0, 0, cDim, nItems); }
            push(it.label, strlen(it.label), 0, ms::kAccent, nItems);
            ++nItems;
            continue;
        }
        const int col = it.indent * 2, rc = rightCols(it), full = kCols - col;
        int labelCols = full - (rc ? rc + 2 : 0);
        const bool ownLine = rc && labelCols < 14;  // value too wide next to the label: it gets its own line below
        if (ownLine) labelCols = full;
        const int start = n;
        wrap(it.label[0] ? it.label : " ", col, full, labelCols, it.c, nItems);
        if (ownLine) { push("", 0, col, it.c, nItems); lines[n - 1].right = true; }
        else if (rc && start < n) lines[start].right = true;
        if (it.note[0] && n > start) {
            const Ln& last = lines[n - 1];
            const int used = last.col + (int)strlen(last.t), room = kCols - used - 2 - (last.right ? rc + 2 : 0);
            if ((int)strlen(it.note) <= room) { push(it.note, strlen(it.note), used + 2, cDim, nItems); lines[n - 1].inl = true; }
            else wrap(it.note, col + 2, full - 2, full - 2, cDim, nItems);
        }
        if (it.sel) { selFirst = start; selLast = n - 1; }
        ++nItems;
    }
    auto visual = [&](int from, int to) { int v = 0; for (int i = from; i < to; ++i) v += !lines[i].inl; return v; };
    const int bodyN = n, bodyV = visual(0, n);
    if (g_rematchView.footer[0]) wrap(g_rematchView.footer, 0, kCols, kCols, cGray, -1);
    const int toastStart = n;
    if (g_rematchView.toast[0]) wrap(g_rematchView.toast, 1, kCols - 1, kCols - 1, g_rematchView.toastColor | 0xFF000000u, -1);
    const int tailV = n - bodyN;
    const bool statusOwnLine = status[0] && (int)(strlen(title) + strlen(status)) + 3 > kCols;
    const bool tabHints = !g_rematchFromAddon;  // #44232
    const float tabBarH = tabHints ? line : 0.f;  // #44353: tab bar row inside the title band, under the page name
    const float titleH = line * (statusOwnLine ? 2.f : 1.f) + pad * 1.5f + tabBarH, hintH = line * (tabHints ? 2.f : 1.f) + pad;
    const float tailH = tailV ? line * (float)tailV + pad : 0.f;
    const int room = std::max(4, (int)((maxH - titleH - pad - tailH - hintH) / line));
    // scroll window over the body (visual lines), the selected item kept in view with one line of context
    static int scroll = 0;
    int firstV = 0, shownV = bodyV;
    if (bodyV > room) {
        shownV = room;
        const int s0 = selFirst < 0 ? 0 : visual(0, selFirst), s1 = selLast < 0 ? 0 : visual(0, selLast + 1) - 1;
        if (s0 - 1 < scroll) scroll = std::max(0, s0 - 1);
        if (s1 + 2 > scroll + shownV) scroll = s1 + 2 - shownV;
        scroll = std::clamp(scroll, 0, bodyV - shownV);
        firstV = scroll;
    } else scroll = 0;
    const float bodyH = line * (float)shownV, h = maxH;  // #38102: fixed height, the panel does not resize while navigating
    ms::Panel(x, y0, width, h, scale, ms::kPanelBgSide, a);
    // title bar: page name left, worker status right (or below when both do not fit)
    ms::PanelBand(x, y0, width, h, scale, 2.f * scale, titleH - 2.f * scale, scale, ms::Fade(ms::kTitleBg, a));
    osd::DrawText(tx, y0 + pad, title, ms::Fade(ms::kTitleText, a), scale);
    if (status[0]) {
        if (statusOwnLine) osd::DrawText(tx, y0 + pad + line, status, ms::Fade(statusC, a), scale);
        else osd::DrawText(x + width - pad - (float)strlen(status) * cw, y0 + pad, status, ms::Fade(statusC, a), scale);
    }
    if (tabHints) DrawTabBar(tx, y0 + pad + line * (statusOwnLine ? 2.f : 1.f), kCols, cw, line, scale, a);
    ms::Divider(x + scale, y0 + titleH, width - 2.f * scale, scale, a);
    // body
    const float bodyTop = y0 + titleH + pad * 0.5f, rx = x + width - pad - (bodyV > shownV ? cw * 0.5f : 0.f);  // rx: value right edge
    for (int i = 0, v = -1; i < bodyN; ++i) {
        const Ln& l = lines[i];
        if (!l.inl) ++v;
        if (v < firstV || v >= firstV + shownV) continue;
        const float ly = bodyTop + line * (float)(v - firstV) + 2.f * scale;
        const hostui::Item& it = items[l.item];
        if (it.sel && !l.inl) ms::SelectBar(x + scale, ly - 2.f * scale, width - 2.f * scale, line, scale, a);
        osd::DrawText(tx + (float)l.col * cw, ly, l.t, ms::Fade(it.sel && l.c != cDim ? cWhite : l.c, a), scale);
        if (it.section && l.t[0]) {  // header: a thin gold rule after the name
            const float hx = tx + (float)strlen(l.t) * cw + cw;
            if (hx < x + width - pad) ms::Divider(hx, ly + line * 0.5f - 2.f * scale, x + width - pad - hx, scale, a);
            continue;
        }
        if (!l.right) continue;
        const uint32_t vc = it.sel ? cWhite : it.c;
        const float vw = (float)strlen(it.value) * cw;
        if (it.slider >= 0) {
            osd::DrawText(rx - vw, ly, it.value, ms::Fade(vc, a), scale);
            ms::Slider(rx - vw - cw * 11.f, ly, cw * 10.f, line, it.slider, scale, a);
        } else if (it.value[0]) {
            uint32_t bg, fg;
            const float vx = rx - vw - (it.sel && it.adj ? cw * 2.f : cw * 0.5f);
            const char* mark = strrchr(it.value, ' ');  // human #44361: "Attack  [ON]" = text + a trailing state pill
            if (ms::PillColors(it.value, bg, fg)) ms::Pill(vx, ly, it.value, bg, it.c == cDim ? cDim : fg, cw, line, scale, a);
            else if (mark && mark[1] == '[' && ms::PillColors(mark + 1, bg, fg)) {
                const int pre = (int)(mark - it.value);
                char head[112];
                snprintf(head, sizeof head, "%.*s", pre, it.value);
                hostui::Trim(head);
                osd::DrawText(vx, ly, head, ms::Fade(vc, a), scale);
                ms::Pill(vx + (float)(pre + 1) * cw, ly, mark + 1, bg, it.c == cDim ? cDim : fg, cw, line, scale, a);
            } else osd::DrawText(vx, ly, it.value, ms::Fade(it.sel ? cYellow : vc, a), scale);
            if (it.sel && it.adj) {
                osd::DrawText(vx - cw * 1.5f, ly, "<", ms::Fade(ms::kAccent, a), scale);
                osd::DrawText(vx + vw + cw * 0.5f, ly, ">", ms::Fade(ms::kAccent, a), scale);
            }
        } else if (it.sub) osd::DrawText(rx - cw, ly, ">", ms::Fade(it.sel ? ms::kAccent : cDim, a), scale);
    }
    if (bodyV > shownV) {  // scroll bar on the right edge of the body
        const float sx = x + width - pad * 0.5f - scale, th = std::max(line, bodyH * (float)shownV / (float)bodyV);
        osd::DrawRect(sx, bodyTop, 2.f * scale, bodyH, ms::Fade(ms::kTrack, a));
        osd::DrawRect(sx, bodyTop + (bodyH - th) * (float)firstV / (float)(bodyV - shownV), 2.f * scale, th, ms::Fade(ms::kAccent, a));
    }
    // explanation of the selected row, then the toast (with a coloured marker)
    float y = bodyTop + bodyH + pad * 0.5f;
    if (tailV) {
        ms::Divider(x + pad, y, width - 2.f * pad, scale, a);
        y += pad * 0.5f;
        for (int i = bodyN; i < n; ++i, y += line) {
            if (i == toastStart) osd::DrawRect(tx, y, 2.f * scale, line - 2.f * scale, ms::Fade(lines[i].c, a));
            osd::DrawText(tx + (float)lines[i].col * cw, y + 2.f * scale, lines[i].t, ms::Fade(lines[i].c, a), scale);
        }
    }
    // button prompts in a darker footer band
    y = y0 + h - hintH;
    ms::PanelBand(x, y0, width, h, scale, h - hintH, hintH - scale, scale, ms::Fade(ms::kFooterBg, a));
    const float hy = y + std::floor((hintH - scale - ms::BadgeH(scale) - (tabHints ? line : 0.f)) * 0.5f);  // badges centred in the band (#13537)
    static const ms::HintItem kPad[] = {{"A", "Select"}, {"B", "Back"}, {"Y", "Close"}, {"LR", "Change"}, {"UD", "Move"}};
    ms::HintRow(tx, hy, x + width - pad, kPad, 5, cw, line, scale, a);
    if (tabHints) {  // #44232: second prompt line, LB / RB tabs + View first page (not on the Return object's own page)
        const ms::HintItem kTabs[] = {{"LB RB", "Switch tab"}, {ms::PadWord("View", "Create"), "First page"}};
        ms::HintRow(tx, hy + line, x + width - pad, kTabs, 2, cw, line, scale, a);
    }
    return scale;
}

// #8902/#9133 Nioh3EngineFix_LoadScreen: the same 'Loading boss rematch' look for Nioh3Training's own transitions (Return to where I was),
// title + stage text given by the caller. Independent of the Rematch cover state machine (no native request, no input capture).
SRWLOCK g_lsLock=SRWLOCK_INIT;volatile LONG g_lsOn=0;char g_lsTitle[64]="";char g_lsStage[96]="";
float DrawImpl(unsigned W, unsigned H) {
    const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    g_updateMouseEdge = mouseDown && !g_updateMouseDown;
    g_updateMouseDown = mouseDown;
    g_coverDrawnToken=0;
    if(g_lsOn && !g_coverToken.load() && !g_open) {
        char title[64],stage[96];
        AcquireSRWLockShared(&g_lsLock);memcpy(title,g_lsTitle,sizeof title);memcpy(stage,g_lsStage,sizeof stage);ReleaseSRWLockShared(&g_lsLock);
        if(!osd::DrawCoverRect((float)W,(float)H))return 0;
        const float scale=W<960?1.25f:2.0f;
        const float x=(float)W*0.06f,y=(float)H*0.43f;
        const float width=(float)W*0.72f<640.0f?(float)W*0.72f:640.0f;
        osd::DrawText(x,y,title,0xFFFFFFFF,scale);
        const float travel=width*0.78f;
        const float cycle=(float)(Now()%1800)/900.0f;
        const float offset=travel*(cycle<=1.0f?cycle:2.0f-cycle);
        osd::DrawRect(x,y+52.0f,width,8.0f,0xFF3A3A3A);
        osd::DrawRect(x+offset,y+52.0f,width*0.22f,8.0f,0xFFE6E6E6);
        if(stage[0])osd::DrawText(x,y+78.0f,stage,0xFFBBBBBB,1.0f);
        return scale;
    }
    if(!g_coverToken.load() && Now()<g_coverNoticeUntil && !g_open) {
        osd::DrawText(30.0f,40.0f,g_coverNotice,0xFFFFFFFF,1.0f);return 1.0f;
    }
    if(g_coverToken.load() && !g_coverCancel.load() && g_coverEnabled.load()) {
        if(!osd::DrawCoverRect((float)W,(float)H))return 0;
        const float scale=W<960?1.25f:2.0f;
        const float x=(float)W*0.06f,y=(float)H*0.43f;
        const float width=(float)W*0.72f<640.0f?(float)W*0.72f:640.0f;
        osd::DrawText(x,y,"Loading boss match",0xFFFFFFFF,scale);  // user #13293: Rematch -> Boss Matches
        // Indeterminate activity, never a fabricated asset/time percentage.
        // Stage labels derive only from the coherent provider phase.
        const float travel=width*0.78f;
        const float cycle=(float)(Now()%1800)/900.0f;
        const float offset=travel*(cycle<=1.0f?cycle:2.0f-cycle);
        osd::DrawRect(x,y+52.0f,width,8.0f,0xFF3A3A3A);
        osd::DrawRect(x+offset,y+52.0f,width*0.22f,8.0f,0xFFE6E6E6);
        const char* stage=g_rematchView.coverPhase==1?"Preparing...":
            (g_rematchView.coverPhase==2?"Starting boss match...":"Entering boss arena...");
        osd::DrawText(x,y+78.0f,stage,0xFFBBBBBB,1.0f);
        osd::DrawText(x,(float)H-50.0f,"B / Escape: show game",0xFFBBBBBB,1.0f);
        g_coverDrawnToken=g_coverToken.load();return scale;
    }

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

    if (g_rematchPage) return DrawRematchHost(W, H);
    // ---- the menu panel: title, a scrolling body (section headers + items in kOrder), then the explanation of the selected
    // row, the diagnostics lines and the footer. Rows are filled by put(); kValueCol keeps the values in one column.
    Row head;
    Row body[128], tail[28];
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
    // user #13279/#13296: plain title (the page name), the version goes to the right edge of the title bar
    snprintf(title, sizeof(title), "%s", PageName(g_pages.page));
    {
        int c = 0;
        char ver[40];
        snprintf(ver, sizeof(ver), "EngineFix v%s", g_host.version);
        put(&head, c, 1, title, "", ver, cGold, cDim, false, true);
    }
    const char* pending = nullptr;
    int pageRows[kMenuRowCap];
    const int pageN = PageRows(pageRows, false);
    for (int oi = 0; oi < pageN; ++oi) {
        const int id = pageRows[oi];
        if (id == kRematchRow || id == kPracticeRow) {
            if (id == g_sel) selRow = nb;
            const char* label = id == kRematchRow ? (id == g_sel ? "> Boss Matches" : "  Boss Matches") : (id == g_sel ? "> Training" : "  Training");
            put(body, nb, 128, label, "", "A: open", cWhite, cCyan, id == g_sel, false);
            continue;
        }
        if(id>=kSaveNamedFirst&&id<kSaveNamedFirst+savefiles::kMaxSlots){
            if(id==g_sel)selRow=nb;
            char text[96];snprintf(text,sizeof text,"%s%s%s",id==g_sel?"> ":"  ",savefiles::IsActive(id-kSaveNamedFirst)?"[x] ":"[ ] ",savefiles::SlotName(id-kSaveNamedFirst));
            put(body,nb,128,text,savefiles::IsActive(id-kSaveNamedFirst)?"in use":"","A: switch to this save",cWhite,cCyan,id==g_sel,false);continue;
        }
        if(id>=kSaveFilesRow&&id<=kSaveOpenRow){
            if(id==g_sel)selRow=nb;
            const char* label=id==kSaveFilesRow?"Save files":id==kSaveSlotRow?"Copy slot":id==kSaveBackupRow?"Back up current save":id==kSaveCancelRow?"Cancel queued save":id==kSaveRefreshRow?"Refresh saved slots":id==kSaveLiveRow?"Import now, refresh title":id==kSaveOpenRow?"Open saves folder":"Switch after Quit";
            char text[96],value[32]{};snprintf(text,sizeof text,"%s%s",id==g_sel?"> ":"  ",label);

            put(body,nb,128,text,value,"A: select",cWhite,cCyan,id==g_sel,false);continue;
        }
        if (id == kUpdatePageRow) {
            if (id == g_sel) selRow = nb;
            put(body, nb, 128, id == g_sel ? "> Update + Page" : "  Update + Page", "",
                "A: open", cWhite, cCyan, id == g_sel, false);
            continue;
        }
        if (id == kReverseModsRow) {
            if (id == g_sel) selRow = nb;
            const bool off = reversemods::IsOff(g_reverseDir.empty() ? reversemods::GameDir() : g_reverseDir);
            const bool armed = g_reverseConfirmAt && Now() - g_reverseConfirmAt <= 5000;
            put(body, nb, 128, id == g_sel ? "> Reverse Mods" : "  Reverse Mods", off ? "OFF next start" : "",
                armed ? "A again: confirm" : off ? "A: put back" : "A: turn off", cWhite, armed ? cYellow : cCyan, id == g_sel, false);
            continue;
        }
        if (id == kModMenuRow || id == kCloseTopRow || id == kReportRow) {
            if (id == g_sel) selRow = nb;
            const bool mod = id == kModMenuRow;
            const char* lab = mod ? (id == g_sel ? "> Mod menu" : "  Mod menu") : id == kCloseTopRow ? (id == g_sel ? "> Close" : "  Close") : (id == g_sel ? "> Report" : "  Report");
            put(body, nb, 128, lab, "",
                mod ? "A: top page" : id == kCloseTopRow ? "A: close" : "A: report",
                id == g_sel ? cWhite : cGray, cCyan, id == g_sel, false);
            continue;
        }
        if (id >= kPsRowFirst && id < kPsRowFirst + kPsToggleN) {
            if (id == g_sel) selRow = nb;
            const PsToggle& pt = kPsToggles[id - kPsRowFirst];
            const int v = PsGet(pt.what);
            char lab[96], text[32];
            snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", pt.label);
            snprintf(text, sizeof(text), "%s", v < 0 ? "needs Training" : v ? "ON" : "OFF");
            put(body, nb, 128, lab, "", text, id == g_sel ? cWhite : cGray, v > 0 ? cGreen : cGray, id == g_sel, false);
            continue;
        }
        if (id == kPsAllRowsRow || id == kPsAllKeysRow) {
            if (id == g_sel) selRow = nb;
            const int st = PsMasterState(id == kPsAllKeysRow);
            char lab[96];
            snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", id == kPsAllKeysRow ? "All key indicators" : "All frame rows");
            put(body, nb, 128, lab, "", st < 0 ? "needs Training" : st == 2 ? "MIXED" : st ? "ON" : "OFF", id == g_sel ? cWhite : cGray,
                st == 1 ? cGreen : st == 2 ? cYellow : cGray, id == g_sel, false);
            continue;
        }
        if (id == kTimeOfDayRow || id == kWeatherRow) {
            if (id == g_sel) selRow = nb;
            const int value = PsGet(id == kTimeOfDayRow ? kTimeOfDayWhat : kWeatherWhat);
            char lab[96], text[32];
            snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", id == kTimeOfDayRow ? "Time of day" : "Weather");
            SkyValueText(id, value, text, sizeof(text));
            put(body, nb, 128, lab, "", text, id == g_sel ? cWhite : cGray, value > 0 ? cYellow : cGray, id == g_sel, false);
            continue;
        }
        if (id >= kCamRowFirst && id < kCamRowFirst + kCamRowN) {
            CamReadIni();
            if (id == g_sel) selRow = nb;
            static const char* const kCamLabels[kCamRowN] = {"Camera left / right", "Camera down / up", "Distance (closer / farther)", "Lens (narrower / wider)", "Return to default camera"};
            char lab[96], text[40];
            const int k = id - kCamRowFirst;
            snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", kCamLabels[k]);
            bool on = false;
            if (k != kCamReset) {
                const float value = CamValue(k);
                on = k == kCamDistance ? g_camDist > 0.f : k == kCamLens ? g_camFov > 0.f : value != 0.f;
                if (on) snprintf(text, sizeof(text), k == kCamLens ? "< %+.1f >" : "< %+.0f >", value);
                else snprintf(text, sizeof(text), "< 0 (default) >");
            } else { on = CamAny(); snprintf(text, sizeof(text), "%s", on ? "A: reset" : "default"); }
            put(body, nb, 128, lab, "", text, id == g_sel ? cWhite : cGray, on ? cYellow : cGray, id == g_sel, false);
            continue;
        }
        if (id == kFrameMeterSizeRow) {
            if(id==g_sel)selRow=nb;
            const int v=PsGet(kFrameMeterSizeWhat);
            put(body,nb,128,id==g_sel?"> Ring size":"  Ring size","",
                v<0?"needs newer Training":v==0?"< Small >":v==1?"< Medium >":"< Large >",
                id==g_sel?cWhite:cGray,cGray,id==g_sel,false);
            continue;
        }
        if (id == kFrameMeterStyleRow) {
            if (id == g_sel) selRow = nb;
            const int v = PsGet(kFrameMeterStyleWhat);
            put(body, nb, 128, id == g_sel ? "> Frame meter style" : "  Frame meter style", "",
                v < 0 ? "needs newer Training" : v ? "< Ring >" : "< Row >",
                id == g_sel ? cWhite : cGray, cGray, id == g_sel, false);
            continue;
        }
        if (id == kFrameMeterRow) {
            if (id == g_sel) selRow = nb;
            char text[48];
            const int v = FrameMeterGet();
            if (v < 0) snprintf(text, sizeof(text), "needs Training");
            else snprintf(text, sizeof(text), "%s", v ? "ON" : "OFF");
            put(body, nb, 128, id == g_sel ? "> Frame meter" : "  Frame meter", "", text, id == g_sel ? cWhite : cGray,
                v > 0 ? cGreen : cGray, id == g_sel, false);
            continue;
        }
        if (id == kKiWindowRow) {
            if (id == g_sel) selRow = nb;
            char text[48];
            const int v = KiWindowGet();
            if (v < 0) snprintf(text, sizeof(text), "needs Training");
            else snprintf(text, sizeof(text), "%s", v ? "ON" : "OFF");
            put(body, nb, 128, id == g_sel ? "> Ki window" : "  Ki window", "", text, id == g_sel ? cWhite : cGray,
                v > 0 ? cGreen : cGray, id == g_sel, false);
            continue;
        }
        if (id >= kSettingRow && id < kSettingRow + kConfigN) {
            const int index = id - kSettingRow;
            const auto& d = kConfigSettings[index];
            const auto& v = g_configValues.values[(size_t)index];
            char lab[128], text[128];
            snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", d.label);
            // #13296: value only (ON / OFF pill for switches); how it applies is in the explanation below the list
            snprintf(text, sizeof(text), "%.48s%s", setting_ui::Shown(d, v.text), d.kind == setting_ui::Kind::Manual ? "  (ini only)"
                : v.changed ? (d.apply == setting_ui::Apply::NextLaunch ? "  * restart to apply" : "  * changed") : "");
            if (id == g_sel) selRow = nb;
            put(body, nb, 128, lab, "", text, id == g_sel ? cWhite : cGray,
                d.kind == setting_ui::Kind::Manual ? cDim : v.changed ? cYellow : cGray, id == g_sel, false);
            continue;
        }
        if (id >= kCount) {
            char lab[128];
            snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", id == kBackRow ? "Back" : PageName(id - kPageRow));
            if (id == g_sel) selRow = nb;
            put(body, nb, 128, lab, "", id == kBackRow ? "B" : "A: open", id == g_sel ? cWhite : cGray, cCyan, id == g_sel, false);
            continue;
        }
        if (!g_cfg.layout || g_pages.page == pAll) {
            // Recover the preceding section even when its first row is hidden.
            pending = nullptr;
            for (int p = OrderPos(id); p >= 0; --p)
                if (kOrder[p].section) { pending = kOrder[p].section; break; }
            if (oi && pageRows[oi - 1] < kCount) {
                const char* prev = nullptr;
                for (int p = OrderPos(pageRows[oi - 1]); p >= 0; --p)
                    if (kOrder[p].section) { prev = kOrder[p].section; break; }
                if (prev == pending) pending = nullptr;
            }
        }
        if (pending) {
            put(body, nb, 128, pending, "", "", cCyan, cWhite, false, false);
            pending = nullptr;
        }
        char v[64];
        uint32_t vc;
        Value(id, v, sizeof(v), vc);
        char lab[96];
        const char* plain = kItems[id].plain;
        snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", plain);
        if (id == g_sel) selRow = nb;
        const bool avail = Available(id) || id >= kSave || id == kPerfNote;
        put(body, nb, 128, lab, kItems[id].tech, v, id == g_sel ? cWhite : (avail ? cGray : cDim), vc, id == g_sel, false);
    }
    // explanation of the selected item: what it does, then picture / timing, when it applies and the measured effect
    char what[200], effect[200];
    snprintf(what, sizeof(what), "%s", g_sel >= kCount ? (g_sel == kModMenuRow ? "Jump to the first page of the mod menu." : g_sel == kCloseTopRow ? "Close the mod menu." : g_sel == kReportRow ? "Report a problem: saves a snapshot, opens its folder, and opens the report website." : g_sel == kBackRow ? "Return to the parent menu." : "A / Enter opens this settings group.") : kItems[g_sel].what);
    snprintf(effect, sizeof(effect), "%s", g_sel >= kCount ? (g_sel >= kPsRowFirst && g_sel < kPsRowFirst + kPsToggleN ? "Training saves it. A / Left / Right flips it." : "Settings keep their current values when you change pages.") : kItems[g_sel].effect);
    if (g_sel == kRematchRow) {
        snprintf(what, sizeof(what), "Boss Matches: replay cleared bosses in their arena or from their shrine. Death and loot rules are shared with Training.");
        snprintf(effect, sizeof(effect), "Arena matches use your death and loot rules. Mission replays keep normal rewards. Solo and cleared-boss checks still apply.");
    } else if (g_sel == kPracticeRow) {
        snprintf(what, sizeof(what), "Training: choose a location, add bosses, and control their moves. Shared player and boss stats also affect spawned Boss Matches.");
        snprintf(effect, sizeof(effect), "Training retries in place when you die. Return to where I was ends the session. Frame meter: Display Overlays. Save manager: Mod menu at title.");
    } else if(g_sel>=kSaveFilesRow&&g_sel<kSaveNamedFirst+savefiles::kMaxSlots){
        snprintf(what,sizeof what,"%s",savefiles::Pending()?"Cancel stops the pending save operation.":!savefiles::Allowed()?"Return to the title screen to manage saves; no game restart needed.":savefiles::Note());
        snprintf(effect,sizeof effect,"%s",g_saveConfirm&&Now()-g_saveConfirmAt<5000?"Press again to confirm. Then close this menu and Quit the game normally.":"Live import refreshes the title now. Switch after Quit waits for normal exit.");
    } else if (g_sel == kUpdatePageRow) {
        snprintf(what, sizeof(what), "Open website and show Nioh3Updater.exe in the game folder.");
        snprintf(effect, sizeof(effect), "Or use the Nioh3 Mod Website shortcut in the game folder.");
    } else if (g_sel == kReverseModsRow) {
        snprintf(what, sizeof(what), "%s", g_reverseNote);
        snprintf(effect, sizeof(effect), "Same as 'Restore Vanilla Game'. Mod stays on until you quit.");
    } else if (g_sel == kReportRow) {
        snprintf(what, sizeof(what), "%s", g_reportNote);
        snprintf(effect, sizeof(effect), "Opens the report folder and the website. File: Nioh3EngineFix-report-latest.txt beside the mod (system, settings, bosses, log).");
    } else if (g_sel >= kPsRowFirst && g_sel < kPsRowFirst + kPsToggleN) {
        snprintf(what, sizeof(what), "%s", g_psNote[0] ? g_psNote : kPsToggles[g_sel - kPsRowFirst].note);
        snprintf(effect, sizeof(effect), "Frame meter switch; needs Nioh3Training. Training saves it.");
    } else if (g_sel == kPsAllRowsRow || g_sel == kPsAllKeysRow) {
        snprintf(what, sizeof(what), "%s", g_psNote[0] ? g_psNote : g_sel == kPsAllKeysRow ? "Turns the KEY under the meter and all of its entries ON or OFF together. The Key rows below switch single entries."
            : "Turns the frame-meter rows (boss, player, i-frames) ON or OFF together. The rows below keep only some.");
        snprintf(effect, sizeof(effect), "A flips; Left = all OFF, Right = all ON. Needs Nioh3Training. Training saves it.");
    } else if (g_sel == kFrameMeterSizeRow) {
        snprintf(what,sizeof what,"Size of the circular frame meter: Small, Medium or Large.");
        snprintf(effect,sizeof effect,"Small is the compact default. Row layout is unchanged. Saved in Training.");
    } else if (g_sel == kFrameMeterStyleRow) {
        snprintf(what, sizeof(what), "Row bars or separate PLAYER and BOSS rings. Same lanes, colours, Ki marks and legend.");
        snprintf(effect, sizeof(effect), "Left: Row. Right: Ring. A / Enter switches. Saved in Training. %s", g_psNote);
    } else if (g_sel == kFrameMeterRow) {
        snprintf(what, sizeof(what), "%s", g_frameMeterNote);
        snprintf(effect, sizeof(effect), "Frame meter settings live only here (Display Overlays). Training saves it.");
    } else if (g_sel == kTimeOfDayRow || g_sel == kWeatherRow) {
        snprintf(what, sizeof(what), "%s", g_skyNote[0] ? g_skyNote : g_sel == kTimeOfDayRow
            ? "Set the sky time in 30-minute steps. Stock follows the area's own time."
            : "Choose the sky weather. Stock follows the area's own weather; indoor changes may be subtle.");
        snprintf(effect, sizeof(effect), "Left / Right changes; A / Enter restores Stock. Applies during gameplay, outside loads and cutscenes. Saved.");
    } else if (g_sel >= kCamRowFirst && g_sel < kCamRowFirst + kCamRowN) {
        snprintf(what, sizeof(what), "Left / Right adjusts the selected axis; A / Enter resets it. Zero uses the game camera. Return to default resets all four.");
        snprintf(effect, sizeof(effect), "%s", g_camNote[0] ? g_camNote : "Steps: position 20, distance 60, lens 5 degrees. Left = closer / narrower; right = farther / wider.");
    } else if (g_sel == kKiWindowRow) {
        snprintf(what, sizeof(what), "%s", g_kiWindowNote);
        snprintf(effect, sizeof(effect), "Off by default: only the perfect Ki Pulse frames (teal) show. On adds the rest of the window in dark blue. Training saves it.");
    } else if (!g_cfg.layout && g_sel >= kPageRow && g_sel < kBackRow) {
        const int pg = g_sel - kPageRow;
        const char* w = pg == pDisplay ? "Display: overlays, FPS, time of day, weather and loading-screen auto-click."
            : pg == pCamera ? "Camera settings: lens adjustment, return to the native camera, and motion blur / depth of field / bloom options."
            : pg == pSettings ? "Optimizations: a preset at the top, then CPU, GPU, Game and Frame Gen."
            : pg == pOvController ? "Controller overlay: on/off, position, size, opacity, pressed colour, button style."
            : pg == pOvFrameMeter ? "Frame meter: the meter, Ki window, player and i-frame lanes, readout and every colour-key group."
            : pg == pOvInput ? "Input display: the vertical input history, its side and row count."
            : pg == pOvHud ? "Game HUD: hide the whole gameplay HUD with the master row, or hide health / Ki, style crest, stance, shortcuts, guardian, amrita, money, boss bar, minimap, enemy health bars and lock-on one by one."
            : pg == pOvFps ? "FPS counter, frame time, game speed, position and size, timing diagnostics."
            : pg == pSCpu ? "CPU fixes and experiments, thread scheduling." : pg == pSGpu ? "GPU fixes, experiments and the look-changing Extra performance rows."
            : pg == pSGame ? "Frame pacing, latency, job-thread limits and other next-launch game settings." : pg == pFrameGen ? "Frame-gen presenter fixes." : "";
        snprintf(what, sizeof(what), "%s", w);
        snprintf(effect, sizeof(effect), "A / Enter opens this page.");
    } else if (g_sel >= kSettingRow && g_sel < kSettingRow + kConfigN) {
        const int index = g_sel - kSettingRow;
        const auto& d = kConfigSettings[index];
        snprintf(what, sizeof(what), "[%s] %s: %s", d.section, d.key, d.help);
        snprintf(effect, sizeof(effect), "%s", d.kind == setting_ui::Kind::Manual ? "Shown for coverage. No controller editor; this row cannot change it."
            : index == g_configReplyRow ? g_configReply : d.apply == setting_ui::Apply::NextLaunch ? "Startup preference only. A / Left / Right edits; Save to ini, then restart."
            : "Requested value; applies through the existing setter if installed. No enablement until you edit it.");
    }
    if (g_sel < kCount && !Available(g_sel)) {
        if ((g_sel == kL1 && g_host.l1 == FixState::WineSkipped) || (g_sel == kF1 && g_host.f1 == FixState::WineSkipped))
            snprintf(what, sizeof(what), "Skipped under Proton - turn on '%s on Proton' below, save, restart.", g_sel == kL1 ? "L1" : "F1");
        else if (g_sel == kFg)
            snprintf(what, sizeof(what), "Status only: change frame generation in the game's Options.");
        else
            snprintf(what, sizeof(what), "Not installed this session (off in the ini or a site was not found - see the log).");
        effect[0] = 0;
    }
    addT("", cWhite, true);
    if(g_pages.page==pSaveFiles)addT("Choose save data  ([x] = in use)",cCyan);
    if(g_pages.page==pSaveActions&&!g_saveName.empty())addT(g_saveName.c_str(),cCyan);
    {char st[96];const int pr=savefiles::Progress(st,sizeof st);if(pr>=0){char bar[48];int f=pr*20/100;for(int i=0;i<20;++i)bar[i]=i<f?'#':'-';bar[20]=0;char ln[128];snprintf(ln,sizeof ln,"Switching save: [%s] %d%%",bar,pr);addT(ln,cGreen);addT(st,cCyan);}}
    if(savefiles::Pending()){addT(savefiles::Note(),cCyan);addT(savefiles::LivePending()?"Applies live: the title reloads on its own, no quit needed.":savefiles::Native()?"Runs now at the title; no quit needed.":"Applies after you quit the game.",cWhite);}
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
        if (ss == 2) snprintf(b, sizeof(b), "Saved to Nioh3EngineFix.ini");
        else snprintf(b, sizeof(b), "Save FAILED (error %ld) - see the log", (long)g_saveErr);
        addT(b, ss == 2 ? cGreen : cOrange);
    } else {
        addT(g_cfg.layout && g_pages.depth ? "\x01P" : "\x01p", cDim);  // #13537: drawn as button badges (drawRow)
    }
    addT("\x01T", cDim);  // #44232: LB / RB tabs + View first page, on every page
    addT("\x01K", cDim);

    // #38102/#38134: every flat page is the same fixed right-side panel as the hosted pages (58 columns, 0.66 H high); long rows are clipped
    constexpr size_t cols = 58, valueCol = 34;
    auto clip = [&](Row& rw, bool isHead) {
        if (rw.label[0] == '') return;
        auto cut = [](char* sx, size_t lim) { if (strlen(sx) > lim) { if (lim >= 3) memcpy(sx + lim - 2, "..", 3); else sx[lim] = 0; } };
        const size_t tl = rw.tech[0] ? 1 + strlen(rw.tech) : 0;
        if (!isHead && rw.value[0]) {
            cut(rw.value, cols - valueCol);
            cut(rw.label, valueCol > tl + 4 ? valueCol - 2 - tl : 4);
        } else cut(rw.label, cols - tl);
    };
    for (int i = 0; i < nb; ++i) clip(body[i], false);
    for (int i = 0; i < nt; ++i) clip(tail[i], false);
    const float sc = PanelScale(W, H);
    const int totalRows = std::max(8, (int)std::floor((0.66f * (float)H - 12 * sc) / ((osd::kGlyphH + 2) * sc)));
    const int avail = std::max(3, totalRows - 2 - nt);  // head + the #44353 tab bar row
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
    const int n = totalRows;
    const float lineH = (osd::kGlyphH + 2) * sc, pad = 6 * sc, cw = osd::kGlyphW * sc;
    const float w = (float)cols * cw + 2 * pad + cw, h = n * lineH + 2 * pad;
    const float x0 = std::floor((float)W - w - 12.0f), y0 = 12.0f;
    // #13257 look: shared menu_style panel (gold top line, outline), title bar, selection bar with stripe, gold section headers
    // with a rule, ON / OFF pills, dimmed footer band. Layout (columns, rows, scale) is unchanged.
    namespace ms = menu_style;
    const float fa = ms::OpenFade(t);
    ms::Panel(x0, y0, w, h, sc, ms::kPanelBg, fa);
    ms::PanelBand(x0, y0, w, h, sc, 2 * sc, pad + lineH - 2 * sc, sc, ms::Fade(ms::kTitleBg, fa));
    int line = 0;
    auto drawRow = [&](const Row& rw, bool isHead) {
        const float y = y0 + pad + line * lineH;
        if (rw.label[0] == '\x01') {  // #13537 control prompts: pad (P = back, p = close) or keyboard (K), badges + labels
            const float right = x0 + w - pad;
            if (rw.label[1] == 'K') {
                static const ms::HintItem kKeyPrompts[] = {{"Arrows", "Move"}, {"Enter", "Select"}, {"Esc", "Back"}, {"PgUp PgDn", "Page"}, {"F10", "Exit"}};
                const float kx = osd::DrawText(x0 + pad, y, "Keys", ms::Fade(cDim, fa), sc) + cw;
                ms::HintRow(kx, y, right, kKeyPrompts, 5, cw, lineH, sc, fa);
            } else if (rw.label[1] == 'T') {  // #44232: shoulder caps follow ButtonStyle (LB RB / L1 R1)
                const ms::HintItem kTabs[] = {{"LB RB", "Switch tab"}, {ms::PadWord("View", "Create"), "First page"}};
                ms::HintRow(x0 + pad, y, right, kTabs, 2, cw, lineH, sc, fa);
            } else {
                const ms::HintItem kPad[] = {{"A", "Select"}, {"B", rw.label[1] == 'P' ? "Back" : "Close"}, {"Y", "Close"}, {"UD", "Move"}, {"LR", "Change"}};
                ms::HintRow(x0 + pad, y, right, kPad, 5, cw, lineH, sc, fa);
            }
            ++line;
            return;
        }
        if (rw.sel) ms::SelectBar(x0 + sc, y - sc, w - 2 * sc, lineH, sc, fa);
        if (rw.rule && line > 0) ms::Divider(x0 + pad, y + lineH / 2, w - 2 * pad, sc, fa);
        const bool section = !isHead && !rw.value[0] && !rw.tech[0] && rw.lc == cCyan && rw.label[0] && rw.label[0] != ' ' && rw.label[0] != '>';
        const bool marked = !isHead && (rw.label[0] == '>' || rw.label[0] == ' ') && rw.label[1] == ' ';  // "> " / "  " row marker
        const char* label = marked ? rw.label + 2 : rw.label;
        float lx = x0 + pad + (marked ? 2 * cw : 0);
        if (label[0]) osd::DrawText(lx, y, label, ms::Fade(isHead ? ms::kTitleText : section ? ms::kAccent : rw.lc, fa), sc);
        lx += (float)strlen(label) * cw;
        if (section) ms::Divider(lx + cw, y + lineH / 2 - sc, x0 + w - pad - lx - cw, sc, fa);
        if (rw.tech[0]) osd::DrawText(lx + cw, y, rw.tech, ms::Fade(rw.sel ? cGray : cDim, fa), sc);
        if (rw.value[0]) {
            const size_t lc = strlen(rw.label) + (rw.tech[0] ? 1 + strlen(rw.tech) : 0);
            const float vx = isHead ? x0 + w - pad - (float)strlen(rw.value) * cw : x0 + pad + (float)std::max(lc + 2, valueCol) * cw;
            uint32_t bg, fg;
            char tok[8] = {};
            const size_t tl = strcspn(rw.value, " ");
            if (tl < sizeof tok) memcpy(tok, rw.value, tl);
            const char* pt = ms::PillText(tok);  // human #44361: ON / OFF drawn as [ON] / [OFF]
            const uint32_t vcol = rw.vc == cGreen ? cGray : rw.vc;  // human #44361: no green text for ON values (the [ON] pill says it)
            if (!isHead && tok[0] && ms::PillColors(pt, bg, fg)) {  // "ON", "OFF  * restart", "ON  (ini)": pill + the rest as text
                ms::Pill(vx, y, pt, bg, rw.lc == cDim ? cDim : fg, cw, lineH, sc, fa);
                if (rw.value[tl]) osd::DrawText(vx + (float)(strlen(pt) + 1) * cw, y, rw.value + tl + 1, ms::Fade(vcol, fa), sc);
            } else if (!isHead && rw.value[0] && strchr("ABXY", rw.value[0]) &&
                       (!rw.value[1] || (rw.value[1] == ':' && rw.value[2] == ' '))) {
                // Use the existing active-controller badge, including PlayStation symbols.
                char key[2] = {rw.value[0], 0};
                ms::Hint(vx, y, key, rw.value[1] ? rw.value + 3 : "", cw, lineH, sc, fa);
            } else osd::DrawText(vx, y, rw.value, ms::Fade(vcol, fa), sc);
        }
        ++line;
    };
    if (nt >= 3) {  // the three control lines at the bottom (pad, tabs, keys) sit in a darker footer band
        const float fy = y0 + pad + (float)(n - 3) * lineH - 2 * sc;
        ms::PanelBand(x0, y0, w, h, sc, fy - y0, y0 + h - fy - sc, sc, ms::Fade(ms::kFooterBg, fa));
    }
    drawRow(head, true);
    DrawTabBar(x0 + pad, y0 + pad + (float)line * lineH, (int)cols, cw, lineH, sc, fa);  // #44353: tabs under the title, every page
    ++line;
    for (int i = 0; i < shown; ++i) drawRow(body[first + i], false);
    line = n - nt;
    for (int i = 0; i < nt; ++i) drawRow(tail[i], false);
    return sc;
}

void Fail(const char* where, DWORD code) {
    InterlockedExchange(&g_disabled, 1);
    InterlockedExchange(&g_open, 0);
    SyncFreeze();
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

// #38102/#38167: one scale for the right-side panel (every page) and the FPS overlay on the left, so both look the same size
float PanelScale(unsigned W, unsigned H) {
    return std::max(0.5f, std::min(AutoScale(H) * 0.75f, ((float)W * 0.46f - 16.f) / ((float)(58 + 3) * osd::kGlyphW)));
}

// ---- public ------------------------------------------------------------------------------------------------------------------
void Configure(const Host& h) {
    g_host = h;
    g_log = h.log;
    g_ini = h.iniPath ? h.iniPath : L"";
    savefiles::Configure(h.base,h.knownBuild,g_ini.c_str(),h.log);
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
    g_configValues.Load(kConfigSettings, f);
    g_cfg.enabled = (int)GetPrivateProfileIntW(L"Menu", L"Enabled", 1, f);
    if (g_cfg.enabled) StartRematchHostResolver();
    g_cfg.layout = GetPrivateProfileIntW(L"Menu", L"Layout", 0, f) != 0;  // 0 = Settings holds the two pages; 1 = older grouped folders
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
    GetPrivateProfileStringW(L"Menu", L"ButtonStyle", L"Xbox", b, 96, f);
    menu_style::g_btnStyle = !_wcsicmp(b, L"PlayStation") ? 1 : 0;
    padviz::g_on = GetPrivateProfileIntW(L"Menu", L"ControllerDisplay", 0, f) ? 1 : 0;
    GetPrivateProfileStringW(L"Menu", L"ControllerDisplaySize", L"1.0", b, 96, f);
    padviz::g_size100 = std::clamp((int)std::lround(_wtof(b) * 100.0), 50, 400);
    padviz::g_posX = std::clamp((int)GetPrivateProfileIntW(L"Menu", L"ControllerDisplayX", 0, f), -100, 100);
    padviz::g_posY = std::clamp((int)GetPrivateProfileIntW(L"Menu", L"ControllerDisplayY", 0, f), -100, 100);
    GetPrivateProfileStringW(L"Menu", L"ControllerDisplayPressColor", L"White", b, 96, f);
    { char nb[96]; WideCharToMultiByte(CP_UTF8, 0, b, -1, nb, sizeof(nb), nullptr, nullptr); padviz::g_pressRGB = padviz::PressColor(nb); }
    inputhist::g_on = GetPrivateProfileIntW(L"Menu", L"InputDisplay", 0, f) ? 1 : 0;
    hudhide::g_want = GetPrivateProfileIntW(L"Menu", L"HideGameHud", 0, f) ? 1 : 0;
    for (int i = 0; i < hudhide::kElemN; ++i) {  // #44877 per-element HUD hide
        wchar_t wk[64]; MultiByteToWideChar(CP_UTF8, 0, hudhide::kElems[i].key, -1, wk, 64);
        hudhide::g_elemOn[i] = GetPrivateProfileIntW(L"Menu", wk, 0, f) ? 1 : 0;
    }
    GetPrivateProfileStringW(L"Menu", L"HideHudIds", L"", b, 96, f);
    { char nb[96]; WideCharToMultiByte(CP_UTF8, 0, b, -1, nb, sizeof(nb), nullptr, nullptr); hudhide::ParseIds(nb); }
    GetPrivateProfileStringW(L"Menu", L"InputDisplaySide", L"Left", b, 96, f);
    inputhist::g_side = !_wcsicmp(b, L"Right") ? 1 : 0;
    inputhist::g_rows = std::clamp((int)GetPrivateProfileIntW(L"Menu", L"InputDisplayRows", 20, f), inputhist::kMinRows, inputhist::kMaxRows);
    GetPrivateProfileStringW(L"Menu", L"ControllerDisplayOpacity", L"0.45", b, 96, f);
    padviz::g_opacity100 = std::clamp((int)std::lround(_wtof(b) * 100.0), 20, 100);
    GetPrivateProfileStringW(L"Menu", L"Scale", L"0", b, 96, f);
    g_cfg.scale = (float)std::clamp(_wtof(b), 0.0, 8.0);
    if (g_cfg.scale > 0 && g_cfg.scale < 0.5f) g_cfg.scale = 0.5f;
    g_cfg.pauseHint = (int)GetPrivateProfileIntW(L"Menu", L"PauseHint", 2, f);
    g_cfg.freezeGame = GetPrivateProfileIntW(L"Menu", L"FreezeGame", 1, f) != 0;
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
    MLOG("frame generation: status only; change it in the game's Options");
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
    SyncFreeze();  // after every path that may have closed the menu (Close, hooks, cover, add-on request)
}

float DrawFolderNotice(unsigned W, unsigned H, float scale, bool focused) {
    char notice[2048]{};
    static bool shown=false;
    if (!updatecheck::CopyFolderNotice(notice,sizeof(notice),focused)) { shown=false; return scale; }
    if (!shown) { MLOG("folder notice visible (20 seconds): %s",notice); shown=true; }
    const float sc=scale>0 ? scale : 1.0f;
    const float pad=4*sc, lineH=(osd::kGlyphH+2)*sc;
    const size_t cols=std::max<size_t>(1,static_cast<size_t>(std::max(1.0f,((float)W-32-2*pad)/(osd::kGlyphW*sc))));
    const size_t len=strlen(notice), lines=(len+cols-1)/cols;
    osd::DrawRect(8,8,(float)W-16,(float)lines*lineH+2*pad,0xF0202020);
    for(size_t i=0;i<lines;++i) {
        char row[2048]{}; const size_t n=std::min(cols,len-i*cols);
        memcpy(row,notice+i*cols,n);
        osd::DrawText(8+pad,8+pad+(float)i*lineH,row,cGold,sc);
    }
    (void)H;
    return sc;
}

float Draw(unsigned W, unsigned H) {
    if (!Enabled()) return 0;
    __try {
        const float scale=DrawImpl(W, H);
        return DrawFolderNotice(W,H,scale,ForegroundIsGame());
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        Fail("Draw", GetExceptionCode());
        return 0;
    }
}

cover_present::Identity CoverDrawn(uint64_t swapchain,uint64_t generation) {
    return {g_coverDrawnToken,g_coverSession.load(),swapchain,generation};
}
void CoverPresented(const cover_present::Ticket& ticket,uint64_t generation,int32_t result) {
    cover_present::Identity current{g_coverToken.load(),g_coverSession.load(),ticket.id.swapchain,generation};
    if(!cover_present::Accept(ticket,current,result,g_coverCancel.load()||!g_coverEnabled.load()))return;
    const uint64_t now=GetTickCount64();
    if(!CoverCaptures(now))return; // a blocked Present cannot revive an expired worker/input lease
    g_coverPresented=now;
    PublishCoverTip(now);
    const auto present=g_rematchApi.PresentCover;
    if(present)present(ticket.id.session,ticket.id.token); // try-only; no native or worker UI call
}
bool CoverTipAllowed() {
    if(!Enabled() || !TryAcquireSRWLockShared(&g_coverTipLock))return false;
    const auto lease=g_coverTipLease;
    ReleaseSRWLockShared(&g_coverTipLock);
    const uint64_t now=GetTickCount64();
    return cover_present::Captures(lease,now) && CoverCaptures(now) && g_coverEnabled.load() && !g_coverCancel.load() &&
        lease.token==g_coverToken.load() && lease.session==g_coverSession.load() &&
        lease.session==g_rematchSession.load();
}
bool IsOpen() { return g_open != 0 && !g_disabled; }
int GamePauseReason() { return (int)g_pauseReason; }
void ReleaseFreeze() {  // DLL unload: give the stop request back whatever state the menu is in
    InterlockedExchange(&g_open, 0);
    for (int i = 0; i < 100 && InterlockedCompareExchange(&g_freezeBusy, 1, 0) != 0; ++i) Sleep(1);
    SyncFreezeLocked();
    InterlockedExchange(&g_freezeBusy, 0);
}

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
    if(g_coverToken.load()) {
        if(vk==VK_ESCAPE && (msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN)){
            if(!g_coverCancel.load())CancelCoverInput();
            return Now()<g_coverCancelUntil.load();
        }
        return CoverCaptures(Now());
    }

    if (g_cfg.vk && vk == g_cfg.vk) return true;
    if (g_open || g_keyLatch)
        for (const NavKey& k : kNavKeys)
            if (k.vk == vk) return true;
    return false;
}

}  // namespace menu

// Nioh3EngineFix_LoadScreen(on, title, stage): full-screen loading screen (black, title, moving bar, stage line) drawn by the overlay until
// called with on = 0. title/stage may be null (kept). Any thread. Returns 1 when the menu overlay is enabled (it draws it), else 0.
extern "C" __declspec(dllexport) int Nioh3EngineFix_MaskGameButtons(int mask, int ms) {
    InterlockedExchange64(&menu::g_gameMaskUntil, (LONG64)(GetTickCount64() + (ms > 0 ? (ULONGLONG)ms : 0)));
    InterlockedExchange(&menu::g_gameMask, ms > 0 ? mask : 0);
    return menu::Enabled() ? 1 : 0;
}
// #9664/#9670 Nioh3EngineFix_MenuOpen(): 1 while the mod menu (host of the Boss Rematch pages) is open. Nioh3Training reads it so a pad A press
// near the Return gate opens the gate page only when no menu is up (the A that picks a menu row must not reopen the page).
extern "C" __declspec(dllexport) int Nioh3EngineFix_MenuOpen() { return menu::g_open ? 1 : 0; }
// #13796 safety net: Nioh3Training asks to close the mod menu when a native modal (the game's own Yes/No box) opened at the Return object,
// so our menu never masks the pad over a box the player must answer. Any thread; the close itself runs on the overlay's Present.
extern "C" __declspec(dllexport) int Nioh3EngineFix_MenuClose(const char* why) {
    if (why) { char b[96]; snprintf(b, sizeof b, "%s", why); if (menu::g_log) menu::g_log("MENU: close requested by an add-on: %s", b); }
    InterlockedExchange(&menu::g_extCloseReq, 1); return menu::g_open ? 1 : 0;
}
extern "C" __declspec(dllexport) int Nioh3EngineFix_LoadScreen(int on, const char* title, const char* stage) {
    AcquireSRWLockExclusive(&menu::g_lsLock);
    if (title) snprintf(menu::g_lsTitle, sizeof(menu::g_lsTitle), "%s", title);
    if (stage) snprintf(menu::g_lsStage, sizeof(menu::g_lsStage), "%s", stage);
    ReleaseSRWLockExclusive(&menu::g_lsLock);
    InterlockedExchange(&menu::g_lsOn, on ? 1 : 0);
    return menu::Enabled() ? 1 : 0;
}
extern "C" __declspec(dllexport) uint32_t Nioh3EngineMenuHostVersion() {
    const uint64_t tick = menu::g_rematchRendererTick.load();
    return menu::Enabled() && tick && GetTickCount64() - tick < 1500 ? rematch_menu_bridge::kVersion : 0u;
}
