// TASK #141407: offline check of the FormSteerBoost wall slide (src/steer_wall.h). Build -O0 without NDEBUG (memory zig-o2-ndebug).
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdio>
#include "../src/steer_wall.h"
static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }
int main() {
    float ox, oz;
    // straight into a wall facing -x (normal +x... wall at the player's +x side has normal -x): all of the step is dropped
    assert(SteerWallSlide(10.f, 0.f, -1.f, 0.f, &ox, &oz) && near(ox, 0.f) && near(oz, 0.f));
    // 45 degrees into the same wall: only the along-wall part stays
    assert(SteerWallSlide(10.f, 10.f, -1.f, 0.f, &ox, &oz) && near(ox, 0.f) && near(oz, 10.f));
    // a step away from the wall is kept as is
    assert(SteerWallSlide(-10.f, 5.f, -1.f, 0.f, &ox, &oz) && near(ox, -10.f) && near(oz, 5.f));
    // the normal's vertical part is ignored, a sloped wall normal is renormalised in xz
    assert(SteerWallSlide(10.f, 0.f, -0.5f, 0.f, &ox, &oz) && near(ox, 0.f) && near(oz, 0.f));
    // a floor/ceiling hit (no xz normal) gives no slide
    assert(!SteerWallSlide(10.f, 0.f, 0.f, 0.f, &ox, &oz));
    // after the slide nothing of the step points into the wall
    const float n[2] = {-0.6f, -0.8f};
    assert(SteerWallSlide(7.f, 9.f, n[0], n[1], &ox, &oz) && ox * n[0] + oz * n[1] > -1e-3f);
    // probe end = step + 30 cm lead in the step direction, height kept
    const float from[3] = {100.f, 200.f, 300.f};
    float to[4];
    SteerWallProbeEnd(from, 30.f, 40.f, 30.f, to);  // |step| 50 -> 80
    assert(near(to[0], 148.f) && near(to[1], 200.f) && near(to[2], 364.f));
    SteerWallProbeEnd(from, 0.f, 0.f, 30.f, to);
    assert(near(to[0], 100.f) && near(to[2], 300.f));
    std::puts("steer_wall_test: 8/8 PASS");
    return 0;
}
