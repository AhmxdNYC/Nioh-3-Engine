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

    w.ch[kSsr].enabled = 1;
    CHECK(Decide(w, kSsr, true) == Act::Update, "SSR first frame updates");
    CHECK(Decide(w, kSsr, true) == Act::Reuse, "SSR second frame reuses");
    w.ch[kAo].enabled = 1;
    CHECK(Decide(w, kAo, true) == Act::Update, "AO first frame updates");
    CHECK(Decide(w, kAo, true) == Act::Reuse, "AO second frame reuses");
    CHECK(ObserveCamera(w, 120, 0, 0, 4), "cut clears SSR and AO");
    CHECK(w.ch[kSsr].have == 0 && w.ch[kAo].have == 0, "SSR and AO results dropped");

    const EstMs sh = Estimate(kShadow), gi = Estimate(kGi), cl = Estimate(kCloud);
    const EstMs ssr = Estimate(kSsr), ao = Estimate(kAo);
    CHECK(sh.lo == 0.10f && sh.hi == 0.20f, "shadow estimate");
    CHECK(gi.lo == 0.50f && gi.hi == 1.00f, "GI estimate");
    CHECK(cl.lo == 0.10f && cl.hi == 0.15f, "cloud estimate");
    CHECK(ssr.lo == 0.04f && ssr.hi == 0.04f, "SSR estimate");
    CHECK(ao.lo == 0.02f && ao.hi == 0.08f, "AO estimate");
    CHECK(BloomAndPostUnhooked(), "bloom and post are not hooked");
    CHECK(PresetLeavesOff(), "presets leave half-rate off");
    std::printf("skip/reuse/cut ok  shadow %.2f-%.2f ms  GI %.2f-%.2f ms  clouds %.2f-%.2f ms  SSR %.2f ms  AO %.2f-%.2f ms\n",
                sh.lo, sh.hi, gi.lo, gi.hi, cl.lo, cl.hi, ssr.lo, ao.lo, ao.hi);
    return g_fail ? 1 : 0;
}
