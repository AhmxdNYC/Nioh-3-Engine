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
static HMODULE TestModule(const wchar_t*) { return (HMODULE)1; }
static FARPROC TestProc(HMODULE, const char* name) {
#define EXPORT(n, f) if (!strcmp(name, "Nioh3Training_" n)) return (FARPROC)f
    EXPORT("GetTuning", TuneGet); EXPORT("SetTuning", TuneSet);
    EXPORT("GetIdleHold", IdleGet); EXPORT("SetIdleHold", IdleSet);
    EXPORT("SummonSlot", SummonSlot); EXPORT("GetSummonPassive", PassiveGet); EXPORT("SetSummonPassive", PassiveSet);
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
    // user #13197: root = Settings / Boss Rematch / Practice / Summon Boss, + Practice tools while a boss session is live (GetMoveBoss != 0), Close
    int n = Items(items, kMaxRows + 4), moves = -1;
    CHECK(n == 6 && items[0] == kItSettings && items[1] == kItRootRematch && items[2] == kItRootTraining && items[3] == kItRootSummon && items[4] == kItRootTools && items[5] == kItClose);
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
    // Practice (Training arenas): arena rows only
    g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uTraining);
    n = Items(items, kMaxRows + 4);
    for (int i = 0; i < n; ++i) CHECK((items[i] >= kItTeleport && items[i] < kItSummonA) || items[i] == kItBack);
    CHECK(items[n - 1] == kItBack && !UiBack() && g_uiPages.page == uRoot);
    // Practice tools: stance, moves, Boss A/B moves, repeat delay, despawn
    g_sel = 4; CHECK(!UiAct() && g_uiPages.page == uTools);
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
    CHECK(!UiBack() && g_uiPages.page == uTools && !UiBack() && g_uiPages.page == uRoot);
    // Settings: no Save row (auto-save), Practice tools always reachable from it
    g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uSettings);
    n = Items(items, kMaxRows + 4);
    CHECK(n == kConfigGroupN + 2 && items[n - 2] == kItRootTools && items[n - 1] == kItBack);
    for (int i = 0; i < n; ++i) CHECK(items[i] != kItSave);
    g_sel = n - 2; CHECK(!UiAct() && g_uiPages.page == uTools);
    CHECK(!UiBack() && g_uiPages.page == uSettings && !UiBack() && g_uiPages.page == uRoot);
    // Summon Boss: passive-time slider, slot switch, boss rows summon into the chosen slot
    g_sel = 3; CHECK(!UiAct() && g_uiPages.page == uSummon);
    n = Items(items, kMaxRows + 4);
    CHECK(n == 4 && items[0] == kItSumPassive && items[1] == kItSumSlot && items[2] == kItSummonA && items[3] == kItBack);
    g_sel = 0; UiChange(1); CHECK(passiveMs == 4000); UiChange(-1); UiChange(-1); CHECK(passiveMs == 2000);
    for (int i = 0; i < 40; ++i) UiChange(-1);
    CHECK(passiveMs == 0);
    g_sel = 1; UiChange(1); n = Items(items, kMaxRows + 4); CHECK(items[2] == kItSummonB);
    UiRow rows[24]; char footer[240];
    CHECK(UiFill(rows, 24, footer, sizeof(footer)) > 0 && strstr(rows[0].text, "Summon Boss"));
    g_sel = 2; CHECK(UiAct() && summonCalls == 1 && summonSlot == 1 && summonMission == 0x1CBC);
    // no live session (GetMoveBoss 0, empty slots): Practice tools leaves the root, stays on Settings
    bossLive = false; UiOpen();
    n = Items(items, kMaxRows + 4);
    CHECK(n == 5 && items[4] == kItClose);
    puts("PASS: root = Settings / Boss Rematch / Practice / Summon Boss (+ Practice tools when live); arena-only Practice page; tools, sliders, idle, passive time and slot summon dispatch independently");
    return 0;
}
