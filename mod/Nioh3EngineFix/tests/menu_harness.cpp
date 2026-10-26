// Exercise the real menu.cpp model and input feed; no game, graphics, hooks or files.
#include "../src/menu.cpp"
#include <cstdlib>
#include <string>
#include <vector>

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
struct Rect { float x,y,w,h; };
std::vector<Rect> drawnRects;
std::vector<std::string> drawnTexts;
void DrawRect(float x, float y, float w, float h, uint32_t) { drawnRects.push_back({x,y,w,h}); }
bool DrawCoverRect(float,float){return true;}
std::vector<float> drawnTextX, drawnTextScale;
float DrawText(float x, float, const char* text, uint32_t, float s) { drawnTexts.emplace_back(text);drawnTextX.push_back(x);drawnTextScale.push_back(s);return x; }
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
int g_fm = 0, g_kw = 0;
int HarnessPstGet(int what) { return what == 7 ? g_fm : what == 21 ? g_kw : -1; }
int HarnessPstSet(int what, int value, int) {
    int* p = what == 7 ? &g_fm : what == 21 ? &g_kw : nullptr;
    if (!p) return -1;
    *p = value < 0 ? !*p : (value ? 1 : 0);
    return *p;
}
namespace host_fixture {
namespace mb = rematch_menu_bridge;
mb::Snapshot view{};
uint32_t posted = 999;
uint64_t coveredToken=0;
int Covered(uint64_t,uint64_t token){coveredToken=token;return mb::Accepted;}
bool closeBusy = false;
int Attach(uint64_t, uint64_t*) { return mb::Accepted; }
int Detach(uint64_t) { return mb::Accepted; }
int Post(uint64_t, uint64_t, uint32_t action) {
    if (closeBusy && action == mb::CloseMenu) return mb::Busy;
    posted = action; return mb::Accepted;
}
int Copy(uint64_t, mb::Snapshot* out, uint32_t) { *out = view; return mb::Accepted; }
void Complete(bool open) { ++view.generation; view.flags = mb::HostedReady | (open ? mb::Open : 0u); view.workerTickMs=GetTickCount64(); }
}
int main() {
    using namespace menu;
    g_cfg.holdMask = 0;
    g_padHooked = g_keyHooked = true;
    g_lastPresent = (LONG64)Now();
    g_lastPadFeed = g_lastKeyFeed = (LONG64)Now();
    int rows[kMenuRowCap];
    g_cfg.layout = 0;
    g_page = 0;
    g_sel = kPage;
    Open("harness");
    CHECK(g_pages.page == pRoot && g_sel == kPageRow + pDisplay);
    int rootN = PageRows(rows, true);
    CHECK(rootN == 5 && rows[0] == kModMenuRow && rows[1] == kCloseTopRow && rows[2] == kPageRow + pDisplay && rows[3] == kPageRow + pSettings && rows[4] == kClose);  // #38077 top rows
    Change(g_sel, 0);
    CHECK(g_pages.page == pDisplay && g_sel == kFps);
    const int disp = PageRows(rows, true);
    // #37433: the moved non-performance rows are on Gameplay / Overlays (status rows are listed, not selectable) and off Settings
    auto countOf = [&](int n, int a, int b, int c, int d) {
        int hits = 0;
        for (int i = 0; i < n; ++i) hits += rows[i] == a || rows[i] == b || rows[i] == c || rows[i] == d;
        return hits;
    };
    // (kFgState / kUpscaler need a known build, which this harness host is not: their placement is checked above)
    CHECK(countOf(PageRows(rows, false), kXTS, kXWS, kXTS, kXWS) == 2);
    g_pages.page = pSettings;
    CHECK(countOf(PageRows(rows, false), kXTS, kXWS, kXTS, kXWS) == 0);
    CHECK(countOf(PageRows(rows, false), kLat, kXIK, kVrr, kVrr) == 3);  // performance rows stay on Settings
    g_pages.page = pDisplay;
    PageRows(rows, true);  // rows was overwritten by the Settings listing above
    int pad = -1, padX = -1, padY = -1, padSize = -1, padOp = -1, padColor = -1;
    for (int i = 0; i < kConfigN; ++i) {
        const char* key = kConfigSettings[i].key;
        if (!strcmp(key, "ControllerDisplay")) pad = i;
        else if (!strcmp(key, "ControllerDisplayX")) padX = i;
        else if (!strcmp(key, "ControllerDisplayY")) padY = i;
        else if (!strcmp(key, "ControllerDisplaySize")) padSize = i;
        else if (!strcmp(key, "ControllerDisplayOpacity")) padOp = i;
        else if (!strcmp(key, "ControllerDisplayPressColor")) padColor = i;
    }
    CHECK(pad >= 0 && padX >= 0 && padY >= 0 && padSize >= 0 && padOp >= 0 && padColor >= 0);
    bool sawFps = false, sawMeter = false, sawKi = false, sawLook = false, sawPad = false, sawX = false, sawY = false;
    int meterAt = -1, kiAt = -1;
    bool sawSize = false, sawOp = false, sawColor = false;
    for (int i = 0; i < disp; ++i) {
        if (rows[i] == kFps) sawFps = true;
        if (rows[i] == kFrameMeterRow) { sawMeter = true; meterAt = i; }
        if (rows[i] == kKiWindowRow) { sawKi = true; kiAt = i; }
        if (rows[i] == kEpPre || rows[i] == kFg) sawLook = true;
        if (rows[i] == kSettingRow + pad) sawPad = true;
        if (rows[i] == kSettingRow + padX) sawX = true;
        if (rows[i] == kSettingRow + padY) sawY = true;
        if (rows[i] == kSettingRow + padSize) sawSize = true;
        if (rows[i] == kSettingRow + padOp) sawOp = true;
        if (rows[i] == kSettingRow + padColor) sawColor = true;
    }
    CHECK(sawFps && sawMeter && sawKi && kiAt == meterAt + 1);
    CHECK(sawPad && sawX && sawY && sawSize && sawOp && sawColor);
    CHECK(!sawLook);
    CHECK(rows[disp - 1] == kBackRow && strcmp(PageName(g_pages.page), "Gameplay / Overlays") == 0);
    {
        bool sawAutoO = false, sawStyleO = false;
        int style = -1;
        for (int i = 0; i < kConfigN; ++i) if (!strcmp(kConfigSettings[i].key, "ButtonStyle")) style = i;
        for (int i = 0; i < disp; ++i) { if (rows[i] == kAutoCont) sawAutoO = true; if (style >= 0 && rows[i] == kSettingRow + style) sawStyleO = true; }
        CHECK(sawAutoO && sawStyleO);  // #37423
        int ox = -1, cx = -1;
        for (int i = 0; i < kConfigN; ++i) { if (!strcmp(kConfigSettings[i].key, "OffsetX")) ox = i; if (!strcmp(kConfigSettings[i].key, "ControllerDisplayX")) cx = i; }
        CHECK(ox >= 0 && cx >= 0 && kConfigSettings[ox].lo < 0 && kConfigSettings[cx].lo < 0);
    }
    const int pageBefore = g_pages.page;
    Change(kFrameMeterRow, 0);
    CHECK(g_pages.page == pageBefore);
    g_pstGet = HarnessPstGet;
    g_pstSet = HarnessPstSet;
    g_fm = 0;
    Change(kFrameMeterRow, 0);
    CHECK(g_fm == 1);
    Change(kFrameMeterRow, -1);
    CHECK(g_fm == 0);
    g_kw = 1;
    Change(kKiWindowRow, 0);
    CHECK(g_kw == 0);
    Change(kKiWindowRow, 0);
    CHECK(g_kw == 1);
    g_pstGet = nullptr;
    g_pstSet = nullptr;
    Back();
    CHECK(g_pages.page == pRoot);
    Change(kPageRow + pSettings, 0);
    CHECK(g_pages.page == pSettings && g_sel == kPage);
    const int page1 = PageRows(rows, true);
    CHECK(rows[0] == kModMenuRow && rows[1] == kCloseTopRow && rows[2] == kPage && rows[page1 - 1] == kBackRow);
    bool sawSafe = false, sawMeterOnSettings = false, sawKiOnSettings = false, sawAuto = false;
    for (int i = 0; i < page1; ++i) {
        if (rows[i] == kFps) sawSafe = true;
        if (rows[i] == kEpPre || rows[i] == kFg) sawLook = true;
        if (rows[i] == kFrameMeterRow) sawMeterOnSettings = true;
        if (rows[i] == kKiWindowRow) sawKiOnSettings = true;
        if (rows[i] == kAutoCont) sawAuto = true;
    }
    char pageName[64];
    uint32_t pageColor = 0;
    Value(kPage, pageName, sizeof(pageName), pageColor);
    CHECK(!sawSafe && !sawLook && !sawMeterOnSettings && !sawKiOnSettings && !sawAuto && strcmp(pageName, "1/2 same picture") == 0);
    Move(-1);
    CHECK(g_sel == kCloseTopRow);
    Move(-1);
    CHECK(g_sel == kModMenuRow);
    Move(-1);
    CHECK(g_sel == kBackRow);
    Move(1);
    Move(1);
    Change(kModMenuRow, 0);  // #38077: Mod menu = the top-level page
    CHECK(g_pages.page == pRoot && g_sel == kPageRow + pDisplay);
    Change(kPageRow + pSettings, 0);
    CHECK(g_pages.page == pSettings && g_sel == kPage);
    Change(kCloseTopRow, 0);  // #38077: Close closes the menu
    CHECK(!g_open);
    Open("harness");
    Close("harness");
    Open("harness");
    CHECK(g_sel == kPageRow + pDisplay);
    Close("harness");
    Open("harness");
    Change(kPageRow + pSettings, 0);
    g_page = 1;
    const int page2 = PageRows(rows, true);
    sawSafe = false;
    sawLook = false;
    bool sawSwitcher = false;
    for (int i = 0; i < page2; ++i) {
        if (rows[i] == kFps) sawSafe = true;
        if (rows[i] == kEpPre) sawLook = true;
        if (rows[i] == kPage) sawSwitcher = true;
    }
    Value(kPage, pageName, sizeof(pageName), pageColor);
    CHECK(!sawSafe && sawLook && sawSwitcher && strcmp(pageName, "2/2 may change the look") == 0);
    Change(kPage, 0);
    CHECK(g_page == 0);
    Back();
    CHECK(g_pages.page == pRoot);
    g_rematchSession = 7;
    int bossN = PageRows(rows, true);
    CHECK(bossN == 7 && rows[2] == kRematchRow && rows[3] == kPracticeRow && rows[4] == kPageRow + pDisplay &&
          rows[5] == kPageRow + pSettings && rows[6] == kClose);
    Change(kPageRow + pSettings, 0);
    g_page = 1;
    bossN = PageRows(rows, true);
    bool sawBoss = false, sawPractice = false;
    for (int i = 0; i < bossN; ++i) {
        sawBoss |= rows[i] == kRematchRow;
        sawPractice |= rows[i] == kPracticeRow;
    }
    CHECK(!sawBoss && !sawPractice);
    g_rematchSession = 0;
    g_page = 0;
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
    n = PageRows(rows, true);
    bool allHasSwitcher = false;
    for (int i = 0; i < n; ++i) allHasSwitcher |= rows[i] == kPage;
    CHECK(g_sel == kClose && !allHasSwitcher);  // grouped All hides the two-page switcher
    Move(1);
    CHECK(g_sel == kModMenuRow);  // wraps from the last row to the new top
    Move(1);
    Move(1);
    CHECK(g_sel == kFps);
    MovePage(1);
    CHECK(g_sel == rows[12]);  // #38077: Mod menu + Close sit above the 10 steps
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
    // #37433: status readouts and timing diagnostics are not performance fixes: they live on Gameplay / Overlays, not Settings
    CHECK(SettingsPage(kFgState) == pDisplay && SettingsPage(kUpscaler) == pDisplay);
    CHECK(SettingsPage(kXTS) == pDisplay && SettingsPage(kXWS) == pDisplay);
    CHECK(SettingsPage(kFg) == pFrameGen && SettingsPage(kLat) == pSafe && SettingsPage(kXIK) == pCpu);
    CHECK(OverlayRow(kFgState) && OverlayRow(kUpscaler) && OverlayRow(kXTS) && OverlayRow(kXWS) && OverlayRow(kAutoCont));
    CHECK(!OverlayRow(kFg) && !OverlayRow(kLat) && !OverlayRow(kXIK) && !OverlayRow(kVrr));
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
    // Actual host page routing: worker acknowledgement, Back priority, close
    // retry, and no fallthrough into a performance setting or native command.
    namespace hf = host_fixture;
    hf::view.size=sizeof(hf::view); hf::view.version=rmb::kVersion;
    hf::view.session=7; hf::view.generation=1; hf::view.flags=rmb::HostedReady;
    hf::view.workerTickMs=Now();
    g_rematchApi={sizeof(rmb::Api),rmb::kVersion,rmb::kRequiredCapabilities,0,hf::Attach,hf::Detach,hf::Post,hf::Copy,hf::Covered};
    g_rematchSession=7;
    const uint64_t beforeCopy=Now();while(Now()<=beforeCopy)Sleep(1);
    hf::view.workerTickMs=Now();
    CHECK(RefreshRematchHost(beforeCopy)); // worker legitimately published after caller sampled time
    hf::view.workerTickMs=Now()+60000;CHECK(!RefreshRematchHost(Now()));
    hf::view.workerTickMs=Now()-1500;CHECK(!RefreshRematchHost(Now()));
    hf::view.workerTickMs=Now();CHECK(RefreshRematchHost(Now()));

    Open("host fixture");
    n=PageRows(rows,true); bool found=false; for(int i=0;i<n;++i) found |= rows[i]==kRematchRow;
    // #13223 first page: Settings / Boss Rematch / Practice / Close (no Save row)
    CHECK(g_pages.page==pRoot && n==6 && rows[2]==kPageRow+pSettings && rows[3]==kRematchRow && rows[4]==kPracticeRow && rows[5]==kClose);
    CHECK(found);
    Change(kRematchRow,0);
    CHECK(g_rematchPage && g_rematchAwait && hf::posted==rmb::OpenRematch);  // #13223: opens the Boss Rematch page directly
    hf::Complete(true); g_pressed=0; OnPresentImpl();
    CHECK(g_rematchOpened && !g_rematchAwait);
    g_pressed=N_BACK|N_OK; OnPresentImpl();
    CHECK(hf::posted==rmb::Back && g_rematchAwait);
    hf::Complete(false); g_pressed=0; OnPresentImpl();
    CHECK(!g_rematchPage && g_open); // addon root Back returns to the host root
    Change(kRematchRow,0); hf::Complete(true); OnPresentImpl();
    g_pressed=N_OK; OnPresentImpl(); CHECK(hf::posted==rmb::Accept);
    hf::Complete(false); hf::closeBusy=true; OnPresentImpl();
    CHECK(!g_open && !g_rematchPage && g_rematchClosePending);
    hf::closeBusy=false; OnPresentImpl(); CHECK(!g_rematchClosePending && hf::posted==rmb::CloseMenu);
    Open("frozen worker"); Change(kRematchRow,0); hf::Complete(true); OnPresentImpl();
    hf::view.workerTickMs=Now()-2000; OnPresentImpl(); CHECK(!g_open && !g_rematchPage);
    hf::Complete(false); Open("pending timeout"); Change(kRematchRow,0);
    g_rematchAwaitAt=Now()-2000; OnPresentImpl(); CHECK(!g_open && !g_rematchPage);
    hf::Complete(false); hf::view.workerTickMs=Now()+5000; CHECK(!RefreshRematchHost(Now()));
    // Actual host draw/accepted-Present handshake and full keyboard/pad capture.
    g_configured=true;
    hf::Complete(true);g_rematchAwait=false;g_rematchPage=true;g_open=1;
    hf::view.flags|=rmb::CoverActive;hf::view.coverToken=42;hf::view.coverPhase=1;hf::view.coverDeadline=Now()+90000;
    OnPresentImpl();CHECK(g_coverToken==42 && !hf::coveredToken);
    CHECK(DrawImpl(1280,720)>0);
    strcpy_s(g_rematchView.coverReason,"INTERNAL DIAGNOSTIC MUST NOT BE DISPLAYED");
    for(unsigned width:{640u,1280u,2560u}) {
        const unsigned height=width*9/16;
        for(uint32_t phase:{1u,2u,3u}) {
            g_rematchView.coverPhase=phase;osd::drawnRects.clear();osd::drawnTexts.clear();
            CHECK(DrawImpl(width,height)>0 && g_coverDrawnToken==42);
            CHECK(osd::drawnRects.size()==2 && osd::drawnTexts.size()==3);
            for(const auto& r:osd::drawnRects)CHECK(r.x>=0&&r.y>=0&&r.w>0&&r.h>0&&r.x+r.w<=width&&r.y+r.h<=height);
            CHECK(osd::drawnTexts[0]=="Loading boss match");
            CHECK(osd::drawnTexts[1]==(phase==1?"Preparing...":phase==2?"Starting boss match...":"Entering boss arena..."));
            for(const auto& text:osd::drawnTexts)CHECK(text.find("INTERNAL")==std::string::npos&&text.find('%')==std::string::npos);
        }
    }
    g_rematchView.coverPhase=1;
    cover_present::Ticket ticket{CoverDrawn(9,1),true,true,false};
    auto rejectedTicket=ticket;rejectedTicket.testPresent=true;CoverPresented(rejectedTicket,1,0);CHECK(!hf::coveredToken);
    rejectedTicket=ticket;rejectedTicket.outermost=false;CoverPresented(rejectedTicket,1,0);CHECK(!hf::coveredToken);
    rejectedTicket=ticket;rejectedTicket.submitted=false;CoverPresented(rejectedTicket,1,0);CHECK(!hf::coveredToken);
    CoverPresented(ticket,1,1);CHECK(!hf::coveredToken); // OCCLUDED/non-S_OK
    CoverPresented(ticket,2,0);CHECK(!hf::coveredToken); // stale context
    const uint64_t workerBefore=g_coverWorker.load(),deadlineBefore=g_coverDeadline.load(),armedBefore=g_coverArmedAt.load();
    g_coverWorker=Now()+60000;CoverPresented(ticket,1,0);CHECK(!hf::coveredToken && !g_coverPresented);
    g_coverWorker=Now()-2000;CoverPresented(ticket,1,0);CHECK(!hf::coveredToken && !g_coverPresented);
    g_coverWorker=workerBefore;g_coverDeadline=Now();CoverPresented(ticket,1,0);CHECK(!hf::coveredToken && !g_coverPresented);
    g_coverDeadline=deadlineBefore;g_coverArmedAt=Now()-5001;CoverPresented(ticket,1,0);CHECK(!hf::coveredToken && !g_coverPresented);
    g_coverArmedAt=armedBefore;g_disabled=1;CoverPresented(ticket,1,0);CHECK(!hf::coveredToken && !g_coverPresented);g_disabled=0;
    CoverPresented(ticket,1,0);CHECK(hf::coveredToken==42 && CoverCaptures(Now()));
    CHECK(!menu::CoverTipAllowed()); // AwaitPresent is never load-tip permission
    hf::view.coverPhase=3;hf::view.workerTickMs=Now();OnPresentImpl();
    CHECK(!menu::CoverTipAllowed()); // require covered successful Present of Loading
    CoverPresented(ticket,1,0);CHECK(menu::CoverTipAllowed());
    const auto validTip=g_coverTipLease;
    g_coverTipLease.workerMs=Now()-1501;CHECK(!menu::CoverTipAllowed());g_coverTipLease=validTip;
    g_coverTipLease.presentedMs=Now()-1501;CHECK(!menu::CoverTipAllowed());g_coverTipLease=validTip;
    g_coverTipLease.deadlineMs=Now();CHECK(!menu::CoverTipAllowed());g_coverTipLease=validTip;
    g_coverTipLease.token++;CHECK(!menu::CoverTipAllowed());g_coverTipLease=validTip;
    g_coverTipLease.session++;CHECK(!menu::CoverTipAllowed());g_coverTipLease=validTip;
    AcquireSRWLockExclusive(&g_coverTipLock);CHECK(!menu::CoverTipAllowed());ReleaseSRWLockExclusive(&g_coverTipLock);
    g_disabled=1;CHECK(!menu::CoverTipAllowed());g_disabled=0;
    // Contradictory fresh provider snapshot cannot end or refresh the last coherent lease.
    const auto validView=hf::view;const auto knownWorker=g_rematchFreshAt;
    hf::view.flags&=~rmb::CoverActive;hf::view.workerTickMs=Now()+1;
    OnPresentImpl();CHECK(g_coverToken==42 && g_rematchFreshAt==knownWorker);
    hf::view=validView;
    uint8_t allKeys[256]{};allKeys[DIK_W]=0x80;allKeys[DIK_SPACE]=0x80;KeyFeed(allKeys,false);
    CHECK(!allKeys[DIK_W] && !allKeys[DIK_SPACE]);
    XINPUT_STATE coverPad{};coverPad.Gamepad.wButtons=XINPUT_GAMEPAD_A;coverPad.Gamepad.sThumbLX=20000;
    PadFeed(0,&coverPad,false);CHECK(!coverPad.Gamepad.wButtons && !coverPad.Gamepad.sThumbLX);
    // Worker loss must pass input even before another Present executes.
    g_coverWorker=Now()-2000;coverPad.Gamepad.wButtons=XINPUT_GAMEPAD_A;
    PadFeed(0,&coverPad,false);CHECK(coverPad.Gamepad.wButtons==XINPUT_GAMEPAD_A);
    g_coverWorker=Now();g_coverCancel=false;g_pad[0].prev=0;
    coverPad.Gamepad.wButtons=XINPUT_GAMEPAD_B;PadFeed(0,&coverPad,false);
    CHECK(g_coverCancel && !coverPad.Gamepad.wButtons && !menu::CoverTipAllowed());
    coverPad.Gamepad.wButtons=XINPUT_GAMEPAD_B;PadFeed(0,&coverPad,false);CHECK(!coverPad.Gamepad.wButtons);
    allKeys[DIK_ESCAPE]=0x80;KeyFeed(allKeys,false);CHECK(!allKeys[DIK_ESCAPE]);
    allKeys[DIK_ESCAPE]=0x80;KeyFeed(allKeys,false);CHECK(!allKeys[DIK_ESCAPE]);
    OnPresentImpl();CHECK(!g_coverToken && !g_open && !g_pad[0].latch);
    OnPresentImpl();CHECK(!g_coverToken && !g_open); // same stale active token cannot resurrect
    hf::view.coverToken=43;hf::view.workerTickMs=Now();OnPresentImpl();CHECK(g_coverToken==43);
    g_coverPresented=Now()-2000;OnPresentImpl();CHECK(!g_coverToken && !g_open); // stale rendered cover cannot persist over live input
    hf::view.coverToken=44;hf::view.coverDeadline=Now()+180000;hf::view.workerTickMs=Now()-1501;
    OnPresentImpl();CHECK(!g_coverToken && !g_open && !menu::CoverTipAllowed()); // long budget never relaxes stale worker
    g_rematchSession=0;
    // #13257 side panel: 58 columns (was 44), docked right, nothing drawn outside the panel, no text past its right edge,
    // long labels / notes wrap instead of clipping, values parsed into the right column, control hints moved to the prompt line.
    {
        rmb::Snapshot& v=g_rematchView; v={}; v.flags=rmb::HostedReady|rmb::Open; v.workerTickMs=Now(); g_rematchFreshAt=Now();
        const char* hostRows[]={"Practice - Boss summoning (1.9.0) - active: native practice with a long gate reason text that wraps",
            "-- Options --", "> Wait before attacking: 5 s     Left / Right", "  Summon into: Boss A (now: Tachibana Muneshige the Thunder God of Kyushu)",
            "  Cutscene on kill:        ON   A / Left / Right   (n/a: Final blow required is OFF)",
            "  0x00A1  RPT  Overhead slash into a delayed sweeping kick follow-up with grab (?)",
            "  0x4E30  REPEAT  Unlabelled action",  // user #13618: the repeating move row shows a REPEAT pill
            "      Teleport to arena (fast load)   - not beaten on this journey and some more words to force a wrap",
            "  Return to where I was   <- boss defeated: A takes you back", "  Arenas   A: open",
            "  Kashin Koji                              Kyoto      beaten     Native         unverified"};
        v.rowCount=(uint32_t)(sizeof hostRows/sizeof hostRows[0]);
        for(uint32_t i=0;i<v.rowCount;++i){strcpy_s(v.rows[i].text,hostRows[i]);v.rows[i].color=0x00B4B4B4;v.rows[i].selected=i==2;}
        strcpy_s(v.footer,"Seconds a summoned boss stays passive (AI off) before it attacks. 0 = at once. Applies to the next summon, saved.");
        strcpy_s(v.toast,"Summoned: Boss A"); v.toastColor=0x0060FF60;
        for(unsigned width:{800u,1280u,1920u,2560u,3840u}) {
            const unsigned height=width==800u?600u:width*9/16;
            osd::drawnRects.clear();osd::drawnTexts.clear();osd::drawnTextX.clear();osd::drawnTextScale.clear();
            const float sc=DrawRematchHost(width,height); CHECK(sc>0 && !osd::drawnRects.empty());
            const auto panel=osd::drawnRects[0]; const float cw=osd::kGlyphW*sc;
            CHECK(panel.w/cw>=58.f && panel.x>=(float)width*0.5f && panel.h<=0.70f*(float)height);
            for(const auto& r:osd::drawnRects)CHECK(r.x>=panel.x-1&&r.y>=panel.y-1&&r.x+r.w<=panel.x+panel.w+1&&r.y+r.h<=panel.y+panel.h+1);
            bool sawPill=false,sawWrap=false,sawHeader=false,sawRepeat=false;
            for(size_t i=0;i<osd::drawnTexts.size();++i){
                if(!(osd::drawnTextX[i]>=panel.x && osd::drawnTextX[i]+(float)osd::drawnTexts[i].size()*cw<=panel.x+panel.w+0.5f))printf("W%u x=%.1f px=%.1f pw=%.1f cw=%.1f [%s]\n",width,osd::drawnTextX[i],panel.x,panel.w,cw,osd::drawnTexts[i].c_str());
                CHECK(osd::drawnTextX[i]>=panel.x && osd::drawnTextX[i]+(float)osd::drawnTexts[i].size()*cw<=panel.x+panel.w+0.5f);
                CHECK(osd::drawnTexts[i].find("Left / Right")==std::string::npos && osd::drawnTexts[i].find("A: open")==std::string::npos);
                sawPill|=osd::drawnTexts[i]=="ON"||osd::drawnTexts[i]=="RPT"; sawRepeat|=osd::drawnTexts[i]=="REPEAT"; sawWrap|=osd::drawnTexts[i].find("Kyushu)")!=std::string::npos;
                sawHeader|=osd::drawnTexts[i]=="Options"; CHECK(osd::drawnTexts[i].find("--")==std::string::npos);
            }
            if(!sawPill||!sawWrap){printf("pill %d wrap %d\n",sawPill,sawWrap);for(auto&t:osd::drawnTexts)printf("  [%s]\n",t.c_str());}
            CHECK(sawPill && sawWrap && sawHeader && sawRepeat);
            { uint32_t bg=0,fg=0; CHECK(menu_style::PillColors("REPEAT",bg,fg) && bg==menu_style::kRptBg); }
        }
        g_rematchFreshAt=0; v={};
    }
    puts("ALL PASSED: real settings pages, legacy order, back priority, paging, keys and pad isolation");
    return 0;
}
