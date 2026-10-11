// Exercise the real menu.cpp model and input feed; no game, graphics, hooks or files.
#include "../src/menu.cpp"
#include <cstdlib>

namespace osd {
OverlaySettings& Overlay() { static OverlaySettings o; return o; }
LatencyStatus Latency() { return {}; }
const char* PositionName(int) { return "TopLeft"; }
int OffsetX() { return 0; }
int OffsetY() { return 0; }
int DefaultOffsetY(int) { return 0; }
float CounterScale(unsigned) { return 1; }
int FgStateText(const FgStatus&, char* b, size_t n) { snprintf(b, n, "OFF"); return 0; }
FgStatus FrameGen() { return {}; }
const char* FgTypeName(int) { return "OFF"; }
const char* FgReason(const FgStatus&) { return nullptr; }
void NoteFgSwitch() {}
void DrawRect(float, float, float, float, uint32_t) {}
float DrawText(float x, float, const char*, uint32_t, float) { return x; }
}
namespace gpuopt {
State GetState() { return State::Off; }
bool Batching() { return false; }
int BarriersIni() { return 0; }
bool Rewrite(int) { return false; }
void SetBatching(bool) {}
void SetRewrite(int, bool) {}
}
namespace extraperf {
bool Available(int) { return false; }
bool AnyAvailable() { return false; }
int Levels(int) { return 4; }
int Level(int) { return 0; }
const char* Key(int) { return "stub"; }
const char* LevelName(int, int) { return "OFF"; }
void SetLevel(int, int) {}
const char* PresetName(int) { return "OFF"; }
void ApplyPreset(int) {}
int MatchPreset() { return 0; }
}

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    using namespace menu;
    g_cfg.holdMask = 0;
    g_padHooked = g_keyHooked = true;
    g_lastPresent = (LONG64)Now();
    g_lastPadFeed = g_lastKeyFeed = (LONG64)Now();
    int rows[kMenuRowCap];
    g_cfg.layout = 0;
    g_sel = kFps;
    Open("harness");
    const int legacyN = PageRows(rows, true);
    CHECK(rows[0] == kFps && rows[legacyN - 1] == kClose);
    Move(-1);
    CHECK(g_sel == kClose);
    Close("harness");
    Open("harness");
    CHECK(g_sel == kClose);  // flat-mode scripts still park their cursor
    Close("harness");
    g_cfg.layout = 1;
    Open("harness");
    CHECK(g_pages.page == pRoot && g_sel == kPageRow + pSettings);
    Change(g_sel, 0);
    CHECK(g_pages.page == pSettings && g_sel == kPageRow + pDisplay);
    Change(g_sel, 0);
    CHECK(g_pages.page == pDisplay && g_sel == kFps);
    int n = PageRows(rows, true);
    CHECK(rows[n - 1] == kBackRow);
    const int overlayValue = osd::Overlay().fpsCounter;
    g_pressed = N_BACK | N_OK;
    OnPresentImpl();
    CHECK(g_pages.page == pSettings && osd::Overlay().fpsCounter == overlayValue);
    EnterPage(pAll);
    CHECK(g_sel == kClose && PageRows(rows, true) == legacyN);
    Move(1);
    CHECK(g_sel == kFps);
    MovePage(1);
    CHECK(g_sel == rows[10]);
    Back();
    CHECK(g_pages.page == pSettings);
    Back();
    CHECK(g_pages.page == pRoot);
    Back();
    CHECK(!g_open);
    // Every stable item belongs to one leaf. Status-only rows cannot be selected.
    for (int id = 0; id < kCount; ++id) {
        int owners = 0;
        for (int p = pRoot; p < pAll; ++p) owners += PageContains(p, id);
        CHECK(owners == 1);
    }
    CHECK(FindItem("GF") == kXGF && FindItem("WakeStats") == kXWS);
    CHECK(FindItem("R1") == kR1 && FindItem("FPS") == kFps && FindItem("AutoContinueLoad") == kAutoCont);
    EnterPage(pFrameGen);
    n = PageRows(rows, true);
    for (int i = 0; i < n; ++i) CHECK(rows[i] != kFgState && rows[i] != kUpscaler);
    CHECK(PadNav(XINPUT_GAMEPAD_DPAD_DOWN | XINPUT_GAMEPAD_A) == (N_DOWN | N_OK));
    CHECK(PadNav(XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_B) == (N_PAGEUP | N_BACK));
    CHECK(KeyNav(K_PAGEUP | K_PAGEDOWN) == (N_PAGEUP | N_PAGEDOWN));
    // The real XInput feed masks open input, then the closing A/B until release.
    Open("harness");
    XINPUT_STATE st = {};
    PadFeed(2, &st, false);  // release opening latch
    st.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    PadFeed(2, &st, false);
    CHECK(st.Gamepad.wButtons == 0 && (g_pressed & N_OK));
    Close("harness");
    st.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    PadFeed(2, &st, false);
    CHECK(st.Gamepad.wButtons == 0);
    st.Gamepad.wButtons = 0;
    PadFeed(2, &st, false);
    st.Gamepad.wButtons = XINPUT_GAMEPAD_A;
    PadFeed(2, &st, false);
    CHECK(st.Gamepad.wButtons == XINPUT_GAMEPAD_A);
    // All catalog keys are reachable exactly once through their section pages.
    int seen[kConfigN] = {};
    for (int i = 0; i < kConfigN; ++i) g_configValues.values[(size_t)i] = {kConfigSettings[i].initial, false};
    for (int group = 0; group < kConfigGroupN; ++group) {
        g_pages.page = pCfgFirst + group;
        n = PageRows(rows, true);
        CHECK(n > 2 && rows[n - 2] == kSave && rows[n - 1] == kBackRow);
        for (int i = 0; i < n; ++i)
            if (rows[i] >= kSettingRow) ++seen[rows[i] - kSettingRow];
    }
    for (int i = 0; i < kConfigN; ++i) {
        CHECK(seen[i] == 1);
        const auto& d = kConfigSettings[i];
        std::string next;
        if (d.kind == setting_ui::Kind::Manual) {
            CHECK(!setting_ui::Next(d, d.initial, 0, next));
        } else if (d.kind == setting_ui::Kind::Number) {
            setting_ui::Next(d, std::to_string(d.lo), -1, next);
            double value;
            CHECK(setting_ui::Number(next, value) && value == d.lo);
            setting_ui::Next(d, std::to_string(d.hi), 1, next);
            CHECK(setting_ui::Number(next, value) && value == d.hi);
            CHECK(next.find('e') == std::string::npos);  // Win32 integer profiles do not read scientific notation
        } else {
            const auto choices = setting_ui::Choices(d.choices);
            setting_ui::Next(d, choices.front(), -1, next);
            CHECK(next == choices.front());
            setting_ui::Next(d, choices.back(), 1, next);
            CHECK(next == choices.back());
        }
    }
    int enabled = -1, brightness = -1, fps = -1;
    for (int i = 0; i < kConfigN; ++i) {
        const auto& d = kConfigSettings[i];
        if (!strcmp(d.section, "General") && !strcmp(d.key, "Enabled")) enabled = i;
        if (!strcmp(d.section, "Overlay") && !strcmp(d.key, "Brightness")) brightness = i;
        if (d.legacy == kFps) fps = i;
    }
    CHECK(enabled >= 0 && brightness >= 0 && fps >= 0);
    const int menuEnabled = g_cfg.enabled;
    ChangeConfig(enabled, 0);
    CHECK(g_configValues.values[(size_t)enabled].changed && g_cfg.enabled == menuEnabled);
    ChangeConfig(brightness, 1);
    CHECK(std::fabs(osd::Overlay().brightness - 1.1f) < 0.0001f);
    ChangeConfig(fps, 0);
    CHECK(g_configValues.values[(size_t)fps].changed);
    Change(kFps, 0);
    CHECK(!g_configValues.values[(size_t)fps].changed);  // last explicit live edit wins at Save
    int writes = 0;
    g_configValues.Save(kConfigSettings, [&](const wchar_t*, const wchar_t*, const std::wstring&) { ++writes; });
    CHECK(writes == 2);  // only explicitly edited, controller-supported rows
    puts("ALL PASSED: real settings pages, legacy order, back priority, paging, keys and pad isolation");
    return 0;
}
