// bosslog.h - BossLog: read-only boss fight logger ([Debug] BossLog = 0 by default). Log only, never changes game
// behaviour. Background and the research it feeds (rematch step 1): notes\boss-fights.md sections 3 and 9.
// Hooks (v2.02 only; function entries, first whole instructions relocated into a thunk that calls the logger with the
// original argument registers, then runs them and jumps back; every entry's bytes are verified first):
//   Mission::BossBattleMode 23A2DCC / BossBattleModeEnd 23A2F70  fight window (#n, duration)
//   Character::Create impl PlacementEnemyActivate 1180740        placement id (ring of the last 16 is dumped at fight start)
//   MissionFlagSet 70275C                                        flag id + value, inside the window and 20 s after it
//   UI::ShowBossGauge 23B8678                                    boss actor id of the HP bar
//   MissionClear impl 1C1DB88                                    mission id of SetMissionClearFlag
// 1.7.20 rewards (BL7+): script-native wrappers for drop tables, items/equipment, guardian/skill/money grants, SetReward and the
//   game-clear reward, plus GameClearRewardGrant 2280FA0; logged as raw VM args (ids) during the fight and 30 s after it.
// 1.7.21 [Debug] RematchLog = 1: Phase 0 of notes/boss-rematch-design.md (rematch hooks RM1..RM29, patches.json group RM), also installs the
//   BossLog hooks above. Read only. Off (all 0): nothing is installed.
// [Debug] InvLog = 1 (rematch2 branch): InventoryAddItem 54D324 (BL31), every inventory add of the session (chests, drops, pickups,
//   rewards, script grants) with the caller, its function start and earlier return addresses; independent of BossLog. Read only.
#pragma once
#include <cstdint>

namespace bosslog {

struct StartInfo {
    void (*log)(const char* fmt, ...) = nullptr;
    const wchar_t* iniPath = nullptr;  // [Debug] BossLog / RematchLog / InvLog
    uintptr_t base = 0;                // Nioh3.exe image base
    bool knownBuild = false;           // v2.02 only
};

// Reads [Debug] BossLog. When != 0 and the build is known: verifies and installs the hooks. Returns true if any hook is on.
bool Start(const StartInfo& info);

}  // namespace bosslog
