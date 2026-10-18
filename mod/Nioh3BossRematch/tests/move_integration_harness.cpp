// Real merged menu dispatch with synthetic Training exports; no game or ASI load.
#include <windows.h>
#include <cstring>
static int tuneCalls, idleCalls, moveCalls;
static int TuneGet(int which) { return which ? 1000 : 6000; }
static int TuneSet(int, int, int) { ++tuneCalls; return 1; }
static int IdleGet() { return 0; }
static int IdleSet(int) { ++idleCalls; return 1; }
static bool bossLive = true;
static unsigned BossGet() { return bossLive ? 16102u : 0u; }
static int MoveGet(unsigned, unsigned) { return 1; }
static int MoveSet(unsigned, unsigned, int, int) { ++moveCalls; return 1; }
static int summonCalls, summonSlot = -1, passiveMs = 3000; static unsigned summonMission;
static int SummonSlot(int slot, unsigned mission) { ++summonCalls; summonSlot = slot; summonMission = mission; return 1; }
static int PassiveGet() { return passiveMs; }
static int PassiveSet(int ms, int) { passiveMs = ms; return ms; }
static int trainHereCalls; static unsigned trainHereMission;
static int TrainHere(unsigned mission) { ++trainHereCalls; trainHereMission = mission; return 1; }
static int sessionMode = 0, fightAgainCalls;  // user #13262: Nioh3Training_SessionMode (0 none, 1 Boss Rematch, 2 Training)
static int SessionMode() { return sessionMode; }
static int GateFightAgain() { ++fightAgainCalls; return 1; }
static HMODULE TestModule(const wchar_t*) { return (HMODULE)1; }
static FARPROC TestProc(HMODULE, const char* name) {
#define EXPORT(n, f) if (!strcmp(name, "Nioh3Training_" n)) return (FARPROC)f
    EXPORT("GetTuning", TuneGet); EXPORT("SetTuning", TuneSet);
    EXPORT("GetIdleHold", IdleGet); EXPORT("SetIdleHold", IdleSet);
    EXPORT("SummonSlot", SummonSlot); EXPORT("GetSummonPassive", PassiveGet); EXPORT("SetSummonPassive", PassiveSet); EXPORT("TrainHere", TrainHere);
    EXPORT("SessionMode", SessionMode); EXPORT("GateFightAgain", GateFightAgain);
    EXPORT("GetMoveBoss", BossGet); EXPORT("GetMoveAllowed", MoveGet); EXPORT("SetMoveAllowed", MoveSet);
#undef EXPORT
    return nullptr;
}
#define GetModuleHandleW TestModule
#define GetProcAddress TestProc
#define main legacy_ui_main
#include "ui_harness.cpp"
#undef main
#undef GetModuleHandleW
#undef GetProcAddress
int main() {
    using namespace addon::rematch;
    int items[kMaxRows + 4];
    Row boss = {}; strcpy_s(boss.name, "Summon Test"); boss.missionId = 0x1CBC; boss.route = rcore::kR; g_rows.push_back(boss);
    UiOpen();
    // user #13223: root = Settings / Boss Rematch / Practice / Close (the host's first page opens Boss Rematch / Practice directly)
    int n = Items(items, kMaxRows + 4), moves = -1;
    CHECK(n == 4 && items[0] == kItSettings && items[1] == kItRootRematch && items[2] == kItRootTraining && items[3] == kItClose);
    g_sel = 1; CHECK(!UiAct() && g_uiPages.page == uRematch);
    n = Items(items, kMaxRows + 4);
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) CHECK(items[i] != items[j]);
        CHECK(items[i] != kItMoves && items[i] != kItMovesA && items[i] != kItDespawnAll && items[i] != kItIdleHold && !(items[i] >= kItTeleport));
        CHECK(items[i] != kItReturnHome && items[i] != kItSettings && items[i] != kItSave);
        if (items[i] == kItFinT || items[i] == kItHp0W) { g_sel = i; UiChange(1); }
    }
    CHECK(tuneCalls == 2 && !idleCalls && !moveCalls && items[n - 1] == kItBack);
    CHECK(!UiBack() && g_uiPages.page == uRoot && g_sel == 1);
    // user #13293 Practice: Train here / Train in a boss arena / Training mods / Summon a boss here / Boss Matches
    g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uPracticeHub);
    n = Items(items, kMaxRows + 4);
    CHECK(n == 6 && items[0] == kItTrainHereOpen && items[1] == kItArenas && items[2] == kItRootTools && items[3] == kItBossSumm && items[4] == kItRootRematch && items[5] == kItBack);
    g_sel = 4; CHECK(!UiAct() && g_uiPages.page == uRematch && !UiBack() && g_uiPages.page == uPracticeHub);
    g_sel = 1; CHECK(!UiAct() && g_uiPages.page == uTraining);  // Arenas: arena rows only
    n = Items(items, kMaxRows + 4);
    for (int i = 0; i < n; ++i) CHECK((items[i] >= kItTeleport && items[i] < kItSummonA) || items[i] == kItBack);
    CHECK(items[n - 1] == kItBack && !UiBack() && g_uiPages.page == uPracticeHub && g_sel == 1);
    g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uTools);  // Training mods
    n = Items(items, kMaxRows + 4);
    int seen = 0;
    for (int i = 0; i < n; ++i) {
        CHECK(items[i] != kItFinT && items[i] != kItDeathCut && !(items[i] >= kItTeleport && items[i] < kItSummonA));
        if (items[i] == kItMoves) moves = i;
        seen |= items[i] == kItIdleHold ? 1 : items[i] == kItMovesA ? 2 : items[i] == kItMovesB ? 4 : items[i] == kItDespawnAll ? 8 : 0;
    }
    CHECK(moves >= 0 && seen == 15);
    g_sel = moves; CHECK(!UiAct() && g_uiPages.page == uMoves);
    n = Items(items, kMaxRows + 4);
    CHECK(n == 16); // Yoritomo: 14 recorded entries + idle control + Back
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) CHECK(items[i] != items[j]);
        if (items[i] == kItIdleHold) { g_sel = i; UiChange(1); CHECK(!UiAct()); }
        if (IsMoveItem(items[i]) && !N3MoveProtected(kN3Moves[items[i]-kItMoveBase].id)) { g_sel = i; UiChange(-1); }
    }
    CHECK(tuneCalls == 2 && idleCalls == 2 && moveCalls == 12);
    CHECK(!UiBack() && g_uiPages.page == uTools && !UiBack() && g_uiPages.page == uPracticeHub);
    // Training here: boss list, A queues the in-place session and closes
    g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uTrainHere);
    n = Items(items, kMaxRows + 4);
    CHECK(n == 2 && items[0] == kItTrainHere && items[1] == kItBack);
    UiRow rows[24]; char footer[240];
    CHECK(UiFill(rows, 24, footer, sizeof(footer)) > 0 && strstr(rows[0].text, "Train here") && strstr(rows[1].text, "Summon Test"));
    g_sel = 0; CHECK(UiAct() && trainHereCalls == 1 && trainHereMission == 0x1CBC);
    // host first page -> Practice / Boss Rematch as the base page: Back closes to the host
    UiOpenAt(2); CHECK(g_uiPages.page == uPracticeHub && UiBack());
    UiOpenAt(1); CHECK(g_uiPages.page == uRematch && UiBack());
    // Settings: no Save row (auto-save), Practice tools reachable from it
    UiOpen(); g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uSettings);
    n = Items(items, kMaxRows + 4);
    CHECK(n == kConfigGroupN + 2 && items[n - 2] == kItRootTools && items[n - 1] == kItBack);
    for (int i = 0; i < n; ++i) CHECK(items[i] != kItSave);
    // Boss summoning: passive-time slider, slot switch, boss rows summon into the chosen slot
    UiOpenAt(2); g_sel = 3; CHECK(!UiAct() && g_uiPages.page == uSummon);
    n = Items(items, kMaxRows + 4);  // user #13293: the Boss Match rules also on the summon page
    CHECK(n == 5 && items[0] == kItSumPassive && items[1] == kItSumSlot && items[2] == kItSummonA && items[3] == kItFightOpts && items[4] == kItBack);
    g_sel = 0; UiChange(1); CHECK(passiveMs == 4000); UiChange(-1); UiChange(-1); CHECK(passiveMs == 2000);
    for (int i = 0; i < 40; ++i) UiChange(-1);
    CHECK(passiveMs == 0);
    g_sel = 1; UiChange(1); n = Items(items, kMaxRows + 4); CHECK(items[2] == kItSummonB);
    CHECK(UiFill(rows, 24, footer, sizeof(footer)) > 0 && strstr(rows[0].text, "Summon a boss here") && strstr(rows[1].text, "-- Options --"));
    g_sel = 2; CHECK(UiAct() && summonCalls == 1 && summonSlot == 1 && summonMission == 0x1CBC);
    // user #13262: the Return gate page follows the session mode; #13265: boss-fight toggles reachable in Training
    auto has = [&](int it) { for (int i = 0; i < n; ++i) if (items[i] == it) return true; return false; };
    sessionMode = 0; UiGatePage(); n = Items(items, kMaxRows + 4);
    CHECK(n == 4 && items[0] == kItFightAgain && items[1] == kItReturnHome && items[2] == kItBack && items[3] == kItClose);  // #13529 Back row
    sessionMode = 1; n = Items(items, kMaxRows + 4);
    CHECK(items[0] == kItFightAgain && has(kItFinT) && has(kItHp0W) && !has(kItIdleHold) && !has(kItMoves) && !has(kItFightOpts) && items[n - 3] == kItReturnHome && items[n - 2] == kItBack && items[n - 1] == kItClose);
    CHECK(UiFill(rows, 24, footer, sizeof(footer)) > 0 && strstr(rows[0].text, "Boss Match") && !strstr(rows[0].text, "Rematch"));
    sessionMode = 2; n = Items(items, kMaxRows + 4);
    CHECK(items[0] == kItFightAgain && has(kItIdleHold) && has(kItMoves) && has(kItBossSumm) && has(kItDespawnAll) && has(kItFightOpts) && !has(kItFinT) && items[n - 3] == kItReturnHome && items[n - 2] == kItBack && items[n - 1] == kItClose);
    CHECK(UiFill(rows, 24, footer, sizeof(footer)) > 0 && strstr(rows[0].text, "Practice") && strstr(rows[1].text, "-- This fight --") && strstr(rows[2].text, "Reset round"));
    for (int i = 0; i < n; ++i) if (items[i] == kItFightOpts) g_sel = i;
    CHECK(!UiAct() && g_uiPages.page == uFightOpts);
    n = Items(items, kMaxRows + 4);
    CHECK(n == 3 && items[0] == kItFinT && items[1] == kItHp0W && items[2] == kItBack);
    g_sel = 0; UiChange(1); CHECK(tuneCalls == 3);
    CHECK(UiFill(rows, 24, footer, sizeof(footer)) > 0 && strstr(rows[0].text, "Boss Match rules"));
    CHECK(!UiBack() && g_uiPages.page == uGate);
    g_sel = 0; CHECK(UiAct() && fightAgainCalls == 1);
    UiGatePage(); CHECK(UiBack());  // #13529: the gate is a base page: Back closes to the host's first page
    UiOpenAt(2); g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uTools); n = Items(items, kMaxRows + 4); CHECK(has(kItFightOpts));
    sessionMode = 0;
    (void)bossLive;
    puts("PASS: root = Settings / Boss Matches / Practice; Practice = Train here / arena / Training mods / Summon a boss here / Boss Matches; section headers; direct opens; sliders, idle, moves, passive time, train-here and slot summon dispatch; mode-aware gate page; boss fight settings page");
    return 0;
}
