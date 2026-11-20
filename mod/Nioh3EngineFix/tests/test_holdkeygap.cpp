// test_holdkeygap.cpp - HoldKeyGap window rule (holdkeygap.h WindowStart). Simulates the key scan over uneven frame steps:
// stock [t, t+step) skips a key in the gap, the widened window tests it exactly once; fixed dt is unchanged.
#undef NDEBUG
#include <cassert>
#include <cstdio>
#include "../src/holdkeygap.h"

static int FiresStock(const float* steps, int n, float t0, float key, bool fix) {
    // scan sees (t, s_n); then t += s_{n+1}  (window of this frame uses the step the frame came in with)
    int fired = 0;
    float t = t0, prevEnd = 0, prevT = 0;
    bool same = false;
    for (int i = 0; i < n; ++i) {
        const float s = steps[i];
        float lo = fix ? holdkeygap::WindowStart(same, prevEnd, prevT, t, s) : t;
        if (lo <= key && key < t + s) ++fired;
        same = true; prevEnd = t + s; prevT = t;
        if (i + 1 < n) t += steps[i + 1];
    }
    return fired;
}

// Mode 3 (the shipped fix): the game counts held frames (c) and the stock getter returns c * 60 / setting. Simulate a
// 120 setting running at real fps; return the getter value the fix gives after holdMs of real hold.
static int Mode3Held(float realFps, int setting, float holdMs, int* stockOut) {
    const float d = 1.0f / realFps;
    float held = 0.0f;
    uint32_t last = 0, c = 0;
    for (float t = 0.0f; t + 1e-6f < holdMs / 1000.0f; t += d) {
        ++c;
        held = holdkeygap::NextHeldSec(held, c, last, last ? d : 0.0f, d);
        last = c;
    }
    const int stock = (int)((float)c * 60.0f / (float)setting);
    if (stockOut) *stockOut = stock;
    return holdkeygap::HeldReal(stock, held, c, last);
}

static void TestMode3() {
    int stock = 0;
    // #120730 case: 120 setting, ~90 fps real, 220 ms hold -> stock 9-10 (< 12, MA key fails), fix >= 12 (13 real 1/60 s)
    const int fixed = Mode3Held(90.0f, 120, 220.0f, &stock);
    assert(stock < 12);
    assert(fixed >= 12 && fixed <= 14);
    // setting == real fps: no change
    assert(Mode3Held(60.0f, 60, 220.0f, &stock) == stock);
    assert(Mode3Held(120.0f, 120, 300.0f, &stock) == stock);
    // never below stock
    for (float fps = 30.0f; fps <= 240.0f; fps += 7.0f)
        for (float ms = 20.0f; ms <= 2000.0f; ms += 37.0f) assert(Mode3Held(fps, 120, ms, &stock) >= stock);
    // release / untracked / count went back: stock
    assert(holdkeygap::NextHeldSec(0.5f, 0, 30, 0.01f, 0.01f) == 0.0f);
    assert(holdkeygap::HeldReal(7, 1.0f, 0, 10) == 7);
    assert(holdkeygap::HeldReal(7, 1.0f, 10, 0) == 7);
    assert(holdkeygap::HeldReal(7, 1.0f, 5, 10) == 7);
    // new press re-seeds from the count (c < last): 3 frames at 10 ms EMA
    assert(holdkeygap::NextHeldSec(2.0f, 3, 50, 0.01f, 0.01f) > 0.029f && holdkeygap::NextHeldSec(2.0f, 3, 50, 0.01f, 0.01f) < 0.031f);
    // pause: a 1 s gap is not added; cap at 30 fps worth per frame
    assert(holdkeygap::NextHeldSec(0.1f, 11, 10, 1.0f, 0.01f) == 0.1f);
    assert(holdkeygap::NextHeldSec(0.3f, 11, 10, 0.2f, 0.01f) <= 11.0f / 30.0f + 1e-6f);
    std::printf("PASS mode 3\n");
}

// GH #147: the tracker table recycles stale slots instead of filling up for the rest of the session.
static void TestTrackers() {
    int64_t t[4] = {100, 200, 300, 400};
    const int64_t stale = 1000;
    assert(holdkeygap::PickTracker(t, 2, 4, 450, stale) == 2);   // free slot first
    assert(holdkeygap::PickTracker(t, 4, 4, 1150, stale) == 0);  // full: 100 and 200 are stale, oldest wins
    assert(holdkeygap::PickTracker(t, 4, 4, 1250, stale) == 0);
    assert(holdkeygap::PickTracker(t, 4, 4, 1050, stale) == -1); // full and all ticked in the last 'stale': none
    int64_t u[4] = {5000, 900, 5000, 5000};
    assert(holdkeygap::PickTracker(u, 4, 4, 5100, stale) == 1);  // only the idle one is taken, busy ones (the player) kept
    // 40 input objects come and go (enemies, loads); a new player object after them still gets a slot
    int64_t ring[32] = {};
    int n = 0;
    for (int obj = 0; obj < 40; ++obj) {
        const int64_t now = 10000 + obj * 3000;
        const int i = holdkeygap::PickTracker(ring, n, 32, now, stale);
        assert(i >= 0);
        if (i == n) ++n;
        ring[i] = now;
    }
    assert(holdkeygap::PickTracker(ring, n, 32, 10000 + 41 * 3000, stale) >= 0);
    std::printf("PASS trackers\n");
}

int main() {
    TestMode3();
    TestTrackers();
    const float fixed[] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
    assert(FiresStock(fixed, 8, 10.0f, 12.0f, false) == 1);
    assert(FiresStock(fixed, 8, 10.0f, 12.0f, true) == 1);
    // 11.82 (step 0.12) -> 12.48 (step 0.66): stock misses 12.0, fix fires once
    const float uneven[] = {0.12f, 0.66f, 0.5f, 0.5f};
    assert(FiresStock(uneven, 4, 11.82f, 12.0f, false) == 0);
    assert(FiresStock(uneven, 4, 11.82f, 12.0f, true) == 1);
    // overlap (big then small): stock may fire twice, fix never narrows (same count as stock)
    const float shrink[] = {0.9f, 0.2f, 0.2f};
    assert(FiresStock(shrink, 3, 11.5f, 12.3f, true) == FiresStock(shrink, 3, 11.5f, 12.3f, false));
    // new move / seek / long jump: stock
    assert(holdkeygap::WindowStart(false, 11.0f, 10.9f, 12.0f, 0.5f) == 12.0f);
    assert(holdkeygap::WindowStart(true, 11.0f, 10.9f, 20.0f, 0.5f) == 20.0f);
    assert(holdkeygap::WindowStart(true, 13.0f, 12.9f, 12.0f, 0.5f) == 12.0f);
    assert(holdkeygap::WindowStart(true, 11.0f, 10.9f, 12.0f, 0.0f) == 12.0f);
    assert(holdkeygap::WindowStart(true, 11.94f, 11.82f, 12.48f, 0.66f) == 11.94f);
    // randomized: with the fix every integer key in range fires at least once for any step sequence
    unsigned r = 12345;
    for (int trial = 0; trial < 2000; ++trial) {
        float steps[64];
        for (int i = 0; i < 64; ++i) { r = r * 1103515245u + 12345u; steps[i] = 0.2f + (float)((r >> 8) % 1000) / 1000.0f; }
        float total = 0; for (int i = 1; i < 64; ++i) total += steps[i];
        for (int k = 1; k < (int)total - 1; ++k) assert(FiresStock(steps, 64, 0.0f, (float)k, true) >= 1);
    }
    std::printf("PASS test_holdkeygap\n");
    return 0;
}
