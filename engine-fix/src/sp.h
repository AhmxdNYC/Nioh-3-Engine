// sp.h - in-process sampling profiler for Nioh3EngineFix ([Profiler] SP = 1). Measurement only: never changes the game.
//
// Why: the ROG Ally (Proton) has no perf, and /proc sampling only shows "running" or "in ioctl" without telling WHICH game
// code is hot. SP samples the game's own threads from inside the process and writes the hottest Nioh3.exe functions to
// the log every ReportSec seconds:
//   - [App]Game (the main/logic thread) every IntervalUs (default 1000 us);
//   - with Workers = 1, one engine worker per tick, round-robin (KIDS* job workers, SynergisticDispatcherTask, other
//     [App]* threads), so the total stays at ~2 suspends per tick.
// Each sample: SuspendThread -> GetThreadContext -> copy StackBytes of its stack with ReadProcessMemory -> ResumeThread.
// Nothing that could take a user-mode lock (heap, loader) runs while the target is suspended. The analysis runs after
// ResumeThread: RIP inside Nioh3.exe is attributed to its .pdata function (fragment) start; RIP anywhere else (ntdll,
// win32u, kernelbase, a driver, Wine's unix side) is attributed to the first return address on the copied stack that
// points into Nioh3.exe's code right after a call instruction, i.e. the game function that called into the system.
// So a Sleep(0)/SwitchToThread/WaitFor* spin shows up as "[ntdll.dll] <- fn 0x7D9E90".
#pragma once
#include <cstdint>

namespace sp {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Profiler] SP, IntervalUs, Workers, ReportSec, TopN, StackBytes, StartDelaySec
    uintptr_t base = 0;                // Nioh3.exe image base
};

// Reads [Profiler]; with SP = 0 returns at once and does nothing. Otherwise starts the sampler thread.
void Start(const StartInfo& info);
// 1.7.36 Tour (tour.cpp): with SetExternalWindows(true) the sampler only samples between BeginWindow() and EndWindow(tag)
// and writes one report per window ("SP window <tag>" + the usual lines) instead of every ReportSec. No-ops when SP = 0.
void SetExternalWindows(bool on);
void BeginWindow();
void EndWindow(const char* tag);

}  // namespace sp
