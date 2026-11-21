// human #127076 (TASK #127085): style meter pop-ups + motion. Pure (no Win32), shared by the game draw thread (style_meter_draw.h) and
// tests/style_meter_test.c / tests/style_meter_demo.c. Times are wall-clock ms (the draw thread has no game clock).
//   pop-ups  one '<Bonus> +X' line per scoring bonus under the rank, newest on top, stacking (max SM_POPS); the same bonus again within
//            SM_POP_MERGE_MS adds to the line (no spam). Each line pops in (SM_POP_IN_MS), holds, then fades; gone after its life.
//   motion   rank-up scale bounce, small shake on a big gain, glow pulse at S and up, eased gauge fill.
#ifndef STYLE_METER_FX_H
#define STYLE_METER_FX_H
#include <stdint.h>
#include <string.h>
#include <math.h>

#define SM_POPS 5
#define SM_POP_LIFE_MS 1200     // [StyleMeter] PopUpMs: total life (hold 40%, fade the rest)
#define SM_POP_IN_MS 120
#define SM_POP_MERGE_MS 400
#define SM_BOUNCE_MS 360
#define SM_SHAKE_MS 260
#define SM_SHAKE_PTS 40         // [StyleMeter] ShakePts: bonus points in one tick that shake the meter
#define SM_GLOW_RANK 4          // S
#define SM_GLOW_PERIOD_MS 1100
#define SM_EASE_TAU_MS 90

typedef struct { char label[24]; float pts; int count; double t0, tLast; } SmPop;  // count: times this bonus merged in (#127166)
typedef struct { SmPop p[SM_POPS]; int n; } SmPopQ;  // p[0] = oldest

static inline void SmPopPrune(SmPopQ* q, double t, int lifeMs) {
    int k = 0;
    for (int i = 0; i < q->n; ++i) if (t - q->p[i].tLast < (double)lifeMs) q->p[k++] = q->p[i];
    q->n = k;
}
static inline void SmPopPush(SmPopQ* q, const char* label, float pts, double t, int lifeMs) {
    SmPopPrune(q, t, lifeMs);
    for (int i = 0; i < q->n; ++i)
        if (!strcmp(q->p[i].label, label) && t - q->p[i].tLast < SM_POP_MERGE_MS) {  // same bonus again: add to it, move it to the top
            SmPop m = q->p[i]; m.pts += pts; m.count++; m.tLast = t;
            for (int j = i; j + 1 < q->n; ++j) q->p[j] = q->p[j + 1];
            q->p[q->n - 1] = m; return;
        }
    if (q->n == SM_POPS) { for (int j = 0; j + 1 < q->n; ++j) q->p[j] = q->p[j + 1]; q->n--; }
    SmPop* p = &q->p[q->n++]; memset(p, 0, sizeof *p);
    strncpy(p->label, label, sizeof p->label - 1); p->pts = pts; p->count = 1; p->t0 = p->tLast = t;
}
// 0..1 opacity of a line last refreshed at tLast; *in = 0..1 pop-in progress (slide / scale).
static inline float SmPopAlpha(const SmPop* p, double t, int lifeMs, float* in) {
    const double age = t - p->tLast, a0 = t - p->t0;
    if (in) { const double u = a0 / SM_POP_IN_MS; *in = u <= 0 ? 0.f : u >= 1 ? 1.f : (float)(1.0 - (1.0 - u) * (1.0 - u)); }
    if (age < 0) return 1.f;
    if (age >= lifeMs) return 0.f;
    const double hold = lifeMs * 0.4;
    if (age <= hold) return 1.f;
    const double u = (age - hold) / (lifeMs - hold);
    return (float)(1.0 - u * u);
}
// Rank-up bounce: scale 1 -> ~1.14 -> 1 over SM_BOUNCE_MS (dt = ms since the rank went up).
static inline float SmBounce(double dt) {
    if (dt < 0 || dt >= SM_BOUNCE_MS) return 1.f;
    const double u = dt / SM_BOUNCE_MS;
    return (float)(1.0 + 0.35 * sin(u * 3.14159265358979) * (1.0 - u));
}
// Shake after a big gain: pixel offset at 1080p (caller scales), decays to 0 over SM_SHAKE_MS.
static inline void SmShake(double dt, float* dx, float* dy) {
    *dx = *dy = 0.f;
    if (dt < 0 || dt >= SM_SHAKE_MS) return;
    const double a = 7.0 * (1.0 - dt / SM_SHAKE_MS);
    *dx = (float)(a * sin(dt * 0.13)); *dy = (float)(a * 0.6 * cos(dt * 0.19));
}
// Glow pulse 0..1 at rank >= S (stronger at SS / SSS); 0 below.
static inline float SmGlow(int rank, double t) {
    if (rank < SM_GLOW_RANK) return 0.f;
    const double ph = 0.5 + 0.5 * sin(t * 2.0 * 3.14159265358979 / SM_GLOW_PERIOD_MS);
    const double k = 0.45 + 0.2 * (rank - SM_GLOW_RANK);
    return (float)((k > 0.85 ? 0.85 : k) * (0.35 + 0.65 * ph));
}
// Eased gauge: follows target (0..1000) with time constant SM_EASE_TAU_MS; a drop to a new rank (wrap) snaps.
static inline float SmEase(float cur, float target, double dtMs) {
    if (dtMs <= 0) return cur;
    if (target < cur - 500.f) return target;  // rank changed down / wrapped: no long slide backwards
    const float k = (float)(1.0 - exp(-dtMs / SM_EASE_TAU_MS));
    const float v = cur + (target - cur) * k;
    return fabsf(target - v) < 1.f ? target : v;
}
#endif
