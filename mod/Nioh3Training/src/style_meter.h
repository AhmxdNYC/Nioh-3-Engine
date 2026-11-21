// human #126496 / #126502: DMC-style "style meter" (rank D, C, B, A, S, SS, SSS + a gauge inside the rank). Pure state machine, no game access
// (tests/style_meter_test.c). Game glue: style_meter_game.h (fed once per FRAMEPROBE pump tick, game clock in ms).
// Generic (human rule #87625): no per-boss or per-move table. Events and their base points:
//   hit     the player's hit took HP off an enemy                  cfg.hit   (x variety factor, below)
//   dodge   an enemy hit window opened and closed while the player had i-frames in it and took no damage   cfg.dodge
//   ki      a Ki pulse landed                                       cfg.ki
//   damaged the player lost HP: one rank down and the gauge empty (rank D with an empty gauge = the meter is idle / hidden)
// Variety: every new "use" (player action id + action run) is compared with the last SM_HIST uses; each earlier use of the same action id
// multiplies the points by repeatPct % (floor repeatFloorPct %). Further hits of the same use (multi-hit, several enemies) get multiPct % of that.
// Decay: decayDelayMs after the last scoring event the gauge drains decayPerS points per second, +decayRankPct % per rank above D;
// draining below 0 drops a rank (gauge refilled from that rank's top), at D it ends empty (idle).
// Promotion: rankPts[r] points fill rank r; the overflow carries into the next rank. SSS caps at a full gauge.
#ifndef STYLE_METER_H
#define STYLE_METER_H
#include <stdint.h>

#define SM_RANKS 7
#define SM_HIST 8
#define SM_HIT_PTS 10
#define SM_DODGE_PTS 30
#define SM_KI_PTS 20
#define SM_REPEAT_PCT 60
#define SM_REPEAT_FLOOR_PCT 10
#define SM_MULTI_PCT 50
#define SM_DECAY_DELAY_MS 2500
#define SM_DECAY_PER_S 12
#define SM_DECAY_RANK_PCT 35
#define SM_COMBAT_MS 8000
#define SM_DAMAGE_GRACE_MS 1500  // live #126502: one multi-hit enemy combo = one rank drop
#define SM_ACT_NONE 0xFFFFFFFFu

static const char* const sm_rank_name[SM_RANKS] = {"D", "C", "B", "A", "S", "SS", "SSS"};
// Words of the approved sprite set (Temp/rank126376/sprites/sprites.py RANKS).
static const char* const sm_rank_word[SM_RANKS] = {"Dull", "Cunning", "Bold", "Ardent", "Asura", "Sword Saint", "Heavenly"};
static const float sm_rank_pts_default[SM_RANKS] = {40.f, 60.f, 80.f, 100.f, 130.f, 160.f, 200.f};

typedef struct {
    float hit, dodge, ki;                 // base points per event
    int repeatPct, repeatFloorPct, multiPct;
    int decayDelayMs; float decayPerS; int decayRankPct;
    int damageGraceMs;                    // further damage within this window after a drop costs no extra rank
    int combatMs;                         // the meter stays up this long after the last event when no boss fight is live
    float rankPts[SM_RANKS];
} SmCfg;

typedef struct { int inWin, sawIfr, hit; } SmDodge;

typedef struct {
    int rank;               // 0 = D .. 6 = SSS
    float pts;              // points inside the rank, 0 .. rankPts[rank]
    int active;             // 0 = idle (D, empty): hidden
    double lastScore;       // game ms of the last scoring event (decay delay)
    double lastEvent;       // game ms of the last event of any kind (combat timeout)
    uint32_t hist[SM_HIST]; int histN, histPos;
    uint32_t useAct; uint64_t useRun; float useFactor; int useOn;
    SmDodge dodge;
    double lastDamaged;     // game ms of the last rank drop (damage grace), <0 = none
    uint32_t hits, dodges, kis, damaged;   // counters (log / tests)
} SmState;

