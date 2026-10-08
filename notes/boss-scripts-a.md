# Boss scripts A: deep decode of rows 1-52 (2026-10-08, static)

Companion to `notesoss-fights.md` section 7. CSV: `notesoss-scripts-a.csv` (script, imports, calls_with_args, constants, candidate_boss_selector). Decoder: throwaway, same opcode table as the note plus three additions found here (below). All 52 scripts decode linearly to EOF (0 desync).

## Format additions

- Symbol operand of 0x1b/0x1f/0x3b is the sym **id** (field 2 of the `{nameIdx,id,kind}` record); name = `names[nameIdx]`. Sym index != id after the first block.
- 0x2d = push float32 (4 B). 0x3a = u16 sym (rare, 15 uses). 0x2c n,n*i32 = int-vector constant (e.g. placement struct `I[20002 6 <placementId> <flag>]` used by `Gadget::SetReplacementStatusPlacement`); 0x2e n,n*f32 = float vector (world position). 0x15/0x16 = push false/true (0x2a/0x2b = other small constants); 0x03 precedes call frames and is skipped in arg recovery.
- Code starts at the first `0x35 u16 n` dispatch table after the string pool (n = sym count + 1). Functions are `0x3b sym` markers. The first ~190 functions are the SHARED library (`Time::*`, `Function::*`, `Hell::*`, ...: identical text across scripts); the script's own logic is only the trailing functions whose names have no `::` (`mission_setup`, `mission_end`, `script_main`, `sub_*`, `boss_id`, ...). Arg recovery = the run of push ops before a call (nested-call results show as the preceding `FCALL fn` in the ordered call list).

## Findings

1. **Two families.** 24 of 52 scripts are the trivial duel skeleton: user functions are only `mission_setup`, `mission_end`, `script_main` (+ maybe `fall_prevent_*`), and `script_main` = `Mission::WaitUpdateSetup; Function::DuelCommonFlow(); Mission::EndLoop` with **no arguments**. The boss is therefore NOT named in the script: it comes from data (stage placement / `Duel::GetBossPlacementID` inside the shared flow).
2. **The only per-script value in the trivial family** is the gadget id in `mission_setup`/`mission_end`: `Gadget::SetReplacementStatus(<gadgetId int32>, 50)` on setup and `(…, 61)` on end (e.g. 874591CC96D6 = 1605534040, 4073BB6CD7BF = 1247651686, 4A9D7FC0C78E = -36258476). Unique per script in rows 1-52 apart from shared S00_EDO teacher gadgets. It is a gadget/placement id (not a KIDS hash: none of ~950 int constants matched kidshash, FNV, djb2 against Nioh3.dump.exe identifiers, all LFM pool strings and stage_names.tsv). It is the best static discriminator for those scripts, and a candidate key for the data-side lookup, but it selects the arena gadget, not the boss directly. Header, name table and library text are identical.
3. **Boss ids as constants (second family).** Many scripts pass a placement id straight to `Mission::BossBattleMode(<id>)` / `Function::CreateClearGadgetBossPosition(<type 10|11>, <id>)` (e.g. 63742543AEB8: 4075848; 663667288B5D: 572110; C33DF13FF6D2: 353508; D38178C9B28E: 4097963; F5C49D3F2BD2: 1110441). Wave-battle scripts (`wave_battle_*`) call `Mission::BossBattleMode` once per wave boss (several ids) and `WaveBattle::RequestIDWithWhoWarpSetting(<waveId>,...)`. Others hide the id in a constant-return user function (`boss_id()`, `boss_pl_enemy_id()`, `*_pl_enemy_id()`), e.g. 397961F3ACF3 boss_id=1010621, 45C15A1FFB1A boss_pl_enemy_id=8062848, 6C05ACE2DDE5 boss_id=159068 (also BossBattleMode(150817)). Placement ids look like `<stage digit><6 digits>`; the id range tracks the stage family (1xxxxxx S01, 2xxxxxx, 3xxxxxx, 4xxxxxx S03/04, 5xxxxxx S00_EDO, 8xxxxxx hell/DLC).
4. **Indirect forms.** 0F72DF1DECEF and 8A1A6CA9DDC5 call `Duel::GetBossPlacementID()` and feed the result (local `$1.8`) to `Mission::BossBattleMode` / `CreateClearGadgetBossPosition`: the id is read at runtime from the duel manager. 8B5B6D7A96F2 uses `DuelCommonFlowAnyRange(4500.0 / 680.0)` (range floats) plus `GetBossPlacementID`. S00_EDO teacher scripts (699A2C366549, 8C8BDE81D976, 937889AF5AA9, 96A326DD1D0A) call `Function::TeacherDuelCommonFlow(I[a b 1803141834 c d flag])`: a 6-word struct whose third word (1803141834) is the same in all four and words 4-5 differ per script (30594682, 30061904 ...), i.e. a likely opponent/duel record id; `mission_setup` gadget 1302396295 (arg 120) is shared by three of them, 539105804 (arg 50) by 0AD2EFD76D68/23B6AC585823/699A.
5. `Gadget::SetReplacementStatusPlacement(I[20002 6 <placementId> flag])` (8A1A6CA9DDC5: 2077212; 6C05ACE2DDE5: 151367) names the arena gadget placement; 20002 is the same type word everywhere.
6. Scripts with no boss-ish call at all (33F79583DFA3, 5E68F49FC65F, E0E7C53E6980, B44697EFB296 partially) still imported BossBattleMode at table-build time; their flow is inherited from `Function::DuelCommonFlow`.
7. Tutorial/story scripts (23B11AD2E77F, 21FE10656120, A5A9F2211EC6, C34725FB97C6, C217B8C15062, 2F49D478D114, EC2CEC687176) are long mission scripts (37-172 user fns): boss selection there is a `*_pl_enemy_id()` / `Character::Create(<id>)` family per encounter plus `WaveBattle::IsActiveID/RequestID` ids; `boss_id` stays per-function.

