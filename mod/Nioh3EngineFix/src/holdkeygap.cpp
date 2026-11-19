// holdkeygap.cpp - HoldKeyGap ([General] HoldKeyGap = 0 by default). See holdkeygap.h.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "gamerva.h"

#include <cstdint>
#include <cstring>

#include "holdkeygap.h"

namespace holdkeygap {
namespace {

using ScanFn = uint64_t (*)(int64_t* self, int64_t res, float t, uint8_t slot);

void (*g_log)(const char*, ...) = nullptr;
ScanFn g_orig = nullptr;
volatile long g_fills = 0;
volatile long g_calls = 0;
volatile long g_same = 0, g_gaps = 0, g_overlaps = 0;
float g_maxGap = 0.0f;
bool g_on = false;

// v2.02 bytes at 20DFC0 (prologue up to the first push); the first kStolen are position independent and move to the trampoline.
const uint8_t kHead[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x68, 0x18, 0x48, 0x89, 0x70, 0x20,
                         0x48, 0x89, 0x50, 0x10, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xEC, 0x60};

// Per (this, slot) last window. One character is updated by one thread at a time; a torn entry only costs one stock window
// (every field is re-checked).
struct Entry {
    int64_t* self;
    int64_t res;
    float prevEnd;
    float prevT;
    uint32_t slot;
};
constexpr unsigned kSlots = 4096;
Entry g_tab[kSlots];

Entry& Lookup(int64_t* self, uint8_t slot) {
    uint64_t h = (((uint64_t)self >> 3) ^ ((uint64_t)slot << 40) ^ ((uint64_t)slot * 0x9E3779B97F4A7C15ull)) * 0xFF51AFD7ED558CCDull;
    return g_tab[(h >> 52) & (kSlots - 1)];
}

uint64_t Hook(int64_t* self, int64_t res, float t, uint8_t slot) {
    if (!self || !res || slot > 3) return g_orig(self, res, t, slot);
    InterlockedIncrement(&g_calls);
    float* stepp = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(self) + 0x40 + slot * 4);
    const float step = *stepp;
    Entry& e = Lookup(self, slot);
    const bool same = e.self == self && e.slot == slot && e.res == res;
    const float lo = WindowStart(same, e.prevEnd, e.prevT, t, step);
    if (same && e.prevT <= t) {
        const float d = t - e.prevEnd;
        InterlockedIncrement(&g_same);
        if (d > 0.001f) { InterlockedIncrement(&g_gaps); if (d > g_maxGap && d < 100.0f) g_maxGap = d; }
        else if (d < -0.001f) InterlockedIncrement(&g_overlaps);
    }
    if (g_log && (g_calls == 1 || g_calls == 300 || (g_calls & 0xFFF) == 0))
        g_log("HOLDKEYGAP: calls %ld same-move %ld gaps %ld overlaps %ld max gap %.3f fills %ld (last this=%p slot=%u t=%.3f step=%.3f)",
              g_calls, g_same, g_gaps, g_overlaps, g_maxGap, g_fills, (void*)self, (unsigned)slot, t, step);
    e.self = self;
    e.slot = slot;
    e.res = res;
    e.prevEnd = t + step;
    e.prevT = t;
    if (lo == t) return g_orig(self, res, t, slot);
    const float wide = t + step - lo;
    const long n = InterlockedIncrement(&g_fills);
    if (g_log && (n <= 40 || (n & 1023) == 0))
        g_log("HOLDKEYGAP: fill #%ld this=%p slot=%u t=%.3f step=%.3f gap=[%.3f, %.3f) calls=%ld", n, (void*)self,
              (unsigned)slot, t, step, lo, t, g_calls);
    *stepp = wide;
    const uint64_t r = g_orig(self, res, lo, slot);
    if (*stepp == wide) *stepp = step;  // the scan's own writes (none seen) win
    return r;
}

uint8_t* AllocNear(uintptr_t at, size_t size) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    const uintptr_t g = si.dwAllocationGranularity;
    for (uintptr_t d = 0x10000; d < 0x70000000; d += g) {
        for (int sgn = 0; sgn < 2; ++sgn) {
            const uintptr_t a = sgn ? ((at + d) & ~(g - 1)) : ((at - d) & ~(g - 1));
            if (sgn == 0 && d > at) continue;
            if (void* p = VirtualAlloc((void*)a, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE)) return (uint8_t*)p;
        }
    }
    return nullptr;
}

void PutAbsJmp(uint8_t* p, uintptr_t to) {
    p[0] = 0xFF; p[1] = 0x25; p[2] = p[3] = p[4] = p[5] = 0;  // jmp [rip+0]
    memcpy(p + 6, &to, 8);
}

}  // namespace

long Fills() { return g_fills; }
bool Installed() { return g_on; }

void Start(const StartInfo& info) {
    g_log = info.log;
    if (!g_log || !info.base || !info.iniPath || g_on) return;
    const int want = (int)GetPrivateProfileIntW(L"General", L"HoldKeyGap", 0, info.iniPath);
    if (want == 0) {
        g_log("HOLDKEYGAP: off ([General] HoldKeyGap = 0) - move key windows are stock");
        return;
    }
    const uintptr_t fn = info.base + Grva(kFnRva);
    if (fn == info.base || memcmp((const void*)fn, kHead, sizeof kHead) != 0 || (fn & 7) != 0) {
        g_log("HOLDKEYGAP: not installed (key scan bytes differ - another patch or build)");
        return;
    }
    uint8_t* page = AllocNear(fn, 0x1000);
    if (!page) {
        g_log("HOLDKEYGAP: not installed (no page near the game)");
        return;
    }
    uint8_t* stub = page;          // jmp Hook
    uint8_t* tramp = page + 0x40;  // stolen bytes + jmp fn+kStolen
    PutAbsJmp(stub, (uintptr_t)&Hook);
    memcpy(tramp, (const void*)fn, kStolen);
    PutAbsJmp(tramp + kStolen, fn + kStolen);
    FlushInstructionCache(GetCurrentProcess(), page, 0x100);
    g_orig = reinterpret_cast<ScanFn>(tramp);

    DWORD old;
    if (!VirtualProtect((void*)fn, 8, PAGE_EXECUTE_READWRITE, &old)) {
        g_log("HOLDKEYGAP: not installed (VirtualProtect failed)");
        return;
    }
    uint8_t nb[8];
    memcpy(nb, (const void*)fn, 8);
    nb[0] = 0xE9;
    const int32_t rel = (int32_t)((intptr_t)stub - (intptr_t)(fn + 5));
    memcpy(nb + 1, &rel, 4);
    int64_t v;
    memcpy(&v, nb, 8);
    InterlockedExchange64((volatile LONG64*)fn, v);  // one aligned 8-byte store: no thread sees half a jmp
    VirtualProtect((void*)fn, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)fn, 8);
    g_on = true;
    g_log("HOLDKEYGAP: installed at +%X (%s) - [General] HoldKeyGap = %d (a move key between two uneven frames is tested "
          "instead of skipped; max gap %.1f frames)",
          (unsigned)(fn - info.base), info.knownBuild ? "v2.02" : "found by pattern", want, kMaxGap);
}

}  // namespace holdkeygap
