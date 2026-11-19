// Offline harness: actual veto/API implementation, synthetic actor identity and native calls.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#undef NDEBUG // Zig -O2 defines it; harness assertions must execute, including API calls.
#include <assert.h>
#define N3T_API
static uintptr_t B;
static uintptr_t Grva(uintptr_t rva) { return rva; }  // AOB build: identity in the fixture
static char g_dir[MAX_PATH];
static struct { int active, practice; uint32_t chr; } g_tr;
static struct { int active; uint32_t id; uint64_t actor; } g_bm;
// bridge 12666 summon slots (none in this fixture) and the pump-side natives move_toggle.h now uses
static int g_nsp; static uint64_t g_sp_actor[2]; static uint32_t g_sp_chr[2];
static uint32_t Nioh3Training_SlotChr(int slot) { (void)slot; return 0; }
static uint32_t cur_act = 0xFFFFFFFFu, forced_id, forced_n;  // user #14423: the boss's live action and the forced-transition stand-in
static uint32_t ActIdOf(uint64_t st) { (void)st; return cur_act; }
static DWORD fake_now = 1;
#define GetTickCount() (fake_now)
#define MV_FORCE(st, id) (forced_id = (uint32_t)(id), ++forced_n, cur_act = (uint32_t)(id))
static int standing;  // drill.h MvRepStanding stand-in

static int read_ok = 1, hook_calls;
static uint64_t player = 0x4000, actor = 0x1000;
static uint64_t ActorByHandle(uint32_t id) { return id == 7 ? actor : 0; }
static uint64_t PlayerChar(void) { return player; }
static int Rd64(uintptr_t address, uint64_t* out) {
    if (!read_ok) return 0;
    if (address == 0x13B0) { *out = 0x2000; return 1; }
    if (address == 0x2008) { *out = 0x3000; return 1; }
    return 0;
}
static void Log(const char* fmt, ...) { (void)fmt; }
static uint32_t selected[10], selection_calls, committed;
static uint32_t Select(uint64_t st, uint64_t tr) {
    (void)st; (void)tr;
    return selected[selection_calls++ % 10];
}
static uint64_t Commit(uint64_t st, uint32_t id, uint64_t tbl, int layer, uint64_t slot) {
    (void)st; (void)tbl; (void)layer; (void)slot;
    committed = id; return id != 0xFFFF;
}
static void* Hook(uintptr_t rva, int ncopy, void* detour) {
    (void)ncopy; (void)detour; ++hook_calls;
    return rva == 0x35EF04 ? (void*)Select : (void*)Commit;
}
static uint32_t IhSelect(uint64_t st, uint32_t id) { (void)st; return id; }
#define HOOK_SET(dst,rva,n,detour) (dst = (void*)Hook(rva,n,detour))
#define N3T_SLOTS 2
static uint64_t g_slot_act[2]; static uint32_t g_slot_native[2], g_slot_chr[2];
static int native_live;
static int SlotAlive(int s) { return native_live && s == 0; }
static int NativeSlotsActive(void) { return native_live; }
#include "../src/auto_moves_cmp.h"
#include "../../move_catalog.h"
#include "../src/move_toggle.h"
static inline int MvRepStanding(uint32_t chr) { (void)chr; return standing; }

