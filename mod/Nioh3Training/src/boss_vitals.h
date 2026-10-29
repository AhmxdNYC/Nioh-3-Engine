// user #14320 / #14323 / #14324: per-boss HP and Ki controls in Training (one page per boss: Boss A, Boss B, ...).
// Pure decisions, no game access (tests/boss_vitals_test.c). Game glue (actor lookup, native writes, exports): boss_vitals_game.h.
// Per slot (session only, every game start = defaults OFF / 100 %):
//   infHp  'Boss infinite health': HP never stays below the HP level (hpPct of max; 100 % = always full).
//   infKi  'Boss infinite Ki'    : Ki never stays below the Ki level (kiPct of max). OPTIONAL (default OFF) so Ki breaks still happen;
//                                  even when ON a boss that is already Ki broken is left alone until the break is over.
//   hpPct  'Set boss HP %'       : one-shot set to that % of max (also the infinite-health level). 1..100 (0 would kill).
//   kiPct  'Set boss Ki %'       : one-shot set to that % of max (also the infinite-Ki level). 0..100; 0 % = Ki 0.5, one hit
//                                  from a break (a plain Ki 0 write is the 'bare' break KiBreakBareFix cancels back to 0.5).
//   'Refill boss'                : both levels back to 100 % and both set at once.
#pragma once
#define BV_SLOTS 26  // user #14309: boss letters A..Z the API accepts (Training N3T_SLOTS; the menu shows the live ones)
enum { BV_INF_HP = 0, BV_INF_KI = 1, BV_HP_PCT = 2, BV_KI_PCT = 3, BV_REFILL = 4 };
enum { BV_REQ_HP = 1, BV_REQ_KI = 2 };
typedef struct { int infHp, infKi, hpPct, kiPct; } BvCfg;
static inline void BvDefaults(BvCfg* c) { c->infHp = 0; c->infKi = 0; c->hpPct = 100; c->kiPct = 100; }
static inline int BvClampPct(int v, int lo) { return v < lo ? lo : v > 100 ? 100 : v; }
// Left / Right steps: 0 (Ki only), 1, 5, 10, 20, ... 100 (A on the menu row re-applies the shown value).
static inline int BvStepPct(int v, int dir, int lo) {
    static const int st[] = {0, 1, 5, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
    const int n = (int)(sizeof st / sizeof st[0]);
    v = BvClampPct(v, lo);
    if (dir > 0) { for (int i = 0; i < n; ++i) if (st[i] > v) return st[i]; return 100; }
    if (dir < 0) { for (int i = n - 1; i >= 0; --i) if (st[i] < v && st[i] >= lo) return st[i]; return lo; }
    return v;
}
static inline uint64_t BvHpTarget(uint64_t m, int pct) {
    if (!m) return 0;
    uint64_t t = m / 100 * (uint64_t)BvClampPct(pct, 1) + m % 100 * (uint64_t)BvClampPct(pct, 1) / 100;
    return t < 1 ? 1 : t > m ? m : t;
}
static inline float BvKiTarget(float m, int pct) {
    if (!(m > 0)) return 0;
    const int p = BvClampPct(pct, 0);
    const float t = m * (float)p / 100.0f;
    return t < 0.5f ? 0.5f : t;
}
// Infinite health: 1 = write *out this tick. Not on a dead boss (no revive) or in the KiBreakNoFinisher HP-0 window (it owns HP there).
static inline int BvHpHold(const BvCfg* c, int dead, int window, uint64_t cur, uint64_t max, uint64_t* out) {
    if (!c->infHp || dead || window || !max) return 0;
    const uint64_t t = BvHpTarget(max, c->hpPct);
    if (cur >= t) return 0;
    *out = t; return 1;
}
// Infinite Ki: never on a Ki-broken / exhausted boss (the break plays out as normal) or a dead boss / HP-0 window.
static inline int BvKiHold(const BvCfg* c, int dead, int window, int broken, float cur, float max, float* out) {
    if (!c->infKi || dead || window || broken || !(max > 0)) return 0;
    const float t = BvKiTarget(max, c->kiPct);
    if (cur >= t - 0.01f) return 0;
    *out = t; return 1;
}
// Ki ceiling loss (Ki object +0x88): KiSetCur and the native regen stop at max - loss. 1 = clear it before writing v (v above that ceiling,
// loss sane). Without it a refill to 100 % reads back max - loss ('WRITE DID NOT STICK', #22592).
static inline int BvKiCeilClear(float v, float max, float loss) { return loss > 0.0f && loss <= max && v > max - loss + 0.25f; }
// One-shot Ki set (Set boss Ki % / Refill) while the boss is Ki broken: waits for the break to end, at most this long, then dropped.
#define BV_KI_WAIT_MS 8000
// HP write with the KiBreakNoFinisher hidden reserve (#9319: real HP = shown + off): keep the reserve when it fits under max, else drop it.
// Returns the raw value to write; *keepOff = 1 when the reserve stays.
static inline uint64_t BvHpRaw(uint64_t target, uint64_t max, int64_t off, int* keepOff) {
    if (off > 0 && target + (uint64_t)off <= max) { *keepOff = 1; return target + (uint64_t)off; }
    *keepOff = 0; return target;
}
