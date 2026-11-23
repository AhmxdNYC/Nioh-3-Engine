// motionguard.cpp - MotionGuard ([General] MotionGuard = 1 by default; GitHub #143 / #144 / #103). See motionguard.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamerva.h"

#include <cstdint>
#include <cstring>

#include "motionguard.h"
#include "motionguard_bytes.h"

namespace motionguard {
namespace {

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
uintptr_t g_siteA = 0, g_exitA = 0;
uintptr_t g_siteB = 0, g_exitB = 0;
uintptr_t g_chunkSite[3] = {};
PVOID g_veh = nullptr;
volatile long g_hits = 0, g_hitsA = 0, g_hitsB = 0;
volatile LONG64 g_lastRip = 0, g_lastAddr = 0;

bool SameMasked(const uint8_t* code, const uint8_t* want, size_t n, const uint32_t* mask, size_t nMask) {
    for (size_t i = 0; i < n; ++i) {
        bool skip = false;
        for (size_t m = 0; m < nMask; ++m)
            if (i >= mask[m] && i < mask[m] + 4) skip = true;
        if (!skip && code[i] != want[i]) return false;
    }
    return true;
}

int32_t Rel32(uintptr_t at) {
    int32_t r;
    memcpy(&r, (const void*)at, 4);
    return r;
}

LONG CALLBACK Veh(EXCEPTION_POINTERS* ep) {
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || er->NumberParameters < 2 || er->ExceptionInformation[0] != 0)
        return EXCEPTION_CONTINUE_SEARCH;  // only reads
    CONTEXT* c = ep->ContextRecord;
    const uintptr_t rip = (uintptr_t)c->Rip, addr = (uintptr_t)er->ExceptionInformation[1];
    if (rip == g_siteA) {
        if (addr != c->Rcx + 8) return EXCEPTION_CONTINUE_SEARCH;  // movzx r11d, word [rcx+8]
        InterlockedIncrement(&g_hitsA);
        c->Rip = g_exitA;  // xor bl, bl ; epilogue: "no root-motion track" (rbx/rbp/rsi/rdi are restored from the frame)
    } else if (rip == g_siteB) {
        if (addr != c->R8 + 8) return EXCEPTION_CONTINUE_SEARCH;  // movss xmm3, [r8+8]
        InterlockedIncrement(&g_hitsB);
        c->Rip = g_exitB;  // xor eax, eax ; epilogue: the empty-list result
    } else if (rip && (rip == g_chunkSite[0] || rip == g_chunkSite[1] || rip == g_chunkSite[2])) {
        if (addr != c->R9 + 8) return EXCEPTION_CONTINUE_SEARCH;  // movss xmm0, [r9+8]
        InterlockedIncrement(&g_hitsB);
        c->Rip = g_exitB;
    } else {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    g_lastRip = (LONG64)rip;
    g_lastAddr = (LONG64)addr;
    InterlockedIncrement(&g_hits);
    return EXCEPTION_CONTINUE_EXECUTION;
}

// One line per hit up to the 20th, then at most one line every 10 s (with the totals). No logging inside the handler.
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
        const uintptr_t rip = (uintptr_t)g_lastRip;
        g_log("MOTIONGUARD: %s at +%X read %llX (missing or freed motion data) - treated as 'no motion data' (hit %ld: root "
              "track %ld, key list %ld; #143/#144 crash avoided)%s",
              rip == g_siteA ? "root-motion track" : "motion key list", (unsigned)(rip - g_base),
              (unsigned long long)g_lastAddr, n, (long)g_hitsA, (long)g_hitsB,
              lines == 20 ? " - further hits: one line per 10 s" : "");
    }
}

}  // namespace

long Hits() { return g_hits; }
long HitsA() { return g_hitsA; }
long HitsB() { return g_hitsB; }
bool Installed() { return g_veh != nullptr; }

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath || g_veh) return;
    const int want = (int)GetPrivateProfileIntW(L"General", L"MotionGuard", 1, info.iniPath);
    if (want == 0) {
        g_log("MOTIONGUARD: off ([General] MotionGuard = 0) - the game's motion reads are stock");
        return;
    }
    // A: root-motion track lookup
    const uintptr_t fa = info.base + Grva(kFnA);  // Grva = 0 on another build when the pattern is missing / not unique
    const bool okA = fa != info.base && memcmp((const void*)fa, kHeadA, sizeof kHeadA) == 0 &&
                     memcmp((const void*)(fa + kEpiAOff), kEpiA, sizeof kEpiA) == 0;
    // B: motion key walk + its cold chunk (target of the jne at +6A; the chunk's two jumps back must land in this function)
    const uintptr_t fb = info.base + Grva(kFnB);
    bool okB = fb != info.base && SameMasked((const uint8_t*)fb, kBodyB, sizeof kBodyB, kBodyBMask, 3);
    uintptr_t chunk = 0;
    if (okB) {
        chunk = fb + kJneB + 6 + (intptr_t)Rel32(fb + kJneB + 2);
        okB = chunk > info.base && SameMasked((const uint8_t*)chunk, kChunkB, sizeof kChunkB, kChunkBMask, 4) &&
              chunk + 0x8F + (intptr_t)Rel32(chunk + 0x8B) == fb + 0x70 &&  // jmp A0C18
              chunk + 0xF9 + (intptr_t)Rel32(chunk + 0xF5) == fb + 0xC5;    // jmp A0C6D (epilogue)
    }
    if (!okA && !okB) {
        g_log("MOTIONGUARD: not installed (motion code bytes differ - another patch or build)");
        return;
    }
    g_base = info.base;
    if (okA) {
        g_exitA = fa + kExitA;
        g_siteA = fa + kSiteA;
    }
    if (okB) {
        g_exitB = fb + kExitB;
        for (int i = 0; i < 3; ++i) g_chunkSite[i] = chunk + kChunkSites[i];
        g_siteB = fb + kSiteB;
    }
    g_veh = AddVectoredExceptionHandler(1, &Veh);
    if (!g_veh) {
        g_siteA = g_siteB = 0;
        g_chunkSite[0] = g_chunkSite[1] = g_chunkSite[2] = 0;
        g_log("MOTIONGUARD: not installed (AddVectoredExceptionHandler failed)");
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr)) CloseHandle(h);
    g_log("MOTIONGUARD: installed (%s) - root-motion track +%X %s, motion key list +%X %s - [General] MotionGuard = %d "
          "(#143/#144: missing or freed motion data reads as 'no motion data' instead of crashing)",
          info.knownBuild ? "v2.02" : "found by pattern", okA ? (unsigned)(g_siteA - info.base) : 0u, okA ? "on" : "bytes differ",
          okB ? (unsigned)(g_siteB - info.base) : 0u, okB ? "on" : "bytes differ", want);
}

}  // namespace motionguard
