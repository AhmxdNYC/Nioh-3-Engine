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
    int64_t qpc;
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
        if (want && GI == 3) {
            const uint8_t* k = reinterpret_cast<const uint8_t*>(b);
            const uint64_t* self = reinterpret_cast<const uint64_t*>(a);
            uint64_t inp = self[0x18 / 8] ? self[0x18 / 8] : self[0x10 / 8];
            uint64_t inpFn = inp ? (*reinterpret_cast<const uint64_t* const*>(inp))[0xB0 / 8] - g_base : 0;
            uint64_t f140 = inp ? (*reinterpret_cast<const uint64_t* const*>(inp))[0x140 / 8] : 0;
            int held = -999;
            if (f140) held = reinterpret_cast<int (*)(uint64_t, uint32_t)>(f140)(inp, 1);
            g_log("HKGH key=%llx held1=%d f140=+%llX", (unsigned long long)b, held, (unsigned long long)(f140 ? f140 - g_base : 0));
            g_log("HKGK key=%llx cond %02x%02x %02x%02x %02x%02x | %02x %02x %02x | %02x%02x %02x%02x %02x%02x flags a8=%llx b0=%llx "
                  "inp=%p inpB0=+%llX -> %llx",
                  (unsigned long long)b, k[0x11], k[0x10], k[0x13], k[0x12], k[0x15], k[0x14], k[0x16], k[0x17], k[0x18], k[0x1B],
                  k[0x1A], k[0x1D], k[0x1C], k[0x1F], k[0x1E], (unsigned long long)self[0x15], (unsigned long long)self[0x16],
                  (void*)inp, (unsigned long long)inpFn, (unsigned long long)r);
        }
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


// ---- mode 3: held-time fix. The input object's held getter (vtable +0x140, Nioh3+0x901BA4) returns
// frames_held * 60 / fps_setting ([settings+0x614]). Below the fps setting (CPU load, uneven frames) that under-counts the
// real hold, so a key that needs "held >= N" at anim frame N (held attack -> martial art, N = 12) fails by one count and the
// move waits for a later key. Mode 3 returns max(stock, frames_held * measured frame time * 60): the hold in real 1/60 s.
constexpr uint32_t kHeldRva = 0x901BA4;
using HeldFn = int (*)(uint64_t inp, int button);
HeldFn g_heldOrig = nullptr;
volatile long g_heldUp = 0;
float g_emaDt = 0.0f;       // seconds per frame, EMA over the player's scan calls
void* g_heldVt[8];
volatile long g_nHeldVt = 0;
SRWLOCK g_heldLock = SRWLOCK_INIT;

// Real seconds each button has been held, per input object of the patched class, advanced once per frame by Tick
// (called from the owner's slot-0 key scan).
struct Tracker {
    uint64_t inp;
    int64_t lastTick;
    uint32_t lastCnt[0x40];
    float heldSec[0x40];
};
constexpr int kTrackers = 32;
constexpr double kStaleSec = 2.0;  // an input object not ticked for 2 s gives up its slot (GH #147)
Tracker g_tr[kTrackers];
volatile long g_nTr = 0;
SRWLOCK g_trLock = SRWLOCK_INIT;
long g_trNew = 0, g_trReused = 0, g_trFull = 0;

Tracker* FindTr(uint64_t inp) {
    const long n = g_nTr < kTrackers ? g_nTr : kTrackers;
    for (long i = 0; i < n; ++i) if (g_tr[i].inp == inp) return &g_tr[i];
    return nullptr;
}

// New input object: take a free slot or recycle the stalest one (GH #147: the table used to fill up and never recycle).
Tracker* NewTr(uint64_t inp, int64_t now, double freq) {
    AcquireSRWLockExclusive(&g_trLock);
    Tracker* tr = FindTr(inp);
    bool reused = false;
    if (!tr) {
        int64_t ticks[kTrackers];
        const int n = (int)(g_nTr < kTrackers ? g_nTr : kTrackers);
        for (int i = 0; i < n; ++i) ticks[i] = g_tr[i].lastTick;
        const int i = PickTracker(ticks, n, kTrackers, now, (int64_t)(kStaleSec * freq));
        if (i >= 0) {
            tr = &g_tr[i];
            reused = i < n;
            tr->inp = 0;  // a concurrent FindTr sees no owner while the slot is reset
            MemoryBarrier();
            tr->lastTick = 0;
            memset(tr->lastCnt, 0, sizeof tr->lastCnt);
            memset(tr->heldSec, 0, sizeof tr->heldSec);
            MemoryBarrier();
            tr->inp = inp;
            if (!reused) InterlockedIncrement(&g_nTr);
        }
    }
    ReleaseSRWLockExclusive(&g_trLock);
    if (!tr) {
        if (g_log && (++g_trFull == 1 || (g_trFull & 4095) == 0))
            g_log("HOLDKEYGAP: tracker table full (%d busy input objects) - input %p stays stock (#%ld)", kTrackers,
                  (void*)inp, g_trFull);
        return nullptr;
    }
    const long k = reused ? ++g_trReused : ++g_trNew;
    if (g_log && (k <= 20 || (k & 255) == 0))
        g_log("HOLDKEYGAP: tracking input %p (%s #%ld, %ld slots used)", (void*)inp, reused ? "reused stale slot" : "new slot",
              k, g_nTr < kTrackers ? g_nTr : (long)kTrackers);
    return tr;
}

