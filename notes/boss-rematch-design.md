# Boss Rematch: mod-menu feature design (2026-10-08, design only)

Design only: no mod source edited, game not run. Static evidence: v2.02, RVAs relative to the image base. Inputs: `boss-rematch-paths.md` (paths), `boss-fights.md` s.4/s.9 (where "defeated" lives), `boss-rift-ngplus.md` (slots), `boss-arenas.md`, `boss-duel-flow.md`, `boss-rewards.md`, `boss-achievements.md`, `frame-timing.md` s.10 (online state), `src\menu.cpp` + `src\bosslog.cpp`. New decompiles for this note: 0x10DBED0, 0x1C2794C, 0x1C1425C, 0x1C14FD0, 0x37AFC0, 0x284944, 0xF1F060, callers of 0x223AF50.

**Decision (revised with `battle-scrolls.md`).** In-game, "Battle Scroll" is shrine menu item 3. It has two halves: **mission replay** over the 130-record mission table (62 rows listable, which covers every main boss), and **Picture Scroll / Emaki** (the single mission 0xCC96). So the build is ordered like this:
- **MVP, route R (official replay, no data patch).** The beaten-boss list maps each boss to its mission id (`battle-scrolls.csv`). Starting a boss replays that mission through the game's own replay path (s.5.5). Two optional memory helpers, both reverted: **un-hide** the 3 data-hidden duels by clearing bit 0x40 of mission record `+0x40` (0xD22D Honda Tadakatsu, 0xB3F8 Yoshitsune, 0xFB96 Jubei/Chuya), and an **early unlock** that sets mission flag 0xAB (off by default; it is a saved story flag, see s.6).
- **Route E (Emaki boss-id override)** covers bosses that have no mission record: open-field, Crucible/small-hell and secret bosses. E0 uses an existing Emaki record unchanged (s.5.2). E1 points one Emaki record at the wanted placement in memory for one fight, then restores it (s.5.3).
- **Route Q** is the raw mission-start queue, debug only.

Every game call runs on the game thread from a new hook at `MissionLoadStep`, never on the Present thread. Solo and offline only. Off by default (`[Rematch] Enable = 0`). Phase 0 is log-only and must answer the open questions in s.8 before any write ships. Tier names used below: Tier 0 = E0, Tier 1 = E1, Tier 2 = R, Tier 3 = Q.

## 1. Corrections to boss-rematch-paths.md (from the new decompiles)
- **Build corrections (ASI 1.7.40, `boss-rematch-build.md` s.2)**: the 3 hidden duels are excluded by record `+0x40` bit **0x4000** (flags 0x24180; the list builder 378B83 tests 1 | 0x40 | 0x4000), not 0x40 as s.5.5 says; a consumed queue slot keeps its bytes (slot 0 stays `{1, .., E0}` after the save load), so "no slot busy" (s.2 gate 3) never holds: the build uses "slot bytes changed in the last 10 s"; `MissionLoadStep` does not tick at the title; mission-state `+0x14` is a stage id, not the record id.
- **EmakiKey is 12 bytes**, not `{u32, u8@4, u8@7}`. The item-use handler 0xF1F060 builds: `+0 u32` = item+0x20, `+4 u16` = item+0x12, `+6 u8` = item+0x0F, `+7 u8 kind` (item id 0x1E82->1, 0x516D->2, 0xDD82->4, 0xD523->5, else 3, on path 0; on path 1 it is `0x20E544C(itemId)`), `+8 u8` use mode (1 on path 0, 0 on path 1), `+9..+B` = 0. The request copies all 12 bytes into the queue. Byte `+8` (not `+4`) goes to `[[0x45B9E40]]+0x2BF4` unless `+0x2C18` is set.
- **ref (arg 1) is the scroll item's `+0x28` u64**, not a `-1` sentinel. The meaning is unknown, so mimic the item path.
- **StartParams is 0x18 bytes**: `{u32 0x101, u32 0x1000000, u32 0x10100, u32 0, u64 0}`. Queue slot `+0x40..+0x57` holds them.
- **The Emaki record is re-resolved at run time from the current emaki byte.** `EmakiRecordGet` 0x1C1425C (12 callers: placement build 0x1C1E470, 0x3B35B4, the Emaki script natives 0x239A2C8..0x239A630, 0x23CB9CC/0x23CBAD0, 0x1CA7EE8, UI 0x1F131F4) calls 0x1C14FD0 every time. So **a patched record must stay patched for the whole mission** (load, fight, retry, result screen). It can only be restored after the mission has ended.
- From `battle-scrolls.md`: the replay menu setup is **0x1DC8424** (not 0x1DD8424), and 0x1DAFE64 is `BattleScrollTop_Select` (option 0 replay, option 1 Emaki), reached from `ShrineTopMenu_Update` 0x1E1E258 item 3. Without `FeatureUnlocked(8)` + flag 0x217, item 3 opens the replay menu directly. Mission record `+0x40` bit 0x1000000 ("Emaki-capable") is the same bit that `MissionSetupEmaki` reads as record byte `+0x43` bit0.
- `EmakiMissionRequest` does **not** check inventory or online state before it enqueues. It records the online mode in the slot (`+0x38`) when `[0x4709B50]+0x34` is 1 or 2. When all 3 slots are busy it returns without enqueueing, so success = "a slot now has `+4 == 0xCC96`".

