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
bool DrawCoverRect(float,float){return true;}
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
    CHECK(found);
    Change(kRematchRow,0);
    CHECK(g_rematchPage && g_rematchAwait && hf::posted==rmb::OpenMenu);
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
    puts("ALL PASSED: real settings pages, legacy order, back priority, paging, keys and pad isolation");
    return 0;
}
