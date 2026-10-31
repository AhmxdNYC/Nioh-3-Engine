// bkstats.cpp - BKS back-end census + KL kick-limit knob. See bkstats.h and notes\renderer\backend.md.
#include "bkstats.h"
#include "gamerva.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace bkstats {
namespace {

// ---- v2.02 layout (notes\renderer\backend.md; decompiles of 254110, 25426C, 418B94, 418C70, 418F40, 1A7C3E0, 4930C8,
//      4920E0, 86F760, 86F828, 252758, 7F64DC, 7F6904) ----------------------------------------------------------------
constexpr uintptr_t kRvaGfx = 0x45BA588;        // GfxDevice* g (== KtglContext*)
constexpr uintptr_t kRvaClock = 0x45BA780;      // EngineClock*: +12h HasQpc (u8), +18h QPC frequency
constexpr uintptr_t kRvaDrainBusy = 0x45CE8A0;  // u8 g_DispatcherDrainBusy, written every iteration of 418F40
// dispatcher ring (GfxDevice+3BE0h)
constexpr uintptr_t kRingCap = 0x3C00, kRingWrite = 0x3C04, kRingRead = 0x3C08, kRingInFlight = 0x3C10;
constexpr uintptr_t kStreamSize = 0x3C28, kStreamGen = 0x3C60, kFlushLimit = 0x3C70;
// last-frame stream stats snapshot (copy of +3C78h..+3CF8h made by CmdStreamEndFrame)
constexpr uintptr_t kSnapDraws = 0x3D00, kSnapKicks = 0x3D04, kSnapBytes = 0x3D0C;
constexpr uintptr_t kSnapFullTot = 0x3D10, kSnapFullCnt = 0x3D20, kSnapDrainTot = 0x3D28, kSnapDrainMax = 0x3D30,
                    kSnapDrainCnt = 0x3D38;
// FOV (frame-overlap viability, notes/renderer/frame-overlap.md): the other main-thread waits of the same snapshot block
//   +3CB8h stat (owner not identified)                       -> snapshot +3D40h tot / +3D50h cnt
//   +3CD0h handle wait 7D99FC (spin until the dispatcher has replayed past an 0xA7 marker; sub-stream wrap, sync points)
//                                                             -> snapshot +3D58h tot / +3D60h max / +3D68h cnt
//   +3CE8h sync-point GPU wait 3D2B90 (handle replayed, then GfxWaitFenceValue on that list's fence; e.g. the meshlet
//          scene-buffer set reuse in MeshletUpdateSceneBuffers 4BBEC)  -> snapshot +3D70h tot / +3D78h max / +3D80h cnt
constexpr uintptr_t kSnapX3Tot = 0x3D40, kSnapX3Cnt = 0x3D50, kSnapHwTot = 0x3D58, kSnapHwMax = 0x3D60, kSnapHwCnt = 0x3D68,
                    kSnapSwTot = 0x3D70, kSnapSwMax = 0x3D78, kSnapSwCnt = 0x3D80;
// frame-boundary sizes the overlap design depends on
constexpr uintptr_t kSyncTables = 0x3C18;   // u32 per-frame sync-point tables (ring indexed by frame counter +3C1Ch)
constexpr uintptr_t kFrameSlots = 0x4240;   // u32 engine frame slots (BufferCount)
constexpr uintptr_t kPresentThr = 0x3F80;   // u8 present on the present thread
constexpr uintptr_t kSubStreams = 0x3E28;   // u32 sub-streams (+74D8h array, token 0x90, reset at Present)
// SynCtx = g+10h; CmdListRing = SynCtx+5A0h = g+5B0h
constexpr uintptr_t kListRing = 0x5B0;
constexpr uintptr_t kLrCount = kListRing + 0x18;      // u32 command lists in the ring (grows on demand, 4931B0)
constexpr uintptr_t kLrEclCap = kListRing + 0x98;     // u32 ECL item ring capacity (== list count)
constexpr uintptr_t kLrEclInFlight = kListRing + 0xA4;
constexpr uintptr_t kLrEclHigh = kListRing + 0xA8;    // u32 ECL in-flight high-water
constexpr uintptr_t kLrStallEcl = kListRing + 0xB0;   // stat: EclRingPushItem blocked (ECL ring full)
constexpr uintptr_t kLrIdleSpin = kListRing + 0xC8;   // stat: RingWaitIdleSpin in GfxSubmitFrameCmdList (main)
constexpr uintptr_t kLrRecycle = kListRing + 0xF8;    // stat: allocator / list recycle (fence wait) in 4920E0
constexpr uintptr_t kLrSubmits = kListRing + 0x11C;   // u32 lists handed to the ECL ring (cumulative)
// KtglCache (g+61A0h)
constexpr uintptr_t kCache = 0x61A0;
constexpr uintptr_t kCacheTok95 = kCache + 0x9F8;     // stat: 7F6904 (tok 0x95 per-frame cache upload, under SRW +CF8h)
constexpr uintptr_t kCacheJob = kCache + 0x950;       // stat: per request in the tile-mapping job drain 492310
constexpr uintptr_t kCacheLock = kCache + 0xCF8;      // SRWLOCK shared by tok 0x95 / exclusive by cache insert 250ADC, 2EEF440

struct Stat {
    uint64_t tot = 0, max = 0;
    uint32_t cnt = 0;
};

struct Cfg {
    int enabled = 0, sampleUs = 1000, sec = 30, delay = 20;
} g_cfg;

void (*g_log)(const char*, ...) = nullptr;
uintptr_t g_base = 0;
bool g_known = false;
volatile LONG g_klDiv = 0;       // live
volatile LONG g_klStock = -1;    // engine value before KL touched it (-1 = never read)
volatile LONG g_klWritten = -1;  // last value KL wrote
volatile LONG g_threadUp = 0;
CRITICAL_SECTION g_klCs;

// ---- SEH readers (no C++ objects in these) ------------------------------------------------------------------------
bool RdU8(uintptr_t a, uint8_t& v) {
    if (a < 0x10000) return false;
    __try {
        v = *reinterpret_cast<const volatile uint8_t*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool RdU32(uintptr_t a, uint32_t& v) {
    if (a < 0x10000) return false;
    __try {
        v = *reinterpret_cast<const volatile uint32_t*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool RdU64(uintptr_t a, uint64_t& v) {
    if (a < 0x10000) return false;
    __try {
        v = *reinterpret_cast<const volatile uint64_t*>(a);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool WrU32(uintptr_t a, uint32_t v) {
    if (a < 0x10000) return false;
    __try {
        InterlockedExchange(reinterpret_cast<volatile LONG*>(a), (LONG)v);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}
bool RdStat(uintptr_t a, Stat& s) { return RdU64(a, s.tot) && RdU64(a + 8, s.max) && RdU32(a + 0x10, s.cnt); }

uintptr_t Gfx() {
    uint64_t g = 0;
    return RdU64(g_base + Grva(kRvaGfx), g) ? (uintptr_t)g : 0;
}

// engine stat ticks -> ms (QPC when EngineClock::HasQpc, else clock() = ms)
double g_tickMs = 0.0;
void InitClock() {
    uint64_t clk = 0, f = 0;
    uint8_t has = 0;
    if (RdU64(g_base + Grva(kRvaClock), clk) && clk && RdU8(clk + 0x12, has) && has && RdU64(clk + 0x18, f) && f) {
        g_tickMs = 1000.0 / (double)f;
    } else {
        LARGE_INTEGER q;
        QueryPerformanceFrequency(&q);
        g_tickMs = has ? 1000.0 / (double)q.QuadPart : 1.0;
    }
}

// ---- KL -----------------------------------------------------------------------------------------------------------
uint32_t KlTarget(uint32_t stock, int div) { return div <= 1 ? stock : std::max<uint32_t>(16u, stock / (uint32_t)div); }

// Apply the current g_klDiv. Called from SetTuning (LiveCtl thread) and the census thread; serialized by g_klCs.
// Returns the value now in the field (0 = no engine).
uint32_t KlApply(bool logChange) {
    EnterCriticalSection(&g_klCs);
    uint32_t cur = 0, now = 0;
    const uintptr_t g = Gfx();
    if (g && RdU32(g + kFlushLimit, cur) && cur) {
        // the engine rewrote the field (first sight, or device re-create) -> that is the new stock value
        if (g_klStock < 0 || (LONG)cur != g_klWritten) {
            if (g_klStock >= 0 && logChange && (LONG)cur != g_klStock)
                g_log("BKS: KL: engine set CmdFlushLimit %u (was stock %ld, KL wrote %ld)", cur, g_klStock, g_klWritten);
            g_klStock = (LONG)cur;
        }
        const int div = (int)g_klDiv;
        const uint32_t want = KlTarget((uint32_t)g_klStock, div);
        if (want != cur) WrU32(g + kFlushLimit, want);
        g_klWritten = (LONG)want;
        RdU32(g + kFlushLimit, now);
    }
    LeaveCriticalSection(&g_klCs);
    return now;
}

// ---- census -------------------------------------------------------------------------------------------------------
struct Acc {
    // per-frame snapshots
    uint32_t frames = 0;
    uint64_t draws = 0, kicks = 0, bytes = 0, fullTot = 0, fullCnt = 0, drainTot = 0, drainMax = 0;
    uint64_t x3Tot = 0, x3Cnt = 0, hwTot = 0, hwMax = 0, hwCnt = 0, swTot = 0, swMax = 0, swCnt = 0, swFrames = 0;
    // sampler
    uint64_t n = 0, nDrain = 0, nRec = 0, nStarvedRec = 0, nStarved = 0, nFull = 0, backlog = 0, nEclFull = 0, nLockHeld = 0,
             nLockWait = 0, nLockExcl = 0;
};

const char* Ms(char* b, size_t n, double v) {
    snprintf(b, n, "%.2f", v);
    return b;
}

DWORD WINAPI CensusThread(LPVOID) {
    Sleep((DWORD)g_cfg.delay * 1000);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    InitClock();
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /*HIGH_RESOLUTION*/, TIMER_ALL_ACCESS);
    const int us = g_cfg.enabled ? std::clamp(g_cfg.sampleUs, 250, 5000) : 100000;  // KL-only: 10 Hz re-assert
    if (g_cfg.enabled)
        g_log("BKS: census every %d us (%s timer), report every %d s, stat tick %.6f ms; KL_Div %ld", us,
              timer ? "high-resolution" : "Sleep", g_cfg.sec, g_tickMs, g_klDiv);
    LARGE_INTEGER f, last, now;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&last);
    Acc a;
    uint32_t lastGen = 0, lastSnapDraws = 0, lastSnapCnt = 0;
    uint64_t lastSnapDrain = 0;
    bool snapPending = false, haveGen = false;
    Stat s0Ecl, s0Idle, s0Rec, s0Tok, s0Job;
    uint32_t sub0 = 0;
    bool base0 = false;
    int klTick = 0;
    for (;;) {
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)us * 10;
            SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE);
            WaitForSingleObject(timer, INFINITE);
        } else {
            Sleep((DWORD)std::max(1, us / 1000));
        }
        if (++klTick >= std::max(1, 100000 / us)) {  // KL re-assert ~10x / s (cheap: two reads)
            klTick = 0;
            if (g_klDiv != 0 || (g_klWritten >= 0 && g_klWritten != g_klStock)) KlApply(true);
        }
        if (!g_cfg.enabled) continue;
        const uintptr_t g = Gfx();
        if (!g) continue;
        // per-frame snapshot: generation +1 at the final kick, the snapshot is copied right after -> read it next tick
        uint32_t gen = 0;
        if (RdU32(g + kStreamGen, gen)) {
            if (snapPending) {
                uint32_t d = 0, k = 0, b = 0, fc = 0, dc = 0;
                uint64_t ft = 0, dt = 0, dm = 0;
                if (RdU32(g + kSnapDraws, d) && RdU32(g + kSnapKicks, k) && RdU32(g + kSnapBytes, b) && RdU64(g + kSnapFullTot, ft) &&
                    RdU32(g + kSnapFullCnt, fc) && RdU64(g + kSnapDrainTot, dt) && RdU64(g + kSnapDrainMax, dm) &&
                    RdU32(g + kSnapDrainCnt, dc) && !(d == lastSnapDraws && dt == lastSnapDrain && dc == lastSnapCnt)) {
                    ++a.frames;
                    a.draws += d;
                    a.kicks += k;
                    a.bytes += b;
                    a.fullTot += ft;
                    a.fullCnt += fc;
                    a.drainTot += dt;
                    a.drainMax = std::max<uint64_t>(a.drainMax, dm);
                    uint64_t v = 0;
                    uint32_t c = 0;
                    if (RdU64(g + kSnapX3Tot, v) && RdU32(g + kSnapX3Cnt, c)) a.x3Tot += v, a.x3Cnt += c;
                    if (RdU64(g + kSnapHwTot, v) && RdU32(g + kSnapHwCnt, c)) {
                        a.hwTot += v, a.hwCnt += c;
                        if (RdU64(g + kSnapHwMax, v)) a.hwMax = std::max(a.hwMax, v);
                    }
                    if (RdU64(g + kSnapSwTot, v) && RdU32(g + kSnapSwCnt, c)) {
                        a.swTot += v, a.swCnt += c;
                        if (c) ++a.swFrames;
                        if (RdU64(g + kSnapSwMax, v)) a.swMax = std::max(a.swMax, v);
                    }
                    lastSnapDraws = d;
                    lastSnapDrain = dt;
                    lastSnapCnt = dc;
                }
                snapPending = false;
            }
            if (haveGen && gen != lastGen) snapPending = true;
            lastGen = gen;
            haveGen = true;
        }
        // queue state sample
        uint8_t drain = 0;
        uint32_t w = 0, r = 0, inf = 0, cap = 0, eInf = 0, eCap = 0;
        uint64_t lockWord = 0;
        if (RdU8(g_base + Grva(kRvaDrainBusy), drain) && RdU32(g + kRingWrite, w) && RdU32(g + kRingRead, r) &&
            RdU32(g + kRingInFlight, inf) && RdU32(g + kRingCap, cap)) {
            ++a.n;
            const bool starved = (w == r) && inf == 0;
            if (drain) ++a.nDrain;
            else {
                ++a.nRec;
                if (starved) ++a.nStarvedRec;
            }
            if (starved) ++a.nStarved;
            if (cap && w - r >= cap) ++a.nFull;
            a.backlog += inf;  // segments handed over and not finished (queued + the one being replayed)
            if (RdU32(g + kLrEclInFlight, eInf) && RdU32(g + kLrEclCap, eCap) && eCap && eInf >= eCap) ++a.nEclFull;
            if (RdU64(g + kCacheLock, lockWord) && lockWord) {
                ++a.nLockHeld;
                if (lockWord & 2) ++a.nLockWait;                 // waiters queued (an exclusive or shared acquirer is blocked)
                else if ((lockWord >> 4) == 0) ++a.nLockExcl;    // locked, share count 0 = exclusive owner
            }
        }
        QueryPerformanceCounter(&now);
        const double el = (double)(now.QuadPart - last.QuadPart) / (double)f.QuadPart;
        if (!base0) {
            base0 = RdStat(g + kLrStallEcl, s0Ecl) && RdStat(g + kLrIdleSpin, s0Idle) && RdStat(g + kLrRecycle, s0Rec) &&
                    RdStat(g + kCacheTok95, s0Tok) && RdStat(g + kCacheJob, s0Job) && RdU32(g + kLrSubmits, sub0);
            last = now;
            a = Acc();
            continue;
        }
        if (el < g_cfg.sec) continue;
        Stat e, i, rc, t, j;
        uint32_t sub = 0, lists = 0, eh = 0, lim = 0, ssz = 0;
        if (!(RdStat(g + kLrStallEcl, e) && RdStat(g + kLrIdleSpin, i) && RdStat(g + kLrRecycle, rc) && RdStat(g + kCacheTok95, t) &&
              RdStat(g + kCacheJob, j) && RdU32(g + kLrSubmits, sub))) {
            base0 = false;
            continue;
        }
        RdU32(g + kLrCount, lists);
        RdU32(g + kLrEclHigh, eh);
        RdU32(g + kFlushLimit, lim);
        RdU32(g + kStreamSize, ssz);
        // engine counters that the engine itself resets (per frame / per snapshot) go backwards: count from 0 then (CPU5 showed
        // underflowed values for the recycle / idle-spin / tok95 timers)
        auto D = [](uint64_t cur, uint64_t prev) { return cur >= prev ? cur - prev : cur; };
        const double fr = a.frames ? (double)a.frames : 1.0;
        auto perF = [&](uint64_t ticks) { return (double)ticks * g_tickMs / fr; };
        auto pct = [&](uint64_t k, uint64_t of) { return of ? 100.0 * (double)k / (double)of : 0.0; };
        char b1[16], b2[16], b3[16], b4[16], b5[16], b6[16], b7[16], b8[16], b9[16];
        g_log("BKS: %.0f s | frames %u (%.1f/s) | main: kicks/f %.1f draws/f %.0f streamKB/f %.0f (buf %u KB) limit %u (stock %ld, "
              "KL_Div %ld) | ring cap %u full %s ms/f (n %llu) | drain %s ms/f (max %s) | submit ECL-idle spin %s ms/f",
              el, a.frames, a.frames / el, a.kicks / fr, a.draws / fr, a.bytes / fr / 1024.0, ssz / 1024u, lim, g_klStock, g_klDiv,
              cap, Ms(b1, 16, perF(a.fullTot)), (unsigned long long)a.fullCnt, Ms(b2, 16, perF(a.drainTot)),
              Ms(b3, 16, (double)a.drainMax * g_tickMs), Ms(b4, 16, perF(D(i.tot, s0Idle.tot))));
        g_log("BKS: disp: starved-while-recording %.1f %% (of %llu recording samples) | starved %.1f %% | main in drain %.1f %% | "
              "backlog %.2f segments | ring full %.1f %% | ECL stall %s ms/f (n %u, max %s ms) ECL full %.1f %% | lists %u ECL hw %u "
              "submits/f %.1f | recycle wait %s ms/f (n %u) | tok95 cache upload %s ms/f | tile job %s ms/f | cache SRW +CF8h held %.1f %% "
              "excl %.1f %% waiters %.1f %% | samples %llu",
              pct(a.nStarvedRec, a.nRec), (unsigned long long)a.nRec, pct(a.nStarved, a.n), pct(a.nDrain, a.n),
              a.n ? (double)a.backlog / (double)a.n : 0.0, pct(a.nFull, a.n), Ms(b5, 16, perF(D(e.tot, s0Ecl.tot))), (unsigned)D(e.cnt, s0Ecl.cnt),
              Ms(b6, 16, (double)e.max * g_tickMs), pct(a.nEclFull, a.n), lists, eh, D(sub, sub0) / fr,
              Ms(b7, 16, perF(D(rc.tot, s0Rec.tot))), (unsigned)D(rc.cnt, s0Rec.cnt), Ms(b8, 16, perF(D(t.tot, s0Tok.tot))),
              Ms(b9, 16, perF(D(j.tot, s0Job.tot))), pct(a.nLockHeld, a.n), pct(a.nLockExcl, a.n), pct(a.nLockWait, a.n),
              (unsigned long long)a.n);
        {
            uint32_t st = 0, fs = 0, ns = 0;
            uint8_t pt = 0;
            RdU32(g + kSyncTables, st);
            RdU32(g + kFrameSlots, fs);
            RdU32(g + kSubStreams, ns);
            RdU8(g + kPresentThr, pt);
            char c1[16], c2[16], c3[16], c4[16], c5[16];
            g_log("BKS: fov: handle wait %s ms/f (n/f %.2f, max %s) | sync-point GPU wait %s ms/f (n/f %.2f, in %.0f %% of frames, max %s) | "
                  "stat3CB8 %s ms/f (n/f %.2f) | sync tables %u frame slots %u sub-streams %u present thread %u",
                  Ms(c1, 16, perF(a.hwTot)), a.hwCnt / fr, Ms(c2, 16, (double)a.hwMax * g_tickMs), Ms(c3, 16, perF(a.swTot)), a.swCnt / fr,
                  pct(a.swFrames, a.frames), Ms(c4, 16, (double)a.swMax * g_tickMs), Ms(c5, 16, perF(a.x3Tot)), a.x3Cnt / fr, st, fs, ns,
                  (unsigned)pt);
        }
        s0Ecl = e;
        s0Idle = i;
        s0Rec = rc;
        s0Tok = t;
        s0Job = j;
        sub0 = sub;
        a = Acc();
        last = now;
    }
}

}  // namespace

bool SetTuning(const char* key, const char* val, char* reply, size_t n) {
    if (_stricmp(key, "KL_Div") != 0) return false;
    const int v = atoi(val);
    if (v < 0 || v > 64) {
        snprintf(reply, n, "KL_Div must be 0 (engine value) or 2..64 (got %s)", val);
        return true;
    }
    if (!g_known) {
        snprintf(reply, n, "KL_Div: unknown game build, not applied");
        return true;
    }
    InterlockedExchange(&g_klDiv, v <= 1 ? 0 : v);
    const uint32_t now = KlApply(false);
    snprintf(reply, n, "KL_Div = %ld (CmdFlushLimit %u, stock %ld)", g_klDiv, now, g_klStock);
    g_log("LIVE tuning: %s", reply);
    return true;
}

namespace {
bool HitchReady() {
    static volatile LONG ok = -1;  // -1 not checked
    if (ok < 0 && g_known && g_base) {
        static const uint8_t kEcl[] = {0x4C, 0x8D, 0x81, 0xB0, 0x00, 0x00, 0x00};
        static const uint8_t kDrain[] = {0x48, 0x8D, 0x42, 0x28};
        const bool same = !memcmp((void*)(g_base + Grva(0x86F775)), kEcl, sizeof kEcl) &&
                          !memcmp((void*)(g_base + Grva(0x418F46)), kDrain, sizeof kDrain);
        if (same && g_tickMs <= 0) InitClock();
        ok = same ? 1 : 0;
    }
    return ok == 1 && g_tickMs > 0;
}
bool ReadMs(const uintptr_t* at, int n, double* out) {
    for (int i = 0; i < n; ++i) {
        uint64_t t = 0;
        if (!RdU64(at[i], t)) return false;
        out[i] = (double)t * g_tickMs;
    }
    return true;
}
}  // namespace

bool CumWaitsMs(double out[4]) {
    const uintptr_t g = HitchReady() ? Gfx() : 0;
    if (!g) return false;
    const uintptr_t at[4] = {g + kLrStallEcl, g + kLrIdleSpin, g + kLrRecycle, g + kCacheTok95};
    return ReadMs(at, 4, out);
}

bool LastFrameWaitsMs(double out[6]) {
    const uintptr_t g = HitchReady() ? Gfx() : 0;
    if (!g) return false;
    const uintptr_t at[6] = {g + kSnapDrainTot, g + kSnapDrainMax, g + kSnapFullTot, g + kSnapHwTot, g + kSnapSwTot, g + kSnapSwMax};
    return ReadMs(at, 6, out);
}

void Start(const StartInfo& info) {
    g_log = info.log;
    g_base = info.base;
    g_known = info.knownBuild;
    InitializeCriticalSection(&g_klCs);
    auto I = [&](const wchar_t* sec, const wchar_t* key, int def) { return (int)GetPrivateProfileIntW(sec, key, def, info.iniPath); };
    g_cfg.enabled = I(L"Profiler", L"Backend", 0);
    g_cfg.sampleUs = I(L"Profiler", L"BackendSampleUs", 1000);
    g_cfg.sec = std::clamp(I(L"Profiler", L"BackendSec", I(L"Profiler", L"ReportSec", 30)), 5, 600);
    g_cfg.delay = std::clamp(I(L"Profiler", L"BackendDelaySec", 20), 0, 600);
    const int kl = I(L"Tuning", L"KL_Div", 0);
    g_klDiv = (kl >= 2 && kl <= 64) ? kl : 0;
    const bool live = I(L"Debug", L"LiveCtl", 0) != 0;
    if (!g_cfg.enabled && !g_klDiv && !live) return;
    if (!g_known) {
        g_log("BKS: unknown game build - Backend census and KL off");
        return;
    }
    // layout self-check (v2.02 bytes): EclRingPush passes CmdListRing+B0h as its stall stat; WaitDispatcherDrainSpin's stat
    // is stream-stats+28h; DispatcherRingBusy reads read +28h / write +24h / in-flight +30h of the ring
    static const uint8_t kEcl[] = {0x4C, 0x8D, 0x81, 0xB0, 0x00, 0x00, 0x00};
    static const uint8_t kDrain[] = {0x48, 0x8D, 0x42, 0x28};
    static const uint8_t kBusy[] = {0x8B, 0x41, 0x28, 0x39, 0x41, 0x24, 0x74, 0x03, 0xB0, 0x01, 0xC3, 0x83, 0x79, 0x30, 0x00};
    if (memcmp((void*)(g_base + Grva(0x86F775)), kEcl, sizeof kEcl) || memcmp((void*)(g_base + Grva(0x418F46)), kDrain, sizeof kDrain) ||
        memcmp((void*)(g_base + Grva(0x418F90)), kBusy, sizeof kBusy)) {
        g_log("BKS: layout check FAILED (86F775 / 418F46 / 418F90) - Backend census and KL off");
        return;
    }
    if (HANDLE h = CreateThread(nullptr, 0, CensusThread, nullptr, 0, nullptr)) {
        using SetDesc = HRESULT(WINAPI*)(HANDLE, PCWSTR);
        if (auto sd = reinterpret_cast<SetDesc>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")))
            sd(h, L"Nioh3EngineFix BKS");
        CloseHandle(h);
        g_threadUp = 1;
    }
    g_log("BKS: %s; KL_Div %ld at boot (live: Tuning.KL_Div)", g_cfg.enabled ? "back-end census armed" : "census off", g_klDiv);
}

}  // namespace bkstats
