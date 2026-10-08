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
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

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
    int pauseHint = 1;
    uint8_t pauseMask = 0x18;  // bit i = G+0x588+i; default 58B + 58C (the two flags that stop game time)
    int fgToggle = 1;          // [Menu] FrameGenToggle: 0 status only, 1 switch on Windows, 2 also under Proton (risky)
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
    if (own) return;
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

HRESULT DiHooked(GetDeviceState_t orig, void* self, DWORD cb, void* data) {
    ++t_diDepth;  // Wine's A method calls the W one: only the outermost call looks at the data
    const HRESULT hr = orig(self, cb, data);
    --t_diDepth;
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
    static const char kText[] = "FG switched on by the mod under Proton; config.xml had FRAME_GENERATION OFF at startup.\r\n"
                                "Nioh3EngineFix puts OFF back at exit, or at the next start if this file is still here.\r\n";
    DWORD wr = 0;
    const bool ok = h != INVALID_HANDLE_VALUE && WriteFile(h, kText, (DWORD)sizeof(kText) - 1, &wr, nullptr) && FlushFileBuffers(h);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    MLOG("Proton: FG switched on by the mod - marker %ls %s (config.xml gets FRAME_GENERATION OFF back at exit / next start)",
         g_fgMarker.c_str(), ok ? "written" : "COULD NOT be written");
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
          kXWG, kXTG, kXDS, kXLK, kXCP, kXSPF, kXMQ, kXPT1, kXDL,
          kGpu, kGR2a, kGR2b, kGR1,
          kFps, kFrameTime, kSpeed, kPos, kScale, kFg, kFgState, kUpscaler,
          kWorkerCap, kP12, kL1Wine, kFgWine, kSave, kClose, kCount };
static_assert(kXDL - kXW6 + 1 == kNumExp, "experimental items must match menu::ExpId");
bool IsExp(int id) { return id >= kXW6 && id <= kXDL; }
const wchar_t* const kExpIniKey[kNumExp] = {L"W6", L"B", L"PP", L"FPC", L"AABB", L"M1", L"X1", L"E1", L"M2", L"J1", L"S1", L"IK", L"TS", L"WakeStats", L"ED", L"GF", L"WL",
                                             L"WG", L"TG", L"DS", L"LK", L"CP", L"SPF", L"MQ", L"PT1", L"DL"};
