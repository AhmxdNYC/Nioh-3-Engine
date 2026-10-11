// jt.h - JT: job-timing / logic-join probe of Nioh3EngineFix (1.7.40). [Profiler] JT = 0 by default. MEASUREMENT ONLY: the
// game's behaviour is unchanged (every hooked call runs exactly as before, with two QPC reads around it).
//
// What it answers (notes\whats-left.md #4, notes\gq-jt-probes.md): per logic tick, where KIDSTaskScheduler0 (the logic
// thread) blocks in a condition-variable wait (the 1.44 ms/frame NtWaitForAlertByThreadId "join" of the PC profiles), which
// thread released it and what that thread was running (the critical job), every job's start / end / thread, the longest
// job overlapping the wait, how busy the other job threads were meanwhile, and the ParallelFor leaf / grain mix.
//
// Hooks (patches.json group JT, jt.asm; written by dllmain's frozen ApplyBatch only when JT = 1):
//   JTc  40E4E   JobWorkerPoll's job call (every KIDS job pool): QPC before / after, job + function + inner callback
//   JTw  B616B7  STL condition_variable wait (SleepConditionVariableSRW, INFINITE)   } only the watched thread
//   JTx  B616F7  the same, wait_for                                                  } leaves the fast path
//   JTn1 B61674  notify_one  (WakeConditionVariable)       } fast path unless the CV is the one the watched thread
//   JTn2 B6165C  notify_all  (WakeAllConditionVariable)    } sleeps on right now: then the waker is recorded
// Log: "JT: ..." lines every JT_ReportSec (default [Profiler] ReportSec, 30 s). Format: notes\gq-jt-probes.md.
#pragma once
#include <cstdint>

namespace jt {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uint8_t* base = nullptr;   // Nioh3.exe
};

// dllmain, inside expGroup("JT") after the sites were found and verified: reads the IAT slots the caves tail-jump through
// (from the site bytes: FF 15 disp32 at siteW / siteX, 48 FF 25 disp32 at siteN1 / siteN2) and checks them. false = group off.
bool Prepare(const StartInfo& info, uint8_t* siteW, uint8_t* siteX, uint8_t* siteN1, uint8_t* siteN2);
// after the group was written: starts the analysis / report thread (finds the watched thread by its description)
void Start();

// tests\jt_harness.cpp: run without Nioh3.exe. exeLo / exeHi = the module whose RVAs are reported; tickBase = a fake image
// with the engine's tick counter pointer at +4759910h; kindNode / kindPfor = RVAs treated as JobNodeExecute / ParallelFor
void TestSetup(void (*log)(const char*, ...), uintptr_t exeLo, uintptr_t exeHi, uint8_t* tickBase, void** iatSleep, void** iatWake,
               void** iatWakeAll, const wchar_t* watched, uint32_t kindNode, uint32_t kindPfor);
void TestReport();   // analyse everything recorded so far and log one window

}  // namespace jt
