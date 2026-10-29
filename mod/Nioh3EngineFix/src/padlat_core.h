// padlat_core.h - PadLatency core (1.7.35): pad state-change detection, the lock-free feeds and the matcher that ties a pad
// change to the Present (and GPU completion) of the logic frame that consumed it. Header-only, no Windows/D3D types, so
// tests/padlat_harness.cpp drives exactly this code with fake XInput states and a synthetic frame timeline.
// Runtime glue (XInput IAT hook, fence waiter, report thread): padlat.cpp. Design: notes\pad-latency.md.
#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <vector>

namespace padlat {

// XINPUT_GAMEPAD fields (same layout as <xinput.h>, kept here so the core has no Windows dependency)
struct Pad {
    uint16_t buttons = 0;
    uint8_t lt = 0, rt = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

enum : uint32_t { kClsButtons = 1, kClsTriggers = 2, kClsSticks = 4 };
constexpr int kTriggerThreshold = 30;                 // XINPUT_GAMEPAD_TRIGGER_THRESHOLD
constexpr int kLeftDeadzone = 7849, kRightDeadzone = 8689;  // XINPUT_GAMEPAD_LEFT/RIGHT_THUMB_DEADZONE

// Stick as a coarse state: 0 inside the deadzone, 1..8 = octant outside it. Raw stick values jitter by a few units every
// poll; only a deadzone crossing or an octant change counts as a "stick change" (flick start/stop, direction change).
inline int StickZone(int x, int y, int dz) {
    const int64_t m2 = (int64_t)x * x + (int64_t)y * y;
    if (m2 <= (int64_t)dz * dz) return 0;
    // octant by comparing |x| and |y| against tan(22.5 deg) ~ 0.41421 (integer: 70/169)
    const int64_t ax = x < 0 ? -(int64_t)x : x, ay = y < 0 ? -(int64_t)y : y;
    int dir;
    if (ay * 169 <= ax * 70) dir = x >= 0 ? 0 : 4;              // E / W
    else if (ax * 169 <= ay * 70) dir = y >= 0 ? 2 : 6;         // N / S
    else dir = x >= 0 ? (y >= 0 ? 1 : 7) : (y >= 0 ? 3 : 5);    // diagonals
    return 1 + dir;
}

// change classes between two consecutive reads of one pad (0 = no change the game could act on)
inline uint32_t Classify(const Pad& a, const Pad& b) {
    uint32_t c = 0;
    if (a.buttons != b.buttons) c |= kClsButtons;
    if ((a.lt > kTriggerThreshold) != (b.lt > kTriggerThreshold) || (a.rt > kTriggerThreshold) != (b.rt > kTriggerThreshold))
        c |= kClsTriggers;
    if (StickZone(a.lx, a.ly, kLeftDeadzone) != StickZone(b.lx, b.ly, kLeftDeadzone) ||
        StickZone(a.rx, a.ry, kRightDeadzone) != StickZone(b.rx, b.ry, kRightDeadzone))
        c |= kClsSticks;
    return c;
}

// ---- lock-free multi-producer / single-consumer ring. A slot is valid when its seq == index + 1; a slow consumer
// loses the oldest records (counted), producers never wait.
template <class T, size_t N>
struct Ring {
    static_assert((N & (N - 1)) == 0, "N must be a power of two");
    struct Slot {
        std::atomic<uint64_t> seq{0};
        T v{};
    };
    Slot s[N];
    std::atomic<uint64_t> head{0};
    uint64_t tail = 0;      // consumer only
    uint64_t lost = 0;      // consumer only
    void Push(const T& v) {
        const uint64_t i = head.fetch_add(1, std::memory_order_relaxed);
        Slot& x = s[i & (N - 1)];
        x.seq.store(0, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        x.v = v;
        x.seq.store(i + 1, std::memory_order_release);
    }
    // consumer: calls f(v) for every record written since the last call, in index order
    template <class F>
    void Drain(F&& f) {
        const uint64_t h = head.load(std::memory_order_acquire);
        if (h - tail > N) { lost += h - tail - N; tail = h - N; }
        while (tail < h) {
            Slot& x = s[tail & (N - 1)];
            const uint64_t q = x.seq.load(std::memory_order_acquire);
            if (q != tail + 1) {
                if (q > tail + 1) { ++lost; ++tail; continue; }  // overwritten by a newer lap
                break;                                          // not written yet: next drain
            }
            T v = x.v;
            std::atomic_thread_fence(std::memory_order_acquire);
            if (x.seq.load(std::memory_order_relaxed) != tail + 1) { ++lost; ++tail; continue; }  // torn by a newer lap
            f(v);
            ++tail;
        }
    }
};

struct InputRec {
    int64_t q;        // QPC when the game had the state in hand (after XInputGetState returned) / poll-round start
    uint32_t kind;    // 1 pad change, 2 poll round start
    uint32_t cls;     // kCls* (kind 1), reads in the previous round (kind 2)
};
struct PresentRec {
    int64_t q;        // QPC at the hooked Present call
    uint64_t fence;   // fence value signalled on the game's queue right before it (0 = none)
};
struct GpuRec {
    uint64_t fence;
    int64_t q;        // QPC when the waiter saw the fence reach this value
};

// ---- per-thread read tracker (the game reads its pads on [App]InputUpdateWorker only; the tracker tolerates others)
struct PadTracker {
    Pad last[4]{};
    uint32_t packet[4]{};
    bool have[4]{};
    int64_t lastEnd = 0, roundStart = 0;
    uint32_t readsThisRound = 0;
    int64_t gapTicks = 0;  // > this between the end of one read and the start of the next = a new poll round
    // t0/t1 = QPC before / after the XInputGetState call; returns the change classes (0 = none)
    template <class PushFn>
    uint32_t OnRead(int64_t t0, int64_t t1, unsigned idx, uint32_t pkt, const Pad& p, PushFn&& push) {
        if (!lastEnd || t0 - lastEnd > gapTicks) {
            if (roundStart) push(InputRec{t0, 2u, readsThisRound});
            roundStart = t0;
            readsThisRound = 0;
        }
        ++readsThisRound;
        lastEnd = t1;
        if (idx >= 4) return 0;
        if (!have[idx]) {  // first read of this slot (or after a disconnect): baseline, no event
            have[idx] = true;
            last[idx] = p;
            packet[idx] = pkt;
            return 0;
        }
        if (pkt == packet[idx] && !memcmp(&p, &last[idx], sizeof(Pad))) return 0;
        const uint32_t c = Classify(last[idx], p);
        last[idx] = p;
        packet[idx] = pkt;
        if (c) push(InputRec{t1, 1u, c});
        return c;
    }
    void OnDisconnected(unsigned idx) {
        if (idx < 4) have[idx] = false;
    }
};

// ---- stats ----------------------------------------------------------------------------------------------------------
inline double Pct(std::vector<double>& v, double p) {  // nearest rank; -1 = no data
    if (v.empty()) return -1;
    std::sort(v.begin(), v.end());
    size_t k = (size_t)(p / 100.0 * (double)v.size() + 0.999999);
    if (k < 1) k = 1;
    if (k > v.size()) k = v.size();
    return v[k - 1];
}

// whole-session histogram, 0.05 ms bins up to 500 ms (+ overflow bin)
struct Hist {
    static constexpr int kBins = 10001;
    static constexpr double kBinMs = 0.05;
    uint32_t b[kBins]{};
    uint64_t n = 0;
    void Add(double ms) {
        int i = ms <= 0 ? 0 : (int)(ms / kBinMs);
        if (i >= kBins) i = kBins - 1;
        ++b[i];
        ++n;
    }
    double Pct(double p) const {  // upper edge of the bin holding the nearest-rank sample; -1 = no data
        if (!n) return -1;
        uint64_t k = (uint64_t)(p / 100.0 * (double)n + 0.999999);
        if (k < 1) k = 1;
        uint64_t acc = 0;
        for (int i = 0; i < kBins; ++i) {
            acc += b[i];
            if (acc >= k) return (i + 1) * kBinMs;
        }
        return (kBins)*kBinMs;
    }
};

struct Window {
    uint32_t events = 0, buttons = 0, triggers = 0, sticks = 0, matched = 0, unmatched = 0, gpuMissing = 0;
    std::vector<double> inPresent, inGpu, inPresentButtons, poll;
    uint64_t reads = 0, rounds = 0;
    void Clear() { *this = Window{}; }
};

// ---- matcher (report thread only) ------------------------------------------------------------------------------------
// Frame hand-off, same chain as the TS "K->P sample->present" median (dllmain TsAnalyse; notes\input-latency.md 6.3):
//   pad change read at t  ->  C = first pad-consumed stamp after t (the logic frame that uses this read)
//   -> L = first limiter exit after C -> W = first render begin after L -> P = first present hand-off after W
//   -> present = first hooked DXGI Present at or after P (P itself when the Present hook is not running)
//   -> GPU done = completion time of the fence signalled on the game's queue right before that Present.
// TS ring ids: 1 K, 2 C, 3 W, 4 B, 5 P, 6 L.
struct Matcher {
    std::deque<int64_t> C, L, W, P;  // sorted QPC stamps
    std::deque<PresentRec> pres;     // sorted by q
    std::deque<GpuRec> gpu;          // sorted by fence value
    struct Ev { int64_t t; uint32_t cls; int64_t present; uint64_t fence; };
    std::deque<Ev> pend;
    int64_t freq = 1;
    int64_t lastPresentSeen = 0;     // QPC of the newest hooked Present (0 = Present hook never fed us)
    uint64_t gpuDoneMax = 0;         // highest completed fence value seen
    bool gpuActive = false;          // the fence waiter delivered at least one completion
    Window win;
    Hist hPresent, hGpu, hPoll, hPresentButtons;
    uint64_t totEvents = 0, totMatched = 0, totUnmatched = 0, totGpuMissing = 0, totButtons = 0, totTriggers = 0, totSticks = 0;
    uint64_t totReads = 0, totRounds = 0;
    int64_t lastRoundStart = 0;

