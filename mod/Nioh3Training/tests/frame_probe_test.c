// user #22356: frame probe bookkeeping (frame_probe.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/frame_probe_test.c -o build/frame_probe_test.exe && build/frame_probe_test.exe
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/frame_probe.h"
static int near(double a, double b) { return a - b < 0.001 && b - a < 0.001; }
int main(void) {
    FpRun r; double ran, su, ac, rc;
    // two-hit move: start 1000 ms, hits at +500 / +700, windows end at +550 / +760, next action at +1500
    FpRunStart(&r, 0x4E6E, 1000.0, 10);
    assert(r.hits == 0 && r.act == 0x4E6E && r.end[0] < 0);
    assert(FpRunHit(&r, 1500.0, 1) == 0 && FpRunHit(&r, 1700.0, 0) == 1 && r.great == 1);
    FpRunHitEnd(&r, 0, 1550.0); FpRunHitEnd(&r, 1, 1760.0); FpRunHitEnd(&r, 1, 1999.0) /* first end kept */;
    assert(near(r.end[1], 760.0));
    assert(FpRunSummary(&r, 2500.0, &ran, &su, &ac, &rc));
    assert(near(ran, 1500.0) && near(su, 500.0) && near(ac, 260.0) && near(rc, 740.0));
    assert(near(FpFrames60(500.0), 30.0));
    // no end seen: active runs to the last creation
    FpRunStart(&r, 0x4E50, 0.0, 0); FpRunHit(&r, 400.0, 1); FpRunHit(&r, 450.0, 1);
    assert(FpRunSummary(&r, 1000.0, &ran, &su, &ac, &rc) && near(su, 400.0) && near(ac, 50.0) && near(rc, 550.0));
    // no hit
    FpRunStart(&r, 0x4E21, 0.0, 0); assert(!FpRunSummary(&r, 300.0, &ran, &su, &ac, &rc) && near(ran, 300.0) && su < 0);
    // overflow: hits past FP_MAX_HITS counted but not stored
    FpRunStart(&r, 1, 0.0, 0); for (int i = 0; i < FP_MAX_HITS; ++i) assert(FpRunHit(&r, 10.0 * i, 0) == i);
    assert(FpRunHit(&r, 999.0, 0) == -1 && r.hits == FP_MAX_HITS + 1);
    FpRunHitEnd(&r, -1, 5.0); FpRunHitEnd(&r, FP_MAX_HITS, 5.0);  // ignored
    // rate limit: 40 per second burst then refill
    FpRate b = {0, 0}; int ok = 0;
    for (int i = 0; i < 100; ++i) ok += FpRateOk(&b, 1000.0, 40.0);
    assert(ok == 40);
    assert(!FpRateOk(&b, 1010.0, 40.0) && FpRateOk(&b, 1030.0, 40.0));
    puts("frame_probe_test: ok");
    return 0;
}
