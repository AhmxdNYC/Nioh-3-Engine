// vpad.cpp - VirtualPad + KeepRunningUnfocused ([Debug], default 0). See vpad.h and notes\virtual-input.md.
// Threads: the game's input worker runs HkXInputGetState (+ KeyFilter via the menu's DirectInput hook); "VPad pipe" accepts
// clients, one "VPad client" thread per connection runs the commands; "VPad idle" releases held inputs after
// VirtualPadIdleReleaseSec without a command. Everything shared sits behind one SRW lock held for a few hundred ns.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "gamerva.h"
#include <intrin.h>
#include <xinput.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "vpad.h"
#include "vpad_core.h"

namespace vpad {
namespace {

// ---- v2.02 (patches.json group VP, check-only; verified by tests\test_patterns.py) ----
constexpr uint32_t kIsActive = 0x436CF8;  // IsAppActive: G ? G->appActive(+1ABh) : 0 - leaf, writes rax + flags only
const uint8_t kIsActiveBytes[] = {0x48, 0x8B, 0x05, 0x11, 0x2C, 0x32, 0x04, 0x48, 0x85, 0xC0, 0x74, 0x07,
                                  0x8A, 0x80, 0xAB, 0x01, 0x00, 0x00, 0xC3, 0xC3};
struct Site {
    uint32_t rva;
    const char* what;
    uint8_t after[8];  // the bytes right after the call (context check)
    int nAfter;
};
const Site kSites[] = {
    {0x436758, "PadMgrUpdate (pads ignored while inactive)", {0x45, 0x33, 0xED, 0x84, 0xC0, 0x75, 0x1F}, 7},
    {0x436A20, "pad slot read 436A1C", {0x84, 0xC0, 0x75, 0x16, 0x48, 0x8B, 0x05}, 7},
    {0x89B2FD, "focus-loss pause 89B230", {0x84, 0xC0, 0x75, 0x0C, 0x41, 0x88, 0xA9}, 7},
};
constexpr uint32_t kRvaGameMain = 0x4759910;  // G (GameMain / frame manager): +1ABh appActive, +588h stop flags, +610h frames
constexpr uint32_t kRvaPlayerTab = 0x4745348; // player 0 = [P + 0xCD * 0x18], position floats at +110h (tour.cpp)

void (*g_log)(const char* fmt, ...) = nullptr;
#define VLOG(...) \
    do {          \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

StartInfo g_info;
int g_mode = 0;               // 0 off, 1 exclusive (real pads hidden), 2 merge
volatile LONG g_armed = 0;    // 1 = virtual slot live (ini mode); 'disarm' -> plain pass-through
volatile LONG g_realOff = 0;  // merge mode: 'realpads off' hides the real pads (exclusive mode always does)
int g_idleSec = 600;
SRWLOCK g_lock = SRWLOCK_INIT;
Composer g_comp;
Pad g_lastOut;
DWORD g_packet = 1;
volatile LONG64 g_lastCmdUs = 0;
volatile LONG g_clients = 0;
volatile uint8_t* g_kruSwitch = nullptr;  // thunk page +28h
volatile long* g_kruCount = nullptr;      // thunk page +2Ch: inactive answers turned into "active"
int g_kruSites = 0;
wchar_t g_pipe[128] = L"\\\\.\\pipe\\nioh3vpad";

int64_t NowUs() {
    static const int64_t f = [] { LARGE_INTEGER q; QueryPerformanceFrequency(&q); return q.QuadPart; }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (int64_t)((double)c.QuadPart * 1e6 / (double)f);
}

bool ForegroundIsGame() {
    HWND w = GetForegroundWindow();
    DWORD pid = 0;
    if (w) GetWindowThreadProcessId(w, &pid);
    return pid == GetCurrentProcessId();
}

// ---- XInputGetState hook ---------------------------------------------------------------------------------------------
using XInputGetState_t = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
XInputGetState_t g_xiOrig = nullptr;

void Fill(XINPUT_STATE* st, const Pad& p) {
    if (p != g_lastOut) {
        g_lastOut = p;
        ++g_packet;
    }
    st->dwPacketNumber = g_packet;
    st->Gamepad.wButtons = p.buttons;
    st->Gamepad.bLeftTrigger = p.lt;
    st->Gamepad.bRightTrigger = p.rt;
    st->Gamepad.sThumbLX = p.lx;
    st->Gamepad.sThumbLY = p.ly;
    st->Gamepad.sThumbRX = p.rx;
    st->Gamepad.sThumbRY = p.ry;
}

DWORD WINAPI HkXInputGetState(DWORD idx, XINPUT_STATE* st) {
    if (!g_armed) return g_xiOrig(idx, st);
    if (g_mode == 1) {  // exclusive: slot 0 is the virtual pad, the real pads do not exist for the game
        if (idx != 0) return ERROR_DEVICE_NOT_CONNECTED;
        if (!st) return ERROR_BAD_ARGUMENTS;
        const int64_t now = NowUs();
        AcquireSRWLockExclusive(&g_lock);
        const Pad p = g_comp.ReadPad(now);
        Fill(st, p);
        ReleaseSRWLockExclusive(&g_lock);
        return ERROR_SUCCESS;
    }
    // merge: real pads as usual; slot 0 gets the virtual input on top (and exists even without a real pad there).
    // 'realpads off' (test popup's block toggle): the real pads are hidden, slot 0 carries only the virtual input.
    if (g_realOff) {
        if (idx != 0) return ERROR_DEVICE_NOT_CONNECTED;
        if (!st) return ERROR_BAD_ARGUMENTS;
        const int64_t now = NowUs();
        AcquireSRWLockExclusive(&g_lock);
        Fill(st, g_comp.ReadPad(now));
        ReleaseSRWLockExclusive(&g_lock);
        return ERROR_SUCCESS;
    }
    DWORD r = g_xiOrig(idx, st);
    if (idx != 0 || !st) return r;
    if (r != ERROR_SUCCESS) {
        memset(st, 0, sizeof(*st));
        r = ERROR_SUCCESS;
    }
    const int64_t now = NowUs();
    AcquireSRWLockExclusive(&g_lock);
    Pad p = g_comp.ReadPad(now);
    ReleaseSRWLockExclusive(&g_lock);
    XINPUT_GAMEPAD& g = st->Gamepad;
    g.wButtons |= p.buttons;
    if (p.lt > g.bLeftTrigger) g.bLeftTrigger = p.lt;
    if (p.rt > g.bRightTrigger) g.bRightTrigger = p.rt;
    if (p.lx || p.ly) g.sThumbLX = p.lx, g.sThumbLY = p.ly;
    if (p.rx || p.ry) g.sThumbRX = p.rx, g.sThumbRY = p.ry;
    if (p != Pad{}) st->dwPacketNumber += 0x20000000u;  // own packet numbers while virtual input is mixed in
    return r;
}

// the game's IAT slot for XInputGetState (import by name, or ordinal 2 of xinput1_x) - same lookup as menu.cpp / padlat.cpp
void** FindXInputSlot(uint8_t* base) {
    __try {
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
        const IMAGE_DATA_DIRECTORY& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dd.VirtualAddress) return nullptr;
        for (auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dd.VirtualAddress); d->Name; ++d) {
            const char* dll = reinterpret_cast<const char*>(base + d->Name);
            if (_strnicmp(dll, "xinput", 6) != 0 || !d->OriginalFirstThunk || !d->FirstThunk) continue;
            const bool ord2 = _strnicmp(dll, "xinput1_", 8) == 0;
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
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
    return nullptr;
}

bool HookSlot(void** slot, uint8_t* hostBase) {
    void* cur = *slot;
    HMODULE m = nullptr;
    if (!cur || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)cur, &m) ||
        (uint8_t*)m == hostBase) {
        VLOG("VPad: XInputGetState IAT slot %p holds %p, which is not inside a loaded DLL - not hooked", (void*)slot, cur);
        return false;
    }
    DWORD old;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) {
        VLOG("VPad: VirtualProtect on the XInputGetState IAT slot failed (%lu)", GetLastError());
        return false;
    }
    g_xiOrig = reinterpret_cast<XInputGetState_t>(cur);  // published before the slot so the hook never sees a null original
    MemoryBarrier();
    InterlockedExchangePointer(slot, (void*)&HkXInputGetState);
    DWORD t;
    VirtualProtect(slot, sizeof(void*), old, &t);
    return true;
}

