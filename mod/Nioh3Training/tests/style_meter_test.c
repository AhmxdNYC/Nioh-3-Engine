// human #126496 / #126502: style meter state machine (style_meter.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu -O0 -UNDEBUG tests/style_meter_test.c -o build/style_meter_test.exe && build/style_meter_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/style_meter.h"
#include "../src/style_meter_fx.h"
// The mechanics cases below were written against the first tuning; pin it so they keep testing the rules, not the numbers.
static void RealDefaults(SmCfg* c) { SmCfgDefaults(c); }
static void PinnedDefaults(SmCfg* c) {
    static const float old[SM_RANKS] = {40.f, 60.f, 80.f, 100.f, 130.f, 160.f, 200.f};
    SmCfgDefaults(c); c->decayPerS = 12.f; c->decayDelayMs = 2500; for (int i = 0; i < SM_RANKS; ++i) c->rankPts[i] = old[i];
    // #127568: the shipped defaults moved to the human's tuning; the mechanics cases keep the numbers they were written for.
    c->hit = 10.f; c->dodge = 30.f; c->ki = 20.f; c->repeatPct = 60; c->repeatFloorPct = 10; c->multiPct = 50; c->decayRankPct = 35; c->decayRampPct = 0;
    c->loopReps = 0; c->multiHitCap = 0;  // loop penalty / multi-hit cap came later: off here, covered by Loop() / MultiCap()
}
#define SmCfgDefaults PinnedDefaults
static int g_checks;
#define CHECK(x) do { assert(x); ++g_checks; } while (0)
static int near(float a, float b) { return a - b < 0.01f && b - a < 0.01f; }
// A Ki pulse with the hit gate off (the older cases use it as a plain points source).
static float KiFree(SmState* s, const SmCfg* c, double t) { SmCfg z = *c; z.kiHitMs = 0; return SmOnKi(s, &z, t); }
// human #126744: a Ki pulse after a whiff scores nothing; after a landed hit (within kiHitMs) it scores.
static void KiGate(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s);
    CHECK(c.kiHitMs == SM_KI_HIT_MS);
    CHECK(SmOnKi(&s, &c, 1000) == 0.f && !s.active && s.kis == 1 && SmGauge(&s, &c) == 0.f);  // whiff + pulse: meter stays idle
    SmOnHit(&s, &c, 2000, 7, 1);
    CHECK(near(SmOnKi(&s, &c, 2000 + c.kiHitMs), c.ki) && near(s.pts, c.hit + c.ki));       // pulse inside the window
    CHECK(SmOnKi(&s, &c, 2001 + c.kiHitMs) == 0.f && near(s.pts, c.hit + c.ki));              // just past it: 0
    // a gated pulse does not refresh the decay delay
    const double ls = s.lastScore; SmOnKi(&s, &c, 9000); CHECK(s.lastScore == ls);
    SmCfg z = c; z.kiHitMs = 0; SmState q; SmReset(&q); CHECK(near(SmOnKi(&q, &z, 0), c.ki));  // 0 = no gate
    SmCfg n = c; n.kiHitMs = -5; SmCfgClamp(&n); CHECK(n.kiHitMs == 0);
}

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
    KiFree(&b, &big, 0);
    CHECK(b.rank == 3 && near(b.pts, 5.f));
}
// SSS caps at a full gauge.
static void Cap(void) {
    SmCfg c; SmCfgDefaults(&c); c.ki = 10000.f; SmState s; SmReset(&s);
    KiFree(&s, &c, 0);
    CHECK(s.rank == SM_RANKS - 1 && near(SmGauge(&s, &c), 1.f));
    KiFree(&s, &c, 1);
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
    KiFree(&s, &c, 0);
    CHECK(s.rank == 2 && near(s.pts, 30.f));
    SmOnDamaged(&s, &c, 1);
    CHECK(s.rank == 1 && near(s.pts, 0.f) && s.active && s.damaged == 1);
    SmOnDamaged(&s, &c, 2);  // live #126502: same combo (within the grace) = no second drop
    CHECK(s.rank == 1 && s.damaged == 2);
    SmOnDamaged(&s, &c, 1 + c.damageGraceMs);
    CHECK(s.rank == 0 && !s.active && SmGauge(&s, &c) == 0.f);
    SmOnDamaged(&s, &c, 2 + 2.0 * c.damageGraceMs);
    CHECK(s.rank == 0 && !s.active);
    // grace 0 = every hit drops
    { SmCfg z = c; z.damageGraceMs = 0; SmState q; SmReset(&q); KiFree(&q, &z, 0); SmOnDamaged(&q, &z, 1); SmOnDamaged(&q, &z, 1); CHECK(q.rank == 0); }
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
    SmState r; SmReset(&r); SmCfg k = c; k.ki = 41.f; KiFree(&r, &k, 0);
    CHECK(r.rank == 1 && near(r.pts, 1.f));
    SmTick(&r, &k, 3500.0, 1000.0);  // 1 s past the delay: 16.2 points -> 1 - 16.2 = -15.2 -> D at 40 - 15.2
    CHECK(r.rank == 0 && near(r.pts, 24.8f));
    // a partial tick that straddles the delay only drains the part past it
    SmState p; SmReset(&p); KiFree(&p, &c, 0);
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
// human #126755: shipped tuning is 2.5x the first (#126785); #127568: now the human's Main PC tuning.
static void Tuning(void) {
    SmCfg c; RealDefaults(&c);
    CHECK(near(c.rankPts[0], 120.f) && near(c.rankPts[1], 180.f) && near(c.deflect, 23.f) && near(c.rankPts[SM_RANKS - 1], 500.f) && near(c.decayPerS, 30.f) && c.decayDelayMs == 500);
    CHECK(near(c.hit, 20.f) && near(c.dodge, 45.f) && near(c.ki, 30.f) && c.repeatPct == 80 && c.repeatFloorPct == 40 && c.multiPct == 100 && c.decayRankPct == 50 && c.decayRampPct == 150);
    SmState s; SmReset(&s);
    for (uint32_t i = 0; i < 8; ++i) SmOnHit(&s, &c, 10.0 * i, 3000 + i, i);
    CHECK(s.rank == 1 && near(SmGauge(&s, &c), 40.f / 180.f));  // 8 different moves = 160 pts: D (120) full, 40 into C (180)
}
// #126821 startup scaling: x1 at <= min, x(1+pct) at >= max, linear between; unknown = x1; multi-hit keeps the use's factor.
static void Startup(void) {
    SmCfg c; RealDefaults(&c);
    CHECK(near(SmStartupMult(&c, -1.f), 1.f) && near(SmStartupMult(&c, 5.f), 1.f) && near(SmStartupMult(&c, 15.f), 1.f));
    CHECK(near(SmStartupMult(&c, 37.5f), 1.5f) && near(SmStartupMult(&c, 60.f), 2.f) && near(SmStartupMult(&c, 200.f), 2.f));
    SmState s; SmReset(&s);
    CHECK(near(SmOnHitEx(&s, &c, 0, 5, 1, 60.f, 0), 2.f * c.hit));            // slow startup: double
    CHECK(near(SmOnHitEx(&s, &c, 10, 5, 1, 60.f, 0), 2.f * c.hit * c.multiPct / 100.f));     // second hit of the same use: multi-hit share of it
    CHECK(near(SmOnHitEx(&s, &c, 20, 6, 2, 10.f, 0), c.hit));                  // fast jab: plain
    SmCfg z = c; z.startupPct = 0; CHECK(near(SmStartupMult(&z, 60.f), 1.f));
    SmCfg b = c; b.startupMaxF = 3; b.startupMinF = 9; SmCfgClamp(&b); CHECK(b.startupMaxF == 10);
}
// #126843 counter: flat bonus once per use, on top of the scaled hit.
static void Counter(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(near(SmOnHitEx(&s, &c, 0, 7, 1, -1.f, 1), c.hit + c.counter) && s.counters == 1);
    CHECK(near(SmOnHitEx(&s, &c, 5, 7, 1, -1.f, 1), c.hit * c.multiPct / 100.f) && s.counters == 1);  // same use: no second bonus
}
// #126850 / #126854 skills: only in combat (a hit within combatMs); outside they do not wake the meter.
static void Skills(void) {
    SmCfg c; RealDefaults(&c); c.rankPts[0] = 1000.f; SmState s; SmReset(&s);  // #127568: keep the sum inside rank D
    CHECK(SmOnSkill(&s, &c, 0, 0) == 0.f && SmOnSkill(&s, &c, 0, 1) == 0.f && !s.active && s.nins == 1 && s.souls == 1);
    SmOnHit(&s, &c, 1000, 1, 1);
    CHECK(near(SmOnSkill(&s, &c, 1000 + c.combatMs, 0), c.nin));
    CHECK(near(SmOnSkill(&s, &c, 1000 + c.combatMs, 1), c.soul));
    CHECK(SmOnSkill(&s, &c, 1001 + c.combatMs, 1) == 0.f);
    CHECK(near(s.pts, c.hit + c.nin + c.soul));
}
// #126843 style switch: in combat, one bonus per landed hit between switches.
static void Style(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(near(SmOnStyle(&s, &c, 0), c.style) && s.active);  // #127065: from an idle meter too
    SmOnHit(&s, &c, 100, 1, 1);
    CHECK(near(SmOnStyle(&s, &c, 200), c.style));
    CHECK(SmOnStyle(&s, &c, 300) == 0.f);              // switch spam: nothing until the next hit
    SmOnHit(&s, &c, 400, 2, 2);
    CHECK(near(SmOnStyle(&s, &c, 500), c.style) && s.styles == 4);
}
// #126913 a switch inside a soul core form: once per scored soul core use, even with no hit since the last switch; never out of a fight.
static void StyleSoul(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(SmOnSkill(&s, &c, 0, 1) == 0.f && near(SmOnStyle(&s, &c, 100), c.style));  // soul core out of combat: 0; the switch counts as idle (#127065)
    SmOnHit(&s, &c, 1000, 1, 1);
    CHECK(near(SmOnStyle(&s, &c, 1100), c.style));                            // by the hit
    CHECK(near(SmOnSkill(&s, &c, 1200, 1), c.soul) && s.soulsScored == 1);
    CHECK(near(SmOnStyle(&s, &c, 1300), c.style));                            // mid soul core, no new hit
    CHECK(SmOnStyle(&s, &c, 1400) == 0.f);                                    // spam inside the same form
    CHECK(near(SmOnSkill(&s, &c, 1500, 1), c.soul));
    CHECK(near(SmOnStyle(&s, &c, 1600), c.style) && SmOnStyle(&s, &c, 1700) == 0.f);  // the 2nd use pays one more, then spam stops
}
static void Ramp(void) {  // human #126972: drain speeds up with idle time past the delay; 0 = flat (old behaviour)
    SmCfg c; RealDefaults(&c); c.decayDelayMs = 0; c.decayRankPct = 0; c.decayPerS = 10.f; c.decayRampPct = 0;
    SmState a; SmReset(&a); a.active = 1; a.rank = 2; a.pts = 100.f; a.lastScore = 0;
    SmTick(&a, &c, 2000.0, 1000.0); const float flat = 100.f - a.pts;            // 1 s slice at idle 2 s, no ramp
    CHECK(near(flat, 10.f));
    c.decayRampPct = 100; SmState b; SmReset(&b); b.active = 1; b.rank = 2; b.pts = 100.f; b.lastScore = 0;
    SmTick(&b, &c, 2000.0, 1000.0); CHECK(near(100.f - b.pts, 30.f));              // x(1 + 1.0 * 2 s) = 3x
    SmState e; SmReset(&e); e.active = 1; e.rank = 2; e.pts = 100.f; e.lastScore = 1900.0;
    SmTick(&e, &c, 2000.0, 100.0); CHECK(100.f - e.pts < 1.2f);                    // fresh hit: ramp barely started
}
// #127003 move logger line: hit / miss / no attack, pad combo text.
static void MoveFmt(void) {
    char b[400]; SmMoveRec m; memset(&m, 0, sizeof m);
    m.act = 0x2A31; m.ms = 500; m.windows = 1; m.landed = 0; m.startupF = 18; m.style = 0; m.btn = 0x0200 | 0x8000; m.rt = 255;
    SmMoveFmt(b, sizeof b, &m);
    CHECK(strstr(b, "act 0x2A31 (10801)") && strstr(b, "MISS") && strstr(b, "startup 18 f") && strstr(b, "Samurai") && strstr(b, "pad Y+RB+RT"));
    m.landed = 2; m.soulForm = 1; m.style = 1; SmMoveFmt(b, sizeof b, &m);
    CHECK(strstr(b, "HIT") && strstr(b, "Ninja") && strstr(b, "soul core form"));
    m.slot = -1; SmMoveFmt(b, sizeof b, &m); CHECK(strstr(b, "slot") == 0);
    m.slot = 1; m.swapped = 1; SmMoveFmt(b, sizeof b, &m); CHECK(strstr(b, "| slot 1 swap attack"));
    m.windows = 0; m.landed = 0; m.startupF = -1; m.btn = 0; m.rt = 0; SmMoveFmt(b, sizeof b, &m);
    CHECK(strstr(b, "no attack") && strstr(b, "startup - |") && strstr(b, "pad -"));
    CHECK(strstr(b, "spirit") == 0);                                           // #127213 gauge unread: no column
    m.sf0 = 80; m.sfMin = 55; m.sfMax = 100; m.gs = 1; SmMoveFmt(b, sizeof b, &m);
    CHECK(strstr(b, "| spirit 80.0->55.0/100.0 guardian spirit"));
    m.gs = 0; SmMoveFmt(b, sizeof b, &m); CHECK(strstr(b, "| spirit 80.0->55.0/100.0") && !strstr(b, "guardian"));
    CHECK(strstr(b, "skill") == 0);                                            // #127295 row word unread: no column
    m.skillRd = 1; m.skill = 0xFFFFFFFFu; SmMoveFmt(b, sizeof b, &m); CHECK(strstr(b, "| skill -"));
    m.skill = 0xCEC4; m.ma = 1; SmMoveFmt(b, sizeof b, &m); CHECK(strstr(b, "| skill 0xCEC4 martial art"));
    m.ma = 0; SmMoveFmt(b, sizeof b, &m); CHECK(strstr(b, "| skill 0xCEC4") && !strstr(b, "martial"));
}
// #127213 guardian spirit skill: Spirit Force gauge drop (>= 1 below the tick before the move) near the move start, once per move.
// Feeds n ticks of 16 ms on move `run` with gauge `cur`; pays like the game glue (the move's own points so far > 0).
static int GsTicks(SmGsMove* g, SmState* s, const SmCfg* c, double* t, uint64_t run, const float* cur, int n, float movePts, float* paid) {
    int marks = 0;
    for (int i = 0; i < n; ++i) { *t += 16; marks += SmGsStep(g, *t, run, cur[i], c->gsMs); *paid += SmGsPay(s, c, g, *t, movePts); }
    return marks;
}
static void GuardianSpirit(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s); SmGsMove g; SmGsReset(&g);
    CHECK(c.gs == SM_GS_PTS && c.gs == 25.f && c.gsMs == SM_GS_MS);
    double t = 1000; float paid = 0;
    const float idle[3] = {100, 100, 100}, cast[4] = {100, 70, 70, 70}, flat[4] = {70, 70, 70, 70}, refill[4] = {70, 100, 100, 100};
    GsTicks(&g, &s, &c, &t, 1, idle, 3, 0, &paid);                                        // standing: base 100
    // gauge drop at the start of a move -> one bonus, even with several hits / ticks after
    CHECK(GsTicks(&g, &s, &c, &t, 2, cast, 4, 12, &paid) == 1 && near(paid, c.gs) && s.gsHits == 1);
    CHECK(GsTicks(&g, &s, &c, &t, 2, flat, 4, 30, &paid) == 0 && near(paid, c.gs));      // same move, more hits: no second bonus
    // no drop -> none
    paid = 0; CHECK(GsTicks(&g, &s, &c, &t, 3, flat, 4, 12, &paid) == 0 && paid == 0.f);
    // refill (gauge rises, Practice / RecoverGuardianAction) -> none, and the refilled value is the new base
    CHECK(GsTicks(&g, &s, &c, &t, 4, refill, 4, 12, &paid) == 0 && paid == 0.f);
    // a drop smaller than 1 (float noise) -> none
    { const float tiny[3] = {99.5f, 99.5f, 99.5f}; CHECK(GsTicks(&g, &s, &c, &t, 5, tiny, 3, 12, &paid) == 0); }
    // drop too long after the move start -> none
    { SmGsMove h; SmGsReset(&h); double u = 0; float pp = 0; const float a[1] = {100}; GsTicks(&h, &s, &c, &u, 1, a, 1, 0, &pp);
      CHECK(SmGsStep(&h, 100, 2, 100, c.gsMs) == 0 && SmGsStep(&h, 100 + c.gsMs + 1, 2, 50, c.gsMs) == 0 && !h.gs); }
    // marked but no points yet: bonus waits for the move's first scored hit
    { SmGsMove h; SmGsReset(&h); SmState q; SmReset(&q); double u = 0; float pp = 0; const float a[1] = {100}, b[2] = {60, 60};
      GsTicks(&h, &q, &c, &u, 1, a, 1, 0, &pp);
      CHECK(GsTicks(&h, &q, &c, &u, 2, b, 2, 0, &pp) == 1 && pp == 0.f && q.gsHits == 0);
      CHECK(near(SmGsPay(&q, &c, &h, u, 15), c.gs) && SmGsPay(&q, &c, &h, u, 30) == 0.f && q.gsHits == 1); }
    // two moves, each spending -> two bonuses
    { SmGsMove h; SmGsReset(&h); SmState q; SmReset(&q); double u = 0; float pp = 0;
      const float a[1] = {100}, m1[3] = {100, 75, 75}, m2[3] = {75, 50, 50};
      GsTicks(&h, &q, &c, &u, 1, a, 1, 0, &pp);
      CHECK(GsTicks(&h, &q, &c, &u, 2, m1, 3, 10, &pp) == 1 && GsTicks(&h, &q, &c, &u, 3, m2, 3, 10, &pp) == 1);
      CHECK(near(pp, 2 * c.gs) && q.gsHits == 2); }
    // unreadable gauge (-1) before the move: no base, nothing
    { SmGsMove h; SmGsReset(&h); SmState q; SmReset(&q); double u = 0; float pp = 0; const float a[1] = {-1}, b[2] = {50, 50};
      GsTicks(&h, &q, &c, &u, 1, a, 1, 0, &pp); CHECK(GsTicks(&h, &q, &c, &u, 2, b, 2, 10, &pp) == 0 && pp == 0.f); }
    // ini 0 = off; clamp
    { SmCfg z = c; z.gs = 0; SmGsMove h; SmGsReset(&h); SmState q; SmReset(&q); h.gs = 1; CHECK(SmGsPay(&q, &z, &h, 0, 10) == 0.f); }
    { SmCfg z = c; z.gs = -5; z.gsMs = 99999; SmCfgClamp(&z); CHECK(z.gs == 0.f && z.gsMs == 5000); }
}
// #127295 martial art: the action row skill word (dword R+0xDC, the game's AttackSkill test), one bonus per move = the ninjutsu bonus.
static void MartialArt(void) {
    SmCfg c; RealDefaults(&c);
    CHECK(near(c.ma, c.nin) && c.ma == (float)SM_NIN_PTS && SM_MA_PTS == SM_NIN_PTS);
    // the game's rule: bit 31 clear and low word != 0xBA71 (row words from the extracted player files)
    CHECK(SmSkillRow(0x0000CEC4u) && SmSkillRow(0x00004DA8u) && !SmSkillRow(0xFFFFFFFFu) && !SmSkillRow(0x80001234u) && !SmSkillRow(0x0000BA71u));
    // ini: unset (-1) follows NinjutsuPts; a set value wins; changing NinjutsuPts changes the default
    CHECK(near(SmMaPtsResolve(-1, c.nin), c.nin));
    { SmCfg z; RealDefaults(&z); z.nin = 33; z.ma = SmMaPtsResolve(-1, z.nin); SmCfgClamp(&z); CHECK(near(z.ma, 33)); }
    { SmCfg z; RealDefaults(&z); z.nin = 33; z.ma = SmMaPtsResolve(12, z.nin); CHECK(near(z.ma, 12)); }
    { SmCfg z; RealDefaults(&z); z.nin = 0; z.ma = SmMaPtsResolve(-1, z.nin); SmMaMove m; SmMaReset(&m); SmState q; SmReset(&q);
      SmMaStep(&m, 1, 1, 0); CHECK(SmMaPay(&q, &z, &m, 0, 10) == 0.f); }                       // NinjutsuPts 0 -> martial art off too
    SmState s; SmReset(&s); SmMaMove m; SmMaReset(&m);
    // martial art move: marked on its first tick, paid once on its first scored points (= ninjutsu pts), never again in the same move
    CHECK(SmMaStep(&m, 7, 1, 0) == 1 && SmMaPay(&s, &c, &m, 100, 0) == 0.f && s.maHits == 0);  // no points yet: waits
    CHECK(SmMaStep(&m, 7, 1, 0) == 0 && near(SmMaPay(&s, &c, &m, 116, 14), c.nin) && s.maHits == 1);
    CHECK(SmMaStep(&m, 7, 1, 0) == 0 && SmMaPay(&s, &c, &m, 132, 30) == 0.f && s.maHits == 1);
    // normal move (row word FFFFFFFF) or unreadable -> none
    CHECK(SmMaStep(&m, 8, 0, 0) == 0 && SmMaPay(&s, &c, &m, 150, 14) == 0.f);
    CHECK(SmMaStep(&m, 9, -1, 0) == 0 && SmMaPay(&s, &c, &m, 170, 14) == 0.f && s.maHits == 1);
    // the next martial art move pays again
    CHECK(SmMaStep(&m, 10, 1, 0) == 1 && near(SmMaPay(&s, &c, &m, 190, 9), c.nin) && s.maHits == 2);
    // guardian spirit skill / ninjutsu (their rows carry the word too): no martial art bonus, also when the gauge mark comes a tick later
    CHECK(SmMaStep(&m, 11, 1, 1) == 1 && SmMaPay(&s, &c, &m, 210, 20) == 0.f);
    SmMaStep(&m, 12, 1, 0); SmMaStep(&m, 12, 1, 1); CHECK(SmMaPay(&s, &c, &m, 230, 20) == 0.f && s.maHits == 2);
    // the bonus goes into the meter like the others
    { SmState q; SmReset(&q); SmMaMove h; SmMaReset(&h); SmMaStep(&h, 1, 1, 0); SmMaPay(&q, &c, &h, 0, 5); CHECK(near(q.pts, c.ma)); }
    { SmCfg z = c; z.ma = -4; SmCfgClamp(&z); CHECK(z.ma == 0.f); }
}
// #127057 weapon swap attack: a hitting move that started within swapMs of a slot change, once per change.
static void Swap(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(SmSwapHit(&s, &c, 100, 100) == 0.f);                       // no swap yet
    SmOnSwap(&s, 1000);
    CHECK(SmSwapHit(&s, &c, 2000, 1000 + c.swapMs + 1) == 0.f);      // move started too late
    CHECK(near(SmSwapHit(&s, &c, 1300, 1100), c.swap) && s.swapHits == 1 && s.lastScore == 1300);
    CHECK(SmSwapHit(&s, &c, 1350, 1100) == 0.f);                     // second hit of the same swap attack
    SmOnSwap(&s, 5000);
    CHECK(near(SmSwapHit(&s, &c, 5100, 4900), c.swap) && s.swaps == 2);  // move began just before the slot flipped
}
// #127085 pop-ups: stack newest last, merge the same bonus, cap, fade to 0 at the end of life.
static void Pops(void) {
    SmPopQ q; memset(&q, 0, sizeof q); float in;
    SmPopPush(&q, "Ninjutsu", 20, 0, SM_POP_LIFE_MS);
    SmPopPush(&q, "Ki Pulse", 30, 100, SM_POP_LIFE_MS);
    CHECK(q.n == 2 && !strcmp(q.p[1].label, "Ki Pulse"));
    SmPopPush(&q, "Ninjutsu", 20, 200, SM_POP_LIFE_MS);                    // merged, moved to the top
    CHECK(q.n == 2 && !strcmp(q.p[1].label, "Ninjutsu") && near(q.p[1].pts, 40) && q.p[1].count == 2);  // #127166 count shown as xN
    CHECK(q.p[0].count == 1 && q.p[0].bonus == 0.f);
    { SmPopQ q2; memset(&q2, 0, sizeof q2); SmPopPush2(&q2, "Perfect Ki", 30, 15, 0, SM_POP_LIFE_MS); SmPopPush2(&q2, "Perfect Ki", 30, 15, 50, SM_POP_LIFE_MS);
      CHECK(q2.n == 1 && near(q2.p[0].pts, 60) && near(q2.p[0].bonus, 30)); }  // #127171 value and bonus merge separately
    SmPopPush(&q, "Ninjutsu", 20, 200 + SM_POP_MERGE_MS + 1, SM_POP_LIFE_MS);  // too late to merge: own line
    CHECK(q.n == 3);
    for (int i = 0; i < 10; ++i) SmPopPush(&q, i & 1 ? "A" : "B", 1, 1000 + i * 1000.0 / 10, SM_POP_LIFE_MS);
    CHECK(q.n <= SM_POPS);
    SmPop p; memset(&p, 0, sizeof p); p.t0 = p.tLast = 0;
    CHECK(SmPopAlpha(&p, 0, 1200, &in) == 1.f && in == 0.f);
    CHECK(SmPopAlpha(&p, SM_POP_IN_MS, 1200, &in) == 1.f && in == 1.f);
    CHECK(SmPopAlpha(&p, 400, 1200, 0) == 1.f);                              // hold 40%
    const float a1 = SmPopAlpha(&p, 800, 1200, 0), a2 = SmPopAlpha(&p, 1100, 1200, 0);
    CHECK(a1 < 1.f && a2 < a1 && a2 > 0.f && SmPopAlpha(&p, 1200, 1200, 0) == 0.f);
    SmPopPrune(&q, 1e9, SM_POP_LIFE_MS); CHECK(q.n == 0);
}
// #127085 motion: bounce peaks then returns to 1, shake decays to 0, glow only at S+, eased gauge converges and snaps on a wrap.
static void Motion(void) {
    CHECK(SmBounce(-1) == 1.f && SmBounce(SM_BOUNCE_MS) == 1.f && SmBounce(SM_BOUNCE_MS * 0.3) > 1.08f && SmBounce(SM_BOUNCE_MS * 0.3) < 1.2f);
    float dx, dy; SmShake(SM_SHAKE_MS, &dx, &dy); CHECK(dx == 0 && dy == 0);
    SmShake(30, &dx, &dy); CHECK(fabsf(dx) <= 7.f && fabsf(dy) <= 7.f && (dx != 0 || dy != 0));
    CHECK(SmGlow(3, 123) == 0.f && SmGlow(4, 0) > 0.f && SmGlow(6, 0) >= SmGlow(4, 0) && SmGlow(6, 1e6) <= 0.85f);
    float g = 0; for (int i = 0; i < 60; ++i) g = SmEase(g, 800, 16.7);
    CHECK(g == 800.f);
    CHECK(SmEase(10, 400, 16.7) > 10.f && SmEase(10, 400, 16.7) < 400.f && SmEase(900, 100, 16.7) == 100.f && SmEase(5, 5, 0) == 5.f);
    // #127253 rank-up: the shown bar reaches the end on the old rank before the shown rank steps up; two-rank jump steps twice; drop snaps
    SmShownBar b = {-1, 0.f}; SmBarStep(&b, 1, 600.f, 16.7); CHECK(b.rank == 1 && b.g == 600.f);
    float maxOld = 0.f; int ups = 0, steps = 0;
    while (b.rank < 3 && steps < 200) { const int r0 = b.rank; const int up = SmBarStep(&b, 3, 100.f, 16.7); ups += up; ++steps;
        if (!up && b.rank == 1 && b.g > maxOld) maxOld = b.g; if (up) CHECK(b.rank == r0 + 1); }
    CHECK(maxOld == 1000.f && ups == 2 && b.rank == 3 && steps < 40);
    for (int i = 0; i < 60; ++i) SmBarStep(&b, 3, 100.f, 16.7); CHECK(b.g == 100.f);
    SmBarStep(&b, 2, 900.f, 16.7); CHECK(b.rank == 2 && b.g == 900.f);
    // #127299 rank-up transition: drop-in starts big and faded, old letter fades out, flash + slash only after impact, all rest after SM_UP_MS
    SmUpFx u0 = SmRankUp(0), um = SmRankUp(SM_UP_IMPACT / 2), ui = SmRankUp(SM_UP_IMPACT + 1), ue = SmRankUp(SM_UP_MS), un = SmRankUp(-1);
    CHECK(u0.ns > 1.7f && u0.na < 0.01f && u0.oa > 0.99f && u0.slash < 0.f && u0.flash == 0.f);
    CHECK(um.ns > 1.f && um.ns < u0.ns && um.oa > 0.f && um.oa < 1.f && um.ody < 0.f);
    CHECK(ui.oa == 0.f && ui.flash > 0.9f && ui.slash >= 0.f && ui.ns <= 1.f && ui.na == 1.f);
    CHECK(ue.ns == 1.f && ue.flash == 0.f && ue.slash < 0.f && ue.shx == 0.f && un.ns == 1.f && un.oa == 0.f);
    // #127304 per-rank rank-ups build up: SSS has the most of everything, C has sparks, S has the ring, A the cross-cut
    CHECK(ui.imp >= 0.f && u0.imp < 0.f && ue.imp < 0.f);
    CHECK(sm_up_style[1].sparks > 0 && sm_up_style[3].slashes == 2 && sm_up_style[4].ring && sm_up_style[6].embers > 0 && sm_up_style[6].slashes == 3);
    for (int r = 2; r < 7; ++r) CHECK(sm_up_style[r].shake >= sm_up_style[r - 1].shake);
    CHECK(SmRand01(1, 2, 3) == SmRand01(1, 2, 3) && SmRand01(1, 2, 3) >= 0.f && SmRand01(1, 2, 3) <= 1.f);
}
// #127065 mode switch from an idle meter scores (one per combatMs with no hit), spam still does not.
static void StyleIdle(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(near(SmOnStyle(&s, &c, 0), c.style) && s.active);                 // idle / empty meter: counts
    CHECK(SmOnStyle(&s, &c, 100) == 0.f);                                   // spam: nothing
    CHECK(near(SmOnStyle(&s, &c, 101 + c.combatMs), c.style));              // much later: a fresh one
}
// #127065 one ninjutsu per throw, #127075 only in Ninja mode.
static void NinGate(void) {
    double last = -1e18;
    CHECK(SmNinGate(&last, 1000, 1, 1) == 1);
    CHECK(SmNinGate(&last, 1010, 1, 1) == 0);                               // second gauge drop of the same throw
    CHECK(SmNinGate(&last, 1300, 0, 1) == 0);
    CHECK(SmNinGate(&last, 1010 + SM_NIN_GAP_MS, 1, 1) == 1);               // next throw
    CHECK(SmNinGate(&last, 9000, 1, 0) == 0);                               // Samurai mode: not a ninjutsu
    CHECK(SmNinGate(&last, 20000, 2, -1) == 1);                             // mode unknown: counts once
}
// #127075 perfect Ki pulse: ki + kiPerfect; no points after a whiff either way.
static void KiPerfect(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(SmOnKiEx(&s, &c, 0, 1) == 0.f && s.kiPerfects == 0);              // no hit before it
    SmOnHit(&s, &c, 1000, 1, 1);
    CHECK(near(SmOnKiEx(&s, &c, 1100, 1), c.ki + c.kiPerfect) && s.kiPerfects == 1);
    SmOnHit(&s, &c, 2000, 2, 2);
    CHECK(near(SmOnKiEx(&s, &c, 2100, 0), c.ki) && s.kiPerfects == 1);
}
// Weapon switch: a hit with another weapon than the last hit, within switchMs of it; once per switch; unknown slot = nothing.
static void WeaponSwitch(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(SmWeaponHit(&s, &c, 0, 0) == 0.f);                                  // first hit: nothing to compare
    CHECK(SmWeaponHit(&s, &c, 500, 0) == 0.f);                                // same weapon
    CHECK(near(SmWeaponHit(&s, &c, 1500, 1), c.swap) && s.swapHits == 1);     // switched within 3 s
    CHECK(SmWeaponHit(&s, &c, 1800, 1) == 0.f);                               // once per switch
    CHECK(SmWeaponHit(&s, &c, 1800 + c.switchMs + 1, 0) == 0.f);              // too slow: not a fluid switch
    CHECK(SmWeaponHit(&s, &c, 9000, -1) == 0.f && s.hitSlot == 0);            // unknown slot: ignored, memory kept
    CHECK(near(SmWeaponHit(&s, &c, 9500, 1), 0.f) == 1);                      // 9000 was not remembered: last known hit is too old
    CHECK(near(SmWeaponHit(&s, &c, 10000, 0), c.swap) && s.swapHits == 2);
}
static void GuardianSummon(void) {  // summoned guardian spirit: its own hits score inside the summon window, bonus once
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SmSummon m; SmSummonReset(&m); float b = 0.f;
    CHECK(SmSummonHits(&s, &c, &m, 100, 2, &b) == 0.f && b == 0.f && s.hits == 0);       // no summon: ally hits are not ours
    SmSummonOpen(&m, 1000, 77, 17076);
    const float p = SmSummonHits(&s, &c, &m, 1500, 2, &b);
    CHECK(near(p, c.hit * (1.f + c.multiPct / 100.f)) && near(b, c.gs) && s.hits == 2 && s.gsHits == 1 && s.lastHit == 1500 && s.lastScore == 1500);
    CHECK(SmSummonHits(&s, &c, &m, 1000 + c.gsSummonMs, 1, &b) > 0.f && b == 0.f && s.gsHits == 1);   // later hits: points, no second bonus
    CHECK(SmSummonHits(&s, &c, &m, 1001 + c.gsSummonMs, 1, &b) == 0.f && s.hits == 3);               // window over
    SmSummonOpen(&m, 20000, 78, 17076); m.paid = 1;                                                 // the player's own hits already paid it
    CHECK(SmSummonHits(&s, &c, &m, 20100, 1, &b) > 0.f && b == 0.f && s.gsHits == 1);
    SmCfg z = c; z.gsSummonMs = 0; SmSummonOpen(&m, 30000, 79, 17076);
    CHECK(SmSummonHits(&s, &z, &m, 30001, 1, &b) == 0.f);                                          // 0 = off
}
static void Combo(void) {  // combo: hits within comboMs, broken by a gap or a hit taken; milestone bonus every comboStep hits
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); uint32_t m = 0;
    CHECK(c.comboStep == 10 && c.comboBase == 25 && c.comboInc == 15 && c.comboMax == 100);
    CHECK(near(SmComboMilestone(&c, 10), 25) && near(SmComboMilestone(&c, 20), 40) && near(SmComboMilestone(&c, 50), 85) && near(SmComboMilestone(&c, 60), 100) && near(SmComboMilestone(&c, 200), 100));
    CHECK(SmComboMilestone(&c, 9) == 0.f && SmComboMilestone(&c, 15) == 0.f && SmComboMilestone(&c, 0) == 0.f);
    double t = 0; float b = 0;
    for (int i = 0; i < 9; ++i) { b += SmComboHits(&s, &c, t, 1, &m); t += 1000; }
    CHECK(s.combo == 9 && b == 0.f);
    const float before = s.pts + s.rank * 1000.f;
    CHECK(near(SmComboHits(&s, &c, t, 1, &m), 25) && m == 10 && s.comboBonuses == 1 && s.lastScore == t && s.pts + s.rank * 1000.f != before);
    SmComboTick(&s, &c, t + c.comboMs); CHECK(s.combo == 10);              // still inside the window
    SmComboTick(&s, &c, t + c.comboMs + 1); CHECK(s.combo == 0 && s.comboBest == 10);  // gap: combo over, best kept
    t += c.comboMs + 1000;
    SmComboHits(&s, &c, t, 3, &m); CHECK(s.combo == 3 && m == 0);           // multi-hit: 3 hits at once
    SmOnDamaged(&s, &c, t + 10); CHECK(s.combo == 0 && s.comboBest == 10);  // getting hit breaks it
    SmComboHits(&s, &c, t + 20, 12, &m); CHECK(s.combo == 12 && m == 10 && s.comboBonuses == 2);  // one tick through a milestone
    SmCfg z = c; z.comboBase = 0; z.comboMax = 0; SmState q; SmReset(&q);
    CHECK(SmComboHits(&q, &z, 0, 20, &m) == 0.f && q.combo == 20);         // bonus off: still counts
}
static float Use(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run) { return SmOnHit(s, c, t, act, run); }
static void Loop(void) {  // loop penalty: period 1 / period 2 past loopReps cycles cost loopPts per use, hits score 0
    SmCfg c; RealDefaults(&c);
    CHECK(c.loopReps == 2 && near(c.loopPts, 10.f) && c.multiHitCap == 4);
    {   // the default (2): A A fine, the 3rd A costs; A B A B fine, the 5th use costs
        SmCfg d = c; d.hit = 10.f; d.repeatPct = 100; d.multiPct = 100; d.startupPct = 0; d.counter = 0.f; SmState u; SmReset(&u);
        CHECK(near(SmOnHit(&u, &d, 10, 7, 1), 10.f) && near(SmOnHit(&u, &d, 20, 7, 2), 10.f) && SmOnHit(&u, &d, 30, 7, 3) == 0.f && u.loopPens == 1);
        SmReset(&u); for (int i = 0; i < 4; ++i) CHECK(near(SmOnHit(&u, &d, 10.0 * i, i & 1 ? 21 : 20, (uint64_t)i + 10), 10.f));
        CHECK(SmOnHit(&u, &d, 50, 20, 20) == 0.f && u.loopPens == 1);
    }
    c.loopReps = 3;  // the cases below were written for 3 cycles
    c.hit = 10.f; c.repeatPct = 100; c.multiPct = 100; c.startupPct = 0; c.counter = 0.f;
    for (int i = 0; i < SM_RANKS; ++i) c.rankPts[i] = 100.f;
    SmState s; SmReset(&s); uint64_t r = 1; double t = 0;
    // period 1: A A A score, the 4th A costs 10 and scores 0 (all its hits), the 5th too; the decay delay is not refreshed
    for (int i = 0; i < 3; ++i) CHECK(near(Use(&s, &c, t += 10, 7, r++), 10.f));
    CHECK(near(s.pts, 30.f) && s.loopPens == 0);
    const double ls = s.lastScore;
    CHECK(Use(&s, &c, t += 10, 7, r) == 0.f && near(s.pts, 20.f) && s.loopPens == 1 && s.lastScore == ls);
    CHECK(Use(&s, &c, t += 10, 7, r++) == 0.f && near(s.pts, 20.f) && s.loopPens == 1);   // 2nd hit of that use: 0, no second penalty
    CHECK(Use(&s, &c, t += 10, 7, r++) == 0.f && near(s.pts, 10.f) && s.loopPens == 2);   // 5th A
    CHECK(s.hits == 6 && s.countedHits == 3);
    CHECK(near(Use(&s, &c, t += 10, 8, r++), 10.f));    // B breaks it (A A A A A B: 2-cycle check sees only A B)
    CHECK(near(Use(&s, &c, t += 10, 7, r++), 10.f) && s.loopPens == 2);  // A B A: fine
    // period 2: A B A B A B score, the 7th use (A) and 8th (B) cost
    SmReset(&s); r = 100;
    for (int i = 0; i < 6; ++i) CHECK(near(Use(&s, &c, t += 10, i & 1 ? 21 : 20, r++), 10.f));
    CHECK(s.loopPens == 0 && near(s.pts, 60.f));
    CHECK(Use(&s, &c, t += 10, 20, r++) == 0.f && s.loopPens == 1 && near(s.pts, 50.f));
    CHECK(Use(&s, &c, t += 10, 21, r++) == 0.f && s.loopPens == 2 && near(s.pts, 40.f));
    CHECK(near(Use(&s, &c, t += 10, 22, r++), 10.f) && s.loopPens == 2);   // C breaks it
    CHECK(near(Use(&s, &c, t += 10, 20, r++), 10.f) && near(Use(&s, &c, t += 10, 21, r++), 10.f));  // a new loop starts counting from scratch
    // reset: A A A B A A A never costs; an unknown act breaks a loop too
    SmReset(&s); r = 200;
    const uint32_t seq[] = {7, 7, 7, 8, 7, 7, 7, 7, 7, SM_ACT_NONE, 7, 7, 7, 0, 7, 7, 7};
    int pen = 0; for (unsigned i = 0; i < sizeof seq / sizeof seq[0]; ++i) { const uint32_t p0 = s.loopPens; Use(&s, &c, t += 10, seq[i], r++); if (s.loopPens != p0) pen |= 1 << i; }
    CHECK(pen == (1 << 7 | 1 << 8));  // only the 4th and 5th A of the second run
    // the penalty drains like decay: below 0 drops a rank (refilled from its top); at D it ends empty / idle
    SmReset(&s); r = 300; SmCfg k = c; k.loopPts = 30.f;
    for (int i = 0; i < 3; ++i) Use(&s, &k, t += 10, 9, r++);
    KiFree(&s, &k, t); KiFree(&s, &k, t); KiFree(&s, &k, t);   // + 3 x 30: 120 = rank C, 20 in
    CHECK(s.rank == 1 && near(s.pts, 20.f));
    Use(&s, &k, t += 10, 9, r++); CHECK(s.rank == 0 && near(s.pts, 90.f) && s.active);   // 20 - 30 -> D at 90
    k.loopPts = 1000.f; Use(&s, &k, t += 10, 9, r++); CHECK(s.rank == 0 && s.pts == 0.f && !s.active && s.loopPens == 2);
    // summoned spirit hits between the player's hits do not split or repeat the player's use
    SmReset(&s); SmSummon m; SmSummonReset(&m); SmSummonOpen(&m, 0, 999, 50); float b;
    Use(&s, &c, 10, 60, 1); SmSummonHits(&s, &c, &m, 20, 1, &b); Use(&s, &c, 30, 60, 1); SmSummonHits(&s, &c, &m, 40, 1, &b); Use(&s, &c, 50, 60, 1);
    CHECK(s.lpN1 == 1 && s.loopPens == 0 && s.lpHits == 3);
    // off / clamp
    SmCfg z = c; z.loopReps = 0; SmReset(&s); for (int i = 0; i < 10; ++i) CHECK(near(Use(&s, &z, i, 7, (uint64_t)i + 1), 10.f));
    SmCfg q = c; q.loopReps = 1; q.loopPts = -5.f; SmCfgClamp(&q); CHECK(q.loopReps == 2 && q.loopPts == 0.f);
    q.loopReps = 99; SmCfgClamp(&q); CHECK(q.loopReps == 7); q.loopReps = -1; SmCfgClamp(&q); CHECK(q.loopReps == 0);
}
static void MultiCap(void) {  // multi-hit cap: per use only the first multiHitCap hits score / count toward the combo
    SmCfg c; RealDefaults(&c); c.loopReps = 0; c.hit = 10.f; c.repeatPct = 100; c.multiPct = 100; c.startupPct = 0; c.counter = 0.f;
    SmState s; SmReset(&s); uint32_t mile = 0;
    for (int i = 0; i < 4; ++i) CHECK(near(SmOnHit(&s, &c, 100 + i, 7, 1), 10.f));
    const float p4 = s.pts; const double ls = s.lastScore;
    CHECK(SmOnHit(&s, &c, 200, 7, 1) == 0.f && SmOnHit(&s, &c, 201, 7, 1) == 0.f);
    CHECK(near(s.pts, p4) && s.lastScore == ls && s.lastHit == 201 && s.hits == 6 && s.countedHits == 4 && s.cappedHits == 2);
    // the glue feeds the combo only the counted hits: 6 real hits -> combo 4
    SmComboHits(&s, &c, 201, (int)s.countedHits, &mile); CHECK(s.combo == 4);
    // a new use starts its own count
    CHECK(near(SmOnHit(&s, &c, 300, 8, 2), 10.f) && s.countedHits == 5);
    // combo does not advance past the cap: a 14-hit move then a 10-hit move = 8 counted, no 10-hit milestone
    SmState q; SmReset(&q); uint32_t h0 = q.countedHits;
    for (int i = 0; i < 14; ++i) SmOnHit(&q, &c, 1000, 30, 1);
    float bon = SmComboHits(&q, &c, 1000, (int)(q.countedHits - h0), &mile); h0 = q.countedHits;
    for (int i = 0; i < 10; ++i) SmOnHit(&q, &c, 1100, 31, 2);
    bon += SmComboHits(&q, &c, 1100, (int)(q.countedHits - h0), &mile);
    CHECK(q.combo == 8 && bon == 0.f && q.hits == 24);
    // summoned guardian spirit: one use of its own, capped the same way
    SmState u; SmReset(&u); SmSummon m; SmSummonReset(&m); SmSummonOpen(&m, 0, 77, 17076); float b = 0;
    const float ps = SmSummonHits(&u, &c, &m, 100, 6, &b);
    CHECK(near(ps, 40.f) && u.hits == 6 && u.countedHits == 4 && u.cappedHits == 2 && m.hitsN == 4);
    CHECK(SmSummonHits(&u, &c, &m, 200, 2, &b) == 0.f && u.hits == 8 && u.countedHits == 4);
    SmSummonOpen(&m, 300, 78, 17076); CHECK(near(SmSummonHits(&u, &c, &m, 300, 1, &b), 10.f));  // a new summon counts again
    // 0 = no cap
    SmCfg z = c; z.multiHitCap = 0; SmState w; SmReset(&w);
    for (int i = 0; i < 10; ++i) CHECK(near(SmOnHit(&w, &z, i, 7, 1), 10.f));
    CHECK(w.countedHits == 10 && w.cappedHits == 0);
    SmCfg k = c; k.multiHitCap = -3; SmCfgClamp(&k); CHECK(k.multiHitCap == 0);
    // negative pop-up (loop penalty): pushed as a loss, merges like any other line
    SmPopQ pq; memset(&pq, 0, sizeof pq);
    SmPopPush2(&pq, "Repetitive!", -10.f, 0.f, 0, 1200); SmPopPush2(&pq, "Repetitive!", -10.f, 0.f, 100, 1200);
    CHECK(pq.n == 1 && near(pq.p[0].pts, -20.f) && pq.p[0].count == 2);
}
// Multi-segment martial art = one use for the cap. Infinite Retribution as logged 2026-11-25: 25351 (3 hits) -> 25352 (13 hits) -> 25353
// (1 hit), all skill word 0x42B, back to back. Per tick: SmChainStep(run, act, skill, word), then the tick's hits with the use key.
typedef struct { uint32_t act; int hits, skill; uint32_t w; } ChainSeg;
static uint64_t g_chainRun = 5000;
static void ChainPlay(SmState* s, const SmCfg* c, SmChain* ch, double* t, const ChainSeg* g, int n) {
    for (int i = 0; i < n; ++i) {
        const uint64_t run = ++g_chainRun << 32 | g[i].act;
        for (int k = 0; k < 2 + g[i].hits; ++k) {  // a wind-up tick, one tick per hit, a recovery tick
            const uint64_t key = SmChainStep(ch, run, g[i].act, g[i].skill, g[i].w);
            if (k >= 1 && k <= g[i].hits) SmOnHit(s, c, *t, g[i].act, key);
            *t += 16;
        }
    }
}
static void MultiChain(void) {
    SmCfg c; RealDefaults(&c); c.loopReps = 0; c.hit = 10.f; c.repeatPct = 100; c.multiPct = 100; c.startupPct = 0; c.counter = 0.f;
    const ChainSeg ir[3] = {{25351, 3, 1, 0x42B}, {25352, 13, 1, 0x42B}, {25353, 1, 1, 0x42B}};
    SmState s; SmReset(&s); SmChain ch; SmChainReset(&ch); double t = 1000; uint32_t mile = 0;
    ChainPlay(&s, &c, &ch, &t, ir, 3);
    CHECK(s.hits == 17 && s.countedHits == 4 && s.cappedHits == 13 && near(s.pts, 40.f));  // was 3 + 4 + 1 = 8 counted
    SmComboHits(&s, &c, t, (int)s.countedHits, &mile); CHECK(s.combo == 4);
    // the same martial art cast again right after (its lead-in act repeats): a new use, 4 more
    ChainPlay(&s, &c, &ch, &t, ir, 3);
    CHECK(s.countedHits == 8 && s.hits == 34);
    // a gap tick (no act / not the player) between segments: separate uses
    SmState g; SmReset(&g); SmChainReset(&ch);
    ChainPlay(&g, &c, &ch, &t, ir, 1); SmChainStep(&ch, 0, SM_ACT_NONE, -1, 0); ChainPlay(&g, &c, &ch, &t, ir + 1, 1);
    CHECK(g.countedHits == 3 + 4);
    // a different skill word, or a plain (non-skill) attack, after a martial art: a new use
    const ChainSeg mix[3] = {{25351, 3, 1, 0x42B}, {25368, 3, 1, 0x6EC0}, {25094, 3, 0, 0x55}};
    SmState m; SmReset(&m); SmChainReset(&ch); ChainPlay(&m, &c, &ch, &t, mix, 3);
    CHECK(m.countedHits == 9);
    // plain attack strings (no skill row) keep one use per run, as before
    const ChainSeg str[2] = {{25094, 5, 0, 0x55}, {25095, 5, 0, 0x55}};
    SmState p; SmReset(&p); SmChainReset(&ch); ChainPlay(&p, &c, &ch, &t, str, 2);
    CHECK(p.countedHits == 8);
    // row unreadable on a segment's first ticks: tentatively the same use, decided when readable
    SmState u; SmReset(&u); SmChainReset(&ch);
    const uint64_t r1 = ++g_chainRun << 32 | 25351, r2 = ++g_chainRun << 32 | 25352, r3 = ++g_chainRun << 32 | 25368;
    CHECK(SmChainStep(&ch, r1, 25351, 1, 0x42B) == r1);
    CHECK(SmChainStep(&ch, r2, 25352, -1, 0) == r1 && SmChainStep(&ch, r2, 25352, 1, 0x42B) == r1);  // same move once known
    CHECK(SmChainStep(&ch, r3, 25368, -1, 0) == r1 && SmChainStep(&ch, r3, 25368, 1, 0x6EC0) == r3);  // another move once known
    CHECK(SmChainStep(&ch, r3, 25368, 1, 0x6EC0) == r3);
    const uint64_t r4 = ++g_chainRun << 32 | 25369; uint64_t k4 = 0;
    for (int i = 0; i <= SM_CHAIN_WAIT; ++i) k4 = SmChainStep(&ch, r4, 25369, -1, 0);
    CHECK(k4 == r4);  // never readable: a new use after SM_CHAIN_WAIT ticks
    // loop penalty sees one use per cast: three casts in a row with the default LoopReps 2 -> the 3rd costs
    SmCfg l = c; l.loopReps = 2; SmState q; SmReset(&q); SmChainReset(&ch);
    for (int i = 0; i < 3; ++i) { ChainPlay(&q, &l, &ch, &t, ir, 3); SmChainStep(&ch, 0, SM_ACT_NONE, -1, 0); }
    CHECK(q.loopPens == 1 && q.countedHits == 8);
}
static void BurstBreak(void) {  // human: a boss's red (GreatAttack) attack interrupted by a hit in its wind-up
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SmBurst b; SmBurstEnd e; const uint32_t N = SM_ACT_NONE;
    CHECK(near(c.burst, 40) && c.burstMs == 1500 && c.burstSkill == 0);
    // interrupted: burst row, hit at 300 ms, the action changes (stagger) at 600 ms before any unblockable collider -> award
    SmBurstReset(&b);
    CHECK(SmBurstStep(&b, &c, 0, 1, 10, 100, 0, N, 0, 0, 0, &e) == SM_BB_NONE);  // a normal action first
    CHECK(SmBurstStep(&b, &c, 100, 1, 11, 200, 1, 0x205, 0, 0, 0, &e) == SM_BB_NONE && b.burst);
    CHECK(SmBurstStep(&b, &c, 300, 1, 11, 200, 1, 0x205, 0, 1, 22, &e) == SM_BB_NONE && b.hits == 1);
    CHECK(SmBurstStep(&b, &c, 600, 1, 12, 300, 0, N, 0, 0, 0, &e) == SM_BB_AWARD && e.act == 200 && e.newAct == 300 && near((float)e.ago, 300) && near(e.hitPts, 22) && e.hits == 1);
    CHECK(near(SmOnBurst(&s, &c, 600), 40) && s.bursts == 1 && s.lastScore == 600);
    // completes normally: hit in the wind-up, then the unblockable collider comes out -> none
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 20, 200, 1, N, 0, 0, 0, &e); SmBurstStep(&b, &c, 100, 1, 20, 200, 1, N, 0, 1, 20, &e);
    SmBurstStep(&b, &c, 500, 1, 20, 200, 1, N, 1, 0, 0, &e);  // run.great
    CHECK(SmBurstStep(&b, &c, 900, 1, 21, 300, 0, N, 0, 0, 0, &e) == SM_BB_MISS && e.great);
    // burst run with no hit at all: not even a candidate
    SmBurstReset(&b); SmBurstStep(&b, &c, 0, 1, 25, 200, 1, N, 0, 0, 0, &e);
    CHECK(SmBurstStep(&b, &c, 800, 1, 26, 300, 0, N, 0, 0, 0, &e) == SM_BB_NONE);
    // hit after the unblockable window opened: not counted -> none
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 30, 200, 1, N, 0, 0, 0, &e); SmBurstStep(&b, &c, 400, 1, 30, 200, 1, N, 1, 0, 0, &e);
    SmBurstStep(&b, &c, 500, 1, 30, 200, 1, N, 1, 1, 20, &e); CHECK(b.hits == 0);
    CHECK(SmBurstStep(&b, &c, 700, 1, 31, 300, 0, N, 0, 0, 0, &e) == SM_BB_NONE);
    // collider and hit in the same tick: the attack is out, the hit is too late
    SmBurstReset(&b); SmBurstStep(&b, &c, 0, 1, 35, 200, 1, N, 0, 0, 0, &e); SmBurstStep(&b, &c, 300, 1, 35, 200, 1, N, 1, 1, 20, &e); CHECK(b.hits == 0);
    // non-burst run hit and interrupted: none
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 40, 200, 0, N, 0, 0, 0, &e); SmBurstStep(&b, &c, 100, 1, 40, 200, 0, N, 0, 1, 20, &e);
    CHECK(SmBurstStep(&b, &c, 200, 1, 41, 300, 0, N, 0, 0, 0, &e) == SM_BB_NONE);
    // unread row (-1) never marks a burst
    SmBurstReset(&b); SmBurstStep(&b, &c, 0, 1, 45, 200, -1, N, 0, 1, 20, &e); CHECK(!b.burst);
    // same tick: the hit and the action change arrive together -> the hit counts for the old (burst) run
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 50, 200, 1, N, 0, 0, 0, &e);
    CHECK(SmBurstStep(&b, &c, 300, 1, 51, 300, 0, N, 0, 1, 18, &e) == SM_BB_AWARD && e.hits == 1 && e.ago == 0.0 && near(e.hitPts, 18));
    CHECK(b.hits == 0 && b.act == 300);  // the new run did not get that hit too
    // one award per burst run: several hits in the wind-up, one award when it ends; the next (non-burst) run gives none
    SmBurstReset(&b); int aw = 0;
    SmBurstStep(&b, &c, 0, 1, 60, 200, 1, N, 0, 0, 0, &e);
    for (int i = 1; i <= 4; ++i) aw += SmBurstStep(&b, &c, 100.0 * i, 1, 60, 200, 1, N, 0, 1, 20, &e) == SM_BB_AWARD;
    aw += SmBurstStep(&b, &c, 600, 1, 61, 300, 0, N, 0, 1, 20, &e) == SM_BB_AWARD; CHECK(e.hits == 5);
    for (int i = 0; i < 5; ++i) aw += SmBurstStep(&b, &c, 700.0 + 100 * i, 1, 61, 300, 0, N, 0, 0, 0, &e) == SM_BB_AWARD;
    aw += SmBurstStep(&b, &c, 1500, 1, 62, 400, 0, N, 0, 0, 0, &e) == SM_BB_AWARD;
    CHECK(aw == 1);
    // too late: the burst run ends more than BurstBreakMs after the last counted hit
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 70, 200, 1, N, 0, 1, 20, &e);  // first tick: marked and hit together (row read first)
    CHECK(b.hits == 1);
    CHECK(SmBurstStep(&b, &c, 1501, 1, 71, 300, 0, N, 0, 0, 0, &e) == SM_BB_MISS && e.late);
    // continuation: the wind-up flows into its row's next action, or into another burst row -> none
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 80, 200, 1, 0x12C, 0, 1, 20, &e);
    CHECK(SmBurstStep(&b, &c, 300, 1, 81, 0x12C, 0, N, 0, 0, 0, &e) == SM_BB_MISS && e.cont);
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 85, 200, 1, 0xFFFF, 0, 1, 20, &e);
    CHECK(SmBurstStep(&b, &c, 300, 1, 86, 201, 1, N, 0, 0, 0, &e) == SM_BB_MISS && e.cont);
    // boss gone mid wind-up: forgotten, no award
    SmBurstReset(&b);
    SmBurstStep(&b, &c, 0, 1, 90, 200, 1, N, 0, 1, 20, &e);
    CHECK(SmBurstStep(&b, &c, 100, 0, 0, N, -1, N, 0, 0, 0, &e) == SM_BB_NONE && !b.on);
    // BurstBreakSkillOnly: a plain hit is a miss, a skill / guardian spirit hit (2) awards
    SmCfg k = c; k.burstSkill = 1;
    SmBurstReset(&b); SmBurstStep(&b, &k, 0, 1, 100, 200, 1, N, 0, 1, 20, &e);
    CHECK(SmBurstStep(&b, &k, 300, 1, 101, 300, 0, N, 0, 0, 0, &e) == SM_BB_MISS && e.noSkill);
    SmBurstReset(&b); SmBurstStep(&b, &k, 0, 1, 110, 200, 1, N, 0, 2, 20, &e);
    CHECK(SmBurstStep(&b, &k, 300, 1, 111, 300, 0, N, 0, 0, 0, &e) == SM_BB_AWARD && e.hitKind == 2);
    // bonus 0: still counted, no points; clamp
    SmCfg z = c; z.burst = 0; SmState q; SmReset(&q); CHECK(SmOnBurst(&q, &z, 0) == 0.f && q.bursts == 1 && !q.active);
    SmCfg n = c; n.burst = -5; n.burstMs = 5; n.burstSkill = 7; SmCfgClamp(&n); CHECK(n.burst == 0.f && n.burstMs == 100 && n.burstSkill == 1);
}
static void Blocked(void) {  // human: a guarded hit is worth BlockPct % (default 50) of the same hit, same use rules, counts in the combo
    SmCfg r; RealDefaults(&r); CHECK(r.blockPct == 50);
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SmState h; SmReset(&h);
    const float full = SmOnHitEx(&h, &c, 1000, 7, 1, -1.f, 0);
    CHECK(near(SmOnBlockedHit(&s, &c, 1000, 7, 1, -1.f, 0), full * 0.5f) && s.blocked == 1 && s.hits == 1 && s.lastHit == 1000 && s.active);
    // same use: multi-hit factor applies to the blocked hit exactly as to a normal hit
    const float h2 = SmOnHitEx(&h, &c, 1010, 7, 1, -1.f, 0);
    CHECK(near(SmOnBlockedHit(&s, &c, 1010, 7, 1, -1.f, 0), h2 * 0.5f) && s.blocked == 2);
    // a new use of the same move: variety penalty as for a normal hit; a blocked hit fills the variety history like a hit
    const float h3 = SmOnHitEx(&h, &c, 2000, 7, 2, -1.f, 0);
    CHECK(near(SmOnBlockedHit(&s, &c, 2000, 7, 2, -1.f, 0), h3 * 0.5f));
    // startup and counter bonus scale too
    SmState a; SmReset(&a); SmState b; SmReset(&b);
    const float hc = SmOnHitEx(&b, &c, 0, 9, 3, 60.f, 1);
    CHECK(near(SmOnBlockedHit(&a, &c, 0, 9, 3, 60.f, 1), hc * 0.5f) && a.counters == 1);
    // a normal hit after a blocked hit of the same use continues the use (multi-hit)
    SmState m; SmReset(&m); SmOnBlockedHit(&m, &c, 0, 11, 4, -1.f, 0);
    CHECK(near(SmOnHitEx(&m, &c, 5, 11, 4, -1.f, 0), c.hit * c.multiPct / 100.f));
    // pct 100 = a normal hit; 0 = off (nothing counted)
    SmCfg f = c; f.blockPct = 100; SmState q; SmReset(&q); CHECK(near(SmOnBlockedHit(&q, &f, 0, 5, 1, -1.f, 0), c.hit));
    SmCfg z = c; z.blockPct = 0; SmState o; SmReset(&o); CHECK(SmOnBlockedHit(&o, &z, 0, 5, 1, -1.f, 0) == 0.f && o.blocked == 0 && o.hits == 0 && !o.active);
    SmCfg k = c; k.blockPct = -3; SmCfgClamp(&k); CHECK(k.blockPct == 0); k.blockPct = 250; SmCfgClamp(&k); CHECK(k.blockPct == 100);
    // the combo counts blocked hits like hits (the game glue passes hits + blocked)
    uint32_t mi = 0; SmState cb; SmReset(&cb); SmComboHits(&cb, &c, 0, 1 + 1, &mi); CHECK(cb.combo == 2);
}
static void BlockQueue(void) {  // which hits were guarded: (hit, victim) entries, decided one tick after the first note
    SmBlkQ q; memset(&q, 0, sizeof q); SmBlkEv o[4];
    SmBlkNote(&q, 0x100, 0xA0, SM_BLK_KI, 12.f);                // guarded: Ki damage, no HP damage call
    SmBlkNote(&q, 0x200, 0xA0, SM_BLK_HP, 0.f);                 // normal hit: HP then Ki (same hit object)
    SmBlkNote(&q, 0x200, 0xA0, SM_BLK_KI, 8.f);
    SmBlkNote(&q, 0x300, 0xB0, SM_BLK_HP0, 0.f);                // guarded: a 0 HP damage call
    SmBlkNote(&q, 0x400, 0xB0, SM_BLK_KI, 5.f);                 // Ki first, HP damage comes in the next tick (still one hit)
    CHECK(q.n == 4);
    CHECK(SmBlkTake(&q, o, 4) == 0 && q.n == 4);                // nothing is decided on the tick it was noted
    SmBlkNote(&q, 0x400, 0xB0, SM_BLK_HP, 0.f);
    SmBlkNote(&q, 0x300, 0xB0, SM_BLK_KI, 3.f);                 // late Ki of the 0-HP hit merges
    SmBlkNote(&q, 0x500, 0xA0, SM_BLK_KI, 1.f);                 // new this tick
    CHECK(SmBlkTake(&q, o, 4) == 2 && q.n == 1);
    CHECK(o[0].hit == 0x100 && near(o[0].ki, 12.f) && o[0].fl == SM_BLK_KI);
    CHECK(o[1].hit == 0x300 && o[1].fl == (SM_BLK_HP0 | SM_BLK_KI) && near(o[1].ki, 3.f));
    CHECK(SmBlkTake(&q, o, 0) == 1 && q.n == 0);                // counted even when no copy is wanted
    // one hit object on two victims: separate entries
    SmBlkNote(&q, 0x600, 0xA0, SM_BLK_HP, 0.f); SmBlkNote(&q, 0x600, 0xB0, SM_BLK_KI, 2.f);
    SmBlkTake(&q, o, 4); CHECK(SmBlkTake(&q, o, 4) == 1 && o[0].vic == 0xB0);
    SmBlkNote(&q, 0, 0xA0, SM_BLK_KI, 1.f); SmBlkNote(&q, 0x700, 0, SM_BLK_KI, 1.f); SmBlkNote(&q, 0x700, 0xA0, 0, 1.f); CHECK(q.n == 0);  // bad input ignored
    for (int i = 0; i < SM_BLK_Q + 3; ++i) SmBlkNote(&q, 0x1000 + (uint64_t)i, 0xA0, SM_BLK_KI, 1.f);
    CHECK(q.n == SM_BLK_Q && q.dropped == 3);
    SmBlkTake(&q, 0, 0); CHECK(SmBlkTake(&q, 0, 0) == SM_BLK_Q && q.n == 0);
}
int main(void) {
    Loop(); MultiCap(); MultiChain();
    BurstBreak();
    Blocked(); BlockQueue();
    Combo();
    GuardianSummon();
    WeaponSwitch();
    Defaults(); Promotion(); Cap(); Variety(); Damage(); Decay(); Dodge(); Visible(); Clamp(); KiGate(); Tuning(); Startup(); Counter(); Skills(); Style(); StyleSoul(); MoveFmt(); Swap(); Pops(); Motion(); StyleIdle(); NinGate(); KiPerfect(); Ramp(); GuardianSpirit(); MartialArt();
    printf("style_meter_test: all %d checks passed\n", g_checks);
    return 0;
}
