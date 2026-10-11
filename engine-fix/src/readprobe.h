// readprobe.h - RP read probe ([Profiler] ReadProbe = 0 by default; MEASUREMENT ONLY) and EP event pool ([Patches] EP = 0
// by default; the one exact read fix of notes\loading-ally-fixed.md 8.3 row 4). Spec: notes\loading-ally-fixed.md 8.2.
//
// RP (needs LoadStats >= 1; v2.02 only). Hooks, all FF 15 [IAT] -> E8 thunk + NOP with verified bytes (patches.json RP1-RP18):
//   R1: all 13 ReadFile call sites + ReadFileEx 631FD5. Per call: site, handle (path / size / synchronous-or-overlapped
//       handle, queried once per handle per load), offset (OVERLAPPED, or the file pointer when there is none), size,
//       ok / pending / error, duration, calling thread's pool (GetThreadDescription "KIDSTS[s%d:<pool>]"), threads in
//       ReadFile at entry (global and same handle), locality per handle (reread / seq / near < 64 KiB / inblk = one of
//       the last 8 64-KiB blocks / far). At the 7 LoadStats read sites RP's hook replaces LoadStats' and feeds its counters.
//   R2: event churn: CreateEventA 311EE1 (FileReqAlloc), CloseHandle 311F89 (FileReqFree), WaitForMultipleObjects 3127A1
//       (FileReqWaitAll): calls + in-call time (WFMO also per pool).
//   R3: the game's own read cache: the E8 at 198357C -> FsReadCacheLookup 2F77A6C (calls; hits = return > 0), counted by a
//       register-neutral asm thunk (only rax/flags, as the original call).
//   Per 58A window: "ReadProbe #n" lines per site / top files / events / pools / total, plus a split at LoadGates' last
//   failing evaluation (the work end) when LoadGates = 1. Once at the title (58A clear, 120 frames in): calibration of the
//   cached-read cost (synchronous vs overlapped handle, 4 KiB and 1 B), first-touch random reads of 4/64/256 KiB on the
//   biggest archive seen, and the CreateEventA + CloseHandle pair. ReadProbe = 2 also writes every call of a window as
//   32-byte records to Nioh3EngineFix_reads_<n>.bin (+ .txt with the handle names) next to the ini (32 MB RAM ring).
// EP (independent of RP; v2.02 only): the events FileReqAlloc creates (manual-reset, non-signalled, unnamed: argument
//   bytes verified) come from a pool of up to 4096; FileReqFree's CloseHandle returns an event to the pool only when
//   NtResetEvent reports it WAS signalled (a signalled event has no read pending on it; anything else is really closed,
//   as stock). FileReqFree waits for all submitted reads (FileReqWaitAll 312754) before it closes them anyway.
// Thunks keep rcx/rdx/r8/r9/r10/r11 (bosslog.cpp style; 1.7.17 crashed on an r11 clobber) and forward the 5th stack arg.
#pragma once
#include <cstdint>

namespace readprobe {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
    bool loadStatsOn = false;  // RP needs LoadStats running (it feeds LoadStats' read counters at the 7 shared sites)
};

// True when LoadStats >= 1 and ([Profiler] ReadProbe >= 1 or readfix wants the read sites: [Patches] RC / AQ); LoadStats then
// leaves its 7 ReadFile sites to readprobe, which feeds its counters. 1.7.24: with RC / AQ on, readprobe installs its R1
// read hooks even when ReadProbe = 0 (no probe accounting then) and routes the archive sites through readfix::Read;
// readfix::Start is called from Start below and borrows the thunk page for its CreateFileW / CloseHandle hooks.
bool WantsReads(const wchar_t* iniPath);
// Installs RP and/or EP per the ini. Call after loadstats::Start. Returns true when anything was installed.
bool Start(const StartInfo& info);

}  // namespace readprobe
