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
int g_mode = 0;  // 1 = fill gaps, 2 = trace only (stock windows)
volatile long g_trace = 0;

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


// ---- mode 2 gate probe: wrap vtable slots the scan calls, log them while a scan near t 10-15 runs on this thread ----
using G4 = uint64_t (*)(uint64_t, uint64_t, uint64_t, uint64_t);
constexpr int kGates = 6;
const unsigned kGateOff[kGates] = {0xC0, 0xE8, 0xF0, 0x120, 0x128, 0x138};
struct VtRec { void** vt; G4 orig[kGates]; };
VtRec g_vts[16];
volatile long g_nvt = 0;
SRWLOCK g_vtLock = SRWLOCK_INIT;
uintptr_t g_base = 0;
thread_local int t_in = 0;
thread_local float t_t = 0.0f;

G4 OrigFor(uint64_t self, int gi) {
    void** vt = *reinterpret_cast<void***>(self);
    for (long i = 0; i < g_nvt; ++i) if (g_vts[i].vt == vt) return g_vts[i].orig[gi];
    return nullptr;
}
uint64_t g_maKeys[64];
volatile long g_nMa = 0;
thread_local uint64_t t_lastKey = 0;
bool IsMa(uint64_t k) {
    for (long i = 0; i < g_nMa && i < 64; ++i) if (g_maKeys[i] == k) return true;
    return false;
}
template <int GI>
uint64_t GateW(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
    G4 o = OrigFor(a, GI);
    const uint64_t r = o(a, b, c, d);
    if (GI == 5 && b == 0x5A3 && !IsMa(c)) {
        const long i = InterlockedIncrement(&g_nMa) - 1;
        if (i < 64) g_maKeys[i] = c;
    }
    if (t_in && g_log && g_trace < 60000) {
        bool want = false;
        if (GI == 3) { t_lastKey = b; want = IsMa(b); }
        else if (GI == 1 || GI == 2 || GI == 4) want = IsMa(b) || IsMa(t_lastKey);
        else if (GI == 5) want = IsMa(c);
        else if (GI == 0) want = IsMa(t_lastKey);
        if (want) {
            InterlockedIncrement(&g_trace);
            g_log("HKGG +%X this=%p t=%.3f a2=%llx a3=%llx -> %llx", kGateOff[GI], (void*)a, t_t, (unsigned long long)b,
                  (unsigned long long)c, (unsigned long long)r);
        }
    }
    return r;
}
const G4 kGateW[kGates] = {&GateW<0>, &GateW<1>, &GateW<2>, &GateW<3>, &GateW<4>, &GateW<5>};

void WrapVt(int64_t* self) {
    void** vt = *reinterpret_cast<void***>(self);
    for (long i = 0; i < g_nvt; ++i) if (g_vts[i].vt == vt) return;
    AcquireSRWLockExclusive(&g_vtLock);
    bool have = false;
    for (long i = 0; i < g_nvt; ++i) if (g_vts[i].vt == vt) have = true;
    if (!have && g_nvt < 16) {
        VtRec& v = g_vts[g_nvt];
        v.vt = vt;
        for (int gi = 0; gi < kGates; ++gi) v.orig[gi] = reinterpret_cast<G4>(vt[kGateOff[gi] / 8]);
        MemoryBarrier();
        InterlockedIncrement(&g_nvt);
        DWORD old;
        if (VirtualProtect(vt + 0x18, 0x30 * 8, PAGE_READWRITE, &old)) {
            for (int gi = 0; gi < kGates; ++gi) vt[kGateOff[gi] / 8] = reinterpret_cast<void*>(kGateW[gi]);
            VirtualProtect(vt + 0x18, 0x30 * 8, old, &old);
        }
        if (g_log) {
            g_log("HKGV vt=+%llX gates c0=+%llX e8=+%llX f0=+%llX 120=+%llX 128=+%llX 138=+%llX b0=+%llX",
                  (unsigned long long)((uintptr_t)vt - g_base), (unsigned long long)((uintptr_t)v.orig[0] - g_base),
                  (unsigned long long)((uintptr_t)v.orig[1] - g_base), (unsigned long long)((uintptr_t)v.orig[2] - g_base),
                  (unsigned long long)((uintptr_t)v.orig[3] - g_base), (unsigned long long)((uintptr_t)v.orig[4] - g_base),
                  (unsigned long long)((uintptr_t)v.orig[5] - g_base), (unsigned long long)((uintptr_t)vt[0xB0 / 8] - g_base));
        }
    }
    ReleaseSRWLockExclusive(&g_vtLock);
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
    if (g_mode == 2) {
        WrapVt(self);
        const bool near12 = t >= 10.0f && t < 15.0f;
        if (near12) { t_in = 1; t_t = t; }
        const uint64_t r = g_orig(self, res, t, slot);
        t_in = 0;
        if (g_log && t >= 8.0f && t < 34.0f && g_trace < 60000) {
            const uint8_t* k = reinterpret_cast<const uint8_t*>(r);
            if (r || (t >= 10.0f && t < 15.0f)) {
                InterlockedIncrement(&g_trace);
                g_log("HKGT this=%p s=%u res=%p t=%.3f st=%.3f r=%p ty=%d fl=%08x a69=%p", (void*)self, (unsigned)slot, (void*)res, t,
                      step, (void*)r, r ? (int)k[0x19] : -1, r ? *reinterpret_cast<const uint32_t*>(k + 0x48) : 0u,
                      (void*)self[0x69 + slot]);
            }
        }
        return r;
    }
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
    g_mode = want;
    g_base = info.base;
    g_on = true;
    g_log("HOLDKEYGAP: installed at +%X (%s) - [General] HoldKeyGap = %d (a move key between two uneven frames is tested "
          "instead of skipped; max gap %.1f frames)",
          (unsigned)(fn - info.base), info.knownBuild ? "v2.02" : "found by pattern", want, kMaxGap);
}

}  // namespace holdkeygap
