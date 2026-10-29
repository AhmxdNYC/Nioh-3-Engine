// user #22589 / #22592: Practice freeze + frame advance state machine (practice_freeze.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/practice_freeze_test.c -o build/practice_freeze_test.exe && build/practice_freeze_test.exe
// Simulates the engine: a stop counter + the edge latch 89B6EC + FrameDtSetup's "flag set -> time scale 0", one pump per frame.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#undef NDEBUG
#include <assert.h>
#include "../src/practice_freeze.h"
typedef struct { int counter, prev, flag, over; double clock; FzState s; int nativePause; } Sim;
// One frame at fps-for-dt fps: latch (+ our override), time scale, pump. Returns the game ms this frame ran.
static double Frame(Sim* m, double fps, int reason, int* done) {
    if (m->prev == 0 && m->counter != 0) m->flag = 1; else if (m->prev != 0 && m->counter == 0) m->flag = 0;  // native latch (edge)
    m->prev = m->counter;
    const int run = FzLatch(&m->s, m->nativePause);
    if (run || m->over) m->flag = FzFlag(m->counter != 0, run);
    m->over = run;
    const double d = (m->flag || m->nativePause) ? 0.0 : 1000.0 / fps;
    m->clock += d;
    const int a = FzPump(&m->s, reason, m->clock, d, done);
    if (a == FZ_HOLD) m->counter++;
    else if (a == FZ_RELEASE && m->counter > 0) m->counter--;
    return d;
}
static int StepOnce(Sim* m, double fps, int* framesRun) {
    int done = 0, n = 0; *framesRun = 0;
    assert(FzRequestStep(&m->s) == 1);
    for (n = 0; n < 20 && !done; ++n) if (Frame(m, fps, FZR_OK, &done) > 0) (*framesRun)++;
    assert(done);
    for (int i = 0; i < 5; ++i) { const double d = Frame(m, fps, FZR_OK, &done); if (d != 0.0 || done) { printf("fps %.0f i %d d %f done %d flag %d cnt %d open %d q %d\n", fps, i, d, done, m->flag, m->counter, m->s.open, m->s.queued); fflush(stdout); } assert(d == 0.0 && !done); }  // re-frozen, world static
    return m->s.lastAdv;
}
static void Freeze(Sim* m, double fps) { int done; FzRequest(&m->s, 1); for (int i = 0; i < 3; ++i) Frame(m, fps, FZR_OK, &done); assert(m->s.held && m->flag); }
int main(void) {
    int done, fr;
    // 1) freeze stops time; the frame after the hold is the last one that runs
    {   Sim m; memset(&m, 0, sizeof m); FzReset(&m.s);
        for (int i = 0; i < 50; ++i) Frame(&m, 100, FZR_OK, &done);
        FzRequest(&m.s, 1);
        Frame(&m, 100, FZR_OK, &done); assert(m.s.held && m.counter == 1);
        const double c = m.clock;
        for (int i = 0; i < 200; ++i) assert(Frame(&m, 100, FZR_OK, &done) == 0.0);
        assert(m.clock == c && FzFrames(&m.s) == 0);
        // 2) steps at ~100 fps (tick 0.6 cell): every press moves the meter exactly one cell
        for (int k = 1; k <= 40; ++k) { assert(StepOnce(&m, 100, &fr) == 1 && (fr == 1 || fr == 2)); assert(FzFrames(&m.s) == k); }
        // unfreeze: time runs again, the counter is back to 0, flag cleared by the native latch
        FzRequest(&m.s, 0); Frame(&m, 100, FZR_OK, &done); assert(!m.s.held && m.counter == 0);
        assert(Frame(&m, 100, FZR_OK, &done) > 0 && m.flag == 0);
    }
    // 3) 120 fps (2 ticks per cell), 60 fps (1 tick), VRR-like varying fps
    {   Sim m; memset(&m, 0, sizeof m); FzReset(&m.s); m.clock = 12345.6; Freeze(&m, 120);
        for (int k = 1; k <= 30; ++k) { assert(StepOnce(&m, 120, &fr) == 1 && fr >= 1 && fr <= 3); assert(FzFrames(&m.s) == k); }
        for (int k = 31; k <= 60; ++k) { assert(StepOnce(&m, 60, &fr) == 1 && fr == 1); assert(FzFrames(&m.s) == k); }
        const double fpsv[] = {87, 93, 101, 110, 116, 95};
        for (int k = 61; k <= 120; ++k) { assert(StepOnce(&m, fpsv[k % 6], &fr) == 1); assert(FzFrames(&m.s) == k); }
        // below 60 fps one engine tick covers 2 cells: the step reports it
        assert(StepOnce(&m, 30, &fr) == 2 && fr == 1);
    }
    // 4) another holder (menu pause) on the same counter: stepping still works, and the flag is never left set with the counter at 0
    {   Sim m; memset(&m, 0, sizeof m); FzReset(&m.s); Freeze(&m, 100); m.counter++;  // menu opened
        assert(StepOnce(&m, 100, &fr) == 1);
        m.counter--;  // menu closed
        FzRequest(&m.s, 0); for (int i = 0; i < 3; ++i) Frame(&m, 100, FZR_OK, &done);
        assert(m.counter == 0 && m.flag == 0 && Frame(&m, 100, FZR_OK, &done) > 0);
    }
    // 5) unfreeze in the middle of an open step
    {   Sim m; memset(&m, 0, sizeof m); FzReset(&m.s); Freeze(&m, 200);
        FzRequestStep(&m.s); Frame(&m, 200, FZR_OK, &done); assert(m.s.open && !done);
        FzRequest(&m.s, 0); Frame(&m, 200, FZR_OK, &done); Frame(&m, 200, FZR_OK, &done);
        assert(!m.s.held && !m.s.open && m.counter == 0 && m.flag == 0);
    }
    // 6) safety: any reason releases the hold at once and drops the queue
    for (int r = FZR_NOT_LIVE; r <= FZR_TOO_LONG; ++r) {
        Sim m; memset(&m, 0, sizeof m); FzReset(&m.s); Freeze(&m, 100);
        FzRequestStep(&m.s); FzRequestStep(&m.s);
        Frame(&m, 100, r, &done); assert(!m.s.held && !m.s.want && !m.s.queued && m.counter == 0);
        Frame(&m, 100, FZR_OK, &done); assert(m.flag == 0 && Frame(&m, 100, FZR_OK, &done) > 0 && !m.s.held);  // never re-takes on its own
    }
    // 7) FzSafeReason priorities
    assert(FzSafeReason(1, 0, 1, 0, 1, 1000) == FZR_OK);
    assert(FzSafeReason(0, 1, 0, 1, 0, 0) == FZR_NOT_LIVE && FzSafeReason(1, 1, 0, 0, 1, 0) == FZR_BUSY);
    assert(FzSafeReason(1, 0, 0, 1, 1, 0) == FZR_BOSS_GONE && FzSafeReason(1, 0, 1, 1, 1, 0) == FZR_BOSS_DEAD);
    assert(FzSafeReason(1, 0, 1, 0, 0, 0) == FZR_PLAYER_CHANGED && FzSafeReason(1, 0, 1, 0, 1, FZ_MAX_HOLD_MS + 1) == FZR_TOO_LONG);
    // 8) native pause (G+0x58C) up: a step waits and then aborts, no time passes, state stays frozen
    {   Sim m; memset(&m, 0, sizeof m); FzReset(&m.s); Freeze(&m, 100); FzRequestStep(&m.s);
        m.nativePause = 1; const double c = m.clock;
        for (int i = 0; i < FZ_STALL_TICKS + 10; ++i) Frame(&m, 100, FZR_OK, &done);
        assert(m.clock == c && m.s.held && m.s.queued == 1);  // not opened while the native pause holds
        m.nativePause = 0; const int f0 = FzFrames(&m.s); done = 0; for (int i = 0; i < 5 && !done; ++i) Frame(&m, 100, FZR_OK, &done);
        assert(done && FzFrames(&m.s) == f0 + 1 && m.s.queued == 0);  // the queued step runs once the pause is gone
        for (int i = 0; i < 5; ++i) assert(Frame(&m, 100, FZR_OK, &done) == 0.0);
    }
    // 9) step while not frozen = freeze; queue cap; toggle
    {   FzState s; FzReset(&s); assert(FzRequestStep(&s) == 0 && s.want == 1 && s.queued == 0);
        for (int i = 0; i < 10; ++i) FzRequestStep(&s);
        assert(s.queued == FZ_MAX_QUEUE);
        FzRequest(&s, -1); assert(!s.want && !s.queued); FzRequest(&s, -1); assert(s.want);
        assert(FzFlag(0, 0) == 0 && FzFlag(0, 1) == 0 && FzFlag(1, 0) == 1 && FzFlag(1, 1) == 0);
    }
    puts("practice_freeze_test: all passed");
    return 0;
}
