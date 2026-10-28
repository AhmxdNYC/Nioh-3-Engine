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
}  // namespace hudhide
