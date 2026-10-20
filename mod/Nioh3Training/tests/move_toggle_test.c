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
static struct { int active; uint32_t chr; } g_tr;
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
    assert(N3_MOVE_COUNT == 361);
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
    puts("PASS: zero-session startup; 361 chr=0 queries/setters rejected without hooks");
    g_tr.active = g_bm.active = 1; g_bm.id = 7;
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
        for (int j = 0; j < N3_MOVE_COUNT; ++j)
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
    assert(yoritomo == 173 && tested + protected_count == 361);
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
        g_tr.active = g_bm.active = 1; g_bm.id = 7; g_mv_hooked = 1; o_mvsel = Select; o_mvcom = Commit; g_cfg_rep_gap = 0;
        for (int i = 0; i < N3_MOVE_COUNT; ++i) {
            const N3Move* r = &kN3Moves[i];
            g_tr.chr = r->chr; standing = i & 1;
            if (N3MoveProtected(r->id)) { assert(Nioh3Training_SetMoveRepeat(r->chr, r->id, 1) == 0 && Nioh3Training_GetMoveRepeat(r->chr) == 0); continue; }
            assert(Nioh3Training_SetMoveAllowed(r->chr, r->id, 0, 0) == 1);   // OFF, then Right: the menu switches it ON, but OFF must still work
            assert(Nioh3Training_SetMoveRepeat(r->chr, r->id, 1) == 1 && Nioh3Training_GetMoveRepeat(r->chr) == r->id);
            Pick(0x4E20 + 0x7FF); selected[0] = r->id == 0x4E21 ? 0x4E22 : 0x4E21;
            for (int k = 0; k < N3_MOVE_COUNT; ++k) if (kN3Moves[k].chr == r->chr && kN3Moves[k].id >= 0x4E20 && kN3Moves[k].id != r->id) { selected[0] = kN3Moves[k].id; break; }
            if (N3MoveFind(r->chr, selected[0]) >= 0) { selection_calls = 0; assert(h_mvsel(0x3000, 0) == r->id); }
            // user #13787: aggro ON and the boss far (its state in g_mv_chase): its own pick passes, the repeat is back once it is close
            if (N3MoveFind(r->chr, selected[0]) >= 0 && selected[0] != r->id) {
                g_mv_chase[1] = 0x3000; selection_calls = 0; assert(h_mvsel(0x3000, 0) == selected[0]);
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
    // user #14423: 'Sequences' repeat of Yoritomo 0x4E7B > 0x4EA7 (live 22:17-22:18: the game picks 0x4E30 after the head and the old code
    // forced 0x4E7B again every ~0.5 s). Only the head is forced; while the chain advances (a member, or the boss's own follow-up) every AI
    // pick passes untouched and nothing is forced; the sequence ends back in idle, then the gap, then the head again.
    {
        g_tr.active = g_bm.active = 1; g_bm.id = 7; g_mv_hooked = 1; o_mvsel = Select; o_mvcom = Commit; g_tr.chr = 16102u; standing = 0;
        assert(Nioh3Training_SetRepeatGapChr(16102u, 500, 0) == 500);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0x4E7B, 1) == 1 && Nioh3Training_GetMoveRepeat(16102u) == 0x4E7B && Nioh3Training_GetMoveRepeatSeq(16102u) == 1);
        int ri = MvRepFind(16102u); assert(ri >= 0 && g_rep_nchain[ri] == 2 && g_rep_chain[ri][0] == 0x4E7B && g_rep_chain[ri][1] == 0x4EA7);
        uint32_t c5[MV_SEQ_MAX]; assert(MvSeqBuild(16102u, 0x4E5C, c5, MV_SEQ_MAX) == 3 && c5[1] == 0x4E60 && c5[2] == 0x4E61);  // same chain as the menu
        assert(MvSeqBuild(16102u, 0x4E30, c5, MV_SEQ_MAX) == 2 && c5[1] == 0x4E31);  // looping part kept once
        forced_n = 0;
        // idle, first AI pick (0x4E30): the head. Still idle: keep asking; the pump forces it after 500 ms in idle.
        cur_act = 2; fake_now = 1000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        fake_now = 1100; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        fake_now = 1400; MvRepTick(); assert(forced_n == 0);
        fake_now = 1600; MvRepTick(); assert(forced_n == 1 && forced_id == 0x4E7B && cur_act == 0x4E7B);
        // head plays: every AI pick (the bug: 0x4E30 -> 0x4E7B every 500 ms) passes as the game's own; no force, no gap
        for (DWORD t = 1620; t < 3300; t += 17) { fake_now = t; uint32_t p = t & 1 ? 0x4E30 : 0x4E55; Pick(p); assert(h_mvsel(0x3000, 0) == p); MvRepTick(); assert(forced_n == 1); }
        assert(g_rep_phase[ri] == 2 && g_rep_end[ri] == 0);
        cur_act = 0xFFFF; fake_now = 3310; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E30); MvRepTick(); assert(forced_n == 1);  // transition frame
        // the head's end picks its link 0x4EA7 while that part is switched OFF: returned as is (no reroll / veto cuts the chain)
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 0, 0) == 1);
        cur_act = 0x4E7B; Pick(0x4EA7); assert(h_mvsel(0x3000, 0) == 0x4EA7);
        Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E30);  // a non-member pick takes the normal path
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 1, 0) == 1);
        // its link 0x4EA7 plays: picks still pass (also the link itself), nothing forced
        cur_act = 0x4EA7; for (DWORD t = 3320; t < 3800; t += 17) { fake_now = t; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E30); Pick(0x4EA7); assert(h_mvsel(0x3000, 0) == 0x4EA7); MvRepTick(); assert(forced_n == 1); }
        // the boss's own follow-up outside the decoded links (live 22:18:09: 0x4EA7 > 0x4E7C > 0x18D1): still the sequence, not overridden
        cur_act = 0x4E7C; fake_now = 3820; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E30); MvRepTick();
        cur_act = 0x18D1; for (DWORD t = 3840; t < 8500; t += 50) { fake_now = t; Pick(0x4E55); assert(h_mvsel(0x3000, 0) == 0x4E55); MvRepTick(); assert(forced_n == 1); }
        assert(g_rep_phase[ri] == 3 && g_rep_end[ri] == 0);
        // back in idle: the sequence has ended, the gap starts now (8500), not at the head's first frame
        cur_act = 2; fake_now = 8500; MvRepTick(); assert(forced_n == 1 && g_rep_phase[ri] == 0 && g_rep_end[ri] == 8500);
        fake_now = 8700; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2); MvRepTick(); assert(forced_n == 1);
        fake_now = 8999; MvRepTick(); assert(forced_n == 1);
        fake_now = 9000; MvRepTick(); assert(forced_n == 2 && forced_id == 0x4E7B && g_rep_phase[ri] == 1);  // gap passed: head forced once
        fake_now = 9020; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E30 && g_rep_phase[ri] == 2);
        // a reaction (hit) during the chain: the boss's own business, never overridden; the sequence ends when it is back in idle
        cur_act = 0x100; fake_now = 9500; MvRepTick(); assert(g_rep_phase[ri] == 3 && forced_n == 2);
        Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E30);
        cur_act = 2; fake_now = 10000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2 && g_rep_phase[ri] == 0 && g_rep_end[ri] == 10000);
        fake_now = 10501; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B && g_rep_phase[ri] == 1);
        // the head never starts (busy in something else for > 3 s): counted as ended, gap again
        cur_act = 0x100; fake_now = 13600; MvRepTick(); assert(g_rep_phase[ri] == 0 && g_rep_end[ri] == 13600 && forced_n == 2);
        // a part switched OFF is not vetoed at commit while its sequence repeats; a single-move repeat of the head vetoes it again
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 0, 0) == 1);
        assert(h_mvcom(0x3000, 0x4EA7, 0, 0, 0) == 1 && committed == 0x4EA7);
        assert(Nioh3Training_SetMoveRepeat(16102u, 0x4E7B, 1) == 1 && Nioh3Training_GetMoveRepeatSeq(16102u) == 0);
        assert(h_mvcom(0x3000, 0x4EA7, 0, 0, 0) == 0 && committed == 0xFFFF);
        assert(Nioh3Training_SetMoveAllowed(16102u, 0x4EA7, 1, 0) == 1);
        // single-move repeat keeps its old behaviour (pick -> the move, then idle in the gap)
        cur_act = 0x4E7B; fake_now = 6000; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 0x4E7B);
        fake_now = 6010; Pick(0x4E30); assert(h_mvsel(0x3000, 0) == 2);
        assert(Nioh3Training_SetMoveRepeatSeq(16102u, 0, 0) == 1 && Nioh3Training_GetMoveRepeat(16102u) == 0 && Nioh3Training_GetMoveRepeatSeq(16102u) == 0);
        cur_act = 0xFFFFFFFFu; g_cfg_rep_gap = 0;
        puts("PASS: sequence repeat 0x4E7B > 0x4EA7: head forced once, game picks / links / own follow-ups pass, gap from idle after the chain, re-forced, OFF part allowed");
    }
    printf("PASS: %d catalogue entries; %d toggle/veto cases; %d protected; Yoritomo %d; ownership, reroll, commit, persistence, deferred install, mismatch\n", N3_MOVE_COUNT, tested, protected_count, yoritomo);
    return 0;
}
