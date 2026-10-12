// Test the real rematch UI model and the hook's input decision helper without game calls.
#include "../src/rematch.cpp"
#include "../src/ui_input.h"

namespace addon {
Image g_img = {};
void Log(const char*, ...) {}
wchar_t testIni[MAX_PATH]{};
const wchar_t* IniPath() { return testIni; }
const wchar_t* ModDir() { return L""; }
int IniInt(const wchar_t* section, const wchar_t* key, int def) { return testIni[0] ? (int)GetPrivateProfileIntW(section,key,def,testIni) : def; }
bool uiReads = false;
bool Copy(void* out, uint64_t address, size_t n) {
    if (!uiReads) return false;
    __try { memcpy(out, reinterpret_cast<const void*>(address), n); return true; }
    __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool WrU32(uint64_t, uint32_t) { return false; }
bool WrBytes(uint64_t, const void*, size_t) { return false; }
void Hex(char*, size_t, uint64_t, size_t) {}
uintptr_t Site(const char*) { return 0; }
bool GroupOk(const char*, const char**) { return false; }
void OnEntry(const char*, Handler) {}
bool InstallGroup(const char*, const char**) { return false; }
bool InstallPump(void (*)(), const char**) { return false; }
bool InstallCallRetarget(const char*, void*, void**, const char**) { return false; }
namespace fight {
volatile LONG g_fights = 0, g_active = 0;
void LastFightPlacements(char* b, size_t n) { if (n) b[0] = 0; }
bool PlacementSeenRecently(uint32_t, double) { return false; }
double Now() { return 0; }
}
namespace bosslog { void FightBegin(uint64_t) {} }
}

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    using namespace addon::rematch;
    CHECK(g_cfg.fightNow && g_cfg.fightWarp && g_cfg.fightSpawn);
    wchar_t tempPath[MAX_PATH]{};
    CHECK(GetTempPathW(MAX_PATH,tempPath) && GetTempFileNameW(tempPath,L"n3d",0,addon::testIni));
    // Actual Prepare reads a private temporary INI; missing Enable remains off.
    g_cfg.fightNow=false; g_cfg.fightWarp=false;
    CHECK(!Prepare() && !g_cfg.fightNow && !g_cfg.fightWarp);
    CHECK(WritePrivateProfileStringW(L"Rematch",L"Enable",L"1",addon::testIni));
    // GroupOk stub refuses hook installation after actual defaults are read.
    CHECK(!Prepare() && g_cfg.fightNow && g_cfg.fightWarp && g_cfg.fightSpawn);
    CHECK(WritePrivateProfileStringW(L"Rematch",L"FightNow",L"0",addon::testIni));
    CHECK(WritePrivateProfileStringW(L"Rematch",L"FightNowWarp",L"0",addon::testIni));
    CHECK(!Prepare() && !g_cfg.fightNow && !g_cfg.fightWarp && g_cfg.fightSpawn);
    CHECK(DeleteFileW(addon::testIni)); addon::testIni[0]=0;
    g_live.store(true);  // UI fixture only; no game/pump is started
    g_cfg.showAll = true;
    for (int i = 0; i < 25; ++i) {
        Row row = {};
        snprintf(row.name, sizeof(row.name), "Boss %d", i);
        row.route = rcore::kR;
        row.fightStart = 7;
        g_rows.push_back(row);
        g_status[(size_t)i].store(2);
    }
    UiOpen();
    CHECK(g_uiPages.page == uRoot && !UiAct());
    CHECK(g_uiPages.page == uBosses);
    UiMove(10);
    CHECK(g_sel == 10);
    CHECK(!UiAct() && g_uiPages.page == uActions && g_uiBoss == 10);
    int it[kMaxRows + 4];
    CHECK(Items(it, kMaxRows + 4) == 3 && it[0] == kItFight + 10 && it[1] == kItShrine + 10 && it[2] == kItBack);
    g_confirm = 10;
    CHECK(!UiBack() && g_confirm == -9 && g_uiPages.page == uActions);
    CHECK(!UiBack() && g_uiPages.page == uBosses && g_sel == 10);
    UiMove(10);
    UiMove(10);
    CHECK(g_sel == 25);  // final Back row, paging clamps instead of wrapping
    CHECK(!UiAct() && g_uiPages.page == uRoot);
    CHECK(UiBack());
    UiOpen();
    UiMove(-1);
    CHECK(UiAct());  // explicit Close at root
    UiOpen(); UiAct(); UiAct();
    g_cfg.fightNow = true; g_fnowOk = true;
    CHECK(Items(it, kMaxRows + 4) == 3 && it[0] == kItFight && it[1] == kItShrine && it[2] == kItBack);
    UiRow rows[24]; char footer[240];
    CHECK(UiFill(rows, 24, footer, sizeof(footer)) > 0);
    CHECK(strstr(rows[0].text, "Boss actions") != nullptr);

