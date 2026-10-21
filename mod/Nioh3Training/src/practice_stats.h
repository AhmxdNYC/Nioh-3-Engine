// user #22389: Practice stats (Player / Boss options in the Practice menu). Pure decisions, no game access (tests/practice_stats_test.c).
// Game glue: practice_stats_game.h. Shared with Boss Rematch's menu (practice_stats_menu.h).
// Settings (saved in Nioh3Training.ini [Practice], every one defaults OFF / 100 %):
//   PS_PLAYER_INF_KI   PlayerInfiniteKi    0/1    player Ki (stamina) refilled to max whenever it drops
//   PS_DMG_DEALT       DamageDealtPct      0..1000 HP damage the player deals (to every non-player victim of a hit) x pct / 100
//   PS_DMG_TAKEN       DamageTakenPct      0..1000 HP damage the player takes from hits x pct / 100
//   PS_BOSS_KI_DRAIN   BossKiDrainPct      0..50  Training bosses lose this % of max Ki per second (floor Ki 0.5 = one hit from a break)
// Not available yet (no lever proven offline, notes/practice-stats-22389.md): boss speed multiplier, infinite player ninjutsu / onmyo.
#pragma once
#include <stdint.h>
enum { PS_PLAYER_INF_KI = 0, PS_DMG_DEALT = 1, PS_DMG_TAKEN = 2, PS_BOSS_KI_DRAIN = 3, PS_N = 4 };
typedef struct { int v[PS_N]; } PstCfg;
static inline int PstDefault(int what) { return what == PS_DMG_DEALT || what == PS_DMG_TAKEN ? 100 : 0; }
static inline void PstDefaults(PstCfg* c) { for (int i = 0; i < PS_N; ++i) c->v[i] = PstDefault(i); }
static inline int PstClamp(int what, int v) {
    const int lo = 0, hi = what == PS_PLAYER_INF_KI ? 1 : what == PS_BOSS_KI_DRAIN ? 50 : 1000;
    return v < lo ? lo : v > hi ? hi : v;
}
// Left / Right steps of the menu sliders (A on an ON/OFF row flips it).
static inline int PstStep(int what, int v, int dir) {
    static const int dmg[] = {0, 10, 25, 50, 75, 100, 125, 150, 200, 300, 500, 1000};
    static const int drain[] = {0, 1, 2, 3, 5, 10, 20, 50};
    const int* st = what == PS_BOSS_KI_DRAIN ? drain : dmg;
    const int n = what == PS_BOSS_KI_DRAIN ? (int)(sizeof drain / sizeof drain[0]) : (int)(sizeof dmg / sizeof dmg[0]);
    if (what == PS_PLAYER_INF_KI) return dir > 0 ? 1 : dir < 0 ? 0 : !v;
    v = PstClamp(what, v);
    if (dir > 0) { for (int i = 0; i < n; ++i) if (st[i] > v) return st[i]; return st[n - 1]; }
    if (dir < 0) { for (int i = n - 1; i >= 0; --i) if (st[i] < v) return st[i]; return st[0]; }
    return v;
}
// Scaled HP damage of one hit. Only positive damage is scaled (0 / negative = heal or no-op, untouched); 100 % = unchanged; rounds to nearest;
// a positive hit at a non-zero % never rounds down to 0; never overflows int.
static inline int PstScaleDmg(int dmg, int pct) {
    if (dmg <= 0 || pct == 100) return dmg;
    if (pct <= 0) return 0;
    const int64_t r = ((int64_t)dmg * pct + 50) / 100;
    return r < 1 ? 1 : r > 0x7FFFFFFF ? 0x7FFFFFFF : (int)r;
}
// Ki drain for one tick of dtMs: new Ki (floor 0.5, so a drain alone never makes the 'bare' Ki-0 break KiBreakBareFix cancels). Returns 1 = write *out.
// Never on a Ki-broken / dead boss, in the KiBreakNoFinisher HP-0 window, or while that boss's infinite Ki holds it (infKi wins).
static inline int PstKiDrain(int pct, int dtMs, int dead, int window, int broken, int infKi, float cur, float max, float* out) {
    if (pct <= 0 || dtMs <= 0 || dead || window || broken || infKi || !(max > 0) || !(cur > 0.5f)) return 0;
    if (dtMs > 500) dtMs = 500;  // a long hitch (load, menu) does not drain a big chunk at once
    float t = cur - max * (float)pct / 100.0f * (float)dtMs / 1000.0f;
    if (t < 0.5f) t = 0.5f;
    if (!(t < cur - 0.001f)) return 0;
    *out = t; return 1;
}
// Player infinite Ki: 1 = write max this tick. Sanity bounds guard a wrong field read (the player Ki offset is the boss one, live check pending).
static inline int PstPlayerKiRefill(int on, float cur, float max) {
    if (!on || !(max > 0.0f) || max > 100000.0f || !(cur >= -100000.0f && cur <= 100000.0f)) return 0;
    return cur < max - 0.5f;
}
