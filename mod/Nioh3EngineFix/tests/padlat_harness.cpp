// padlat_harness.cpp - offline tests of PadLatency (src/padlat_core.h, src/padlat.cpp). Not a game test.
// Built and run by tests/run_padlat_harness.py; must print ALL PASSED.
//   1. change classes: buttons, trigger threshold, stick deadzone / octant, stick noise ignored
//   2. synthetic 60 Hz frame timeline (stock kick order and IK order) driven through the real PadTracker -> Ring ->
//      Matcher with the game's 6 XInputGetState reads per frame: every change must match its frame's Present with the
//      exact expected input->present / input->GPU-done, poll interval = the logic frame, reads/round = 6
//   3. no Present hook (P stamp stands in), unmatched expiry, lost GPU completions, 250 ms present guard
//   4. Ring: 4 producers x 200k records with a concurrent consumer: no torn or duplicated records
//   5. the real XInputGetState hook on a fake IAT slot: result and state passed through unchanged, counters, cost per call
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <xinput.h>

#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

#include "../src/padlat.h"
#include "../src/padlat_core.h"

using namespace padlat;

static int g_fail = 0;
#define CHECK(c, ...)                         \
    do {                                      \
        if (!(c)) {                           \
            ++g_fail;                         \
            printf("FAIL %s:%d: ", __FILE__, __LINE__); \
            printf(__VA_ARGS__);              \
            printf("\n");                     \
        }                                     \
    } while (0)

static bool Near(double a, double b, double eps = 0.002) { return std::fabs(a - b) <= eps; }

// ---- 1 ----
static void TestClassify() {
    Pad a, b;
    CHECK(Classify(a, b) == 0, "identical");
    b.lx = 300, b.ly = -200, b.rx = 1000;  // stick noise inside the deadzones
    CHECK(Classify(a, b) == 0, "noise counted");
    b = a, b.buttons = 0x1000;
    CHECK(Classify(a, b) == kClsButtons, "button");
    b = a, b.lt = 25;
    CHECK(Classify(a, b) == 0, "trigger below threshold");
    b.lt = 31;
    CHECK(Classify(a, b) == kClsTriggers, "trigger crossing");
    b = a, b.rx = 20000;
    CHECK(Classify(a, b) == kClsSticks, "stick out of deadzone");
    Pad c = b;
    c.rx = 21000, c.ry = 500;
    CHECK(Classify(b, c) == 0, "same octant");
    c.rx = 0, c.ry = 20000;
    CHECK(Classify(b, c) == kClsSticks, "octant change E->N");
    CHECK(StickZone(-20000, -20000, kLeftDeadzone) == 1 + 5, "SW octant");
    CHECK(StickZone(7849, 0, kLeftDeadzone) == 0 && StickZone(7850, 0, kLeftDeadzone) == 1, "deadzone edge");
    b = a, b.buttons = 1, b.lt = 200, b.ly = 30000;
    CHECK(Classify(a, b) == (kClsButtons | kClsTriggers | kClsSticks), "all classes");
}

// ---- 2/3: synthetic timeline. Fake QPC at 10 MHz. ----
constexpr int64_t kF = 10000000;
static int64_t Ms(double ms) { return (int64_t)std::llround(ms * kF / 1000.0); }

struct Sim {
    Ring<InputRec, 4096> in;
    Ring<PresentRec, 1024> pr;
    Ring<GpuRec, 1024> gr;
    PadTracker trk;
    Matcher m;
    std::vector<std::pair<int64_t, int>> stamps;  // the TS ring stand-in
    Sim() {
        trk.gapTicks = kF / 500;
        m.freq = kF;
    }
    void Read(int64_t t, unsigned idx, uint32_t pkt, const Pad& p, bool connected = true) {
        auto push = [this](const InputRec& r) { in.Push(r); };
        if (connected) trk.OnRead(t, t + Ms(0.01), idx, pkt, p, push);
        else trk.OnRead(t, t + Ms(0.01), 4u, 0, Pad{}, push), trk.OnDisconnected(idx);
    }
    void Pump(int64_t now) {
        in.Drain([this](const InputRec& r) { m.AddInput(r); });
        pr.Drain([this](const PresentRec& r) { m.AddPresent(r); });
        gr.Drain([this](const GpuRec& r) { m.AddGpu(r); });
        for (auto& s : stamps)
            if (s.first <= now) m.AddStamp(s.first, s.second);
        stamps.erase(std::remove_if(stamps.begin(), stamps.end(), [now](const std::pair<int64_t, int>& s) { return s.first <= now; }),
                     stamps.end());
        m.Resolve(now - kF / 33, 2 * kF);
    }
};

struct TimelineOpts {
    bool ik = false, presentHook = true, gpu = true, gpuBatch = false, dropC = false;
};