    // Actual menu actions: visible native practice, two presses to start,
    // cancellation, and owned return before the generic active-start gate.
    Row& practice = g_rows[0];
    strcpy_s(practice.id, "R1CBC"); practice.missionId = 0x1CBC;
    practice.placement = 65092; practice.fightStart = 67064;
    uint8_t gm[0x600]{}, pt[0x1400]{}, ms[0x40]{}, mgr[16]{}, ch[0x2D00]{}, pm[0x40]{}, net[0x200]{}, queue[3*0x58]{};
    uint64_t pgm=(uint64_t)gm, ppt=(uint64_t)pt, pms=(uint64_t)ms, pmgr=(uint64_t)mgr;
    uint64_t pch=(uint64_t)ch, ach=(uint64_t)&pch, ppm=(uint64_t)pm, pnet=(uint64_t)net, player=1;
    memcpy(pt+0xCD*0x18,&player,8); memcpy(mgr+8,&pms,8);
    uint32_t retry=67064, mission=0x1111; memcpy(ms+0x14,&retry,4); memcpy(ms+0x28,&mission,4);
    G_GM=(uint64_t)&pgm; G_PT=(uint64_t)&ppt; G_MS=(uint64_t)&pmgr; G_QUEUE=(uint64_t)queue;
    G_CH=(uint64_t)&ach; G_PM=(uint64_t)&ppm; G_NET=(uint64_t)&pnet;
    addon::uiReads=true; g_lastTick=Ms(); g_qChangedAt=Ms()-10001;
    // Every boss has exactly two stable choices, including unavailable routes.
    UiOpen(); UiAct(); UiAct();
    auto shown = [&](const char* text) {
        const int count = UiFill(rows,24,footer,sizeof(footer));
        for (int i=0;i<count;++i) if (strstr(rows[i].text,text)) return true;
        return false;
    };
    CHECK(Items(it,kMaxRows+4)==3 && it[0]==kItFight && it[1]==kItShrine && it[2]==kItBack);
    CHECK(!shown("Replay mission:"));
    g_cfg.fightNow=false;
    CHECK(shown("Fight now") && shown("Shrine before the boss") && shown("FightNow = 1"));
    g_sel=0; CHECK(!UiAct() && !g_req && strstr(g_msg,"Fight now is off"));
    g_sel=1; CHECK(!UiAct() && !g_req && strstr(g_msg,"Fight now is off"));
    g_cfg.fightNow=true; practice.atFight=true; g_confirm=-9;
    CHECK(Items(it,kMaxRows+4)==3 && shown("no separate shrine"));
    CHECK(!UiAct() && !g_req && strstr(g_msg,"no separate shrine"));
    g_sel=0; CHECK(!UiAct() && UiAct() && (g_req.load()&0xFF)==kReqFight); g_req=0;
    practice.atFight=false; practice.fightStart=0; g_confirm=-9;
    CHECK(shown("no start point"));
    g_sel=0; CHECK(!UiAct() && !g_req); g_sel=1; CHECK(!UiAct() && !g_req);
    practice.fightStart=67064; practice.route=rcore::kX;
    CHECK(Items(it,kMaxRows+4)==3 && shown("replayable missions only"));
    CHECK(!UiAct() && !g_req); g_sel=0; CHECK(!UiAct() && !g_req);
    practice.route=rcore::kR; g_cfg.fightWarp=false;
    CHECK(shown("arena warp off")); CHECK(!UiAct() && !g_req);
    g_sel=1; CHECK(!UiAct() && UiAct() && (g_req.load()&0xFF)==kReqShrine); g_req=0;
    g_cfg.fightWarp=true; g_warpOk=true; practice.arenaOk=true; g_confirm=-9; g_sel=0;
    CHECK(!UiAct() && UiAct() && (g_req.load()&0xFF)==kReqFight); g_req=0;
    // Actual hosted confirmation stages only; no request before exact presentation ACK.
    g_coverReadOk=true; g_confirm=-9;
    g_attempt={};g_attemptSequence=7; // normal return resets record, never monotonic admission counter
    CHECK(!UiActHosted() && !UiActHosted() && !g_req);
    CoverInfo cover{};CHECK(CoverSnapshot(cover) && cover.phase==transition_cover::AwaitPresent);
    const uint64_t coverToken=cover.token;
    CoverPresented(coverToken+1);CoverTick(ReadSnap(),Ms());CHECK(!g_req);
    // Tick sampled time before a concurrent successful covered Present acquired
    // the cover lock. The historical tick timestamp must not age that newer ACK.
    const uint64_t cachedTick=g_cover.requestedAt;
    while(Ms()<=cachedTick)Sleep(1);
    CoverPresented(coverToken);CHECK(g_coverHostAt>cachedTick);
    CoverTick(ReadSnap(),cachedTick);
    CHECK((g_req.load()&0xFF)==kReqFight && g_cover.issued && g_cover.attempt==8);
    g_attempt.sequence=8;g_sp.st=3;g_sp.rec=0x12345678;g_sp.origFlags=73;
    const auto activeCover=g_cover;
    g_coverHostAt=Ms()+60000;CoverTick(ReadSnap(),Ms());CHECK(g_cover.phase==transition_cover::Failed);
    g_cover=activeCover;g_coverHostAt=Ms()-1500;CoverTick(ReadSnap(),Ms());CHECK(g_cover.phase==transition_cover::Failed);
    g_cover=activeCover;g_coverHostAt=Ms();g_lastTick=Ms()+60000;
    CHECK(CoverSnapshot(cover) && cover.phase==transition_cover::Failed);
    g_cover=activeCover;g_coverHostAt=Ms();g_lastTick=Ms()-1500;
    CHECK(CoverSnapshot(cover) && cover.phase==transition_cover::Failed);
    g_cover=activeCover;g_coverHostAt=Ms();g_lastTick=Ms();
    CoverCancel(coverToken);CHECK(CoverSnapshot(cover) && cover.phase==transition_cover::Cancelled);
    CHECK((g_req.load()&0xFF)==kReqFight && g_attempt.sequence==8 && g_sp.st==3 && g_sp.rec==0x12345678 && g_sp.origFlags==73); // presentation cancel retains native recovery
    // Actual hooks select only the branch created for this exact request/start.
    mission=0x1DEA;retry=4095000;memcpy(ms+0x28,&mission,4);memcpy(ms+0x14,&retry,4);
    g_cover={};CHECK(g_cover.Begin(90,0,mission,retry,4092708,Ms()));
    CHECK(g_cover.Presented(90,Ms())&&g_cover.Issue(8));
    retry=4095001;memcpy(ms+0x14,&retry,4);HkCoverCreate(4097963,0,0,0,nullptr);CHECK(!g_cover.placementEntry);
    retry=4095000;memcpy(ms+0x14,&retry,4);HkCoverCreate(999,0,0,0,nullptr);CHECK(!g_cover.placementEntry);
    HkCoverCreate(4097963,0,0,0,nullptr);CHECK(g_cover.ActorPlacement()==4097963&&g_cover.placementEntry);
    // Real CoverTick + passive player reader: known dead terminates, unknown
    // during loading does not. All native/recovery ownership sentinels survive.
    uint8_t livePlayer[0x400]{},playerStatus[0x50]{};
    uint64_t statusPointer=(uint64_t)playerStatus;memcpy(livePlayer+0x3A0,&statusPointer,8);
    player=(uint64_t)livePlayer;memcpy(pt+0xCD*0x18,&player,8);
    addon::g_img.base=(uint64_t)&ppt-0x4745348;
    g_attempt.evidence=aMatched;g_attempt.mission=mission;g_coverHostAt=Ms();
    int32_t dead=1;memcpy(playerStatus+0x48,&dead,4);
    const auto deathCover=g_cover;
    statusPointer=0;memcpy(livePlayer+0x3A0,&statusPointer,8);CoverTick(ReadSnap(),Ms());CHECK(g_cover.Active());
    statusPointer=(uint64_t)playerStatus;memcpy(livePlayer+0x3A0,&statusPointer,8);
    CoverTick(ReadSnap(),Ms());CHECK(g_cover.phase==transition_cover::Failed&&strstr(g_cover.reason,"Player died"));
    CHECK((g_req.load()&0xFF)==kReqFight&&g_attempt.sequence==8&&g_sp.st==3&&g_sp.rec==0x12345678&&g_sp.origFlags==73);
    g_cover=deathCover;g_attempt.sequence=9;CoverTick(ReadSnap(),Ms());CHECK(strstr(g_cover.reason,"Player died")==nullptr);
    addon::g_img.base=0;player=1;memcpy(pt+0xCD*0x18,&player,8);
    mission=0x1111;retry=67064;memcpy(ms+0x28,&mission,4);memcpy(ms+0x14,&retry,4);
    g_sp={};g_attempt={};
    g_req=0;g_cover={};g_coverHostAt=0;g_coverReadOk=false;
    g_status[0]=1; g_confirm=-9; CHECK(!UiAct() && !g_req);
    g_sel=1; CHECK(!UiAct() && !g_req); g_status[0]=2;
    g_cfg.fightWarp=false; practice.arenaOk=false;
    practice.gateId=1; g_gateOk=true; g_sel=0; g_confirm=-9;
    CHECK(!UiAct() && !g_req && strstr(g_msg,"arena warp off"));
    g_cfg.fightWarp=true; CHECK(!UiAct() && !g_req && strstr(g_msg,"no arena position"));
    g_cfg.fightWarp=false;
    practice.gateId=0; g_gateOk=false; g_confirm=-9;
    UiOpen(); UiMove(1); CHECK(!UiAct() && g_uiPages.page == uPractice);
    CHECK(UiFill(rows,24,footer,sizeof(footer)) > 0 && strstr(footer,"Normal rewards, drops and progression apply"));
    CHECK(strstr(rows[0].text,"Practice Yamagata"));
    g_status[0]=1; CHECK(!UiAct() && !g_req);
    g_status[0]=2; CHECK(!UiAct() && g_confirm == kItPracticeStart && !g_req);
    CHECK(!UiBack() && g_confirm == -9 && g_uiPages.page == uPractice);
    CHECK(!UiAct() && UiAct() && (g_req.load() & 0xFF) == kReqPractice);
    g_req=0; g_practiceOwned=true; g_activeRow=0; g_attemptPhase=aActive;
    g_state=kInReplay; g_exitOk=true; mission=0x1CBC; memcpy(ms+0x28,&mission,4);
    UiOpen(); UiMove(1); CHECK(!UiAct()); UiMove(1);
    CHECK(!UiAct() && g_confirm == kItPracticeReturn && !g_req);
    CHECK(UiAct() && (g_req.load() & 0xFF) == kReqPracticeExit);
    g_req=0; g_attemptPhase=aQuarantined;
    CHECK(!UiAct() && !g_req);
    g_attemptPhase=aActive; g_practiceOwned=false;
    CHECK(!UiAct() && !g_req);
    g_attemptPhase=aNone; g_state=kIdle; g_activeRow=-1; addon::uiReads=false;

