// #128463 player move-speed log (player_speed.h): per-move anim rate, slow stretches, hit-stop filtered.
#include <stdio.h>
#include <math.h>
#include <string.h>
#undef NDEBUG  // keep asserts in -O2 builds (zig cc -O2 defines NDEBUG)
#include <assert.h>
#include "../src/player_speed.h"

static int near(double a, double b, double e) { return fabs(a - b) <= e; }
static const double F = 1000.0 / 60.0;

int main(void) {
    PlSpd s = {0}; PlSpdEvt e; float t = 0; int ev = 0;
    // move A at 60 fps, normal speed, 30 frames
    assert(!PlSpdFrame(&s, 0xA, t, 1.0f, 1, F, &e));
    for (int i = 0; i < 30; ++i) { t += 1.0f; ev += PlSpdFrame(&s, 0xA, t, 1.0f, 1, F, &e); }
    assert(ev == 0);
    // move B: 0.6x for 60 frames (1 s), fps and step normal -> one SPD event ~1 s, rate ~0.6, step 1.0
    t = 0; assert(!PlSpdFrame(&s, 0xB, t, 1.0f, 1, F, &e));
    assert(s.prevAct == 0xA && near(s.prevRate, 1.0, 0.01));
    for (int i = 0; i < 60; ++i) { t += 0.6f; ev += PlSpdFrame(&s, 0xB, t, 1.0f, 1, F, &e); assert(!ev); }
    for (int i = 0; i < 10 && !ev; ++i) { t += 1.0f; ev += PlSpdFrame(&s, 0xB, t, 1.0f, 1, F, &e); }
    assert(ev == 1 && e.act == 0xB);
    assert(e.ms > 950 && e.ms < 1150);
    assert(near(e.rate, 0.62, 0.05) && near(e.minRate, 0.6, 0.01) && near(e.stepRate, 1.0, 0.01));
    // hit-stop: 5 frames frozen inside a normal move -> no event
    ev = 0; t = 0; PlSpdFrame(&s, 0xC, t, 1.0f, 1, F, &e);
    for (int i = 0; i < 40; ++i) { if (i < 10 || i >= 15) t += 1.0f; ev += PlSpdFrame(&s, 0xC, t, 1.0f, 1, F, &e); }
    assert(ev == 0);
    // move change while slow ends the stretch and reports it; prevRate of the slow move ~0.5
    t = 0; PlSpdFrame(&s, 0xD, t, 1.0f, 1, F, &e);
    for (int i = 0; i < 30; ++i) { t += 0.5f; PlSpdFrame(&s, 0xD, t, 1.0f, 1, F, &e); }
    assert(PlSpdFrame(&s, 0xE, 0.0f, 1.0f, 1, F, &e) == 1 && e.act == 0xD && near(e.rate, 0.5, 0.02));
    assert(s.prevAct == 0xD && near(s.prevRate, 0.5, 0.02));
    // 30 fps: step 2.0, anim +2 per frame, 2 engine frames counted per call -> normal, no event
    ev = 0; t = 0;
    for (int i = 0; i < 40; ++i) { t += 2.0f; ev += PlSpdFrame(&s, 0xE, t, 2.0f, 1, 2 * F, &e); }
    assert(ev == 0);
    // anim restart (time goes back) is skipped, not counted as slow
    t = 1.0f; ev = PlSpdFrame(&s, 0xE, t, 2.0f, 1, 2 * F, &e);
    for (int i = 0; i < 20; ++i) { t += 2.0f; ev += PlSpdFrame(&s, 0xE, t, 2.0f, 1, 2 * F, &e); }
    assert(ev == 0);
    // engine slow-mo (step 0.5 at 60 fps, anim +0.5) -> event with stepRate ~0.5 (the engine clock, not the move)
    t = 0; PlSpdFrame(&s, 0xF, t, 1.0f, 1, F, &e); ev = 0;
    for (int i = 0; i < 30; ++i) { t += 0.5f; PlSpdFrame(&s, 0xF, t, 0.5f, 1, F, &e); }
    for (int i = 0; i < 10 && !ev; ++i) { t += 1.0f; ev = PlSpdFrame(&s, 0xF, t, 1.0f, 1, F, &e); }
    assert(ev == 1 && e.stepRate < 0.7);
    assert(near(PlSpdRateOf(&s, 0xF), 1.0, 0.5) && PlSpdRateOf(&s, 0x99) < 0);
    // frozen pose (load fade-in: anim stuck for 2 s) -> no SPD event
    t = 0; PlSpdFrame(&s, 0x7D0, t, 1.0f, 1, F, &e); ev = 0;
    for (int i = 0; i < 120; ++i) { if (i % 40 == 0) t += 0.1f; ev += PlSpdFrame(&s, 0x7D0, t, 1.6f, 1, F, &e); }
    for (int i = 0; i < 10; ++i) { t += 1.0f; ev += PlSpdFrame(&s, 0x7D0, t, 1.0f, 1, F, &e); }
    assert(ev == 0);
    // ---- GH #178 SPDW (pure parts) ----
    // channel filter = the 0x134e68 loop: ch 0 counts 0-5, 6, 13; not 7-12, 14-19
    { int want[PLSPD_SLOTS] = {1,1,1,1,1,1,1,0,0,0,0,0,0,1,0,0,0,0,0,0};
      for (int i = 0; i < PLSPD_SLOTS; ++i) assert(PlSpdSlotCounts(i, 0) == want[i]);
      assert(PlSpdSlotCounts(7, 1) && PlSpdSlotCounts(14, 1) && !PlSpdSlotCounts(6, 1) && !PlSpdSlotCounts(12, 0) && !PlSpdSlotCounts(19, 0)); }
    // code shapes copied from Nioh3.dump.exe (0x13553c active check, 0x133ad0 rate, 0x2365b88 rate with the fresh-byte 1.0)
    { const uint8_t act[16] = {0xf3,0x0f,0x10,0x41,0x08,0x0f,0x2f,0x05,0x8c,0xec,0xa9,0x03,0x0f,0x97,0xc0,0xc3};
      const uint8_t r1[25] = {0xf3,0x0f,0x10,0x41,0x0c,0xc3,0xcc,0xcc};
      const uint8_t r2[25] = {0x80,0x79,0x10,0x00,0x75,0x06,0xf3,0x0f,0x10,0x41,0x0c,0xc3,0xf3,0x0f,0x10,0x05,0x44,0xe6,0x86,0x01,0xc6,0x41,0x10,0x00,0xc3};
      uint8_t bad[25]; memcpy(bad, r2, 25); bad[22] = 0x11;
      uint8_t act25[25] = {0}; memcpy(act25, act, 16);
      int32_t d = 0;
      assert(PlSpdCodeActive(act, &d) == 1 && d == 0x03a9ec8c);
      assert(PlSpdCodeActive(r1, &d) == 0);
      assert(PlSpdCodeRate(r1, &d) == 1);
      assert(PlSpdCodeRate(r2, &d) == 2 && d == 0x0186e644);
      assert(PlSpdCodeRate(bad, &d) == 0 && PlSpdCodeRate(act25, &d) == 0); }
    // product / base / format: slot 6 at 0.71, slot 12 (other channel) at 0.5, slot 7 (ch 1) at 0.2 ignored
    { PlSpdMods m; memset(&m, 0, sizeof m);
      m.total = 0.71f; m.dc0 = 1.0f; m.f684 = 0.9f; m.mdl = 1.0f; m.on684 = 0;
      m.st[6] = PLSPD_ON; m.rate[6] = 0.71f; m.timer[6] = 41.0f; m.vt[6] = 0x38FEF30;
      m.st[12] = PLSPD_ON; m.rate[12] = 0.5f; m.st[7] = PLSPD_ON; m.rate[7] = 0.2f;
      int u = -1;
      assert(near(PlSpdModsProduct(&m, &u), 0.71, 1e-5) && u == 0);
      assert(near(PlSpdModsBase(&m), 1.0, 1e-4));
      char b[640]; PlSpdWFormat(b, sizeof b, 0x61D7, &m, 0.69, 0.05f);
      assert(strstr(b, "0x61D7  total 0.71x = mods [6 0.71x t 41 vt 38FEF30] x base 1.00x x 684 off  step 0.69x"));
      assert(strstr(b, "other [7 0.20x] [12 0.50x]") && strstr(b, "min total 0.05x"));
      m.on684 = 1; m.total = 0.71f * 0.9f;  // 684 on: base excludes it
      assert(near(PlSpdModsBase(&m), 1.0, 1e-4));
      PlSpdWFormat(b, sizeof b, 0x61D7, &m, 0.69, 0.05f); assert(strstr(b, "x 684 0.90x"));
      m.st[6] = PLSPD_OFF; m.on684 = 0; m.total = 0.7f;  // no slot active: the slow-down lands in base
      assert(near(PlSpdModsBase(&m), 0.7, 1e-4));
      PlSpdWFormat(b, sizeof b, 0x61D8, &m, 0.58, 0.5f); assert(strstr(b, "mods none x base 0.70x"));
      m.st[3] = PLSPD_UNK; m.vt[3] = 0x1234;  // unknown vtable code in a counted slot: base cannot be said
      assert(PlSpdModsBase(&m) < 0); PlSpdModsProduct(&m, &u); assert(u == 1);
      PlSpdWFormat(b, sizeof b, 0x61D8, &m, 0.58, 0.5f); assert(strstr(b, "mods [3 ? vt 1234] x base ?"));
      char t[16]; PlSpdWFormat(t, sizeof t, 0x61D8, &m, 0.58, 0.5f); assert(strlen(t) < sizeof t); }  // tiny buffer
    // samples: the middle one survives decimation and stays in time order; min tracked over every frame
    { static PlSpdSamp z; PlSpdSampReset(&z); assert(!PlSpdSampMid(&z));
      PlSpdMods m; memset(&m, 0, sizeof m);
      for (int i = 0; i < 300; ++i) { m.total = (float)i; m.dc0 = (float)i; if (i == 5) m.total = -0.5f; PlSpdSampAdd(&z, &m); }
      assert(z.n > 8 && z.n <= PLSPD_SAMP && near(z.minTotal, -0.5, 1e-6));
      for (int i = 1; i < z.n; ++i) assert(z.s[i].dc0 > z.s[i - 1].dc0);
      const PlSpdMods* mid = PlSpdSampMid(&z); assert(mid && mid->dc0 > 90 && mid->dc0 < 210);
      PlSpdSampReset(&z); m.total = 0.7f; PlSpdSampAdd(&z, &m); assert(z.n == 1 && near(PlSpdSampMid(&z)->total, 0.7, 1e-6)); }
    printf("player_speed_test PASS\n");
    return 0;
}
