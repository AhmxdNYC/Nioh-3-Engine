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
static int moveOff[0x10000];  // 1 = that action id is OFF (Yoritomo only in this fixture)
static int MoveGet(unsigned, unsigned id) { return !moveOff[id & 0xFFFF]; }
static int MoveSet(unsigned, unsigned id, int on, int) { ++moveCalls; moveOff[id & 0xFFFF] = !on; return 1; }
// user #13618: Repeat exports (one repeat per boss, protected idle / init refused like the real Nioh3Training_SetMoveRepeat)
static unsigned repChr, repId; static int repArms, repStops;
static int RepSet(unsigned chr, unsigned id, int on) {
    if (!on) { ++repStops; if (repChr == chr) repChr = repId = 0; return 1; }
    if (id == 0 || id == 2) return 0;
    ++repArms; repChr = chr; repId = id; return 1;
}
static unsigned RepGet(unsigned chr) { return chr == repChr ? repId : 0; }
static int RepGapGet() { return 1250; }
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
    EXPORT("SetMoveRepeat", RepSet); EXPORT("GetMoveRepeat", RepGet); EXPORT("GetRepeatGap", RepGapGet);
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
    // user #14291: Yoritomo hub = Repeat selected move + Repeating move + stance + one row per move group (6 groups, 173 moves) + Back
    CHECK(n == 10 && items[0] == kItRepSel && items[1] == kItRepNow && items[2] == kItIdleHold && items[n - 1] == kItBack);
    {
        const int want[kMcCount] = {24, 70, 8, 13, 44, 14};  // Attacks, Unnamed attacks, Twitches, Ki break, Movement and reactions, Unsorted
        int total = 0;
        for (int c = 0; c < kMcCount; ++c) { CHECK(items[3 + c] == kItMoveCatBase - c); CHECK(MoveCatCount(16102u, c) == want[c]); total += MoveCatCount(16102u, c); }
        CHECK(total == 173);
        auto cat = [](uint32_t id) { return N3MoveCat(&kN3Moves[N3MoveFind(16102u, id)]); };
        CHECK(cat(0x4E38) == kMcKiBreak && cat(0x4E3E) == kMcKiBreak && cat(0xFA6) == kMcKiBreak && cat(0x4E4C) == kMcKiBreak && cat(0x4E40) == kMcKiBreak);
        CHECK(cat(0x4E64) == kMcAttack && cat(0x4E24) == kMcTwitch && cat(0x3FB) == kMcMovement && cat(0x2) == kMcMovement &&
              cat(0x4E4F) == kMcUnnamedAtk && cat(0x4E21) == kMcUnsorted);
        UiRow hrows[24]; char hfooter[240];
        const int k = UiFill(hrows, 24, hfooter, sizeof hfooter);
        bool atk = false, ki = false, hdr = false;
        for (int i = 0; i < k; ++i) {
            atk |= strstr(hrows[i].text, "Attacks") && strstr(hrows[i].text, "24 moves") && strstr(hrows[i].text, "A: open");
            ki |= strstr(hrows[i].text, "Ki break") && strstr(hrows[i].text, "13 moves");
            hdr |= strstr(hrows[i].text, "-- Pick a move group") != nullptr;
        }
        CHECK(atk && ki && hdr);
    }
    for (int i = 0; i < n; ++i) if (items[i] == kItIdleHold) { g_sel = i; UiChange(1); CHECK(!UiAct()); }
    auto openGroup = [&](int c) -> int {  // from the hub: A on the group row -> its page, cursor on the first move
        g_sel = 3 + c; CHECK(!UiAct() && g_uiPages.page == uMoveCat && g_moveCat == c && g_sel == 1);
        n = Items(items, kMaxRows + 4);
        CHECK(items[0] == kItRepNow && items[n - 1] == kItBack && n == MoveCatCount(16102u, c) + 2);
        for (int i = 1; i < n - 1; ++i) CHECK(IsMoveItem(items[i]) && N3MoveCat(&kN3Moves[items[i] - kItMoveBase]) == c);
        return 0;
    };
    CHECK(!openGroup(kMcAttack));
    int rowOff = -1, rowOn = -1, nAtk = 0;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) CHECK(items[i] != items[j]);
        if (IsMoveItem(items[i]) && !N3MoveProtected(kN3Moves[items[i]-kItMoveBase].id)) {
            ++nAtk;
            g_sel = i; CHECK(!UiAct()); CHECK(moveOff[kN3Moves[items[i]-kItMoveBase].id]);  // A: ON -> OFF
            if (rowOff < 0) rowOff = i; else if (rowOn < 0) { rowOn = i; CHECK(!UiAct()); CHECK(!moveOff[kN3Moves[items[i]-kItMoveBase].id]); }  // A again: ON
        }
    }
    CHECK(nAtk == 24 && tuneCalls == 2 && idleCalls == 2 && moveCalls == nAtk + 1 && !repArms);
    UiRow mrows[40]; char mfooter[240];
    {   // user #13618: Right on an OFF move switches it ON and arms the repeat at once (was: only switched ON, the repeat never armed)
        const N3Move& off = kN3Moves[items[rowOff] - kItMoveBase];
        CHECK(moveOff[off.id] && !RepGet(16102));
        g_sel = rowOff; UiChange(1);
        CHECK(!moveOff[off.id] && repArms == 1 && RepGet(16102) == off.id && moveCalls == nAtk + 2);
        char tb[160]; uint32_t tc = 0;
        CHECK(Toast(tb, sizeof tb, tc) && strstr(tb, "REPEAT") && tc == cGreen);
        const int k = UiFill(mrows, 40, mfooter, sizeof mfooter);
        bool pill = false, top = false, title = false;
        char want[16]; snprintf(want, sizeof want, "0x%04X  REPEAT", off.id);
        char what[64]; snprintf(what, sizeof what, "Boss uses 0x%04X each time it attacks (gap 1.25 s)", off.id);
        for (int i = 0; i < k; ++i) {
            pill |= strstr(mrows[i].text, want) != nullptr;
            top |= strstr(mrows[i].text, "Repeating move: ") && strstr(mrows[i].text, what);
            title |= strstr(mrows[i].text, "Boss moves - Minamoto no Yoritomo - Attacks (24)") != nullptr;
        }
        CHECK(pill && top && title && strstr(mfooter, "REPEAT"));
        // Right on another (ON) move moves the repeat there; Left anywhere stops it; Left again: nothing repeating (toast says so)
        const N3Move& on = kN3Moves[items[rowOn] - kItMoveBase];
        g_sel = rowOn; UiChange(1); CHECK(RepGet(16102) == on.id && repArms == 2 && moveCalls == nAtk + 2);
        g_sel = rowOff; UiChange(-1); CHECK(!RepGet(16102) && repStops == 1 && moveCalls == nAtk + 2);
        g_sel = rowOff; UiChange(-1); CHECK(repStops == 1 && Toast(tb, sizeof tb, tc) && strstr(tb, "No move is repeating"));
        // B: back to the group list, cursor on the Attacks row; 'Repeat selected move' (hub row 0) repeats the last move pressed (rowOff)
        CHECK(!UiBack() && g_uiPages.page == uMoves && g_sel == 3 + kMcAttack);
        g_sel = 0; CHECK(!UiAct() && RepGet(16102) == off.id && repArms == 3);
        {   // the group row shows where the repeat is
            const int hk = UiFill(mrows, 40, mfooter, sizeof mfooter);
            char rp[24]; snprintf(rp, sizeof rp, "REPEAT 0x%04X", off.id);
            bool shown = false;
            for (int i = 0; i < hk; ++i) shown |= strstr(mrows[i].text, "Attacks") && strstr(mrows[i].text, rp);
            CHECK(shown);
        }
        // A turning a repeating move OFF also stops its repeat
        CHECK(!openGroup(kMcAttack));
        g_sel = rowOff; CHECK(!UiAct() && moveOff[off.id] && !RepGet(16102));
        // Right on idle / init (group 'Movement and reactions'): refused with a reason, nothing armed
        CHECK(!UiBack() && g_uiPages.page == uMoves);
        CHECK(!openGroup(kMcMovement));
        bool prot = false;
        for (int i = 0; i < n; ++i) if (IsMoveItem(items[i]) && N3MoveProtected(kN3Moves[items[i]-kItMoveBase].id)) { g_sel = i; UiChange(1); prot = true; break; }
        CHECK(prot && !RepGet(16102) && repArms == 3 && Toast(tb, sizeof tb, tc) && tc == cRed && strstr(tb, "cannot be repeated"));
        // 'Repeating move' row (page row 0): A stops
        CHECK(!UiBack() && g_uiPages.page == uMoves);
        CHECK(!openGroup(kMcAttack));
        g_sel = rowOn; UiChange(1); CHECK(RepGet(16102) == on.id); g_sel = 0; CHECK(!UiAct() && !RepGet(16102));
        CHECK(!UiBack() && g_uiPages.page == uMoves);
        // Ki break group: the live-logged ids first with plain names, then the rest of the motion family
        CHECK(!openGroup(kMcKiBreak));
        for (int i = 1; i <= 6; ++i) CHECK(N3KiNote(16102u, kN3Moves[items[i] - kItMoveBase].id) != nullptr);
        CHECK(!N3KiNote(16102u, kN3Moves[items[7] - kItMoveBase].id));
        const int kk = UiFill(mrows, 40, mfooter, sizeof mfooter);
        bool kiName = false;
        for (int i = 0; i < kk; ++i) kiName |= strstr(mrows[i].text, "0x4E38") && strstr(mrows[i].text, "good, seen live") && !strstr(mrows[i].text, "(?)");
        CHECK(kiName && strstr(mfooter, "seen live"));
        CHECK(!UiBack() && g_uiPages.page == uMoves && g_sel == 3 + kMcKiBreak);
        for (int i = 0; i < 0x10000; ++i) moveOff[i] = 0;
    }
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
    puts("PASS: Boss moves groups (#14291: 6 groups, 173 Yoritomo moves, Ki break ids first, B back to the group list); Right on an OFF move = ON + REPEAT, Left stops, Repeat selected move, REPEAT pill / top row text / toasts; root = Settings / Boss Matches / Practice; Practice = Train here / arena / Training mods / Summon a boss here / Boss Matches; section headers; direct opens; sliders, idle, moves, passive time, train-here and slot summon dispatch; mode-aware gate page; boss fight settings page");
    return 0;
}