// Runs 600 logic frames. Pad state changes on every 7th frame (alternating button / trigger / stick). Returns the sim
// after the last report-thread pump.
static Sim* RunTimeline(const TimelineOpts& o, double* expPresent, double* expGpu) {
    Sim* s = new Sim();
    const double T = 1000.0 / 60.0;
    const int N = 600;
    Pad pad;
    uint32_t pkt = 1;
    uint64_t fence = 0;
    int64_t nextPump = Ms(100);
    // build the whole timeline, then replay in time order with pumps every 100 ms
    struct E { int64_t t; int kind; int n; };
    std::vector<E> ev;
    for (int n = 0; n < N; ++n) {
        const double S = n * T;
        if (!o.dropC) s->stamps.push_back({Ms(S + 0.2), 2});
        s->stamps.push_back({Ms(S + 16.0), 6});
        s->stamps.push_back({Ms(S + 16.5), 3});
        s->stamps.push_back({Ms(S + 16.5 + 1.0), 4});
        s->stamps.push_back({Ms(S + 23.5), 5});
        ev.push_back({Ms(o.ik ? S + 0.05 : S + 8.0), 0, n});  // the poll round (stock: at the kick; IK: before C)
        ev.push_back({Ms(S + 23.6), 1, n});                  // the hooked Present of logic frame n
        ev.push_back({Ms(S + 23.6 + 9.0), 2, n});            // GPU done
    }
    std::sort(ev.begin(), ev.end(), [](const E& a, const E& b) { return a.t < b.t; });
    for (const E& e : ev) {
        while (e.t >= nextPump) {
            s->Pump(nextPump);
            nextPump += Ms(100);
        }
        if (e.kind == 0) {
            if (e.n % 7 == 3) {  // change this round
                ++pkt;
                switch ((e.n / 7) % 3) {
                    case 0: pad.buttons ^= 0x1000; break;
                    case 1: pad.rt = pad.rt > 30 ? 0 : 255; break;
                    default: pad.ly = pad.ly ? 0 : 32000; break;
                }
            } else if (e.n % 2) {
                pad.lx = (int16_t)(e.n % 5) * 100;  // noise inside the deadzone, new packet number
                ++pkt;
            }
            // the game's reads: PadPollDevices slot 0, scan slots 0..3 (1..3 empty), then the active slot 0
            int64_t t = e.t;
            s->Read(t, 0, pkt, pad);
            for (unsigned k = 0; k < 4; ++k) t += Ms(0.03), s->Read(t, k, pkt, pad, k == 0);
            t += Ms(0.03), s->Read(t, 0, pkt, pad);
        } else if (e.kind == 1) {
            if (o.presentHook) s->pr.Push(PresentRec{e.t, o.gpu ? ++fence : 0});
        } else if (o.gpu && o.presentHook) {
            if (!o.gpuBatch || e.n % 2) s->gr.Push(GpuRec{(uint64_t)e.n + 1, e.t});  // batch: every 2nd completion seen
        }
    }
    for (int k = 0; k < 40; ++k) s->Pump(nextPump), nextPump += Ms(100);
    // expected: a change read in round n (t_in = read time + 0.01 ms) is consumed by C(n+1) (stock) / C(n) (IK)
    const double tin = (o.ik ? 0.05 : 8.0) + 0.01;
    const double consumedFrame = o.ik ? 0 : T;
    *expPresent = consumedFrame + (o.presentHook ? 23.6 : 23.5) - tin;
    *expGpu = consumedFrame + 23.6 + 9.0 - tin;
    return s;
}

