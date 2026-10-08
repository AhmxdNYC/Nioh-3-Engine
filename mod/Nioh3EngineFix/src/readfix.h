// readfix.h - the RP-chosen read fixes of notes\loading-ally-fixed.md 8.3, all default off (v2.02 only):
//   RC  [Patches] RC = 0|1|2      read cache: 1 = inside level-load windows (G+58Ah set) only, 2 = always.
//       RCMB = 64 (16..256)       cache size in MB (committed while active; RC=1 decommits it 10 s after a load window).
//       RCKB = 64 (16..1024, pow2) block size in KiB.  RCMaxReadKB = 64 (<= RCKB) largest read the cache serves.
//   AQ  [Patches] AQ = 0|1|2      async archive reads (1 = load windows only, 2 = always). AQThreads = 4 (1..16).
//   SR  [Patches] SR = -1|0|1|2   synchronous redirect (-1 = auto, default since 1.7.28: 1 under Wine/Proton, 0 on Windows;
//       1 = load windows only, 2 = always; Wine/Proton only): a read at an
//       archive site on an OVERLAPPED game handle is done as a positioned synchronous ReadFile on a private synchronous
//       handle to the same file (opened once per game handle at classification, closed with it) and completed like a
//       synchronous success. Ally 1.7.23 calib: cached 4K overlapped 52-58 us vs synchronous 2 us; Wine already does the
//       overlapped file read inside the call (in-call ~= device time, pending 100 %), so nothing that was asynchronous
//       becomes blocking. Any failure passes the original call through (same error). Under Wine RC block loads and AQ
//       reads also use the private handle.
//   TSY [Patches] TSY = 0|3..8    TrySync pool threads (boot-time; next launch). Stock = 3.
//   SH  [Patches] SH = 0|1        synchronous archive handles (boot-time; Wine/Proton only - refused on Windows, where
//       a synchronous file object serializes its I/O). FileOpenWin32 314181 `and eax,200h` -> `and eax,0`: no open gets
//       FILE_FLAG_OVERLAPPED. The file object keeps mode bit 0x200, so FileReadAsyncCheck 313128 still allows async
//       submits; their ReadFile (OVERLAPPED + hEvent) then completes synchronously, returns TRUE and signals hEvent - a
//       path the callers already take (31305A / 3130D6 / 740392: TRUE = ok, then FileReqWaitAll). Excludes AQ.
//
// How it plugs in: readprobe.cpp owns the 13 ReadFile call sites (R1 hooks). When RC or AQ is on, readprobe installs its
// R1 hooks even with ReadProbe = 0 and routes the 6 archive sites (311D17, 312DD2, 31305A, 3130D6, 740392, 32562BE;
// never 1FD9CA3 = save data) through readfix::Read. readfix adds two call-site hooks of its own through readprobe's
// thunk page: FileOpenWin32's CreateFileW 314199 and FileClose's CloseHandle 313AE6 (both reset the handle's table
// entry, so a reused handle value never sees another file's blocks).
//
// Eligible handles (classified once per open): the final path is inside <game>\package\ (immutable game archives), the
// granted access has no write/append right (FileOpenWin32 mode 1 = GENERIC_READ + FILE_SHARE_READ only: no writer can
// exist while the handle is open) and the size is known. Everything else is passed to ReadFile untouched.
//
// RC serve (exact by construction; proven by tests\run_readfix_harness.py): a read of n <= RCMaxReadKB bytes at an
// explicit OVERLAPPED offset that ends inside the file is copied from the aligned block(s) holding it (at most 2). A miss
// reads the whole block with the real ReadFile (own OVERLAPPED + per-thread event + GetOverlappedResult, so it is also
// correct on Windows); other threads wanting a block that is loading wait on it (WaitOnAddress). A served read completes
// exactly like a synchronous ReadFile: TRUE, *got = n, Internal = 0, InternalHigh = n, hEvent signalled, last error
// unchanged, and on a synchronous handle the file pointer moved to offset + n. Reads past EOF, failed block reads and
// short blocks pass the original call through (same errors). CLOCK eviction of unpinned blocks.
// AQ (sites 31305A / 3130D6 / 740392, overlapped handles, hEvent != 0; their callers accept ERROR_IO_PENDING and wait
// in FileReqWaitAll 312754 = WFMO on hEvent): ResetEvent(hEvent), Internal = STATUS_PENDING, queue, return FALSE +
// ERROR_IO_PENDING. A worker reads into the game's buffer (through RC when on) with its own OVERLAPPED, copies the final
// Internal (NTSTATUS) / InternalHigh into the game's OVERLAPPED, then SetEvent(hEvent) - the Windows overlapped contract.
// A full queue (4096) or a read past EOF falls back to the real call.
#pragma once
#include <cstdint>

namespace readfix {

// readprobe.cpp's call-site hooker: FF 15 [iat] -> E8 save-thunk forwarding `stackArgs` (1..3) stack arguments.
// Returns 0 ok, 1 bytes differ, 2 no room / not atomic.
using HookCallFn = int (*)(uint32_t rva, uint32_t iat, const void* hook, int stackArgs);

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
    HookCallFn hookCall = nullptr;
};

// Effective SR mode (0..2). [Patches] SR = -1 (default, 1.7.28) = auto: 1 under Wine/Proton, 0 on native Windows.
// iniOut / wineOut (optional) receive the raw ini value and whether ntdll exports wine_get_version.
int SrMode(const wchar_t* iniPath, int* iniOut, bool* wineOut);
// True when [Patches] RC or AQ > 0 or SrMode > 0 (readprobe then installs its R1 read hooks and routes the archive sites here).
bool Wants(const wchar_t* iniPath);
// Reads the ini, allocates the cache, starts the gate / AQ threads, installs the open/close hooks. True = Read is live.
bool Start(const StartInfo& info);
// The R1 hook's call for read site `site` (readprobe's site index). Same contract as ReadFile.
int __stdcall Read(int site, void* h, void* buf, unsigned long n, unsigned long* got, void* ov);

// Boot-time part (TSY, SH), called from dllmain's decrypt-wait loop like P12/WC (known build only).
struct BootCfg {
    int tsy = 0;     // 0 off, else 3..8 TrySync threads
    bool sh = false; // synchronous archive handles (Wine only)
};
// Reads [Patches] TSY / SH. True when either is on.
bool BootWanted(const wchar_t* iniPath, BootCfg& cfg);
// One attempt per call; returns true while something is still pending. TSY writes the TrySync pool weight (data
// 4B723B8, stock 4, count u8 at 4B723B4 = 3) once the table holds its stock values; SH writes the open-flag immediate
// once the code is decrypted. wcCap = [Patches] WorkerCap when WC is on, else 0 (the pool base it implies picks the
// weight). After TSY lands a report thread logs the pool's real thread count once SizeWorkerPools has run.
bool BootTry(uintptr_t base, BootCfg& cfg, int wcCap, unsigned long logicalCpus, void (*log)(const char* fmt, ...));
// Logs what is still pending after the decrypt-wait loop and gives up on it.
void BootGiveUp(BootCfg& cfg, void (*log)(const char* fmt, ...));

}  // namespace readfix