    static void InsSorted(std::deque<int64_t>& d, int64_t q) {
        if (d.empty() || d.back() <= q) { d.push_back(q); return; }
        d.insert(std::upper_bound(d.begin(), d.end(), q), q);
    }
    void AddStamp(int64_t q, int id) {
        if (q <= 0) return;
        switch (id) {
            case 2: InsSorted(C, q); break;
            case 6: InsSorted(L, q); break;
            case 3: InsSorted(W, q); break;
            case 5: InsSorted(P, q); break;
            default: break;
        }
    }
    void AddInput(const InputRec& r) {
        if (r.kind == 2) {
            if (lastRoundStart && r.q > lastRoundStart) {
                const double ms = (double)(r.q - lastRoundStart) * 1000.0 / (double)freq;
                win.poll.push_back(ms);
                hPoll.Add(ms);
            }
            lastRoundStart = r.q;
            win.reads += r.cls;
            totReads += r.cls;
            ++win.rounds;
            ++totRounds;
            return;
        }
        ++win.events;
        ++totEvents;
        if (r.cls & kClsButtons) ++win.buttons, ++totButtons;
        if (r.cls & kClsTriggers) ++win.triggers, ++totTriggers;
        if (r.cls & kClsSticks) ++win.sticks, ++totSticks;
        pend.push_back(Ev{r.q, r.cls, 0, 0});
    }
    void AddPresent(const PresentRec& r) {
        if (pres.empty() || pres.back().q <= r.q) pres.push_back(r);
        else pres.insert(std::upper_bound(pres.begin(), pres.end(), r, [](const PresentRec& a, const PresentRec& b) { return a.q < b.q; }), r);
        if (r.q > lastPresentSeen) lastPresentSeen = r.q;
    }
    void AddGpu(const GpuRec& r) {
        gpuActive = true;
        if (r.fence > gpuDoneMax) gpuDoneMax = r.fence;
        if (gpu.empty() || gpu.back().fence < r.fence) gpu.push_back(r);
    }
    static int64_t Next(const std::deque<int64_t>& d, int64_t after) {  // first stamp > after, -1 = none yet
        if (after < 0) return -1;
        auto it = std::upper_bound(d.begin(), d.end(), after);
        return it == d.end() ? -1 : *it;
    }
    // GPU completion time of a fence value: the first record with fence >= v (the waiter stamps the highest completed
    // value it saw; lower values completed at or before that time). -1 = not completed yet.
    int64_t GpuDone(uint64_t v) const {
        auto it = std::lower_bound(gpu.begin(), gpu.end(), v, [](const GpuRec& a, uint64_t x) { return a.fence < x; });
        return it == gpu.end() ? -1 : it->q;
    }
    // settle = QPC up to which every feed is complete (the drain time minus a margin); expire = ticks after which a pending
    // change without a full chain is given up
    void Resolve(int64_t settle, int64_t expire) {
        const double toMs = 1000.0 / (double)freq;
        while (!pend.empty()) {
            Ev& e = pend.front();
            bool done = false, give = settle - e.t > expire;
            if (!e.present) {
                const int64_t c = Next(C, e.t), l = Next(L, c), w = Next(W, l), p = Next(P, w);
                int64_t pr = -1;
                uint64_t fv = 0;
                if (p >= 0) {
                    if (lastPresentSeen) {
                        auto it = std::lower_bound(pres.begin(), pres.end(), p, [](const PresentRec& a, int64_t x) { return a.q < x; });
                        if (it != pres.end()) pr = it->q, fv = it->fence;
                    } else {
                        pr = p;  // no Present hook: the engine's present hand-off stamp stands in
                    }
                }
                if (pr >= 0 && pr - p > freq / 4) {  // no hooked Present within 250 ms of the hand-off (hook skipped
                    ++win.unmatched;                  // frames / swapchain switch): not this frame's Present - drop
                    ++totUnmatched;
                    pend.pop_front();
                    continue;
                }
                if (pr >= 0 && pr <= settle) {
                    e.present = pr;
                    e.fence = fv;
                    const double ms = (double)(pr - e.t) * toMs;
                    win.inPresent.push_back(ms);
                    hPresent.Add(ms);
                    if (e.cls & kClsButtons) win.inPresentButtons.push_back(ms), hPresentButtons.Add(ms);
                    ++win.matched;
                    ++totMatched;
                } else if (give) {
                    ++win.unmatched;
                    ++totUnmatched;
                    pend.pop_front();
                    continue;
                } else {
                    break;  // keep order: later changes cannot be resolved before this one
                }
            }
            if (!e.fence || !gpuActive) {
                done = true;
            } else {
                const int64_t g = GpuDone(e.fence);
                if (g >= 0) {
                    const double ms = (double)(g - e.t) * toMs;
                    win.inGpu.push_back(ms);
                    hGpu.Add(ms);
                    done = true;
                } else if (give) {
                    ++win.gpuMissing;
                    ++totGpuMissing;
                    done = true;
                }
            }
            if (!done) break;
            pend.pop_front();
        }
        // prune stamps nobody can need any more
        const int64_t keep = (pend.empty() ? settle : std::min(settle, pend.front().t)) - expire;
        auto prune = [&](std::deque<int64_t>& d) {
            while (d.size() > 2 && d[1] < keep) d.pop_front();
        };
        prune(C), prune(L), prune(W), prune(P);
        while (pres.size() > 2 && pres[1].q < keep) pres.pop_front();
        const uint64_t needFence = pend.empty() ? gpuDoneMax : (pend.front().fence ? pend.front().fence : 0);
        while (gpu.size() > 2 && gpu[1].fence < needFence && gpu[1].q < keep) gpu.pop_front();
    }
};

}  // namespace padlat
