// Reuse isolated addon stubs, then exercise the actual installed-hook body.
#define main ExistingUiFixtureMain
#include "ui_harness.cpp"
#undef main
#include "../src/ui.cpp"
int padCalls = 0;
XINPUT_STATE expectedPad{};
DWORD WINAPI FakePreviousPad(DWORD, XINPUT_STATE* out) { ++padCalls; *out = expectedPad; return ERROR_SUCCESS; }
int main() {
    using namespace addon::ui;
    g_prevGet = FakePreviousPad;
    g_hostRequested = true;
    g_open = true; // even an old standalone-open bit must not mask a host's pad
    expectedPad.dwPacketNumber = 42;
    expectedPad.Gamepad.wButtons = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_DPAD_UP;
    expectedPad.Gamepad.bLeftTrigger = 255;
    expectedPad.Gamepad.sThumbLX = 17000;
    for (DWORD slot = 0; slot < 4; ++slot) {
        XINPUT_STATE out{};
        CHECK(HkGetState(slot, &out) == ERROR_SUCCESS);
        CHECK(memcmp(&out, &expectedPad, sizeof(out)) == 0);
    }
    CHECK(padCalls == 4 && g_padEdges.load() == 0);
    puts("ALL PASSED: actual addon XInput hook is transparent in hosted mode across all four slots");
}
