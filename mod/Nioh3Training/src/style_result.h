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
// HUD-only bosses (Picture Scroll). A Hundred Demon Realms Picture Scroll boss is a wave enemy: it has the boss HP bar but no native boss
// slot (its wrapper has no placement record, NATIVE SLOT "wrapper record did not match"), so the glue also takes actors on the HUD boss bar
// (SrHud below). Picture Scroll waves reuse the actor: the next wave's boss can sit at the dead boss's address with a new handle / HP block.
// Such a boss carries an identity tag (0 for boss slots: no tag logic) and SrBossHitTag treats a hit on a known address with another tag as
// a new boss: the next fight (one card per boss) or, inside the form window of the end wait, its next form.
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
// Since 2026-11-26 the card grades by the grade score instead (SrGradeScore below); the average above is only logged.
// The grade is the highest R that passes both. The card says which rule held it back when rule 2 lowered it.
#ifndef STYLE_RESULT_H
#define STYLE_RESULT_H
#include <stdint.h>
#include "style_meter.h"

#define SR_BOSSES 8
#define SR_ACTS 192             // distinct move ids remembered per fight (more just stop counting)
#define SR_HOLD_PCT 50          // [StyleMeter] ResultHoldPct (human 2026-11-26: 50 with rule 3; 2026-11-25 was 45)
#define SR_HOLD_SSS_PCT 50      // [StyleMeter] ResultHoldPctSSS: % of the fight at SSS an SSS grade needs (other grades ResultHoldPct). human 2026-11-27: back to 50 (60 was too hard)
#define SR_END_DELAY_MS 3000    // [StyleMeter] ResultEndDelayMs: wait after the last boss death for a next form
#define SR_FORM_MS 1500         // [StyleMeter] ResultFormMs: a boss first seen this long after the fight went down is still its next form
#define SR_FORM_SLACK_MS 500    // ... or this long before it (the next form's bar can show a frame before the old HP reads 0)
#define SR_HIT_PENALTY 12.5f     // [StyleMeter] ResultHitPenalty: average lost per hit taken, in hundredths (12.5 = 0.125; human 2026-11-26: half of 25)
#define SR_SCALE_PCT 140        // [StyleMeter] ResultScalePct: card size (100 = 640 px wide at 1080p)
#define SR_SHOW_MS 0            // [StyleMeter] ResultShowMs: 0 = the card stays up until the player presses confirm; > 0 = also hides after that many wall ms

// Counters taken from SmState at the fight start and at the end (the card shows the difference).
typedef struct {
    uint32_t hits, dodges, kis, kiPerfects, damaged, counters, nins, souls, styles, swapHits, gsHits, maHits, deflects, counterAvoids, bursts;
    uint32_t displaces, crouchCancels, flux1, flux2, blockCounters;
    float earned[SM_PC_N], lost[SM_PL_N];  // human 2026-11-26: points by source / losses (card page 2)
} SrCount;
static inline SrCount SrCountOf(const SmState* s) {
    SrCount c; c.hits = s->hits; c.dodges = s->dodges; c.kis = s->kis; c.kiPerfects = s->kiPerfects; c.damaged = s->damaged;
    c.counters = s->counters; c.nins = s->nins; c.souls = s->souls; c.styles = s->styles; c.swapHits = s->swapHits; c.gsHits = s->gsHits;
    c.maHits = s->maHits; c.deflects = s->deflects; c.counterAvoids = s->counterAvoids; c.bursts = s->bursts; c.displaces = s->displaces; c.crouchCancels = s->crouchCancels; c.flux1 = s->flux1; c.flux2 = s->flux2; c.blockCounters = s->blockCounters;
    for (int i = 0; i < SM_PC_N; ++i) c.earned[i] = s->earned[i]; for (int i = 0; i < SM_PL_N; ++i) c.lost[i] = s->lost[i]; return c;
}
static inline SrCount SrCountSub(SrCount a, SrCount b) {
    SrCount c; c.hits = a.hits - b.hits; c.dodges = a.dodges - b.dodges; c.kis = a.kis - b.kis; c.kiPerfects = a.kiPerfects - b.kiPerfects;
    c.damaged = a.damaged - b.damaged; c.counters = a.counters - b.counters; c.nins = a.nins - b.nins; c.souls = a.souls - b.souls;
    c.styles = a.styles - b.styles; c.swapHits = a.swapHits - b.swapHits; c.gsHits = a.gsHits - b.gsHits; c.maHits = a.maHits - b.maHits; c.deflects = a.deflects - b.deflects; c.counterAvoids = a.counterAvoids - b.counterAvoids; c.bursts = a.bursts - b.bursts;
    c.displaces = a.displaces - b.displaces; c.crouchCancels = a.crouchCancels - b.crouchCancels; c.flux1 = a.flux1 - b.flux1; c.flux2 = a.flux2 - b.flux2; c.blockCounters = a.blockCounters - b.blockCounters;
    for (int i = 0; i < SM_PC_N; ++i) c.earned[i] = a.earned[i] - b.earned[i]; for (int i = 0; i < SM_PL_N; ++i) c.lost[i] = a.lost[i] - b.lost[i];
    return c;
}