## Candidate selector summary (per script)

| script | sym | selector candidates |
|---|---|---|
| 0F72DF1DECEF | 357 | Duel::GetBossPlacementID(); Function::CreateClearGadgetBossPosition($1.11,$1.8); Mission::BossBattleMode($1.8) |
| 17B9D0901379 | 337 | Function::DuelCommonFlow() |
| 21FE10656120 | 399 | none (data-driven) |
| 23B11AD2E77F | 412 | none (data-driven) |
| 33F79583DFA3 | 330 | none (data-driven) |
| 35A880867F1E | 349 | Mission::BossBattleMode(1160051); Mission::BossBattleMode(1161567); Mission::BossBattleMode(1162241); WaveBattle::RequestIDWithWhoWarpSetting(17668,o15,o15,o15,o15) |
| 397961F3ACF3 | 353 | const-return fns: boss_id()=1010621; Function::CreateClearGadgetBossPosition(); Mission::BossBattleMode() |
| 4073BB6CD7BF | 333 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(1247651686,50) |
| 45C15A1FFB1A | 489 | const-return fns: boss_pl_enemy_id()=8062848; Function::CreateClearGadgetBossPosition(); Mission::BossBattleMode(); WaveBattle::RequestID(1956) |
| 4A9D7FC0C78E | 351 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(-36258476,50) |
| 5E68F49FC65F | 330 | none (data-driven) |
| 63742543AEB8 | 388 | Function::CreateClearGadgetBossPosition(10,4075848); Mission::BossBattleMode(4075848); mission_setup Gadget::SetReplacementStatus(-607116677,20) |
| 663667288B5D | 588 | Function::CreateClearGadgetBossPosition(10,572110); Mission::BossBattleMode(572110); WaveBattle::RequestID(16889); WaveBattle::RequestID(24284); WaveBattle::RequestID(32468); WaveBattle::RequestID(34449); WaveBattle::RequestID(63339); WaveBattle::RequestID(65218) |
| 6C05ACE2DDE5 | 464 | const-return fns: boss_id()=159068; Mission::BossBattleMode(150817); mission_setup Gadget::SetReplacementStatusPlacement(I[20002 6 151367 2]) |
| 6F565110C482 | 359 | Mission::BossBattleMode(1291484); Mission::BossBattleMode(1292577); Mission::BossBattleMode(1299059); Mission::BossBattleMode(1299728); Mission::BossBattleMode(1299909); WaveBattle::RequestIDWithWhoWarpSetting(26930,o15,o15,o15,o15); mission_setup Gadget::SetReplacementStatus(-409207643,50) |
| 70D08AB4D345 | 482 | Mission::BossBattleMode(165758); WaveBattle::RequestID(10277) |
| 7F8686E97E9E | 315 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(-1079264977,50) |
| 874591CC96D6 | 342 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(1605534040,50) |
| 8A1A6CA9DDC5 | 336 | Duel::GetBossPlacementID(); Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatusPlacement(I[20002 6 2077212 0]) |
| 8B5B6D7A96F2 | 351 | Duel::GetBossPlacementID(); Function::DuelCommonFlowAnyRange(f4500); Function::DuelCommonFlowAnyRange(f680); mission_setup Gadget::SetReplacementStatus(1902982327,50) |
| 8B9C9260E89D | 333 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(1393551013,50) |
| 8C931009FD0E | 349 | WaveBattle::RequestIDWithWhoWarpSetting(52864,o15,o15,o15,o15); mission_setup Gadget::SetReplacementStatus(-607116677,20) |
| 8EDAAD75A6E4 | 315 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(-1704233373,50) |
| 8F45C6314FB9 | 335 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(-1978846972,50) |
| 94B9A85DF3C6 | 384 | Mission::BossBattleMode(2161177); Mission::BossBattleMode(2161257); Mission::BossBattleMode(2162272); Mission::BossBattleMode(2163067); Mission::BossBattleMode(2167800); WaveBattle::RequestIDWithWhoWarpSetting(35426,o15,o15,o15,o15) |
| 959B14897863 | 333 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(856889094,50) |
| 9B7E22C510B1 | 477 | Mission::BossBattleMode(8039332) |
| A4B3B1F1B741 | 339 | Function::CreateClearGadgetBossPosition(10,1008922); mission_setup Gadget::SetReplacementStatus(-1261995606,3) |
| B44697EFB296 | 339 | Function::DuelCommonFlow() |
| B4F138592131 | 333 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(1423159560,50) |
| B8041D275E2B | 359 | Mission::BossBattleMode(1282396); Mission::BossBattleMode(1284876); WaveBattle::RequestIDWithWhoWarpSetting(5395,o15,o15,o15,o15); mission_setup Gadget::SetReplacementStatus(262638326,50) |
| C33DF13FF6D2 | 453 | Function::CreateClearGadgetBossPosition(11,353508); Mission::BossBattleMode(353508); mission_setup Gadget::SetReplacementStatus(262638326,61) |
| C6DA46D668EC | 358 | Mission::BossBattleMode(1264363); Mission::BossBattleMode(1264386); Mission::BossBattleMode(1265215); Mission::BossBattleMode(1268341); Mission::BossBattleMode(1268863); Mission::BossBattleMode(1269826); WaveBattle::RequestIDWithWhoWarpSetting(46014,o15,o15,o15,o15) |
| C9995129CF08 | 359 | Mission::BossBattleMode(3035151); mission_setup Gadget::SetReplacementStatus(657319613,50) |
| D38178C9B28E | 448 | Function::CreateClearGadgetBossPosition(10,4097963); Mission::BossBattleMode(4092708); Mission::BossBattleMode(4097963) |
| E0E7C53E6980 | 332 | Function::DuelCommonFlow() |
| EC2CEC687176 | 415 | WaveBattle::RequestID(2884) |
| EE4550F06AA6 | 333 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(-1634462304,50) |
| F269FE4AD196 | 366 | Mission::BossBattleMode(1030798); Mission::BossBattleMode(1033534); Mission::BossBattleMode(1035190); Mission::BossBattleMode(1037353); Mission::BossBattleMode(1038159); WaveBattle::RequestIDWithWhoWarpSetting(32639,o15,o15,o15,o15) |
| F4393CE2152C | 333 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(1240177138,50) |
| F5C49D3F2BD2 | 364 | Function::CreateClearGadgetBossPosition(11,1110441); Mission::BossBattleMode(1110441); Mission::BossBattleMode(1114358); WaveBattle::RequestIDWithWhoWarpSetting(10232,o15,o15,o15,o15) |
| F9B3F190397C | 333 | Function::DuelCommonFlow(); mission_setup Gadget::SetReplacementStatus(1514097910,50) |
| C34725FB97C6 | 538 | mission_setup Gadget::SetReplacementStatus(-1448153669,5); stages H01_RETURN_NEKOMATA,H02_FIRST_NEKOMATA,H02_LAST_NEKOMATA,H02_SECON_NEKOMATA |
| A5A9F2211EC6 | 437 | const-return fns: get_middle_boss_kamaitachi_pl_enemy_id()=44271 get_under_the_bridge_pl_enemy_id()=45712 bridge_youki_pl_enemy_id()=48179 bridge_shikijirou_pl_enemy_id()=43808 farm_ENKI_pl_enemy_id()=44455 masunamiki_ENKI_pl_enemy_id()=42632; stages H02_WORLD_SENGOKU_3,S01_SEN_03_KOZOKU,S01_SEN_03_ |
| C217B8C15062 | 537 | const-return fns: baragaki_climb_01_pl_enemy_id()=4039088 baragaki_climb_02_pl_enemy_id()=4037167 baragaki_climb_03_pl_enemy_id()=4032994; Mission::BossBattleMode(4035349); Mission::BossBattleMode(4036409); Mission::BossBattleMode(4037837); WaveBattle::RequestID(31546); WaveBattle::RequestID(6985);  |
| 0AD2EFD76D68 | 399 | mission_setup Gadget::SetReplacementStatus(539105804,50); stages S00_EDO_ALL |
| 23B6AC585823 | 399 | mission_setup Gadget::SetReplacementStatus(539105804,50); stages S00_EDO_ALL |
| 2F49D478D114 | 622 | const-return fns: boss_pl_enemy_id()=555606; Mission::BossBattleMode(); WaveBattle::RequestIDWithWhoWarpSetting(26810,o16,o15,o15,o15); WaveBattle::RequestIDWithWhoWarpSetting(56968,o16,o16,o15,o15); stages S00_EDO_ALL |
| 699A2C366549 | 394 | Function::TeacherDuelCommonFlow(I[15969 65507 1803141834 32410843 33266850 1]); mission_setup Gadget::SetReplacementStatus(539105804,50); stages S00_EDO_ALL |
| 8C8BDE81D976 | 397 | Function::TeacherDuelCommonFlow(I[25206 36000 1803141834 30813022 30594682 0]); mission_setup Gadget::SetReplacementStatus(1302396295,120); stages S00_EDO_ALL |
| 937889AF5AA9 | 397 | Function::TeacherDuelCommonFlow(I[34326 283 1803141834 31626974 30061904 0]); mission_setup Gadget::SetReplacementStatus(1302396295,120); stages S00_EDO_ALL |
| 96A326DD1D0A | 399 | Function::TeacherDuelCommonFlow(I[19264 14137 1803141834 31067288 29633093 0]); mission_setup Gadget::SetReplacementStatus(1302396295,120); stages S00_EDO_ALL |

## Caveats

- Arg recovery is heuristic (push-run before the call); calls whose arguments come from nested expressions can show fewer args. `$k.i` = local/param (kind k, slot i); `o15/o16` small bool constants. Large ints are unresolved ids, not hashes.
- 'Which boss' for the trivial family needs the data side (stage placement table keyed by the gadget id / mission id); the next step is to log `Gadget::SetReplacementStatus` + `Duel::GetBossPlacementID` live, or match these ids against `data\extract\stage` placement dumps.
