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

int main() {
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
