// Offline harness: actual veto/API implementation, synthetic actor identity and native calls.
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#undef NDEBUG // Zig -O2 defines it; harness assertions must execute, including API calls.
#include <assert.h>
#define N3T_API
static uintptr_t B;
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
#include "../src/move_toggle.h"
static inline int MvRepStanding(uint32_t chr) { (void)chr; return standing; }

static void Pick(uint32_t id) {
    selection_calls = 0;
    for (int i = 0; i < 10; ++i) selected[i] = id;
}
int main(void) {
    assert(MvContinues(16102u, 0x4E79, 0x4E8C));
    assert(MvContinues(16102u, 0x4E67, 0x4E70));
    assert(!MvContinues(16102u, 0x4E79, 0x4E78)); // unrelated backwards pick stays refused
    assert(!MvContinues(16102u, 0x4E67, 0x4E8C));
    printf("N=%d\n", (int)N3_MOVE_COUNT);
    // Startup: no Training session, no boss identity, and no game image needed.
    assert(!g_tr.active && !g_bm.active && !B);
    MvTick();
    assert(Nioh3Training_GetMoveBoss() == 0 && hook_calls == 0);
    assert(MvBossChr(0) == 0 && MvBossChr(0x3000) == 0);
    for (int i = 0; i < N3_MOVE_COUNT; ++i) {
        assert(kN3Moves[i].chr != 0);
        assert(Nioh3Training_GetMoveAllowed(0, kN3Moves[i].id) == -1);
        assert(Nioh3Training_SetMoveAllowed(0, kN3Moves[i].id, 0, 0) == 0);
    }
    assert(!g_mv_install_pending && hook_calls == 0);
    puts("PASS: zero-session startup; all chr=0 queries/setters rejected without hooks");
    g_tr.practice = 1; g_tr.active = g_bm.active = 1; g_bm.id = 7;
    o_mvsel = Select; o_mvcom = Commit;
    int tested = 0, protected_count = 0, yoritomo = 0;
    for (int i = 0; i < N3_MOVE_COUNT; ++i) {
        const N3Move* m = &kN3Moves[i];
        assert(N3MoveFind(m->chr, m->id) == i);
        if (!strcmp(m->boss, "Minamoto no Yoritomo")) ++yoritomo;
        assert(Nioh3Training_GetMoveAllowed(m->chr, m->id) == 1);
        g_tr.chr = m->chr;
        if (N3MoveProtected(m->id)) {
            ++protected_count;
            assert(Nioh3Training_SetMoveAllowed(m->chr, m->id, 0, 0) == 0);
            continue;
        }
        ++tested;
        assert(Nioh3Training_SetMoveAllowed(m->chr, m->id, 0, 0) == 1);
        assert(Nioh3Training_GetMoveAllowed(m->chr, m->id) == 0);
        // Same action number on any other boss stays allowed.
        for (int j = i % 7; j < N3_MOVE_COUNT; j += 7)
            if (kN3Moves[j].chr != m->chr) assert(!MvBlocked(kN3Moves[j].chr, kN3Moves[j].id));
        Pick(m->id);
        assert(h_mvsel(0x3000, 0) == 2 && selection_calls == 9);
        assert(h_mvcom(0x3000, m->id, 0, 0, 0) == 0 && committed == 0xFFFF);
        Pick(m->id); selected[2] = 2;
        assert(h_mvsel(0x3000, 0) == 2 && selection_calls == 3);
        Pick(m->id);
        assert(h_mvsel(0x5000, 0) == m->id && selection_calls == 1);
        assert(h_mvcom(0x5000, m->id, 0, 0, 0) == 1 && committed == m->id);
        g_tr.active = 0; Pick(m->id); assert(h_mvsel(0x3000, 0) == m->id); g_tr.active = 1;
        g_bm.active = 0; Pick(m->id); assert(h_mvsel(0x3000, 0) == m->id); g_bm.active = 1;
        player = actor; Pick(m->id); assert(h_mvsel(0x3000, 0) == m->id); player = 0x4000;
        read_ok = 0; Pick(m->id); assert(h_mvsel(0x3000, 0) == m->id); read_ok = 1;
        assert(Nioh3Training_SetMoveAllowed(m->chr, m->id, 1, 0) == 1);
        Pick(m->id); assert(h_mvsel(0x3000, 0) == m->id && selection_calls == 1);
    }
    assert(yoritomo == 173 && tested + protected_count == N3_MOVE_COUNT);
    assert(Nioh3Training_GetMoveAllowed(123, 0x4E21) == -1);
    assert(Nioh3Training_SetMoveAllowed(123, 0x4E21, 0, 0) == 0);
    // Legacy list cannot veto idle; an explicit menu ON overrides a legacy block.
    g_mv_hooked = 1;
    const N3Move* m = &kN3Moves[2];
    uint32_t legacy[] = {m->id, 0, 2};
    MvSet(legacy, 3);
    g_mv_pref[2] = 0;
    assert(MvBlocked(m->chr, m->id));
    assert(!MvBlocked(m->chr, 0) && !MvBlocked(m->chr, 2));
    assert(Nioh3Training_SetMoveAllowed(m->chr, m->id, 1, 0) == 1);
    assert(!MvBlocked(m->chr, m->id));
    MvSet(0, 0);
    // Preferences survive a real Windows INI round trip, with boss sections isolated.
    char temp[MAX_PATH], ini[MAX_PATH];
    assert(GetTempPathA(sizeof temp, temp));
    snprintf(g_dir, sizeof g_dir, "%sn3-moves-test-%lu", temp, GetCurrentProcessId());
    assert(CreateDirectoryA(g_dir, 0));
    snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    assert(Nioh3Training_SetMoveAllowed(m->chr, m->id, 0, 1) == 1);
    memset((void*)g_mv_pref, 0, sizeof g_mv_pref);
    MvLoadPrefs(ini);
    for (int i = 0; i < N3_MOVE_COUNT; ++i)
        assert(Nioh3Training_GetMoveAllowed(kN3Moves[i].chr, kN3Moves[i].id) == (i != 2));
    assert(Nioh3Training_SetMoveAllowed(m->chr, m->id, 1, 1) == 1);
    memset((void*)g_mv_pref, 0, sizeof g_mv_pref); MvLoadPrefs(ini);
    assert(Nioh3Training_GetMoveAllowed(m->chr, m->id) == 1);
    assert(DeleteFileA(ini)); assert(RemoveDirectoryA(g_dir));
    assert(Nioh3Training_SetMoveAllowed(m->chr, m->id, 0, 1) == -1); // missing directory
    // UI setters defer installation. Pump publishes boss identity and installs once.
    assert(hook_calls == 0);
    B = (uintptr_t)calloc(1, 0x360000); assert(B);
    const uint8_t selp[] = {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x30};
    const uint8_t comp[] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20};
    memcpy((void*)(B+0x35EF04), selp, sizeof selp); memcpy((void*)(B+0x35BC68), comp, sizeof comp);
    g_mv_hooked = 0; g_tr.chr = m->chr; MvTick();
    assert(hook_calls == 2 && g_mv_hooked == 1 && Nioh3Training_GetMoveBoss() == m->chr);
    MvTick(); assert(hook_calls == 2);
    g_tr.active = 0; MvTick(); assert(Nioh3Training_GetMoveBoss() == 0);
    *(uint8_t*)(B+0x35EF04) = 0; g_mv_hooked = 0; MvInstall();
    assert(g_mv_hooked == -1 && hook_calls == 2);
    assert(Nioh3Training_GetMoveHookStatus() == -1);
    free((void*)B);
    // user #13618: Repeat arms for every editable catalogue entry (unlabelled ones included), wins the AI pick, passes the commit veto even when
    // the move is OFF, and refuses only idle / init and unknown ids. Stand still only changes the log text (the drill plays it).
    {
        int armed = 0, unlabelled = 0;
        g_tr.practice = 1; g_tr.active = g_bm.active = 1; g_bm.id = 7; g_mv_hooked = 1; o_mvsel = Select; o_mvcom = Commit; g_cfg_rep_gap = 0;
        for (int i = 0; i < N3_MOVE_COUNT; ++i) {
            const N3Move* r = &kN3Moves[i];
            g_tr.chr = r->chr; standing = i & 1;
            if (N3MoveProtected(r->id)) { assert(Nioh3Training_SetMoveRepeat(r->chr, r->id, 1) == 0 && Nioh3Training_GetMoveRepeat(r->chr) == 0); continue; }
            if (N3MoveUnsafe(r->chr, r->id)) { assert(Nioh3Training_SetMoveRepeat(r->chr, r->id, 1) == 0 && Nioh3Training_GetMoveRepeat(r->chr) == 0); continue; }  // user #14655: boss-killing move refused
            assert(Nioh3Training_SetMoveAllowed(r->chr, r->id, 0, 0) == 1);   // OFF, then Right: the menu switches it ON, but OFF must still work
            assert(Nioh3Training_SetMoveRepeat(r->chr, r->id, 1) == 1 && Nioh3Training_GetMoveRepeat(r->chr) == r->id);
            Pick(0x4E20 + 0x7FF); selected[0] = r->id == 0x4E21 ? 0x4E22 : 0x4E21;
            for (int k = 0; k < N3_MOVE_COUNT; ++k) if (kN3Moves[k].chr == r->chr && kN3Moves[k].id >= 0x4E20 && kN3Moves[k].id != r->id) { selected[0] = kN3Moves[k].id; break; }
            if (N3MoveFind(r->chr, selected[0]) >= 0) { selection_calls = 0; assert(h_mvsel(0x3000, 0) == r->id); }
            // user #13787: aggro ON and the boss far (its state in g_mv_chase): its own pick passes, the repeat is back once it is close
            if (N3MoveFind(r->chr, selected[0]) >= 0 && selected[0] != r->id) {
                // user #14462: while far only locomotion passes; an attack pick becomes idle (when the boss has a recorded idle)
                const int sk = N3MoveFind(r->chr, selected[0]);
                const uint32_t want = (sk >= 0 && kN3Moves[sk].kind == 'M') || N3MoveFind(r->chr, 2) < 0 ? selected[0] : 2;
                g_mv_chase[1] = 0x3000; selection_calls = 0; assert(h_mvsel(0x3000, 0) == want);
                g_mv_chase[1] = 0; selection_calls = 0; assert(h_mvsel(0x3000, 0) == r->id);
            }
            assert(h_mvcom(0x3000, r->id, 0, 0, 0) == 1 && committed == r->id);   // OFF but repeating: not vetoed
            assert(Nioh3Training_SetMoveRepeat(r->chr, 0, 0) == 1 && Nioh3Training_GetMoveRepeat(r->chr) == 0);
            assert(h_mvcom(0x3000, r->id, 0, 0, 0) == 0 && committed == 0xFFFF);  // repeat stopped: OFF vetoes again
            assert(Nioh3Training_SetMoveAllowed(r->chr, r->id, 1, 0) == 1);
            ++armed; unlabelled += !*r->label;
        }
        assert(Nioh3Training_SetMoveRepeat(123, 0x4E21, 1) == 0);   // unknown boss / id
        // table of 8 bosses: a 9th distinct boss is refused, re-arming an armed boss replaces its move in place
        uint32_t chrs[16]; int nc = 0;
        for (int i = 0; i < N3_MOVE_COUNT && nc < 16; ++i) { int dup = 0; for (int k = 0; k < nc; ++k) dup |= chrs[k] == kN3Moves[i].chr; if (!dup) chrs[nc++] = kN3Moves[i].chr; }
        int ok = 0;
        for (int k = 0; k < nc; ++k) { int idx = -1; for (int i = 0; i < N3_MOVE_COUNT; ++i) if (kN3Moves[i].chr == chrs[k] && !N3MoveProtected(kN3Moves[i].id)) { idx = i; break; }
            if (idx >= 0) ok += Nioh3Training_SetMoveRepeat(chrs[k], kN3Moves[idx].id, 1); }
        assert(nc > MV_REP_MAX && ok == MV_REP_MAX);
        for (int k = 0; k < nc; ++k) Nioh3Training_SetMoveRepeat(chrs[k], 0, 0);
        // user #14309: per-boss gap (per character), others keep RepeatGapMs
        g_cfg_rep_gap = 1000;
        assert(Nioh3Training_SetRepeatGapChr(16102u, 500, 0) == 500 && Nioh3Training_GetRepeatGapChr(16102u) == 500 && Nioh3Training_GetRepeatGapChr(42450u) == 1000);
        assert(Nioh3Training_SetRepeatGapChr(16102u, 99999, 0) == MV_REP_GAP_MAX && Nioh3Training_SetRepeatGapChr(0u, 100, 0) == -1);
        g_cfg_rep_gap = 0;
        printf("PASS: repeat armed on %d editable entries (%d unlabelled), OFF moves too, protected / unknown refused, table limit %d\n", armed, unlabelled, MV_REP_MAX);
    }
    // user #14423 / #14462: 'Sequences' repeat of Yoritomo 0x4E7B > 0x4EA7. Only the head is forced (from the gap, never again while a request
    // is pending); the head counts as started only once it held SEQ_CONFIRM_MS or the boss moved on to another move. user #14702 / #14710: while
    // the chain plays, the game's own next piece passes and another attack is refused (MV_KEEP); the round ends back in idle by itself.
    {
        g_tr.practice = 1; g_tr.active = g_bm.active = 1; g_bm.id = 7; g_mv_hooked = 1; o_mvsel = Select; o_mvcom = Commit; g_tr.chr = 16102u; standing = 0;
        assert(Nioh3Training_SetRepeatGapChr(16102u, 500, 0) == 500);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0x4E7B, 1) == 1 && Nioh3Training_GetMoveRepeat(16102u) == 0x4E7B && Nioh3Training_GetMoveRepeatSeq(16102u) == 1);
        int ri = MvRepFind(16102u); assert(ri >= 0 && g_rep_nchain[ri] == 2 && g_rep_chain[ri][0] == 0x4E7B && g_rep_chain[ri][1] == 0x4EA7);
        uint32_t c5[MV_SEQ_MAX]; assert(MvSeqBuild(16102u, 0x4E5C, c5, MV_SEQ_MAX) == 3 && c5[1] == 0x4E60 && c5[2] == 0x4E61);  // same chain as the menu
        assert(MvSeqBuild(16102u, 0x4E30, c5, MV_SEQ_MAX) == 2 && c5[1] == 0x4E31);  // looping part kept once
        // the 'next piece' rule on the decoded records (motion family = motion / 10, next-action links)
        assert(MvMotion(16102u, 0x4E78) == 1102 && MvMotion(16102u, 0x4E8D) == 1104 && MvMotion(16102u, 0x2) == 0 && MvMotion(42450u, 0x4E64) == 0);
        assert(MvContinues(16102u, 0x4E78, 0x4E79) && MvContinues(16102u, 0x4E79, 0x4E7A) && MvContinues(16102u, 0x4E7A, 0x4E8D));  // 1102 > 1103 > 1104 > 1104
        assert(MvContinues(16102u, 0x4E77, 0x4E78) && MvContinues(16102u, 0x4E22, 0x4E23) && MvContinues(16102u, 0x4E60, 0x4E62) && MvContinues(16102u, 0x4EA7, 0x4E7C));
        assert(MvContinues(16102u, 0x4E22, 0x4E22) && !MvContinues(16102u, 0x4E7A, 0x4E7A));       // a loop part goes on; a finished attack is not restarted
        assert(!MvContinues(16102u, 0x4E23, 0x4E5C) && !MvContinues(16102u, 0x4E62, 0x4E68) && !MvContinues(16102u, 0x4E8D, 0x4E5A));  // another attack
        assert(!MvContinues(16102u, 0x4E62, 0x4E61) && !MvContinues(16102u, 0x4E7A, 0x4E78));       // no restart of an earlier piece (1022 > 1020)
        assert(!MvContinues(16102u, 0x4E21, 0x4E3B) && !MvContinues(16102u, 0x4E3D, 0x4E3B));       // #14655: the boss-killing move never
        forced_n = 0;
        // idle, first AI pick (0x4E30): the head. Still idle: keep asking; NOT re-forced every 500 ms while the request is pending
        cur_act = 2; fake_now = 1000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1);
        fake_now = 1100; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        fake_now = 1600; MvRepTick(); fake_now = 2100; MvRepTick(); assert(forced_n == 0 && g_rep_phase[ri] == 1);
        // never taken within SEQ_START_MS: counted as not started, gap, then the pump forces it once
        fake_now = 2500; MvRepTick(); assert(g_rep_phase[ri] == 0 && g_rep_end[ri] == 2500 && forced_n == 0);
        fake_now = 2700; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2);  // gap: idle
        fake_now = 2999; MvRepTick(); assert(forced_n == 0);
        fake_now = 3000; MvRepTick(); assert(forced_n == 1 && forced_id == 0x4E7B && cur_act == 0x4E7B && g_rep_phase[ri] == 1);
        // live 22:39 repro: the action reads the head right after the force, then the AI's 0x4E30 pick and idle 0x2 within a frame: not an end
        fake_now = 3005; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1);
        cur_act = 2; fake_now = 3016; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1 && g_rep_end[ri] == 2500);
        for (DWORD t = 3020; t < 3500; t += 17) { fake_now = t; MvRepTick(); assert(forced_n == 1 && g_rep_phase[ri] == 1); }
        // the head really plays: read for SEQ_CONFIRM_MS -> started
        cur_act = 0x4E7B; fake_now = 3500; MvRepTick(); assert(g_rep_phase[ri] == 1);
        fake_now = 3599; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1);
        fake_now = 3600; MvRepTick(); assert(g_rep_phase[ri] == 2 && g_rep_kmax[ri] == 0);
        // head plays: AI picks of other attacks (0x4E30 / 0x4E55) are refused, the head plays on (was: replaced by the next part); nothing forced
        for (DWORD t = 3610; t < 5000; t += 17) { fake_now = t; uint32_t p = t & 1 ? 0x4E30 : 0x4E55; Pick(p); assert(h_mvsel(0x3000, 0) == MV_KEEP); MvRepTick(); assert(forced_n == 1); }
        assert(g_rep_phase[ri] == 2 && g_rep_end[ri] == 2500);
        cur_act = 0xFFFF; fake_now = 5010; MvRepTick(); assert(forced_n == 1 && g_rep_phase[ri] == 2);  // transition frame keeps the state
        // the head's end picks its link 0x4EA7 while that part is switched OFF: returned as is (no reroll / veto cuts the chain)
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 0, 0) == 1);
        cur_act = 0x4E7B; Pick(0x4EA7); assert(h_mvsel(0x3000, 0) == 0x4EA7);
        // an OFF AI pick of another attack during the chain: refused (no veto idle that cut the chain live, 22:37:56 0x4E64 -> 0x2)
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4E64, 0, 0) == 1);
        Pick(0x4E64); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4E64, 1, 0) == 1 && Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 1, 0) == 1);
        // the last listed part plays: its own next piece 0x4E7C (same motion 1062) passes, also when switched OFF (commit not vetoed); no idle
        cur_act = 0x4EA7; fake_now = 5100; MvRepTick(); assert(g_rep_kmax[ri] == 1 && g_rep_phase[ri] == 2);
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4E7C, 0, 0) == 1);
        Pick(0x4E7C); assert(h_mvsel(0x3000, 0) == 0x4E7C && h_mvcom(0x3000, 0x4E7C, 0, 0, 0) == 1 && committed == 0x4E7C);
        Pick(0x4E30); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        // the native follow-up (live 0x4EA7 > 0x4E7C > 0x18D1) still belongs to the round: other attacks refused, nothing forced, no gap yet
        cur_act = 0x4E7C; fake_now = 5200; MvRepTick(); Pick(0x4E55); assert(h_mvsel(0x3000, 0) == MV_KEEP && g_rep_phase[ri] == 3);
        cur_act = 0x18D1; for (DWORD t = 5210; t < 6000; t += 50) { fake_now = t; Pick(0x4E55); assert(h_mvsel(0x3000, 0) == MV_KEEP); MvRepTick(); assert(forced_n == 1); }
        assert(g_rep_phase[ri] == 3 && g_rep_end[ri] == 2500);
        // back in idle by itself: the sequence has ended, the gap starts now (6000); the passed follow-up is vetoed again next round
        cur_act = 2; fake_now = 6000; MvRepTick(); assert(forced_n == 1 && g_rep_phase[ri] == 0 && g_rep_end[ri] == 6000);
        assert(h_mvcom(0x3000, 0x4E7C, 0, 0, 0) == 0 && committed == 0xFFFF && Nioh3Training_SetMoveAllowed(16102u, 0x4E7C, 1, 0) == 1);
        fake_now = 6200; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2); MvRepTick(); assert(forced_n == 1);
        fake_now = 6499; MvRepTick(); assert(forced_n == 1);
        fake_now = 6500; MvRepTick(); assert(forced_n == 2 && forced_id == 0x4E7B && g_rep_phase[ri] == 1);  // gap passed: head forced once
        // the head branches at once into another move (live 0x4E21 -> 0x4E23 after 16 ms): started
        cur_act = 0x4E7C; fake_now = 6516; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == MV_KEEP && g_rep_phase[ri] == 3 && g_rep_kmax[ri] == 0);
        // a reaction (hit) during the chain: the boss's own business; back in idle = ended
        cur_act = 0x100; fake_now = 6600; MvRepTick(); assert(g_rep_phase[ri] == 3 && forced_n == 2);
        Pick(0x4E30); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        cur_act = 2; fake_now = 7000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2 && g_rep_phase[ri] == 0 && g_rep_end[ri] == 7000);
        fake_now = 7501; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1);
        // a reaction instead of the head (< 0x4E20): not a start; after SEQ_START_MS ended, gap again
        cur_act = 0x100; fake_now = 8000; MvRepTick(); assert(g_rep_phase[ri] == 1);
        fake_now = 9001; MvRepTick(); assert(g_rep_phase[ri] == 0 && g_rep_end[ri] == 9001 && forced_n == 2);
        // user #14462: aggro ON and far: only locomotion passes; an attack pick -> idle (was: the AI fought its way in, live 22:23)
        cur_act = 2; fake_now = 9600; g_mv_chase[1] = 0x3000;
        Pick(0x4E64); assert(h_mvsel(0x3000, 0) == 2);
        Pick(0x4E27); assert(h_mvsel(0x3000, 0) == 0x4E27);  // kind 'M' walk
        MvRepTick(); assert(forced_n == 2);                     // nothing forced from range
        g_mv_chase[1] = 0;
        // user #14462 (live 22:23:12): a new boss of the same character while the repeat is armed: stale state (phase, AI state) is dropped,
        // the boss stands idle for one gap, then the head
        g_rep_phase[ri] = 3; g_rep_st[ri] = 0x7777; g_rep_last[ri] = 1;
        fake_now = 20000; assert(MvRepFresh(16102u) == 1 && g_rep_phase[ri] == 0 && g_rep_st[ri] == 0 && g_rep_end[ri] == 20000);
        assert(MvRepFresh(42450u) == 0);
        fake_now = 20100; Pick(0x4E64); assert(h_mvsel(0x3000, 0) == 2);
        fake_now = 20500; Pick(0x4E64); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1);
        // user #14712 (live 23:48:38 / 00:15:47): the head asked for while the boss still plays the old action (0x4E7D): that is not 'the head
        // branched' (was: started 0 ms after the request); picks keep asking for the head until it plays
        g_rep_phase[ri] = 0; g_rep_end[ri] = 0; cur_act = 0x4E7D; fake_now = 21000; Pick(0x4E77); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1 && g_rep_from[ri] == 0x4E7D);
        fake_now = 21050; Pick(0x4E77); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1);
        cur_act = 0x4E7B; fake_now = 21060; MvRepTick(); fake_now = 21160; MvRepTick(); assert(g_rep_phase[ri] == 2);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0, 0) == 1);
        puts("PASS: sequence repeat (#14423 / #14462 / #14712): head confirmed before it counts (not the old action), no re-force while pending, other attacks refused, own follow-up passes, gap from idle, aggro walk-up, new boss resets");
    }
    // user #14702 / #14710 / #14712: offline replay of the live log (captures/pc/sequence-14702/training-tail.txt, repeats 39-42, 00:38:20-00:38:28).
    // Listed 0x4E78 > 0x4E7A. Live (old): forced head; at +1078 ms three game picks 0x4E79 -> 'part 2 of 2 0x4E7A'; 0x4E7A plays; at +1468 ms
    // the game pick 0x4E8D -> idle ('all 2 part(s) played'); 0x4E7A ran only 375-422 ms, round 1468-1547 ms. Now: 0x4E79 and 0x4E8D pass (the
    // attack's own pieces), the AI's 0x4E5A after them is refused, the round ends when the boss is back in idle by itself, gap 500 ms, head again.
    {
        g_tr.practice = 1; g_tr.active = g_bm.active = 1; g_bm.id = 7; g_mv_hooked = 1; o_mvsel = Select; o_mvcom = Commit; g_tr.chr = 16102u; standing = 1;
        assert(Nioh3Training_SetRepeatGapChr(16102u, 500, 0) == 500);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0x4E78, 1) == 1);
        // the listed chain is 0x4E78 > 0x4E7A (+0xE0 links) or, with the #14702 table override (308067b8), 0x4E78 > 0x4E79 > 0x4E7A: same result
        const int ri = MvRepFind(16102u), k79 = MvSeqIndex(ri, 0x4E79), k7a = MvSeqIndex(ri, 0x4E7A);
        assert(g_rep_chain[ri][0] == 0x4E78 && k7a == g_rep_nchain[ri] - 1 && (k79 < 0 ? g_rep_nchain[ri] == 2 : k79 == 1));
        // the boss arrives in idle; MvRepTick needs its AI state: one idle pick in the gap (as live: the gap idle before #39)
        g_rep_end[ri] = 100000; cur_act = 2; fake_now = 100010; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2);
        forced_n = 0;
        for (int rep = 39; rep <= 42; ++rep) {
            const DWORD t0 = g_rep_end[ri] + 500;  // the gap has passed
            fake_now = t0 - 1; MvRepTick(); assert(forced_n == (uint32_t)(rep - 39));
            fake_now = t0; MvRepTick(); assert(forced_n == (uint32_t)(rep - 38) && forced_id == 0x4E78 && g_rep_phase[ri] == 1);
            // 0x4E78 plays (ACTLOG +23 ms; 'part 1 of 2 plays' +125 ms)
            for (DWORD t = t0 + 16; t < t0 + 1078; t += 16) { fake_now = t; MvRepTick(); }
            assert(g_rep_phase[ri] == 2 && g_rep_cur[ri] == 0x4E78);
            // +1078: three game picks 0x4E79 in one frame (old: -> 0x4E7A). Pass as is
            fake_now = t0 + 1078; for (int k = 0; k < 3; ++k) { Pick(0x4E79); assert(h_mvsel(0x3000, 0) == 0x4E79); }
            cur_act = 0x4E79; for (DWORD t = t0 + 1094; t < t0 + 1500; t += 16) { fake_now = t; MvRepTick(); }
            assert(g_rep_phase[ri] == (k79 >= 0 ? 2 : 3) && g_rep_cur[ri] == 0x4E79);
            // 0x4E79 hands over to its link 0x4E7A (listed part 2) by itself; a game pick of it passes too
            fake_now = t0 + 1500; Pick(0x4E7A); assert(h_mvsel(0x3000, 0) == 0x4E7A);
            cur_act = 0x4E7A; for (DWORD t = t0 + 1516; t < t0 + 1900; t += 16) { fake_now = t; MvRepTick(); }
            assert(g_rep_phase[ri] == 2 && g_rep_kmax[ri] == k7a);
            // ~400 ms into 0x4E7A: game picks 0x4E8D (same motion 1104). Old: 'all 2 part(s) played -> idle'. Now it passes, never idle
            fake_now = t0 + 1900; for (int k = 0; k < 3; ++k) { Pick(0x4E8D); assert(h_mvsel(0x3000, 0) == 0x4E8D); }
            assert(g_rep_phase[ri] == 2 && g_rep_end[ri] != t0 + 1900);
            cur_act = 0x4E8D;
            // 0x4E8D runs ~3.2 s natively (ACTLOG 0x2 prev 0x4E8D ran 32xx ms); the AI's 0x4E5A (another attack, live 0x4E8D > 0x4E5A) is refused
            int c = 0;
            for (DWORD t = t0 + 1916; t < t0 + 5100; t += 16, ++c) {
                fake_now = t; MvRepTick(); assert(forced_n == (uint32_t)(rep - 38) && g_rep_phase[ri] == 3);
                if ((c & 3) == 0) { Pick(0x4E5A); assert(h_mvsel(0x3000, 0) == MV_KEEP); }
            }
            // back in idle by itself: ended at that moment (round 5.1 s instead of ~1.5 s), the gap starts
            cur_act = 2; fake_now = t0 + 5100; MvRepTick(); assert(g_rep_phase[ri] == 0 && g_rep_end[ri] == t0 + 5100);
            fake_now = t0 + 5300; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2);  // gap: idle
        }
        assert(forced_n == 4 && g_rep_count[ri] == 4);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0, 0) == 1);
        // live 22:36 (old pass-through, user #14462 'random stuff'): 0x4E21 > 0x4E23 > 0x4E5C > 0x4E60 > 0x4E62 > 0x4E68. Now the 0x4E21 round is
        // 0x4E21 > 0x4E23 (the same attack, motion 10 > 12); 0x4E5C / 0x4E68 are refused, back in idle = ended
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0x4E21, 1) == 1);
        const int r2 = MvRepFind(16102u); assert(g_rep_nchain[r2] == 2 && g_rep_chain[r2][1] == 0x4E22);
        forced_n = 0; int heads = 0;
        for (int round = 0; round < 3; ++round) {
            const DWORD t0 = 130000 + (DWORD)round * 3000;
            cur_act = 2; fake_now = t0; Pick(0x4E2D); heads += h_mvsel(0x3000, 0) == 0x4E21;
            cur_act = 0x4E21; fake_now = t0 + 1; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E21);
            fake_now = t0 + 20; Pick(0x4E23); assert(h_mvsel(0x3000, 0) == 0x4E21);             // still a request (head not confirmed yet)
            cur_act = 0x4E23; fake_now = t0 + 32; MvRepTick(); assert(g_rep_phase[r2] == 3);     // branched into its own finisher
            fake_now = t0 + 94; Pick(0x4E5C); assert(h_mvsel(0x3000, 0) == MV_KEEP);              // live: 0x4E5C played here
            fake_now = t0 + 200; Pick(0x4E68); assert(h_mvsel(0x3000, 0) == MV_KEEP);
            for (DWORD t = t0 + 210; t < t0 + 700; t += 16) { fake_now = t; Pick(0x4E5C); assert(h_mvsel(0x3000, 0) == MV_KEEP); MvRepTick(); }
            cur_act = 2; fake_now = t0 + 700; MvRepTick(); assert(g_rep_phase[r2] == 0 && g_rep_end[r2] == t0 + 700);
            for (DWORD t = t0 + 716; t < t0 + 1200; t += 16) { fake_now = t; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2); }
            assert(forced_n == 0);
            fake_now = t0 + 1200; MvRepTick(); assert(forced_n == 1 && g_rep_phase[r2] == 1);  // the gap passed in idle: next round's head
            forced_n = 0; g_rep_phase[r2] = 0;  // next round starts from the AI pick again
        }
        assert(heads == 3);
        // the loop part 0x4E22: the game's exit 0x4E23 passes; a loop that never ends by itself takes the game's pick after MV_STUCK_MS,
        // never a boss-killing move (#14655)
        cur_act = 2; fake_now = 150000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E21);
        cur_act = 0x4E22; fake_now = 150010; MvRepTick(); assert(g_rep_phase[r2] == 2);
        fake_now = 150100; Pick(0x4E5C); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        Pick(0x4E22); assert(h_mvsel(0x3000, 0) == 0x4E22);
        fake_now = 150010 + MV_STUCK_MS - 1; Pick(0x4E5C); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        fake_now = 150010 + MV_STUCK_MS; Pick(0x4E3B); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        Pick(0x4E5C); assert(h_mvsel(0x3000, 0) == 0x4E5C);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0, 0) == 1);
        // 0x4E5C > 0x4E60 > 0x4E61 (live: 'AI pick 0x4E62 -> part 3 of 3 0x4E61', then 'all 3 part(s) played, AI pick 0x4E62 -> idle'): the
        // finisher 0x4E62 passes, the AI's 0x4E68 after it is refused
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0x4E5C, 1) == 1);
        const int r3 = MvRepFind(16102u); assert(g_rep_nchain[r3] == 3);
        cur_act = 2; fake_now = 160000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E5C);
        cur_act = 0x4E5C; fake_now = 160010; MvRepTick(); fake_now = 160110; MvRepTick(); assert(g_rep_phase[r3] == 2);
        fake_now = 161700; Pick(0x4E60); assert(h_mvsel(0x3000, 0) == 0x4E60);
        cur_act = 0x4E60; fake_now = 161720; MvRepTick();
        fake_now = 162300; Pick(0x4E62); assert(h_mvsel(0x3000, 0) == 0x4E62);
        cur_act = 0x4E62; fake_now = 162320; MvRepTick(); assert(g_rep_phase[r3] == 3);
        fake_now = 163000; Pick(0x4E68); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        Pick(0x4E61); assert(h_mvsel(0x3000, 0) == MV_KEEP);  // an earlier listed part is not restarted
        cur_act = 2; fake_now = 163500; MvRepTick(); assert(g_rep_phase[r3] == 0 && g_rep_end[r3] == 163500);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0, 0) == 1);
        standing = 0;
        puts("PASS: live #14702 replay (0x4E78 > 0x4E79 > 0x4E7A > 0x4E8D plays to idle, 4 rounds, gap from idle), 0x4E21 > 0x4E23 without 'random stuff', 0x4E5C > 0x4E60 > 0x4E62, loop guard, killer refused");
    }
    // user #14710: a single-move repeat follows its move the same way (was: the first pick after it = 'finished' -> gap idle, live 22:17:24
    // 0x4EE8 > 0x4EE9 cut, 0x4E7B > 0x4EA7 cut by the 0x4E7C pick)
    {
        g_tr.practice = 1; g_tr.active = g_bm.active = 1; g_bm.id = 7; g_mv_hooked = 1; o_mvsel = Select; o_mvcom = Commit; g_tr.chr = 16102u; standing = 0;
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0x4E7B, 1) == 1); int ri = MvRepFind(16102u);
        // a part switched OFF is not vetoed at commit while its sequence repeats; a single-move repeat of the head vetoes it again
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 0, 0) == 1);
        assert(h_mvcom(0x3000, 0x4EA7, 0, 0, 0) == 1 && committed == 0x4EA7);
        assert(Nioh3Training_SetMoveRepeat(16102u, 0x4E7B, 1) == 1 && Nioh3Training_GetMoveRepeatSeq(16102u) == 0);
        assert(h_mvcom(0x3000, 0x4EA7, 0, 0, 0) == 0 && committed == 0xFFFF);
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 1, 0) == 1);
        ri = MvRepFind(16102u);
        cur_act = 2; fake_now = 200000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        fake_now = 200001; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);    // same frame, still idle: keep asking (not 'finished')
        cur_act = 0x4E7B; fake_now = 200020; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == MV_KEEP);  // another attack: refused, the move plays on
        assert(!g_rep_end[ri]);
        fake_now = 201650; Pick(0x4EA7); assert(h_mvsel(0x3000, 0) == 0x4EA7);   // its link
        cur_act = 0x4EA7; fake_now = 202100; Pick(0x4E7C); assert(h_mvsel(0x3000, 0) == 0x4E7C);  // its own next piece (motion 1062)
        cur_act = 0x4E7C; fake_now = 202500; Pick(0x4E55); assert(h_mvsel(0x3000, 0) == MV_KEEP && !g_rep_end[ri]);
        // back in idle by itself: the first pick there ends the move, gap idle
        cur_act = 2; fake_now = 203000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2 && g_rep_end[ri] == 203000);
        fake_now = 203400; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2);
        fake_now = 203500; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        // the move started by somebody else (Stand still drill forced it): its follow-up picks are followed, not turned into the move again
        cur_act = 2; fake_now = 203600; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        cur_act = 0x4E7B; fake_now = 203620; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == MV_KEEP);
        cur_act = 2; fake_now = 205000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2);
        fake_now = 205600; g_rep_end[ri] = 205000; cur_act = 0x4E7B;  // drill-forced after the gap
        Pick(0x4EA7); assert(h_mvsel(0x3000, 0) == 0x4EA7 && !g_rep_end[ri]);
        // a pick that never takes: after SEQ_START_MS the move counts as ended (no endless asking)
        cur_act = 2; fake_now = 206000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2 && g_rep_end[ri] == 206000);
        fake_now = 206600; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        fake_now = 206600 + SEQ_START_MS; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2 && g_rep_end[ri] == 206600 + SEQ_START_MS);
        // a reaction (hit) instead of the move: it has ended (gap idle)
        fake_now = 208700; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        cur_act = 0x100; fake_now = 208800; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2 && g_rep_end[ri] == 208800);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0, 0) == 1 && Nioh3Training_GetMoveRepeat(16102u) == 0 && Nioh3Training_GetMoveRepeatSeq(16102u) == 0);
        cur_act = 0xFFFFFFFFu; g_cfg_rep_gap = 0;
        puts("PASS: single-move repeat (#14710): other attacks refused while the move plays, its link / own next piece pass, ends back in idle, drill-forced move followed");
    }
    // A saved block and repeat must resume after a match without being overwritten.
    g_tr.active=1;g_tr.practice=1;g_tr.chr=16102u;g_mv_hooked=1;
    assert(Nioh3Training_SetMoveAllowed(16102u,0x4E21,0,0)==1);
    assert(Nioh3Training_SetMoveRepeatSeq(16102u,0x4E21,1)==1);
    assert(MvBlocked(16102u,0x4E21) && MvRepeatId(16102u)==0x4E21);
    g_tr.practice=0;
    assert(!MvBlocked(16102u,0x4E21) && !MvRepeatId(16102u));
    assert(!Nioh3Training_GetMoveRepeatSeq(16102u));
    assert(!MvRepeatPick(0x3000,0x4E21));
    Pick(0x4E21);assert(h_mvsel(0x3000,0)==0x4E21);
    { int ri=MvRepFind(16102u);g_rep_st[ri]=0x3000;cur_act=2;
      const uint32_t before=forced_n;MvRepTick();assert(forced_n==before); }
    g_tr.practice=1;
    assert(MvBlocked(16102u,0x4E21) && MvRepeatId(16102u)==0x4E21);
    assert(Nioh3Training_GetMoveRepeatSeq(16102u));
    puts("PASS: Boss Match suppresses saved repeat and move veto; Training preferences survive");
    printf("PASS: %d catalogue entries; %d toggle/veto cases; %d protected; Yoritomo %d; ownership, reroll, commit, persistence, deferred install, mismatch\n", N3_MOVE_COUNT, tested, protected_count, yoritomo);
    return 0;
}
