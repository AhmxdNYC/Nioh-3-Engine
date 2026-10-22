# Yoritomo (chr 16102): hits per move and what makes a move unblockable (#22305/#22306)

2026-10-21, offline only. The game (PID 26532) was not touched. No installed file was changed.
Builds on `notes/yoritomo-hit-source-22305.md` (same request, earlier today: the unblockable code chain) and Grok's notes in
`nioh3-sort-20999/notes/yoritomo-attack-values.md` (hitbox table, which rows each action owns). Those are not repeated here.
Data: archive_01 `FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54`. Exe: `re/Nioh3.dump.exe` (file offset = RVA).
Table made by `py -I tools/yoritomo_move_facts.py <file> --check` -> `notes/yoritomo-move-facts.tsv` (94 actions).

## Answer in short
- **Unblockable = action row u16 `+0x04` bit `0x2000`** (qword `+0x00` bit 45). It is set on 19 Yoritomo moves, including `0x4E6E`.
  The other note proves the code path: this bit becomes the hit's "great attack" flag (`Collision::Damage::Hit::IsAttackGreatAttack`).
- **Hit count = the number of hit windows in the move's hitbox rows.** Each hitbox row stores a start frame, and an end frame or a length.
  The game reads these every tick (proven in code below). `0x4E6E` has **2 windows: frames 96-107 and 143-158**, which is the user's 2 hits.
  `0x4E68` has 3 (52, 93, 113). `0x4E6C` has 2. `0x4E74` has 1 element window plus 2 spawned objects.
- The method works for every boss file with this layout (checked on 3 other bosses). It gives no count for moves that own no hitbox
  rows (34 Yoritomo actions, including the multi-hitter `0x4EA5`). See "Still unknown".

## 1. Proven

### 1a. Hitbox rows are timed by motion frame (new; corrects "nothing walks the slots")
- **RVA `0x35A220`** walks **every** hitbox slot of the current action on each tick. `R = [state+0xE8]`. The count is `word [R+0x4A]` (`0x35A388`).
  The loop is `r13d = 0 .. count-1` (`0x35A780` / `0x35A794`). Each row is `0x2E98F0(R+0x40, slot)`, the row lookup Grok already found.
- A row with `word [row+0x08] == 0xFFFF` is skipped (`0x35A3DF`). Otherwise `float(word [row+0x08])` is compared with the motion's current
  frame (vcall `+0xB0`) and the frame step `[state+0x40]` (`0x35A3F4..0x35A41E`, again at `0x35A5C7..0x35A617`). So **`+0x08` = start frame**.
- **RVA `0x907FC0`** returns the row's end frame: `word [+0x0A]` if it is not `0xFFFF`, else `word [+0x08] + word [+0x0C]`. So **`+0x0A` = end frame,
  `+0x0C` = length**. `0x35A4A4` compares that end frame with the start frame.
- Data check. Grok had read `+0x08` as a bone id. On `0x4E8E` the six rows start at frames 52, 53, 54, 55, 56, 57, each 5 frames long. That is
  one capsule sweeping through a swing, not six bones. `0x4E64`'s rows sit in two clusters, 120-123 and 167-170.
- `0x4E6C`/`0x4E6E`'s windows (96-107, 143-158) line up with the events that every motion-1050 variant fires at `39@92+14` and `40@142+12`.

### 1b. Spawned objects own the untimed rows (new)
- Rows with start `0xFFFF` belong to spawned objects. Sub-table 7 rows (spawned by action events `20000+N`) store an action id at `+0x12` and a
  slot at `+0x15`. Every such slot points at an untimed row of that action. Example: sub7 rows 24-28 -> `0x4E7A` slots 1-5; 5-11 -> `0x4E89` slot 0.
  So those hits come from the object. How many times one object hits is not in these fields.

### 1c. Unblockable flag (summary of the other note, plus a family check)
- Code chain from `yoritomo-hit-source-22305.md`: action-start `0x4F54C4` copies row bit 45 into state bit 11 (`0x4F6155..0x4F6199`). Hit setup
  `0x2B4A3C` copies that into hit `+0x13C` bit 10. The script API `IsAttackGreatAttack` (`0x23C7EA0`) returns that bit.
  Not re-proven here. Seen again: `0x4F5F78..0x4F5F9D` posts event `0x16` when an action with the bit is followed by one without it.
  The AI condition switch `0x2E9948` can also test the bit (`0x2EA0CB`).
