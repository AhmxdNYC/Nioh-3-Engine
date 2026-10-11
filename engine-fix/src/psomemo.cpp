// psomemo.cpp - PCM graphics-PSO lookup memo (see psomemo.h).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "psomemo.h"

namespace psomemo {
namespace {

void (*g_log)(const char*, ...) = nullptr;
#define PLOG(...)                      \
    do {                               \
        if (g_log) g_log(__VA_ARGS__); \
    } while (0)

// v2.02 (patches.json group PCM): PsoGetOrCreateGfx 119670: `lea rbp,[rbx+708h]; mov rdx,rsi; mov rcx,rbp; call 11976C;
// xor edi,edi; test al,al`
constexpr uint32_t kSite = 0x1196BD, kCallee = 0x11976C;
const uint8_t kBefore[] = {0x48, 0x8D, 0xAB, 0x08, 0x07, 0x00, 0x00, 0x48, 0x8B, 0xD6, 0x48, 0x8B, 0xCD};
const uint8_t kAfter[] = {0x33, 0xFF, 0x84, 0xC0};
constexpr size_t kEnt = 0x90;      // hash u32, pad u32, key[0x80], PSO*
constexpr size_t kCountOff = 0x28; // cache entry count (PsoGetOrCreateGfx compares [c+730h] = cache+28h with the capacity)
constexpr size_t kCapOff = 0x34;   // capacity [c+73Ch]; the root node pointer is cache+0 (PsoCacheFindGfx: mov rcx,[rcx])

using FindFn = bool(__fastcall*)(void* cache, uint8_t* key);
FindFn g_orig = nullptr;
volatile uint32_t g_sw = 0;
bool g_installed = false;

constexpr uint32_t kSlots = 4096;
struct Slot {
    uint32_t valid;
    uint32_t pad;
    uint8_t ent[kEnt];
};
Slot* g_tab = nullptr;
void* volatile g_cache = nullptr;
uint32_t g_count = 0xFFFFFFFFu, g_cap = 0;
void* g_root = nullptr;
volatile LONG64 g_look = 0, g_hit = 0, g_miss = 0, g_flush = 0, g_other = 0, g_off = 0;

bool __fastcall PmFind(void* cache, uint8_t* key) {
    if (!g_sw) {
        InterlockedIncrement64(&g_off);
        return g_orig(cache, key);
    }
    void* c0 = g_cache;
    if (!c0) {
        void* prev = InterlockedCompareExchangePointer((PVOID volatile*)&g_cache, cache, nullptr);
        c0 = prev ? prev : cache;
    }
    if (c0 != cache) {  // a second cache (another context): stock path, never touches the table
        InterlockedIncrement64(&g_other);
        return g_orig(cache, key);
    }
    // from here on: under this cache's critical section (119670 took it before the call)
    // generation: entry count, capacity and root node. Any insert, grow or clear changes one of them -> start over. A
    // cached entry never changes otherwise (a failed re-insert keeps the old PSO), so a stored copy stays exact.
    const uint8_t* cb = static_cast<const uint8_t*>(cache);
    const uint32_t cnt = *reinterpret_cast<const uint32_t*>(cb + kCountOff), cap = *reinterpret_cast<const uint32_t*>(cb + kCapOff);
    void* const root = *reinterpret_cast<void* const*>(cb);
    if (cnt != g_count || cap != g_cap || root != g_root) {
        for (uint32_t i = 0; i < kSlots; ++i) g_tab[i].valid = 0;
        g_count = cnt;
        g_cap = cap;
        g_root = root;
        ++g_flush;
    }
    ++g_look;
    uint32_t h;
    memcpy(&h, key, 4);
    Slot& s = g_tab[(h ^ (h >> 12)) & (kSlots - 1)];
    if (s.valid && memcmp(s.ent, key, 4) == 0 && memcmp(s.ent + 8, key + 8, 0x80) == 0) {
        memcpy(key, s.ent, kEnt);  // what PsoCacheFindGfx copies from the cached entry
        ++g_hit;
        return true;
    }
    const bool r = g_orig(cache, key);
    ++g_miss;
    if (r) {
        memcpy(s.ent, key, kEnt);
        s.valid = 1;
    }
    return r;
}

uint8_t* AllocIn(uintptr_t from, uintptr_t to, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    MEMORY_BASIC_INFORMATION mbi;
    for (uintptr_t a = (from + g - 1) & ~(g - 1); a < to;) {
        if (!VirtualQuery((void*)a, &mbi, sizeof(mbi))) break;
        const uintptr_t rb = (uintptr_t)mbi.BaseAddress, re = rb + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            const uintptr_t c = (rb + g - 1) & ~(g - 1);
            if (c >= from && c + size <= re && c + size <= to)
                if (void* p = VirtualAlloc((void*)c, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
        if (re <= a) break;
        a = re;
    }
    return nullptr;
}
uint8_t* AllocNear(uintptr_t lo, uintptr_t hi, size_t size) {
    const uintptr_t reach = 0x7FF00000ull - (hi - lo);
    if (uint8_t* p = AllocIn(hi, hi + reach, size)) return p;
    return AllocIn(lo > reach + 0x10000 ? lo - reach : 0x10000, lo, size);
}

// The 5 call bytes cross a 16-byte boundary (1196BD..1196C1), so no single atomic store: write them with every other
// thread suspended and none stopped inside 1196BE..1196C1 (same scheme as drawprep.cpp FrozenWrite). No heap / CRT use
// while threads are suspended.
bool FrozenWrite5(uint8_t* at, const uint8_t* bytes) {
    DWORD old = 0;
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
    constexpr int kMaxT = 1024;
    static DWORD tids[kMaxT];
    static HANDLE hs[kMaxT];
    static bool sus[kMaxT];
    bool written = false;
    for (int attempt = 0; attempt < 200 && !written; ++attempt) {
        int nt = 0;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            THREADENTRY32 te{};
            te.dwSize = sizeof(te);
            for (BOOL ok = Thread32First(snap, &te); ok && nt < kMaxT; ok = Thread32Next(snap, &te))
                if (te.th32OwnerProcessID == pid && te.th32ThreadID != self) tids[nt++] = te.th32ThreadID;
            CloseHandle(snap);
        }
        int nh = 0;
        for (int i = 0; i < nt; ++i)
            if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tids[i])) hs[nh++] = h;
        // ---- frozen section ----
        for (int i = 0; i < nh; ++i) sus[i] = SuspendThread(hs[i]) != (DWORD)-1;
        bool clash = false;
        for (int i = 0; i < nh && !clash; ++i) {
            if (!sus[i]) continue;
            CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;
            if (!GetThreadContext(hs[i], &ctx)) continue;
            if (ctx.Rip > (uintptr_t)at && ctx.Rip < (uintptr_t)at + 5) clash = true;
        }
        if (!clash) {
            memcpy(at, bytes, 5);
            FlushInstructionCache(GetCurrentProcess(), at, 5);
            written = true;
        }
        for (int i = 0; i < nh; ++i)
            if (sus[i]) ResumeThread(hs[i]);
        // ---- end frozen section ----
        for (int i = 0; i < nh; ++i) CloseHandle(hs[i]);
        if (!written) Sleep(1 + attempt % 4);
    }
    VirtualProtect(at, 5, old, &old);
    return written && memcmp(at, bytes, 5) == 0;
}

}  // namespace

