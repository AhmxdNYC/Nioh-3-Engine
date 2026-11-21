// #127933 player log game clock (player_clock.h): frames counted once per engine update, ratio, stalls, nostep.
#include <stdio.h>
#include <math.h>
#undef NDEBUG  // keep asserts in -O2 builds (zig cc -O2 defines NDEBUG)
#include <assert.h>
#include "../src/player_clock.h"

static int near(double a, double b, double e) { return fabs(a - b) <= e; }

int main(void) {
    PlClk c = {0}; PlClkLine l;
    // 120 fps, 2 pump ticks per frame (the second must not count), step = 60/120 = 0.5
    double t = 0; uint32_t n = 100; int lines = 0;
    assert(!PlClkSample(&c, t, n, 0.5f, 1000.0 / 120.0, &l));
    for (int i = 0; i < 130; ++i) {
        t += 1000.0 / 120.0; ++n;
        lines += PlClkSample(&c, t, n, 0.5f, 1000.0 / 120.0, &l);
        if (lines) break;
        assert(!PlClkSample(&c, t + 0.1, n, 0.5f, 1000.0 / 120.0, &l));  // same frame, second pump tick
    }
    assert(lines == 1);
    assert(near(l.ratio, 1.0, 0.01) && near(l.engRatio, 1.0, 0.01));
    assert(l.fps == 120 && l.frames == 120 && l.nostep == 0 && l.stalls == 0);
    assert(near(l.worst, 1000.0 / 120.0, 0.01));
    assert(c.fps == 120 && near(c.frameMs, 8.33, 0.01));

    // slow motion: 60 fps real but the game steps only 0.5 (half speed) -> ratio 0.5; one 40 ms hitch -> worst 40
    lines = 0;
    for (int i = 0; i < 70 && !lines; ++i) {
        const double f = i == 10 ? 40.0 : 1000.0 / 60.0;
        t += f; ++n;
        lines = PlClkSample(&c, t, n, 0.5f, 1000.0 / 120.0, &l);
    }
    assert(lines == 1);
    assert(near(l.ratio, 0.5, 0.02));
    assert(near(l.worst, 40.0, 0.01));
    assert(l.fps >= 57 && l.fps <= 60);

    // missed pump: counter moved by 2 in one sample -> 2 frames, frame ms = span / 2, game = 2 steps
    PlClk d = {0};
    PlClkSample(&d, 0, 5, 1.0f, 1000.0 / 60.0, &l);
    assert(!PlClkSample(&d, 33.4, 7, 1.0f, 1000.0 / 60.0, &l));
    assert(d.frames == 2 && near(d.frameMs, 16.7, 0.01) && near(d.game, 33.33, 0.01));

    // no player step (menu): counted as nostep, game 0
    assert(!PlClkSample(&d, 50.1, 8, -1.0f, 1000.0 / 60.0, &l));
    assert(d.nostep == 1 && d.frames == 3);

    // load: counter jumps by 500 -> window re-seeded, stall counted, nothing summed
    assert(!PlClkSample(&d, 3000.0, 508, 1.0f, 1000.0 / 60.0, &l));
    assert(d.frames == 0 && d.game == 0 && d.stalls == 1 && d.w0 == 3000.0);
    // 2 s without a frame then one frame: also a stall (span > 1 s)
    assert(!PlClkSample(&d, 5000.0, 509, 1.0f, 1000.0 / 60.0, &l));
    assert(d.stalls == 2 && d.frames == 0);  // re-seeded again; the window keeps counting stalls
    // counter wraps (uint32): 0xFFFFFFFF -> 1 = 2 frames
    PlClk w = {0};
    PlClkSample(&w, 0, 0xFFFFFFFFu, 1.0f, -1.0, &l);
    assert(!PlClkSample(&w, 33.4, 1u, 1.0f, -1.0, &l));
    assert(w.frames == 2 && w.eng == 0);
    printf("player_clock_test PASS\n");
    return 0;
}
