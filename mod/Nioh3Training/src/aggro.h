// user #13787: two Training drill helpers. Included by training.c after drill.h.
// (1) 'Boss aggro on me' ([Training] BossAggro, default 0, menu row on the Boss moves / Return gate pages): every 250 ms, for the Training
//     session boss and the summoned bosses A / B whose AI is ON (Fight normally; Stand still holds the AI, so nothing changes there):
//     - SetAwareFlg 1FF1FA0(ai, 1, 0) every 2 s (the call every spawned boss already gets at +AwareDelayMs, bridge 4942)
//     - the player becomes the current perception target when it is in the list but not current (2BE2E8(perc, idx), the native insert tail, 5201)
//     - accumulated hate on the player every 2 s (1FF23C8(aiBoss, aiPlayer, 500), the SetInitAccumulatedHate body, plug/hate.c 4616)
//     - farther than [Training] AggroRangeCm (default 500 = 5 m): the AI state goes into g_mv_chase (move_toggle.h), so the repeat
//       neither replaces its picks nor is forced from range: the boss closes in with its own AI, then the drill / gap logic resumes.
//     4616 showed hate alone does not make a passive boss approach; the range rule is what lets the AI walk instead of idling in the gap.
// (2) 'Reset boss position': queued to the game thread; the session boss goes back to its round-1 spawn point (g_tr.bp), summoned bosses
//     to where they appeared (WatchTick), facing the player. Mover = the game's own non-player warp (WarpCoopCharacterToPlayer 2398884):
//     669AB8(ch, pos, 0) + rotation rows ch+0x120..0x15F (5ECE50 yaw matrix, as CharacterSetPositionYaw 1C27010) + 66C330(ctrl, 0).
//     1C27010 itself is NOT used: it forces action 0 and writes the player-focus position [[45B9E40]+48h]+D0h.
static int g_cfg_aggro = 0, g_cfg_aggro_range = 500;
static int g_ag_off;  // set after an exception in a native aggro call: no more aggro calls this game session
static DWORD g_ag_t, g_ag_slow;
static LONG g_ag_logs;
static volatile LONG g_ag_reset_req;
static int AgInTable(uint64_t a, const uint64_t* cur, int n) { for (int i = 0; i < n; ++i) if (cur[i] == a) return 1; return 0; }
// Live Training bosses: the session boss first, then slots A / B (deduplicated). Every actor is checked against the live actor table.
static int AgBosses(uint64_t* out, uint64_t* session) {
    uint64_t cur[1024], me = PlayerChar(); int n = Actors(cur, 1024), k = 0;
    *session = 0;
    if (g_tr.active && g_bm.active) {
        uint64_t a = g_bm.actor ? g_bm.actor : ActorByHandle(g_bm.id);
        if (a && a != me && AgInTable(a, cur, n)) { out[k++] = a; *session = a; }
    }
    for (int s = 0; s < 2; ++s) {
        uint64_t a = g_slot_act[s];
        if (!a || a == me || !SlotAlive(s) || !AgInTable(a, cur, n) || (k && out[0] == a) || (k > 1 && out[1] == a)) continue;
        out[k++] = a;
    }
    return k;
}
static __attribute__((noinline)) int AgTarget(uint64_t a, uint64_t comp, uint64_t perc, uint64_t me, int slow) {
    __try {
        if (slow) ((void (*)(uint64_t, uint8_t, uint32_t))(B + 0x1FF1FA0))(comp, 1, 0);
        int idx = PercFind(perc, me);
        if (idx >= 0 && *(int32_t*)(perc + 0x2D0) != idx) {
            int was = *(int32_t*)(perc + 0x2D0);
            ((void (*)(uint64_t, int))(B + 0x2BE2E8))(perc, idx);
            if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO boss %p: current target %d -> player (entry %d)", (void*)a, was, idx);
        }
        uint64_t aiP = 0;
        if (slow && Rd64(me + 0x3B0, &aiP) && aiP) ((void (*)(uint64_t, uint64_t, float))(B + 0x1FF23C8))(comp, aiP, 500.0f);
        return idx;
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AGGRO boss %p: native call faulted %08lX, aggro off for this game session", (void*)a, GetExceptionCode()); g_ag_off = 1; return -2; }
}
static void AggroTick(void) {
    DWORD now = GetTickCount();
    if (now - g_ag_t < 250) return;
    g_ag_t = now;
    uint64_t chase[MV_CHASE_MAX] = {0};
    int nc = 0;
    if (g_cfg_aggro && !g_ag_off) {
        uint64_t me = PlayerChar(), bs[3], session = 0; float pp[4];
        if (me && PlayerPos(pp)) {
            const int slow = now - g_ag_slow >= 2000; if (slow) g_ag_slow = now;
            const int n = AgBosses(bs, &session);
            for (int i = 0; i < n && !g_ag_off; ++i) {
                uint64_t a = bs[i], comp = 0, ai = 0, perc = 0, st = 0, b01 = 0, b23 = 0;
                if (!IhAlive(a) || BhAiOn(a) != 1 || (g_ih.phase && g_ih.actor == a) || (g_bh.phase && g_bh.a == a)) continue;  // AI held: Stand still / load hold
                if (!(perc = PercOf(a, &comp, &ai)) || !Rd64(comp + 8, &st) || !st || !Rd64(a + 0x110, &b01) || !Rd64(a + 0x118, &b23)) continue;
                const int idx = AgTarget(a, comp, perc, me, slow);
                float b[4]; memcpy(b, &b01, 8); memcpy(b + 2, &b23, 8);
                const float dx = b[0] - pp[0], dz = b[2] - pp[2], d = sqrtf(dx * dx + dz * dz);
                if (d > (float)g_cfg_aggro_range && nc < MV_CHASE_MAX) chase[nc++] = st;
                if (slow && InterlockedIncrement(&g_ag_logs) <= 200)
                    Log("AGGRO boss %p%s: %.1f m from the player, player entry %d, %s", (void*)a, a == session ? " (session)" : "", d / 100.0f, idx,
                        d > (float)g_cfg_aggro_range ? "closing in with its own AI (repeat waits)" : "in range (repeat / gap as set)");
            }
        }
    }
    for (int i = 0; i < MV_CHASE_MAX; ++i) g_mv_chase[i] = chase[i];
}
static DWORD WINAPI AgSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    WritePrivateProfileStringA("Training", "BossAggro", g_cfg_aggro ? "1" : "0", ini);
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
N3T_API int Nioh3Training_GetBossAggro(void) { return g_cfg_aggro; }
// on = 1 / 0, persist = save [Training] BossAggro. Returns the new value.
N3T_API int Nioh3Training_SetBossAggro(int on, int persist) {
    g_cfg_aggro = on != 0;
    InterlockedExchange(&g_ag_logs, 0);
    Log("AGGRO %s (range %d cm)%s", g_cfg_aggro ? "ON: Training bosses with their AI on keep the player as target and close in" : "OFF", g_cfg_aggro_range, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, AgSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return g_cfg_aggro;
}
static __attribute__((noinline)) int BossPlace(uint64_t a, const float* p3, float yawDeg, const char* who) {
    uint64_t comp = 0, st = 0;
    if (!Rd64(a + 0x3B0, &comp) || !comp || !Rd64(comp + 8, &st) || !st) { Log("RESETPOS %s %p: no AI / state object", who, (void*)a); return 0; }
    __attribute__((aligned(16))) float p[4] = {p3[0], p3[1], p3[2], 1.0f}, m[16];
    float was[4] = {0};
    __try {
        memcpy(was, (void*)(a + 0x110), 16);
        ((void (*)(uint64_t, const float*, int))(B + 0x669AB8))(a, p, 0);
        ((void* (*)(float*, float))(B + 0x5ECE50))(m, yawDeg * 0.0174532925f);
        memcpy((void*)(a + 0x120), m, sizeof m);
        ((void (*)(uint64_t, int))(B + 0x66C330))(st, 0);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("RESETPOS %s %p: native call faulted %08lX", who, (void*)a, GetExceptionCode()); return 0; }
    Log("RESETPOS %s %p: %.0f %.0f %.0f -> %.0f %.0f %.0f yaw %.1f", who, (void*)a, was[0], was[1], was[2], p[0], p[1], p[2], yawDeg);
    return 1;
}
static float AgFacePlayer(const float* bp, float fallback) {
    float pp[4];
    if (!PlayerPos(pp)) return fallback;
    const float dx = pp[0] - bp[0], dz = pp[2] - bp[2];
    return dx * dx + dz * dz < 1.0f ? fallback : atan2f(dx, dz) * 57.2957795f;  // same yaw convention as SpawnTick's 'face the player'
}
// 1 = queued (a session boss with a spawn point or a summoned boss exists), 0 = nothing to put back
N3T_API int Nioh3Training_ResetBossPos(void) {
    const int ok = (g_tr.active && g_tr.hasStart && g_bm.active) || (SlotAlive(0) && g_slot_sp_ok[0]) || (SlotAlive(1) && g_slot_sp_ok[1]);
    if (ok) InterlockedExchange(&g_ag_reset_req, 1);
    Log("RESETPOS request from the menu: %s", ok ? "queued" : "no Training boss with a start point");
    return ok;
}
static void ResetPosTick(void) {
    if (!InterlockedExchange(&g_ag_reset_req, 0)) return;
    uint64_t bs[3], session = 0; int n = AgBosses(bs, &session), done = 0;
    for (int i = 0; i < n; ++i) {
        uint64_t a = bs[i];
        if (!IhAlive(a)) continue;
        if (a == session) {
            if (!g_tr.hasStart) continue;
            done += BossPlace(a, g_tr.bp, AgFacePlayer(g_tr.bp, g_tr.face), "session boss");
            continue;
        }
        for (int s = 0; s < 2; ++s)
            if (g_slot_act[s] == a && g_slot_sp_ok[s]) done += BossPlace(a, g_slot_sp[s], AgFacePlayer(g_slot_sp[s], 0.0f), s ? "Boss B" : "Boss A");
    }
    if (!done) Log("RESETPOS: no live Training boss with a start point (session %p, %d boss(es))", (void*)session, n);
}
