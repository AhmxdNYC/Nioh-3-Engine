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
// #35176: i-frames are a separate stripe, not recovery. A dodge (no hit window) with no-damage frames is shown and frozen.
static void NoDmgTests(void) {
    const double f = 1000.0 / 60.0; FmMeter g; FmReset(&g); double t = 1;
    // attack 1: hit at frames 10-14, recovery after; no i-frames
    for (int i = 0; i < 30; ++i, t += f) { FmSampleEx(&g, t, 0x10, i >= 10 && i < 15, 0, 0); FmMarkNoDmg(&g, t, 0); }
    // dodge 2: 24 frames, no hit, i-frames on frames 3..12
    for (int i = 0; i < 24; ++i, t += f) { FmSampleEx(&g, t, 0x20, 0, 0, 0); FmMarkNoDmg(&g, t, i >= 3 && i <= 12); }
    assert(g.on && g.nw == 0 && g.nd == 10 && FmLiveRun(&g));  // live: the dodge, not the frozen attack
    assert((FmViewCell(&g, 0) & FM_EDGE) && FmViewNoDmg(&g, 3) && FmViewNoDmg(&g, 12) && !FmViewNoDmg(&g, 2) && !FmViewNoDmg(&g, 13));
    for (int i = 0; i < 24; ++i) assert((FmViewCell(&g, i) & 0x7F) != FM_RECOVERY);  // i-frame != recovery
    // idle 3: the dodge stays frozen with its stripe
    for (int i = 0; i < 5; ++i, t += f) { FmSampleEx(&g, t, 0x2, 0, 0, 0); FmMarkNoDmg(&g, t, 0); }
    assert(!FmLiveRun(&g) && g.haveLast && g.nodmgFrozenN == 24);
    int n = 0; for (int i = 0; i < FM_CELLS; ++i) n += FmViewNoDmg(&g, i);
    assert(n == 10 && (FmViewCell(&g, 0) & 0x7F) == FM_NEUTRAL);
    // attack 4 with i-frames during its own active frames: both shown, counts unchanged
    for (int i = 0; i < 20; ++i, t += f) { FmSampleEx(&g, t, 0x30, i >= 5 && i < 8, 0, 0); FmMarkNoDmg(&g, t, i >= 4 && i < 9); }
    assert(g.nw == 1 && g.nd == 5 && FmViewNoDmg(&g, 4) && (FmViewCell(&g, 5) & 0x7F) == FM_ACTIVE && (FmViewCell(&g, 12) & 0x7F) == FM_RECOVERY && !FmViewNoDmg(&g, 12));
    // a hitch: one sample jumps 6 cells with the flag on -> the skipped cells are stamped, counted once
    FmReset(&g); t = 1;
    FmSampleEx(&g, t, 0x40, 0, 0, 0); FmMarkNoDmg(&g, t, 0); t += 6 * f;
    FmSampleEx(&g, t, 0x40, 0, 0, 0); FmMarkNoDmg(&g, t, 1);
    FmMarkNoDmg(&g, t, 1);  // same cell again: no double count
    assert(g.nd == 6);
    // the ring reuses slots: an old stamp must not leak into a new cell
    FmReset(&g); t = 1;
    for (int i = 0; i < FM_CELLS + 50; ++i, t += f) { FmSampleEx(&g, t, 0x50 + (i >= FM_CELLS), 0, 0, 0); FmMarkNoDmg(&g, t, i < 20); }
    assert(g.nd == 0 && !FmLiveRun(&g));
}
int main(void) {
    NoDmgTests();
    { FmMeter g; FmReset(&g);
      FmSampleEx(&g,0,1,0,0,0);
      FmSampleEx(&g,20,1,1,1,1);
      assert((g.cell[1]&0x7F)==FM_GRAB && g.cur.hits==1);
      FmSampleEx(&g,21,1,1,0,0); // simultaneous normal collider cannot hide grab
      assert((g.cell[1]&0x7F)==FM_GRAB);
      FmSampleEx(&g,40,1,0,0,0);
      assert((g.cell[2]&0x7F)==FM_RECOVERY);
      FmSampleEx(&g,60,2,0,0,0);
      assert(g.haveLast && (g.frozen[1]&0x7F)==FM_GRAB);
      FmReset(&g); FmSampleEx(&g,0,1,1,1,0);
      assert((g.cell[0]&0x7F)==FM_ACTIVE_UB);
    }

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
    // Ki window (#34558): KiTick K+0x74 closed at <=0 and above 31, open from just above 0 through 31.
    { float bad; memset(&bad, 0xFF, sizeof bad);
      assert(!FmKiWindowOpen(-1.f) && !FmKiWindowOpen(0.f) && !FmKiWindowOpen(0.05f) && !FmKiWindowOpen(31.1f) && !FmKiWindowOpen(bad));
      assert(FmKiWindowOpen(0.06f) && FmKiWindowOpen(30.f) && FmKiWindowOpen(31.f));
      assert(FmKiPerfect(0.06f) && FmKiPerfect(2.31f) && FmKiPerfect(4.f));
      assert(FmKiPressPerfect(1, 1, 3.96f) && !FmKiPressPerfect(1, 0, 3.96f) && !FmKiPressPerfect(0, 1, 3.96f) && !FmKiPressPerfect(1, 1, 15.f));
      assert(!FmKiPerfect(0.f) && !FmKiPerfect(0.05f) && !FmKiPerfect(4.01f) && !FmKiPerfect(15.47f) && !FmKiPerfect(bad));
      assert(FmScope(1, 1, 1) == 1 && FmScope(0, 1, 0) == 2 && FmScope(1, 0, 0) == 0 && FmScope(0, 0, 1) == 0);
      assert(FmKiPulseGrade(3.5f, -1.f, 1019.5f, 1073.4f, 1.f, 5.f) == 2);
      assert(FmKiPulseGrade(15.5f, -1.f, 1019.5f, 1073.4f, 1.f, 5.f) == 1);
      assert(FmKiPulseGrade(0.f, -1.f, 1000.f, 1153.f, 1.f, 0.f) == 0); }
    g_fm_ki_perfect_only = 0;  // whole-bar mode; the default (1, #37066) draws only the perfect end, checked below
    FmReset(&m); t = 0;
    FmSample(&m, t, 1, 1, 0); FmMarkKiWin(&m, t, 1); assert(FmViewKiWin(&m, 0) == 1);
    for (int i = 0; i < 20; ++i) { t += 1000.0 / 60.0; FmSample(&m, t, 1, 0, 0); FmMarkKiWin(&m, t, 0); }
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) n += FmViewKiWin(&m, i); assert(n == 1); }
    for (int i = 0; i < 30; ++i) { t += 1000.0 / 60.0; FmSample(&m, t, 1, 0, 0); FmMarkKiWin(&m, t, 1); }
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) n += FmViewKiWin(&m, i); assert(n >= 30 && n <= 32); }
    FmSample(&m, t + 1000.0 / 60.0, 2, 0, 0); assert(m.haveLast);
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) n += FmViewKiWin(&m, i); assert(n >= 30 && n <= 32); }
    // #36098: the perfect end of the bar is mark 2, and it stays after the action ends.
    FmReset(&m); t = 0;
    FmSample(&m, t, 1, 1, 0); FmMarkKiWin(&m, t, 1);
    for (int i = 0; i < 4; ++i) { t += 1000.0 / 60.0; FmSample(&m, t, 1, 0, 0); FmMarkKiWin(&m, t, 2); }
    g_fm_ki_perfect_only = 1; { int n = 0; for (int i = 0; i < FM_CELLS; ++i) if (FmViewKiWin(&m, i) == 1) n++; assert(n == 0); } g_fm_ki_perfect_only = 0;
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) if (FmViewKiWin(&m, i) == 2) n++; assert(n == 4); }
    FmSample(&m, t + 1000.0 / 60.0, 2, 0, 0); assert(m.haveLast);
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) if (FmViewKiWin(&m, i) == 2) n++; assert(n == 4); }
    // #36326: the perfect timer is read on the tick the action changes, after the bar already froze blue.
    FmReset(&m); t = 0;
    FmSample(&m, t, 1, 1, 0); FmMarkKiWin(&m, t, 1);
    for (int i = 0; i < 25; ++i) { t += 1000.0 / 60.0; FmSample(&m, t, 1, 0, 0); FmMarkKiWin(&m, t, 1); }
    FmSample(&m, t + 1000.0 / 60.0, 2, 0, 0); // freeze first, the way the game tick does
    assert(m.haveLast);
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) if (FmViewKiWin(&m, i) == 2) n++; assert(n == 0); }
    FmSealPerfectEnd(&m);
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) if (FmViewKiWin(&m, i) == 2) n++; assert(n == 1); }
    { FmMeter k; memset(&k,0,sizeof k); k.kiwinFrozenN=3; k.kipulseFrozenN=3; k.haveLast=1; k.kiwinFrozen[1]=1; k.kiwinFrozen[2]=1;
      assert(FmSealPerfectCheck(&k) && k.kipulseFrozen[2]==1 && k.kipulseFrozen[0]==0); }
    // Landed Ki pulse (#34866): +0x5C near 54 jumping by 40 or more, with Ki moving by 20 or more.
    assert(FmKiPulseLanded(53.94f, 133.43f, 1019.5f, 921.1f));
    assert(!FmKiPulseLanded(-1.f, 53.94f, 1153.f, 1019.5f));
    assert(!FmKiPulseLanded(53.94f, 53.94f, 1019.5f, 1016.f));
    { float bad; memset(&bad, 0xFF, sizeof bad); assert(!FmKiPulseLanded(bad, 133.f, 1000.f, 900.f)); }
    { FmPulseArm a; int hit; memset(&a, 0, sizeof a);
      hit = FmKiPulseStep(&a, -1.f, 1153.f); assert(!hit && !a.have);
      hit = FmKiPulseStep(&a, 53.94f, 1019.5f); assert(!hit && a.have);
      for (int i = 0; i < 20; ++i) assert(!FmKiPulseStep(&a, 53.94f, 1019.5f));
      assert(!FmKiPulseStep(&a, 53.94f, 921.1f));
      assert(FmKiPulseStep(&a, 133.43f, 921.1f));
      assert(!FmKiPulseStep(&a, 133.43f, 921.1f)); }
    FmReset(&m); t = 0;
    // v2: the second combo hit (Ki down 98, window not open) is not a land.
    assert(!FmKiPulseLanded2(-1.f, -1.f, 53.94f, 1019.5f, 921.1f));
    assert(!FmKiPulseLanded2(20.f, 19.f, 53.94f, 1019.5f, 921.1f));
    // a vpad miss from log #2301 (18:10:58): window counts down, pool drains ~3.5 per sample, closes from 2.1 -> 0
    { FmPulseWatch w; memset(&w, 0, sizeof w);
      static const float t74[] = {30.f, 25.f, 20.f, 15.f, 12.f, 9.f, 6.f, 4.f, 2.1f, 0.f, 0.f};
      static const float pool[] = {53.9f, 53.9f, 50.4f, 46.9f, 43.6f, 33.7f, 23.0f, 12.4f, 2.1f, 0.f, 0.f};
      static const float ki[] = {1019.5f, 1019.5f, 1023.1f, 1026.5f, 1029.8f, 1039.7f, 1050.4f, 1061.0f, 1071.3f, 1075.0f, 1092.7f};
      for (int i = 0; i < 11; ++i) assert(!FmKiPulseStep2(&w, t74[i], pool[i], ki[i])); }
    // a pulse: window still at 18, Ki jumps by the pool at once and the window ends early
    { FmPulseWatch w; memset(&w, 0, sizeof w);
      assert(!FmKiPulseStep2(&w, 18.f, 53.9f, 1019.5f));
      assert(FmKiPulseStep2(&w, 0.f, 0.f, 1080.f));
      assert(!FmKiPulseStep2(&w, 0.f, 0.f, 1090.f)); }
    // window closed early with a small Ki rise still counts; a NaN never does
    assert(FmKiPulseLanded2(12.f, 0.f, 4.f, 1060.f, 1066.f));
    { float bad; memset(&bad, 0xFF, sizeof bad); assert(!FmKiPulseLanded2(bad, 0.f, 50.f, 1000.f, 1060.f)); }
    FmSample(&m, t, 1, 1, 0); FmMarkKiPulse(&m, t, 0);
    t += 1000.0 / 60.0; FmSample(&m, t, 1, 0, 0); FmMarkKiPulse(&m, t, 1);
    t += 1000.0 / 60.0; FmSample(&m, t, 1, 0, 0); FmMarkKiPulse(&m, t, 0);
    { int n = 0, at = -1; for (int i = 0; i < FM_CELLS; ++i) if (FmViewKiPulse(&m, i)) { n++; at = i; } assert(n == 1 && at == 1); }
    FmSample(&m, t + 1000.0 / 60.0, 2, 0, 0);
    { int n = 0; for (int i = 0; i < FM_CELLS; ++i) n += FmViewKiPulse(&m, i); assert(n == 1); }
    puts("frame_meter_test: all passed");
    return 0;
}
