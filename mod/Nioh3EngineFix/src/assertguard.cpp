// assertguard.cpp - AssertGuard ([General] AssertGuard = 1 by default; GitHub #153). See assertguard.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamerva.h"

#include <cstdint>
#include <cstring>

#include "assertguard.h"

namespace assertguard {
namespace {

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
uintptr_t g_site = 0;  // the int3
const char* g_buf = nullptr;
PVOID g_veh = nullptr;
volatile long g_hits = 0;
volatile long g_msgBusy = 0;  // 0 = g_msg free, 1 = handler writing / watcher reading
volatile long g_msgHit = 0;   // hit number the copied message belongs to
volatile LONG64 g_lastRet = 0;
char g_msg[200];

// Logger 18CBDAC: save args, lock, format into the buffer, int3, unlock, ret. ?? = rip-relative disp / call target.
const int16_t kBody[] = {
    0x48, 0x8B, 0xC4, 0x48, 0x89, 0x48, 0x08, 0x48, 0x89, 0x50, 0x10, 0x4C, 0x89, 0x40, 0x18, 0x4C, 0x89, 0x48, 0x20,
    0x48, 0x83, 0xEC, 0x48, 0xBA, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x0D, -1,   -1,   -1,   -1,   0xE8, -1,   -1,
    -1,   -1,   0x4C, 0x8B, 0x44, 0x24, 0x50, 0x48, 0x8D, 0x44, 0x24, 0x58, 0x45, 0x33, 0xC9, 0x48, 0x89, 0x44, 0x24,
    0x20, 0xBA, 0x00, 0x02, 0x00, 0x00, 0x48, 0x8D, 0x0D, -1,   -1,   -1,   -1,   0xE8, -1,   -1,   -1,   -1,   0xCC,
    0xBA, 0x01, 0x00, 0x00, 0x00, 0x48, 0x8D, 0x0D, -1,   -1,   -1,   -1,   0xE8, -1,   -1,   -1,   -1,   0x48, 0x83,
    0xC4, 0x48, 0xC3};

bool Readable(const void* p, size_t n) {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    __try {
        volatile uint8_t x = 0;
        for (size_t i = 0; i < n; ++i) x = b[i];
        (void)x;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

bool LoggerMatches(const uint8_t* fn) {
    if (!Readable(fn, sizeof kBody / sizeof kBody[0])) return false;
    for (size_t i = 0; i < sizeof kBody / sizeof kBody[0]; ++i)
        if (kBody[i] >= 0 && fn[i] != static_cast<uint8_t>(kBody[i])) return false;
    return true;
}

void CopyMsg() {
    if (!g_buf || InterlockedCompareExchange(&g_msgBusy, 1, 0) != 0) return;  // watcher is reading: keep the older text
    __try {
        size_t i = 0;
        for (; i + 1 < sizeof g_msg && i < kBufSize && g_buf[i]; ++i) {
            const char ch = g_buf[i];
            g_msg[i] = (ch == '\r' || ch == '\n' || ch == '%') ? ' ' : ch;  // one log line; no format specifiers
        }
        g_msg[i] = 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_msg[0] = 0;
    }
    g_msgHit = g_hits;
    InterlockedExchange(&g_msgBusy, 0);
}

LONG CALLBACK Veh(EXCEPTION_POINTERS* ep) {
    const EXCEPTION_RECORD* er = ep->ExceptionRecord;
    if (er->ExceptionCode != EXCEPTION_BREAKPOINT || !g_site) return EXCEPTION_CONTINUE_SEARCH;
    CONTEXT* c = ep->ContextRecord;
    if (reinterpret_cast<uintptr_t>(er->ExceptionAddress) != g_site || static_cast<uintptr_t>(c->Rip) != g_site)
        return EXCEPTION_CONTINUE_SEARCH;
    InterlockedIncrement(&g_hits);
    // [rsp+0x48] = the logger's return address (sub rsp, 0x48 after the call) = which game check fired
    __try {
        g_lastRet = static_cast<LONG64>(*reinterpret_cast<const uintptr_t*>(c->Rsp + 0x48));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_lastRet = 0;
    }
    CopyMsg();
    c->Rip = g_site + 1;  // mov edx, 1 ; unlock ; ret - the rest of the game's own logger
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
        char msg[sizeof g_msg] = "";
        if (InterlockedCompareExchange(&g_msgBusy, 1, 0) == 0) {
            memcpy(msg, g_msg, sizeof msg);
            InterlockedExchange(&g_msgBusy, 0);
        }
        msg[sizeof msg - 1] = 0;
        const uintptr_t ret = static_cast<uintptr_t>(g_lastRet);
        const unsigned from = (ret > g_base && ret - g_base < 0x10000000u) ? static_cast<unsigned>(ret - g_base) : 0u;
        g_log("ASSERTGUARD: game assert from +%X \"%s\" - continued past the game's debug break at +%X (hit %ld; "
              "#153 crash 0x80000003 avoided)%s",
              from, msg, static_cast<unsigned>(g_site - g_base), n, lines == 20 ? " - further hits: one line per 10 s" : "");
    }
}

}  // namespace

bool Installed() { return g_veh != nullptr; }
long Hits() { return g_hits; }
uintptr_t Site() { return g_veh ? g_site : 0; }

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath || g_veh) return;
    const int want = static_cast<int>(GetPrivateProfileIntW(L"General", L"AssertGuard", 1, info.iniPath));
    if (want == 0) {
        g_log("ASSERTGUARD: off ([General] AssertGuard = 0) - a game assert still hits the game's debug break and closes it");
        return;
    }
    const uint32_t rva = Grva(kLogger);
    if (!rva) {
        g_log("ASSERTGUARD: not installed (game assert logger not found)");
        return;
    }
    const uint8_t* fn = reinterpret_cast<const uint8_t*>(info.base + Grva(kLogger));
    if (!LoggerMatches(fn)) {
        g_log("ASSERTGUARD: not installed (assert logger bytes differ - another patch or build)");
        return;
    }
    int32_t disp = 0;
    memcpy(&disp, fn + kBufLea + 3, 4);
    const char* buf = reinterpret_cast<const char*>(fn + kBufLea + 7 + disp);
    g_buf = Readable(buf, 1) ? buf : nullptr;
    g_base = info.base;
    g_site = reinterpret_cast<uintptr_t>(fn) + kInt3;
    g_veh = AddVectoredExceptionHandler(1, &Veh);
    if (!g_veh) {
        g_site = 0;
        g_log("ASSERTGUARD: not installed (AddVectoredExceptionHandler failed)");
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr)) CloseHandle(h);
    g_log("ASSERTGUARD: installed at +%X (%s) - a game assert logs its message and the game continues instead of "
          "closing on the debug break (#153) - [General] AssertGuard = %d",
          static_cast<unsigned>(g_site - info.base), info.knownBuild ? "v2.02" : "found by pattern", want);
}

}  // namespace assertguard
