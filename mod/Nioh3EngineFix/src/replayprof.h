// replayprof.h - RPF: replay-thread profiler (1.7.59-rpf1, [Profiler] ReplayProf = 0 by default). MEASUREMENT ONLY.
// notes\renderer\replay-profile.md (design, overhead, results).
//
// What it measures, per frame, on SynergisticDispatcherTask (the single KTGL token replay thread, notes\renderer\backend.md):
//   * every token handler of the 185-entry table KtglTokenHandlerTable 3C02220: calls, inclusive and self time (a handler
//     that replays a sub-range through the table, 115750 / 495EFC, gets its children subtracted);
//   * every D3D12 method the replay thread calls on command lists, the device, queues, fences and resources (the D3D12
//     runtime classes found through dummy objects, plus the engine's own list / device / queue objects when they have a
//     different vtable, e.g. a Streamline proxy), per method and per (handler, call group): set state, root signature /
//     arguments, PSO / heaps, barriers, draw / dispatch, copy / clear, queries / markers, list close / reset, descriptor
//     copies, view creation, other device, ExecuteCommandLists, queue sync, fence, Map / Unmap, other resource, COM;
//   * blocking inside handlers: WaitForSingleObject(Ex), Sleep(Ex), SwitchToThread called through Nioh3.exe's imports;
//   * idle: the dispatcher loop's own WaitForSingleObject (1A7C3FC, the semaphore main's kicks release).
// How: the token table entries point at per-token thunks (replayprof.asm) -> RpTok (two TSC reads around the original
// handler); D3D12 vtable slots point at per-slot thunks that time the call only on the replay thread (other threads: a TEB
// read + compare + jump); four KERNEL32 import slots of Nioh3.exe point at wrappers (same thread filter). No code bytes are
// written. Nothing changes what the game does: every original is called with the caller's exact arguments.
// Log: "RPF:" lines every ReplayProfSec (default 10 s): replay-thread time split, top-N handlers, call groups, top methods.
// Live: LiveCtl 'Tuning.RPF = 0/1' removes / re-installs every hook (0 = the game runs exactly as without RPF), for the
// on/off overhead A/B.
#pragma once
#include <cstddef>
#include <cstdint>

namespace replayprof {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};
// Reads [Profiler] ReplayProf*; with ReplayProf = 1 / 2 starts the install + report thread.
void Start(const StartInfo& info);
// LiveCtl 'Tuning.RPF = 0/1'. true = key handled.
bool SetTuning(const char* key, const char* val, char* reply, size_t n);
// true when `slotValue` (a vtable slot) is an RPF thunk that calls `want` (gpuopt's watch thread: RPF chained in front of
// its thunk is not a slot restore, batching stays on).
bool Forwards(const void* slotValue, const void* want);

}  // namespace replayprof
