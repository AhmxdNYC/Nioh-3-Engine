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

// #127171 pts = what the move / event itself is worth, bonus = the extra on top (0 = none); shown 'Label +pts +bonus'. count = merges.
typedef struct { char label[24]; float pts, bonus; int count; double t0, tLast; } SmPop;
typedef struct { SmPop p[SM_POPS]; int n; } SmPopQ;  // p[0] = oldest

static inline void SmPopPrune(SmPopQ* q, double t, int lifeMs) {
    int k = 0;
    for (int i = 0; i < q->n; ++i) if (t - q->p[i].tLast < (double)lifeMs) q->p[k++] = q->p[i];
    q->n = k;
}
static inline void SmPopPush2(SmPopQ* q, const char* label, float pts, float bonus, double t, int lifeMs) {
    SmPopPrune(q, t, lifeMs);
    for (int i = 0; i < q->n; ++i)
        if (!strcmp(q->p[i].label, label) && t - q->p[i].tLast < SM_POP_MERGE_MS) {  // same bonus again: add to it, move it to the top
            SmPop m = q->p[i]; m.pts += pts; m.bonus += bonus; m.count++; m.tLast = t;
            for (int j = i; j + 1 < q->n; ++j) q->p[j] = q->p[j + 1];
            q->p[q->n - 1] = m; return;
        }
    if (q->n == SM_POPS) { for (int j = 0; j + 1 < q->n; ++j) q->p[j] = q->p[j + 1]; q->n--; }
    SmPop* p = &q->p[q->n++]; memset(p, 0, sizeof *p);
    strncpy(p->label, label, sizeof p->label - 1); p->pts = pts; p->bonus = bonus; p->count = 1; p->t0 = p->tLast = t;
}
static inline void SmPopPush(SmPopQ* q, const char* label, float pts, double t, int lifeMs) { SmPopPush2(q, label, pts, 0.f, t, lifeMs); }
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
// #127299 rank-up transition ("needs to be cooler"), dt = ms since the shown rank stepped up:
//   0..SM_UP_IMPACT   old letter breaks away (grows, rises, fades) while the new one drops in from SM_UP_BIG x (accelerating)
//   impact            white flash on the new letter (and bar), a sword-slash streak across it, the meter shakes
//   impact..+140      small settle squash, then rest.
#define SM_UP_IMPACT 190
#define SM_UP_HOLD 70   // #127323 hit-stop: the rank-up freezes this long at the impact (letter white-hot)
#define SM_UP_MS 900   // #127304 room for sparks / ring / embers after the impact (+ the #127323 hit-stop)
#define SM_UP_BIG 1.75f
typedef struct { float ns, na, os, oa, ody, flash, slash, shx, shy, imp; } SmUpFx;  // imp = ms since impact (<0 before)  // new scale/alpha, old scale/alpha/rise (1080p px), flash 0..1, slash progress (<0 none), impact shake
static inline SmUpFx SmRankUp(double dt) {
    SmUpFx f = {1.f, 1.f, 1.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, -1.f};
    if (dt < 0 || dt >= SM_UP_MS) return f;
    if (dt < SM_UP_IMPACT) {
        const double u = dt / SM_UP_IMPACT;
        f.ns = (float)(SM_UP_BIG - (SM_UP_BIG - 1.0) * u * u); f.na = (float)(u * 2.5 > 1 ? 1 : u * 2.5);
        f.os = (float)(1.0 + 0.45 * u); f.oa = (float)(1.0 - u); f.ody = (float)(-18.0 * u);
        return f;
    }
    const double e = dt - SM_UP_IMPACT;
    f.imp = (float)e;
    if (e < 140) f.ns = (float)(1.0 - 0.07 * sin(e / 140.0 * 3.14159265358979) * (1.0 - e / 140.0));
    if (e < 240) f.flash = (float)(1.0 - e / 240.0);
    if (e < 200) f.slash = (float)(e / 200.0);
    if (e < 220) { const double a = 18.0 * (1.0 - e / 220.0); f.shx = (float)(a * sin(e * 0.21)); f.shy = (float)(a * 0.7 * cos(e * 0.27)); }
    return f;
}
// #127304 each rank gets its own rank-up, building up as you climb (index = the rank reached, 1 = C .. 6 = SSS):
//   slashes (1 = one cut, 2 = cross-cut, 3 = cross + level cut), parry sparks (count), ki shockwave ring, rising embers, shake strength
typedef struct { int slashes, sparks, ring, embers; float shake; } SmUpStyle;
static const SmUpStyle sm_up_style[7] = {
    {1, 0, 0, 0, 0.6f},    // D (never reached by a rank-up)
    {1, 22, 0, 0, 0.7f},   // C   parry sparks
    {1, 30, 0, 0, 0.8f},   // B   one clean cut + sparks
    {2, 30, 0, 0, 0.9f},   // A   cross-cut
    {1, 26, 1, 0, 1.0f},   // S   ki shockwave ring
    {2, 40, 1, 0, 1.15f},  // SS  cross-cut + ring + more sparks
    {3, 54, 1, 26, 1.4f},  // SSS everything + rising embers
};
static inline unsigned SmHash(unsigned x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
static inline float SmRand01(unsigned seed, unsigned i, unsigned k) { return (float)(SmHash(seed * 9781u + i * 6271u + k * 131u + 17u) & 0xFFFFFF) / 16777215.f; }
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
// #127253 the shown bar on a rank-up: the old rank's bar fills to the end first, then the shown rank steps up and the bar starts from
// empty (one rank at a time on multi-rank jumps). Rank down / reset snaps. Shown rank drives the letter and the rank-up bounce.
#ifndef SM_FILL_PER_S
#define SM_FILL_PER_S 5000.f   // minimum fill speed on the way to the end (gauge units / s; 1000 = full bar), so a rank-up takes <= ~0.2 s
#endif
typedef struct { int rank; float g; } SmShownBar;
// returns 1 when the shown rank stepped up this call
static inline int SmBarStep(SmShownBar* b, int rank, float gauge, double dtMs) {
    if (rank < b->rank || b->rank < 0) { b->rank = rank; b->g = gauge; return 0; }
    if (dtMs <= 0) return 0;
    if (rank > b->rank) {
        if (b->g < 1000.f) {  // fill on: at least one frame shows the bar full before the step
            float v = SmEase(b->g, 1000.f, dtMs); const float minV = b->g + SM_FILL_PER_S * (float)dtMs / 1000.f;
            if (v < minV) v = minV;
            b->g = v >= 999.f ? 1000.f : v; return 0;
        }
        b->rank += 1; b->g = 0.f;
        if (b->rank == rank) b->g = SmEase(0.f, gauge, dtMs);
        return 1;
    }
    b->g = SmEase(b->g, gauge, dtMs);
    return 0;
}
#endif
