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

## 7. Boss list from the mission scripts (2026-10-08, static, game not run)
Table: `notes\boss-fights-table.csv` (104 rows; one per script in `data\extract\lfm\archive_09` that actually calls `Mission::BossBattleMode`; the other 6 of the 110 only import the name). Generated by throwaway scripts (not committed); renames in `re\symbols\incoming\boss2-renames.tsv`.

**Script file format (decoded and validated: all 110 decode linearly to EOF with no desync).**
- `u8 len + date string`, `u16 count` (=507 here), then name table `{u16 len, u32 hash, name\0}`; then `u16 n` and n x `{u16 nameIdx, u16 id, u16 kind=1}` (native/user symbol table; a call operand is the index into this table). Then waitable-id list, a UTF-8 string pool (debug text, stage names such as `S02_HEI_01_KAMOSHI`, user function names), then the code. The code start is the first even offset after the pool from which the stream decodes cleanly to EOF.
- Code = u16 opcodes (dispatcher `ScriptVmRunOpcodes` 0x13A830), operand bytes by opcode: 0x1b call native u16 sym; 0x1f call script function u16 sym (this is also how user functions and the `Time::Wait*`/`Function::*` library wrappers are called); 0x29 push int32; 0x3e/0x3d load/store var `{u16 kind,u16 idx}`; 0x22/0x23/0x24/0x30/0x31/0x5 rel32 jumps; 0x35 u16 n + n*8 table; 0x3b u16 sym (function/yield entry marker, names the function that follows); 0x2c/0x2e u16 n + n*4 inline words; 0x11 u32; 0x01/0x2f/0x50/0x51 u16; all others no operand. Values on the VM stack are 8 bytes {u32, u16 type}.
- Args are pushed before the call; constant args show up as `29 imm32`, parameters as `3e 01 00 idx`.

**What the table contains and how far to trust it.**
- `stages`: stage ids found in the script's string pool (stage family S01_SEN / S02_HEI / S03_KOD / S04_BAK / S90_DLC1 / H0x hell). Empty for scripts that do not carry one; `mission_tags` = `kitan_NN` style ids in user function names.
- `boss_battle_mode_calls` etc. are call counts. `Mission::BossBattleMode` is mostly called inside the shared library function `Function::DuelCommonFlowAnyRange` (89 scripts), so the boss itself is NOT a constant at the BossBattleMode call. 96 of 104 scripts also call `Function::CreateClearGadgetBossPosition`, 98 call `Function::EndMissionClearState`: these two are the practical "this is a boss mission" markers.
- `create_ids_*`: constant placement ids passed to `Character::Create*` from script-specific functions (e.g. 1401-1407 group ids, 142122 NPC, big 8-digit values for hell/wave enemies). `get_boss_placement_args`: constant args of `Duel::GetBossPlacementID` from script-specific functions (23 scripts; elsewhere the arg is a local, set by the caller). **A single "boss actor id" per mission is not recoverable from the script constants alone**: the shared flow takes it from VM locals/globals filled by earlier calls, i.e. from data. Needs the live log (Section 8, step 1).
- `flags_*`: constants passed to `Mission::SetMissionFlag` from script-specific functions only (library functions excluded: flags 392-396 etc. belong to the shared `MoveToFixedPosition` helper, not to a boss). `set_clear_flag_calls` counts `SetMissionClearFlag` (the clear db, no id arg: it clears the running mission). So the per-boss "defeated" state is usually the mission-clear entry (hash map in g_ClearDb, section 4) plus the story flags listed here.
- `name_tokens`: romaji tokens from user function names that mention boss/battle/duel/defeat (e.g. toragozen, maria, kumagai, mitsukuni). No text-table or KIDS-hash resolution of a display name was achievable: the ids are placement ids, not hashes of names, and `kidshash.py` does not apply to the script hashes. 58 of 104 rows have a token.

## 8. Save side (read-only, copy of `D:\nioh3-re\save-backups\20261007-2346` and `20261008-0407`, never the live save)
- `SAVEDATA.BIN` is 9,437,616 B (min 0x900198, as in `notes\amrita-tools.md` section 2). Entropy is flat from byte 0. `BACKUP.BIN` is a byte-identical copy of `SAVEDATA.BIN` in both snapshots, so an edit must update both.
- Two saves taken hours apart differ in 589,838 of 589,851 16-byte blocks: every save re-randomises the whole ciphertext (chained mode or per-save IV/key from the header). A diff of encrypted saves cannot reveal which bytes changed; the flag block can only be located after decryption.
- Protection: (1) 344-byte XOR header, (2) block cipher over the body (AES per `amrita-tools.md`; key not extracted by us), (3) u32 checksum at 0x900194 over the decrypted buffer (algorithm recorded in `amrita-tools.md`, easy to recompute). So a clean offline edit = decrypt, edit, recompute checksum, re-encrypt. The checksum is not the obstacle; the cipher key is.
- Mission flag block: the body is the tagged stream `{u32 hash, u32 size, data}`; 22 known fields (`re\symbols\amrita\save_field_hashes.tsv`) are gear/stats, none is the mission state. The runtime mission flag array (bytes at `[[g_MissionMgr+8]+0x130+slot*0x400]`) and clear-db are not among them, and the serializer was not found: no SaveStream writer references the mission manager among the cached callers (the 297 callers of 0x6bc190 are tiny per-type field writers called from 0x105ae20-class section writers). Not located; hash and offset unknown.
- Fastest way to find it without breaking the cipher question: hook-free offline route is to decrypt a save with a public tool (e.g. the Rust savecore WASM of amrita.tools run in Node), list record hashes/sizes, and look for a record of 0x1800 B (6 slots x 0x400) or 0x400 B; confirm by comparing two saves before/after one boss kill. Not done here (needs the key path to be accepted first).

## 9. Rematch options, refined (offline/solo only)
1. **A. Runtime re-arm (recommended).** No cipher, no checksum, no save edit. Step 1 is log-only: hook `MissionFlagSet` (0x70275C), `SetMissionClearFlag` (0x1C1DB88 path) and `Character::Create` (0x1180740) during one real boss kill to capture the exact flag ids, mission id and placement id per fight (the static table cannot give the shared-flow boss id, section 7). Step 2: on a hotkey in a loaded boss stage, clear that fight's story flags via `MissionFlagSet(mgr,id,0,0)`, clear the clear-db byte for the mission slot, `ResetReceivedDead` (0x1180940) + `PlacementEnemyActivate` (0x1180740) with the captured placement id, and re-run the arena. Because 96/104 boss scripts use the same `DuelCommonFlowAnyRange` + `EndMissionClearState` + `CreateClearGadgetBossPosition` skeleton, one generic re-arm path (re-enter the duel flow, delete the clear gadget, clear mission-clear state) should cover most bosses; per-boss data is only the flag/placement ids from the log.
2. **B. Save edit.** Now clearly costlier than assumed: needs the AES key and cipher mode (not extracted), the mission-state record (not located), checksum recompute (known) and double write of BACKUP.BIN. Best only as a later convenience if the key is obtained from a public tool.
3. **C. Read-hook (MissionFlagGet 0x284944 / MissionClearedGet 0x37AFC0).** Unchanged: 297 callers on MissionFlagGet, high collateral risk; only viable gated per stage. Not preferred.
**Recommendation: A**, log-only phase first, solo/offline gate (no `Online::*` activity, no co-op session). No DRM or online code is touched by any option.