## 2. Scope and gates (disarm rules)
The rematch row and the start action are **available only when all of these hold**. Each gate is re-checked on the game thread just before the call:
1. The build is v2.02 (`g_host.knownBuild`), every hook site and call target is byte-verified (patches.json group `RM`), and `[Rematch] Enable = 1`.
2. **Offline**: net session `[0x4709B50]` is null, or its `+0x34` is not 1 or 2 (the same test as `IsOnlinePlay` 0x85834). Not a guest: `[0x45C84F8]` is null or `+0x1DC == 0`. All reads go through SEH (`Rd`).
3. Not loading: stop flag 58A clear (`PauseDetect`), and no queue slot busy (`[0x45BC424 + i*0x58] == 0 && [0x45BC428 + i*0x58] == 0` for i = 0..2).
4. Not already in an Emaki mission (`[[0x45B9E40]]+0x2C18 == 0`) and not in a boss fight. The rematch module owns a light BossBattleMode/End flag, the same sites as BossLog. Share them if BossLog is on.
5. The game's own menus are closed (`5A37D4`/`D63C0` signal false) and the mod menu is closing. The item path closes the game's menus itself (vtbl+0x150); we have no menu object, so we require them closed. Recommended place: standing at a shrine, after its menu is closed (unverified: see Q7).
6. No rematch is active (state machine in s.5 is `Idle`).

If an online session appears while a rematch is active (a summon accepted mid-fight), the mod cannot undo the patch safely. It logs `Rematch: online session during patched mission` and shows a red OSD line. Peers load their own unpatched record, so the boss may differ for them. This is documented as a user error. v1 makes no attempt to block summons.