static void TestTimeline() {
    for (int ik = 0; ik < 2; ++ik) {
        TimelineOpts o;
        o.ik = ik != 0;
        double ep = 0, eg = 0;
        Sim* s = RunTimeline(o, &ep, &eg);
        Window& w = s->m.win;
        uint32_t expChanges = 0;
        for (int n = 0; n < 600; ++n) expChanges += n % 7 == 3;
        CHECK(w.events == expChanges, "ik=%d changes %u, expected %u (noise must not count)", ik, w.events, expChanges);
        CHECK(w.matched == w.events && w.unmatched == 0, "ik=%d matched %u of %u (unmatched %u)", ik, w.matched, w.events, w.unmatched);
        CHECK(w.buttons + w.triggers + w.sticks == w.events && w.buttons > 0 && w.triggers > 0 && w.sticks > 0, "class counts");
        bool allExact = true;
        for (double v : w.inPresent) allExact &= Near(v, ep);
        CHECK(allExact, "ik=%d input->present not all %.3f ms (p50 %.3f)", ik, ep, Pct(w.inPresent, 50));
        bool gpuExact = w.inGpu.size() == w.inPresent.size();
        for (double v : w.inGpu) gpuExact &= Near(v, eg);
        CHECK(gpuExact, "ik=%d input->GPU-done (n %zu) not all %.3f ms", ik, w.inGpu.size(), eg);
        const double p50 = Pct(w.inPresent, 50), p99 = Pct(w.inPresent, 99);
        CHECK(Near(p50, ep) && Near(p99, ep), "ik=%d percentiles %.3f/%.3f", ik, p50, p99);
        CHECK(Near(s->m.hPresent.Pct(50), std::ceil(ep / Hist::kBinMs) * Hist::kBinMs, 0.051), "hist p50 %.3f vs %.3f", s->m.hPresent.Pct(50), ep);
        const double i50 = Pct(w.poll, 50), i99 = Pct(w.poll, 99);
        CHECK(Near(i50, 1000.0 / 60.0, 0.01) && Near(i99, 1000.0 / 60.0, 0.01), "poll interval %.3f / %.3f", i50, i99);
        CHECK(w.rounds >= 598 && w.reads == w.rounds * 6, "reads/round %llu / %llu", (unsigned long long)w.reads, (unsigned long long)w.rounds);
        CHECK(Near(Pct(w.inPresentButtons, 50), ep), "buttons p50");
        printf("timeline %s: %u changes matched, input->present %.3f ms, input->GPU-done %.3f ms, poll %.3f ms, %llu reads/round\n",
               ik ? "IK   " : "stock", w.matched, p50, Pct(w.inGpu, 50), i50, (unsigned long long)(w.reads / (w.rounds ? w.rounds : 1)));
        delete s;
    }
    {  // no Present hook: the P stamp stands in, no GPU timing
        TimelineOpts o;
        o.presentHook = false;
        double ep = 0, eg = 0;
        Sim* s = RunTimeline(o, &ep, &eg);
        Window& w = s->m.win;
        CHECK(w.matched == w.events && w.events > 80 && w.inGpu.empty(), "no-hook: matched %u of %u, gpu %zu", w.matched, w.events, w.inGpu.size());
        CHECK(Near(Pct(w.inPresent, 50), ep) && Near(Pct(w.inPresent, 100), ep), "no-hook: %.3f vs %.3f", Pct(w.inPresent, 50), ep);
        printf("timeline no Present hook: input->P stamp %.3f ms (expected %.3f)\n", Pct(w.inPresent, 50), ep);
        delete s;
    }
    {  // GPU waiter sees every 2nd completion: the skipped frames take the next observation (later, never earlier)
        TimelineOpts o;
        o.gpuBatch = true;
        double ep = 0, eg = 0;
        Sim* s = RunTimeline(o, &ep, &eg);
        Window& w = s->m.win;
        bool ok = w.inGpu.size() == w.matched;
        for (double v : w.inGpu) ok &= v >= eg - 0.002 && v <= eg + 1000.0 / 60.0 + 0.002;
        CHECK(ok, "gpu batch: n %zu, p50 %.3f (exact %.3f)", w.inGpu.size(), Pct(w.inGpu, 50), eg);
        delete s;
    }
    {  // logic frames never consume (no C stamps): every change expires as unmatched, nothing hangs
        TimelineOpts o;
        o.dropC = true;
        double ep = 0, eg = 0;
        Sim* s = RunTimeline(o, &ep, &eg);
        Window& w = s->m.win;
        CHECK(w.matched == 0 && w.unmatched == w.events && s->m.pend.empty(), "dropC: matched %u unmatched %u pending %zu", w.matched,
              w.unmatched, s->m.pend.size());
        CHECK(s->m.C.size() <= 2 && s->m.P.size() <= 40 && s->m.pres.size() <= 40, "stamps pruned (P %zu, presents %zu)", s->m.P.size(),
              s->m.pres.size());
        delete s;
    }
    {  // 250 ms guard: a Present long after the hand-off (hook gap) is not taken as the frame's Present
        Matcher m;
        m.freq = kF;
        m.AddInput(InputRec{Ms(1), 1, kClsButtons});
        m.AddStamp(Ms(2), 2), m.AddStamp(Ms(3), 6), m.AddStamp(Ms(4), 3), m.AddStamp(Ms(5), 5);
        m.AddPresent(PresentRec{Ms(0.5), 0});
        m.AddPresent(PresentRec{Ms(400), 0});
        m.Resolve(Ms(1000), 2 * kF);
        CHECK(m.win.unmatched == 1 && m.win.matched == 0, "guard: matched %u unmatched %u", m.win.matched, m.win.unmatched);
    }
}

