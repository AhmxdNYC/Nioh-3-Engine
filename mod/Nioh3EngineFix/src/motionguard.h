// motionguard.h - MotionGuard ([General] MotionGuard = 1 by default; GitHub #143 / #144 / #103). Crash guard for two game
// animation (motion) reads whose data is missing or already freed.
//
// A. Root-motion track lookup 48A150 (called from 489F40, the player's root-motion step, via 48A364 / 5F990C / 28E038):
//    its first read `movzx r11d, word [rcx+8]` (48A164) faulted in #144 with rcx = a motion track pointer into memory that
//    was already released (MEM_FREE), 4 s after a level load. The function's own "no track found" exit (48A1C7: bl = 0,
//    epilogue) is what the caller 489F40 already handles for a null track (it returns 0: no root motion this frame).
// B. Motion key walk A0BA8 (called from A0AE0 <- A0958 / 9F580 <- 35B024, the character motion update): an entry of the key
//    list whose data pointer (entry+0x28) is null. #143 (and #103, another PC on 1.8.12) faulted at A0C2A
//    `movss xmm3, [r8+8]` with r8 = 0, 19 s after a level load. The caller A0AE0 itself returns 0 when the entry it picks has
//    entry+0x28 == 0, and both its callers treat 0 as "motion not found"; the guard takes A0BA8's own empty-list exit
//    (A0C95: eax = 0, epilogue). The same null read in the multi-key loop (cold chunk 1930AFC, found through the jne at
//    A0C12) is covered at its three `[r9+8]` reads.
//
// The guard is a vectored exception handler that touches nothing on the normal path. It acts only on a read access violation
// AT one of those instructions whose fault address is exactly that instruction's base register + 8, with the code bytes
// verified at start (rip-relative displacements and the cross-chunk branches are not compared). Any other fault is passed on
// (crash report unchanged). Logs "MOTIONGUARD: ..." (rate-limited) whenever it fires.
#pragma once
#include <cstdint>

namespace motionguard {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [General] MotionGuard (default 1)
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // other builds: functions found by pattern (Grva) and still byte-verified
};

constexpr uint32_t kFnA = 0x48A150;     // v2.02 root-motion track lookup (rvas.json)
constexpr uint32_t kSiteA = 0x14;       // 48A164 movzx r11d, word [rcx+8]
constexpr uint32_t kExitA = 0x77;       // 48A1C7 xor bl, bl ; jmp epilogue (return 0)
constexpr uint32_t kHeadALen = 0x7B;    // 48A150..48A1CB, verified byte for byte
constexpr uint32_t kEpiAOff = 0xB7;     // 48A207..48A21E epilogue, verified byte for byte
constexpr uint32_t kEpiALen = 0x17;

constexpr uint32_t kFnB = 0xA0BA8;      // v2.02 motion key walk (rvas.json)
constexpr uint32_t kSiteB = 0x82;       // A0C2A movss xmm3, [r8+8]
constexpr uint32_t kExitB = 0xED;       // A0C95 xor eax, eax ; jmp epilogue (return 0)
constexpr uint32_t kBodyBLen = 0xF6;    // A0BA8..A0C9E, verified (3 rel32/disp32 masked)
constexpr uint32_t kJneB = 0x6A;        // A0C12 jne rel32 -> cold chunk (1930AFC on v2.02)
constexpr uint32_t kChunkLen = 0xF9;    // 1930AFC..1930BF5, verified (4 rel32/disp32 masked)
constexpr uint32_t kChunkSites[3] = {0x37, 0x8F, 0xC4};  // 1930B33 / 1930B8B / 1930BC0 movss xmm0, [r9+8]

// Verifies the bytes and adds the handler. Call after the code is decrypted.
void Start(const StartInfo& info);
// Faults caught since start (0 when not installed): all sites, A only, B (main + chunk) only.
long Hits();
long HitsA();
long HitsB();
// True once the handler is added.
bool Installed();

}  // namespace motionguard
