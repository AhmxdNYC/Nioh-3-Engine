// #128463 player move-speed log (player_speed.h): per-move anim rate, slow stretches, hit-stop filtered.
#include <stdio.h>
#include <math.h>
#undef NDEBUG  // keep asserts in -O2 builds (zig cc -O2 defines NDEBUG)
#include <assert.h>
#include "../src/player_speed.h"

static int near(double a, double b, double e) { return fabs(a - b) <= e; }
static const double F = 1000.0 / 60.0;

int main(void) {
    PlSpd s = {0}; PlSpdEvt e; float t = 0; int ev = 0;
    // move A at 60 fps, normal speed, 30 frames
    assert(!PlSpdFrame(&s, 0xA, t, 1.0f, 1, F, &e));
    for (int i = 0; i < 30; ++i) { t += 1.0f; ev += PlSpdFrame(&s, 0xA, t, 1.0f, 1, F, &e); }
    assert(ev == 0);
    // move B: 0.6x for 60 frames (1 s), fps and step normal -> one SPD event ~1 s, rate ~0.6, step 1.0
    t = 0; assert(!PlSpdFrame(&s, 0xB, t, 1.0f, 1, F, &e));
    assert(s.prevAct == 0xA && near(s.prevRate, 1.0, 0.01));
    for (int i = 0; i < 60; ++i) { t += 0.6f; ev += PlSpdFrame(&s, 0xB, t, 1.0f, 1, F, &e); assert(!ev); }
    for (int i = 0; i < 10 && !ev; ++i) { t += 1.0f; ev += PlSpdFrame(&s, 0xB, t, 1.0f, 1, F, &e); }
    assert(ev == 1 && e.act == 0xB);
    assert(e.ms > 950 && e.ms < 1150);
    assert(near(e.rate, 0.62, 0.05) && near(e.minRate, 0.6, 0.01) && near(e.stepRate, 1.0, 0.01));
    // hit-stop: 5 frames frozen inside a normal move -> no event
    ev = 0; t = 0; PlSpdFrame(&s, 0xC, t, 1.0f, 1, F, &e);
    for (int i = 0; i < 40; ++i) { if (i < 10 || i >= 15) t += 1.0f; ev += PlSpdFrame(&s, 0xC, t, 1.0f, 1, F, &e); }
    assert(ev == 0);
    // move change while slow ends the stretch and reports it; prevRate of the slow move ~0.5
    t = 0; PlSpdFrame(&s, 0xD, t, 1.0f, 1, F, &e);
    for (int i = 0; i < 30; ++i) { t += 0.5f; PlSpdFrame(&s, 0xD, t, 1.0f, 1, F, &e); }
    assert(PlSpdFrame(&s, 0xE, 0.0f, 1.0f, 1, F, &e) == 1 && e.act == 0xD && near(e.rate, 0.5, 0.02));
    assert(s.prevAct == 0xD && near(s.prevRate, 0.5, 0.02));
    // 30 fps: step 2.0, anim +2 per frame, 2 engine frames counted per call -> normal, no event
    ev = 0; t = 0;
    for (int i = 0; i < 40; ++i) { t += 2.0f; ev += PlSpdFrame(&s, 0xE, t, 2.0f, 1, 2 * F, &e); }
    assert(ev == 0);
    // anim restart (time goes back) is skipped, not counted as slow
    t = 1.0f; ev = PlSpdFrame(&s, 0xE, t, 2.0f, 1, 2 * F, &e);
    for (int i = 0; i < 20; ++i) { t += 2.0f; ev += PlSpdFrame(&s, 0xE, t, 2.0f, 1, 2 * F, &e); }
    assert(ev == 0);
    // engine slow-mo (step 0.5 at 60 fps, anim +0.5) -> event with stepRate ~0.5 (the engine clock, not the move)
    t = 0; PlSpdFrame(&s, 0xF, t, 1.0f, 1, F, &e); ev = 0;
    for (int i = 0; i < 30; ++i) { t += 0.5f; PlSpdFrame(&s, 0xF, t, 0.5f, 1, F, &e); }
    for (int i = 0; i < 10 && !ev; ++i) { t += 1.0f; ev = PlSpdFrame(&s, 0xF, t, 1.0f, 1, F, &e); }
    assert(ev == 1 && e.stepRate < 0.7);
    assert(near(PlSpdRateOf(&s, 0xF), 1.0, 0.5) && PlSpdRateOf(&s, 0x99) < 0);
    // frozen pose (load fade-in: anim stuck for 2 s) -> no SPD event
    t = 0; PlSpdFrame(&s, 0x7D0, t, 1.0f, 1, F, &e); ev = 0;
    for (int i = 0; i < 120; ++i) { if (i % 40 == 0) t += 0.1f; ev += PlSpdFrame(&s, 0x7D0, t, 1.6f, 1, F, &e); }
    for (int i = 0; i < 10; ++i) { t += 1.0f; ev += PlSpdFrame(&s, 0x7D0, t, 1.0f, 1, F, &e); }
    assert(ev == 0);
    printf("player_speed_test PASS\n");
    return 0;
}
