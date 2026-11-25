// human 2026-11-25 (research prototype, not wired into the game glue yet): "Displacement!" = a move that carries the player out of a boss
// attack and into a counter in one motion (Switch Glaive Just Reprisal, Sword Swift Step Heavy, teleports behind the boss). Pure state
// machine (tests/style_displace_test.c), fed once per FRAMEPROBE tick by the game glue with:
//   run / use / act  the player's action run key, its use key (SmChainStep: lead-in + follow-up segments of one skill = one use), act id
//   elig             1 = this run may be a displacement move: a skill row (SmSkillRow, not the universal deflect) or an act in DisplaceActs
//   pl / plOk        player position, horizontal plane (actor+0x110 x and z, cm)
//   win              a boss hit window is open this tick (SmCapture's g_sm_enemy)
//   boss / bossOk    position (x, z, cm) of the boss whose window is open (nearest one if several; while no window: the nearest boss slot)
//   took             the player lost HP this tick
//   hits             the player's hits that took HP off a boss this tick (the follow-up)
//   avoid            SM_AV_* that SmDodgeStepEx returned this tick (Dodge! / Counter! / Deflect! of the same window)
// Rule. A use that is eligible and was in range of the boss (distance <= threatCm when its hit window opened) and saw the window close
// without damage (and no damage within graceMs before the use: knockdown / get-up i-frames are not an evade) becomes a candidate. Its
// first landed hit on a boss within followMs of the window close, in the same use or at most followRuns runs later (a cancel into a
// follow-up), decides it: the player moved >= minCm from where the use started AND went around the boss by >= angDeg (bearing of the
// player seen from the boss, before vs. at the hit; a straight lunge or a roll away does not turn it) -> SM_DP_AWARD. Anything else
// (damage, timeout, too many runs, too little motion) -> SM_DP_MISS. The window's own avoid (if any) is held while the candidate is open
// (o->ate): on an award it is replaced by Displacement (not paid), on a miss it is paid after all (o->release). A hit that lands before
// the window closes (fast counters) is remembered and decides the candidate at the close.
#ifndef STYLE_DISPLACE_H
#define STYLE_DISPLACE_H
#include <stdint.h>
#include <math.h>
#include "style_meter.h"
#define SM_VAR_DISPLACE 0xFFFFFF05u  // variety pseudo id (next to SM_VAR_KI)
#define SM_LOOP_DISPLACE 0xFFFFFF05u // loop sequence pseudo id (next to SM_LOOP_COUNTER)

#define SM_DP_PTS 65          // [StyleMeter] DisplacePts: replaces the window's Dodge! / Counter! (45): +20 for doing it with a displacement move
#define SM_DP_FOLLOW_MS 900   // DisplaceFollowMs: the counter hit must land this soon after the boss's hit window closed
#define SM_DP_FOLLOW_RUNS 1   // DisplaceFollowRuns: ... in the same use or this many runs after it (a cancel into another move)
#define SM_DP_MIN_CM 250      // DisplaceMinCm: motion from the use's start to the hit (a deflect / step is ~1-2 m)
#define SM_DP_ANG_DEG 60      // DisplaceAngleDeg: turn around the boss (bearing change) from the use's start to the hit
#define SM_DP_THREAT_CM 500   // DisplaceThreatCm: the player was this close to the attacking boss when its hit window opened
#define SM_DP_GRACE_MS 1500   // no damage this long before the use (knockdown i-frames)
#define SM_DP_ACTS 16
enum { SM_DP_NONE = 0, SM_DP_AWARD = 1, SM_DP_MISS = 2 };
enum { SM_DPM_DAMAGE = 1, SM_DPM_LATE = 2, SM_DPM_RUNS = 3, SM_DPM_MOTION = 4, SM_DPM_ANGLE = 5 };

typedef struct { int on; float pts; int followMs, followRuns, minCm, angDeg, threatCm, graceMs; uint32_t acts[SM_DP_ACTS]; int nActs; } SmDispCfg;
typedef struct { uint64_t run, use; uint32_t act; int elig; int plOk; float pl[2]; int win; int bossOk; float boss[2]; int took, hits, avoid; } SmDispIn;
typedef struct { uint32_t act; int why, runs, release, ate, replaced; double ago; float moved, ang, dist0; } SmDispOut;
typedef struct {
    uint64_t run, use; uint32_t act; int elig, runOn; double useT0, lastDmg;
    int p0ok; float p0[2], b0[2];            // player / boss at the use start (b0 valid with p0ok only when the boss was known)
    int winSeen, winOk, dmg; float dist0;     // this use saw a boss window open / it was in range then / damage since the use started
    int pre; float preP[2], preB[2];          // a hit landed while the window was still open (geometry at that hit)
    int pend, runsAfter, held; double tClose; uint32_t pAct; uint64_t pUse; float pP0[2], pB0[2], pDist0;
} SmDisp;

