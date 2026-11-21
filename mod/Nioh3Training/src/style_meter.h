// human #126496 / #126502: DMC-style "style meter" (rank D, C, B, A, S, SS, SSS + a gauge inside the rank). Pure state machine, no game access
// (tests/style_meter_test.c). Game glue: style_meter_game.h (fed once per FRAMEPROBE pump tick, game clock in ms).
// Generic (human rule #87625): no per-boss or per-move table. Events and their base points:
//   hit     the player's hit took HP off an enemy                  cfg.hit   (x variety factor, below)
//   dodge   an enemy hit window opened and closed while the player had i-frames in it and took no damage   cfg.dodge
//   ki      a Ki pulse landed within cfg.kiHitMs of a landed hit     cfg.ki   (human #126744: a pulse after a whiff scores 0; kiHitMs 0 = no gate)
//   startup (#126821) a NEW use's points x (1 + startupPct % x how far its first hit window opened between startupMinF and startupMaxF, 60 fps
//           frames, from the frame meter's own run: run.hit[0]). Slow, risky startups earn more; unknown startup = x1.
//   counter (#126843) + cfg.counter once per use when the player had i-frames while an enemy hit window was open in the same action run
//           (an invincible counter / attack through an attack), then landed the hit.
//   skill   (#126850 soul core, #126854 ninjutsu) cfg.soul / cfg.nin, only in combat (a landed hit within cfg.combatMs): no out-of-fight farming.
//   style   (#126843 Samurai <-> Ninja switch) cfg.style, only in combat and only once per landed hit between switches (no switch spam).
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
#define SM_DECAY_DELAY_MS 4000  // human #126785: was 2500
#define SM_DECAY_PER_S 12   // human #126785: harder to lose rank (2.5x ranks drain at the old 12/s)
#define SM_DECAY_RANK_PCT 35
#define SM_COMBAT_MS 8000
#define SM_KI_HIT_MS 3000        // human #126744: whiffs must not raise the meter (Ki pulse only counts in a combo that landed)
#define SM_DAMAGE_GRACE_MS 1500  // live #126502: one multi-hit enemy combo = one rank drop
#define SM_STARTUP_MIN_F 15   // #126821: startups at or below this many 60 fps frames get no bonus
#define SM_STARTUP_MAX_F 60   // ... at or above this the full bonus
#define SM_STARTUP_PCT 100    // full bonus: points x2
#define SM_COUNTER_PTS 30
#define SM_NIN_PTS 20
#define SM_SOUL_PTS 40
#define SM_STYLE_PTS 15
#define SM_ACT_NONE 0xFFFFFFFFu

static const char* const sm_rank_name[SM_RANKS] = {"D", "C", "B", "A", "S", "SS", "SSS"};
// Words of the approved sprite set (Temp/rank126376/sprites/sprites.py RANKS).
static const char* const sm_rank_word[SM_RANKS] = {"Dull", "Cunning", "Bold", "Ardent", "Asura", "Sword Saint", "Heavenly"};
// human #126755 too easy (SS in ~40 hits), #126766 4x, #126785 4x too hard (C after 52 hits): 2.5x {40, 60, 80, 100, 130, 160, 200}
static const float sm_rank_pts_default[SM_RANKS] = {100.f, 150.f, 200.f, 250.f, 325.f, 400.f, 500.f};

typedef struct {
    float hit, dodge, ki;                 // base points per event
    int repeatPct, repeatFloorPct, multiPct;
    int decayDelayMs; float decayPerS; int decayRankPct;
    int damageGraceMs;                    // further damage within this window after a drop costs no extra rank
    int kiHitMs;                          // a Ki pulse scores only this long after the last landed hit (0 = always)
    int combatMs;                         // the meter stays up this long after the last event when no boss fight is live
    int startupMinF, startupMaxF, startupPct;  // #126821 startup scaling
    float counter, nin, soul, style;      // #126843 / #126854 / #126850 bonus points
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
    double lastHit;         // game ms of the last landed hit (Ki pulse gate)
    double lastDamaged;     // game ms of the last rank drop (damage grace), <0 = none
    uint32_t hitsAtStyle;   // s->hits at the last scored style switch (one switch bonus per landed hit)
    uint32_t hits, dodges, kis, damaged;   // counters (log / tests)
    uint32_t counters, nins, souls, styles;
} SmState;

