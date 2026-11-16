// aichainguard.cpp - AiChainGuard ([General] AiChainGuard = 1 by default; GitHub #95 / #30). See aichainguard.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamerva.h"

#include <cstdint>
#include <cstring>

#include "aichainguard.h"

namespace aichainguard {
namespace {

// v2.02 bytes (dump 310698..31075A); the two rel32 calls (3106DA memmove, 310718 reserve) are not compared.
constexpr uint8_t kHead[kHeadLen] = {
    0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x10, 0x48, 0x89, 0x70, 0x18, 0x48, 0x89, 0x78, 0x20, 0x41, 0x56, 0x48,
    0x83, 0xEC, 0x20, 0x48, 0x8B, 0x31, 0x48, 0x8B, 0xF9, 0x48, 0x8B, 0x49, 0x08, 0x4D, 0x8B, 0xF0, 0x4C, 0x2B, 0xC2, 0x48, 0x2B, 0xCE,
    0x49, 0x8B, 0xD8, 0x48, 0xC1, 0xF9, 0x03, 0x48, 0xC1, 0xFB, 0x03, 0x48, 0x8B, 0xEA, 0x48, 0x3B, 0xD9, 0x77, 0x2B, 0x48, 0x8B, 0xCE};
// 3106DF..310702: exit block (lea ; mov [rdi+8],rcx ; restore ; add rsp ; pop r14 ; ret)
constexpr uint8_t kExitBlk[] = {0x48, 0x8D, 0x0C, 0xDE, 0x48, 0x89, 0x4F, 0x08, 0x48, 0x8B, 0x5C, 0x24, 0x30, 0x48, 0x8B, 0x6C, 0x24, 0x38,
                                0x48, 0x8B, 0x74, 0x24, 0x40, 0x48, 0x8B, 0x7C, 0x24, 0x48, 0x48, 0x83, 0xC4, 0x20, 0x41, 0x5E, 0xC3};
constexpr uint32_t kExitBlkOff = 0x47;
// 310738..31075A: mov rcx,[rdi+8] ; lea rdx,[rbx+rbp] ; loop: cmp rdx,r14 ; je exit ; test rcx,rcx ; je +6 ;
//                 mov rax,[rdx] ; mov [rcx],rax ; add rcx,8 ; add rdx,8 ; jmp loop
constexpr uint8_t kLoop[kLoopLen] = {0x48, 0x8B, 0x4F, 0x08, 0x48, 0x8D, 0x14, 0x2B, 0x49, 0x3B, 0xD6, 0x74, 0x9E, 0x48, 0x85, 0xC9, 0x74,
                                     0x06, 0x48, 0x8B, 0x02, 0x48, 0x89, 0x01, 0x48, 0x83, 0xC1, 0x08, 0x48, 0x83, 0xC2, 0x08, 0xEB, 0xE6};

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0, g_site = 0, g_exit = 0;
PVOID g_veh = nullptr;
volatile long g_hits = 0;
volatile LONG64 g_lastCopied = 0, g_lastSkipped = 0;

LONG CALLBACK Veh(EXCEPTION_POINTERS* ep) {
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    CONTEXT* c = ep->ContextRecord;
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || c->Rip != g_site) return EXCEPTION_CONTINUE_SEARCH;
    // a read of exactly the source pointer, still inside the source range [rdx, r14)
    if (er->NumberParameters < 2 || er->ExceptionInformation[0] != 0 || er->ExceptionInformation[1] != c->Rdx ||
        c->Rdx >= c->R14 || ((c->R14 - c->Rdx) & 7))
        return EXCEPTION_CONTINUE_SEARCH;
    g_lastCopied = (LONG64)((c->Rdx - c->Rbp) / 8);  // rbp = first (source begin) for the whole loop
    g_lastSkipped = (LONG64)((c->R14 - c->Rdx) / 8);
    InterlockedIncrement(&g_hits);
    c->Rip = g_exit;  // the loop's own exit: [rdi+8] = rcx (end of what was copied), epilogue, ret
    return EXCEPTION_CONTINUE_EXECUTION;
}

// One line per hit up to the 20th, then at most one line every 10 s (with the total). No logging inside the handler.
DWORD WINAPI Watch(void*) {
    long seen = 0, lines = 0;
    ULONGLONG last = 0;
    for (;;) {
        Sleep(100);
        const long n = g_hits;
        if (n == seen) continue;
        const ULONGLONG now = GetTickCount64();
        if (lines >= 20 && now - last < 10000) continue;
        seen = n;
        last = now;
        ++lines;
        g_log("AICHAINGUARD: boss AI chain list ran into unreadable memory at +%X - copy ended after %lld rows, %lld rows "
              "skipped (hit %ld; #95/#30 crash avoided)%s",
              (unsigned)(g_site - g_base), (long long)g_lastCopied, (long long)g_lastSkipped, n,
              lines == 20 ? " - further hits: one line per 10 s" : "");
    }
}

}  // namespace

long Hits() { return g_hits; }
bool Installed() { return g_veh != nullptr; }

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath || g_veh) return;
    const int want = (int)GetPrivateProfileIntW(L"General", L"AiChainGuard", 1, info.iniPath);
    if (want == 0) {
        g_log("AICHAINGUARD: off ([General] AiChainGuard = 0) - the boss AI chain copy is stock");
        return;
    }
    const uintptr_t fn = info.base + Grva(kFnRva);  // Grva = 0 on another build when the pattern is missing / not unique
    if (fn == info.base || memcmp((const void*)fn, kHead, sizeof kHead) != 0 ||
        memcmp((const void*)(fn + kExitBlkOff), kExitBlk, sizeof kExitBlk) != 0 ||
        memcmp((const void*)(fn + kLoopOff), kLoop, sizeof kLoop) != 0) {
        g_log("AICHAINGUARD: not installed (boss AI chain copy bytes differ - another patch or build)");
        return;
    }
    g_base = info.base;
    g_site = fn + kSiteOff;
    g_exit = fn + kExitOff;
    g_veh = AddVectoredExceptionHandler(1, &Veh);
    if (!g_veh) {
        g_log("AICHAINGUARD: not installed (AddVectoredExceptionHandler failed)");
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr)) CloseHandle(h);
    g_log("AICHAINGUARD: installed at +%X (%s) - [General] AiChainGuard = %d (#95/#30: a boss AI chain list that runs into "
          "unreadable memory ends the copy at the last readable row instead of crashing)",
          (unsigned)(g_site - info.base), info.knownBuild ? "v2.02" : "found by pattern", want);
}

}  // namespace aichainguard
