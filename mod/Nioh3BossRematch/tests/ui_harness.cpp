#define _CRT_SECURE_NO_WARNINGS  // rematch.cpp uses sscanf; this file builds it at /W4
// Test the real rematch UI model and the hook's input decision helper without game calls.
#include "../src/rematch.cpp"
#include "../src/ui_input.h"

namespace addon {
// Test stub: the rematch.cpp call site needs it to link; the harness never installs a retarget.
bool InstallCallRetargetAt(uint8_t*, const char*, void*, void**, const char**) { return false; }
Image g_img = {};
void Log(const char*, ...) {}
wchar_t testIni[MAX_PATH]{};
const wchar_t* IniPath() { return testIni; }
wchar_t testDir[MAX_PATH]{};
const wchar_t* ModDir() { return testDir; }
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
bool InstallVeto(const char*, bool (*)(int), const char**) { return false; }
bool InstallPump(void (*)(), const char**) { return false; }
bool InstallCallRetarget(const char*, void*, void**, const char**) { return false; }
namespace fight {
volatile LONG g_fights = 0, g_active = 0;
void LastFightPlacements(char* b, size_t n) { if (n) b[0] = 0; }
bool PlacementSeenRecently(uint32_t, double) { return false; }
double Now() { return 0; }
void ForceEnd(const char*) {}  // f7fed86 LeaveArena (stub so the harness links again)
void ConfirmNativeWin(uint32_t) {}
}
namespace bosslog { void FightBegin(uint64_t) {} }
}

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)
int main() {
    using namespace addon::rematch;
    CHECK(g_cfg.fightNow && g_cfg.fightWarp && g_cfg.fightSpawn);
    wchar_t tempPath[MAX_PATH]{};
    CHECK(GetTempPathW(MAX_PATH,tempPath) && GetTempFileNameW(tempPath,L"n3d",0,addon::testIni));
    wcscpy_s(addon::testDir, tempPath);
    // Actual Prepare reads a private temporary INI; explicit off stays off, missing Enable defaults on.
    CHECK(WritePrivateProfileStringW(L"Rematch",L"Enable",L"0",addon::testIni));
    g_cfg.fightNow=false; g_cfg.fightWarp=false;
    CHECK(!Prepare() && !g_cfg.fightNow && !g_cfg.fightWarp);
    CHECK(WritePrivateProfileStringW(L"Rematch",L"Enable",nullptr,addon::testIni));
    // GroupOk stub refuses hook installation after actual defaults are read.
    CHECK(!Prepare() && g_cfg.fightNow && g_cfg.fightWarp && g_cfg.fightSpawn);
    CHECK(WritePrivateProfileStringW(L"Rematch",L"FightNow",L"0",addon::testIni));
    CHECK(WritePrivateProfileStringW(L"Rematch",L"FightNowWarp",L"0",addon::testIni));
    CHECK(!Prepare() && !g_cfg.fightNow && !g_cfg.fightWarp && g_cfg.fightSpawn);
    CHECK(DeleteFileW(addon::testIni)); addon::testIni[0]=0;
    g_live.store(true);  // UI fixture only; no game/pump is started
    g_cfg.showAll = true;
    g_cfg.showUnverified = true;  // #14336 item 10: the legacy Practice Yamagata row is shown only with ShowUnverified = 1
    for (int i = 0; i < 25; ++i) {
        Row row = {};
        snprintf(row.name, sizeof(row.name), "Boss %d", i);
        row.route = rcore::kR;
        row.fightStart = 7;
        g_rows.push_back(row);
        g_status[(size_t)i].store(2);
    }
    UiOpen();
    CHECK(g_uiPages.page == uRoot); g_sel = 1; CHECK(!UiAct());  // user #13223: root = Settings / Boss Rematch / Practice / Close
    CHECK(g_uiPages.page == uRematch);
    int it[kMaxRows + 4];
    {
        const int fn = Items(it, kMaxRows + 4);
        CHECK(fn == 28 && it[0] == kItListFilter);  // filter + 25 bosses + Practice + Back
        for (int i = 0; i < 25; ++i) CHECK(it[i + 1] == i);
        CHECK(it[26] == kItPractice && it[27] == kItBack);
        CHECK(g_listStop == 0);
    }
    g_sel = 11;  // filter occupies row 0; catalogue row 10 is still item 10
    CHECK(!UiAct() && g_uiPages.page == uActions && g_uiBoss == 10);
    CHECK(Items(it, kMaxRows + 4) == 4 && it[0] == kItFight + 10 && it[1] == kItBossTrainHere + 10 && it[2] == kItShrine + 10 && it[3] == kItBack);
    g_confirm = 10;
    CHECK(!UiBack() && g_confirm == -9 && g_uiPages.page == uActions);
    CHECK(!UiBack() && g_uiPages.page == uRematch && g_sel == 11);
    UiMove(10);
    UiMove(10);
    CHECK(g_sel == 27);  // filter + 25 bosses + Practice + Back: paging clamps on Back
    snprintf(g_rows[3].region, sizeof g_rows[3].region, "Sengoku");
    snprintf(g_rows[4].region, sizeof g_rows[4].region, "Heian");
    g_rows[5].route = rcore::kE;
    snprintf(g_rows[5].region, sizeof g_rows[5].region, "Sengoku");
    g_rows[5].emakiIndex = 1;
    g_rows[10].missionId = 99;
    g_sel = 0;
    CHECK(!UiAct() && g_uiPages.page == uRematch && g_listStop == 1 && g_sel == 0);  // A cycles; these test rows are not multi-boss rosters
    {
        const int fn = Items(it, kMaxRows + 4);
        for (int i = 0; i < fn; ++i) CHECK(it[i] < 0 || it[i] >= kItFight);
        CHECK(strcmp(ListFilterLabel(), "Multi") == 0);
    }
    UiChange(1);  // Sengoku, including the scroll of that era
    CHECK(g_listStop == 2 && g_sel == 0);
    {
        const int fn = Items(it, kMaxRows + 4);
        int bosses = 0;
        for (int i = 0; i < fn; ++i) if (it[i] >= 0 && it[i] < kItFight) { CHECK(it[i] == 3 || it[i] == 5); ++bosses; }
        CHECK(bosses == 2);
    }
    UiChange(1);  // Heian
    {
        const int fn = Items(it, kMaxRows + 4);
        int bosses = 0;
        for (int i = 0; i < fn; ++i) if (it[i] >= 0 && it[i] < kItFight) { CHECK(it[i] == 4); ++bosses; }
        CHECK(bosses == 1);
    }
    UiChange(1);  // Scrolls: the scroll's item id is still 5, so Fight now is kItFight + 5
    {
        const int fn = Items(it, kMaxRows + 4);
        int bosses = 0;
        for (int i = 0; i < fn; ++i) if (it[i] >= 0 && it[i] < kItFight) { CHECK(it[i] == 5); ++bosses; }
        CHECK(bosses == 1 && it[1] == 5);
        g_sel = 1;
        CHECK(!UiAct() && g_uiPages.page == uActions && g_uiBoss == 5);
        CHECK(Items(it, kMaxRows + 4) == 4 && it[0] == kItFight + 5);
        CHECK(!UiBack() && g_uiPages.page == uRematch);
    }
    g_sel = 0;
    UiChange(1);  // Favorites, none marked yet: only the filter row and the rows below the list
    CHECK(strcmp(ListFilterLabel(), "Favorites") == 0);
    {
        const int fn = Items(it, kMaxRows + 4);
        for (int i = 0; i < fn; ++i) CHECK(it[i] < 0 || it[i] >= kItFight);
    }
    UiChange(1);  // back to All
    CHECK(g_listStop == 0);
    {
        const int fn = Items(it, kMaxRows + 4);
        CHECK(fn == 28 && it[1] == 0 && it[11] == 10);
    }
    g_listStop = 4;
    UiOpenAt(2);
    CHECK(g_listStop == 4 && g_uiPages.page == uPracticeHub);  // leaving the Boss tab keeps the filter
    UiOpenAt(1);
    CHECK(g_listStop == 4 && g_uiPages.page == uRematch);
    {  // #79226: LB / RB back to a hosted tab restores its page, row, boss and filter; the plain open still starts at the root
        UiOpenAt(1); g_uiPages.Enter(uActions, 8); g_sel = 3; g_uiBoss = 5; g_listStop = 2;
        UiOpenAt(2); g_listStop = 0;
        UiOpenAt(1, true);
        CHECK(g_uiPages.page == uActions && g_uiPages.depth == 1 && g_sel == 3 && g_uiBoss == 5 && g_listStop == 2);
        UiOpenAt(2); g_uiPages.Enter(uTrainHere, 1); g_sel = 4; g_trainStop = 3;
        UiOpenAt(1); CHECK(g_uiPages.page == uRematch && g_uiPages.depth == 0);  // plain open = root
        UiOpenAt(2, true);
        CHECK(g_uiPages.page == uTrainHere && g_sel == 4 && g_trainStop == 3);
        UiOpenAt(1, true);
        CHECK(g_uiPages.page == uRematch);  // the Boss tab was left at its root by the plain open
        g_trainStop = 0; g_listStop = 4;
    }
    UiOpen();
    CHECK(g_listStop == 4);  // closing and reopening keeps the filter; a game restart is what puts it back on All
    g_uiPages.Enter(uRematch, 1);  // the checks above reopen at the root; the next press still leaves Boss Matches
    g_listStop = 0;
    g_sel = 1;
    {
        UiRow rows[8]; char footer[240];
        CHECK(UiFill(rows, 8, footer, sizeof footer) > 0);
        CHECK(strstr(footer, "PgUp/PgDn skips ten rows") != nullptr);
        CHECK(strstr(footer, "LB/RB switches tab") != nullptr);
        CHECK(strstr(footer, "LB/RB or PgUp") == nullptr);
    }
    g_rows[3].region[0] = 0;
    g_rows[4].region[0] = 0;
    g_rows[5].route = rcore::kR;
    g_rows[5].region[0] = 0;
    g_rows[5].emakiIndex = -1;
    g_rows[10].missionId = 0;
    g_listStop = 0;
    g_sel = 27;
    CHECK(!UiAct() && g_uiPages.page == uRoot);
    CHECK(UiBack());
    UiOpen();
    UiMove(-1);
    CHECK(UiAct());  // explicit Close at root
    UiOpen(); g_sel = 1; UiAct(); g_sel = 1; UiAct();  // row 0 is the list filter; catalogue boss 0 is row 1
    g_cfg.fightNow = true; g_fnowOk = true;
    CHECK(Items(it, kMaxRows + 4) == 4 && it[0] == kItFight && it[1] == kItBossTrainHere && it[2] == kItShrine && it[3] == kItBack);
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
    UiOpen(); g_sel = 1; UiAct(); g_sel = 1; UiAct();  // catalogue boss 0 is the row after the list filter
    auto shown = [&](const char* text) {
        const int count = UiFill(rows,24,footer,sizeof(footer));
        for (int i=0;i<count;++i) if (strstr(rows[i].text,text)) return true;
        return false;
    };
    CHECK(Items(it,kMaxRows+4)==4 && it[0]==kItFight && it[1]==kItBossTrainHere && it[2]==kItShrine && it[3]==kItBack);
    CHECK(!shown("Replay mission:"));
    // Catalogue full: the old 0x30000 row was move 0. These rows must stay outside that span.
    const int savedMoves = g_n3cat_n;
    g_n3cat_n = N3CAT_CAP;
    {
        const int nAct = Items(it, kMaxRows + 4);
        for (int i = 0; i < nAct; ++i) CHECK(!IsMoveItem(it[i]));
    }
    CHECK(shown("Train here") && !shown("Idle"));
    g_repPick = -1; g_confirm = -9; g_msg[0] = 0; g_sel = 1;
    CHECK(!UiAct() && g_repPick == -1 && strstr(g_msg, "Training is unavailable"));
    g_msg[0] = 0; UiChange(1); CHECK(g_repPick == -1 && !strstr(g_msg, "Idle"));
    g_msg[0] = 0; UiChange(-1); CHECK(g_repPick == -1 && !strstr(g_msg, "Idle"));
    g_n3cat_n = savedMoves;
    g_sel = 0; g_msg[0] = 0; g_confirm = -9;
    g_cfg.fightNow=false;
    CHECK(shown("Fight now") && shown("Shrine before the boss") && shown("FightNow = 1"));
    g_sel=0; CHECK(!UiAct() && !g_req && strstr(g_msg,"Fight now is off"));
    g_sel=2; CHECK(!UiAct() && !g_req && strstr(g_msg,"Fight now is off"));
    g_cfg.fightNow=true; practice.atFight=true; g_confirm=-9;
    CHECK(Items(it,kMaxRows+4)==4 && shown("no separate shrine"));
    CHECK(!UiAct() && !g_req && strstr(g_msg,"no separate shrine"));
    g_sel=0; CHECK(!UiAct() && UiAct() && (g_req.load()&0xFF)==kReqFight); g_req=0;
    practice.atFight=false; practice.fightStart=0; g_confirm=-9;
    CHECK(shown("no start point"));
    g_sel=0; CHECK(!UiAct() && !g_req); g_sel=2; CHECK(!UiAct() && !g_req);
    practice.fightStart=67064; practice.route=rcore::kX;
    CHECK(Items(it,kMaxRows+4)==4 && shown("replayable missions only"));
    CHECK(!UiAct() && !g_req); g_sel=0; CHECK(!UiAct() && !g_req);
    practice.route=rcore::kR; g_cfg.fightWarp=false;
    CHECK(shown("arena warp off")); CHECK(!UiAct() && !g_req);
    g_sel=2; CHECK(!UiAct() && UiAct() && (g_req.load()&0xFF)==kReqShrine); g_req=0;
    g_cfg.fightWarp=true; g_warpOk=true; practice.arenaOk=true; g_confirm=-9; g_sel=0;
    CHECK(!UiAct() && UiAct() && (g_req.load()&0xFF)==kReqFight); g_req=0;
    // Picture Scroll row: Fight now posts that record. Train here and shrine do not.
    {
        const Row saved = practice;
        const bool emakiWas = g_emakiOk;
        practice.route = rcore::kE;
        practice.emakiIndex = 3;
        practice.missionId = 0xCC96;
        g_emakiOk = true;
        g_cfg.fightNow = true;
        g_req = 0; g_confirm = -9; g_msg[0] = 0;
        UiOpen(); g_sel = 1; UiAct(); g_sel = 1; UiAct();  // list filter is row 0; this scroll is catalogue boss 0
        CHECK(Items(it, kMaxRows + 4) == 4 && it[0] == kItFight && it[1] == kItBossTrainHere && it[2] == kItShrine && it[3] == kItBack);
        CHECK(shown("Fight now (this scroll's waves)"));
        CHECK(shown("Train here does not pick this scroll"));
        CHECK(shown("a Picture Scroll has no shrine"));
        g_sel = 1; CHECK(!UiAct() && !g_req && strstr(g_msg, "no shrine and no Train here"));
        g_sel = 2; g_msg[0] = 0; CHECK(!UiAct() && !g_req && strstr(g_msg, "no shrine and no Train here"));
        g_sel = 0; g_msg[0] = 0; g_confirm = -9;
        CHECK(!UiAct() && !g_req && strstr(g_msg, "No item used"));
        CHECK(UiAct() && (g_req.load() & 0xFF) == kReqEmaki && (g_req.load() >> 8) == 1);
        practice = saved;
        g_emakiOk = emakiWas;
        g_req = 0; g_confirm = -9; g_msg[0] = 0;
        UiOpen(); g_sel = 1; UiAct(); g_sel = 1; UiAct();
        g_sel = 0;
    }
    // #79069 Training at a Picture Scroll arena, the Training filters, favorites. Real menu calls; rows restored at the end.
    {
        const std::vector<Row> savedRows = g_rows;
        const bool emakiWas = g_emakiOk, vetoWas = g_waveVetoOk;
        const int stateWas = g_state.load();
        for (int i = 0; i < 25; ++i) if (i) snprintf(g_rows[(size_t)i].id, sizeof g_rows[(size_t)i].id, "T%02d", i);
        Row& sc = g_rows[7];
        sc.route = rcore::kE; sc.emakiIndex = 2; sc.missionId = 0xCC96; sc.storyMission = 0xCC96;  // the catalogue story_mission of every scroll row
        snprintf(sc.region, sizeof sc.region, "Waves");
        snprintf(sc.name, sizeof sc.name, "Scroll Seven");
        g_rows[8].missionId = 0x2222; snprintf(g_rows[8].region, sizeof g_rows[8].region, "Sengoku");
        g_rows[9].missionId = 0x3333;
        g_emakiOk = true; g_req = 0; g_confirm = -9; g_msg[0] = 0;
        g_listStop = 0; g_trainStop = 0;
        auto indexOf = [&](int item) { const int c = Items(it, kMaxRows + 4); for (int i = 0; i < c; ++i) if (it[i] == item) return i; return -1; };
        auto rowsOn = [&](int base) { int k = 0; const int c = Items(it, kMaxRows + 4); for (int i = 0; i < c; ++i) if (it[i] >= base && it[i] < base + kMaxRows) ++k; return k; };

        // 3: both Training lists start with the Boss Matches filter row; the stops are the same and the two filters stay separate.
        UiOpen(); g_uiPages.page = uTrainHere; g_sel = 0;
        CHECK(Items(it, kMaxRows + 4) > 2 && it[0] == kItListFilter && rowsOn(kItTrainHere) >= 3);
        CHECK(strcmp(ListFilterLabel(), "All") == 0);
        CHECK(!UiAct() && g_uiPages.page == uTrainHere && g_trainStop == 1 && g_listStop == 0 && g_sel == 0);  // A cycles, starts nothing
        CHECK(!g_req);
        CHECK(shown("List: Multi"));
        g_trainStop = 2;
        const char* era = ListFilterLabel();
        const int eraStops = g_listCycle.nEra;
        CHECK(eraStops == 2 && !strcmp(era, "Waves"));
        CHECK(rowsOn(kItTrainHere) == 1 && indexOf(kItTrainHere + 7) == 1);
        g_trainStop = 3;
        CHECK(!strcmp(ListFilterLabel(), "Sengoku") && rowsOn(kItTrainHere) == 1 && indexOf(kItTrainHere + 8) == 1);
        g_trainStop = 4;
        CHECK(!strcmp(ListFilterLabel(), "Scrolls") && rowsOn(kItTrainHere) == 1 && indexOf(kItTrainHere + 7) == 1);
        g_trainStop = 5;
        CHECK(!strcmp(ListFilterLabel(), "Favorites") && rowsOn(kItTrainHere) == 0);
        g_sel = 0; UiChange(1);
        CHECK(g_trainStop == 0 && strcmp(ListFilterLabel(), "All") == 0);  // wraps
        g_uiPages.page = uTraining; g_sel = 0;
        CHECK(Items(it, kMaxRows + 4) > 2 && it[0] == kItListFilter && rowsOn(kItTeleport) == 25);
        g_trainStop = 3;  // the same filter on the arena list
        CHECK(rowsOn(kItTeleport) == 1 && indexOf(kItTeleport + 8) == 1);
        g_trainStop = 4;
        CHECK(rowsOn(kItTeleport) == 1 && indexOf(kItTeleport + 7) == 1);
        g_trainStop = 0;

        // 4: Right marks, Left unmarks, by row id. Boss Matches and Training have separate lists. Saved in our file and read back.
        const std::wstring favFile = std::wstring(addon::testDir) + L"Nioh3BossRematch_favorites.txt";
        DeleteFileW(favFile.c_str());
        g_favs.clear(); g_favLoaded = false;
        g_uiPages.page = uTrainHere;
        g_sel = indexOf(kItTrainHere + 8); CHECK(g_sel > 0);
        UiChange(1);
        CHECK(FavHas(kFavTrain, "T08") && !FavHas(kFavBoss, "T08") && strstr(g_msg, "favorite"));
        CHECK(shown("* Boss 8"));
        CHECK(GetFileAttributesW(favFile.c_str()) != INVALID_FILE_ATTRIBUTES);
        g_trainStop = 5;  // Favorites
        CHECK(rowsOn(kItTrainHere) == 1 && indexOf(kItTrainHere + 8) == 1);
        // The same id is found after the catalogue moves: rows 8 and 9 swap places.
        std::swap(g_rows[8], g_rows[9]);
        CHECK(rowsOn(kItTrainHere) == 1 && indexOf(kItTrainHere + 9) == 1);
        std::swap(g_rows[8], g_rows[9]);
        // Survives a restart: forget everything in memory, read the file again.
        g_favs.clear(); g_favLoaded = false;
        CHECK(FavHas(kFavTrain, "T08"));
        CHECK(rowsOn(kItTrainHere) == 1);
        // The arena list shares Training's favorites; a Left there removes it from Favorites at once.
        g_uiPages.page = uTraining;
        CHECK(rowsOn(kItTeleport) == 1);
        g_sel = indexOf(kItTeleport + 8); CHECK(g_sel == 1);
        UiChange(-1);
        CHECK(!FavHas(kFavTrain, "T08") && rowsOn(kItTeleport) == 0);
        g_favs.clear(); g_favLoaded = false;
        CHECK(!FavHas(kFavTrain, "T08"));
        g_trainStop = 0;
        // Boss Matches: its own list, its own stop. A still opens the boss, Left / Right never start anything.
        UiOpen(); g_uiPages.page = uRematch;
        g_sel = indexOf(10); CHECK(g_sel > 0);
        UiChange(1);
        CHECK(FavHas(kFavBoss, "T10") && !FavHas(kFavTrain, "T10") && !g_req && g_uiPages.page == uRematch);
        CHECK(shown("* Boss 10"));
        UiChange(1);  // already marked: stays marked
        CHECK(FavHas(kFavBoss, "T10"));
        EnsureListCycle();
        g_listStop = g_listCycle.Stops() - 1;
        CHECK(!strcmp(ListFilterLabel(), "Favorites"));
        CHECK(indexOf(10) == 1 && Items(it, kMaxRows + 4) >= 3 && it[2] < 0);
        g_sel = 1;
        CHECK(!UiAct() && g_uiPages.page == uActions && g_uiBoss == 10 && !g_req);
        CHECK(!UiBack() && g_uiPages.page == uRematch);
        g_sel = 1; UiChange(-1);
        CHECK(!FavHas(kFavBoss, "T10") && indexOf(10) == -1);
        g_listStop = 0;
        {   // a row without an id cannot be marked
            char keep[sizeof g_rows[10].id]; memcpy(keep, g_rows[10].id, sizeof keep);
            g_rows[10].id[0] = 0;
            g_sel = indexOf(10); UiChange(1);
            CHECK(strstr(g_msg, "no id") && g_favs.empty());
            memcpy(g_rows[10].id, keep, sizeof keep);
        }
        {   // a damaged file loses the bad lines only
            std::set<std::string> f;
            rcore::FavParse("B R1CBC\r\njunk\nT a b\nX E00\nT E00\nB \n", f);
            CHECK(f.size() == 2 && f.count("B R1CBC") && f.count("T E00"));
            CHECK(rcore::FavText(f) == "B R1CBC\r\nT E00\r\n");
            CHECK(!rcore::FavIdOk("") && !rcore::FavIdOk("a b") && !rcore::FavIdOk(nullptr) && rcore::FavIdOk("R1CBC"));
        }
        DeleteFileW(favFile.c_str());
        g_favs.clear(); g_favLoaded = false;

        // 1: every Training path posts the no-wave request, Fight now posts the normal one, and no guard means no start.
        UiOpen(); g_uiPages.page = uTrainHere; g_sel = indexOf(kItTrainHere + 7); g_req = 0; g_confirm = -9; g_msg[0] = 0;
        g_waveVetoOk = false;
        CHECK(!UiAct() && !g_req && strstr(g_msg, "wave guard"));
        g_waveVetoOk = true; g_msg[0] = 0;
        CHECK(!UiAct() && !g_req && strstr(g_msg, "No waves"));
        CHECK(UiAct() && (g_req.load() & 0xFF) == kReqEmakiTrain && (g_req.load() >> 8) == 8);
        g_req = 0; g_confirm = -9;
        UiOpen(); g_uiPages.page = uTraining; g_sel = indexOf(kItTeleport + 7);
        CHECK(!UiAct() && !g_req && strstr(g_msg, "No waves"));
        CHECK(UiAct() && (g_req.load() & 0xFF) == kReqEmakiTrain);
        g_req = 0; g_confirm = -9;
        UiOpen(); g_uiPages.page = uRematch; g_sel = indexOf(7); CHECK(!UiAct() && g_uiPages.page == uActions && g_uiBoss == 7);
        CHECK(shown("Train here (no waves)"));
        g_sel = indexOf(kItBossTrainHere + 7);
        CHECK(!UiAct() && !g_req && strstr(g_msg, "No waves"));
        CHECK(UiAct() && (g_req.load() & 0xFF) == kReqEmakiTrain);
        g_req = 0; g_confirm = -9;
        g_sel = indexOf(kItFight + 7);
        CHECK(!UiAct() && !g_req);
        CHECK(UiAct() && (g_req.load() & 0xFF) == kReqEmaki);  // Fight now keeps its waves
        g_req = 0; g_confirm = -9; g_msg[0] = 0;

        // The script's wave call is skipped only on a live Training trip inside 0xCC96.
        const uint32_t missionWas = mission;
        mission = 0xCC96; memcpy(ms + 0x28, &mission, 4);
        g_scrollNoWaves.store(false); g_state.store(kEmaki);
        CHECK(!VetoWaveRequest(0));  // Fight now: waves run
        g_scrollNoWaves.store(true);
        CHECK(VetoWaveRequest(0));
        g_state.store(kIdle);
        CHECK(!VetoWaveRequest(0));  // stale flag, no trip
        g_state.store(kEmaki);
        mission = 0x1111; memcpy(ms + 0x28, &mission, 4);
        CHECK(!VetoWaveRequest(0));  // stale flag, other mission
        mission = 0xCC96; memcpy(ms + 0x28, &mission, 4);

        // Training hub is the way out; the gate has no Fight again on a no-wave trip, keeps it on a normal one.
        g_et.st = rcore::kEIn; g_activeRow.store(7);
        CHECK(WavesGateNow());
        UiOpen(); g_uiPages.page = uPracticeHub;
        CHECK(indexOf(kItReturnHome) > 0 && indexOf(kItArenas) == -1);
        CHECK(shown("Leave Picture Scroll"));
        UiGatePage();
        CHECK(indexOf(kItFightAgain) == -1 && indexOf(kItReturnHome) >= 0);
        g_scrollNoWaves.store(false);
        UiGatePage();
        CHECK(indexOf(kItFightAgain) == 0 && indexOf(kItReturnHome) >= 1 && shown("Leave Picture Scroll"));
        UiOpen(); g_uiPages.page = uPracticeHub;
        CHECK(indexOf(kItReturnHome) == -1 && indexOf(kItArenas) >= 0);  // a normal hub, unchanged

        // No leak: every ExecEmaki starts from off, a refused Training request leaves it off, ToIdle clears it.
        g_scrollNoWaves.store(true);
        g_state.store(kIdle); g_et = {}; g_activeRow.store(-1);
        g_waveVetoOk = false;
        ExecEmaki(7, Ms(), true);
        CHECK(!g_scrollNoWaves.load() && g_state.load() == kIdle);
        g_scrollNoWaves.store(true);
        ExecEmaki(-1, Ms());
        CHECK(!g_scrollNoWaves.load());
        g_scrollNoWaves.store(true);
        ToIdle("harness", true);
        CHECK(!g_scrollNoWaves.load());
        // The arrival: two warps to the wake spot, never the walk.
        CHECK(rcore::WaveTryCount(true, 4) == 2 && rcore::WaveTryOffset(true, 4, 0) == 4 && rcore::WaveTryOffset(true, 4, 1) == 4);
        CHECK(rcore::WaveTryCount(true, 0) == 2 && rcore::WaveTryOffset(true, 0, 1) == 0);
        CHECK(rcore::WaveNudgeDecide(-1, 1, false, 2) == 1 && rcore::WaveNudgeDecide(0, 1, true, 2) == 1 && rcore::WaveNudgeDecide(1, 1, true, 2) == 3);
        CHECK(rcore::WaveTryCount(false, 4) == rcore::WaveWalkCount(4) && rcore::WaveTryOffset(false, 4, 1) == rcore::WaveWalkOffset(4, 1));

        mission = missionWas; memcpy(ms + 0x28, &mission, 4);
        g_state.store(stateWas);
        g_rows = savedRows;
        g_emakiOk = emakiWas; g_waveVetoOk = vetoWas;
        g_listStop = 0; g_trainStop = 0;
        g_req = 0; g_confirm = -9; g_msg[0] = 0;
        UiOpen(); g_sel = 1; UiAct(); g_sel = 1; UiAct();
        g_sel = 0;
    }
    // Actual hosted confirmation stages only; no request before exact presentation ACK.
    const Row originalPractice=practice;
    practice.missionId=0x1A5E;practice.fightStart=5021112;practice.placement=5027806;
    g_coverReadOk=true;
    mission=0x1A5E;memcpy(ms+0x28,&mission,4);
    CHECK(!CoverBegin(0)&&!g_req&&!g_cover.Active());
    const uint64_t originalMS=G_MS;G_MS=0;
    CHECK(!CoverBegin(0)&&!g_req&&!g_cover.Active());G_MS=originalMS;
    mission=0x1111;memcpy(ms+0x28,&mission,4);
    CHECK(CoverBegin(0)&&!g_req);
    // Current source may change while presentation is pending: recheck before Post.
    CoverPresented(g_cover.token);mission=0x1A5E;memcpy(ms+0x28,&mission,4);
    CoverTick(ReadSnap(),Ms());CHECK(g_cover.phase==transition_cover::Failed&&!g_req);
    g_cover={};g_coverHostAt=0;practice=originalPractice;mission=0x1111;memcpy(ms+0x28,&mission,4);
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
    uint64_t dispatchedToken=0;char dispatchReason[160]{};
    g_coverReadOk=true;g_lastTick=Ms();
    CHECK(!UiTestFight("INVALID",dispatchedToken,dispatchReason,sizeof(dispatchReason))&&!g_req);
    g_status[0]=1;CHECK(!UiTestFight("R1CBC",dispatchedToken,dispatchReason,sizeof(dispatchReason))&&!g_req);
    g_status[0]=2;g_cfg.fightWarp=false;CHECK(!UiTestFight("R1CBC",dispatchedToken,dispatchReason,sizeof(dispatchReason))&&!g_req);
    g_cfg.fightWarp=true;
    CHECK(UiTestFight("R1CBC",dispatchedToken,dispatchReason,sizeof(dispatchReason))&&dispatchedToken&&!g_req);
    CHECK(g_cover.phase==transition_cover::AwaitPresent&&!g_cover.issued);
    CoverCancel(dispatchedToken);g_cover={};g_coverReadOk=false;
    g_status[0]=1; g_confirm=-9; CHECK(!UiAct() && !g_req);
    g_sel=1; CHECK(!UiAct() && !g_req); g_status[0]=2;
    g_cfg.fightWarp=false; practice.arenaOk=false;
    practice.gateId=1; g_gateOk=true; g_sel=0; g_confirm=-9;
    CHECK(!UiAct() && !g_req && strstr(g_msg,"arena warp off"));
    g_cfg.fightWarp=true; CHECK(!UiAct() && !g_req && strstr(g_msg,"no arena position"));
    g_cfg.fightWarp=false;
    practice.gateId=0; g_gateOk=false; g_confirm=-9;
    UiOpen(); g_sel = 1; UiAct(); g_sel = 26; CHECK(!UiAct() && g_uiPages.page == uPractice);
    CHECK(UiFill(rows,24,footer,sizeof(footer)) > 0 && strstr(footer,"Normal rewards, drops and progression apply"));
    CHECK(strstr(rows[0].text,"Yamagata mission replay"));
    g_status[0]=1; CHECK(!UiAct() && !g_req);
    g_status[0]=2; CHECK(!UiAct() && g_confirm == kItPracticeStart && !g_req);
    CHECK(!UiBack() && g_confirm == -9 && g_uiPages.page == uPractice);
    CHECK(!UiAct() && UiAct() && (g_req.load() & 0xFF) == kReqPractice);
    g_req=0; g_practiceOwned=true; g_activeRow=0; g_attemptPhase=aActive;
    g_state=kInReplay; g_exitOk=true; mission=0x1CBC; memcpy(ms+0x28,&mission,4);
    UiOpen(); g_sel = 1; UiAct(); g_sel = 26; CHECK(!UiAct()); UiMove(1);
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
    CHECK(Items(it, kMaxRows + 4) == 4 && it[0] == kItSettings && it[1] == kItRootRematch && it[2] == kItRootTraining && it[3] == kItClose);  // #13223
    g_sel = 1; CHECK(!UiAct() && g_uiPages.page == uRematch);
    CHECK(Items(it, kMaxRows + 4) == 1 && it[0] == kItBack);  // #13197: no Settings / Return to where I was on the Boss Rematch page
    CHECK(!UiBack() && g_uiPages.page == uRoot && g_sel == 1);
    g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uSettings);
    { const int ns = Items(it, kMaxRows + 4); CHECK(ns == kConfigGroupN + 1 && it[ns - 1] == kItBack); for (int i = 0; i < ns; ++i) CHECK(it[i] != kItSave && it[i] != kItRootTools); }
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