static inline void SmCfgDefaults(SmCfg* c) {
    c->hit = (float)SM_HIT_PTS; c->dodge = (float)SM_DODGE_PTS; c->ki = (float)SM_KI_PTS;
    c->repeatPct = SM_REPEAT_PCT; c->repeatFloorPct = SM_REPEAT_FLOOR_PCT; c->multiPct = SM_MULTI_PCT;
    c->decayDelayMs = SM_DECAY_DELAY_MS; c->decayPerS = (float)SM_DECAY_PER_S; c->decayRankPct = SM_DECAY_RANK_PCT;
    c->startupMinF = SM_STARTUP_MIN_F; c->startupMaxF = SM_STARTUP_MAX_F; c->startupPct = SM_STARTUP_PCT;
    c->counter = (float)SM_COUNTER_PTS; c->nin = (float)SM_NIN_PTS; c->soul = (float)SM_SOUL_PTS; c->style = (float)SM_STYLE_PTS;
    c->combatMs = SM_COMBAT_MS; c->kiHitMs = SM_KI_HIT_MS; c->damageGraceMs = SM_DAMAGE_GRACE_MS;
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
    if (c->kiHitMs < 0) c->kiHitMs = 0;
    if (c->startupMinF < 0) c->startupMinF = 0; if (c->startupMaxF <= c->startupMinF) c->startupMaxF = c->startupMinF + 1;
    if (c->startupPct < 0) c->startupPct = 0; if (c->startupPct > 500) c->startupPct = 500;
    if (c->counter < 0) c->counter = 0; if (c->nin < 0) c->nin = 0; if (c->soul < 0) c->soul = 0; if (c->style < 0) c->style = 0;
    for (int i = 0; i < SM_RANKS; ++i) if (!(c->rankPts[i] >= 1.f)) c->rankPts[i] = 1.f;
}
static inline void SmReset(SmState* s) {
    uint8_t* p = (uint8_t*)s; for (unsigned i = 0; i < sizeof *s; ++i) p[i] = 0;
    s->useAct = SM_ACT_NONE; s->lastScore = s->lastEvent = s->lastHit = -1e18; s->lastDamaged = -1;
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
// #126821 startup multiplier: startupF = 60 fps frames from the action start to its first hit window (< 0 = unknown -> 1).
static inline float SmStartupMult(const SmCfg* c, float startupF) {
    if (!(startupF >= 0.f) || c->startupPct <= 0) return 1.f;
    float k = (startupF - (float)c->startupMinF) / (float)(c->startupMaxF - c->startupMinF);
    k = k < 0.f ? 0.f : k > 1.f ? 1.f : k;
    return 1.f + k * (float)c->startupPct / 100.f;
}
// One hit landed. act = the player's action id at the hit, run = a key of that action run (a new run of the same id = a new use).
// startupF: that run's startup (frames, < 0 unknown); counter: 1 = the run had i-frames inside an enemy hit window (bonus once per use).
static inline float SmOnHitEx(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run, float startupF, int counter) {
    float f, bonus = 0.f;
    if (s->useOn && act == s->useAct && run == s->useRun) f = s->useFactor * (float)c->multiPct / 100.f;
    else {
        f = SmVariety(s, c, act) * SmStartupMult(c, startupF);
        if (counter && c->counter > 0.f) { bonus = c->counter; s->counters++; }
        s->useOn = 1; s->useAct = act; s->useRun = run; s->useFactor = f;
        if (act != SM_ACT_NONE && act != 0) { s->hist[s->histPos] = act; s->histPos = (s->histPos + 1) % SM_HIST; if (s->histN < SM_HIST) s->histN++; }
    }
    const float p = c->hit * f + bonus;
    SmAdd(s, c, p); s->hits++; s->lastEvent = s->lastHit = t; if (p > 0.f) s->lastScore = t;
    return p;
}
static inline float SmOnHit(SmState* s, const SmCfg* c, double t, uint32_t act, uint64_t run) { return SmOnHitEx(s, c, t, act, run, -1.f, 0); }
// In combat = a hit landed within combatMs (skills / style switches outside a fight score nothing and do not wake the meter).
static inline int SmInCombat(const SmState* s, const SmCfg* c, double t) { return t - s->lastHit <= (double)c->combatMs; }
// #126850 soul core / #126854 ninjutsu used. kind 0 = ninjutsu, 1 = soul core.
static inline float SmOnSkill(SmState* s, const SmCfg* c, double t, int kind) {
    if (kind) s->souls++; else s->nins++;
    if (!SmInCombat(s, c, t)) return 0.f;
    const float p = kind ? c->soul : c->nin;
    if (p <= 0.f) return 0.f;
    SmAdd(s, c, p); s->lastEvent = s->lastScore = t; return p;
}
// #126843 Samurai <-> Ninja style switch: scores in combat, once per landed hit since the last scored switch.
static inline float SmOnStyle(SmState* s, const SmCfg* c, double t) {
    s->styles++;
    if (!SmInCombat(s, c, t) || s->hits == s->hitsAtStyle || c->style <= 0.f) return 0.f;
    s->hitsAtStyle = s->hits;
    SmAdd(s, c, c->style); s->lastEvent = s->lastScore = t; return c->style;
}
static inline float SmOnDodge(SmState* s, const SmCfg* c, double t) { SmAdd(s, c, c->dodge); s->dodges++; s->lastEvent = s->lastScore = t; return c->dodge; }
static inline float SmOnKi(SmState* s, const SmCfg* c, double t) {
    s->kis++;
    if (c->kiHitMs > 0 && !(t - s->lastHit <= (double)c->kiHitMs)) return 0.f;  // pulse after a whiff: no points, no decay reset
    SmAdd(s, c, c->ki); s->lastEvent = s->lastScore = t; return c->ki;
}
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
