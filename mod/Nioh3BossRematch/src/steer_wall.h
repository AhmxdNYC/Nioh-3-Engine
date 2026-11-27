// steer_wall.h - TASK #141407 (escape walls #141235/#141194, RE notes/collision-move-141253.md).
// The FormSteerBoost extra step is cast against the world first, the way the game's own 2043E84 does before it moves a character
// (WorldRaycast 19BB08 from pos+(0,100,0) to that + step, character mask 70EEA8). A hit never crosses: the step slides along the wall
// (the part going into the wall's normal is dropped). Pure math, no game reads, so tests/steer_wall_test.cpp runs it offline.
#pragma once
#include <cmath>

// Horizontal slide of step (dx, dz) along a wall with normal (nx, nz) (only its xz part counts). Returns false when the normal has
// no horizontal part (a floor/ceiling hit: nothing to slide along) - the caller then takes no extra step.
inline bool SteerWallSlide(float dx, float dz, float nx, float nz, float* ox, float* oz) {
    const float nl = std::sqrt(nx * nx + nz * nz);
    if (!(nl > 1e-4f)) return false;
    nx /= nl; nz /= nl;
    const float into = dx * nx + dz * nz;  // < 0: the step goes into the wall
    if (into >= 0.f) { *ox = dx; *oz = dz; return true; }  // moving away from / along the wall: keep the step
    *ox = dx - into * nx;
    *oz = dz - into * nz;
    return true;
}
// End point of the probe ray: the step plus the game's own recovery step length (2043E84 casts dir * 30) ahead, so the body does not
// end with its front inside the wall.
inline void SteerWallProbeEnd(const float* from, float dx, float dz, float lead, float* to) {
    const float d = std::sqrt(dx * dx + dz * dz);
    const float s = d > 1e-4f ? (d + lead) / d : 1.f;
    to[0] = from[0] + dx * s; to[1] = from[1]; to[2] = from[2] + dz * s; to[3] = 1.f;
}
