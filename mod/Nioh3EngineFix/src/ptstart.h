// ptstart.h - PTS: present-thread start guard (crash report #86, same crash 2026-11-06 Ally crash file #5918).
//
// Game bug (v2.02): the renderer init (2EE9AF5) creates and resumes the Present thread BEFORE it creates the thread's
// wake event (CreateEventW at 2EE9B44, stored to [dev+7720h] at 2EE9B4A). The thread (1A82298) reads [dev+7720h] once
// at 1A822A1; if it runs first it reads 0, WaitForSingleObject(NULL) fails at once, and the loop presents the
// swapchain at [dev+7730h] = 0 -> 0xC0000005 at Nioh3+1A822CC ("Fatal error!" popup 1-2 s into the boot).
// Seen twice, both on the ROG Ally under Proton (few cores, so the new thread often runs before the creator).
//
// Fix: the 7-byte load at 1A822A1 becomes a call to a small stub that waits (Sleep(1) per round) until the handle is
// non-zero, then loads it exactly as the original did. Once the handle exists the stub costs one compare, once per
// thread start. Written at boot from the decryption wait loop (before the renderer starts), known build only.
// [Patches] PTS = 1 (default); 0 = stock.
#pragma once
#include <cstdint>

namespace ptstart {

using LogFn = void (*)(const char* fmt, ...);

constexpr uint32_t kSiteRva = 0x1A822A1;
constexpr uint32_t kCtxRva = 0x1A82298;  // the thread proc; the context check covers 1A82298..1A822BF
constexpr uint32_t kCtxOffset = kSiteRva - kCtxRva;

// [Patches] PTS (default 1).
bool BootWanted(const wchar_t* iniPath);

// Known build: checks the code around base+kSiteRva and writes the guard. Returns true while still pending (code not
// decrypted yet); false once written or given up (a reason is logged).
bool BootTry(uintptr_t base, LogFn log);

// The writer itself (also used by the offline harness): `ctx` points at the thread proc's first byte (the 39-byte
// context pattern must match). Returns true when the guard was written.
bool Apply(uint8_t* ctx, LogFn log);

// true once a guard was written in this process
bool Applied();

// stub rounds that found the event handle still 0 (each one Sleep(1)); > 0 = the race happened and was held off
long WaitRounds();

}  // namespace ptstart
