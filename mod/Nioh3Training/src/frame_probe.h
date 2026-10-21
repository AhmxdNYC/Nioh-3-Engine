// user #22356 / #22310: frame probe (log only). Pure bookkeeping, no game access (tests/frame_probe_test.c).
// Game glue: frame_probe_game.h. One "run" = one boss action from its start to the next action id.
//   startup  = ms (and 60 fps frames) from the action start to the first hit-record creation (= first hit window opening)
//   active   = first creation .. end of the last window (pool entry of the attack collision released), or .. last creation if no end was seen
//   recovery = action end - end of the last window
#pragma once
#include <stdint.h>
#define FP_MAX_HITS 8
typedef struct {
    uint32_t act;          // action id (FFFFFFFF = none)
    double t0;             // action start, ms (QPC)
    uint32_t f0;           // pump frame at the start
    int hits;              // hit-record creations during the run
    double hit[FP_MAX_HITS], end[FP_MAX_HITS];  // ms since t0 of each creation / its window end (-1 = not seen)
    int great;             // any creation had hit +0x13C bit 10 (GreatAttack)
} FpRun;
static inline double FpFrames60(double ms) { return ms * 0.06; }
static inline void FpRunStart(FpRun* r, uint32_t act, double t, uint32_t f) {
    r->act = act; r->t0 = t; r->f0 = f; r->hits = 0; r->great = 0;
    for (int i = 0; i < FP_MAX_HITS; ++i) r->hit[i] = r->end[i] = -1.0;
}
// Returns the hit index (0-based) or -1 when the run is full.
static inline int FpRunHit(FpRun* r, double t, int great) {
    if (great) r->great = 1;
    if (r->hits >= FP_MAX_HITS) { r->hits++; return -1; }
    r->hit[r->hits] = t - r->t0; return r->hits++;
}
static inline void FpRunHitEnd(FpRun* r, int idx, double t) { if (idx >= 0 && idx < FP_MAX_HITS && idx < r->hits && r->end[idx] < 0) r->end[idx] = t - r->t0; }
// Summary numbers of a finished run (ms). Returns 0 when the run had no hit.
static inline int FpRunSummary(const FpRun* r, double tEnd, double* ran, double* startup, double* active, double* recovery) {
    *ran = tEnd - r->t0; *startup = *active = *recovery = -1.0;
    if (r->hits <= 0) return 0;
    const int n = r->hits < FP_MAX_HITS ? r->hits : FP_MAX_HITS;
    double last = r->hit[n - 1];
    for (int i = 0; i < n; ++i) if (r->end[i] > last) last = r->end[i];
    *startup = r->hit[0]; *active = last - r->hit[0]; *recovery = *ran - last;
    return 1;
}
// Token bucket: at most `perSec` lines per second, refilled continuously. Returns 1 when a line may be written.
typedef struct { double tokens, t; } FpRate;
static inline int FpRateOk(FpRate* b, double nowMs, double perSec) {
    if (b->t <= 0) { b->t = nowMs; b->tokens = perSec; }
    b->tokens += (nowMs - b->t) * perSec / 1000.0; b->t = nowMs;
    if (b->tokens > perSec) b->tokens = perSec;
    if (b->tokens < 1.0) return 0;
    b->tokens -= 1.0; return 1;
}