    // With Rematch disabled, Settings remains available and changes no game state.
    g_live.store(false);
    UiOpen();
    CHECK(Items(it, kMaxRows + 4) == 2 && it[0] == kItSettings && it[1] == kItClose);
    CHECK(!UiAct() && g_uiPages.page == uSettings);
    for (int i = 0; i < kConfigN; ++i) g_configValues.values[(size_t)i] = {kConfigSettings[i].initial, false};
    UiMove(1);  // Training group
    CHECK(!UiAct() && g_uiPages.page == uSettingFirst + 1);
    UiChange(1);
    int edited = 0;
    for (int i = 0; i < kConfigN; ++i) if (g_configValues.values[(size_t)i].changed) ++edited;
    CHECK(edited == 1 && !g_live.load());
    UiMove(1);  // reserved Training Enable
    CHECK(!UiAct());
    const int reserved = kItSettingBase - it[1];
    (void)reserved;
    edited = 0;
    for (int i = 0; i < kConfigN; ++i) {
        const auto& d = kConfigSettings[i];
        if (g_configValues.values[(size_t)i].changed) ++edited;
        if (!strcmp(d.section, "Training") && !strcmp(d.key, "Enable"))
            CHECK(g_configValues.values[(size_t)i].text == "0" && !g_configValues.values[(size_t)i].changed);
    }
    CHECK(edited == 1);
    int writes = 0;
    g_configValues.Save(kConfigSettings, [&](const wchar_t*, const wchar_t*, const std::wstring&) { ++writes; });
    CHECK(writes == 1);
    CHECK(!UiBack() && g_uiPages.page == uSettings);
    CHECK(!UiBack() && g_uiPages.page == uRoot && UiBack());

