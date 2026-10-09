# Play session 2026-10-08 20:59-21:22 (PC, ASI 1.7.39): game data for the boss rematch

Source: `captures\playsession\20261008-2059-session\modlog_run1.log` (2491 lines; BossLog + RematchLog on), digested with scripts
(no game run, no Ghidra; capstone on `re\Nioh3.dump.exe`). The user replayed main mission 52 "Crucible Manifested" (chapter "The
Hamamatsu Incident") from a shrine through Battle Scroll -> Mission Replay, fought Jakotsu-baba (placement 76781), cleared, then rested
and saved at a shrine. Chests, enemy drops and item pickups happened, but no hook in this build was on their path (s.3.4).

## 1. How the replay started (Q10): the exact call, and what R-b must call
| time | line | meaning |
|---|---|---|
| 21:03:05.449 | `L7: MissionSelectListBuild caller=+9C4EC3` (+ 271 `getters` lines) | Battle Scroll mission list built |
| 21:03:23.034 | `replay menu open (1DDA258) caller=+1DE2910` / `MissionReplayMenu_Setup ... p5=0 p6(era)=6` | replay menu opened from the shrine (via `MenuOpenMissionReplay` 1DE28E8; the `BattleScrollTop_Select` and `UI::OpenMissionReplayMenu` hooks did **not** fire) |
| 21:03:54.326 | `Q7: ItemUse_F1FD80 caller=+34A474 rcx=1F2D10BF018 rdx=0` | **the start.** F1FD80 is not an item use: it is `MissionReplayMenu_Decide`, the confirm callback (std::function invoke at 34A474). rcx = closure `{menu*, bool}`, edx = 0 = choice 0 |
| 21:03:54.946 | 16 x `L5: ClearMapSetEntry ... caller=+1C27D0E` | consumer side (s.3) |
| 21:03:56.482 | `L4 slot0 ... mission=52 region=96 stageMission=72636 ... bytes[58]=01 00 00 00 34 00 00 00 96 ...` | the request in queue slot 0 |
| 21:04:07.560 | `L3: MissionSetupEmaki caller=+223B12C ... mission=52` | MissionLoadStep consumed it (setup runs for every mission; `isEmaki` stays 0) |

**No `MissionRequestById` / `MissionRequestEnqueue` line exists for the replay.** F1FD80 with edx = 0 (F1FDC3) writes the 0x58-byte request
into the first free queue slot itself (F1FE63..F1FF05, F20365): `+0` u8 `[menu+2588h]`, `+4` u32 `[menu+24F4h]` (the selected mission,
set by the list at 1DD6BCA), `+8` u8 record `+15Ch` (via MissionIdToIndex 377B8C), `+C` u32 `1C17D8C(id)` (stage), `+10` u16 0, `+14` u64 2,
`+1C` 0, `+20` {0, 1}, `+28` u16 1, `+30` -1, `+38`/`+3C` 0 offline (online: mode and flags as Enqueue's 2239C94), `+40` params
`{101h, 1000000h, 10100h, 0, 0, 0}`. Then it closes the menu (vfunc +150h). `[menu+2588h]` = the Setup's 6th argument, and when that is 6
(the shrine passes 6) = `[[47514D0]+8]+10h` (1DC849C; L4 `lvl=1`). The logged slot matches byte for byte (52 / 0x96 / 72636 / mode 2 /
params). The other branch of F1FD80 (edx = 1 with the closure bool set) calls `MissionRequestById(id, slot byte, 0, params)` 2239600, which
builds exactly the same request (223965C..22396B1) and Enqueues it, then writes the replay marker `[[4751848]+8]+CF8h = 1`; the
session's path did **not** write that marker.

**R-b = `MissionRequestById(missionId, u8 [[47514D0]+8]+10h, 0, &params)` on the game thread, params 16-byte aligned (movaps at
2239660), nothing else.** Success = the first free slot then holds the id at `+4` with `+14` = 2. Request `+14`: 2 = replay (mission
state `+24` becomes 2 in the replay: `ms{... mode=2}` at 21:03:56), 1 = continue (21:01:17, caller +2239A64) and return-to-hub after the
clear (21:19:54, caller +22398BD, `+18` = 52 the mission just left). Built on branch `rematch2` (s.5).

## 2. Phase 0 answers (design s.8)
| Q | answer | evidence |
|---|---|---|
| Q1 key -> byte | **Partly.** The Emaki map key byte equals record `+30h` for all 20 records (map D4->0, B9->15, ...); the key -> byte function and "key without the item" are still open (no Picture Scroll used: no L2 line) | L1 `EMAKI[i]` / `EMAKIMAP` (20 valid of 41) |
| Q2 record fields | **Partly.** `+30h` = map key byte, `+31h` = region byte (0x96 for records 0-6 = mission 52's region; 0x4B, 0xB1, 0xE0, 0xD8, 0x35 for the others), `+2Ch` u16 = hell id (100, 166, 200, 401, 403, 806 or 0), `+2Eh` u16 = 0/1/2, `+1C..+28h` = floats in game units (e.g. 204649, 12392, 238099, yaw 180). `+0Ch/+10h/+14h` hold 17,000,009..17,009,835-range ids: **not** a 16-bit mission id at `+0Ch` (design s.5.3 is wrong there); placement-like ids of the scroll stage. No record references mission 52 or placement 76781 | L1 dump |
| Q3 script needed | Route R: the full mission script ran: placement 76781 created 2.4 s before `BossBattleMode`, one `SetMissionFlag` inside, `BossBattleModeEnd` after 26.8 s. `ShowBossGauge` (23B8678) never fired: this boss's HP bar does not use that native (boss-fights-table `show_boss_gauge 0` for DE3B2E075980). Under 0xCC96: open | BossLog #1 |
| Q4 story flags | Replay of a cleared mission wrote `SetMissionFlag 118 = 1` inside the fight (2 s before End). Writes outside the fight window were **not captured** (the flag hook logs only fight + 20 s, and the replay window never opened, s.3.4) | BossLog #1 |
| Q5 clear calls | `SetMissionClearFlag(52)` twice at 21:19:54.760 (20.4 s after End, same ms as the return-to-hub request); mission 52 already read cleared (702B64 = 1) at 21:03:05, so a repeat write | BossLog, getters |
| Q6 item count | Not tested (no scroll used). `invCnt` (inv+23B4E0) read 400 on all 29 L4 lines despite pickups: that field is not an item count | L4 |
| Q7 where | Only the shrine route was used (Setup p6 = 6). Field use still open | L7 |
| Q8 drop calls | **No script drop/grant native fired** in the fight + 30 s window (24 hooks: 23 natives AddDropTable ... GetGameClearRewardHard + GameClearRewardGrant): the boss drop is granted natively. The engine funnel is `InventoryAddItem` 54D324 (s.3.3) | 0 REWARD lines |
| Q9 save between menu and start | Not answerable: `BuildSaveFilePath` fired only at boot (20:59:59 x2), not for the 21:22 shrine save, so it is not a save detector. Use the save writer call (tour's SB1 site 5B6F37 -> SaveFileWriteAtomic) next time | Q9 lines |
| Q10 enqueue args | **Answered**, s.1 | s.1 |
| s.4 getter conflict | 702B64 == 909218 on all 271 getter lines (77 ids, 35 cleared). 37AFC0 differs 9 times, only when 37AFC0 itself was the hooked call (its own db argument, `via=1`, callers 9090D0 / 37ADF4 / 909099). Keep 702B64 | getters |

## 3. Rewards and state: every line accounted for
### 3.1 Count
Reward/state lines in the log (regex `REWARD|L5:|SetMissionFlag|SetMissionClearFlag|Trophy|ClearMapSetEntry|unique|AddItem|AddGuardian|AddSkill|AddMoney|SetReward|GameClear|Drop|changed:|END:|amrita`): **41 seen, 41 accounted for.**
| # | time | line(s) | source | first clear / repeat | note |
|---|---|---|---|---|---|
| 21 | 21:00:50-21:01:16 | `END: MissionResultUpdate (rate 1/s) caller=+565159` | title / Continue screen before the save loaded (t 56-81 s; first mission tick at 83 s) | - | UI tick, not a reward; nothing at the replay's end |
| 16 | 21:03:54.946 | `L5: ClearMapSetEntry id=101,150,151,152,153,161,162,163,164,165,166,167,168,170,180,181 p3=1 p4=0 caller=+1C27D0E` | replay start (consumer side, fn 1C27BC4(cl, mission)): every area record of `[[45B9E30]+630h]` whose `+BCh` == cl and `+B8h` != the mission's `+182h` is set cleared, notify 0 | persistent clear-db write; on this journey these were very likely set already (cannot tell: no before value) | **not a reward**; area markers of region 0x96 (ids 150-153 etc.) |
| 1 | 21:18:53 | `L5: TrophyUnlock id=36 flag=1 caller=+19E65C3` | stat counter: `StatSet` 605034 -> table 3978EB0 maps stat 0x84 -> trophy 36; every update of stat 0x84 calls TrophyUnlock (idempotent) | not new evidence of an unlock | gameplay stat 14 s before the boss, not a boss trophy |
| 1 | 21:19:32.373 | `SetMissionFlag id 118 value 1 (in fight)` | boss script, 2 s before `BossBattleModeEnd` | repeat (mission cleared before) | the defeat flag of this fight (s.3.5) |
| 2 | 21:19:54.760 | `SetMissionClearFlag mission id 52` x2 | mission end | repeat | followed in the same ms by the return-to-hub request |
Zero lines of: `REWARD` (all 23 natives + GameClearRewardGrant), the Emaki unique-defeat serializer, L2 item `changed`, `ShowBossGauge`.

### 3.2 Why the reward hooks saw nothing (two reasons)
1. **Wrong layer.** The BossLog reward hooks (BL7..BL30) are script natives (`Item::AddItem`, `Character::AddDropTable`, ...). Only mission
   scripts call them. Chest opens, enemy/boss drop pickups and pickups go through engine code that calls the inventory directly.
2. **Wrong window.** They log only during a fight + 30 s, or while the RematchLog replay window is open. The window opens from
   `MissionRequestById/Enqueue/EmakiMissionRequest` hooks, and this replay used none of them (s.1), so the replay mission was never a
   window. The mission-end clear (21:19:54) was inside the 30 s grace, so a script reward there would have been logged: none fired.

### 3.3 The funnel and the new hook (branch `rematch2`, `[Debug] InvLog = 0`)
`InventoryAddItem` **54D324**`(rcx save [4751530], rdx out, r8 ItemDesc*, r9, [rsp+28h] u8 notify)`: looks the id up in the item table
(`[g_DataMgr+68h]` via F4DBC), branches on item record `+182h` (category: 0x13, 0x12, 0x0A, 0x0B, 3 ...), bumps stats (StatAdd 604F48 ids
0xD6/0xCD/0x1C7) and adds. ItemDesc = the 0xE8-byte inventory record built by `54CCD0(desc, id, n)`: `+0` u16 id, `+2` u16 base id,
`+0Eh` u32 init 0x4000, `+18h` flags, `+28h` serial (global counter), `+30h` u8 from item record `+184h`. 87 call sites in 70+ functions,
including all five `Item::Add*` natives (23A0208 / 23A0414 / 23A07D4 / 23A0A98 / 23A0C64), the game-clear reward (2280C84) and the inventory
internals (54CD9C, 54EFA0); no second add path was found (54E574 has 1 caller = 54D324). InvLog logs every call session-wide: id, base,
`+30h`, notify, r9, caller + its pdata function start + up to 3 earlier return addresses from the stack + desc[0x48], 30 lines/s cap. The
source (chest / enemy drop / boss drop / mission reward / shop) is classified offline from the caller chain plus the session timeline.
patches.json BL31, `tests\run_invlog_harness.py` ALL PASSED. Also on `rematch2`: F1FD80 choice 0 now opens the RematchLog window, so the
next replay logs flags and rewards for the whole mission. `tools\playsession\session_lib.ps1` capture keys now include `InvLog = 1`
(needs the rematch2 ASI; older builds ignore the key).

### 3.4 Not captured (and the hook that would)
| missing | hook |
|---|---|
| every item / equipment / material gained (chests, drops, pickups, shop, rewards) | InvLog 54D324 (built) |
| amrita / exp / gold | none yet (engine add not traced; `Player::AddMoney` is script-only) |
| story flags outside the fight | RematchLog window fix (built) or log `MissionFlagSet` session-wide |
| saves (Q9) | the save writer call 5B6F37 (tour SB1) |
| kodama / collectibles / soul core count | not hooked; InvLog shows soul cores if they are inventory items |

### 3.5 Data tables vs the log (Jakotsu-baba, mission 52)
| table | predicted | log | verdict |
|---|---|---|---|
| boss-master row (script DE3B2E075980) | placement 76781, char 947280 / 20833, defeat `flags=115;512`, `QuestClear=mission:52`, clear gadget 12bed | placement 76781 created; flag **118**; clear 52 | placement OK, clear OK, **flags mismatch** (118 not 115/512) |
| boss-defeat-checks | `clear_map mission:52`; flag 512 = boss battle START | clear 52; no 512 inside the window | clear OK; 512 unverifiable (it may be set before `BossBattleMode` starts the window) |
| boss-fights-table DE3B2E075980 | flags_all 114;115;512, show_boss_gauge 0 | 118, no gauge native | **118 missing from the script decode**; gauge OK |
| battle-scrolls.csv row 35 | id 0x34, region_15c 0x96, "Crucible Manifested", Jakotsu-baba | queue +8 = 0x96, replay of 52 | OK |
| boss-master online row "The Hamamatsu Incident (obj: Crucible Manifested)" | mission_id 3150, in_battle_scrolls = no | mission 52, replayed from Battle Scroll | **mismatch** (web guess) |
| boss-drops / boss-loot | rewardset 60558, soul core item 132302 (inferred, L) | no observation (native drop) | unverified, not contradicted |
| boss-rewards.csv | no row | - | - |
| boss-achievements | no Jakotsu trophy | trophy 36 = stat 0x84 | consistent (36 is a stat trophy) |

### 3.6 What this means for the rematch rewards policy (design s.7)
- **Drops = 1 (as the game's replay):** confirmed in mechanism: the boss drop is not a script grant, so route R gives whatever the
  native drop gives; *what* it gave is unknown until InvLog.
- **No first-clear rewards:** on a repeat replay, zero script grants (guardian spirit, skill, SetReward, game-clear reward) fired in the
  fight window or at the clear 20 s later. Consistent with the design; first-clear behaviour itself is untested (needs an uncleared mission).
- **Story flags:** a replay re-writes the fight's defeat flag (118 = 1) and the clear (52), and sets 16 area cleared-markers at the start.
  All are idempotent on a journey where 52 is cleared, so route R needs no guard. The 1C27BC4 area write is a reason to never start
  uncleared missions through R-b (R-b refuses them).
- Replay vs normal clear: nothing extra was granted and nothing was visibly withheld; the normal-clear baseline (P0-4) is still missing.

## 4. Session timeline
20:59:52 ASI 1.7.39 load | 21:01:17 Continue (`MissionStartFromSave`, Enqueue +2239A64, hub stage 0x3D5538, load 20.3 s) | 21:03:05 Battle
Scroll list | 21:03:23 replay menu | 21:03:54 confirm (F1FD80 choice 0) | 21:04:07 setup mission 52, load 31.7 s to 21:04:26 (stage 72636,
mode 2) | 21:18:53 stat trophy 36 | 21:19:02.5 load 5.0 s ends 21:19:07.547 | 21:19:05.1 placement 76781 created | 21:19:07.546
BossBattleMode | 21:19:32.373 flag 118 | 21:19:34.385 End (26.8 s) | 21:19:54.760 clear 52 x2 + return request (+22398BD) | load 39.3 s
to 21:20:34 | 21:20:35 the game reopens the replay menu (p5 = 52 preselected, p6 = 1 = slot byte directly) | 21:22:16 shrine rest load 0.8 s
| 21:22:30 exit.

## 5. Blocker for the 1.7.40 rematch build: the pump does not run in the field
`MissionLoadStep` 223AF50 is the state-5 handler of the dispatcher 5A29F0 (19DBAC0 is one `jmp` of a switch on `[rcx+4]`), not a
per-frame call. RematchLog counted ticks 1..1093 in the 9 s save load, then **2 ticks in the next 150 s** of field play (tick 1095 at
21:03:56 = the replay load) and ~890 ticks over the 15-minute mission (the loads). rematch.cpp runs all requests and its "not in game (no mission
tick)" gate from that hook, so in the field R-a and R-b requests never execute. Fix (src owner): move the pump to a per-frame game-thread
site, e.g. the PerFrameUpdater call 56529C that tour.cpp already retargets (TT1), chained. R-b itself is built on `rematch2`
(`ExecDirect`, `DirectStart = 0` default, harness + dump pins) and will work once the pump ticks.

## 6. Other findings
- The replay wrote slot 0 although slot 0's last logged bytes were `+8 = E0` (not free by the game's own test): slot 0 was freed
  between ticks unlogged (the ticks stop in the field). "A consumed slot is not cleared" (boss-rematch-build.md s.2) is unproven; R-b
  checks the slot it expects instead.
- `tour.cpp` passes `static const uint32_t params[6]` to MissionRequestById, which reads it with `movaps`: alignment is not guaranteed
  (crash risk); R-b uses `alignas(16)`.
- After the clear the game re-opened the replay menu by itself with the last mission preselected.
