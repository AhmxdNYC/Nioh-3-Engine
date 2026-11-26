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
    CHECK(sc_fast_speed_mul(0, 1.6f) == 1.6f && sc_fast_speed_mul(0, 0.f) == 1.f && sc_fast_speed_mul(0, 0.8f) == 1.f && sc_fast_speed_mul(20, 1.6f) == 2.f);
    printf(g_fail ? "soul_core_api_test: %d FAIL\n" : "soul_core_api_test: PASS%.0d\n", g_fail);
    return g_fail ? 1 : 0;
}
