// user #22356 / #22310: FRAMEPROBE, a log-only probe for boss frame data (startup / active / recovery). Included by training.c after
// practice_stats_game.h. No game memory is written (the only patch is the hook itself). Default OFF: Nioh3Training.ini [Debug] FrameProbe = 1 turns it on
// (read at start; the hook is installed only then, so OFF costs nothing).
// Hook: the hit-record setup 2B4A3C(rcx = hit record H, rdx = attacker AI object (= actor+0x3B0), r8d -> H+0x16C, r9 = hitbox row R -> H+0)
// (notes/yoritomo-hit-source-22305.md). Its entry is `test rcx,rcx / je rel32`, so the detour sits right after the null check at 2B4A45
// (mov rax,rsp | mov [rax+10h],rbx | mov [rax+20h],r9 | push rbp: 12 bytes, no rip-relative operand; rax is dead there).
// Callers: 2B30A0 (attack collision setup, rbx = the attack collision object, H = [obj+0xB0]; reached from the action attack command 5CA5xx),
// 1179220 and 23F2760. The collision object lives in the pool [45C6F38]+0x20+id*8 (0xFA0 entries, null when free, 2B88BC); its release = the window end.
// Lines (rate limited, 40 per second, 30000 total):
//   FRAMEPROBE start  chr .. act ..                                 action start seen (pump frame or the hook, whichever is first)
//   FRAMEPROBE hit    chr .. act .. #k at T ms ..                   one hit-record creation = one hit window opening
//   FRAMEPROBE hitend chr .. act .. #k window W ms ..               the attack collision object left its pool slot
//   FRAMEPROBE run    chr .. act .. ran .. startup .. active .. recovery ..   at the next action id (summary)
// Clock (user #22575, notes/frame-fps-independence-22575.md): every "ms" is GAME time, the sum of the game's own step per update tick
// (G = [4759910]: ticks G+0x610 x 1000 * time scale G+0x624 / fps-for-dt G+0x614, notes/frame-timing.md), so 1 f60 = 1/60 s of game time at any
// frame rate, slow-down (stock engine below its fps cap) and pause included. The real-time (QPC) value of each edge is printed next to it as "wall".
#include "frame_probe.h"
static int g_cfg_fp;
#define FP_PL N3T_SLOTS  // user #22796: the extra g_fp entry that follows the PLAYER (same bookkeeping as a boss slot, only the player frame meter reads it)
#define FPB(s) ((s) == FP_PL ? 'P' : 'A' + (s))
static volatile LONG g_fp_hooked;  // 0 not tried, 1 on, 2 bytes differ
typedef uint64_t (*FpFn)(uint64_t, uint64_t, uint64_t, uint64_t);
static FpFn o_fp;
volatile uint64_t g_fp_rbx __attribute__((used));  // caller's rbx at 2B4A45 (the collision object when the caller is 2B30A0); checked against [obj+0xB0] == H
static double g_fp_qpf; static volatile uint32_t g_fp_frame;
static struct { uint64_t a, ai, ctrl; uint32_t chr; int valid, pulse, pulseUb, pulseGrab, nodmg; FpRun run, runW; double stopMs; } g_fp[N3T_SLOTS + 1];  // run = game clock, runW = wall clock; stopMs = game ms with the boss motion play rate 0 (hit-stop)  // pulse: a hit since the last frame meter sample
static volatile LONG g_fp_calls, g_fp_match, g_fp_diag; static DWORD g_fp_diagT;
#define FP_WIN 128  // open colliders tracked (#22575: a 30 fps cap makes ~100 per 0x4E6E swing, about 50 open at once)
static struct { uint64_t obj, H; int pid, slot, idx, used, great, grab; double t, t0, gt, gt0; uint32_t f, act; } g_fpw[FP_WIN];  // t/t0 wall, gt/gt0 game clock
static volatile LONG g_fp_lock, g_fp_lines; static LONG g_fp_lastCalls = -1; static FpRate g_fp_rate; static LONG g_fp_drop;
static double FpNow(void) { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart * g_fp_qpf; }
// Game clock (ms), advanced once per pump tick under the lock by FpGameStep. g_fp_g* = diagnostics for the stats line.
static double g_fp_gms, g_fp_gwall0, g_fp_gms0; static uint32_t g_fp_g610; static int g_fp_g610ok, g_fp_gfps, g_fp_gfb; static float g_fp_gts; static LONG g_fp_gticks;
static __attribute__((noinline)) double FpGameStep(double wallDt) {
    uint64_t g = 0; double d = -1.0;
    if (Rd64(B + Grva(0x4759910), &g) && g) {
        __try {
            const uint32_t n = *(volatile uint32_t*)(g + 0x610); const int fps = *(volatile int32_t*)(g + 0x614); const float ts = *(volatile float*)(g + 0x624);
            uint32_t k = g_fp_g610ok ? n - g_fp_g610 : 1; g_fp_g610 = n; g_fp_g610ok = 1;
            if (k > 8) k = 8;  // load / long stall: at most 8 steps
            if (fps > 0 && fps <= 1000 && ts >= 0.0f && ts < 16.0f) { d = (double)k * 1000.0 * (double)ts / (double)fps; g_fp_gticks += (LONG)k; g_fp_gfps = fps; g_fp_gts = ts; }
        } __except (EXCEPTION_EXECUTE_HANDLER) { d = -1.0; }
    }
    if (d < 0) { g_fp_gfb++; d = wallDt < 0 ? 0 : wallDt > 100 ? 100 : wallDt; }  // engine fields unreadable: real time (counted as fallback ticks)
    g_fp_gms += d; return d;
}
static int FpLock(void) { for (int i = 0; i < 200000; ++i) { if (InterlockedCompareExchange(&g_fp_lock, 1, 0) == 0) return 1; YieldProcessor(); } return 0; }
static void FpUnlock(void) { InterlockedExchange(&g_fp_lock, 0); }
static void FmFeed(double gdt);  // frame_meter_game.h (Practice frame meter), called at the end of FpTick under the lock
static void FpLog(const char* fmt, ...) {
    if (!g_cfg_fp || g_fp_lines > 30000) return;  // log only with [Debug] FrameProbe 1 (the frame meter alone installs the hook silently)
    if (!FpRateOk(&g_fp_rate, FpNow(), 40.0)) { g_fp_drop++; return; }
    InterlockedIncrement(&g_fp_lines);
    char m[900]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
    if (g_fp_drop) { Log("%s (%ld lines dropped by the rate limit)", m, g_fp_drop); g_fp_drop = 0; } else Log("%s", m);
}
static void FpRunEnd(int s, uint32_t next, double now) {
    FpRun* r = &g_fp[s].run; FpRun* w = &g_fp[s].runW; double ran, su, ac, rc, wran, wsu, wac, wrc;
    const int n = r->hits;
    const int cut = FpRunCut(r, g_fp_gms); FpRunCut(w, now);  // windows still open at the action change end with it (the hitend line still follows)
    char hs[300], ws[300]; int o = 0, ow = 0; hs[0] = ws[0] = 0;
    // first 12 only (log line cap); every window also has its own hit / hitend line
    for (int i = 0; i < n && i < 12 && o < (int)sizeof hs - 40; ++i) o += snprintf(hs + o, sizeof hs - o, "%s%.1f/%.1f", i ? " " : "", r->hit[i], r->end[i]);
    for (int i = 0; i < n && i < 12 && ow < (int)sizeof ws - 40; ++i) ow += snprintf(ws + ow, sizeof ws - ow, "%s%.1f/%.1f", i ? " " : "", w->hit[i], w->end[i]);
    FpRunSummary(w, now, &wran, &wsu, &wac, &wrc);
    if (FpRunSummary(r, g_fp_gms, &ran, &su, &ac, &rc))
        FpLog("FRAMEPROBE run chr %u boss %c act 0x%X -> 0x%X ran %.1f ms (%.1f f60, wall %.1f ms, %u pump frames, fps %d) hits %d (colliders %d, cut at the action end %d) great %d startup %.1f ms (%.1f f) active %.1f ms (%.1f f) recovery %.1f ms (%.1f f) hit/end ms [%s] wall hit/end [%s] boss stopped %.1f f",
            g_fp[s].chr, FPB(s), r->act, next, ran, FpFrames60(ran), wran, g_fp_frame - r->f0, g_fp_gfps, r->hits, r->colliders, cut, r->great, su, FpFrames60(su), ac, FpFrames60(ac), rc, FpFrames60(rc), hs, ws, FpFrames60(g_fp[s].stopMs));
    else FpLog("FRAMEPROBE run chr %u boss %c act 0x%X -> 0x%X ran %.1f ms (%.1f f60, wall %.1f ms, %u pump frames, fps %d) hits 0", g_fp[s].chr, FPB(s), r->act, next, ran, FpFrames60(ran), wran, g_fp_frame - r->f0, g_fp_gfps);
}
static void FpObserve(int s, uint32_t id, double now, const char* by) {
    if (g_fp[s].valid && g_fp[s].run.act == id) return;
    if (g_fp[s].valid) FpRunEnd(s, id, now);
    FpRunStart(&g_fp[s].run, id, g_fp_gms, g_fp_frame); FpRunStart(&g_fp[s].runW, id, now, g_fp_frame); g_fp[s].stopMs = 0; g_fp[s].valid = 1;
    FpLog("FRAMEPROBE start chr %u boss %c act 0x%X pump %u (seen by %s)", g_fp[s].chr, FPB(s), id, g_fp_frame, by);
}
static __attribute__((noinline)) int FpPoolId(uint64_t obj) {
    uint64_t pool = 0; if (!obj || !Rd64(B + Grva(0x45C6F38), &pool) || !pool) return -1;
    __try { for (int i = 0; i < 0xFA0; ++i) if (*(volatile uint64_t*)(pool + 0x20 + i * 8) == obj) return i; } __except (EXCEPTION_EXECUTE_HANDLER) { }
    return -1;
}
static __attribute__((noinline)) void FpHit(uint64_t H, uint64_t atk, uint32_t r8, uint64_t R, uint64_t obj, uintptr_t ra) {
    InterlockedIncrement(&g_fp_calls);
    uint64_t a8 = 0; if (atk) Rd64(atk + 8, &a8);
    int s = -1, how = 0;
    const uint64_t owner = atk ? FpOwnerChar(atk, (FpRd64)Rd64) : 0;  // projectiles / shots: the owner character (game resolver 325F4C), match kind 4
    for (int i = 0; i < N3T_SLOTS; ++i) if (atk && g_fp[i].a) { const int h = g_fp[i].a == atk ? 1 : g_fp[i].ai == atk ? 2 : g_fp[i].ctrl && a8 == g_fp[i].ctrl ? 3 : owner && owner == g_fp[i].a ? 4 : 0; if (h) { s = i; how = h; } }
    if (s < 0 && (g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) && g_fp[FP_PL].a && atk) { const int h = g_fp[FP_PL].a == atk ? 1 : g_fp[FP_PL].ai == atk ? 2 : g_fp[FP_PL].ctrl && a8 == g_fp[FP_PL].ctrl ? 3 : owner && owner == g_fp[FP_PL].a ? 4 : 0; if (h) { s = FP_PL; how = h; } }  // user #22796: the player's own hits
    if (s < 0) {  // not a Training boss (player, allies, NPCs, projectiles without an owner link); the first 40 are logged for the attacker mapping
        if (g_cfg_fp && InterlockedIncrement(&g_fp_diag) <= 40) Log("FRAMEPROBE diag call from %X atk %p [atk+8] %p owner %p H %p R %p obj %p | boss A a %p ai %p ctrl %p | player %p ai %p", (uint32_t)(ra - B), (void*)atk, (void*)a8, (void*)owner, (void*)H, (void*)R, (void*)obj,
            (void*)g_fp[0].a, (void*)g_fp[0].ai, (void*)g_fp[0].ctrl, (void*)PlayerChar(), (void*)(PlayerChar() ? *(volatile uint64_t*)(PlayerChar() + 0x3B0) : 0));
        return;
    }
    InterlockedIncrement(&g_fp_match);
    uint64_t ctrl = 0, act = 0, rowf = 0, q0 = 0; uint32_t id = 0xFFFFFFFF, h13c = 0; uint16_t pw = 0, el = 0, ty = 0, r28 = 0, flags4c = 0;
    __try {
        ctrl = g_fp[s].ctrl; act = ctrl ? *(volatile uint64_t*)(ctrl + 0xE8) : 0;  // attacker = actor (how 1), AI (2) or an object whose +8 is the ctrl (3)
        if (act) { id = *(volatile uint32_t*)act; rowf = *(volatile uint64_t*)(act + 0x20); if (rowf) q0 = *(volatile uint64_t*)rowf; }
        h13c = *(volatile uint32_t*)(H + 0x13C);
        if (R) { pw = *(volatile uint16_t*)(R + 0x10); el = *(volatile uint16_t*)(R + 0x12); ty = *(volatile uint16_t*)(R + 0x14); r28 = *(volatile uint16_t*)(R + 0x28); flags4c = *(volatile uint16_t*)(R + 0x4C); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { if (g_cfg_fp && InterlockedIncrement(&g_fp_diag) <= 60) Log("FRAMEPROBE diag: boss call (match %d) read faulted, ctrl %p act %p H %p R %p", how, (void*)ctrl, (void*)act, (void*)H, (void*)R); return; }
    int own = 0; uint64_t hb = 0;
    if (ra - B == Grva(0x2B30E2) && obj && Rd64(obj + 0xB0, &hb) && hb == H) own = 1;
    const int pid = own ? FpPoolId(obj) : -1;
    if (!FpLock()) return;
    const double now = FpNow();
    FpObserve(s, id, now, "hook");
    const int grab = (flags4c & 1) != 0; // Native grab filter 117A99F: [[collision+0xB0]+0]+0x4C bit 0.
    if (!grab && FpNoDamageRow(R != 0, pw, el)) {  // live Hino-enma: a power 0 / 0 type 12 collider at the start of its steps 0x4E20-22 (static: no own rows)
        if (++g_fp[s].nodmg <= 3) FpLog("FRAMEPROBE nodmg chr %u boss %c act 0x%X at %.1f ms: row %p power 0 0 type %u r28 %u from %X match %d (not a hit window; not on the meter)",
            g_fp[s].chr, FPB(s), id, g_fp_gms - g_fp[s].run.t0, (void*)R, ty, r28, (uint32_t)(ra - B), how);
        FpUnlock(); return;
    }
    int wf = -1; if (pid >= 0) for (int w = 0; w < FP_WIN; ++w) if (!g_fpw[w].used) { wf = w; break; }
    FpRun* r = &g_fp[s].run; int isNew = 0; const int great = (h13c >> 10) & 1;
    const int k = FpRunHitEx(r, g_fp_gms, great, wf >= 0, -1, &isNew); FpRunHitEx(&g_fp[s].runW, now, great, wf >= 0, k, 0);
    g_fp[s].pulse = 1; if (grab) g_fp[s].pulseGrab = 1; if (great) g_fp[s].pulseUb = 1;  // frame meter: hit window opened (unblockable = GreatAttack bit)
    if (wf >= 0) for (int w = wf; w == wf; ++w) {
        g_fpw[w].obj = obj; g_fpw[w].H = H; g_fpw[w].pid = pid; g_fpw[w].slot = s; g_fpw[w].idx = k; g_fpw[w].t = now; g_fpw[w].t0 = g_fp[s].runW.t0; g_fpw[w].gt = g_fp_gms; g_fpw[w].gt0 = r->t0; g_fpw[w].f = g_fp_frame; g_fpw[w].act = id; g_fpw[w].great = (h13c >> 10) & 1; g_fpw[w].grab = grab; g_fpw[w].used = 1; break; }
    if (isNew || r->colliders <= 12) FpLog("FRAMEPROBE hit chr %u boss %c act 0x%X #%d at %.1f ms (%.1f f60, wall %.1f ms, pump +%u, collider %d) great %d (row bit45 %d) power %u %u %u (type %u) r28 %u h13C %08X r8 %u row %p obj %p pool %d from %X match %d",
        g_fp[s].chr, FPB(s), id, k, g_fp_gms - r->t0, FpFrames60(g_fp_gms - r->t0), now - g_fp[s].runW.t0, g_fp_frame - r->f0, r->colliders, great, (int)((q0 >> 45) & 1), pw, el, ty, ty, r28, h13c, r8, (void*)R, (void*)obj, pid, (uint32_t)(ra - B), how);
    FpUnlock();
}
__attribute__((used)) uint64_t FpDetour(uint64_t H, uint64_t atk, uint64_t r8, uint64_t R) {
    const uint64_t obj = g_fp_rbx; const uintptr_t ra = (uintptr_t)__builtin_return_address(0);
    const uint64_t ret = o_fp(H, atk, r8, R);
    __try { FpHit(H, atk, (uint32_t)r8, R, obj, ra); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    return ret;
}
// Entry from the site jump: save the caller's rbx (untouched by 2B4A3C before 2B4A45), then the C detour with the original stack and arguments.
__attribute__((naked, used)) static void FpEntry(void) { __asm__ volatile("movq %rbx, g_fp_rbx(%rip)\n\tjmp FpDetour\n\t"); }
static void FpInstall(void) {
    if (g_fp_hooked) return;
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_fp_qpf = 1000.0 / (double)f.QuadPart;
    static const uint8_t pro[21] = {0x48, 0x85, 0xC9, 0x0F, 0x84, 0x80, 0x0F, 0x00, 0x00, 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x10, 0x4C, 0x89, 0x48, 0x20, 0x55};
    if (memcmp((void*)(B + Grva(0x2B4A3C)), pro, sizeof pro)) { g_fp_hooked = 2; Log("FRAMEPROBE hook 2B4A3C: bytes differ, probe off"); return; }
    o_fp = (FpFn)Hook(0x2B4A45, 12, (void*)FpEntry); g_fp_hooked = 1;
    Log("FRAMEPROBE hook 2B4A3C (+9 = 2B4A45): on, read only (%s)", g_cfg_fp ? "log on, [Debug] FrameProbe 1" : "frame meter only, no log lines");
}
// user #22796: the player's action run, same bookkeeping as a boss slot (actor = PlayerChar(), ctrl = [[actor+0x3B0]+8], id = 358528(ctrl)).
// UNTESTED live: that the player's ctrl gives a changing action id the same way (notes/player-frame-meter-22796.md); the hit windows are proven (the probe
// already sees the player's 2B30A0 calls, atk == player actor).
int FmIFrameProbeOn(void);  // frame_meter_game.h. 1 when [Debug] IFrameProbe is on, even with no Practice session.
int FmKiPressOn(void);      // frame_meter_game.h. 1 when [Debug] KiPressProbe is on.
static void FpPlayerTick(double now, double gdt) {
    const int s = FP_PL; static int bad;
    const uint64_t a = (g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW]) || FmIFrameProbeOn() || FmKiPressOn() ? PlayerChar() : 0; uint64_t ai = 0, ctrl = 0; uint32_t id = 0xFFFFFFFF;
    if (!a || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) { if (g_fp[s].valid) FpRunEnd(s, 0xFFFFFFFF, now); g_fp[s].valid = 0; g_fp[s].a = g_fp[s].ai = g_fp[s].ctrl = 0; return; }
    if (g_fp[s].a != a) { if (g_fp[s].valid) FpRunEnd(s, 0xFFFFFFFF, now); g_fp[s].valid = 0; g_fp[s].a = a; g_fp[s].chr = 0; }
    g_fp[s].ai = ai; g_fp[s].ctrl = ctrl;
    __try { id = ((uint32_t (*)(uint64_t))(B + Grva(0x358528)))(ctrl); } __except (EXCEPTION_EXECUTE_HANDLER) { if (!bad++) Log("FRAME METER player: action id read faulted (ctrl %p)", (void*)ctrl); return; }
    FpObserve(s, id, now, "pump");
    (void)gdt;
}
// Pump (game thread, every frame): action changes of every Training boss, and the end of open hit windows.
static void FpTick(void) {
    if (!g_fp_hooked && ((g_pst.v[PS_FRAME_METER] && g_pst_live) || g_pst.v[PS_PLAYER_METER] || g_pst.v[PS_KI_WINDOW])) FpInstall();  // #36186 the player row also runs with no boss
    if (g_fp_hooked != 1) return;
    g_fp_frame++;
    if (!FpLock()) return;
    static double lastWall; const double now = FpNow();
    const double gdt = FpGameStep(lastWall > 0 ? now - lastWall : 0); lastWall = now;
    if (g_fp_gwall0 <= 0) { g_fp_gwall0 = now; g_fp_gms0 = g_fp_gms; }
    if (g_cfg_fp && GetTickCount() - g_fp_diagT > 10000 && g_fp_calls != g_fp_lastCalls) {
        g_fp_diagT = GetTickCount(); g_fp_lastCalls = g_fp_calls;
        Log("FRAMEPROBE stats: hook calls %ld, boss calls %ld, pump frames %u, game ticks %ld (fallback %d), fps-for-dt %d, time scale %.3f, game/wall %.4f since the last stats line",
            g_fp_calls, g_fp_match, g_fp_frame, g_fp_gticks, g_fp_gfb, g_fp_gfps, g_fp_gts, now - g_fp_gwall0 > 0 ? (g_fp_gms - g_fp_gms0) / (now - g_fp_gwall0) : 0.0);
        g_fp_gwall0 = now; g_fp_gms0 = g_fp_gms;
    }
    for (int s = 0; s < N3T_SLOTS; ++s) {
        const uint64_t a = BvActor(s); uint64_t ai = 0, ctrl = 0; uint32_t id = 0xFFFFFFFF;
        if (!a || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) { if (g_fp[s].valid) FpRunEnd(s, 0xFFFFFFFF, now); g_fp[s].valid = 0; g_fp[s].a = g_fp[s].ai = 0; continue; }
        if (g_fp[s].a != a) { if (g_fp[s].valid) FpRunEnd(s, 0xFFFFFFFF, now); g_fp[s].valid = 0; g_fp[s].a = a; g_fp[s].chr = g_slot_chr[s]; g_fp[s].nodmg = 0;
            InterlockedExchange(&g_fp_diag, 0); }  // new boss: fresh diag budget, so unmatched calls during this fight (projectiles, helpers) are logged too
        g_fp[s].ai = ai; g_fp[s].ctrl = ctrl;
        __try { id = ((uint32_t (*)(uint64_t))(B + Grva(0x358528)))(ctrl); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        FpObserve(s, id, now, "pump");
        { uint64_t mo = 0; float sp = 1.0f;  // boss main motion play rate [a+0x40]+0x324 (0 = hit-stop, notes/boss-speed-lever-22389.md): counted, not removed
          if (Rd64(a + 0x40, &mo) && mo) { __try { sp = *(volatile float*)(mo + 0x324); } __except (EXCEPTION_EXECUTE_HANDLER) { sp = 1.0f; } }
          if (sp == 0.0f) g_fp[s].stopMs += gdt; }
    }
    FpPlayerTick(now, gdt);
    uint64_t pool = 0; Rd64(B + Grva(0x45C6F38), &pool);
    for (int w = 0; w < FP_WIN; ++w) {
        if (!g_fpw[w].used) continue;
        uint64_t e = 0, hb = 0;
        const int gone = !pool || !Rd64(pool + 0x20 + (uint64_t)g_fpw[w].pid * 8, &e) || e != g_fpw[w].obj || !Rd64(g_fpw[w].obj + 0xB0, &hb) || hb != g_fpw[w].H;
        const int s = g_fpw[w].slot;
        if (gone) {
            const int same = g_fp[s].valid && g_fp[s].runW.t0 == g_fpw[w].t0, k = g_fpw[w].idx;
            if (same) { FpRunHitEnd(&g_fp[s].run, k, g_fp_gms); FpRunHitEnd(&g_fp[s].runW, k, now); }
            // one line per merged window when its last tracked collider is gone (a later joining collider prints a new end for the same #)
            if (!same || g_fp[s].run.open[k] == 0) FpLog("FRAMEPROBE hitend chr %u boss %c act 0x%X #%d window %.1f ms (%.1f f60, wall %.1f ms, %u pump frames) ends at %.1f ms (wall %.1f ms)",
                g_fp[s].chr, FPB(s), g_fpw[w].act, k, same ? g_fp[s].run.end[k] - g_fp[s].run.hit[k] : g_fp_gms - g_fpw[w].gt, FpFrames60(same ? g_fp[s].run.end[k] - g_fp[s].run.hit[k] : g_fp_gms - g_fpw[w].gt),
                same ? g_fp[s].runW.end[k] - g_fp[s].runW.hit[k] : now - g_fpw[w].t, g_fp_frame - g_fpw[w].f, g_fp_gms - g_fpw[w].gt0, now - g_fpw[w].t0);
            g_fpw[w].used = 0;
        } else if (now - g_fpw[w].t > 5000) {
            if (g_fp[s].valid && g_fp[s].runW.t0 == g_fpw[w].t0) { const int k = g_fpw[w].idx; if (g_fp[s].run.open[k] > 0) g_fp[s].run.open[k]--; if (g_fp[s].runW.open[k] > 0) g_fp[s].runW.open[k]--; }
            FpLog("FRAMEPROBE hitend chr %u act 0x%X #%d: pool slot %d still held after 5 s (no end seen)", g_fp[s].chr, g_fpw[w].act, g_fpw[w].idx, g_fpw[w].pid); g_fpw[w].used = 0; }
    }
    FmFeed(gdt);
    FpUnlock();
}
