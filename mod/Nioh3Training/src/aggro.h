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
// user #14147: aggro in the 'Stand still' stance (Idle hold phase 2, AI off, drill.h forces the repeat every gap). Out of AggroRangeCm and
// between two forced moves: walk-up = drill paused (g_dr_hold), AI on for the drill (g_ih_drill_ai, as the drill fallback), the AI state in
// g_mv_chase (the repeat does not replace its picks) and IhSelect passes its movement picks (attacks -> idle for the first 6 s, idle_hold.h).
// In range: AI stays on until the boss is back in idle 0x2 (max 4 s; never cut a move), then AI off again and the drill resumes ~0.5 s later.
// Each forced move with aggro ON first turns the boss to the player ([Training] AggroFace, default 1): rotation rows ch+0x120 from the
// 5ECE50 yaw matrix, the same write as BossPlace below (position untouched). Aggro OFF: nothing here runs, the stance is unchanged.
static int g_cfg_aggro_face = 1;
static struct { int st; uint64_t a; DWORD t; } g_agd;  // st 0 none, 1 walking up, 2 arrived (AI on until idle)
static float AgFacePlayer(const float* bp, float fallback);
static void AgDrillEnd(const char* why) {
    if (!g_agd.st) return;
    const uint64_t a = g_agd.a;
    const int ours = g_ih.phase == 2 && g_ih.actor == a && g_dr.chr && g_dr_state == 1;  // stance / drill still ours: put the AI back off
    InterlockedExchange(&g_ih_chase_t, 0);
    if (ours) { InterlockedExchange(&g_ih_drill_ai, 0); if (IhAlive(a) && BhAiOn(a) == 1) IhNativeAi(a, 0); }
    InterlockedExchange(&g_dr_hold, 0);
    Log("AGGRO drill walk-up end (%s): boss %p%s, %lu ms", why, (void*)a, ours ? ", AI off again, drill resumes" : "", GetTickCount() - g_agd.t);
    memset(&g_agd, 0, sizeof g_agd);
}
static void AgDrillFace(uint64_t a) {
    if (!g_cfg_aggro || g_ag_off || !g_cfg_aggro_face || !a) return;
    uint64_t b01 = 0, b23 = 0;
    if (!Rd64(a + 0x110, &b01) || !Rd64(a + 0x118, &b23)) return;
    float b[4]; memcpy(b, &b01, 8); memcpy(b + 2, &b23, 8);
    const float yaw = AgFacePlayer(b, 1e9f);
    if (yaw > 1e8f) return;
    __attribute__((aligned(16))) float m[16];
    __try {
        ((void* (*)(float*, float))(B + 0x5ECE50))(m, yaw * 0.0174532925f);
        memcpy((void*)(a + 0x120), m, sizeof m);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AGGRO face: native yaw matrix faulted %08lX, AggroFace off", GetExceptionCode()); g_cfg_aggro_face = 0; return; }
    if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO drill: boss %p turned to the player (yaw %.1f) before the forced move", (void*)a, yaw);
}
// One Stand-still drill boss (g_ih.actor). Returns its AI state when it must be in g_mv_chase this tick, else 0.
static uint64_t AgDrillBoss(uint64_t a, uint64_t me, const float* pp, int slow, DWORD now) {
    uint64_t comp = 0, ai = 0, perc = 0, st = 0, b01 = 0, b23 = 0;
    if (!IhAlive(a) || (g_bh.phase && g_bh.a == a)) { AgDrillEnd("boss dead / load hold"); return 0; }
    if (!Rd64(a + 0x110, &b01) || !Rd64(a + 0x118, &b23)) return 0;
    float b[4]; memcpy(b, &b01, 8); memcpy(b + 2, &b23, 8);
    const float dx = b[0] - pp[0], dz = b[2] - pp[2], d = sqrtf(dx * dx + dz * dz), r = (float)g_cfg_aggro_range;
    if (g_agd.st == 0) {
        if (d <= r || g_dr.phase != 0 || g_dr.chr == 0) return 0;  // in range, or a forced move still plays: the drill goes on
        g_agd.st = 1; g_agd.a = a; g_agd.t = now;
        InterlockedExchange(&g_dr_hold, 1);
        InterlockedExchange(&g_ih_chase_t, (LONG)(now | 1));
        Log("AGGRO drill: boss %p is %.1f m from the player (range %.1f m): AI on, it walks up (drill paused)", (void*)a, d / 100.0f, r / 100.0f);
    }
    if (g_agd.st == 1) {
        if (d <= r) {
            g_agd.st = 2; g_agd.t = now; InterlockedExchange(&g_ih_chase_t, 0);
            Log("AGGRO drill: boss %p arrived (%.1f m): AI off at its next idle, then the drill move", (void*)a, d / 100.0f);
            return 0;
        }
        if (g_dr_state == 1) { InterlockedExchange(&g_ih_drill_ai, 1); if (BhAiOn(a) == 0) IhNativeAi(a, 1); }  // IhTick leaves it on while set
        if ((perc = PercOf(a, &comp, &ai)) && Rd64(comp + 8, &st) && st) {
            const int idx = AgTarget(a, comp, perc, me, slow);
            if (slow && InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO drill boss %p: %.1f m from the player, player entry %d, walking up", (void*)a, d / 100.0f, idx);
            return st;
        }
        return 0;
    }
    // st 2: arrived; the AI may be mid-step: wait for idle 0x2 (or 4 s) before taking the AI off
    if (IhReadAction(g_ih.ctrl) == 2 || now - g_agd.t >= 4000) AgDrillEnd("in range");
    return 0;
}
static void AggroTick(void) {
    DWORD now = GetTickCount();
    if (now - g_ag_t < 250) return;
    g_ag_t = now;
    uint64_t chase[MV_CHASE_MAX] = {0};
    int nc = 0;
    if (g_agd.st && (!g_cfg_aggro || g_ag_off || g_ih.phase != 2 || g_ih.actor != g_agd.a || !g_dr.chr))
        AgDrillEnd(!g_cfg_aggro ? "aggro off" : g_ag_off ? "aggro faulted" : "stance / drill ended");
    if (g_cfg_aggro && !g_ag_off) {
        uint64_t me = PlayerChar(), bs[3], session = 0; float pp[4];
        if (me && PlayerPos(pp)) {
            const int slow = now - g_ag_slow >= 2000; if (slow) g_ag_slow = now;
            const int n = AgBosses(bs, &session);
            for (int i = 0; i < n && !g_ag_off; ++i) {
                uint64_t a = bs[i], comp = 0, ai = 0, perc = 0, st = 0, b01 = 0, b23 = 0;
                if (g_ih.phase == 2 && g_ih.actor == a && g_dr.chr) {  // user #14147: Stand-still drill boss
                    const uint64_t cs = AgDrillBoss(a, me, pp, slow, now);
                    if (cs && nc < MV_CHASE_MAX) chase[nc++] = cs;
                    continue;
                }
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
