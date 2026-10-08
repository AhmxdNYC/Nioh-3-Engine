// prefetch.h - PF trace prefetch ([Patches] PF = 0 by default; v2.02 only). Design + Ally data:
// notes\loading-ally-fixed.md 8.8.
//
// What it attacks: the two read sites that are still device-bound after EP / SR on the Ally (1.7.23 R1, EP on):
//   - ReadFileEx 631FD5 (ResourceLoad pool, 12.6k calls, 2.7 GB, avg 740-810 us, 9.3-10.2 thread-s). The RIS resource job
//     429E4 opens the entry's fdata archive per resource (CreateFileA 631F20, GENERIC_READ, share READ, OVERLAPPED), reads
//     the IDRK entry through stream vtable 38EE660 slot +40h (631C48 -> 631F70 -> ReadFileEx, completion 631C04 sets
//     req+34h = 1) and polls it at once through slot +48h (716280: SleepEx(0, TRUE), then cmpxchg req+34h 1 -> 0). Under
//     Wine the read is done inside ReadFileEx, so the first poll always succeeds: the ResourceLoad thread sits out the whole
//     device read (cold random reads; first-touch 4K ~526 us in the 1.7.23 calibration).
//   - FileReadAsyncSubmit 31305A / FileReadOverlappedIssue 3130D6 (archive layer, 64 KiB chunk reads, far 82 %, 3.2-3.6 s).
// Neither can be made faster per call (the bytes have to come off the device); PF makes them page-cache hits.
//
// How: the read hooks readprobe.cpp already owns at those 3 sites call Note() before the real call. Note() only looks:
//   file id (NtQueryInformationFile FileInternalInformation: robust to the per-resource handle reuse), path once per file
//   (must be inside <game>\package\: immutable archives), offset from the OVERLAPPED, size. Inside a load window
//   (G+58Ah set) every such read is recorded. At the clear the list is written to Nioh3EngineFix_pf<k>.bin next to the ini.
//   At the next load the first 24 package reads pick the stored trace that contains them (>= PFMatchPct %); PFThreads
//   workers then read that trace's ranges in recorded order (merged into runs of <= 8 MiB, gaps <= 256 KiB) on their own
//   read-only synchronous handles into a scratch buffer, at most PFLeadMB ahead of the game's progress (the furthest run
//   the game has asked for). Effect: the game's own reads find the bytes in the OS page cache.
// Exactness: the game's calls, arguments, buffers, results, errors, events, APCs and handles are untouched (Note() keeps
//   the last-error value). PF only adds reads of package files on separate handles (share READ|WRITE|DELETE, so no open
//   of the game can fail because of them). A wrong or stale trace costs device time only: the replay stops when fewer
//   than 25 % of the game's reads are in the trace.
// Modes: PF = 1 record + prefetch, PF = 2 record only (the B arm of an A/B, so both arms pay the same hook cost and the
//   trace stays fresh). Windows is allowed (it only warms the system cache).
#pragma once
#include <cstdint>

namespace prefetch {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
};

// True when [Patches] PF > 0 (readprobe then installs its R1 read hooks and feeds Note()).
bool Wants(const wchar_t* iniPath);
// Reads the ini, finds <game>\package\, starts the 58A gate thread. True = Note() is live.
bool Start(const StartInfo& info);
// Called by readprobe's hooks at 631FD5 (site 13), 31305A (2) and 3130D6 (3) BEFORE the real call. Observation only;
// keeps GetLastError(). ov may be null (ignored).
void Note(int site, void* h, const void* ov, unsigned long n);

}  // namespace prefetch
