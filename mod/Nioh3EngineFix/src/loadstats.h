// loadstats.h - LoadStats: one summary per level load ([Profiler] LoadStats = 0 by default). MEASUREMENT ONLY.
// Background, phase model and the fixes it is meant to rank: notes\loading.md. Integration: loadstats_INTEGRATION.md.
//
// A "load" is the engine stop-flag window G+0x58A (set by the mission script command Pause::SetFlag 0x23A763C, the
// same window the mod log prints as "engine stop flags ... now: 58A" -> "none"). For every window LoadStats logs:
//   LoadStats #n: total s | frames, mean frame ms | process CPU-s (cores) | stage-ready time, loader-drained time,
//                 loader in-flight average and % of busy time at the in-flight cap, ready-wait time (IsLoadEnd 120 timeout
//                 hit or not), event-scene seconds
//   LoadStats #n io: MB read (archive async / sync, sync ms) | inflate CPU-s, chunks, MB in -> out, max parallel, threads
//                    | PSOs created (gfx / compute, preload vs runtime, CPU-s, max ms) | manager Sleep(1) counts
//   LoadStats #n split: the same counters for [58A set -> stage ready] and [stage ready -> 58A clear]
// LoadStats = 3 also ticks outside the window; [Profiler] LoadGates = 1 adds probe LG (gate hooks + sequencer polls, see loadstats.cpp).
// LoadStats = 2 adds one "LoadStats tick" line per second inside the window (in-flight, MB read, inflate ms, PSOs, frames).
//
// How (v2.02 only; every site's bytes are verified first, a mismatching site is skipped and named in the log):
//   - polling, no hooks: stop flags, frame counter (G+610h), stage state ([45B9E58]+760h == 3 = Load::IsStageLoadEnd),
//     IsLoadEnd's ready-wait accumulator (float 45C84A4, timeout 120 dt60 units), event-scene object (45CE800),
//     GetProcessTimes; and the two loader managers' queues (pending +200h/+210h resp. +1E0h/+1F0h, active vector
//     +230h..+238h resp. +218h..+220h, 16-byte entries), whose addresses the Sleep hooks capture.
//   - call-site hooks (E8 rel32 retargeted to a near thunk; FF 15 [IAT] replaced by E8 rel32 + NOP):
//       Sleep(1) of ResourceLoadIdlePoll 1FEE28 (1FEE68 idle, 1FF18C no progress) and of manager B 1FF320 (1FF371 idle):
//         thunk passes r14 (the manager) -> counts + manager address; then the original Sleep(ms).
//       zlib 16 KiB chunk wrappers 420654 (sites 258A5A 4201F5 420605), 324D064 (324BD61 324C409), A09A40 (A099DD
//         324C372 324C4A1): QPC time, chunks, compressed/inflated bytes, concurrent callers, distinct threads.
//       CreateGfxPso 9290D8 (11970A runtime, 2EFE47C preload), CreateComputePso 58C2B0 (119AE1 119D86 58BF92 58C07A
//         runtime, 2EFE1F2 preload): count + QPC time. 11970A is skipped when the ASI's SPR patch SPg owns it.
//       ReadFile IAT calls of the archive file layer (311D17, 312DD2 sync, 31305A async/overlapped, 3130D6) and three
//         other readers (740392, 1FD9CA3, 32562BE): bytes requested, calls, time inside the call.
//   Patches are written with one interlocked 8- or 16-byte compare-exchange; the 4 sites whose bytes straddle a 16-byte
//   boundary (1FF18C, A099DD, 2EFE47C, 32562BE) go through "EB FE first, tail, then head" instead, so a thread running
//   the code never sees a torn instruction (re\scripts_load\ls_sites.py checks both cases offline).
// Cost when on: two QPC reads per inflate chunk / PSO / read; a 100 Hz sampler thread. Off (0): nothing is installed.
#pragma once
#include <cstdint>

namespace loadstats {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Profiler] LoadStats (0 off / 1 summary / 2 summary + per-second ticks)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // v2.02 only; other builds refuse
    int loaderCap = 10;                // resource-load in-flight cap actually in force (16 when the ASI patch LC is applied)
    bool readsByProbe = false;         // 1.7.23: [Profiler] ReadProbe owns the 7 ReadFile sites and feeds NoteRead (readprobe.cpp)
};

// Reads [Profiler] LoadStats. When != 0 and the build is known: verifies and installs the hooks and starts the sampler.
// Returns false (nothing installed) when off or refused. Call once, after the game code is unpacked (next to
// loadmem::Start), before the first level load.
bool Start(const StartInfo& info);
bool Enabled();

// 1.7.23 (readprobe.cpp): ReadProbe hooks the 7 kRead sites itself (one hook per site) and reports each call here so the
// LoadStats read counters stay the same. cls 0 = archive layer (311D17 312DD2 31305A 3130D6), 1 = other (740392 1FD9CA3 32562BE).
void NoteRead(int cls, uint32_t bytes, bool overlapped, uint64_t qpcTicks);
// LoadGates: QPC of the last LoadIsLoadEnd evaluation that had a failing gate in the current/last 58A window (the "work end"
// of notes\loading-ally-fixed.md 7); 0 when LoadGates is off or no evaluation failed.
uint64_t LastFailEvalQpc();
// hitchlog.cpp: loader request managers A/B in flight (active) and pending right now; false (all -1) when LoadStats is off
// or no manager was seen yet. InflatesNow: inflate calls running right now (-1 = LoadStats off).
bool LoaderQueues(int act[2], int pend[2]);
int InflatesNow();

// Menu / OSD: "LS: idle" or "LS: load 12.3 s, A 9/10, 412 MB, PSO 300" (empty when off).
const char* StatusLine(char* buf, unsigned n);

}  // namespace loadstats
