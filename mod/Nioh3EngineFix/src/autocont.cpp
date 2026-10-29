// autocont.cpp - AutoContinueLoad ([General] AutoContinueLoad = 0 by default). See autocont.h and notes\auto-continue-load.md.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "autocont.h"
#include "autocont_thunk.h"

namespace autocont {
namespace {

// ---- v2.02 (RVAs; patches.json site AC1 + deps, verified by tests\test_patterns.py) ----
constexpr uint32_t kSite = 0x1CECAED;        // E8 rel32 in TipPageConfirmCheck 1CECAE4 -> ConfirmPressed 283950
constexpr uint32_t kCallee = 0x283950;       // ConfirmPressed: GameMain+3Eh || GameMain+3Fh (al), leaf, clobbers rax only
constexpr uint32_t kCheck = 0x1CECAE4;       // TipPageConfirmCheck (only caller: TipPageLoadDoneTick 1CECB24)
// TipPageConfirmCheck, whole: push rbx; sub rsp,20h; mov rbx,rcx; call 283950; test al,al; je; call 28393C; test al,al;
// jne; call 27FBC44; test al,al; jne; call 22C5248; mov rax,[rbx]; xor edx,edx; mov rcx,rbx; call [rax+38h]; mov al,1
const uint8_t kCheckBytes[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9, 0xE8, 0x5E, 0x6E, 0x59, 0xFE, 0x84, 0xC0, 0x74, 0x26,
                               0xE8, 0x41, 0x6E, 0x59, 0xFE, 0x84, 0xC0, 0x75, 0x1D, 0xE8, 0x40, 0xF1, 0xB0, 0x00, 0x84, 0xC0, 0x75, 0x14,
                               0xE8, 0x3B, 0x87, 0x5D, 0x00, 0x48, 0x8B, 0x03, 0x33, 0xD2, 0x48, 0x8B, 0xCB, 0xFF, 0x50, 0x38, 0xB0, 0x01};
constexpr uint32_t kTick = 0x1CECB24;        // TipPageLoadDoneTick head: push rbx; sub rsp,0B0h; cmp byte [rcx+71Bh],0 (load done)
const uint8_t kTickBytes[] = {0x40, 0x53, 0x48, 0x81, 0xEC, 0xB0, 0x00, 0x00, 0x00, 0x80, 0xB9, 0x1B, 0x07, 0x00, 0x00, 0x00};

void (*g_log)(const char*, ...) = nullptr;
bool (*g_coverPermission)() = nullptr;
bool CoverPermission() { return g_coverPermission && g_coverPermission(); }
uint8_t* g_page = nullptr;            // thunk page (within rel32 reach of the image)
volatile uint8_t* g_switch = nullptr;  // private user preference byte
volatile long* g_count = nullptr;      // answered tip checks
RUNTIME_FUNCTION g_thunkFunction{};

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

// One interlocked 8- or 16-byte compare-exchange (same scheme as loadstats.cpp / bosslog.cpp).
bool PatchAtomic(uint8_t* p, const uint8_t* bytes, size_t len) {
    const uintptr_t a = (uintptr_t)p, w8 = a & ~uintptr_t(7), w16 = a & ~uintptr_t(15);
    const bool fit8 = a + len <= w8 + 8, fit16 = a + len <= w16 + 16;
    if (!fit8 && !fit16) return false;
    void* win = fit8 ? (void*)w8 : (void*)w16;
    const size_t wlen = fit8 ? 8 : 16;
    DWORD old = 0;
    if (!VirtualProtect(win, wlen, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = false;
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
// The site (1CECAED..1CECAF1) straddles a 16-byte boundary: park the first two bytes on "jmp $" (EB FE), write the tail,
// then the head. A thread that already decoded the old call runs it once, unchanged.
bool PatchSpin(uint8_t* p, const uint8_t* bytes, size_t len) {
    static const uint8_t spin[2] = {0xEB, 0xFE};
    uint8_t orig[2] = {p[0], p[1]};
    if (len < 3 || !PatchAtomic(p, spin, 2)) return false;
    DWORD old = 0;
    if (!VirtualProtect(p + 2, len - 2, PAGE_EXECUTE_READWRITE, &old)) {
        PatchAtomic(p, orig, 2);
        return false;
    }
    memcpy(p + 2, bytes + 2, len - 2);
    VirtualProtect(p + 2, len - 2, old, &old);
    FlushInstructionCache(GetCurrentProcess(), p, len);
    return PatchAtomic(p, bytes, 2);
}
bool Patch(uint8_t* p, const uint8_t* bytes, size_t len) { return PatchAtomic(p, bytes, len) || PatchSpin(p, bytes, len); }

// One log line at the start of each burst of answered checks (normally exactly one per load).
DWORD WINAPI Watch(void*) {
    long last = 0;
    bool burst = false;
    int n = 0;
    for (;;) {
        Sleep(500);
        const long c = *g_count;
        if (c != last) {
            if (!burst) g_log("AutoContinue #%d: load done - answered the tip page's 'press to continue' check", ++n);
            burst = true;
            last = c;
        } else {
            burst = false;
        }
    }
}

}  // namespace

long Count() { return g_count ? *g_count : 0; }

volatile uint8_t* Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath) return nullptr;
    const int want = (int)GetPrivateProfileIntW(L"General", L"AutoContinueLoad", 0, info.iniPath);
    if (!info.knownBuild) {
        g_log("AutoContinue: not installed (unknown build; addresses are v2.02)");
        return nullptr;
    }
    uint8_t* site = (uint8_t*)(info.base + kSite);
    int32_t rel = 0;
    memcpy(&rel, site + 1, 4);
    if (site[0] != 0xE8 || (uintptr_t)(site + 5 + rel) != info.base + kCallee ||
        memcmp((const void*)(info.base + kCheck), kCheckBytes, sizeof(kCheckBytes)) != 0 ||
        memcmp((const void*)(info.base + kTick), kTickBytes, sizeof(kTickBytes)) != 0) {
        g_log("AutoContinue: not installed (tip page code bytes differ - another patch or build)");
        return nullptr;
    }
    const auto* dos = (const IMAGE_DOS_HEADER*)info.base;
    const auto* nt = (const IMAGE_NT_HEADERS64*)(info.base + dos->e_lfanew);
    g_page = AllocNear(info.base, info.base + nt->OptionalHeader.SizeOfImage, 4096);
    if (!g_page) {
        g_log("AutoContinue: not installed (no executable page within 2 GB of the image)");
        return nullptr;
    }
    // Separate ephemeral cover permission never writes the persistent user switch.
    g_coverPermission = info.coverPermission;
    uint8_t* t = g_page;
    const size_t thunkSize = autocont_thunk::Build(t, info.base + kCallee, reinterpret_cast<uintptr_t>(&CoverPermission));
    if (thunkSize >= autocont_thunk::kSwitch) return nullptr;
    // Real nonleaf thunk: register its fixed C8 stack allocation for unwinding.
    // Version1, prolog7, two unwind slots: UWOP_ALLOC_LARGE / C8 divided by8.
    memcpy(t+autocont_thunk::kUnwind,autocont_thunk::kUnwindBytes,sizeof(autocont_thunk::kUnwindBytes));
    g_thunkFunction={0,static_cast<DWORD>(thunkSize),static_cast<DWORD>(autocont_thunk::kUnwind)};
    if(!RtlAddFunctionTable(&g_thunkFunction,1,reinterpret_cast<DWORD64>(t))) {
        g_log("AutoContinue: not installed (thunk unwind registration failed)");
        return nullptr;
    }
    g_switch = t + autocont_thunk::kSwitch;
    g_count = reinterpret_cast<volatile long*>(t + autocont_thunk::kCount);
    *g_switch = want ? 1 : 0;
    *g_count = 0;
    FlushInstructionCache(GetCurrentProcess(), t, thunkSize);
    const intptr_t d = (intptr_t)t - (intptr_t)(site + 5);
    if (d < INT32_MIN || d > INT32_MAX) {
        g_log("AutoContinue: not installed (thunk out of rel32 reach)");
        return nullptr;
    }
    uint8_t nb[5] = {0xE8, 0, 0, 0, 0};
    const int32_t nrel = (int32_t)d;
    memcpy(nb + 1, &nrel, 4);
    if (!Patch(site, nb, 5)) {
        g_log("AutoContinue: not installed (site write failed)");
        g_switch = nullptr;
        RtlDeleteFunctionTable(&g_thunkFunction);
        return nullptr;
    }
    if (HANDLE h = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr)) CloseHandle(h);
    g_log("AutoContinue: installed (tip page confirm check 1CECAED), %s - [General] AutoContinueLoad = %d; switch in the mod menu",
          want ? "ON: the 'press to continue' page after a load closes as soon as the load is done" : "OFF (stock)", want);
    return g_switch;
}

}  // namespace autocont
