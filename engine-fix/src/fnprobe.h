// fnprobe.h - FnProbe ([Debug] FnProbe = 0 by default): live probe of named-later game functions (notes\remaining-work.md 10.1).
// MEASUREMENT ONLY, never changes what the game does. Per function in [Debug] FnProbeList (comma RVAs; empty = the 14 last
// Unknown_ functions of remaining-work 10.1): total hits, hits/s per report window and the peak 1 s, first/last hit wall time,
// the thread of the first hit, the top 4 return addresses (callers; RVA, resolved to names offline by
// tools\playsession\summarize.py from re\symbols\symbols.tsv), and for the first FnProbeArgs hits the 4 integer argument
// registers + the first 4 dwords behind each argument that points to readable memory, with hash-looking 32-bit values marked.
// Log: "FnProbe ..." lines every FnProbeReportSec (default [Profiler] ReportSec = 30).
// Hook: function-entry thunk (fnprobe_core.h). Fast path = one locked add; the C++ part runs only for the first FnProbeArgs
// hits and then 1 in FnProbeSample hits (callers). Every volatile register incl. xmm0-5 and the flags is preserved.
// Installed with all other threads suspended and none of them inside a patched prologue. v2.02 only for the default list
// (bytes verified, patches.json group FP); a custom RVA is accepted only if its prologue decodes to the plain forms in
// fnprobe_core.h and lies in an executable section of Nioh3.exe.
#pragma once
#include <cstdint>

namespace fnprobe {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;
    uintptr_t base = 0;       // Nioh3.exe image base
    bool knownBuild = false;  // v2.02 only
};

// Reads [Debug] FnProbe*. When FnProbe != 0 and the build is known: installs the hooks and starts the report thread.
bool Start(const StartInfo& info);

}  // namespace fnprobe
