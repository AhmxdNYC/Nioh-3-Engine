# Per-core soul transformation pick (human #81220, cx-pc, offline build, not tested in game)

Base: cx-pc/release-1810-prep 87dcdc89 (public 1.8.10). Branch cx-pc/formcore-pick-81220. Only Nioh3BossRematch changes.

## What it does
Each Yang core can be set to **transform** (today's mod form: hide/follow/pad block/invuln/smoke/x8 throw/OrbSkip/SealSpeed) or
**game summon** (the game's own yokai throw, exactly like the global `SoulCoreForms=0`). Default = all transform.

## Ini / menu
- `[Spirit] FormCoresOff = 5B402,30744` (hex core ids = desc[0] of the seal spawn hit, as in the log). Empty = all transform. Max 16.
- `[Spirit] FormCoreLeft / FormCoreDown` (written by the mod): last core id seen on each d-pad direction, so the next session knows
  the core at press time.
- Menu: Training > Player (same page as 'Soul core transformations'): one row per core seen ('Core 5129B (LEFT): transforms / game summon'),
  A flips, Left = game summon... (Left OFF, Right ON). Saved at once. Before any core was seen: one placeholder row. The global
  'Soul core transformations' OFF still overrides everything.
- Names: the mod has no name for a core (desc[0] is the yokai chr key, not an item id); rows show id + direction.

## How an OFF core is vanilla
All form / press-time gates (`g_scForms.load()` in 16 places: FormHide begins, FormSlot0 capture, FastThrow/RunThrow, OrbSkip, AirBlock,
HasSkill hook, attack-cancel gate patch via AirGateTick, SealSpeed, FormKey swap/appear) now read `ScOn()` = SoulCoreForms && !pickOff.
`PickPoll()` runs at the d-pad edge (HkHasSkill 0xEDC9 = the accept, and every AirGateTick frame): d-pad LEFT=slot 1, DOWN=slot 2 ->
last core seen on that slot (`g_slotCore`) -> if in FormCoresOff, `pickOff` = true for that use (cleared on the next L/D press, or 10 s
after the last press). A form already up always ends normally.
Log per press: `FormCore <id> dir=<L/D> transform=<0/1> reason=...`. Menu change: `FormCore <id>: menu -> transform=<0/1>`.

## Limits (known, by design)
1. Press-time knowledge is the direction -> last core seen. The **first use of a newly equipped core** on a direction has no known core:
   it starts as a form (press-time smoke / x8 throw run). At the spawn hit the core is learned; if it is OFF, `pickOff` turns on from there
   (form suppressed; log 'first seen at the spawn hit'). Next uses are vanilla from the press.
2. Same press: the attack-cancel gate patch is flipped by AirGateTick, so a press on an OFF core during an attack may still cancel once.
3. Swapping cores in the Onmyo Box between sessions: the first use of the new core falls back as in 1.
4. RIGHT d-pad (talismans) never changes the pick. Human-ally summons (A9F39) are also recorded as 'the core of the last pressed dir'.
5. `tests/formhide4_offline.py` could not run here (missing toolchain path); sc22_replay.py ALL OK, hosted-input harness 95 checks pass.

## In-game test (pc)
1. `FormCoresOff` empty: LEFT + DOWN each once -> `FormCore <id> dir=L transform=1 reason=core unknown...`; then `FormCoreLeft/Down` in the ini.
2. Menu Training > Player: rows list both cores; set the LEFT core OFF. LEFT press -> `FormCore 5129B dir=L transform=0`, game's own throw, no
   `FormHide=4 begin`, no `SealCover`/`OrbSkip`/`RunThrow` lines, no smoke. DOWN core still transforms.
3. Set it back ON -> transforms again from the next use. Restart the game: setting kept (ini).
4. Equip a different core on LEFT while it is OFF-listed by id: first use may transform briefly or be suppressed at the spawn hit (limit 1).

## Known limit: keyboard-only presses (#81425)
The pick reads the d-pad from the pad only (game's XInputGetState import, fallback xinput export). A seal pressed from the keyboard
shortcut is not seen, so no direction is learned/applied and the core always transforms. No cheap fix: the game's keyboard binding for
the seal shortcut is not hooked anywhere in BR (no key-state read found); needs RE of the seal-use key path first.
