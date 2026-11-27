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
#include <stdio.h>
#define PLSPD_WIN 6          // frames in the rolling rate window
#define PLSPD_SLOW 0.80      // rate below this starts a slow stretch
#define PLSPD_BACK 0.90      // rate above this ends it
#define PLSPD_FROZEN 0.15   // stretches slower than this are a frozen pose (load fade, pause), not slow motion
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
    int ok = s->slow && s->sWall >= PLSPD_MIN_MS;
    if (ok && s->sAnim * (1000.0 / 60.0) / s->sWall < PLSPD_FROZEN) ok = 0;  // live 11-21: load fade-in idle pose = 0.02x
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
// GH #178 SPDW: name the cause of a slow stretch. Static RE (Nioh3.dump.exe, notes in the GH #178 findings):
//   [ctrl+0x40] = [ctrl+0xdc4] x [ctrl+0xdc0] (FUN 0x6860b8): channel 0 of the 6 speed channels [ctrl+0xdc4+ch*4] times the
//   global step, so step 0.69x with anim 0.71x = channel 0 itself is ~0.7. Channel 0 = MoveSpeedBase(ctrl, 0, 0) (0x134e68) =
//   product of the active modifier slots x MoveSpeedWorldScale(ctrl) (0x13555c, 'base') x [ai+0x684] (only when chr+0xacc8
//   is set and the action flag bit 0x400000000000000), with overrides (mount / linked char, 0 when frozen).
//   Slots: [ctrl+0x6b8+i*8], i 0..19, each -> a 0x18-byte object {vtbl, f32 timer +8, f32 rate +0xc, u8 fresh +0x10}
//   (constructed at ctrl+0x4d8+i*0x18 by 0x74dc40). Active = vfunc +0x18 (default = timer > 0); rate = vfunc +0x20
//   (= [+0xc]; one vtable returns 1.0 while the fresh byte is set). Channel ch counts slots 0-5, 6+ch and 13+ch; slot 12 is the
//   flag-1 channel ([ctrl+0xddc]), slot 19 is never in the product. Slot 6+ch / 12 = the move-data speed event (0x9b8034:
//   rate = percent / 100 for N frames; 0x66c814 event 0x65: gauge-gated rate x stat x percent).
// Everything below is pure (tests/player_speed_test.c); player_log.h fills PlSpdMods with SEH reads, no game calls.
#define PLSPD_SLOTS 20
enum { PLSPD_OFF = 0, PLSPD_ON = 1, PLSPD_UNK = 2 };  // slot state: inactive, active with a known rate, code not recognised
typedef struct {
    float total, dc0;            // [ctrl+0xdc4] channel 0, [ctrl+0xdc0] global step
    float f684, mdl; int on684;  // [ai+0x684] and whether the getter applies it now; mdl = [[chr+0x3a0]+0x3018] (guess)
    uint32_t vt[PLSPD_SLOTS];    // slot vtable rva (0 = no object)
    float timer[PLSPD_SLOTS], rate[PLSPD_SLOTS];
    uint8_t st[PLSPD_SLOTS];
} PlSpdMods;
// Does slot i count in channel ch (flag 0)? Mirrors the 0x134e68 loop filter.
static int PlSpdSlotCounts(int i, int ch) { return i >= 0 && i <= 18 && (i < 6 || i == 6 + ch || i == 13 + ch); }
// Product of the active channel-0 slots; *unk = number of active counted slots whose code was not recognised (rate unknown).
static double PlSpdModsProduct(const PlSpdMods* m, int* unk) {
    double p = 1.0; int u = 0;
    for (int i = 0; i < PLSPD_SLOTS; ++i) {
        if (!PlSpdSlotCounts(i, 0)) continue;
        if (m->st[i] == PLSPD_ON) p *= m->rate[i]; else if (m->st[i] == PLSPD_UNK) ++u;
    }
    if (unk) *unk = u;
    return p;
}
// 'base' = what is left of the total after the slots and 684 (MoveSpeedWorldScale or an override); < 0 = cannot say.
static double PlSpdModsBase(const PlSpdMods* m) {
    int u = 0; const double p = PlSpdModsProduct(m, &u) * (m->on684 ? (double)m->f684 : 1.0);
    return (u || !(p > 1e-6)) ? -1.0 : (double)m->total / p;
}
// Code shape of a slot vfunc (the bytes at its address). Active check +0x18: movss xmm0,[rcx+8]; comiss xmm0,[rip+d]; seta al;
// ret -> 1 and *disp = d (the caller checks that [fn+12+d] is 0.0). Rate +0x20: movss xmm0,[rcx+0xc]; ret -> 1;
// cmp byte [rcx+0x10],0; jne +6; movss xmm0,[rcx+0xc]; ret; movss xmm0,[rip+d]; mov byte [rcx+0x10],0; ret -> 2 (*disp = d,
// constant at fn+20+d returned while the fresh byte is set). 0 = unknown.
static int PlSpdCodeActive(const uint8_t* b, int32_t* disp) {
    static const uint8_t h[8] = {0xF3, 0x0F, 0x10, 0x41, 0x08, 0x0F, 0x2F, 0x05}, t[4] = {0x0F, 0x97, 0xC0, 0xC3};
    for (int i = 0; i < 8; ++i) if (b[i] != h[i]) return 0;
    for (int i = 0; i < 4; ++i) if (b[12 + i] != t[i]) return 0;
    *disp = (int32_t)((uint32_t)b[8] | (uint32_t)b[9] << 8 | (uint32_t)b[10] << 16 | (uint32_t)b[11] << 24);
    return 1;
}
static int PlSpdCodeRate(const uint8_t* b, int32_t* disp) {  // b: 25 bytes
    static const uint8_t r[6] = {0xF3, 0x0F, 0x10, 0x41, 0x0C, 0xC3};
    static const uint8_t f[16] = {0x80, 0x79, 0x10, 0x00, 0x75, 0x06, 0xF3, 0x0F, 0x10, 0x41, 0x0C, 0xC3, 0xF3, 0x0F, 0x10, 0x05};
    static const uint8_t g[5] = {0xC6, 0x41, 0x10, 0x00, 0xC3};
    int i;
    for (i = 0; i < 6 && b[i] == r[i]; ++i) {}
    if (i == 6) { *disp = 0; return 1; }
    for (i = 0; i < 16; ++i) if (b[i] != f[i]) return 0;
    for (i = 0; i < 5; ++i) if (b[20 + i] != g[i]) return 0;
    *disp = (int32_t)((uint32_t)b[16] | (uint32_t)b[17] << 8 | (uint32_t)b[18] << 16 | (uint32_t)b[19] << 24);
    return 2;
}
// Samples over one slow stretch, kept in time order; when full, every other one is dropped and the stride doubles, so
// s[n/2] stays near the middle of the stretch (the steady part, not the hit-stop at either end).
#define PLSPD_SAMP 32
typedef struct { PlSpdMods s[PLSPD_SAMP]; int n, stride, tick; float minTotal; } PlSpdSamp;
static void PlSpdSampReset(PlSpdSamp* z) { z->n = 0; z->stride = 1; z->tick = 0; z->minTotal = 1e30f; }
static void PlSpdSampAdd(PlSpdSamp* z, const PlSpdMods* m) {
    if (z->stride < 1) z->stride = 1;
    if (m->total < z->minTotal) z->minTotal = m->total;
    if (z->tick++ % z->stride) return;
    if (z->n == PLSPD_SAMP) {
        for (int i = 0; i < PLSPD_SAMP / 2; ++i) z->s[i] = z->s[2 * i];
        z->n = PLSPD_SAMP / 2; z->stride *= 2; z->tick = 1;
    }
    z->s[z->n++] = *m;
}
static const PlSpdMods* PlSpdSampMid(const PlSpdSamp* z) { return z->n ? &z->s[z->n / 2] : 0; }
// One SPDW line body: "0x61D7  total 0.71x = mods [6 0.71x t 41 vt 38FEF30] x base 1.00x x 684 1.00x  step 0.69x  (...)"
static int PlSpdWFormat(char* o, size_t cap, uint32_t act, const PlSpdMods* m, double step, float minTotal) {
    int n = snprintf(o, cap, "0x%X  total %.2fx = mods", act, m->total), any = 0;
#define PLW_ADD(...) do { if (n >= 0 && (size_t)n < cap) n += snprintf(o + n, cap - n, __VA_ARGS__); } while (0)
    for (int i = 0; i < PLSPD_SLOTS; ++i) {
        if (!PlSpdSlotCounts(i, 0) || m->st[i] == PLSPD_OFF) continue;
        if (m->st[i] == PLSPD_ON) PLW_ADD(" [%d %.2fx t %.0f vt %X]", i, m->rate[i], m->timer[i], m->vt[i]);
        else PLW_ADD(" [%d ? vt %X]", i, m->vt[i]);
        any = 1;
    }
    if (!any) PLW_ADD(" none");
    const double base = PlSpdModsBase(m);
    if (base >= 0.0) PLW_ADD(" x base %.2fx", base); else PLW_ADD(" x base ?");
    if (m->on684) PLW_ADD(" x 684 %.2fx", m->f684); else PLW_ADD(" x 684 off");
    PLW_ADD("  step %.2fx  (min total %.2fx  mdl %.2f  dc0 %.2f", step, minTotal, m->mdl, m->dc0);
    any = 0;
    for (int i = 0; i < PLSPD_SLOTS; ++i) {
        if (PlSpdSlotCounts(i, 0) || m->st[i] == PLSPD_OFF) continue;
        if (!any) PLW_ADD("  other"); any = 1;
        if (m->st[i] == PLSPD_ON) PLW_ADD(" [%d %.2fx]", i, m->rate[i]); else PLW_ADD(" [%d ?]", i);
    }
    PLW_ADD(")");
#undef PLW_ADD
    return n;
}
#endif
