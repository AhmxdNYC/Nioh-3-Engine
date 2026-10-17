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
static struct { int active; uint32_t id; } g_bm;
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

static void Pick(uint32_t id) {
    selection_calls = 0;
    for (int i = 0; i < 10; ++i) selected[i] = id;
}
int main(void) {
    assert(N3_MOVE_COUNT == 202);
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
    puts("PASS: zero-session startup; 202 chr=0 queries/setters rejected without hooks");
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
    assert(yoritomo == 14 && tested + protected_count == 202);
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
    printf("PASS: %d catalogue entries; %d toggle/veto cases; %d protected; Yoritomo %d; ownership, reroll, commit, persistence, deferred install, mismatch\n", N3_MOVE_COUNT, tested, protected_count, yoritomo);
    return 0;
}
