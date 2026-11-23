// human #127918 / #127933: game-clock check for the player log (pure, unit-tested in tests/player_clock_test.c).
// One sample per pump tick. A game frame is counted only when the engine update counter G+0x610 (G = [4759910], the
// counter FpGameStep uses) moved: the pump can run more than once per frame, so summing per pump tick would double-count.
// Game time per frame = the player's step float [ctrl+0x40] in 1/60 s units (60/fps, #116908), so 1.0 = 16.67 ms of game
// time. The engine clock (ticks x 1000 x time scale G+0x624 / fps-for-dt G+0x614) is summed next to it as a cross-check.
//   ratio = game ms / wall ms over the window: 1.00 = normal speed; < 0.9 with fps under the cap = the engine slowing time.
#ifndef N3_PLAYER_CLOCK_H
#define N3_PLAYER_CLOCK_H
#include <stdint.h>
#define PLCLK_MAX_STEP 8u  // counter jumps above this (load / long stall) re-seed the window instead of counting
typedef struct {
    int ok;                 // seeded
    uint32_t cnt;           // counter at the last counted frame
    double lastWall;        // wall ms at the last counted frame
    double w0;              // window start (wall ms)
    double game, eng;       // window sums: player step ms, engine clock ms
    double worst;           // longest wall frame (ms) in the window
    uint32_t frames, nostep, stalls;  // frames counted; frames with no player step; re-seeds (load / stall)
    // last finished frame / window, for the PAD / ACT lines
    double frameMs;         // wall ms of the newest frame
    int fps;                // fps of the last finished window (0 = none yet)
} PlClk;
typedef struct { double wall, game, eng, ratio, engRatio, worst; uint32_t frames, nostep, stalls; int fps; } PlClkLine;

static void PlClkWin(PlClk* c, double now) { c->w0 = now; c->game = c->eng = c->worst = 0; c->frames = c->nostep = c->stalls = 0; }
// wallMs = real time now; cnt = engine update counter; step60 = player [ctrl+0x40] (< 0 = unreadable); engMs = engine ms per
// tick (< 0 = unreadable). Returns 1 when a 1 s window finished: *out is filled and a new window starts.
static int PlClkSample(PlClk* c, double wallMs, uint32_t cnt, float step60, double engMs, PlClkLine* out) {
    if (!c->ok) { c->ok = 1; c->cnt = cnt; c->lastWall = wallMs; c->frameMs = 0; c->fps = 0; PlClkWin(c, wallMs); return 0; }
    const uint32_t k = cnt - c->cnt;
    if (k == 0) return 0;  // same frame: another pump tick
    const double span = wallMs - c->lastWall;
    c->cnt = cnt; c->lastWall = wallMs;
    if (k > PLCLK_MAX_STEP || span < 0 || span > 1000.0) {  // load / stall: neither clock is comparable, start over
        const uint32_t st = c->stalls + 1;
        PlClkWin(c, wallMs); c->stalls = st; return 0;
    }
    const double f = span / (double)k;
    c->frameMs = f;
    if (f > c->worst) c->worst = f;
    c->frames += k;
    if (step60 >= 0.0f && step60 < 16.0f) c->game += (double)step60 * (double)k * (1000.0 / 60.0); else c->nostep += k;
    if (engMs >= 0) c->eng += engMs * (double)k;
    const double wall = wallMs - c->w0;
    if (wall < 1000.0) return 0;
    out->wall = wall; out->game = c->game; out->eng = c->eng; out->worst = c->worst;
    out->frames = c->frames; out->nostep = c->nostep; out->stalls = c->stalls;
    out->ratio = c->game / wall; out->engRatio = c->eng / wall;
    out->fps = (int)((double)c->frames * 1000.0 / wall + 0.5);
    c->fps = out->fps;
    PlClkWin(c, wallMs);
    return 1;
}
#endif
