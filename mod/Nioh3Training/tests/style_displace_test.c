// Displacement! state machine (style_displace.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu -O0 -UNDEBUG tests/style_displace_test.c -o build/style_displace_test.exe && build/style_displace_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/style_displace.h"
static int g_checks;
#define CHECK(x) do { assert(x); ++g_checks; } while (0)
// A scripted tick: boss fixed at the origin unless moved.
static SmDispIn In(uint64_t run, uint64_t use, uint32_t act, int elig, float px, float pz, int win, int hits, int avoid) {
    SmDispIn i; memset(&i, 0, sizeof i);
    i.run = run; i.use = use; i.act = act; i.elig = elig; i.plOk = 1; i.pl[0] = px; i.pl[1] = pz; i.win = win; i.bossOk = 1; i.hits = hits; i.avoid = avoid;
    return i;
}
static int Step(SmDisp* d, const SmDispCfg* c, double t, SmDispIn i, SmDispOut* o) { memset(o, 0, sizeof *o); return SmDispStep(d, c, t, &i, o); }

// Just Reprisal shape: one skill use carries the player 90 deg around the boss through its hit window, the same use then hits.
static void SameUse(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispReset(&d); SmDispOut o;
    CHECK(Step(&d, &c, 5000, In(1, 1, 25601, 1, 0, 300, 0, 0, 0), &o) == SM_DP_NONE);
    CHECK(Step(&d, &c, 5100, In(1, 1, 25601, 1, 100, 250, 1, 0, 0), &o) == SM_DP_NONE);   // window opens at 2.7 m
    CHECK(Step(&d, &c, 5200, In(1, 1, 25601, 1, 250, 100, 1, 0, 0), &o) == SM_DP_NONE);
    CHECK(Step(&d, &c, 5250, In(1, 1, 25601, 1, 300, 0, 0, 0, SM_AV_COUNTER), &o) == SM_DP_NONE && o.ate && d.pend && d.held == SM_AV_COUNTER);
    CHECK(Step(&d, &c, 5450, In(1, 1, 25601, 1, 300, 0, 0, 1, 0), &o) == SM_DP_AWARD && !o.release && o.act == 25601);
    CHECK(o.moved > 420.f && o.moved < 430.f && o.ang > 89.f && o.ang < 91.f && !d.pend);
}
// Swift Step Heavy shape: the step (skill row) goes behind the boss, a different move (cancel, not a skill row) lands the hit.
static void CancelFollow(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispReset(&d); SmDispOut o;
    Step(&d, &c, 0, In(10, 10, 25400, 1, 0, 300, 0, 0, 0), &o);
    Step(&d, &c, 50, In(10, 10, 25400, 1, 0, 200, 1, 0, 0), &o);
    Step(&d, &c, 100, In(10, 10, 25400, 1, 50, -150, 0, 0, SM_AV_DODGE), &o); CHECK(o.ate && d.pend);
    CHECK(Step(&d, &c, 150, In(11, 11, 25570, 0, 40, -200, 0, 0, 0), &o) == SM_DP_NONE && d.pend && d.runsAfter == 1);
    CHECK(Step(&d, &c, 400, In(11, 11, 25570, 0, 40, -200, 0, 2, 0), &o) == SM_DP_AWARD && o.ang > 160.f && o.runs == 1);
}
// Roll away then poke: a plain roll is not eligible (no skill row): no candidate, the Dodge! is not touched.
static void PlainRoll(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispReset(&d); SmDispOut o;
    Step(&d, &c, 0, In(1, 1, 25157, 0, 0, 200, 0, 0, 0), &o);
    Step(&d, &c, 50, In(1, 1, 25157, 0, 200, 100, 1, 0, 0), &o);
    CHECK(Step(&d, &c, 100, In(1, 1, 25157, 0, 300, -100, 0, 0, SM_AV_DODGE), &o) == SM_DP_NONE && !o.ate && !d.pend);
    CHECK(Step(&d, &c, 300, In(2, 2, 25112, 0, 300, -100, 0, 1, 0), &o) == SM_DP_NONE);
    // ... unless its act is listed (DisplaceActs)
    c.acts[c.nActs++] = 25157; CHECK(SmDispListed(&c, 25157) && !SmDispListed(&c, 25112));
}
// A skill move that lunges straight at the boss (bearing unchanged) is not a displacement: the held avoid is paid after all.
static void StraightLunge(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispReset(&d); SmDispOut o;
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 450, 0, 0, 0), &o);
    Step(&d, &c, 50, In(1, 1, 7, 1, 0, 350, 1, 0, 0), &o);
    Step(&d, &c, 100, In(1, 1, 7, 1, 0, 150, 0, 0, SM_AV_COUNTER), &o); CHECK(o.ate);
    CHECK(Step(&d, &c, 200, In(1, 1, 7, 1, 0, 100, 0, 1, 0), &o) == SM_DP_MISS && o.why == SM_DPM_ANGLE && o.release == SM_AV_COUNTER);
    // a small sidestep around the boss: turns, but too little motion
    SmDispReset(&d);
    Step(&d, &c, 1000, In(2, 2, 7, 1, 0, 100, 0, 0, 0), &o);
    Step(&d, &c, 1050, In(2, 2, 7, 1, 80, 80, 1, 0, 0), &o);
    Step(&d, &c, 1100, In(2, 2, 7, 1, 100, 0, 0, 0, SM_AV_DODGE), &o);
    CHECK(Step(&d, &c, 1200, In(2, 2, 7, 1, 100, 0, 0, 1, 0), &o) == SM_DP_MISS && o.why == SM_DPM_MOTION && o.release == SM_AV_DODGE);
}
static void Misses(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispOut o;
    // late: no hit within followMs of the window close
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 300, 0, 0, 0), &o); Step(&d, &c, 50, In(1, 1, 7, 1, 0, 300, 1, 0, 0), &o);
    Step(&d, &c, 100, In(1, 1, 7, 1, 300, 0, 0, 0, SM_AV_DODGE), &o);
    CHECK(Step(&d, &c, 100 + c.followMs, In(1, 1, 7, 1, 300, 0, 0, 0, 0), &o) == SM_DP_NONE && d.pend);
    CHECK(Step(&d, &c, 101 + c.followMs, In(1, 1, 7, 1, 300, 0, 0, 0, 0), &o) == SM_DP_MISS && o.why == SM_DPM_LATE && o.release == SM_AV_DODGE);
    // too many runs: displacement, then two other moves before the hit
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 300, 0, 0, 0), &o); Step(&d, &c, 50, In(1, 1, 7, 1, 0, 300, 1, 0, 0), &o);
    Step(&d, &c, 100, In(1, 1, 7, 1, 300, 0, 0, 0, 0), &o); CHECK(d.pend && d.held == 0);  // no i-frames: nothing held (teleport / out of reach)
    Step(&d, &c, 150, In(2, 2, 8, 0, 300, 0, 0, 0, 0), &o);
    CHECK(Step(&d, &c, 200, In(3, 3, 9, 0, 300, 0, 0, 0, 0), &o) == SM_DP_MISS && o.why == SM_DPM_RUNS && !o.release);
    // a later segment of the same use (chain) is not a new run for the count
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 25376, 1, 0, 300, 0, 0, 0), &o); Step(&d, &c, 50, In(1, 1, 25376, 1, 0, 300, 1, 0, 0), &o);
    Step(&d, &c, 100, In(2, 1, 25377, 1, 300, 0, 0, 0, SM_AV_COUNTER), &o); CHECK(d.pend && d.runsAfter == 0);
    CHECK(Step(&d, &c, 300, In(2, 1, 25377, 1, 300, 0, 0, 1, 0), &o) == SM_DP_AWARD);
    // damage after the candidate: miss, the earlier window's avoid is paid
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 300, 0, 0, 0), &o); Step(&d, &c, 50, In(1, 1, 7, 1, 0, 300, 1, 0, 0), &o);
    Step(&d, &c, 100, In(1, 1, 7, 1, 300, 0, 0, 0, SM_AV_COUNTER), &o);
    SmDispIn h = In(1, 1, 7, 1, 300, 0, 0, 0, 0); h.took = 1;
    CHECK(Step(&d, &c, 200, h, &o) == SM_DP_MISS && o.why == SM_DPM_DAMAGE && o.release == SM_AV_COUNTER);
    // damage inside the window: no candidate at all
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 300, 0, 0, 0), &o);
    h = In(1, 1, 7, 1, 0, 300, 1, 0, 0); h.took = 1; Step(&d, &c, 50, h, &o);
    CHECK(Step(&d, &c, 100, In(1, 1, 7, 1, 300, 0, 0, 0, 0), &o) == SM_DP_NONE && !d.pend);
    // knockdown: a skill-row use right after damage (grace) is not eligible (live log: 132 'Dodge!' on knockdown acts 10087 / 10084 / 1480)
    SmDispReset(&d);
    h = In(1, 1, 3, 0, 0, 300, 0, 0, 0); h.took = 1; Step(&d, &c, 0, h, &o);
    Step(&d, &c, 500, In(2, 2, 7, 1, 0, 300, 0, 0, 0), &o); Step(&d, &c, 550, In(2, 2, 7, 1, 0, 300, 1, 0, 0), &o);
    CHECK(Step(&d, &c, 600, In(2, 2, 7, 1, 300, 0, 0, 0, SM_AV_DODGE), &o) == SM_DP_NONE && !d.pend && !o.ate);
    // out of range when the window opened (boss attacking someone / something 8 m away)
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 800, 0, 0, 0), &o); Step(&d, &c, 50, In(1, 1, 7, 1, 0, 800, 1, 0, 0), &o);
    CHECK(Step(&d, &c, 100, In(1, 1, 7, 1, 800, 0, 0, 0, SM_AV_DODGE), &o) == SM_DP_NONE && !d.pend && !o.ate);
}
// A fast counter that hits before the boss's window closes: judged at the close with the geometry of that hit.
static void PreHit(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispReset(&d); SmDispOut o;
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 300, 0, 0, 0), &o);
    Step(&d, &c, 50, In(1, 1, 7, 1, 0, 300, 1, 0, 0), &o);
    Step(&d, &c, 100, In(1, 1, 7, 1, -300, 0, 1, 1, 0), &o); CHECK(d.pre);
    CHECK(Step(&d, &c, 150, In(1, 1, 7, 1, -300, -50, 0, 0, SM_AV_COUNTER), &o) == SM_DP_AWARD && o.ate && o.ang > 89.f);
    // the avoid closes one tick after the candidate opened: still held
    SmDispReset(&d);
    Step(&d, &c, 1000, In(2, 2, 7, 1, 0, 300, 0, 0, 0), &o); Step(&d, &c, 1050, In(2, 2, 7, 1, 0, 300, 1, 0, 0), &o);
    Step(&d, &c, 1100, In(2, 2, 7, 1, 300, 0, 0, 0, 0), &o); CHECK(d.pend && !o.ate);
    Step(&d, &c, 1117, In(2, 2, 7, 1, 300, 0, 0, 0, SM_AV_DODGE), &o); CHECK(o.ate && d.held == SM_AV_DODGE);
}
// Paying: replaces the avoid, a loop step of the use, variety pseudo id; off / clamp.
static void Pay(void) {
    SmCfg sc; SmCfgDefaults(&sc); SmDispCfg c; SmDispCfgDefaults(&c); SmState s; SmReset(&s);
    SmOnHitUse(&s, &sc, 100, 25601, 1, -1.f, 0, 0);
    CHECK(SmOnDisplace(&s, &sc, &c, 100, 25601, 1) == c.pts && s.varN == 2);
    // the same use looped (A A A with LoopReps 2): its hit scored 0, so does the displacement
    SmOnHitUse(&s, &sc, 200, 25601, 2, -1.f, 0, 0); SmOnHitUse(&s, &sc, 300, 25601, 3, -1.f, 0, 0); CHECK(s.lpPen);
    CHECK(SmOnDisplace(&s, &sc, &c, 300, 25601, 3) == 0.f);
    // follow-up of another use: one pseudo step, the follow-up's multi-hit count is untouched
    SmState q; SmReset(&q); SmOnHitUse(&q, &sc, 0, 25570, 11, -1.f, 0, 0); const int lh = q.lpHits;
    CHECK(SmOnDisplace(&q, &sc, &c, 0, 25400, 10) == c.pts && q.lpHits == lh && q.lpRun == 11 && q.lpA == SM_LOOP_DISPLACE);
    SmDispCfg z = c; z.on = 0; SmDisp d; SmDispReset(&d); SmDispOut o;
    CHECK(Step(&d, &z, 0, In(1, 1, 7, 1, 0, 300, 1, 0, 0), &o) == SM_DP_NONE && !d.winSeen);
    SmDispCfg k = c; k.followMs = 5; k.angDeg = 999; k.followRuns = 99; k.pts = -1; SmDispCfgClamp(&k);
    CHECK(k.followMs == 100 && k.angDeg == 180 && k.followRuns == 4 && k.pts == 0.f);
    CHECK(SmDispTurn((float[]){0, 1}, (float[]){0, 0}, (float[]){0, -1}, (float[]){0, 0}) > 179.f);
    CHECK(SmDispTurn((float[]){1, 0}, (float[]){0, 0}, (float[]){1, 1}, (float[]){0, 1}) < 0.1f);  // the boss moved with the player: no turn
}
int main(void) {
    SameUse(); CancelFollow(); PlainRoll(); StraightLunge(); Misses(); PreHit(); Pay();
    printf("style_displace_test: all %d checks passed\n", g_checks);
    return 0;
}