static inline void SmDispCfgDefaults(SmDispCfg* c) {
    c->on = 1; c->pts = (float)SM_DP_PTS; c->followMs = SM_DP_FOLLOW_MS; c->followRuns = SM_DP_FOLLOW_RUNS; c->minCm = SM_DP_MIN_CM;
    c->angDeg = SM_DP_ANG_DEG; c->threatCm = SM_DP_THREAT_CM; c->graceMs = SM_DP_GRACE_MS; c->nActs = 0;
}
static inline void SmDispCfgClamp(SmDispCfg* c) {
    if (c->pts < 0) c->pts = 0; if (c->followMs < 100) c->followMs = 100; if (c->followMs > 3000) c->followMs = 3000;
    if (c->followRuns < 0) c->followRuns = 0; if (c->followRuns > 4) c->followRuns = 4; if (c->minCm < 0) c->minCm = 0;
    if (c->angDeg < 0) c->angDeg = 0; if (c->angDeg > 180) c->angDeg = 180; if (c->threatCm < 100) c->threatCm = 100;
    if (c->graceMs < 0) c->graceMs = 0; if (c->nActs < 0) c->nActs = 0; if (c->nActs > SM_DP_ACTS) c->nActs = SM_DP_ACTS;
}
static inline void SmDispReset(SmDisp* d) { uint8_t* p = (uint8_t*)d; for (unsigned i = 0; i < sizeof *d; ++i) p[i] = 0; d->lastDmg = -1e18; }
// a DisplaceActs entry (curated ids for displacement moves that are not skill rows)
static inline int SmDispListed(const SmDispCfg* c, uint32_t act) { for (int i = 0; i < c->nActs; ++i) if (c->acts[i] == act) return 1; return 0; }
static inline float SmDispDist(const float* a, const float* b) { const float dx = a[0] - b[0], dz = a[1] - b[1]; return sqrtf(dx * dx + dz * dz); }
// |bearing change| of the player around the boss, degrees 0..180: player p0 seen from boss b0, then p1 seen from b1
static inline float SmDispTurn(const float* p0, const float* b0, const float* p1, const float* b1) {
    const float a0 = atan2f(p0[0] - b0[0], p0[1] - b0[1]), a1 = atan2f(p1[0] - b1[0], p1[1] - b1[1]);
    float d = (a1 - a0) * 57.2957795f;
    while (d > 180.f) d -= 360.f; while (d < -180.f) d += 360.f;
    return d < 0 ? -d : d;
}
static inline int SmDispEnd(SmDisp* d, SmDispOut* o, int why, double t) {
    o->act = d->pAct; o->why = why; o->runs = d->runsAfter; o->ago = t - d->tClose; o->dist0 = d->pDist0;
    o->release = d->held; d->held = 0; d->pend = 0;
    return SM_DP_MISS;
}
static inline int SmDispJudge(SmDisp* d, const SmDispCfg* c, SmDispOut* o, double t, const float* p, const float* b) {
    o->moved = SmDispDist(d->pP0, p); o->ang = SmDispTurn(d->pP0, d->pB0, p, b);
    if (o->moved < (float)c->minCm) return SmDispEnd(d, o, SM_DPM_MOTION, t);
    if (o->ang < (float)c->angDeg) return SmDispEnd(d, o, SM_DPM_ANGLE, t);
    o->act = d->pAct; o->why = 0; o->runs = d->runsAfter; o->ago = t - d->tClose; o->dist0 = d->pDist0;
    o->release = 0; o->replaced = d->held; d->held = 0; d->pend = 0;  // the held avoid becomes the Displacement
    return SM_DP_AWARD;
}
static inline int SmDispStep(SmDisp* d, const SmDispCfg* c, double t, const SmDispIn* in, SmDispOut* o) {
    o->ate = o->release = o->replaced = 0; o->why = 0; o->moved = o->ang = 0.f;
    if (!c->on) { SmDispReset(d); return SM_DP_NONE; }
    int r = SM_DP_NONE;
    if (in->took) {  // damage ends everything: a candidate pays its held avoid (it was a real avoid of the earlier window)
        d->lastDmg = t; d->dmg = 1;
        if (d->pend) return SmDispEnd(d, o, SM_DPM_DAMAGE, t);
    }
    if (!d->runOn || in->run != d->run) {
        if (d->pend && in->use != d->pUse && ++d->runsAfter > c->followRuns) r = SmDispEnd(d, o, SM_DPM_RUNS, t);
        const int same = d->runOn && in->use == d->use;  // a later segment of the same use keeps the use's start
        d->runOn = 1; d->run = in->run; d->act = in->act;
        if (!same) {
            d->use = in->use; d->useT0 = t; d->elig = 0; d->winSeen = d->winOk = d->pre = 0; d->dmg = in->took;
            d->p0ok = in->plOk && in->bossOk; if (d->p0ok) { d->p0[0] = in->pl[0]; d->p0[1] = in->pl[1]; d->b0[0] = in->boss[0]; d->b0[1] = in->boss[1]; }
        }
    }
    if (in->elig && d->useT0 - d->lastDmg > (double)c->graceMs) d->elig = 1;  // any tick of the use (the row can be unreadable on its first tick)
    if (r == SM_DP_MISS) return r;
    if (d->pend && t - d->tClose > (double)c->followMs) return SmDispEnd(d, o, SM_DPM_LATE, t);
    if (in->win) {
        if (!d->winSeen) { d->winSeen = 1; d->winOk = d->p0ok && in->plOk && in->bossOk && SmDispDist(in->pl, in->boss) <= (float)c->threatCm;
                           d->dist0 = in->plOk && in->bossOk ? SmDispDist(in->pl, in->boss) : -1.f; }
        if (in->hits > 0 && d->winOk && !d->pre && in->plOk && in->bossOk) {  // a fast counter: the hit came before the window closed
            d->pre = 1; d->preP[0] = in->pl[0]; d->preP[1] = in->pl[1]; d->preB[0] = in->boss[0]; d->preB[1] = in->boss[1];
        }
    } else if (d->winSeen) {  // the window closed this tick: a candidate when clean
        d->winSeen = 0;
        if (!d->pend && d->elig && d->winOk && !d->dmg) {
            d->pend = 1; d->runsAfter = 0; d->tClose = t; d->pAct = d->act; d->pUse = d->use; d->pDist0 = d->dist0; d->held = 0;
            d->pP0[0] = d->p0[0]; d->pP0[1] = d->p0[1]; d->pB0[0] = d->b0[0]; d->pB0[1] = d->b0[1];
            if (in->avoid) { d->held = in->avoid; o->ate = 1; }
            if (d->pre) { d->pre = 0; return SmDispJudge(d, c, o, t, d->preP, d->preB); }
        }
        d->pre = 0;
    }
    if (d->pend && in->avoid && !o->ate && !d->held) { d->held = in->avoid; o->ate = 1; }  // the avoid closed a tick later
    if (d->pend && in->hits > 0 && t > d->tClose) {
        if (!in->plOk || !in->bossOk) return SmDispEnd(d, o, SM_DPM_MOTION, t);
        return SmDispJudge(d, c, o, t, in->pl, in->boss);
    }
    return r;
}
// Pay an award: a step of the displacement move's own use in the loop sequence (like the avoid counter: Just Reprisal x3 in a row is
// looped, scores 0 and costs loopPts), then the points; a scored one is a distinct move for the variety count.
static inline float SmOnDisplace(SmState* s, const SmCfg* c, const SmDispCfg* dc, double t, uint32_t act, uint64_t use) {
    s->lastEvent = t;
    // same use as the counter hit: that hit already stepped the loop (SmLoopUse returns its verdict). A follow-up of another use stepped
    // the loop with its own key; SmLoopUse here would start a new use and reset its multi-hit count, so the displacement is one pseudo step.
    if (s->lpOn && s->lpRun == use ? s->lpPen : SmLoopEvent(s, c, SM_LOOP_DISPLACE)) return 0.f;
    (void)act;
    if (dc->pts <= 0.f) return 0.f;
    SmAdd(s, c, dc->pts); s->lastScore = t;
    SmVarietyMove(s, c, t, SM_VAR_DISPLACE);
    return dc->pts;
}
#endif
