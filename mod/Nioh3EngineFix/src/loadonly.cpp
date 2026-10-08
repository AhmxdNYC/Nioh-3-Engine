// loadonly.cpp - LO loads-only gating of GF / W6 / RWb on the 58A level-load window. See loadonly.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <cstdint>
#include <cstring>

#include "loadonly.h"

namespace loadonly {
namespace {

constexpr uint32_t kGameMainPtr = 0x4759910, kGm58A = 0x58A;
constexpr uint32_t kRwbSite = 0x1A3146A;  // ResStateWaitSwap cold spin: mov edx,[rbp+3A8h] (the RWb site, patches.json RWb)
constexpr uint32_t kRwbBack = 0x1A31470;  // mov rcx,rbp; call JobWakeOneInPool 48B9C; inc ebx; test bl,bl; jne 1A31486; ...
constexpr uint32_t kRwbSkip = 0x1A31486;  // after `xor ecx,ecx; call [Sleep]`
constexpr uint32_t kWakeFn = 0x48B9C;
// The 22 original bytes at 1A3146A (patches.json RWb orig) + the Sleep(0) call that follows (FF 15 -> KERNEL32!Sleep).
constexpr uint8_t kRwbOrig[22] = {0x8B, 0x95, 0xA8, 0x03, 0x00, 0x00, 0x48, 0x8B, 0xCD, 0xE8, 0x24,
                                  0x77, 0x61, 0xFE, 0xFF, 0xC3, 0x84, 0xDB, 0x75, 0x08, 0x33, 0xC9};

// Cave (72 + 4 bytes). BACK/SKIP absolute qwords are filled at install; the word lives at +48h.
// tests\test_patterns.py parses this array and simulates both paths against the stock and RWb byte sequences.
constexpr uint8_t kRwbCave[0x4C] = {
    /* 00 */ 0x83, 0x3D, 0x41, 0x00, 0x00, 0x00, 0x00,  // cmp dword [rip+41h],0   -> word at +48h
    /* 07 */ 0x75, 0x14,                                // jne +14h -> 1Dh (RWb order)
    /* 09 */ 0x8B, 0x95, 0xA8, 0x03, 0x00, 0x00,        // mov edx,[rbp+3A8h]       (stock order)
    /* 0F */ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,        // jmp [rip+0] -> BACK
    /* 15 */ 0, 0, 0, 0, 0, 0, 0, 0,                    //   BACK qword
    /* 1D */ 0xFF, 0xC3,                                // inc ebx
    /* 1F */ 0x84, 0xDB,                                // test bl,bl
    /* 21 */ 0x75, 0x16,                                // jne +16h -> 39h (skip wake + Sleep(0))
    /* 23 */ 0xFF, 0xCB,                                // dec ebx (the stock tail increments it again)
    /* 25 */ 0x8B, 0x95, 0xA8, 0x03, 0x00, 0x00,        // mov edx,[rbp+3A8h]
    /* 2B */ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,        // jmp [rip+0] -> BACK
    /* 31 */ 0, 0, 0, 0, 0, 0, 0, 0,                    //   BACK qword
    /* 39 */ 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00,        // jmp [rip+0] -> SKIP
    /* 3F */ 0, 0, 0, 0, 0, 0, 0, 0,                    //   SKIP qword
    /* 47 */ 0xCC,
    /* 48 */ 0, 0, 0, 0};                               // word (0 stock, 1 RWb)
constexpr size_t kBack1 = 0x15, kBack2 = 0x31, kSkipQ = 0x3F, kWord = 0x48;

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
volatile uint32_t* g_words[3] = {nullptr, nullptr, nullptr};  // GF, W6, RWb
const char* const kNames[3] = {"GF", "W6", "RWb"};

bool SafeRead(uintptr_t a, void* out, size_t n) {
    if (a < 0x10000) return false;
    __try {
        memcpy(out, (const void*)a, n);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

uint8_t* AllocNear(uintptr_t lo, uintptr_t hi) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity, reach = 0x7FF00000ull - (hi - lo);
    auto in = [&](uintptr_t from, uintptr_t to) -> uint8_t* {
        MEMORY_BASIC_INFORMATION mbi;
        for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
            if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
            const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
            if (mbi.State == MEM_FREE) {
                const uintptr_t c = (rb + g - 1) & ~(g - 1);
                if (c >= from && c + 4096 <= re && c + 4096 <= to)
                    if (void* p = VirtualAlloc((void*)c, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
            }
            if (re <= a) break;
            a = re;
        }
        return nullptr;
    };
    if (uint8_t* p = in(hi, hi + reach)) return p;
    return in(lo > reach + 0x10000 ? lo - reach : 0x10000, lo);
}

// 6 bytes at p in one interlocked 8- or 16-byte compare-exchange (1A3146A..1A3146F fits the 8-byte word 1A31468).
bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok;
    if (fit8) {
        volatile LONG64* q = (volatile LONG64*)w8;
        const LONG64 cur = *q;
        LONG64 nw = cur;
        memcpy((uint8_t*)&nw + (a - w8), bytes, len);
        ok = InterlockedCompareExchange64(q, nw, cur) == cur;
    } else {
        volatile LONG64* q = (volatile LONG64*)w16;
        alignas(16) LONG64 cur[2] = {q[0], q[1]};
        LONG64 nw[2] = {cur[0], cur[1]};
        memcpy((uint8_t*)nw + (a - w16), bytes, len);
        ok = _InterlockedCompareExchange128(q, nw[1], nw[0], cur) != 0;
    }
    VirtualProtect(win, wlen, old, &old);
    FlushInstructionCache(GetCurrentProcess(), win, wlen);
    return ok;
}

// 0 ok, else a reason string index: 1 bytes differ, 2 no page, 3 not atomic.
int InstallRwb(uint32_t rwbUser) {
    uint8_t* p = (uint8_t*)(g_base + kRwbSite);
    if (memcmp(p, kRwbOrig, sizeof(kRwbOrig)) != 0) return 1;
    int32_t rel;
    memcpy(&rel, p + 10, 4);
    if ((uintptr_t)(p + 14 + rel) != g_base + kWakeFn) return 1;
    const uint8_t* s = p + 22;  // FF 15 [Sleep] at 1A31480 (the import itself is checked offline)
    if (s[0] != 0xFF || s[1] != 0x15) return 1;
    const auto* dos = (const IMAGE_DOS_HEADER*)g_base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(g_base + dos->e_lfanew);
    uint8_t* c = AllocNear(g_base, g_base + nt->OptionalHeader.SizeOfImage);
    if (!c) return 2;
    memcpy(c, kRwbCave, sizeof(kRwbCave));
    const uint64_t back = g_base + kRwbBack, skip = g_base + kRwbSkip;
    memcpy(c + kBack1, &back, 8);
    memcpy(c + kBack2, &back, 8);
    memcpy(c + kSkipQ, &skip, 8);
    volatile uint32_t* w = reinterpret_cast<volatile uint32_t*>(c + kWord);
    *w = rwbUser ? 1u : 0u;
    FlushInstructionCache(GetCurrentProcess(), c, sizeof(kRwbCave));
    const intptr_t d = (intptr_t)c - (intptr_t)(p + 5);
    if (d < INT32_MIN || d > INT32_MAX) return 2;
    uint8_t nb[6] = {0xE9, 0, 0, 0, 0, 0x90};
    const int32_t r = (int32_t)d;
    memcpy(nb + 1, &r, 4);
    if (!PatchAtomic(p, nb, 6)) return 3;
    g_words[2] = w;
    return 0;
}

DWORD WINAPI Gate(void*) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    bool on = false;
    int n = 0;
    uint32_t saved[3] = {0, 0, 0};
    LARGE_INTEGER fq, t0, t1;
    QueryPerformanceFrequency(&fq);
    t0.QuadPart = 0;
    for (;;) {
        Sleep(10);
        uintptr_t gm = 0;
        uint8_t f = 0;
        if (!SafeRead(g_base + kGameMainPtr, &gm, sizeof(gm)) || !gm || !SafeRead(gm + kGm58A, &f, 1)) continue;
        if (f && !on) {
            on = true;
            ++n;
            for (int i = 0; i < 3; ++i)
                if (g_words[i]) {
                    saved[i] = *g_words[i];
                    *g_words[i] = 1;
                }
            QueryPerformanceCounter(&t0);
            g_log("LO: on at 58A set #%d (GF %s, W6 %s, RWb %s were %u/%u/%u)", n, g_words[0] ? "1" : "-", g_words[1] ? "1" : "-",
                  g_words[2] ? "1" : "-", saved[0], saved[1], saved[2]);
        } else if (!f && on) {
            on = false;
            for (int i = 0; i < 3; ++i)
                if (g_words[i]) *g_words[i] = saved[i];
            QueryPerformanceCounter(&t1);
            g_log("LO: off at 58A clear #%d (+%.1f s), restored GF %u W6 %u RWb %u", n,
                  (double)(t1.QuadPart - t0.QuadPart) / (double)fq.QuadPart, saved[0], saved[1], saved[2]);
        }
    }
}

}  // namespace

bool Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    if (!g_log || !g_base) return false;
    if (!info.knownBuild) {
        g_log("LO: refused (unknown build; addresses are v2.02)");
        return false;
    }
    g_words[0] = info.gf;
    g_words[1] = info.w6;
    const int r = InstallRwb(info.rwbUser);
    if (r) g_log("LO: RWb switchable cave not installed (%s) - RWb stays stock", r == 1 ? "bytes differ" : r == 2 ? "no page" : "not atomic");
    if (!g_words[0] && !g_words[1] && !g_words[2]) {
        g_log("LO: nothing to gate (GF, W6 and RWb all unavailable)");
        return false;
    }
    HANDLE th = CreateThread(nullptr, 0, &Gate, nullptr, 0, nullptr);
    if (!th) return false;
    CloseHandle(th);
    g_log("LO: loads-only gating on: %s%s%s at G+58Ah; outside loads GF %u W6 %u RWb %u (the user's setting)", g_words[0] ? "GF " : "",
          g_words[1] ? "W6 " : "", g_words[2] ? "RWb" : "", g_words[0] ? *g_words[0] : 0u, g_words[1] ? *g_words[1] : 0u,
          g_words[2] ? *g_words[2] : 0u);
    return true;
}

}  // namespace loadonly
