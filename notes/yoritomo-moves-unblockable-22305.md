# Yoritomo (chr 16102): what makes 0x4E6E "unblockable", and what can sort the move menu (#22305)

2026-10-21, offline only. Game left running and not touched (no attach, no memory read/write, no launch/close).
Sources: the action file `data/extract/lfm/archive_01/FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54`
(nioh3-engine checkout), `re/Nioh3.dump.exe` (capstone disassembly, RVAs below), and the sorter CSV
`nioh3-sort-20999/notes/yoritomo-action-fields.csv`. Builds on `nioh3-sort-20999/notes/yoritomo-attack-values.md`,
`yoritomo-action-fields.md`, `yoritomo-move-table-search.md`, `.handoff/yoritomo-training-stop-21839.md`.

Live log check (read only): `Nioh3Training.log` at 11:09 shows `DRILL chr 16102 forced 0x4E6E` every ~5.5 s (#35..#41),
boss HP 38222/42525, player HP 2658/2658. No per-hit or guard line exists in the log.

## B. The red / "unblockable" flag: PROVEN as the engine's **GreatAttack** flag

The action-row **u16 at +0x04, bit 0x2000** (the CSV column `flags04`) is what the game turns into "this attack is a
GreatAttack". The chain, all in `Nioh3.dump.exe`:

1. **Action start, RVA 0x4F6155..0x4F6199** (function 0x4F54C4): loads the current action object `[chr+0xE8]`, its file-row
   pointer `[+0x20]`, and tests `qword [row+0x00] & 0x200000000000` (bit 45 = u16 `+0x04` bit 0x2000). If set, it calls the
   bitset setter `0xFC1A20(chr+0x1060, 11, 1)`. If clear, it calls the same with value 0. So character state bit 11 =
   "current action has +0x04 bit 0x2000".
2. **Flag query 0x75**: `0x4893B8` → `0x2E9948` switch case `0x75` (at `0x2EABAF` → `0x2E9A5E`) returns
   `([chr+0x1060] >> 11) & 1`. That is the same bit 11.
3. **Hit record**: in the attack-hit builder `0x2B4A3C`, `0x2B4FF6 call 0x4893B8(attacker, 0x75)` → `0x2B4FFF bts [hit+0x13C], 10`
   (second site `0x2B50B0` for the owner of a spawned object, `[r13+0x3B0]`).
4. **Name**: the script function `Collision::Damage::Hit::IsAttackGreatAttack` (name ptr at file 0x397A980, code `0x23C7EA0`)
   returns `[hit+0x13C] & 0x400`. Neighbours: `IsAttackPowerAttack` = `byte [hit+0x13C] & 2`,
   `IsAttackCanCounterGreatAttack` (`0x23C7C1C`) is a stub that always returns 0 in this build.
5. **Consumers**: `0x786898` (called from the hit resolve `0x326C94` at `0x32742D`, and from `0x78CE8C` at `0x795C9F`) returns
   true only when the attacker has flag 0x75 AND the defender's current state id (`0x787354`/`0x786908`) is 0x5E or 0x5F. That
   result is a "GreatAttack was countered" bool stored at `[rbp+0x30]` and tested at `0x327A71` / `0x327D2D` in the resolve.
   `0x488378` scans for a nearby character with flag 0x75 (a tracker/notice). `0x1A62920` copies the hit bit into a record.

**0x4E6E result (PROVEN from file bytes):** `+0x04 = 0x2200` → bit 0x2000 **set** → GreatAttack. Its blockable neighbours on the
same motion are clear: 0x4E6C `0x0100`, 0x4E6D `0x0100`, 0x4E68 `0x0100`, 0x4E69 `0x0100`, 0x4E74 `0x0100`.

Yoritomo has **19 GreatAttack attack rows** (of 88 attack rows; motion → ids):
1000: 4E50 4E51 4E90 4E91 · 1010: 4E57 4E58 4E97 4E98 · 1040: 4E6A 4E6B 4EAA 4EAB · 1050: **4E6E** 4E6F 4EAE 4EAF ·
1081: 4E72 4E73 4EB7. Every one is a variant that shares its motion with ordinary rows, except motion 1081 (all great).

Supporting data (PROVEN numbers, meaning GUESSED):
- 18 of the 19 carry the common events **10250, 10251, 10254, 10255** in their event slots; no other Yoritomo row has 10254/10255.
  0x4E6E: 10254@0, 10255@163, 10250/10251@180. Guess: the red-flash VFX/SE. Family-wide (374 action files): 84 attack rows carry
  10254 and 79 of them have bit 0x2000; 148 attack rows in 37 files have bit 0x2000.
- Hitbox rows (sub-table 2) of 0x4E6E vs 0x4E6C have identical shapes/bones; only values change: `+0x10` 42→50 and 47→55,
  `+0x1E` 175→320 and 200→368, `+0x1C` 100/53→96 and 60→110. Family-wide, rows owned by bit-0x2000 attacks have about twice the
  values of ordinary attacks (medians `+0x10` 50 vs 25, `+0x1C` 96 vs 50, `+0x1E` 250 vs 120; 455 vs 4555 rows, 311 files).
- 0x4E6E sched_a starts `125@0-52` (ordinary 0x4E6C: `80@0`), so its wind-up runs on a different speed curve.

**Not proven:** that GreatAttack means "cannot be guarded". The hit-bit `+0x13C & 0x400` has only 2 readers (script getter
and the record copy at 0x1A62920); no guard test on it was found. The guard/Ki branch of the resolve `0x326C94` reads the
"countered" bool, but the guard-break path itself was not traced. So: "0x4E6E is the engine's GreatAttack (red/burst-style)
variant of the 0x4E6C swing" is PROVEN; "GreatAttack is why the guard fails" is a strong GUESS (Nioh red attacks behave that
way, and the in-game observation matches), not a code proof. The older +0x115 34/54 split is unrelated (not read by attack code).

