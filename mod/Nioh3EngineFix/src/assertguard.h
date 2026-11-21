// assertguard.h - AssertGuard ([General] AssertGuard = 1 by default; GitHub #153 / #145 / #53).
//
// The game's shared assert logger 18CBDAC (19 call sites in v2.02) takes a lock, formats the message into a 0x200-byte
// buffer (47090B0), hits a leftover debug break `int3` at 18CBDF7, then unlocks and returns. Every call site has its own
// recovery code after the call: #153's site (cold block 1984892 of the resource-create step 3154F8) first marks the
// resource database "create failed" (324E4F4 sets bit 0x80000000 at +0xC8), logs
// "CResourceDatabase::DeqCreate CreateResource failed. [nResFileID:%d]", then does Sleep(1) and returns normally. With no
// debugger attached that int3 is an unhandled 0x80000003 and the game closes, so the recovery never runs.
//
// The guard is a vectored exception handler that acts only on EXCEPTION_BREAKPOINT at exactly that int3 (logger bytes
// verified at start) and resumes at the next instruction, i.e. the logger finishes the way the game's own code intends.
// It copies the game's assert message so the log says what the game complained about
// ("ASSERTGUARD: game assert ... - continued"). Any other breakpoint or fault is passed on (crash report unchanged).
// ResInfoGuard (#145) stays: it avoids that one assert entirely.
#pragma once
#include <cstdint>

namespace assertguard {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] AssertGuard (default 1)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // other builds: logger found by pattern (Grva) and still byte-verified
};

constexpr uint32_t kLogger = 0x18CBDAC;  // v2.02 shared assert logger (rvas.json)
constexpr uint32_t kInt3 = 0x4B;         // 18CBDF7 int3 (the debug break)
constexpr uint32_t kBufLea = 0x3F;       // 18CBDEB lea rcx, [rip+disp] -> message buffer (0x200 bytes)
constexpr uint32_t kBufSize = 0x200;

void Start(const StartInfo& info);
bool Installed();
long Hits();
uintptr_t Site();  // absolute address of the int3 once installed (0 otherwise)

}  // namespace assertguard
