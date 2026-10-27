// user #23400: Negative Ki meter (pure decisions + geometry, no game access; tests/negki_meter_test.c). Game side: negki_meter_game.h.
// Any negative Ki pool of a Training boss is auto-detected and shown as a second thin bar right under the boss's own Ki bar:
//   cur  = K+0x14 (Ki, goes below 0 when the boss is Ki-broken; KiSetCur 2F88D0 clamps it to >= the Ki floor)
//   k60  = K+0x60 (the second pool KiDamage 844604 pushes down toward the floor while the boss is exhausted; KiBreakClear 8455B4 zeroes it)
// (K = [actor+0x3A0]+0x98, notes/human-boss-negative-ki-23013.md). The meter shows only while one of them is below 0 (plus a short linger so it does
// not blink between hits), fill = |neg| / |floor| with floor = the effective Ki floor (KiFloor 2F89C8 / Ki floor depth). A missing / non-negative
// floor (KiFloor returns 0.0 when its tuning record is missing) uses the boss's max Ki as the scale, like PstKiFloor.
#pragma once
#include <stdint.h>
enum { NKM_LINGER_MS = 600 };
static inline int NkmSane(float v) { return v == v && v > -1000000.0f && v < 1000000.0f; }
// The negative part of one pool: v when it is below 0 (by more than half a Ki point = not rounding noise), else 0. Garbage (NaN / huge) = 0.
static inline float NkmNegPart(float v) { return NkmSane(v) && v < -0.5f ? v : 0.0f; }
// Deepest negative pool of a boss (<= 0; 0 = none). *which = 0 none, 1 cur, 2 K+0x60.
static inline float NkmNeg(float cur, float k60, int* which) {
    const float c = NkmNegPart(cur), r = NkmNegPart(k60);
    int w = 0; float n = 0.0f;
    if (c < 0.0f) { n = c; w = 1; }
    if (r < n) { n = r; w = 2; }
    if (which) *which = w;
    return n;
}
// Scale of the meter (> 0; 0 = cannot scale): |floor| when the floor is negative, else max Ki (sane), else 0.
static inline float NkmScale(float floorv, float max) {
    if (NkmSane(floorv) && floorv < -0.5f) return -floorv;
    return max > 0.0f && max <= 100000.0f ? max : 0.0f;
}
// Fill fraction 0..1 of one negative value: |neg| / scale, clamped. Not negative / no scale = 0.
static inline float NkmFraction(float neg, float floorv, float max) {
    const float s = NkmScale(floorv, max);
    if (!(s > 0.0f) || !NkmSane(neg) || !(neg < 0.0f)) return 0.0f;
    const float f = -neg / s;
    return f > 1.0f ? 1.0f : f;
}
// Show the meter of one boss: setting ON, Practice live, boss alive, and a negative pool now or within the linger window (msSinceNeg = ms since the
// last tick that saw one; < 0 = never).
static inline int NkmVisible(int on, int live, int dead, float neg, int32_t msSinceNeg) {
    if (!on || !live || dead) return 0;
    return neg < 0.0f || (msSinceNeg >= 0 && msSinceNeg < NKM_LINGER_MS);
}
// Geometry of the overlay in a W x H game client: bar x / width (wPm = width in 1/1000 of W, centred), bar height, font px and row pitch (one row
// per shown boss, the first row's top at yPm / 1000 of H). Defaults (yPm 89, wPm 334) = just under the native boss Ki bar on a 16:9 screen
// (captures/frame-meter-20261021/done_full_1.jpg: Ki bar x 426..854, y 59..62 of 1280 x 720).
typedef struct { int x, w, barH, fontPx, pitch, top; } NkmGeo;
static inline NkmGeo NkmGeom(int W, int H, int yPm, int wPm) {
    NkmGeo g;
    if (W < 1) W = 1; if (H < 1) H = 1;
    if (yPm < 0) yPm = 0; if (yPm > 950) yPm = 950;
    if (wPm < 50) wPm = 50; if (wPm > 1000) wPm = 1000;
    g.w = (int)((int64_t)W * wPm / 1000); if (g.w < 1) g.w = 1;
    g.x = (W - g.w) / 2;
    g.barH = (int)((int64_t)H * 6 / 1080); if (g.barH < 3) g.barH = 3;
    g.fontPx = (int)((int64_t)H * 22 / 1080); if (g.fontPx < 18) g.fontPx = 18;
    g.pitch = (g.fontPx > g.barH ? g.fontPx : g.barH) + 3;
    g.top = (int)((int64_t)H * yPm / 1000);
    return g;
}
// Filled width in px of a fraction on a bar of w px (at least 1 px for any visible negative value).
static inline int NkmFillPx(float f, int w) {
    if (!(f > 0.0f) || w <= 0) return 0;
    int px = (int)(f * (float)w + 0.5f);
    if (px < 1) px = 1; if (px > w) px = w;
    return px;
}