static inline void SmCfgDefaults(SmCfg* c) {
    c->hit = (float)SM_HIT_PTS; c->dodge = (float)SM_DODGE_PTS; c->ki = (float)SM_KI_PTS;
    c->repeatPct = SM_REPEAT_PCT; c->repeatFloorPct = SM_REPEAT_FLOOR_PCT; c->multiPct = SM_MULTI_PCT;
    c->decayDelayMs = SM_DECAY_DELAY_MS; c->decayPerS = (float)SM_DECAY_PER_S; c->decayRankPct = SM_DECAY_RANK_PCT;
    c->combatMs = SM_COMBAT_MS; c->damageGraceMs = SM_DAMAGE_GRACE_MS;
    for (int i = 0; i < SM_RANKS; ++i) c->rankPts[i] = sm_rank_pts_default[i];
}
// Clamp ini values into sane ranges (no zero-size rank, no negative points).
static inline void SmCfgClamp(SmCfg* c) {
    if (c->hit < 0) c->hit = 0; if (c->dodge < 0) c->dodge = 0; if (c->ki < 0) c->ki = 0;
    if (c->repeatPct < 0) c->repeatPct = 0; if (c->repeatPct > 100) c->repeatPct = 100;
    if (c->repeatFloorPct < 0) c->repeatFloorPct = 0; if (c->repeatFloorPct > 100) c->repeatFloorPct = 100;
    if (c->multiPct < 0) c->multiPct = 0; if (c->multiPct > 100) c->multiPct = 100;
    if (c->decayDelayMs < 0) c->decayDelayMs = 0; if (c->decayPerS < 0) c->decayPerS = 0; if (c->decayRankPct < 0) c->decayRankPct = 0;
    if (c->combatMs < 0) c->combatMs = 0;
    if (c->damageGraceMs < 0) c->damageGraceMs = 0;
    for (int i = 0; i < SM_RANKS; ++i) if (!(c->rankPts[i] >= 1.f)) c->rankPts[i] = 1.f;
}
static inline void SmReset(SmState* s) {
    uint8_t* p = (uint8_t*)s; for (unsigned i = 0; i < sizeof *s; ++i) p[i] = 0;
    s->useAct = SM_ACT_NONE; s->lastScore = s->lastEvent = -1e18; s->lastDamaged = -1;
}
static inline float SmGauge(const SmState* s, const SmCfg* c) {
    if (!s->active) return 0.f;
    const float g = s->pts / c->rankPts[s->rank];
    return g < 0.f ? 0.f : g > 1.f ? 1.f : g;
}
static inline void SmAdd(SmState* s, const SmCfg* c, float pts) {
    if (pts <= 0.f) return;
    s->active = 1; s->pts += pts;
    while (s->rank < SM_RANKS - 1 && s->pts >= c->rankPts[s->rank]) { s->pts -= c->rankPts[s->rank]; s->rank++; }
    if (s->rank == SM_RANKS - 1 && s->pts > c->rankPts[s->rank]) s->pts = c->rankPts[s->rank];
}
// Variety factor for a NEW use of action id `act` (1.0 for an unknown id: no data, no penalty).
static inline float SmVariety(const SmState* s, const SmCfg* c, uint32_t act) {
    if (act == SM_ACT_NONE || act == 0) return 1.f;
    int n = 0; for (int i = 0; i < s->histN; ++i) if (s->hist[i] == act) n++;
    float f = 1.f; for (int i = 0; i < n; ++i) f *= (float)c->repeatPct / 100.f;
    const float fl = (float)c->repeatFloorPct / 100.f;
    return f < fl ? fl : f;
}
// One hit landed. act = the player's action id at the hit, run = a key of that action run (a new run of the same id = a new use).
static inline float SmOnHit(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run) {
    float f;
    if (s->useOn && act == s->useAct && run == s->useRun) f = s->useFactor * (float)c->multiPct / 100.f;
    else {
        f = SmVariety(s, c, act);
        s->useOn = 1; s->useAct = act; s->useRun = run; s->useFactor = f;
        if (act != SM_ACT_NONE && act != 0) { s->hist[s->histPos] = act; s->histPos = (s->histPos + 1) % SM_HIST; if (s->histN < SM_HIST) s->histN++; }
    }
    const float p = c->hit * f;
    SmAdd(s, c, p); s->hits++; s->lastEvent = t; if (p > 0.f) s->lastScore = t;
    return p;
}
static inline float SmOnDodge(SmState* s, const SmCfg* c, double t) { SmAdd(s, c, c->dodge); s->dodges++; s->lastEvent = s->lastScore = t; return c->dodge; }
static inline float SmOnKi(SmState* s, const SmCfg* c, double t) { SmAdd(s, c, c->ki); s->kis++; s->lastEvent = s->lastScore = t; return c->ki; }
static inline void SmOnDamaged(SmState* s, const SmCfg* c, double t) {
    s->damaged++; s->lastEvent = t;
    if (s->dodge.inWin) s->dodge.hit = 1;
    if (s->lastDamaged >= 0 && t - s->lastDamaged < (double)c->damageGraceMs) { s->pts = 0.f; return; }  // same combo: no second drop
    s->lastDamaged = t;
    if (s->rank > 0) s->rank--;
    s->pts = 0.f; s->active = s->rank > 0;
    s->useOn = 0; s->useAct = SM_ACT_NONE;
}
// Per tick: idle decay. t = game ms now, dt = game ms since the last tick.
static inline void SmTick(SmState* s, const SmCfg* c, double t, double dt) {
    if (!s->active || dt <= 0) return;
    double idle = t - s->lastScore - (double)c->decayDelayMs;
    if (idle <= 0) return;
    if (idle > dt) idle = dt;
    float d = c->decayPerS * (1.f + (float)s->rank * (float)c->decayRankPct / 100.f) * (float)(idle / 1000.0);
    s->pts -= d;
    while (s->pts < 0.f) {
        if (s->rank == 0) { s->pts = 0.f; s->active = 0; s->useOn = 0; break; }
        s->rank--; s->pts += c->rankPts[s->rank];
    }
}
// Dodge detector, once per tick: enemyActive = an enemy hit window is open now, ifr = the player has i-frames now, tookHit = the player lost HP
// this tick. 1 on the tick the window closes when i-frames were seen inside it and no damage came in.
static inline int SmDodgeStep(SmDodge* d, int enemyActive, int ifr, int tookHit) {
    int award = 0;
    if (enemyActive) {
        if (!d->inWin) { d->inWin = 1; d->sawIfr = 0; d->hit = 0; }
        if (ifr) d->sawIfr = 1;
        if (tookHit) d->hit = 1;
    } else if (d->inWin) {
        if (tookHit) d->hit = 1;  // the damage of the last frame of the window lands on the close tick
        award = d->sawIfr && !d->hit;
        d->inWin = 0;
    }
    return award;
}
// Visible: something to show, and a boss fight is live or the last event is recent.
static inline int SmVisible(const SmState* s, const SmCfg* c, double t, int fight) {
    return s->active && (fight || t - s->lastEvent < (double)c->combatMs);
}
#endif
