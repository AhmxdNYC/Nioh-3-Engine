// user #14546: 'Boss aggro on me' for a Stand-still Training boss that repeats a whole sequence (no drill: move_toggle.h MvRepTick forces the
// head with the AI off). Real move_toggle.h + idle_hold.h + drill.h + aggro.h, synthetic native state, no game access.
// Live 23:39-23:41 (Yoritomo 16102, 0x4E21 > 0x4E22): aggro ON logged nothing more; the boss was never walked up / targeted.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#undef NDEBUG // Zig -O2 defines it; harness assertions must execute.
#include <assert.h>
#define N3T_API
static DWORD clock_ms = 100;
#define GetTickCount() clock_ms
static uintptr_t B;
static uintptr_t Grva(uintptr_t rva) { return rva; }  // AOB build: identity in the fixture
static char g_dir[MAX_PATH];
static struct { int active, round, practice, hasStart; uint32_t chr; DWORD tCmd; float bp[3], face; } g_tr;
static struct { int active; uint32_t id; uint64_t actor; } g_bm;
static struct { int phase; uint32_t h; uint64_t a; } g_bh;
#define N3T_SLOTS 2
#define MV_CHASE_MAX (N3T_SLOTS + 1)
static int g_nsp; static uint64_t g_sp_actor[2]; static uint32_t g_sp_chr[2];
static uint64_t g_slot_act[N3T_SLOTS]; static uint32_t g_slot_chr[N3T_SLOTS]; static volatile LONG g_slot_aggro[N3T_SLOTS];
static float g_slot_sp[N3T_SLOTS][4]; static int g_slot_sp_ok[N3T_SLOTS], g_slot_ih[N3T_SLOTS];
static int SlotOf(uint64_t a) { for (int s = 0; a && s < N3T_SLOTS; ++s) if (g_slot_act[s] == a) return s; return -1; }
static int SlotAlive(int s) { return s >= 0 && s < N3T_SLOTS && g_slot_act[s] != 0; }
static uint32_t Nioh3Training_SlotChr(int slot) { return SlotAlive(slot) ? g_slot_chr[slot] : 0; }
static const uint64_t actor = 0x1000, player = 0x4000, CTRL = 0x3000;
static uint32_t act = 2;  // the boss's live action (ActIdOf / IhReadAction)
static float bpos[4] = {0, 0, 1000, 1}, ppos[4] = {0, 0, 0, 1};
static int ai_on, forced_n, faces, aware_calls; static uint32_t forced_id;
static uint32_t ActIdOf(uint64_t st) { assert(st == CTRL); return act; }
static uint64_t ActorByHandle(uint32_t h) { return h == 7 ? actor : 0; }
static uint64_t PlayerChar(void) { return player; }
static int Rd64(uintptr_t address, uint64_t* out) {
    if (address == actor + 0x3B0) { *out = 0x2000; return 1; }
    if (address == 0x2008) { *out = CTRL; return 1; }
    if (address == actor + 0x110) { memcpy(out, bpos, 8); return 1; }
    if (address == actor + 0x118) { memcpy(out, bpos + 2, 8); return 1; }
    return 0;  // player + 0x3B0: no hate call
}
static void Log(const char* fmt, ...) { (void)fmt; }
static void* Hook(uintptr_t rva, int ncopy, void* detour) { (void)rva; (void)ncopy; (void)detour; return 0; }
#define MV_FORCE(st, id) (assert((st) == CTRL), forced_id = (uint32_t)(id), ++forced_n, act = (uint32_t)(id))
static void AgSeqFace(uint64_t st);
#define MV_PREFORCE(st) (++faces, AgSeqFace(st))
#include "../src/move_toggle.h"
static int BhAiOn(uint64_t a) { assert(a == actor); return ai_on; }
static int IhAlive(uint64_t a) { return a == actor; }
static int IhEligible(uint64_t a, uint64_t ctrl) { return a == actor && ctrl == CTRL; }
static uint32_t IhReadAction(uint64_t ctrl) { assert(ctrl == CTRL); return act; }
static void IhNativeAi(uint64_t a, int on) { assert(a == actor); ai_on = on != 0; }
#include "../src/idle_hold.h"
static int dr_forces;
static void DrForceNative(uint64_t ctrl, uint32_t id) { (void)ctrl; (void)id; ++dr_forces; }
static void AgDrillFace(uint64_t a);
#define DR_SEQ(chr) Nioh3Training_GetMoveRepeatSeq(chr)
#define DR_GAP(chr) MvGapFor(chr)
#include "../src/drill.h"
static int Actors(uint64_t* out, int cap) { (void)cap; out[0] = actor; out[1] = player; return 2; }
static uint64_t PercOf(uint64_t a, uint64_t* comp, uint64_t* ai) { assert(a == actor); if (comp) *comp = 0x2000; if (ai) *ai = 0x2000; return 0x5000; }
static int PercFind(uint64_t perc, uint64_t who) { (void)perc; (void)who; return -1; }  // no 2BE2E8 call
static int PlayerPos(float* p) { memcpy(p, ppos, sizeof ppos); return 1; }
#include "../src/slot_hold.h"  // user #14462: held slot bosses (training.c SphTick)
static struct { uint64_t a; DWORD t0, tRe; int ms, phase, nOff, sess, chase; } g_sph[N3T_SLOTS];
#include "../src/aggro.h"
static void AwareStub(uint64_t comp, uint8_t on, uint32_t x) { (void)comp; (void)on; (void)x; ++aware_calls; }
static void Unused(void) { (void)MvSet; (void)MvRepFresh; (void)MvLoadPrefs; (void)MvTick; (void)IhDefault; (void)IhAdoptBossHold; (void)ResetPosTick; (void)SphReleaseWhy; (void)SphAiAction; }  // SetAwareFlg 1FF1FA0 stand-in

