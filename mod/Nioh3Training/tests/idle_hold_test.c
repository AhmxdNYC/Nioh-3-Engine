// Actual idle-hold lifecycle, with synthetic native state and no game access.
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
static struct { int active, round; uint32_t chr; DWORD tCmd; } g_tr;
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
#include "../src/idle_hold.h"
static void Reset(void) {
    memset(&g_ih, 0, sizeof g_ih);
    g_ih_request = g_ih_status = g_ih_epoch = g_ih_serial = 0;
    g_ih_seen_active = g_ih_seen_round = 0; g_ih_seen_chr = g_ih_seen_start = 0;
    g_tr.active = g_tr.round = 1; g_tr.chr = 42450; g_tr.tCmd = 1;
    g_bm.active = 1; g_bm.id = 7; g_bh.phase = 0; g_bh.h = 7;
    actor = 0x1000; player = 0x4000; ai_on = can_idle = alive = readable = disable_works = 1;
    off_calls = on_calls = 0; clock_ms = 100; action = 0x4E64; g_mv_hooked = 1;
    IhTick();
}
static void Request(void) {
    assert(Nioh3Training_SetIdleHold(1)); IhTick();
    assert(Nioh3Training_GetIdleHold() == 1 && off_calls == 0);
}
static void Hold(void) {
    Request(); action = 2; IhTick();
    assert(Nioh3Training_GetIdleHold() == 2 && off_calls == 1 && ai_on == 0);
}
int main(void) {
    Reset(); g_tr.active = 0; IhTick(); assert(!Nioh3Training_SetIdleHold(1));
    Reset(); Request();
    assert(IhSelect(0x3000, 0x4E64) == 2);
    assert(IhSelect(0x5000, 0x4E64) == 0x4E64); // other actor
    assert(IhSelect(0x3000, 0x308) == 0x308); // death/status
    assert(IhSelect(0x3000, 0xFFFF) == 0xFFFF); // unknown
    assert(IhSelect(0x3000, 2) == 2);
    can_idle = 0; assert(IhSelect(0x3000, 0x4E64) == 0x4E64); // Ki break / finisher / hidden
    can_idle = 1; Nioh3Training_SetIdleHold(0);
    assert(IhSelect(0x3000, 0x4E64) == 0x4E64); IhTick(); assert(!off_calls && !on_calls);
    Reset(); Hold(); Nioh3Training_SetIdleHold(0); IhTick();
    assert(ai_on == 1 && on_calls == 1 && Nioh3Training_GetIdleHold() == 0); IhTick(); assert(on_calls == 1);
    Reset(); Hold(); g_tr.active = 0; IhTick(); assert(on_calls == 1 && !g_ih_request);
    Reset(); Hold(); ++g_tr.round; IhTick(); assert(on_calls == 1 && !g_ih_request);
    Reset(); Hold(); ++g_tr.tCmd; IhTick(); assert(on_calls == 1 && !g_ih_request);
    Reset(); Hold(); g_tr.chr = 16102; IhTick(); assert(on_calls == 1 && !g_ih_request);
    Reset(); Hold(); actor = 0x5000; IhTick(); assert(on_calls == 0 && !g_ih_request); // reused handle, no stale write
    Reset(); Hold(); g_bh.phase = 2; IhTick(); assert(on_calls == 0 && !g_ih_request && ai_on == 0);
    Reset(); ai_on = 0; Request(); action = 2; IhTick(); assert(!off_calls && !on_calls);
    clock_ms += 10000; IhTick(); assert(Nioh3Training_GetIdleHold() == -1 && !on_calls);
    Reset(); Request(); can_idle = 0; action = 2; IhTick(); assert(!off_calls);
    clock_ms += 10000; IhTick(); assert(Nioh3Training_GetIdleHold() == -1 && !on_calls);
    Reset(); Hold(); alive = 0; IhTick(); assert(!on_calls && !g_ih_request); // dead/HP-floor never revived
    Reset(); Hold(); readable = 0; IhTick(); assert(!on_calls && !g_ih_request);
    Reset(); Hold(); ai_on = 1; clock_ms += 100; IhTick(); assert(off_calls == 2 && ai_on == 0);
    Reset(); Hold(); action = 0x308; IhTick(); assert(off_calls == 1 && on_calls == 0); // reactions are not frozen/rewritten
    Reset(); g_mv_hooked = -1; Nioh3Training_SetIdleHold(1); IhTick(); assert(!off_calls && Nioh3Training_GetIdleHold() == -1);
    Reset(); disable_works = 0; Request(); action = 2; IhTick(); assert(off_calls == 1 && on_calls == 0 && Nioh3Training_GetIdleHold() == -1);
    Reset(); Hold(); IhStop("TrainClear before despawn", 0); assert(on_calls == 1 && ai_on == 1);
    Reset(); g_tr.chr = 123; IhTick(); Nioh3Training_SetIdleHold(1); IhTick(); assert(!off_calls && Nioh3Training_GetIdleHold() == -1);
    Reset(); g_bm.active = 0; Nioh3Training_SetIdleHold(1); IhTick(); assert(!off_calls && Nioh3Training_GetIdleHold() == -1);
    Reset(); player = actor; Nioh3Training_SetIdleHold(1); IhTick(); assert(!off_calls && Nioh3Training_GetIdleHold() == -1);
    puts("PASS: idle hold acquisition, OFF/exit restore, round/session change, actor replacement, loading ownership, no AI adoption, timeout, HP floor, unreadable state, re-hold, reactions, hook/disable failure, pre-despawn cleanup, unknown boss, player isolation");
    return 0;
}
