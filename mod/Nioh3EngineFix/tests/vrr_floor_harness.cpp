// vrr_floor_harness.cpp - offline test of [VRR] Tier120Floor in caveFps (src/caves.asm, GitHub #98). Not a game test.
#include <cstdint>
#include <cstdio>
#include <cstring>
extern "C" {
uint32_t CallFps(void* g, uint32_t tier);
extern uint64_t vrr_minNs[3], vrr_maxNs[3], vrr_pNear, vrr_floorNs, vrr_floorHoldNs, vrr_floorProbeNs;
extern uint32_t vrr_enable, vrr_tier, vrr_capCur, vrr_arm[3], vrr_cap[3], vrr_fpsOut, vrr_floorOn, vrr_floorEnters;
// caves.asm externs not used by caveFps
void L1Wait() {} void W6Wait() {} void MWWait() {} void MWComplete() {} void WgWake() {} void WgSleep() {}
void GfWait() {} void DsQpc() {} void RDWait() {} void RDComplete() {}
}
static uint8_t G[0x800];
static uint32_t nearPage[4];
static uint64_t now = 1000000000ull;
static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL " __VA_ARGS__); printf("\n"); } } while (0)
static void Frame(double fps, uint32_t tier = 120) {
    now += (uint64_t)(1e9 / fps);
    memcpy(G + 0x7D8, &now, 8);
    CallFps(G, tier);
}
static uint32_t G614() { uint32_t v; memcpy(&v, G + 0x614, 4); return v; }
static void Setup(int floorFps) {
    const int tiers[3] = {30, 60, 120}, mins[3] = {20, 24, 48};
    for (int i = 0; i < 3; ++i) {
        vrr_arm[i] = 1; vrr_cap[i] = tiers[i];
        vrr_minNs[i] = 1000000000ull / tiers[i]; vrr_maxNs[i] = 1000000000ull / mins[i];
    }
    vrr_pNear = (uint64_t)(uintptr_t)nearPage;
    vrr_enable = 1;
    vrr_floorNs = floorFps ? 1000000000ull / floorFps : 0;
    vrr_floorHoldNs = 2000000000ull;
    vrr_floorProbeNs = 30000000000ull;
    vrr_floorOn = 0; vrr_floorEnters = 0;
    Frame(120);  // primes vrr_prev
}
int main() {
    // 1. off: 68 fps in the 120 tier stays VRR 120 (G[614] = 68, cap 120)
    Setup(0);
    for (int i = 0; i < 68 * 5; ++i) Frame(68);
    CHECK(vrr_floorOn == 0 && vrr_tier == 120 && vrr_capCur == 120 && G614() == 68, "off: on=%u tier=%u cap=%u g614=%u", vrr_floorOn, vrr_tier, vrr_capCur, G614());
    // 2. floor 90: 68 fps -> after ~2 s acts as the 60 tier
    Setup(90);
    for (int i = 0; i < 68; ++i) Frame(68);
    CHECK(vrr_floorOn == 0 && vrr_tier == 120, "1 s below floor must not switch yet");
    for (int i = 0; i < 80; ++i) Frame(68);
    CHECK(vrr_floorOn == 1 && vrr_floorEnters == 1, "2 s below floor switches (on=%u enters=%u)", vrr_floorOn, vrr_floorEnters);
    for (int i = 0; i < 10; ++i) Frame(60);
    CHECK(vrr_tier == 60 && vrr_capCur == 60 && G614() == 60 && nearPage[0] == 2 && nearPage[1] == 1,
          "floor mode = 60 tier: tier=%u cap=%u g614=%u t120=%u t60=%u", vrr_tier, vrr_capCur, G614(), nearPage[0], nearPage[1]);
    // 3. probe after 30 s: back to 120 for re-measure
    for (int i = 0; i < 60 * 31; ++i) Frame(60);
    CHECK(vrr_floorEnters >= 1, "probe");
    Setup(90);
    while (!vrr_floorOn) Frame(68);
    int n = 0;
    while (vrr_floorOn && n < 60 * 40) { Frame(60); ++n; }
    CHECK(n >= 60 * 30 - 2 && n <= 60 * 30 + 2, "probe after 30 s in floor mode (left after %d frames at 60)", n);
    Frame(110);
    CHECK(vrr_floorOn == 0 && vrr_tier == 120 && vrr_capCur == 120, "probe at 110 fps runs the 120 tier");
    for (int i = 0; i < 600; ++i) Frame(110);
    CHECK(vrr_floorEnters == 1 && vrr_tier == 120, "110 fps after the probe stays 120 (enters=%u)", vrr_floorEnters);
    // 4. fast PC: 110 fps with dips never switches
    Setup(90);
    for (int i = 0; i < 6000; ++i) Frame(i % 10 == 0 ? 70 : 110);
    CHECK(vrr_floorEnters == 0, "110 fps with 10%% dips must not switch (enters=%u)", vrr_floorEnters);
    // 5. 60 tier unaffected by the floor
    Setup(90);
    for (int i = 0; i < 600; ++i) Frame(50, 60);
    CHECK(vrr_floorEnters == 0 && vrr_tier == 60 && G614() == 50, "60 tier untouched (g614=%u)", G614());
    printf(fails ? "vrr floor harness FAILED (%d)\n" : "vrr floor harness ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
