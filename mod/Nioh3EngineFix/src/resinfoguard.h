// resinfoguard.h - ResInfoGuard ([General] ResInfoGuard = 1 by default; GitHub #145).
//
// Resource-info header reader 2E02E4 reads 0x38 bytes and requires
//   sectionBytes == 0x38 + dataBytes + paramHeaderBytes + paramDataBytes
// (cmp rdx, rcx at 2E0381). A mismatch jumps to cold block 191235C, which calls the shared
// assert logger 18CBDAC. That logger ends in a shipping int3 at 18CBDF7 (0x80000003) and only
// then returns. The reader would then do `xor al, al` (1912378) and every caller already
// treats al == 0 as "header rejected" (the same path a short read takes, which never calls
// the assert). #145 died on that int3 during a level load (EngineFix 1.8.24, stack
// 18CBDF7 <- 1912378 <- 42BA7).
//
// The guard retargets the mismatch branch at 2E0384 from the assert call to that `xor al, al`,
// so a bad header is rejected instead of closing the game. The success path (mov al, 1) is
// unchanged. Bytes on both sides are checked first; a different build or a different site is
// left alone.
#pragma once
#include <cstdint>

namespace resinfoguard {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] ResInfoGuard (default 1)
    uintptr_t base = 0;
    bool knownBuild = false;
};

constexpr uint32_t kJne = 0x2E0384;  // v2.02 jne to the assert cold block (rvas.json)

void Start(const StartInfo& info);
bool Installed();

}  // namespace resinfoguard
