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
// user #14309: aggro is per boss (Boss X page, g_slot_aggro, a new boss starts with the saved [Training] BossAggro). A boss in no slot uses BossAggro.
static int AgOn(uint64_t a) { if (g_tr.active && !g_tr.practice) return 0; const int s = SlotOf(a); return s >= 0 && SlotAlive(s) ? g_slot_aggro[s] != 0 : g_cfg_aggro; }
static int AgAny(void) { if (g_cfg_aggro) return 1; for (int s = 0; s < N3T_SLOTS; ++s) if (g_slot_aggro[s] && SlotAlive(s)) return 1; return 0; }
#define AG_MAX (N3T_SLOTS + 1)
// Live Training bosses: the session boss first, then every slot A, B, C ... (deduplicated). Every actor is checked against the live actor table.
static int AgBosses(uint64_t* out, uint64_t* session) {
    uint64_t cur[1024], me = PlayerChar(); int n = Actors(cur, 1024), k = 0;
    *session = 0;
    if (g_tr.active && g_bm.active) {
        uint64_t a = g_bm.actor ? g_bm.actor : ActorByHandle(g_bm.id);
        if (a && a != me && AgInTable(a, cur, n)) { out[k++] = a; *session = a; }
    }
    for (int s = 0; s < N3T_SLOTS && k < AG_MAX; ++s) {
        uint64_t a = g_slot_act[s];
        if (!a || a == me || !SlotAlive(s) || !AgInTable(a, cur, n)) continue;
        int dup = 0; for (int j = 0; j < k; ++j) dup |= out[j] == a;
        if (!dup) out[k++] = a;
    }
    return k;
}
static __attribute__((noinline)) int AgTarget(uint64_t a, uint64_t comp, uint64_t perc, uint64_t me, int slow) {
    __try {
        if (slow) ((void (*)(uint64_t, uint8_t, uint32_t))(B + Grva(0x1FF1FA0)))(comp, 1, 0);
        int idx = PercFind(perc, me);
        if (idx >= 0 && *(int32_t*)(perc + 0x2D0) != idx) {
            int was = *(int32_t*)(perc + 0x2D0);
            ((void (*)(uint64_t, int))(B + Grva(0x2BE2E8)))(perc, idx);
            if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO boss %p: current target %d -> player (entry %d)", (void*)a, was, idx);
        }
        uint64_t aiP = 0;
        if (slow && Rd64(me + 0x3B0, &aiP) && aiP) ((void (*)(uint64_t, uint64_t, float))(B + Grva(0x1FF23C8)))(comp, aiP, 500.0f);
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
// user #14546 (live 23:39-23:41, 'Training here' session, Yoritomo 0x4E21 > 0x4E22 / 0x4E5C > 0x4E60 > 0x4E61): a Stand-still boss repeating a
// whole SEQUENCE has no drill (drill.h DR_SEQ: move_toggle.h MvRepTick forces the head with the AI off), so the walk-up below, keyed on g_dr,
// never ran and the generic AI-on path skips a held boss: aggro ON did nothing at all (no AGGRO line after 'Boss A ON'). Same walk-up rules
// for it: start only between two sequences (MvSeqBusy 0), AI on + locomotion only until in range, then AI off; the head is forced after the gap.
static int AgStandSeq(uint64_t a) { return g_ih.phase == 2 && g_ih.actor == a && g_ih.chr && Nioh3Training_GetMoveRepeatSeq(g_ih.chr); }
static int AgStandRep(uint64_t a) { return g_ih.phase == 2 && g_ih.actor == a && (g_dr.chr || AgStandSeq(a)); }  // drill or sequence repeat
static void AgDrillEnd(const char* why) {
    if (!g_agd.st) return;
    const uint64_t a = g_agd.a;
    const int ours = g_ih.phase == 2 && g_ih.actor == a && ((g_dr.chr && g_dr_state == 1) || AgStandSeq(a));  // stance / repeat still ours: AI back off
    InterlockedExchange(&g_ih_chase_t, 0);
    if (ours) { InterlockedExchange(&g_ih_drill_ai, 0); if (IhAlive(a) && BhAiOn(a) == 1) IhNativeAi(a, 0); }
    else if (g_dr_state != 2) InterlockedExchange(&g_ih_drill_ai, 0);  // repeat stopped mid walk-up: Idle hold takes the AI off again (not the drill fallback)
    InterlockedExchange(&g_dr_hold, 0);
    if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO drill walk-up end (%s): boss %p%s, %lu ms", why, (void*)a, ours ? ", AI off again, drill resumes" : "", GetTickCount() - g_agd.t);
    memset(&g_agd, 0, sizeof g_agd);
}
static void AgDrillFace(uint64_t a) {
    if (!a || !AgOn(a) || g_ag_off || !g_cfg_aggro_face) return;
    uint64_t b01 = 0, b23 = 0;
    if (!Rd64(a + 0x110, &b01) || !Rd64(a + 0x118, &b23)) return;
    float b[4]; memcpy(b, &b01, 8); memcpy(b + 2, &b23, 8);
    const float yaw = AgFacePlayer(b, 1e9f);
    if (yaw > 1e8f) return;
    __attribute__((aligned(16))) float m[16];
    __try {
        ((void* (*)(float*, float))(B + Grva(0x5ECE50)))(m, yaw * 0.0174532925f);
        memcpy((void*)(a + 0x120), m, sizeof m);
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("AGGRO face: native yaw matrix faulted %08lX, AggroFace off", GetExceptionCode()); g_cfg_aggro_face = 0; return; }
    if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO drill: boss %p turned to the player (yaw %.1f) before the forced move", (void*)a, yaw);
}
// user #14546: move_toggle.h MV_PREFORCE, right before a sequence head is forced: a Stand-still boss (its AI state = g_ih.ctrl) faces the player
static void AgSeqFace(uint64_t st) { if (st && g_ih.phase == 2 && g_ih.ctrl == st && g_ih.actor) AgDrillFace(g_ih.actor); }
// One Stand-still drill boss (g_ih.actor). Returns its AI state when it must be in g_mv_chase this tick, else 0.
// #14267 (live 20:02): hysteresis = walk-up starts only beyond AggroRangeCm + 100 (never while the player is inside the range) and ends at
// AggroRangeCm; the 'arrived, AI on until idle' part keeps the locomotion-only filter, so the AI cannot start an attack before the drill does.
// The AI walked away / stood (6.5 -> 8.7 m) with its attacks filtered: no closing by >= 0.5 m within AggroWalkTimeoutMs (default 6000, 0 = never)
// -> the boss is placed AggroRangeCm - 100 from the player on its own side, facing the player (BossPlace, the Reset boss position mover).
static int g_cfg_aggro_walk_ms = 6000;
static __attribute__((noinline)) int BossPlace(uint64_t a, const float* p3, float yawDeg, const char* who);
static float g_agd_best;
static uint64_t AgDrillBoss(uint64_t a, uint64_t me, const float* pp, int slow, DWORD now) {
    uint64_t comp = 0, ai = 0, perc = 0, st = 0, b01 = 0, b23 = 0;
    if (!IhAlive(a) || (g_bh.phase && g_bh.a == a)) { AgDrillEnd("boss dead / load hold"); return 0; }
    if (!Rd64(a + 0x110, &b01) || !Rd64(a + 0x118, &b23)) return 0;
    float b[4]; memcpy(b, &b01, 8); memcpy(b + 2, &b23, 8);
    const float dx = b[0] - pp[0], dz = b[2] - pp[2], d = sqrtf(dx * dx + dz * dz), r = (float)g_cfg_aggro_range;
    const int haveSt = (perc = PercOf(a, &comp, &ai)) && Rd64(comp + 8, &st) && st;
    const int seq = AgStandSeq(a);  // user #14546: sequence repeat (no drill)
    if (g_agd.st == 0) {
        if (d <= r + 100.0f || (seq ? MvSeqBusy(g_ih.chr) : g_dr.phase != 0 || g_dr.chr == 0)) return 0;  // inside range (+1 m hysteresis), or a forced move / sequence still plays
        g_agd.st = 1; g_agd.a = a; g_agd.t = now; g_agd_best = d;
        InterlockedExchange(&g_dr_hold, 1);
        InterlockedExchange(&g_ih_chase_t, (LONG)(now | 1));
        if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO drill: boss %p is %.1f m from the player (range %.1f m + 1 m): AI on, it walks up (drill paused, locomotion only)", (void*)a, d / 100.0f, r / 100.0f);
    }
    if (g_agd.st == 1) {
        if (d <= r) {
            g_agd.st = 2; g_agd.t = now;
            if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO drill: boss %p arrived (%.1f m): AI off at its next idle, then the drill move", (void*)a, d / 100.0f);
            return haveSt ? st : 0;
        }
        if (d < g_agd_best - 50.0f) { g_agd_best = d; g_agd.t = now; }  // progress: restart the no-progress clock
        if (g_cfg_aggro_walk_ms > 0 && now - g_agd.t >= (DWORD)g_cfg_aggro_walk_ms) {
            const float k = (r - 100.0f > 100.0f ? r - 100.0f : 100.0f) / (d > 1.0f ? d : 1.0f);
            const float np[3] = {pp[0] + dx * k, b[1], pp[2] + dz * k};
            Log("AGGRO drill: boss %p did not close in for %d ms (%.1f m): placed %.1f m from the player", (void*)a, g_cfg_aggro_walk_ms, d / 100.0f, (r - 100.0f) / 100.0f);
            g_agd.st = 2; g_agd.t = now;
            BossPlace(a, np, AgFacePlayer(np, 0.0f), "aggro walk-up");
            return haveSt ? st : 0;
        }
        if (g_dr_state == 1 || seq) { InterlockedExchange(&g_ih_drill_ai, 1); if (BhAiOn(a) == 0) IhNativeAi(a, 1); }  // IhTick leaves it on while set
        if (haveSt) {
            const int idx = AgTarget(a, comp, perc, me, slow);
            if (slow && InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO drill boss %p: %.1f m from the player, player entry %d, walking up", (void*)a, d / 100.0f, idx);
            return st;
        }
        return 0;
    }
    // st 2: arrived; the AI may be mid-step: wait for idle 0x2 (or 4 s) before taking the AI off; still locomotion only meanwhile
    if (IhReadAction(g_ih.ctrl) == 2 || now - g_agd.t >= 4000) { AgDrillEnd("in range"); return 0; }
    return haveSt ? st : 0;
}
// user #14462: an AI-on Training boss with a repeat armed walks up with locomotion only while it is far (move_toggle.h MvChaseFilter: its
// attack picks become idle). If it does not close in by >= 0.5 m within AggroWalkTimeoutMs it is placed AggroRangeCm - 100 from the player,
// facing the player (the Stand-still walk-up rule, BossPlace). Returns 1 when it was placed.
static struct { uint64_t a; float best; DWORD t; } g_agw[AG_MAX];
static int AgRepWalk(uint64_t a, uint32_t chr, float d, const float* b, const float* pp, DWORD now) {
    int e = -1, f = -1;
    for (int k = 0; k < AG_MAX; ++k) { if (g_agw[k].a == a) { e = k; break; } if (f < 0 && !g_agw[k].a) f = k; }
    const float r = (float)g_cfg_aggro_range;
    if (!chr || !MvRepeatId(chr) || d <= r || g_cfg_aggro_walk_ms <= 0) { if (e >= 0) g_agw[e].a = 0; return 0; }
    if (e < 0) { if (f < 0) return 0; e = f; g_agw[e].a = a; g_agw[e].best = d; g_agw[e].t = now; return 0; }
    if (d < g_agw[e].best - 50.0f) { g_agw[e].best = d; g_agw[e].t = now; return 0; }
    if (now - g_agw[e].t < (DWORD)g_cfg_aggro_walk_ms) return 0;
    const float dx = b[0] - pp[0], dz = b[2] - pp[2];
    const float k = (r - 100.0f > 100.0f ? r - 100.0f : 100.0f) / (d > 1.0f ? d : 1.0f);
    const float np[3] = {pp[0] + dx * k, b[1], pp[2] + dz * k};
    Log("AGGRO repeat boss %p (chr %u) did not close in for %d ms (%.1f m, attacks held while far): placed %.1f m from the player", (void*)a, chr, g_cfg_aggro_walk_ms, d / 100.0f, (r - 100.0f) / 100.0f);
    g_agw[e].a = 0;
    BossPlace(a, np, AgFacePlayer(np, 0.0f), "aggro repeat walk-up");
    return 1;
}
// user #14462: a summoned boss held idle by its slot (training.c SphTick, SPH_TRAINING) keeps the AI off while it repeats a move / sequence.
// With aggro ON and a repeat armed it walks up like the Stand-still drill boss: beyond AggroRangeCm + 1 m and not mid-move -> g_sph.chase
// (SphTick switches the AI on), its AI state in g_mv_chase (MvChaseFilter: locomotion only, MvRepTick forces nothing); inside the range the
// chase ends and SphTick holds the AI again, the repeat goes on. Returns the AI state to put in g_mv_chase, else 0.
static uint64_t AgHeldSlot(int sl, uint64_t a, uint64_t me, const float* pp, int slow, DWORD now) {
    const uint32_t chr = g_slot_chr[sl];
    uint64_t comp = 0, ai = 0, perc = 0, st = 0, b01 = 0, b23 = 0;
    if (!chr || !MvRepeatId(chr) || !(perc = PercOf(a, &comp, &ai)) || !Rd64(comp + 8, &st) || !st ||
        !Rd64(a + 0x110, &b01) || !Rd64(a + 0x118, &b23)) { g_sph[sl].chase = 0; return 0; }
    float b[4]; memcpy(b, &b01, 8); memcpy(b + 2, &b23, 8);
    const float dx = b[0] - pp[0], dz = b[2] - pp[2], d = sqrtf(dx * dx + dz * dz), r = (float)g_cfg_aggro_range;
    const int ri = MvRepFind(chr);
    const uint32_t cur = ActIdOf(st);
    const int k = cur >= 0x4E20 && cur != 0xFFFF && cur != 0xFFFFFFFFu ? N3MoveFind(chr, cur) : -1;
    const int busy = (ri >= 0 && g_rep_seq[ri] && g_rep_phase[ri] != 0) || (cur >= 0x4E20 && cur != 0xFFFF && cur != 0xFFFFFFFFu && !(k >= 0 && kN3Moves[k].kind == 'M'));
    const int was = g_sph[sl].chase, want = SphChaseWanted(was, d, r, busy);
    if (want != was && InterlockedIncrement(&g_ag_logs) <= 200)
        Log(want ? "AGGRO Boss %c %p: %.1f m from the player (range %.1f m + 1 m): AI on, it walks up (locomotion only, repeat waits)"
                 : "AGGRO Boss %c %p: in range (%.1f m, range %.1f m): AI held again, the repeat goes on", 'A' + sl, (void*)a, d / 100.0f, r / 100.0f);
    g_sph[sl].chase = want;
    if (!want) return 0;
    AgTarget(a, comp, perc, me, slow);
    if (AgRepWalk(a, chr, d, b, pp, now)) { g_sph[sl].chase = 0; return 0; }  // no closing in for AggroWalkTimeoutMs: placed in range
    return st;
}
static void AggroTick(void) {
    DWORD now = GetTickCount();
    if (now - g_ag_t < 250) return;
    g_ag_t = now;
    uint64_t chase[MV_CHASE_MAX] = {0};
    int nc = 0, vis[N3T_SLOTS] = {0};  // #14462: held slot bosses walked up this tick (any other slot's walk-up ends: aggro off / boss gone)
    if (g_agd.st && (!AgOn(g_agd.a) || g_ag_off || !AgStandRep(g_agd.a)))
        AgDrillEnd(!AgOn(g_agd.a) ? "aggro off" : g_ag_off ? "aggro faulted" : "stance / drill ended");
    if (AgAny() && !g_ag_off) {
        uint64_t me = PlayerChar(), bs[AG_MAX], session = 0; float pp[4];
        if (me && PlayerPos(pp)) {
            const int slow = now - g_ag_slow >= 2000; if (slow) g_ag_slow = now;
            const int n = AgBosses(bs, &session);
            for (int i = 0; i < n && !g_ag_off; ++i) {
                uint64_t a = bs[i], comp = 0, ai = 0, perc = 0, st = 0, b01 = 0, b23 = 0;
                if (!AgOn(a)) continue;  // user #14309: this boss has aggro OFF
                if (AgStandRep(a)) {  // user #14147: Stand-still drill boss; #14546: or a Stand-still sequence repeat
                    const uint64_t cs = AgDrillBoss(a, me, pp, slow, now);
                    if (cs && nc < MV_CHASE_MAX) chase[nc++] = cs;
                    continue;
                }
                { const int sl = SlotOf(a);  // user #14462: summoned boss held idle by its slot, repeating with the AI off
                  if (sl >= 0 && g_sph[sl].phase && g_sph[sl].ms == SPH_TRAINING && g_sph[sl].a == a && !(g_ih.phase && g_ih.actor == a)) {
                      if (IhAlive(a) && !(g_bh.phase && g_bh.a == a)) {
                          const uint64_t cs = AgHeldSlot(sl, a, me, pp, slow, now);
                          if (cs && nc < MV_CHASE_MAX) { chase[nc++] = cs; vis[sl] = 1; }
                      }
                      continue;
                  } }
                if (!IhAlive(a) || BhAiOn(a) != 1 || (g_ih.phase && g_ih.actor == a) || (g_bh.phase && g_bh.a == a)) continue;  // AI held: Stand still / load hold
                if (!(perc = PercOf(a, &comp, &ai)) || !Rd64(comp + 8, &st) || !st || !Rd64(a + 0x110, &b01) || !Rd64(a + 0x118, &b23)) continue;
                const int idx = AgTarget(a, comp, perc, me, slow);
                float b[4]; memcpy(b, &b01, 8); memcpy(b + 2, &b23, 8);
                const float dx = b[0] - pp[0], dz = b[2] - pp[2], d = sqrtf(dx * dx + dz * dz);
                { const int sl = SlotOf(a); const uint32_t chr = sl >= 0 ? g_slot_chr[sl] : a == session ? g_tr.chr : 0;
                  if (AgRepWalk(a, chr, d, b, pp, now)) continue; }  // user #14462: placed in range, no chase this tick
                if (d > (float)g_cfg_aggro_range && nc < MV_CHASE_MAX) chase[nc++] = st;
                if (slow && InterlockedIncrement(&g_ag_logs) <= 200)
                    Log("AGGRO boss %p%s: %.1f m from the player, player entry %d, %s", (void*)a, a == session ? " (session)" : "", d / 100.0f, idx,
                        d > (float)g_cfg_aggro_range ? "closing in with its own AI (repeat waits)" : "in range (repeat / gap as set)");
            }
        }
    }
    for (int s = 0; s < N3T_SLOTS; ++s) if (!vis[s] && g_sph[s].chase) { g_sph[s].chase = 0; if (InterlockedIncrement(&g_ag_logs) <= 200) Log("AGGRO Boss %c: walk-up ended (aggro off / in range / boss gone), AI held again", 'A' + s); }
    for (int i = 0; i < MV_CHASE_MAX; ++i) g_mv_chase[i] = chase[i];
}
static DWORD WINAPI AgSaveThread(LPVOID p) {
    (void)p; char ini[MAX_PATH]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    WritePrivateProfileStringA("Training", "BossAggro", g_cfg_aggro ? "1" : "0", ini);
    WritePrivateProfileStringA(0, 0, 0, ini); return 0;
}
N3T_API int Nioh3Training_GetBossAggro(void) { return g_cfg_aggro; }
// on = 1 / 0, persist = save [Training] BossAggro. Returns the new value.
N3T_API int Nioh3Training_SetBossAggro(int on, int persist) {  // every boss + the default for new ones (old menu / command)
    g_cfg_aggro = on != 0;
    for (int s = 0; s < N3T_SLOTS; ++s) InterlockedExchange(&g_slot_aggro[s], g_cfg_aggro);
    InterlockedExchange(&g_ag_logs, 0);
    Log("AGGRO %s (range %d cm)%s", g_cfg_aggro ? "ON: Training bosses with their AI on keep the player as target and close in" : "OFF", g_cfg_aggro_range, persist ? " (saved to ini)" : "");
    if (persist) { HANDLE t = CreateThread(0, 0, AgSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    return g_cfg_aggro;
}
// user #14309: per boss. -1 = empty slot. Set: on 1 / 0; the value is also saved as the default for the next boss ([Training] BossAggro).
N3T_API int Nioh3Training_GetSlotAggro(int s) { return SlotAlive(s) ? (int)(g_slot_aggro[s] != 0) : -1; }
N3T_API int Nioh3Training_SetSlotAggro(int s, int on) {
    if (!SlotAlive(s)) return -1;
    InterlockedExchange(&g_slot_aggro[s], on != 0); InterlockedExchange(&g_ag_logs, 0);
    g_cfg_aggro = on != 0; { HANDLE t = CreateThread(0, 0, AgSaveThread, 0, 0, 0); if (t) CloseHandle(t); }
    Log("AGGRO Boss %c %s (range %d cm; saved as the default for new bosses)", 'A' + s, on ? "ON" : "OFF", g_cfg_aggro_range);
    return on != 0;
}
static __attribute__((noinline)) int BossPlace(uint64_t a, const float* p3, float yawDeg, const char* who) {
    uint64_t comp = 0, st = 0;
    if (!Rd64(a + 0x3B0, &comp) || !comp || !Rd64(comp + 8, &st) || !st) { Log("RESETPOS %s %p: no AI / state object", who, (void*)a); return 0; }
    __attribute__((aligned(16))) float p[4] = {p3[0], p3[1], p3[2], 1.0f}, m[16];
    float was[4] = {0};
    __try {
        memcpy(was, (void*)(a + 0x110), 16);
        ((void (*)(uint64_t, const float*, int))(B + Grva(0x669AB8)))(a, p, 0);
        ((void* (*)(float*, float))(B + Grva(0x5ECE50)))(m, yawDeg * 0.0174532925f);
        memcpy((void*)(a + 0x120), m, sizeof m);
        ((void (*)(uint64_t, int))(B + Grva(0x66C330)))(st, 0);
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
// 1 = queued (a session boss with a spawn point or a summoned boss exists), 0 = nothing to put back. g_ag_reset_req: bit 31 = every boss,
// bit s = only Boss <s> (user #14309, Nioh3Training_ResetSlotPos).
N3T_API int Nioh3Training_ResetBossPos(void) {
    int ok = g_tr.active && g_tr.hasStart && g_bm.active;
    for (int s = 0; s < N3T_SLOTS; ++s) ok |= SlotAlive(s) && g_slot_sp_ok[s];
    if (ok) InterlockedOr(&g_ag_reset_req, (LONG)0x80000000);
    Log("RESETPOS request from the menu: %s", ok ? "queued" : "no Training boss with a start point");
    return ok;
}
N3T_API int Nioh3Training_ResetSlotPos(int s) {
    const int ok = SlotAlive(s) && (g_slot_sp_ok[s] || (g_slot_ih[s] && g_tr.active && g_tr.hasStart));
    if (ok) InterlockedOr(&g_ag_reset_req, (LONG)(1u << s));
    Log("RESETPOS Boss %c from the menu: %s", 'A' + s, ok ? "queued" : "no start point");
    return ok;
}
static void ResetPosTick(void) {
    const LONG req = InterlockedExchange(&g_ag_reset_req, 0);
    if (!req) return;
    uint64_t bs[AG_MAX], session = 0; int n = AgBosses(bs, &session), done = 0;
    for (int i = 0; i < n; ++i) {
        uint64_t a = bs[i];
        const int sl = SlotOf(a);
        if (!IhAlive(a) || (!(req & (LONG)0x80000000) && (sl < 0 || !(req & (LONG)(1u << sl))))) continue;
        if (a == session && g_tr.hasStart) {
            done += BossPlace(a, g_tr.bp, AgFacePlayer(g_tr.bp, g_tr.face), "session boss");
            continue;
        }
        if (sl >= 0 && g_slot_sp_ok[sl]) { char who[8] = {'B', 'o', 's', 's', ' ', (char)('A' + sl), 0}; done += BossPlace(a, g_slot_sp[sl], AgFacePlayer(g_slot_sp[sl], 0.0f), who); }
    }
    if (!done) Log("RESETPOS: no live Training boss with a start point (session %p, %d boss(es))", (void*)session, n);
}
