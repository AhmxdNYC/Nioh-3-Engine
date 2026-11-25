// Style meter fight result: an average rank for one boss fight, shown on a card when the boss dies. Pure (no game access, no Win32);
// tests/style_result_test.c. Game glue: style_meter_game.h (SrFeed from SmFeed, once per FRAMEPROBE pump tick, game clock in ms).
//
// The fight runs from the player's first hit that takes HP off a boss to the death of the last boss that was hit. One fight can hold several
// bosses: a duo (a second boss hit while one of the fight is still up) or a second form (see below). The player dying, an engaged boss
// leaving with HP left (Training reset, quitting) or a load while a boss of the fight is still up drops the fight with no card (f->drop says why).
//
// Down / dead. A boss is down when its death is applied or its HP is 0 now (the death blow / finisher animation runs at HP 0 before the death
// flag). A boss that leaves the actor table after its HP reached 0 (Picture Scroll / mission end tear-down) counts as killed. A load that
// starts once every boss of the fight is down finishes the fight at once (the card shows after the load) instead of dropping it.
//
// Gauntlet vs second form (several bosses in a row, one card each). When every boss of the fight is down the fight waits endDelayMs for a
// next form. A new boss hit during that wait is a second form (same fight, same card) only if it was first seen (got its boss slot / HUD bar)
// from formSlack ms before the fight went down to formMs after it: a second form replaces the first at once. A boss that was already up
// earlier (the next gauntlet boss waiting) or came later is the next fight: SrBossHitEx returns 1, the caller finishes this fight (its card)
// and starts a fresh fight with that hit. A boss that is up but never hit never holds a card back.
//
// Score. Every tick the meter is one number v = rank + gauge (D = 0 .. SSS with a full gauge = 7; an idle meter is 0). The score is the time
// average of v over the fight, weighted by game time (pause, menus and loading stop the clock), so a few seconds at a rank barely move it.
// Fight length is never part of the score: a fast kill earns nothing for being fast (a one-shot fight is a few ms long and scores whatever
// the meter showed, usually D).
//
// Hit penalty. Every hit taken during the fight subtracts hitPenalty (default 0.25, [StyleMeter] ResultHitPenalty in hundredths) from the
// average before grading: 4 hits cost a full rank. (The live meter still drops one rank per hit on top of that.)
//
// Grade. Two rules, both must hold for rank R:
//   1. floor(average) >= R                                (C for half the fight and A for the other half: average about 2.5 -> B)
//   2. at least holdPct % of the fight was spent at rank R or higher   (a high average built on a short burst does not count)
// The grade is the highest R that passes both. The card says which rule held it back when rule 2 lowered it.
#ifndef STYLE_RESULT_H
#define STYLE_RESULT_H
#include <stdint.h>
#include "style_meter.h"

#define SR_BOSSES 8
#define SR_ACTS 192             // distinct move ids remembered per fight (more just stop counting)
#define SR_HOLD_PCT 45          // [StyleMeter] ResultHoldPct (human 2026-11-25: was 50)
#define SR_END_DELAY_MS 3000    // [StyleMeter] ResultEndDelayMs: wait after the last boss death for a next form
#define SR_FORM_MS 1500         // [StyleMeter] ResultFormMs: a boss first seen this long after the fight went down is still its next form
#define SR_FORM_SLACK_MS 500    // ... or this long before it (the next form's bar can show a frame before the old HP reads 0)
#define SR_HIT_PENALTY 25        // [StyleMeter] ResultHitPenalty: average lost per hit taken, in hundredths (25 = 0.25)
#define SR_SCALE_PCT 140        // [StyleMeter] ResultScalePct: card size (100 = 640 px wide at 1080p)
#define SR_SHOW_MS 0            // [StyleMeter] ResultShowMs: 0 = the card stays up until the player presses confirm; > 0 = also hides after that many wall ms

