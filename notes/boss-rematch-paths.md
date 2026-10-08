# Boss rematch: how the game itself replays encounters (research only, 2026-10-08)

Static analysis of v2.02 (image base 0x7FF74F410000, RVAs below), via `D:\nioh3-re\decomp_ro.cmd` and the script binding table (`notes\script-bindings.csv`). Game not run, no mod code. Builds on `notes\boss-fights.md`. Names: `re\symbols\incoming\boss-rematch-renames.tsv`. Confidence: medium (decompiles clear, data side not dumped live).

## Ranking (cleanest rematch first)

| # | Path | Clean? | Why |
|---|---|---|---|
| 1 | **Battle Scrolls = Emaki mission 0xCC96** | best | Designed refight. One generic mission (id 0xCC96 = 52374) that loads a stage and spawns only the boss placement named by an Emaki record. No story flags touched. A mod needs only `EmakiMissionRequest` (0x10DBED0) with a record key. |
| 2 | **Mission replay menu** (shrine -> `MENU_ID_MISSION_REPLAY`) | good | Official replay of a cleared mission through the normal mission-start queue. Replays the whole mission (stage, script, boss at the end), not just the boss. |
| 3 | `Mission::ChangeMission` / mission-start queue (0x2239600) | good, raw | Same queue the menus use; a mod can enqueue any mission id. Script decides boss presence (`Mission::IsMissionAlreadyClear`). |
| 4 | NG+/cycle (per-cycle flag slots) | heavy | Mission flags are stored per slot (`[[g_MissionMgr+8]+0x130 + slot*0x400]`); a new cycle uses a fresh slot. Whole-game reset, not per boss. |
| 5 | Retry after death | n/a | Boss is a placed actor rebuilt on stage load; dying already refights an *undefeated* boss. `Gadget::Retry::*` is the co-op/ido revive gadget, not a boss refight. |
| 6 | Clear-map reset (`Hell::ResetClearMapOnlySmallHell`) | narrow | Game-owned writer for the clear map at [0x4751848], but only for Crucible "small hell" areas (type 3). |
| 7 | Unused `Mugen::IsAllMugenBossesDead` | dead | Binding is a stub that always returns 0: a cut multi-boss mode. No boss-rush/test-battle strings exist in the exe. |

## 1. Battle Scrolls (Emaki) - recommended
- **Data**: Emaki table `[[g_DataDb=0x45B9E30]+0xA88]`, records 0x34 B, looked up by a 1-byte emaki id (`EmakiRecordGet` 0x1C1425C; hash lookup inside `EmakiMissionRequest`). Fields seen: `+0x0C` mission id (0 -> 0xCC96), `+0x10` **boss placement/enemy id** (the only placement spawned, see below), `+0x14` wave-battle start box id, `+0x1C..+0x28` warp pos (xyz, yaw), `+0x2C` hell id, `+0x31` region byte.
- **Current scroll**: struct at `[[g_CharMgr=0x45B9E40]]+0x2BD0`; emaki id byte at `+0x2BEF`, `+0x2BF4` region, `+0x2C18` "is emaki mission" (mission record `+0x43` bit0, set in `MissionSetupEmaki` 0x1C2794C).
- **Item side**: scrolls are inventory items (`emaki_item_`, inventory `[0x4751530]+0x224A60`, stride 0xE8, count `+0x23B4E0`). The item-use menu handler `EmakiItemUse_MenuHandler` 0xF1F060 reads item fields `+0x20` (u32 key), `+0x12` (u16), `+0x0F` (byte), `+0x28` (u64 ref) and item id specials (0x1E82->1, 0x516D->2, 0xD523->5, 0xDD82->4, else 3), then calls `EmakiMissionRequest`.
- **Entry**: `EmakiMissionRequest(u64 ref /*-1 = none*/, EmakiKey *key /*{u32 id, u8 region@+4, u8 @+7}*/, int missionId /*0=default*/, StartParams *p)` 0x10DBED0. It writes a pending mission (id 0xCC96) into the mission request queue (`g_MissionRequestQueue` 0x45BC420, 3 x 0x58 B, field `+4` mission id). Other callers: `MissionStartFromSave` 0x2237244 (resume into an Emaki mission), online lobby 0x21DD494/0x21DF450.
- **Spawn filter**: on stage placement build (0x1C1E470), when the running mission is 0xCC96, placements whose id != `emakiRecord+0x10` are skipped. So the scroll *is* "this stage, this boss only".
- **State**: completion is in the clear db: `Emaki::IsClearWaveBattle` -> 0x107BB78(g_ClearDb, 0xFFFF). The unique-enemy defeat flag (`emaki_unique_enemy_defeat_flag_`, field writer 0x10C6E70) is per-scroll, synced online (`Online::MissionSyncEmakiUniqueEnemyDefeatFlag`). Story mission flags are not touched.
- **Unlock condition**: not located statically (item-acquire side; the online note says after "Crucible Manifested"). Inventory gating only: no scroll item, no menu entry.
- **Mod trigger (solo/offline, at a shrine/hub, not in a mission)**: build an `EmakiKey` for the wanted emaki id and default `StartParams` (copy the constants from 0xF1F060: `{0x101, 0x1000000, 0x10100, 0}`), call `EmakiMissionRequest(-1, &key, 0, &p)`; the next tick of `MissionLoadStep` (0x223AF50) consumes the queue. To refight a boss that has no scroll, the table record `+0x10` (and stage/warp/hell fields) would have to name it: either patch a record in memory before the call or add one (table is game data, not in LFM; owner of 0xA88 not traced). Preconditions: queue slot free (`[0x45BC424+i*0x58]==0`), not in co-op (`0x5A9004`/`Online::*` idle), not mid-load.
- Log-first step: hook 0x10DBED0 and dump `key`, the resolved record and `+0x2BEF` while using one real scroll.

