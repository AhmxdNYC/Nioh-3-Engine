// hitchlog.h - [Debug] HitchLog (default 1, measurement only): one 'HITCH' log line per game-thread frame gap over HitchLogMs.
//
// Called from osd.cpp's outermost Present hook on the game's presenting thread (never the FSR presenter). Per frame it reads
// three process/thread counters (GetThreadTimes, GetProcessMemoryInfo, GetProcessIoCounters) and swaps the per-stage wait
// accumulators; only when the gap is over the threshold does it read the rest (RAM, VRAM, mod/Training state, loader queues)
// and write the line. The wait sites in dllmain.cpp (W3 MsgWait, L1 limiter, F1 FSR wait, W6 present wait, GF fence wait,
// DS retry-spin sleep) report through NoteWait (last + longest single wait only; counts and totals come from the existing
// counters in StartInfo::Counters, the engine's own back-end wait stats via bkstats::HitchWaits and the PJ PSO counters).
// Summary: tools\hitch_from_log.py Nioh3EngineFix.log
#pragma once
#include <windows.h>
#include <cstdint>

namespace hitchlog {

// the mod's existing wait counters (dllmain stats line / menu); read per frame, logged as deltas. nullptr = not shown.
struct Counters {
    volatile long *w3 = nullptr, *l1 = nullptr, *f1 = nullptr, *w6 = nullptr, *gf = nullptr, *ds = nullptr;
    volatile long long *w6Ticks = nullptr, *gfTicks = nullptr, *dsTicks = nullptr;  // QPC ticks
};
struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    Counters c;
};

// reads [Debug] HitchLog / HitchLogMs; starts the delayed VRAM adapter lookup. Call before osd::Start.
void Start(const StartInfo& si);
bool Enabled();  // osd installs its Present hook when this is on

enum Stage : int { kNone = 0, kW3, kL1, kF1, kW6, kGF, kDS, kNumStages };

extern volatile long g_on;

inline int64_t Now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
void NoteWaitSlow(int stage, int64_t t0, int64_t t1);
// a wait at an instrumented site ended (t0/t1 from Now()); no-op when off
inline void NoteWait(int stage, int64_t t0, int64_t t1) {
    if (g_on) NoteWaitSlow(stage, t0, t1);
}

// osd.cpp Hooked(): outermost, non-test Present on a non-FSR thread. now = QPC.
void OnGamePresent(int64_t now);

}  // namespace hitchlog