// Counters taken from SmState at the fight start and at the end (the card shows the difference).
typedef struct {
    uint32_t hits, dodges, kis, kiPerfects, damaged, counters, nins, souls, styles, swapHits, gsHits, maHits, deflects, counterAvoids;
} SrCount;
static inline SrCount SrCountOf(const SmState* s) {
    SrCount c; c.hits = s->hits; c.dodges = s->dodges; c.kis = s->kis; c.kiPerfects = s->kiPerfects; c.damaged = s->damaged;
    c.counters = s->counters; c.nins = s->nins; c.souls = s->souls; c.styles = s->styles; c.swapHits = s->swapHits; c.gsHits = s->gsHits;
    c.maHits = s->maHits; c.deflects = s->deflects; c.counterAvoids = s->counterAvoids; return c;
}
static inline SrCount SrCountSub(SrCount a, SrCount b) {
    SrCount c; c.hits = a.hits - b.hits; c.dodges = a.dodges - b.dodges; c.kis = a.kis - b.kis; c.kiPerfects = a.kiPerfects - b.kiPerfects;
    c.damaged = a.damaged - b.damaged; c.counters = a.counters - b.counters; c.nins = a.nins - b.nins; c.souls = a.souls - b.souls;
    c.styles = a.styles - b.styles; c.swapHits = a.swapHits - b.swapHits; c.gsHits = a.gsHits - b.gsHits; c.maHits = a.maHits - b.maHits; c.deflects = a.deflects - b.deflects; c.counterAvoids = a.counterAvoids - b.counterAvoids;
    return c;
}

// The finished fight, as the card and the log show it.
typedef struct {
    int grade;                   // 0 = D .. 6 = SSS
    int byAvg;                   // floor(average) before rule 2 (grade < byAvg = rule 2 held it back)
    float avg;                   // 0 .. 7, after the hit penalty (what the grade uses)
    float rawAvg, penalty;       // the meter average before the penalty / the penalty subtracted
    float pctAt[SM_RANKS];       // % of the fight spent at each rank
    int peak;                    // highest rank reached during the fight
    double ms;                   // fight length, game ms (shown, never scored)
    int moves, maMoves;          // distinct move ids that landed a hit / distinct martial art moves that scored
    int bosses;                  // bosses killed in this fight
    int hold;                    // rule 2 share used (ResultHoldPct)
    SrCount n;                   // event counts during the fight
    uint32_t comboBest;          // longest combo during the fight (shown, not graded)
} SrResult;

typedef struct {
    int on;                      // a fight is running (first boss hit seen)
    int ending;                  // every engaged boss is down: waiting endDelayMs (wall) for a next form
    double endWall;              // wall ms the wait started (= when the fight went down)
    int drop;                    // why SrStep returned -1 (SR_DROP_*), for the log
    double ms;                   // fight game time so far
    double area;                 // sum of v x dt
    double atRank[SM_RANKS];     // game ms at each rank
    int peak;
    uint32_t comboBest;
    uint64_t boss[SR_BOSSES]; int dead[SR_BOSSES], hp0[SR_BOSSES]; int nBoss;  // hp0: HP was 0 when last seen (a later disappearance = killed)
    uint32_t acts[SR_ACTS]; int nActs;
    uint32_t maActs[SR_ACTS]; int nMa;
    SrCount c0;
} SrFight;

