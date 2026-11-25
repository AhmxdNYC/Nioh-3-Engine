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
    CHECK(SrStep(&f, now, 1, 0, 0, 1000) == 1);                         // dying after the kill still gets the card
    CHECK(SrStep(&f, now, 0, 0, 2000, 1000) == 2);
}
// Gauntlet: 3 bosses in a row, each spawning a few seconds after the last one died -> 3 fights, 3 results, each with its own time.
// The next boss can already be up (not hit) while the previous one dies: it never holds that card back.
static void Gauntlet(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossNow now[SR_BOSSES]; int cards = 0; double t = 0;
    for (int b = 0; b < 3; ++b) {
        const uint64_t a = 0x100 + b;
        CHECK(SrBossHitEx(&f, &s, a, t, SR_FORM_MS) == 0 && f.on && f.nBoss == 1 && f.ms == 0);
        Run(&f, &s, &c, b + 1, 0.5f, 20000 * (b + 1)); t += 20000 * (b + 1);
        memset(now, 0, sizeof now);
        CHECK(SrStep(&f, now, 0, 0, t, SR_END_DELAY_MS) == 0);         // still up (another boss up and not hit: no effect)
        now[0].dead = 1;
        CHECK(SrStep(&f, now, 0, 0, t, SR_END_DELAY_MS) == 1);
        t += SR_END_DELAY_MS;
        CHECK(SrStep(&f, now, 0, 0, t, SR_END_DELAY_MS) == 2);
        const SrResult r = SrFinishEx(&f, &s, SR_HOLD_PCT, 0); SrReset(&f); ++cards;
        CHECK(r.bosses == 1 && near(r.ms, 20000.0 * (b + 1), 1) && r.grade == b + 1);
        t += 4000;                                                      // the next boss walks in later
    }
    CHECK(cards == 3);
    // The next boss was already up (seen at the fight start) and is hit 1 s after the kill, inside the end wait: card for the first boss now.
    SrReset(&f); t = 0;
    SrBossHitEx(&f, &s, 0xA, 0, SR_FORM_MS); Run(&f, &s, &c, 2, 0.5f, 10000); t = 10000;
    memset(now, 0, sizeof now); now[0].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, t, SR_END_DELAY_MS) == 1);
    CHECK(SrBossHitEx(&f, &s, 0xB, 0, SR_FORM_MS) == 1 && f.nBoss == 1 && f.ending);  // not a form: caller finishes this fight first
    SrResult r = SrFinishEx(&f, &s, SR_HOLD_PCT, 0); SrReset(&f);
    CHECK(r.bosses == 1 && near(r.ms, 10000, 1));
    CHECK(SrBossHitEx(&f, &s, 0xB, 0, SR_FORM_MS) == 0 && f.on && f.nBoss == 1 && f.boss[0] == 0xB && f.ms == 0);
    // A boss that only appears after the form window but is hit inside the end wait is the next boss too.
    SrReset(&f); SrBossHitEx(&f, &s, 0xA, 0, SR_FORM_MS); now[0].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 5000, SR_END_DELAY_MS) == 1);
    CHECK(SrBossHitEx(&f, &s, 0xB, 5000 + SR_FORM_MS + 1, SR_FORM_MS) == 1);
}
// Duo: two bosses up together, both hit -> one fight, one result with 2 bosses; the card waits for the second.
static void Duo(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossNow now[SR_BOSSES]; memset(now, 0, sizeof now);
    SrBossHitEx(&f, &s, 0xA, 0, SR_FORM_MS); Run(&f, &s, &c, 3, 0.5f, 5000);
    CHECK(SrBossHitEx(&f, &s, 0xB, 0, SR_FORM_MS) == 0 && f.nBoss == 2);   // A still up: B joins whatever its seen time
    now[0].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 6000, SR_END_DELAY_MS) == 0);
    Run(&f, &s, &c, 3, 0.5f, 5000);
    now[1].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 11000, SR_END_DELAY_MS) == 1 && SrStep(&f, now, 0, 0, 14000, SR_END_DELAY_MS) == 2);
    const SrResult r = SrFinishEx(&f, &s, SR_HOLD_PCT, 0);
    CHECK(r.bosses == 2 && near(r.ms, 10000, 1));
}
// Second form: a new actor that gets its bar just as the first form goes down (or a moment before) -> same fight, one result.
static void SecondForm(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SrBossNow now[SR_BOSSES]; memset(now, 0, sizeof now);
    SrBossHitEx(&f, &s, 0xA, 0, SR_FORM_MS); Run(&f, &s, &c, 2, 0.5f, 8000);
    now[0].hp0 = 1;                                                     // HP 0, death not applied yet: down
    CHECK(SrStep(&f, now, 0, 0, 8000, SR_END_DELAY_MS) == 1);
    now[0].gone = 1; now[0].hp0 = 0;                                    // the first form's actor is torn down
    CHECK(SrStep(&f, now, 0, 0, 8500, SR_END_DELAY_MS) == 0 && f.dead[0]);
    CHECK(SrBossHitEx(&f, &s, 0xB, 8000 + 1200, SR_FORM_MS) == 0 && f.nBoss == 2 && !f.ending);   // seen 1.2 s after: next form
    memset(now, 0, sizeof now); now[0].gone = 1;
    CHECK(SrStep(&f, now, 0, 0, 9500, SR_END_DELAY_MS) == 0);
    Run(&f, &s, &c, 2, 0.5f, 8000);
    now[1].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 17000, SR_END_DELAY_MS) == 1 && SrStep(&f, now, 0, 0, 20000, SR_END_DELAY_MS) == 2);
    SrResult r = SrFinishEx(&f, &s, SR_HOLD_PCT, 0);
    CHECK(r.bosses == 2 && near(r.ms, 16000, 1));
    SrReset(&f); memset(now, 0, sizeof now);                            // its bar showed a frame before the old HP read 0: still the form
    SrBossHitEx(&f, &s, 0xA, 0, SR_FORM_MS); now[0].dead = 1;
    CHECK(SrStep(&f, now, 0, 0, 3000, SR_END_DELAY_MS) == 1 && SrBossHitEx(&f, &s, 0xB, 3000 - 100, SR_FORM_MS) == 0 && f.nBoss == 2);
}
// Picture Scroll / mission end: a boss torn down right at the kill, and a load right after it.
static void Teardown(void) {
    SmState s; SmReset(&s); SrFight f; SrBossNow now[SR_BOSSES];
    SrReset(&f); memset(now, 0, sizeof now); SrBossHit(&f, &s, 0xA);
    now[0].hp0 = 1; CHECK(SrStep(&f, now, 0, 0, 100, 1000) == 1);        // HP 0 seen
    now[0].hp0 = 0; now[0].gone = 1;
    CHECK(SrStep(&f, now, 0, 0, 200, 1000) == 0 && f.dead[0]);          // then gone: killed, not 'left'
    CHECK(SrStep(&f, now, 0, 0, 1200, 1000) == 2 && SrFinish(&f, &s, SR_HOLD_PCT).bosses == 1);
    SrReset(&f); memset(now, 0, sizeof now); SrBossHit(&f, &s, 0xA);
    now[0].gone = 1; now[0].hp0 = 1;                                    // gone on the same tick as the lethal hit (no HP 0 read)
    CHECK(SrStep(&f, now, 0, 0, 0, 1000) == 1);
    SrReset(&f); memset(now, 0, sizeof now); SrBossHit(&f, &s, 0xA);
    now[0].hp0 = 1; SrStep(&f, now, 0, 0, 0, 1000);                     // HP 0 then healed back (phase heal / floor): up again
    now[0].hp0 = 0; CHECK(SrStep(&f, now, 0, 0, 50, 1000) == 0 && !f.ending);
    now[0].gone = 1; CHECK(SrStep(&f, now, 0, 0, 60, 1000) == -1 && f.drop == SR_DROP_LEFT);
    // load after the kill: the card at once (also with the player gone), not a drop
    SrReset(&f); memset(now, 0, sizeof now); SrBossHit(&f, &s, 0xA);
    now[0].hp0 = 1; CHECK(SrStep(&f, now, 0, 0, 0, 3000) == 1);
    CHECK(SrStep(&f, now, 0, 1, 10, 3000) == 2 && SrFinish(&f, &s, SR_HOLD_PCT).bosses == 1);
    SrReset(&f); memset(now, 0, sizeof now); SrBossHit(&f, &s, 0xA);
    now[0].dead = 1; CHECK(SrStep(&f, now, 1, 1, 0, 3000) == 2);       // killed and the load on the same tick
    // load with the boss up: drop (mission left / quit)
    SrReset(&f); memset(now, 0, sizeof now); SrBossHit(&f, &s, 0xA);
    CHECK(SrStep(&f, now, 0, 1, 0, 3000) == -1 && f.drop == SR_DROP_LOAD);
    SrReset(&f); memset(now, 0, sizeof now); SrBossHit(&f, &s, 0xA);
    CHECK(SrStep(&f, now, 1, 0, 0, 3000) == -1 && f.drop == SR_DROP_PLAYER);
}
// Counts are the difference over the fight; distinct moves / martial arts are sets.
static void Counts(void) {
    SmCfg c; SmCfgDefaults(&c); SmState s; SmReset(&s); SrFight f; SrReset(&f);
    SmOnHit(&s, &c, 0, 5, 1); SmOnHit(&s, &c, 10, 6, 2);                // before the fight: not counted
    SrBossHit(&f, &s, 0xA);
    SmOnHit(&s, &c, 100, 7, 3); SmOnHit(&s, &c, 200, 8, 4); SmOnDodge(&s, &c, 300); SmOnBurst(&s, &c, 350); SmOnDamaged(&s, &c, 400);
    SrMove(&f, 7); SrMove(&f, 8); SrMove(&f, 7); SrMove(&f, SM_ACT_NONE); SrMartialArt(&f, 8); SrMartialArt(&f, 8);
    const SrResult r = SrFinish(&f, &s, SR_HOLD_PCT);
    CHECK(r.n.bursts == 1 && r.n.hits == 2 && r.n.dodges == 1 && r.n.damaged == 1 && r.moves == 2 && r.maMoves == 1);
    char b[600]; CHECK(SrFmt(b, sizeof b, &r) > 0 && strstr(b, "moves 2") && strstr(b, "1 different") && strstr(b, "burst breaks 1"));
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
    Gauntlet(); Duo(); SecondForm(); Teardown();
    printf("style_result_test: all %d checks passed\n", g_checks);
    return 0;
}
