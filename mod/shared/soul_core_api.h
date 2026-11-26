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
// Fast summon speed, x10 (one factor for every core): 0 = 90 % of the Transform speed ([Spirit] SealSpeed, the default), 10..30 = x1.0..x3.0.
#define SC_FAST_SPEED_MIN 10
#define SC_FAST_SPEED_MAX 30
#define SC_FAST_SPEED_DEFAULT 0
inline int sc_fast_speed_clamp(int x10) { return x10 <= 0 ? 0 : x10 < SC_FAST_SPEED_MIN ? SC_FAST_SPEED_MIN : x10 > SC_FAST_SPEED_MAX ? SC_FAST_SPEED_MAX : x10; }
// The yokai rate factor for this use: own value, or the Transform speed (sealMul <= 1 = off -> x1).
// TASK #137013 (human #137006): the auto value runs a tiny bit under the Transform speed (90 %), never under x1.
#define SC_FAST_SPEED_AUTO_PCT 90
inline float sc_fast_speed_mul(int x10, float sealMul) {
    if (x10 > 0) return x10 / 10.f;
    const float a = sealMul * (SC_FAST_SPEED_AUTO_PCT / 100.f);
    return sealMul > 1.f && a > 1.f ? a : 1.f;
}
// TASK #137323 (human #137208): Fast summon keeps the yokai's attack at the speed above but skips its startup (appear / wind-up before its
// first hit) and its end pose (after its last hit until the game despawns it). One phase per press for all its yokai chrs:
// START (armed by certain data only, see sc_fast_start_arm) until the first hit, ATTACK, END once every chr that hits is past its last hit (certain game-data count only) + grace.
// [Spirit] FastSummonStartSpeed / FastSummonEndSpeed (<= 1 = off = the attack speed). Same rule for every core; the player is never touched.
#define SC_FAST_PH_START 0
#define SC_FAST_PH_ATTACK 1
#define SC_FAST_PH_END 2
#define SC_FAST_EDGE_DEFAULT 6.f
#define SC_FAST_EDGE_MAX 12.f
inline const char* sc_fast_phase_name(int p) { return p == SC_FAST_PH_START ? "start" : p == SC_FAST_PH_END ? "end" : "attack"; }
inline float sc_fast_edge_clamp(float v) { return !(v > 1.f) ? 1.f : v > SC_FAST_EDGE_MAX ? SC_FAST_EDGE_MAX : v; }
inline float sc_fast_phase_mul(int phase, float attackMul, float startMul, float endMul) {
    const float m = phase == SC_FAST_PH_START ? startMul : phase == SC_FAST_PH_END ? endMul : 0.f;
    return m > 1.f ? m : attackMul;
}
// The chr's place against its own action chain (game data, read when it hits): actPos = index of its current action in the chain,
// -1 = a readable action outside the chain (it moved on), -2 = unreadable.
// Before its first hit: reached the first hit's action at (start frame - lead).
inline bool sc_fast_at_first(int actPos, int firstIdx, float frame, int firstStart, int leadFrames) {
    if (firstIdx < 0 || actPos < 0) return false;
    return actPos > firstIdx || (actPos == firstIdx && frame >= (float)(firstStart - leadFrames));
}
// After its last hit: in the last hit's action past its end frame, a later chain action, or out of the chain.
inline bool sc_fast_past_last(int actPos, int lastIdx, float frame, int lastEnd) {
    if (lastIdx < 0 || actPos == -2 || actPos < -2) return false;
    return actPos == -1 || actPos > lastIdx || (actPos == lastIdx && frame > (float)lastEnd);
}
// Per chr end state: BLOCK (may still hit / unknown), DONE (all its data hits seen live and past its last), INERT (its data has no hit at all).
#define SC_FAST_CHR_BLOCK 0
#define SC_FAST_CHR_DONE 1
#define SC_FAST_CHR_INERT 2
inline int sc_fast_chr_end(bool dataOk, int bursts, int untimed, int spawns, bool chainCut, bool everHit, int liveBursts, bool openNow, bool pastLast) {
    if (!dataOk) return SC_FAST_CHR_BLOCK;
    if (!bursts && !untimed && !spawns && !everHit && !chainCut) return SC_FAST_CHR_INERT;
    if (bursts <= 0 || untimed || spawns || chainCut) return SC_FAST_CHR_BLOCK;  // count not certain: leave the end alone
    if (openNow || liveBursts < bursts || !pastLast) return SC_FAST_CHR_BLOCK;
    return SC_FAST_CHR_DONE;
}
// TASK #137323 fix (live BR 33E76024, core 3B1AB): the phase must never speed a yokai before the data proves it safe. A x6 rate on the
// spawn frame (phase defaulted to START) left the yokai frozen and the game's summon never ended (no re-summon on the next presses).
// The startup skip arms only when a chr's own data is CERTAIN, it is still before its first hit in that chain, it is at least minAgeMs old
// (its spawn / appear frames always play at the attack speed) and the speed is on. Never armed = attack speed, the 645efbf5 behaviour.
inline bool sc_fast_start_arm(bool certain, int actPos, int firstIdx, float frame, int firstStart, int leadFrames, int ageMs, int minAgeMs, float startMul) {
    if (!certain || !(startMul > 1.f) || ageMs < minAgeMs || firstIdx < 0 || actPos < 0 || actPos > firstIdx) return false;
    return !sc_fast_at_first(actPos, firstIdx, frame, firstStart, leadFrames);
}
// A live attack collider counts as a hit only at / after the chr's first data hit (an appearance collider at spawn is not a hit). A chr
// without usable data: counted (it can only block the end, never start a skip).
inline bool sc_fast_hit_counts(bool usable, int actPos, int firstIdx, float frame, int firstStart, int leadFrames) {
    if (!usable) return true;
    return actPos == -1 || sc_fast_at_first(actPos, firstIdx, frame, firstStart, leadFrames);
}
// Press phase. startOn = armed and its first hit not reached yet (sticky off once reached / capped).
inline int sc_fast_group_phase(bool startOn, int msSinceArm, int startCapMs, int nDone, int nBlock, bool openAny, int msSinceLastClose, int graceMs) {
    if (startOn && msSinceArm < startCapMs) return SC_FAST_PH_START;
    if (nDone > 0 && nBlock == 0 && !openAny && msSinceLastClose >= graceMs) return SC_FAST_PH_END;
    return SC_FAST_PH_ATTACK;
}
// TASK #137616 (human #137208): per-chr phase by the yokai's OWN action ids (live trace 11-26, core 3B1AB: C53 spawn, no hit rows, 2.3-7.2 s
// -> C56 hit -> C54 two hits -> 1 end pose, no hit rows, 566 ms -> gone). An action "can hit" when its own data has a damaging row (timed or
// untimed) or a spawn event; read from the yokai's loaded action file, no per-core list. START = still in the action it spawned in, which
// cannot hit, it has never been in an action that can hit, and it is at least minAgeMs old and under capMs. END = it has been in an action
// that can hit, is now in a known action of its file that cannot hit, and none of its attack colliders is open. Anything else (unknown /
// unreadable action, an action that can hit, data unreadable) = ATTACK. Only no-hit actions are ever sped up, so damage timing never changes.
#define SC_FAST_ACT_UNKNOWN 0
#define SC_FAST_ACT_NOHIT 1
#define SC_FAST_ACT_HIT 2
inline int sc_fast_chr_phase(bool dataOk, int actKind, bool inSpawnAct, bool everHitAct, bool colliderOpen, int ageMs, int minAgeMs, int capMs,
                             float startMul, float endMul) {
    if (!dataOk || actKind != SC_FAST_ACT_NOHIT) return SC_FAST_PH_ATTACK;
    if (!everHitAct) return inSpawnAct && startMul > 1.f && ageMs >= minAgeMs && ageMs < capMs ? SC_FAST_PH_START : SC_FAST_PH_ATTACK;
    return endMul > 1.f && !colliderOpen ? SC_FAST_PH_END : SC_FAST_PH_ATTACK;
}
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
