// Real merged menu dispatch with synthetic Training exports; no game or ASI load.
// user #14336 menu cleanup build: Training Bosses (one page per boss, unlimited letters), Summon another boss, timed CPU / idle switch,
// Boss X page (stance / aggro / gap / reset / health and Ki / move groups / Sequences / remove), 'Repeat this move' per move row.
#include <windows.h>
#include <cstring>
static int tuneCalls, moveCalls;
static int TuneGet(int which) { return which ? 1000 : 6000; }
static int TuneSet(int, int, int) { ++tuneCalls; return 1; }
static int moveOff[0x10000];  // 1 = that action id is OFF (Yoritomo only in this fixture)
static int MoveGet(unsigned, unsigned id) { return !moveOff[id & 0xFFFF]; }
static int MoveSet(unsigned, unsigned id, int on, int) { ++moveCalls; moveOff[id & 0xFFFF] = !on; return 1; }
static unsigned repChr, repId; static int repArms, repStops;
static int RepSet(unsigned chr, unsigned id, int on) {
    if (!on) { ++repStops; if (repChr == chr) repChr = repId = 0; return 1; }
    if (id == 0 || id == 2) return 0;
    ++repArms; repChr = chr; repId = id; return 1;
}
static unsigned RepGet(unsigned chr) { return chr == repChr ? repId : 0; }
static int repSeq, repSeqArms;  // user #14423: sequence repeat (whole chain)
static int RepSetSeq(unsigned chr, unsigned id, int on) { const int r = RepSet(chr, id, on); repSeq = on && r; repSeqArms += on && r; return r; }
static int RepSetOne(unsigned chr, unsigned id, int on) { repSeq = 0; return RepSet(chr, id, on); }
static int RepGetSeq(unsigned chr) { return chr == repChr && repId && repSeq; }
// slots: A = Yoritomo (live), the rest empty; 26 letters
static unsigned slotChr[26] = {16102u};
static int SlotCount() { return 26; }
static unsigned SlotChrX(int s) { return s >= 0 && s < 26 ? slotChr[s] : 0; }
static int stance[26] = {2}, stanceSets, lastStanceSlot = -1, lastStanceIdle = -1;
static int GetSlotStance(int s) { return slotChr[s] ? stance[s] : -2; }
static int SetSlotStance(int s, int idle) { if (!slotChr[s]) return 0; ++stanceSets; lastStanceSlot = s; lastStanceIdle = idle; stance[s] = idle ? 2 : 0; return 1; }
static int aggro[26];
static int GetSlotAggro(int s) { return slotChr[s] ? aggro[s] : -1; }
static int SetSlotAggro(int s, int on) { if (!slotChr[s]) return -1; return aggro[s] = on; }
static int gapMs = 1000;
static int GetGapChr(unsigned) { return gapMs; }
static int SetGapChr(unsigned, int ms, int) { return gapMs = ms; }
static int resetSlot = -1;
static int ResetSlotPos(int s) { resetSlot = s; return slotChr[s] != 0; }
static int despawnSlot = -1, despawnAll;
static int DespawnSlot(int s) { despawnSlot = s; return 1; }
static int DespawnAll() { ++despawnAll; return 1; }
static int timerMs = -1, timerIdle = -1;
static int StanceTimer(int ms, int idle) { timerMs = ms; timerIdle = idle; return ms > 0; }
static int GetStanceTimer(int* idle) { if (idle) *idle = timerIdle; return 0; }
static int summonCalls, summonSlot = -1; static unsigned summonMission;
static int SummonSlot(int slot, unsigned mission) { ++summonCalls; summonSlot = slot; summonMission = mission; return 1; }
static int trainHereCalls; static unsigned trainHereMission;
static int TrainHere(unsigned mission) { ++trainHereCalls; trainHereMission = mission; return 1; }
static int sessionMode = 0, sessionActive = 0, fightAgainCalls;
static int SessionMode() { return sessionMode; }
static int SessionActive() { return sessionActive; }
static int GateFightAgain() { ++fightAgainCalls; return 1; }
static int bvCfg[26][4], bvSets, bvRefills;
static int BvGetX(int slot, unsigned long long* hc, unsigned long long* hm, float* kc, float* km, int* cfg) {
    if (slot < 0 || slot > 25) return -1;
    for (int i = 0; i < 4; ++i) cfg[i] = bvCfg[slot][i];
    *hc = slot == 0 ? 25856 : 0; *hm = slot == 0 ? 51712 : 0; *kc = slot == 0 ? 520.0f : 0; *km = slot == 0 ? 1040.0f : 0;
    return slotChr[slot] != 0;
}
static int BvSetX(int slot, int what, int value) {
    ++bvSets;
    if (slot < 0 || slot > 25) return -1;
    if (what == 4) { bvCfg[slot][2] = bvCfg[slot][3] = 100; ++bvRefills; return 1; }
    return bvCfg[slot][what] = what < 2 ? (value < 0 ? !bvCfg[slot][what] : value != 0) : value;
}
static int pst[18] = {0, 100, 100, 0, 100, 0, 100, 0, 100, 0, 0, 0, 0, 0, 0, 0, 100, 1}, pstSets, pstPersist;  // user #22389 Practice stats (PS_* order, practice_stats.h)
static int PstGetX(int what) { return what >= 0 && what < 18 ? pst[what] : -1; }
static int resetFightCalls; static int ResetFightX() { ++resetFightCalls; return 15; }  // user #22592 Reset fight
static int PstSetX(int what, int v, int persist) { if (what < 0 || what > 17) return -1; if ((what == 7 || what == 9 || what == 10 || what == 11 || what == 14 || what == 15 || what == 17) && v < 0) v = !pst[what]; ++pstSets; pstPersist += persist != 0; return pst[what] = v; }
static HMODULE TestModule(const wchar_t*) { return (HMODULE)1; }
static FARPROC TestProc(HMODULE, const char* name) {
#define EXPORT(n, f) if (!strcmp(name, "Nioh3Training_" n)) return (FARPROC)f
    EXPORT("GetTuning", TuneGet); EXPORT("SetTuning", TuneSet);
    EXPORT("GetMoveAllowed", MoveGet); EXPORT("SetMoveAllowed", MoveSet);
    EXPORT("SetMoveRepeat", RepSetOne); EXPORT("GetMoveRepeat", RepGet); EXPORT("SetMoveRepeatSeq", RepSetSeq); EXPORT("GetMoveRepeatSeq", RepGetSeq);
    EXPORT("SlotCount", SlotCount); EXPORT("SlotChr", SlotChrX); EXPORT("GetSlotStance", GetSlotStance); EXPORT("SetSlotStance", SetSlotStance);
    EXPORT("GetSlotAggro", GetSlotAggro); EXPORT("SetSlotAggro", SetSlotAggro); EXPORT("GetRepeatGapChr", GetGapChr); EXPORT("SetRepeatGapChr", SetGapChr);
    EXPORT("ResetSlotPos", ResetSlotPos); EXPORT("DespawnSlot", DespawnSlot); EXPORT("DespawnAll", DespawnAll);
    EXPORT("StanceTimer", StanceTimer); EXPORT("GetStanceTimer", GetStanceTimer);
    EXPORT("SummonSlot", SummonSlot); EXPORT("TrainHere", TrainHere);
    EXPORT("SessionMode", SessionMode); EXPORT("SessionActive", SessionActive); EXPORT("GateFightAgain", GateFightAgain);
    EXPORT("BossVitalsGet", BvGetX); EXPORT("BossVitalsSet", BvSetX);
    EXPORT("ResetFight", ResetFightX); EXPORT("PracticeStatGet", PstGetX); EXPORT("PracticeStatSet", PstSetX);
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
    int n = 0;
    auto list = [&]() { n = Items(items, kMaxRows + 4); for (int i = 0; i < n; ++i) for (int j = i + 1; j < n; ++j) CHECK(items[i] != items[j]); return n; };
    auto has = [&](int it) { for (int i = 0; i < n; ++i) if (items[i] == it) return true; return false; };
    auto at = [&](int it) { for (int i = 0; i < n; ++i) if (items[i] == it) return i; return -1; };
    UiRow rows[40]; char footer[240];
    auto shown = [&](const char* a, const char* b = nullptr) {
        const int k = UiFill(rows, 40, footer, sizeof footer);
        for (int i = 0; i < k; ++i) if (strstr(rows[i].text, a) && (!b || strstr(rows[i].text, b))) return true;
        return false;
    };
    Row boss = {}; strcpy_s(boss.name, "Summon Test"); boss.missionId = 0x1CBC; boss.route = rcore::kR; g_rows.push_back(boss);
    UiOpen();
    // root = Settings / Boss Matches / Practice / Close (no Leave arena here, #14336 item 9)
    list(); CHECK(n == 4 && items[0] == kItSettings && items[1] == kItRootRematch && items[2] == kItRootTraining && items[3] == kItClose);
    g_sel = 1; CHECK(!UiAct() && g_uiPages.page == uRematch);
    list();
    for (int i = 0; i < n; ++i) {
        CHECK(items[i] != kItLeaveArena && items[i] != kItPractice && items[i] != kItOpen && items[i] != kItScroll);  // #14336 items 9 / 10
        if (items[i] == kItFinT || items[i] == kItHp0W) { g_sel = i; UiChange(1); }
    }
    CHECK(tuneCalls == 2 && items[n - 1] == kItBack);
    CHECK(!UiBack() && g_uiPages.page == uRoot);
    // Practice hub: Train here / arena / Training Bosses / Summon / Boss Matches
    g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uPracticeHub);
    list(); CHECK(n == 7 && items[0] == kItTrainHereOpen && items[1] == kItArenas && items[2] == kItRootTools && items[3] == kItPstResetFight && items[4] == kItBossSumm && items[5] == kItRootRematch && items[6] == kItBack);
    CHECK(!has(kItPstBase + PS_TIME_OF_DAY) && !has(kItPstBase + PS_WEATHER));  // global sky controls live in Display only
    CHECK(!has(kItPstBase + PS_FRAME_METER) && !has(kItPstAllRows) && !has(kItPstAllKeys) && !shown("Frame meter:"));  // frame meter: Display Overlays only
    CHECK(shown("Reset fight")); g_sel = 3; UiChange(1); CHECK(resetFightCalls == 0); CHECK(!UiAct() && resetFightCalls == 1 && strstr(g_msg, "Fight reset") && strstr(g_msg, "back at your Practice start")); g_sel = 0;  // #22592 one press, Left / Right do nothing
    CHECK(shown("Training Bosses (1 boss)") && !shown("Training mods"));
    sessionActive = 1; list();  // #14333: already in a Training arena: no Train here / Train in a boss arena
    CHECK(n == 5 && !has(kItTrainHereOpen) && !has(kItArenas) && items[0] == kItRootTools && items[1] == kItPstResetFight);
    sessionActive = 0; list();
    // Training Bosses: Boss A row, Summon another boss, timer, Remove all (A listed), no old duplicate rows
    g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uTools);
    list();
    CHECK(n == 5 && items[0] == kItBossOpenBase && items[1] == kItSumAnother && items[2] == kItStanceTimer && items[3] == kItDespawnAll && items[4] == kItBack);
    for (int i = 0; i < n; ++i) CHECK(items[i] != kItMoves && items[i] != kItMovesA && items[i] != kItMovesB && items[i] != kItRepGap && items[i] != kItIdleHold &&
                                      items[i] != kItFightOpts && !IsBvOpenItem(items[i]) && items[i] != kItAggro && items[i] != kItRepNow);
    CHECK(shown("Practice - Training Bosses") && shown("Boss A: Minamoto no Yoritomo", "Stand still (idle)") && shown("it will be Boss B"));
    CHECK(shown("Remove all summoned bosses (A: Minamoto no Yoritomo)") && !shown("B: empty"));
    // timer slider 1..10 s; A arms it for every boss (A stands still -> all fight), closes the menu
    g_sel = 2; for (int i = 0; i < 20; ++i) UiChange(1);
    CHECK(g_stSec == 10); for (int i = 0; i < 20; ++i) UiChange(-1);
    CHECK(g_stSec == 1); UiChange(1); UiChange(1); CHECK(g_stSec == 3 && shown("Switch every boss to Fight (CPU) after:", " 3 s"));
    CHECK(UiAct() && timerMs == 3000 && timerIdle == 0);
    // Boss A page: every per-boss setting once
    UiOpenAt(2); g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uTools);
    g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uBoss && g_bossSlot == 0 && MoveBoss() == 16102u);
    list();
    CHECK(items[0] == kItBossStance && items[1] == kItBossAggro && items[2] == kItBossGap && items[3] == kItBossReset && items[4] == kItBvOpenBase + 0);
    {   // user #22349: Yoritomo's groups from the game data: 1 / 2 / 3 hits, Unblockable, Other attacks (hits unknown), then twitches, Sequences
        const int want[] = {kMcHit1, kMcHit1 + 1, kMcHit1 + 2, kMcGreat, kMcHitUnknown, kMcTwitch, kMcSeq};
        const int nw = (int)(sizeof want / sizeof want[0]);
        for (int c = 0; c < nw; ++c) CHECK(items[5 + c] == kItMoveCatBase - want[c] && IsMoveCatItem(items[5 + c]));
        CHECK(items[5 + nw] == kItBossRemove && items[6 + nw] == kItBack && n == 7 + nw);
        CHECK(!MoveCatCount(16102u, kMcAttack) && !MoveCatCount(16102u, kMcUnnamedAtk));  // every attack is sorted by the data now
        auto cat = [](uint32_t chr, uint32_t id) { const int i = N3MoveFind(chr, id); return i < 0 ? -9 : N3MoveCat(&kN3Moves[i]); };
        CHECK(cat(16102u, 0x4E6E) == kMcGreat && cat(16102u, 0x4E6C) == kMcHit1 + 1 && cat(16102u, 0x4E68) == kMcHit1 + 2 && cat(16102u, 0x4E55) == kMcHit1);
        CHECK(cat(16102u, 0x4E74) == kMcHitUnknown && cat(16102u, 0x4EA5) == kMcHitUnknown && cat(16102u, 0x4E6F) == kMcGreat);  // unknown hits: not guessed
        CHECK(cat(16102u, 0x4E3B) == -1 && cat(16102u, 0x4E3C) == -1 && cat(16102u, 0x4E24) == kMcTwitch && cat(16102u, 0x2) == kMcTwitch);  // #14655 / twitches
        char tb[40];
        CHECK(!strcmp(N3MoveHitTag(&kN3Moves[N3MoveFind(16102u, 0x4E6E)], tb, sizeof tb), "2 hits, unblockable"));
        CHECK(!strcmp(N3MoveHitTag(&kN3Moves[N3MoveFind(16102u, 0x4E6F)], tb, sizeof tb), "unblockable, hits unknown"));
        CHECK(!*N3MoveHitTag(&kN3Moves[N3MoveFind(16102u, 0x4E6C)], tb, sizeof tb));
        CHECK(!strcmp(CatName(kMcHit1), "1 hit") && !strcmp(CatName(kMcHit1 + 2), "3 hits") && !strcmp(CatName(kMcGreat), "Unblockable"));
        // user #22709: every bound boss has game-data facts (same algorithm as Yoritomo); a boss without facts keeps the old groups
        const uint32_t kBound[] = {482895u, 257162u, 478320u, 451004u, 269750u, 109203u, 268880u, 835773u, 42450u, 83953u, 307259u, 352684u, 371449u, 392871u, 405836u, 778951u, 816820u};
        for (uint32_t b : kBound) {
            int hitMoves = 0, great = 0;
            CHECK(N3MoveFactsBoss(b));
            for (int i = 0; i < N3_MOVE_COUNT; ++i) if (kN3Moves[i].chr == b) {
                const int c = N3MoveCat(&kN3Moves[i]);
                hitMoves += N3MoveHitCat(c); great += c == kMcGreat;
                CHECK(c != kMcAttack && c != kMcUnnamedAtk);  // an attack is sorted by the data, never left in the old groups
            }
            CHECK(hitMoves > 0 || b == 835773u);  // Himiko: every attack spawns projectiles (hits unknown, not guessed)
            (void)great;
        }
        CHECK(!N3MoveFactsBoss(894462u));  // Ibaraki Doji: no bound action file, old groups
        CHECK(cat(83953u, 0x4E67) == kMcHit1 + 2 && cat(83953u, 0x4E66) == kMcHit1 + 1);  // Hino-enma: 3 hits / 2 hits from the game data
    }
    CHECK(shown("Boss A - Minamoto no Yoritomo") && shown("Stance: Stand still (idle)") && shown("Gap between repeats: 1.00 s") && shown("Sequences", "sequences"));
    g_sel = 0; CHECK(!UiAct() && lastStanceSlot == 0 && lastStanceIdle == 0 && stance[0] == 0 && shown("Stance: Fight normally (CPU)"));
    UiChange(1); CHECK(lastStanceIdle == 1 && stance[0] == 2);
    g_sel = 1; CHECK(!UiAct() && aggro[0] == 1); UiChange(-1); CHECK(aggro[0] == 0);
    g_sel = 2; UiChange(1); CHECK(gapMs == 1250); UiChange(-1); UiChange(-1); CHECK(gapMs == 750);
    g_sel = 3; CHECK(!UiAct() && resetSlot == 0);
    {   // the Training Bosses row shows the stance of each boss
        CHECK(!UiBack() && g_uiPages.page == uTools && shown("Boss A: Minamoto no Yoritomo", "Stand still"));
        g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uBoss);
    }
    // '2 hits' group (#22349): A = Repeat this move ON / OFF, Left / Right = used
    const int kTwo = kMcHit1 + 1, twoN = MoveCatCount(16102u, kMcHit1 + 1);
    g_sel = 6; CHECK(!UiAct() && g_uiPages.page == uMoveCat && g_moveCat == kTwo && g_sel == 0);
    list(); CHECK(n == twoN + 1 && n > 3 && items[n - 1] == kItBack);
    for (int i = 0; i < n - 1; ++i) CHECK(IsMoveItem(items[i]) && N3MoveCat(&kN3Moves[items[i] - kItMoveBase]) == kTwo);
    {
        const N3Move& m0 = kN3Moves[items[0] - kItMoveBase];
        const N3Move& m1 = kN3Moves[items[1] - kItMoveBase];
        g_sel = 0; CHECK(!UiAct() && RepGet(16102) == m0.id && repArms == 1);
        char want[48]; snprintf(want, sizeof want, "0x%04X", m0.id);
        char title[64]; snprintf(title, sizeof title, "Boss A - Minamoto no Yoritomo - 2 hits (%d)", twoN);
        CHECK(shown(want, "Repeat ON") && shown(title) && !shown("RPT") && !shown("REPEAT"));
        g_sel = 1; CHECK(!UiAct() && RepGet(16102) == m1.id && repArms == 2);   // one repeat per boss: moves to the other move
        CHECK(!UiAct() && !RepGet(16102) && repStops == 1);                    // A again: stops
        g_sel = 0; UiChange(-1); CHECK(moveOff[m0.id] && moveCalls == 1);      // Left: used OFF
        UiChange(-1); CHECK(moveCalls == 1);                                    // already OFF: no call
        CHECK(!UiAct() && !moveOff[m0.id] && RepGet(16102) == m0.id);           // A on an OFF move: switched ON + repeats
        UiChange(-1); CHECK(moveOff[m0.id] && !RepGet(16102));                  // used OFF stops its repeat
        UiChange(1); CHECK(!moveOff[m0.id]);
        char tb[160]; uint32_t tc = 0; CHECK(Toast(tb, sizeof tb, tc));
        CHECK(strstr(footer, "Repeat") || strstr(footer, "repeat"));
    }
    {
        char cnt[24]; snprintf(cnt, sizeof cnt, "%d moves", twoN);
        CHECK(!UiBack() && g_uiPages.page == uBoss && g_sel == 6 && shown("2 hits", cnt));
    }
    // Unblockable group (#22349): only great moves, each row says its hit count
    list(); g_sel = at(kItMoveCatBase - kMcGreat); CHECK(!UiAct() && g_uiPages.page == uMoveCat && g_moveCat == kMcGreat);
    list(); CHECK(n == MoveCatCount(16102u, kMcGreat) + 1 && n == 20);  // 19 GreatAttack moves (notes/yoritomo-hit-source-22305.md)
    CHECK(shown("0x4E6E", "[2 hits, unblockable]") && shown("0x4E72", "[unblockable, hits unknown]") && shown("[2 hits, unblockable]", "Used ON") &&
          shown("[unblockable, hits unknown]", "Used ON"));  // the tag never pushes the controls off the row
    CHECK(!UiBack() && g_uiPages.page == uBoss);
    // Sequences (#14317): chains from the next-action links, A repeats the whole sequence (its first move)
    list(); g_sel = at(kItMoveCatBase - kMcSeq); CHECK(!UiAct() && g_uiPages.page == uMoveCat && g_moveCat == kMcSeq);
    list(); CHECK(n == SeqCount(16102u) + 1 && n > 3);  // 6 Yoritomo sequences since the #14665 / #14702 filters (was > 10 before them)
    for (int i = 0; i < n - 1; ++i) CHECK(IsSeqItem(items[i]));
    {
        const N3Move& h = kN3Moves[items[0] - kItSeqBase];
        uint32_t ids[8]; bool loop = false;
        CHECK(SeqChain(16102u, h.id, ids, 8, &loop) >= 2);
        uint32_t c5[8]; bool l5 = false; CHECK(SeqChain(16102u, 0x4E5C, c5, 8, &l5) == 3 && c5[1] == 0x4E60 && c5[2] == 0x4E61 && l5);
        CHECK(SeqHead(16102u, 0x4E5C) && !SeqHead(16102u, 0x4E60) && !SeqHead(16102u, 0x2));
        g_sel = 0; CHECK(!UiAct() && RepGet(16102) == h.id && RepGetSeq(16102) && repSeqArms == 1);  // #14423: armed as a whole sequence
        CHECK(shown(" > ", "Repeat ON") && shown("Boss A - Minamoto no Yoritomo - Sequences"));
        CHECK(!UiAct() && !RepGet(16102));
        // a single-move repeat of the head is not shown as the sequence repeating; A on the row arms the sequence instead of stopping
        RepSetOne(16102u, h.id, 1); list(); CHECK(shown(" > ", "Repeat OFF"));
        CHECK(!UiAct() && RepGetSeq(16102) && repSeqArms == 2);
        CHECK(!UiAct() && !RepGet(16102));
    }
    CHECK(!UiBack() && g_uiPages.page == uBoss);
    // Health and Ki from the Boss A page
    g_sel = 4; CHECK(!UiAct() && g_uiPages.page == uBossVitals && g_bvSlot == 0 && g_sel == 1);
    CHECK(shown("Boss A health and Ki") && shown("HP 25856 / 51712 (50 %)"));
    CHECK(!UiAct() && bvCfg[0][0] == 1);
    {   // user #22389: Boss X page = that boss's infinite Ki row, then Damage boss deals / Boss Ki drain / Boss speed (all Training bosses, saved)
        list(); const int d = at(kItPstBase + PS_BOSS_KI_DRAIN), t = at(kItPstBase + PS_DMG_TAKEN);
        CHECK(at(kItBvInfKi) >= 0 && t > at(kItBvInfKi) && d == t + 1 && at(kItPstBase + PS_ONE_HIT_KI) == d + 1 && at(kItPstBase + PS_HOLD_KI_BREAK) == d + 2 && at(kItPstBase + PS_HOLD_KI_V2) == d + 3 && at(kItPstBase + PS_KI_FLOOR) == d + 4 && at(kItPstBase + PS_KI_LOG) == d + 5 && at(kItPstBase + PS_NEG_KI_METER) == d + 6 && at(kItPstBase + PS_BOSS_SPEED) == d + 7 && items[n - 1] == kItBack);
        CHECK(!has(kItPstBase + PS_DMG_DEALT) && !has(kItPstBase + PS_PLAYER_INF_KI) && !has(kItPstBase + PS_KI_DEALT));
        {   // user #22588 One-hit Ki break: toggle, default OFF, saved
            CHECK(shown("One-hit Ki break:", "OFF")); const int o = at(kItPstBase + PS_ONE_HIT_KI), p1 = pstPersist;
            g_sel = o; CHECK(!UiAct() && pst[9] == 1 && shown("One-hit Ki break:", "ON") && pstPersist == p1 + 1); CHECK(!UiAct() && pst[9] == 0 && shown("One-hit Ki break:", "OFF"));
            UiChange(1); CHECK(pst[9] == 1); UiChange(-1); CHECK(pst[9] == 0);
        }
        {   // user #22918 Hold Ki break: toggle right after One-hit Ki break, default OFF, saved
            CHECK(shown("Hold Ki break:", "OFF")); const int o = at(kItPstBase + PS_HOLD_KI_BREAK), p1 = pstPersist;
            g_sel = o; CHECK(!UiAct() && pst[11] == 1 && shown("Hold Ki break:", "ON") && pstPersist == p1 + 1); CHECK(!UiAct() && pst[11] == 0 && shown("Hold Ki break:", "OFF"));
            UiChange(1); CHECK(pst[11] == 1); UiChange(-1); CHECK(pst[11] == 0);
        }
        {   // user #23013: Hold Ki break v2 / Ki break log toggles (default OFF, saved), Ki floor depth slider x0 .. x10 (default x1.00 = stock)
            CHECK(shown("Hold Ki break v2:", "OFF") && shown("Ki break log:", "OFF") && shown("Ki floor depth:", "x1.00"));
            const int p1 = pstPersist;
            g_sel = at(kItPstBase + PS_HOLD_KI_V2); CHECK(!UiAct() && pst[15] == 1 && shown("Hold Ki break v2:", "ON") && pstPersist == p1 + 1); CHECK(!UiAct() && pst[15] == 0);
            g_sel = at(kItPstBase + PS_KI_LOG); CHECK(!UiAct() && pst[14] == 1 && shown("Ki break log:", "ON")); UiChange(-1); CHECK(pst[14] == 0 && shown("Ki break log:", "OFF"));
            g_sel = at(kItPstBase + PS_KI_FLOOR); UiChange(1); CHECK(pst[16] == 150 && shown("Ki floor depth:", "x1.50")); UiChange(1); UiChange(1); CHECK(pst[16] == 300 && shown("Ki floor depth:", "x3.00"));
            for (int i = 0; i < 20; ++i) UiChange(1);
            CHECK(pst[16] == 1000 && shown("Ki floor depth:", "x10.00")); for (int i = 0; i < 20; ++i) UiChange(-1);
            CHECK(pst[16] == 0 && shown("Ki floor depth:", "x0.00")); pst[16] = 100;
        }
        {   // user #23400 Negative Ki meter: toggle right after Ki break log, default ON, saved
            CHECK(shown("Negative Ki meter:", "ON")); const int p1 = pstPersist;
            g_sel = at(kItPstBase + PS_NEG_KI_METER); CHECK(!UiAct() && pst[17] == 0 && shown("Negative Ki meter:", "OFF") && pstPersist == p1 + 1);
            UiChange(1); CHECK(pst[17] == 1 && shown("Negative Ki meter:", "ON"));
        }
        CHECK(shown("Boss Ki drain:", "OFF") && shown("Damage boss deals:", "x1.00") && shown("-- All Training bosses (saved) --"));
        const int p0 = pstPersist;
        g_sel = d; UiChange(1); UiChange(1); CHECK(pst[3] == 10 && shown("Boss Ki drain:", "10 Ki / s") && pstPersist == p0 + 2);
        UiChange(-1); UiChange(-1); UiChange(-1); CHECK(pst[3] == 0 && shown("Boss Ki drain:", "OFF"));
        g_sel = t; for (int i = 0; i < 20; ++i) UiChange(-1);
        CHECK(pst[2] == 0 && shown("Damage boss deals:", "x0.00")); for (int i = 0; i < 20; ++i) UiChange(1);
        CHECK(pst[2] == 1000 && shown("Damage boss deals:", "x10.00")); pst[2] = 100;
    }
    CHECK(!UiBack() && g_uiPages.page == uBoss);
    // Remove this boss -> back to Training Bosses
    list(); g_sel = at(kItBossRemove); CHECK(!UiAct() && despawnSlot == 0 && g_uiPages.page == uTools);
    // Summon another boss: boss list only (no slot picker / wait slider), next free letter = B
    list(); g_sel = at(kItSumAnother); CHECK(!UiAct() && g_uiPages.page == uSummon);
    list(); CHECK(n == 2 && items[0] == kItSummonA && items[1] == kItBack);
    CHECK(shown("Practice - Summon another boss") && !shown("Wait before attacking") && !shown("Summon into"));
    g_sel = 0; CHECK(UiAct() && summonCalls == 1 && summonSlot == 1 && summonMission == 0x1CBC);
    // more bosses: every spawned letter gets a row (C here), the free one is offered next
    slotChr[1] = 16102u; slotChr[2] = 16102u; stance[1] = 0; stance[2] = 2;
    UiOpenAt(2); g_sel = 2; CHECK(!UiAct() && g_uiPages.page == uTools);
    list(); CHECK(items[0] == kItBossOpenBase && items[1] == kItBossOpenBase + 1 && items[2] == kItBossOpenBase + 2 && items[3] == kItSumAnother);
    CHECK(shown("Boss C: Minamoto no Yoritomo") && shown("Boss B:", "Fight normally") && shown("it will be Boss D") && shown("Remove all summoned bosses (A:", "C:"));
    g_sel = 2; CHECK(!UiAct() && g_bossSlot == 2); g_sel = 0; CHECK(!UiAct() && lastStanceSlot == 2 && stance[2] == 0 && stance[0] == 2 && stance[1] == 0);  // independent
    CHECK(!UiBack() && g_uiPages.page == uTools);
    list(); g_sel = at(kItDespawnAll); CHECK(!UiAct() && despawnAll == 1);
    for (int s = 0; s < 26; ++s) slotChr[s] = 0;
    list(); CHECK(n == 2 && items[0] == kItSumAnother && items[1] == kItBack);  // nothing spawned: no boss rows, no timer, no Remove all
    // Training here
    UiOpenAt(2); g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uTrainHere);
    list(); CHECK(n == 2 && items[0] == kItTrainHere && items[1] == kItBack);
    g_sel = 0; CHECK(UiAct() && trainHereCalls == 1 && trainHereMission == 0x1CBC);
    UiOpenAt(2); CHECK(g_uiPages.page == uPracticeHub && UiBack());
    UiOpenAt(1); CHECK(g_uiPages.page == uRematch && UiBack());
    // Settings: no second Training door (#14336 item 8)
    UiOpen(); g_sel = 0; CHECK(!UiAct() && g_uiPages.page == uSettings);
    list(); CHECK(n == kConfigGroupN + 1 && items[n - 1] == kItBack && !has(kItRootTools));
    // Return gate (#14336 item 11): short lists, one Back + Close
    sessionMode = 0; UiGatePage(); list();
    CHECK(n == 4 && items[0] == kItFightAgain && items[1] == kItReturnHome && items[2] == kItBack && items[3] == kItClose);
    sessionMode = 1; list();
    CHECK(items[0] == kItFightAgain && has(kItFinT) && has(kItHp0W) && !has(kItIdleHold) && !has(kItMoves) && !has(kItFightOpts) && items[n - 3] == kItReturnHome && items[n - 2] == kItBack && items[n - 1] == kItClose);
    sessionMode = 2; list();
    CHECK(n == 5 && items[0] == kItFightAgain && items[1] == kItRootTools && items[2] == kItReturnHome && items[3] == kItBack && items[4] == kItClose);
    CHECK(shown("Practice") && shown("-- Training Bosses --") && shown("Reset round"));
    g_sel = 1; CHECK(!UiAct() && g_uiPages.page == uTools && !UiBack() && g_uiPages.page == uGate);
    g_sel = 0; CHECK(UiAct() && fightAgainCalls == 1);
    UiGatePage(); CHECK(UiBack());
    sessionMode = 0;
    {   // user #22389 Practice > Player page: Infinite health, then Infinite Ki / Damage I deal / Infinite ninjutsu / Player speed (defaults OFF / x1.00)
        UiOpenAt(2); g_uiPages.Enter(uPlayer, g_sel); g_sel = 0; list();
        CHECK(!has(kItPstBase + PS_BOSS_KI_DRAIN) && !has(kItPstBase + PS_DMG_TAKEN) && !has(kItPstBase + PS_BOSS_SPEED) && has(kItPstBase + PS_PLAYER_INF_KI) && has(kItPstBase + PS_DMG_DEALT));
        CHECK(!has(kItPstBase + PS_ONE_HIT_KI) && !has(kItPstBase + PS_HOLD_KI_BREAK) && at(kItPstBase + PS_KI_DEALT) == at(kItPstBase + PS_DMG_DEALT) + 1);  // user #22588
        CHECK(has(kItPstBase + PS_PLAYER_INF_NIN) && has(kItPstBase + PS_PLAYER_SPEED) && items[n - 1] == kItBack);
        CHECK(at(kItPstBase + PS_PLAYER_INF_SOUL) == at(kItPstBase + PS_PLAYER_INF_NIN) + 1);  // pc #57367 Infinite soul cores right after Infinite ninjutsu
        CHECK(!has(kItPstBase + PS_PLAYER_METER) && !shown("Player frame meter:"));  // frame meter: Display Overlays only
        CHECK(shown("Infinite Ki (stamina):", "OFF") && shown("Damage I deal:", "x1.00"));
        g_sel = at(kItPstBase + PS_PLAYER_INF_KI); CHECK(!UiAct() && pst[0] == 1 && shown("Infinite Ki (stamina):", "ON")); CHECK(!UiAct() && pst[0] == 0);
        g_sel = at(kItPstBase + PS_DMG_DEALT); for (int i = 0; i < 20; ++i) UiChange(1);
        CHECK(pst[1] == 1000 && shown("Damage I deal:", "x10.00")); for (int i = 0; i < 20; ++i) UiChange(-1);
        CHECK(pst[1] == 10 && shown("Damage I deal:", "x0.10"));
        pst[1] = 100;
        {   // user #22588 Ki damage I deal: x0 .. x10, default x1.00
            CHECK(shown("Ki damage I deal:", "x1.00")); g_sel = at(kItPstBase + PS_KI_DEALT);
            for (int i = 0; i < 20; ++i) UiChange(1); CHECK(pst[8] == 1000 && shown("Ki damage I deal:", "x10.00"));
            for (int i = 0; i < 20; ++i) UiChange(-1); CHECK(pst[8] == 0 && shown("Ki damage I deal:", "x0.00")); pst[8] = 100;
        }
        CHECK(!UiBack());
    }
    puts("PASS: #14336 menu cleanup: root / Boss Matches without Leave arena or dev rows; Practice hub hides Train here / arena in a session (#14333); "
         "Training Bosses = one row per spawned boss (A, B, C ...), Summon another boss (next free letter), CPU / idle timer 1-10 s (#14313), Remove all lists the bosses; "
         "Boss X page = stance / aggro / gap / reset / health and Ki / move groups / Sequences (#14317) / remove, independent per boss; one 'Repeat ON / OFF' per move row; "
         "Settings has no Training door; short Return gate pages; #22389 Practice stats rows (Player page, boss Ki drain)");
    return 0;
}
