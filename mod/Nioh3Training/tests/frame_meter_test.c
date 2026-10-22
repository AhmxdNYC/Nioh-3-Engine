// user #22356 follow-up: frame meter state machine (frame_meter.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/frame_meter_test.c -o build/frame_meter_test.exe && build/frame_meter_test.exe
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/frame_meter.h"
static int within(int a, int b, int tol) { return a - b <= tol && b - a <= tol; }
static int count(const FmMeter* m, uint8_t st) { int n = 0; for (int i = 0; i < FM_CELLS; ++i) n += (FmCellAt(m, i) & 0x7F) == st; return n; }
// Yoritomo 0x4E6E as measured live (notes/frame-probe-22356.md): windows 94.0-105.7 and 141.2-156.9 frames, 8 colliders, action 333 frames.
// Sampled at ~97 fps like the game ran. Colliders: 4 per hit, overlapping, with a short gap inside the second hit (< one frame: same hit).
static void Drill(FmMeter* m, double* t, double step, int ub) {
    const double t0 = *t, f = 1000.0 / 60.0;
    for (; *t < t0 + 333.0 * f; *t += step) {
        const double fr = (*t - t0) / f;
        const int act = (fr >= 94.0 && fr < 105.7) || (fr >= 141.2 && fr < 148.0) || (fr >= 148.5 && fr < 156.9);
        FmSample(m, *t, 0x4E6E, act, act && ub);
    }
}
int main(void) {
    FmMeter m; FmReset(&m); char l1[200], l2[200];
    assert(m.lastCell == -1 && FmCellAt(&m, 0) == FM_EMPTY && FmCellAt(&m, FM_CELLS - 1) == FM_EMPTY);
    FmText(&m.cur, l1, sizeof l1, l2, sizeof l2); assert(strstr(l1, "waiting"));
    double t = 1000.0; const double step = 1000.0 / 97.0;
    // idle 0x2 for 1 s: pending while it runs (no hit yet), neutral once it ends without a hit
    for (; t < 2000.0; t += step) FmSample(&m, t, 0x2, 0, 0);
    assert(count(&m, FM_PENDING) >= 59 && count(&m, FM_NEUTRAL) == 0);
    Drill(&m, &t, step, 1);
    assert(count(&m, FM_PENDING) == 0 && count(&m, FM_NEUTRAL) >= 20 /* the idle part still in the ring */);
    // in progress (recovery not over yet): numbers already there, marked '+'
    assert(m.cur.hits == 2 && !m.cur.done);
    FmSample(&m, t, 0x2, 0, 0);  // back to idle: the run is complete
    // fixed view: the bar starts at the move's first cell (the idle before it has scrolled out of the view, not of the ring)
    { int st = 0, ubc = 0; for (int i = 0; i < FM_CELLS; ++i) { const uint8_t c = FmViewCell(&m, i) & 0x7F; st += c == FM_STARTUP; ubc += c == FM_ACTIVE_UB; }
      assert((FmViewCell(&m, 0) & FM_EDGE) && (FmViewCell(&m, 0) & 0x7F) == FM_STARTUP && within(st, 94, 2) && within(ubc, 28, 3)); }
    { FmMeter v = m; double tv = t; for (int i = 0; i < 600; ++i, tv += step) FmSample(&v, tv, 0x2, 0, 0);  // long idle after it: still frozen
      int st = 0; for (int i = 0; i < FM_CELLS; ++i) st += (FmViewCell(&v, i) & 0x7F) == FM_STARTUP;
      assert((FmViewCell(&v, 0) & 0x7F) == FM_STARTUP && within(st, 94, 2) && FmViewCell(&v, FM_CELLS - 1) == FM_EMPTY && within(v.frozenN, 333, 2));
      // the next attack takes over at its first hit, from its own first cell
      const double n0 = tv; for (; tv < n0 + 1000.0; tv += step) FmSample(&v, tv, 0x4E4E, (tv - n0) * 0.06 >= 48.3, 0);
      assert((FmViewCell(&v, 0) & FM_EDGE) && (FmViewCell(&v, 0) & 0x7F) == FM_STARTUP && (FmViewCell(&v, 48) & 0x7F) == FM_STARTUP && (FmViewCell(&v, 50) & 0x7F) == FM_ACTIVE); }
    { FmMeter v; FmReset(&v); double tv = 0; for (int i = 0; i < 900; ++i, tv += step) FmSample(&v, tv, 0x2, 0, 0);  // before any attack: scrolls
      assert(FmViewCell(&v, FM_CELLS - 1) == FmCellAt(&v, FM_CELLS - 1) && (FmViewCell(&v, 0) & 0x7F) == FM_PENDING); }
    const FmSum* s = &m.last;
    assert(m.haveLast && s->done && s->act == 0x4E6E && s->hits == 2 && s->ub == 2);
    assert(within(s->startup, 95, 1) && within(s->ws[0], 94, 1) && within(s->we[0], 106, 1) && within(s->ws[1], 141, 1) && within(s->we[1], 157, 1));
    assert(within(s->total, 333, 1) && within(s->recovery, 176, 2) && s->recovery == s->total - s->we[1]);
    // bar: ~94 startup cells, ~12 + ~16 unblockable active cells, recovery up to the end, an edge at each action start
    assert(within(count(&m, FM_STARTUP), 94, 2) && within(count(&m, FM_ACTIVE_UB), 28, 3) && count(&m, FM_ACTIVE) == 0);
    assert(within(count(&m, FM_RECOVERY), 333 - 94 - 28, 4));
    int edges = 0; for (int i = 0; i < FM_CELLS; ++i) edges += !!(FmCellAt(&m, i) & FM_EDGE);
    assert(edges >= 2);
    FmText(s, l1, sizeof l1, l2, sizeof l2);
    assert(strstr(l1, "act 0x4E6E") && strstr(l1, "hits 2") && strstr(l1, "UNBLOCKABLE") && strstr(l2, "startup ") && strstr(l2, "/ total ") && !strstr(l2, "+"));
    printf("  %s\n  %s\n", l1, l2);
    // idle after the move: the summary of the last attack stays shown
    for (int i = 0; i < 50; ++i, t += step) FmSample(&m, t, 0x2, 0, 0);
    assert(m.cur.hits == 2 && m.cur.act == 0x4E6E && m.cur.done);
    // a blockable 1-hit move: active cells (not unblockable), one hit
    FmSample(&m, t, 0x4E4E, 0, 0); const double a0 = t;
    for (; t < a0 + 146.0 * 1000.0 / 60.0; t += step) { const double fr = (t - a0) * 0.06; FmSample(&m, t, 0x4E4E, fr >= 48.3 && fr < 53.5, 0); }
    FmSample(&m, t, 0x2, 0, 0);
    assert(m.last.hits == 1 && m.last.ub == 0 && within(m.last.startup, 49, 1) && within(m.last.total, 146, 1) && count(&m, FM_ACTIVE) >= 4);
    FmText(&m.last, l1, sizeof l1, l2, sizeof l2); assert(!strstr(l1, "UNBLOCKABLE"));
    // a move without a hit (0x4E50): stays neutral, the previous attack's numbers stay
    FmSample(&m, t, 0x4E50, 0, 0); for (int i = 0; i < 100; ++i, t += step) FmSample(&m, t, 0x4E50, 0, 0);
    FmSample(&m, t, 0x2, 0, 0); assert(m.last.act == 0x4E4E && m.cur.act == 0x4E4E);
    // a hit seen for one tick only (no window end tracked): one active cell, still one hit
    FmReset(&m); t = 0;
    FmSample(&m, t, 7, 0, 0); t += 500; FmSample(&m, t, 7, 1, 0); t += step; FmSample(&m, t, 7, 0, 0); t += 500; FmSample(&m, t, 8, 0, 0);
    assert(m.last.hits == 1 && m.last.startup == 31 && count(&m, FM_ACTIVE) == 1);
    // low fps (30 fps samples): no holes in the bar
    FmReset(&m); t = 0; for (int i = 0; i < 90; ++i, t += 33.3) FmSample(&m, t, 9, 0, 0);
    assert(count(&m, FM_EMPTY) == FM_CELLS - (int)FmCellOf(t - 33.3) - 1);
    // a long pause / hitch: the ring keeps the newest FM_CELLS frames only, clock never goes back
    FmSample(&m, t + 600000.0, 9, 0, 0); FmSample(&m, t, 9, 1, 0) /* back in time: ignored for the bar */;
    assert(count(&m, FM_EMPTY) == 0);
    // more hits than FM_MAXW merge into the last one
    FmReset(&m); t = 0; FmSample(&m, t, 5, 0, 0);
    for (int k = 0; k < 20; ++k) { t += 100; FmSample(&m, t, 5, 1, 0); t += 100; FmSample(&m, t, 5, 0, 0); }
    FmSample(&m, t + 100, 6, 0, 0); assert(m.last.hits == FM_MAXW);
    // user #22918 slot pick: follow the boss that is busy, not always the first valid slot
    {   int v[4] = {1, 1, 0, 0}, b[4] = {0, 0, 0, 0};
        assert(FmPickSlot(-1, 4, v, b) == 0 && FmPickSlot(1, 4, v, b) == 1 /* idle: keep the current */ && FmPickSlot(3, 4, v, b) == 0 /* current gone: first valid */);
        b[1] = 1; assert(FmPickSlot(0, 4, v, b) == 1 /* Boss B attacks while stale/idle Boss A is followed */ && FmPickSlot(-1, 4, v, b) == 1);
        b[0] = 1; assert(FmPickSlot(1, 4, v, b) == 1 && FmPickSlot(0, 4, v, b) == 0 /* both busy: keep current */);
        b[2] = 1; assert(FmPickSlot(0, 4, v, b) == 0 /* busy but not valid: ignored */);
        v[0] = v[1] = 0; assert(FmPickSlot(0, 4, v, b) == -1);
    }
    puts("frame_meter_test: all passed");
    return 0;
}
