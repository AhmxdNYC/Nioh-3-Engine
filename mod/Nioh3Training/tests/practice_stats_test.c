// user #22389: Practice stats decisions (practice_stats.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/practice_stats_test.c -o build/practice_stats_test.exe && build/practice_stats_test.exe
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/practice_stats.h"
int main(void) {
    PstCfg c; PstDefaults(&c);
    // defaults: everything OFF / 1.0x
    assert(c.v[PS_PLAYER_INF_KI] == 0 && c.v[PS_DMG_DEALT] == 100 && c.v[PS_DMG_TAKEN] == 100 && c.v[PS_BOSS_KI_DRAIN] == 0);
    // clamps
    assert(PstClamp(PS_PLAYER_INF_KI, 5) == 1 && PstClamp(PS_DMG_DEALT, -3) == 10 && PstClamp(PS_DMG_TAKEN, -3) == 0 && PstClamp(PS_DMG_TAKEN, 5000) == 1000);
    assert(PstClamp(PS_BOSS_KI_DRAIN, -1) == 0 && PstClamp(PS_BOSS_KI_DRAIN, 99) == 99 && PstClamp(PS_BOSS_KI_DRAIN, 5000) == 1000);
    // steps: Damage I deal x0.1 .. x10, Damage boss deals x0 .. x10, Ki drain 0 / 5 .. 1000 Ki per second, toggles
    assert(PstStep(PS_DMG_DEALT, 100, 1) == 125 && PstStep(PS_DMG_DEALT, 100, -1) == 75 && PstStep(PS_DMG_DEALT, 1000, 1) == 1000);
    assert(PstStep(PS_DMG_DEALT, 25, -1) == 10 && PstStep(PS_DMG_DEALT, 10, -1) == 10 && PstStep(PS_DMG_DEALT, 0, 0) == 10 /* never x0 */);
    assert(PstStep(PS_DMG_TAKEN, 10, -1) == 0 && PstStep(PS_DMG_TAKEN, 0, -1) == 0 && PstStep(PS_DMG_TAKEN, 0, 1) == 10 && PstStep(PS_DMG_TAKEN, 1000, 1) == 1000);
    assert(PstStep(PS_DMG_TAKEN, 130, -1) == 125 && PstStep(PS_DMG_TAKEN, 130, 1) == 150 && PstStep(PS_DMG_TAKEN, 130, 0) == 130);
    assert(PstStep(PS_BOSS_KI_DRAIN, 0, 1) == 5 && PstStep(PS_BOSS_KI_DRAIN, 5, -1) == 0 && PstStep(PS_BOSS_KI_DRAIN, 1000, 1) == 1000 && PstStep(PS_BOSS_KI_DRAIN, 40, -1) == 30);
    assert(PstStep(PS_PLAYER_INF_KI, 0, 0) == 1 && PstStep(PS_PLAYER_INF_KI, 1, 0) == 0 && PstStep(PS_PLAYER_INF_KI, 0, 1) == 1 && PstStep(PS_PLAYER_INF_KI, 1, -1) == 0);
    // damage scaling
    assert(PstScaleDmg(400, 100) == 400 && PstScaleDmg(400, 200) == 800 && PstScaleDmg(400, 50) == 200 && PstScaleDmg(400, 0) == 0);
    assert(PstScaleDmg(1, 10) == 1 /* never rounds a real hit to 0 */ && PstScaleDmg(5, 25) == 1 && PstScaleDmg(7, 50) == 4);
    assert(PstScaleDmg(0, 500) == 0 && PstScaleDmg(-30, 500) == -30 /* heals untouched */);
    assert(PstScaleDmg(0x7FFFFFFF, 1000) == 0x7FFFFFFF /* no overflow */);
    // boss Ki drain in Ki per second (live boss Ki 1040)
    float k = 0; const float K = 1040.0f;
    assert(!PstKiDrain(0, 50, 0, 0, 0, 0, 1000, K, &k));  // OFF
    assert(PstKiDrain(100, 50, 0, 0, 0, 0, 1000, K, &k) && k > 994.9f && k < 995.1f);  // 100 Ki/s for 50 ms = 5
    assert(PstKiDrain(100, 5000, 0, 0, 0, 0, 1000, K, &k) && k > 949.9f && k < 950.1f);  // hitch capped at 500 ms
    assert(PstKiDrain(50, 500, 0, 0, 0, 0, 10, K, &k) && k == 0.5f);  // floor 0.5, never a bare 0 break
    assert(PstKiDrain(5, 50, 0, 0, 0, 0, 10, 50, &k) && k > 9.74f && k < 9.76f);  // independent of max Ki
    assert(!PstKiDrain(50, 50, 0, 0, 0, 0, 0.5f, K, &k));  // already at the floor
    assert(!PstKiDrain(10, 50, 1, 0, 0, 0, 1000, K, &k) && !PstKiDrain(10, 50, 0, 1, 0, 0, 1000, K, &k) && !PstKiDrain(10, 50, 0, 0, 1, 0, 1000, K, &k));
    assert(!PstKiDrain(10, 50, 0, 0, 0, 1, 1000, K, &k));  // infinite Ki wins
    assert(!PstKiDrain(10, 50, 0, 0, 0, 0, 1000, 0, &k));
    // player infinite Ki
    assert(!PstPlayerKiRefill(0, 10, 100) && PstPlayerKiRefill(1, 10, 100) && !PstPlayerKiRefill(1, 100, 100) && !PstPlayerKiRefill(1, 99.8f, 100));
    assert(!PstPlayerKiRefill(1, 0, 0) && !PstPlayerKiRefill(1, 0, 1e9f) && !PstPlayerKiRefill(1, 0, -5));  // garbage read: no write
    // boss speed + infinite ninjutsu
    assert(c.v[PS_BOSS_SPEED] == 100 && c.v[PS_PLAYER_INF_NIN] == 0 && PS_N == 8);
    assert(PstClamp(PS_BOSS_SPEED, 0) == 10 && PstClamp(PS_BOSS_SPEED, 999) == 300 && PstClamp(PS_PLAYER_INF_NIN, 7) == 1);
    assert(PstStep(PS_BOSS_SPEED, 100, 1) == 125 && PstStep(PS_BOSS_SPEED, 100, -1) == 75 && PstStep(PS_BOSS_SPEED, 10, -1) == 10 && PstStep(PS_BOSS_SPEED, 300, 1) == 300);
    assert(PstStep(PS_PLAYER_INF_NIN, 0, 0) == 1 && PstStep(PS_PLAYER_INF_NIN, 1, 0) == 0);
    assert(PstSpeedScale(100, 1.0f) == 1.0f && PstSpeedScale(50, 1.0f) == 0.5f && PstSpeedScale(200, 0.75f) == 1.5f);
    assert(PstSpeedScale(50, 0.0f) == 0.0f /* hit-stop stays 0 */ && PstSpeedScale(50, -1.0f) == -1.0f && PstSpeedScale(50, 1e9f) == 1e9f);
    assert(PstNinjutsuFill(1, 10, 100) && !PstNinjutsuFill(0, 10, 100) && !PstNinjutsuFill(1, 100, 100) && !PstNinjutsuFill(1, 0, 0) && !PstNinjutsuFill(1, 0, 1e9f));
    assert(c.v[PS_PLAYER_SPEED] == 100 && PS_N == 8 && PstClamp(PS_PLAYER_SPEED, 0) == 10 && PstClamp(PS_PLAYER_SPEED, 999) == 300);
    assert(PstStep(PS_PLAYER_SPEED, 100, 1) == 125 && PstStep(PS_PLAYER_SPEED, 100, -1) == 75 && PstStep(PS_PLAYER_SPEED, 300, 1) == 300);
    // damage multiplier by attacker side: player hits -> dealt, Training boss hits -> boss, everyone else x1
    assert(PstDmgPct(1, 0, 0, 200, 50) == 200 && PstDmgPct(0, 1, 1, 200, 50) == 50 && PstDmgPct(0, 1, 0, 200, 50) == 50);
    assert(PstDmgPct(0, 0, 1, 200, 50) == 100 /* an ally / NPC / unknown attacker hitting the player */ && PstDmgPct(0, 0, 0, 200, 50) == 100);
    assert(PstDmgPct(1, 0, 1, 200, 50) == 100 /* the player hurting itself */ && PstDmgPct(1, 1, 0, 200, 0) == 200 /* player wins */);
    assert(PstScaleDmg(400, PstDmgPct(0, 1, 1, 100, 0)) == 0 && PstScaleDmg(400, PstDmgPct(1, 0, 0, 10, 100)) == 40);
    // frame meter toggle (default OFF)
    assert(c.v[PS_FRAME_METER] == 0 && PstDefault(PS_FRAME_METER) == 0 && PstIsToggle(PS_FRAME_METER) && !PstIsToggle(PS_BOSS_SPEED));
    assert(PstClamp(PS_FRAME_METER, 5) == 1 && PstClamp(PS_FRAME_METER, -1) == 0);
    assert(PstStep(PS_FRAME_METER, 0, 0) == 1 && PstStep(PS_FRAME_METER, 1, 0) == 0 && PstStep(PS_FRAME_METER, 0, 1) == 1 && PstStep(PS_FRAME_METER, 1, -1) == 0);
    puts("practice_stats_test: all passed");
    return 0;
}