// The finished fight, as the card and the log show it.
typedef struct {
    int grade;                   // 0 = D .. 6 = SSS
    int byAvg;                   // floor(average) before rule 2 (grade < byAvg = rule 2 held it back)
    int byHold;                  // highest rank held holdPct % of the fight (before the hit penalty)
    float score, rawScore;       // human 2026-11-26 grade score 0 .. 7 (after / before the hit penalty): grade = floor(score)
    float nextPct;               // % of the fight at the next rank or higher (SSS: at SSS)
    float earnedTotal, lostTotal; // human 2026-11-26: all points earned / lost during the fight (card page 2; by source in n.earned / n.lost)
    float avg;                   // 0 .. 7, after the hit penalty (what the grade uses)
    float rawAvg, penalty;       // the meter average before the penalty / the penalty subtracted
    float pctAt[SM_RANKS];       // % of the fight spent at each rank
    int peak;                    // highest rank reached during the fight
    double ms;                   // fight length, game ms (shown, never scored)
    int moves, maMoves;          // distinct move ids that landed a hit / distinct martial art moves that scored
    int bosses;                  // bosses killed in this fight
    int hold;                    // rule 2 share used (ResultHoldPct)
    int holdTop;                 // ... for the SSS grade (ResultHoldPctSSS)
    SrCount n;                   // event counts during the fight
    uint32_t comboBest;          // longest combo during the fight (shown, not graded)
    uint32_t inputs;             // human 2026-11-25: button presses during the fight (pad buttons / triggers, keys, mouse buttons)
    float apm;                   // ... per minute of fight time (shown, not graded)
    uint32_t chr[SR_BOSSES]; int nChr;  // human 2026-11-26: the fight's boss character ids (0 = not known: HUD-only boss), for its name and best scores
    char name[96];               // ... the bosses' names (glue: Nioh3Training_BossName), empty = not known: card title '<name>: Results'
    int place;                   // ... 1..3 = this score's place in that boss's best scores (style_scores.h), 0 = not in them / not kept
    char note[64];               // ... a saved card viewed again from the mod menu: '#2 best, build 1.8.48, 2026-11-26 19:40' (replaces the place text)
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
    uint64_t tag[SR_BOSSES];     // HUD-only boss identity (handle / HP block), 0 = a boss slot (see SrBossHitTag)
    uint32_t chr[SR_BOSSES];     // human 2026-11-26: character id of each boss (SrBossChr; 0 = not known)
    uint32_t acts[SR_ACTS]; int nActs;
    uint32_t maActs[SR_ACTS]; int nMa;
    SrCount c0;
    uint32_t inputs;             // button presses counted while the fight runs (SrInputs)
} SrFight;

