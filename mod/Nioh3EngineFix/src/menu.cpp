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
    float scale = 0;    // 0 = auto
    int pauseHint = 1;
    uint8_t pauseMask = 0x18;  // bit i = G+0x588+i; default 58B + 58C (the two flags that stop game time)
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
    if (hm) {
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

// ---- items ----------------------------------------------------------------------------------------------------------
enum Id { kVrr, kW3, kW3Mode, kR1, kL1, kF1, kWk, kFps, kFrameTime, kSpeed, kPos, kScale, kWorkerCap, kP12, kL1Wine, kFgWine, kSave, kClose, kCount };
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
    {"OVERLAY", "FPS counter", "Real FPS (engine) and FG FPS (presented, incl. generated frames)."},
    {nullptr, "Frame time", "Adds the real frame time (ms) to the counter."},
    {nullptr, "Speed factor", "Adds the game speed: 1.00x = real time, below 1 = the game runs slow."},
    {nullptr, "Counter position", "Corner of the FPS counter."},
    {nullptr, "Counter text size", "1.0 = 8x16 px letters (x2 at 4K). Whole numbers look sharpest."},
    {"NEXT LAUNCH - saved to the ini", "WorkerCap", "Handhelds: cap the job thread pools at N (try 10, then 8). Off on desktops."},
    {nullptr, "P12 worker-pool lift", "Keeps 3 more pool threads on CPUs with <= 12 threads. Experimental."},
    {nullptr, "L1 on Proton", "Install L1 under Wine/Proton too (Wine may not honour the timer)."},
    {nullptr, "F1 on Proton", "Install F1 under Wine/Proton too (same timer caveat as L1)."},
    {"", "Save to ini", "Write every setting above to Nioh3EngineFix.ini (used from the next launch)."},
    {nullptr, "Close menu", "Also: B / Esc / the menu key / the open gesture."},
};

int g_sel = 0;
bool g_dirty = false;
int g_bootWorkerCap = 0, g_bootP12 = 0, g_bootL1Wine = 0, g_bootFgWine = 0;

bool Visible(int id) {
    switch (id) {
        case kW3Mode: return g_host.wine && g_host.w3Enable && g_host.w3UseSleep;
        case kL1Wine:
        case kFgWine: return g_host.wine;
        default: return true;
    }
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
        default: return false;
    }
}

void Value(int id, char* buf, size_t n, uint32_t& color) {
    buf[0] = 0;
    color = cWhite;
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
    if (!Visible(g_sel)) g_sel = 0;
    MLOG("opened (%s) - game pad%s input is held neutral while open", how, g_keyHooked ? " and navigation-key" : "");
}

void Close(const char* how) {
    for (PadSlot& s : g_pad) s.latch = true;
    InterlockedOr(&g_keyLatch, (LONG)K_NAV);
    InterlockedExchange(&g_open, 0);
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

// dir: -1 left, +1 right, 0 = A / Enter
void Change(int id, int dir) {
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
    switch (id) {
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
        if (Visible(g_sel)) return;
    }
}

// ---- per-present processing -----------------------------------------------------------------------------------------------
struct Rates {
    ULONGLONG t0 = 0;
    long w3 = 0, l1 = 0, fg = 0;
    uint32_t kk = 0, ks = 0;
    double rW3 = -1, rL1 = -1, rFg = -1, rKk = -1, rKs = -1;
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
    } else if (r.t0) {
        return;
    }
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
    char label[72];
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
        if (g_cfg.holdMs > 0) snprintf(b, sizeof(b), "Hold %s %gs", g_cfg.holdName, g_cfg.holdMs / 1000.0);
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
            const std::string s = GestureText() + ": Mod menu";
            return DrawCornerNote(W, H, s.c_str(), cWhite);
        }
        return 0;
    }

    // ---- the menu panel
    Row rows[48];
    int n = 0;
    auto add = [&](const char* label, const char* value, uint32_t lc, uint32_t vc, bool sel, bool rule) {
        if (n >= 48) return;
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
        else
            snprintf(desc, sizeof(desc), "Not installed this session (off in the ini or a site was not found - see the log).");
    }
    add("", "", cWhite, cWhite, false, true);
    add(desc, "", cWhite, cWhite, false, false);
    char stats[160];
    const Rates& r = g_rates;
    if (r.rW3 < 0) snprintf(stats, sizeof(stats), "Stats/s: measuring...");
    else
        snprintf(stats, sizeof(stats), "Stats/s: W3 %.0f  L1 %.0f  F1 %.0f  WK kept %.0f skipped %.0f", r.rW3, r.rL1, r.rFg,
                 r.rKk, r.rKs);
    add(stats, "", cGray, cWhite, false, false);
    // footer / save status
    const LONG ss = g_saveState;
    if (ss >= 2 && !g_saveShownAt) g_saveShownAt = t;
    if (holdNote[0]) add(holdNote, "", cGold, cWhite, false, false);
    else if (ss == 1) add("Saving to Nioh3EngineFix.ini...", "", cYellow, cWhite, false, false);
    else if (ss >= 2 && t - g_saveShownAt < 4000) {
        char b[96];
        if (ss == 2) snprintf(b, sizeof(b), "Saved to Nioh3EngineFix.ini");
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
    if (!fits(sc) && sc > 0.5f) sc = 0.5f;  // tiny back buffers (< ~500 px): half-size text rather than clipping
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
void Configure(const Host& h) {
    g_host = h;
    g_log = h.log;
    g_ini = h.iniPath ? h.iniPath : L"";
    g_host.iniPath = nullptr;  // not kept (g_ini owns the copy)
    g_bootWorkerCap = h.workerCap;
    g_bootP12 = h.p12;
    g_bootL1Wine = h.l1OnWine;
    g_bootFgWine = h.fgOnWine;
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
    if (g_cfg.holdMask && g_cfg.holdMask == XINPUT_GAMEPAD_START && g_cfg.holdMs == 0)
        MLOG("[Menu] HoldSeconds=0 with HoldButton=Start opens the menu on every Start press - consider a combo");
    g_configured = true;
    if (!g_cfg.enabled) {
        MLOG("off in ini ([Menu] Enabled=0)");
        return;
    }
    char hold[96];
    if (!g_cfg.holdMask) snprintf(hold, sizeof(hold), "none");
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
