// thrsched.h - thread-scheduling census (TSTATE, measurement only) and RTP render-thread priority (1.7.57, both default
// off). notes\cpu-bound-midrange.md.
//
// Why: on a 6-core CPU (pc2: Ryzen 5 5600X) with Resizable BAR the GPU needs 19 ms per frame but frames take ~26 ms. The
// process keeps ~10 of 12 logical processors busy (sysmon 0.9-1.05 k %), mostly the logic thread and its job workers
// ticking at 120-240 logic ticks/s while only ~35 frames/s are rendered. The render path ([App]Game draw prep,
// SynergisticDispatcherTask replay, the Present thread) then competes for CPU with work nobody sees. SP shows WHERE a
// thread's wall time goes but not whether it was running or runnable-and-waiting-for-a-CPU; this module does.
//
// TSTATE ([Profiler] ThreadState = 1): a sampler thread (HIGHEST priority, pinned to the last logical processor so the
// one thread it preempts per snapshot is always on that CPU) takes a kernel snapshot of all threads
// (NtQuerySystemInformation(SystemProcessInformation)) every ThreadStateUs (default 10000) and counts, per game thread,
// Running / Ready (runnable, no CPU = CPU contention) / Waiting (by wait reason: sleep, object/alert, suspended, other).
// Every ThreadStateSec (default = [Profiler] ReportSec) it logs "TSTATE:" lines: system and process CPU, the critical
// render/logic threads by role, the busiest other threads, and per-group sums (workers, effect threads, ...). CPU % is
// exact (kernel+user time deltas from the same snapshots); state shares are sampled.
//
// RTP ([Patches] RTP = 0 | 1 | 2, live menu row "Render threads first"): raises the scheduling priority of the threads
// on the render critical path - [App]Game (draw prep), SynergisticDispatcherTask (D3D12 replay), the thread that calls
// the game's Present, and an ExecuteCommandLists thread when it has a name - to [Tuning] RTP_Level (1 =
// ABOVE_NORMAL, 2 = HIGHEST). Same game code, same output, only the OS's choice of who runs first when every core is
// busy: the logic / job workers keep ticking on the remaining CPU time (VRR keeps the game speed, as on a slower CPU).
// 0 = nothing, 1 = installed + ON, 2 = installed, switch OFF. OFF restores each thread's previous priority.
#pragma once
#include <cstdint>

namespace thrsched {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;                          // Nioh3.exe image base (thread start addresses are logged as RVAs)
    unsigned long (*presentTid)() = nullptr;     // osd: the thread that calls the game swapchain's Present (0 = not seen)
};
// Reads [Profiler] ThreadState / [Patches] RTP; starts the census thread and / or the RTP maintenance thread.
void Start(const StartInfo& info);
// RTP live switch (nullptr when RTP = 0).
volatile uint32_t* RtpSwitch();
// One status line for the 30 s stats log ("" when RTP is not installed).
void RtpStatusLine(char* buf, unsigned n);

}  // namespace thrsched
