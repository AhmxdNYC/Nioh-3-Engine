// human 2026-11-25 (prototype, live-tuned 2026-11-25 round 2): "Displacement!" = a move that carries the player out of a boss
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
//   move             this run is a move: it opened an attack window, or is a skill row / guardian spirit / ninjutsu (not a transition run)
// Rule. A span = a use that is a move plus the non-move runs after it for up to tailMs (the transition runs of a cancel: live, Swift Step
// Heavy cancelled = 25563 > 10493 > 2096 > 1775 > 2066 > 2069 > 2070 > Y attack, 1-8 frames each). An eligible span that was in range of
// the boss (distance <= threatCm when its hit window opened) and saw the window close without damage (and no damage within graceMs before
// the use: knockdown / get-up i-frames are not an evade) becomes a candidate; so does one whose window is still open when the next move
// starts (then that move is its first follow-up). Its first landed hit on a boss within followMs of the window close, in the same use or
// at most followRuns MOVES later (transition runs do not count), decides it: the player got >= minCm from where the use started (farthest
// point until the hit: a step through the boss ends close to where it started) AND the bearing of the player seen from the boss turned by
// >= angDeg from the use's start to the hit (a straight lunge or a roll away does not turn it) -> SM_DP_AWARD. A DisplaceActs move (known
// to reposition) uses listMinCm / listAngDeg instead. Anything else (damage, timeout, too many moves, too little motion) -> SM_DP_MISS.
// The window's own avoid (if any) is held while the candidate is open (o->ate): on an award it is replaced by Displacement (not paid), on
// a miss it is always released (o->release) - as a Dodge! when it was a Counter! of a move that displaced (Counter! is for parry skills).
// A hit that lands before the window closes (fast counters) is remembered and decides the candidate at the close.
#ifndef STYLE_DISPLACE_H
#define STYLE_DISPLACE_H
#include <stdint.h>
#include <math.h>
#include "style_meter.h"
#define SM_VAR_DISPLACE 0xFFFFFF05u  // variety pseudo id (next to SM_VAR_KI)
#define SM_LOOP_DISPLACE 0xFFFFFF05u // loop sequence pseudo id (next to SM_LOOP_COUNTER)

#define SM_DP_PTS 65          // [StyleMeter] DisplacePts: replaces the window's Dodge! / Counter! (45): +20 for doing it with a displacement move
#define SM_DP_FOLLOW_MS 1500  // DisplaceFollowMs: the counter hit must land this soon after the boss's hit window closed (live 2026-11-25:
                              // Swift Step Heavy's slash lands ~850 ms into it, 0.93 / 0.98 s after a window that closed early in the step;
                              // a cancel's Y attack 0.9 .. 1.25 s after; 900 ms missed 2 of 7 clean uses)
#define SM_DP_FOLLOW_RUNS 1   // DisplaceFollowRuns: ... in the same use or this many MOVES after it (a cancel into another move). Only a run
                              // that is a move counts (attack window / skill row / hit): the cancel's transition runs (10493 2096 1775 2066
                              // 2069 2070, 1-8 frames each, no attack window) are not moves
#define SM_DP_MIN_CM 200      // DisplaceMinCm: farthest from the use's start until the hit (a deflect / step is ~1-2 m; was 250: Swift Step
                              // that went behind the boss from 1.4-1.7 m away measured 2.04 / 2.25 m)
#define SM_DP_ANG_DEG 60      // DisplaceAngleDeg: turn around the boss (bearing change) from the use's start to the hit
#define SM_DP_LIST_MIN_CM 100 // DisplaceListMinCm / DisplaceListAngleDeg: the same test for a DisplaceActs move (known to reposition: a short
#define SM_DP_LIST_ANG_DEG 0  //   or point-blank step through the boss still counts; the bearing is noise that close to the boss's centre)
#define SM_DP_THREAT_CM 500   // DisplaceThreatCm: the player was this close to the attacking boss when its hit window opened
#define SM_DP_GRACE_MS 1500   // no damage this long before the use (knockdown i-frames)
#define SM_DP_TAIL_MS 800     // DisplaceTailMs: non-move runs this long after the displacement use still belong to it (a boss window that
                              // opens or closes during a cancel's transition runs is the displacement's)
