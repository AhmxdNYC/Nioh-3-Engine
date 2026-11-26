#include <cmath>
// Offline test for mod/shared/soul_core_api.h (TASK #136197). No asserts (NDEBUG-safe): counts failures, exit 1 on any.
#include "../../shared/soul_core_api.h"
#include <wchar.h>
#include <string.h>
static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++g_fail; } } while (0)
int main() {
    CHECK(SC_CALL_N == 4);  // TASK #136502: + Fast summon
    CHECK(sc_call_name(SC_CALL_GAME) && sc_call_name(SC_CALL_TRANSFORM) && sc_call_name(SC_CALL_TRANSFORM_MOVE) && sc_call_name(SC_CALL_FAST_SUMMON));
    for (int c = 0; c < SC_CALL_N; ++c) CHECK(!strstr(sc_call_name(c), "moveable"));  // TASK #136987
    CHECK(sc_call_name(4) == 0 && sc_call_name(-1) == 0);
    // menu step wraps both ways
    CHECK(sc_call_step(SC_CALL_GAME, 1) == SC_CALL_TRANSFORM);
    CHECK(sc_call_step(SC_CALL_TRANSFORM, 1) == SC_CALL_FAST_SUMMON);  // TASK #136987: no Transform moveable stop
    CHECK(sc_call_step(SC_CALL_TRANSFORM_MOVE, 1) == SC_CALL_FAST_SUMMON);  // retired 2 steps like Transform
    CHECK(sc_call_step(SC_CALL_FAST_SUMMON, -1) == SC_CALL_TRANSFORM && sc_call_step(SC_CALL_TRANSFORM, -1) == SC_CALL_GAME);
    for (int c = 0; c < SC_CALL_N; ++c) for (int d = -1; d <= 1; ++d) CHECK(sc_call_step(c, d) != SC_CALL_TRANSFORM_MOVE);
    CHECK(sc_call_norm(SC_CALL_TRANSFORM_MOVE) == SC_CALL_TRANSFORM && sc_call_norm(SC_CALL_FAST_SUMMON) == SC_CALL_FAST_SUMMON);
    CHECK(sc_call_step(SC_CALL_FAST_SUMMON, 1) == SC_CALL_GAME);
    CHECK(sc_call_step(SC_CALL_GAME, -1) == SC_CALL_FAST_SUMMON);
    CHECK(sc_call_step(SC_CALL_TRANSFORM, 0) == SC_CALL_FAST_SUMMON);  // A = next
    // TASK #136987: every Transform use may steer (detection decides); the global switch no longer gates it
    CHECK(sc_call_moveable(SC_CALL_TRANSFORM_MOVE, false));
    CHECK(sc_call_moveable(SC_CALL_TRANSFORM, false) && sc_call_moveable(SC_CALL_TRANSFORM, true));
    CHECK(!sc_call_moveable(SC_CALL_GAME, true));
    CHECK(!sc_call_moveable(SC_CALL_FAST_SUMMON, true));  // TASK #136502: the game's own summon never steers
    CHECK(sc_call_game_path(SC_CALL_FAST_SUMMON) && sc_call_game_path(SC_CALL_GAME) && !sc_call_game_path(SC_CALL_TRANSFORM) && !sc_call_forms(SC_CALL_FAST_SUMMON));
    CHECK(sc_call_moveable(-1, true) && sc_call_moveable(-1, false));
    // table: same path for any id
    ScCallTable t; sc_table_clear(&t);
    CHECK(sc_table_get(&t, 0x5129B) == -1);
    CHECK(!sc_table_set(&t, 0, 1));
    CHECK(sc_table_set(&t, 0x5129B, SC_CALL_TRANSFORM_MOVE));
    CHECK(sc_table_set(&t, 0x5216E, SC_CALL_TRANSFORM));
    CHECK(!sc_table_set(&t, 0x1, 7));
    CHECK(sc_table_get(&t, 0x5129B) == 1 && sc_table_get(&t, 0x5216E) == 1 && t.n == 2);  // TASK #136987: 2 stores as 1
    CHECK(sc_table_set(&t, 0x5129B, SC_CALL_TRANSFORM) && sc_table_get(&t, 0x5129B) == 1 && t.n == 2);
    wchar_t b[256];
    sc_table_format(&t, b, 256);
    CHECK(wcscmp(b, L"5129B:1,5216E:1") == 0);
    CHECK(sc_table_set(&t, 0x5129B, -1) && t.n == 1 && sc_table_get(&t, 0x5129B) == -1);
    // round trip + junk + future call values skipped
    ScCallTable u;
    CHECK(sc_table_parse(&u, L"5129B:2, 5216E:1,zz,ABC:9,0:1,DEAD:0,,77:,80D5F:3") == 4);
    CHECK(sc_table_get(&u, 0x80D5F) == SC_CALL_FAST_SUMMON);
    CHECK(sc_table_get(&u, 0x5129B) == 1 && sc_table_get(&u, 0x5216E) == 1 && sc_table_get(&u, 0xDEAD) == 0 && sc_table_get(&u, 0xABC) == -1);
    sc_table_format(&u, b, 256);
    ScCallTable w; sc_table_parse(&w, b);
    CHECK(w.n == u.n);
    for (int i = 0; i < u.n; ++i) CHECK(sc_table_get(&w, u.id[i]) == u.call[i]);
    // capacity + small buffer never overflows
    sc_table_clear(&t);
    for (uint32_t i = 1; i <= SC_TABLE_MAX; ++i) CHECK(sc_table_set(&t, i, 1));
    CHECK(!sc_table_set(&t, 0xFFFF, 1));
    wchar_t s[12]; s[11] = L'Q';
    sc_table_format(&t, s, 11);
    CHECK(s[11] == L'Q' && wcslen(s) < 11);
    // TASK #136492: one move sensitivity for every moveable core, x1..x8, menu default x5
    CHECK(SC_API_VERSION >= 2 && SC_MOVE_SENS_DEFAULT == 5);
    CHECK(sc_move_sens_clamp(0) == 1 && sc_move_sens_clamp(-7) == 1 && sc_move_sens_clamp(1) == 1);
    CHECK(sc_move_sens_clamp(5) == 5 && sc_move_sens_clamp(8) == 8 && sc_move_sens_clamp(99) == 8);
    // TASK #136502: Fast summon speed x10; 0 = the Transform speed (SealSpeed), else x1.0..x3.0
    CHECK(SC_API_VERSION == 4 && SC_FAST_SPEED_DEFAULT == 0);
    CHECK(sc_fast_speed_clamp(0) == 0 && sc_fast_speed_clamp(-3) == 0 && sc_fast_speed_clamp(5) == 10 && sc_fast_speed_clamp(16) == 16 && sc_fast_speed_clamp(99) == 30);
    CHECK(fabsf(sc_fast_speed_mul(0, 1.6f) - 1.44f) < 1e-4f && sc_fast_speed_mul(0, 1.05f) == 1.f && sc_fast_speed_mul(0, 0.f) == 1.f && sc_fast_speed_mul(0, 0.8f) == 1.f && sc_fast_speed_mul(20, 1.6f) == 2.f);
    // TASK #137323: Fast summon startup / end pose phases (pure logic; every core the same)
    CHECK(sc_fast_phase_mul(SC_FAST_PH_START, 1.44f, 6.f, 6.f) == 6.f && sc_fast_phase_mul(SC_FAST_PH_ATTACK, 1.44f, 6.f, 6.f) == 1.44f);
    CHECK(sc_fast_phase_mul(SC_FAST_PH_END, 1.44f, 6.f, 4.f) == 4.f);
    CHECK(sc_fast_phase_mul(SC_FAST_PH_START, 1.44f, 1.f, 6.f) == 1.44f && sc_fast_phase_mul(SC_FAST_PH_END, 1.44f, 6.f, 0.f) == 1.44f);  // <= 1 = off
    CHECK(sc_fast_edge_clamp(0.f) == 1.f && sc_fast_edge_clamp(-2.f) == 1.f && sc_fast_edge_clamp(6.f) == 6.f && sc_fast_edge_clamp(99.f) == SC_FAST_EDGE_MAX);
    CHECK(SC_FAST_EDGE_DEFAULT == 6.f && !strcmp(sc_fast_phase_name(SC_FAST_PH_START), "start") && !strcmp(sc_fast_phase_name(SC_FAST_PH_END), "end"));
    // first hit: chain 1 at f20, lead 4 frames
    CHECK(!sc_fast_at_first(0, 1, 99.f, 20, 4));    // still in the appear action
    CHECK(!sc_fast_at_first(1, 1, 15.f, 20, 4) && sc_fast_at_first(1, 1, 16.f, 20, 4) && sc_fast_at_first(2, 1, 0.f, 20, 4));
    CHECK(!sc_fast_at_first(-1, 1, 99.f, 20, 4) && !sc_fast_at_first(-2, 1, 99.f, 20, 4) && !sc_fast_at_first(1, -1, 99.f, 20, 4));
    // last hit: chain 2 ends f45
    CHECK(!sc_fast_past_last(1, 2, 99.f, 45) && !sc_fast_past_last(2, 2, 45.f, 45) && sc_fast_past_last(2, 2, 46.f, 45));
    CHECK(sc_fast_past_last(3, 2, 0.f, 45) && sc_fast_past_last(-1, 2, 0.f, 45) && !sc_fast_past_last(-2, 2, 99.f, 45) && !sc_fast_past_last(0, -1, 99.f, 45));
    // per chr end state
    // TASK #137616: per-chr phase by its own action ids (3B1AB trace: C53 spawn no-hit -> C56/C54 hit -> 1 no-hit end pose)
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, true, false, true, 300, 200, 4000, 6.f, 6.f) == SC_FAST_PH_START);   // C53, its appearance collider ignored
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, true, false, false, 100, 200, 4000, 6.f, 6.f) == SC_FAST_PH_ATTACK);  // spawn frames untouched
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, true, false, false, 4000, 200, 4000, 6.f, 6.f) == SC_FAST_PH_ATTACK); // startup cap
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, false, false, false, 900, 200, 4000, 6.f, 6.f) == SC_FAST_PH_ATTACK); // a no-hit action before any attack, not the spawn one
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_HIT, false, true, false, 900, 200, 4000, 6.f, 6.f) == SC_FAST_PH_ATTACK);    // C54: attack speed
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, false, true, false, 9000, 200, 4000, 6.f, 6.f) == SC_FAST_PH_END);    // action 1 after the hits
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, false, true, true, 9000, 200, 4000, 6.f, 6.f) == SC_FAST_PH_ATTACK);  // a collider still open
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_UNKNOWN, false, true, false, 9000, 200, 4000, 6.f, 6.f) == SC_FAST_PH_ATTACK);
    CHECK(sc_fast_chr_phase(false, SC_FAST_ACT_NOHIT, true, false, false, 300, 200, 4000, 6.f, 6.f) == SC_FAST_PH_ATTACK);  // data unreadable
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, true, false, false, 300, 200, 4000, 1.f, 6.f) == SC_FAST_PH_ATTACK);   // start off
    CHECK(sc_fast_chr_phase(true, SC_FAST_ACT_NOHIT, false, true, false, 9000, 200, 4000, 6.f, 1.f) == SC_FAST_PH_ATTACK);  // end off
    CHECK(sc_fast_chr_end(true, 2, 0, 0, false, true, 2, false, true) == SC_FAST_CHR_DONE);
    CHECK(sc_fast_chr_end(true, 2, 0, 0, false, true, 1, false, true) == SC_FAST_CHR_BLOCK);   // second hit not seen yet
    CHECK(sc_fast_chr_end(true, 2, 0, 0, false, true, 2, true, true) == SC_FAST_CHR_BLOCK);    // a hit still open
    CHECK(sc_fast_chr_end(true, 2, 0, 0, false, true, 3, false, false) == SC_FAST_CHR_BLOCK);  // live over-count, data says not past the last
    CHECK(sc_fast_chr_end(true, 2, 0, 1, false, true, 2, false, true) == SC_FAST_CHR_BLOCK);   // spawn event: not certain, end left alone
    CHECK(sc_fast_chr_end(true, 2, 1, 0, false, true, 2, false, true) == SC_FAST_CHR_BLOCK);   // untimed row
    CHECK(sc_fast_chr_end(true, 2, 0, 0, true, true, 2, false, true) == SC_FAST_CHR_BLOCK);    // chain cut at the max
    CHECK(sc_fast_chr_end(false, 0, 0, 0, false, false, 0, false, false) == SC_FAST_CHR_BLOCK);  // unread
    CHECK(sc_fast_chr_end(true, 0, 0, 0, false, false, 0, false, false) == SC_FAST_CHR_INERT);   // body part, no hit rows
    CHECK(sc_fast_chr_end(true, 0, 0, 0, false, true, 1, false, true) == SC_FAST_CHR_BLOCK);     // 'inert' part that hit: unknown
    CHECK(sc_fast_chr_end(true, 1, 0, 0, false, false, 0, false, false) == SC_FAST_CHR_BLOCK);   // hitter that has not hit yet
    // press phase
    CHECK(sc_fast_group_phase(true, 100, 2000, 0, 0, false, 1 << 30, 150) == SC_FAST_PH_START);
    CHECK(sc_fast_group_phase(true, 2000, 2000, 0, 0, false, 1 << 30, 150) == SC_FAST_PH_ATTACK);  // startup cap
    CHECK(sc_fast_group_phase(false, 500, 2000, 0, 1, true, 1 << 30, 150) == SC_FAST_PH_ATTACK);
    CHECK(sc_fast_group_phase(false, 900, 2000, 1, 0, false, 100, 150) == SC_FAST_PH_ATTACK);        // grace
    CHECK(sc_fast_group_phase(false, 900, 2000, 1, 0, false, 150, 150) == SC_FAST_PH_END);
    CHECK(sc_fast_group_phase(false, 900, 2000, 1, 1, false, 500, 150) == SC_FAST_PH_ATTACK);        // a helper still may hit
    CHECK(sc_fast_group_phase(false, 900, 2000, 1, 0, true, 500, 150) == SC_FAST_PH_ATTACK);         // something open
    CHECK(sc_fast_group_phase(false, 900, 2000, 0, 0, false, 500, 150) == SC_FAST_PH_ATTACK);        // only inert chrs: no end skip
    CHECK(sc_fast_group_phase(false, 0, 2000, 0, 0, false, 1 << 30, 150) == SC_FAST_PH_ATTACK);  // not armed: attack speed from the spawn frame
    // arm: certain data only, before the first hit, old enough, speed on (live 3B1AB: C53:0 > 1:0, not certain -> never armed)
    CHECK(!sc_fast_start_arm(false, 0, -1, 0.f, 0, 4, 500, 200, 6.f));      // 3B1AB-like data: no timed hit, not certain
    CHECK(!sc_fast_start_arm(true, 0, 1, 0.f, 20, 4, 0, 200, 6.f));         // spawn frame: never
    CHECK(!sc_fast_start_arm(true, 0, 1, 0.f, 20, 4, 199, 200, 6.f) && sc_fast_start_arm(true, 0, 1, 0.f, 20, 4, 200, 200, 6.f));
    CHECK(!sc_fast_start_arm(true, 0, 1, 0.f, 20, 4, 500, 200, 1.f));       // FastSummonStartSpeed off
    CHECK(!sc_fast_start_arm(true, 1, 1, 17.f, 20, 4, 500, 200, 6.f) && sc_fast_start_arm(true, 1, 1, 15.f, 20, 4, 500, 200, 6.f));  // at first hit - lead: no
    CHECK(!sc_fast_start_arm(true, -1, 1, 0.f, 20, 4, 500, 200, 6.f) && !sc_fast_start_arm(true, -2, 1, 0.f, 20, 4, 500, 200, 6.f) && !sc_fast_start_arm(true, 2, 1, 0.f, 20, 4, 500, 200, 6.f));
    // appearance collider (before the data's first hit) is not a hit; no usable data -> counted
    CHECK(!sc_fast_hit_counts(true, 0, 1, 5.f, 20, 4) && sc_fast_hit_counts(true, 1, 1, 18.f, 20, 4) && sc_fast_hit_counts(true, -1, 1, 0.f, 20, 4) && sc_fast_hit_counts(false, 0, -1, 0.f, 0, 4));
    // keys 1/1 (off): every phase gives the 645efbf5 rate
    for (int p = 0; p < 3; ++p) CHECK(sc_fast_phase_mul(p, 1.44f, 1.f, 1.f) == 1.44f);
    printf(g_fail ? "soul_core_api_test: %d FAIL\n" : "soul_core_api_test: PASS%.0d\n", g_fail);
    return g_fail ? 1 : 0;
}
