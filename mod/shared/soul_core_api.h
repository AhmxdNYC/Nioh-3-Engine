// Soul core API (human #136126 / #136194, TASK #136197).
// One file of calls. For every soul core the mod detects, the caller picks one call by core id:
//   SC_CALL_GAME            the game's own soul core (no mod form)
//   SC_CALL_TRANSFORM       start that core's yokai form. Whether the left stick steers is detected, not chosen (TASK #136987,
//                           human #136983): FormSteerTick arms the stick once the yokai travels or its body moves at idle; a still
//                           core never arms, so its stick stays locked (bosslog.cpp: body pin, facing, travel arm, stick unblock)
//   SC_CALL_TRANSFORM_MOVE  retired value (was 'Transform moveable'); kept so old ini values and API callers still work: it reads
//                           and saves as SC_CALL_TRANSFORM, and the menu no longer offers it
//   SC_CALL_FAST_SUMMON     the game's own summon (no mod form), sped up: the toss + orb startup is cut like Transform's and the
//                           yokai plays at one speed factor ([Spirit] FastSummonSpeed); the game ends it (TASK #136502, human #136500)
// The rule is the same for every core id; nothing is per boss or per core in the code (#87625).
// Room for more calls: append a value before SC_CALL_N and a name in sc_call_name(); old ini values keep their meaning.
//
// Exports (Nioh3BossRematch.asi, GetProcAddress, all safe from any thread):
//   int Nioh3BossRematch_SoulCoreApiVersion(void)                    = SC_API_VERSION
//   int Nioh3BossRematch_SoulCoreCallCount(void)                     = SC_CALL_N
//   const char* Nioh3BossRematch_SoulCoreCallName(int call)          "Game's own" / "Transform" / "Transform" (2, retired) / "Fast summon", 0 if out of range
//   int Nioh3BossRematch_SoulCoreDetected(uint32_t* ids, int max)    cores the menu shows (equipped + seen), returns count
//   int Nioh3BossRematch_SoulCoreCallGet(uint32_t id)                the call that core uses on its next use
//   int Nioh3BossRematch_SoulCoreCallSet(uint32_t id, int call)      sets it, saved to [Spirit] FormCoreCalls; returns the call now in effect
//   int Nioh3BossRematch_SoulCoreMoveSensGet(void)                   move sensitivity for every Transform moveable core (x1..x8, [Spirit] FormSteerMove)
//   int Nioh3BossRematch_SoulCoreMoveSensSet(int x)                  sets it (clamped x1..x8), live + saved; returns the value now in effect
//     (API version 2, TASK #136492 / human #136487: one value for all cores; x1 = the game's own step, default x5)
//   int Nioh3BossRematch_SoulCoreFastSpeedGet(void)                  Fast summon yokai speed x10 (0 = same as Transform speed, 10..30 = x1.0..x3.0, [Spirit] FastSummonSpeed)
//   int Nioh3BossRematch_SoulCoreFastSpeedSet(int x10)               sets it (clamped), live + saved; returns the value now in effect
//     (API version 3, TASK #136502: call 3 Fast summon + its one speed factor)
//     (API version 4, TASK #136987: call 2 retired; Set(id, 2) stores Transform, Get never returns 2)
//   int Nioh3BossRematch_SoulCoreDamageGet(void)                     'Soul core damage' % for every core's yokai hits (10..1000, 100 = normal), -1 = no Training
//   int Nioh3BossRematch_SoulCoreDamageSet(int pct)                  sets it (Nioh3Training [Practice] SoulCoreDamagePct, everywhere), returns the value now in effect
//   int Nioh3BossRematch_SoulCoreActorIs(uint64_t chr)               1 = chr is a soul-core yokai seen in the last 60 s (what Training's damage hook asks)
// Pure logic, no game reads: tested offline by mod/Nioh3BossRematch/tests/soul_core_api_test.cpp.
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#define SC_API_VERSION 4  // 2: move sensitivity (TASK #136492); 3: Fast summon (TASK #136502); 4: Transform moveable retired (TASK #136987)
enum ScCall { SC_CALL_GAME = 0, SC_CALL_TRANSFORM = 1, SC_CALL_TRANSFORM_MOVE = 2, SC_CALL_FAST_SUMMON = 3, SC_CALL_N = 4 };