struct ItemDef {
    const char* section;  // header printed before the item (nullptr = none)
    const char* label;
    const char* desc;
};
const ItemDef kItems[kCount] = {
    {"LIVE FIXES - apply at once", "VRR timing", "Real game speed below the fps cap. Offline only: OFF for online co-op."},
    {nullptr, "W3 message pump wait", "Message thread sleeps (<=1 ms) instead of spinning a CPU core."},
    {nullptr, "W3 wait mode (Proton)", "Sleep 1 ms: few wake-ups. MsgWait: Windows wait (~24k wakes/s on Wine)."},
    {nullptr, "R1 render wait", "Game thread waits for the GPU with this timeout per round. 0 = stock busy poll."},
    {nullptr, "L1 frame limiter sleep", "Frame limiter sleeps on a high-resolution timer instead of spinning."},
    {nullptr, "F1 FSR-FG presenter", "FSR frame-gen presenter sleeps instead of spinning (FG on only)."},
    {nullptr, "WK job wake gate", "Skips waking a job worker when no queue holds work (less CPU power)."},
    {nullptr, "Max frame latency", "Frames queued ahead (game: 1). Proton: lowering applies after swapchain rebuild / restart."},
    {"EXPERIMENTAL CPU POWER - A/B live", "W6 present wait", "Main thread sleeps while Present blocks, instead of Sleep(0)-spinning a core."},
    {nullptr, "B gallop search", "Draw-list group length: galloping instead of full binary search (main thread)."},
    {nullptr, "PP page-pool sweep /N", "Streaming page-pool usage sweep every N-th frame (Tuning PP_Period, default 4)."},
    {nullptr, "FPC FindParam cache", "Per-frame cache of material parameter lookups on the render thread."},
    {nullptr, "AABB box math (SSE)", "Character bounding boxes in SSE, bit-identical (game-update thread)."},
    {nullptr, "M1 meshlet pool sum", "Meshlet-request workers stop re-summing the page-pool capacity per chunk."},
    {nullptr, "X1 empty pad slots", "Active-pad scan skips XInput slots the game knows are empty."},
    {nullptr, "E1 hidden effects /N", "Effects nobody can see update every N-th pass with N x the time step (E1_N=2)."},
    {nullptr, "M2 meshlet atomics", "Meshlet workers read bitmaps with plain loads (code bytes: next launch)."},
    {nullptr, "J1 job-queue polls", "Idle job workers poll queues with plain loads (code bytes: next launch)."},
    {nullptr, "S1 drain-spin pause", "PAUSE in the main thread's end-of-frame spin (code bytes: next launch)."},
    {"INPUT LATENCY", "IK input kick early", "Pad read at the start of the logic frame, not the end (-4..9 ms). Toggle while idle."},
    {nullptr, "TS input -> display", "Timestamp probes: median button-sample -> frame presented (add half a refresh)."},
    {"WORKER WAKES", "WakeStats counters", "Counts job-worker wakes and sleeps (log \"stats wakes/s\" every 30 s). Measurement only."},
    {nullptr, "ED skip empty effect lists", "Effect Disp workers with nothing to update are not woken (same output)."},
    {"GPU FENCE WAITS", "GF fence wait sleeps", "Main thread / workers sleep until the GPU fence fires instead of Sleep(0)-spinning."},
    {nullptr, "WL close waitable handles", "Closes the per-frame frame-latency handle the game leaks (Windows; no-op on Proton)."},
    {"1.6.0 - OPT-IN", "WG wake gate", "No second wake-one while a woken worker of the pool is still on its way (same jobs run)."},
    {nullptr, "TG tick governor (exp.)", "EXPERIMENT: logic ticks follow rendered fps (floor 30). Step 1/30 s like 30-FPS mode. Needs L1."},
    {nullptr, "DS retry-spin sleep", "Main loop's 1 ms no-new-frame spin sleeps on a timer (exit test unchanged). Needs L1."},
    {nullptr, "LK plain loads (18)", "Locked 'atomic loads' in worker code become plain loads (code bytes: next launch)."},
    {nullptr, "CP collision prefetch", "Collision workers prefetch the next colliders (same results, less memory stall)."},
    {nullptr, "SPF stage prefetch", "Stage-update worker prefetches placeables 4 ahead (same results, less memory stall)."},
    {nullptr, "MQ bone matrix SSE", "Skeleton matrix build in SSE, bit-identical (about 1 % of each scheduler thread)."},
    {nullptr, "PT1 texture pool lock", "Texture tile requests take the pool lock once per texture (same state, fewer lock pairs)."},
    {nullptr, "DL skip dead lookup", "Collision update skips a part-map search whose result is discarded (CP site group)."},
    {"GPU", "GPU barrier batching", "Batches the game's D3D12 barriers into fewer calls. ~0 on the Ally, 2-3 % on PC."},
    {nullptr, "R2a COMMON barriers", "Proton: buffer COMMON->X barriers get their real read state (cheaper wait on RADV). A/B with GPU W/fps."},
    {nullptr, "R2b COMMON barriers (UAV)", "EXPERIMENT. Same for UAV buffers. May show flicker or stale data: judge with screenshots."},
    {nullptr, "R1 read->read barriers", "EXPERIMENT. Drops read->read superset barriers. Needs an image compare."},
    {"OVERLAY", "FPS counter", "Real FPS (engine) and FG FPS (presented, incl. generated frames)."},
    {nullptr, "Frame time", "Adds the real frame time (ms) to the counter."},
    {nullptr, "Speed factor", "Adds the game speed: 1.00x = real time, below 1 = the game runs slow."},
    {nullptr, "Counter position", "Corner of the FPS counter."},
    {nullptr, "Counter text size", "1.0 = 8x16 px letters (x2 at 4K). Whole numbers look sharpest."},
    {"GAME SETTING - live, the game's own apply", "Frame generation",
     "Same as the game's Options > Frame Generation. Screen blinks while it switches."},
    {nullptr, "  FG running", "Real FPS (engine) vs presented FPS: FG runs when presented > 1.4x real."},
    {nullptr, "  Upscaler (game setting)", "The game's upscaler setting. FSR frame gen does not need the FSR upscaler."},
    {"NEXT LAUNCH - saved to the ini", "WorkerCap", "Handhelds: cap the job thread pools at N (try 10, then 8). Off on desktops."},
    {nullptr, "P12 worker-pool lift", "Keeps 3 more pool threads on CPUs with <= 12 threads. Experimental."},
    {nullptr, "L1 on Proton", "Install L1 under Wine/Proton too (Wine may not honour the timer)."},
    {nullptr, "F1 on Proton", "Install F1 under Wine/Proton too (same timer caveat as L1)."},
    {"", "Save to ini", "Save to Nioh3EngineFix.ini; FG changed here also goes to the game's config.xml."},
    {nullptr, "Close menu", "Also: B / Esc / the menu key / the open gesture."},
};

