// #35411 empty-area i-frame probe, pure part (frame_meter_iframe.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/frame_meter_iframe_test.c -o build/frame_meter_iframe_test.exe && build/frame_meter_iframe_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/frame_meter_iframe.h"

static void StripeTests(void) {
    uint32_t seen = 0;
    // a bit that is on from the first sample does not light the stripe until it was seen off once
    assert(!FmiStripe(FMI_CANNOT0, &seen));
    assert(!FmiStripe(FMI_CANNOT0, &seen));
    assert(!FmiStripe(0, &seen));
    assert(FmiStripe(FMI_CANNOT0, &seen));
    // bits outside the stripe set never light it (s48 depends on the attack row, cond4 is the guard path)
    assert(!FmiStripe(FMI_S48 | FMI_COND4 | FMI_ARMOR | FMI_ISNODMG, &seen));
    assert(FmiStripe(FMI_NODMGACT, &seen) && FmiStripe(FMI_NOHIT, &seen) && FmiStripe(FMI_HURTOFF, &seen));
    char b[200]; FmiNames(FMI_CANNOT0 | FMI_NODMGACT, b, sizeof b); assert(!strcmp(b, "cannot0|nodmgact"));
    FmiNames(0, b, sizeof b); assert(!strcmp(b, "-"));
}
static void FrameTests(void) {
    const double f = 1000.0 / 60.0;
    assert(FmiFrame(100.0, 100.0) == 0 && FmiFrame(100.0 + f, 100.0) == 1 && FmiFrame(100.0 + 24 * f, 100.0) == 24 && FmiFrame(50.0, 100.0) == 0);
    assert(FmiFrame(100.0 + 0.99 * f, 100.0) == 0);
}
static void TrackTests(void) {
    enum { N = 6 };
    FmiWord w[N]; uint32_t cur[N] = {0, 0, 0x3F800000, 5, 7, 9};
    FmiTrackReset(w, cur, N);
    for (int fr = 1; fr <= 40; ++fr) {
        cur[0] = (fr >= 4 && fr < 14) ? 1 : 0;        // window frames 4..13 -> on at 4, back at 14
        cur[1] = fr >= 10 ? 0x4000 : 0;               // open from 10
        cur[2] = 0x3F800000 + (uint32_t)fr;           // a float that moves every tick: noisy
        cur[4] = (fr == 0) ? 1 : 7;                   // never changes in the run
        cur[5] = (fr >= 5 && fr < 8) ? 2 : (fr >= 8 && fr < 12) ? 3 : 9;  // 9 -> 2 -> 3 -> 9: still a window (3 changes)
        FmiTrackStep(w, cur, N, fr);
    }
    assert(FmiTrackKind(&w[0]) == 1 && w[0].f1 == 4 && w[0].f2 == 14 && w[0].first == 1);
    assert(FmiTrackKind(&w[1]) == 2 && w[1].f1 == 10);
    assert(FmiTrackKind(&w[2]) == 0 && FmiTrackNoisy(w, N) == 1);
    assert(FmiTrackKind(&w[3]) == 0 && FmiTrackKind(&w[4]) == 0);
    assert(FmiTrackKind(&w[5]) == 1 && w[5].f1 == 5 && w[5].f2 == 12);
    char b[300]; int o = 0; const int n = FmiTrackFormat(w, N, 'c', 0, b, sizeof b, &o, 10);
    assert(n == 3);
    assert(!strcmp(b, " c0=0>1@4-14 c4=0>4000@10- c14=9>2@5-12"));
    // a change on frame 0 (the action-start tick) is not a window: the field belongs to the action change itself
    FmiWord z[1]; uint32_t v = 0; FmiTrackReset(z, &v, 1); v = 1; FmiTrackStep(z, &v, 1, 0); v = 0; FmiTrackStep(z, &v, 1, 5);
    assert(FmiTrackKind(&z[0]) == 0);
    // the formatter stops at `max` and never overruns the buffer
    o = 0; assert(FmiTrackFormat(w, N, 'a', 0x100, b, sizeof b, &o, 1) == 1 && !strcmp(b, " a100=0>1@4-14"));
    char small[60]; o = 0; FmiTrackFormat(w, N, 's', 0, small, sizeof small, &o, 10); assert(o < (int)sizeof small);
}
int main(void) {
    StripeTests(); FrameTests(); TrackTests();
    puts("frame_meter_iframe_test: all passed");
    return 0;
}