## A. Source data for an automatic move menu

PROVEN columns (all from the one action file; generalises to every boss file with this layout):
| purpose | field | note |
|---|---|---|
| is an attack | `+0x13C` or `+0x142` != FFFF (`has_attack_block`) | 88/173 |
| same move, different variant | motion `+0x20` | 88 attacks → 20 motions |
| red / GreatAttack variant | `+0x04` & 0x2000 | 19 rows; see above |
| automatic follow-on | next action `+0xE0` | 2 = idle; chains 4E21→4E22 etc. |
| AI follow-up choices | private transition rows (`yoritomo-attack-values.md`, 7:41-8:25 pm sections) | candidates, not forced strings |
| hitbox rows owned | sub-table 0 packed record count, rows = `running_sum + slot` | 0x4E68 3, 0x4E6C 5, **0x4E6E 5**, 0x4E74 5; 0x4E6A/0x4E6B/0x4E6F/0x4EAF own 0 |

Suggested algorithm (uses only the proven columns): attack rows → group by motion → inside a group, mark `+0x04&0x2000` rows
"red" → chain rows by `+0xE0` (non-idle) → order groups by motion number (1000-1104 already runs in move families: 100x, 101x,
102x, 103x, 104x, 105x, 106x/107x, 108x, 109x, 110x). Example group 1050: 4E6C 4E6D 4EAC 4EAD (normal) | 4E6E 4E6F 4EAE 4EAF (red).

**Hit count: still UNKNOWN. Not proven, do not label.** Candidate only (GUESS): count of distinct attack-value groups among the
move's hitbox rows (rows sharing `+0x10/+0x1C/+0x1E` look like one swing hit by several colliders). Values: 0x4E68 3
(30, 25, 40), 0x4EA8 3, 0x4E6C 2 (42, 47), **0x4E6E 2 (50, 55)**, 0x4E74 2 (one 300/300 set + 55), 0x4E64 2, 0x4E72 3,
0x4E7A 2. The slot lookup (`0x5C9348`) uses one row per call and nothing walks the rows (earlier notes), and the one live 0x4E68
measurement (221 HP) matches none of these values, so this grouping is not a hit count until a live per-hit log confirms it.

## Still unknown
- Whether/where the guard check rejects a guard because of flag 0x75 / hit bit 0x400 (trace `0x326C94` guard branch, `[rdi+0x1E0]`,
  `[rdi+0x1E4]`, from `0x327999`).
- Real hits per move (needs a per-hit log line while drilling; the current log has none).
- Meaning of common events 10250/10251/10254/10255 (their definitions live outside this file).
- Why great variants 0x4E6A/0x4E6B (motion 1040) own no hitbox rows.
