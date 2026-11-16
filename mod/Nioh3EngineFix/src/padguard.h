// padguard.h - PadListGuard ([General] PadListGuard = 1 by default; GitHub #79 / #55). Crash guard for the game's pad manager:
// a list walk in the per-frame pad update that reads a null `next` link (another thread is relinking the list) ends that walk
// early, as if the list ended there, instead of faulting on [0+10h]. Same frame result as a list that ended; the next frame
// walks the list again. Non-null nodes run the original code path unchanged. Sites + cave: padguard_cave.h.
// Logs "PADGUARD: null list link at +42FB3E (hit N)" (rate-limited) whenever the guard fires.
#pragma once
#include <cstdint>

namespace padguard {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] PadListGuard (default 1)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // other builds: sites found by pattern (Grva) and still byte-verified
};

// Verifies every site, builds the caves and writes the site jumps with all other threads suspended. Call after the code is
// decrypted. Each site installs only when its own bytes match.
void Start(const StartInfo& info);
// Total null links caught since start (0 when not installed).
long Hits();
// #96: pad-walk at() misses answered with a zeroed value since start (0 when not installed).
long MapHits();

}  // namespace padguard
