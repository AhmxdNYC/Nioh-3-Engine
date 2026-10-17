// Boss-only idle pause. Native reads/calls are supplied by training.c (or the offline harness).
// Never writes action state, position, player input, animation time or the debug-freeze state.
static volatile LONG g_ih_epoch, g_ih_request, g_ih_status;
static LONG g_ih_serial;
static uint32_t g_ih_seen_chr;
static int g_ih_seen_round, g_ih_seen_active;
static DWORD g_ih_seen_start;
static struct {
    int phase, owns_ai;
    LONG epoch;
    uint32_t handle, chr;
    uint64_t actor, ctrl;
    DWORD started, recheck;
} g_ih;

static int IhSameActor(void) {
    uint64_t ai = 0, ctrl = 0;
    return g_ih.actor && g_ih.actor != PlayerChar() && ActorByHandle(g_ih.handle) == g_ih.actor &&
        Rd64(g_ih.actor + 0x3B0, &ai) && ai && Rd64(ai + 8, &ctrl) && ctrl == g_ih.ctrl;
}
static int IhForeignHold(void) {
    return g_bh.phase && g_bh.h == g_ih.handle;
}
static void IhStop(const char* why, int failed) {
    // Restore only an AI flag this feature changed, and never release the loading hold.
    if (g_ih.owns_ai && IhSameActor() && IhAlive(g_ih.actor) && !IhForeignHold() && BhAiOn(g_ih.actor) == 0)
        IhNativeAi(g_ih.actor, 1);
    if (g_ih.phase) Log("IDLEHOLD stop %s: phase %d actor %p owned %d", why, g_ih.phase, (void*)g_ih.actor, g_ih.owns_ai);
    memset(&g_ih, 0, sizeof g_ih);
    InterlockedExchange(&g_ih_request, 0);
    InterlockedExchange(&g_ih_status, failed ? -1 : 0);
}
N3T_API int Nioh3Training_GetIdleHold(void) {
    // 0 OFF, 1 awaiting safe idle, 2 AI held, -1 acquisition/validation failed.
    LONG status = InterlockedCompareExchange(&g_ih_status, 0, 0);
    if (!status && InterlockedCompareExchange(&g_ih_request, 0, 0)) return 1;
    return (int)status;
}
N3T_API int Nioh3Training_SetIdleHold(int on) {
    LONG epoch = InterlockedCompareExchange(&g_ih_epoch, 0, 0);
    if (on && !epoch) return 0;
    InterlockedExchange(&g_ih_request, on ? epoch : 0);
    // Game-thread tick owns both native calls and the observed status.
    return 1;
}
static void IhPublishSession(void) {
    if (g_tr.active != g_ih_seen_active || g_tr.chr != g_ih_seen_chr ||
        g_tr.round != g_ih_seen_round || g_tr.tCmd != g_ih_seen_start) {
        g_ih_seen_active = g_tr.active; g_ih_seen_chr = g_tr.chr;
        g_ih_seen_round = g_tr.round; g_ih_seen_start = g_tr.tCmd;
        if (g_ih_serial == 0x7FFFFFFF) g_ih_serial = 1; else ++g_ih_serial;
        InterlockedExchange(&g_ih_epoch, g_tr.active ? g_ih_serial : 0);
    }
}
static uint32_t IhSelect(uint64_t st, uint32_t id) {
    // Only substitute an ordinary, catalogued move at the existing native transition.
    // Unknown/status/reaction IDs pass through. Never force an action mid-animation.
    if (!g_ih.phase || !g_tr.active || !g_bm.active || g_bm.id != g_ih.handle ||
        g_tr.chr != g_ih.chr || g_tr.round != g_ih_seen_round || g_tr.tCmd != g_ih_seen_start ||
        st != g_ih.ctrl || id < 0x4E20 ||
        N3MoveFind(g_ih.chr, id) < 0 ||
        InterlockedCompareExchange(&g_ih_request, 0, 0) != g_ih.epoch ||
        InterlockedCompareExchange(&g_ih_epoch, 0, 0) != g_ih.epoch ||
        !IhSameActor() || IhForeignHold() || !IhEligible(g_ih.actor, g_ih.ctrl)) return id;
    Log("IDLEHOLD transition chr %u 0x%X -> idle 0x2", g_ih.chr, id);
    return 2;
}
static void IhTick(void) {
    IhPublishSession();
    LONG request = InterlockedCompareExchange(&g_ih_request, 0, 0);
    LONG epoch = InterlockedCompareExchange(&g_ih_epoch, 0, 0);
    if (!request) { if (g_ih.phase) IhStop("OFF", 0); return; }
    if (!epoch || request != epoch || !g_tr.active) { IhStop("session changed", 0); return; }
    DWORD now = GetTickCount();
    if (!g_ih.phase) {
        if (N3MoveFind(g_tr.chr, 2) < 0) { IhStop("no recorded idle for this boss", 1); return; }
        uint64_t actor = g_bm.active ? ActorByHandle(g_bm.id) : 0, ai = 0, ctrl = 0;
        if (!actor || actor == PlayerChar() || !Rd64(actor + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) {
            IhStop("no session boss", 1); return;
        }
        g_ih.phase = 1; g_ih.epoch = epoch; g_ih.handle = g_bm.id; g_ih.chr = g_tr.chr;
        g_ih.actor = actor; g_ih.ctrl = ctrl; g_ih.started = now;
        MvInstall();
        if (g_mv_hooked != 1) { IhStop("move hook unavailable", 1); return; }
        InterlockedExchange(&g_ih_status, 1);
        Log("IDLEHOLD pending chr %u actor %p (10 s timeout)", g_ih.chr, (void*)actor);
    }
    if (epoch != g_ih.epoch || !g_bm.active || g_bm.id != g_ih.handle || !IhSameActor()) {
        IhStop("boss gone/replaced", 0); return;
    }
    if (IhForeignHold()) {
        // Grace/cover owns this actor now. Do not retain a claim that could re-enable it later.
        IhStop("loading hold owns AI", 0); return;
    }
    if (!IhAlive(g_ih.actor)) { IhStop("boss defeated", 0); return; }
    if (g_ih.phase == 1) {
        if (now - g_ih.started >= 10000) { IhStop("no safe idle within 10 s", 1); return; }
        if (!IhEligible(g_ih.actor, g_ih.ctrl) || IhReadAction(g_ih.ctrl) != 2) return;
        // An already-disabled AI belongs to somebody else. Never adopt or re-enable it.
        if (BhAiOn(g_ih.actor) != 1) return;
        g_ih.owns_ai = 1; // record ownership before the native call, so exception cleanup restores
        IhNativeAi(g_ih.actor, 0);
        if (BhAiOn(g_ih.actor) != 0) { IhStop("AI disable did not take", 1); return; }
        g_ih.phase = 2; g_ih.recheck = now;
        InterlockedExchange(&g_ih_status, 2);
        Log("IDLEHOLD held chr %u actor %p at action 0x2; player unchanged", g_ih.chr, (void*)g_ih.actor);
    } else if (now - g_ih.recheck >= 100) {
        g_ih.recheck = now;
        int ai = BhAiOn(g_ih.actor);
        if (ai < 0) { IhStop("AI state unreadable", 1); return; }
        if (ai == 1) IhNativeAi(g_ih.actor, 0);
        // Hits may play native reactions: this is an AI pause, not an animation freeze.
    }
}
