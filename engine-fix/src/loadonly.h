// loadonly.h - LO loads-only gating ([Patches] LoadOnlyCpuFixes = 0 by default; notes\loading-ally-fixed.md 8.5).
//
// With LoadOnlyCpuFixes = 1 (v2.02 only) the CPU-power fixes GF, W6 and RWb are installed even when their own ini key is 0,
// but their runtime words follow the user's setting outside loads. A 10 ms gate thread watches the level-load window byte
// [g_GameMainPtr]+58Ah: on the rising edge it saves the three words and sets them to 1; on the falling edge it restores the
// saved values (so GF/W6 menu choices and RWb = 1 "always" survive). In gameplay with GF/W6/RWb = 0 the game runs stock code
// paths (GF/W6 fall back to the stock Sleep(0); the RWb cave runs the original instruction order).
// RWb gets a switchable cave instead of its 22-byte rewrite: only the first instruction at 1A3146A (mov edx,[rbp+3A8h],
// 6 bytes) becomes `jmp cave; nop`, written with one interlocked compare-exchange. The cave (kRwbCave in loadonly.cpp,
// proven against both orders by tests\test_patterns.py "LO") does:
//   word == 0: mov edx,[rbp+3A8h]; jmp 1A31470                      (stock: wake every spin)
//   word != 0: inc ebx; test bl,bl; jne 1A31486                      (RWb: skip wake + Sleep(0) unless bl wrapped)
//              dec ebx; mov edx,[rbp+3A8h]; jmp 1A31470               (wrapped: the stock tail wakes, re-incs, Sleep(0))
// so the JobWakeOneInPool call stays in the game's code (no relocated call, unwinding unchanged).
#pragma once
#include <cstdint>

namespace loadonly {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
    volatile uint32_t* gf = nullptr;  // g_gfEnable when GF is installed, else nullptr
    volatile uint32_t* w6 = nullptr;  // g_w6Enable when W6 is installed, else nullptr
    uint32_t rwbUser = 0;             // [Patches] RWb as written by the user (1 = also outside loads)
};

// Installs the RWb cave and starts the gate thread. False when nothing could be gated.
bool Start(const StartInfo& info);

}  // namespace loadonly
