// halfrate_core.h - pure skip / reuse / camera-cut policy for the half-rate option.
// No game, no hooks. tests/halfrate_harness.cpp runs this.
//
// A channel that is on updates on one call and reuses the last result on the next.
// A camera cut (the eye moved farther than the cut distance in one sample) drops every
// kept result, so the next call updates again. Off channels always update and remember nothing.
#pragma once
#include <cstdint>

namespace halfrate {

enum Channel { kShadow = 0, kGi = 1, kCloud = 2, kChannels = 3 };

enum class Act : int { Update = 0, Reuse = 1 };

// Far cascades only. Cascade 0 and 1 stay every frame. A VSM tile batch is not a cascade.
inline bool ShadowMaySkip(uint32_t cascade, uint32_t vsm) { return vsm == 0 && cascade >= 2; }

struct Chan {
    int enabled = 0;
    int have = 0;  // a result is sitting in the target
    int arm = 0;   // 1 = the next allowed call reuses
};

struct World {
    Chan ch[kChannels] = {};
    int haveCam = 0;
    float x = 0, y = 0, z = 0;
};

// True when this sample is a cut. The first sample only stores the eye.
inline bool ObserveCamera(World& w, float x, float y, float z, float cutMeters) {
    bool cut = false;
    if (w.haveCam) {
        const float dx = x - w.x, dy = y - w.y, dz = z - w.z;
        const float m = cutMeters < 0 ? 0 : cutMeters;
        cut = dx * dx + dy * dy + dz * dz > m * m;
    }
    w.x = x;
    w.y = y;
    w.z = z;
    w.haveCam = 1;
    if (cut) {
        for (int i = 0; i < kChannels; ++i) {
            w.ch[i].have = 0;
            w.ch[i].arm = 0;
        }
    }
    return cut;
}

// allowSkip is false for a near cascade or a VSM batch: those calls do not move the phase.
inline Act Decide(World& w, int channel, bool allowSkip) {
    if (channel < 0 || channel >= kChannels) return Act::Update;
    Chan& c = w.ch[channel];
    if (!c.enabled || !allowSkip) return Act::Update;
    if (!c.have || c.arm == 0) {
        c.have = 1;
        c.arm = 1;
        return Act::Update;
    }
    c.arm = 0;
    return Act::Reuse;
}

// Notes, Ally USER, not a new measurement. Shadow is the far-cascade share only.
// GI is half of the 1.77 ms gather family. Clouds already cost at most ~0.3 ms of frame time.
struct EstMs {
    float lo, hi;
};
inline EstMs Estimate(int channel) {
    if (channel == kShadow) return {0.10f, 0.20f};
    if (channel == kGi) return {0.50f, 1.00f};
    if (channel == kCloud) return {0.10f, 0.15f};
    return {0, 0};
}

// Extra-performance presets must not turn these on. The keys are not XLOD and not an MPC scalar.
inline bool PresetLeavesOff() { return true; }

}  // namespace halfrate
