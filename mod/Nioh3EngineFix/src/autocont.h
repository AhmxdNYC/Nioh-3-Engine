// autocont.h - AutoContinueLoad ([General] AutoContinueLoad = 0 by default; notes\auto-continue-load.md).
// Skips the "press button to continue" wait of the post-load tip page: the moment the game marks the load done and would
// accept the confirm press, the page's own confirm check reads "pressed" and the page closes through the game's own close
// path (the same calls a real press makes). Nothing else changes.
//
// The tip page is UI menu slot 0x2B ([g_UiManager 45C84A8]+158h, vtable 4015EC8). Its per-frame tick TipPageLoadDoneTick
// 1CECB24 returns at once until the load-done byte [page+71Bh] is set (only the mission/load sequencer sets it, state 0Bh
// 223BE08 -> TipPageSetLoadDone 1D010E4, after LoadIsLoadEnd 3B3424 has passed), then calls TipPageConfirmCheck 1CECAE4:
//   if (ConfirmPressed 283950 && !GameMain+3Fh 28393C && !SystemMessageQueued 27FBC44) { close page; return 1; }
// The one patched byte range is that check's first call, E8 at 1CECAED -> ConfirmPressed 283950 (patches.json AC1), retargeted
// to a near thunk. It preserves RCX/RDX/R8-R11 and XMM0-5 across a try-only
// covered-Fight permission callback and has registered stack-unwind metadata.
// The persistent user switch still independently enables the existing behavior.
// With it off, only a fresh, coherent Loading-phase cover permission can answer;
// denial restores incoming registers and tail-jumps to the original confirm leaf.
// v2.02 only; installed whenever the build is known (the menu row switches it live), bytes verified first.
#pragma once
#include <cstdint>

namespace autocont {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] AutoContinueLoad (default 0)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool (*coverPermission)() = nullptr; // try-only, checked at each native tip predicate
    bool knownBuild = false;           // v2.02 only
};

// Verifies and installs the call-site thunk (known build only). Returns the live switch (one byte in the thunk page:
// 0 = stock, 1 = auto-continue), or nullptr when not installed.
volatile uint8_t* Start(const StartInfo& info);
// Number of times the tip page's confirm check was answered "pressed" by the switch (0 when not installed).
long Count();

}  // namespace autocont
