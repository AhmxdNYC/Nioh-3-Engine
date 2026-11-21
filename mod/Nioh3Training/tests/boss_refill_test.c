// human #127382: delayed infinite boss health / Ki refill (boss_vitals.h BvRefillDue / BvHpFloorDmg / BvKiFloorAmt), offline.
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/boss_vitals.h"
int main(void) {
    const uint64_t M = 51712; const float K = 1040.0f; const int D = 2000;
    BvTimer t; BvTimerReset(&t);
    // first read / no drop: refill may run at once (a boss already below the level when the toggle goes ON is not left low)
    assert(BvRefillDue(&t, (double)M, 1000, D, 0.5));
    // hits keep it down: every drop restarts the wait, so no refill while the player keeps hitting
    assert(!BvRefillDue(&t, 50000.0, 1050, D, 0.5));
    assert(!BvRefillDue(&t, 50000.0, 2000, D, 0.5));  // 950 ms after the hit
    assert(!BvRefillDue(&t, 47000.0, 2900, D, 0.5));  // new hit at 2900 resets the timer
    assert(!BvRefillDue(&t, 47000.0, 4850, D, 0.5));  // 1950 ms after the last hit: still waiting (2000 after the first one)
    assert(BvRefillDue(&t, 47000.0, 4900, D, 0.5));   // 2000 ms quiet: refill now
    assert(BvRefillDue(&t, (double)M, 4950, D, 0.5)); // our own refill raises the value: not a hit
    assert(BvRefillDue(&t, (double)M, 5000, D, 0.5));
    // a rise (native Ki regen) is never a hit; a drop below eps is noise
    BvTimer k; BvTimerReset(&k);
    assert(BvRefillDue(&k, 500.0, 0, D, 0.01) && BvRefillDue(&k, 520.0, 50, D, 0.01) && BvRefillDue(&k, 519.995, 100, D, 0.01));
    assert(!BvRefillDue(&k, 400.0, 150, D, 0.01));
    // HP and Ki timers are independent: a Ki hit does not hold back the HP refill
    BvTimer h; BvTimerReset(&h);
    assert(BvRefillDue(&h, 30000.0, 0, D, 0.5) && BvRefillDue(&h, 30000.0, 200, D, 0.5));
    assert(!BvRefillDue(&k, 300.0, 200, D, 0.01));
    // delay 0 = the old instant refill: due every tick even right after a hit
    BvTimer z; BvTimerReset(&z);
    assert(BvRefillDue(&z, (double)M, 0, 0, 0.5) && BvRefillDue(&z, 100.0, 50, 0, 0.5) && BvRefillDue(&z, 50.0, 100, 0, 0.5));
    // GetTickCount wrap: elapsed is unsigned
    BvTimer w; BvTimerReset(&w);
    BvRefillDue(&w, 100.0, 0xFFFFFF00u, D, 0.5);
    assert(!BvRefillDue(&w, 90.0, 0xFFFFFF80u, D, 0.5) && !BvRefillDue(&w, 90.0, 0x00000100u, D, 0.5) && BvRefillDue(&w, 90.0, 0x00000800u, D, 0.5));
    // end to end with BvHpHold: down while hit, back to the level after the delay
    BvCfg c; BvDefaults(&c); c.infHp = 1; uint64_t out = 0; BvTimerReset(&h);
    assert(BvRefillDue(&h, (double)M, 0, D, 0.5) && !BvHpHold(&c, 0, 0, M, M, &out));
    assert(!(BvRefillDue(&h, 40000.0, 50, D, 0.5) && BvHpHold(&c, 0, 0, 40000, M, &out)));  // 40000 stays on the bar
    assert(BvRefillDue(&h, 40000.0, 2050, D, 0.5) && BvHpHold(&c, 0, 0, 40000, M, &out) && out == M);
    // floor: the boss never dies while the refill waits
    assert(BvFloorOn(1, D) && !BvFloorOn(0, D) && !BvFloorOn(1, 0));
    assert(BvHpFloorDmg(1, 5000, 1, 300) == 300);       // normal hit
    assert(BvHpFloorDmg(1, 5000, 1, 4999) == 4999);     // lands exactly on 1 HP
    assert(BvHpFloorDmg(1, 5000, 1, 9000) == 4999);     // lethal hit stops at 1 HP
    assert(BvHpFloorDmg(1, 1, 1, 500) == 0 && BvHpFloorDmg(1, 0, 1, 500) == 0);  // at / below the floor: no more HP lost
    assert(BvHpFloorDmg(1, 12000, 10343, 5000) == 1657);  // NOFIN reserve 10342: real floor 10343 = shown 1
    assert(BvHpFloorDmg(0, 5000, 1, 9000) == 9000 && BvHpFloorDmg(BvFloorOn(1, 0), 5000, 1, 9000) == 9000);  // OFF / delay 0: untouched
    assert(BvHpFloorDmg(1, 5000, 1, 0) == 0 && BvHpFloorDmg(1, 5000, 1, -20) == -20);  // heals / zero hits pass
    // Ki: hits smaller than the Ki level stop at 0.5 (no break); a hit >= the level breaks as with the instant refill
    const float L = BvKiTarget(K, 100);
    assert(BvKiFloorAmt(1, 600.0f, L, 100.0f) == 100.0f);
    assert(BvKiFloorAmt(1, 100.0f, L, 300.0f) == 99.5f);   // 100 - 99.5 = 0.5
    assert(BvKiFloorAmt(1, 0.5f, L, 300.0f) == 0.0f);      // already at the floor: stays there
    assert(BvKiFloorAmt(1, 0.3f, L, 300.0f) == 0.0f);
    assert(BvKiFloorAmt(1, 100.0f, L, 1040.0f) == 1040.0f);  // a full-level hit still breaks it
    assert(BvKiFloorAmt(1, 0.0f, L, 50.0f) == 50.0f);      // Ki 0 (broken): the game's
    const float L0 = BvKiTarget(K, 0);                      // Ki 0 % level = 0.5: any real hit (>= 0.5) breaks, as before
    assert(BvKiFloorAmt(1, 0.5f, L0, 5.0f) == 5.0f && BvKiFloorAmt(1, 0.5f, L0, 0.2f) == 0.0f);
    const float L50 = BvKiTarget(K, 50);                    // 520: a 600 hit breaks, a 300 hit at Ki 200 stops at 0.5
    assert(BvKiFloorAmt(1, 200.0f, L50, 600.0f) == 600.0f && BvKiFloorAmt(1, 200.0f, L50, 300.0f) == 199.5f);
    assert(BvKiFloorAmt(0, 100.0f, L, 300.0f) == 300.0f && BvKiFloorAmt(BvFloorOn(1, 0), 100.0f, L, 300.0f) == 300.0f);
    puts("PASS: boss refill delay: hits keep HP / Ki down, floor holds, refill after the delay, a new hit resets the timer, delay 0 = instant (#127382)");
    return 0;
}