- On Yoritomo the flag picks exactly **one or two variants per motion family**. 1000: 4E50/51/90/91. 1010: 4E57/58/97/98. 1040: 4E6A/6B/AA/AB.
  1050: 4E6E/6F/AE/AF. 1081: 4E72/73/B7. The variants without it (4E6C, 4E68 ...) have hitbox rows of the same shape. `0x4E6E` is
  `0x4E6C` with more power (50/55 instead of 42/47).
- 18 of those 19 also fire common events **10250, 10251, 10254, 10255** (10254 at frame 0, the others at the end). No Yoritomo move without
  the flag fires them. Over all 374 family files: flagged rows fire them 365 of 552 times; unflagged rows fire them 50 of 40,707 times.
  This looks like the warning glow / sound of an unblockable attack. It is a guess: the common-event table is not decoded.
- The same bit 45 on a **hitbox** row (1,271 rows family-wide) is a different flag. It almost never comes with the action flag (1 row).
  It picks the parameter `+0x744` instead of `+0x740` (`0x35AA19`, `0x2B56CF`). Do not use it as "unblockable".

## 2. Not proven (stated as such in the tool)
- **One window = one hit.** The windows are proven to be the frames when a hitbox is live. That a live window hits the player once is
  an assumption. The live 0x4E6E HP drops in the other note (447 and 491, ratio 1.098, power ratio 1.100) fit one hit per window.
- Merge rule: rows with the same group byte `+0xA5` that overlap or touch merge into one window. So do group-0 rows with the same power.
  The game does not name `+0xA5`. It fits all checked moves: 0x4E99 = 2 = its motion sibling 0x4E59, and 0x4E92 = 2 = 0x4E52.
- 2026-10-22 update (proven part): `+0xA5` is the hit group, read by getter `0x2B5B98` (`0xFF` -> row bit 17 + 2 x bit 18). `0x78EADF..0x78EB74`
  keys every slot by (start frame, hit group), keeps the max power per key and sums the keys, so rows with the same start + group are one hit.
  The tool now applies that first (Gaki Chief 0x4E27 14 -> 7, Tsurara-Onna 0x4E67 9 -> 5; Yoritomo unchanged). Details:
  `notes/all-bosses-move-facts-coverage.md` section 3.
- Guard side: no code was found that says "cannot guard" when bit 10 is set. Unproven: it could instead mean "breaks guard" or "big Ki damage".
  It is the only static flag that separates 0x4E6E from 0x4E6C.

## 3. Still unknown
- **34 moves own 0 hitbox rows** (`hits` = `?`): for example 0x4E6F, 0x4E6A/6B, 0x4E50/51, 0x4EA5, 0x4E54, 0x4E7B, 0x4E82-86. A command can ask the slot
  lookup `0x5C9348` for another action's rows. These moves probably use a sibling's rows (0x4E6F -> 0x4E6E), but no field says which. A Training
  sort should either show "?" or copy the count from the variant with the same motion number that has rows (a heuristic, shown as such).
- Hits from a spawned object (`spawns` column): the count is per object and not in the file.
- `0x4E7C` / `0x4EA7`: 1 timed row with power 0 and element 0 at frame 30. It may be a grab or push check. Not counted as a hit.

## 4. Training-mode algorithm (what the tool does)
1. The action row says attack (`+0x13C`/`+0x142` set) and unblockable (`+0x04 & 0x2000`).
2. Owned hitbox rows: sub-table 0 packed records -> sub-table 2 rows.
3. Keep rows with a start frame and power or element > 0. Window = `[+0x08, end)`. Merge as in section 2. **Hits = number of windows.**
4. Spawns = events `20000+N`. Label them "+ projectile" and give no number.
5. No rows and no spawns -> "?" (or borrow from a sibling with the same motion, marked as a guess).

## 5. Table (Yoritomo, 94 actions; 0x4E21..0x4EB9 and 0x18D1 that are attacks or own hitboxes)
`hits`: `?` = no own hitbox rows. `+ spawn` = the move also creates objects with their own hits. Power/element = row `+0x10`/`+0x12`.

