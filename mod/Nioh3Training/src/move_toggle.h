// Included by training.c after the training-session and boss ownership helpers.
#include "../../boss_moves.h"

static uint32_t g_mv_block[64];
static volatile int g_mv_nblock;
static volatile LONG g_mv_vetoes, g_mv_rerolls;
static volatile LONG g_mv_hooked;
static SRWLOCK g_mv_legacy_lock = SRWLOCK_INIT;
static uint32_t g_mv_fallback = 2;
// 0 inherits legacy MoveBlock; 1 explicitly allows; 2 explicitly blocks.
static volatile LONG g_mv_pref[N3_MOVE_COUNT], g_mv_current_chr, g_mv_install_pending;
typedef uint32_t (*MvSelFn)(uint64_t, uint64_t);
typedef uint64_t (*MvComFn)(uint64_t, uint32_t, uint64_t, int, uint64_t);
static MvSelFn o_mvsel;
static MvComFn o_mvcom;

static int MvBlocked(uint32_t chr, uint32_t id) {
    if (N3MoveProtected(id) || id == g_mv_fallback) return 0;
    int index = N3MoveFind(chr, id);
    if (index >= 0) {
        LONG pref = InterlockedCompareExchange(&g_mv_pref[index], 0, 0);
        if (pref) return pref == 2;
    }
    int blocked = 0;
    AcquireSRWLockShared(&g_mv_legacy_lock);
    for (int i = 0; i < g_mv_nblock; ++i) if (g_mv_block[i] == id) { blocked = 1; break; }
    ReleaseSRWLockShared(&g_mv_legacy_lock);
    return blocked;
}
static uint32_t MvBossChr(uint64_t st) {
    // Positive ownership check, not merely "not the player". Unknown ownership passes through.
    if (!st || !g_tr.active || !g_bm.active) return 0;
    uint64_t actor = ActorByHandle(g_bm.id), ai = 0, ctrl = 0;
    if (!actor || actor == PlayerChar() || !Rd64(actor + 0x3B0, &ai) || !ai ||
        !Rd64(ai + 8, &ctrl) || ctrl != st) return 0;
    return g_tr.chr;
}
static uint32_t h_mvsel(uint64_t st, uint64_t tr) {
    uint32_t id = o_mvsel(st, tr);
    if (!MvBlocked(g_tr.chr, id)) return id;
    uint32_t chr = MvBossChr(st);
    if (!chr || !MvBlocked(chr, id)) return id;
    for (int i = 0; i < 8; ++i) {
        uint32_t r = o_mvsel(st, tr);
        if (!MvBlocked(chr, r)) {
            InterlockedIncrement(&g_mv_rerolls);
            if (g_mv_rerolls < 200) Log("MOVETOG reroll chr %u 0x%X -> 0x%X (state %p)", chr, id, r, (void*)st);
            return r;
        }
    }
    InterlockedIncrement(&g_mv_vetoes);
    if (g_mv_vetoes < 200) Log("MOVETOG select veto chr %u 0x%X -> 0x%X (state %p)", chr, id, g_mv_fallback, (void*)st);
    // 0xFFFF made Shingen retry forever; idle lets his AI pick again (#10001).
    return g_mv_fallback;
}
static uint64_t h_mvcom(uint64_t st, uint32_t id, uint64_t tbl, int layer, uint64_t slot) {
    if (MvBlocked(g_tr.chr, id) && MvBossChr(st)) {
        InterlockedIncrement(&g_mv_vetoes);
        if (g_mv_vetoes < 200) Log("MOVETOG commit veto chr %u 0x%X (state %p)", g_tr.chr, id, (void*)st);
        id = 0xFFFF; // native refuses before any write
    }
    return o_mvcom(st, id, tbl, layer, slot);
}
static void MvInstall(void) {
    if (g_mv_hooked) return;
    static const uint8_t selp[15] = {0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xEC,0x30};
    static const uint8_t comp[15] = {0x48,0x8B,0xC4,0x48,0x89,0x58,0x08,0x48,0x89,0x70,0x18,0x48,0x89,0x78,0x20};
    if (memcmp((void*)(B + 0x35EF04), selp, 15) || memcmp((void*)(B + 0x35BC68), comp, 15)) {
        Log("MoveToggle: prologue mismatch (35EF04/35BC68), not installed"); InterlockedExchange(&g_mv_hooked, -1); return;
    }
    o_mvsel = (MvSelFn)Hook(0x35EF04, 15, (void*)h_mvsel);
    o_mvcom = (MvComFn)Hook(0x35BC68, 15, (void*)h_mvcom);
    InterlockedExchange(&g_mv_hooked, o_mvsel && o_mvcom ? 1 : -1);
    Log("MoveToggle hooks %s (35EF04 select, 35BC68 commit)", g_mv_hooked == 1 ? "installed" : "FAILED");
}
static void MvSet(const uint32_t* ids, int n) {
    AcquireSRWLockExclusive(&g_mv_legacy_lock);
    g_mv_nblock = 0;
    for (int i = 0; i < n && i < 64; ++i) g_mv_block[i] = ids[i];
    g_mv_nblock = n > 64 ? 64 : n;
    ReleaseSRWLockExclusive(&g_mv_legacy_lock);
    if (n > 0) MvInstall();
    Log("MoveToggle: %d legacy blocked id(s)%s", g_mv_nblock, g_mv_hooked == 1 ? "" : " (hooks NOT installed)");
    for (int i = 0; i < g_mv_nblock; ++i) Log("  block 0x%X", g_mv_block[i]);
}
static void MvIniKey(uint32_t chr, uint32_t id, char* section, size_t cap, char* key, size_t kcap) {
    snprintf(section, cap, "Moves.%u", chr); snprintf(key, kcap, "0x%X", id);
}
N3T_API uint32_t Nioh3Training_GetMoveBoss(void) {
    return (uint32_t)InterlockedCompareExchange(&g_mv_current_chr, 0, 0);
}
N3T_API int Nioh3Training_GetMoveHookStatus(void) {
    return (int)InterlockedCompareExchange(&g_mv_hooked, 0, 0);
}
N3T_API int Nioh3Training_GetMoveAllowed(uint32_t chr, uint32_t id) {
    if (N3MoveFind(chr, id) < 0) return -1;
    return !MvBlocked(chr, id);
}
N3T_API int Nioh3Training_SetMoveAllowed(uint32_t chr, uint32_t id, int allowed, int persist) {
    int index = N3MoveFind(chr, id);
    if (index < 0 || N3MoveProtected(id) || id == g_mv_fallback) return 0;
    InterlockedExchange(&g_mv_pref[index], allowed ? 1 : 2);
    if (!allowed) InterlockedExchange(&g_mv_install_pending, 1);
    if (persist) {
        char ini[MAX_PATH], section[40], key[24];
        snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
        MvIniKey(chr, id, section, sizeof section, key, sizeof key);
        if (!WritePrivateProfileStringA(section, key, allowed ? "1" : "0", ini)) {
            Log("MOVEMENU save FAILED chr %u action 0x%X error %lu", chr, id, GetLastError());
            return -1; // applied in memory, persistence failed
        }
    }
    Log("MOVEMENU chr %u action 0x%X %s%s", chr, id, allowed ? "ON" : "OFF", persist ? " saved" : "");
    return 1;
}
static void MvLoadPrefs(const char* ini) {
    for (int i = 0; i < N3_MOVE_COUNT; ++i) {
        char section[40], key[24];
        MvIniKey(kN3Moves[i].chr, kN3Moves[i].id, section, sizeof section, key, sizeof key);
        int value = (int)GetPrivateProfileIntA(section, key, -1, ini);
        if (value == 0 || value == 1) {
            InterlockedExchange(&g_mv_pref[i], value ? 1 : 2);
            if (!value && !N3MoveProtected(kN3Moves[i].id)) InterlockedExchange(&g_mv_install_pending, 1);
        }
    }
}
static void MvTick(void) {
    InterlockedExchange(&g_mv_current_chr, g_tr.active ? (LONG)g_tr.chr : 0);
    if (InterlockedExchange(&g_mv_install_pending, 0)) MvInstall();
}