#ifdef PSOMEMO_TEST  // tests\psomemo_harness.cpp: the memo logic against a model of PsoCacheFindGfx
void TestInit(bool (*orig)(void*, uint8_t*)) {
    g_orig = (FindFn)orig;
    if (!g_tab) g_tab = static_cast<Slot*>(VirtualAlloc(nullptr, sizeof(Slot) * kSlots, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_sw = 1;
    g_installed = true;
}
bool TestFind(void* cache, uint8_t* key) { return PmFind(cache, key); }
#endif

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!info.iniPath || !info.base) return;
    const int pm = (int)GetPrivateProfileIntW(L"Patches", L"PCM", 0, info.iniPath);
    if (pm != 1 && pm != 2) return;
    if (!info.knownBuild) {
        PLOG("PCM: not installed (unknown build; the 1196BD site is v2.02)");
        return;
    }
    uint8_t* site = (uint8_t*)(info.base + kSite);
    int32_t rel = 0;
    memcpy(&rel, site + 1, 4);
    if (site[0] != 0xE8 || (uintptr_t)(site + 5 + rel) != info.base + kCallee || memcmp(site - sizeof kBefore, kBefore, sizeof kBefore) != 0 ||
        memcmp(site + 5, kAfter, sizeof kAfter) != 0) {
        PLOG("PCM: not installed (PsoGetOrCreateGfx call 1196BD bytes differ - another patch or build)");
        return;
    }
    g_tab = static_cast<Slot*>(VirtualAlloc(nullptr, sizeof(Slot) * kSlots, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    const auto* dos = (const IMAGE_DOS_HEADER*)info.base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(info.base + dos->e_lfanew);
    uint8_t* page = AllocNear(info.base, info.base + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_tab || !page) {
        PLOG("PCM: not installed (no memory for the table or no executable page within 2 GB of the image)");
        return;
    }
    g_orig = (FindFn)(info.base + kCallee);
    // relay: mov rax, imm64 (PmFind); jmp rax  (rax is no argument; the callee returns in it)
    page[0] = 0x48;
    page[1] = 0xB8;
    const uint64_t fn = (uint64_t)(uintptr_t)&PmFind;
    memcpy(page + 2, &fn, 8);
    page[10] = 0xFF;
    page[11] = 0xE0;
    FlushInstructionCache(GetCurrentProcess(), page, 16);
    const intptr_t d = (intptr_t)page - (intptr_t)(site + 5);
    if (d < INT32_MIN || d > INT32_MAX) {
        PLOG("PCM: not installed (relay out of rel32 reach)");
        return;
    }
    g_sw = pm == 1 ? 1u : 0u;
    uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
    const int32_t nrel = (int32_t)d;
    memcpy(nb + 1, &nrel, 4);
    if (!FrozenWrite5(site, nb)) {
        PLOG("PCM: not installed (site write failed)");
        return;
    }
    g_installed = true;
    PLOG("PCM: graphics-PSO lookup memo installed at PsoGetOrCreateGfx 1196BD (%u slots), switch %s", kSlots, g_sw ? "ON" : "off");
}

volatile uint32_t* Switch() { return g_installed ? &g_sw : nullptr; }

void StatusLine(char* buf, unsigned n, unsigned secs) {
    if (!n) return;
    buf[0] = 0;
    if (!g_installed) return;
    static LONG64 pL = 0, pH = 0, pM = 0, pF = 0, pO = 0, pX = 0;
    const LONG64 l = g_look, h = g_hit, m = g_miss, f = g_flush, o = g_off, x = g_other;
    const double s = secs ? (double)secs : 1.0;
    snprintf(buf, n, "PCM: switch %s | lookups %.0f/s, hits %.0f/s (%.1f %%), misses %.0f/s, table flushes %lld (cache count changes), "
             "switch-off calls %.0f/s, other-cache calls %.0f/s",
             g_sw ? "ON" : "off", (l - pL) / s, (h - pH) / s, l - pL ? 100.0 * (double)(h - pH) / (double)(l - pL) : 0.0, (m - pM) / s,
             (long long)(f - pF), (o - pO) / s, (x - pX) / s);
    pL = l; pH = h; pM = m; pF = f; pO = o; pX = x;
}

}  // namespace psomemo