static inline void SrReset(SrFight* f) { uint8_t* p = (uint8_t*)f; for (unsigned i = 0; i < sizeof *f; ++i) p[i] = 0; }
// The meter as one number: rank + gauge (0..1). An idle meter is 0.
static inline float SrValue(const SmState* s, const SmCfg* c) { return s->active ? (float)s->rank + SmGauge(s, c) : 0.f; }
static inline int SrAddId(uint32_t* set, int* n, uint32_t id) {
    if (id == SM_ACT_NONE || id == 0) return 0;
    for (int i = 0; i < *n; ++i) if (set[i] == id) return 0;
    if (*n >= SR_ACTS) return 0;
    set[(*n)++] = id; return 1;
}
static inline int SrBossIdx(const SrFight* f, uint64_t a) { for (int i = 0; i < f->nBoss; ++i) if (f->boss[i] == a) return i; return -1; }
// A new boss hit during the end wait: its next form? seenWall = wall ms the boss was first seen (got its slot), see the header comment.
static inline int SrIsForm(const SrFight* f, double seenWall, int formMs) {
    return seenWall >= f->endWall - SR_FORM_SLACK_MS && seenWall <= f->endWall + (formMs > 0 ? formMs : 0);
}
// A hit of the player took HP off boss actor `a`. Starts the fight, or adds a boss to it: a second boss while one is up (duo), or during the
// end wait a next form (SrIsForm). Returns 1 (nothing changed) when `a` is the next boss of a gauntlet instead: finish this fight (SrFinish,
// card), SrReset, then call again to start the new fight.
static inline int SrBossHitEx(SrFight* f, const SmState* s, uint64_t a, double seenWall, int formMs) {
    if (!a) return 0;
    if (f->on && f->ending && SrBossIdx(f, a) < 0 && !SrIsForm(f, seenWall, formMs)) return 1;
    if (!f->on) { SrReset(f); f->on = 1; f->peak = s->active ? s->rank : 0; f->c0 = SrCountOf(s); f->comboBest = s->combo; }
    if (SrBossIdx(f, a) < 0 && f->nBoss < SR_BOSSES) { f->boss[f->nBoss] = a; f->dead[f->nBoss] = 0; f->hp0[f->nBoss] = 0; f->nBoss++; f->ending = 0; }
    return 0;
}
// Old entry (tests): every new boss in the end wait is a next form.
static inline void SrBossHit(SrFight* f, const SmState* s, uint64_t a) { SrBossHitEx(f, s, a, f->endWall, SR_FORM_MS); }
// One tick of fight time: dt game ms with the meter at state s (after this tick's events). Nothing counts while ending.
static inline void SrTick(SrFight* f, const SmState* s, const SmCfg* c, double dt) {
    if (!f->on || f->ending || !(dt > 0.0)) return;
    const float v = SrValue(s, c); const int r = s->active ? s->rank : 0;
    f->ms += dt; f->area += (double)v * dt; f->atRank[r] += dt;
    if (r > f->peak) f->peak = r;
    if (s->combo > f->comboBest) f->comboBest = s->combo;
}
static inline void SrMove(SrFight* f, uint32_t act) { if (f->on && !f->ending) SrAddId(f->acts, &f->nActs, act); }
static inline void SrMartialArt(SrFight* f, uint32_t act) { if (f->on && !f->ending) SrAddId(f->maActs, &f->nMa, act); }
// Grade from the time at each rank and the average (rules 1 and 2 above). ms <= 0: D.
static inline int SrGrade(const double* atRank, double ms, double area, int holdPct, double penalty, float* avg, int* byAvg) {
    *avg = 0.f; *byAvg = 0;
    if (!(ms > 0.0)) return 0;
    double a = area / ms - (penalty > 0 ? penalty : 0); if (a < 0) a = 0; if (a > SM_RANKS) a = SM_RANKS;
    *avg = (float)a;
    int g = (int)a; if (g > SM_RANKS - 1) g = SM_RANKS - 1;
    *byAvg = g;
    for (; g > 0; --g) {
        double up = 0; for (int r = g; r < SM_RANKS; ++r) up += atRank[r];
        if (up * 100.0 >= (double)holdPct * ms) break;
    }
    return g;
}
// hitPenaltyCenti: average lost per hit taken, in hundredths (0 = none).
static inline SrResult SrFinishEx(const SrFight* f, const SmState* s, int holdPct, int hitPenaltyCenti) {
    SrResult r; uint8_t* p = (uint8_t*)&r; for (unsigned i = 0; i < sizeof r; ++i) p[i] = 0;
    r.n = SrCountSub(SrCountOf(s), f->c0);
    r.rawAvg = f->ms > 0 ? (float)(f->area / f->ms) : 0.f;
    r.penalty = hitPenaltyCenti > 0 ? (float)r.n.damaged * (float)hitPenaltyCenti / 100.f : 0.f;
    r.grade = SrGrade(f->atRank, f->ms, f->area, holdPct, r.penalty, &r.avg, &r.byAvg); r.hold = holdPct;
    for (int i = 0; i < SM_RANKS; ++i) r.pctAt[i] = f->ms > 0 ? (float)(f->atRank[i] * 100.0 / f->ms) : (i == 0 ? 100.f : 0.f);
    r.peak = f->peak; r.ms = f->ms; r.moves = f->nActs; r.maMoves = f->nMa; r.comboBest = f->comboBest;
    for (int i = 0; i < f->nBoss; ++i) r.bosses += f->dead[i] || f->hp0[i];
    return r;
}
static inline SrResult SrFinish(const SrFight* f, const SmState* s, int holdPct) { return SrFinishEx(f, s, holdPct, 0); }
// Per tick boss check. For each engaged boss: gone = no longer in the actor table, dead = death applied, hp0 = HP is 0 now (for a gone boss:
// the last hit seen on it was lethal). loading = a load started / no player character (the mission ended or was left).
// Returns: 0 nothing, 1 the fight just entered the end wait, 2 finish now (call SrFinish, then SrReset), -1 drop the fight (f->drop says why).
enum { SR_DROP_NONE, SR_DROP_PLAYER, SR_DROP_LEFT, SR_DROP_LOAD };
static const char* const sr_drop_name[] = {"-", "the player died", "a boss left with HP left (reset / quit)", "a load started with a boss still up"};
typedef struct { int gone, dead, hp0; } SrBossNow;
static inline int SrStep(SrFight* f, const SrBossNow* now, int playerDead, int loading, double wall, int endDelayMs) {
    if (!f->on) return 0;
    int alive = 0, left = 0;
    for (int i = 0; i < f->nBoss; ++i) {
        if (now[i].dead) f->dead[i] = 1;
        if (now[i].gone) { if (now[i].hp0) f->hp0[i] = 1; if (f->hp0[i]) f->dead[i] = 1; }
        else f->hp0[i] = now[i].hp0 != 0;                 // in the table: the HP read is the truth (a floored / healed boss is up again)
        if (f->dead[i] || f->hp0[i]) continue;            // down
        if (now[i].gone) left = 1; else alive = 1;
    }
    if (left) { f->drop = SR_DROP_LEFT; return -1; }
    if (alive && playerDead) { f->drop = SR_DROP_PLAYER; return -1; }  // dying once every boss is down still gets the card
    if (alive && loading) { f->drop = SR_DROP_LOAD; return -1; }
    if (loading) return 2;                                // every boss down, then the load: the card now
    if (alive) { f->ending = 0; return 0; }
    if (!f->ending) { f->ending = 1; f->endWall = wall; return 1; }
    return wall - f->endWall >= (double)endDelayMs ? 2 : 0;
}
// One log / card line of the counts.
static inline int SrFmt(char* o, int n, const SrResult* r) {
    return snprintf(o, n, "grade %s avg %.2f (meter %.2f - %.2f for hits taken; by average %s) peak %s | %.1f s, %d boss%s | moves %d, martial arts %u (%d different), ninjutsu %u, "
        "soul cores %u, guardian spirit %u, weapon switches %u, style switches %u | hits %u, counters %u, dodges %u, deflects %u, Ki pulses %u (%u perfect), hits taken %u, best combo %u"
        " | D %.0f%% C %.0f%% B %.0f%% A %.0f%% S %.0f%% SS %.0f%% SSS %.0f%%",
        sm_rank_name[r->grade], r->avg, r->rawAvg, r->penalty, sm_rank_name[r->byAvg], sm_rank_name[r->peak], r->ms / 1000.0, r->bosses, r->bosses == 1 ? "" : "es",
        r->moves, r->n.maHits, r->maMoves, r->n.nins, r->n.souls, r->n.gsHits, r->n.swapHits, r->n.styles, r->n.hits, r->n.counters + r->n.counterAvoids, r->n.dodges, r->n.deflects,
        r->n.kis, r->n.kiPerfects, r->n.damaged, r->comboBest,
        r->pctAt[0], r->pctAt[1], r->pctAt[2], r->pctAt[3], r->pctAt[4], r->pctAt[5], r->pctAt[6]);
}
#endif
