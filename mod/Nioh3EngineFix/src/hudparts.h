// hudparts.h - per-element "Hide ..." switches for the game's gameplay HUD (next to the master [Menu] HideGameHud in hudhide.h).
// Research: notes\hud-toggle-research.md (branch pc/hud-toggle, static RE; the byte offsets below are for the v2.02 build).
// The game's UI manager [Nioh3.exe+45C84A8] holds one widget pointer per HUD slot; UI::Set*HUDVisible stores a byte in the widget.
// While a switch is on we write 0 to that byte from the Present thread and give the game back the value it last wanted when it
// goes off. No code is patched. All default 0 (shown). Only active when the master's byte check says this is the known build.
#pragma once
#include <windows.h>
#include <stdint.h>
#include <string.h>

namespace hudparts {
constexpr int kNum = 7;
constexpr uintptr_t kUiMgr = 0x45C84A8;
struct Slot { uint32_t widget, flag; const char* key; };
inline const Slot kSlots[kNum] = {
    {0x28, 0x43B, "HideHudHealthKi"}, {0x58, 0x425, "HideHudShortcuts"}, {0x20, 0x3C5, "HideHudStyle"}, {0x50, 0x407, "HideHudStance"},
    {0x68, 0x3DA, "HideHudGuardian"}, {0x88, 0x3F2, "HideHudAmrita"},     {0x90, 0x3D8, "HideHudMoney"},
};
inline int g_want[kNum] = {};
inline uintptr_t g_widget[kNum] = {};
inline uint8_t g_gameVal[kNum] = {};
inline bool g_forcing[kNum] = {};

inline int Find(const char* key) {
    for (int e = 0; e < kNum; ++e) if (!strcmp(kSlots[e].key, key)) return e;
    return -1;
}
inline bool Any() { for (int e = 0; e < kNum; ++e) if (g_want[e] || g_forcing[e]) return true; return false; }

// Present thread, every frame. knownBuild = hudhide's byte check of the game version passed (never touch memory otherwise).
inline void Tick(bool knownBuild) {
    if (!knownBuild || !Any()) return;
    const uint8_t* base = (const uint8_t*)GetModuleHandleW(nullptr);
    __try {
        const uintptr_t mgr = *(const uintptr_t*)(base + kUiMgr);
        if (!mgr) return;
        for (int e = 0; e < kNum; ++e) {
            const uintptr_t w = *(const uintptr_t*)(mgr + kSlots[e].widget);
            if (!w) continue;
            if (w != g_widget[e]) { g_widget[e] = w; g_forcing[e] = false; }  // new widget (level load): forget the old one
            uint8_t* p = (uint8_t*)(w + kSlots[e].flag);
            const uint8_t v = *p;
            if (g_want[e]) {
                if (!g_forcing[e]) { g_gameVal[e] = v; g_forcing[e] = true; }
                else if (v) g_gameVal[e] = v;  // the game switched it on again: that is what it wants now
                if (v) *p = 0;
            } else if (g_forcing[e]) {
                *p = g_gameVal[e];
                g_forcing[e] = false;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        for (int e = 0; e < kNum; ++e) { g_want[e] = 0; g_forcing[e] = false; }
    }
}
}  // namespace hudparts
