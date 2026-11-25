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
    i.run = run; i.use = use; i.act = act; i.elig = elig; i.move = 1; i.plOk = 1; i.pl[0] = px; i.pl[1] = pz; i.win = win; i.bossOk = 1; i.hits = hits; i.avoid = avoid;
    return i;
}
static int Step(SmDisp* d, const SmDispCfg* c, double t, SmDispIn i, SmDispOut* o) { memset(o, 0, sizeof *o); return SmDispStep(d, c, t, &i, o); }
// a transition run (no attack window, not a skill row: the cancel's 10493 / 2096 / 1775 / 2066 / 2069 / 2070)
static SmDispIn Tr(uint64_t run, uint32_t act, float px, float pz, int win, int avoid) { SmDispIn i = In(run, run, act, 0, px, pz, win, 0, avoid); i.move = 0; return i; }
// an attack run before its hit window opened (Y attack 25165 startup): not a move yet
static SmDispIn Wind(uint64_t run, uint32_t act, float px, float pz, int win) { SmDispIn i = In(run, run, act, 0, px, pz, win, 0, 0); i.move = 0; return i; }

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

// ---- replays of the 2026-11-25 live test (Sword vs Demon of Pride, Swift Step Heavy = 25563 step -> 25564 slash, one use) ----
// Old rule results: 4 Displacement, 3 Counter (2 late: 930 / 980 ms > 900; 1 moved 2.25 m < 2.5), 1 nothing (2.04 m), every cancel
// either Counter (too many moves: the 1-frame transition runs counted) or nothing (window during the transition runs).
// Boss at the origin. Use 7 = the Swift Step Heavy use (runs 70 step, 71 slash).
static void SwiftStepSlash(SmDisp* d, const SmDispCfg* c, double t0, float sx, float sz, float ex, float ez, double close, int avoid, double hit, SmDispOut* o, int* res) {
    *res = Step(d, c, t0, In(70, 7, 25563, 1, sx, sz, 1, 0, 0), o);  // the boss window is open at the step's start ("enemy attacking")
    for (double t = t0 + 50; t <= t0 + 2600; t += 50) {
        const int stepping = t < t0 + 250; const float f = stepping ? (float)((t - t0) / 250.0) : 1.f;
        const float x = sx + (ex - sx) * f, z = sz + (ez - sz) * f;
        const int win = t < t0 + close, av = t >= t0 + close && t < t0 + close + 50 ? avoid : 0, h = t >= t0 + hit && t < t0 + hit + 50;
        const int r = Step(d, c, t, In(stepping ? 70 : 71, 7, stepping ? 25563 : 25564, 1, x, z, win, h, av), o);
        if (r) { *res = r; return; }
    }
}
static void ReplayNoCancel(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispOut o; int r;
    CHECK(SmDispListed(&c, 25563) && SmDispListed(&c, 25564) && c.nActs == 2);
    // 07:48:46 boss 1.51 m away, window closed ~170 ms into the step (Counter! held), the slash hit 930 ms after: was "no hit in time"
    SmDispReset(&d); SwiftStepSlash(&d, &c, 1000, 0, 151, -150, -250, 150, SM_AV_COUNTER, 1100, &o, &r);
    CHECK(r == SM_DP_AWARD && o.replaced == SM_AV_COUNTER && o.ago > 900 && o.listed);
    SmDispCfg old = c; old.followMs = 900; SmDispReset(&d); SwiftStepSlash(&d, &old, 1000, 0, 151, -150, -250, 150, SM_AV_COUNTER, 1100, &o, &r);
    CHECK(r == SM_DP_MISS && o.why == SM_DPM_LATE && o.release == SM_AV_DODGE && o.asDodge);  // and even then: a Dodge!, not a Counter!
    // 07:56:54 boss 2.92 m away, window closed as the slash started (250 ms), hit 980 ms after it
    SmDispReset(&d); SwiftStepSlash(&d, &c, 1000, 0, 292, 120, -300, 250, SM_AV_COUNTER, 1230, &o, &r);
    CHECK(r == SM_DP_AWARD && o.ago > 950);
    // 07:48:25 step cut short by the boss: 2.26 m, 172 deg (was "moved too little" < 2.5 m -> Counter!)
    SmDispReset(&d); SwiftStepSlash(&d, &c, 1000, 0, 141, 30, -84, 480, SM_AV_COUNTER, 1100, &o, &r);
    CHECK(r == SM_DP_AWARD && o.moved > 220.f && o.moved < 230.f);
    // 07:48:10 2.04 m, 82 deg, no i-frame avoid (out of reach): was nothing at all
    SmDispReset(&d); SwiftStepSlash(&d, &c, 1000, 0, 168, 160, 26, 200, 0, 1100, &o, &r);
    CHECK(r == SM_DP_AWARD && !o.replaced);
    // the same short step by a skill row that is NOT listed passes the generic test too (>= 2.0 m, >= 60 deg); a 1.2 m one does not
    SmDispCfg g = c; g.nActs = 0;
    SmDispReset(&d); SwiftStepSlash(&d, &g, 1000, 0, 168, 160, 26, 200, 0, 1100, &o, &r); CHECK(r == SM_DP_AWARD && !o.listed);
    SmDispReset(&d); SwiftStepSlash(&d, &g, 1000, 0, 100, 60, 0, 200, SM_AV_COUNTER, 1100, &o, &r);
    CHECK(r == SM_DP_MISS && o.why == SM_DPM_MOTION && o.release == SM_AV_COUNTER && !o.asDodge);
    // 07:54:48 the slash whiffed (no hit): miss, the held Counter! is paid as a Dodge!
    SmDispReset(&d); SwiftStepSlash(&d, &c, 1000, 0, 163, -100, -400, 220, SM_AV_COUNTER, 99999, &o, &r);
    CHECK(r == SM_DP_MISS && o.why == SM_DPM_LATE && o.release == SM_AV_DODGE && o.asDodge);
}
// The cancel: 25563 (200-250 ms) > 10493 > 2096 > 1775 > 2066 > 2069 > 2070 (transition runs, 1-8 frames) > Y attack 25165 (startup 21 f)
static double CancelChain(SmDisp* d, const SmDispCfg* c, double t, float x, float z, double winFrom, double winTo, int avoid, SmDispOut* o, int* res) {
    static const uint32_t tr[] = {10493, 2096, 1775, 2066, 2069, 2070}; static const double len[] = {17, 17, 83, 17, 133, 67};
    const double t0 = t; *res = SM_DP_NONE;
    for (int k = 0; k < 6; ++k) for (double e = t + len[k]; t < e; t += 17) {
        const int win = t >= winFrom && t < winTo, av = t >= winTo && t < winTo + 17 ? avoid : 0;
        const int r = Step(d, c, t, Tr(80 + k, tr[k], x, z, win, av), o); if (r) *res = r;
    }
    (void)t0; return t;
}
static void ReplayCancel(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispOut o; int r;
    // 07:55:54: window closed 100 ms into the step (Counter! held), cancel chain, Y attack hits 980 ms after the step's start.
    // Old: "too many moves before the hit" (2 moves = 10493, 2096) -> Counter!. Now: the transition runs are not moves -> Displacement.
    SmDispReset(&d);
    Step(&d, &c, 1000, In(70, 7, 25563, 1, 0, 88, 1, 0, 0), &o);
    Step(&d, &c, 1050, In(70, 7, 25563, 1, 100, 0, 1, 0, 0), &o);
    CHECK(Step(&d, &c, 1100, In(70, 7, 25563, 1, 200, -100, 0, 0, SM_AV_COUNTER), &o) == SM_DP_NONE && o.ate && d.pend);
    Step(&d, &c, 1200, In(70, 7, 25563, 1, 300, -300, 0, 0, 0), &o);
    double t = CancelChain(&d, &c, 1233, 300, -300, 0, 0, 0, &o, &r); CHECK(r == SM_DP_NONE && d.pend && d.runsAfter == 0);
    for (; t < 1900; t += 17) CHECK(Step(&d, &c, t, Wind(90, 25165, 250, -250, 0), &o) == SM_DP_NONE);
    SmDispIn y = In(90, 90, 25165, 0, 100, -100, 0, 0, 0); CHECK(Step(&d, &c, 1917, y, &o) == SM_DP_NONE && d.runsAfter == 1);
    y.hits = 1; CHECK(Step(&d, &c, 1980, y, &o) == SM_DP_AWARD && o.runs == 1 && o.replaced == SM_AV_COUNTER && o.act == 25563);
    // 07:47:00 / 07:56:43: the boss window opens and closes during the transition runs (after the step ended). Old: no candidate and,
    // with no i-frames in those runs, no avoid either: nothing at all. Now the window belongs to the step's span.
    SmDispReset(&d);
    Step(&d, &c, 1000, In(70, 7, 25563, 1, 0, 120, 0, 0, 0), &o);
    Step(&d, &c, 1200, In(70, 7, 25563, 1, -200, -200, 0, 0, 0), &o);
    t = CancelChain(&d, &c, 1217, -200, -200, 1340, 1460, 0, &o, &r); CHECK(r == SM_DP_NONE && d.pend && !d.held);
    for (; t < 1800; t += 17) Step(&d, &c, t, Wind(90, 25165, -150, -150, 0), &o);
    y = In(90, 90, 25165, 0, -60, -60, 0, 1, 0); CHECK(Step(&d, &c, 1990, y, &o) == SM_DP_AWARD && !o.replaced);
    // 07:49:04: the window closes on the 1775 transition run with an avoid (Counter!: it was paid as one): now held and replaced
    SmDispReset(&d);
    Step(&d, &c, 1000, In(70, 7, 25563, 1, 0, 147, 0, 0, 0), &o);
    Step(&d, &c, 1200, In(70, 7, 25563, 1, -150, -150, 1, 0, 0), &o);
    t = CancelChain(&d, &c, 1217, -150, -150, 1217, 1260, SM_AV_COUNTER, &o, &r); CHECK(d.pend && d.held == SM_AV_COUNTER);
    y = In(90, 90, 25165, 0, -60, -60, 0, 1, 0); CHECK(Step(&d, &c, 2000, y, &o) == SM_DP_AWARD && o.replaced == SM_AV_COUNTER);
    // the window is still open when the follow-up move starts: the candidate opens then, the avoid at the real close is held
    SmDispReset(&d);
    Step(&d, &c, 1000, In(70, 7, 25563, 1, 0, 150, 0, 0, 0), &o);
    Step(&d, &c, 1200, In(70, 7, 25563, 1, -150, -150, 1, 0, 0), &o);
    t = CancelChain(&d, &c, 1217, -150, -150, 1217, 3000, 0, &o, &r); CHECK(!d.pend && d.winSeen);
    y = In(90, 90, 25165, 0, -100, -100, 1, 0, 0); CHECK(Step(&d, &c, t, y, &o) == SM_DP_NONE && d.pend && d.runsAfter == 1);
    y.win = 0; y.avoid = SM_AV_DODGE; CHECK(Step(&d, &c, t + 50, y, &o) == SM_DP_NONE && o.ate && d.held == SM_AV_DODGE);
    y.avoid = 0; y.hits = 1; CHECK(Step(&d, &c, t + 300, y, &o) == SM_DP_AWARD && o.replaced == SM_AV_DODGE);
    // 07:48:57: the Y attack whiffed (25165 MISS), the 2nd Y attack 25166 hit: 2 moves -> miss, the held Counter! paid as a Dodge!
    SmDispReset(&d);
    Step(&d, &c, 1000, In(70, 7, 25563, 1, 0, 73, 1, 0, 0), &o);
    Step(&d, &c, 1080, In(70, 7, 25563, 1, 100, 0, 0, 0, SM_AV_COUNTER), &o); CHECK(d.pend);
    Step(&d, &c, 1150, In(70, 7, 25563, 1, 200, -300, 0, 0, 0), &o);
    t = CancelChain(&d, &c, 1167, 200, -300, 0, 0, 0, &o, &r);
    CHECK(Step(&d, &c, 1700, In(90, 90, 25165, 0, 100, -100, 0, 0, 0), &o) == SM_DP_NONE && d.runsAfter == 1);
    r = Step(&d, &c, 2000, In(91, 91, 25166, 0, 50, -50, 0, 0, 0), &o);
    CHECK(r == SM_DP_MISS && o.why == SM_DPM_RUNS && o.release == SM_AV_DODGE && o.asDodge);
    // a cancel into nothing: the transition runs end in idle past the tail; a window that opens then is not the step's
    SmDispReset(&d);
    Step(&d, &c, 1000, In(70, 7, 25563, 1, 0, 150, 0, 0, 0), &o);
    Step(&d, &c, 1200, In(70, 7, 25563, 1, -150, -150, 0, 0, 0), &o);
    for (t = 1217; t < 1200 + c.tailMs + 50; t += 17) Step(&d, &c, t, Tr(85, 2070, -150, -150, 0, 0), &o);
    CHECK(!d.sLive);
    Step(&d, &c, t, Tr(85, 2070, -150, -150, 1, 0), &o);
    CHECK(Step(&d, &c, t + 100, Tr(85, 2070, -150, -150, 0, SM_AV_DODGE), &o) == SM_DP_NONE && !d.pend && !o.ate);
}
// Kept out: the parry counter (25376 -> 25377, skill word 0xF53B, barely moves), deflects / rolls (not eligible: glue), and the avoid of
// a parry skill stays a Counter!.
static void ReplayKeptOut(void) {
    SmDispCfg c; SmDispCfgDefaults(&c); SmDisp d; SmDispOut o;
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 25376, 1, 0, 150, 1, 0, 0), &o);
    Step(&d, &c, 100, In(1, 1, 25376, 1, 20, 140, 0, 0, SM_AV_COUNTER), &o); CHECK(d.pend && o.ate);
    CHECK(Step(&d, &c, 300, In(2, 1, 25377, 1, 40, 110, 0, 1, 0), &o) == SM_DP_MISS && o.why == SM_DPM_MOTION && o.release == SM_AV_COUNTER && !o.asDodge);
    // a deflect skill row (elig 0 from the glue) next to the boss: nothing
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 25510, 0, 0, 150, 1, 0, 0), &o);
    CHECK(Step(&d, &c, 100, In(1, 1, 25510, 0, 250, -50, 0, 0, SM_AV_DEFLECT), &o) == SM_DP_NONE && !o.ate && !o.conv);
    // a DisplaceActs move's avoid with no candidate (boss 7 m away when the window opened): paid, but as a Dodge! (conv)
    SmDispReset(&d);
    Step(&d, &c, 0, In(70, 7, 25563, 1, 0, 700, 1, 0, 0), &o);
    CHECK(Step(&d, &c, 100, In(70, 7, 25563, 1, 0, 500, 0, 0, SM_AV_COUNTER), &o) == SM_DP_NONE && !o.ate && o.conv);
    // ... a skill row that is not listed keeps its Counter!
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 25516, 1, 0, 700, 1, 0, 0), &o);
    CHECK(Step(&d, &c, 100, In(1, 1, 25516, 1, 0, 500, 0, 0, SM_AV_COUNTER), &o) == SM_DP_NONE && !o.conv);
    // a held avoid is released when Displace is turned off mid-candidate
    SmDispReset(&d);
    Step(&d, &c, 0, In(1, 1, 7, 1, 0, 300, 1, 0, 0), &o);
    Step(&d, &c, 100, In(1, 1, 7, 1, 300, 0, 0, 0, SM_AV_DODGE), &o); CHECK(d.pend && d.held == SM_AV_DODGE);
    SmDispCfg z = c; z.on = 0;
    CHECK(Step(&d, &z, 150, In(1, 1, 7, 1, 300, 0, 0, 0, 0), &o) == SM_DP_MISS && o.why == SM_DPM_OFF && o.release == SM_AV_DODGE);
    // DisplaceActs parsing (ini string)
    SmDispCfg p; SmDispCfgDefaults(&p); CHECK(SmDispParseActs(&p, "0x63DB, 25564,,7") == 3 && p.acts[0] == 25563 && p.acts[1] == 25564 && p.acts[2] == 7);
    CHECK(SmDispParseActs(&p, "") == 0);
}
int main(void) {
    SameUse(); CancelFollow(); PlainRoll(); StraightLunge(); Misses(); PreHit(); Pay();
    ReplayNoCancel(); ReplayCancel(); ReplayKeptOut();
    printf("style_displace_test: all %d checks passed\n", g_checks);
    return 0;
}