int g_sel = 0;
bool g_dirty = false;
int g_bootWorkerCap = 0, g_bootP12 = 0, g_bootL1Wine = 0, g_bootFgWine = 0;
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
        case kFg: return g_fgOk;
        case kGpu:
        case kGR2a: case kGR2b: case kGR1:
            return gpuopt::GetState() == gpuopt::State::Active || gpuopt::GetState() == gpuopt::State::Paused;
        default: return true;
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
    snprintf(buf, n, "%s%s", g_expIni[e] ? "ON" : "OFF", changed ? "  * restart" : (x.applied ? "" : "  (ini)"));
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
    if (id == kFgState) {
        if (s.active) {
            snprintf(buf, n, "yes %.0f -> %.0f fps", s.realFps, s.presentFps);
            color = cCyan;
        } else if (s.setting == 0) {
            snprintf(buf, n, "no (off in game settings)");
            color = cGray;
        } else if (!s.measured || (s.sinceSwitch < 12.0 && s.setting > 0)) {
            snprintf(buf, n, s.measured ? "switching..." : "measuring...");
            color = cWhite;
        } else {
            snprintf(buf, n, "NO");
            color = cYellow;
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
    } else if (s.setting == 0) {
        snprintf(buf, n, "OFF%s", ro);
        color = cGray;
    } else {
        snprintf(buf, n, "%s%s", name, ro);
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
        case kFrameTime: onoff(o.frameTime != 0); break;
        case kSpeed: onoff(o.speed != 0); break;
        case kPos: snprintf(buf, n, "%s", osd::PositionName(o.pos)); break;
        case kScale: snprintf(buf, n, "%.1f", o.scale); break;
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
        if ((e == kExpED || e == kExpWG || e == kExpDS || e == kExpCP || e == kExpSPF || e == kExpMQ || e == kExpPT1 || e == kExpDL) && g_host.exp[e].enable &&
            !*g_host.exp[e].enable && v == 0)
            v = 2;
        add(L"Patches", kExpIniKey[e], std::to_wstring(v));
    }
    if (Available(kGpu))  // hooks installed only for the GP profiler (Barriers 0 at launch) stay 0 unless switched on here
        add(L"GpuOpt", L"Barriers", gpuopt::Batching() ? L"1" : gpuopt::BarriersIni() ? L"2" : L"0");
    if (Available(kGpu) && g_host.wine) {
        add(L"GpuOpt", L"R2a", gpuopt::Rewrite(0) ? L"1" : L"0");
        add(L"GpuOpt", L"R2b", gpuopt::Rewrite(1) ? L"1" : L"0");
        add(L"GpuOpt", L"R1", gpuopt::Rewrite(2) ? L"1" : L"0");
    }
    add(L"Latency", L"MaxFrameLatency", std::to_wstring(osd::Overlay().maxLatency));
    add(L"Patches", L"WorkerCap", std::to_wstring(g_bootWorkerCap));
    add(L"Patches", L"P12", b(g_bootP12 != 0));
    if (g_host.wine) {
        add(L"Patches", L"L1OnWine", b(g_bootL1Wine != 0));
        add(L"Patches", L"FGOnWine", b(g_bootFgWine != 0));
    }
    osd::OverlaySettings& o = osd::Overlay();
    add(L"Overlay", L"FpsCounter", b(o.fpsCounter != 0));
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
const char* SrcName(LONG src) { return src == 1 ? g_cfg.keyName : src == 2 ? "controller gesture" : "?"; }

void Open(const char* how) {
    InterlockedExchange(&g_pressed, 0);
    for (PadSlot& s : g_pad) s.latch = true;
    InterlockedExchange(&g_open, 1);
    MessageBeep(MB_OK);  // audible confirmation of the hold gesture, even if the overlay can't draw
    if (!Visible(g_sel)) g_sel = 0;
    MLOG("opened (%s) - game pad%s input is held neutral while open", how, g_keyHooked ? " and navigation-key" : "");
}

void Close(const char* how) {
    for (PadSlot& s : g_pad) s.latch = true;
    InterlockedOr(&g_keyLatch, (LONG)K_NAV);
    InterlockedExchange(&g_open, 0);
    MessageBeep(MB_ICONASTERISK);
    MLOG("closed (%s)%s", how, g_dirty ? " - unsaved changes are active until the game exits" : "");
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

// Frame generation: A/Enter = OFF <-> last type (FSR), Left/Right = cycle OFF / FSR / XeSS / DLSS (available ones).
// One switch per 2 s: every change makes the game rebuild its swapchain.
void ChangeFg(int dir) {
    if (!Available(kFg)) return;
    const ULONGLONG t = Now();
    if (g_fgChangedAt && t - g_fgChangedAt < 2000) return;
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
        case kFrameTime: o.frameTime = !o.frameTime; break;
        case kSpeed: o.speed = !o.speed; break;
        case kPos: o.pos = (o.pos + (dir < 0 ? 3 : 1)) & 3; break;
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
    MLOG("%s -> %s", kItems[id].label, v);
}

void Move(int dir) {
    for (int k = 0; k < kCount; ++k) {
        g_sel = (g_sel + dir + kCount) % kCount;
        if (Visible(g_sel) && !ReadOnly(g_sel)) return;
    }
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

void PauseDetect(ULONGLONG t) {
    if (!g_host.knownBuild || !g_host.base) return;
    uint64_t gp = 0;
    if (!Rd(g_host.base + kRvaFrameMgrPtr, gp) || gp < 0x10000) return;
    const uint8_t* G = reinterpret_cast<const uint8_t*>((uintptr_t)gp);
    uint8_t b[6] = {};
    uint32_t fc = 0;
    for (int i = 0; i < 6; ++i)
        if (!Rd(G + kOffStopFlags + i, b[i])) return;
    if (!Rd(G + kOffFrameCounter, fc)) return;
    uint8_t flags = 0;
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
    constexpr uint8_t kHard = 0x23;  // 588, 589, 58D: shutdown / system stops - never show the hint for those
    const bool cand = (flags & g_cfg.pauseMask) && !(flags & kHard & ~g_cfg.pauseMask) && t - g_frameAdvAt < 300;
    if (!cand) {
        g_pauseCandSince = 0;
        if (g_paused) g_paused = false;
        return;
    }
    if (!g_pauseCandSince) g_pauseCandSince = t;
    if (!g_paused && t - g_pauseCandSince >= 250) g_paused = true;
}

void OnPresentImpl() {
    const ULONGLONG t = Now();
    g_lastPresent = (LONG64)t;
    OwnPolling(t);
    SampleRates(t);
    if (g_cfg.pauseHint) PauseDetect(t);
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
    if ((pressed & N_BACK) && g_open) Close("B / Esc");
}

// ---- drawing --------------------------------------------------------------------------------------------------------------
struct Row {
    char label[128];
    char value[48];
    uint32_t lc, vc;
    bool sel, rule;
};

std::string GestureText() {
    std::string s;
    if (g_cfg.vk) s = g_cfg.keyName;
    if (g_cfg.holdMask) {
        if (!s.empty()) s += " / ";
        char b[96];
        if (g_cfg.gesture == 1) snprintf(b, sizeof(b), "Press %s 3x", g_cfg.holdName);
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
        y += (osd::kGlyphH + 2) * csc + 8 * csc - 2 * csc + 4 * csc;
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
        if (g_cfg.pauseHint && g_paused) {
            const std::string s = GestureText() + ": Mod menu";  // drawn every frame while the pause flags are set (not a timed popup)
            return DrawCornerNote(W, H, s.c_str(), cWhite);
        }
        return 0;
    }

    // ---- the menu panel
    Row rows[64];
    int n = 0;
    auto add = [&](const char* label, const char* value, uint32_t lc, uint32_t vc, bool sel, bool rule) {
        if (n >= 64) return;
        Row& r = rows[n++];
        snprintf(r.label, sizeof(r.label), "%s", label);
        snprintf(r.value, sizeof(r.value), "%s", value);
        r.lc = lc;
        r.vc = vc;
        r.sel = sel;
        r.rule = rule;
    };
    char title[96];
    snprintf(title, sizeof(title), "Nioh3EngineFix v%s - mod menu", g_host.version);
    add(title, g_dirty ? "unsaved changes" : "", cGold, cYellow, false, true);
    for (int id = 0; id < kCount; ++id) {
        if (!Visible(id)) continue;
        if (kItems[id].section) add(kItems[id].section, "", cCyan, cWhite, false, false);
        char v[64];
        uint32_t vc;
        Value(id, v, sizeof(v), vc);
        char lab[72];
        snprintf(lab, sizeof(lab), "%s%s", id == g_sel ? "> " : "  ", kItems[id].label);
        add(lab, v, id == g_sel ? cWhite : (Available(id) || id >= kSave ? cGray : cDim), vc, id == g_sel, false);
    }
    // description of the selected item
    char desc[160];
    snprintf(desc, sizeof(desc), "%s", kItems[g_sel].desc);
    if (!Available(g_sel)) {
        if ((g_sel == kL1 && g_host.l1 == FixState::WineSkipped) || (g_sel == kF1 && g_host.f1 == FixState::WineSkipped))
            snprintf(desc, sizeof(desc), "Skipped under Proton - turn on '%s on Proton' below, save, restart.", g_sel == kL1 ? "L1" : "F1");
        else if (g_sel == kFg)
            snprintf(desc, sizeof(desc), "%s",
                     !g_cfg.fgToggle ? "Status only ([Menu] FrameGenToggle = 0): switch it in the game's Options."
                     : g_host.wine && g_cfg.fgToggle == 1 ? "Status only on Proton: FG is reported to crash there (FrameGenToggle=2)."
                                                          : "Status only: the game's FG setter did not verify on this build (see the log).");
        else
            snprintf(desc, sizeof(desc), "Not installed this session (off in the ini or a site was not found - see the log).");
    }
    add("", "", cWhite, cWhite, false, true);
    add(desc, "", cWhite, cWhite, false, false);
    if (g_sel == kFg) {  // the row stays while FG is selected, so the panel does not jump when the reason changes
        const char* why = osd::FgReason(osd::FrameGen());
        add(why ? why : "", "", cOrange, cWhite, false, false);
    }
    char stats[160];
    const Rates& r = g_rates;
    if (r.rW3 < 0) snprintf(stats, sizeof(stats), "Stats/s: measuring...");
    else
        snprintf(stats, sizeof(stats), "Stats/s: W3 %.0f  L1 %.0f  F1 %.0f  WK kept %.0f skipped %.0f", r.rW3, r.rL1, r.rFg,
                 r.rKk, r.rKs);
    add(stats, "", cGray, cWhite, false, false);
    bool anyExp = false;
    for (const ExpFix& x : g_host.exp) anyExp |= x.applied;
    if (anyExp && r.rW3 >= 0) {
        char fpc[16];
        if (r.fpcPct >= 0) snprintf(fpc, sizeof(fpc), "%.0f%%", r.fpcPct);
        else snprintf(fpc, sizeof(fpc), "-");
        snprintf(stats, sizeof(stats), "Exp/s: W6 %.0f x %.0fus  GF %.0f x %.0fus  PP skip %.0f  FPC hit %s  M1 %.0f  X1 %.0f  E1 %.0f  WG skip %.0f  DS %.0f",
                 r.rW6, r.w6Us, r.rGf, r.gfUs, r.rPps, fpc, r.rM1, r.rX1, r.rE1, r.rWg, r.rDs);
        add(stats, "", cGray, cWhite, false, false);
    }
    if (gpuopt::GetState() != gpuopt::State::Off && g_host.gpuStatus) {
        char gs[120];
        g_host.gpuStatus(gs, sizeof(gs));
        char line[128];
        snprintf(line, sizeof(line), "GPU: %s", gs);
        add(line, "", cGray, cWhite, false, false);
    }
    if (g_host.gpStatus) {  // GP profiler (1.7.0): GPU frame time and the top 5 passes
        char gs[200];
        g_host.gpStatus(gs, sizeof(gs));
        if (gs[0]) {
            char line[220];
            snprintf(line, sizeof(line), "GP: %s", gs);
            add(line, "", cCyan, cWhite, false, false);
        }
    }
    // footer / save status
    const LONG ss = g_saveState;
    if (ss >= 2 && !g_saveShownAt) g_saveShownAt = t;
    if (holdNote[0]) add(holdNote, "", cGold, cWhite, false, false);
    else if (ss == 1) add("Saving to Nioh3EngineFix.ini...", "", cYellow, cWhite, false, false);
    else if (ss >= 2 && t - g_saveShownAt < 4000) {
        char b[96];
        const LONG xml = g_cfgXmlState;
        if (ss == 2) snprintf(b, sizeof(b), "Saved to Nioh3EngineFix.ini%s", xml == 1 ? " + game config.xml (FG)" : "");
        else if (xml == 3) snprintf(b, sizeof(b), "config.xml NOT changed (FG) - see the log");
        else snprintf(b, sizeof(b), "Save FAILED (error %ld) - see the log", (long)g_saveErr);
        add(b, "", ss == 2 ? cGreen : cOrange, cWhite, false, false);
    } else {
        add("Up/Down: move   A/Enter: toggle   Left/Right: change   B/Esc: close", "", cDim, cWhite, false, false);
    }

    constexpr int kValueCol = 30;
    size_t cols = 0;
    for (int i = 0; i < n; ++i) {
        const size_t lc = strlen(rows[i].label), vc = strlen(rows[i].value);
        cols = std::max(cols, vc ? std::max<size_t>(lc + 2, kValueCol) + vc : lc);
    }
    float sc = AutoScale(H);
    auto fits = [&](float s) {
        const float lineH = (osd::kGlyphH + 2) * s, pad = 6 * s;
        return n * lineH + 2 * pad <= 0.94f * (float)H && (float)cols * osd::kGlyphW * s + 2 * pad <= 0.96f * (float)W;
    };
    while (sc > 1.0f && !fits(sc)) sc -= 0.5f;
    while (!fits(sc) && sc > 0.5f) sc -= 0.25f;  // small back buffers (800p with every row): 0.75, then 0.5
    const float lineH = (osd::kGlyphH + 2) * sc, pad = 6 * sc, cw = osd::kGlyphW * sc;
    const float w = (float)cols * cw + 2 * pad, h = n * lineH + 2 * pad;
    const float x0 = std::floor(((float)W - w) / 2), y0 = std::floor(((float)H - h) / 2);
    osd::DrawRect(x0, y0, w, h, cMenuPanel);
    osd::DrawRect(x0, y0, w, std::max(1.0f, sc), cRule);
    osd::DrawRect(x0, y0 + h - std::max(1.0f, sc), w, std::max(1.0f, sc), cRule);
    for (int i = 0; i < n; ++i) {
        const Row& rw = rows[i];
        const float y = y0 + pad + i * lineH;
        if (rw.sel) osd::DrawRect(x0 + pad / 2, y - sc, w - pad, lineH, cHighlight);
        if (rw.rule && i > 0) osd::DrawRect(x0 + pad, y + lineH / 2, w - 2 * pad, std::max(1.0f, sc / 2), 0x80808080);
        if (rw.label[0]) osd::DrawText(x0 + pad, y, rw.label, rw.lc, sc);
        if (rw.value[0]) {
            const size_t lc = strlen(rw.label);
            const float vx = x0 + pad + (float)std::max<size_t>(lc + 2, kValueCol) * cw;
            osd::DrawText(i == 0 ? x0 + w - pad - (float)strlen(rw.value) * cw : vx, y, rw.value, rw.vc, sc);
        }
    }
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
void ProtonFgEarly(void (*log)(const char* fmt, ...), const wchar_t* markerPath) {
    if (!g_log) g_log = log;
    g_fgMarker = markerPath ? markerPath : L"";
    wchar_t e[MAX_PATH] = {};
    const DWORD n = ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\\KoeiTecmo\\NIOH3\\config.xml", e, MAX_PATH);
    if (n && n <= MAX_PATH && e[0] != L'%') g_cfgPathEarly = e;
    g_fgAtStart = g_cfgPathEarly.empty() ? "" : ReadGameConfigFg(g_cfgPathEarly);
    const bool marker = !g_fgMarker.empty() && GetFileAttributesW(g_fgMarker.c_str()) != INVALID_FILE_ATTRIBUTES;
    MLOG("Proton: config.xml %ls: FRAME_GENERATION %s at startup%s", g_cfgPathEarly.empty() ? L"(no path)" : g_cfgPathEarly.c_str(),
         g_fgAtStart.empty() ? "?" : g_fgAtStart.c_str(), marker ? "; FG marker from the last session found" : "");
    if (!marker) return;
    if (!g_fgAtStart.empty() && g_fgAtStart != "OFF") {
        MLOG("Proton: the last session ended with FG switched on by the mod (crash?) and config.xml has %s - putting OFF back "
             "before the game reads it", g_fgAtStart.c_str());
        const int r = WriteGameConfigFg(g_cfgPathEarly, "OFF");
        if (r == 1 || r == 2) g_fgAtStart = "OFF";
    }
    DeleteFileW(g_fgMarker.c_str());
}

void ProtonFgExit(const char* how) {
    if (!g_fgMarkerSet || InterlockedExchange(&g_fgExitDone, 1)) return;
    std::wstring path = g_host.base && g_host.knownBuild ? GameConfigPath() : g_cfgPathEarly;
    if (path.empty()) path = g_cfgPathEarly;
    const std::string cur = ReadGameConfigFg(path);
    MLOG("Proton exit (%s): config.xml FRAME_GENERATION is %s", how, cur.empty() ? "?" : cur.c_str());
    if (cur != "OFF") {
        const int r = WriteGameConfigFg(path, "OFF");
        MLOG("Proton exit: FRAME_GENERATION OFF %s", r == 1 ? "restored" : r == 2 ? "already set" : "NOT restored - the marker stays for the next start");
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
    g_cfg.pauseHint = (int)GetPrivateProfileIntW(L"Menu", L"PauseHint", 1, f);
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
                                     : g_fgOk ? (g_host.wine ? "live switch enabled under Proton ([Menu] FrameGenToggle = 2); FG on is "
                                                               "never saved to the game's config here"
                                                             : "live switch through the game's FrameGenSettingWrite (0x1FDDFCC) verified")
                                     : g_cfg.fgToggle ? "status only under Proton (FG is reported to crash there; FrameGenToggle = 2 "
                                                        "allows the switch)"
                                                      : "status only ([Menu] FrameGenToggle = 0)");
    }
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
