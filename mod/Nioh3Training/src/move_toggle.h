// Included by training.c after the training-session and boss ownership helpers.
#include "../../boss_moves.h"

static uint32_t g_mv_block[64];
static volatile int g_mv_nblock;
static volatile LONG g_mv_vetoes, g_mv_rerolls;
static volatile LONG g_mv_hooked;
static SRWLOCK g_mv_legacy_lock = SRWLOCK_INIT;
static uint32_t g_mv_fallback = 2;
// 0 inherits legacy MoveBlock; 1 explicitly allows; 2 explicitly blocks.
#ifndef N3CAT_CAP
#define N3CAT_CAP N3_MOVE_COUNT  // tests: the generated table; training.c: the live catalogue capacity (move_catalog.h)
#endif
static volatile LONG g_mv_pref[N3CAT_CAP], g_mv_current_chr, g_mv_install_pending;
typedef uint32_t (*MvSelFn)(uint64_t, uint64_t);
typedef uint64_t (*MvComFn)(uint64_t, uint32_t, uint64_t, int, uint64_t);
static MvSelFn o_mvsel;
static MvComFn o_mvcom;
static uint32_t IhSelect(uint64_t st, uint32_t id);

static int MvBlocked(uint32_t chr, uint32_t id) {
    if (g_tr.active && !g_tr.practice) return 0; // saved Training vetoes do not alter Boss Matches
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
// user #14309 / #14336: 'Gap between repeats' is per boss (Boss X page). Per character id (the repeat table is per character too); a boss with
// no entry uses RepeatGapMs. Saved as [Moves.<chr>] RepeatGapMs, loaded with the move switches (MvLoadPrefs).
#define MV_GAP_MAX 32
static volatile LONG g_gap_chr[MV_GAP_MAX], g_gap_ms[MV_GAP_MAX];
static int MvGapFor(uint32_t chr) {
    for (int i = 0; chr && i < MV_GAP_MAX; ++i) if ((uint32_t)g_gap_chr[i] == chr) return (int)g_gap_ms[i];
    return g_cfg_rep_gap;
}
static int MvGapPut(uint32_t chr, int ms) {
    int f = -1;
    for (int i = 0; i < MV_GAP_MAX; ++i) { if ((uint32_t)g_gap_chr[i] == chr) { f = i; break; } if (f < 0 && !g_gap_chr[i]) f = i; }
    if (f < 0 || !chr) return 0;
    InterlockedExchange(&g_gap_ms[f], ms); InterlockedExchange(&g_gap_chr[f], (LONG)chr);
    return 1;
}
// user #13787 'Boss aggro on me' (aggro.h): AI states of bosses that are farther from the player than AggroRangeCm while aggro is ON.
// Their own AI picks pass (the boss walks / closes in natively) and no repeat is forced from range; the drill resumes once close.
#ifndef MV_CHASE_MAX
#define MV_CHASE_MAX 3  // training.c: one per Training boss (N3T_SLOTS + 1)
#endif
static volatile uint64_t g_mv_chase[MV_CHASE_MAX];
static int MvChasing(uint64_t st) { for (int i = 0; st && i < MV_CHASE_MAX; ++i) if (g_mv_chase[i] == st) return 1; return 0; }
static int MvRepFind(uint32_t chr) { for (int i = 0; i < MV_REP_MAX; ++i) if (chr && (uint32_t)g_rep_chr[i] == chr) return i; return -1; }
static uint32_t MvRepeatId(uint32_t chr) { if (g_tr.active && !g_tr.practice) return 0; int i = MvRepFind(chr); return i >= 0 ? (uint32_t)g_rep_id[i] : 0; }
// user #14423: 'Sequences' repeat (Boss moves > Sequences). The old path repeated only the chain's first move: the select hook runs at the
// boss's decision points (end of each action, many calls per frame in idle), so 'first pick after a repeat = the move finished' turned the
// pick at the end of the head into the gap's idle 0x2 (live 22:17:46-22:18:33: 0x4E7B -> 0x2 instead of its link 0x4EA7) and once the gap
// passed every AI pick (0x4E30, 0x4E55, 0x4E6E ...) became the head again, about every 0.5 s. Sequence mode forces ONLY the head, then
// follows the real action (ActIdOf) and never overrides a pick while the chain advances: while the action is a chain member (head + its
// +0xE0 next-action links, mod/boss_sequences.h) or the boss carries on with its own follow-up (0x4EA7 > 0x4E7C > 0x18D1), the game's
// own pick passes. The sequence counts as ended when the boss is back in idle 0x2; the gap starts then, and only after it is the head
// picked / forced again (MvRepTick).
#include "../../boss_sequences.h"
#define MV_SEQ_MAX 8
static volatile LONG g_rep_seq[MV_REP_MAX];          // 1 = repeat the whole chain from g_rep_id
static uint32_t g_rep_chain[MV_REP_MAX][MV_SEQ_MAX]; static int g_rep_nchain[MV_REP_MAX];
static int g_rep_phase[MV_REP_MAX];                  // sequence mode: 0 ended / gap, 1 head requested, 2 chain playing, 3 own follow-up
static uint32_t g_rep_cur[MV_REP_MAX];               // last action seen while the sequence runs (log once per action)
#ifndef MV_FORCE
#define MV_FORCE(st, id) ((void (*)(uint64_t, uint64_t, uint64_t, float))(B + Grva(0x66C4A8)))((st), (uint64_t)(id), 0, 0.0f)
#endif
// user #14546: runs right before the sequence head is forced (training.c: aggro.h AgSeqFace, a Stand-still boss with aggro ON turns to the player)
#ifndef MV_PREFORCE
#define MV_PREFORCE(st) ((void)0)
#endif
static const N3Next* MvSeqLink(uint32_t chr, uint32_t id) { for (int i = 0; i < N3_NEXT_COUNT; ++i) if (kN3Next[i].chr == chr && kN3Next[i].id == id) return &kN3Next[i]; return 0; }
// head, next, ... like the menu's SeqChain (training_bosses_menu.h): stops at a repeat, after a looping part, or at idle / init
static int MvSeqBuild(uint32_t chr, uint32_t head, uint32_t* ids, int cap) {
    int n = 0;
    for (uint32_t c = head; n < cap && c && !N3MoveProtected(c);) {
        ids[n++] = c;
        const N3Next* l = MvSeqLink(chr, c); if (!l) break;
        int seen = 0; for (int k = 0; k < n; ++k) seen |= ids[k] == l->next;
        if (seen) break;
        c = l->next;
        if (l->loop) { if (n < cap && !N3MoveProtected(c)) ids[n++] = c; break; }
    }
    return n;
}
static int MvSeqIndex(int i, uint32_t act) { for (int k = 0; k < g_rep_nchain[i]; ++k) if (g_rep_chain[i][k] == act) return k; return -1; }
// user #14462 (live 22:39, Yoritomo 0x4E21 > 0x4E22): right after the head was picked / forced ActIdOf already reads 0x4E21 (the request), the
// old code took that as 'the head started', the AI's next pick (0x4E30, or the veto idle for a part switched OFF) replaced it within a frame,
// the action read idle 0x2 again and the sequence 'ended' 0-32 ms after the start, gap, head again: every 500 ms, the chain never played.
// Now: the head counts as started only after it was read for SEQ_CONFIRM_MS, or the boss moved on to another action >= 0x4E20 (its branch,
// live 0x4E21 -> 0x4E23) that is not the action it was still playing when the head was asked for (#14712, live 23:48:38 / 00:15:47: the
// old 0x4E7D / 0x4E5C read as 'the head branched' 0 ms after the request); until then every AI pick is answered with the head. Not started
// within SEQ_START_MS: gap, then again (no re-force every 500 ms). While the chain plays: MvFollow (game's own next piece passes, another
// attack is refused); back in idle by itself = ended, then the gap.
#define SEQ_CONFIRM_MS 100
#define SEQ_START_MS 1500
static int g_rep_kmax[MV_REP_MAX];                  // highest chain part seen playing this round (-1 none)
static DWORD g_rep_seen[MV_REP_MAX];                // phase 1: first read of the head (0 = not read yet); single move: the move was read playing
static volatile LONG g_rep_subs;                    // log cap for follow-up pick decisions
// user #14702 / #14710 / #14712 (live 00:37-00:38, Yoritomo 0x4E78 > 0x4E7A, repeats 39-42): the select hook is also the game's own 'next
// piece' decision inside an attack (it fires with the AI off too). Every listed sequence lost its real pieces: the native 0x4E79 at the end
// of 0x4E78 became the listed 0x4E7A, then the native 0x4E8D ~400 ms into 0x4E7A became idle, so part two lasted 375-422 ms and the whole
// round ~1.5 s. Same for 0x4E22 > 0x4E23 (loop exit -> loop again), 0x4E60 > 0x4E62, 0x4E77 > 0x4E78, 0x4EA7 > 0x4E7C. The decoded
// records show these are pieces of ONE attack: same motion family (motion / 10: 0x4E76..0x4E7A = 1100..1104, 0x4E8D = 1104, 0x4E5C /
// 0x4E60 / 0x4E62 = 1020 / 1021 / 1022, 0x4E21 / 0x4E22 / 0x4E23 = 10 / 11 / 12, 0x4E7B / 0x4EA7 / 0x4E7C = 1060 / 1062 / 1062). A pick of
// ANOTHER family is the AI starting a new attack (live 22:36: 0x4E21 > 0x4E23 > 0x4E5C > 0x4E60 > 0x4E62 > 0x4E68 = 'random stuff',
// user #14462). So while a repeated move / sequence plays: the game's pick passes when it is the running action's own next piece (its
// +0xE0 link, the same motion family, a looping part going on, a later listed part, or the game's own idle 0x2); any other pick is
// refused (MV_KEEP: the commit refuses 0xFFFF before any write, the running action plays to its end, recovery included). The round ends
// when the boss is back in idle by itself (ActIdOf 0x2), never at a listed-part count; the gap starts then.
#include "move_motion.h"
#define MV_KEEP 0xFFFFu            // select result: no new action now (the running one plays on)
#define MV_STUCK_MS 5000           // an action that did not change for this long takes the game's pick anyway (no endless loop part)
#define MV_PASS_MAX 8
static uint32_t g_rep_pass[MV_REP_MAX][MV_PASS_MAX]; static int g_rep_npass[MV_REP_MAX];  // follow-up picks passed this round (commit veto bypass)
static DWORD g_rep_curt[MV_REP_MAX];                  // when g_rep_cur last changed
static uint32_t g_rep_from[MV_REP_MAX];               // the action read when the head was requested (still playing = not the head yet)
static uint64_t g_rep_logk[MV_REP_MAX];               // last (action, pick) logged by MvFollow
static unsigned MvMotion(uint32_t chr, uint32_t id) { for (int i = 0; i < N3_MOTION_COUNT; ++i) if (kN3Motion[i].chr == chr && kN3Motion[i].id == id) return kN3Motion[i].motion; return 0; }
// 1 = pick is the running action cur's own next piece (see above). Boss-killing moves never are.
// 1 = id is a looping part (a link into it carries the loop flag: 0x4E21 > 0x4E22 loops until the game picks the exit 0x4E23)
static int MvLoops(uint32_t chr, uint32_t id) { for (int i = 0; i < N3_NEXT_COUNT; ++i) if (kN3Next[i].chr == chr && kN3Next[i].next == id && kN3Next[i].loop) return 1; return 0; }
static int MvContinues(uint32_t chr, uint32_t cur, uint32_t pick) {
    if (N3MoveUnsafe(chr, pick) || N3MoveProtected(pick) || N3MoveProtected(cur)) return 0;
    const N3Next* l = MvSeqLink(chr, cur);
    const int hands = l || MvLoops(chr, cur);      // cur does not end in idle by itself: it hands over to a next action
    if (pick == cur) return (l && l->next == cur) || MvLoops(chr, cur);  // only a looping part goes on with itself (no restart)
    if (l && l->next == pick) return 1;            // its own next-action link
    // Packed transition rows 288/290: Yoritomo's native continuation can
    // decrease motion 1103 -> 1102. Admit only a pick already made by the
    // game; never synthesize a chain or override its conditions/frame gates.
    if (chr == 16102u && ((cur == 0x4E79 && pick == 0x4E8C) ||
                         (cur == 0x4E67 && pick == 0x4E70))) return 1;
    const unsigned mc = MvMotion(chr, cur), mp = MvMotion(chr, pick);
    if (mc && mp) return mc / 10 == mp / 10 && mp >= mc;  // decoded records: same attack family, forward (no restart of an earlier piece)
    return hands;                                  // no motion data (other bosses): a part that hands over by itself takes the game's pick
}
static int MvPassed(int i, uint32_t id) { for (int k = 0; k < g_rep_npass[i]; ++k) if (g_rep_pass[i][k] == id) return 1; return 0; }
static void MvRoundReset(int i) { g_rep_npass[i] = 0; g_rep_cur[i] = 0; g_rep_curt[i] = 0; g_rep_seen[i] = 0; g_rep_logk[i] = 0; }
// a repeated move / sequence is playing and the game picked id while the boss runs act: id (pass) or MV_KEEP (refused, act plays on)
static uint32_t MvFollow(int i, uint32_t chr, uint32_t act, uint32_t id, DWORD now) {
    const uint32_t cur = act == 0xFFFF || act == 0xFFFFFFFFu ? g_rep_cur[i] : act;  // transition frame: the last action read
    const int kc = MvSeqIndex(i, cur), kp = MvSeqIndex(i, id);
    const char* why = 0;
    if (id == 2) why = "the game's own idle";
    else if (kp >= 0 && kc >= 0 && kp > kc && !N3MoveUnsafe(chr, id)) why = "a later listed part";  // after the running listed part
    else if (cur && MvContinues(chr, cur, id)) why = "the next piece of the running attack";
    else if (cur && g_rep_curt[i] && now - g_rep_curt[i] >= MV_STUCK_MS && !N3MoveUnsafe(chr, id)) why = "the running action did not end by itself";
    const uint64_t key = ((uint64_t)cur << 32) | id;
    if (g_rep_logk[i] != key) {
        g_rep_logk[i] = key;
        if (InterlockedIncrement(&g_rep_subs) <= 400) Log("REPEAT chr %u %s: game pick 0x%X during 0x%X %s (%s, %lu ms after the start)", chr, g_rep_seq[i] ? "sequence" : "move", id, cur,
                                                          why ? "passes" : "refused", why ? why : "another attack: the running action plays to its end", now - g_rep_last[i]);
    }
    if (!why) return MV_KEEP;
    if (id != 2 && !MvPassed(i, id) && g_rep_npass[i] < MV_PASS_MAX) g_rep_pass[i][g_rep_npass[i]++] = id;
    return id;
}
// user #14546: 1 = this character's sequence repeat is between its head request and its end (aggro.h: no walk-up starts mid-sequence)
static int MvSeqBusy(uint32_t chr) { const int i = MvRepFind(chr); return i >= 0 && g_rep_id[i] && g_rep_seq[i] && g_rep_phase[i] != 0; }
static void MvSeqEnd(int i, uint32_t chr, uint32_t act, DWORD now, const char* why) {
    Log("REPEAT chr %u sequence ended (%s, now 0x%X, last 0x%X, %lu ms after the start), gap %d ms", chr, why, act, g_rep_cur[i], now - g_rep_last[i], MvGapFor(chr));
    g_rep_phase[i] = 0; g_rep_end[i] = now ? now : 1; MvRoundReset(i);
}
// from = the boss's action when the head is asked for (#14712: while it still reads, the head has not started)
static void MvSeqRequest(int i, DWORD now, uint32_t from) { g_rep_phase[i] = 1; g_rep_last[i] = now ? now : 1; MvRoundReset(i); g_rep_kmax[i] = -1; g_rep_from[i] = from; }
// sequence mode, game thread (select hook + pump): follow the boss's real action. 1 = the sequence is running (phase 2 / 3).
static int MvSeqTrack(int i, uint32_t chr, uint32_t act, DWORD now) {
    const int ph = g_rep_phase[i];
    if (act == 0xFFFFFFFFu || act == 0xFFFF) return ph >= 2;  // unreadable / in transition: keep the state
    const int k = MvSeqIndex(i, act);
    if (ph == 1) {
        int started = 0;
        if (k == 0) {  // the head reads as the action: a request until it holds SEQ_CONFIRM_MS (the forced id can be replaced within a frame)
            if (!g_rep_seen[i]) g_rep_seen[i] = now ? now : 1;
            started = now - g_rep_seen[i] >= SEQ_CONFIRM_MS;
        } else if (act >= 0x4E20 && act != g_rep_from[i]) started = 1;  // another move: the head's branch / next piece (not the old action)
        else g_rep_seen[i] = 0;  // idle again, a reaction / step, or the action from before the request: the head was not taken yet
        if (!started) {
            if (now - g_rep_last[i] >= SEQ_START_MS) { MvSeqEnd(i, chr, act, now, "the head did not start"); }
            return 0;
        }
        g_rep_phase[i] = 2; g_rep_cur[i] = 0; if (g_rep_kmax[i] < 0) g_rep_kmax[i] = 0;  // the head played (or branched)
    }
    if (g_rep_phase[i] < 2) return 0;
    if (act == 2) { MvSeqEnd(i, chr, act, now, "back in idle by itself"); return 0; }
    if (now - g_rep_last[i] > 30000) { MvSeqEnd(i, chr, act, now, "still busy after 30 s"); return 0; }
    if (k > g_rep_kmax[i]) g_rep_kmax[i] = k;
    if (g_rep_cur[i] != act) {
        g_rep_cur[i] = act; g_rep_curt[i] = now ? now : 1;
        if (g_rep_count[i] <= 500) {
            if (k >= 0) Log("REPEAT chr %u sequence: part %d of %d 0x%X plays (%lu ms after the start)", chr, k + 1, g_rep_nchain[i], act, now - g_rep_last[i]);
            else Log("REPEAT chr %u sequence: 0x%X plays (the game's own next piece / reaction, %lu ms after the start)", chr, act, now - g_rep_last[i]);
        }
    }
    g_rep_phase[i] = k >= 0 ? 2 : 3;
    return 1;
}
static uint32_t MvIdleOr0(uint32_t chr, uint32_t id) { return id == 2 ? 0 : (N3MoveFind(chr, 2) >= 0 ? 2 : 0); }
// user #14462: aggro ON and the boss far (g_mv_chase): only locomotion passes (kind 'M', idle), an attack pick becomes idle 0x2, as the
// Stand-still walk-up does (idle_hold.h IhSelect, #14267). Before, every AI pick passed, so a repeat boss fought its way in (live 22:23).
static uint32_t MvChaseFilter(uint32_t chr, uint32_t id) {
    const int k = N3MoveFind(chr, id);
    if (id == 2 || (k >= 0 && kN3Moves[k].kind == 'M')) return 0;
    LONG n = InterlockedIncrement(&g_rep_gaps);
    if (n <= 500) Log("REPEAT chr %u aggro walk-up: AI pick 0x%X -> idle (locomotion only while it closes in)", chr, id);
    return MvIdleOr0(chr, id);
}
static uint32_t MvSeqPick(int i, uint64_t st, uint32_t chr, uint32_t id) {
    DWORD now = GetTickCount(); int gap = MvGapFor(chr);
    g_rep_st[i] = st;
    const uint32_t act = ActIdOf(st);
    // user #14702 / #14710: the chain plays: the game's own next piece passes (also when switched OFF), another attack waits (MV_KEEP);
    // no listed-part substitution and no idle after the last listed part (that cut 0x4E7A after ~400 ms)
    if (MvSeqTrack(i, chr, act, now)) return MvFollow(i, chr, act, id, now);
    if (g_rep_phase[i] == 1) return (uint32_t)g_rep_id[i];  // head requested, not started yet: keep asking for it
    if (MvChasing(st)) return MvChaseFilter(chr, id);  // user #13787 / #14462: aggro ON and the boss is far: it walks up, no attacks
    if (gap > 0 && g_rep_end[i] && now - g_rep_end[i] < (DWORD)gap) {
        LONG g = InterlockedIncrement(&g_rep_gaps);
        if (g <= 500) Log("REPEAT chr %u sequence gap hold pick 0x%X -> idle (%lu/%d ms since the sequence ended)", chr, id, now - g_rep_end[i], gap);
        return MvIdleOr0(chr, id);
    }
    LONG n = InterlockedIncrement(&g_rep_count[i]);
    if (n <= 500) Log("REPEAT chr %u sequence pick 0x%X -> head 0x%X (#%ld, %d parts, gap %d ms, %lu ms since prev end, action 0x%X)", chr, id, (uint32_t)g_rep_id[i], n,
                      g_rep_nchain[i], gap, g_rep_end[i] ? now - g_rep_end[i] : 0UL, act);
    MvSeqRequest(i, now, act);
    return (uint32_t)g_rep_id[i];
}
static uint32_t MvRepeatPick(uint64_t st, uint32_t id) {
    if (g_tr.active && !g_tr.practice) return 0;
    if (id < 0x4E20 && id != 2) return 0;  // idle picks too: a calm / out-of-range boss only re-picks idle (live 19:10, every 6 s)
    uint32_t chr = MvBossChr(st); int i = MvRepFind(chr); if (i < 0) return 0;
    uint32_t rep = (uint32_t)g_rep_id[i]; if (!rep || N3MoveFind(chr, id) < 0) return 0;
    if (g_rep_seq[i]) return MvSeqPick(i, st, chr, id);  // user #14423: chain repeat (also lets a started chain finish while chasing)
    DWORD now = GetTickCount(); int gap = MvGapFor(chr);
    const uint32_t act = ActIdOf(st);
    const int attack = act >= 0x4E20 && act != 0xFFFF && act != 0xFFFFFFFFu;
    // user #14710: the move plays (the drill or the pump forced it, or the boss picked it itself): followed like a sequence round
    if (act == rep && (!g_rep_last[i] || g_rep_end[i])) { g_rep_last[i] = now ? now : 1; g_rep_end[i] = 0; MvRoundReset(i); }
    if (g_rep_last[i] && !g_rep_end[i]) {
        if (act == rep && !g_rep_seen[i]) g_rep_seen[i] = now ? now : 1;
        if (attack && g_rep_cur[i] != act) { g_rep_cur[i] = act; g_rep_curt[i] = now ? now : 1; }
        // the move or its own next piece plays: the game's follow-up passes, another attack waits; the move ends back in idle by itself
        // (was: the first pick after the repeat = 'finished' -> gap idle, live 22:17:24 0x4EE8 > 0x4EE9 and 0x4E7B > 0x4EA7 > 0x4E7C cut short)
        if (g_rep_seen[i] && attack && now - g_rep_last[i] < 30000) return MvFollow(i, chr, act, id, now);
        if (!g_rep_seen[i] && (act == 2 || attack) && now - g_rep_last[i] < SEQ_START_MS) return rep;  // asked for, not playing yet: keep asking
        g_rep_end[i] = now ? now : 1; MvRoundReset(i);  // back in idle / a reaction / never started: the move has ended, the gap starts now
    }
    if (MvChasing(st)) return MvChaseFilter(chr, id);  // user #13787 / #14462: aggro ON and the boss is far: it walks up (no attacks), the repeat waits
    if (gap > 0 && g_rep_end[i] && now - g_rep_end[i] < (DWORD)gap) {
        LONG g = InterlockedIncrement(&g_rep_gaps);
        if (g <= 500) Log("REPEAT chr %u gap hold pick 0x%X -> idle (%lu/%d ms since end)", chr, id, now - g_rep_end[i], gap);
        g_rep_st[i] = st;  // MvRepTick ends the idle once the gap has passed (the native idle lasts ~6 s, live 19:35)
        return id == 2 ? 0 : (N3MoveFind(chr, 2) >= 0 ? 2 : 0);
    }
    LONG n = InterlockedIncrement(&g_rep_count[i]);
    if (n <= 500) Log("REPEAT chr %u pick 0x%X -> 0x%X (#%ld, gap %d ms, %lu ms since prev end, state %p)", chr, id, rep, n, gap,
                      g_rep_end[i] ? now - g_rep_end[i] : 0UL, (void*)st);
    g_rep_last[i] = now ? now : 1; g_rep_end[i] = 0; MvRoundReset(i);
    return rep;
}
// the repeated move, (sequence repeat, user #14423) any part of the repeated chain, or a follow-up piece MvFollow passed this round: never
// vetoed at commit even when switched OFF
static int MvRepAllows(uint32_t chr, uint32_t id) {
    int i = MvRepFind(chr); if (i < 0 || !g_rep_id[i]) return 0;
    return (uint32_t)g_rep_id[i] == id || (g_rep_seq[i] && MvSeqIndex(i, id) >= 0) || MvPassed(i, id);  // + the game's own next pieces (#14710)
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
    if (vchr && MvBlocked(vchr, id) && !MvRepAllows(vchr, id)) {
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
    if (memcmp((void*)(B + Grva(0x35EF04)), selp, 15) || memcmp((void*)(B + Grva(0x35BC68)), comp, 15)) {
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
    LONG vs = g_mv_view_slot; if (vs >= 0) { uint32_t c = Nioh3Training_SlotChr((int)vs); if (c) return c; }
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
static inline int MvRepStanding(uint32_t chr);  // drill.h / training.c: 1 = Stand still holds this boss (the drill plays the repeat)
// bridge 12953: on = 1 repeats (chr, id), on = 0 stops that boss's repeat. 1 = done, 0 = unknown move / table full.
static int MvRepArm(uint32_t chr, uint32_t id, int on, int seq);
N3T_API int Nioh3Training_SetMoveRepeat(uint32_t chr, uint32_t id, int on) { return MvRepArm(chr, id, on, 0); }
// user #14423: repeat the whole sequence that starts with id (Boss moves > Sequences). on = 0 stops that boss's repeat (move or sequence).
N3T_API int Nioh3Training_SetMoveRepeatSeq(uint32_t chr, uint32_t id, int on) { return MvRepArm(chr, id, on, 1); }
// 1 = this boss repeats a whole sequence, 0 = a single move / nothing
N3T_API int Nioh3Training_GetMoveRepeatSeq(uint32_t chr) { if (g_tr.active && !g_tr.practice) return 0; int i = MvRepFind(chr); return i >= 0 && g_rep_id[i] && g_rep_seq[i]; }
static int MvRepArm(uint32_t chr, uint32_t id, int on, int seq) {
    int i = MvRepFind(chr);
    if (!on) {
        if (i >= 0) { Log("REPEAT chr %u 0x%X%s stopped after %ld repeat(s)", chr, (uint32_t)g_rep_id[i], g_rep_seq[i] ? " sequence" : "", g_rep_count[i]); InterlockedExchange(&g_rep_id[i], 0); InterlockedExchange(&g_rep_seq[i], 0); InterlockedExchange(&g_rep_chr[i], 0); g_rep_st[i] = 0; g_rep_phase[i] = 0; }
        else Log("REPEAT chr %u stop: nothing was repeating", chr);
        return 1;
    }
    // user #13618: every refusal is logged with its reason (the live log had no trace of a failed or missing arm)
    if (N3MoveFind(chr, id) < 0) { Log("REPEAT chr %u 0x%X refused: not in this boss's recorded move list", chr, id); return 0; }
    if (N3MoveProtected(id)) { Log("REPEAT chr %u 0x%X refused: idle / initialization cannot be repeated", chr, id); return 0; }
    if (N3MoveUnsafe(chr, id)) { Log("REPEAT chr %u 0x%X refused: this move kills the boss (#14655)", chr, id); return 0; }
    if (seq) { uint32_t ch[MV_SEQ_MAX]; const int n = MvSeqBuild(chr, id, ch, MV_SEQ_MAX); for (int k = 0; k < n; ++k) if (N3MoveUnsafe(chr, ch[k])) { Log("REPEAT chr %u 0x%X refused: sequence part 0x%X kills the boss (#14655)", chr, id, ch[k]); return 0; } }
    if (i < 0) for (int k = 0; k < MV_REP_MAX; ++k) if (!g_rep_chr[k]) { i = k; break; }
    if (i < 0) { Log("REPEAT chr %u 0x%X refused: %d bosses already repeat a move (stop one first)", chr, id, MV_REP_MAX); return 0; }
    g_rep_nchain[i] = seq ? MvSeqBuild(chr, id, g_rep_chain[i], MV_SEQ_MAX) : 0;
    g_rep_phase[i] = 0; g_rep_st[i] = 0; g_rep_kmax[i] = -1; MvRoundReset(i);
    InterlockedExchange(&g_rep_seq[i], seq ? 1 : 0);
    InterlockedExchange(&g_rep_id[i], (LONG)id); InterlockedExchange(&g_rep_count[i], 0); g_rep_last[i] = 0; g_rep_end[i] = 0; InterlockedExchange(&g_rep_chr[i], (LONG)chr);
    InterlockedExchange(&g_mv_install_pending, 1);
    for (int k = 0; k < g_nsp; ++k) { uint64_t a = g_sp_actor[k], ai = 0, ctrl = 0; if (a && g_sp_chr[k] == chr && Rd64(a + 0x3B0, &ai) && ai) Rd64(ai + 8, &ctrl); if (a && g_sp_chr[k] == chr) Log("REPEAT chr %u actor %p ai %p ctrl %p", chr, (void*)a, (void*)ai, (void*)ctrl); }
    if (seq) {
        char t[96]; size_t o = 0; t[0] = 0;
        for (int k = 0; k < g_rep_nchain[i] && o < sizeof t; ++k) o += (size_t)snprintf(t + o, sizeof t - o, "%s0x%X", k ? " > " : "", g_rep_chain[i][k]);
        Log("REPEAT chr %u sequence %s ON (%d parts; the game's links play the rest, the gap starts when the last part ends)", chr, t, g_rep_nchain[i]);
    }
    Log("REPEAT chr %u 0x%X ON (gap %d ms, %s, move %s, hooks %ld)", chr, id, MvGapFor(chr),
        MvRepStanding(chr) ? (seq ? "Stand still: the head is forced after every gap" : "Stand still: drill forces it every gap") : "Fight normally: used at each AI pick",
        MvBlocked(chr, id) ? "OFF (repeat still allowed)" : "ON", (long)g_mv_hooked);
    return 1;
}
N3T_API uint32_t Nioh3Training_GetMoveRepeat(uint32_t chr) { return MvRepeatId(chr); }
// user #14462 (live 22:23:12): a boss summoned / respawned while its character still has a repeat armed inherited the old boss's state (same
// AI state address, sequence phase, last start), so it was handled as 'busy' and its own AI fought. Game thread (spawn watch): the repeat
// state starts over for the new boss and the first repeat waits one full gap (the boss stands still first). 1 = this character repeats.
static int MvRepFresh(uint32_t chr) {
    const int i = MvRepFind(chr); if (i < 0 || !g_rep_id[i]) return 0;
    const DWORD now = GetTickCount();
    g_rep_st[i] = 0; g_rep_phase[i] = 0; g_rep_kmax[i] = -1; MvRoundReset(i);
    g_rep_last[i] = now ? now : 1; g_rep_end[i] = now ? now : 1;
    Log("REPEAT chr %u 0x%X%s: new boss, repeat state reset, first repeat after the %d ms gap", chr, (uint32_t)g_rep_id[i], g_rep_seq[i] ? " sequence" : "", MvGapFor(chr));
    return 1;
}
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
struct MvGapSave { uint32_t chr; int ms; };
static DWORD WINAPI MvGapChrSaveThread(LPVOID p) {
    struct MvGapSave* g = (struct MvGapSave*)p; char ini[MAX_PATH], sec[40], v[16]; snprintf(ini, sizeof ini, "%s\\Nioh3Training.ini", g_dir);
    snprintf(sec, sizeof sec, "Moves.%u", g->chr); snprintf(v, sizeof v, "%d", g->ms); WritePrivateProfileStringA(sec, "RepeatGapMs", v, ini);
    WritePrivateProfileStringA(0, 0, 0, ini); HeapFree(GetProcessHeap(), 0, g); return 0;
}
// user #14309: per-boss gap (Boss X page). Clamped 0..10000 ms; persist = save [Moves.<chr>] RepeatGapMs. Returns the new value, -1 = table full / no boss.
N3T_API int Nioh3Training_GetRepeatGapChr(uint32_t chr) { return MvGapFor(chr); }
N3T_API int Nioh3Training_SetRepeatGapChr(uint32_t chr, int ms, int persist) {
    if (ms < 0) ms = 0; if (ms > MV_REP_GAP_MAX) ms = MV_REP_GAP_MAX;
    if (!MvGapPut(chr, ms)) return -1;
    Log("REPEAT chr %u gap %d ms%s", chr, ms, persist ? " (saved to ini)" : "");
    if (persist) {
        struct MvGapSave* g = (struct MvGapSave*)HeapAlloc(GetProcessHeap(), 0, sizeof *g);
        if (g) { g->chr = chr; g->ms = ms; HANDLE t = CreateThread(0, 0, MvGapChrSaveThread, g, 0, 0); if (t) CloseHandle(t); else HeapFree(GetProcessHeap(), 0, g); }
    }
    return ms;
}
// bridge 12958: game thread (pump). The native idle the boss gets during the gap runs ~6 s before the AI picks again (live 19:35: 1000 ms set,
// 6.0 s measured), so when the gap has passed and the boss is still in idle 0x2 the repeated move is started with SetActionID's forced
// transition 66C4A8(ctrl, id, 0, 0.0) (the KiBreakActFix path).
static __attribute__((noinline)) void MvRepTick(void) {
    if (g_tr.active && !g_tr.practice) return;
    DWORD now = GetTickCount();
    for (int i = 0; i < MV_REP_MAX; ++i) {
        uint32_t chr = (uint32_t)g_rep_chr[i], rep = (uint32_t)g_rep_id[i]; uint64_t st = g_rep_st[i];
        if (chr && rep && st && g_rep_seq[i]) {  // user #14423: sequence mode follows the real action every frame
            if (MvBossChr(st) != chr) { g_rep_st[i] = 0; g_rep_phase[i] = 0; continue; }
            const uint32_t cur = ActIdOf(st);
            if (MvSeqTrack(i, chr, cur, now) || cur != 2) continue;  // the chain advances, or not back in idle yet: never forced
            // user #14462: forced only from the gap (phase 0), never again while a request is pending (phase 1 waits SEQ_START_MS, then the gap)
            const DWORD since = g_rep_end[i] ? now - g_rep_end[i] : 0;
            if (g_rep_phase[i] != 0 || !g_rep_end[i] || since < (DWORD)MvGapFor(chr)) continue;
            if (MvChasing(st)) continue;
            MV_PREFORCE(st);
            int ok = 1;
            __try { MV_FORCE(st, rep); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
            LONG n = InterlockedIncrement(&g_rep_count[i]);
            if (n <= 500) Log("REPEAT chr %u sequence forced head 0x%X after %lu ms idle (#%ld, gap %d ms)%s, now 0x%X", chr, rep, since, n, MvGapFor(chr), ok ? "" : " FAULTED", ActIdOf(st));
            MvSeqRequest(i, now, 2);  // forced from idle
            continue;
        }
        if (!chr || !rep || !st || !g_rep_end[i] || now - g_rep_end[i] < (DWORD)MvGapFor(chr)) continue;
        if (MvBossChr(st) != chr) { g_rep_st[i] = 0; continue; }
        if (MvChasing(st)) continue;  // user #13787: no repeat forced from range while aggro ON brings the boss closer
        uint32_t cur = ActIdOf(st);
        if (cur != 2) { if (cur != 0xFFFFFFFFu) g_rep_st[i] = 0; continue; }
        int ok = 1;
        __try { MV_FORCE(st, rep); } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
        LONG n = InterlockedIncrement(&g_rep_count[i]);
        if (n <= 500) Log("REPEAT chr %u forced 0x%X after %lu ms idle (#%ld, gap %d ms)%s, now 0x%X", chr, rep, now - g_rep_end[i], n, MvGapFor(chr), ok ? "" : " FAULTED", ActIdOf(st));
        g_rep_last[i] = now ? now : 1; g_rep_end[i] = 0; g_rep_st[i] = 0; MvRoundReset(i);
    }
}
static void MvLoadPrefs(const char* ini) {
    { int gap = (int)GetPrivateProfileIntA("Training", "RepeatGapMs", 1000, ini); g_cfg_rep_gap = gap < 0 ? 0 : gap > MV_REP_GAP_MAX ? MV_REP_GAP_MAX : gap; }
    for (int i = 0; i < N3_MOVE_COUNT; ++i) {
        char section[40], key[24];
        if (i == 0 || kN3Moves[i].chr != kN3Moves[i - 1].chr) {  // user #14309: per-boss gap, once per character
            snprintf(section, sizeof section, "Moves.%u", kN3Moves[i].chr);
            const int gap = (int)GetPrivateProfileIntA(section, "RepeatGapMs", -1, ini);
            if (gap >= 0) MvGapPut(kN3Moves[i].chr, gap > MV_REP_GAP_MAX ? MV_REP_GAP_MAX : gap);
        }
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