// ---- KeepRunningUnfocused: thunk + call-site retargets (same scheme as autocont.cpp) -----------------------------------
uint8_t* AllocIn(uintptr_t from, uintptr_t to, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + size <= re && c + size <= to)
                if (void* p = VirtualAlloc((void*)c, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}
bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}
// a site straddling a 16-byte boundary: park the first two bytes on "jmp $", write the tail, then the head
bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}
bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

bool CodeOk(uint8_t* base) {
    __try {
        if (memcmp(base + Grva(kIsActive), kIsActiveBytes, sizeof(kIsActiveBytes)) != 0) return false;
        for (const Site& s : kSites) {
            const uint8_t* p = base + Grva(s.rva);
            int32_t rel = 0;
            memcpy(&rel, p + 1, 4);
            if (p[0] != 0xE8 || p + 5 + rel != base + Grva(kIsActive) || memcmp(p + 5, s.after, (size_t)s.nAfter) != 0) return false;
        }
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Windows throttles background processes: EcoQoS (execution speed) and, on Windows 11, timer-resolution requests of
// processes whose windows are occluded / minimised are ignored. Opt out, so an unfocused test runs with focused timing.
void PowerThrottlingOff() {
    struct PPTS {
        ULONG Version, ControlMask, StateMask;
    } s = {1, 0x1 /* EXECUTION_SPEED */ | 0x4 /* IGNORE_TIMER_RESOLUTION */, 0};
    using SetPI_t = BOOL(WINAPI*)(HANDLE, int, LPVOID, DWORD);
    auto f = reinterpret_cast<SetPI_t>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetProcessInformation"));
    if (!f) {
        VLOG("KeepRunningUnfocused: SetProcessInformation not available - power throttling left as is");
        return;
    }
    const BOOL ok = f(GetCurrentProcess(), 4 /* ProcessPowerThrottling */, &s, sizeof(s));
    const DWORD e = ok ? 0 : GetLastError();
    if (!ok && e == ERROR_INVALID_PARAMETER) {  // older Windows 10: no timer-resolution bit
        s.ControlMask = 0x1;
        if (f(GetCurrentProcess(), 4, &s, sizeof(s))) {
            VLOG("KeepRunningUnfocused: power throttling off (EcoQoS; this Windows has no timer-resolution throttling bit)");
            return;
        }
    }
    if (ok) VLOG("KeepRunningUnfocused: power throttling off (EcoQoS + Windows 11 timer-resolution throttling of hidden windows)");
    else VLOG("KeepRunningUnfocused: SetProcessInformation(ProcessPowerThrottling) failed (%lu)", e);
}

void BuildThunk(uint8_t* t, uint64_t target) {
    // Thunk (writes al + flags; IsAppActive itself writes rax + flags, every register the callers keep live survives):
    //   00 call qword [rip+1Ah]       ; IsAppActive (leaf, no stack use)
    //   06 test al, al / 08 jne 1Ch   ; really active: unchanged
    //   0A cmp byte [rip+17h], 0      ; switch at +28h
    //   11 je 1Ch
    //   13 mov al, 1                  ; "active"
    //   15 lock inc dword [rip+10h]   ; count at +2Ch
    //   1C ret
    //   20 dq IsAppActive
    static const uint8_t code[0x1D] = {0xFF, 0x15, 0x1A, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x75, 0x12, 0x80, 0x3D, 0x17, 0x00, 0x00,
                                       0x00, 0x00, 0x74, 0x09, 0xB0, 0x01, 0xF0, 0xFF, 0x05, 0x10, 0x00, 0x00, 0x00, 0xC3};
    memcpy(t, code, sizeof(code));
    memset(t + 0x1D, 0xCC, 3);
    memcpy(t + 0x20, &target, 8);
    g_kruSwitch = t + 0x28;
    g_kruCount = (volatile long*)(t + 0x2C);
    *g_kruSwitch = 1;
    *g_kruCount = 0;
    FlushInstructionCache(GetCurrentProcess(), t, 0x30);
}

void InstallUnfocused(uint8_t* base) {
    if (!CodeOk(base)) {
        VLOG("KeepRunningUnfocused: not installed (IsAppActive call sites differ - another patch or build)");
        return;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    uint8_t* t = AllocNear((uintptr_t)base, (uintptr_t)base + nt->OptionalHeader.SizeOfImage, 4096);
    if (!t) {
        VLOG("KeepRunningUnfocused: not installed (no executable page within 2 GB of the image)");
        return;
    }
    BuildThunk(t, (uint64_t)(base + Grva(kIsActive)));
    for (const Site& s : kSites) {
        uint8_t* site = base + s.rva;
        const intptr_t d = (intptr_t)t - (intptr_t)(site + 5);
        if (d < INT32_MIN || d > INT32_MAX) continue;
        uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
        const int32_t nrel = (int32_t)d;
        memcpy(nb + 1, &nrel, 4);
        if (Patch(site, nb, 5)) ++g_kruSites;
        else VLOG("KeepRunningUnfocused: site %X (%s) write failed", s.rva, s.what);
    }
    VLOG("KeepRunningUnfocused: installed at %d/3 IsAppActive call sites (pad update 436758, pad slot read 436A20, focus-loss "
         "pause 89B2FD): pads keep working and the game does not pause while its window is inactive. Mouse / keyboard / cursor "
         "keep the stock answer (an unfocused game never reads the user's mouse or moves the cursor)", g_kruSites);
    PowerThrottlingOff();
}

// ---- game state (state command) ------------------------------------------------------------------------------------
struct GameState {
    bool ok;
    uint32_t frame;
    uint8_t stop[6];
    uint8_t active;
    bool player;
    float pos[3];
};
GameState ReadGame() {
    GameState s = {};
    if (!g_info.base || !g_info.knownBuild) return s;
    __try {
        const uint64_t g = *(const volatile uint64_t*)(g_info.base + Grva(kRvaGameMain));
        if (g < 0x10000) return s;
        const uint8_t* G = (const uint8_t*)(uintptr_t)g;
        s.frame = *(const volatile uint32_t*)(G + 0x610);
        for (int i = 0; i < 6; ++i) s.stop[i] = G[0x588 + i];
        s.active = G[0x1AB];
        s.ok = true;
        const uint64_t pt = *(const volatile uint64_t*)(g_info.base + Grva(kRvaPlayerTab));
        if (pt >= 0x10000) {
            const uint64_t pl = *(const volatile uint64_t*)(uintptr_t)(pt + 0xCD * 0x18);
            if (pl >= 0x10000) {
                s.player = true;
                for (int i = 0; i < 3; ++i) s.pos[i] = *(const volatile float*)(uintptr_t)(pl + 0x110 + 4 * i);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        s.ok = false;
    }
    return s;
}

// ---- commands ------------------------------------------------------------------------------------------------------
int Reply(char* r, size_t n, int rc, const char* fmt, ...) {
    va_list a;
    va_start(a, fmt);
    char body[400];
    vsnprintf(body, sizeof(body), fmt, a);
    va_end(a);
    snprintf(r, n, "%s %s", rc ? "err" : "ok", body);
    return rc;
}

// wait until a timed entry is gone (shown >= 2 game frames and its time is up); cancel it on timeout
bool WaitTimed(uint32_t id, int ms, uint32_t* rounds) {
    const int64_t t0 = NowUs(), limit = (int64_t)(ms + 3000) * 1000;
    uint32_t r0;
    AcquireSRWLockShared(&g_lock);
    r0 = g_comp.PadRound() + g_comp.KeyRound();
    ReleaseSRWLockShared(&g_lock);
    for (;;) {
        AcquireSRWLockShared(&g_lock);
        const int st = g_comp.Status(id);
        const uint32_t r = g_comp.PadRound() + g_comp.KeyRound();
        ReleaseSRWLockShared(&g_lock);
        if (st == 0) {
            if (rounds) *rounds = r - r0;
            return true;
        }
        if (NowUs() - t0 > limit) {
            AcquireSRWLockExclusive(&g_lock);
            g_comp.Cancel(id);
            ReleaseSRWLockExclusive(&g_lock);
            return false;
        }
        Sleep(1);
    }
}
// wait for the next pad read round (a held state was seen by the game); false = no read within 1.5 s
bool WaitPadRound() {
    AcquireSRWLockShared(&g_lock);
    const uint32_t r0 = g_comp.PadRound();
    ReleaseSRWLockShared(&g_lock);
    for (int i = 0; i < 1500; ++i) {
        AcquireSRWLockShared(&g_lock);
        const uint32_t r = g_comp.PadRound();
        ReleaseSRWLockShared(&g_lock);
        if (r - r0 >= 2) return true;
        Sleep(1);
    }
    return false;
}
const char* SeenNote(bool seen) { return seen ? "(seen by the game)" : "(set; the game has not read the pad for 1.5 s)"; }

int Exec(const char* line, char* r, size_t n) {
    const Cmd c = Parse(line);
    InterlockedExchange64(&g_lastCmdUs, NowUs());
    const bool needPad = c.op == Op::Press || c.op == Op::Hold || c.op == Op::Release || c.op == Op::ReleaseAll ||
                         c.op == Op::Stick || c.op == Op::Trigger;
    if (needPad && !g_xiOrig) return Reply(r, n, 1, "the XInputGetState hook is not installed");
    if (needPad && !g_armed) return Reply(r, n, 1, "disarmed (send 'arm')");
    switch (c.op) {
        case Op::Bad: return Reply(r, n, 1, "%s", c.text);
        case Op::Help: return Reply(r, n, 0, "%s", HelpText());
        case Op::Ping:
            return Reply(r, n, 0, "vpad %s mode=%s armed=%ld unfocused=%s", g_info.version, g_mode == 1 ? "exclusive" : g_mode == 2 ? "merge" : "off",
                         (long)g_armed, g_kruSwitch && *g_kruSwitch ? "on" : "off");
        case Op::Arm: InterlockedExchange(&g_armed, g_mode ? 1 : 0); return Reply(r, n, 0, "armed=%ld", (long)g_armed);
        case Op::Disarm:
            AcquireSRWLockExclusive(&g_lock);
            g_comp.ReleaseAll();
            ReleaseSRWLockExclusive(&g_lock);
            InterlockedExchange(&g_armed, 0);
            return Reply(r, n, 0, "disarmed: the game reads the real pads again");
        case Op::Log: VLOG("VPad log: %s", c.text); return Reply(r, n, 0, "logged");
        case Op::RealPads:
            if (!g_xiOrig) return Reply(r, n, 1, "the XInputGetState hook is not installed");
            if (g_mode == 1) return Reply(r, n, 0, "real pads hidden (VirtualPad = 1 hides them for the whole session)");
            InterlockedExchange(&g_realOff, c.value ? 0 : 1);
            return Reply(r, n, 0, "real pads %s", c.value ? "reach the game" : "hidden from the game");
        case Op::Wait: Sleep((DWORD)c.ms); return Reply(r, n, 0, "waited %d ms", c.ms);
        case Op::Press: {
            AcquireSRWLockExclusive(&g_lock);
            const uint32_t id = g_comp.AddButtons(c.mask, c.ms);
            ReleaseSRWLockExclusive(&g_lock);
            if (!id) return Reply(r, n, 1, "too many timed inputs pending");
            uint32_t rounds = 0;
            if (!WaitTimed(id, c.ms, &rounds)) return Reply(r, n, 1, "timeout: the game did not read the pad (press cancelled)");
            return Reply(r, n, 0, "press %04X %d ms (%u pad reads rounds)", c.mask, c.ms, rounds);
        }
        case Op::Hold:
        case Op::Release:
        case Op::ReleaseAll: {
            AcquireSRWLockExclusive(&g_lock);
            if (c.op == Op::Hold) g_comp.Hold(c.mask);
            else if (c.op == Op::Release) g_comp.Release(c.mask);
            else g_comp.ReleaseAll();
            const uint16_t held = g_comp.Held().buttons;
            ReleaseSRWLockExclusive(&g_lock);
            return Reply(r, n, 0, "held=%04X %s", held, SeenNote(WaitPadRound()));
        }
        case Op::Stick: {
            const int16_t x = AxisFrom(c.x), y = AxisFrom(c.y);
            if (c.ms < 0) {
                AcquireSRWLockExclusive(&g_lock);
                g_comp.SetStick(c.side, x, y);
                ReleaseSRWLockExclusive(&g_lock);
                return Reply(r, n, 0, "stick %c %d %d held %s", c.side, x, y, SeenNote(WaitPadRound()));
            }
            AcquireSRWLockExclusive(&g_lock);
            const uint32_t id = g_comp.AddStick(c.side, x, y, c.ms);
            ReleaseSRWLockExclusive(&g_lock);
            if (!id) return Reply(r, n, 1, "too many timed inputs pending");
            uint32_t rounds = 0;
            if (!WaitTimed(id, c.ms, &rounds)) return Reply(r, n, 1, "timeout: the game did not read the pad (stick cancelled)");
            return Reply(r, n, 0, "stick %c %d %d %d ms (%u rounds)", c.side, x, y, c.ms, rounds);
        }
        case Op::Trigger: {
            const uint8_t v = TriggerFrom(c.x);
            if (c.ms < 0) {
                AcquireSRWLockExclusive(&g_lock);
                g_comp.SetTrigger(c.side, v);
                ReleaseSRWLockExclusive(&g_lock);
                return Reply(r, n, 0, "trigger %c %u held %s", c.side, v, SeenNote(WaitPadRound()));
            }
            AcquireSRWLockExclusive(&g_lock);
            const uint32_t id = g_comp.AddTrigger(c.side, v, c.ms);
            ReleaseSRWLockExclusive(&g_lock);
            if (!id) return Reply(r, n, 1, "too many timed inputs pending");
            uint32_t rounds = 0;
            if (!WaitTimed(id, c.ms, &rounds)) return Reply(r, n, 1, "timeout: the game did not read the pad (trigger cancelled)");
            return Reply(r, n, 0, "trigger %c %u %d ms (%u rounds)", c.side, v, c.ms, rounds);
        }
        case Op::Key: {
            AcquireSRWLockShared(&g_lock);
            const int64_t last = g_comp.LastKeyReadUs();
            ReleaseSRWLockShared(&g_lock);
            if (!last) return Reply(r, n, 1, "no keyboard read seen yet (needs the mod menu's DirectInput hook: [Menu] Enabled = 1)");
            AcquireSRWLockExclusive(&g_lock);
            const uint32_t id = g_comp.AddKey(c.dik, c.ms);
            ReleaseSRWLockExclusive(&g_lock);
            if (!id) return Reply(r, n, 1, "too many timed inputs pending");
            uint32_t rounds = 0;
            if (!WaitTimed(id, c.ms, &rounds)) return Reply(r, n, 1, "timeout: the game did not read the keyboard (key cancelled)");
            return Reply(r, n, 0, "key %02X %d ms (%u rounds)", c.dik, c.ms, rounds);
        }
        case Op::MenuGet:
        case Op::MenuSet: {
            if (!g_info.menuRemote) return Reply(r, n, 1, "mod menu not available");
            char mr[200] = {};
            const int rc = g_info.menuRemote(c.key, c.op == Op::MenuSet, c.value, mr, sizeof(mr), 3000);
            return Reply(r, n, rc < 0 ? 1 : 0, "%s", mr);
        }
        case Op::State: {
            const GameState g = ReadGame();
            const int64_t now = NowUs();
            AcquireSRWLockShared(&g_lock);
            const int64_t lp = g_comp.LastPadReadUs(), lk = g_comp.LastKeyReadUs();
            const uint64_t reads = g_comp.PadReads();
            const Pad h = g_comp.Held();
            ReleaseSRWLockShared(&g_lock);
            char gs[240] = "game=?";
            const int mp = g_info.pauseReason ? g_info.pauseReason() : -1;
            if (g.ok)
                snprintf(gs, sizeof(gs), "frame=%u active=%u loading=%u paused=%u player=%d pos=%.1f,%.1f,%.1f field=%d", g.frame, g.active,
                         g.stop[2], (g.stop[3] | g.stop[4]) ? 1u : 0u, g.player ? 1 : 0, g.pos[0], g.pos[1], g.pos[2],
                         (g.player && !g.stop[2] && !(g.stop[3] | g.stop[4]) && mp == 0) ? 1 : 0);
            return Reply(r, n, 0, "%s menu_pause=%d fg=%d armed=%ld pad_reads=%llu pad_age_ms=%lld key_age_ms=%lld held=%04X lx=%d ly=%d "
                         "unfocused=%s/%ld clients=%ld", gs, mp, ForegroundIsGame() ? 1 : 0,
                         (long)g_armed, (unsigned long long)reads, lp ? (long long)((now - lp) / 1000) : -1LL,
                         lk ? (long long)((now - lk) / 1000) : -1LL, h.buttons, h.lx, h.ly,
                         g_kruSwitch && *g_kruSwitch ? "on" : "off", g_kruCount ? *g_kruCount : 0L, (long)g_clients);
        }
    }
    return Reply(r, n, 1, "unhandled");
}

// ---- pipe server ---------------------------------------------------------------------------------------------------
DWORD WINAPI ClientThread(void* hp) {
    HANDLE h = (HANDLE)hp;
    char buf[2048];
    size_t have = 0;
    for (;;) {
        DWORD got = 0;
        if (!ReadFile(h, buf + have, (DWORD)(sizeof(buf) - 1 - have), &got, nullptr) || got == 0) break;
        have += got;
        buf[have] = 0;
        char* start = buf;
        for (char* nl; (nl = (char*)memchr(start, '\n', have - (size_t)(start - buf))) != nullptr; start = nl + 1) {
            *nl = 0;
            if (nl > start && nl[-1] == '\r') nl[-1] = 0;
            char reply[640];
            Exec(start, reply, sizeof(reply) - 2);
            VLOG("VPad: %s -> %s", start, reply);
            const size_t rl = strlen(reply);
            reply[rl] = '\n';
            DWORD wr = 0;
            if (!WriteFile(h, reply, (DWORD)rl + 1, &wr, nullptr)) {
                have = 0;
                goto done;
            }
        }
        have -= (size_t)(start - buf);
        memmove(buf, start, have);
        if (have >= sizeof(buf) - 1) {  // a line longer than the buffer: drop it
            static const char e[] = "err line too long\n";
            DWORD wr = 0;
            WriteFile(h, e, sizeof(e) - 1, &wr, nullptr);
            have = 0;
        }
    }
done:
    FlushFileBuffers(h);
    DisconnectNamedPipe(h);
    CloseHandle(h);
    InterlockedDecrement(&g_clients);
    return 0;
}

DWORD WINAPI PipeServer(void*) {
    bool logged = false;
    for (;;) {
        HANDLE h = CreateNamedPipeW(g_pipe, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                    4, 4096, 4096, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) {
            const DWORD e = GetLastError();
            if (!logged) VLOG("VPad: CreateNamedPipe failed (%lu) - retrying every second", e);
            logged = true;
            Sleep(1000);
            continue;
        }
        if (!ConnectNamedPipe(h, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
            CloseHandle(h);
            continue;
        }
        InterlockedIncrement(&g_clients);
        if (HANDLE t = CreateThread(nullptr, 0, ClientThread, h, 0, nullptr)) {
            CloseHandle(t);
        } else {
            DisconnectNamedPipe(h);
            CloseHandle(h);
            InterlockedDecrement(&g_clients);
        }
    }
}

// releases whatever is held when no command came for VirtualPadIdleReleaseSec (a harness that died mid-hold)
DWORD WINAPI IdleThread(void*) {
    for (;;) {
        Sleep(1000);
        if (g_idleSec <= 0) continue;
        if (NowUs() - g_lastCmdUs < (int64_t)g_idleSec * 1000000) continue;
        AcquireSRWLockExclusive(&g_lock);
        const bool any = g_comp.AnyActive();
        if (any) g_comp.ReleaseAll();
        ReleaseSRWLockExclusive(&g_lock);
        if (any) VLOG("VPad: no command for %d s - released every held virtual input", g_idleSec);
    }
}

bool StartPipe() {
    HANDLE t = CreateThread(nullptr, 0, PipeServer, nullptr, 0, nullptr);
    if (!t) return false;
    CloseHandle(t);
    if (HANDLE i = CreateThread(nullptr, 0, IdleThread, nullptr, 0, nullptr)) CloseHandle(i);
    return true;
}

}  // namespace

bool KeysWanted() { return g_mode != 0; }

bool KeyFilter(uint8_t* keys, bool readOk) {
    if (!g_mode || !keys) return readOk;
    if (!readOk || !ForegroundIsGame()) memset(keys, 0, 256);  // the user's typing in another window never reaches the game
    const int64_t now = NowUs();
    AcquireSRWLockExclusive(&g_lock);
    if (g_armed) g_comp.ReadKeys(now, keys);
    ReleaseSRWLockExclusive(&g_lock);
    return true;
}

void Start(const StartInfo& info) {
    g_info = info;
    g_log = info.log;
    if (!info.iniPath || !info.base) return;
    const int vp = (int)GetPrivateProfileIntW(L"Debug", L"VirtualPad", 0, info.iniPath);
    const int kru = (int)GetPrivateProfileIntW(L"Debug", L"KeepRunningUnfocused", 0, info.iniPath);
    g_idleSec = (int)GetPrivateProfileIntW(L"Debug", L"VirtualPadIdleReleaseSec", 600, info.iniPath);
    if (kru) {
        if (info.knownBuild) InstallUnfocused(info.base);
        else VLOG("KeepRunningUnfocused: not installed (unknown build; addresses are v2.02)");
    }
    if (vp != 1 && vp != 2) {
        if (vp) VLOG("VPad: VirtualPad = %d is not 0, 1 or 2 - off", vp);
        return;
    }
    void** slot = FindXInputSlot(info.base);
    if (!slot) {
        VLOG("VPad: no XInputGetState import in Nioh3.exe - off");
        return;
    }
    g_mode = vp;
    InterlockedExchange64(&g_lastCmdUs, NowUs());
    if (!HookSlot(slot, info.base)) {
        g_mode = 0;
        return;
    }
    InterlockedExchange(&g_armed, 1);
    if (info.pipeName) wcsncpy_s(g_pipe, info.pipeName, _TRUNCATE);
    const bool pipe = StartPipe();
    VLOG("VPad: installed (VirtualPad = %d, %s): the game's XInputGetState (IAT Nioh3.exe+%llX) answers slot 0 from the pipe %ls%s; "
         "idle release after %d s. Commands: %s", vp,
         vp == 1 ? "exclusive - the real pads are hidden from the game this session" : "merge - real pads still work",
         (unsigned long long)((uint8_t*)slot - info.base), g_pipe, pipe ? "" : " (PIPE THREAD FAILED)", g_idleSec, HelpText());
}

// ---- tests ----
bool TestHookSlot(void** slot, uint8_t* hostBase, int mode) {
    g_mode = mode;
    InterlockedExchange(&g_armed, 1);
    InterlockedExchange64(&g_lastCmdUs, NowUs());
    return HookSlot(slot, hostBase);
}
int TestExec(const char* line, char* reply, size_t n) { return Exec(line, reply, n); }
uint8_t* TestBuildThunk(void* isActive) {
    uint8_t* t = (uint8_t*)VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (t) BuildThunk(t, (uint64_t)isActive);
    return t;
}
void TestSetHost(const StartInfo& info) {
    g_info = info;
    g_log = info.log;
}
bool TestStartPipe(const wchar_t* name) {
    wcsncpy_s(g_pipe, name, _TRUNCATE);
    return StartPipe();
}

}  // namespace vpad
