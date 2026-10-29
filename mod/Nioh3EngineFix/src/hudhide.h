// hudhide.h - [Menu] HideGameHud (#44871): hide / restore the game's gameplay HUD (health / ki / stamina bars, minimap, item and
// ninjutsu shortcuts, currency, boss gauge, ...) from the mod menu. Default 0 = HUD shown, the game is untouched.
//
// How (RE + live test on pc2, 2026-10-28): every HUD widget's "should I show" virtual (e.g. the player gauge's at RVA 0x95746C:
// menu open? && own visible byte [w+0x43B] && F0228()) calls one shared check at RVA 0xF0228:
//     mov rax,[UiMgr 0x45C84A8] ; mov rcx,[rax+18h] ; test rcx,rcx ; je ; mov al,[rcx+3C0h] ; ret
// 24 callers, all HUD widgets. Making it return 0 hides the HUD; the game's own menus (pause, shrine, map) do not use it.
// The interact prompt ("E Pray") is drawn by its own widget and stays.
// The patch swaps the first 8 bytes atomically (the function start is 8-aligned), so a thread inside it never sees a torn
// instruction: 31 C0 C3 = xor eax,eax ; ret. The bytes are checked first; any other build leaves the game untouched.
#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>

namespace hudhide {
inline int g_want = 0;     // 1 = hide (menu / ini)
inline int g_applied = 0;  // what is in the game code now
inline int g_bad = 0;      // bytes did not match: never patch
constexpr uintptr_t kRva = 0xF0228;
constexpr uint8_t kOrig[23] = {0x48, 0x8B, 0x05, 0x79, 0x82, 0x4D, 0x04, 0x48, 0x8B, 0x48, 0x18, 0x48,
                               0x85, 0xC9, 0x74, 0x07, 0x8A, 0x81, 0xC0, 0x03, 0x00, 0x00, 0xC3};

// Call once per frame (present): applies g_want when it differs from what is installed. Returns false if the build does not match.
inline bool Sync() {
    if (g_bad || g_want == g_applied) return !g_bad;
    uint8_t* p = (uint8_t*)GetModuleHandleW(nullptr) + kRva;
    __try {
        if (memcmp(p + 3, kOrig + 3, sizeof(kOrig) - 3) != 0 || (memcmp(p, kOrig, 3) != 0 && !(p[0] == 0x31 && p[1] == 0xC0 && p[2] == 0xC3))) {
            g_bad = 1;
            return false;
        }
        uint64_t v;
        memcpy(&v, p, 8);
        uint8_t nb[8];
        memcpy(nb, &v, 8);
        if (g_want) { nb[0] = 0x31; nb[1] = 0xC0; nb[2] = 0xC3; }
        else memcpy(nb, kOrig, 3);
        uint64_t nv;
        memcpy(&nv, nb, 8);
        DWORD old = 0, t = 0;
        if (!VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) { g_bad = 1; return false; }
        InterlockedExchange64((volatile LONG64*)p, (LONG64)nv);
        VirtualProtect(p, 8, old, &t);
        FlushInstructionCache(GetCurrentProcess(), p, 8);
        g_applied = g_want;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_bad = 1;
        return false;
    }
    return true;
}

// ---- Per-element hide (#44877) ---------------------------------------------------------------------------------------
// Every UI widget's IsVisible (vtable+0x40) runs RVA 0xF2D8C: parent visible? (recursive) && F3444(UiMgr, id = vt+0x128()).
// The one call to F3444 at RVA 0xF2DCB is retargeted to a small thunk: if bit `id` of a 256-bit mask is set, return 0
// (hidden), else tail-jump to F3444 unchanged. Hiding id 3 (the HUD parent) hides every child (4,5,10,13,17-20,23,24,33,35-38).
// The call is redirected only while some bit is set; with no bits set the original call bytes are put back.
// Live ids (pc2 2026-10-28, bridge #44958): 4 style, 5 player gauge (HP + ki + 3rd bar are one widget), 10 stance,
// 11 item shortcut, 13 guardian spirit, 17 amrita, 18 money, 21 boss gauge, 25 minimap; found by hiding ids one at a time
// in game (pc2 #44877): 6 enemy health bars over enemies, 12 lock-on marker. Damage numbers / interact prompt ids not found yet.
struct Elem { const char* key; uint8_t ids[4]; };
inline const Elem kElems[] = {
    {"HideHudPlayerBars", {5}},  {"HideHudStyle", {4}},  {"HideHudStance", {10}},   {"HideHudItems", {11}},
    {"HideHudGuardian", {13}},   {"HideHudAmrita", {17}}, {"HideHudMoney", {18}},   {"HideHudBossBar", {21}},
    {"HideHudMinimap", {25}},    {"HideHudEnemyBars", {6}}, {"HideHudLockOn", {12}},
};
constexpr int kElemN = (int)(sizeof(kElems) / sizeof(kElems[0]));
inline int ElemIndex(const char* key) {
    for (int i = 0; i < kElemN; ++i) if (key && !strcmp(kElems[i].key, key)) return i;
    return -1;
}
inline int g_elemOn[kElemN] = {};
inline uint32_t g_extra[8] = {};      // [Menu] HideHudIds = "7, 22": raw widget ids (advanced / finding new ids)
inline uint8_t* g_page = nullptr;     // thunk page; mask = 8 dwords at g_page + 0x100
inline int g_idsBad = 0;
inline int g_redirected = 0;
constexpr uintptr_t kCallRva = 0xF2DCB, kTargetRva = 0xF3444;
constexpr uint8_t kCallSite[12] = {0x8B, 0xD0, 0x48, 0x8B, 0xCF, 0xE8, 0x74, 0x06, 0x00, 0x00, 0xEB, 0x02};  // at 0xF2DC6
constexpr uint8_t kTarget[9] = {0x8B, 0x81, 0xD0, 0x07, 0x00, 0x00, 0x83, 0xF8, 0xFF};
inline uint32_t* Mask() { return g_page ? (uint32_t*)(g_page + 0x100) : nullptr; }

inline void ParseIds(const char* s) {
    memset(g_extra, 0, sizeof(g_extra));
    for (const char* p = s; p && *p;) {
        if (*p >= '0' && *p <= '9') {
            int v = 0;
            while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); ++p; if (v > 999) break; }
            if (v < 256) g_extra[v >> 5] |= 1u << (v & 31);
        } else ++p;
    }
}