inline const char* sc_call_name(int c) {
    switch (c) {
    case SC_CALL_GAME: return "Game's own";
    case SC_CALL_TRANSFORM: return "Transform";
    case SC_CALL_TRANSFORM_MOVE: return "Transform";  // TASK #136987: retired, same as Transform
    case SC_CALL_FAST_SUMMON: return "Fast summon";
    default: return 0;
    }
}
// TASK #136492: global move sensitivity range (FormSteerBoost multiplies one stick step by it).
#define SC_MOVE_SENS_MIN 1
#define SC_MOVE_SENS_MAX 8
#define SC_MOVE_SENS_DEFAULT 5
inline int sc_move_sens_clamp(int x) { return x < SC_MOVE_SENS_MIN ? SC_MOVE_SENS_MIN : x > SC_MOVE_SENS_MAX ? SC_MOVE_SENS_MAX : x; }
inline bool sc_call_valid(int c) { return c >= 0 && c < SC_CALL_N; }
inline bool sc_call_forms(int c) { return c == SC_CALL_TRANSFORM || c == SC_CALL_TRANSFORM_MOVE; }
// TASK #136987: the retired Transform moveable value means Transform.
inline int sc_call_norm(int c) { return c == SC_CALL_TRANSFORM_MOVE ? SC_CALL_TRANSFORM : c; }
// TASK #136502: the game's own summon runs (no mod form) for Game's own and Fast summon; Fast summon is kept in FormCoresOff too, so an
// older build reads it as Game's own.
inline bool sc_call_game_path(int c) { return c == SC_CALL_GAME || c == SC_CALL_FAST_SUMMON; }
// Fast summon speed, x10 (one factor for every core): 0 = the same as the Transform speed ([Spirit] SealSpeed, the default), 10..30 = x1.0..x3.0.
#define SC_FAST_SPEED_MIN 10
#define SC_FAST_SPEED_MAX 30
#define SC_FAST_SPEED_DEFAULT 0
inline int sc_fast_speed_clamp(int x10) { return x10 <= 0 ? 0 : x10 < SC_FAST_SPEED_MIN ? SC_FAST_SPEED_MIN : x10 > SC_FAST_SPEED_MAX ? SC_FAST_SPEED_MAX : x10; }
// The yokai rate factor for this use: own value, or the Transform speed (sealMul <= 1 = off -> x1).
inline float sc_fast_speed_mul(int x10, float sealMul) { return x10 > 0 ? x10 / 10.f : sealMul > 1.f ? sealMul : 1.f; }
// Step through the calls for a menu row (Left -1 / Right +1 / A 0 = next), wraps. TASK #136987: the menu offers
// Game's own -> Transform -> Fast summon (Transform moveable is detected, not a choice).
inline int sc_call_step(int c, int dir) {
    static const int order[] = {SC_CALL_GAME, SC_CALL_TRANSFORM, SC_CALL_FAST_SUMMON};
    const int n = (int)(sizeof order / sizeof order[0]);
    c = sc_call_valid(c) ? sc_call_norm(c) : SC_CALL_TRANSFORM;
    int i = 0;
    while (i < n && order[i] != c) ++i;
    if (i == n) i = 1;
    const int d = dir < 0 ? -1 : 1;
    return order[(i + d + n) % n];
}
// TASK #136987 (human #136983): every Transform use may steer; FormSteerTick's travel / idle-body detection decides if it does
// (still cores never arm). The game's own summon (Game's own, Fast summon) never steers. globalSteer ([Spirit] FormSteer) is
// no longer a gate; the parameter stays so old callers build.
inline bool sc_call_moveable(int explicitCall, bool globalSteer) {
    (void)globalSteer;
    return !sc_call_game_path(explicitCall);
}

// Per-core explicit calls. id 0 is never stored. Not thread safe by itself; the caller holds its lock.
#define SC_TABLE_MAX 64
struct ScCallTable {
    uint32_t id[SC_TABLE_MAX];
    uint8_t call[SC_TABLE_MAX];
    int n;
};
inline void sc_table_clear(ScCallTable* t) { t->n = 0; }
// -1 = no explicit call for this core.
inline int sc_table_get(const ScCallTable* t, uint32_t id) {
    if (!id) return -1;
    for (int i = 0; i < t->n; ++i) if (t->id[i] == id) return t->call[i];
    return -1;
}
// call < 0 removes. Returns false when the table is full or the call is invalid.
inline bool sc_table_set(ScCallTable* t, uint32_t id, int call) {
    if (!id) return false;
    for (int i = 0; i < t->n; ++i) if (t->id[i] == id) {
        if (call < 0) { for (int j = i + 1; j < t->n; ++j) { t->id[j - 1] = t->id[j]; t->call[j - 1] = t->call[j]; } --t->n; return true; }
        if (!sc_call_valid(call)) return false;
        t->call[i] = (uint8_t)sc_call_norm(call); return true;
    }
    if (call < 0) return true;
    if (!sc_call_valid(call) || t->n >= SC_TABLE_MAX) return false;
    t->id[t->n] = id; t->call[t->n] = (uint8_t)sc_call_norm(call); ++t->n;  // TASK #136987: saved 2 reads as Transform
    return true;
}
// "5129B:2,5216E:1" (hex id : call). Bad pieces are skipped; unknown future calls are skipped, not clamped.
inline int sc_table_parse(ScCallTable* t, const wchar_t* s) {
    sc_table_clear(t);
    if (!s) return 0;
    const wchar_t* p = s;
    while (*p) {
        wchar_t* e = 0;
        const unsigned long id = wcstoul(p, &e, 16);
        if (e == p) { ++p; continue; }
        p = e;
        if (*p != L':') continue;
        ++p;
        const long c = wcstol(p, &e, 10);
        if (e == p) continue;
        p = e;
        if (id && id <= 0xFFFFFFFFul && sc_call_valid((int)c)) sc_table_set(t, (uint32_t)id, (int)c);
    }
    return t->n;
}
inline int sc_table_format(const ScCallTable* t, wchar_t* b, size_t cap) {
    if (!b || !cap) return 0;
    b[0] = 0;
    size_t o = 0;
    for (int i = 0; i < t->n; ++i) {
        wchar_t piece[24];
        const int k = swprintf(piece, 24, L"%ls%X:%d", i ? L"," : L"", (unsigned)t->id[i], (int)t->call[i]);
        if (k <= 0 || o + (size_t)k + 1 > cap) break;
        for (int j = 0; j < k; ++j) b[o++] = piece[j];
        b[o] = 0;
    }
    return (int)o;
}
