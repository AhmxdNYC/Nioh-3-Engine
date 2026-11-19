// fhlinger.h - FileLinger ([Patches] FileLinger = 0 by default (#118013; was 1 per #117317), menu toggle; #117226): keep the game's archive file handles open
// for a while after their last user lets go, instead of CloseHandle now + CreateFileW again on the next streaming touch.
// Same bytes are read from the same files; only the open/close churn goes away. Evidence: notes/streaming-guard-117226.md.
//
// Game objects (v2.02, static + live FnProbe 2026-11-18): one entry per archive file (vtable 38F4E38, array stride 154h).
//   +08h owner (shared by all entries: +84h open-handle cap, live 64; +88h open count)   +10h OS file handle (0 = closed)
//   +2Ch users                                                                        +34h spinlock (cmpxchg 0 -> 1)
//   vt+10h 491B30 Acquire(entry, 1): spin; if no handle and open < cap -> open (315064), open++; users++ (vt+20h)
//   vt+28h 493050 Release(entry): spin; users--; at 0 -> call 751784 (site 4930B5) = close (+ wait for in-flight I/O),
//                                 open--; unlock
// Hooks (both verified byte for byte first; anything different -> not installed):
//   1. call site 4930B5 (E8 -> 751784) retargeted to a near thunk -> OnLastRelease(entry): keep the handle (entry goes in
//      a ring) while the ring has room and the open count is below cap - FileLingerFree; otherwise evict the oldest
//      lingering entry, then keep this one. Runs under the entry's own spinlock, exactly where the game closes.
//   2. vtable slot vt+10h -> AcquireWrap: before a fresh open (no handle) with the open count near the cap, evict lingering
//      entries so the game's own cap never refuses a load because of us. Then the stock 491B30.
// Eviction = try-lock the entry's spinlock (never wait), users == 0 and a handle -> the stock close 751784, unlock.
// Busy entries go back to the ring; entries in use again leave it (they come back at their next last release).
//
// Ini: [Patches] FileLinger = 0 off (nothing installed) | 1 on (default). FileLingerMax = 32 (1-60 lingering files),
//      FileLingerFree = 24 (open slots always kept free under the game's cap). Log line every 30 s: grep FileLinger.
#pragma once
#include <cstdint>

namespace fhlinger {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;
    bool knownBuild = false;
};

void Start(const StartInfo& info);

}  // namespace fhlinger
