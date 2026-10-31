#define _CRT_SECURE_NO_WARNINGS
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
struct Rect { float x,y,w,h; uint32_t color; };
std::vector<Rect> drawnRects;
std::vector<std::string> drawnTexts;
void DrawRect(float x, float y, float w, float h, uint32_t color) { drawnRects.push_back({x,y,w,h,color}); }
bool DrawCoverRect(float,float){return true;}
std::vector<float> drawnTextX, drawnTextScale, drawnTextY;
std::vector<uint32_t> drawnTextColor;
float DrawText(float x, float y, const char* text, uint32_t color, float s) { drawnTextY.push_back(y);drawnTextColor.push_back(color);drawnTexts.emplace_back(text);drawnTextX.push_back(x);drawnTextScale.push_back(s);return x; }
}
namespace savefiles {
bool testAllowed=false,testPending=false;int requests=0,cancels=0;
bool Pending(){return testPending;}
bool LivePending(){return false;}
void CancelPending(){if(testPending){++cancels;testPending=false;}}
void Configure(uint8_t*,bool,const wchar_t*,void (*)(const char*,...)){}
void Poll(){} bool Allowed(){return testAllowed;}
bool ActionAllowed(const char*){return testAllowed;}
std::string queuedName;
void RefreshSlots(){}
int SlotCount(){return 3;}
const char* SlotName(int i){static const char* names[]={"New Game","Yoritomo Practice","slot1"};return i>=0&&i<3?names[i]:"";}
bool QueueNamed(const char* name,bool){if(!testAllowed)return false;queuedName=name;++requests;return true;}
bool QueueNewBackup(){return QueueNamed("Backup-test",true);}
bool TakeMenuCloseRequest(){return false;}
bool testLive=false;std::string liveName;bool LiveAvailable(){return testLive;}
bool QueueLive(const char* name){if(!testAllowed||!testLive)return false;liveName=name;return true;}
bool Queue(int,bool){return false;}
bool Native(){return true;}
void OpenFolder(){}
bool IsActive(int){return false;}
int Progress(char*,size_t){return -1;}
const char* Note(){return testPending?"Switch queued: copy slot1.":"Copy-only test: choose a slot, then queue and quit normally.";}
}
namespace updatecheck {
std::string testNotice;
int pageOpens=0;
bool wine=false;
std::string folderNotice;
bool RunningUnderWine(){return wine;}
bool CopyFolderNotice(char* dst,size_t n,bool focused){snprintf(dst,n,"%s",focused?folderNotice.c_str():"");return focused&&!folderNotice.empty();}
void OpenPage() { ++pageOpens; }
bool CopyNotice(char* dst, size_t n) { snprintf(dst,n,"%s",testNotice.c_str());return !testNotice.empty(); }
}
namespace gpuopt {
State GetState() { return State::Off; }
bool Batching() { return false; }
int BarriersIni() { return 0; }
bool Rewrite(int) { return false; }
void SetBatching(bool) {}
void SetRewrite(int, bool) {}
}
namespace crashreport {  // #38437 / #43461: the Report row opens the folder and the website
int g_snapshots = 0, g_urlOpens = 0, g_folderOpens = 0;
char g_folderPath[260] = {};
bool Snapshot(char* p, unsigned cap) { ++g_snapshots; snprintf(p, cap, "stub-report.txt"); return true; }
const char* ReportUrl() { return "https://example.invalid/report"; }
bool ReportOpenWanted() { return false; }
bool CopyReportUrl() { return true; }
void OpenReportUrl() { ++g_urlOpens; }
void OpenReportFolder(const char* path) { ++g_folderOpens; snprintf(g_folderPath, sizeof g_folderPath, "%s", path ? path : ""); }
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
int g_fm = 0, g_kw = 0, g_psv[35] = {};  // g_psv: the other Training switches (frame-meter master rows)
int HarnessPstGet(int what) { return what == 7 ? g_fm : what == 21 ? g_kw : what >= 0 && what < 35 ? g_psv[what] : -1; }
int HarnessPstUnavailable(int) { return -1; }
int g_skyPersist = 0;
int HarnessPstSet(int what, int value, int persist) {
    int* p = what == 7 ? &g_fm : what == 21 ? &g_kw : what >= 0 && what < 35 ? &g_psv[what] : nullptr;
    if (!p) return -1;
    if (what == 12 || what == 13) { *p = std::clamp(value, 0, what == 12 ? 48 : 5); g_skyPersist += persist != 0; }
    else if (what == 34) *p=std::clamp(value,0,2);
    else *p = value < 0 ? !*p : (value ? 1 : 0);
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
    { using namespace menu;
      int rows[kMenuRowCap];g_pages.Reset();savefiles::testAllowed=false;
      int n=PageRows(rows,true);bool visible=false;for(int i=0;i<n;++i)visible|=rows[i]==kSaveFilesRow;CHECK(visible);
      Change(kSaveFilesRow,0);CHECK(g_pages.page==pRoot);
      savefiles::testAllowed=true;n=PageRows(rows,true);bool found=false;for(int i=0;i<n;++i)found|=rows[i]==kSaveFilesRow;CHECK(found);
      Change(kSaveFilesRow,0);CHECK(g_pages.page==pSaveFiles);
      n=PageRows(rows,true);for(int i=0;i<3;++i){bool seen=false;for(int j=0;j<n;++j)seen|=rows[j]==kSaveNamedFirst+i;CHECK(seen);}
      Change(kSaveNamedFirst+1,0);CHECK(g_pages.page==pSaveFiles);Change(kSaveNamedFirst+1,0);CHECK(g_pages.page==pSaveActions);  // A twice (65817b40)CHECK(g_saveName=="Yoritomo Practice");
      Change(kSaveSwitchRow,0);CHECK(savefiles::requests==0);
      savefiles::testAllowed=false;Change(kSaveSwitchRow,0);CHECK(savefiles::requests==0);
      savefiles::testAllowed=true;Change(kSaveSwitchRow,0);Change(kSaveSwitchRow,0);CHECK(savefiles::requests==1);CHECK(savefiles::queuedName=="Yoritomo Practice");
      n=PageRows(rows,true);for(int i=0;i<n;++i)CHECK(rows[i]!=kSaveCancelRow+1);
      Change(kSaveCancelRow+1,0);CHECK(savefiles::requests==1);
      CHECK(savefiles::queuedName=="Yoritomo Practice");
      n=PageRows(rows,true);{bool live=false;for(int i=0;i<n;++i)live|=rows[i]==kSaveLiveRow;CHECK(!live);}
      savefiles::testLive=true;n=PageRows(rows,true);{bool live=false;for(int i=0;i<n;++i)live|=rows[i]==kSaveLiveRow;CHECK(live);}
      Change(kSaveLiveRow,0);CHECK(savefiles::liveName=="Yoritomo Practice");savefiles::testLive=false;savefiles::liveName.clear();
      savefiles::testAllowed=false;savefiles::testPending=true;g_pages.Reset();
      Change(kSaveFilesRow,0);CHECK(g_pages.page==pSaveFiles);
      n=PageRows(rows,true);bool cancelFound=false;
      for(int i=0;i<n;++i){cancelFound|=rows[i]==kSaveCancelRow;CHECK(rows[i]!=kSaveSwitchRow&&rows[i]!=kSaveBackupRow);}
      CHECK(cancelFound);Change(kSaveSwitchRow,0);CHECK(savefiles::requests==1);
      Change(kSaveCancelRow,0);CHECK(!savefiles::testPending&&savefiles::cancels==1);
      savefiles::testAllowed=false;g_pages.Reset();g_saveConfirm=0;
    }

    using namespace menu;
    // Update hint is fitted to small viewports and leaves the ordinary reminder intact.
    updatecheck::testNotice = "Update 1.8.0 available: Mod Menu > Update + Page";
    osd::drawnRects.clear(); osd::drawnTexts.clear();
    DrawCornerNote(800, 600, "Mod menu: F10 / Start x3", 0xFFFFFFFF);
    bool updateSeen=false, urlSeen=false, folderSeen=false;
    for (const auto& text : osd::drawnTexts) { updateSeen |= text == updatecheck::testNotice; urlSeen |= text == updatecheck::kSite; folderSeen |= text == "Click: website + updater folder"; }
    CHECK(updateSeen && !urlSeen && !folderSeen);
    CHECK(osd::drawnTexts.size() == 2);
    for (const auto& rect : osd::drawnRects) CHECK(rect.x >= 0 && rect.x + rect.w <= 800.01f);
    for (unsigned width : {320u, 800u, 1280u}) {
        osd::drawnRects.clear(); osd::drawnTexts.clear();
        DrawCornerNote(width, 600, "Mod menu: F10 / Start x3", 0xFFFFFFFF);
        CHECK(osd::drawnTexts.size() == 2 && osd::drawnTexts[1] == updatecheck::testNotice);
        for (const auto& rect : osd::drawnRects) CHECK(rect.x >= 0 && rect.x + rect.w <= width + .01f);
    }
    updatecheck::testNotice.clear();
    osd::drawnRects.clear(); osd::drawnTexts.clear();

    CHECK(!TryUpdateHintClick(false,true,15,15,10,10,20,20));
    CHECK(!TryUpdateHintClick(true,false,15,15,10,10,20,20));
    CHECK(!TryUpdateHintClick(true,true,30,15,10,10,20,20));
    CHECK(TryUpdateHintClick(true,true,10,10,10,10,20,20));
    CHECK(updatecheck::pageOpens == 1);
    Change(kUpdatePageRow, 1); CHECK(updatecheck::pageOpens == 1);
    Change(kUpdatePageRow, 0); CHECK(updatecheck::pageOpens == 2);
    updatecheck::wine=true; g_open=1; g_pressed=K_NAV; g_toggleReq=1;
    Change(kUpdatePageRow,0); CHECK(!g_open && !g_pressed && !g_toggleReq);
    updatecheck::wine=false;
    {   // human #45507: Reverse Mods = A twice parks the mod files (saves / ini stay), A twice again puts them back
        wchar_t tmp[MAX_PATH]; GetTempPathW(MAX_PATH, tmp);
        g_reverseDir = std::wstring(tmp) + L"n3rev-" + std::to_wstring(GetCurrentProcessId()) + L"\\";
        CreateDirectoryW(g_reverseDir.c_str(), nullptr);
        auto mk = [](const std::wstring& p) { HANDLE h = CreateFileW(p.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr); CloseHandle(h); };
        auto ex = [](const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; };
        for (const wchar_t* f : {L"dinput8.dll", L"Nioh3EngineFix.asi", L"Nioh3Training.asi", L"Nioh3EngineFix.ini", L"SAVEDATA.BIN"}) mk(g_reverseDir + f);
        Change(kReverseModsRow, 1); CHECK(ex(g_reverseDir + L"dinput8.dll"));           // Left/Right never act
        Change(kReverseModsRow, 0); CHECK(ex(g_reverseDir + L"dinput8.dll"));           // first A only arms the confirm
        Change(kReverseModsRow, 0);                                                      // second A turns the mod off
        CHECK(reversemods::IsOff(g_reverseDir) && ex(g_reverseDir + L"Nioh3EngineFix.asi.mod-off") && ex(g_reverseDir + L"Nioh3Training.asi.mod-off"));
        CHECK(ex(g_reverseDir + L"Nioh3EngineFix.ini") && ex(g_reverseDir + L"SAVEDATA.BIN") && strstr(g_reverseNote, "OFF"));
        Change(kReverseModsRow, 0); Change(kReverseModsRow, 0);                          // A twice again puts it back
        CHECK(!reversemods::IsOff(g_reverseDir) && ex(g_reverseDir + L"dinput8.dll") && ex(g_reverseDir + L"Nioh3Training.asi") && !ex(g_reverseDir + L"dinput8.dll.mod-off"));
        mk(g_reverseDir + L"Nioh3Training.asi.mod-off");                                 // a stale parked copy blocks: nothing changes
        CHECK(reversemods::Move(g_reverseDir, true) == -1 && ex(g_reverseDir + L"dinput8.dll") && ex(g_reverseDir + L"Nioh3EngineFix.asi") && !ex(g_reverseDir + L"dinput8.dll.mod-off"));
        for (const wchar_t* f : {L"dinput8.dll", L"Nioh3EngineFix.asi", L"Nioh3Training.asi", L"Nioh3Training.asi.mod-off", L"Nioh3EngineFix.ini", L"SAVEDATA.BIN"}) DeleteFileW((g_reverseDir + f).c_str());
        RemoveDirectoryW(g_reverseDir.c_str()); g_reverseDir.clear(); g_reverseConfirmAt = 0;
    }
    updatecheck::folderNotice="Game folder: /home/player/Steam Games/Nioh3/";
    g_open=1; g_cfg.pauseHint=0;
    osd::drawnTexts.clear(); DrawFolderNotice(320,600,1,true);
    CHECK(!osd::drawnTexts.empty());
    std::string pathText; for(const auto& row:osd::drawnTexts)pathText+=row;
    CHECK(pathText==updatecheck::folderNotice);
    osd::drawnTexts.clear(); DrawFolderNotice(320,600,1,false);CHECK(osd::drawnTexts.empty());
    updatecheck::folderNotice.clear(); g_open=0;
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
    // #38437 top rows: Mod menu, Close, Report; then Display Overlays, Camera settings, Settings, Close
    CHECK(rootN == 10 && rows[0] == kModMenuRow && rows[1] == kCloseTopRow && rows[2] == kReportRow && rows[3] == kPageRow + pDisplay &&
          rows[4] == kPageRow + pCamera && rows[5] == kPageRow + pSettings && rows[6] == kSaveFilesRow && rows[7] == kUpdatePageRow && rows[8] == kReverseModsRow && rows[9] == kClose);
    CHECK(strcmp(PageName(pDisplay), "Display Overlays") == 0 && strcmp(PageName(pFrameGen), "Frame Gen") == 0);
    // every page keeps the three top rows first and ends with Back (stable layout: nothing else is added dynamically)
    for (int p : {pSettings, pDisplay, pOvController, pOvFrameMeter, pOvInput, pOvFps, pOvHud, pCamera, pSCpu, pSGpu, pSGame, pFrameGen}) {
        g_pages.page = p;
        const int n = PageRows(rows, true);
        CHECK(n >= 4 && rows[0] == kModMenuRow && rows[1] == kCloseTopRow && rows[2] == kReportRow && rows[n - 1] == kBackRow);
    }
    g_pages.Reset();
    // Display Overlays: four sub-pages, global sky controls and loading-screen auto-click
    Change(kPageRow + pDisplay, 0);
    {
        const int n = PageRows(rows, true);
        CHECK(g_pages.page == pDisplay && n == 12 && rows[3] == kPageRow + pOvController && rows[4] == kPageRow + pOvFrameMeter &&
              rows[5] == kPageRow + pOvInput && rows[6] == kPageRow + pOvFps && rows[7] == kTimeOfDayRow && rows[8] == kWeatherRow &&
              rows[9] == kPageRow + pOvHud && rows[10] == kAutoCont && rows[11] == kBackRow);
    }
    {   // 'Game HUD >' page: master Hide game HUD on top, then every per-element row once (hudhide::kElems), then Back
        const int saved = g_pages.page;
        g_pages.page = pOvHud;
        const int n = PageRows(rows, true);
        CHECK(n == 3 + 1 + hudhide::kElemN + 1 && rows[3] == HideHudSettingRow() && rows[n - 1] == kBackRow);
        for (int e = 0; e < hudhide::kElemN; ++e) {
            int seen = 0;
            for (int r = 4; r < n - 1; ++r) seen += !strcmp(kConfigSettings[rows[r] - kSettingRow].key, hudhide::kElems[e].key);
            CHECK(seen == 1);
        }
        g_pages.page = saved;
    }
    // FPS page: FPS rows and timing diagnostics, no performance rows
    g_pages.page = pOvFps;
    {
        const int n = PageRows(rows, false);
        bool fps = false, xts = false, perf = false;
        for (int i = 0; i < n; ++i) { fps |= rows[i] == kFps; xts |= rows[i] == kXTS; perf |= rows[i] == kLat || rows[i] == kEpPre; }
        CHECK(fps && xts && !perf);
    }
    // Controller / Input display pages split the config keys
    auto cfgIndex = [&](const char* key) { for (int i = 0; i < kConfigN; ++i) if (!strcmp(kConfigSettings[i].key, key)) return i; return -1; };
    const int pad = cfgIndex("ControllerDisplay"), padColor = cfgIndex("ControllerDisplayPressColor"), style = cfgIndex("ButtonStyle");
    const int inp = cfgIndex("InputDisplay"), inpRows = cfgIndex("InputDisplayRows");
    CHECK(pad >= 0 && padColor >= 0 && style >= 0 && inp >= 0 && inpRows >= 0);
    auto has = [&](int page, int id) {
        g_pages.page = page;
        const int n = PageRows(rows, false);
        for (int i = 0; i < n; ++i) if (rows[i] == id) return true;
        return false;
    };
    CHECK(has(pOvController, kSettingRow + pad) && has(pOvController, kSettingRow + padColor) && has(pOvController, kSettingRow + style));
    CHECK(!has(pOvController, kSettingRow + inp) && has(pOvInput, kSettingRow + inp) && has(pOvInput, kSettingRow + inpRows) && !has(pOvInput, kSettingRow + pad));
    // Frame meter page: meter, Ki window and every Training toggle row
    {
        g_pages.page = pOvFrameMeter;
        const int n = PageRows(rows, true);
        CHECK(n == 3 + 5 + kPsToggleN + 2 && rows[3] == kFrameMeterRow && rows[4] == kFrameMeterStyleRow && rows[5] == kFrameMeterSizeRow && rows[6] == kPsAllRowsRow && rows[7] == kKiWindowRow && rows[8] == kPsRowFirst &&
              rows[15] == kPsAllKeysRow && rows[16] == kPsRowFirst + 7 && rows[n - 2] == kPsRowFirst + kPsToggleN - 1);
    }
    g_pstGet = HarnessPstGet;
    g_pstSet = HarnessPstSet;
    CHECK(has(pOvFrameMeter,kFrameMeterSizeRow) && kFrameMeterSizeWhat==34);
    Change(kFrameMeterSizeRow,1);CHECK(g_psv[34]==1);
    Change(kFrameMeterSizeRow,1);CHECK(g_psv[34]==2);
    Change(kFrameMeterSizeRow,1);CHECK(g_psv[34]==2);
    Change(kFrameMeterSizeRow,0);CHECK(g_psv[34]==0);
    Change(kFrameMeterSizeRow,-1);CHECK(g_psv[34]==0);
    // Style is persistent, directional, append-only, and unavailable on an older Training module.
    CHECK(has(pOvFrameMeter,kFrameMeterStyleRow) && !has(pDisplay,kFrameMeterStyleRow));
    CHECK(kFrameMeterStyleWhat==33 && g_psv[33]==0);
    Change(kFrameMeterStyleRow,1); CHECK(g_psv[33]==1);
    Change(kFrameMeterStyleRow,1); CHECK(g_psv[33]==1);
    Change(kPsAllRowsRow,-1); CHECK(g_psv[33]==1);
    Change(kPsAllKeysRow,1); CHECK(g_psv[33]==1);
    Change(kFrameMeterStyleRow,-1); CHECK(g_psv[33]==0);
    Change(kFrameMeterStyleRow,0); CHECK(g_psv[33]==1);
    g_pstGet=HarnessPstUnavailable;
    Change(kFrameMeterStyleRow,-1); CHECK(g_psv[33]==1 && strstr(g_psNote,"Needs"));
    g_pstGet=HarnessPstGet; Change(kFrameMeterStyleRow,-1); CHECK(g_psv[33]==0);
    for(int i=0;i<35;++i)g_psv[i]=0; g_psNote[0]=0;
    // Display sky sliders use the persistent Training ABI, including midnight/noon and endpoint clamps.
    CHECK(has(pDisplay, kTimeOfDayRow) && has(pDisplay, kWeatherRow));
    CHECK(!has(pOvFrameMeter, kTimeOfDayRow) && !has(pSettings, kWeatherRow));
    char skyText[32];
    SkyValueText(kTimeOfDayRow, 0, skyText, sizeof(skyText)); CHECK(!strcmp(skyText, "Stock"));
    Change(kTimeOfDayRow, 1); CHECK(g_psv[12] == 1 && g_skyPersist == 1);
    SkyValueText(kTimeOfDayRow, g_psv[12], skyText, sizeof(skyText)); CHECK(!strcmp(skyText, "12:00 am"));
    Change(kTimeOfDayRow, 1); CHECK(g_psv[12] == 2);
    SkyValueText(kTimeOfDayRow, 2, skyText, sizeof(skyText)); CHECK(!strcmp(skyText, "12:30 am"));
    SkyValueText(kTimeOfDayRow, 25, skyText, sizeof(skyText)); CHECK(!strcmp(skyText, "12:00 pm"));
    SkyValueText(kTimeOfDayRow, 48, skyText, sizeof(skyText)); CHECK(!strcmp(skyText, "11:30 pm"));
    g_psv[12] = 48; Change(kTimeOfDayRow, 1); CHECK(g_psv[12] == 48);
    Change(kTimeOfDayRow, -1); CHECK(g_psv[12] == 47);
    Change(kTimeOfDayRow, 0); CHECK(g_psv[12] == 0);
    const int saved = g_skyPersist; Change(kTimeOfDayRow, -1); CHECK(g_psv[12] == 0 && g_skyPersist == saved);
    const char* weather[] = {"Stock", "Clear", "Overcast", "Rain", "Storm", "Snow"};
    for (int i = 0; i < 6; ++i) { SkyValueText(kWeatherRow, i, skyText, sizeof(skyText)); CHECK(!strcmp(skyText, weather[i])); }
    for (int i = 0; i < 9; ++i) Change(kWeatherRow, 1);
    CHECK(g_psv[13] == 5); Change(kWeatherRow, -1); CHECK(g_psv[13] == 4);
    Change(kWeatherRow, 0); CHECK(g_psv[13] == 0);
    g_cfg.layout = 1; CHECK(has(pDisplay, kTimeOfDayRow) && has(pDisplay, kWeatherRow)); g_cfg.layout = 0;
    g_pstGet = HarnessPstUnavailable;  // installed Training is missing/too old: no speculative writes
    const int noWrite = g_skyPersist;
    Change(kTimeOfDayRow, 1); Change(kWeatherRow, 1);
    CHECK(g_psv[12] == 0 && g_psv[13] == 0 && g_skyPersist == noWrite);
    CHECK(strstr(g_skyNote, "Needs Nioh3Training"));
    SkyValueText(kTimeOfDayRow, -1, skyText, sizeof(skyText)); CHECK(!strcmp(skyText, "needs Training"));
    g_pstGet = HarnessPstGet; g_skyNote[0] = 0;
    // the masters moved from Practice: Right = all ON, Left = all OFF (the boss meter is in the rows set)
    Change(kPsAllRowsRow, 1);
    CHECK(g_fm == 1 && PsMasterState(false) == 1);
    Change(kPsAllRowsRow, -1);
    CHECK(g_fm == 0 && PsMasterState(false) == 0);
    g_psv[24] = 1;
    CHECK(PsMasterState(true) == 2);
    Change(kPsAllKeysRow, 0);
    CHECK(PsMasterState(true) == 1 && g_psv[18] == 1 && g_psv[32] == 1 && g_psv[10] == 0);
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
    // Camera page: the existing camera-effects option
    CHECK(has(pCamera, kEp9) && !has(pCamera, kLat));
    // The Camera page always draws a panel (every row + Back) at several screen sizes, and B leaves it
    {
        g_pages.Reset(); g_sel = TopLevelSel();
        const LONG wasOpen = g_open; g_open = 1;
        EnterPage(pCamera);
        CHECK(g_pages.page == pCamera && g_sel >= kCamRowFirst && g_sel < kCamRowFirst + kCamRowN);
        for (unsigned width : {800u, 1280u, 1920u, 3840u}) {
            const unsigned height = width == 800u ? 600u : width * 9 / 16;
            osd::drawnRects.clear(); osd::drawnTexts.clear();
            CHECK(DrawImpl(width, height) > 0 && !osd::drawnRects.empty() && !osd::drawnTexts.empty());
            bool title = false, back = false, reset = false;
            for (const auto& t : osd::drawnTexts) { title |= t == "Camera settings"; back |= t == "Back"; reset |= t == "Return to default camera"; }
            if (!(title && back && reset)) { printf("camera page W%u title %d back %d reset %d\n", width, title, back, reset); for (auto& t : osd::drawnTexts) printf("  [%s]\n", t.c_str()); }
            CHECK(title);
        }
        for (int i = 0; i < 20; ++i) Move(+1);  // walk the whole page incl. wrap, drawing each step
        for (int i = 0; i < 14; ++i) { Move(+1); osd::drawnTexts.clear(); CHECK(DrawImpl(1280, 720) > 0 && !osd::drawnTexts.empty()); }
        g_sel = kBackRow; osd::drawnTexts.clear();
        CHECK(DrawImpl(1280, 720) > 0);
        bool back = false; for (const auto& t : osd::drawnTexts) back |= t == "Back";
        CHECK(back);
        Back();
        CHECK(g_pages.page == pRoot && g_open);
        g_open = wasOpen; g_pages.Reset(); g_sel = TopLevelSel();
    }
    // Camera controls write the real probe keys, preserve the other axis, and reset both.
    {
        g_camProbePresentForTest = 0;
        Change(kCamRowFirst + kCamRight, -1);
        CHECK(g_camRight == 0.f && strstr(g_camNote, "CamKiProbe"));
        g_pages.Reset(); EnterPage(pCamera); g_sel = kCamRowFirst + kCamRight; g_open = 1;
        osd::drawnTexts.clear();
        CHECK(DrawImpl(1920, 1080) > 0);
        bool need = false;
        for (const auto& t : osd::drawnTexts) need |= t == "needs CamKiProbe add-on";
        CHECK(need);
        g_camProbePresentForTest = 1;
        wchar_t temp[MAX_PATH], folder[MAX_PATH];
        GetTempPathW(MAX_PATH, temp); GetTempFileNameW(temp, L"cam", 0, folder);
        DeleteFileW(folder); CHECK(CreateDirectoryW(folder, nullptr));
        const auto savedIni = g_ini;
        g_ini = std::wstring(folder) + L"\\EngineFix.ini";
        const auto probe = CamIniPath();
        WritePrivateProfileStringW(L"CamKiProbe", L"CamOffsetUp", L"80", probe.c_str());
        Change(kCamRowFirst + kCamRight, -1);
        CHECK(GetPrivateProfileIntW(L"CamKiProbe", L"CamOffsetRight", 999, probe.c_str()) == -20);
        CHECK(g_camUp == 80.f);
        for (int i = 0; i < 30; ++i) Change(kCamRowFirst + kCamRight, 1);
        CHECK(g_camRight == 400.f);
        Change(kCamRowFirst + kCamUp, -1); CHECK(g_camUp == 60.f);
        Change(kCamRowFirst + kCamRight, 0); CHECK(g_camRight == 0.f && g_camUp == 60.f);
        Change(kCamRowFirst + kCamReset, 0);
        CHECK(!CamAny());
        CHECK(GetPrivateProfileIntW(L"CamKiProbe", L"CamOffsetUp", 999, probe.c_str()) == 999);
        CHECK(GetPrivateProfileIntW(L"CamKiProbe", L"CamOffsetRight", 999, probe.c_str()) == 999);
        CHECK(has(pCamera, kCamRowFirst + kCamRight) && has(pCamera, kCamRowFirst + kCamUp));
        CHECK(kCamRowN == 5 && has(pCamera, kCamRowFirst + kCamDistance) && has(pCamera, kCamRowFirst + kCamLens));
        // Every slider crosses the actual unset center; restarting preserves saved values.
        for (int axis : {kCamRight, kCamUp, kCamDistance, kCamLens}) {
            Change(kCamRowFirst + axis, -1); CHECK(CamValue(axis) < 0.f);
            CamReadIni(); CHECK(CamValue(axis) < 0.f);
            Change(kCamRowFirst + axis, 1); CHECK(CamValue(axis) == 0.f && !CamAny());
            Change(kCamRowFirst + axis, 1); CHECK(CamValue(axis) > 0.f);
            Change(kCamRowFirst + axis, -1); CHECK(CamValue(axis) == 0.f && !CamAny());
            Change(kCamRowFirst + axis, 1); Change(kCamRowFirst + axis, 0); CHECK(!CamAny());
        }
        CHECK(GetPrivateProfileIntW(L"CamKiProbe", L"CamDist", 999, probe.c_str()) == 999);
        CHECK(GetPrivateProfileIntW(L"CamKiProbe", L"CamFovDeg", 999, probe.c_str()) == 999);
        Change(kCamRowFirst + kCamDistance, -1); CHECK(g_camDist == kCamDistDefault - kCamDistStep);
        Change(kCamRowFirst + kCamLens, 1); CHECK(g_camFov == kCamFovDefault + kCamFovStep);
        CHECK(g_camDist == kCamDistDefault - kCamDistStep); // preserve other axis
        for (int i = 0; i < 100; ++i) { Change(kCamRowFirst + kCamDistance, -1); Change(kCamRowFirst + kCamLens, -1); }
        CHECK(g_camDist == kCamDistMin && g_camFov == kCamFovMin);
        for (int i = 0; i < 200; ++i) { Change(kCamRowFirst + kCamDistance, 1); Change(kCamRowFirst + kCamLens, 1); }
        CHECK(g_camDist == kCamDistMax && g_camFov == kCamFovMax);
        Change(kCamRowFirst + kCamReset, 0); CHECK(!CamAny());
        // Old off-grid absolute overrides still reach zero rather than skipping it.
        WritePrivateProfileStringW(L"CamKiProbe", L"CamDist", L"470", probe.c_str());
        WritePrivateProfileStringW(L"CamKiProbe", L"CamFovDeg", L"46", probe.c_str());
        Change(kCamRowFirst + kCamDistance, 1); CHECK(g_camDist == 530.f);
        Change(kCamRowFirst + kCamDistance, 1); CHECK(g_camDist == 0.f);
        Change(kCamRowFirst + kCamLens, 1); CHECK(g_camFov == 0.f && !CamAny());
        DeleteFileW(probe.c_str()); RemoveDirectoryW(folder); g_ini = savedIni;
    }
    // Settings: CPU / GPU / Game / Frame Gen, performance only
    g_pages.Reset();
    {
        g_pages.page = pSettings;
        const int n = PageRows(rows, true);
        CHECK(n == 3 + 1 + 4 + 2 && rows[3] == kEpPre && rows[4] == kPageRow + pSCpu && rows[5] == kPageRow + pSGpu && rows[6] == kPageRow + pSGame && rows[7] == kPageRow + pFrameGen && rows[8] == kSave && rows[9] == kBackRow);
        CHECK(!has(pSettings, kFps) && !has(pSettings, kXTS) && !has(pSettings, kAutoCont) && !has(pSettings, kLat));
    }
    CHECK(has(pSCpu, kW3) && has(pSCpu, kXIK) && !has(pSCpu, kLat) && !has(pSCpu, kEpPre));
    CHECK(has(pSettings, kEpPre) && !has(pSGpu, kGpu) && /* kGpu is hidden while gpuopt is Off (stub) */ !has(pSGpu, kEpPre) && !has(pSGpu, kEp9) && has(pSGpu, kXVSMA) && has(pSGpu, kEp0));
    CHECK(has(pSGame, kLat) && has(pSGame, kVrr) && !has(pSGame, kW3));
    CHECK(has(pFrameGen, kF1) && !has(pFrameGen, kFps));
    // navigation: Move wraps through the top rows; Mod menu row returns to the root; Report row writes a snapshot via crashreport
    g_pages.Reset();
    Change(kPageRow + pSettings, 0);
    CHECK(g_pages.page == pSettings && g_sel == kEpPre);
    Move(-1);
    CHECK(g_sel == kReportRow);
    Move(-1);
    CHECK(g_sel == kCloseTopRow);
    Move(-1);
    CHECK(g_sel == kModMenuRow);
    Move(-1);
    CHECK(g_sel == kBackRow);
    Move(1);
    Change(kReportRow, 0);  // #38437: Report row writes one snapshot and keeps the page
    CHECK(crashreport::g_snapshots == 1 && crashreport::g_urlOpens == 1 && crashreport::g_folderOpens == 1
        && strstr(crashreport::g_folderPath, "stub-report.txt") && g_pages.page == pSettings
        && strstr(g_reportNote, "stub-report.txt") && strstr(g_reportNote, "example.invalid"));
    updatecheck::wine = true;
    Change(kReportRow, 0);
    CHECK(crashreport::g_snapshots == 2 && crashreport::g_urlOpens == 2 && crashreport::g_folderOpens == 1
        && strstr(g_reportNote, "stub-report.txt") && strstr(g_reportNote, "Wine"));
    updatecheck::wine = false;
    Change(kModMenuRow, 0);  // #38077: Mod menu = the top-level page
    CHECK(g_pages.page == pRoot && g_sel == kPageRow + pDisplay);
    Change(kPageRow + pSettings, 0);
    Change(kCloseTopRow, 0);  // #38077: Close closes the menu
    CHECK(!g_open);
    Open("harness");
    Close("harness");
    Open("harness");
    CHECK(g_sel == kPageRow + pDisplay);
    Close("harness");
    Open("harness");
    g_rematchSession = 7;
    int bossN = PageRows(rows, true);
    CHECK(bossN == 12 && rows[3] == kRematchRow && rows[4] == kPracticeRow && rows[5] == kPageRow + pDisplay &&
          rows[6] == kPageRow + pCamera && rows[7] == kPageRow + pSettings && rows[8] == kSaveFilesRow && rows[9] == kUpdatePageRow && rows[10] == kReverseModsRow && rows[11] == kClose);
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
    Move(1);
    CHECK(g_sel == kFps);
    MovePage(1);
    CHECK(g_sel == rows[13]);  // #38437: Mod menu + Close + Report sit above the 10 steps
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
    CHECK(PadNav(XINPUT_GAMEPAD_LEFT_SHOULDER | XINPUT_GAMEPAD_B) == (N_TAB_PREV | N_BACK));  // #44232: bumpers = tabs
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
    CHECK(g_pages.page==pRoot && n==10 && rows[3]==kPageRow+pSettings && rows[4]==kRematchRow && rows[5]==kPracticeRow && rows[6]==kSaveFilesRow && rows[7]==kUpdatePageRow && rows[8]==kReverseModsRow && rows[9]==kClose);
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
            "  1 : Anim 1.0s  1Hit3  0x4E4E\tAttack  [OFF]",  // human #44361: TAB = right column, type text + a state pill
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
            // #38505: the panel body is drawn as rounded rows top to bottom; its bounds = the union of that first run of rects
            auto panel=osd::drawnRects[0];
            for(size_t k=1;k<osd::drawnRects.size()&&osd::drawnRects[k].y>osd::drawnRects[k-1].y;++k){
                const auto& q=osd::drawnRects[k]; const float r=std::max(panel.x+panel.w,q.x+q.w),b=std::max(panel.y+panel.h,q.y+q.h);
                panel.x=std::min(panel.x,q.x); panel.w=r-panel.x; panel.h=b-panel.y;
            }
            const float cw=osd::kGlyphW*sc;
            CHECK(panel.w/cw>=58.f && panel.x>=(float)width*0.5f && panel.h<=0.70f*(float)height);
            for(const auto& r:osd::drawnRects)CHECK(r.x>=panel.x-1&&r.y>=panel.y-1&&r.x+r.w<=panel.x+panel.w+1&&r.y+r.h<=panel.y+panel.h+1);
            bool sawPill=false,sawWrap=false,sawHeader=false,sawRepeat=false,sawOn=false,sawOff=false,sawMoveLeft=false,sawType=false;
            for(size_t i=0;i<osd::drawnTexts.size();++i){
                if(!(osd::drawnTextX[i]>=panel.x && osd::drawnTextX[i]+(float)osd::drawnTexts[i].size()*cw<=panel.x+panel.w+0.5f))printf("W%u x=%.1f px=%.1f pw=%.1f cw=%.1f [%s]\n",width,osd::drawnTextX[i],panel.x,panel.w,cw,osd::drawnTexts[i].c_str());
                CHECK(osd::drawnTextX[i]>=panel.x && osd::drawnTextX[i]+(float)osd::drawnTexts[i].size()*cw<=panel.x+panel.w+0.5f);
                CHECK(osd::drawnTexts[i].find("Left / Right")==std::string::npos && osd::drawnTexts[i].find("A: open")==std::string::npos);
                sawPill|=osd::drawnTexts[i]=="ON"||osd::drawnTexts[i]=="RPT"; sawRepeat|=osd::drawnTexts[i]=="REPEAT"; sawWrap|=osd::drawnTexts[i].find("Kyushu)")!=std::string::npos;
                sawHeader|=osd::drawnTexts[i]=="Options"; CHECK(osd::drawnTexts[i].find("--")==std::string::npos);
                sawOn|=osd::drawnTexts[i]=="[ON]"; sawOff|=osd::drawnTexts[i]=="[OFF]"; sawType|=osd::drawnTexts[i]=="Attack";
                sawMoveLeft|=osd::drawnTexts[i]=="1 : Anim 1.0s  1Hit3  0x4E4E"; CHECK(osd::drawnTexts[i].find('\t')==std::string::npos && osd::drawnTexts[i]!="ON");
            }
            if(!sawPill||!sawWrap){printf("pill %d wrap %d\n",sawPill,sawWrap);for(auto&t:osd::drawnTexts)printf("  [%s]\n",t.c_str());}
            CHECK(sawPill && sawWrap && sawHeader && sawRepeat);
            if(!sawOn||!sawOff||!sawMoveLeft||!sawType){printf("on %d off %d left %d type %d\n",sawOn,sawOff,sawMoveLeft,sawType);for(auto&t:osd::drawnTexts)printf("  [%s]\n",t.c_str());}
            CHECK(sawOn && sawOff && sawMoveLeft && sawType);  // human #44361: ON / OFF as [ON] / [OFF] text markers, the move row split in two columns
            { uint32_t bg=0,fg=0; CHECK(menu_style::PillColors("[ON]",bg,fg) && bg==menu_style::kOnBg && fg==menu_style::kOnText); }
            { uint32_t bg=0,fg=0; CHECK(menu_style::PillColors("[OFF]",bg,fg) && bg==menu_style::kOffBg); }
            CHECK(!strcmp(menu_style::PillText("ON"),"[ON]") && !strcmp(menu_style::PillText("OFF"),"[OFF]") && !strcmp(menu_style::PillText("x1.00"),"x1.00"));
            static_assert((menu_style::kOnBg&0x00FF00FFu)!=0x0030003Cu, "the ON pill is not green");  // the ON pill is not green any more
            { uint32_t bg=0,fg=0; CHECK(menu_style::PillColors("REPEAT",bg,fg) && bg==menu_style::kRptBg); }
        }
        char* dump = nullptr; size_t dumpLen = 0;
        if (!_dupenv_s(&dump, &dumpLen, "N3_MENU_DUMP") && dump) {  // human #44361 preview: a move page drawn by the real host code, dumped for tests/menu_preview.py
            const char* moveRows[]={"Boss A - Minamoto no Yoritomo - 1 hit (5)", "-- 1 hit --",
                "  1 : Anim 1.2s  1Hit1  0x4E4E\tAttack  [ON]", "> 2 : Anim 0.9s  1Hit2  0x4E51\tAttack  [OFF]",
                "  3 : Anim 2.4s  1Hit3  0x4E55\tAttack  [REPEAT]", "  4 : Anim 1.6s  Grab1  0x4E7B\tGrab  [ON]",
                "  5 : Anim 3.1s  Unblockable2\tUnblockable  [ON]", "-- Boss settings --",
                "  Follow and target me: ON   A / Left / Right", "  Infinite health: OFF   A / Left / Right"};
            v.rowCount=(uint32_t)(sizeof moveRows/sizeof moveRows[0]);
            for(uint32_t i=0;i<v.rowCount;++i){strcpy_s(v.rows[i].text,moveRows[i]);v.rows[i].color=i==4?0x0040E6FF:0x00B4B4B4;v.rows[i].selected=i==3;}
            strcpy_s(v.footer,"0x4E51  Overhead slash. A: repeat | Left: block | Right: allow."); v.toast[0]=0;
            osd::drawnRects.clear();osd::drawnTexts.clear();osd::drawnTextX.clear();osd::drawnTextY.clear();osd::drawnTextColor.clear();osd::drawnTextScale.clear();
            const float sc=DrawRematchHost(1920u,1080u);
            FILE* f=nullptr;
            if (!fopen_s(&f,dump,"w") && f) {
                fprintf(f,"S %g %d %d\n",sc,osd::kGlyphW,osd::kGlyphH);
                for(const auto& r:osd::drawnRects)fprintf(f,"R %g %g %g %g %08X\n",r.x,r.y,r.w,r.h,r.color);
                for(size_t i=0;i<osd::drawnTexts.size();++i)fprintf(f,"T %g %g %g %08X %s\n",osd::drawnTextX[i],osd::drawnTextY[i],osd::drawnTextScale[i],osd::drawnTextColor[i],osd::drawnTexts[i].c_str());
                fclose(f);
            }
        }
        free(dump);
        g_rematchFreshAt=0; v={};
    }
    // Y/Triangle closes root, nested and hosted menus, but never steals edit input.
    g_rematchSession=0;g_rematchPage=false;g_rematchAwait=false;g_cfg.holdMask=0;g_dirty=false;
    g_disabled=0;g_pressed=0;g_toggleReq=0;
    for(auto& closePad:g_pad)closePad={};for(int i=0;i<4;++i){g_padHeld[i]=0;g_padHeldAt[i]=0;}g_keyHeld=0;g_keyHeldAt=0;
    CHECK(PadNav(XINPUT_GAMEPAD_Y)==N_CLOSE);
    Open("Y root");g_pressed=N_CLOSE;OnPresentImpl();CHECK(!g_open);
    Open("Y nested");EnterPage(pCamera);g_pressed=N_CLOSE;OnPresentImpl();CHECK(!g_open);
    Open("edit");CHECK(TryPadClose(N_CLOSE|N_RIGHT,Now())&&g_open);
    CHECK(TryPadClose(N_CLOSE|N_OK,Now())&&g_open);
    g_padHeld[0]=XINPUT_GAMEPAD_DPAD_LEFT;g_padHeldAt[0]=Now();CHECK(TryPadClose(N_CLOSE,Now())&&g_open);g_padHeld[0]=0;
    g_keyHeld=K_RIGHT;g_keyHeldAt=Now();CHECK(TryPadClose(N_CLOSE,Now())&&g_open);g_keyHeld=0;
    g_rematchPage=true;g_rematchView.flags=rmb::Open|rmb::Editing;
    CHECK(TryPadClose(N_CLOSE,Now())&&g_open); // explicit hosted text/slider edit
    g_rematchView.flags=rmb::Open;g_rematchAwait=true;CHECK(TryPadClose(N_CLOSE,Now())&&g_open);
    g_rematchAwait=false;CHECK(TryPadClose(N_CLOSE,Now())&&!g_open&&!g_rematchPage);
    // The close press cannot leak into gameplay until released; later Y passes unchanged.
    g_lastPresent=Now();XINPUT_STATE ypad{};ypad.Gamepad.wButtons=XINPUT_GAMEPAD_Y;
    PadFeed(0,&ypad,false);CHECK(ypad.Gamepad.wButtons==0);
    ypad={};PadFeed(0,&ypad,false);ypad.Gamepad.wButtons=XINPUT_GAMEPAD_Y;
    PadFeed(0,&ypad,false);CHECK(ypad.Gamepad.wButtons==XINPUT_GAMEPAD_Y);
    g_pressed=N_CLOSE;OnPresentImpl();CHECK(!g_open); // closed menu ignores Y
    // Existing Badge() maps Y to the correct glyph without spelling Triangle as text.
    for(int closeStyle=0;closeStyle<2;++closeStyle){menu_style::g_btnStyle=closeStyle;osd::drawnTexts.clear();
        menu_style::Badge(10,10,"Y",1,8,1,1);
        bool sawY=false;for(auto& text:osd::drawnTexts)sawY|=text=="Y";CHECK(sawY==(closeStyle==0));}
    // Inline action values use the same controller glyphs as the footer.
    for (int actionStyle = 0; actionStyle < 2; ++actionStyle) {
        menu_style::g_btnStyle = actionStyle;
        osd::drawnTexts.clear(); osd::drawnRects.clear();
        g_open = true; g_rematchPage = false; g_pages.Reset(); g_sel = kPageRow + pDisplay;
        g_cfg.layout = 0; DrawImpl(1920,1080);
        bool actionText = false;
        for (auto& text : osd::drawnTexts) {
            CHECK(text.find("Cross:") == std::string::npos);
            CHECK(text.find("A: open") == std::string::npos);
            actionText |= text == "open";
        }
        CHECK(actionText); CHECK(!osd::drawnRects.empty());
    }
    menu_style::g_btnStyle=0;
    char previewBase[1024]{};
    if(GetEnvironmentVariableA("N3_Y_CLOSE_PREVIEW",previewBase,sizeof previewBase)){
      for(int previewStyle=0;previewStyle<2;++previewStyle){
        g_open=true;g_rematchPage=false;g_cfg.layout=0;g_pages.Reset();g_sel=kPageRow+pDisplay;menu_style::g_btnStyle=previewStyle;
        if(GetEnvironmentVariableA("N3_SAVE_PREVIEW",nullptr,0)){
          savefiles::testAllowed=true;savefiles::testPending=previewStyle==1;
          EnterPage(pSaveFiles);g_sel=previewStyle==1?kSaveCancelRow:kSaveNamedFirst+1;
          if(GetEnvironmentVariableA("N3_SAVE_ACTIONS_PREVIEW",nullptr,0)){
            savefiles::testPending=false;g_saveName="Yoritomo Practice";EnterPage(pSaveActions);savefiles::testLive=true;g_sel=kSaveLiveRow;
          }
        }
        osd::drawnRects.clear();osd::drawnTexts.clear();osd::drawnTextX.clear();osd::drawnTextY.clear();osd::drawnTextScale.clear();osd::drawnTextColor.clear();
        DrawImpl(1920,1080);char path[1100];snprintf(path,sizeof path,"%s-%d.tsv",previewBase,previewStyle);
        FILE* f=nullptr;fopen_s(&f,path,"w");CHECK(f);
        for(auto& r:osd::drawnRects)fprintf(f,"R\t%f\t%f\t%f\t%f\t%u\n",r.x,r.y,r.w,r.h,r.color);
        for(size_t i=0;i<osd::drawnTexts.size();++i)fprintf(f,"T\t%f\t%f\t%f\t%u\t%s\n",osd::drawnTextX[i],osd::drawnTextY[i],osd::drawnTextScale[i],osd::drawnTextColor[i],osd::drawnTexts[i].c_str());
        fclose(f);
      }
    }
    puts("ALL PASSED: real settings pages, legacy order, back priority, paging, keys and pad isolation");
    return 0;
}
