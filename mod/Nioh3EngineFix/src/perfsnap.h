// perfsnap.h - performance snapshot for every player report and crash report (human #97631/#97632, TASK #97638).
//
// What it carries (JSON "perf" in the crash record and the player report; "-- performance --" in the crash / snapshot text):
//   last ~10 s: average fps, 1 % low, mean / p99 / max frame time; CPU-bound / GPU-bound / capped verdict and why
//   (perfsnap_core.h Classify); CPU model + logical cores; GPU model, PCI ids, driver, VRAM total / used / budget; RAM total /
//   used; back buffer size; the game's own settings that matter for speed (SCR_REZ, REN_RAT, REN_FPS, RESOLUTION_SCALING,
//   FRAME_GENERATION, quality keys; decoded from the engine's settings table on v2.02 only); the mod's performance sections
//   ([Perf] [ExtraPerf] [VRR] [Latency] and every [Patches] key set to 0); the top frame-cost parts already measured (game thread
//   CPU, GPU fence wait, present wait, DS retry, the engine's own back-end waits, the GP pass line when [Profiler] GPUProf is on).
//
// Per-thread CPU (#97673): busiest thread + top 5 over a rolling 30 s, % of one core, by name (GetThreadDescription, else the
// start module+offset; the presenting thread is marked "present"). Own lowest-priority thread, 1 tick/s, nothing per frame.
//
// Cost: no new hook. osd's existing Present hook calls OnGamePresent (game thread only): one float into a 4096-frame ring, and
// every 250 ms one counter sample (GetThreadTimes + GetProcessTimes + the existing wait counters) into a 64-slot ring. No frames
// = nothing runs. The GPU name / driver / VRAM adapter is opened once by a background thread 20 s after start; VRAM / RAM use
// is refreshed by that thread every 5 s only while frames are arriving. The crash path reads only these static buffers.
#pragma once
#include <cstddef>
#include <cstdint>

namespace perfsnap {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;              // Nioh3.exe image base
    bool knownBuild = false;         // v2.02: the settings table may be decoded
    // the mod's existing counters (dllmain); nullptr = not shown
    volatile long* l1 = nullptr;
    volatile long long *w6Ticks = nullptr, *gfTicks = nullptr, *dsTicks = nullptr;  // QPC ticks
    bool (*backendWaitsMs)(double out[4]) = nullptr;  // bkstats::CumWaitsMs
    const char* (*gpuPasses)(char* buf, size_t n) = nullptr;  // gp::StatusLine
    bool (*menuOpen)() = nullptr;    // menu::IsOpen: the frame window freezes when the menu opens (#98365, TASK #98471)
};
void Start(const StartInfo& info);  // never blocks

// osd Hooked(): outermost non-test Present on the game's presenting thread (not the FSR presenter). now = QPC.
void OnGamePresent(int64_t now);
// osd draw path: back buffer size of the drawing swapchain (one store when it changes).
void NoteBackBuffer(unsigned w, unsigned h);

// #98471: one freeze (hitchlog's HITCH line), from the game's presenting thread. Every freeze enters the whole-session totals
// (count / ms / max per cause, never dropped); the newest 64 keep their details; the thread sampler names the busiest threads
// over each gap within ~1 s. Cheap: one short exclusive lock, no heap, no syscall.
struct HitchIn;  // perfsnap_core.h
void NoteHitch(const HitchIn& h);
void SetHitchThresholdMs(int ms);  // hitchlog Start (HitchLog on); never called = "hitch_log":"off" in the report

// JSON object (no key, starts with '{'). crashPath = true: static data only (no DXGI, no ini, no heap). Returns length.
size_t Json(char* out, size_t cap, bool crashPath);
// Same content as readable lines ("  avg fps ..."), for the crash / snapshot text report.
size_t Text(char* out, size_t cap, bool crashPath);

// tests: feed fixed frames / samples / hardware strings instead of the live sources
void ResetForTest(const StartInfo& info);  // like Start without the worker thread; clears the rings
void SetHardwareForTest(const char* cpu, int cores, const char* gpu, const char* ids, const char* driver, uint64_t vramTotalMb,
                        uint64_t vramUsedMb, uint64_t vramBudgetMb, uint64_t ramTotalMb, uint64_t ramUsedMb);
void ThreadTickForTest();  // one per-thread CPU tick now (the live worker runs one per second)
void RefreshModForTest();                  // the worker's 30 s mod_settings re-read, now (#97850)
const char* ThreadNameForTest(unsigned long tid);  // cached per-thread name ("" = not tracked) (#97817)
bool SettingsTableFoundForTest(uintptr_t base, uintptr_t end);  // #98375 hash-free table gate
int HitchKeptForTest();  // detailed records held (<= 64)
const char* HitchThreadsForTest(int newest);  // busiest-thread text of the newest-but-`newest` record ("" = not filled yet)
void SetSettingsForTest(const char* gameJson, const char* modJson, int fpsCap, const char* upscaler, const char* frameGen);

}  // namespace perfsnap
