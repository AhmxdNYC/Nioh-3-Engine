// user #22389: Practice stats, game side. Included by training.c after boss_vitals_game.h; PstTick runs on the game thread (h_pump, after BvTick).
// Decisions: practice_stats.h. Menu: Boss Rematch practice_stats_menu.h (Practice > Player page and the Boss pages).
// Levers (notes/practice-stats-22389.md has the evidence table):
//   Player infinite Ki : player status st = [player+0x3A0]. The game's own Player::RecoverStamina binding (23AACA4, slot 397EA60) does
//                        GetPlayer 20A914(0) -> 5FB5C8([p+0x3A0]+0x98): cur +0x14 = max +0x18, i.e. st+0xAC = st+0xB0 (PROVEN, disasm). Same raw write
//                        here (like the ninjutsu gauges, FullNinjutsuGauge), guarded by sanity bounds; the first read is logged once.
//   Damage multipliers : PstDmgScale in NfHpDamage (HP damage 787268 at the hit apply 329890, HpDmgHookInstall), training.c; by attacker side.
//   Ki damage I deal / One-hit Ki break (user #22588): PstKiDamage on the KiDamage 844604 call at 329E9D in the hit apply 327DEC (PstKiHookInstall).
//   Boss Ki drain      : Training bosses (BvActor) lose BossKiDrainPerSec Ki per second via KiSetCur at 20 Hz, floor 0.5, never during a Ki break /
//                        HP-0 window / infinite Ki.
//   Hold Ki break      : user #22918, PstHoldBreak (break timers K+0x74 / K+0x7C / ctrl+0x1014 kept running, AI held off while a Training boss is broken).
//   Ki break log / Hold Ki break v2 / Ki floor depth: user #23013 (notes/human-boss-negative-ki-23013.md designs 1-3), see 'user #23013' below.
//   Time of day / Weather: user #22942, PstEnvApplyAll from the 39BEA4 call at 39B67D in RenderFrameSettingsApply (PstEnvHookInstall).
// Gameplay stats remain Practice-only (g_pst_live). Time of day / weather are global Display settings (#38871),
// applied only to the active gameplay sky outside loading and event scenes; existing [Practice] ini keys stay compatible.
static volatile LONG g_pst_logs; static DWORD g_pst_t; static int g_pst_kiFail, g_pst_kiSeen; static int g_pst_drainOn[N3T_SLOTS];
static void PstLog(const char* fmt, ...) { if (InterlockedIncrement(&g_pst_logs) > 200) return; char m[400]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m); }
static const char* PstKey(int what) {
    if (what == PS_FRAME_STYLE) return "FrameMeterStyle";
    if (what == PS_FRAME_SIZE) return "FrameMeterRingSize";
    if (what == PS_KI_WINDOW) return "KiWindow";
    if (what == PS_FRAME_IFR) return "FrameMeterIFrames";
    if (what >= PS_FRAME_PROW) { static const char* const k[] = {"FrameMeterPlayerRow", "FrameMeterKeyStartup", "FrameMeterKeyActive", "FrameMeterKeyRecovery", "FrameMeterKeyUnblockable", "FrameMeterKeyGrab", "FrameMeterKeyOther", "FrameMeterKeyTiming", "FrameMeterKeyKi", "FrameMeterKeyIFrames"}; return k[what - PS_FRAME_PROW]; }
    if (what == PS_KI_LOG) return "KiLog";
    if (what == PS_HOLD_KI_V2) return "HoldKiBreakV2";
    if (what == PS_KI_FLOOR) return "KiFloorDepth";
    if (what == PS_NEG_KI_METER) return "NegKiMeter";
    return what == PS_FRAME_KI ? "FrameMeterKi" : what == PS_FRAME_READOUT ? "FrameMeterReadout" : what == PS_FRAME_LEGEND ? "FrameMeterLegend" : what == PS_TIME_OF_DAY ? "TimeOfDayMin" : what == PS_WEATHER ? "Weather" : what == PS_HOLD_KI_BREAK ? "HoldKiBreak" : what == PS_KI_DEALT ? "KiDamageDealtPct" : what == PS_ONE_HIT_KI ? "OneHitKiBreak" : what == PS_PLAYER_METER ? "PlayerFrameMeter" : what == PS_FRAME_METER ? "FrameMeter" : what == PS_BOSS_SPEED ? "BossSpeedPct" : what == PS_PLAYER_SPEED ? "PlayerSpeedPct" : what == PS_PLAYER_INF_NIN ? "PlayerInfiniteNinjutsu" : what == PS_PLAYER_INF_KI ? "PlayerInfiniteKi" : what == PS_DMG_DEALT ? "DamageDealtPct" : what == PS_DMG_TAKEN ? "DamageTakenPct" : "BossKiDrainPerSec";
}
static void PstLoadIni(const char* ini) {
    for (int i = 0; i < PS_N; ++i) g_pst.v[i] = PstFromIni(i, (int)GetPrivateProfileIntA("Practice", PstKey(i), PstToIni(i, PstDefault(i)), ini));
    Log("PRACTICE stats (#22389): PlayerInfiniteKi %d, DamageDealtPct %d, DamageTakenPct %d, BossKiDrainPerSec %d, BossSpeedPct %d, PlayerInfiniteNinjutsu %d, PlayerSpeedPct %d, FrameMeter %d, KiDamageDealtPct %d, OneHitKiBreak %d, HoldKiBreak %d, TimeOfDayMin %d, Weather %d (%s)",
        g_pst.v[0], g_pst.v[1], g_pst.v[2], g_pst.v[3], g_pst.v[4], g_pst.v[5], g_pst.v[6], g_pst.v[7], g_pst.v[8], g_pst.v[9], g_pst.v[PS_HOLD_KI_BREAK],
        PstTodToIni(g_pst.v[PS_TIME_OF_DAY]), g_pst.v[PS_WEATHER], PstWeatherName(g_pst.v[PS_WEATHER]));
    Log("PRACTICE Ki break (#23013): KiLog %d, HoldKiBreakV2 %d, KiFloorDepth %d %%", g_pst.v[PS_KI_LOG], g_pst.v[PS_HOLD_KI_V2], g_pst.v[PS_KI_FLOOR]);
}
static volatile LONG g_pst_saving; static ULONGLONG g_pst_iniStamp;  // ini mtime we last wrote / loaded (live reload, user #22826: a hand-edited 0 must switch a setting off at once)
static ULONGLONG PstIniStamp(const char* ini) { WIN32_FILE_ATTRIBUTE_DATA d; if (!GetFileAttributesExA(ini, GetFileExInfoStandard, &d)) return 0; return ((ULONGLONG)d.ftLastWriteTime.dwHighDateTime << 32) | d.ftLastWriteTime.dwLowDateTime; }
static DWORD WINAPI PstSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH], v[16]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    for (int i = 0; i < PS_N; ++i) { snprintf(v, sizeof v, "%d", PstToIni(i, g_pst.v[i])); WritePrivateProfileStringA("Practice", PstKey(i), v, ini); }
    WritePrivateProfileStringA(0, 0, 0, ini); g_pst_iniStamp = PstIniStamp(ini); InterlockedDecrement(&g_pst_saving); return 0;
}
// Once a second: if Nioh3Training.ini changed on disk (not by our own save), re-read [Practice] into memory. Switching OneHitKiBreak (or any toggle) to 0 by hand
// therefore takes effect in the running game; the hooks read g_pst.v on every hit / tick.
static void PstIniPoll(void) {
    static DWORD last; const DWORD now = GetTickCount(); if (now - last < 1000) return; last = now;
    if (g_pst_saving > 0) return;
    char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    const ULONGLONG st = PstIniStamp(ini); if (!st) return;
    if (!g_pst_iniStamp) { g_pst_iniStamp = st; return; }
    if (st == g_pst_iniStamp) return;
    g_pst_iniStamp = st;
    for (int i = 0; i < PS_N; ++i) { const int nv = PstClamp(i, PstFromIni(i, (int)GetPrivateProfileIntA("Practice", PstKey(i), PstToIni(i, PstDefault(i)), ini))); if (nv != g_pst.v[i]) { Log("PRACTICE stats: ini changed on disk, %s %d -> %d", PstKey(i), g_pst.v[i], nv); g_pst.v[i] = nv; } }
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
// #43283: a Boss Match is a Training session that is not Practice. g_bmd_on is the wrong flag: a loot or no-finisher menu change clears it. This never writes g_pst.v.
static int PstBossMatchOn(void) { return g_tr.active && !g_tr.practice; }
static __attribute__((noinline)) int PstIsBossActor(uint64_t a) {
    if (!a) return 0;
    for (int s = 0; s < N3T_SLOTS; ++s) if (BvActor(s) == a) return 1;
    return 0;
}
static float PstSpdDetour(uint64_t ctrl, uint8_t layer, uint8_t flag) {
    const float r = o_pstspd(ctrl, layer, flag);
    const int bp = PstBossMatchEffective(PS_BOSS_SPEED, g_pst.v[PS_BOSS_SPEED], PstBossMatchOn()), pp = g_pst.v[PS_PLAYER_SPEED];
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
// user #23013 (Kagetoki / Jubei 'negative Ki'), notes/human-boss-negative-ki-23013.md. Static RE re-checked on re/Nioh3.dump.exe (decomp.cmd, 2026-10-22):
//   KiFloor 2F89C8() : sub rsp,28h | mov rcx,[45B9E30] | mov edx,7E3Dh | mov rcx,[rcx+230h] | call 608530 -> rec ? (float)int[rec+10h] * float[rec+18h] : 0.0.
//                      No argument: one floor for every actor. Callers: KiSetCur 2F88D0 (call at 2F8914) and KiDamage 844604 (call at 84464E), nothing else.
//   KiSetCur 2F88D0(K, v): cur K+0x14 = min(max(v, KiFloor()), max(0, K+0x18 - K+0x88)); while KiIsExhausted 844FE4 a RISE of cur adds to K+0x60
//                      (capped at max) and subtracts from K+0x58 / K+0x5C (floor 0).
//   KiDamage 844604(K, amt): unless KiDamageImmune 2F8884: KiSetCur(K, cur - amt); then if exhausted and amt > 0: K+0x60 = max(K+0x60 - amt, KiFloor()).
//   KiTick 2F8068: Ki regen only while K+0x54 != 0; while K+0x74 > 0 it counts down and K+0x58 = clamp(K+0x58 + dt * K+0x6C, 0, K+0x5C) (2F8800);
//                      when K+0x74 runs out: K+0x58 = K+0x5C, and if K+0x7C <= 0 KiBreakClear 8455B4 (= recovered); else K+0x7C counts down unless
//                      K+0x85 != 0, KiBreakClear at <= 0. KiBreakClear zeroes K+0x58..+0x67 (so K+0x60 too), +0x68/+0x6C, +0x80..+0x85, timers +0x70..+0x7C = -1.
//   Both KiFloor call sites keep K in rbx and, MSVC LTCG style, expect xmm1..xmm5 to survive the call (2F8919 subss xmm1,xmm0 / 84464E..663 uses xmm3),
//   so the floor hook is a call-site stub that saves rcx rdx r8..r11 + xmm1..xmm5 (the PsHook D90C0 stub pattern), passes rbx as K and calls KfFloor.
static struct { uint64_t a; int aiOff, nAi; DWORD tAi, t0; LONG nTop, nKi; float kiLow; int v2; uint8_t sav54, sav85; LONG nV2; } g_pst_hb[N3T_SLOTS];  // Hold Ki break (v2 = Ki-pool method, #23013)
static volatile uint64_t g_kf_K[N3T_SLOTS];  // Ki struct K (= st + 0x98) of each Training boss, refreshed by PstTick at 20 Hz (compared, never dereferenced)
static volatile LONG g_kf_hooked, g_kf_logs, g_kl_lines; static int g_kf_fnOk = -1;
static __attribute__((noinline)) int KfFnOk(void) {  // KiFloor 2F89C8 is the function above (also guards the direct calls below)
    if (g_kf_fnOk < 0) { __try { g_kf_fnOk = !memcmp((void*)(B + 0x2F89C8), "\x48\x83\xEC\x28\x48\x8B\x0D", 7) && !memcmp((void*)(B + 0x2F89D3), "\xBA\x3D\x7E\x00\x00\x48\x8B\x89\x30\x02\x00\x00\xE8", 13); } __except (EXCEPTION_EXECUTE_HANDLER) { g_kf_fnOk = 0; } }
    return g_kf_fnOk;
}
static __attribute__((noinline)) float KfStock(void) { float f = 0.0f; if (!KfFnOk()) return 0.0f; __try { f = ((float (*)(void))(B + 0x2F89C8))(); } __except (EXCEPTION_EXECUTE_HANDLER) { f = 0.0f; } return f; }
static int KfBossSlot(uint64_t K) { if (!K) return -1; for (int s = 0; s < N3T_SLOTS; ++s) if (g_kf_K[s] == K) return s; return -1; }
static void KfRefresh(int live) {
    for (int s = 0; s < N3T_SLOTS; ++s) { uint64_t a = live ? BvActor(s) : 0, st = 0; g_kf_K[s] = a && Rd64(a + 0x3A0, &st) && st ? st + 0x98 : 0; }
}
// effective floor of K for a given stock value: KiFloorDepth for a Training boss while the hook is on, else stock
static __attribute__((noinline)) float KfEffective(uint64_t K, float stock) {
    const int pct = PstBossMatchEffective(PS_KI_FLOOR, g_pst.v[PS_KI_FLOOR], PstBossMatchOn());
    if (g_kf_hooked != 1 || pct == 100 || !g_pst_live || KfBossSlot(K) < 0) return stock;
    float mx = 0.0f; __try { mx = *(volatile float*)(K + 0x18); } __except (EXCEPTION_EXECUTE_HANDLER) { return stock; }
    return PstKiFloor(stock, mx, pct, 1);
}
static float KfFloor(uint64_t K) {  // called by the stub instead of KiFloor 2F89C8, K = the caller's rbx
    const float stock = ((float (*)(void))(B + 0x2F89C8))();
    if (PstBossMatchEffective(PS_KI_FLOOR, g_pst.v[PS_KI_FLOOR], PstBossMatchOn()) == 100 || !g_pst_live) return stock;
    const float r = KfEffective(K, stock);
    if (r != stock && InterlockedIncrement(&g_kf_logs) <= 10) Log("PRACTICE Ki floor depth %d %%: Training boss K %p floor %.1f -> %.1f", g_pst.v[PS_KI_FLOOR], (void*)K, stock, r);
    return r;
}
static uint8_t* KfStub(uint8_t* site) {
    static const uint8_t c0[] = {
        0x51, 0x52, 0x41, 0x50, 0x41, 0x51, 0x41, 0x52, 0x41, 0x53,           // push rcx rdx r8 r9 r10 r11 (entry rsp = 8 mod 16 -> still 8)
        0x48, 0x83, 0xEC, 0x78,                                               // sub rsp,78h (20h shadow + 5 xmm) -> rsp 16-aligned for the call
        0x0F, 0x11, 0x4C, 0x24, 0x20, 0x0F, 0x11, 0x54, 0x24, 0x30, 0x0F, 0x11, 0x5C, 0x24, 0x40,
        0x0F, 0x11, 0x64, 0x24, 0x50, 0x0F, 0x11, 0x6C, 0x24, 0x60,           // movups [rsp+20h..60h], xmm1..xmm5
        0x48, 0x8B, 0xCB,                                                     // mov rcx,rbx (K)
        0x48, 0xB8 };                                                         // mov rax, imm64 (KfFloor)
    static const uint8_t c1[] = {
        0xFF, 0xD0,                                                           // call rax (result in xmm0)
        0x0F, 0x10, 0x4C, 0x24, 0x20, 0x0F, 0x10, 0x54, 0x24, 0x30, 0x0F, 0x10, 0x5C, 0x24, 0x40,
        0x0F, 0x10, 0x64, 0x24, 0x50, 0x0F, 0x10, 0x6C, 0x24, 0x60,           // movups xmm1..xmm5
        0x48, 0x83, 0xC4, 0x78,                                               // add rsp,78h
        0x41, 0x5B, 0x41, 0x5A, 0x41, 0x59, 0x41, 0x58, 0x5A, 0x59,           // pop r11 r10 r9 r8 rdx rcx
        0xC3 };                                                               // ret
    uint8_t* s = NearPage(site); if (!s) return 0;
    int k = 0; memcpy(s, c0, sizeof c0); k += sizeof c0; *(uint64_t*)(s + k) = (uint64_t)KfFloor; k += 8; memcpy(s + k, c1, sizeof c1); k += sizeof c1;
    FlushInstructionCache(GetCurrentProcess(), s, k);
    return s;
}
static void KfHookInstall(void) {
    if (g_kf_hooked) return;
    g_kf_hooked = 2;
    uint8_t* s1 = (uint8_t*)(B + 0x2F8914); uint8_t* s2 = (uint8_t*)(B + 0x84464E);
    int ok = 0;
    __try {
        ok = KfFnOk()
            && !memcmp((void*)(B + 0x2F88F6), "\x48\x8B\xD9", 3) && !memcmp(s1 - 3, "\x0F\x28\xD4", 3) && !memcmp(s1 + 5, "\xF3\x0F\x5C\xC8", 4)       // KiSetCur: mov rbx,rcx .. call KiFloor; subss xmm1,xmm0
            && !memcmp((void*)(B + 0x84460F), "\x48\x8B\xD9", 3) && !memcmp(s2 - 2, "\x76\xED", 2) && !memcmp(s2 + 5, "\xF3\x0F\x10\x53\x60", 5)   // KiDamage: mov rbx,rcx .. call KiFloor; movss xmm2,[rbx+60h]
            && s1[0] == 0xE8 && (uintptr_t)(s1 + 5) + *(int32_t*)(s1 + 1) == B + 0x2F89C8 && s2[0] == 0xE8 && (uintptr_t)(s2 + 5) + *(int32_t*)(s2 + 1) == B + 0x2F89C8;
    } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
    if (!ok) { Log("hook Ki floor 2F8914 / 84464E: bytes differ, Ki floor depth off"); return; }
    uint8_t* t1 = KfStub(s1); uint8_t* t2 = KfStub(s2);
    if (!t1 || !t2) { Log("hook Ki floor: no stub page, Ki floor depth off"); return; }
    if (!NfPatchCall(s1, B + 0x2F89C8, t1, "Ki floor 2F8914")) return;
    if (!NfPatchCall(s2, B + 0x2F89C8, t2, "Ki floor 84464E")) { NfPatchCall(s1, (uintptr_t)t1, (uint8_t*)(B + 0x2F89C8), "Ki floor 2F8914 undo"); return; }
    g_kf_hooked = 1;
    Log("hook Ki floor 2F8914 (KiSetCur) + 84464E (KiDamage): on (user #23013: KiFloorDepth %d %%, stock floor %.1f)", g_pst.v[PS_KI_FLOOR], KfStock());
}
// user #23013 design 1 'KiLog': log only. Every 100 ms while a Training boss is Ki-broken (state 0x23 || KiIsExhausted || Ki <= 0): the Ki pool fields
// and the floor; one start / end line per break. Per hit: PstKiDamage (KiDamage call 329E9D) logs amount and cur / K+0x60 before and after.
static struct { uint64_t a; int on; DWORD t0, tLast; LONG n; } g_kl[N3T_SLOTS];
static __attribute__((noinline)) int KlRead(uint64_t K, float* f, uint8_t* b) {  // f: cur max 58 5C 60 74 7C ; b: 83 54 85
    __try {
        f[0] = *(volatile float*)(K + 0x14); f[1] = *(volatile float*)(K + 0x18); f[2] = *(volatile float*)(K + 0x58); f[3] = *(volatile float*)(K + 0x5C);
        f[4] = *(volatile float*)(K + 0x60); f[5] = *(volatile float*)(K + 0x74); f[6] = *(volatile float*)(K + 0x7C);
        b[0] = *(volatile uint8_t*)(K + 0x83); b[1] = *(volatile uint8_t*)(K + 0x54); b[2] = *(volatile uint8_t*)(K + 0x85);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    return 1;
}
static void KlLog(const char* fmt, ...) { if (InterlockedIncrement(&g_kl_lines) > 6000) return; char m[600]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m); }
static __attribute__((noinline)) void KlTick(int s) {
    const uint64_t a = BvActor(s);
    if (g_kl[s].a != a) memset(&g_kl[s], 0, sizeof g_kl[s]);
    if (!a) return;
    uint64_t st = 0, ai = 0; float f[7]; uint8_t b[3]; int s23 = 0, exh = 0, dead = 0;
    if (!Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai) return;
    const uint64_t K = st + 0x98;
    __try { dead = *(volatile int32_t*)(st + 0x48) == 1 || !*(volatile uint64_t*)(st + 0x50); if (!dead) { s23 = KbState(ai, 0x23); exh = ((uint8_t (*)(uint64_t))(B + 0x844FE4))(K) != 0; } } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!KlRead(K, f, b)) return;
    g_kl[s].a = a;
    const int broken = !dead && (s23 || exh || (f[1] > 0.0f && f[1] <= 100000.0f && f[0] <= 0.0f));
    const DWORD now = GetTickCount();
    if (!broken) {
        if (g_kl[s].on) KlLog("KILOG end #%ld Boss %c %p after %lu ms: Ki %.1f / %.1f K+58 %.2f +5C %.2f +60 %.2f +74 %.3f +7C %.3f +83 %u%s", g_kl[s].n, 'A' + s, (void*)a, now - g_kl[s].t0, f[0], f[1], f[2], f[3], f[4], f[5], f[6], b[0], dead ? " (dead)" : "");
        g_kl[s].on = 0; return;
    }
    const float stock = KfStock(), eff = KfEffective(K, stock);
    if (!g_kl[s].on) { g_kl[s].on = 1; g_kl[s].t0 = now; g_kl[s].tLast = 0; g_kl[s].n++;
        KlLog("KILOG start #%ld Boss %c %p chr %u: Ki %.1f / %.1f floor %.1f (stock %.1f, depth %d %%) state23 %d exhausted %d", g_kl[s].n, 'A' + s, (void*)a, Nioh3Training_SlotChr(s), f[0], f[1], eff, stock, g_pst.v[PS_KI_FLOOR], s23, exh); }
    if (g_kl[s].tLast && now - g_kl[s].tLast < 100) return;
    g_kl[s].tLast = now;
    KlLog("KILOG Boss %c +%lu ms: cur %.1f max %.1f K+58 %.2f +5C %.2f +60 %.2f +74 %.3f +7C %.3f +83 %u | floor %.1f (stock %.1f) | 23 %d exh %d regen54 %u pause85 %u%s",
        'A' + s, now - g_kl[s].t0, f[0], f[1], f[2], f[3], f[4], f[5], f[6], b[0], eff, stock, s23, exh, b[1], b[2], g_pst_hb[s].a == a ? (g_pst_hb[s].v2 ? " held v2" : " held v1") : "");
}
static __attribute__((noinline)) void KlHit(uint64_t K, float amt0, float amt, float c0, float s0, uint8_t r) {
    float f[7]; uint8_t b[3]; const int s = KfBossSlot(K);
    if (s < 0 || !KlRead(K, f, b)) return;
    if (amt != amt0) KlLog("KILOG hit Boss %c: Ki damage %.1f (scaled to %.1f), cur %.1f -> %.1f, K+60 %.2f -> %.2f, K+74 %.3f +7C %.3f +83 %u, ret %u", 'A' + s, amt0, amt, c0, f[0], s0, f[4], f[5], f[6], b[0], r);
    else KlLog("KILOG hit Boss %c: Ki damage %.1f, cur %.1f -> %.1f, K+60 %.2f -> %.2f, K+74 %.3f +7C %.3f +83 %u, ret %u", 'A' + s, amt0, c0, f[0], s0, f[4], f[5], f[6], b[0], r);
}
// user #22588 'Ki damage I deal' (PS_KI_DEALT) and 'One-hit Ki break' (PS_ONE_HIT_KI). Static RE (re/Nioh3.dump.exe, capstone): KiDamage 844604(rcx = K,
// xmm1 = amount) = if !KiDamageImmune 2F8884(K): KiSetCur 2F88D0(K, [K+0x14] - amount). Its only call in the hit apply 327DEC is 329E9D:
//   329E1B..329E31  bl = 2A3758([victim+0x3A0]+0x98) = 'Ki <= 0' BEFORE the hit   (victim = 327DD4(hit), hit = r13)
//   329E89          mov r14d,98h | movaps xmm1,xmm13 | mov rcx,[rax+3A0h] | add rcx,r14 | call 844604
//   329F25..329F46  if !bl && 2A3758(K) (Ki <= 0 AFTER) -> the native Ki-break transition (78C530 ...)
// Hook: the E8 at 329E9D -> stub 'mov r8, r13 (the hit); jmp PstKiDamage' (K in rcx, amount in xmm1, hit in r8 = the C argument order); r8 is volatile
// across the call, so the site never reads it back. Attacker = 325F40(hit) like the HP damage multipliers. One-hit: the amount becomes cur + max(scaled, 1),
// so the same hit crosses 0 = the native break (the NOFIN prime 0.5 relies on the same crossing, live #9319). Infinite boss Ki wins (no forced break).
typedef uint8_t (*PstKiFn)(uint64_t, float); static volatile LONG g_pst_ki_hooked, g_pst_ki_logs;  // 0 not tried, 1 on, 2 failed
static __attribute__((noinline)) float PstKiScale(uint64_t K, float amt, uint64_t hit) {
    const int kd = g_pst.v[PS_KI_DEALT], one = PstBossMatchEffective(PS_ONE_HIT_KI, g_pst.v[PS_ONE_HIT_KI], PstBossMatchOn()), dealt = g_pst.v[PS_DMG_DEALT], boss = g_pst.v[PS_DMG_TAKEN];
    if (!g_pst_live || (kd == 100 && !one && dealt == 100 && boss == 100) || !K || !hit || !g_pst_atk_ok) return amt;
    uint64_t me = PlayerChar(), pst = 0; if (me) Rd64(me + 0x3A0, &pst);
    const uint64_t atk = ((uint64_t (*)(uint64_t))(B + 0x325F40))(hit);
    const int atkPlayer = me && atk == me, vicPlayer = pst && pst + 0x98 == K;
    // user #23924: a Training boss's hit on the player also scales the Ki damage (Damage boss deals); the boss's hit on anyone else is left alone
    if (!atkPlayer) {
        if (!vicPlayer || boss == 100 || !PstIsBossActor(atk)) return amt;
        const float r = PstScaleKi(amt, boss);
        if (r != amt && InterlockedIncrement(&g_pst_ki_logs) <= 30) PstLog("PRACTICE Ki damage: boss hit on the player, Ki damage %.1f -> %.1f (x%d.%02d)", amt, r, boss / 100, boss % 100);
        return r;
    }
    if (vicPlayer) return amt;
    const int pct = PstKiPct(1, 0, 0, kd, dealt, boss);
    int slot = -1, blocked = 1; uint64_t a = 0;
    for (int s = 0; s < N3T_SLOTS && slot < 0; ++s) { uint64_t st = 0; a = BvActor(s); if (a && Rd64(a + 0x3A0, &st) && st && st + 0x98 == K) slot = s; }
    const float cur = *(volatile float*)(K + 0x14);
    if (slot >= 0 && one) {
        uint64_t st = K - 0x98, ai = 0; const int nf = BvNfSlot(a);
        const int dead = *(volatile int32_t*)(st + 0x48) == 1 || !*(volatile uint64_t*)(st + 0x50);
        blocked = dead || g_bv[slot].infKi || (nf >= 0 && g_nf[nf].win == 1) || ((uint8_t (*)(uint64_t))(B + 0x844FE4))(K) || (Rd64(a + 0x3B0, &ai) && ai && KbState(ai, 0x23));
    }
    const float r = PstKiDmg(atkPlayer, vicPlayer, slot >= 0, pct, one, blocked, cur, amt);
    if (r != amt && InterlockedIncrement(&g_pst_ki_logs) <= 30)
        PstLog("PRACTICE Ki damage: player hit on %s, Ki %.1f, Ki damage %.1f -> %.1f (x%d.%02d%s)", slot >= 0 ? "a Training boss" : "a non-Training target", cur, amt, r, pct / 100, pct % 100, r > PstScaleKi(amt, pct) ? ", ONE-HIT Ki BREAK" : "");
    return r;
}
static uint8_t PstKiDamage(uint64_t K, float amt, uint64_t hit) {
    const float amt0 = amt; float c0 = 0.0f, s0 = 0.0f;
    __try { amt = PstKiScale(K, amt, hit); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    const int lg = g_pst.v[PS_KI_LOG] && g_pst_live && KfBossSlot(K) >= 0;  // user #23013 KiLog: one line per Ki-damage hit on a Training boss
    if (lg) { __try { c0 = *(volatile float*)(K + 0x14); s0 = *(volatile float*)(K + 0x60); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    const uint8_t r = ((PstKiFn)(B + 0x844604))(K, amt);
    if (lg) { __try { KlHit(K, amt0, amt, c0, s0, r); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    return r;
}
static void PstKiHookInstall(void) {
    if (g_pst_ki_hooked) return;
    g_pst_ki_hooked = 2;
    // 325F40 (attacker of a hit, same check as HpDmgHookInstall): mov rcx,[rcx+198h] | jmp 325F4C
    if (!g_pst_atk_ok) g_pst_atk_ok = !memcmp((void*)(B + 0x325F40), "\x48\x8B\x89\x98\x01\x00\x00\xE9\x00\x00\x00\x00", 12);
    uint8_t* site = (uint8_t*)(B + 0x329E9D);
    // site - 20: mov r14d,98h | movaps xmm1,xmm13 | mov rcx,[rax+3A0h] | add rcx,r14 (then call 844604)
    if (!g_pst_atk_ok || memcmp(site - 20, "\x41\xBE\x98\x00\x00\x00\x41\x0F\x28\xCD\x48\x8B\x88\xA0\x03\x00\x00\x49\x03\xCE", 20)) {
        Log("hook Ki damage 329E9D: bytes differ (attacker getter ok %d), Ki damage I deal / one-hit Ki break off", (int)g_pst_atk_ok); return; }
    uint8_t* s = NearPage(site); if (!s) { Log("hook Ki damage 329E9D: no stub page, off"); return; }
    uint8_t* p = s; *p++ = 0x4D; *p++ = 0x8B; *p++ = 0xC5;  // mov r8, r13 (the hit) = PstKiDamage's 3rd argument
    p = E_mov_rax(p, (uint64_t)PstKiDamage); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s, 16);
    if (NfPatchCall(site, B + 0x844604, s, "Ki damage 329E9D")) { g_pst_ki_hooked = 1;
        Log("hook Ki damage 329E9D: on (user #22588: Ki damage I deal %d %%, one-hit Ki break %d)", g_pst.v[PS_KI_DEALT], g_pst.v[PS_ONE_HIT_KI]); }
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
// 'Reset fight' (user #22592): one press = every Training boss full HP + Ki (BV_REFILL) and back at its start point (Nioh3Training_ResetBossPos),
// player full HP + Ki, player back at the Practice start (g_tr.pp, the round-1 player pose that TrainRetry / TrainLost also Warp to).
// Boss parts reuse the existing exports (they queue to the game thread); the player parts run in PstResetFightTick on the game thread.
static volatile LONG g_pst_reset_req; static volatile LONG g_pst_reset_warp;
static __attribute__((noinline)) void PstResetFightTick(void) {
    if (!InterlockedExchange(&g_pst_reset_req, 0)) return;
    const int warp = InterlockedExchange(&g_pst_reset_warp, 0);
    int hpOk = 0, kiOk = 0, w = -9;
    __try { PlayerRecoverFull(); hpOk = 1; } __except (EXCEPTION_EXECUTE_HANDLER) { Log("RESETFIGHT player RecoverFull faulted %08lX", GetExceptionCode()); }
    {   // Ki: same raw write as infinite Ki (st+0xAC = st+0xB0), with its sanity bounds
        uint64_t me = PlayerChar(), st = 0;
        if (me && Rd64(me + 0x3A0, &st) && st) {
            __try { volatile float* cur = (volatile float*)(st + 0xAC); const float km = *(volatile float*)(st + 0xB0); if (km > 0.0f && km <= 100000.0f) { *cur = km; kiOk = 1; } } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }
    if (warp && g_tr.active && g_tr.hasStart) {
        __try { w = Warp(g_tr.pp[0], g_tr.pp[1], g_tr.pp[2], g_tr.pp[3], 1); } __except (EXCEPTION_EXECUTE_HANDLER) { Log("RESETFIGHT player Warp faulted %08lX", GetExceptionCode()); }
    }
    Log("RESETFIGHT player: HP refill %d, Ki refill %d, back to start %s (Warp %d, start %.0f %.0f %.0f)", hpOk, kiOk, warp ? "requested" : "no start recorded", w, g_tr.pp[0], g_tr.pp[1], g_tr.pp[2]);
}
// user #22918 'Hold Ki break' (PS_HOLD_KI_BREAK). Native break timers (static RE of re/Nioh3.dump.exe, capstone; not run in game):
//   KiTick 2F8068(K = st+0x98, dt): K+0x74 counts down by dt while > 0; at <= 0 the phase-2 timer K+0x7C counts down (unless K+0x85), then
//     KiBreakClear 8455B4 + K+0x81 = 1 (= recovered). KiIsExhausted 844FE4 = K+0x5C > 0 || K+0x74 > 0 || K+0x7C > 0 || K+0x83.
//   8D8CC0(ctrl): runtime bit 0x3A (ctrl+0x1060, = StateTest 0x23) with ctrl+0x1014 > 0: -= dt (ctrl+0x40); cleared at <= 0, or at once when
//     8D8D58 is false (481290, or none of ctrl+0xA8 bit 41 / StateTest 0xA / StateTest 9). A4F098 sets the bit and ctrl+0x1014 = the break length.
// Hold (20 Hz, PstTick on the game thread): while a Training boss is naturally Ki-broken, every RUNNING timer above is kept >= PST_HB_HOLD_S (never
// started from idle), and its AI is held off (668D74, re-applied every 100 ms when the game turns it back on, like the NOFIN HP-0 window AI hold).
// Release (toggle OFF, boss dead / gone, session over, or the game itself ended the break, e.g. a grapple): nothing more is written, so the native
// timers run out within PST_HB_HOLD_S = the normal recovery, and the AI comes back (66982C) if the hold switched it off.
// The HP-0 window of KiBreakNoFinisher owns its boss (its own AI hold): never held there.
// user #23013 HoldKiBreakV2 = 1 (method 2, notes/human-boss-negative-ki-23013.md design 2; HoldKiBreak still switches the hold on / off): instead of the
// timer top-ups above, each tick of a held boss: Ki cur = floor (KiSetCur 2F88D0, so the native clamp applies), K+0x60 = floor (the negative pool KiDamage
// pushes down while exhausted), K+0x58 = K+0x5C = 0, Ki regen off (K+0x54 = 0, KiTick 2F8068 only regenerates while it is set). The note's 'the climb
// never completes' does not hold the break by itself: KiTick ends it when the timers run out (phase 1 K+0x74, then K+0x7C), so v2 also sets the
// native phase-2 pause K+0x85 = 1 and keeps a RUNNING phase-1 timer >= PST_HB_HOLD_S (PstHbTop, as v1). AI held off as in v1.
// Release: K+0x54 / K+0x85 back to the values saved at the start, then KiBreakClear 8455B4(K) (skipped when the boss died), AI back.
#define PST_HB_HOLD_S 1.0f
static __attribute__((noinline)) int PstHbTop(volatile float* p) { float o = 0; __try { if (PstHoldTimer(*p, PST_HB_HOLD_S, &o)) { *p = o; return 1; } } __except (EXCEPTION_EXECUTE_HANDLER) { } return 0; }
static int g_pst_kbc_ok = -1;  // KiBreakClear 8455B4: xor eax,eax | mov [rcx+5Ch],rax | mov [rcx+64h],eax
static __attribute__((noinline)) void PstHbV2Restore(int s, int dead) {
    const uint64_t a = g_pst_hb[s].a; uint64_t st = 0;
    if (!a || !Rd64(a + 0x3A0, &st) || !st) return;
    const uint64_t K = st + 0x98; int clr = 0;
    if (g_pst_kbc_ok < 0) g_pst_kbc_ok = !memcmp((void*)(B + 0x8455B4), "\x33\xC0\x48\x89\x41\x5C\x89\x41\x64", 9);
    __try {
        *(volatile uint8_t*)(K + 0x54) = g_pst_hb[s].sav54; *(volatile uint8_t*)(K + 0x85) = g_pst_hb[s].sav85;
        if (!dead && g_pst_kbc_ok) { ((void (*)(uint64_t))(B + 0x8455B4))(K); clr = 1; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    PstLog("PRACTICE hold Ki break v2 Boss %c %p: Ki regen K+54 = %u, K+85 = %u restored, KiBreakClear %s (%ld pool writes)", 'A' + s, (void*)a, g_pst_hb[s].sav54, g_pst_hb[s].sav85, clr ? "called" : dead ? "skipped (dead)" : "skipped (bytes differ)", g_pst_hb[s].nV2);
}
static __attribute__((noinline)) void PstHbRelease(int s, int dead, const char* why) {
    const uint64_t a = g_pst_hb[s].a; int ai = -1, back = 0;
    if (a && g_pst_hb[s].v2) PstHbV2Restore(s, dead);
    if (a && !dead) ai = BhAiOn(a);
    if (a && PstHoldAiRestore(g_pst_hb[s].aiOff, ai, dead)) { __try { ((void (*)(uint64_t))(B + 0x66982C))(a); back = 1; } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    if (a) PstLog("PRACTICE hold Ki break Boss %c %p: released (%s) after %lu ms, %ld timer top-ups, AI %s", 'A' + s, (void*)a, why, GetTickCount() - g_pst_hb[s].t0, g_pst_hb[s].nTop, back ? "back on" : g_pst_hb[s].aiOff ? "left (already on / dead)" : "untouched");
    memset(&g_pst_hb[s], 0, sizeof g_pst_hb[s]);
}
static __attribute__((noinline)) void PstHoldBreak(int s, int on) {
    const uint64_t a = BvActor(s);
    if (g_pst_hb[s].a && g_pst_hb[s].a != a) memset(&g_pst_hb[s], 0, sizeof g_pst_hb[s]);  // slot reused / despawned: never touch the old actor
    if (!a) return;
    uint64_t st = 0, ai = 0, ctrl = 0; int dead = 0, s23 = 0, exh = 0; float kc = 1.0f, km = 0.0f;
    if (!Rd64(a + 0x3A0, &st) || !st || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) return;
    __try {
        dead = *(volatile int32_t*)(st + 0x48) == 1 || !*(volatile uint64_t*)(st + 0x50);
        if (!dead) { s23 = KbState(ai, 0x23); exh = ((uint8_t (*)(uint64_t))(B + 0x844FE4))(st + 0x98) != 0; kc = *(volatile float*)(st + 0xAC); km = *(volatile float*)(st + 0xB0); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
    const int nf = BvNfSlot(a), window = nf >= 0 && g_nf[nf].win == 1;
    const int v2 = g_pst.v[PS_HOLD_KI_V2] != 0;
    if (g_pst_hb[s].a == a && g_pst_hb[s].v2 != v2) { PstHbRelease(s, dead, "hold method changed (HoldKiBreakV2)"); return; }  // re-held next tick with the new method
    const int held = g_pst_hb[s].a == a, act = PstHoldBreakStep(on, g_pst_live != 0, dead, PstHoldIsBroken(s23, exh, kc, km, g_pst_hb[s].a == a) && !window, held);
    if (act == PS_HB_NONE) return;
    if (act == PS_HB_RELEASE) { PstHbRelease(s, dead, !on ? (PstBossMatchOn() ? "Boss Match" : "toggle OFF") : dead ? "boss died" : window ? "HP-0 window" : "break ended by the game"); return; }
    const DWORD now = GetTickCount();
    if (act == PS_HB_START) {
        memset(&g_pst_hb[s], 0, sizeof g_pst_hb[s]); g_pst_hb[s].a = a; g_pst_hb[s].t0 = now;
        float t74 = 0, t7c = 0, t1014 = 0; __try { t74 = *(volatile float*)(st + 0x98 + 0x74); t7c = *(volatile float*)(st + 0x98 + 0x7C); t1014 = *(volatile float*)(ctrl + 0x1014); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        g_pst_hb[s].kiLow = kc < 0.0f ? kc : 0.0f;
        if (v2) { __try { g_pst_hb[s].sav54 = *(volatile uint8_t*)(st + 0x98 + 0x54); g_pst_hb[s].sav85 = *(volatile uint8_t*)(st + 0x98 + 0x85); } __except (EXCEPTION_EXECUTE_HANDLER) { memset(&g_pst_hb[s], 0, sizeof g_pst_hb[s]); return; } g_pst_hb[s].v2 = 1; }
        PstLog("PRACTICE hold Ki break%s Boss %c %p: Ki-broken (state23 %d, exhausted %d, Ki %.1f / %.1f, K+74 %.2f K+7C %.2f ctrl+1014 %.2f%s): holding until OFF / death", v2 ? " v2 (Ki pool)" : "", 'A' + s, (void*)a, s23, exh, kc, km, t74, t7c, t1014, v2 ? (g_pst_hb[s].sav54 ? ", regen K+54 on" : ", regen K+54 already off") : "");
    }
    if (g_pst_hb[s].v2) {  // user #23013 method 2: Ki pool held at the floor (see above); the v1 timer / Ki-zero writes below are skipped
        const uint64_t K = st + 0x98; const float floorv = KfEffective(K, KfStock());
        float c = 0, k60 = 0, k58 = 0, k5c = 0; int m = 0;
        __try {
            c = *(volatile float*)(K + 0x14); k60 = *(volatile float*)(K + 0x60); k58 = *(volatile float*)(K + 0x58); k5c = *(volatile float*)(K + 0x5C);
            m = KfFnOk() ? PstHoldV2Writes(c, k60, k58, k5c, floorv) : 0;
            if (m & 1) ((void (*)(uint64_t, float))(B + 0x2F88D0))(K, floorv);
            if (m & 2) *(volatile float*)(K + 0x60) = floorv;
            if (m & 4) *(volatile float*)(K + 0x58) = 0.0f;
            if (m & 8) *(volatile float*)(K + 0x5C) = 0.0f;
            *(volatile uint8_t*)(K + 0x54) = 0; *(volatile uint8_t*)(K + 0x85) = 1;
        } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (PstHbTop((volatile float*)(K + 0x74))) g_pst_hb[s].nTop++;
        if (m && ++g_pst_hb[s].nV2 <= 3) PstLog("PRACTICE hold Ki break v2 Boss %c %p: floor %.1f, cur %.1f K+60 %.2f K+58 %.2f K+5C %.2f -> writes 0x%X (#%ld)", 'A' + s, (void*)a, floorv, c, k60, k58, k5c, m, g_pst_hb[s].nV2);
    } else {
    // keep the native break from running out: Ki-exhausted countdown (phase 1, else phase 2) and the state-0x23 stun window
    int top = PstHbTop((volatile float*)(st + 0x98 + 0x74));
    if (!top) top = PstHbTop((volatile float*)(st + 0x98 + 0x7C));
    uint64_t bits = 0; if (Rd64(ctrl + 0x1060, &bits) && (bits & (1ull << 0x3A))) top |= PstHbTop((volatile float*)(ctrl + 0x1014));
    if (top) g_pst_hb[s].nTop++;
    // Ki-zero break (the one-hit break on an idle boss is only this): the native regen lifts Ki above 0 within ~0.3 s = 'recovered'; write it back (user #22918)
    if (kc < g_pst_hb[s].kiLow) g_pst_hb[s].kiLow = kc;
    { float want = 0; if (PstHoldKi(kc, g_pst_hb[s].kiLow, &want)) {
        __try { ((void (*)(uint64_t, float))(B + 0x2F88D0))(st + 0x98, want); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
        if (++g_pst_hb[s].nKi <= 3) PstLog("PRACTICE hold Ki break Boss %c %p: Ki regen %.1f put back to %.1f (#%ld)", 'A' + s, (void*)a, kc, want, g_pst_hb[s].nKi); } }
    }
    // boss does not act: AI off while held
    if (!g_pst_hb[s].aiOff || now - g_pst_hb[s].tAi >= 100) {
        g_pst_hb[s].tAi = now;
        if (BhAiOn(a) == 1) {
            __try { ((void (*)(uint64_t))(B + 0x668D74))(a); } __except (EXCEPTION_EXECUTE_HANDLER) { return; }
            if (!g_pst_hb[s].aiOff) { g_pst_hb[s].aiOff = 1; PstLog("PRACTICE hold Ki break Boss %c %p: AI off while held", 'A' + s, (void*)a); }
            else if (++g_pst_hb[s].nAi <= 10) PstLog("PRACTICE hold Ki break Boss %c %p: AI switched back on by the game, off again (#%d)", 'A' + s, (void*)a, g_pst_hb[s].nAi);
        }
    }
}
// user #22942 'Time of day' (PS_TIME_OF_DAY) / 'Weather' (PS_WEATHER). Static RE only (re/Nioh3.dump.exe, notes/training-time-of-day-22942.md):
//   GameUpdateTick 2339C6: S = 164188() = [[[B+0x4751490]] + 0x28] (GetActiveSessionObj) -> RenderFrameSettingsApply 39B570(rcx, rdx, r8 = S); rsi = S there.
//   39B67A  mov rcx,rsi | 39B67D  call 39BEA4 (-> 39BED0 storm lerp on S: +88 / +7C / +13C8 / +F70 / +F84 / +1968) | 39B684 S+CC = 1e20 s (no clock)
// Hook: the E8 at 39B67D -> stub 'jmp PstEnvDetour' (S in rcx). The detour calls 39BEA4(S) first (native storm lerp), then, with a player,
// no load or event scene (IsEventScene DA7C0), and S the active session object, writes S+0xB0 (SkylightDateTime = min / 1440)
// and the weather preset fields before the renderer consumes S. No Practice session or Training boss is required (#38871).
// Unknown game/event state fails closed. Stock = no write; a field switched back to Stock is restored once if
// the game did not re-fill it itself (PstEnvRelease). Game thread, aligned 4-byte float stores.
typedef void (*PstEnvFn)(uint64_t); static volatile LONG g_pst_env_hooked, g_pst_env_logs;  // 0 not tried, 1 on, 2 failed
static int g_pst_env_evOk = -1; static uint64_t g_pst_env_S; static PstEnvField g_pst_env_tod, g_pst_env_wx[PS_EF_N];
static __attribute__((noinline)) uint64_t PstEnvObj(void) { uint64_t g = 0, o = 0, S = 0; if (Rd64(B + 0x4751490, &g) && g && Rd64(g, &o) && o) Rd64(o + 0x28, &S); return S; }
static __attribute__((noinline)) int PstEnvEvent(void) {
    __try {
        if (g_pst_env_evOk < 0) {
            g_pst_env_evOk = !memcmp((void*)(B + 0xDA7C0), "\x48\x8B\x0D\x39\x40\x4F\x04\x48\x85\xC9\x0F\x85", 12);  // mov rcx,[rip+..] | test rcx,rcx | jne
            if (!g_pst_env_evOk) Log("DISPLAY env: event-scene signature differs; sky overrides disabled");
        }
        if (!g_pst_env_evOk) return 1;  // unknown event state is never permission to override a cutscene
        return ((uint8_t (*)(void))(B + 0xDA7C0))() != 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 1; }
}
static __attribute__((noinline)) int PstEnvRdF(uint64_t a, float* v) { __try { *v = *(volatile float*)a; return 1; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
// read-only state line (each session start): the values the plan's probe step 1 asks for
static __attribute__((noinline)) void PstEnvLogState(const char* why) {
    const uint64_t S = PstEnvObj(); uint64_t w = 0; float f[16] = {0}, storm = -1.0f;
    static const uint32_t off[16] = {0xB0, 0xAC, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x98, 0xB4, 0xB8, 0xC0, 0xC4, 0xCC, 0x15C8, 0x174};
    int ok = S != 0;
    for (int i = 0; i < 16 && ok; ++i) ok = PstEnvRdF(S + off[i], &f[i]);
    if (Rd64(B + 0x4757710, &w) && w) PstEnvRdF(w + 0x2C, &storm);
    if (!ok) { Log("PRACTICE env (#22942, %s): active RenderConfig S %p not readable (read-only, nothing written)", why, (void*)S); return; }
    const int m = f[0] >= 0.0f && f[0] < 1.0f ? (int)(f[0] * 1440.0f + 0.5f) : -1;
    int32_t step = 0; memcpy(&step, &f[15], 4);
    Log("PRACTICE env (#22942, %s, read-only): S %p SkylightDateTime +B0 %.4f (%02d:%02d) MonthDay +AC %.2f | Cloudy %.3f Temp %.1f Humid %.3f Precip %.3f Fog %.3f Haze %.3f SkyClearly %.3f RainDrop %.3f | Lon %.3f Lat %.3f North %.1f MoonAge %.2f TimeSpanPerDay %g AutoStep %d | storm weight %.3f | event scene %d | ini TimeOfDayMin %d Weather %s",
        why, (void*)S, f[0], m < 0 ? -1 : m / 60, m < 0 ? -1 : m % 60, f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[14], f[9], f[10], f[11], f[12], f[13], step, storm, PstEnvEvent(),
        PstTodToIni(g_pst.v[PS_TIME_OF_DAY]), PstWeatherName(g_pst.v[PS_WEATHER]));
}
static __attribute__((noinline)) void PstEnvField1(uint64_t S, uint32_t off, PstEnvField* e, int on, float want, const char* name) {
    volatile float* p = (volatile float*)(S + off);
    __try {
        const float cur = *p;
        if (on) {
            if (!e->wrote && !PstEnvSane(cur)) return;  // garbage S: never write
            const uint32_t r0 = e->refills, f0 = e->frames, first = !e->wrote;
            *p = PstEnvApply(e, cur, want);
            if (first && InterlockedIncrement(&g_pst_env_logs) <= 60) Log("PRACTICE env: %s S+%X %.4f -> %.4f (override on)", name, off, cur, want);
            if (f0 == 0 && e->frames == 1 && InterlockedIncrement(&g_pst_env_logs) <= 60)  // the plan's open question: does the game re-fill S each frame?
                Log("PRACTICE env: %s S+%X reads %.4f one frame after our write: %s", name, off, cur, e->refills > r0 ? "re-filled by the game each frame" : "kept (no per-frame re-fill; the saved stock value is restored on Stock)");
        } else if (e->wrote) {
            float o = 0; const float last = e->last;
            if (PstEnvRelease(e, cur, &o)) *p = o;
            if (InterlockedIncrement(&g_pst_env_logs) <= 60) Log("PRACTICE env: %s S+%X back to Stock (now %.4f, our last %.4f%s)", name, off, cur, last, cur == last ? ", restored the saved stock value" : ", the game already re-filled it");
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { e->wrote = 0; }
}
static __attribute__((noinline)) void PstEnvApplyAll(uint64_t S) {
    static const char* const nm[PS_EF_N] = {"Cloudy", "Temperature", "Humidity", "Precipitation", "Foggy", "Hazy", "SkyClearly", "RainDropPrecipitation", "FogKeyLightScatter", "GustPower", "GustIntervalPower", "Tree2WindScaler"};
    const int tod = g_pst.v[PS_TIME_OF_DAY], wx = g_pst.v[PS_WEATHER];
    int any = g_pst_env_tod.wrote; for (int f = 0; f < PS_EF_N && !any; ++f) any = g_pst_env_wx[f].wrote;
    if (!any && !tod && !wx) return;  // all Stock and nothing to give back: no game-state reads
    const uint8_t* gs = GameState();
    // A non-active or loading RenderConfig is not ours to write, even for restoration. A retained override
    // on the same object can be restored after loading; a changed object drops the old pointer below.
    // Check this before calling the native player/event getters during a transition.
    if (!gs || gs[2] || !S || S != PstEnvObj()) return;
    const int scope = PstEnvScope(1, 0, 1, PlayerChar() != 0, PstEnvEvent());
    if (S != g_pst_env_S) {  // another stage / RenderConfig: forget the old one (its fields are not ours to restore)
        if (g_pst_env_S && InterlockedIncrement(&g_pst_env_logs) <= 60) Log("PRACTICE env: RenderConfig S %p -> %p", (void*)g_pst_env_S, (void*)S);
        g_pst_env_S = S; memset(&g_pst_env_tod, 0, sizeof g_pst_env_tod); memset(g_pst_env_wx, 0, sizeof g_pst_env_wx);
    }
    const int live = scope == 2;  // global gameplay override, independent of Practice / Training boss state
    PstEnvField1(S, 0xB0, &g_pst_env_tod, live && tod > 0, PstTodFraction(tod), "SkylightDateTime");
    for (int f = 0; f < PS_EF_N; ++f) { float v = 0; const int on = live && PstWeatherValue(wx, f, &v); if (on || g_pst_env_wx[f].wrote) PstEnvField1(S, PstEnvFieldOff(f), &g_pst_env_wx[f], on, v, nm[f]); }
}
static void PstEnvDetour(uint64_t S) {
    ((PstEnvFn)(B + 0x39BEA4))(S);
    __try { PstEnvApplyAll(S); } __except (EXCEPTION_EXECUTE_HANDLER) { }
}
static void PstEnvHookInstall(void) {
    if (g_pst_env_hooked) return;
    g_pst_env_hooked = 2;
    uint8_t* site = (uint8_t*)(B + 0x39B67D);
    // site - 3: mov rcx,rsi (then call 39BEA4); 39BEA4: sub rsp,28h | mov rdx,rcx | mov rcx,[rip+..] (the storm lerp wrapper)
    if (memcmp(site - 3, "\x48\x8B\xCE", 3) || memcmp((void*)(B + 0x39BEA4), "\x48\x83\xEC\x28\x48\x8B\xD1\x48\x8B\x0D", 10)) { Log("hook env 39B67D: bytes differ, time of day / weather off"); return; }
    uint8_t* s = NearPage(site); if (!s) { Log("hook env 39B67D: no stub page, off"); return; }
    uint8_t* p = E_mov_rax(s, (uint64_t)PstEnvDetour); *p++ = 0xFF; *p++ = 0xE0; FlushInstructionCache(GetCurrentProcess(), s, 16);
    if (NfPatchCall(site, B + 0x39BEA4, s, "env 39B67D")) { g_pst_env_hooked = 1;
        Log("hook env 39B67D: on (user #22942: TimeOfDayMin %d, Weather %s)", PstTodToIni(g_pst.v[PS_TIME_OF_DAY]), PstWeatherName(g_pst.v[PS_WEATHER])); }
}
static int g_pst_env_sess;
static void NkmTick(int live);  // user #23400 negative Ki meter (negki_meter_game.h)
static void PstTick(void) {
    const DWORD now = GetTickCount();
    if (now - g_pst_t < 50) return;
    const int dt = g_pst_t ? (int)(now - g_pst_t) : 50; g_pst_t = now;
    if (GameState() && GameState()[2]) { __try { NkmTick(0); } __except (EXCEPTION_EXECUTE_HANDLER) { } return; }  // loading (#23400: no stale negative Ki meter over a load)
    PstIniPoll();
    // Install independently of Practice. The detour still requires the active gameplay sky and fails
    // closed during loading, without a player, or in an event scene (including an unknown signature).
    if ((g_pst.v[PS_TIME_OF_DAY] || g_pst.v[PS_WEATHER]) && !g_pst_env_hooked) PstEnvHookInstall();
    PstResetFightTick();
    int live = g_tr.active != 0;
    for (int s = 0; s < N3T_SLOTS && !live; ++s) if (BvActor(s)) live = 1;
    if (live != g_pst_live) { InterlockedExchange(&g_pst_live, live); InterlockedExchange(&g_pst_logs, 0); g_pst_kiSeen = 0; InterlockedExchange(&g_kl_lines, 0); InterlockedExchange(&g_kf_logs, 0); }
    KfRefresh(live);  // user #23013: Training boss Ki structs for the floor hook / KiLog
    __try { NkmTick(live); } __except (EXCEPTION_EXECUTE_HANDLER) { }  // user #23400 negative Ki meter (read only; clears itself when not live)
    {   // user #22942: one read-only line of the sky time / weather at each session start (Training boss alive, then a Practice session)
        const int sess = live ? 1 + (g_tr.active != 0) : 0;
        if (sess > g_pst_env_sess) PstEnvLogState(g_tr.active ? "Practice session start" : "Training boss alive");
        g_pst_env_sess = sess;
    }
    if (!live) { memset(g_pst_hb, 0, sizeof g_pst_hb); memset(g_kl, 0, sizeof g_kl); return; }  // user #22918: no session and no Training boss = nothing held (no actor left to touch)
    if ((g_pst.v[PS_DMG_DEALT] != 100 || g_pst.v[PS_DMG_TAKEN] != 100) && !g_hpdmg_hooked) HpDmgHookInstall("damage multipliers (#22389)");
    if (g_pst.v[PS_BOSS_SPEED] != 100 || g_pst.v[PS_PLAYER_SPEED] != 100) PstSpdHookInstall();
    if ((g_pst.v[PS_KI_DEALT] != 100 || g_pst.v[PS_DMG_DEALT] != 100 || g_pst.v[PS_DMG_TAKEN] != 100 || g_pst.v[PS_ONE_HIT_KI] || g_pst.v[PS_KI_LOG]) && !g_pst_ki_hooked) PstKiHookInstall();  // user #22588 (+ #23013 KiLog per-hit lines)
    if (g_pst.v[PS_KI_FLOOR] != 100 && !g_kf_hooked) KfHookInstall();  // user #23013 Ki floor depth (first non-stock value; stays installed, 100 = stock pass-through)
    if (g_pst.v[PS_PLAYER_INF_NIN]) PstPlayerNin();
    if (g_pst.v[PS_PLAYER_INF_KI]) PstPlayerKi();
    {   // #43283: Boss Match applies dummy mutations as off. g_pst.v stays the saved preference, so PstSaveThread cannot persist the gate.
        const int drain = PstBossMatchEffective(PS_BOSS_KI_DRAIN, g_pst.v[PS_BOSS_KI_DRAIN], PstBossMatchOn());
        const int holdOn = PstBossMatchEffective(PS_HOLD_KI_BREAK, g_pst.v[PS_HOLD_KI_BREAK], PstBossMatchOn());
        static int bmTold;
        if (PstBossMatchOn() && !bmTold) { bmTold = 1; Log("BOSS MATCH practice gate: HoldKiBreak %d and OneHitKiBreak %d stay saved, applied off", g_pst.v[PS_HOLD_KI_BREAK], g_pst.v[PS_ONE_HIT_KI]); }
        if (!PstBossMatchOn()) bmTold = 0;
        if (drain > 0) for (int s = 0; s < N3T_SLOTS; ++s) PstBossDrain(s, dt);
        for (int s = 0; s < N3T_SLOTS; ++s) if (holdOn || g_pst_hb[s].a) PstHoldBreak(s, holdOn);  // user #22918; on=0 releases a hold left over from Practice
    }
    if (g_pst.v[PS_KI_LOG]) for (int s = 0; s < N3T_SLOTS; ++s) KlTick(s);  // user #23013 (after the hold, so a held tick logs the written values)
}
// Reset fight: returns a bit mask (0 = nothing to do: no Practice session and no Training boss). 1 = boss(es) refilled, 2 = boss position put back queued,
// 4 = player HP + Ki refill queued, 8 = player put back at the Practice start queued.
N3T_API int Nioh3Training_ResetFight(void) {
    int r = 0;
    for (int s = 0; s < N3T_SLOTS && s < BV_SLOTS; ++s) if (SlotAlive(s)) { Nioh3Training_BossVitalsSet(s, BV_REFILL, 0); r |= 1; }
    if (Nioh3Training_ResetBossPos()) r |= 2;
    if (g_tr.active || r) {
        r |= 4;
        if (g_tr.active && g_tr.hasStart) { InterlockedExchange(&g_pst_reset_warp, 1); r |= 8; }
        InterlockedExchange(&g_pst_reset_req, 1);
    }
    Log("RESETFIGHT request from the menu: mask %d (1 boss refill, 2 boss pos, 4 player refill, 8 player pos)", r);
    return r;
}
// C API for Practice pages and Display time/weather rows. what = PS_* (practice_stats.h).
// Get: the value (-1 = bad what). Set: value clamped (PS_PLAYER_INF_KI: -1 flips), persist = save [Practice] in Nioh3Training.ini; returns the new value.
N3T_API int Nioh3Training_PracticeStatGet(int what) { return what >= 0 && what < PS_N ? g_pst.v[what] : -1; }
N3T_API int Nioh3Training_PracticeStatSet(int what, int value, int persist) {
    if (what < 0 || what >= PS_N) return -1;
    if (PstIsToggle(what) && value < 0) value = !g_pst.v[what];
    const int v = g_pst.v[what] = PstClamp(what, value);
    if (what == PS_FRAME_METER) {  // human #45946: the master 'Frame meter' switch must hide the whole meter (player row, I-FR row, Ki window), not just the boss row
        g_pst.v[PS_PLAYER_METER] = v; g_pst.v[PS_FRAME_IFR] = v;
        if (!v) g_pst.v[PS_KI_WINDOW] = 0;
    }
    if (what == PS_PLAYER_INF_KI) g_pst_kiFail = 0, g_pst_kiSeen = 0;
    if (what == PS_DMG_DEALT || what == PS_DMG_TAKEN) InterlockedExchange(&g_pst_dmg_logs, 0);
    if (what == PS_KI_DEALT || what == PS_ONE_HIT_KI) InterlockedExchange(&g_pst_ki_logs, 0);
    if (what == PS_BOSS_KI_DRAIN) for (int s = 0; s < N3T_SLOTS; ++s) g_pst_drainOn[s] = 0;
    if (what == PS_BOSS_SPEED || what == PS_PLAYER_SPEED) InterlockedExchange(&g_pst_spd_logs, 0);
    if (what == PS_TIME_OF_DAY || what == PS_WEATHER) InterlockedExchange(&g_pst_env_logs, 0);
    if (what == PS_KI_LOG || what == PS_KI_FLOOR) { InterlockedExchange(&g_kl_lines, 0); InterlockedExchange(&g_kf_logs, 0); }
    InterlockedExchange(&g_pst_logs, 0);
    Log("PRACTICE stats: %s -> %d%s", PstKey(what), v, persist ? " (saved to ini)" : "");
    if (persist) { InterlockedIncrement(&g_pst_saving); HANDLE t = CreateThread(0, 0, PstSaveThread, 0, 0, 0); if (t) CloseHandle(t); else InterlockedDecrement(&g_pst_saving); }
    return v;
}
