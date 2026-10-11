// padlat.h - PadLatency ([Debug] PadLatency = 0 by default, 1.7.35): controller input -> Present / GPU-done latency,
// measured inside the game. MEASUREMENT ONLY: the game reads the same pad states at the same time as without it.
//   * XInputGetState: the game's IAT slot (XINPUT1_4 ordinal 2, the only pad API it imports besides the Sony HID path)
//     is chained like the mod menu does it. Each call is timed (QPC before/after); a change of buttons, a trigger past
//     the XInput threshold or a stick crossing its deadzone / changing octant is stamped when the game has it in hand.
//     Calls more than 2 ms apart start a new poll round: the round-to-round interval = the game's own input sampling period.
//   * Frame hand-off: the TS probe ring (caves.asm lat_ring, ids C/L/W/P; PadLatency switches the TS probes on):
//     change -> next pad-consumed C -> limiter exit L -> render begin W -> present hand-off P -> next hooked Present.
//   * GPU done: osd's Present hook calls OnPresent() for the drawing swapchain; we Signal our own fence on the game's
//     queue right before that Present (one extra Signal per frame) and a waiter thread stamps each completion.
// Log every PadLatencyReportSec (default [Profiler] ReportSec): "PadLatency <n>s: ..." (window) + "PadLatency total: ...".
// Not covered: DualShock/DualSense through the game's own HID reader threads (no XInput call), keyboard/mouse (PresentMon
// measures those). See notes\pad-latency.md.
#pragma once
#include <cstdint>

struct ID3D12CommandQueue;

namespace padlat {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uint8_t* base = nullptr;                    // Nioh3.exe image base
    bool knownBuild = false;
    // TS probe ring (caves.asm); nullptr when the TS group is not installed (no frame hand-off then)
    const volatile int64_t* tsRing = nullptr;   // 4096 x {QPC, id}
    const volatile uint64_t* tsRingIdx = nullptr;
    const volatile uint32_t* tsOn = nullptr;    // lat_ts (live switch)
};

// [Debug] PadLatency from the ini (dllmain reads it before the patch groups: PadLatency turns the TS probes on)
bool Wanted(const wchar_t* iniPath);
// Hooks XInputGetState and starts the report thread when PadLatency != 0. Returns at once.
void Start(const StartInfo& info);
// osd: PadLatency needs the Present hook even with the overlay and the menu off
bool WantsPresentHook();
// osd Present-hook thread (drawing swapchain, under the osd lock), right before the original Present: q = the game's queue
void OnPresent(ID3D12CommandQueue* q, int64_t qpcNow);

// harness: install the XInputGetState hook on an arbitrary IAT-like slot (hostBase = module that must NOT own the
// target) and read the tracker counters. Not used by the game path.
bool TestHookSlot(void** slot, uint8_t* hostBase);
void TestCounters(uint64_t* reads, uint64_t* changes);

}  // namespace padlat
