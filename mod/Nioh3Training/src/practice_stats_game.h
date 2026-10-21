// user #22389: Practice stats, game side. Included by training.c after boss_vitals_game.h; PstTick runs on the game thread (h_pump, after BvTick).
// Decisions: practice_stats.h. Menu: Boss Rematch practice_stats_menu.h (Practice > Player page and the Boss pages).
// Levers (notes/practice-stats-22389.md has the evidence table):
//   Player infinite Ki : player status st = [player+0x3A0]. The game's own Player::RecoverStamina binding (23AACA4, slot 397EA60) does
//                        GetPlayer 20A914(0) -> 5FB5C8([p+0x3A0]+0x98): cur +0x14 = max +0x18, i.e. st+0xAC = st+0xB0 (PROVEN, disasm). Same raw write
//                        here (like the ninjutsu gauges, FullNinjutsuGauge), guarded by sanity bounds; the first read is logged once.
//   Damage multipliers : PstDmgScale in NfHpDamage (HP damage 787268 at the hit apply 329890, HpDmgHookInstall), training.c; by attacker side.
//   Boss Ki drain      : Training bosses (BvActor) lose BossKiDrainPerSec Ki per second via KiSetCur at 20 Hz, floor 0.5, never during a Ki break /
//                        HP-0 window / infinite Ki.
// Active only while a Practice session runs or a Training boss is alive (g_pst_live), so the normal game is untouched even with a value saved.
static volatile LONG g_pst_logs; static DWORD g_pst_t; static int g_pst_kiFail, g_pst_kiSeen; static int g_pst_drainOn[N3T_SLOTS];
static void PstLog(const char* fmt, ...) { if (InterlockedIncrement(&g_pst_logs) > 200) return; char m[400]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m); }
static const char* PstKey(int what) {
    return what == PS_FRAME_METER ? "FrameMeter" : what == PS_BOSS_SPEED ? "BossSpeedPct" : what == PS_PLAYER_SPEED ? "PlayerSpeedPct" : what == PS_PLAYER_INF_NIN ? "PlayerInfiniteNinjutsu" : what == PS_PLAYER_INF_KI ? "PlayerInfiniteKi" : what == PS_DMG_DEALT ? "DamageDealtPct" : what == PS_DMG_TAKEN ? "DamageTakenPct" : "BossKiDrainPerSec";
}
static void PstLoadIni(const char* ini) {
    for (int i = 0; i < PS_N; ++i) g_pst.v[i] = PstClamp(i, (int)GetPrivateProfileIntA("Practice", PstKey(i), PstDefault(i), ini));
    Log("PRACTICE stats (#22389): PlayerInfiniteKi %d, DamageDealtPct %d, DamageTakenPct %d, BossKiDrainPerSec %d, BossSpeedPct %d, PlayerInfiniteNinjutsu %d, PlayerSpeedPct %d, FrameMeter %d",
        g_pst.v[0], g_pst.v[1], g_pst.v[2], g_pst.v[3], g_pst.v[4], g_pst.v[5], g_pst.v[6], g_pst.v[7]);
}
static DWORD WINAPI PstSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[16]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    for (int i = 0; i < PS_N; ++i) { snprintf(v, sizeof v, "%d", g_pst.v[i]); WritePrivateProfileStringA("Practice", PstKey(i), v, ini); }
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
static __attribute__((noinline)) void PstPlayerKi(void) {
    uint64_t me = PlayerChar(), st = 0;
    if (!me || !Rd64(me + 0x3A0, &st) || !st) return;
    float kc = 0, km = 0;
    __try {
        volatile float* cur = (volatile float*)(st + 0xAC); kc = *cur; km = *(volatile float*)(st + 0xB0);
        if (!g_pst_kiSeen) { g_pst_kiSeen = 1; PstLog("PRACTICE infinite Ki: player Ki read %.1f / %.1f (st %p +0xAC / +0xB0)", kc, km, (void*)st); }
        if (PstPlayerKiRefill(1, kc, km)) {
            *cur = km;  // = Player::RecoverStamina 5FB5C8(st+0x98): +0x14 = +0x18
            if (g_pst_kiSeen == 1) { g_pst_kiSeen = 2; PstLog("PRACTICE infinite Ki applied: player Ki %.1f -> %.1f (logged once)", kc, km); }
        } else if ((!(km > 0.0f) || km > 100000.0f) && !g_pst_kiFail) { g_pst_kiFail = 1; PstLog("PRACTICE infinite Ki: player Ki max %.1f looks wrong, not written (logged once)", km); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
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
    if (!g_pst_drainOn[s]) { g_pst_drainOn[s] = 1; PstLog("PRACTICE Ki drain Boss %c %p: %d Ki per second (Ki %.1f / %.1f -> %.1f)", 'A' + s, (void*)a, g_pst.v[PS_BOSS_KI_DRAIN], kc, km, t); }
}
// Boss speed (notes/boss-speed-lever-22389.md): entry hook on ComputeMotionSpeed 134E68(ctrl, dl layer, r8b flag) -> xmm0; [ctrl+0xE0] is the actor.
// Only a Training/Rematch boss (BvActor) gets native * pct / 100; everyone else and 100 % pass through. Takes effect at the boss's next motion start / layer refresh.
typedef float (*PstSpdFn)(uint64_t, uint8_t, uint8_t); static PstSpdFn o_pstspd; static volatile LONG g_pst_spd_hooked, g_pst_spd_logs;
static __attribute__((noinline)) int PstIsBossActor(uint64_t a) {
    if (!a) return 0;
    for (int s = 0; s < N3T_SLOTS; ++s) if (BvActor(s) == a) return 1;
    return 0;
}
static float PstSpdDetour(uint64_t ctrl, uint8_t layer, uint8_t flag) {
    const float r = o_pstspd(ctrl, layer, flag);
    const int bp = g_pst.v[PS_BOSS_SPEED], pp = g_pst.v[PS_PLAYER_SPEED];
    if ((bp == 100 && pp == 100) || !g_pst_live) return r;
    uint64_t a = 0;
    if (!Rd64(ctrl + 0xE0, &a) || !a) return r;
    const int pct = a == PlayerChar() ? pp : PstIsBossActor(a) ? bp : 100;
    if (pct == 100) return r;
    const float o = PstSpeedScale(pct, r);
    if (o != r && InterlockedIncrement(&g_pst_spd_logs) <= 20) Log("PRACTICE %s speed %d %%: actor %p layer %d play rate %.3f -> %.3f", a == PlayerChar() ? "player" : "boss", pct, (void*)a, layer, r, o);
    return o;
}
static void PstSpdHookInstall(void) {
    if (g_pst_spd_hooked) return;
    g_pst_spd_hooked = 2;
    // 134E68: mov rax,rsp | mov [rax+8],rbx | mov [rax+10h],rbp | mov [rax+18h],rsi | mov [rax+20h],rdi (19 bytes, no rip-relative operand)
    if (memcmp((void*)(B + 0x134E68), "\x48\x8B\xC4\x48\x89\x58\x08\x48\x89\x68\x10\x48\x89\x70\x18\x48\x89\x78\x20", 19)) { Log("hook ComputeMotionSpeed 134E68: bytes differ, boss speed off"); return; }
    o_pstspd = (PstSpdFn)Hook(0x134E68, 19, (void*)PstSpdDetour); g_pst_spd_hooked = 1;
    Log("hook ComputeMotionSpeed 134E68: on (boss speed #22389)");
}
static __attribute__((noinline)) void PstPlayerNin(void) {
    uint64_t me = PlayerChar(), st = 0;
    if (!me || !Rd64(me + 0x3A0, &st) || !st) return;
    for (int k = 0; k < 3; ++k) {
        __try {
            volatile float* cur = (volatile float*)(st + 0x36AC + k * 0x30); const float c = *cur, m = *(volatile float*)(st + 0x36B0 + k * 0x30);
            if (PstNinjutsuFill(1, c, m)) { *cur = m; if (InterlockedIncrement(&g_pst_logs) <= 6) PstLog("PRACTICE infinite ninjutsu: gauge %d %.1f -> %.1f", k, c, m); }
        } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    }
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
    if (g_pst.v[PS_BOSS_SPEED] != 100 || g_pst.v[PS_PLAYER_SPEED] != 100) PstSpdHookInstall();
    if (g_pst.v[PS_PLAYER_INF_NIN]) PstPlayerNin();
    if (g_pst.v[PS_PLAYER_INF_KI]) PstPlayerKi();
    if (g_pst.v[PS_BOSS_KI_DRAIN] > 0) for (int s = 0; s < N3T_SLOTS; ++s) PstBossDrain(s, dt);
}
// C API for Boss Rematch's Practice pages. what = PS_* (practice_stats.h).
// Get: the value (-1 = bad what). Set: value clamped (PS_PLAYER_INF_KI: -1 flips), persist = save [Practice] in Nioh3Training.ini; returns the new value.
N3T_API int Nioh3Training_PracticeStatGet(int what) { return what >= 0 && what < PS_N ? g_pst.v[what] : -1; }
N3T_API int Nioh3Training_PracticeStatSet(int what, int value, int persist) {
    if (what < 0 || what >= PS_N) return -1;
    if (PstIsToggle(what) && value < 0) value = !g_pst.v[what];
    const int v = g_pst.v[what] = PstClamp(what, value);
    if (what == PS_PLAYER_INF_KI) g_pst_kiFail = 0, g_pst_kiSeen = 0;
    if (what == PS_DMG_DEALT || what == PS_DMG_TAKEN) InterlockedExchange(&g_pst_dmg_logs, 0);
    if (what == PS_BOSS_KI_DRAIN) for (int s = 0; s < N3T_SLOTS; ++s) g_pst_drainOn[s] = 0;
    if (what == PS_BOSS_SPEED || what == PS_PLAYER_SPEED) InterlockedExchange(&g_pst_spd_logs, 0);
    InterlockedExchange(&g_pst_logs, 0);
    Log("PRACTICE stats: %s -> %d%s", PstKey(what), v, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, PstSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return v;
}