inline uint8_t* AllocIn(uintptr_t from, uintptr_t to) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + 4096 <= re && c + 4096 <= to)
                if (void* p = VirtualAlloc((void*)c, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}

// Builds the thunk once (call from the present thread). False = wrong game build or no near memory: never redirect.
inline bool BuildThunk() {
    if (g_page) return true;
    if (g_idsBad) return false;
    uint8_t* b = (uint8_t*)GetModuleHandleW(nullptr);
    __try {
        if (memcmp(b + kCallRva - 5, kCallSite, sizeof(kCallSite)) != 0 || memcmp(b + kTargetRva, kTarget, sizeof(kTarget)) != 0) {
            g_idsBad = 1;
            return false;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_idsBad = 1; return false; }
    const auto* nt = (const IMAGE_NT_HEADERS*)(b + ((const IMAGE_DOS_HEADER*)b)->e_lfanew);
    const uintptr_t lo = (uintptr_t)b, hi = lo + nt->OptionalHeader.SizeOfImage, reach = 0x7FF00000ull - (hi - lo);
    uint8_t* p = AllocIn(hi, hi + reach);
    if (!p) p = AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
    if (!p) { g_idsBad = 1; return false; }
    const uint64_t mask = (uint64_t)(p + 0x100), target = (uint64_t)(b + kTargetRva);
    uint8_t c[64] = {
        0x81, 0xFA, 0xFF, 0x00, 0x00, 0x00,  // cmp edx, 255
        0x77, 0x12,                          // ja pass
        0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0,  // mov rax, mask
        0x0F, 0xA3, 0x10,                    // bt [rax], edx
        0x73, 0x03,                          // jnc pass
        0x31, 0xC0,                          // xor eax, eax  (hidden)
        0xC3,                                // ret
        0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,  // pass: jmp [rip] -> F3444 (the original check)
        0, 0, 0, 0, 0, 0, 0, 0};
    memcpy(c + 10, &mask, 8);
    memcpy(c + 32, &target, 8);
    memset(p, 0xCC, 4096);
    memcpy(p, c, 40);
    memset(p + 0x100, 0, 32);
    FlushInstructionCache(GetCurrentProcess(), p, 4096);
    g_page = p;
    return true;
}

// Rewrites only the call's rel32 (RVA 0xF2DCC, 4-aligned): one interlocked 32-bit store, never a torn instruction.
inline bool Redirect(bool on) {
    uint8_t* b = (uint8_t*)GetModuleHandleW(nullptr);
    uint8_t* r = b + kCallRva + 1;
    const int32_t orig = (int32_t)(kTargetRva - (kCallRva + 5));
    const int32_t thunk = (int32_t)((intptr_t)g_page - (intptr_t)(b + kCallRva + 5));
    DWORD old = 0, t = 0;
    if (!VirtualProtect(r, 4, PAGE_EXECUTE_READWRITE, &old)) return false;
    InterlockedExchange((volatile LONG*)r, on ? thunk : orig);
    VirtualProtect(r, 4, old, &t);
    FlushInstructionCache(GetCurrentProcess(), r, 4);
    g_redirected = on ? 1 : 0;
    return true;
}

// Call once per frame (present). Writes the wanted mask into the thunk page and redirects / restores the call.
inline void SyncIds() {
    uint32_t want[8];
    memcpy(want, g_extra, sizeof(want));
    bool any = false;
    for (int i = 0; i < kElemN; ++i)
        if (g_elemOn[i])
            for (uint8_t id : kElems[i].ids) if (id) want[id >> 5] |= 1u << (id & 31);
    for (uint32_t w : want) any |= w != 0;
    static uint32_t s_last[8] = {};
    if (!any && !g_redirected) return;
    if (!memcmp(want, s_last, sizeof(want)) && any == (g_redirected != 0)) return;  // unchanged: leave the mask as it is
    if (!BuildThunk()) return;
    uint32_t* m = Mask();
    for (int i = 0; i < 8; ++i) InterlockedExchange((volatile LONG*)&m[i], (LONG)want[i]);
    memcpy(s_last, want, sizeof(want));
    if (any != (g_redirected != 0)) Redirect(any);
}
}  // namespace hudhide
