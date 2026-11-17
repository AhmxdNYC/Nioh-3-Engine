// vrr_floor_harness.cpp - offline test of [VRR] Tier120Floor in caveFps (src/caves.asm, GitHub #98). Not a game test.
#include <cstdint>
#include <cstdio>
#include <cstring>
extern "C" {
uint32_t CallFps(void* g, uint32_t tier);
uint32_t CallHold(void* entry);
extern uint32_t vrr_hold60;
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
    // 6. HoldWindowFix (P10, GitHub #98): a notice aged by caveHold expires after 31 ages (> 0x1E) in real time
    //    ~0.5 s at any fps while armed; stock frames add 1 per walk (30-frame window, the game's own behaviour)
    {
        auto life = [](double fps, uint32_t tier) {
            uint8_t entry[0x28] = {};
            int frames = 0;
            for (;;) {
                Frame(fps, tier);
                ++frames;
                uint32_t c = CallHold(entry);
                if (c == 0xFFFFFFFFu) return -1.0;
                memcpy(entry + 0x20, &c, 4);
                if ((int)c > 0x1E) break;
                if (frames > 10000) return -2.0;
            }
            return frames / fps;
        };
        const double rates[] = {60, 75, 95, 105, 112, 120};
        for (double f : rates) {
            Setup(0);
            double t = life(f, 120);
            CHECK(t >= 0.49 && t <= 0.54, "hold window at %.0f fps armed = %.3f s (want ~0.5)", f, t);
        }
        // below 60 fps: min(ticks,1) keeps the stock frame window (never shorter than stock; regression-pc #112892)
        const double low[] = {30, 48};
        for (double f : low) {
            Setup(0);
            double t = life(f, 120);
            CHECK(t >= 31.0 / f - 0.001 && t <= 31.0 / f + 0.001, "hold window at %.0f fps armed = %.3f s (want stock %.3f)", f, t, 31.0 / f);
        }
        Setup(0);
        vrr_enable = 0;  // stock: +1 per walk
        double t = life(120, 120);
        CHECK(t > 0.25 && t < 0.27, "stock hold window at 120 = %.3f s (31 frames)", t);
        Setup(0);
        double sum = 0;
        for (int i = 0; i < 1120; ++i) { Frame(112); sum += vrr_hold60; }
        CHECK(sum >= 599 && sum <= 601, "60 Hz hold ticks over 10 s at 112 fps = %.0f", sum);
    }
    printf(fails ? "vrr floor harness FAILED (%d)\n" : "vrr floor harness ALL PASSED\n", fails);
    return fails ? 1 : 0;
}