#define SM_DP_ACTS 16
// DisplaceActs defaults (human 2026-11-25 live test, Sword vs Demon of Pride): Swift Step Heavy = 25563 (0x63DB, the step, 117-250 ms,
// 1.2-4.6 m, skill word 0xCCF6) -> 25564 (0x63DC, the slash, its hits ~850 ms in). Both are one use (same skill word, SmChainStep).
#define SM_DP_DEFAULT_ACTS "25563,25564"
enum { SM_DP_NONE = 0, SM_DP_AWARD = 1, SM_DP_MISS = 2 };
enum { SM_DPM_DAMAGE = 1, SM_DPM_LATE = 2, SM_DPM_RUNS = 3, SM_DPM_MOTION = 4, SM_DPM_ANGLE = 5, SM_DPM_OFF = 6 };

typedef struct { int on; float pts; int followMs, followRuns, minCm, angDeg, listMinCm, listAngDeg, threatCm, graceMs, tailMs; uint32_t acts[SM_DP_ACTS]; int nActs; } SmDispCfg;
// move: this tick's run is a move (an attack window opened / a skill row / a guardian spirit / ninjutsu), not a transition run
typedef struct { uint64_t run, use; uint32_t act; int elig, move; int plOk; float pl[2]; int win; int bossOk; float boss[2]; int took, hits, avoid; } SmDispIn;
// conv: this tick's avoid (not held) is a Counter! of a DisplaceActs move: the glue pays it as a Dodge! (Counter! is for parry skills)
// asDodge: the released held Counter! was turned into a Dodge! (a displacement move that only missed the follow-up)
typedef struct { uint32_t act; int why, runs, release, ate, replaced, conv, asDodge, listed; double ago; float moved, ang, dist0; } SmDispOut;
typedef struct {
    uint64_t run, use; uint32_t act; int runOn; double lastDmg;
    double uT0; int uOn, uP0ok; float uP0[2], uB0[2]; int uMove;      // the use in progress: start, and whether it showed itself a move
    uint64_t sUse; uint32_t sAct; int sLive, sElig, sListed; double sT0, sLast;  // the span: a move and the transition runs after it
    int p0ok; float p0[2], b0[2], sMax;      // player / boss at the span start, farthest the player got from that start
    int winSeen, winOk, dmg; float dist0;     // the span saw a boss window open / it was in range then / damage since the span started
    int pre; float preP[2], preB[2];          // a hit landed while the window was still open (geometry at that hit)
    int pend, runsAfter, held, pListed; double tClose; uint32_t pAct; uint64_t pUse; float pP0[2], pB0[2], pDist0, pMax, pAng;
} SmDisp;

