// user #22589 / #22592: Practice FREEZE + FRAME-ADVANCE (fighting-game training-mode style). Pure decisions, no game access
// (tests/practice_freeze_test.c). Game glue: practice_freeze_game.h. Menu rows: Boss Rematch practice_stats_menu.h (Practice hub).
// Mechanism (the existing F9 freeze, #9595 / #9658): engine stop request #3 (counter G+0x59C, G = [base+4759910]). FrameDtSetup 5AD5F8 calls the
// latch 89B6EC(G+0x588) at the start of every frame: flag G+0x58B goes 1 on the counter's 0 -> non-0 edge and 0 on non-0 -> 0, and the same function
// then zeroes the game time scale G+0x624 while G+0x58B is set (5AD6C5). Logic and render keep running, game time does not advance.
// Step: the latch call 5AD64A is chained; while a step is OPEN the flag G+0x58B is cleared right after the latch (at frame start, before the time
// scale is computed), so that whole frame runs at the normal time scale; the first frame after the step closes gets the flag back (= counter != 0).
// One step = advance until the frame meter's game clock (ms, sum of ticks x 1000 x G+0x624 / G+0x614, the same clock as the meter) reaches the
// NEXT meter cell (1 cell = 1/60 s of game time): (int64)(clock * 0.06) + 1. At an fps-for-dt above 60 an engine tick is shorter than one cell, so
// every step moves the meter by exactly one cell (1 or 2 engine ticks at ~100 fps); at 60 one tick = one cell; below 60 one tick is longer than a
// cell and the step moves by the cells that tick covers (reported, never hidden).
// Fail closed: the hold is dropped (and nothing is queued) whenever FzSafeReason() gives a reason: session ended / no Training boss, loading or
// a mod transition (MenuPauseBusy), the boss we froze despawned or died, the player character changed (mission change), the hold is older than
// FZ_MAX_HOLD_MS, the native pause (G+0x58C) or the pump stopped (watchdog in practice_freeze_game.h).
#pragma once
#include <stdint.h>
#define FZ_STALL_TICKS 90           // an open step that sees no game time for this many pump ticks is aborted (another pause holds time)
#define FZ_MAX_QUEUE 3              // steps waiting at most (hold-to-repeat never builds a backlog)
#define FZ_MAX_HOLD_MS (20 * 60 * 1000)
enum { FZ_NONE = 0, FZ_HOLD, FZ_RELEASE };
enum { FZR_OK = 0, FZR_NOT_LIVE, FZR_BUSY, FZR_BOSS_GONE, FZR_BOSS_DEAD, FZR_PLAYER_CHANGED, FZR_TOO_LONG };
typedef struct {
    int want;        // the user asked for the freeze
    int held;        // our stop request is held (mirrors the glue's counter flag)
    int queued;      // steps waiting for a frame start
    int open;        // a step is open: the latch clears G+0x58B at frame start
    int64_t cell0;   // meter cell when the freeze took hold
    int64_t cell;    // newest cell seen
    int64_t target;  // the open step ends when cell >= target
    int stall;       // pump ticks of the open step without game time
    int ticks;       // engine ticks with game time in the open / last step
    int steps;       // steps completed since the freeze took hold
    int lastAdv;     // cells the last completed step moved (1 except below 60 fps)
    int aborted;     // steps aborted (stall)
} FzState;
static inline void FzReset(FzState* s) { for (int i = 0; i < (int)sizeof *s; ++i) ((uint8_t*)s)[i] = 0; }
static inline int64_t FzCell(double ms) { return ms <= 0 ? 0 : (int64_t)(ms * 0.06); }  // = FmCellOf (frame_meter.h)
static inline const char* FzReasonText(int r) {
    return r == FZR_NOT_LIVE ? "Practice session ended / no Training boss" : r == FZR_BUSY ? "load / transition started" : r == FZR_BOSS_GONE ? "boss despawned"
         : r == FZR_BOSS_DEAD ? "boss died" : r == FZR_PLAYER_CHANGED ? "player changed (mission change)" : r == FZR_TOO_LONG ? "held too long" : "ok";
}
// Safety: first reason that forbids holding (FZR_OK = holding is allowed). bossSame = every boss present when the freeze took hold is still the same actor.
static inline int FzSafeReason(int live, int busy, int bossSame, int bossDead, int playerSame, uint32_t heldMs) {
    if (!live) return FZR_NOT_LIVE;
    if (busy) return FZR_BUSY;
    if (!bossSame) return FZR_BOSS_GONE;
    if (bossDead) return FZR_BOSS_DEAD;
    if (!playerSame) return FZR_PLAYER_CHANGED;
    if (heldMs > FZ_MAX_HOLD_MS) return FZR_TOO_LONG;
    return FZR_OK;
}
// User requests (menu row / hotkey), applied by the pump. op: 0 off, 1 on, -1 toggle. A step request while not frozen freezes (no step).
static inline void FzRequest(FzState* s, int op) { s->want = op < 0 ? !s->want : !!op; if (!s->want) s->queued = 0; }
static inline int FzRequestStep(FzState* s) {
    if (!s->want) { s->want = 1; return 0; }
    if (s->queued < FZ_MAX_QUEUE) { s->queued++; return 1; }
    return 0;
}
// Pump (game thread, once per frame, after the frame meter clock advanced). reason = FzSafeReason(..). clockMs = the meter clock now,
// dMs = game time this tick. Returns FZ_HOLD / FZ_RELEASE (the glue takes / drops the stop request) or FZ_NONE.
// *stepDone = 1 when a step completed on this tick (s->lastAdv cells).
static inline int FzPump(FzState* s, int reason, double clockMs, double dMs, int* stepDone) {
    *stepDone = 0;
    const int64_t c = FzCell(clockMs);
    if (reason != FZR_OK) { s->want = 0; s->queued = 0; s->open = 0; if (s->held) { s->held = 0; return FZ_RELEASE; } return FZ_NONE; }
    if (s->want && !s->held) { s->held = 1; s->cell0 = s->cell = c; s->steps = s->lastAdv = s->aborted = 0; s->open = 0; s->queued = 0; return FZ_HOLD; }
    if (!s->want && s->held) { s->held = 0; s->open = 0; s->queued = 0; return FZ_RELEASE; }
    if (!s->held) { s->open = 0; s->queued = 0; return FZ_NONE; }
    if (s->open) {
        if (dMs > 0) { s->ticks++; s->stall = 0; } else s->stall++;
        if (c >= s->target) { s->open = 0; s->steps++; s->lastAdv = (int)(c - s->cell); *stepDone = 1; s->cell = c; }
        else if (s->stall > FZ_STALL_TICKS) { s->open = 0; s->aborted++; }
    } else s->cell = c;  // frozen: the clock does not move (kept current anyway)
    return FZ_NONE;
}
// Latch (game thread, frame start, right after the native latch). nativePause = G+0x58C set (the game's own pause: never stepped through).
// Opens the next queued step. Returns 1 when this frame must run (G+0x58B cleared for it).
static inline int FzLatch(FzState* s, int nativePause) {
    if (!s->held) { s->open = 0; return 0; }
    if (nativePause) return 0;  // an open step waits (and stalls out) while the game's own pause is up
    if (!s->open && s->queued > 0) { s->queued--; s->open = 1; s->target = s->cell + 1; s->stall = 0; s->ticks = 0; }
    return s->open;
}
// The stop flag G+0x58B to write after the native latch when we override it (counterNonZero = G+0x59C != 0 now): always the counter's own truth,
// except during an open step. Never 1 with the counter at 0 (the latch would then never clear it: stuck freeze).
static inline int FzFlag(int counterNonZero, int runThisFrame) { return counterNonZero && !runThisFrame; }
// Frames advanced since the freeze took hold (the "+N" in the meter header).
static inline int FzFrames(const FzState* s) { return s->held ? (int)(s->cell - s->cell0) : 0; }