    using addon_ui_input::Pad;
    CHECK(addon_ui_input::OwnsInput(-1, 2) && addon_ui_input::OwnsInput(2, 2));
    CHECK(!addon_ui_input::OwnsInput(2, 0));
    CHECK(addon_ui_input::NavigationEdges(0x0010) == 0);  // Start cannot claim a keyboard-opened menu
    CHECK(addon_ui_input::NavigationEdges(0x330F) == 0x330F);
    Pad pads[4];
    const uint16_t gesture = addon_ui_input::kGesture;
    CHECK(!pads[2].Feed(gesture, true, 100, false, true, 0).toggle);
    auto r = pads[2].Feed(gesture, true, 1100, false, true, 0);
    CHECK(r.toggle && r.blank && !r.edges);
    CHECK(!pads[2].Feed(gesture, true, 3000, false, true, 0).toggle);
    CHECK(!pads[2].Feed(0x1000, true, 3100, true, true, 1).edges);  // held opening press
    CHECK(pads[2].Feed(0, false, 3200, true, true, 1).blank);
    CHECK(pads[2].Feed(0x1000, true, 3300, true, true, 1).edges == 0x1000);
    r = pads[2].Feed(0x1000, true, 3400, false, true, 2);
    CHECK(r.blank && !r.edges);  // post-close confirm cannot leak
    CHECK(pads[2].Feed(0, true, 3500, false, true, 2).blank);  // held trigger / stick
    CHECK(!pads[2].Feed(0, false, 3600, false, true, 2).blank);
    CHECK(!pads[2].Feed(0x1000, true, 3700, false, true, 2).blank);
    CHECK(pads[0].Feed(0x2000, true, 3400, true, true, 2).blank);  // every slot is isolated
    CHECK(!pads[3].Feed(gesture, true, 100, false, false, 0).toggle);
    CHECK(!pads[3].Feed(gesture, true, 5100, false, false, 0).toggle);
    Pad stick;
    CHECK(stick.Feed(0, true, 100, true, true, 0, 0x0008).edges == 0x0008);
    CHECK(!stick.Feed(0x0020, true, 1200, false, true, 0, 0x0001).toggle);  // View+stick cannot mimic the physical open gesture
    puts("ALL PASSED: real Boss UI root/list/actions, confirmation/back, paging and per-slot release isolation");
    return 0;
}
