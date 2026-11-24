// Style meter fight result (style_result.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu -O0 -UNDEBUG tests/style_result_test.c -o build/style_result_test.exe && build/style_result_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/style_meter.h"
#include "../src/style_result.h"
static int g_checks;
#define CHECK(x) do { assert(x); ++g_checks; } while (0)
static int near(double a, double b, double e) { return a - b < e && b - a < e; }
// Put the meter at rank r with gauge g (0..1).
static void At(SmState* s, const SmCfg* c, int r, float g) { s->active = r > 0 || g > 0.f; s->rank = r; s->pts = g * c->rankPts[r]; }
static void Run(SrFight* f, SmState* s, const SmCfg* c, int r, float g, double ms) { At(s, c, r, g); for (double t = 0; t < ms; t += 10) SrTick(f, s, c, 10); }
static SrResult Done(SrFight* f, const SmState* s, int hold) { return SrFinish(f, s, hold); }

// Your example: C for half the fight, A for the other half -> B. The gauge counts: mid C (1.5) and mid A (3.5) average 2.5.
static void HalfAndHalf(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossHit(&f, &s, 0x1000);
    Run(&f, &s, &c, 1, 0.5f, 60000); Run(&f, &s, &c, 3, 0.5f, 60000);
    const SrResult r = Done(&f, &s, SR_HOLD_PCT);
    CHECK(near(r.avg, 2.5, 0.01) && r.grade == 2 && r.byAvg == 2 && r.peak == 3);
    CHECK(near(r.pctAt[1], 50, 0.1) && near(r.pctAt[3], 50, 0.1) && near(r.ms, 120000, 1));
}
// A few seconds at SSS in a long fight barely moves the average.
static void ShortBurst(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossHit(&f, &s, 0x1000);
    Run(&f, &s, &c, 2, 0.5f, 170000); Run(&f, &s, &c, 6, 1.f, 10000);
    const SrResult r = Done(&f, &s, SR_HOLD_PCT);
    CHECK(r.peak == 6 && r.grade == 2 && r.avg < 3.f);
}
// Rule 2: a high average built from a short top burst over a low floor does not reach that rank.
static void HoldRule(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossHit(&f, &s, 0x1000);
    Run(&f, &s, &c, 0, 0.f, 70000); Run(&f, &s, &c, 6, 0.9f, 30000);   // idle 70 %, SSS 30 %: average 2.07
    SrResult r = Done(&f, &s, SR_HOLD_PCT);
    CHECK(r.byAvg == 2 && r.grade == 0);                                // only 30 % at B or higher (and at C or higher): D
    r = Done(&f, &s, 25);                                               // looser rule: 30 % >= 25 % at B
    CHECK(r.grade == 2);
    r = Done(&f, &s, 0);                                                // rule 2 off: the average alone
    CHECK(r.grade == r.byAvg);
}
// SSS needs nearly the whole fight at SSS: climbing there first already costs it.
static void SssIsHard(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossHit(&f, &s, 0x1000);
    for (int r = 0; r < 6; ++r) Run(&f, &s, &c, r, 0.5f, 5000);         // 30 s climbing D -> SS
    Run(&f, &s, &c, 6, 1.f, 150000);                                    // then SSS with a full gauge for 2.5 minutes
    SrResult rr = Done(&f, &s, SR_HOLD_PCT);
    CHECK(rr.grade == 6 && rr.avg >= 6.f);
    SrReset(&f); SrBossHit(&f, &s, 0x1000);
    for (int r = 0; r < 6; ++r) Run(&f, &s, &c, r, 0.5f, 5000);
    Run(&f, &s, &c, 6, 0.3f, 60000);                                    // SSS with a low gauge: average under 6
    rr = Done(&f, &s, SR_HOLD_PCT);
    CHECK(rr.grade == 5);
}
// Fight length is not scored: the same meter for 10 s or for 10 minutes grades the same. A one-tick kill grades from that one tick.
static void NoClearTime(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f;
    SrReset(&f); SrBossHit(&f, &s, 0x1000); Run(&f, &s, &c, 3, 0.5f, 10000);
    const SrResult a = Done(&f, &s, SR_HOLD_PCT);
    SrReset(&f); SrBossHit(&f, &s, 0x1000); Run(&f, &s, &c, 3, 0.5f, 600000);
    const SrResult b = Done(&f, &s, SR_HOLD_PCT);
    CHECK(a.grade == b.grade && near(a.avg, b.avg, 0.001));
    SrReset(&f); SrBossHit(&f, &s, 0x1000);                            // one-shot: killed on the first hit, no tick yet
    const SrResult z = Done(&f, &s, SR_HOLD_PCT);
    CHECK(z.grade == 0 && z.avg == 0.f && near(z.pctAt[0], 100, 0.01));
}
// Fight lifecycle: duo (both must die), player death drops it, a boss leaving alive drops it, a next form during the end wait continues.
static void Lifecycle(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossNow now[SR_BOSSES]; memset(now, 0, sizeof now);
    CHECK(SrStep(&f, now, 0, 0, 0, 1000) == 0 && !f.on);                // no fight before a boss hit
    SrBossHit(&f, &s, 0xA); SrBossHit(&f, &s, 0xB); SrBossHit(&f, &s, 0xA);
    CHECK(f.on && f.nBoss == 2);
    now[0].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 100, 1000) == 0);                       // B still up
    now[1].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 200, 1000) == 1 && f.ending);           // both dead: end wait
    Run(&f, &s, &c, 4, 0.5f, 5000);
    CHECK(f.ms == 0);                                                   // nothing counts in the wait
    CHECK(SrStep(&f, now, 0, 0, 700, 1000) == 0);
    SrBossHit(&f, &s, 0xC);                                             // a next form is hit inside the wait: same fight
    memset(now, 0, sizeof now); now[0].dead = now[1].dead = 1;
    CHECK(!f.ending && f.nBoss == 3 && SrStep(&f, now, 0, 0, 800, 1000) == 0);
    now[2].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 900, 1000) == 1 && SrStep(&f, now, 0, 0, 1950, 1000) == 2);
    CHECK(SrFinish(&f, &s, SR_HOLD_PCT).bosses == 3);
    SrReset(&f); SrBossHit(&f, &s, 0xA); memset(now, 0, sizeof now);
    CHECK(SrStep(&f, now, 1, 0, 0, 1000) == -1);                        // player died
    SrReset(&f); SrBossHit(&f, &s, 0xA); now[0].gone = 1;
    CHECK(SrStep(&f, now, 0, 0, 0, 1000) == -1);                        // boss left alive (Training reset)
    SrReset(&f); SrBossHit(&f, &s, 0xA); now[0].gone = 1; now[0].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 0, 1000) == 1);                         // gone after dying: fine
    SrReset(&f); SrBossHit(&f, &s, 0xA); memset(now, 0, sizeof now); now[0].dead = 1;
    CHECK(SrStep(&f, now, 0, 1, 0, 1000) == 0 && !f.ending);            // another boss still up, not hit yet
}
// Counts are the difference over the fight; distinct moves / martial arts are sets.
static void Counts(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SmOnHit(&s, &c, 0, 5, 1); SmOnHit(&s, &c, 10, 6, 2);                // before the fight: not counted
    SrBossHit(&f, &s, 0xA);
    SmOnHit(&s, &c, 100, 7, 3); SmOnHit(&s, &c, 200, 8, 4); SmOnDodge(&s, &c, 300); SmOnDamaged(&s, &c, 400);
    SrMove(&f, 7); SrMove(&f, 8); SrMove(&f, 7); SrMove(&f, SM_ACT_NONE); SrMartialArt(&f, 8); SrMartialArt(&f, 8);
    const SrResult r = SrFinish(&f, &s, SR_HOLD_PCT);
    CHECK(r.n.hits == 2 && r.n.dodges == 1 && r.n.damaged == 1 && r.moves == 2 && r.maMoves == 1);
    char b[600]; CHECK(SrFmt(b, sizeof b, &r) > 0 && strstr(b, "moves 2") && strstr(b, "1 different"));
}
// Dodge / Deflect / Counter: a clean avoid with another skill move in the window = counter; else during the deflect = deflect; else dodge.
// (The glue never reports the deflect as 'another skill', so a deflect alone is a deflect.)
// Damage or no i-frames = none of them.
static void DodgeDeflectCounter(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SmDodge d; memset(&d, 0, sizeof d);
    SmDodgeStepEx(&d, 1, 1, 0, 0, 0); SmDodgeStepEx(&d, 1, 0, 0, 0, 0); CHECK(SmDodgeStepEx(&d, 0, 0, 0, 0, 0) == SM_AV_DODGE);
    SmDodgeStepEx(&d, 1, 0, 0, 0, 0); SmDodgeStepEx(&d, 1, 1, 0, 1, 0); CHECK(SmDodgeStepEx(&d, 0, 0, 0, 0, 0) == SM_AV_DEFLECT);
    SmDodgeStepEx(&d, 1, 1, 0, 0, 0); CHECK(SmDodgeStepEx(&d, 0, 0, 0, 1, 0) == SM_AV_DEFLECT);                     // signal on the close tick
    SmDodgeStepEx(&d, 1, 1, 0, 0, 1); CHECK(SmDodgeStepEx(&d, 0, 0, 0, 0, 0) == SM_AV_COUNTER);                     // skill move: counter
    SmDodgeStepEx(&d, 1, 1, 0, 1, 1); CHECK(SmDodgeStepEx(&d, 0, 0, 0, 0, 0) == SM_AV_COUNTER);                     // skill wins over the deflect flag
    SmDodgeStepEx(&d, 1, 1, 0, 1, 1); CHECK(SmDodgeStepEx(&d, 0, 0, 1, 0, 0) == SM_AV_NONE);                        // hit: nothing
    SmDodgeStepEx(&d, 1, 0, 0, 1, 0); CHECK(SmDodgeStepEx(&d, 0, 0, 0, 1, 0) == SM_AV_NONE);                        // guard flag, no i-frames
    SmDodgeStepEx(&d, 1, 1, 0, 1, 1); SmDodgeStepEx(&d, 0, 0, 0, 0, 0);
    SmDodgeStepEx(&d, 1, 1, 0, 0, 0); CHECK(SmDodgeStepEx(&d, 0, 0, 0, 0, 0) == SM_AV_DODGE);                       // no leak into the next window
    SrFight f; SrReset(&f); SrBossHit(&f, &s, 0xA);
    CHECK(near(SmOnDeflect(&s, &c, 10), c.deflect, 0.01) && near(SmOnDodge(&s, &c, 20), c.dodge, 0.01));
    CHECK(near(SmOnAvoidCounter(&s, &c, 30, 1), c.dodge, 0.01) && near(SmOnAvoidCounter(&s, &c, 40, 0), c.dodge, 0.01));  // 2nd: same move, not counted
    const SrResult r = SrFinish(&f, &s, SR_HOLD_PCT);
    CHECK(r.n.deflects == 1 && r.n.dodges == 1 && r.n.counterAvoids == 1);
}
// Hit penalty: each hit taken lowers the average by 0.25 before grading; 0 = off.
static void HitPenalty(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossHit(&f, &s, 0x1000);
    Run(&f, &s, &c, 3, 0.6f, 60000);                                   // A for the whole fight: 3.6
    SrResult r = SrFinishEx(&f, &s, SR_HOLD_PCT, SR_HIT_PENALTY);
    CHECK(r.grade == 3 && near(r.penalty, 0, 0.001));                  // no hits: A
    for (int i = 0; i < 4; ++i) SmOnDamaged(&s, &c, 100000 + i * 5000);
    r = SrFinishEx(&f, &s, SR_HOLD_PCT, SR_HIT_PENALTY);
    CHECK(r.n.damaged == 4 && near(r.penalty, 1.0, 0.001) && near(r.rawAvg, 3.6, 0.01) && near(r.avg, 2.6, 0.01) && r.grade == 2);  // 4 hits: B
    for (int i = 0; i < 20; ++i) SmOnDamaged(&s, &c, 200000 + i * 5000);
    r = SrFinishEx(&f, &s, SR_HOLD_PCT, SR_HIT_PENALTY);
    CHECK(r.avg == 0.f && r.grade == 0);                               // never below 0
    CHECK(SrFinishEx(&f, &s, SR_HOLD_PCT, 0).grade == 3);              // penalty off
}
int main(void) {
    HitPenalty();
    DodgeDeflectCounter(); HalfAndHalf(); ShortBurst(); HoldRule(); SssIsHard(); NoClearTime(); Lifecycle(); Counts();
    printf("style_result_test: all %d checks passed\n", g_checks);
    return 0;
}
