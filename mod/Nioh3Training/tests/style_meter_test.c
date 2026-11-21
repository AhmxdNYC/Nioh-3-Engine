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
// human #126755: shipped tuning is 2.5x the first (#126785) as the first one.
static void Tuning(void) {
    SmCfg c; RealDefaults(&c);
    CHECK(near(c.rankPts[0], 100.f) && near(c.rankPts[SM_RANKS - 1], 500.f) && near(c.decayPerS, 12.f) && c.decayDelayMs == 4000);
    SmState s; SmReset(&s);
    for (uint32_t i = 0; i < 4; ++i) SmOnHit(&s, &c, 10.0 * i, 3000 + i, i);
    CHECK(s.rank == 0 && near(SmGauge(&s, &c), 0.4f));  // 4 different moves = 40 % of D now
}
// #126821 startup scaling: x1 at <= min, x(1+pct) at >= max, linear between; unknown = x1; multi-hit keeps the use's factor.
static void Startup(void) {
    SmCfg c; RealDefaults(&c);
    CHECK(near(SmStartupMult(&c, -1.f), 1.f) && near(SmStartupMult(&c, 5.f), 1.f) && near(SmStartupMult(&c, 15.f), 1.f));
    CHECK(near(SmStartupMult(&c, 37.5f), 1.5f) && near(SmStartupMult(&c, 60.f), 2.f) && near(SmStartupMult(&c, 200.f), 2.f));
    SmState s; SmReset(&s);
    CHECK(near(SmOnHitEx(&s, &c, 0, 5, 1, 60.f, 0), 2.f * c.hit));            // slow startup: double
    CHECK(near(SmOnHitEx(&s, &c, 10, 5, 1, 60.f, 0), 2.f * c.hit * 0.5f));     // second hit of the same use: multi-hit share of it
    CHECK(near(SmOnHitEx(&s, &c, 20, 6, 2, 10.f, 0), c.hit));                  // fast jab: plain
    SmCfg z = c; z.startupPct = 0; CHECK(near(SmStartupMult(&z, 60.f), 1.f));
    SmCfg b = c; b.startupMaxF = 3; b.startupMinF = 9; SmCfgClamp(&b); CHECK(b.startupMaxF == 10);
}
// #126843 counter: flat bonus once per use, on top of the scaled hit.
static void Counter(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
    CHECK(near(SmOnHitEx(&s, &c, 0, 7, 1, -1.f, 1), c.hit + c.counter) && s.counters == 1);
    CHECK(near(SmOnHitEx(&s, &c, 5, 7, 1, -1.f, 1), c.hit * 0.5f) && s.counters == 1);  // same use: no second bonus
}
// #126850 / #126854 skills: only in combat (a hit within combatMs); outside they do not wake the meter.
static void Skills(void) {
    SmCfg c; RealDefaults(&c); SmState s; SmReset(&s);
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
    SmCfg c; RealDefaults(&c); c.decayDelayMs = 0; c.decayRankPct = 0; c.decayPerS = 10.f;
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
    CHECK(q.n == 2 && !strcmp(q.p[1].label, "Ninjutsu") && near(q.p[1].pts, 40));
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
int main(void) {
    Defaults(); Promotion(); Cap(); Variety(); Damage(); Decay(); Dodge(); Visible(); Clamp(); KiGate(); Tuning(); Startup(); Counter(); Skills(); Style(); StyleSoul(); MoveFmt(); Swap(); Pops(); Motion(); StyleIdle(); NinGate(); KiPerfect(); Ramp();
    printf("style_meter_test: all %d checks passed\n", g_checks);
    return 0;
}
