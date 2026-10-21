// user #22389: Practice stats, game side. Included by training.c after boss_vitals_game.h; PstTick runs on the game thread (h_pump, after BvTick).
// Decisions: practice_stats.h. Menu: Boss Rematch practice_stats_menu.h (Practice > Player page and the Boss pages).
// Levers (notes/practice-stats-22389.md has the evidence table):
//   Player infinite Ki : player status st = [player+0x3A0] (the HP fields PlayerRefill writes, PROVEN). Ki = st+0xAC cur / +0xB0 max (floats), set with
//                        KiSetCur 2F88D0(st+0x98, v): the boss Ki fields / setter of BossVitals, assumed the same for the player (same status class;
//                        the player read is guarded by sanity bounds and logged once so the first live run shows the numbers).
//   Damage multipliers : PstDmgScale in NfHpDamage (HP damage 787268 at the hit apply 329890, HpDmgHookInstall), training.c.
//   Boss Ki drain      : Training bosses (BvActor) Ki lowered with KiSetCur at 20 Hz, floor 0.5, never during a Ki break / HP-0 window / infinite Ki.
// Active only while a Practice session runs or a Training boss is alive (g_pst_live), so the normal game is untouched even with a value saved.
static volatile LONG g_pst_logs; static DWORD g_pst_t; static int g_pst_kiFail, g_pst_kiSeen; static int g_pst_drainOn[N3T_SLOTS];
static void PstLog(const char* fmt, ...) { if (InterlockedIncrement(&g_pst_logs) > 200) return; char m[400]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m); }
static const char* PstKey(int what) {
    return what == PS_PLAYER_INF_KI ? "PlayerInfiniteKi" : what == PS_DMG_DEALT ? "DamageDealtPct" : what == PS_DMG_TAKEN ? "DamageTakenPct" : "BossKiDrainPct";
}
static void PstLoadIni(const char* ini) {
    for (int i = 0; i < PS_N; ++i) g_pst.v[i] = PstClamp(i, (int)GetPrivateProfileIntA("Practice", PstKey(i), PstDefault(i), ini));
    Log("PRACTICE stats (#22389): PlayerInfiniteKi %d, DamageDealtPct %d, DamageTakenPct %d, BossKiDrainPct %d (boss speed / infinite ninjutsu: not available yet)",
        g_pst.v[0], g_pst.v[1], g_pst.v[2], g_pst.v[3]);
}
static DWORD WINAPI PstSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[16]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    for (int i = 0; i < PS_N; ++i) { snprintf(v, sizeof v, "%d", g_pst.v[i]); WritePrivateProfileStringA("Practice", PstKey(i), v, ini); }
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
static __attribute__((noinline)) void PstPlayerKi(void) {
    uint64_t me = PlayerChar(), st = 0;
    if (!me || !Rd64(me + 0x3A0, &st) || !st) return;
    float kc = 0, km = 0, back = -1;
    __try { kc = *(volatile float*)(st + 0xAC); km = *(volatile float*)(st + 0xB0); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!g_pst_kiSeen) { g_pst_kiSeen = 1; PstLog("PRACTICE infinite Ki: player Ki read %.1f / %.1f (st %p +0xAC / +0xB0)", kc, km, (void*)st); }
    if (!PstPlayerKiRefill(1, kc, km)) {
        if (!(km > 0.0f) || km > 100000.0f) { if (g_pst_kiFail < 3) { g_pst_kiFail = 3; PstLog("PRACTICE infinite Ki: player Ki max %.1f looks wrong, not written", km); } }
        return;
    }
    __try { ((void (*)(uint64_t, float))(B + 0x2F88D0))(st + 0x98, km); back = *(volatile float*)(st + 0xAC); } __except (EXCEPTION_EXECUTE_HANDLER) { back = -1; }
    if (back > km - 0.5f) { if (g_pst_kiSeen == 1) { g_pst_kiSeen = 2; PstLog("PRACTICE infinite Ki applied: player Ki %.1f -> %.1f / %.1f (logged once)", kc, back, km); } return; }
    if (++g_pst_kiFail <= 3) PstLog("PRACTICE infinite Ki: player Ki write did not stick (%.1f, try %d of 3)%s", back, g_pst_kiFail, g_pst_kiFail >= 3 ? ": off until restart" : "");
}
static __attribute__((noinline)) void PstBossDrain(int s, int dt) {
    const uint64_t a = BvActor(s);
    uint64_t st = 0, ai = 0; float kc = 0, km = 0, t = 0; int dead = 0, broken = 0;
    if (!a || !Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai) { g_pst_drainOn[s] = 0; return; }
    __try {
        dead = *(volatile int32_t*)(st + 0x48) == 1 || !*(volatile uint64_t*)(st + 0x50);
        kc = *(volatile float*)(st + 0xAC); km = *(volatile float*)(st + 0xB0);
        broken = KbState(ai, 0x23) || ((uint8_t (*)(uint64_t))(B + 0x844FE4))(st + 0x98) || kc <= 0.0f;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    const int nf = BvNfSlot(a), window = nf >= 0 && g_nf[nf].win == 1;
    if (!PstKiDrain(g_pst.v[PS_BOSS_KI_DRAIN], dt, dead, window, broken, g_bv[s].infKi, kc, km, &t)) return;
    __try { ((void (*)(uint64_t, float))(B + 0x2F88D0))(st + 0x98, t); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!g_pst_drainOn[s]) { g_pst_drainOn[s] = 1; PstLog("PRACTICE Ki drain Boss %c %p: %d %% of max Ki per second (Ki %.1f / %.1f -> %.1f)", 'A' + s, (void*)a, g_pst.v[PS_BOSS_KI_DRAIN], kc, km, t); }
}
static void PstTick(void) {
    const DWORD now = GetTickCount();
    if (now - g_pst_t < 50) return;
    const int dt = g_pst_t ? (int)(now - g_pst_t) : 50; g_pst_t = now;
    if (GameState() && GameState()[2]) return;  // loading
    int live = g_tr.active != 0;
    for (int s = 0; s < N3T_SLOTS && !live; ++s) if (BvActor(s)) live = 1;
    if (live != g_pst_live) { InterlockedExchange(&g_pst_live, live); InterlockedExchange(&g_pst_logs, 0); g_pst_kiSeen = 0; }
    if (!live) return;
    if ((g_pst.v[PS_DMG_DEALT] != 100 || g_pst.v[PS_DMG_TAKEN] != 100) && !g_hpdmg_hooked) HpDmgHookInstall("damage multipliers (#22389)");
    if (g_pst.v[PS_PLAYER_INF_KI] && g_pst_kiFail < 3) PstPlayerKi();
    if (g_pst.v[PS_BOSS_KI_DRAIN] > 0) for (int s = 0; s < N3T_SLOTS; ++s) PstBossDrain(s, dt);
}
// C API for Boss Rematch's Practice pages. what = PS_PLAYER_INF_KI / PS_DMG_DEALT / PS_DMG_TAKEN / PS_BOSS_KI_DRAIN.
// Get: the value (-1 = bad what). Set: value clamped (PS_PLAYER_INF_KI: -1 flips), persist = save [Practice] in Nioh3Training.ini; returns the new value.
N3T_API int Nioh3Training_PracticeStatGet(int what) { return what >= 0 && what < PS_N ? g_pst.v[what] : -1; }
N3T_API int Nioh3Training_PracticeStatSet(int what, int value, int persist) {
    if (what < 0 || what >= PS_N) return -1;
    if (what == PS_PLAYER_INF_KI && value < 0) value = !g_pst.v[what];
    const int v = g_pst.v[what] = PstClamp(what, value);
    if (what == PS_PLAYER_INF_KI) g_pst_kiFail = 0, g_pst_kiSeen = 0;
    if (what == PS_DMG_DEALT || what == PS_DMG_TAKEN) InterlockedExchange(&g_pst_dmg_logs, 0);
    if (what == PS_BOSS_KI_DRAIN) for (int s = 0; s < N3T_SLOTS; ++s) g_pst_drainOn[s] = 0;
    InterlockedExchange(&g_pst_logs, 0);
    Log("PRACTICE stats: %s -> %d%s", PstKey(what), v, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, PstSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return v;
}