static inline int SmDispParseActs(SmDispCfg* c, const char* q) {
    c->nActs = 0;
    while (*q && c->nActs < SM_DP_ACTS) {
        while (*q == ',' || *q == ' ') ++q;
        unsigned long v = 0; int any = 0; const int hex = q[0] == '0' && (q[1] == 'x' || q[1] == 'X'); if (hex) q += 2;
        for (;; ++q, any = 1) { const char ch = *q; int dv;
            if (ch >= '0' && ch <= '9') dv = ch - '0'; else if (hex && ch >= 'a' && ch <= 'f') dv = ch - 'a' + 10; else if (hex && ch >= 'A' && ch <= 'F') dv = ch - 'A' + 10; else break;
            v = v * (hex ? 16u : 10u) + (unsigned)dv; }
        if (!any) break;
        if (v) c->acts[c->nActs++] = (uint32_t)v;
    }
    return c->nActs;
}
static inline void SmDispCfgDefaults(SmDispCfg* c) {
    c->on = 1; c->pts = (float)SM_DP_PTS; c->followMs = SM_DP_FOLLOW_MS; c->followRuns = SM_DP_FOLLOW_RUNS; c->minCm = SM_DP_MIN_CM;
    c->angDeg = SM_DP_ANG_DEG; c->listMinCm = SM_DP_LIST_MIN_CM; c->listAngDeg = SM_DP_LIST_ANG_DEG; c->threatCm = SM_DP_THREAT_CM;
    c->graceMs = SM_DP_GRACE_MS; c->tailMs = SM_DP_TAIL_MS; SmDispParseActs(c, SM_DP_DEFAULT_ACTS);
}
static inline void SmDispCfgClamp(SmDispCfg* c) {
    if (c->pts < 0) c->pts = 0; if (c->followMs < 100) c->followMs = 100; if (c->followMs > 3000) c->followMs = 3000;
    if (c->followRuns < 0) c->followRuns = 0; if (c->followRuns > 4) c->followRuns = 4; if (c->minCm < 0) c->minCm = 0;
    if (c->angDeg < 0) c->angDeg = 0; if (c->angDeg > 180) c->angDeg = 180; if (c->listMinCm < 0) c->listMinCm = 0;
    if (c->listAngDeg < 0) c->listAngDeg = 0; if (c->listAngDeg > 180) c->listAngDeg = 180; if (c->threatCm < 100) c->threatCm = 100;
    if (c->graceMs < 0) c->graceMs = 0; if (c->tailMs < 0) c->tailMs = 0; if (c->tailMs > 2000) c->tailMs = 2000;
    if (c->nActs < 0) c->nActs = 0; if (c->nActs > SM_DP_ACTS) c->nActs = SM_DP_ACTS;
}
static inline void SmDispReset(SmDisp* d) { uint8_t* p = (uint8_t*)d; for (unsigned i = 0; i < sizeof *d; ++i) p[i] = 0; d->lastDmg = -1e18; }
// a DisplaceActs entry (curated ids of displacement moves: relaxed motion test; ids that are not skill rows become eligible)
static inline int SmDispListed(const SmDispCfg* c, uint32_t act) { for (int i = 0; i < c->nActs; ++i) if (c->acts[i] == act) return 1; return 0; }
static inline float SmDispDist(const float* a, const float* b) { const float dx = a[0] - b[0], dz = a[1] - b[1]; return sqrtf(dx * dx + dz * dz); }
// |bearing change| of the player around the boss, degrees 0..180: player p0 seen from boss b0, then p1 seen from b1
static inline float SmDispTurn(const float* p0, const float* b0, const float* p1, const float* b1) {
    const float a0 = atan2f(p0[0] - b0[0], p0[1] - b0[1]), a1 = atan2f(p1[0] - b1[0], p1[1] - b1[1]);
    float d = (a1 - a0) * 57.2957795f;
    while (d > 180.f) d -= 360.f; while (d < -180.f) d += 360.f;
    return d < 0 ? -d : d;
}
static inline int SmDispMotionOk(const SmDispCfg* c, int listed, float moved, float ang) {
    return moved >= (float)(listed ? c->listMinCm : c->minCm) && ang >= (float)(listed ? c->listAngDeg : c->angDeg);
}
// A candidate that did not become a Displacement. Its held avoid is ALWAYS released for the glue to pay (never dropped); a held Counter!
// of a move that did displace (a DisplaceActs move, or the motion test passed so far) is released as a Dodge!: Counter! is for parry skills.
static inline int SmDispEnd(SmDisp* d, const SmDispCfg* c, SmDispOut* o, int why, double t) {
    o->act = d->pAct; o->why = why; o->runs = d->runsAfter; o->ago = t - d->tClose; o->dist0 = d->pDist0; o->listed = d->pListed;
    if (!o->moved) { o->moved = d->pMax; o->ang = d->pAng; }
    o->release = d->held;
    if (d->held == SM_AV_COUNTER && (d->pListed || SmDispMotionOk(c, 0, d->pMax, d->pAng))) { o->release = SM_AV_DODGE; o->asDodge = 1; }
    d->held = 0; d->pend = 0;
    return SM_DP_MISS;
}
static inline int SmDispJudge(SmDisp* d, const SmDispCfg* c, SmDispOut* o, double t, const float* p, const float* b) {
    const float m = SmDispDist(d->pP0, p);
    o->moved = m > d->pMax ? m : d->pMax; o->ang = SmDispTurn(d->pP0, d->pB0, p, b);
    if (o->moved < (float)(d->pListed ? c->listMinCm : c->minCm)) return SmDispEnd(d, c, o, SM_DPM_MOTION, t);
    if (o->ang < (float)(d->pListed ? c->listAngDeg : c->angDeg)) return SmDispEnd(d, c, o, SM_DPM_ANGLE, t);
    o->act = d->pAct; o->why = 0; o->runs = d->runsAfter; o->ago = t - d->tClose; o->dist0 = d->pDist0; o->listed = d->pListed;
    o->release = 0; o->replaced = d->held; d->held = 0; d->pend = 0;  // the held avoid becomes the Displacement
    return SM_DP_AWARD;
}
// the span's boss window closed clean (or a new move started while it was still open): the span's use is a candidate
static inline int SmDispCand(SmDisp* d, const SmDispCfg* c, SmDispOut* o, double t, int avoid) {
    d->pend = 1; d->runsAfter = 0; d->tClose = t; d->pAct = d->sAct; d->pUse = d->sUse; d->pDist0 = d->dist0; d->held = 0; d->pListed = d->sListed;
    d->pP0[0] = d->p0[0]; d->pP0[1] = d->p0[1]; d->pB0[0] = d->b0[0]; d->pB0[1] = d->b0[1]; d->pMax = d->sMax; d->pAng = 0.f;
    if (avoid) { d->held = avoid; o->ate = 1; }
    if (d->pre) { d->pre = 0; return SmDispJudge(d, c, o, t, d->preP, d->preB); }
    return SM_DP_NONE;
}
static inline void SmDispSpan(SmDisp* d, const SmDispIn* in, double t) {
    d->sUse = in->use; d->sAct = in->act; d->sLive = 1; d->sElig = d->sListed = 0; d->sT0 = d->uT0; d->sLast = t;
    d->p0ok = d->uP0ok; d->p0[0] = d->uP0[0]; d->p0[1] = d->uP0[1]; d->b0[0] = d->uB0[0]; d->b0[1] = d->uB0[1]; d->sMax = 0.f;
    d->winSeen = d->winOk = d->pre = 0; d->dmg = in->took || d->lastDmg >= d->uT0;
}
static inline int SmDispStep(SmDisp* d, const SmDispCfg* c, double t, const SmDispIn* in, SmDispOut* o) {
    o->ate = o->release = o->replaced = o->conv = o->asDodge = o->listed = 0; o->why = 0; o->moved = o->ang = 0.f;
    if (!c->on) { const int r = d->pend ? SmDispEnd(d, c, o, SM_DPM_OFF, t) : SM_DP_NONE; SmDispReset(d); return r; }
    int r = SM_DP_NONE;
    if (in->took) {  // damage ends everything: a candidate pays its held avoid (it was a real avoid of the earlier window)
        d->lastDmg = t; d->dmg = 1;
        if (d->pend) return SmDispEnd(d, c, o, SM_DPM_DAMAGE, t);
    }
    if (!d->runOn || in->run != d->run) { d->runOn = 1; d->run = in->run; d->act = in->act; }
    if (!d->uOn || in->use != d->use) {  // a new use (a later segment of the same use keeps the use's start)
        d->uOn = 1; d->use = in->use; d->uT0 = t; d->uMove = 0;
        d->uP0ok = in->plOk && in->bossOk; if (d->uP0ok) { d->uP0[0] = in->pl[0]; d->uP0[1] = in->pl[1]; d->uB0[0] = in->boss[0]; d->uB0[1] = in->boss[1]; }
    }
    const int mv = in->move || in->elig || in->hits > 0;
    if (d->sLive && in->use == d->sUse) { d->sLast = t; d->sAct = in->act; }
    else if (mv && !d->uMove) {  // this use shows itself a move: it is the follow-up of a candidate, and the start of a new span
        d->uMove = 1;
        if (d->pend && in->use != d->pUse && ++d->runsAfter > c->followRuns) r = SmDispEnd(d, c, o, SM_DPM_RUNS, t);
        int claimed = 0;
        if (d->sLive && d->winSeen && d->sElig && d->winOk && !d->dmg && !d->pend && r == SM_DP_NONE) {
            // the displacement's boss window is still open while the follow-up starts: the candidate opens now (a hit of that window
            // still ends it as damage; its avoid, at the real close, is held below), this move is its first follow-up
            r = SmDispCand(d, c, o, t, 0); claimed = 1;
            if (d->pend) { d->runsAfter = 1; if (d->runsAfter > c->followRuns) r = SmDispEnd(d, c, o, SM_DPM_RUNS, t); }
        }
        SmDispSpan(d, in, t);
        if (claimed) { d->winSeen = 1; d->winOk = 0; }  // that window is the old span's, not this move's
    }
    else if (d->sLive && t - d->sLast > (double)c->tailMs) { d->sLive = 0; d->winSeen = d->pre = 0; }  // not a cancel: the span ends
    if (d->sLive && in->use == d->sUse) {
        if (in->elig && d->sT0 - d->lastDmg > (double)c->graceMs) d->sElig = 1;  // any tick of the use (the row can be unreadable on its first tick)
        if (SmDispListed(c, in->act)) d->sListed = 1;
    }
    if (d->sLive && d->p0ok && in->plOk) { const float m = SmDispDist(d->p0, in->pl); if (m > d->sMax) d->sMax = m; }
    if (d->pend && in->plOk) { const float m = SmDispDist(d->pP0, in->pl); if (m > d->pMax) d->pMax = m;
                               if (in->bossOk) d->pAng = SmDispTurn(d->pP0, d->pB0, in->pl, in->boss); }
    // a Counter! of a DisplaceActs move that is not held (no candidate: out of range, a window the span did not see open, ...) is a Dodge!
    if (in->avoid == SM_AV_COUNTER && d->sLive && d->sListed) o->conv = 1;
    if (r == SM_DP_MISS) return r;
    if (d->pend && t - d->tClose > (double)c->followMs) return SmDispEnd(d, c, o, SM_DPM_LATE, t);
    if (d->sLive && in->win) {
        if (!d->winSeen) { d->winSeen = 1; d->winOk = d->p0ok && in->plOk && in->bossOk && SmDispDist(in->pl, in->boss) <= (float)c->threatCm;
                           d->dist0 = in->plOk && in->bossOk ? SmDispDist(in->pl, in->boss) : -1.f; }
        if (in->hits > 0 && d->winOk && !d->pre && in->plOk && in->bossOk) {  // a fast counter: the hit came before the window closed
            d->pre = 1; d->preP[0] = in->pl[0]; d->preP[1] = in->pl[1]; d->preB[0] = in->boss[0]; d->preB[1] = in->boss[1];
        }
    } else if (d->sLive && d->winSeen) {  // the window closed this tick: a candidate when clean
        d->winSeen = 0;
        if (!d->pend && d->sElig && d->winOk && !d->dmg) { const int q = SmDispCand(d, c, o, t, in->avoid); if (q) return q; }
        d->pre = 0;
    }
    if (d->pend && in->avoid && !o->ate && !d->held) { d->held = in->avoid; o->ate = 1; o->conv = 0; }  // the avoid closed a tick later
    if (d->pend && in->hits > 0 && t > d->tClose) {
        if (!in->plOk || !in->bossOk) return SmDispEnd(d, c, o, SM_DPM_MOTION, t);
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
