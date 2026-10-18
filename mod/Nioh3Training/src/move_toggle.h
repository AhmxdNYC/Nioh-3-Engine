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
static uint32_t IhSelect(uint64_t st, uint32_t id);

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
    // bridge 12666: every spawned boss is matched by its own AI controller, so two bosses each use their own move list
    if (!st) return 0;
    uint64_t me = PlayerChar();
    for (int i = 0; i < g_nsp; ++i) {
        uint64_t actor = g_sp_actor[i], ai = 0, ctrl = 0;
        if (!actor || actor == me || !g_sp_chr[i]) continue;
        if (Rd64(actor + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl == st) return g_sp_chr[i];
    }
    if (!g_tr.active || !g_bm.active) return 0;
    uint64_t actor = g_bm.actor ? g_bm.actor : ActorByHandle(g_bm.id), ai = 0, ctrl = 0;
    if (!actor || actor == me || !Rd64(actor + 0x3B0, &ai) || !ai ||
        !Rd64(ai + 8, &ctrl) || ctrl != st) return 0;
    return g_tr.chr;
}
// bridge 12953: Repeat move (SF6-style). Per boss character one repeated action: every time that boss's AI picks its next catalogued
// move, the pick becomes the repeated one (overrides Idle stance and OFF switches for that one id). RepeatGapMs (ini [Training]) > 0 lets
// the AI's own pick through until that many ms passed since the last repeat. Only ordinary catalogued attacks (id >= 0x4E20)
// plus the idle pick (0x2) are replaced, so reactions, Ki break and status actions still play natively.
#define MV_REP_MAX 8
static volatile LONG g_rep_chr[MV_REP_MAX], g_rep_id[MV_REP_MAX], g_rep_count[MV_REP_MAX]; static DWORD g_rep_last[MV_REP_MAX], g_rep_end[MV_REP_MAX];
static volatile LONG g_rep_gaps; static uint64_t g_rep_st[MV_REP_MAX];
// bridge 12958: delay between repeats (Training menu slider on the move page, [Training] RepeatGapMs, default 1000). Measured from the end of the
// repeated move (the boss's first new pick after it); until it has passed the boss is given its idle (0x2) instead of the AI's own attack.
static volatile int g_cfg_rep_gap = 1000;
#define MV_REP_GAP_MAX 10000
static int MvRepFind(uint32_t chr) { for (int i = 0; i < MV_REP_MAX; ++i) if (chr && (uint32_t)g_rep_chr[i] == chr) return i; return -1; }
static uint32_t MvRepeatId(uint32_t chr) { int i = MvRepFind(chr); return i >= 0 ? (uint32_t)g_rep_id[i] : 0; }
static uint32_t MvRepeatPick(uint64_t st, uint32_t id) {
    if (id < 0x4E20 && id != 2) return 0;  // idle picks too: a calm / out-of-range boss only re-picks idle (live 19:10, every 6 s)
    uint32_t chr = MvBossChr(st); int i = MvRepFind(chr); if (i < 0) return 0;
    uint32_t rep = (uint32_t)g_rep_id[i]; if (!rep || N3MoveFind(chr, id) < 0) return 0;
    DWORD now = GetTickCount(); int gap = g_cfg_rep_gap;
    if (g_rep_last[i] && !g_rep_end[i]) g_rep_end[i] = now ? now : 1;  // first pick after a repeat = that move has finished
    if (gap > 0 && g_rep_end[i] && now - g_rep_end[i] < (DWORD)gap) {
        LONG g = InterlockedIncrement(&g_rep_gaps);
        if (g <= 500) Log("REPEAT chr %u gap hold pick 0x%X -> idle (%lu/%d ms since end)", chr, id, now - g_rep_end[i], gap);
        g_rep_st[i] = st;  // MvRepTick ends the idle once the gap has passed (the native idle lasts ~6 s, live 19:35)
        return id == 2 ? 0 : (N3MoveFind(chr, 2) >= 0 ? 2 : 0);
    }
    LONG n = InterlockedIncrement(&g_rep_count[i]);
    if (n <= 500) Log("REPEAT chr %u pick 0x%X -> 0x%X (#%ld, gap %d ms, %lu ms since prev end, state %p)", chr, id, rep, n, gap,
                      g_rep_end[i] ? now - g_rep_end[i] : 0UL, (void*)st);
    g_rep_last[i] = now; g_rep_end[i] = 0;
    return rep;
}
static uint32_t h_mvsel(uint64_t st, uint64_t tr) {
    uint32_t raw = o_mvsel(st, tr), rep = MvRepeatPick(st, raw);
    if (rep) return rep;
    uint32_t id = IhSelect(st, raw);
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
    uint32_t vchr = MvBossChr(st);
    if (vchr && MvBlocked(vchr, id) && MvRepeatId(vchr) != id) {
        InterlockedIncrement(&g_mv_vetoes);
        if (g_mv_vetoes < 200) Log("MOVETOG commit veto chr %u 0x%X (state %p)", vchr, id, (void*)st);
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
static volatile LONG g_mv_view_slot = -1;  // bridge 12666: the move page shows this slot's boss (-1 = current boss)
N3T_API void Nioh3Training_SetMoveSlot(int slot) { InterlockedExchange(&g_mv_view_slot, slot); }
N3T_API uint32_t Nioh3Training_GetMoveBoss(void) {
    LONG vs = g_mv_view_slot; if (vs == 0 || vs == 1) { uint32_t c = Nioh3Training_SlotChr((int)vs); if (c) return c; }
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
// bridge 12953: on = 1 repeats (chr, id), on = 0 stops that boss's repeat. 1 = done, 0 = unknown move / table full.
N3T_API int Nioh3Training_SetMoveRepeat(uint32_t chr, uint32_t id, int on) {
    int i = MvRepFind(chr);
    if (!on) { if (i >= 0) { Log("REPEAT chr %u 0x%X stopped after %ld repeat(s)", chr, (uint32_t)g_rep_id[i], g_rep_count[i]); InterlockedExchange(&g_rep_id[i], 0); InterlockedExchange(&g_rep_chr[i], 0); g_rep_st[i] = 0; } return 1; }
    if (N3MoveFind(chr, id) < 0 || N3MoveProtected(id)) return 0;
    if (i < 0) for (int k = 0; k < MV_REP_MAX; ++k) if (!g_rep_chr[k]) { i = k; break; }
    if (i < 0) return 0;
    InterlockedExchange(&g_rep_id[i], (LONG)id); InterlockedExchange(&g_rep_count[i], 0); g_rep_last[i] = 0; g_rep_end[i] = 0; InterlockedExchange(&g_rep_chr[i], (LONG)chr);
    InterlockedExchange(&g_mv_install_pending, 1);
    for (int k = 0; k < g_nsp; ++k) { uint64_t a = g_sp_actor[k], ai = 0, ctrl = 0; if (a && g_sp_chr[k] == chr && Rd64(a + 0x3B0, &ai) && ai) Rd64(ai + 8, &ctrl); if (a && g_sp_chr[k] == chr) Log("REPEAT chr %u actor %p ai %p ctrl %p", chr, (void*)a, (void*)ai, (void*)ctrl); }
    Log("REPEAT chr %u 0x%X ON (gap %d ms)", chr, id, g_cfg_rep_gap);
    return 1;
}
N3T_API uint32_t Nioh3Training_GetMoveRepeat(uint32_t chr) { return MvRepeatId(chr); }
static DWORD WINAPI MvRepGapSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[16]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(v, sizeof v, "%d", g_cfg_rep_gap); WritePrivateProfileStringA("Training", "RepeatGapMs", v, ini);
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
// bridge 12958: Training menu 'Repeat delay' slider. Clamped 0..10000 ms, applies to the next pick, persist = save [Training] RepeatGapMs.
N3T_API int Nioh3Training_GetRepeatGap(void) { return g_cfg_rep_gap; }
N3T_API int Nioh3Training_SetRepeatGap(int ms, int persist) {
    if (ms < 0) ms = 0; if (ms > MV_REP_GAP_MAX) ms = MV_REP_GAP_MAX;
    g_cfg_rep_gap = ms;
    Log("REPEAT gap %d ms%s", ms, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, MvRepGapSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return ms;
}
// bridge 12958: game thread (pump). The native idle the boss gets during the gap runs ~6 s before the AI picks again (live 19:35: 1000 ms set,
// 6.0 s measured), so when the gap has passed and the boss is still in idle 0x2 the repeated move is started with SetActionID's forced
// transition 66C4A8(ctrl, id, 0, 0.0) (the KiBreakActFix path).
static __attribute__((noinline)) void MvRepTick(void) {
    DWORD now = GetTickCount();
    for (int i = 0; i < MV_REP_MAX; ++i) {
        uint32_t chr = (uint32_t)g_rep_chr[i], rep = (uint32_t)g_rep_id[i]; uint64_t st = g_rep_st[i];
        if (!chr || !rep || !st || !g_rep_end[i] || now - g_rep_end[i] < (DWORD)g_cfg_rep_gap) continue;
        if (MvBossChr(st) != chr) { g_rep_st[i] = 0; continue; }
        uint32_t cur = ActIdOf(st);
        if (cur != 2) { if (cur != 0xFFFFFFFFu) g_rep_st[i] = 0; continue; }
        int ok = 1;
        __try { ((void (*)(uint64_t, uint64_t, uint64_t, float))(B + 0x66C4A8))(st, (uint64_t)rep, 0, 0.0f); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
        LONG n = InterlockedIncrement(&g_rep_count[i]);
        if (n <= 500) Log("REPEAT chr %u forced 0x%X after %lu ms idle (#%ld, gap %d ms)%s, now 0x%X", chr, rep, now - g_rep_end[i], n, g_cfg_rep_gap, ok ? "" : " FAULTED", ActIdOf(st));
        g_rep_last[i] = now; g_rep_end[i] = 0; g_rep_st[i] = 0;
    }
}
static void MvLoadPrefs(const char* ini) {
    { int gap = (int)GetPrivateProfileIntA("Training", "RepeatGapMs", 1000, ini); g_cfg_rep_gap = gap < 0 ? 0 : gap > MV_REP_GAP_MAX ? MV_REP_GAP_MAX : gap; }
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
    LONG cur = g_tr.active ? (LONG)g_tr.chr : 0;
    if (!cur) for (int i = g_nsp - 1; i >= 0; --i) if (g_sp_actor[i] && g_sp_chr[i]) { cur = (LONG)g_sp_chr[i]; break; }  // bridge 12666: summoned (no session) boss
    InterlockedExchange(&g_mv_current_chr, cur);
    if (InterlockedExchange(&g_mv_install_pending, 0)) MvInstall();
}
