// drawprep.h - two main-thread ([App]Game) fixes from notes\remaining-work.md 2.5, both default off (1.7.32).
//
// DQ  ([Patches] DQ = 0, [Tuning] DQ_QuantumUs = 200): the main loop 1A74130 retries the frame function after a blind
//     busy wait `while ((QPC - start) / freq < 0.001f) ;` (1A74BD0..1A74C48) when the logic side's frame packet is not
//     new yet. The wait never re-checks readiness, so a packet that is ready 0.1 ms in still waits the full 1 ms.
//     Measured on the PC (captures\cpuprof-r6/r7/r9 + bench-*.csv): 0.08-0.96 ms/frame of [App]Game in that QPC spin
//     (median ~0.6), plus 0.1-0.3 ms loop self. DQ shortens the quantum to DQ_QuantumUs (notes\mainthread-drawprep.md D).
//     The xmm6 load at 1A744E4 runs once before the loop, so the cave replaces the loop's exit test (1A74C45) instead.
//     With DS on as well, DsQpc sleeps only inside the shorter quantum (it asks DqQuantumUs()).
// P1E7 ([Patches] P1E7 = 0): software prefetch in the per-texture loop of 1E7050 (notes\meshlet-mainthread.md P1E7).
//     Measured target: 1E7050 + 128EF0 + 1290B8 self = 2.5-5.3 % of [App]Game = 0.28-0.46 ms/frame on the PC. Round 9
//     ran it only together with C419 + P4AA and that bundle measured slower (mesh-on 96.0/104.9 vs off 103.6/108.4 fps),
//     so it needs its own A/B before it can be KEEP.
// W1B ([Patches] W1B = 0, 1.7.42): glyph padding clear 61B1C, site 61BAC (W1's in-place bytes or the original). On = the
//     original loop with its write-combined `and [m],0` turned into `mov [m],0`: no WC read and no per-row `rep stosd`
//     start-up (pad = 1). Off = what the site held (W1 or the original). notes\w1b-rpa.md
// RPA ([Patches] RPA = 0, 1.7.42): RenderParamsAssign 247950 (29.6 KB of dword moves) -> 21 exact byte-range copies
//     derived from the original code; padding untouched. notes\w1b-rpa.md
// Value per key: 0 = not installed, 1 = installed + on, 2 = installed + off (live switch, for A/B in one launch).
// v2.02 only; every site and dependency is byte-checked first (patches.json groups DQ / P1E7 / W1B / RPA, tests\test_patterns.py).
#pragma once
#include <cstdint>

namespace drawprep {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};

// Reads the ini, verifies and installs the requested caves. Call after the DS group was applied (DS checks the bytes
// DQ overwrites as its dependency).
void Start(const StartInfo& info);
// DQ quantum in microseconds while DQ is installed and switched on, else 0 (DsQpc uses it).
int64_t DqQuantumUs();
// Live switches (nullptr when not installed): 0 = stock, 1 = on.
volatile uint32_t* DqSwitch();
volatile uint32_t* P1e7Switch();
volatile uint32_t* W1bSwitch();
volatile uint32_t* RpaSwitch();
// One status line for the 30 s stats log ("" when nothing is installed).
void StatusLine(char* buf, unsigned n);

}  // namespace drawprep
