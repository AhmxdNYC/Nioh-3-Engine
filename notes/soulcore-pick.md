# Per-core soul transformation picker (#81220, #81514 / #81516)

Base for #81516: cx-pc/formcore-pick-81220 367033fc (BR 9253773e). Offline implementation only; pc owns installation and live testing.

## Equipped cores before any seal press: approach 1

Training > Player lists the currently equipped Yang cores as `Core <hex ID> (Yang 1)` and `(Yang 2)` after the save/player becomes ready. No seal press is needed to discover those IDs. A flips, Left selects game summon, Right selects transform. Choices remain keyed by the seal character ID in `[Spirit] FormCoresOff`, never by Yang position or direction. Identical IDs merge into one row; historical learned IDs are merged without duplicates. Names are not resolved; IDs are the same namespace as seal-spawn `desc[0]`.

A player animation tick checks readiness and equipment every 250 ms. The first ready tick schedules one worker to copy the two bounded game tables and build an item-to-character map. No process-wide scan, VirtualQuery, new hook or game function call is added. The worker publishes its copied map under a lock; the menu only reads already-published IDs. A changed table source invalidates the map and schedules a rebuild (failed attempts are limited to once per five seconds). Unknown executable builds use the fallback because the structure offsets are verified for the supported build only.

Successful equipment reads log:

`FormCore equipped: Y1=<id> Y2=<id> src=OnmyoBox+23B6B8/item+180/yang+18/seal+54`

The worker separately logs mapped-item count and elapsed milliseconds. Empty slots are 0. Unreadable pointers, invalid counts/indices, ambiguous table keys or an unmappable equipped core retain the learned/placeholder fallback instead of inventing an ID.

## Static evidence and exact read chain

- `SaveUserReadEquipSection` 0x1047F30 and writer 0x105EC60 serialize ten u32 core inventory indices at data+0x23B6B8 (AB0E9943 / 36BFE4B6).
- Preset copy 0x10872F0, lines 550-624 of the existing decompile, independently reads eight core slots from `inv = **g:Inventory` (global RVA 0x4751530), indices `inv+0x23B6B8+slot*4`, and copies each item from `inv+0x10+index*0xF0`. The u64 count is at **inv+0x927D0**, not item-array-base+0x927D0. Getter 0x553038 corroborates this. The new reader bounds count to 2500 and double-reads count/indices before publishing.
- Core-slot copy 0x553194 passes `slot > 1` to 0x55285C: slots 0/1 select core-parameter +0x18 (Yang); remaining slots select +0x1C (Yin).
- Getter 0x5549AC: resolve the item's u16 ID in DataDB+0x68 (item master, stride 0x1A0), then resolve item+0x180 in DataDB+0x118 (core parameters, stride 0x54). 0x554BE0 confirms the latter stride. Item IDs are u16 at row+0x152; core-parameter keys are u32 at row+0x0C.
- 0x55285C returns the Yang spell item ID from core-parameter+0x18. Resolve that ID in the item master; its u32 +0x54 is the summoned character ID. Item categories/types and nonempty IDs are checked at every step.
- Offline production mapper over extracted files maps **117 of 119** core item rows. Two rows lack a summon/parameter mapping and are excluded. Verified examples: Ibaraki EB60 -> parameter 59C4 -> seal 9C04 -> **5129B**; Maelstrom BD34 -> D619 -> FC9F -> **A2ACB**; Yasha DFFB -> D51A -> C09D -> **5B402**. These match observed spawn IDs in Ally #81149. No such ID table exists in production code.
- Extracted item table: archive_03/2E/D4E1DB3B... (3362 x 416 B). Core-parameter table: archive_03/23/DAC95BA0146EC13EC2F5D25530800A24F092D403B890FE53419274CEACCBAF (84 x 84 B). Both have an eight-byte header with row count at +4. Local Opus static review independently found the same three-table join; runtime table slot/getter evidence was completed by this worker.

## Learned / pre-use fallback

If equipment is not yet available, two selectable rows exist immediately: `LEFT seal (not used yet)` and `DOWN seal (not used yet)`. A pending choice is saved in `[Spirit] FormCorePendingLeft/Down` (0 summon, 1 transform; absent = no override). It applies before the first spawn, moves into the ordinary per-ID preference at the first accepted spawn, and is deleted from the direction key. It never stays attached to a direction for subsequent cores. Pending rows remain visible if the equipment map becomes ready before their first use.

`FormCoreLeft/Down` retain last-used IDs for press-time behavior, but cached IDs do not masquerade as currently equipped in the menu. Learned fallback rows explicitly say `last used`. The OFF list retains its existing capacity of 16; a full list reports rejection instead of silently retaining a permanent direction override.

## Scope and remaining behavior

- `[Spirit] SoulCoreForms=0` still overrides all per-core choices; an existing form ends normally.
- Press-time gating still uses the last-seen direction-to-core cache. The equipped reader deliberately does not equate Yang 1/2 with LEFT/DOWN: shortcuts are assignable. A newly assigned core may run press-time effects before its spawn supplies the definitive ID. The new equipment menu fixes discovery, not this older shortcut-identification limitation.
- Keyboard-only shortcut presses are not detected by the existing pad-based learning path. This remains the #81425 limitation.
- Learning keeps the previous safeguards: no recent LEFT/DOWN press means no learned row; only the first spawn after a press may learn, so secondary spawn IDs do not replace the core. This is inherited behavior, not a new proof that every first spawn is a seal.
- Structural reads and table copying fail closed, but only live testing can establish readiness timing, real save/load transitions and visible game behavior.

## Validation and live acceptance

Release ASI builds. `tests/form_core_pick_offline.py`: **103 checks**, compiling actual picker and menu with fake pad/in-memory INI; delayed map fallback, two equipped rows without pressing, ID toggles, pending persistence, migration, swaps, duplicates, capacity failure, bounded history and global OFF pass. `tests/form_core_equipment_offline.py --game-data <existing data/extract/lfm>`: **36 checks** of the production mapper/index reader, including corrupt/ambiguous tables, invalid indices, empty slots and torn reads; all 117 extracted mappings and three observed IDs verified. Existing rematch harness: **357 checks, 0 failed**; sc22 replay passes. None is a game test or a performance measurement.

pc/pc2 acceptance: fresh launch and load save; do not press any seal. Open Training > Player and confirm both equipped Yang rows (Ally example 5129B/A2ACB), plus the matching `FormCore equipped` log. Toggle one core, use both, and check independent behavior. Change the Onmyo Box at a shrine and confirm rows update without seal use; exchange positions and ensure preferences follow the ID. Restart and verify preferences; check fallback with unreadable/unsupported data only in a controlled test. Capture the actual menu. Source and artifact are for pc's live validation; cx-pc did not install, launch, stop or otherwise touch the game.