## 3. UI (menu.cpp integration, done later by the src owner)
- New section `"Gameplay (solo/offline)"` with one row `Boss rematch...` (`kRematch`). It is visible when `[Rematch] Enable = 1` and the build is known. It is available when the gates hold; otherwise the value column says why ("online", "loading", "in fight", "in scroll", "not v2.02"). `kItems`/`kOrder`/the `kCount` static_assert grow by one.
- A on the row opens a **sub-page** (new `g_page` state; `Draw` renders the list instead of `kOrder`). B goes back. Rows are built from the catalogue (s.4) after a scan:
  `name | region | status | route`. Status values: `beaten (cycle N)`, `not beaten` (hidden unless `[Rematch] ShowAll = 1`), `unverified` (dim, not startable unless `[Rematch] ShowUnverified = 1`). Route values: `replay` (R, the default for every boss with a mission record), `hidden duel` (R plus the un-hide patch), `scroll` (E0), `patched` (E1). Sub-page footer: `Open the game's Battle Scroll menu`. It works as a shortcut (route R with the game's own UI), and is the fallback when a direct start is not yet verified.
- A on a row opens a confirm line: `Start rematch: <name>? A confirm / B back. Solo only. Drops: <policy>.` Confirming sets `g_req = {catalogue index, route}` (atomic), closes the mod menu and shows the OSD toast `Rematch queued...`. The game thread does the rest and posts the result to an OSD toast: `started`, or `refused: <reason>`.
- Header line: `Active: <name> (restores on exit)` while a patch is live, plus an `Abort and restore now` row. That row is only enabled when not inside the Emaki mission, for example after a failed load.

## 4. Data: boss catalogue and defeat checks
**Catalogue file** `Nioh3EngineFix_bosses.tsv` (next to the ini; read at Configure; it can grow without a rebuild). One row per boss:
`id, name, region, route(R|E0|E1), replay_mission_id (record +0x158, from battle-scrolls.csv), hidden(0/1), story_mission, defeat_kind(flag|clear|scroll), defeat_id, emaki_byte (or -), emaki_key_u32 (or -), placement_id, stage_mission, warp_x, warp_y, warp_z, warp_yaw, box_id, hell_id, region_byte, verified(0/1)`.
- Sources: BossLog captures (placement id, defeat flag id, mission-clear id), `boss-duel-flow` CSV (placement per mission), the Emaki table dump (Phase 0), and `boss-names.csv`. A tool script `tools\rematch_catalog.py` merges them. Rows start `verified = 0` and flip to 1 only after test R-3 (s.9) passes for that boss.
- Known defeat flags now: 0x128/0x129/0x12A (Demons of Pride/Envy/Obsession), 0x210 Otoroshi, 0x211 Murakumo (`boss-achievements.md`). The rest come from BossLog `MissionFlagSet` lines inside or right after the fight window.

**Defeat checks.** All of these are read-only game getters. They run **on the game thread** in the MissionLoadStep hook when the sub-page asks for a scan (`g_wantScan`). The results go to a fixed array plus a generation counter, so the menu shows `scanning...` for about one frame. Nothing is called from the Present thread.
| kind | call | args | notes |
|---|---|---|---|
| flag | `MissionFlagGet` 0x284944 `bool(Sel* s, u32 flagId, char useSlotFn)` | `Sel{u8 kind @0; i32 mode @4; u64 state @8}`, state = `[[0x47514D0]+8]`, useSlotFn = 0 | kind 6 + mode 0 = current journey level. To scan every cycle, kind = 1..5 with mode 1: "beaten in any cycle" = OR over 1..5, and "cycle N" = the highest. A byte value of 0xFF falls back to the definition default (handled inside). |
| clear | `MissionClearedGetSlot` 0x702B64 (= `Mission::IsMissionAlreadyClear`, map `+0x2C8`, byte `+0x38+slot`), per `battle-scrolls.md` | db = `[0x4751848]`, mission id | This is what the replay list itself uses (cleared rows get status 4), so "beaten" here matches what the game would list. Signature not decompiled here: confirm with a log first. **Conflict to settle in Phase 0:** 0x37AFC0 (map `+0x2E0`, byte `+0x38+slot`, which `boss-fights.md` calls "mission cleared") reads the map that `battle-scrolls.md` calls the *unlocked* map (`MissionUnlockedGet` 0x909218, byte `+0x18`). Log both for one cleared mission and one uncleared mission. |
| scroll | unique-enemy defeat flag (writer 0x10C6E70) | n/a | Getter not located. Until it is, a scroll boss counts as "beaten" when its story defeat check is true. |

## 5. Mechanism and exact call sequences
### 5.1 Threads and hooks (new file `src\rematch.cpp/.h`; menu.cpp only reads its state)
- **Game-thread pump**: an entry hook on `MissionLoadStep` 0x223AF50. It is called every frame from the frame loop, at 0x19DBAC0 inside 0x5A29F0, and it is the consumer of the request queue. Use a relocated-prologue thunk like bosslog.cpp, with all argument registers **and r10/r11** preserved (the 1.7.19 lesson) and bytes verified. The hook body does: scan requests, start requests, run the lifecycle tracker, then falls through into the original. A request we enqueue therefore gets consumed by the same or the next tick.
- BossBattleMode 23A2DCC / End 23A2F70 flags: reuse the bosslog sites. If both modules are on, one thunk calls both loggers.

### 5.2 Tier 0 (route E0): an existing Emaki record (no patch)
Preconditions: the s.2 gates. The catalogue row has `emaki_key_u32` (from the Phase 0 dump/log), or the player owns that scroll item.
```
// game thread, inside the MissionLoadStep hook
EmakiKey k{};                                    // 12 B, zeroed
if (row.owned_item_index >= 0) {                 // preferred: mimic 0xF1F060 path 0 exactly
    inv  = [[0x4751530]];  it = inv + 0x224A60 + idx*0xE8;   // count u64 at inv+0x23B4E0
    k.id = *(u32*)(it+0x20); k.w4 = *(u16*)(it+0x12); k.b6 = *(u8*)(it+0x0F);
    k.kind = map(*(u16*)it); k.mode = 1;  ref = *(u64*)(it+0x28);
} else { k.id = row.emaki_key_u32; k.kind/k.w4/k.b6 = values logged for this scroll in Phase 0; k.mode = 1; ref = logged ref; }
StartParams p{0x101, 0x1000000, 0x10100, 0, 0};
EmakiMissionRequest(ref, &k, 0 /*missionId: record+0x0C*/, &p);      // 0x10DBED0
ok = any slot i: [0x45BC424+i*0x58] == 0xCC96 && [0x45BC440+i*0x58] == k.lo8;
```
The original MissionLoadStep then consumes the slot. It sets mode 6, then calls `MissionSetupEmaki(charMgr, list, missionId)` 0x1C2794C, which clears `+0x2C18` and sets it from `MissionDb(missionId)+0x43 & 1`. The stage loads. Placement build 0x1C1E470 skips every placement whose id differs from `record+0x10`.

### 5.3 Tier 1 (route E1): a record-less boss (field, Crucible, secret) through a patched carrier record
The carrier is a scroll record the player has already cleared. It is never an uncleared one, because the carrier's clear/defeat flag may be written (s.6).
Emaki table object `T = [[0x45B9E30]+0xA88]`: `B = [T+0]` (u32 count at B+4, records at `B+8+i*0x34`), `M = [T+0x20]` (hash map: empty-key byte at M+4, entries `[M+8, M+0x10)`, 8 bytes each `{u8 emakiByte; pad; u32 recordIndex @+4}`, hash `h = (((b*9)>>11) ^ (b*9)) * 0x8001 mod n`, linear probe).
```
// game thread; state Idle -> Arming
R = B + 8 + carrierIdx*0x34
VirtualQuery(R) -> RW? else VirtualProtect(R, 0x34, PAGE_READWRITE) and remember the old protection
memcpy(g_snap, R, 0x34);  g_snapIdx = carrierIdx;  g_snapCrc = crc32(R)
write R+0x0C = row.stage_mission      // mission whose stage is loaded (0 = remap 0xCC96 via 0x1C17D8C: never use 0)
write R+0x10 = row.placement_id       // the only placement spawned
write R+0x14 = row.box_id             // wave-battle start box (0 if the log shows the scroll path ignores it)
write R+0x1C..+0x28 = warp x,y,z,yaw  // arena entry, from the Phase 0 position log
write R+0x2C = row.hell_id (u16), R+0x31 = row.region_byte
then exactly Tier 0 with the carrier's key  -> state Armed (t0 = now)
```
Alternative for a scroll boss the player does not own: **redirect the map instead of the record**. Write the carrier byte's map entry `+4 = targetRecordIndex` (one u32), snapshot it, and restore it the same way. No record bytes change. Use this whenever the target already has a record.

### 5.4 Lifecycle tracker (every MissionLoadStep tick while not Idle)
States: `Idle -> Arming -> Armed (queued) -> InMission (+0x2C18 == 1 and [[0x47514D0]+8]+0x14 == the queued mission seen) -> Exited -> restore -> Idle`.
- **Restore** when +0x2C18 drops back to 0, or the running mission id changes away from the queued one, after InMission was seen. The restore is: re-check the CRC of the bytes we wrote (if it changed, someone else wrote it: log it and still restore), `memcpy(R, g_snap)`, restore the protection, log `Rematch: restored record <idx>`.
- **Armed timeout**: if InMission is not seen within 30 s, or the request returned no slot, restore at once and toast `refused`.
- **Death inside the scroll**: no action. A retry reloads the stage and re-reads the record, so the patch must stay live.
- **Quit to title or reload a save**: the mission changes, so the tracker restores. The Emaki table is boot-loaded game data and never in the save, so a patch can never persist on disk. A process exit drops it too.
- **Resume from save** (`MissionStartFromSave` 0x2237244 also calls 0x10DBED0): if the game saved during the rematch and the player continues, it resumes the carrier key with the record already restored, so the carrier's real scroll fight loads. This is harmless and documented. v2 option: detect the resume through the Phase 0 hook on 0x10DBED0 and re-arm.

### 5.5 Route R (MVP): Battle Scroll mission replay
Covers the 62 listable mission records plus the 3 hidden duels. The fight is the whole mission script, the same as the game's own replay, so the arena, phases and cutscenes all work.
- **R-a, open the game's menu (first ship).** On the game thread: `MenuOpenMissionReplay(0, 0, 6)` 0x1DE28E8 (what `UI::OpenMissionReplayMenu` 0x23F90CC does) -> 0x1DDA258 -> `MissionReplayMenu_Setup` 0x1DC8424. The player picks the mission in the official list. No arguments are guessed. Gate: at a shrine (the top-menu context; Q7).
- **R-b, direct start from the mod list (after Phase 0 logs the replay's enqueue).** Replay with exactly the logged arguments: `MissionRequestById(missionId, slotByte, sub, params)` 0x2239600, or `MissionRequestEnqueue(params, req)` 0x223953C. The 0x58-byte request is copied from the log for the same mission, with only the mission id changed. Success = a queue slot holds the mission id at `+4`.
- **Un-hide the 3 duels (optional, `[Rematch] HiddenDuels = 1`).** On the game thread: `rec = MissionRecordGet([0x45B9E30], id)` 0x377B38 for 0xD22D, 0xB3F8, 0xFB96. Snapshot `rec+0x40`, clear bit 0x40, open R-a. Restore the u32 when the replay menu closes or the mission exits (the same tracker as s.5.4). The table is boot-loaded game data and never saved. Whether these scripts run cleanly from replay is untested (R-12).
- **Early unlock (optional, `[Rematch] UnlockEarly = 1`, default 0).** It only matters before "Crucible Manifested". `MissionFlagSet(g_MissionMgr, 0xAB, 1, 0)` 0x70275C, plus 0x217 and `FeatureUnlocked` bit 8 (`[0x4B6A170]+8`) for the Picture Scroll half. Flag 0xAB is a **saved story flag**, so set it only while the shrine menu is open, and write 0 back as soon as the replay request is enqueued or the menu closes. That happens before any save point. Log the revert. If the game saves in between (unknown, Q9), the only effect is that the menu shows early, which the game would grant later anyway.
- Beaten-boss filter for R rows: `MissionClearedGetSlot(db, record+0x158)` (s.4). The list is the intersection of catalogue rows and cleared missions.

### 5.6 Tier 3 (debug only): the raw queue
`MissionRequestById(missionId, slotByte = [[0x47514D0]+8]+0x10, 0, params)` 0x2239600. The boss only appears if the mission script does not skip it for a cleared mission. It sits behind `[Rematch] Debug = 1` and is not in the default UI.

## 6. State safety (what may change, and the policy for each)
| state | under Tier 0/1 | policy |
|---|---|---|
| Story mission flags (`+0x130+slot*0x400`) | Not expected: the story script should not run under 0xCC96 | **Verify** (Q4) with BossLog `MissionFlagSet`. If any story flag changes, add a guard that drops `MissionFlagSet` 0x70275C writes outside the Emaki allow-list while InMission. Do not ship Tier 1 until the guard decision is made. |
| Clear db, mission clear (`0x1C1DB88`) | May fire for 0xCC96 or record+0x0C | Verify (Q5). If it fires with a story mission id, block that call while InMission: clears are idempotent, but it also re-runs rewards, retry points and score. |
| Carrier scroll clear / unique-defeat flag (0x10C6E70, `IsClearWaveBattle` id 0xFFFF) | Likely written | Only use a carrier the player has already cleared, so the write changes nothing. Snapshot those values if their location is found. |
| Scroll item count | Unknown (consumed on clear?) | Verify (Q6). If it is consumed, Tier 0 with an owned item costs the item (the same as the game). Tier 1 must use the key-only call, without the item index. |
| Emaki record / map entry | Patched | Restored on exit (s.5.4). Never saved. |
| NG level `+0x10` / `+0x44` | Untouched | Never written by this feature. |
| Mission record `+0x40` bit 0x40 (hidden duels) | Cleared while the replay menu or mission is open | Restored on close or exit; game data, never saved. |
| Mission flag 0xAB (+0x217) early unlock | Set briefly (opt-in) | Reverted right after enqueue or menu close (s.5.5). This is the **only** story-flag write in the design, and it is off by default. |
| Route R replay | Whatever the official replay does (`IsMissionAlreadyClear` branches; clear db stays set) | Not policed: it is the game's own feature. Phase 0 P0-5 records its flag writes for reference. |
| Trophies | Story trophies are progress-threshold based, so no change. Boss-flag trophies (0x128..0x12A, 0x20F..0x211) already exist for beaten bosses. | Log `TrophyUnlock` 0x297F2DC in Phase 0. Any new unlock during a rematch is a bug to gate. |
| Autosave | May store the in-progress Emaki state with the carrier key | Harmless (s.5.4). Document it. |

## 7. Rewards policy
- **Default `[Rematch] Drops = 1` (game default for a Battle Scroll).** Whatever the boss actor's drop table gives on death (amrita, items, soul core chance) is granted. These are repeatable in the stock game already, through scrolls and later cycles, so they are not an exploit beyond what scrolls allow.
- **No first-clear rewards.** Story-script grants (`AddGuardianSpirit` behind `HasGuardianSpirit`, `AddFixItem` switches, first-clear bits `+0x53/+0x54`) live in the story mission script, which does not run under 0xCC96. Grants that carry `is_first_` stay false because the flags are already set. Phase 0 must show zero `Player::AddGuardianSpirit/AddSkillDojo` and zero new mission-clear reward lines (Q5/Q8).
- **Carrier scroll reward.** The carrier is already cleared, so any scroll-clear reward is the repeat amount, never a first-clear one.
- **`Drops = 0` (practice, v2)** suppresses drops for the rematch boss only. Approach: inside InMission, the hook on the script natives `Character::AddDropTable` 0x238B3EC calls `ClearDropTable` 0x238B194 (the wrapper) for `row.placement_id`. This is not designed further until Q8 shows which call grants the drop.
- Tier 2 (official replay) is not policed: it gives what the game gives.

## 8. Must verify in game first (Phase 0, log-only: `[Debug] BossLog = 1` + new `[Debug] RematchLog = 1`)
New log-only hooks in `rematchlog` (same thunk style, default off, no behaviour change):
- **L1** at the first MissionLoadStep after the title screen: dump the Emaki table: count, all records (0x34 B hex each), all map entries `{byte, index}`. This answers the record and key layout.
- **L2** on entry to `EmakiMissionRequest` 0x10DBED0: log ref, the 12 key bytes, missionId, the 0x18 params bytes, the computed byte and record index (s.5.3 hash), and all 3 queue slots on the next tick.
- **L3** on entry to `MissionSetupEmaki` 0x1C2794C: log param_3 and charMgr `+0x2BEF/+0x2BF4/+0x2C18` before and after (read on the next tick).
- **L4** each tick: log changes of `[[0x47514D0]+8]` `+0x10/+0x11/+0x14/+0x24`, `+0x2C18`, `+0x2BEF`, and the queue slots.
- **L5**: the unique-defeat writer 0x10C6E70, `ClearMapSetEntry` 0x217E884, `TrophyUnlock` 0x297F2DC, the item-grant script wrappers (`Item::AddItem/AddEquipment/AddFixItem`, `Character::ForceDrop*`, `Player::AddGuardianSpirit`) by binding RVA from `script-bindings.csv`.
- **L7** (route R): on entry to `MissionRequestById` 0x2239600, `MissionRequestEnqueue` 0x223953C and `MissionReplayMenu_Setup` 0x1DC8424, plus the list builder candidate `MissionSelectListBuild` 0x378674. Log the args and the full 0x58-byte request, so R-b can replay them verbatim. Also log `MissionClearedGetSlot` 0x702B64 vs 0x37AFC0 vs 0x909218 for the listed missions.
- **L6**: the player position at `BossBattleMode` start. The player-actor position offset is **unknown**: find it from the warp-write side of 0x1C2794C/0x10DBED0 consumers, or from `Player::*` position natives.

Phase 0 sessions (the user plays; we only read logs):
| # | session | answers |
|---|---|---|
| P0-1 | Use one owned Battle Scroll solo and win | Q1 key fields per scroll and which item path (0/1) solo uses; Q2 is +0x10 really the spawned placement; Q3 which script runs, and does `BossBattleMode`/HP bar fire under 0xCC96; Q5 mission-clear id(s); Q6 item count before/after; Q8 drop/grant calls |
| P0-2 | The same scroll: die once, then win | the retry path re-reads the record (L1/L3 lines repeat); the state after death |
| P0-3 | The same scroll: quit to title mid-fight, continue | what the save resumes (L2 caller = 0x2237244?); does +0x2C18 drop at title |
| P0-4 | Fight 2 story bosses normally (one with a fixed id in the duel-flow CSV, one "skeleton" script) | Q4 baseline story flags, defeat flag ids, arena entry position (L6) |
| P0-5 | Shrine -> Battle Scroll -> replay 2 cleared boss missions (one main, one sub) | route R: the L7 enqueue args, the boss present, flags written, rewards, the cleared-getter conflict |
| P0-6 | Read the logs for a 10-minute ordinary play stretch | the MissionLoadStep hook cost (must be zero measurable) |

Open questions: **Q1** the key -> byte mapping (0x102BB60/0x102B840; whether a key alone, without the item, works), **Q2** the record field meanings (+0x00..+0x0B, +0x18, +0x2E..+0x30, +0x32/+0x33 unknown), **Q3** whether story bosses need their mission script (phase transitions, invulnerability, cutscenes), **Q4** story flags under a scroll, **Q5** clear calls, **Q6** item consumption, **Q7** where the game allows scroll use (any time in the field, or a shrine only), **Q8** which call grants boss drops, **Q9** whether anything saves between the shrine menu and the mission start (this decides how safe the 0xAB early unlock is), **Q10** the exact replay enqueue arguments (L7).

**Answers from the 2026-10-08 PC session (P0-5, one main replay; `notes\session-2026-10-08-gamedata.md`):**
- **Q10 answered.** The shrine replay confirm `MissionReplayMenu_Decide` F1FD80 (choice 0) writes the request straight into the queue
  (no MissionRequestById/Enqueue call). Its bytes equal `MissionRequestById(id, u8 [[47514D0]+8]+10h, 0, {101h, 1000000h, 10100h, 0, 0, 0})`
  (params 16-aligned); request `+14` = 2 = replay. R-b = that call (built on branch `rematch2`, DirectStart = 0).
- **Q8:** no script drop/grant native fires for the boss (0 REWARD lines); drops are native. The inventory funnel is `InventoryAddItem`
  54D324 (InvLog on `rematch2`).
- **Q5:** `SetMissionClearFlag(52)` x2 at the clear (repeat; 52 was cleared). **Q4 (route R):** flag 118 = 1 inside the fight; outside the
  fight not captured. The replay start also marks 16 area records cleared (1C27BC4 -> ClearMapSetEntry, notify 0).
- **Q3 (route R):** the full script runs (placement 76781, BossBattleMode/End, 26.8 s); `ShowBossGauge` is not used by this boss.
- **Q1/Q2 partly:** Emaki record `+30h` = the map key byte, `+31h` = region byte, `+2Ch` = hell id; `+0Ch/+10h/+14h` hold 17,00x,xxx ids
  (not a 16-bit mission id at `+0Ch`: s.5.3 must be re-checked before E1). Key-alone start still open.
- **s.4 getters:** 702B64 == 909218 on all 271 lines; keep 702B64.
- **Open:** Q6 (no scroll used), Q7 field use, Q9 (BuildSaveFilePath does not fire on shrine saves; hook the save writer 5B6F37 instead),
  E-route captures (P0-1..P0-3), the normal-clear baseline (P0-4).
- **Blocker found:** `MissionLoadStep` only ticks during mission loads (2 ticks in 150 s of field play), so it cannot be the s.5.1 pump.

## 9. Build plan and test cases
1. **Phase 0 (log-only, ASI 1.7.x).** New files `src\rematchlog.cpp/.h` with hooks L1-L7. Add patches.json group `RM`, run `tests\test_patterns.py` until `ALL N SITES OK`, write a MODLOG entry. Run sessions P0-1..P0-6. Exit criterion: Q1-Q10 answered in an update to this note, plus `tools\rematch_catalog.py` producing the first `Nioh3EngineFix_bosses.tsv` (scroll bosses verified).
2. **Phase 1 = MVP, route R + UI (`[Rematch] Enable = 1`, default 0).** New `src\rematch.cpp/.h`: catalogue load (R rows generated from `battle-scrolls.csv`), gates, the cleared-mission scan, the game-thread pump, R-a (open the official replay menu), then R-b (direct start with the L7 arguments). Also the hidden-duel un-hide with its restore. menu.cpp: section, row, sub-page, confirm, toasts. The src owner integrates these, one agent at a time.
   - R-11: from a shrine, the mod row opens the game's replay menu; a cleared boss mission replays with the boss present.
   - R-12: `HiddenDuels = 1` lists Honda Tadakatsu, Yoshitsune and Jubei/Chuya; one of them replays and finishes; the record `+0x40` is restored after exit (logged).
   - R-13: R-b direct start for 3 bosses (main, sub, DLC). The request bytes equal the L7 log of a menu-started replay of the same mission.
   - R-14 (only with `UnlockEarly = 1`, on a pre-Crucible save copy): the menu appears, flag 0xAB reads 0 again after enqueue, and the save diff shows 0xAB unchanged.

   Then **Phase 1b, route E0** (existing Emaki records, observe-only tracker, nothing to restore):
   - R-1: menu row hidden with Enable=0; visible with Enable=1; greyed and showing the reason while loading, in a fight, or in a scroll.
   - R-2: online gate. With a co-op room open (or the session byte forced in a test build), the row says "online" and a request is refused on the game thread.
   - R-3: start an owned scroll boss from the menu at a shrine. The same L2 key bytes as P0-1 appear, the right boss is the only enemy, and the log shows identical BossLog flag lines to P0-1.
   - R-4: a queue-full or mid-load request (spam the confirm during a warp) is refused cleanly and nothing is enqueued twice.
3. **Phase 2, route E1 (bosses with no mission record: field, Crucible, secret; per boss `verified`).** Map-entry redirect first (scroll bosses not owned), then the record patch. Snapshot/restore and the CRC check. A flag guard only if Q4 demands it.
   - R-5: rematch a story boss and win. Exactly that placement spawns; zero story `MissionFlagSet` writes; the record bytes equal the snapshot after exit (log the CRC).
   - R-6: die twice, then win. The patch stays live across retries and the boss is the same each time.
   - R-7: quit to title mid-fight. Restore is logged before the title; continuing loads the carrier's real scroll or the shrine, never the patched boss.
   - R-8: force the Armed timeout (block the consume in a debug build). Restore within 30 s and the toast says refused.
   - R-9: the defeat list after 3 rematches is unchanged; a save backup diff (`save-work`, savecrypt.py) shows only expected deltas (inventory, amrita, playtime). Mission flag block bytes are identical.
   - R-10: per boss, the HP bar shows, `BossBattleModeEnd` fires, and there is no leash or invulnerable phase. On failure the row stays hidden. Bosses that also have a mission record always use route R instead.
4. **Phase 3, polish**: `Drops = 0` practice mode (after Q8), resume re-arm (s.5.4), and a per-cycle picker (load the boss at the scaling of cycle N: out of scope unless Q-level research shows `+0x11` can be set safely for one mission).

## 10. Risks
- **Story-boss scripting (high).** Many bosses depend on their mission script for phases, arena gates, cutscenes and `BossBattleMode`. Under 0xCC96 only the Emaki script runs. Mitigation: every boss that has a mission record uses route R (its full script). E1 is only for record-less bosses, each gated by `verified`.
- **Replay coverage (low).** Route R is the game's own feature. The open risks are the 3 un-hidden duels (they may be hidden for a reason, R-12) and the guessed enqueue args, which R-b avoids by copying L7 logs.
- **Spawn position or stage streaming (medium).** A warp outside the loaded cells or inside geometry. Mitigation: positions are logged from real fights (L6); the record warp is copied from a scroll of the same stage when one exists.
- **Record lifetime (medium).** Restoring early breaks a live mission. Restoring late leaves the carrier pointing elsewhere until exit (harmless offline). The tracker restores only on a confirmed exit or timeout.
- **Write protection (low).** Data-DB pages could be read-only. Handled with VirtualQuery/VirtualProtect, and the original protection is restored.
- **Online desync (medium, user-caused).** A summon during a patched mission shows peers a different boss. Gated at start; warned during.
- **Thread races (low with this design).** All game calls and writes run on the game thread inside MissionLoadStep. The menu only sets atomics and reads snapshots.
- **Version drift (low).** v2.02 only; byte-verified sites; the feature disables itself on any mismatch, like the other fixes.
- **Unknown grant side effects (medium until Q5/Q8).** Mission-clear re-entry, trophies, and the carrier scroll's state. Phase 0 must prove them benign before Phase 2 ships.

## 11. Struct and offset summary (v2.02)
| item | known | unknown |
|---|---|---|
| Emaki table `T=[[0x45B9E30]+0xA88]` | `[T]` records (`+4` count, `+8+i*0x34`), `[T+0x20]` byte->index map (`+4` empty key, `+8/+0x10` vector of 8 B entries) | loader/owner (candidates 0x102B840, 0x3B35B4); key->byte functions 0x102BB60/0x102B840 |
| Emaki record 0x34 | `+0x0C` mission id, `+0x10` boss placement, `+0x14` box, `+0x1C..+0x28` warp xyz/yaw, `+0x2C` u16 hell id, `+0x31` region | `+0x00..+0x0B`, `+0x18`, `+0x2E..+0x30`, `+0x32..+0x33` |
| EmakiKey 12 B | s.1 | the meaning of `+4/+6`, kind values 1-5 |
| Queue slot `0x45BC420+i*0x58` | `+0` key[7], `+4` 0xCC96, `+8` record region, `+0xC` mission, `+0x14` u64 2, `+0x20` key, `+0x30` ref, `+0x38` online mode, `+0x3C/+0x3D` online flags, `+0x40` params | `+0x10`, `+0x1C` |
| charMgr `[[0x45B9E40]]` | `+0x2BD0` scroll struct, `+0x2BEF` emaki byte, `+0x2BF4` key[8], `+0x2BF8/+0x2BFC/+0x2C00` from list, `+0x2C18` is-emaki, `+0x2C1C` u64 cleared at setup | the rest of the struct |
| Mission record 0x198 (`MissionRecordGet` 0x377B38, `[[0x45B9E30]]`) | `+0x40` flags (0x40 hidden, 0x1000000 Emaki-capable, 0x20000 online), `+0x158` mission id, `+0x15A` kind, `+0x160` era, `+0x17E` list progress (`battle-scrolls.md`) | replay list builder (0x378674 candidate) |
| Mission state `[[0x47514D0]+8]` | `+0x10` level, `+0x11` scaling row, `+0x14` mission id, `+0x24` mode, `+0x130+slot*0x400` flags | which field means "at shrine/hub" |
| Online | `[0x4709B50]+0x34` (1/2 = online), `[0x45C84F8]+0x1DC/+0x2C8` guest | summon-accept hook point |
| Getters | `MissionFlagGet` 0x284944 `(Sel*, flagId, 0)`, `MissionClearedGet` 0x37AFC0 `(db, id)` | the unique-defeat-flag getter |

## How to run Phase 0 (ASI 1.7.21)
Set `[Debug] BossLog = 1` and `RematchLog = 1`, restart, solo/offline, then `grep RematchLog` (and `BossLog`) in the mod log. Save-copy first is optional (nothing writes). Correction from the new decompile: `MissionRequestEnqueue(rcx, rdx)` takes the first 8 StartParams bytes by value in rcx and the 0x58 request in rdx; `EmakiMissionRequest(ref, key*, missionArg, params*)` matches s.1.
1. Boot to a loaded save, stand at a shrine. Expect `L1` table dump (EMAKI[i], EMAKIMAP) and one `L4 first` line (online conn=0, pm null).
2. P0-5 first (menus only): Shrine -> Battle Scroll -> (Mission Replay). Expect window OPEN, `Q7/L7` replay-menu lines, `MissionSelectListBuild`, and `getters` lines comparing 702B64/37AFC0/909218 per listed mission. Back out without starting; window closes after 90 s.
3. Replay one cleared main boss mission and one sub mission to the end (win). Expect `L7 MissionRequestById/Enqueue` (req[58] hex), slot lines, `IN MISSION`, flag/clear/reward lines, `END:` lines, `EXIT`.
4. P0-1: use one owned Picture Scroll (item menu) and win. Expect `Q7 EmakiItemUse_MenuHandler`, `L2` (key, item bytes), `L3`, after-states, `Create`/`ShowBossGauge` (do BossBattleMode lines appear = Q3), reward lines, item `changed` lines (Q6), `EXIT`.
5. P0-2: same scroll, die once (watch `END: RetryGadget/SetDecideRetry/GameOver`, repeated L3), then win. P0-3: start it, quit to title mid-fight, Continue (`Q9 MissionStartFromSave`, `BuildSaveFilePath` timing vs request).
6. P0-4: fight two story bosses normally (SetMissionFlag/clear/trophy baseline). P0-6: 10 min ordinary play, check frame time unchanged.
Not covered: L6 player position (no offset known; scroll record warp floats are in the L1 dump at +0x1C..+0x28). The EmakiKey->byte value is read from `ch.emakiByte` in the `after-MissionSetupEmaki` line instead of calling 0x102BB60.
