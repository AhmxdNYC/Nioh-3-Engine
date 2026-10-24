// halfrate_harness.cpp - skip vs reuse vs camera-cut, offline. Not a game test.
#include <cstdio>
#include <cstring>
#include "../src/halfrate_core.h"

static int g_fail = 0;
#define CHECK(c, ...) do { if (!(c)) { std::printf("FAIL " __VA_ARGS__); std::printf("\n"); ++g_fail; } } while (0)

int main() {
    using namespace halfrate;
    World w;
    w.ch[kShadow].enabled = 0;
    CHECK(Decide(w, kShadow, true) == Act::Update, "off still updates");
    CHECK(w.ch[kShadow].have == 0, "off remembers nothing");

    w.ch[kShadow].enabled = 1;
    CHECK(Decide(w, kShadow, false) == Act::Update, "near cascade is not a skip");
    CHECK(w.ch[kShadow].have == 0, "near cascade does not arm reuse");
    CHECK(!ShadowMaySkip(0, 0) && !ShadowMaySkip(1, 0) && !ShadowMaySkip(3, 1), "near and vsm stay");
    CHECK(ShadowMaySkip(2, 0) && ShadowMaySkip(3, 0), "far cascade may skip");

    CHECK(Decide(w, kShadow, true) == Act::Update, "first far frame updates");
    CHECK(Decide(w, kShadow, true) == Act::Reuse, "second far frame reuses");
    CHECK(Decide(w, kShadow, true) == Act::Update, "third far frame updates");
    CHECK(Decide(w, kShadow, true) == Act::Reuse, "fourth far frame reuses");

    CHECK(!ObserveCamera(w, 0, 0, 0, 4), "first eye sample is not a cut");
    CHECK(!ObserveCamera(w, 0.3f, 0, 0, 4), "a step is not a cut");
    CHECK(Decide(w, kShadow, true) == Act::Update, "phase continues after a step");
    CHECK(ObserveCamera(w, 30, 0, 0, 4), "a 30 m jump is a cut");
    CHECK(w.ch[kShadow].have == 0, "a cut drops the kept shadow");
    CHECK(Decide(w, kShadow, true) == Act::Update, "the frame after a cut updates");
    CHECK(!ObserveCamera(w, 30.2f, 0, 0, 4), "motion after the cut is not another cut");
    CHECK(Decide(w, kShadow, true) == Act::Reuse, "the next far frame reuses the new shadow");

    w.ch[kGi].enabled = 1;
    CHECK(Decide(w, kGi, true) == Act::Update, "GI first frame updates");
    CHECK(Decide(w, kGi, true) == Act::Reuse, "GI second frame reuses");
    CHECK(ObserveCamera(w, 80, 0, 0, 4), "cut clears GI too");
    CHECK(w.ch[kGi].have == 0, "GI result dropped");
    CHECK(Decide(w, kGi, true) == Act::Update, "GI updates after the cut");

    CHECK(Decide(w, kCloud, true) == Act::Update, "clouds stay off");
    CHECK(w.ch[kCloud].have == 0, "clouds remember nothing while off");
    w.ch[kCloud].enabled = 1;
    CHECK(Decide(w, kCloud, true) == Act::Update, "clouds first frame updates");
    CHECK(Decide(w, kCloud, true) == Act::Reuse, "clouds second frame reuses");

    const EstMs sh = Estimate(kShadow), gi = Estimate(kGi), cl = Estimate(kCloud);
    CHECK(sh.lo == 0.10f && sh.hi == 0.20f, "shadow estimate");
    CHECK(gi.lo == 0.50f && gi.hi == 1.00f, "GI estimate");
    CHECK(cl.lo == 0.10f && cl.hi == 0.15f, "cloud estimate");
    CHECK(PresetLeavesOff(), "presets leave half-rate off");
    std::printf("skip/reuse/cut ok  shadow %.2f-%.2f ms  GI %.2f-%.2f ms  clouds %.2f-%.2f ms\n",
                sh.lo, sh.hi, gi.lo, gi.hi, cl.lo, cl.hi);
    return g_fail ? 1 : 0;
}
