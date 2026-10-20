// user #13529: move drill in the 'Stand still (idle)' stance (fighting-game training dummy: the boss waits, then does the picked move
// every RepeatGapMs). Included by training.c after move_toggle.h and idle_hold.h (or by the offline harness with stubs).
// Why: Stand still = Idle hold phase 2 = the boss AI is OFF (668D74), so the AI never picks a move and the Repeat feature (MvRepeatPick in
// the select hook 35EF04 + MvRepTick, which waits for a pick) never fired: 'repeat' only worked in the CPU stance.
// Now, while Idle hold owns the AI and a repeat is set for that boss, this tick starts the repeated move itself with the forced transition
// 66C4A8(ctrl, id, 0, 0.0) (the MvRepTick / KiBreakActFix path) whenever the boss is back in idle 0x2 and the gap has passed since the
// move ended. The AI stays off, so between repeats the boss stands still. Natives used (supplied by the includer): IhReadAction,
// IhNativeAi, IhEligible, IhAlive, IhSameActor, DrForceNative(ctrl, id).
// Fallback (not live-proven that a forced action plays with the AI off): if the forced move never starts 3 times in a row, the AI is
// switched back on for the drill only (g_ih_drill_ai: Idle hold stops re-disabling it); the repeat picks then make the AI use only the
// picked move, with the idle 0x2 in the gaps (MvRepeatPick), as in the CPU stance. Stopping the repeat or the stance ends the drill and
// Idle hold takes the AI off again on its next 100 ms check.
static struct { uint32_t chr, id, last, before; uint64_t ctrl; int phase, fails, seen; DWORD tEnd, tForce, tLast; LONG n; } g_dr;
static int g_dr_off;  // set after a tick exception: no more drill this game session
static volatile LONG g_dr_state;  // 0 no drill, 1 drill with the AI off (forced moves), 2 drill with the AI on (fallback)
// user #14147: 1 while 'Boss aggro on me' (aggro.h AgDrillTick) walks the boss up to the player: the drill forces nothing and restarts
// its gap when the walk-up ends. DR_PREFORCE(actor) runs right before each forced move (aggro.h: turn the boss to face the player).
static volatile LONG g_dr_hold;
#ifndef DR_PREFORCE
#define DR_PREFORCE(a) ((void)0)
#endif
static void DrStop(const char* why) {
    if (g_dr.chr) Log("DRILL stop (%s): chr %u move 0x%X, %ld forced, mode %ld", why, g_dr.chr, g_dr.id, g_dr.n, g_dr_state);
    memset(&g_dr, 0, sizeof g_dr);
    InterlockedExchange(&g_ih_drill_ai, 0);
    InterlockedExchange(&g_dr_state, 0);
}
// user #14546: 1 = this character repeats a whole sequence: move_toggle.h MvRepTick forces its head and follows the chain (AI off), the drill
// stays out of it (it would force the head again mid-chain). aggro.h walks such a boss up with the same rules as the drill.
#ifndef DR_SEQ
#define DR_SEQ(chr) 0
#endif
#ifndef DR_GAP
#define DR_GAP(chr) g_cfg_rep_gap
#endif
// user #13618 (move_toggle.h REPEAT log): 1 = the Stand still stance holds this boss, so the drill (not the AI) plays its repeat
static inline int MvRepStanding(uint32_t chr) { return chr && g_ih.phase == 2 && g_ih.chr == chr; }
// 0 no drill, 1 drill (boss stands still, does the move every gap), 2 drill with the boss AI on (fallback)
N3T_API int Nioh3Training_GetDrill(void) { return (int)InterlockedCompareExchange(&g_dr_state, 0, 0); }
static void DrillTick(void) {
    if (g_dr_off) return;
    const uint32_t chr = g_ih.phase == 2 ? g_ih.chr : 0, seq = chr && DR_SEQ(chr), id = chr && !seq ? MvRepeatId(chr) : 0;
    if (!chr || !id || !IhSameActor() || !IhAlive(g_ih.actor)) { if (g_dr.chr) DrStop(!chr ? "stance is not Stand still" : seq ? "sequence repeat (forced by the repeat tick)" : !id ? "repeat stopped" : "boss gone"); return; }
    const DWORD now = GetTickCount();
    const int gap = DR_GAP(chr);  // user #14309: per-boss gap (training.c: MvGapFor)
    if (g_dr.chr != chr || g_dr.id != id || g_dr.ctrl != g_ih.ctrl) {
        if (g_dr.chr) DrStop("move / boss changed");
        g_dr.chr = chr; g_dr.id = id; g_dr.ctrl = g_ih.ctrl;
        g_dr.tEnd = now - (DWORD)(gap > 500 ? gap - 500 : 0);  // first repeat ~0.5 s after the pick
        InterlockedExchange(&g_dr_state, 1);
        Log("DRILL chr %u move 0x%X: the boss stands still and does this move every %d ms (AI stays off)", chr, id, gap);
    }
    if (InterlockedCompareExchange(&g_dr_hold, 0, 0)) {  // aggro walk-up: no forced move; first repeat ~0.5 s after the boss arrives
        if (g_dr.phase == 0) g_dr.tEnd = now - (DWORD)(gap > 500 ? gap - 500 : 0);
        return;
    }
    if (g_dr_state == 2) return;  // fallback: the AI and the repeat picks own the drill
    const uint32_t cur = IhReadAction(g_dr.ctrl);
    if (cur != g_dr.last) { g_dr.last = cur; g_dr.tLast = now; }
    if (g_dr.phase == 0) {
        // idle 0x2, or a stable non-attack state for 3 s (some bosses rest in another wait action): start the move once the gap has passed
        const int ready = cur == 2 || (cur < 0x4E20 && cur != 0xFFFFFFFFu && now - g_dr.tLast >= 3000);
        if (!ready || now - g_dr.tEnd < (DWORD)gap || !IhEligible(g_ih.actor, g_dr.ctrl)) return;
        DR_PREFORCE(g_ih.actor);
        DrForceNative(g_dr.ctrl, id);
        g_dr.phase = 1; g_dr.tForce = now; g_dr.before = cur; g_dr.seen = IhReadAction(g_dr.ctrl) != cur;
        LONG n = ++g_dr.n;
        if (n <= 300) Log("DRILL chr %u forced 0x%X (#%ld, %lu ms after the last one ended, gap %d ms), action now 0x%X", chr, id, n, now - g_dr.tEnd, gap, IhReadAction(g_dr.ctrl));
        return;
    }
    if (!g_dr.seen) {
        if (cur != g_dr.before && cur != 0xFFFFFFFFu) { g_dr.seen = 1; return; }  // left the rest state: the move (or its first part) runs
        if (now - g_dr.tForce < 500) return;
        // the forced move never started
        g_dr.phase = 0; g_dr.tEnd = now; ++g_dr.fails;
        Log("DRILL chr %u: forced 0x%X did not start (action still 0x%X), try %d of 3", chr, id, cur, g_dr.fails);
        if (g_dr.fails >= 3) {
            InterlockedExchange(&g_ih_drill_ai, 1);
            InterlockedExchange(&g_dr_state, 2);
            if (BhAiOn(g_ih.actor) == 0) IhNativeAi(g_ih.actor, 1);
            Log("DRILL chr %u: a forced move does not play with the AI off: AI on for the drill only (the boss uses just 0x%X, idle in the gaps)", chr, id);
        }
        return;
    }
    if (cur == 2 || now - g_dr.tForce > 20000 || (cur < 0x4E20 && cur != 0xFFFFFFFFu && now - g_dr.tLast >= 3000)) {  // back at rest: the move ended
        g_dr.phase = 0; g_dr.tEnd = now; g_dr.fails = 0;
    }
}
