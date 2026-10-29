// user #13529: move drill in the Stand still stance (drill.h) on top of the actual idle hold, synthetic native state, no game access.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG // Keep behavioral assertions active in optimized test builds.
#include <assert.h>
#include "../../boss_moves.h"
#define N3T_API
static DWORD clock_ms;
#define GetTickCount() clock_ms
static struct { int active, round, practice; uint32_t chr; DWORD tCmd; } g_tr;
static struct { int active; uint32_t id; } g_bm;
static struct { int phase; uint32_t h; } g_bh;
static uint64_t actor, player;
static int ai_on, can_idle, alive, readable, disable_works, off_calls, on_calls;
static uint32_t action;
static int g_mv_hooked;
static void MvInstall(void) { }
static void Log(const char* fmt, ...) { (void)fmt; }
static uint64_t ActorByHandle(uint32_t h) { return h == 7 ? actor : 0; }
static uint64_t PlayerChar(void) { return player; }
static int Rd64(uintptr_t address, uint64_t* out) {
    if (!readable) return 0;
    if (address == 0x13B0) { *out = 0x2000; return 1; }
    if (address == 0x2008) { *out = 0x3000; return 1; }
    return 0;
}
static int BhAiOn(uint64_t a) { assert(a == 0x1000); return ai_on; }
static int IhAlive(uint64_t a) { assert(a == 0x1000); return alive; }
static int IhEligible(uint64_t a, uint64_t ctrl) { assert(a == 0x1000 && ctrl == 0x3000); return can_idle && alive; }
static uint32_t IhReadAction(uint64_t ctrl) { assert(ctrl == 0x3000); return action; }
static void IhNativeAi(uint64_t a, int on) {
    assert(a == 0x1000 && a != player);
    if (on) { ++on_calls; ai_on = 1; }
    else { ++off_calls; if (disable_works) ai_on = 0; }
}
static uint32_t rep_id; static int g_cfg_rep_gap = 1000; static int forces, force_takes;
static uint32_t MvRepeatId(uint32_t chr) { return chr == 42450 ? rep_id : 0; }
static void DrForceNative(uint64_t ctrl, uint32_t id) { assert(ctrl == 0x3000 && id == rep_id); ++forces; if (force_takes) action = id; }
#include "../src/idle_hold.h"
#include "../src/drill.h"
static void Reset(void) {
    memset(&g_ih, 0, sizeof g_ih); memset(&g_dr, 0, sizeof g_dr);
    g_ih_request = g_ih_status = g_ih_epoch = g_ih_serial = 0; g_ih_default_pending = g_ih_idle_stance = 0; g_ih_drill_ai = 0; g_dr_state = 0;
    g_ih_seen_active = g_ih_seen_round = 0; g_ih_seen_chr = g_ih_seen_start = 0;
    g_tr.active = g_tr.round = 1; g_tr.chr = 42450; g_tr.tCmd = 1;
    g_bm.active = 1; g_bm.id = 7; g_bh.phase = 0; g_bh.h = 7;
    actor = 0x1000; player = 0x4000; ai_on = can_idle = alive = readable = disable_works = 1;
    off_calls = on_calls = 0; clock_ms = 100; action = 0x4E64; g_mv_hooked = 1; rep_id = 0; forces = 0; force_takes = 1;
    IhTick();
    assert(Nioh3Training_SetIdleHold(1)); IhTick(); action = 2; IhTick();
    assert(Nioh3Training_GetIdleHold() == 2 && ai_on == 0);
}
static void Step(DWORD ms) { clock_ms += ms; IhTick(); DrillTick(); }
int main(void) {
    (void)IhDefault; (void)IhAdoptBossHold; (void)IhSelect;
    // no repeat: nothing forced, no drill
    Reset(); Step(100); Step(5000); assert(!forces && Nioh3Training_GetDrill() == 0);
    // repeat set while standing still: forced ~0.5 s after the pick, AI stays off
    Reset(); rep_id = 0x4E64; Step(100); assert(Nioh3Training_GetDrill() == 1 && !forces);
    Step(500); assert(forces == 1 && action == 0x4E64 && ai_on == 0);
    Step(2000); assert(forces == 1);              // move still playing: never re-forced mid-move
    action = 2; Step(100);                         // move ended (back to idle)
    Step(500); assert(forces == 1);                // gap (1000 ms) not over
    Step(500); assert(forces == 2 && ai_on == 0 && on_calls == 0);  // every gap
    // reactions / Ki break: no force while not eligible
    action = 2; Step(100); can_idle = 0; Step(3000); assert(forces == 2); can_idle = 1; Step(100); assert(forces == 3);
    // stopping the repeat or switching to Fight normally ends the drill
    action = 2; rep_id = 0; Step(100); assert(Nioh3Training_GetDrill() == 0);
    Step(5000); assert(forces == 3);
    Reset(); rep_id = 0x4E64; Step(100); Step(500); assert(forces == 1);
    action = 2; Nioh3Training_SetIdleHold(0); Step(100); assert(Nioh3Training_GetDrill() == 0 && ai_on == 1); Step(5000); assert(forces == 1);
    // forced move never starts with the AI off: 3 tries, then the AI is switched on for the drill only and the idle re-check leaves it on
    Reset(); force_takes = 0; action = 2; rep_id = 0x4E64; Step(100);
    for (int i = 0; i < 3; ++i) { Step(1100); Step(600); }
    assert(forces == 3 && Nioh3Training_GetDrill() == 2 && ai_on == 1 && on_calls == 1);
    Step(200); Step(200); assert(ai_on == 1 && off_calls == 1);  // Idle hold does not re-disable while the drill owns it
    rep_id = 0; Step(100); assert(Nioh3Training_GetDrill() == 0); Step(150); assert(ai_on == 0);  // repeat stopped: stand still again
    // boss gone / dead: drill stops, no force
    Reset(); rep_id = 0x4E64; Step(100); alive = 0; Step(1000); assert(!forces && Nioh3Training_GetDrill() == 0);
    // user #14147: aggro walk-up hold (aggro.h sets g_dr_hold, AI on via g_ih_drill_ai): nothing forced, Idle hold leaves the AI on;
    // released at idle: first repeat ~0.5 s later, AI off again
    Reset(); rep_id = 0x4E64; action = 2; Step(100); g_dr_hold = 1; g_ih_drill_ai = 1; ai_on = 1;
    Step(200); Step(5000); assert(!forces && ai_on == 1);
    g_dr_hold = 0; g_ih_drill_ai = 0; Step(150); assert(ai_on == 0 && !forces); Step(400); assert(forces == 1);
    // walk-up IhSelect: movement picks pass, attacks -> idle for 6 s, then everything passes
    {
        int ka = -1, km = -1;
        for (int i = 0; i < N3_MOVE_COUNT; ++i) if (kN3Moves[i].chr == 42450 && kN3Moves[i].id >= 0x4E20) {
            if ((kN3Moves[i].kind == 'A' || kN3Moves[i].kind == 0) && ka < 0) ka = i;
            if (kN3Moves[i].kind == 'M' && km < 0) km = i;
        }
        assert(ka >= 0);
        {
            action = 2; Step(2000);
            g_ih_chase_t = (LONG)(clock_ms | 1);
            assert(IhSelect(0x3000, kN3Moves[ka].id) == 2);
            if (km >= 0) assert(IhSelect(0x3000, kN3Moves[km].id) == kN3Moves[km].id);
            clock_ms += 60000; assert(IhSelect(0x3000, kN3Moves[ka].id) == 2);  // #14267: whole walk-up, no 6 s release
            assert(IhSelect(0x3000, 0x4FFF) == 2);  // uncatalogued id >= 0x4E20 during the walk-up -> idle
            assert(IhSelect(0x3000, 0x6B) == 0x6B);  // locomotion < 0x4E20 passes
            g_ih_chase_t = 0; assert(IhSelect(0x3000, kN3Moves[ka].id) == 2 && IhSelect(0x3000, 0x4FFF) == 0x4FFF);  // no walk-up: uncatalogued passes
        }
    }
    puts("PASS: drill forces the picked move every gap while standing still (AI off), waits for idle / eligibility, stops with the repeat / stance / boss, AI-on fallback after 3 failed forces, aggro walk-up hold (#14147)");
    return 0;
}