// Button presses (human 2026-11-25: APM on the card). One sample per tick: pad buttons (wButtons, 16 bits), the two triggers as buttons
// (pressed above SR_TRIG_ON, released below SR_TRIG_OFF), and a 256-bit set of held keys / mouse buttons. A press = a not-held -> held edge.
// The first sample only sets the state (a button held when the tracker starts is not a press).
#define SR_TRIG_ON 64
#define SR_TRIG_OFF 32
typedef struct { int init; uint16_t btn; uint8_t lt, rt; uint32_t keys[8]; } SrInput;
static inline int SrBitCount(uint32_t v) { int n = 0; while (v) { v &= v - 1; ++n; } return n; }
static inline uint32_t SrInputStep(SrInput* in, uint16_t btn, uint8_t lt, uint8_t rt, const uint32_t keys[8]) {
    const int lOn = in->lt ? lt >= SR_TRIG_OFF : lt > SR_TRIG_ON, rOn = in->rt ? rt >= SR_TRIG_OFF : rt > SR_TRIG_ON;
    uint32_t n = 0;
    if (in->init) {
        n += (uint32_t)SrBitCount((uint32_t)(btn & (uint16_t)~in->btn));
        n += (uint32_t)(lOn && !in->lt) + (uint32_t)(rOn && !in->rt);
        for (int i = 0; i < 8; ++i) n += (uint32_t)SrBitCount(keys[i] & ~in->keys[i]);
    }
    in->init = 1; in->btn = btn; in->lt = (uint8_t)lOn; in->rt = (uint8_t)rOn;
    for (int i = 0; i < 8; ++i) in->keys[i] = keys[i];
    return n;
}
// Presses that count toward the fight: only while it runs (not in the end wait) and only on ticks with game time (no pause menus).
static inline void SrInputs(SrFight* f, uint32_t presses, double dt) { if (f->on && !f->ending && dt > 0.0) f->inputs += presses; }

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
// SrBossHitEx for a boss with an identity tag (HUD-only boss; tag 0 = plain SrBossHitEx). The same actor address with another tag is another
// boss: a down entry is the next fight (returns 1, as SrBossHitEx) unless SrIsForm, then that entry is the new form (up again, same fight).
static inline int SrBossHitTag(SrFight* f, const SmState* s, uint64_t a, uint64_t tag, double seenWall, int formMs) {
    int i = a && f->on ? SrBossIdx(f, a) : -1;
    if (i >= 0 && tag && f->tag[i] && f->tag[i] != tag) {
        if (f->dead[i] || f->hp0[i]) {
            if (f->ending && !SrIsForm(f, seenWall, formMs)) return 1;
            f->dead[i] = f->hp0[i] = 0; f->ending = 0;
        }
        f->tag[i] = tag; return 0;
    }
    const int r = SrBossHitEx(f, s, a, seenWall, formMs);
    if (r == 0 && tag && (i = SrBossIdx(f, a)) >= 0 && !f->tag[i]) f->tag[i] = tag;
    return r;
}
// human 2026-11-26: the character id of boss `a` of the fight (first one known wins; 0 = not known).
static inline void SrBossChr(SrFight* f, uint64_t a, uint32_t chr) { const int i = a && chr ? SrBossIdx(f, a) : -1; if (i >= 0 && !f->chr[i]) f->chr[i] = chr; }
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
// Grade score (human 2026-11-26, replaces grading by the average): h = the highest rank held (at that rank or higher) for at least holdPct %
// of the fight. The whole number is h; the decimal is the progress to the next grade = the share of the fight at rank h + 1 or higher out
// of the holdPct % it needs (SSS: how far past holdPct % the SSS share went, out of the rest: 100 % SSS = 7.00). Then the hit penalty comes
// off (0.125 per hit: 8 hits = one grade). grade = floor(score). holdPct 0: the old average (no hold rule).
static inline float SrShareUp(const double* atRank, double ms, int r) { double up = 0; for (int i = r; i < SM_RANKS; ++i) up += atRank[i]; return (float)(up * 100.0 / ms); }
// topPct (1.8.48): the share the SSS grade needs (ResultHoldPctSSS); the decimal toward SSS is out of it too.
static inline float SrGradeScoreEx(const double* atRank, double ms, int holdPct, int topPct, int* held, float* nextPct) {
    *held = 0; *nextPct = 0.f;
    if (!(ms > 0.0)) return 0.f;
    if (topPct <= 0) topPct = holdPct;
    int h = SM_RANKS - 1;
    for (; h > 0; --h) if (SrShareUp(atRank, ms, h) >= (float)(h == SM_RANKS - 1 ? topPct : holdPct)) break;
    *held = h;
    float d;
    if (h < SM_RANKS - 1) { *nextPct = SrShareUp(atRank, ms, h + 1); d = *nextPct / (float)(h + 1 == SM_RANKS - 1 ? topPct : holdPct); }
    else { *nextPct = SrShareUp(atRank, ms, h); d = topPct >= 100 ? 1.f : (*nextPct - (float)topPct) / (100.f - (float)topPct); }
    d = d < 0.f ? 0.f : d > 0.999f ? 0.999f : d;
    if (h == SM_RANKS - 1 && *nextPct >= 99.95f) d = 1.f;
    return (float)h + d;
}
static inline float SrGradeScore(const double* atRank, double ms, int holdPct, int* held, float* nextPct) { return SrGradeScoreEx(atRank, ms, holdPct, holdPct, held, nextPct); }
// hitPenaltyCenti: average lost per hit taken, in hundredths (0 = none).
static inline SrResult SrFinishTop(const SrFight* f, const SmState* s, int holdPct, int topPct, float hitPenaltyCenti) {
    SrResult r; uint8_t* p = (uint8_t*)&r; for (unsigned i = 0; i < sizeof r; ++i) p[i] = 0;
    r.n = SrCountSub(SrCountOf(s), f->c0);
    r.rawAvg = f->ms > 0 ? (float)(f->area / f->ms) : 0.f;
    r.penalty = hitPenaltyCenti > 0 ? (float)r.n.damaged * hitPenaltyCenti / 100.f : 0.f;
    r.grade = SrGrade(f->atRank, f->ms, f->area, holdPct, r.penalty, &r.avg, &r.byAvg); r.hold = holdPct; r.holdTop = topPct > 0 ? topPct : holdPct;
    if (holdPct > 0) {
        r.rawScore = SrGradeScoreEx(f->atRank, f->ms, holdPct, r.holdTop, &r.byHold, &r.nextPct);
        r.score = r.rawScore - r.penalty; if (r.score < 0.f) r.score = 0.f;
        r.grade = (int)r.score; if (r.grade > SM_RANKS - 1) r.grade = SM_RANKS - 1;
    } else { r.rawScore = r.rawAvg; r.score = r.avg; r.byHold = r.grade; }  // ResultHoldPct 0: the average alone
    for (int i = 0; i < SM_RANKS; ++i) r.pctAt[i] = f->ms > 0 ? (float)(f->atRank[i] * 100.0 / f->ms) : (i == 0 ? 100.f : 0.f);
    r.peak = f->peak; r.ms = f->ms; r.moves = f->nActs; r.maMoves = f->nMa; r.comboBest = f->comboBest;
    for (int i = 0; i < SM_PC_N; ++i) r.earnedTotal += r.n.earned[i] > 0.f ? r.n.earned[i] : 0.f;
    for (int i = 0; i < SM_PL_N; ++i) r.lostTotal += r.n.lost[i] > 0.f ? r.n.lost[i] : 0.f;
    r.inputs = f->inputs; r.apm = f->ms > 0 ? (float)(f->inputs * 60000.0 / f->ms) : 0.f;
    for (int i = 0; i < f->nBoss; ++i) r.bosses += f->dead[i] || f->hp0[i];
    for (int i = 0; i < f->nBoss; ++i) r.chr[i] = f->chr[i]; r.nChr = f->nBoss;
    return r;
}
static inline SrResult SrFinishEx(const SrFight* f, const SmState* s, int holdPct, float hitPenaltyCenti) { return SrFinishTop(f, s, holdPct, holdPct, hitPenaltyCenti); }
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
// HUD boss tracker (glue: style_meter_game.h, once per tick). Actors on the HUD boss HP bar that are not a boss slot, with their identity tag
// and the wall ms that identity was first seen (SrBossHitTag's seenWall). An entry stays while it is on the bar and a while after it left
// (keepMs: the lethal hit / death blow can land as the bar goes away); a new tag at a known address is a new boss (first seen now).
#define SR_HUD 4
#define SR_HUD_KEEP_MS 5000
typedef struct { uint64_t a, tag; double seen, last; } SrHudE;
typedef struct { SrHudE e[SR_HUD]; } SrHud;
static inline int SrHudFind(const SrHud* h, uint64_t a) { for (int i = 0; a && i < SR_HUD; ++i) if (h->e[i].a == a) return i; return -1; }
// Seen on the bar now. Returns the entry index (a full table drops the entry seen longest ago); *fresh = 1 when this is a new identity.
static inline int SrHudSee(SrHud* h, uint64_t a, uint64_t tag, double wall, int* fresh) {
    if (fresh) *fresh = 0;
    if (!a) return -1;
    int i = SrHudFind(h, a);
    if (i >= 0 && h->e[i].tag == tag) { h->e[i].last = wall; return i; }
    if (i < 0) { for (int k = 0; k < SR_HUD && i < 0; ++k) if (!h->e[k].a) i = k; }
    if (i < 0) { i = 0; for (int k = 1; k < SR_HUD; ++k) if (h->e[k].last < h->e[i].last) i = k; }
    h->e[i].a = a; h->e[i].tag = tag; h->e[i].seen = wall; h->e[i].last = wall; if (fresh) *fresh = 1;
    return i;
}
static inline void SrHudDrop(SrHud* h, int i) { if (i >= 0 && i < SR_HUD) { h->e[i].a = h->e[i].tag = 0; h->e[i].seen = h->e[i].last = 0; } }
// Off the bar for more than keepMs: forgotten (the address may be reused by a normal enemy later).
static inline void SrHudExpire(SrHud* h, double wall, int keepMs) { for (int i = 0; i < SR_HUD; ++i) if (h->e[i].a && wall - h->e[i].last > (double)keepMs) SrHudDrop(h, i); }
// human 2026-11-26: the points breakdown as one log line (sources with points, then losses).
static inline int SrFmtPts(char* o, int n, const SrResult* r) {
    int k = snprintf(o, n, "final %.0f = earned %.0f - lost %.0f | earned:", r->earnedTotal - r->lostTotal, r->earnedTotal, r->lostTotal);
    for (int i = 0; i < SM_PC_N && k < n; ++i) if (r->n.earned[i] >= 0.5f) k += snprintf(o + k, n - k, " %s %.0f,", sm_pc_name[i], r->n.earned[i]);
    if (k < n) k += snprintf(o + k, n - k, " | lost %.0f:", r->lostTotal);
    for (int i = 0; i < SM_PL_N && k < n; ++i) if (k < n) k += snprintf(o + k, n - k, " %s %.0f,", sm_pl_name[i], r->n.lost[i] > 0.f ? r->n.lost[i] : 0.f);
    return k;
}
// One log / card line of the counts.
static inline int SrFmt(char* o, int n, const SrResult* r) {
    return snprintf(o, n, "grade %s score %.2f (%.2f - %.2f for hits taken; held %s, next rank %.0f %%; average %.2f / raw %.2f, by average %s) peak %s | %.1f s, %d boss%s | moves %d, martial arts %u (%d different), ninjutsu %u, "
        "soul cores %u, guardian spirit %u, weapon switches %u, style switches %u | hits %u, counters %u, block counters %u, burst breaks %u, dodges %u, deflects %u, displacements %u, crouch cancels %u, flux 1 %u, flux 2 %u, Ki pulses %u (%u perfect), hits taken %u, best combo %u, inputs %u (APM %.0f)"
        " | D %.0f%% C %.0f%% B %.0f%% A %.0f%% S %.0f%% SS %.0f%% SSS %.0f%%",
        sm_rank_name[r->grade], r->score, r->rawScore, r->penalty, sm_rank_name[r->byHold], r->nextPct, r->avg, r->rawAvg, sm_rank_name[r->byAvg], sm_rank_name[r->peak], r->ms / 1000.0, r->bosses, r->bosses == 1 ? "" : "es",
        r->moves, r->n.maHits, r->maMoves, r->n.nins, r->n.souls, r->n.gsHits, r->n.swapHits, r->n.styles, r->n.hits, r->n.counters + r->n.counterAvoids, r->n.blockCounters, r->n.bursts, r->n.dodges, r->n.deflects, r->n.displaces, r->n.crouchCancels, r->n.flux1, r->n.flux2,
        r->n.kis, r->n.kiPerfects, r->n.damaged, r->comboBest, r->inputs, r->apm,
        r->pctAt[0], r->pctAt[1], r->pctAt[2], r->pctAt[3], r->pctAt[4], r->pctAt[5], r->pctAt[6]);
}
#endif
