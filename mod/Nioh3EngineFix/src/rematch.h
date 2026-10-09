// rematch.h - Boss rematch, Phase 1 MVP (route R-a), [Rematch] Enable = 0 by default. notes\boss-rematch-design.md s.2/s.3/s.5.5,
// build state in notes\boss-rematch-build.md.
//
// What it does (solo/offline only, v2.02 only, every site byte-verified, patches.json group RM + BL1/BL2):
//   * game-thread pump: an entry hook on MissionLoadStep 0x223AF50 (called every frame by the frame loop). Every game call and
//     every memory write of this module runs there, never on the Present thread. If BossLog/RematchLog already hooked the entry
//     (E9 + NOPs from bosslog.cpp), the hook CHAINS: our thunk runs first and then jumps into bosslog's thunk (one site, two
//     loggers; bosslog.cpp is unchanged). Same for BossBattleMode 23A2DCC / BossBattleModeEnd 23A2F70 (the "in fight" gate).
//   * gates (s.2): offline, not a guest, not loading / paused, a player present (a save is loaded), no mission-start queue slot
//     busy, not in an Emaki mission, not in a boss fight, no game menu open, nothing of ours active. Re-checked on the game
//     thread right before the call.
//   * R-a: MenuOpenMissionReplay(0, 0, 6) 0x1DE28E8 (exactly what the script native UI::OpenMissionReplayMenu 0x23F90CC does)
//     after the same "already open" vcall (+0x48) that BattleScrollTop_Select makes. The player picks the mission in the
//     official list; the game's own replay runs. Nothing is patched.
//   * cleared-mission scan: MissionClearedGetSlot 0x702B64 (db = [0x4751848], = Mission::IsMissionAlreadyClear, the getter the
//     replay list builder 0x378674 uses) per catalogue row, on the game thread, when the sub-page asks for it.
//   * [Rematch] HiddenDuels = 1: clears bit 0x4000 of mission record +0x40 for 0xD22D / 0xB3F8 / 0xFB96 (the bit the list
//     builder 378B83 skips them for; the design note said 0x40; game data, never saved)
//     right before R-a, restores the snapshot when the replay menu closes without a start or the started mission exits.
//   * R-b ([Rematch] DirectStart = 1): code path present, always REFUSED in this build (needs the L7 capture of a menu-started
//     replay: Q10). No story-flag writes (UnlockEarly is not in this build).
// Catalogue: Nioh3EngineFix_bosses.tsv next to the ini (tools\rematch_catalog.py from notes\battle-scrolls.csv).
#pragma once
#include <cstddef>
#include <cstdint>

namespace rematch {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Rematch]
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // v2.02 only
    int (*pauseReason)() = nullptr;    // menu::GamePauseReason (1 = a game menu is open)
};

// dllmain worker, AFTER bosslog::Start (so a BossLog hook on the same entry is chained, not refused). Reads [Rematch], loads the
// catalogue, verifies every site and installs the three hooks. Returns true when the feature is live.
bool Start(const StartInfo& info);

// ---- mod menu (Present thread; only reads snapshots / sets atomics) ----
bool Installed();                                         // row visible ([Rematch] Enable = 1 and live)
void RowValue(char* buf, size_t n, uint32_t& color);     // main-list value column: "ready" or why not
bool PageActive();
void PageOpen();                                          // A on the row: sub-page + a scan request
void PageClose();
void PageMove(int dir);
bool PageAct(int dir);                                    // A (dir 0) on a sub-page row; true = a request was queued -> close the menu
void PageBack();                                          // B: leave the confirm line, then the sub-page
struct PageRow {
    char label[96];
    char tech[24];
    char value[48];
    uint32_t lc, vc;
    bool sel;
};
int PageFill(PageRow* rows, int cap);                     // rows of the sub-page (section header first)
void PageHelp(char* what, size_t nw, char* effect, size_t ne);
bool Toast(char* buf, size_t n, uint32_t& color);         // OSD line while the menu is closed (queued / started / refused)
// vpad remote ('menu get|set Rematch v'): get = state code (0 idle, 1 replay menu open, 2 in replay); set 1 = R-a (open the
// replay menu), set 2 = abort and restore now
int StateCode();
void RemoteRequest(int value);

}  // namespace rematch
