// user #23400: Negative Ki meter decisions / fraction math / geometry (negki_meter.h), offline.
//   py -m ziglang cc -target x86_64-windows-gnu tests/negki_meter_test.c -o build/negki_meter_test.exe && build/negki_meter_test.exe
#include <stdint.h>
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "../src/negki_meter.h"
static int near(float a, float b) { return a - b < 0.0001f && b - a < 0.0001f; }
int main(void) {
    const float nan = 0.0f / 0.0f; int w = -1;
    // negative part: only below -0.5, garbage ignored
    assert(NkmNegPart(-300.0f) == -300.0f && NkmNegPart(0.0f) == 0.0f && NkmNegPart(-0.4f) == 0.0f && NkmNegPart(500.0f) == 0.0f);
    assert(NkmNegPart(nan) == 0.0f && NkmNegPart(-1e9f) == 0.0f);
    // deepest pool: auto-detect cur vs K+0x60
    assert(NkmNeg(800.0f, 0.0f, &w) == 0.0f && w == 0);                 // normal boss: no meter
    assert(NkmNeg(-250.0f, 0.0f, &w) == -250.0f && w == 1);             // Ki below 0
    assert(NkmNeg(-250.0f, -700.0f, &w) == -700.0f && w == 2);          // reserve deeper (hits during the break)
    assert(NkmNeg(-900.0f, -700.0f, &w) == -900.0f && w == 1);
    assert(NkmNeg(120.0f, -400.0f, &w) == -400.0f && w == 2);           // Ki recovering above 0, reserve still negative
    assert(NkmNeg(nan, nan, &w) == 0.0f && w == 0 && NkmNeg(-5.0f, 0.0f, 0) == -5.0f);
    // scale: |floor|, else max Ki, else none
    assert(NkmScale(-1000.0f, 1200.0f) == 1000.0f && NkmScale(0.0f, 1200.0f) == 1200.0f && NkmScale(0.0f, 0.0f) == 0.0f);
    assert(NkmScale(nan, 900.0f) == 900.0f && NkmScale(0.0f, 1e9f) == 0.0f && NkmScale(-3000.0f, 1000.0f) == 3000.0f);
    // fraction = |neg| / |floor| clamped 0..1
    assert(near(NkmFraction(-250.0f, -1000.0f, 1200.0f), 0.25f));
    assert(near(NkmFraction(-1000.0f, -1000.0f, 1200.0f), 1.0f) && NkmFraction(-1500.0f, -1000.0f, 1200.0f) == 1.0f);
    assert(near(NkmFraction(-300.0f, 0.0f, 1200.0f), 0.25f));          // missing floor -> max Ki scale
    assert(near(NkmFraction(-1500.0f, -3000.0f, 1000.0f), 0.5f));      // Ki floor depth x3
    assert(NkmFraction(0.0f, -1000.0f, 1000.0f) == 0.0f && NkmFraction(200.0f, -1000.0f, 1000.0f) == 0.0f);
    assert(NkmFraction(-200.0f, 0.0f, 0.0f) == 0.0f && NkmFraction(nan, -1000.0f, 1000.0f) == 0.0f);
    // visibility: setting, live, alive, negative now or within the linger
    assert(NkmVisible(1, 1, 0, -10.0f, 0) && !NkmVisible(0, 1, 0, -10.0f, 0) && !NkmVisible(1, 0, 0, -10.0f, 0) && !NkmVisible(1, 1, 1, -10.0f, 0));
    assert(!NkmVisible(1, 1, 0, 0.0f, -1));                              // never negative: hidden (auto-detect)
    assert(NkmVisible(1, 1, 0, 0.0f, NKM_LINGER_MS - 1) && !NkmVisible(1, 1, 0, 0.0f, NKM_LINGER_MS));
    // Font expectations include approved larger UI text (7fabe3a3).
    // geometry: defaults at 1280 x 720 = under the native Ki bar (x 426..854, y 59..62)
    NkmGeo g = NkmGeom(1280, 720, 89, 334);
    assert(g.w == 427 && g.x == 426 && g.top == 64 && g.barH == 4 && g.fontPx == 18 && g.pitch == 21);
    g = NkmGeom(1920, 1080, 89, 334);
    assert(g.w == 641 && g.x == 639 && g.top == 96 && g.barH == 6 && g.fontPx == 22 && g.pitch == 25);
    g = NkmGeom(0, 0, -5, 5000); assert(g.w == 1 && g.x == 0 && g.top == 0 && g.barH == 3);
    // fill px: at least 1 px for any negative value, never wider than the bar
    assert(NkmFillPx(0.0f, 400) == 0 && NkmFillPx(0.0001f, 400) == 1 && NkmFillPx(0.5f, 400) == 200 && NkmFillPx(1.0f, 400) == 400 && NkmFillPx(2.0f, 400) == 400 && NkmFillPx(nan, 400) == 0);
    puts("negki_meter_test: all passed");
    return 0;
}
