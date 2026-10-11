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
// to a near thunk that touches only al and the flags (rcx/rdx/r8-r11 and every other register are preserved):
//   cmp byte [switch], 0 ; je original ; mov al, 1 ; lock inc dword [count] ; ret ; original: jmp [283950]
// So with the switch on the page closes on the first frame the press would be accepted; the other two conditions (and
// the game's own reasons to hold the page) still apply. With the switch off the thunk is a plain jump to the original.
// v2.02 only; installed whenever the build is known (the menu row switches it live), bytes verified first.
#pragma once
#include <cstdint>

namespace autocont {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] AutoContinueLoad (default 0)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // v2.02 only
};

// Verifies and installs the call-site thunk (known build only). Returns the live switch (one byte in the thunk page:
// 0 = stock, 1 = auto-continue), or nullptr when not installed.
volatile uint8_t* Start(const StartInfo& info);
// Number of times the tip page's confirm check was answered "pressed" by the switch (0 when not installed).
long Count();

}  // namespace autocont
