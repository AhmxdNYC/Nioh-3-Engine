// human #128463: player move-speed log (pure, unit-tested in tests/player_speed_test.c), part of [Record] PlayerClock.
// GH #157: one hit of a 3-hit chain played at ~0.6x for ~1 s while fps and the game clock stayed normal, so the CLK line
// (frame step [ctrl+0x40]) could not show it. This reads the move's own animation time instead: float [ctrl + 0x60] for
// slot 0 (the getter at vtable +0xb0 = Nioh3+0x4812E8 `movss xmm0,[rcx+rax*4+0x60]`, the time the branch/cancel key scan
// 0x20DFC0 uses, 1/60 s units). Speed = anim frames advanced x 16.667 ms / wall ms: 1.00 = the move plays in real time.
//   ACT lines get "anim 0.61x" for the move that just ended (moves >= 50 ms of counted frames).
//   SPD line once per slow stretch >= 300 ms (hit-stop is shorter):  SPD  slow 0x61D8 980 ms  anim 0.61x (min 0.55x)
//        step 1.00x  t 4.2 -> 14.1   (step = frame step vs wall over the same stretch: 1.00 = not the engine clock)
#ifndef N3_PLAYER_SPEED_H
#define N3_PLAYER_SPEED_H
#include <stdint.h>
#define PLSPD_WIN 6          // frames in the rolling rate window
#define PLSPD_SLOW 0.80      // rate below this starts a slow stretch
#define PLSPD_BACK 0.90      // rate above this ends it
#define PLSPD_MIN_MS 300.0   // shorter stretches (hit-stop ~70-150 ms + the 6-frame window) are not reported
typedef struct {
    uint32_t act; int have; float lastAnim;
    double mAnim, mWall;                     // current move: anim frames, wall ms
    uint32_t prevAct; double prevRate;       // the move that just ended (prevRate < 0 = too short to say)
    double wA[PLSPD_WIN], wW[PLSPD_WIN]; int wi, wn;
    int slow; double sAnim, sStep, sWall, sMin; float sT0, sT1;
} PlSpd;
typedef struct { uint32_t act; double ms, rate, minRate, stepRate; float t0, t1; } PlSpdEvt;

static void PlSpdReset(PlSpd* s) { const uint32_t pa = s->prevAct; const double pr = s->prevRate; PlSpd z = {0}; *s = z; s->prevAct = pa; s->prevRate = pr; }
static int PlSpdEnd(PlSpd* s, PlSpdEvt* e) {
    const int ok = s->slow && s->sWall >= PLSPD_MIN_MS;
    if (ok) { e->act = s->act; e->ms = s->sWall; e->rate = s->sAnim * (1000.0 / 60.0) / s->sWall; e->minRate = s->sMin;
              e->stepRate = s->sStep * (1000.0 / 60.0) / s->sWall; e->t0 = s->sT0; e->t1 = s->sT1; }
    s->slow = 0;
    return ok;
}
// Speed of move `act` so far (it is still the tracked move) or of the move that just ended; < 0 = unknown / under 50 ms.
static double PlSpdRateOf(const PlSpd* s, uint32_t act) {
    if (s->have && s->act == act) return s->mWall >= 50.0 ? s->mAnim * (1000.0 / 60.0) / s->mWall : -1.0;
    return s->prevAct == act ? s->prevRate : -1.0;
}
// One call per counted engine frame. act = player action id, anim = [ctrl+0x60], step60 = [ctrl+0x40] (per frame),
// k = engine frames since the last call, spanMs = wall ms for those k frames. Returns 1 when a slow stretch ended (*e set).
static int PlSpdFrame(PlSpd* s, uint32_t act, float anim, float step60, uint32_t k, double spanMs, PlSpdEvt* e) {
    int r = 0;
    if (!s->have || act != s->act) {
        if (s->have) {
            r = PlSpdEnd(s, e);
            s->prevAct = s->act; s->prevRate = s->mWall >= 50.0 ? s->mAnim * (1000.0 / 60.0) / s->mWall : -1.0;
        }
        PlSpdReset(s); s->have = 1; s->act = act; s->lastAnim = anim;
        return r;
    }
    const double d = (double)anim - (double)s->lastAnim;
    s->lastAnim = anim;
    if (!(d >= 0.0 && d < 16.0) || !(spanMs > 0.0 && spanMs < 1000.0) || k == 0) return 0;  // anim restart / stall: skip frame
    s->mAnim += d; s->mWall += spanMs;
    s->wA[s->wi] = d; s->wW[s->wi] = spanMs; s->wi = (s->wi + 1) % PLSPD_WIN; if (s->wn < PLSPD_WIN) ++s->wn;
    if (s->wn < PLSPD_WIN) return 0;
    double a = 0, w = 0;
    for (int i = 0; i < PLSPD_WIN; ++i) { a += s->wA[i]; w += s->wW[i]; }
    const double rate = a * (1000.0 / 60.0) / w;
    if (!s->slow && rate < PLSPD_SLOW) {
        s->slow = 1; s->sAnim = a; s->sWall = w; s->sMin = rate; s->sT0 = (float)((double)anim - a); s->sT1 = anim;
        s->sStep = (step60 >= 0.0f && step60 < 16.0f) ? (double)step60 * PLSPD_WIN : w * 0.06;  // window start: assume normal
        return 0;
    }
    if (s->slow) {
        s->sAnim += d; s->sWall += spanMs; s->sT1 = anim;
        s->sStep += (step60 >= 0.0f && step60 < 16.0f) ? (double)step60 * (double)k : spanMs * 0.06;
        if (rate < s->sMin) s->sMin = rate;
        if (rate > PLSPD_BACK) return PlSpdEnd(s, e);
    }
    return 0;
}
#endif
