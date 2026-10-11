# Boss/duel strings and the script-binding table in Nioh3.exe (static, 2026-10-08)

Source: `re\Nioh3.dump.exe` (file offset == RVA, image base 0x7FF74F410000, v2.02). Game not run, no mod code.

## Result
- **Script native-binding table: RVA 0x397E770..0x3982A70, 1072 entries x 16 B `{char* name, fn* wrapper}`** (both absolute VAs). Names are plain ASCII `Namespace::Function` strings in .rdata (e.g. 0x3BFE728 = `Mission::BossBattleMode`, entry at 0x3981020). The full list (binding, wrapper RVA, name RVA, namespace) is a CSV in the private dev repo; the wrappers are in symbols.tsv as `ScriptBind_*`. Args are not decoded here (arg counts/types live in each wrapper's body); see boss-fights.md section 7 for call conventions (args pushed before opcode 0x1b).
- Wrapper RVAs: 937 are thin wrappers in 0x2380000-0x23C0000 (`Game::MissionScriptFunction::*`); 135 point straight at engine impls (e.g. Character::Create -> 0xA1D204, Mission::IsMissionFlag -> 0xA36384, Hell::IsInsideHell -> 0x3093B8).
- Namespaces: Character 247, UI 115, Player 107, Mission 94, Gadget 62, Sound 53, Stage 47, FollowPlayer 35, Online 33, Item 32, Graphics 26, RTM 24, Tutorial 18, Load 17, Camera 17, Hell 17, System 14, Coop 11, WaveBattle 11, Effect 10, World 9, Quest 8, WorldDateTime 8, Debug 7, TalkEvent 6, Emaki 6, Value 6, Time 5, ReflectionProbe 5, Script 4, PathFinder 3, Movie 3, Collision/Duel/EnemyArea/Memory/LeadModeMV 2, Pause/Random 1.
- There is **no boss-selection or "spawn boss by id" function by name**. Bosses are pre-placed actors; the relevant calls are `Character::Create/CreateDirect/CreateDynamic/CreateGroupEnemy`, `Character::ResetReceivedDead` (0x1180940), `Character::SetRespawnInfo` (0x2397CD0), `Character::WarpFirstPoint(All)`, `Gadget::SetReplacementStatus*`, `Coop::GetPlacementEnemyID`.

## Boss/duel/arena-related bindings (wrapper RVA)
- Duel: `Duel::IsAliveBoss` 0x2399AD8, `Duel::GetBossPlacementID` 0x2399A48.
- Mission: `BossBattleMode` 0x23A2DCC, `BossBattleModeEnd` 0x23A2F70, `BossBattleModeEndWithoutSpecificBoss` 0x23A3018, `SetPlayHeroDeadBossVoice` 0x23A5B54, `SetProhibitTokoyoCoopGaugeBossBattleRecovery` 0x23A5BDC, `StartNoWarpMode` 0x23A5F7C, `EndNoWarpMode` 0x23A3FB4, `GatherWarp` 0x23A41BC, `IsAppearClearGadget` 0x23A481C.
- WaveBattle (arena/wave fights): `RequestID` 0x23B91F8, `RequestIDWithWhoWarpSetting` 0x23B93BC, `IsClearID` 0x23B9168, `IsActiveID` 0x90EA34, `GetActiveWaveNo` 0x23B8FA8, `GetActiveMaxWaveNo` 0x23B8F30, `GetBossEnemyIDInNowWave` 0x23B901C, `SetPauseFlag`, `StopProgressFlag`, `Cancel` 0x23B8ED0, `GetLastDefeatedEnemyID`.
- Emaki (battle-scroll equivalent): `GetStageID` 0x239A368, `GetHellID` 0x239A2C8, `GetWarpPos`, `GetWaveBattleStartBoxID`, `RequestWaveBattle` 0x239A630, `IsClearWaveBattle`, plus (names only in the string pool, wrappers `Game::MissionScriptFunction::Emaki_*`) `IsActiveWaveBattle`, `IsEmakiMission`, `IsEnterWaveBattleStartBox`, `IsThisGadgetEmakiSmallHell`.
- Hell (rift/Underworld): `EnterHell` 0x239FA28, `LeaveHell` 0x239FDD4, `RequestHellStageSetup` 0x239FF58, `IsEndHellStageSetup`, `IsClearHell` 0x982998, `Request/IsHellClearMoment`, `Request/IsHellAppearMoment`, `StatusUpReward`, `Set/Reset/IsClearMapOnlySmallHell`, `ResetSettingHellEntrance`.
- Others: `Player::SetRetryPointIDAndSave` 0x23AB7C4, `Player::GetRetryPointID` 0xA2A60C, `Player::IsEnterPlacementBox*` (fog/boss-door trigger boxes), `Mugen::*` (see csv), `UI::ShowBossGauge` 0x23B8678 / `HideBossGauge` 0x23B36A8, `Sound::PlayBossBGM` 0x23AD8A4, `ActivateBossBattleBGMWithID` 0x23AD224, `StopBossBGM` 0x23AF10C, `Coop::ResetToBeforeCoop*`.

## Other boss-ish strings (outside the table)
- UI menu ids (.rdata 0x3AB1EA8-0x3AB2AD0): `MENU_ID_BOSS_GAUGE_HUD`, `MENU_ID_EMAKI_GAUGE_HUD`, `MENU_ID_MISSION_REPLAY` (0x3AB22F0, mission replay UI: worth a look for rematch), `MENU_ID_EMAKI_SELECT`, `_EMAKI_MISSION_RESULT`, `_EMAKI_INFO_0/1`, `_EMAKI_TOP`, `_EMAKI_SHAFT_SHIFT`.
- Hell cinematic ids: `HELL_START_01_PRERENDER`, `HELL_CLEAR_01..08_PRERENDER`, `HELL_CLEAR_01(_AFTER)`, `90_S_MIKATA_HELL`; text tags `^05~IN_HELL~`, `^05~HELL_WEAPON~`, `^05~HELL_ARTS~`, `F_CTG_EMAKI`.
- No ASCII/UTF-16 strings at all for rematch, rift, fog gate, arena, encounter, battle scroll (only unrelated "Encountered DLSSG State Error"). No debug/log format strings mention bosses; no RTTI class names match boss/duel/wave/emaki/mission (type registry names are not stored as readable RTTI strings). Boss identity is data (placement ids) plus mission-script bytecode, consistent with boss-fights.md.

Names: the thin wrappers are merged into symbols.tsv as `ScriptBind_Ns_Func` (excluding the 135 direct-impl targets and RVAs that already had names).
