// user #22356 / #22310: frame probe (log only). Pure bookkeeping, no game access (tests/frame_probe_test.c).
// Game glue: frame_probe_game.h. One "run" = one boss action from its start to the next action id.
//   startup  = ms (and 60 fps frames) from the action start to the first hit-record creation (= first hit window opening)
//   active   = first creation .. end of the last window (pool entry of the attack collision released), or .. last creation if no end was seen
//   recovery = action end - end of the last window
#pragma once
#include <stdint.h>
#define FP_MAX_HITS 32  // merged hit windows kept per run (0x4E6E has 2; one more creation after the last kept window joins that window)
#define FP_TOL_MS 17.0  // a creation within one 60 fps frame of the last window's end (or while one of its colliders is open) joins that window
// user #22575: one entry = one MERGED hit window, not one collider. The game makes 8 colliders per 0x4E6E swing at 60-120 fps but 84-108 at a
// 30 fps cap (new colliders on each 33 ms step), which overflowed the old per-collider list (32) and lost the second hit. Merging here keeps the
// numbers the same at every frame rate (tools/frame_probe_summary.py merges the same way).
typedef struct {
    uint32_t act;          // action id (FFFFFFFF = none)
    double t0;             // action start, ms (game clock since #22575; the wall copy runs on QPC)
    uint32_t f0;           // pump frame at the start
    int hits;              // merged hit windows
    int colliders;         // hit-record creations during the run
    double hit[FP_MAX_HITS], end[FP_MAX_HITS];  // ms since t0: window open / latest end of its colliders (-1 = not seen)
    int open[FP_MAX_HITS]; // colliders of the window still in their pool slot (tracked ones only)
    int great;             // any creation had hit +0x13C bit 10 (GreatAttack)
} FpRun;
static inline double FpFrames60(double ms) { return ms * 0.06; }
static inline void FpRunStart(FpRun* r, uint32_t act, double t, uint32_t f) {
    r->act = act; r->t0 = t; r->f0 = f; r->hits = 0; r->colliders = 0; r->great = 0;
    for (int i = 0; i < FP_MAX_HITS; ++i) { r->hit[i] = r->end[i] = -1.0; r->open[i] = 0; }
}
// One collider creation at t. tracked = its end will be reported by FpRunHitEnd. force >= 0: use that window (the wall-clock copy follows the
// game-clock decision). Returns the window index (0-based); *isNew = 1 when it opened a new window.
static inline int FpRunHitEx(FpRun* r, double t, int great, int tracked, int force, int* isNew) {
    if (great) r->great = 1;
    r->colliders++; const double rel = t - r->t0; int k;
    if (isNew) *isNew = 0;
    if (force >= 0 && force < FP_MAX_HITS) {
        while (r->hits <= force) { r->hit[r->hits] = rel; r->end[r->hits] = -1.0; r->open[r->hits] = 0; r->hits++; if (isNew) *isNew = 1; }
        k = force;
    } else {
        const int last = r->hits - 1;
        const double le = last >= 0 ? (r->end[last] > r->hit[last] ? r->end[last] : r->hit[last]) : 0;
        if (last >= 0 && (r->open[last] > 0 || rel - le <= FP_TOL_MS || r->hits == FP_MAX_HITS)) k = last;
        else { k = r->hits++; r->hit[k] = rel; r->end[k] = -1.0; r->open[k] = 0; if (isNew) *isNew = 1; }
    }
    if (tracked) r->open[k]++;
    return k;
}
static inline int FpRunHit(FpRun* r, double t, int great) { return FpRunHitEx(r, t, great, 0, -1, 0); }
// A tracked collider of window idx left its pool slot at t: the window end is the latest such end.
static inline void FpRunHitEnd(FpRun* r, int idx, double t) {
    if (idx < 0 || idx >= FP_MAX_HITS || idx >= r->hits) return;
    if (r->open[idx] > 0) r->open[idx]--;
    if (t - r->t0 > r->end[idx]) r->end[idx] = t - r->t0;
}
// Summary numbers of a finished run (ms). Returns 0 when the run had no hit.
static inline int FpRunSummary(const FpRun* r, double tEnd, double* ran, double* startup, double* active, double* recovery) {
    *ran = tEnd - r->t0; *startup = *active = *recovery = -1.0;
    if (r->hits <= 0) return 0;
    const int n = r->hits;
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