static uint32_t ai_pick = 0x4E23;
static uint32_t Select(uint64_t st, uint64_t tr) { (void)st; (void)tr; return ai_pick; }
static void Setup(void) {
    memset(&g_tr, 0, sizeof g_tr); g_tr.active = g_tr.round = g_tr.practice = 1; g_tr.chr = 16102; g_tr.tCmd = 1;
    g_bm.active = 1; g_bm.id = 7; g_bm.actor = actor; g_bh.phase = 0;
    g_nsp = 1; g_sp_actor[0] = actor; g_sp_chr[0] = 16102;
    g_slot_act[0] = actor; g_slot_chr[0] = 16102; g_slot_aggro[0] = 1; g_slot_ih[0] = 1; g_cfg_aggro = 0;
    g_cfg_aggro_face = 0; g_cfg_aggro_walk_ms = 0; g_cfg_aggro_range = 500; g_mv_hooked = 1; o_mvsel = Select;
    B = (uintptr_t)AwareStub - 0x1FF1FA0;
    // Stand still: the Idle hold owns the AI (adopted the load hold), AI off
    IhPublishSession(); g_ih_request = g_ih_epoch; g_ih_idle_stance = 1;
    memset(&g_ih, 0, sizeof g_ih); g_ih.phase = 2; g_ih.owns_ai = 1; g_ih.epoch = g_ih_epoch; g_ih.handle = 7; g_ih.chr = 16102;
    g_ih.actor = actor; g_ih.ctrl = CTRL; g_ih_status = 2; ai_on = 0; act = 2;
    memset(g_sph, 0, sizeof g_sph);
}
static void Step(DWORD ms) { clock_ms += ms; IhTick(); DrillTick(); AggroTick(); MvRepTick(); }
static int Chasing(void) { return MvChasing(CTRL); }
int main(void) {
    (void)Unused;
    Setup();
    g_cfg_aggro=1;g_slot_aggro[0]=1;
    g_tr.practice=0;assert(!AgOn(actor));
    g_tr.practice=1;assert(AgOn(actor));
    assert(g_cfg_aggro==1 && g_slot_aggro[0]==1);
    assert(Nioh3Training_SetMoveRepeatSeq(16102, 0x4E21, 1) == 1 && Nioh3Training_GetMoveRepeatSeq(16102));
    // the boss is 10 m away (range 5 m): Stand-still sequence boss -> walk-up (AI on, locomotion only), no drill
    Step(300);
    assert(Nioh3Training_GetDrill() == 0 && dr_forces == 0);
    assert(g_agd.st == 1 && g_agd.a == actor && Chasing() && g_ih_chase_t && g_ih_drill_ai == 1 && ai_on == 1);
    Step(300); assert(ai_on == 1);  // Idle hold does not take it back while the walk-up owns it
    // AI picks while far: an attack pick becomes idle (not the head), a movement pick passes; no head forced
    ai_pick = 0x4E26; assert(h_mvsel(CTRL, 0) == 2);
    ai_pick = 0x4E27; assert(h_mvsel(CTRL, 0) == 0x4E27);
    assert(forced_n == 0 && g_rep_phase[MvRepFind(16102)] == 0);
    puts("PASS: Stand-still sequence boss out of range: walk-up starts (AI on, chase, locomotion only), no head, no drill");
    // the boss arrives in range: AI off at its next idle, chase cleared
    bpos[2] = 400; Step(300); assert(g_agd.st == 2 && Chasing());
    act = 2; Step(300); assert(g_agd.st == 0 && !Chasing() && !g_ih_chase_t && g_ih_drill_ai == 0 && ai_on == 0);
    // now in range: the next pick is the head (sequence resumes)
    ai_pick = 0x4E23; assert(h_mvsel(CTRL, 0) == 0x4E21 && g_rep_phase[MvRepFind(16102)] == 1);
    puts("PASS: arrived -> AI off again at idle, the sequence head is picked in range");
    // a running sequence is never cut by a walk-up: far again while the head is pending / the chain plays
    act = 0x4E21; bpos[2] = 1200; Step(300); assert(g_agd.st == 0 && !Chasing() && ai_on == 0);
    Step(150); assert(g_agd.st == 0);  // head confirmed (> SEQ_CONFIRM_MS): chain playing
    act = 2; MvSeqTrack(MvRepFind(16102), 16102, 2, clock_ms);  // back in idle: the sequence ended, the gap runs
    Step(300); assert(g_agd.st == 1 && Chasing() && ai_on == 1);
    // MvRepTick never forces the head while chasing, even after the gap
    Step(2000); assert(forced_n == 0);
    puts("PASS: no walk-up mid-sequence; between sequences it starts and MvRepTick holds the head");
    // repeat stopped mid walk-up: walk-up ends, Idle hold takes the AI off again
    assert(Nioh3Training_SetMoveRepeatSeq(16102, 0x4E21, 0) == 1);
    Step(300); assert(g_agd.st == 0 && g_ih_drill_ai == 0 && !g_ih_chase_t);
    Step(150); assert(ai_on == 0);
    puts("PASS: repeat stopped during the walk-up -> AI back off (Stand still)");
    // in range, gap passed: MvRepTick forces the head after the gap, MV_PREFORCE (face the player) runs first
    Setup(); bpos[2] = 300; faces = 0; forced_n = 0;
    assert(Nioh3Training_SetMoveRepeatSeq(16102, 0x4E21, 1) == 1);
    ai_pick = 0x4E23; assert(h_mvsel(CTRL, 0) == 0x4E21);  // first head (picked)
    act = 0x4E21; Step(150); Step(150); act = 0x4E22; Step(100); act = 2; Step(50);  // chain played, back in idle
    Step(600); assert(forced_n == 0); Step(500); assert(forced_n == 1 && forced_id == 0x4E21 && faces == 1 && g_agd.st == 0);
    puts("PASS: in range the head is forced after the gap, facing hook first");
    // aggro OFF: nothing changes for the Stand-still sequence boss
    Setup(); g_slot_aggro[0] = 0; bpos[2] = 1200;
    assert(Nioh3Training_SetMoveRepeatSeq(16102, 0x4E21, 1) == 1);
    Step(300); Step(300); assert(g_agd.st == 0 && !Chasing() && ai_on == 0);
    // single-move repeat in Stand still: the drill still owns it (DR_SEQ 0)
    assert(Nioh3Training_SetMoveRepeat(16102, 0x4E26, 1) == 1 && !Nioh3Training_GetMoveRepeatSeq(16102));
    Step(100); assert(Nioh3Training_GetDrill() == 1);
    puts("PASS: aggro OFF untouched; a single-move repeat still runs the drill");
    // user #14462: a summoned boss held idle by its slot (SPH_TRAINING, no Idle hold) repeating a sequence with the AI off: aggro ON walks it up
    Setup(); memset(&g_ih, 0, sizeof g_ih); g_ih_request = 0; g_ih_idle_stance = 0; forced_n = 0; g_sph[0].a = actor; g_sph[0].phase = 1; g_sph[0].ms = SPH_TRAINING; bpos[2] = 550; act = 2;
    assert(Nioh3Training_SetMoveRepeatSeq(16102, 0x4E21, 1) == 1);
    Step(300); assert(!g_sph[0].chase && !Chasing());              // 5.5 m: inside range + 1 m, no walk-up
    bpos[2] = 1200; Step(300); assert(g_sph[0].chase == 1 && Chasing() && g_agd.st == 0);  // 12 m: walk-up (SphTick switches the AI on)
    ai_pick = 0x4E26; assert(h_mvsel(CTRL, 0) == 2);               // attack pick while far -> idle
    ai_pick = 0x4E27; assert(h_mvsel(CTRL, 0) == 0x4E27);          // locomotion passes
    Step(2000); assert(forced_n == 0);                               // no head forced while chasing
    bpos[2] = 450; Step(300); assert(!g_sph[0].chase && !Chasing()); // in range: held again, the repeat goes on
    ai_pick = 0x4E23; assert(h_mvsel(CTRL, 0) == 0x4E21);          // the head
    act = 0x4E21; bpos[2] = 1200; Step(300); Step(150); assert(!g_sph[0].chase); // mid-sequence (head confirmed, chain plays): never cut
    act = 2; MvSeqTrack(MvRepFind(16102), 16102, 2, clock_ms); Step(300); assert(g_sph[0].chase == 1);
    g_slot_aggro[0] = 0; Step(300); assert(!g_sph[0].chase && !Chasing());  // aggro OFF: walk-up ends at once
    g_slot_aggro[0] = 1; assert(Nioh3Training_SetMoveRepeatSeq(16102, 0x4E21, 0) == 1); Step(300); assert(!g_sph[0].chase);  // no repeat: stays idle
    puts("PASS: #14462 held slot boss: walk-up beyond range + 1 m with locomotion only, held again in range, never mid-sequence, ends with aggro OFF / no repeat");
    printf("aggro_test: all passed (SetAwareFlg stand-in called %d times)\n", aware_calls);
    return 0;
}
