# Site reports 7 and 8: arena safety

Task: bridge #65065, base `gxl-pc/release-185`. Offline candidate only; pc/3 owns game verification. No install, game control, or publish performed by cx-pc.

## Report 7: inventory disappears (REDONE by pc/1, bridge #65470 / human #65467)

cx-pc's first fix (1f92c3ae: the 10 sandbox rows became summon-here, no arena trip) was REJECTED: players want to fight those bosses
in their own arena with their own gear. Those 10 rows (RC1D9 Kunimatsu tutorial, 9 Yagyu/Hanzo/Fuku dojo rows) are real arena trips
again (TSV, arena_scan.h, auto_arena.py back to the base; Training's GuardInventoryArena removed).

Cause: the sandbox mission scripts call `Item::ReserveAllItemAll/Dojo/Edo` (the game stashes every item: `if !IsReserved(inv) Reserve(inv, mode 1/2/3)`),
`Item::ClearEquipment` (`215F8F4(equip, a, b, 1)`), then give their own kit (`AddEquipment`, `AddEquipmentBySlot`, `AddEquipmentPreset`,
`AddFixItem`, `AddItemNoNotice`, `AddMagicItem`, `SetShortCut`, `Player::EquipNinjutsu`, `Player::AddSkill(Dojo)`). The mission's own
`Item::ResetReserveItem` (restore) only runs at the tutorial's normal end, which a mod trip never reaches.

Fix (pc/arena-keep-gear):
- Training: `Nioh3Training_KeepGear()` = 1 from a Fight now until Return (RhDone), and only while the player is still in that trip's
  mission (or the switch is still loading, 60 s grace). Log `KEEPGEAR trip 0x....`.
- BossRematch `keepgear.cpp` + `hooks.cpp InstallVeto`: the entry thunk can now skip a script native. Strip natives (Reserve*, ClearEquipment)
  are skipped during any trip; the first one marks the trip a sandbox trip, after which the kit natives are skipped too (a normal boss
  mission's own script rewards stay untouched). Skipping runs the binding's own argument reader (ctor 0x139938) and result writer
  (fin 0x1181730 / 0x11803D0) and returns its u16 result, so the script VM stack stays balanced. ctor/fin are found per wrapper by
  instruction shape (`mov rdx,rcx; lea rcx,[ctx]; call` / `movzx ebx,word [ctx+0x10C]; lea rcx,[ctx+0x110]; call`), verified on all
  16 wrappers from the v2.02 dump. Sites resolve by script-native name (survives game patches); unknown layout = not guarded (logged).
- Safety net (BR pump): if the game's reserve flag went 0 -> 1 during a trip anyway, the game's own restore (`21796A8([4751860])`, parsed
  from `Item::ResetReserveItem`) runs when the trip ends. Log `KeepGear: SAFETY NET`.
- `[Rematch] KeepGearInArenas = 1` (default; 0 = game behaviour). Generic: decided by what the script calls; no boss/mission lists.
- Not touched: `Player::SetStoryProgress`, `SetRetryPointIDAndSave` in the tutorial script (not gear; watch in the test).

Offline: `mod/Nioh3BossRematch/tests/run_keepgear_veto.cmd` PASS (real thunk: pass-through, skip keeps ctor/fin/return word, shared
entry handler, non-wrapper refused); Training `tests/arena_safety_test.py` (10 rows are trips, one Ibaraki row) + runtime test PASS.

## Report 8: two Ibaraki Doji

The TSV has exactly one Ibaraki row, REA75 (character 894462, native placement 364374). The mission script's arena-entry trigger references that native placement. `ArenaTick` previously unconditionally issued `CreateDynamic` after entering the arena and `StartArena` cleaned only mod-owned actors. That permits a native boss and a mod boss together; this is the identified code path, pending reproduction/verification by pc/3.

The new guard checks the selected arena's exact native placement wrapper, its placement/character record, current world, and live actor membership. It deletes that native duplicate before creating the mod boss, waits for deferred deletion, and checks for later native spawns through the arena session. It never deletes a player or an actor already registered as mod-owned. The selected native placement is excluded from dynamic owner selection/owner activation. Intentional Boss A/B summons are preserved. Stopping or starting a summon-here session clears the arena placement guard. Summon roster loading also deduplicates identical mission/character entries.

No Ibaraki data row changed and no boss-specific runtime list was introduced.

## Offline verification

- `py mod/Nioh3Training/tests/arena_safety_test.py`: 4 tests passed; actual installed game archives used; 131/131 fresh C/Python scanner rows agree. Confirms all 10 sandbox rows, both safe Kunimatsu variants, and exactly one Ibaraki row.
- `py mod/Nioh3Training/tests/arena_safety_runtime_test.py`: actual C guard bodies compiled and passed against mocked game state. Covers roster dedupe, scan failure/cache fallback, summon failure, existing/deferred/late native duplicate deletion, explicit slots, wrong world/character, player, and raw summon guards.
- Windows Training shared-library build uses the existing Zig flags; artifact is local and not installed. No performance comparison was run.

## pc/3 verification

1. Back up saves and check RC1D9 Fight now from a normal world: summon here, no tutorial travel or equipment change. Repeat with AutoArena disabled and the old TSV, and with arena swapping disabled.
2. Check a dojo row follows the same rule and both safe Kunimatsu variants still enter their own arenas.
3. Enter Ibaraki's arena from another world and from the same world: exactly one fight boss after streaming settles. Check `ARENA dedupe` logs, normal aggression, no premature mission completion/rewards, retry after loss, and Return.
4. Intentionally summon the same character into Boss A/B: both deliberate summons must remain. Check a different boss arena as the generic-policy control.
