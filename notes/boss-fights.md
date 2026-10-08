# Boss fights: how they are defined, started, recorded (research only, 2026-10-08)

Static analysis only (strings.tsv, `D:\nioh3-re\decomp_ro.cmd`, extracted LFM scripts). Game not run, mod not touched. Names: `re\symbols\incoming\boss-renames.tsv`. All RVAs are for v2.02 (image base 0x7FF74F410000). Confidence is medium: the decompiles are clear, the data-side mapping (flag ids, boss placement ids, save field) is not yet traced.

## 1. There is no "boss table"; a boss fight is a mission script
- Mission logic is **bytecode scripts** (coroutine VM, `umud::yield`, header date string `2011-04-29T14:26:48+0900;`) stored in the LFM layer: `data\extract\lfm\archive_09\XX\<62hex>`. Each script carries the C-API names it calls as strings plus a per-name hash (these differ from `kidshash.py`; not yet matched).
- **110 scripts in archive_09 reference `Mission::BossBattleMode`**: the story/sub-mission scripts (stage families S01_SEN_*, S02_HEI_*, S03_KOD_*, S04_BAK_*, S00_EDO_*/`LAST_*`, DLC `S90_DLC1_*`; names in `data\stage_names.tsv`). A boss name list was not produced: boss identity is a placement ID (`Duel::GetBossPlacementID`, `WaveBattle::GetBossEnemyIDInNowWave`) resolved from stage placement data, not a string. Boss-door gadgets seen by name: `set_bak_hnj_BOSS_PALLT_GADGET_A`, `set_sen_htmt_csl_boss_door_01_PALLT_GADGET_A`.
- LooseFileLoader does **not** cover LFM, so scripts cannot be tweaked that way (notes\game-data.md section 2).
- Boss-related script APIs (name table at 0x3BFxxxx, `{name, fn}` pairs; wrappers `Game::MissionScriptFunction::*` at 0x3ABxxxx): `Mission::BossBattleMode/…End/…EndWithoutSpecificBoss`, `Duel::IsAliveBoss`, `Duel::GetBossPlacementID`, `UI::ShowBossGauge/HideBossGauge`, `Sound::PlayBossBGM/ActivateBossBattleBGMWithID/StopBossBGM`, `Mugen::IsAllMugenBossesDead`, `Mission::SetPlayHeroDeadBossVoice`, `Character::IsDeathGroupEnemy`, `Character::EnemyForceKill*`, `Character::ResetReceivedDead`, `Mission::SetMissionClearFlag`, `Mission::IsMissionFlag/SetMissionFlag`, `EnemyArea::*`, `Gadget::SetReplacementStatus*`.

## 2. Key functions (script-binding entry -> impl)
| Script API | RVA | What it does |
|---|---|---|
| Mission::BossBattleMode | 0x23A2DCC | sets `[charMgr]+0x26AA=1`, looks up the actor, clears two flag bytes `[charMgr]+0x3036/37` unless special id 0x4ACDF, calls HUD/boss-mode helpers (1C19E14, 1C20600) |
| Mission::BossBattleModeEnd | 0x23A2F70 | undoes it (1C0D068, 1C0CFB4, 1C0CEEC, 1C0E83C, 9DECF8) |
| UI::ShowBossGauge(actorId) | 0x23B8678 -> 0x1D115D4 -> 0x1D05824 | HUD object `[0x45C84A8]+0xA8`; fills up to 3 gauge rows of 0x60 B. Layout `00_boss_gauge_hud`, `MENU_ID_BOSS_GAUGE_HUD` (string 0x3AB1EA8) |
| Duel::IsAliveBoss | 0x2399AD8 | actor lookup then `0x7E11F0(actor)==0` |
| Character::Create(placementId) | 0xA1D204 -> **0x1180740** | bosses/enemies are **pre-placed actors**; this finds the placement record (flag bit0) and the actor (`ActorFindById` 0x135B78 on charMgr `[0x45B9E40]`), sets `actor+0xDA=1` (activate), `+0xEE=0`, runs spawn init (20A05C, 2869A0) |
| Mission::SetMissionFlag / IsMissionFlag | 0x23E0398 -> **0x70275C** / 0xA36384 -> **0x284944** | mission flag byte array |
| Mission::SetMissionClearFlag / IsMissionCleared | 0x23A582C -> 0x1C1DB88 / 0xA30F58 -> **0x37AFC0** | mission clear state |
| Character::ResetReceivedDead | 0x1180940 | script-side revive hook, rematch candidate |

