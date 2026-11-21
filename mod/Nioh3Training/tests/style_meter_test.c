// human #126496 / #126502: style meter state machine (style_meter.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu -O0 -UNDEBUG tests/style_meter_test.c -o build/style_meter_test.exe && build/style_meter_test.exe
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/style_meter.h"
static int g_checks;
#define CHECK(x) do { assert(x); ++g_checks; } while (0)
static int near(float a, float b) { return a - b < 0.01f && b - a < 0.01f; }

static void Defaults(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    CHECK(!s.active && s.rank == 0 && SmGauge(&s, &c) == 0.f);
    CHECK(!SmVisible(&s, &c, 0, 1));  // idle: hidden even in a boss fight
    CHECK(sm_rank_name[0][0] == 'D' && sm_rank_name[6][2] == 'S');
}
// Distinct moves add the full base; promotion carries the overflow.
static void Promotion(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    for (uint32_t i = 0; i < 4; ++i) CHECK(near(SmOnHit(&s, &c, 100.0 * i, 1000 + i, i), c.hit));
    CHECK(s.active && s.rank == 1 && near(s.pts, 0.f));  // 40 points = D full -> C empty
    SmOnHit(&s, &c, 500, 2000, 9);
    CHECK(s.rank == 1 && near(SmGauge(&s, &c), 10.f / 60.f));
    // a big jump crosses several ranks at once
    SmState b; SmReset(&b); SmCfg big = c; big.ki = 40.f + 60.f + 80.f + 5.f;
    SmOnKi(&b, &big, 0);
    CHECK(b.rank == 3 && near(b.pts, 5.f));
}
// SSS caps at a full gauge.
static void Cap(void) {
    SmCfg c; SmCfgDefaults(&c); c.ki = 10000.f; SmState s; SmReset(&s);
    SmOnKi(&s, &c, 0);
    CHECK(s.rank == SM_RANKS - 1 && near(SmGauge(&s, &c), 1.f));
    SmOnKi(&s, &c, 1);
    CHECK(s.rank == SM_RANKS - 1 && near(s.pts, c.rankPts[SM_RANKS - 1]));
}
// Same action id again: x0.6 per earlier use, floor 10 %; multi-hit of one use: half of that use's factor.
static void Variety(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    CHECK(near(SmOnHit(&s, &c, 0, 77, 1), 10.f));
    CHECK(near(SmOnHit(&s, &c, 1, 77, 2), 6.f));
    CHECK(near(SmOnHit(&s, &c, 2, 77, 3), 3.6f));
    CHECK(near(SmOnHit(&s, &c, 3, 77, 3), 1.8f));    // second hit of the same run
    CHECK(near(SmOnHit(&s, &c, 4, 88, 4), 10.f));    // a different move is full again
    for (int i = 0; i < 10; ++i) SmOnHit(&s, &c, 5 + i, 77, 10 + i);
    CHECK(near(SmOnHit(&s, &c, 20, 77, 99), 1.f));   // floor 10 %
    // history is the last SM_HIST uses: after 8 other moves the repeated id is fresh again
    for (uint32_t i = 0; i < SM_HIST; ++i) SmOnHit(&s, &c, 30 + i, 500 + i, 200 + i);
    CHECK(near(SmOnHit(&s, &c, 50, 77, 300), 10.f));
    // unknown action id: no penalty, not recorded
    SmState u; SmReset(&u);
    for (int i = 0; i < 5; ++i) CHECK(near(SmOnHit(&u, &c, i, SM_ACT_NONE, (uint64_t)i), 10.f));
    CHECK(u.histN == 0);
}
// Damage: one rank down, gauge empty; at D the meter goes idle.
static void Damage(void) {
    SmCfg c; SmCfgDefaults(&c); c.ki = 40.f + 60.f + 30.f; SmState s; SmReset(&s);
    SmOnKi(&s, &c, 0);
    CHECK(s.rank == 2 && near(s.pts, 30.f));
    SmOnDamaged(&s, &c, 1);
    CHECK(s.rank == 1 && near(s.pts, 0.f) && s.active && s.damaged == 1);
    SmOnDamaged(&s, &c, 2);
    CHECK(s.rank == 0 && !s.active && SmGauge(&s, &c) == 0.f);
    SmOnDamaged(&s, &c, 3);
    CHECK(s.rank == 0 && !s.active);
    // damage also resets the variety "use", so the next hit of the same run is a new use (factor from history, not multi-hit)
    SmState v; SmReset(&v); SmOnHit(&v, &c, 0, 5, 1); SmOnDamaged(&v, &c, 1);
    CHECK(near(SmOnHit(&v, &c, 2, 5, 1), 6.f));
}
// Decay: nothing inside the delay, then decayPerS (+35 %/rank); draining past 0 drops a rank from its top; D ends idle.
static void Decay(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    SmOnHit(&s, &c, 0, 1, 1);
    double t = 0; for (int i = 0; i < 150; ++i) { t += 16.0; SmTick(&s, &c, t, 16.0); }  // 2.4 s < 2.5 s delay
    CHECK(near(s.pts, 10.f));
    // 0.5 s past the delay at rank D: 12 * 0.5 = 6 points
    t = 2500.0; SmTick(&s, &c, t, 16.0);  // reaches the delay exactly: no drain yet
    CHECK(near(s.pts, 10.f));
    SmTick(&s, &c, 3000.0, 500.0);
    CHECK(near(s.pts, 4.f));
    SmTick(&s, &c, 4000.0, 1000.0);
    CHECK(!s.active && s.rank == 0 && near(s.pts, 0.f));
    // rank C drains 12 * 1.35 per second and drops into D from the top
    SmState r; SmReset(&r); SmCfg k = c; k.ki = 41.f; SmOnKi(&r, &k, 0);
    CHECK(r.rank == 1 && near(r.pts, 1.f));
    SmTick(&r, &k, 3500.0, 1000.0);  // 1 s past the delay: 16.2 points -> 1 - 16.2 = -15.2 -> D at 40 - 15.2
    CHECK(r.rank == 0 && near(r.pts, 24.8f));
    // a partial tick that straddles the delay only drains the part past it
    SmState p; SmReset(&p); SmOnKi(&p, &c, 0);
    SmTick(&p, &c, 2600.0, 200.0);
    CHECK(near(p.pts, 20.f - 12.f * 0.1f));
    // dt 0 (pause) never drains
    SmTick(&p, &c, 99999.0, 0.0);
    CHECK(near(p.pts, 20.f - 1.2f));
}
// Dodge: award once per enemy window, only with i-frames seen and no damage.
static void Dodge(void) {
    SmDodge d = {0};
    CHECK(!SmDodgeStep(&d, 0, 1, 0));                // i-frames with no enemy window: nothing
    CHECK(!SmDodgeStep(&d, 1, 0, 0)); CHECK(!SmDodgeStep(&d, 1, 1, 0)); CHECK(!SmDodgeStep(&d, 1, 0, 0));
    CHECK(SmDodgeStep(&d, 0, 0, 0));                 // window closed: dodge
    CHECK(!SmDodgeStep(&d, 0, 0, 0));                // once
    CHECK(!SmDodgeStep(&d, 1, 1, 0)); CHECK(!SmDodgeStep(&d, 1, 0, 1));
    CHECK(!SmDodgeStep(&d, 0, 0, 0));                // hit inside the window: no dodge
    CHECK(!SmDodgeStep(&d, 1, 0, 0)); CHECK(!SmDodgeStep(&d, 0, 0, 0));  // no i-frames: no dodge
    CHECK(!SmDodgeStep(&d, 1, 1, 0)); CHECK(!SmDodgeStep(&d, 0, 0, 1));  // damage on the close tick: no dodge
    // SmOnDamaged during a window cancels it too
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    SmDodgeStep(&s.dodge, 1, 1, 0); SmOnDamaged(&s, &c, 0);
    CHECK(!SmDodgeStep(&s.dodge, 0, 0, 0));
    SmDodgeStep(&s.dodge, 1, 1, 0);
    CHECK(SmDodgeStep(&s.dodge, 0, 0, 0) && near(SmOnDodge(&s, &c, 5), 30.f) && s.dodges == 1);
}
// Visibility: live boss fight keeps it up; outside a fight it hides combatMs after the last event.
static void Visible(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    SmOnHit(&s, &c, 1000, 1, 1);
    CHECK(SmVisible(&s, &c, 1000 + 7999, 0));
    CHECK(!SmVisible(&s, &c, 1000 + 8000, 0));
    CHECK(SmVisible(&s, &c, 1000 + 8000, 1));
}
static void Clamp(void) {
    SmCfg c; SmCfgDefaults(&c);
    c.hit = -5; c.repeatPct = 500; c.repeatFloorPct = -1; c.multiPct = 101; c.decayPerS = -1; c.rankPts[3] = 0; c.combatMs = -3;
    SmCfgClamp(&c);
    CHECK(c.hit == 0 && c.repeatPct == 100 && c.repeatFloorPct == 0 && c.multiPct == 100 && c.decayPerS == 0 && c.rankPts[3] == 1.f && c.combatMs == 0);
    // hit 0: nothing scores, the meter stays idle
    SmState s; SmReset(&s); SmOnHit(&s, &c, 0, 1, 1);
    CHECK(!s.active && s.hits == 1);
}
int main(void) {
    Defaults(); Promotion(); Cap(); Variety(); Damage(); Decay(); Dodge(); Visible(); Clamp();
    printf("style_meter_test: all %d checks passed\n", g_checks);
    return 0;
}
