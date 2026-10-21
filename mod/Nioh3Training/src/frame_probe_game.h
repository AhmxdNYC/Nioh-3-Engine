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
#include "frame_probe.h"
static int g_cfg_fp;
static volatile LONG g_fp_hooked;  // 0 not tried, 1 on, 2 bytes differ
typedef uint64_t (*FpFn)(uint64_t, uint64_t, uint64_t, uint64_t);
static FpFn o_fp;
volatile uint64_t g_fp_rbx __attribute__((used));  // caller's rbx at 2B4A45 (the collision object when the caller is 2B30A0); checked against [obj+0xB0] == H
static double g_fp_qpf; static volatile uint32_t g_fp_frame;
static struct { uint64_t a, ai, ctrl; uint32_t chr; int valid, pulse, pulseUb; FpRun run; } g_fp[N3T_SLOTS];  // pulse: a hit since the last frame meter sample
static volatile LONG g_fp_calls, g_fp_match, g_fp_diag; static DWORD g_fp_diagT;
#define FP_WIN 24
static struct { uint64_t obj, H; int pid, slot, idx, used, great; double t, t0; uint32_t f, act; } g_fpw[FP_WIN];
static volatile LONG g_fp_lock, g_fp_lines; static LONG g_fp_lastCalls = -1; static FpRate g_fp_rate; static LONG g_fp_drop;
static double FpNow(void) { LARGE_INTEGER c; QueryPerformanceCounter(&c); return (double)c.QuadPart * g_fp_qpf; }
static int FpLock(void) { for (int i = 0; i < 200000; ++i) { if (InterlockedCompareExchange(&g_fp_lock, 1, 0) == 0) return 1; YieldProcessor(); } return 0; }
static void FpUnlock(void) { InterlockedExchange(&g_fp_lock, 0); }
static void FmFeed(double now);  // frame_meter_game.h (Practice frame meter), called at the end of FpTick under the lock
static void FpLog(const char* fmt, ...) {
    if (!g_cfg_fp || g_fp_lines > 30000) return;  // log only with [Debug] FrameProbe 1 (the frame meter alone installs the hook silently)
    if (!FpRateOk(&g_fp_rate, FpNow(), 40.0)) { g_fp_drop++; return; }
    InterlockedIncrement(&g_fp_lines);
    char m[900]; va_list ap; va_start(ap, fmt); vsnprintf(m, sizeof m, fmt, ap); va_end(ap);
    if (g_fp_drop) { Log("%s (%ld lines dropped by the rate limit)", m, g_fp_drop); g_fp_drop = 0; } else Log("%s", m);
}
static void FpRunEnd(int s, uint32_t next, double now) {
    FpRun* r = &g_fp[s].run; double ran, su, ac, rc;
    const int n = r->hits < FP_MAX_HITS ? r->hits : FP_MAX_HITS;
    char hs[300]; int o = 0; hs[0] = 0;
    // first 12 only (log line cap); every window also has its own hit / hitend line
    for (int i = 0; i < n && i < 12 && o < (int)sizeof hs - 40; ++i) o += snprintf(hs + o, sizeof hs - o, "%s%.1f/%.1f", i ? " " : "", r->hit[i], r->end[i]);
    if (FpRunSummary(r, now, &ran, &su, &ac, &rc))
        FpLog("FRAMEPROBE run chr %u boss %c act 0x%X -> 0x%X ran %.1f ms (%.1f f60, %u pump frames) hits %d great %d startup %.1f ms (%.1f f) active %.1f ms (%.1f f) recovery %.1f ms (%.1f f) hit/end ms [%s]",
            g_fp[s].chr, 'A' + s, r->act, next, ran, FpFrames60(ran), g_fp_frame - r->f0, r->hits, r->great, su, FpFrames60(su), ac, FpFrames60(ac), rc, FpFrames60(rc), hs);
    else FpLog("FRAMEPROBE run chr %u boss %c act 0x%X -> 0x%X ran %.1f ms (%.1f f60, %u pump frames) hits 0", g_fp[s].chr, 'A' + s, r->act, next, ran, FpFrames60(ran), g_fp_frame - r->f0);
}
static void FpObserve(int s, uint32_t id, double now, const char* by) {
    if (g_fp[s].valid && g_fp[s].run.act == id) return;
    if (g_fp[s].valid) FpRunEnd(s, id, now);
    FpRunStart(&g_fp[s].run, id, now, g_fp_frame); g_fp[s].valid = 1;
    FpLog("FRAMEPROBE start chr %u boss %c act 0x%X pump %u (seen by %s)", g_fp[s].chr, 'A' + s, id, g_fp_frame, by);
}
static __attribute__((noinline)) int FpPoolId(uint64_t obj) {
    uint64_t pool = 0; if (!obj || !Rd64(B + 0x45C6F38, &pool) || !pool) return -1;
    __try { for (int i = 0; i < 0xFA0; ++i) if (*(volatile uint64_t*)(pool + 0x20 + i * 8) == obj) return i; } __except (EXCEPTION_EXECUTE_HANDLER) { }
    return -1;
}
static __attribute__((noinline)) void FpHit(uint64_t H, uint64_t atk, uint32_t r8, uint64_t R, uint64_t obj, uintptr_t ra) {
    InterlockedIncrement(&g_fp_calls);
    uint64_t a8 = 0; if (atk) Rd64(atk + 8, &a8);
    int s = -1, how = 0;
    for (int i = 0; i < N3T_SLOTS; ++i) if (atk && g_fp[i].a) { const int h = g_fp[i].a == atk ? 1 : g_fp[i].ai == atk ? 2 : g_fp[i].ctrl && a8 == g_fp[i].ctrl ? 3 : 0; if (h) { s = i; how = h; } }
    if (s < 0) {  // not a Training boss (player, allies, NPCs, projectiles without an owner link); the first 40 are logged for the attacker mapping
        if (g_cfg_fp && InterlockedIncrement(&g_fp_diag) <= 40) Log("FRAMEPROBE diag call from %X atk %p [atk+8] %p H %p R %p obj %p | boss A a %p ai %p ctrl %p | player %p ai %p", (uint32_t)(ra - B), (void*)atk, (void*)a8, (void*)H, (void*)R, (void*)obj,
            (void*)g_fp[0].a, (void*)g_fp[0].ai, (void*)g_fp[0].ctrl, (void*)PlayerChar(), (void*)(PlayerChar() ? *(volatile uint64_t*)(PlayerChar() + 0x3B0) : 0));
        return;
    }
    InterlockedIncrement(&g_fp_match);
    uint64_t ctrl = 0, act = 0, rowf = 0, q0 = 0; uint32_t id = 0xFFFFFFFF, h13c = 0; uint16_t pw = 0, el = 0, ty = 0, r28 = 0;
    __try {
        ctrl = g_fp[s].ctrl; act = ctrl ? *(volatile uint64_t*)(ctrl + 0xE8) : 0;  // attacker = actor (how 1), AI (2) or an object whose +8 is the ctrl (3)
        if (act) { id = *(volatile uint32_t*)act; rowf = *(volatile uint64_t*)(act + 0x20); if (rowf) q0 = *(volatile uint64_t*)rowf; }
        h13c = *(volatile uint32_t*)(H + 0x13C);
        if (R) { pw = *(volatile uint16_t*)(R + 0x10); el = *(volatile uint16_t*)(R + 0x12); ty = *(volatile uint16_t*)(R + 0x14); r28 = *(volatile uint16_t*)(R + 0x28); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { if (g_cfg_fp && InterlockedIncrement(&g_fp_diag) <= 60) Log("FRAMEPROBE diag: boss call (match %d) read faulted, ctrl %p act %p H %p R %p", how, (void*)ctrl, (void*)act, (void*)H, (void*)R); return; }
    int own = 0; uint64_t hb = 0;
    if (ra - B == 0x2B30E2 && obj && Rd64(obj + 0xB0, &hb) && hb == H) own = 1;
    const int pid = own ? FpPoolId(obj) : -1;
    if (!FpLock()) return;
    const double now = FpNow();
    FpObserve(s, id, now, "hook");
    FpRun* r = &g_fp[s].run; const int k = FpRunHit(r, now, (h13c >> 10) & 1);
    g_fp[s].pulse = 1; if ((h13c >> 10) & 1) g_fp[s].pulseUb = 1;  // frame meter: hit window opened (unblockable = GreatAttack bit)
    if (pid >= 0 && k >= 0) for (int w = 0; w < FP_WIN; ++w) if (!g_fpw[w].used) {
        g_fpw[w].obj = obj; g_fpw[w].H = H; g_fpw[w].pid = pid; g_fpw[w].slot = s; g_fpw[w].idx = k; g_fpw[w].t = now; g_fpw[w].t0 = r->t0; g_fpw[w].f = g_fp_frame; g_fpw[w].act = id; g_fpw[w].great = (h13c >> 10) & 1; g_fpw[w].used = 1; break; }
    FpLog("FRAMEPROBE hit chr %u boss %c act 0x%X #%d at %.1f ms (%.1f f60, pump +%u) great %d (row bit45 %d) power %u %u %u (type %u) r28 %u h13C %08X r8 %u row %p obj %p pool %d from %X match %d",
        g_fp[s].chr, 'A' + s, id, k, now - r->t0, FpFrames60(now - r->t0), g_fp_frame - r->f0, (h13c >> 10) & 1, (int)((q0 >> 45) & 1), pw, el, ty, ty, r28, h13c, r8, (void*)R, (void*)obj, pid, (uint32_t)(ra - B), how);
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
    if (memcmp((void*)(B + 0x2B4A3C), pro, sizeof pro)) { g_fp_hooked = 2; Log("FRAMEPROBE hook 2B4A3C: bytes differ, probe off"); return; }
    o_fp = (FpFn)Hook(0x2B4A45, 12, (void*)FpEntry); g_fp_hooked = 1;
    Log("FRAMEPROBE hook 2B4A3C (+9 = 2B4A45): on, read only (%s)", g_cfg_fp ? "log on, [Debug] FrameProbe 1" : "frame meter only, no log lines");
}
// Pump (game thread, every frame): action changes of every Training boss, and the end of open hit windows.
static void FpTick(void) {
    if (!g_fp_hooked && g_pst.v[PS_FRAME_METER] && g_pst_live) FpInstall();  // frame meter switched ON in the menu (game thread, like the other Practice hooks)
    if (g_fp_hooked != 1) return;
    g_fp_frame++;
    if (!FpLock()) return;
    const double now = FpNow();
    if (g_cfg_fp && GetTickCount() - g_fp_diagT > 10000 && g_fp_calls != g_fp_lastCalls) { g_fp_diagT = GetTickCount(); g_fp_lastCalls = g_fp_calls; Log("FRAMEPROBE stats: hook calls %ld, boss calls %ld, pump frames %u", g_fp_calls, g_fp_match, g_fp_frame); }
    for (int s = 0; s < N3T_SLOTS; ++s) {
        const uint64_t a = BvActor(s); uint64_t ai = 0, ctrl = 0; uint32_t id = 0xFFFFFFFF;
        if (!a || !Rd64(a + 0x3B0, &ai) || !ai || !Rd64(ai + 8, &ctrl) || !ctrl) { if (g_fp[s].valid) FpRunEnd(s, 0xFFFFFFFF, now); g_fp[s].valid = 0; g_fp[s].a = g_fp[s].ai = 0; continue; }
        if (g_fp[s].a != a) { if (g_fp[s].valid) FpRunEnd(s, 0xFFFFFFFF, now); g_fp[s].valid = 0; g_fp[s].a = a; g_fp[s].chr = g_slot_chr[s]; }
        g_fp[s].ai = ai; g_fp[s].ctrl = ctrl;
        __try { id = ((uint32_t (*)(uint64_t))(B + 0x358528))(ctrl); } __except (EXCEPTION_EXECUTE_HANDLER) { continue; }
        FpObserve(s, id, now, "pump");
    }
    uint64_t pool = 0; Rd64(B + 0x45C6F38, &pool);
    for (int w = 0; w < FP_WIN; ++w) {
        if (!g_fpw[w].used) continue;
        uint64_t e = 0, hb = 0;
        const int gone = !pool || !Rd64(pool + 0x20 + (uint64_t)g_fpw[w].pid * 8, &e) || e != g_fpw[w].obj || !Rd64(g_fpw[w].obj + 0xB0, &hb) || hb != g_fpw[w].H;
        const int s = g_fpw[w].slot;
        if (gone) {
            if (g_fp[s].valid && g_fp[s].run.t0 == g_fpw[w].t0) FpRunHitEnd(&g_fp[s].run, g_fpw[w].idx, now);
            FpLog("FRAMEPROBE hitend chr %u boss %c act 0x%X #%d window %.1f ms (%.1f f60, %u pump frames) ends at %.1f ms",
                g_fp[s].chr, 'A' + s, g_fpw[w].act, g_fpw[w].idx, now - g_fpw[w].t, FpFrames60(now - g_fpw[w].t), g_fp_frame - g_fpw[w].f, now - g_fpw[w].t0);
            g_fpw[w].used = 0;
        } else if (now - g_fpw[w].t > 5000) { FpLog("FRAMEPROBE hitend chr %u act 0x%X #%d: pool slot %d still held after 5 s (no end seen)", g_fp[s].chr, g_fpw[w].act, g_fpw[w].idx, g_fpw[w].pid); g_fpw[w].used = 0; }
    }
    FmFeed(now);
    FpUnlock();
}
