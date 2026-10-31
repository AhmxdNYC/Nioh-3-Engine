// user #22389: Practice stats decisions (practice_stats.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/practice_stats_test.c -o build/practice_stats_test.exe && build/practice_stats_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/practice_stats.h"
int main(void) {
    PstCfg c; PstDefaults(&c);
    assert(PS_FRAME_SIZE==34 && c.v[PS_FRAME_SIZE]==0 && !PstIsToggle(PS_FRAME_SIZE));
    assert(PstClamp(PS_FRAME_SIZE,-5)==0 && PstClamp(PS_FRAME_SIZE,9)==2);
    for(int i=0;i<3;++i)assert(PstFromIni(PS_FRAME_SIZE,PstToIni(PS_FRAME_SIZE,i))==i);
    assert(PstStep(PS_FRAME_SIZE,0,1)==1 && PstStep(PS_FRAME_SIZE,1,1)==2);
    assert(PstStep(PS_FRAME_SIZE,2,1)==2 && PstStep(PS_FRAME_SIZE,0,-1)==0 && PstStep(PS_FRAME_SIZE,2,0)==0);
    assert(PS_FRAME_STYLE==33 && c.v[PS_FRAME_STYLE]==0);
    assert(PstStep(PS_FRAME_STYLE,0,1)==1 && PstStep(PS_FRAME_STYLE,1,-1)==0);
    assert(PstStep(PS_FRAME_STYLE,0,0)==1 && PstClamp(PS_FRAME_STYLE,9)==1);
    assert(PstFromIni(PS_FRAME_STYLE,PstToIni(PS_FRAME_STYLE,1))==1);
    assert(PS_FRAME_KI==20 && c.v[PS_FRAME_KI]==1 && PstIsToggle(PS_FRAME_KI));
    assert(PS_KI_WINDOW==21 && c.v[PS_KI_WINDOW]==0 && PstIsToggle(PS_KI_WINDOW));
    assert(PstStep(PS_KI_WINDOW,1,0)==0 && PstStep(PS_KI_WINDOW,0,0)==1 && PstClamp(PS_KI_WINDOW,9)==1);
    assert(PstStep(PS_FRAME_KI,1,0)==0 && PstStep(PS_FRAME_KI,0,0)==1);
    assert(PS_FRAME_LEGEND==18 && c.v[PS_FRAME_LEGEND]==1);
    assert(PstIsToggle(PS_FRAME_LEGEND) && PstClamp(PS_FRAME_LEGEND,9)==1 && PstStep(PS_FRAME_LEGEND,1,0)==0 && PstStep(PS_FRAME_LEGEND,0,0)==1);
    assert(PS_FRAME_READOUT==19 && c.v[PS_FRAME_READOUT]==1 && PstIsToggle(PS_FRAME_READOUT));
    assert(PstStep(PS_FRAME_READOUT,0,0)==1 && PstStep(PS_FRAME_READOUT,1,0)==0);
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
    assert(c.v[PS_BOSS_SPEED] == 100 && c.v[PS_PLAYER_INF_NIN] == 0 && PS_N == 36);
    assert(PstClamp(PS_BOSS_SPEED, 0) == 10 && PstClamp(PS_BOSS_SPEED, 999) == 300 && PstClamp(PS_PLAYER_INF_NIN, 7) == 1);
    assert(PstStep(PS_BOSS_SPEED, 100, 1) == 125 && PstStep(PS_BOSS_SPEED, 100, -1) == 75 && PstStep(PS_BOSS_SPEED, 10, -1) == 10 && PstStep(PS_BOSS_SPEED, 300, 1) == 300);
    assert(PstStep(PS_PLAYER_INF_NIN, 0, 0) == 1 && PstStep(PS_PLAYER_INF_NIN, 1, 0) == 0);
    assert(PstSpeedScale(100, 1.0f) == 1.0f && PstSpeedScale(50, 1.0f) == 0.5f && PstSpeedScale(200, 0.75f) == 1.5f);
    assert(PstSpeedScale(50, 0.0f) == 0.0f /* hit-stop stays 0 */ && PstSpeedScale(50, -1.0f) == -1.0f && PstSpeedScale(50, 1e9f) == 1e9f);
    assert(PstNinjutsuFill(1, 10, 100) && !PstNinjutsuFill(0, 10, 100) && !PstNinjutsuFill(1, 100, 100) && !PstNinjutsuFill(1, 0, 0) && !PstNinjutsuFill(1, 0, 1e9f));
    assert(c.v[PS_PLAYER_SPEED] == 100 && PS_N == 36 && PstClamp(PS_PLAYER_SPEED, 0) == 10 && PstClamp(PS_PLAYER_SPEED, 999) == 300);
    // pc #57367 infinite soul cores: an OFF-by-default 0/1 toggle
    assert(PS_PLAYER_INF_SOUL == 35 && c.v[PS_PLAYER_INF_SOUL] == 0 && PstIsToggle(PS_PLAYER_INF_SOUL) && PstClamp(PS_PLAYER_INF_SOUL, 5) == 1 && PstStep(PS_PLAYER_INF_SOUL, 0, 0) == 1);
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
    // user #22588 Ki damage I deal (x0 .. x10, default x1) and One-hit Ki break (toggle, default OFF)
    assert(c.v[PS_KI_DEALT] == 100 && c.v[PS_ONE_HIT_KI] == 0 && PstDefault(PS_KI_DEALT) == 100 && PstDefault(PS_ONE_HIT_KI) == 0);
    assert(PstIsToggle(PS_ONE_HIT_KI) && !PstIsToggle(PS_KI_DEALT));
    assert(PstClamp(PS_KI_DEALT, -5) == 0 && PstClamp(PS_KI_DEALT, 5000) == 1000 && PstClamp(PS_ONE_HIT_KI, 9) == 1 && PstClamp(PS_ONE_HIT_KI, -1) == 0);
    assert(PstStep(PS_KI_DEALT, 100, 1) == 125 && PstStep(PS_KI_DEALT, 10, -1) == 0 && PstStep(PS_KI_DEALT, 0, -1) == 0 && PstStep(PS_KI_DEALT, 1000, 1) == 1000);
    assert(PstStep(PS_ONE_HIT_KI, 0, 0) == 1 && PstStep(PS_ONE_HIT_KI, 1, 0) == 0 && PstStep(PS_ONE_HIT_KI, 0, 1) == 1 && PstStep(PS_ONE_HIT_KI, 1, -1) == 0);
    assert(PstScaleKi(40.0f, 100) == 40.0f && PstScaleKi(40.0f, 200) == 80.0f && PstScaleKi(40.0f, 50) == 20.0f && PstScaleKi(40.0f, 0) == 0.0f);
    assert(PstScaleKi(40.0f, 1000) == 400.0f && PstScaleKi(40.0f, 5000) == 400.0f /* clamped x10 */);
    assert(PstScaleKi(0.0f, 500) == 0.0f && PstScaleKi(-10.0f, 500) == -10.0f /* a Ki gain is untouched */);
    // sides: only the player's hits on someone else; the boss hitting the player / allies untouched
    assert(PstKiPct(1, 0, 0, 100, 100, 100) == 100 && PstKiPct(1, 0, 0, 200, 150, 100) == 300 && PstKiPct(1, 0, 1, 200, 150, 100) == 100 /* self */);
    assert(PstKiPct(1, 0, 0, 1000, 1000, 100) == 1000 /* capped */ && PstKiPct(0, 1, 1, 100, 100, 50) == 50 && PstKiPct(0, 1, 0, 100, 100, 50) == 100 && PstKiPct(0, 0, 1, 100, 100, 50) == 100);
    assert(PstKiDmg(1, 0, 1, 200, 0, 0, 500, 40) == 80.0f && PstKiDmg(1, 0, 0, 200, 0, 0, 500, 40) == 80.0f /* any non-player victim, like Damage I deal */);
    assert(PstKiDmg(0, 1, 0, 200, 1, 0, 500, 40) == 40.0f /* boss hits the player */ && PstKiDmg(1, 1, 0, 0, 1, 0, 500, 40) == 40.0f /* player hurts itself */);
    assert(PstKiDmg(0, 0, 1, 0, 1, 0, 500, 40) == 40.0f /* an ally hits the boss */ && PstKiDmg(1, 0, 1, 100, 0, 0, 500, 40) == 40.0f /* defaults */);
    // one-hit Ki break: the same hit takes the Ki from > 0 to <= 0 (cur - amount = -max(amount, 1))
    { const float cur = 1040.0f, r = PstKiDmg(1, 0, 1, 100, 1, 0, cur, 40); assert(r == 1080.0f && cur - r <= 0.0f); }
    { const float r = PstKiDmg(1, 0, 1, 0, 1, 0, 300, 40); assert(r == 301.0f && 300 - r < 0.0f); }  // x0 Ki damage still breaks (empties to -1)
    { const float r = PstKiDmg(1, 0, 1, 100, 1, 0, 0.5f, 0.0f); assert(r == 1.5f && 0.5f - r <= 0.0f); }  // a zero-Ki hit still breaks
    assert(PstKiDmg(1, 0, 1, 100, 1, 0, 30, 40) == 40.0f /* already breaks natively: untouched */ && PstKiDmg(1, 0, 1, 100, 1, 0, 40, 40) == 40.0f);
    assert(PstKiDmg(1, 0, 1, 200, 1, 0, 60, 40) == 80.0f /* the scaled hit already breaks */);
    assert(PstKiDmg(1, 0, 0, 100, 1, 0, 500, 40) == 40.0f /* not a Training boss */ && PstKiDmg(1, 0, 1, 100, 1, 1, 500, 40) == 40.0f /* blocked: broken / inf Ki / dead / window */);
    assert(PstKiDmg(1, 0, 1, 100, 1, 0, 0.0f, 40) == 40.0f && PstKiDmg(1, 0, 1, 100, 1, 0, -50.0f, 40) == 40.0f /* already out of Ki */);
    assert(PstKiDmg(1, 0, 1, 100, 1, 0, 1e9f, 40) == 40.0f /* garbage Ki read */ && PstKiDmg(1, 0, 1, 100, 1, 0, 500, -5) == -5.0f /* Ki gain */);
    { const float n = 0.0f / 0.0f; const float r = PstKiDmg(1, 0, 1, 200, 1, 0, 500, n); assert(r != r); }  // NaN passes through
    // user #22918 Hold Ki break: toggle, default OFF, saved key slot after PlayerFrameMeter
    assert(c.v[PS_HOLD_KI_BREAK] == 0 && PstDefault(PS_HOLD_KI_BREAK) == 0 && PstIsToggle(PS_HOLD_KI_BREAK) && PS_HOLD_KI_BREAK == 11);
    assert(PstClamp(PS_HOLD_KI_BREAK, 7) == 1 && PstClamp(PS_HOLD_KI_BREAK, -3) == 0);
    assert(PstStep(PS_HOLD_KI_BREAK, 0, 0) == 1 && PstStep(PS_HOLD_KI_BREAK, 1, 0) == 0 && PstStep(PS_HOLD_KI_BREAK, 0, 1) == 1 && PstStep(PS_HOLD_KI_BREAK, 1, -1) == 0);
    // step: start only on a natural break of a live, alive boss with the toggle ON
    assert(PstHoldBreakStep(1, 1, 0, 1, 0) == PS_HB_START);
    assert(PstHoldBreakStep(0, 1, 0, 1, 0) == PS_HB_NONE /* toggle OFF: native break untouched */ && PstHoldBreakStep(1, 1, 0, 0, 0) == PS_HB_NONE /* never forces a break */);
    assert(PstHoldBreakStep(1, 0, 0, 1, 0) == PS_HB_NONE /* no session / Training boss */ && PstHoldBreakStep(1, 1, 1, 1, 0) == PS_HB_NONE /* dead */);
    // held: keep while broken; release on OFF, death, session end, or the game ending the break (grapple)
    assert(PstHoldBreakStep(1, 1, 0, 1, 1) == PS_HB_KEEP);
    assert(PstHoldBreakStep(0, 1, 0, 1, 1) == PS_HB_RELEASE && PstHoldBreakStep(1, 1, 1, 1, 1) == PS_HB_RELEASE && PstHoldBreakStep(1, 0, 0, 1, 1) == PS_HB_RELEASE);
    assert(PstHoldBreakStep(1, 1, 0, 0, 1) == PS_HB_RELEASE && PstHoldBreakStep(1, 1, 0, 1, 0) == PS_HB_START /* the next break is held again */);
    // simulated hold: a 3.9 s native countdown (dt 1/60 s per frame, top-up at 20 Hz) never reaches 0 while ON; after OFF it runs out within the hold value
    {   float t = 3.9f, o = 0; int frame = 0, held = 1, recovered = -1;
        for (frame = 0; frame < 60 * 30; ++frame) {
            const int on = frame < 60 * 20;  // ON for 20 s, then OFF
            if (t > 0.0f) { t -= 1.0f / 60.0f; if (t <= 0.0f) { t = 0.0f; if (recovered < 0) recovered = frame; } }
            const int act = PstHoldBreakStep(on, 1, 0, t > 0.0f, held);
            held = act == PS_HB_START || act == PS_HB_KEEP;
            if (held && frame % 3 == 0 && PstHoldTimer(t, 1.0f, &o)) t = o;
        }
        assert(recovered > 60 * 20 && recovered <= 60 * 20 + 61 && !held);
    }
    // user #22918 live: one-hit break on an idle boss = Ki <= 0 only (23=0 exhausted 0): that must count as broken, and the Ki regen must be put back
    assert(PstHoldIsBroken(0, 0, -11.2f, 1872.0f, 0) && PstHoldIsBroken(0, 0, 0.0f, 1872.0f, 0) && PstHoldIsBroken(1, 0, 500.0f, 1872.0f, 0) && PstHoldIsBroken(0, 1, 500.0f, 1872.0f, 0));
    assert(!PstHoldIsBroken(0, 0, 2.2f, 1872.0f, 0) && !PstHoldIsBroken(0, 0, 0.0f, 0.0f, 0) /* unread max: not a break */ && !PstHoldIsBroken(0, 0, 0.0f, 1e9f, 0));
    assert(PstHoldIsBroken(0, 0, 2.2f, 1872.0f, 1) /* held: regen above 0 is still the hold */ && PstHoldIsBroken(0, 0, 400.0f, 1872.0f, 1) && !PstHoldIsBroken(0, 0, 500.0f, 1872.0f, 1) /* refill: released */);
    { float o = 7; assert(PstHoldKi(2.2f, -11.2f, &o) && o == -11.2f && PstHoldKi(2.2f, 0.0f, &o) && o == 0.0f && PstHoldKi(2.2f, 5.0f, &o) && o == 0.0f);
      o = 7; assert(!PstHoldKi(-3.0f, -11.2f, &o) && !PstHoldKi(0.0f, 0.0f, &o) && !PstHoldKi(1e9f, 0.0f, &o) && o == 7); }
    // simulated one-hit break: Ki -11.2, regen +45/s read at 20 Hz ticks. With the hold the boss is still held and at Ki <= 0 after 20 s (mirrors PstHoldBreak's order:
    // read Ki, step, clamp); without it Ki > 0 (recovered) within 0.4 s.
    {   float ki = -11.2f, low = 0.0f, o = 0; int held = 0, tick;
        for (tick = 0; tick < 20 * 20; ++tick) {
            ki += 45.0f / 20.0f;
            const int act = PstHoldBreakStep(1, 1, 0, PstHoldIsBroken(0, 0, ki, 1872.0f, held), held);
            if (act == PS_HB_START) { held = 1; low = ki < 0 ? ki : 0; } else if (act == PS_HB_RELEASE) held = 0;
            if (held) { if (ki < low) low = ki; if (PstHoldKi(ki, low, &o)) ki = o; }
            assert(held && ki <= 0.0f);
        }
        ki = -11.2f; int recovered = -1;  /* toggle OFF: the same break recovers */
        for (tick = 0; tick < 20 * 20 && recovered < 0; ++tick) { ki += 45.0f / 20.0f; if (ki > 0.0f) recovered = tick; }
        assert(recovered >= 0 && recovered <= 8);
        /* Reset fight refills Ki to max while held: the hold ends instead of pinning Ki back to 0 */
        held = 1; ki = 1872.0f; assert(PstHoldBreakStep(1, 1, 0, PstHoldIsBroken(0, 0, ki, 1872.0f, held), held) == PS_HB_RELEASE);
    }
    // timer top-up: only a running timer below the hold value; idle (0 / -1), garbage and already-long timers untouched
    { float o = 0; assert(PstHoldTimer(0.2f, 1.0f, &o) && o == 1.0f); }
    { float o = 7; assert(!PstHoldTimer(0.0f, 1.0f, &o) && !PstHoldTimer(-1.0f, 1.0f, &o) && !PstHoldTimer(3.5f, 1.0f, &o) && !PstHoldTimer(1.0f, 1.0f, &o) && o == 7); }
    { float o = 7; const float n = 0.0f / 0.0f; assert(!PstHoldTimer(n, 1.0f, &o) && !PstHoldTimer(1e9f, 1.0f, &o) && !PstHoldTimer(0.5f, 0.0f, &o) && o == 7); }
    // AI restore: only what the hold switched off, only if still off, never on a dead boss
    assert(PstHoldAiRestore(1, 0, 0) && !PstHoldAiRestore(0, 0, 0) && !PstHoldAiRestore(1, 1, 0) && !PstHoldAiRestore(1, 0, 1) && !PstHoldAiRestore(1, -1, 0));
    // user #22942 Time of day: default Stock (0 = no write), ini -1 = Stock, 30-min steps 00:00 .. 23:30, Stock at the left end
    assert(c.v[PS_TIME_OF_DAY] == 0 && c.v[PS_WEATHER] == 0 && PS_TIME_OF_DAY == 12 && PS_WEATHER == 13 && !PstIsToggle(PS_TIME_OF_DAY) && !PstIsToggle(PS_WEATHER));
    assert(PstToIni(PS_TIME_OF_DAY, PstDefault(PS_TIME_OF_DAY)) == -1 && PstFromIni(PS_TIME_OF_DAY, -1) == 0 && PstFromIni(PS_TIME_OF_DAY, -50) == 0);
    assert(PstFromIni(PS_TIME_OF_DAY, 0) == 1 && PstFromIni(PS_TIME_OF_DAY, 720) == 25 && PstFromIni(PS_TIME_OF_DAY, 1410) == 48 && PstFromIni(PS_TIME_OF_DAY, 9999) == 48);
    assert(PstFromIni(PS_TIME_OF_DAY, 745) == 25 /* rounds down to 12:00 */ && PstFromIni(PS_TIME_OF_DAY, 29) == 1);
    for (int v = 0; v <= PS_TOD_STEPS; ++v) assert(PstFromIni(PS_TIME_OF_DAY, PstToIni(PS_TIME_OF_DAY, v)) == v);  // ini round trip
    assert(PstToIni(PS_TIME_OF_DAY, 1) == 0 && PstToIni(PS_TIME_OF_DAY, 25) == 720 && PstToIni(PS_TIME_OF_DAY, 48) == 1410 && PstToIni(PS_TIME_OF_DAY, 99) == 1410);
    assert(PstToIni(PS_DMG_DEALT, 250) == 250 && PstFromIni(PS_DMG_DEALT, 5) == 10 && PstFromIni(PS_HOLD_KI_BREAK, 3) == 1);  // others: identity / clamp
    assert(PstClamp(PS_TIME_OF_DAY, -1) == 0 && PstClamp(PS_TIME_OF_DAY, 49) == 48 && PstClamp(PS_WEATHER, 6) == 5 && PstClamp(PS_WEATHER, -2) == 0);
    assert(PstStep(PS_TIME_OF_DAY, 0, 1) == 1 && PstStep(PS_TIME_OF_DAY, 1, -1) == 0 && PstStep(PS_TIME_OF_DAY, 0, -1) == 0 && PstStep(PS_TIME_OF_DAY, 48, 1) == 48);
    assert(PstStep(PS_TIME_OF_DAY, 25, 1) == 26 && PstStep(PS_TIME_OF_DAY, 25, 0) == 25);
    assert(PstTodMinutes(0) == -1 && PstTodFraction(0) < 0.0f && PstTodFraction(1) == 0.0f && PstTodFraction(25) == 0.5f);
    for (int v = 1; v <= PS_TOD_STEPS; ++v) { const float f = PstTodFraction(v); assert(f >= 0.0f && f < 1.0f); }  // SkylightDateTime stays in [0, 1)
    {   const float f = PstTodFraction(PS_TOD_STEPS); assert(f > 0.979f && f < 0.98f); }  // 23:30
    // Weather presets: Stock writes nothing; Clear / Overcast / Rain / Storm / Snow as in the plan
    assert(!strcmp(PstWeatherName(0), "Stock") && !strcmp(PstWeatherName(5), "Snow") && !strcmp(PstWeatherName(9), "?"));
    assert(PstEnvFieldOff(PS_EF_CLOUDY) == 0x7C && PstEnvFieldOff(PS_EF_PRECIP) == 0x88 && PstEnvFieldOff(PS_EF_RAINDROP) == 0x15C8 && PstEnvFieldOff(PS_EF_TREEWIND) == 0x1968 && PstEnvFieldOff(PS_EF_N) == 0);
    for (int f = 0; f < PS_EF_N; ++f) { float v = 7; assert(!PstWeatherValue(PS_WX_STOCK, f, &v) && v == 7 && !PstWeatherValue(PS_WX_N, f, &v)); }
    for (int f = 0; f < PS_EF_N; ++f) assert(PstEnvFieldOff(f) % 4 == 0 && PstEnvFieldOff(f) != 0xB0);  // aligned floats, never the time field
    { float v = 0;
      assert(PstWeatherValue(PS_WX_CLEAR, PS_EF_CLOUDY, &v) && v == 0.3f && PstWeatherValue(PS_WX_CLEAR, PS_EF_CLEAR, &v) && v == 1.0f && PstWeatherValue(PS_WX_CLEAR, PS_EF_PRECIP, &v) && v == 0.0f);
      assert(!PstWeatherValue(PS_WX_CLEAR, PS_EF_TEMP, &v) && !PstWeatherValue(PS_WX_CLEAR, PS_EF_GUST, &v) && !PstWeatherValue(PS_WX_CLEAR, PS_EF_HUMID, &v));
      assert(PstWeatherValue(PS_WX_OVERCAST, PS_EF_CLOUDY, &v) && v == 1.0f && PstWeatherValue(PS_WX_OVERCAST, PS_EF_HUMID, &v) && v == 0.7f && PstWeatherValue(PS_WX_OVERCAST, PS_EF_PRECIP, &v) && v == 0.0f);
      assert(PstWeatherValue(PS_WX_OVERCAST, PS_EF_CLEAR, &v) && v == 0.0f && !PstWeatherValue(PS_WX_OVERCAST, PS_EF_TEMP, &v));
      assert(PstWeatherValue(PS_WX_RAIN, PS_EF_PRECIP, &v) && v == 0.2f && PstWeatherValue(PS_WX_RAIN, PS_EF_RAINDROP, &v) && v == 1.0f && !PstWeatherValue(PS_WX_RAIN, PS_EF_GUST, &v));
      assert(PstWeatherValue(PS_WX_STORM, PS_EF_PRECIP, &v) && v == 0.25f && PstWeatherValue(PS_WX_STORM, PS_EF_FOGSCATTER, &v) && v == 12.0f && PstWeatherValue(PS_WX_STORM, PS_EF_GUST, &v) && v == 750.0f);
      assert(PstWeatherValue(PS_WX_STORM, PS_EF_GUSTINT, &v) && v == 550.0f && PstWeatherValue(PS_WX_STORM, PS_EF_TREEWIND, &v) && v == 2.0f && !PstWeatherValue(PS_WX_STORM, PS_EF_TEMP, &v));
      assert(PstWeatherValue(PS_WX_SNOW, PS_EF_TEMP, &v) && v == -20.0f && PstWeatherValue(PS_WX_SNOW, PS_EF_PRECIP, &v) && v == 0.2f && !PstWeatherValue(PS_WX_SNOW, PS_EF_GUST, &v)); }
    // per-frame override + Stock restore. (a) the game re-fills S every frame: nothing to restore, stock tracks the preset
    {   PstEnvField e; memset(&e, 0, sizeof e); float s = 0.43f, o = 9;
        for (int fr = 0; fr < 5; ++fr) { s = PstEnvApply(&e, fr == 3 ? 0.51f : 0.43f, 0.0f); }
        assert(s == 0.0f && e.wrote && e.stock == 0.43f && e.refills == 4 && e.frames == 4);
        assert(!PstEnvRelease(&e, 0.43f /* re-filled */, &o) && o == 9 && !e.wrote); }
    // (b) the game does NOT re-fill: Stock writes the saved value back once; an area change in between updates the saved stock
    {   PstEnvField e; memset(&e, 0, sizeof e); float cur = 0.43f, o = 9;
        for (int fr = 0; fr < 5; ++fr) cur = PstEnvApply(&e, cur, 0.75f);
        assert(cur == 0.75f && e.stock == 0.43f && e.refills == 0 && e.frames == 4);
        cur = PstEnvApply(&e, 0.62f /* the game set a new area preset */, 0.75f); assert(e.stock == 0.62f && e.refills == 1);
        assert(PstEnvRelease(&e, cur, &o) && o == 0.62f && !e.wrote);
        assert(!PstEnvRelease(&e, 0.62f, &o));  // released once only
        memset(&e, 0, sizeof e); cur = 0.5f; cur = PstEnvApply(&e, cur, 0.5f); assert(!PstEnvRelease(&e, cur, &o));  // wrote the stock value itself: nothing to do
    }
    // Global Display scope: ordinary gameplay needs no Practice session; transitions never override
    // a loading/non-active sky, and event/player loss releases only the override we own.
    assert(PstEnvScope(1, 0, 1, 1, 0) == 2);
    assert(PstEnvScope(0, 0, 1, 1, 0) == 0);
    assert(PstEnvScope(1, 1, 1, 1, 0) == 0);
    assert(PstEnvScope(1, 0, 0, 1, 0) == 0);
    assert(PstEnvScope(1, 0, 1, 0, 0) == 1);
    assert(PstEnvScope(1, 0, 1, 1, 1) == 1);
    {   PstEnvField e; memset(&e, 0, sizeof e); float sky = 0.43f, restore = 0;
        if (PstEnvScope(1, 0, 1, 1, 0) == 2) sky = PstEnvApply(&e, sky, 0.75f);
        assert(sky == 0.75f && e.wrote);
        // Loading is a no-touch interval; the saved stock must survive until a safe restore.
        assert(PstEnvScope(1, 1, 1, 1, 0) == 0 && e.stock == 0.43f);
        if (PstEnvScope(1, 0, 1, 1, 1) == 1 && PstEnvRelease(&e, sky, &restore)) sky = restore;
        assert(sky == 0.43f && !e.wrote);
    }
    assert(PstEnvSane(0.5f) && PstEnvSane(-20.0f) && !PstEnvSane(1e9f) && !PstEnvSane(0.0f / 0.0f));
    {   // user #23013: KiLog / HoldKiBreakV2 toggles default OFF, KiFloorDepth slider default 100 (stock), 0 .. 1000
        PstCfg d; PstDefaults(&d);
        assert(PS_KI_LOG == 14 && PS_HOLD_KI_V2 == 15 && PS_KI_FLOOR == 16 && d.v[PS_KI_LOG] == 0 && d.v[PS_HOLD_KI_V2] == 0 && d.v[PS_KI_FLOOR] == 100);
        assert(PstIsToggle(PS_KI_LOG) && PstIsToggle(PS_HOLD_KI_V2) && !PstIsToggle(PS_KI_FLOOR));
        assert(PstClamp(PS_KI_LOG, 9) == 1 && PstClamp(PS_HOLD_KI_V2, -1) == 0 && PstClamp(PS_KI_FLOOR, -5) == 0 && PstClamp(PS_KI_FLOOR, 5000) == 1000);
        assert(PstStep(PS_KI_FLOOR, 100, 1) == 150 && PstStep(PS_KI_FLOOR, 100, -1) == 50 && PstStep(PS_KI_FLOOR, 50, -1) == 0 && PstStep(PS_KI_FLOOR, 0, -1) == 0);
        assert(PstStep(PS_KI_FLOOR, 500, 1) == 1000 && PstStep(PS_KI_FLOOR, 1000, 1) == 1000 && PstStep(PS_KI_FLOOR, 120, 1) == 150 && PstStep(PS_KI_FLOOR, 120, -1) == 100);
        assert(PstFromIni(PS_KI_FLOOR, 300) == 300 && PstToIni(PS_KI_FLOOR, 300) == 300 && PstFromIni(PS_HOLD_KI_V2, 7) == 1);
        // user #23400 NegKiMeter: toggle, default ON (the only toggle that is)
        assert(PS_NEG_KI_METER == 17 && d.v[PS_NEG_KI_METER] == 0 && PstIsToggle(PS_NEG_KI_METER) && PstClamp(PS_NEG_KI_METER, 9) == 1 && PstStep(PS_NEG_KI_METER, 1, 0) == 0 && PstFromIni(PS_NEG_KI_METER, 0) == 0);
        // Ki floor: stock for anyone but a Training boss, and at 100 %; scaled stock; -max base when the stock floor is missing (0)
        assert(PstKiFloor(-1000.0f, 1000.0f, 300, 0) == -1000.0f && PstKiFloor(-1000.0f, 1000.0f, 100, 1) == -1000.0f);
        assert(PstKiFloor(-1000.0f, 1000.0f, 300, 1) == -3000.0f && PstKiFloor(-800.0f, 1000.0f, 50, 1) == -400.0f && PstKiFloor(-800.0f, 1000.0f, 0, 1) == 0.0f);
        assert(PstKiFloor(0.0f, 1200.0f, 200, 1) == -2400.0f && PstKiFloor(0.0f, 0.0f, 200, 1) == 0.0f && PstKiFloor(0.0f, 1e9f, 200, 1) == 0.0f);
        assert(PstKiFloor(-1000.0f, 1000.0f, 5000, 1) == -10000.0f);
        { const float nan = 0.0f / 0.0f; const float r = PstKiFloor(nan, 1000.0f, 300, 1); assert(r != r); }
        // Hold v2 writes: cur / K+60 only when above the floor, K+58 / K+5C when non-zero; a positive or garbage floor writes nothing
        assert(PstHoldV2Writes(-200.0f, 300.0f, 10.0f, 50.0f, -1000.0f) == (1 | 2 | 4 | 8));
        assert(PstHoldV2Writes(-1000.0f, -1000.0f, 0.0f, 0.0f, -1000.0f) == 0);
        assert(PstHoldV2Writes(-1000.0f, 5.0f, 0.0f, 0.0f, -1000.0f) == 2 && PstHoldV2Writes(0.0f, -1000.0f, 0.0f, 0.0f, -1000.0f) == 1);
        assert(PstHoldV2Writes(-200.0f, 300.0f, 10.0f, 50.0f, 5.0f) == 0 && PstHoldV2Writes(-200.0f, 300.0f, 10.0f, 50.0f, 0.0f / 0.0f) == 0);
        assert(PstHoldV2Writes(5.0f, 5.0f, 0.0f, 0.0f, 0.0f) == 3);
    }
    {   // #43283 Boss Match: gate dummy mutations, keep the saved preference, leave read-only overlays on.
        PstCfg pref; PstDefaults(&pref);
        pref.v[PS_HOLD_KI_BREAK] = 1; pref.v[PS_ONE_HIT_KI] = 1; pref.v[PS_HOLD_KI_V2] = 1;
        pref.v[PS_BOSS_KI_DRAIN] = 50; pref.v[PS_KI_FLOOR] = 300; pref.v[PS_BOSS_SPEED] = 50;
        pref.v[PS_KI_LOG] = 1; pref.v[PS_FRAME_METER] = 1; pref.v[PS_NEG_KI_METER] = 1; pref.v[PS_PLAYER_SPEED] = 50;
        const int match = 1;
        assert(PstBossMatchEffective(PS_HOLD_KI_BREAK, pref.v[PS_HOLD_KI_BREAK], match) == 0);
        assert(PstBossMatchEffective(PS_ONE_HIT_KI, pref.v[PS_ONE_HIT_KI], match) == 0);
        assert(PstBossMatchEffective(PS_HOLD_KI_V2, pref.v[PS_HOLD_KI_V2], match) == 0);
        assert(PstBossMatchEffective(PS_BOSS_KI_DRAIN, pref.v[PS_BOSS_KI_DRAIN], match) == 0);
        assert(PstBossMatchEffective(PS_KI_FLOOR, pref.v[PS_KI_FLOOR], match) == 100);
        assert(PstBossMatchEffective(PS_BOSS_SPEED, pref.v[PS_BOSS_SPEED], match) == 100);
        assert(PstBossMatchEffective(PS_KI_LOG, pref.v[PS_KI_LOG], match) == 1);
        assert(PstBossMatchEffective(PS_FRAME_METER, pref.v[PS_FRAME_METER], match) == 1);
        assert(PstBossMatchEffective(PS_NEG_KI_METER, pref.v[PS_NEG_KI_METER], match) == 1);
        assert(PstBossMatchEffective(PS_PLAYER_SPEED, pref.v[PS_PLAYER_SPEED], match) == 50);
        for (int i = 0; i < PS_N; ++i) assert(PstBossMatchSaved(i, pref.v[i], match) == pref.v[i]);
        // new match with a hold already on: release it. A second tick does not start another.
        const int on = PstBossMatchEffective(PS_HOLD_KI_BREAK, 1, 1);
        assert(PstHoldBreakStep(on, 1, 0, 1, 1) == PS_HB_RELEASE);
        assert(PstHoldBreakStep(on, 1, 0, 1, 0) == PS_HB_NONE);
        // repeated start keeps the same gate and the same saved values
        assert(PstBossMatchEffective(PS_ONE_HIT_KI, pref.v[PS_ONE_HIT_KI], 1) == 0);
        assert(PstBossMatchSaved(PS_ONE_HIT_KI, pref.v[PS_ONE_HIT_KI], 1) == 1);
        assert(PstKiDmg(1, 0, 1, 100, PstBossMatchEffective(PS_ONE_HIT_KI, 1, 1), 0, 1760.0f, 10.0f) == 10.0f);
        // end -> Practice: the stored toggle applies again, including a fresh hold
        assert(PstBossMatchEffective(PS_HOLD_KI_BREAK, 1, 0) == 1);
        assert(PstBossMatchEffective(PS_ONE_HIT_KI, 1, 0) == 1);
        assert(PstHoldBreakStep(PstBossMatchEffective(PS_HOLD_KI_BREAK, 1, 0), 1, 0, 1, 0) == PS_HB_START);
        const float oh = PstKiDmg(1, 0, 1, 100, 1, 0, 1760.0f, 10.0f);
        assert(oh > 1760.0f);
    }
    puts("practice_stats_test: all passed");
    return 0;
}