static void Pick(uint32_t id) {
    selection_calls = 0;
    for (int i = 0; i < 10; ++i) selected[i] = id;
}
// GitHub #126 (live #119387): Giant Toad 210803 attacks with low ids (0xC61..) and rests in 0xBB8, not 0x2 (its 0x2 row is another action).
// Repeat last move armed 4 times live and forced 0 repeats: the select hook let every low-id pick through and waited for action 0x2.
// This runs the real repeat code on the Toad's REAL merged catalogue (AmParse + N3CatMerge of the extracted action file).
static uint8_t* ReadAll(const char* rel, uint32_t* n) {
    static const char* roots[] = {"../../data/extract", "../../../nioh3-engine/data/extract", "../../../../nioh3-engine/data/extract", "data/extract"};
    char p[512]; const char* env = getenv("N3_EXTRACT");
    for (int i = -1; i < (int)(sizeof roots / sizeof roots[0]); ++i) {
        if (i < 0 && !env) continue;
        snprintf(p, sizeof p, "%s/lfm/archive_01/%s", i < 0 ? env : roots[i], rel);
        FILE* f = fopen(p, "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t* d = (uint8_t*)malloc((size_t)L); *n = (uint32_t)fread(d, 1, (size_t)L, f); fclose(f);
        return d;
    }
    return 0;
}
static AmTable g_t;
int main(void) {
    setvbuf(stdout, 0, _IONBF, 0);
    const uint32_t T = 210803u;
    uint32_t n = 0; uint8_t* d = ReadAll("98/A39544553277A061AB34F7A3EB9B7A0FBFA834260AC9BA7CBEFD1C69D3E7EF", &n);
    if (!d) { puts("SKIP Giant Toad repeat: action file not extracted"); return 0; }
    N3CatInit(1);
    assert(AmParse(d, n, &g_t) > 0); N3CatMerge(T, g_t.m, g_t.n, "Giant Toad");
    // data rules: rest = 0xBB8 (and 0x2 kept as before); 0x2 is not held as the Toad's idle; steps are not attacks
    assert(MvRest(T, 0xBB8) && !MvRest(T, 0x11) && !MvRest(T, 0) && !MvRest(T, 0xC61) && MvIdleId(T) == 0xBB8);
    assert(MvIdleId(16102u) == 2 || MvIdleId(16102u) == 0);
    assert(MlAttack(T, 0xC61) && MlAttack(T, 0xC67) && !MlAttack(T, 0xC56) && !MlAttack(T, 0xC5E) && !MlAttack(T, 0x1F));
    // a boss fight with the Toad, its AI controller 0x3000 (Rd64 fixture: actor 0x1000 -> ai 0x2000 -> ctrl 0x3000)
    g_tr.active = g_tr.practice = g_bm.active = 1; g_tr.chr = T; g_nsp = 1; g_sp_actor[0] = actor; g_sp_chr[0] = T;
    assert(MvBossChr(0x3000) == T);
    B = (uintptr_t)calloc(1, 0x360000); assert(B);
    const uint8_t selp[] = {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x30};
    const uint8_t comp[] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20};
    memcpy((void*)(B+0x35EF04), selp, sizeof selp); memcpy((void*)(B+0x35BC68), comp, sizeof comp);
    g_mv_hooked = 0; MvInstall(); assert(g_mv_hooked == 1);
    memset(g_ml, 0, sizeof g_ml); g_ml[0].chr = T; g_ml[0].ids[0] = 0xC61; g_ml[0].n = 1; g_ml[0].t = 1;
    fake_now = 100000;
    assert(Nioh3Training_SetRepeatLast(T, 1) == 1 && MvRepeatId(T) == 0xC61);
    const int ri = MvRepFind(T); assert(ri >= 0);
    // live: the Toad rests in 0xBB8 and its AI picks 0xC67: that pick becomes the repeated 0xC61
    cur_act = 0xBB8; fake_now += 100; Pick(0xC67); assert(h_mvsel(0x3000, 0) == 0xC61 && g_rep_count[ri] == 1);
    // asked for, not playing yet: a step pick (motion 34) from rest keeps asking for 0xC61, nothing counted twice (#119454)
    cur_act = 0xBB8; fake_now += 10; Pick(0xC56); assert(h_mvsel(0x3000, 0) == 0xC61 && g_rep_count[ri] == 1);
    // 0xC61 plays: another attack pick waits (the running move plays to its end)
    cur_act = 0xC61; fake_now += 50; Pick(0xC67); assert(h_mvsel(0x3000, 0) == MV_KEEP);
    // back in rest 0xBB8 by itself: the gap holds the Toad in 0xBB8 (never its 0x2 row)
    cur_act = 0xBB8; fake_now += 2000; Pick(0xC67); assert(h_mvsel(0x3000, 0) == 0xBB8 && g_rep_end[ri]);
    // the pump forces the next repeat from rest once the gap passed
    const uint32_t before = forced_n; fake_now += (DWORD)MvGapEff(ri, T) + 50; MvRepTick();
    assert(forced_n == before + 1 && forced_id == 0xC61 && g_rep_count[ri] == 2);
    // or the AI's next pick after the gap is replaced
    cur_act = 0xBB8; fake_now += 3000; g_rep_end[ri] = fake_now - 5000; g_rep_last[ri] = 0; Pick(0xC7A);
    assert(h_mvsel(0x3000, 0) == 0xC61);
    // live #119454 shape: the Toad never picks an attack from 0xBB8. From rest it picks a step (0xC5E), and its attack (0xC67) while a
    // step (0xC56) plays. Both are the repeat's chance once the gap passed; a step during the running repeat still plays natively.
    assert(MvCalm(T, 0xBB8) && MvCalm(T, 0xC56) && MvCalm(T, 0xC5E) && !MvCalm(T, 0xC61) && !MvCalm(T, 0xC67) && MvCalm(T, 0x11));  // 0x11 = its other idle (kind I, motion 51)
    const LONG c0 = g_rep_count[ri];
    cur_act = 0xBB8; fake_now += 5000; g_rep_end[ri] = fake_now - 5000; g_rep_last[ri] = 0; Pick(0xC5E);
    assert(h_mvsel(0x3000, 0) == 0xC61 && g_rep_count[ri] == c0 + 1);
    cur_act = 0xC61; fake_now += 30; g_rep_seen[ri] = fake_now; Pick(0xC5E); assert(h_mvsel(0x3000, 0) == 0xC5E && g_rep_count[ri] == c0 + 1);
    cur_act = 0xC56; fake_now += 5000; g_rep_end[ri] = fake_now - 5000; g_rep_last[ri] = 0; Pick(0xC67);
    assert(h_mvsel(0x3000, 0) == 0xC61 && g_rep_count[ri] == c0 + 2);
    // within the gap a step from rest is held in 0xBB8 (the pump forces the next round), never a native step out of the gap
    cur_act = 0xBB8; fake_now += 10; g_rep_seen[ri] = 0; g_rep_last[ri] = 0; g_rep_end[ri] = fake_now; Pick(0xC5E);
    assert(h_mvsel(0x3000, 0) == 0xBB8 && g_rep_count[ri] == c0 + 2);
    Nioh3Training_SetRepeatLast(T, 0); assert(MvRepeatId(T) == 0);
    puts("PASS: Giant Toad Repeat last move: low-id attack picks replaced, rest 0xBB8 / a step from it starts the next repeat (GitHub #126, live #119387 / #119454)");
    free(d);
    return 0;
}
