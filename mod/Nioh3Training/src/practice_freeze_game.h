// user #22589 / #22592: Practice FREEZE + FRAME-ADVANCE, game side. Included by training.c after frame_meter_game.h. Decisions: practice_freeze.h
// (tests/practice_freeze_test.c has the engine simulation). Menu: Boss Rematch practice_stats_menu.h (Practice hub rows 'Freeze fight' / 'Step 1 frame').
//   Hold   : the existing F9 freeze mechanism (#9595 / #9658): +1 on the engine stop request counter #3 (G+0x59C), own flag g_fz_hold (CAS, like the
//            menu pause), so it never releases the F9 debug freeze, the menu pause or the game's own requests.
//   Step   : the call to the stop latch at 5AD64A (FrameDtSetup) is chained (E8 rel32 -> near stub -> h_fzlatch, installed on the game thread the
//            first time the freeze is used). During an open step G+0x58B is cleared right after the latch, so the whole frame runs at the normal
//            game time scale; afterwards the flag is put back to the counter's truth (never 1 with the counter at 0).
//   Clock  : the frame meter's game clock g_fm_clock when the meter runs (so each step = exactly +1 meter cell), else the same game-time sum here.
//   Pump   : FzTick (h_pump, after FpTick): requests, safety release, step accounting, log lines.
//   Keys   : own thread FzKeyThread (game window in front, Practice live): [Practice] FreezeKey (default 120 = F9, the same key as the debug
//            freeze: while Practice is live F9 drives this freeze), StepKey (default 117 = F6; hold = repeat after 350 ms, every 80 ms).
//            The same thread is the watchdog: the hold is dropped if the game-thread pump has not ticked for 2 s.
static FzState g_fz;
static volatile LONG g_fz_hold, g_fz_over, g_fz_frames, g_fz_steps, g_fz_rq_op, g_fz_rq_steps, g_fz_off, g_fz_lines;
static int g_fz_vk_freeze = 0x78, g_fz_vk_step = 0x75;
static int g_fz_latch_hooked;  // 0 not tried, 1 on, 2 failed
typedef void (*FzLatchFn)(uint64_t);
static FzLatchFn o_fzlatch;
static uint64_t g_fz_boss[N3T_SLOTS], g_fz_player; static DWORD g_fz_t0; static double g_fz_own; static uint32_t g_fz_610; static int g_fz_610ok, g_fz_src = -1;
static void FzLog(const char* fmt, ...) { if (InterlockedIncrement(&g_fz_lines) > 4000) return; char m[600]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap); Log("%s", m); }
static const char* FzKeyName(int vk, char* b, int n) { if (vk >= 0x70 && vk <= 0x87) snprintf(b, n, "F%d", vk - 0x6F); else if (vk) snprintf(b, n, "key 0x%X", vk); else snprintf(b, n, "none"); return b; }
static void FzLoadIni(const char* ini) {
    g_fz_vk_freeze = (int)GetPrivateProfileIntA("Practice", "FreezeKey", 0x78, ini); g_fz_vk_step = (int)GetPrivateProfileIntA("Practice", "StepKey", 0x75, ini);
    char a[16], b[16]; Log("PRACTICE FREEZE (#22589): [Practice] FreezeKey %s, StepKey %s (freeze + frame advance; default OFF, never saved)", FzKeyName(g_fz_vk_freeze, a, sizeof a), FzKeyName(g_fz_vk_step, b, sizeof b));
}
// Our stop request, thread-safe and idempotent (CAS on g_fz_hold): only the holder's own +1 is ever taken back.
static __attribute__((noinline)) int FzHoldSet(int on, const char* why) {
    volatile LONG* c = DbgStopCounter(); if (!c) { if (on) FzLog("PRACTICE FREEZE: no frame manager, not frozen"); return 0; }
    __try {
        if (on) { if (InterlockedCompareExchange(&g_fz_hold, 1, 0) != 0) return 1; g_fz_t0 = GetTickCount(); InterlockedIncrement(c); }
        else { if (InterlockedCompareExchange(&g_fz_hold, 0, 1) != 1) return 1; LONG v; do { v = *c; if (v <= 0) break; } while (InterlockedCompareExchange(c, v - 1, v) != v); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { Log("PRACTICE FREEZE: stop counter write faulted"); InterlockedExchange(&g_fz_hold, 0); return 0; }
    FzLog("PRACTICE FREEZE %s (%s; stop counter #3 now %ld, time scale %.3f)", on ? "ON" : "OFF", why, *c, DbgTimeScale());
    return 1;
}
// Chained latch 89B6EC(G+0x588) at its FrameDtSetup call (game thread, frame start, before the time scale is computed).
static void h_fzlatch(uint64_t flags) {
    o_fzlatch(flags);
    if (!g_fz_hold && !g_fz_over) return;
    __try {
        volatile uint8_t* f = (volatile uint8_t*)flags; volatile LONG* c = (volatile LONG*)(flags + 8 + 4 * 3);
        const int run = g_fz_hold && !g_fz_off ? FzLatch(&g_fz, f[4] != 0) : 0;
        if (run || g_fz_over) f[3] = (uint8_t)FzFlag(*c != 0, run);
        InterlockedExchange(&g_fz_over, run);
    } __except (EXCEPTION_EXECUTE_HANDLER) { InterlockedExchange(&g_fz_off, 1); InterlockedExchange(&g_fz_over, 0); }
}
static int FzLatchInstall(void) {
    if (g_fz_latch_hooked) return g_fz_latch_hooked == 1;
    g_fz_latch_hooked = 2;
    uint8_t* site = (uint8_t*)(B + Grva(0x5AD64A));
    static const uint8_t pre[7] = {0x48, 0x8D, 0x8F, 0x88, 0x05, 0x00, 0x00};  // lea rcx,[rdi+588h] at 5AD643
    static const uint8_t lat[10] = {0x48, 0x8D, 0x51, 0x08, 0x41, 0xB8, 0x06, 0x00, 0x00, 0x00};  // 89B6EC: lea rdx,[rcx+8] / mov r8d,6
    if (memcmp(site - 7, pre, 7) || site[0] != 0xE8) { Log("PRACTICE FREEZE: latch call 5AD64A bytes differ, frame step OFF (freeze still works)"); return 0; }
    const uintptr_t prev = (uintptr_t)(site + 5) + *(int32_t*)(site + 1);
    if (prev == B + Grva(0x89B6EC) && memcmp((void*)prev, lat, 10)) { Log("PRACTICE FREEZE: latch 89B6EC bytes differ, frame step OFF"); return 0; }
    uint8_t* stub = 0;
    for (uintptr_t a = ((uintptr_t)site - 0x70000000) & ~0xFFFFull; a < (uintptr_t)site + 0x70000000 && !stub; a += 0x10000)
        stub = (uint8_t*)VirtualAlloc((void*)a, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { Log("PRACTICE FREEZE: no near stub, frame step OFF"); return 0; }
    stub[0] = 0x48; stub[1] = 0xB8; *(uint64_t*)(stub + 2) = (uint64_t)h_fzlatch; stub[10] = 0xFF; stub[11] = 0xE0;
    o_fzlatch = (FzLatchFn)prev;
    const int32_t rel = (int32_t)((intptr_t)stub - (intptr_t)(site + 5));
    DWORD old; VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old);
    InterlockedExchange((volatile LONG*)(site + 1), rel);
    VirtualProtect(site, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), site, 5);
    g_fz_latch_hooked = 1;
    Log("PRACTICE FREEZE: latch call 5AD64A chained (previous target rva %llX), frame step ready", (unsigned long long)(prev - B));
    return 1;
}
// Game time this tick (ms), the frame meter's formula (FpGameStep).
static __attribute__((noinline)) double FzGameDt(float* tsOut, int* fpsOut) {
    uint64_t g = 0; double d = 0; *tsOut = -1; *fpsOut = 0;
    if (!Rd64(B + Grva(0x4759910), &g) || !g) return 0;
    __try {
        const uint32_t n = *(volatile uint32_t*)(g + 0x610); const int fps = *(volatile int32_t*)(g + 0x614); const float ts = *(volatile float*)(g + 0x624);
        uint32_t k = g_fz_610ok ? n - g_fz_610 : 1; g_fz_610 = n; g_fz_610ok = 1; if (k > 8) k = 8;
        *tsOut = ts; *fpsOut = fps;
        if (fps > 0 && fps <= 1000 && ts >= 0.0f && ts < 16.0f) d = (double)k * 1000.0 * (double)ts / (double)fps;
    } __except (EXCEPTION_EXECUTE_HANDLER) { d = 0; }
    return d;
}
static __attribute__((noinline)) int FzBossDead(uint64_t a) { uint64_t st = 0; int dead = 0; if (!Rd64(a + 0x3A0, &st) || !st) return 0; __try { dead = *(volatile int32_t*)(st + 0x48) == 1; } __except (EXCEPTION_EXECUTE_HANDLER) { } return dead; }
static void FzTick(void) {
    float ts; int fps; const double d = FzGameDt(&ts, &fps);
    g_fz_own += d;
    if (g_fz_hold && !g_fz.held) FzHoldSet(0, "state mismatch");  // never keep a hold the state machine does not know
    if (g_fz.held && !g_fz_hold) { g_fz.held = 0; g_fz.want = 0; g_fz.open = 0; g_fz.queued = 0; }  // released elsewhere (watchdog / Return)
    const LONG op = InterlockedExchange(&g_fz_rq_op, 0), nst = InterlockedExchange(&g_fz_rq_steps, 0);
    if (op) FzRequest(&g_fz, op == 3 ? -1 : op == 1 ? 1 : 0);
    for (LONG i = 0; i < nst; ++i) FzRequestStep(&g_fz);
    if (!g_fz.want && !g_fz.held && !g_fz_over) { InterlockedExchange(&g_fz_frames, 0); return; }
    if (g_fz.want && !g_fz.held) FzLatchInstall();  // frame step; the freeze alone works without it
    // safety
    int live = g_tr.active != 0, bossSame = 1, bossDead = 0;
    for (int s = 0; s < N3T_SLOTS; ++s) { const uint64_t a = BvActor(s); if (a) live = 1; if (g_fz.held && g_fz_boss[s]) { if (a != g_fz_boss[s]) bossSame = 0; else if (FzBossDead(a)) bossDead = 1; } }
    const uint64_t me = PlayerChar();
    const int reason = g_fz_off ? FZR_BUSY : FzSafeReason(live, MenuPauseBusy(), bossSame, bossDead, !g_fz.held || me == g_fz_player, g_fz.held ? GetTickCount() - g_fz_t0 : 0);
    const int src = g_fp_hooked == 1 && (g_pst.v[PS_FRAME_METER] || g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) && g_pst_live;  // 1 = the meter's clock
    const double clk = src ? g_fm_clock : g_fz_own;
    if (g_fz.held && src != g_fz_src) { const int64_t c = FzCell(clk); g_fz.cell0 += c - g_fz.cell; g_fz.target += c - g_fz.cell; g_fz.cell = c; }
    g_fz_src = src;
    const int wasOpen = g_fz.open, q = g_fz.queued; int done = 0;
    const int act = FzPump(&g_fz, reason, clk, d, &done);
    if (act == FZ_HOLD) {
        for (int s = 0; s < N3T_SLOTS; ++s) g_fz_boss[s] = BvActor(s);
        g_fz_player = me;
        if (!FzHoldSet(1, "Practice freeze")) { g_fz.held = 0; g_fz.want = 0; }
        else FzLog("PRACTICE FREEZE: frame meter cell %lld (%s clock %.1f ms), bosses A %p B %p, frame step %s", (long long)g_fz.cell0, src ? "meter" : "own", clk, (void*)g_fz_boss[0], N3T_SLOTS > 1 ? (void*)g_fz_boss[1] : 0, g_fz_latch_hooked == 1 ? "ready" : "OFF");
    } else if (act == FZ_RELEASE) {
        FzHoldSet(0, reason != FZR_OK ? FzReasonText(reason) : "Practice unfreeze");
        if (reason != FZR_OK) FzLog("PRACTICE FREEZE auto-released: %s (fail closed)", FzReasonText(reason));
        InterlockedExchange(&g_fz_steps, 0);
    }
    if (done) FzLog("PRACTICE STEP #%d: +%d frame(s), now +%d since the freeze (meter cell %lld, %d engine tick(s), clock %.2f ms, time scale %.3f, fps-for-dt %d)",
        g_fz.steps, g_fz.lastAdv, FzFrames(&g_fz), (long long)g_fz.cell, g_fz.ticks, clk, ts, fps);
    if (wasOpen && !g_fz.open && !done && g_fz.held) FzLog("PRACTICE STEP aborted: no game time for %d ticks (another pause holds time; queued %d)", FZ_STALL_TICKS, q);
    InterlockedExchange(&g_fz_frames, FzFrames(&g_fz)); InterlockedExchange(&g_fz_steps, g_fz.steps);
}
static void FzFail(void) { InterlockedExchange(&g_fz_off, 1); FzHoldSet(0, "exception"); g_fz.held = g_fz.want = g_fz.open = g_fz.queued = 0; Log("PRACTICE FREEZE tick exception: released, freeze OFF until restart"); }
static DWORD WINAPI FzKeyThread(LPVOID p) {
    (void)p; int fWas = 1, sWas = 1; DWORD sDown = 0, sLast = 0; LONG lastTicks = -1; DWORD tickSeen = GetTickCount();
    for (;;) {
        Sleep(15);
        const DWORD now = GetTickCount();
        if (g_ticks != lastTicks) { lastTicks = g_ticks; tickSeen = now; }
        else if (g_fz_hold && now - tickSeen > 2000) { FzHoldSet(0, "watchdog: the game-thread pump stopped for 2 s"); FzLog("PRACTICE FREEZE auto-released: pump stopped (fail closed)"); }
        HWND w = GetForegroundWindow(); DWORD pid = 0; if (w) GetWindowThreadProcessId(w, &pid);
        const int ok = pid == GetCurrentProcessId() && g_pst_live && !g_fz_off;
        const int f = ok && g_fz_vk_freeze && (GetAsyncKeyState(g_fz_vk_freeze) & 0x8000), s = ok && g_fz_vk_step && (GetAsyncKeyState(g_fz_vk_step) & 0x8000);
        if (f && !fWas) { InterlockedExchange(&g_fz_rq_op, 3); FzLog("PRACTICE FREEZE key: toggle (now %s)", g_fz_hold ? "frozen -> run" : "running -> freeze"); }
        if (s && !sWas) { InterlockedIncrement(&g_fz_rq_steps); sDown = sLast = now; }
        else if (s && now - sDown >= 350 && now - sLast >= 80) { if (!g_fz.queued && !g_fz.open && !g_fz_rq_steps) InterlockedIncrement(&g_fz_rq_steps); sLast = now; }  // hold = repeat
        fWas = f; sWas = s;
    }
    return 0;
}
// C API for Boss Rematch's Practice hub. op: 0 off, 1 on, -1 toggle, 2 step 1 frame, 3 query. Returns -2 (no Practice session / Training boss,
// nothing done) or the state: bit 0 frozen (held), bit 1 requested, bit 2 frame step available, bits 8.. frames advanced since the freeze.
N3T_API int Nioh3Training_PracticeFreeze(int op) {
    if (op != 3 && !g_pst_live) return -2;
    if (op == 0 || op == 1 || op == -1) InterlockedExchange(&g_fz_rq_op, op == 1 ? 1 : op == 0 ? 2 : 3);
    else if (op == 2) InterlockedIncrement(&g_fz_rq_steps);
    if (op != 3) Log("PRACTICE FREEZE menu op %d", op);
    const int want = op == 1 || (op == -1 ? !g_fz.want : op == 2 ? 1 : g_fz.want);
    return (g_fz_hold ? 1 : 0) | ((op == 0 ? 0 : want) ? 2 : 0) | (g_fz_latch_hooked != 2 ? 4 : 0) | ((int)g_fz_frames << 8);
}
N3T_API int Nioh3Training_PracticeFreezeKeys(void) { return (g_fz_vk_freeze & 0xFF) | ((g_fz_vk_step & 0xFF) << 8); }
