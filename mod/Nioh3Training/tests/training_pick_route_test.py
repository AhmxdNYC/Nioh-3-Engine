"""#121854 / human #121995: a Training page pick never becomes a native-first Boss Match, and it still goes to the boss's arena.

Boss Rematch's Training > Choose an arena row (kItTeleport) calls SetPracticeNext(1) and posts the same Fight now request as a
Boss Matches row (SetPracticeNext(0)). Nioh3Training_FightNow must send the Training pick to the legacy arena route (arena swap /
cross-world switch, Practice kept) for every mission row, fall back to the in-place Practice summon only when that route refuses,
and leave Boss Matches picks on native-first. No row ids are involved.
Runs the production Nioh3Training_FightNow / NativeFirst source against mocks.
"""
from pathlib import Path
import os, subprocess, sys, tempfile

root = Path(__file__).resolve().parents[1]
s = Path(os.environ.get("N3T_TRAINING_SRC", root / "src/training.c")).read_text()

def cut(start, end):
    a = s.index(start); return s[a:s.index(end, a)]

native = cut('static int NativeMissionRow(', 'static __attribute__((noinline)) int MsRead(')
fightnow = cut('N3T_API int Nioh3Training_FightNow(uint32_t mission) {', 'N3T_API void Nioh3Training_SetArenaOnlyNext(')

# The menu contract the route keys on: the Training page arena row sets Practice, the Boss Matches row clears it.
br = (root.parent / 'Nioh3BossRematch/src/rematch.cpp').read_text()
act = br[br.index('bool ActBossRoute(int item) {'):br.index('int ArenaTrainRoute(int row) {')]
assert 'const bool tele = item >= kItTeleport;' in act
assert 'if (arena) SetPracticeNext(tele ? 1 : 0);' in act
assert act.index('SetPracticeNext(tele ? 1 : 0)') < act.index('Post(arena ? kReqFight : kReqShrine')
page = br[br.index('if (g_uiPages.page == uTraining) {'):]
page = page[:page.index('if (g_uiPages.page == uSummon)')]
assert 'rows.push_back(kItTeleport + (int)i);' in page  # Training > Choose an arena lists kItTeleport rows

preamble = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define N3T_API
typedef struct { uint32_t mission; char id[16]; } ArenaRow;
static char lastLog[256];
#define Log(...) snprintf(lastLog, sizeof lastLog, __VA_ARGS__)
static int g_cfg_enable = 1, g_practice_next, g_arena_only_next, g_native_fallback, g_native_roster;
static uint32_t g_practice_arena_mission;
static ArenaRow* g_pick;
static ArenaRow rowR = {0xE18C, "RE18C"}, rowD = {0x2EE9, "D2EE9"}, rowS = {0x7777, "S210803"};
static ArenaRow* FindRow(uint32_t m) { return m == rowR.mission ? &rowR : m == rowD.mission ? &rowD : m == rowS.mission ? &rowS : 0; }
static int entries, summons, summonOk = 1, practiceAtSummon, arenas, arenaOk = 1, practiceAtArena;
static int NativeFirst(const ArenaRow* r);
// the production FightNowEntry: native-first -> 0 (Boss Rematch issues the real mission load); otherwise the arena swap / world switch
static int FightNowEntry(uint32_t m) { if (NativeFirst(FindRow(m))) { ++entries; return 0; } ++arenas; practiceAtArena = g_practice_next; return arenaOk; }
static int Nioh3Training_Summon(uint32_t m) { (void)m; ++summons; practiceAtSummon = g_practice_next; if (summonOk) g_practice_next = 0; return summonOk; }
'''
main = r'''
static void Reset(void) { entries = summons = practiceAtSummon = arenas = practiceAtArena = 0; summonOk = arenaOk = 1; g_practice_next = g_arena_only_next = g_native_fallback = g_native_roster = 0; g_practice_arena_mission = 0; g_cfg_enable = 1; lastLog[0] = 0; }
int main(void) {
    ArenaRow* rows[2] = {&rowR, &rowD};
    for (int i = 0; i < 2; ++i) {
        uint32_t m = rows[i]->mission;
        Reset(); assert(NativeFirst(rows[i]));
        g_practice_next = 1;  // Training page pick: the boss's arena, Practice kept, no native-first, no in-place summon
        assert(Nioh3Training_FightNow(m) == 1);
        assert(arenas == 1 && entries == 0 && summons == 0 && practiceAtArena == 1);
        assert(strstr(lastLog, "practice in the boss arena"));
        assert(!NativeFirst(rows[i]) && NativeFirst(rows[1 - i]));  // the async swap / switch arrival keeps the arena route for this mission only
        Reset(); g_practice_next = 1; arenaOk = 0;  // arena route refused: the boss comes to the player instead (still Practice, never native)
        assert(Nioh3Training_FightNow(m) == 1 && arenas == 1 && summons == 1 && entries == 0 && practiceAtSummon == 1 && NativeFirst(rows[i]));
        Reset(); g_practice_next = 1; arenaOk = 0; summonOk = 0;
        assert(Nioh3Training_FightNow(m) == 0 && summons == 1 && entries == 0 && g_practice_next == 0);  // refused: the flag does not leak
        Reset(); g_practice_next = 1; Nioh3Training_FightNow(m);
        g_practice_next = 0; assert(Nioh3Training_FightNow(m) == 0 && entries == 1);  // a following Boss Matches pick is native-first again
        Reset();  // Boss Matches pick
        assert(Nioh3Training_FightNow(m) == 0 && entries == 1 && summons == 0 && arenas == 0);
        Reset(); g_arena_only_next = 1;  // Train in this arena keeps its own native trip
        Nioh3Training_FightNow(m); assert(entries == 1 && summons == 0 && arenas == 0);
    }
    Reset(); g_practice_next = 1;  // S row: not a mission row, legacy entry consumes Practice itself
    Nioh3Training_FightNow(rowS.mission); assert(arenas == 1 && entries == 0 && summons == 0);
    Reset(); g_practice_next = 1; g_cfg_enable = 0;  // Training disabled: no Training route at all
    Nioh3Training_FightNow(rowR.mission); assert(arenas == 1 && summons == 0 && entries == 0);
    puts("PASS training-pick-route: Training page pick -> boss arena Practice route (fallback in-place summon), never native; Boss Matches / arena-only / S rows unchanged; refused pick clears Practice");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d); (p / 'pick.c').write_text(preamble + native + fightnow + main)
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-O0', '-UNDEBUG', str(p / 'pick.c'), '-o', str(p / 'pick.exe')], check=True)
    subprocess.run([str(p / 'pick.exe')], check=True)
# TrainAttachArena clears the arena-route mark once the Practice arena session is attached
att = s[s.index('static void TrainAttachArena('):]; att = att[:att.index('\n}')]
assert 'g_practice_arena_mission = 0;' in att and att.index('g_tr.practice = g_practice_next') < att.index('g_practice_arena_mission = 0;')