| action | motion | hits | unblockable | hit windows (start-end frame, power/element) | spawns (sub7 row@frame) | how derived |
|---|---:|---:|:-:|---|---|---|
| 0x4E42 | 6910 | 1 + spawn | no | 87-92(20/10) | 0@64 | hitbox windows + spawned objects |
| 0x4E44 | 6910 | 1 + spawn | no | 87-92(20/10) | 0@64 | hitbox windows + spawned objects |
| 0x4E4E | 1000 | 1 | no | 52-59(40/0) | - | hitbox windows |
| 0x4E4F | 1000 | ? | no | - | - | no own hitbox rows |
| 0x4E50 | 1000 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E51 | 1000 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E8E | 1000 | 1 | no | 52-62(30/10) | - | hitbox windows |
| 0x4E8F | 1000 | ? | no | - | - | no own hitbox rows |
| 0x4E90 | 1000 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E91 | 1000 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E52 | 1001 | 2 | no | 54-59(30/0) 73-81(42/0) | - | hitbox windows |
| 0x4E53 | 1001 | 2 | no | 54-59(30/0) 73-81(42/0) | - | hitbox windows |
| 0x4E54 | 1001 | ? | no | - | - | no own hitbox rows |
| 0x4E92 | 1001 | 2 | no | 54-70(20/10) 70-84(28/14) | - | hitbox windows |
| 0x4E93 | 1001 | 2 | no | 54-70(20/12) 70-84(32/18) | - | hitbox windows |
| 0x4E5B | 1001 | ? | no | - | - | no own hitbox rows |
| 0x4E55 | 1010 | 1 | no | 72-80(50/0) | - | hitbox windows |
| 0x4E56 | 1010 | ? | no | - | - | no own hitbox rows |
| 0x4E57 | 1010 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E58 | 1010 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E95 | 1010 | 1 | no | 72-80(45/9) | - | hitbox windows |
| 0x4E96 | 1010 | ? | no | - | - | no own hitbox rows |
| 0x4E97 | 1010 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E98 | 1010 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E59 | 1011 | 2 | no | 88-96(35/0) 104-110(36/0) | - | hitbox windows |
| 0x4E5A | 1011 | 2 | no | 88-96(35/0) 104-110(36/0) | - | hitbox windows |
| 0x4E5E | 1011 | ? | no | - | - | no own hitbox rows |
| 0x4E99 | 1011 | 2 | no | 90-101(30/5) 105-119(30/6) | - | hitbox windows |
| 0x4E9A | 1011 | 2 | no | 90-101(30/5) 105-119(30/6) | - | hitbox windows |
| 0x4E5F | 1011 | ? | no | - | - | no own hitbox rows |
| 0x4E5C | 1020 | 1 | no | 97-107(28/0) | - | hitbox windows |
| 0x4E5D | 1020 | ? | no | - | - | no own hitbox rows |
| 0x4E9C | 1020 | 1 | no | 97-108(21/7) | - | hitbox windows |
| 0x4E9D | 1020 | ? | no | - | - | no own hitbox rows |
| 0x4E60 | 1021 | 1 | no | 27-33(30/0) | - | hitbox windows |
| 0x4EA0 | 1021 | 1 | no | 27-36(24/6) | - | hitbox windows |
| 0x4E61 | 1020 | 1 | no | 97-107(30/0) | - | hitbox windows |
| 0x4EA1 | 1020 | 1 | no | 97-108(24/6) | - | hitbox windows |
| 0x4E62 | 1022 | 1 | no | 36-47(41/0) | - | hitbox windows |
| 0x4EA2 | 1022 | 1 | no | 36-47(31/10) | - | hitbox windows |
| 0x4E64 | 1030 | 2 | no | 120-127(40/0) 167-174(49/0) | - | hitbox windows |
| 0x4E65 | 1030 | ? | no | - | - | no own hitbox rows |
| 0x4EA4 | 1030 | 2 | no | 120-127(30/10) 167-174(37/12) | - | hitbox windows |
| 0x4EA5 | 1030 | ? | no | - | - | no own hitbox rows |
| 0x4E63 | 1030 | 1 | no | 167-174(38/13) | - | hitbox windows |
| 0x4E68 | 1040 | 3 | no | 52-63(30/0) 93-101(25/0) 113-128(40/0) | - | hitbox windows |
| 0x4E69 | 1040 | 2 | no | 93-101(25/0) 113-128(40/0) | - | hitbox windows |
| 0x4E6A | 1040 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E6B | 1040 | ? | **yes** | - | - | no own hitbox rows |
| 0x4EA8 | 1040 | 3 | no | 52-71(16/14) 93-111(18/7) 113-137(25/15) | - | hitbox windows |
| 0x4EA9 | 1040 | 2 | no | 93-111(18/7) 113-137(25/15) | - | hitbox windows |
| 0x4EAA | 1040 | ? | **yes** | - | - | no own hitbox rows |
| 0x4EAB | 1040 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E6C | 1050 | 2 | no | 96-107(42/0) 143-158(47/0) | - | hitbox windows |
| 0x4E6D | 1050 | 2 | no | 96-107(42/0) 143-158(47/0) | - | hitbox windows |
| 0x4E6E | 1050 | 2 | **yes** | 96-107(50/0) 143-158(55/0) | - | hitbox windows |
| 0x4E6F | 1050 | ? | **yes** | - | - | no own hitbox rows |
| 0x4EAC | 1050 | 2 | no | 96-107(34/8) 143-158(37/10) | - | hitbox windows |
| 0x4EAD | 1050 | 2 | no | 96-107(34/8) 143-158(37/10) | - | hitbox windows |
| 0x4EAE | 1050 | 2 | **yes** | 96-107(34/16) 143-158(40/20) | - | hitbox windows |
| 0x4EAF | 1050 | ? | **yes** | - | - | no own hitbox rows |
| 0x4E72 | 1081 | 3 + spawn | **yes** | 58-61(35/0) 84-89(23/0) 131-141(35/15) | 32@135 49@136 | hitbox windows + spawned objects |
| 0x4E73 | 1081 | 0 + spawn | **yes** | - | 32@135 | spawned objects |
| 0x4E74 | 1090 | 1 + spawn | no | 227-252(0/90) | 1@226 2@226 | hitbox windows + spawned objects |
| 0x4E75 | 1090 | 1 + spawn | no | 227-252(0/90) | 1@226 2@226 | hitbox windows + spawned objects |
| 0x4E76 | 1100 | ? | no | - | - | no own hitbox rows |
| 0x4E7D | 1100 | ? | no | - | - | no own hitbox rows |
| 0x4E78 | 1102 | 0 + spawn | no | - | 24@88 | spawned objects |
| 0x4E79 | 1103 | 0 + spawn | no | - | 25@58 | spawned objects |
| 0x4E7A | 1104 | 0 + spawn | no | - | - | spawned objects |
| 0x4E8D | 1104 | 1 + spawn | no | 60-73(0/90) | 3@57 4@57 | hitbox windows + spawned objects |
| 0x4E7B | 1060 | 0 + spawn | no | - | 33@16 | spawned objects |
| 0x4EA6 | 1070 | ? | no | - | - | no own hitbox rows |
| 0x4EA7 | 1062 | 0 (power 0) | no | - | - | zero-power rows only |
| 0x4E7C | 1062 | 0 (power 0) | no | - | - | zero-power rows only |
| 0x18D1 | 1061 | 3 | no | 0-1(15/0) 47-48(30/0) 119-120(45/0) | - | hitbox windows |
| 0x4E81 | 1070 | ? | no | - | - | no own hitbox rows |
| 0x4E82 | 1070 | ? | no | - | - | no own hitbox rows |
| 0x4E83 | 1071 | 0 + spawn | no | - | 10@39 | spawned objects |
| 0x4E84 | 1071 | 0 + spawn | no | - | 10@39 | spawned objects |
| 0x4E85 | 1072 | ? | no | - | - | no own hitbox rows |
| 0x4E86 | 1072 | ? | no | - | - | no own hitbox rows |
| 0x4E87 | 1073 | 0 + spawn | no | - | 11@39 | spawned objects |
| 0x4E88 | 1073 | 0 + spawn | no | - | 11@39 | spawned objects |
| 0x4E89 | 1000 | 0 + spawn | no | - | 5@54 | spawned objects |
| 0x4E8A | 1001 | 0 + spawn | no | - | 6@55 7@77 | spawned objects |
| 0x4E8B | 1011 | 0 + spawn | no | - | 8@92 9@107 | spawned objects |
| 0x4EB1 | 1000 | 0 + spawn | no | - | 34@54 | spawned objects |
| 0x4EB2 | 1000 | 0 + spawn | no | - | 35@54 36@144 | spawned objects |
| 0x4EB3 | 1000 | 0 + spawn | no | - | 37@54 | spawned objects |
| 0x4EB4 | 1000 | 0 + spawn | no | - | 38@54 39@54 40@54 41@54 | spawned objects |
| 0x4EB7 | 1081 | 3 + spawn | **yes** | 58-61(35/0) 84-89(23/0) 131-141(35/15) | 32@135 49@136 | hitbox windows + spawned objects |
| 0x4EB8 | 1104 | ? | no | - | - | no own hitbox rows |
| 0x4EB9 | 1104 | 0 + spawn | no | - | 50@56 | spawned objects |