void Tick(uint64_t inp, int64_t now, double freq) {
    Tracker* tr = FindTr(inp);
    if (!tr) tr = NewTr(inp, now, freq);
    if (!tr) return;
    const float d = tr->lastTick ? (float)((double)(now - tr->lastTick) / freq) : 0.0f;
    if (d > 1.0f) memset(tr->lastCnt, 0, sizeof tr->lastCnt);  // idle over 1 s (pause, freed and reused address): re-seed
    tr->lastTick = now;
    const float dt = g_emaDt;
    for (int b = 0; b < 0x40; ++b) {
        const uint32_t c = *reinterpret_cast<const uint32_t*>(inp + 0xA4 + (uint64_t)b * 4);
        tr->heldSec[b] = NextHeldSec(tr->heldSec[b], c, tr->lastCnt[b], d, dt);
        tr->lastCnt[b] = c;
    }
}

int HeldHook(uint64_t inp, int button) {
    const int s = g_heldOrig(inp, button);
    if (button < 0 || button >= 0x40) return s;
    Tracker* tr = FindTr(inp);
    if (!tr) return s;
    const uint32_t c = *reinterpret_cast<const uint32_t*>(inp + 0xA4 + (uint64_t)button * 4);
    const uint32_t lc = tr->lastCnt[button];
    const int r = HeldReal(s, tr->heldSec[button], c, lc);
    if (r > s) {
        const float dt = tr->heldSec[button] / (float)lc;
        const long n = InterlockedIncrement(&g_heldUp);
        if (g_log && (n <= 30 || (n & 1023) == 0))
            g_log("HOLDKEYGAP: held fix #%ld button %d frames %u stock %d real %d (frame %.2f ms)", n, button, c, s, r, dt * 1000.0f);
    }
    return r;
}

uintptr_t ReadVtSlot(uint64_t inp, void*** vtOut) {
    __try {
        void** vt = *reinterpret_cast<void***>(inp);
        *vtOut = vt;
        return (uintptr_t)vt[0x140 / 8];
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// True when inp is an input object of the patched class (patching it on first sight).
bool PatchHeldVt(uint64_t inp) {
    if (!inp || !g_base || (inp & 7)) return false;
    void** vt = nullptr;
    const uintptr_t cur = ReadVtSlot(inp, &vt);
    if (cur == (uintptr_t)&HeldHook) return true;
    if (cur != g_base + Grva(kHeldRva)) return false;  // another class or unreadable
    AcquireSRWLockExclusive(&g_heldLock);
    if ((uintptr_t)vt[0x140 / 8] == cur && g_nHeldVt < 8) {
        g_heldOrig = reinterpret_cast<HeldFn>(cur);
        g_heldVt[g_nHeldVt] = vt;
        InterlockedIncrement(&g_nHeldVt);
        DWORD old;
        if (VirtualProtect(&vt[0x140 / 8], 8, PAGE_READWRITE, &old)) {
            InterlockedExchangePointer(&vt[0x140 / 8], reinterpret_cast<void*>(&HeldHook));
            VirtualProtect(&vt[0x140 / 8], 8, old, &old);
            if (g_log) g_log("HOLDKEYGAP: held fix on input vtable +%llX", (unsigned long long)((uintptr_t)vt - g_base));
        }
    }
    ReleaseSRWLockExclusive(&g_heldLock);
    return true;
}

uint64_t Hook(int64_t* self, int64_t res, float t, uint8_t slot) {
    if (!self || !res || slot > 3) return g_orig(self, res, t, slot);
    InterlockedIncrement(&g_calls);
    if (g_mode == 3) {
        if (slot == 0 && t > 0.0f) {
            Entry& q = Lookup(self, 0);
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            static LARGE_INTEGER freq = [] { LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
            if (q.self == self && q.qpc) {
                const float d = (float)(now.QuadPart - q.qpc) / (float)freq.QuadPart;
                if (d > 0.002f && d < 0.1f) g_emaDt = g_emaDt > 0.0f ? g_emaDt + 0.2f * (d - g_emaDt) : d;
            }
            q.self = self;
            q.slot = 0;
            q.qpc = now.QuadPart;
            const uint64_t* o = reinterpret_cast<const uint64_t*>(self);
            const uint64_t inp = o[3] ? o[3] : o[2];
            if (PatchHeldVt(inp)) Tick(inp, now.QuadPart, (double)freq.QuadPart);
        }
        return g_orig(self, res, t, slot);
    }
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
    const uintptr_t fn = info.base + Grva(kScanRva);
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
    g_mode = want;  // set before the jmp is live: the first hooked calls must already run the chosen mode (#121117)
    g_base = info.base;
    MemoryBarrier();
    InterlockedExchange64((volatile LONG64*)fn, v);  // one aligned 8-byte store: no thread sees half a jmp
    VirtualProtect((void*)fn, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), (void*)fn, 8);
    g_on = true;
    g_log("HOLDKEYGAP: installed at +%X (%s) - [General] HoldKeyGap = %d (a move key between two uneven frames is tested "
          "instead of skipped; max gap %.1f frames)",
          (unsigned)(fn - info.base), info.knownBuild ? "v2.02" : "found by pattern", want, kMaxGap);
}

}  // namespace holdkeygap
