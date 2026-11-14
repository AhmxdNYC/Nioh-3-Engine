// perfsnap_core.h - pure math of the performance snapshot (perfsnap.cpp; human #97631/#97632). No Windows, no heap, no globals:
// unit-tested by tests/perfsnap_test.cpp.
//
// Summarize: the frames of the last `windowMs` (newest last) -> average fps, 1% low (fps of the mean of the slowest 1% of frame
// times, at least one frame), mean / p99 / max frame time.
// Classify: CPU- vs GPU-bound from the mod's existing wait counters over the same window:
//   gpuWait = GPU fence wait (GF) + present wait (W6) per frame on the game's presenting thread. A thread that waits for the GPU
//   a quarter of its frame or more is GPU-bound; under 10 % the GPU is not what holds the frame back, so CPU-bound; between = mixed.
//   A game fps cap (REN_FPS) that the average reaches within 5 % wins over both: "capped".
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace perfsnap {

struct FrameStats {
    int frames = 0;
    double spanMs = 0;              // sum of the frame times used
    double avgFps = 0, low1Fps = 0; // low1: 1000 / mean of the slowest ceil(1 %) frame times
    double avgMs = 0, p99Ms = 0, maxMs = 0;
};

// ms[0..n) chronological (newest last). Uses the newest frames until their sum reaches windowMs. scratch: n floats.
inline FrameStats Summarize(const float* ms, int n, double windowMs, float* scratch) {
    FrameStats s;
    if (!ms || n <= 0 || !scratch) return s;
    int k = 0;
    double sum = 0;
    for (int i = n - 1; i >= 0 && sum < windowMs; --i) {
        const float v = ms[i];
        if (!(v > 0.f) || v > 10000.f) continue;  // a hole or a load stall far outside a frame
        scratch[k++] = v;
        sum += v;
    }
    if (!k) return s;
    // insertion sort, descending (n <= a few thousand; no heap on the crash path)
    for (int i = 1; i < k; ++i) {
        const float v = scratch[i];
        int j = i - 1;
        while (j >= 0 && scratch[j] < v) { scratch[j + 1] = scratch[j]; --j; }
        scratch[j + 1] = v;
    }
    s.frames = k;
    s.spanMs = sum;
    s.avgMs = sum / k;
    s.avgFps = s.avgMs > 0 ? 1000.0 / s.avgMs : 0;
    s.maxMs = scratch[0];
    const int worst = (k + 99) / 100;  // ceil(1 %)
    double ws = 0;
    for (int i = 0; i < worst; ++i) ws += scratch[i];
    s.low1Fps = ws > 0 ? 1000.0 * worst / ws : 0;
    s.p99Ms = scratch[worst - 1];  // the fastest of the slowest 1 %
    return s;
}

// deltas of the mod's existing counters over the window (all per frame unless named otherwise)
struct WindowCost {
    double spanS = 0;              // wall seconds between the two samples
    double frames = 0;             // game presents in that span
    double gameThreadMs = 0;       // CPU time of the presenting thread per frame (GetThreadTimes)
    double procCores = 0;          // whole-process CPU time / wall time (cores in use)
    double gpuFenceMs = 0;         // GF: game waited on a GPU fence
    double presentWaitMs = 0;      // W6: waitable-swapchain present wait
    double dsRetryMs = 0;          // DS: retry-spin sleep
    double limiterPerFrame = 0;    // L1 limiter sleeps per frame
    bool bk = false;               // engine back-end waits known
    double bkMs[4] = {};           // ECL ring full, submit idle spin, list recycle fence wait, tok 0x95 cache upload
};

enum Bound : int { kUnknown = 0, kCpu = 1, kGpu = 2, kCapped = 3, kMixed = 4 };
inline const char* BoundName(int b) {
    switch (b) {
        case kCpu: return "cpu";
        case kGpu: return "gpu";
        case kCapped: return "capped";
        case kMixed: return "mixed";
        default: return "unknown";
    }
}

// fpsCap: the game's REN_FPS target (0 = unknown). why: one short reason line.
inline int Classify(const FrameStats& f, const WindowCost& w, int fpsCap, char* why, size_t cap) {
    auto say = [&](const char* fmt, double a, double b) {
        if (why && cap) snprintf(why, cap, fmt, a, b);
    };
    if (f.frames < 30 || f.spanMs < 2000 || w.frames < 1 || w.spanS < 1.0) {
        say("only %.0f frames in %.1f s (menu, loading or just started)", (double)f.frames, f.spanMs / 1000.0);
        return kUnknown;
    }
    const double gpuWait = w.gpuFenceMs + w.presentWaitMs;
    const double frac = f.avgMs > 0 ? gpuWait / f.avgMs : 0;
    if (fpsCap > 0 && f.avgFps >= 0.95 * fpsCap) {
        say("at the %.0f fps cap; waits on GPU %.0f%% of the frame", (double)fpsCap, frac * 100);
        return kCapped;
    }
    if (frac >= 0.25) {
        say("game thread waits on the GPU %.0f%% of the frame (%.1f ms)", frac * 100, gpuWait);
        return kGpu;
    }
    if (frac < 0.10) {
        say("GPU waits only %.0f%% of the frame; game thread busy %.0f%%", frac * 100,
            f.avgMs > 0 ? 100.0 * w.gameThreadMs / f.avgMs : 0);
        return kCpu;
    }
    say("GPU waits %.0f%% of the frame; game thread busy %.0f%%", frac * 100, f.avgMs > 0 ? 100.0 * w.gameThreadMs / f.avgMs : 0);
    return kMixed;
}

// ---- per-thread CPU (human #97659, TASK #97673) ----
// One game thread over the rolling window: CPU time / wall time, in % of one core (100 = one core busy all the time).
struct ThreadUse {
    uint32_t tid = 0;
    double pct = 0;
    char name[48] = "";
};
inline double CorePct(uint64_t cpu100ns, double wallS) { return wallS > 0 ? (double)cpu100ns / 1e7 / wallS * 100.0 : 0; }
// CPU time gained between the sample `window` ticks ago (or the slot's first one) and this tick. t[]/ok[] are rings of `hist`
// entries indexed tick % hist. A tick whose GetThreadTimes failed is ok = false and never enters a delta: neither as the newest
// sample nor as the old one (a stale ring entry would be 32 ticks old and inflate the %). false = no valid pair; oldTick = old.
inline bool WindowDelta(const uint64_t* t, const bool* ok, int hist, int tick, int first, int window, int& oldTick, uint64_t& d) {
    if (!t || !ok || hist <= 0 || tick < 0 || first < 0 || window <= 0 || window >= hist) return false;
    int old = tick - window;
    if (old < first) old = first;
    if (old >= tick || !ok[tick % hist] || !ok[old % hist]) return false;
    const uint64_t a = t[old % hist], b = t[tick % hist];
    if (b < a) return false;
    oldTick = old, d = b - a;
    return true;
}
// Top k of in[0..n) by pct, descending (ties: lower tid first). Returns how many were written (<= k).
inline int RankThreads(const ThreadUse* in, int n, ThreadUse* top, int k) {
    if (!in || !top || n <= 0 || k <= 0) return 0;
    int m = 0;
    for (int i = 0; i < n; ++i) {
        const ThreadUse& c = in[i];
        if (!(c.pct >= 0)) continue;
        int pos = m;
        while (pos > 0 && (top[pos - 1].pct < c.pct || (top[pos - 1].pct == c.pct && top[pos - 1].tid > c.tid))) --pos;
        if (pos >= k) continue;
        for (int j = (m < k ? m : k - 1); j > pos; --j) top[j] = top[j - 1];
        top[pos] = c;
        if (m < k) ++m;
    }
    return m;
}

// Bounded string builder (no heap). Overflow truncates; n never passes cap - 1.
// over = something did not fit: the text is cut, so a JSON user must not publish it.
struct Buf {
    char* b;
    size_t cap, n = 0;
    bool over = false;
    Buf(char* p, size_t c) : b(p), cap(c) { if (cap) b[0] = 0; else over = true; }
    void raw(const char* s) {
        if (!cap) return;
        while (*s && n + 1 < cap) b[n++] = *s++;
        if (*s) over = true;
        b[n] = 0;
    }
    template <typename... A>
    void f(const char* fmt, A... a) {
        if (!cap || n + 1 >= cap) { over = true; return; }
        int k = snprintf(b + n, cap - n, fmt, a...);
        if (k < 0 || (size_t)k >= cap - n) over = true;
        if (k > 0) n += (size_t)k < cap - n ? (size_t)k : cap - n - 1;
    }
    void str(const char* s, size_t maxLen = 200) {  // JSON string: quoted, escaped, ASCII only
        raw("\"");
        for (size_t i = 0; s && s[i] && i < maxLen; ++i) {
            const unsigned char c = (unsigned char)s[i];
            if (c == '"' || c == '\\') { char e[3] = {'\\', (char)c, 0}; raw(e); }
            else if (c < 0x20 || c >= 0x7F) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c < 0x20 ? c : '?'); raw(c < 0x20 ? e : "?"); }
            else { char e[2] = {(char)c, 0}; raw(e); }
        }
        raw("\"");
    }
};

}  // namespace perfsnap
