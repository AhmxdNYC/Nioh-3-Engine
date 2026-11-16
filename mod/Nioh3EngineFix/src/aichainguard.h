// aichainguard.h - AiChainGuard ([General] AiChainGuard = 1 by default; GitHub #95 / #30). Crash guard for the game's boss-AI
// chain-row copy.
//
// The boss AI's chain walker (3103B8, called from 480A74 via state+0xE8) copies the rows of an action's runtime slice
// (action+0x50 list, +0x58 start, +0x5A count) into a local vector with the generic vector<ptr>::assign(first, last) at 310698.
// In #95 (1.8.21) and #30 (1.8.6), same install, same boss action 0xFE8, the slice said 190 rows but the list's memory ended
// after 114 (the next page is unmapped), so the copy loop faulted reading [rdx] at 31074A.
//
// The guard is a vectored exception handler that touches nothing on the normal path. It acts only on a read access violation
// AT 31074A whose fault address is the loop's source pointer (rdx) and whose code bytes were verified at start. It resumes at
// the loop's own exit (3106E3: `mov [rdi+8], rcx` + epilogue), exactly as if the source range had ended there: the vector
// holds the rows that were readable, the walker picks from those. Any other fault is passed on (crash report unchanged).
// Logs "AICHAINGUARD: ..." (rate-limited) whenever it fires.
#pragma once
#include <cstdint>

namespace aichainguard {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] AiChainGuard (default 1)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // other builds: function found by pattern (Grva) and still byte-verified
};

constexpr uint32_t kFnRva = 0x310698;    // v2.02 vector<ptr>::assign(first, last) (rvas.json)
constexpr uint32_t kSiteOff = 0xB2;      // 31074A `mov rax, [rdx]` - the faulting read
constexpr uint32_t kExitOff = 0x4B;      // 3106E3 `mov [rdi+8], rcx` - the loop's normal exit
constexpr uint32_t kLoopOff = 0xA0;      // 310738 loop + tail (no rel32 inside), verified byte for byte
constexpr uint32_t kLoopLen = 0x22;
constexpr uint32_t kHeadLen = 0x42;      // prologue .. first call (310698..3106DA), verified byte for byte

// Verifies the bytes and adds the handler. Call after the code is decrypted.
void Start(const StartInfo& info);
// Faults caught since start (0 when not installed).
long Hits();
// True once the handler is added.
bool Installed();

}  // namespace aichainguard