## 3. How a fight runs (inferred from script API order)
Player enters a PlacementBox (`Player::IsEnterPlacementBox*`) or fog/boss-door gadget triggers -> script: `Character::Create(bossPlacement)` -> `Mission::BossBattleMode` (BGM, HUD switch) -> `UI::ShowBossGauge(actor)` -> loop on `Duel::IsAliveBoss` / `Character::GetHpRate` with `umud::yield` -> on death `BossBattleModeEnd`, `UI::HideBossGauge`, `Mission::SetMissionFlag(x,true)`, finally `Mission::SetMissionClearFlag` / `IsAppearClearGadget`.

## 4. Where "defeated" is recorded
- **Mission flags**: `g_MissionMgr = [0x47514D0]`. Bytes at `[[mgr+8] + 0x130 + slot*0x400 + flagIdx]`. `slot` (0..5) from the NG/difficulty fields (`[mgr+8]+0x24`, `+0x10`); `flagIdx` from `0x284A5C(flagDb,id)` with `flagDb = [[0x45B9E30]+0x18]`. Value 0/1; 0xFF = use definition default (`def+0xC`). Setter `MissionFlagSet` (0x70275C) also fires change callbacks (702BBC/702974) and a "newly true" notice (1071120), and skips when the flag def is locked (`def+0xE`, `+0x14`; online/guest rules).
- **Mission cleared**: hash map in `g_ClearDb = [0x4751848]`: FNV-1a (basis 0xCBF29CE484222325) over the 4-byte mission id, table `[db+8]+0x2E0`, entry byte `[entry+0x38+slot]`, slot = `0x285448()`. Getter 0x37AFC0.
- **Persistence**: user save is a tagged stream `{u32 hash, u32 size, data}` (`SaveStreamWriteU32` 0x6BC190); file `SAVEDATA.BIN` / `SAVEDATA%02u` (functions 0x5B71E0, 0x5B6510). The mission flag block is a field in that stream; its hash/offset is **not yet traced** (next: xref the array base `[mgr+8]+0x130` in the save writer). A save copy is in `D:\nioh3-re\save-backups\*\Savedata` for offline format work.
- The boss actor's own death state (alive via 0x7E11F0, received-dead bit) lives in the actor and is rebuilt from placement on stage load, so it is not a save flag. `Online::MissionSyncEmakiUniqueEnemyDefeatFlag` shows unique-enemy defeat flags exist for Emaki fights (sync only).

## 5. Rematch options (offline/solo only; no DRM, no online, no on-disk Nioh3.exe edits)
Gate every option on solo/offline (no co-op session, `Online::*` idle).

**A. ASI feature "re-arm boss" (recommended, medium risk).** Hotkey/menu in a loaded boss stage: (1) clear the boss flags through the game's own `MissionFlagSet(g_MissionMgr, flagId, 0, 0)` (0x70275C keeps notifications consistent); (2) `ResetReceivedDead` (0x1180940) on the boss actor and `PlacementEnemyActivate` (0x1180740, the `Character::Create` path) with the boss placement id; clear the arena script's progress flag so it restarts. Risks: need flag ids and placement id per fight (log them live first by hooking `MissionFlagSet`/`Character::Create` during one real fight); the script state machine may not restart mid-mission; corpse/animation state after reset. Do a log-only phase first.

**B. Save-data tweak (low risk, manual, offline).** Trace the mission-flag field in `SAVEDATA.BIN`, write an offline tool that clears chosen boss flags and the mission-cleared byte in a backed-up copy. The normal game flow then replays the boss. Risks: save checksum/signature (check the writer near 0x5B71E0 first), later quests depending on the flag, Steam cloud overwrite. No game process involved, so best first deliverable; it also yields the exact flag ids for A.

**C. Read-hook memory patch (higher risk).** Hook `MissionFlagGet` (0x284944) / `MissionClearedGet` (0x37AFC0) to return false for a configured id list while in the arena. MissionFlagGet has 297 callers, so a wrong id breaks unrelated logic (doors, shops); restrict by stage and revert on leave. Needs a stage reload so the script re-evaluates `IsMissionFlag`.

Not feasible: editing scripts (LFM not covered by LooseFileLoader, bytecode format unknown) or creating bosses from nothing (actors are stage placements).

## 6. Open items
1. Match script-name hashes to a VM resolver (wrappers near 0x3ABxxxx).
2. Enumerate boss placement ids from per-stage placement kidsobjdb (`data\extract\stage`), cross-checking `Duel::GetBossPlacementID` users.
3. Find the mission-flag field and any checksum in the save stream.