## 2. Mission replay (shrine menu)
- Script binding `UI::OpenMissionReplayMenu` 0x23F90CC -> `MenuOpenMissionReplay` 0x1DE28E8(owner,0,6) -> menu object `[g_MenuMgr=0x45C84A8]+0x4E0` (0x1DDA258 -> 0x1DD8424). The shrine top menu `ShrineTopMenu_Select` 0x1DAFE64 opens it for option 0 (option 1 opens `[g_MenuMgr]+0x4E8`, likely the Emaki/scroll select).
- The menu ends in the shared mission-start queue (functions below). On close with mode 1 (`menu+0x434==1`, 0x1DB6F4C) it calls `MissionRequestReturn` 0x22396D8, which restarts from the saved mission fields of `[g_MissionMgr+8]` (`+0x18` region, `+0x19`, `+0x1C` mission id).
- What resets: a replay is a fresh load of the mission: placements (bosses included) are rebuilt; mission flags are not cleared wholesale, the script branches on `Mission::IsMissionAlreadyClear` (0x23A4D14, clear db by mission id) to decide what to respawn. Clear-db entry stays set. Exact per-mission behaviour is script data.
- Mod trigger: open the menu (`MenuOpenMissionReplay(0,0,6)` while at a shrine) and let the player pick, or skip UI and enqueue directly (section 3).

## 3. Mission-start queue (raw API)
- `MissionRequestEnqueue(params, req)` 0x223953C: first free of 3 slots at 0x45BC420 (0x58 B; `+4` mission id, `+8` region/flag byte, `+0x20` params), consumed by `MissionLoadStep` 0x223AF50 (sets mode 6, runs `MissionSetupEmaki` when needed).
- Wrappers: `MissionRequestById(missionId, u8 slotByte, int sub /*0 = default*/, params)` 0x2239600 (used by `Mission::ChangeMission` 0x23A313C, which only acts when `[[g_MissionMgr+8]+0x24]==2`); `MissionRequestRegion(region, slot, missionId, mode, params)` 0x22399B8 (World::WarpOtherWorld* bindings 0x23B9C28..0x23BA4EC); `MissionRequestReturn` 0x22396D8; `MissionIdRemapForRegion` 0x1C17BE4 (hard-coded region->mission fallbacks); `MissionRequestFromQuest` 0x22398D4 (mission 0x86EC with quest id).
- Mission manager fields used: `[g_MissionMgr+8]` `+0x10` slot byte (difficulty/cycle), `+0x11` region, `+0x14` mission id, `+0x24` mode (1/2), `+0x21848/+0x21850` resume ids.
- A rematch via this path = load the boss's mission again. Boss appears only if the script does not skip it for a cleared mission; combine with the clear-db/flag reset in `boss-fights.md` section 9 if it does.

## 4. NG+ / cycle
- Mission flags and clear-db bytes are per slot (`slot` from `[g_MissionMgr+8]+0x24/+0x10`, function 0x285448). Starting a new cycle switches slot, so all bosses return. Not a per-boss tool; the cycle-start writer was not traced.

## 5. Retry / death
- `Gadget::Retry::Resurrect` 0x23D3090 -> 0x1CA7468 (revive each player) + 0x1C21138 (`[0x45C8468]` retry manager); `SetDecideRetry` 0x23D3388 -> 0x1C251C8. These are the co-op revive/retry gadget. Death -> retry point (`Player::GetRetryPointID`) -> stage reload; an alive boss is rebuilt from placement. Nothing here revives a defeated boss.

## 6. Clear map
- `ClearMapSet(u16 areaId, bool)` 0x219FEB4 -> `ClearMapSetEntry(g_ClearDb, id, v, notify)` 0x217E884; getter `ClearMapGet` 0x2845C8 (FNV-1a over the 2-byte id, table `[db+8]+0x428`, byte `+0x23+slot`; also `+0x2B` discovered, `+0x1B` reward). Area records in `[[0x45B9E30]+0x630]` (0xC4 B, `+0xC0` type: 1 region (trophies 0x97/0x191), 2 set by net handler 0x10C50D4, 3 small hell). Script bindings `Hell::Set/ResetClearMapOnlySmallHell` (0x23A00F0/0x239FFB8) only pass type-3 ids. A mod could call `ClearMapSet(id,0)` for other types, but this resets an area's clear marker, not a boss fight.

## Recommendation
Build the rematch on **Battle Scrolls**: hook-log `EmakiMissionRequest` once, then a solo-only menu that calls it with a chosen emaki id. Extending to bosses without a scroll needs the Emaki table owner (`[0x45B9E30]+0xA88` loader) traced and one record patched in memory before the call. Fallback: mission replay/queue (section 2/3) for story bosses.