// ---- 4: ring under contention ----
static void TestRing() {
    static Ring<InputRec, 4096> r;
    const int P = 4, K = 200000;
    std::vector<std::thread> th;
    std::atomic<int> done{0};
    std::vector<int64_t> last(P, -1);
    uint64_t got = 0, bad = 0;
    for (int p = 0; p < P; ++p)
        th.emplace_back([p, &done]() {
            for (int k = 0; k < K; ++k) r.Push(InputRec{(int64_t)p << 40 | k, (uint32_t)p, (uint32_t)(k * 2654435761u)});
            done.fetch_add(1);
        });
    auto consume = [&](const InputRec& x) {
        const int p = (int)(x.q >> 40), k = (int)(x.q & 0xFFFFFFFFFF);
        if (p < 0 || p >= P || (uint32_t)p != x.kind || x.cls != (uint32_t)(k * 2654435761u) || k <= last[p]) ++bad;
        else last[p] = k, ++got;
    };
    while (done.load() < P) r.Drain(consume);
    for (auto& t : th) t.join();
    r.Drain(consume);
    CHECK(bad == 0, "ring: %llu torn/duplicate/out-of-order records", (unsigned long long)bad);
    CHECK(got + r.lost == (uint64_t)P * K, "ring: got %llu + lost %llu != %d", (unsigned long long)got, (unsigned long long)r.lost, P * K);
    printf("ring: %llu records received, %llu lost to a slow consumer (allowed), 0 torn\n", (unsigned long long)got, (unsigned long long)r.lost);
}

// ---- 5: the real hook ----
static XINPUT_STATE g_fake[4];
static DWORD g_fakeCalls = 0;
static DWORD WINAPI FakeXInputGetState(DWORD idx, XINPUT_STATE* st) {
    ++g_fakeCalls;
    if (idx != 0) return ERROR_DEVICE_NOT_CONNECTED;
    *st = g_fake[0];
    return ERROR_SUCCESS;
}
using XGS = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);

static void TestHook() {
    static void* slot = (void*)&FakeXInputGetState;  // an IAT-like slot
    CHECK(padlat::TestHookSlot(&slot, (uint8_t*)1), "hook not installed on the fake slot");
    CHECK(slot != (void*)&FakeXInputGetState, "slot not swapped");
    XGS call = (XGS)slot;
    // pass-through: same result, same bytes, for connected and empty slots
    bool same = true;
    for (int k = 0; k < 2000; ++k) {
        g_fake[0].dwPacketNumber = k / 3;
        g_fake[0].Gamepad.wButtons = (WORD)((k / 30) & 1 ? 0x1000 : 0);
        g_fake[0].Gamepad.sThumbLX = (SHORT)(k % 7 * 50);
        g_fake[0].Gamepad.bRightTrigger = (BYTE)(k % 300 < 150 ? 0 : 200);
        for (DWORD i = 0; i < 4; ++i) {
            XINPUT_STATE a, b;
            memset(&a, 0xCD, sizeof(a));
            memset(&b, 0xCD, sizeof(b));
            const DWORD ra = call(i, &a);
            const DWORD rb = FakeXInputGetState(i, &b);
            same &= ra == rb && !memcmp(&a, &b, sizeof(a));
        }
    }
    CHECK(same, "hook changed a result or a state");
    uint64_t reads = 0, changes = 0;
    padlat::TestCounters(&reads, &changes);
    CHECK(reads == 8000, "reads %llu", (unsigned long long)reads);
    // button flips every 30 iterations (66), the trigger crosses every 150 (13, always together with a button flip): 66 changes
    CHECK(changes == 66, "changes %llu", (unsigned long long)changes);
    // cost: hooked vs direct, 1M calls
    LARGE_INTEGER f, a, b, c;
    QueryPerformanceFrequency(&f);
    XINPUT_STATE st;
    QueryPerformanceCounter(&a);
    for (int k = 0; k < 1000000; ++k) FakeXInputGetState(0, &st);
    QueryPerformanceCounter(&b);
    for (int k = 0; k < 1000000; ++k) call(0, &st);
    QueryPerformanceCounter(&c);
    const double direct = (double)(b.QuadPart - a.QuadPart) * 1e9 / f.QuadPart / 1e6, hooked = (double)(c.QuadPart - b.QuadPart) * 1e9 / f.QuadPart / 1e6;
    printf("hook: pass-through identical for 8000 calls, %llu changes; cost %.0f ns/call (direct %.0f ns) - the game makes ~6 calls per frame\n",
           (unsigned long long)changes, hooked - direct, direct);
    CHECK(hooked - direct < 2000, "hook too slow: %.0f ns", hooked - direct);
}

int main() {
    TestClassify();
    TestTimeline();
    TestRing();
    TestHook();
    if (g_fail) {
        printf("FAILED (%d)\n", g_fail);
        return 1;
    }
    printf("PADLAT HARNESS: ALL PASSED\n");
    return 0;
}
