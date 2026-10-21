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

## Hit counts and frame data (follow-up)

2026-10-21, offline only (`re/Nioh3.dump.exe` + Ghidra `re/decomp.cmd`, same action file). Game not touched. This corrects the old notes,
which said `+0x08` was a bone id and that startup/active were not in the data. They are in the data: **each hitbox row (sub-table 2,
192 B) has its own active window and hit id.**

### PROVEN: the per-tick activator reads the hitbox row's frame fields
**RVA `0x35A220`** (the action tick; `param_1[0x1d]` = runtime action, rows at `+0x40`, count `+0x4A`) loops slot 0..count-1 with
`0x2E98F0` (no single-slot lookup here, so every owned row runs). For each row:
- **`+0x08` u16 = active START frame** (motion frame). FFFF = never started by this tick. The row is activated when
  `prevFrame <= start < prevFrame + dt` (frame = vcall `+0xB0`, dt = `param_1+0x40` float). Then `0x5C9F94` → `0x2B7F30` builds the
  attack collision, and `+0x250` of the collision gets the slot.
- **`+0x0A` u16 = explicit END frame**, **`+0x0C` u16 = LENGTH in frames**: `0x907FC0` returns `+0x0A` if != FFFF, else `+0x08 + +0x0C`.
- **`+0x0F` byte = repeat interval**: if end > start and `+0x0F` > 0, `0x1FFE718` re-fires the row every `+0x0F` frames inside [start,end).
- **Collision life** (`0x2B7F30`, stored at collision `+0xC4`): `0x9A3C74(row)` = `+0x0C` frames (or `|start - +0x50| + +0x0C` when
  `+0x50` != FFFF). If row dword `+0x40` bit 0 is set the life is -1 (persistent).
- Yoritomo, all 182 rows: `+0x0A` = FFFF, `+0x0F` = 0, `+0x50` = FFFF, `+0x40` bit 0 = 0. So **each row is active for frames
  [`+0x08`, `+0x08` + `+0x0C` - 1]**, and fires once.
- The old `+0x08` "bone" values (52/93/113 on 0x4E68) are start frames. They match the 0x4E68 dope pair 52-117.

### PROVEN: how hits are counted (hit id = row byte `+0xA5`)
- Getter **`0x2B46E0`** (hit record): returns 0xC8/0xC9 if hit `+0x13C` bit 15/23 is set. Otherwise it returns `0x2B5B98(row)` = **row
  byte `+0xA5`** (0xFF becomes 0, or 2 if row qword bit 18 is set).
- **Same-frame merge `0x789844`** (called from `0x7897D0` / `0x1179282` before a pending hit is added to the 50 x 0x1C0 table at
  singleton `+0x5910`): if a pending record has the same attacker object, the same target (`+0x190`), and the same hit id, the new
  hit is merged into it (only its position is copied) and no new hit is added.
- **Target history `0x2B44C4`** (from the hit resolver `0x117A6D0` at `0x117AAF7`, and `0x1FFF76C`): it walks the target's list
  (`[target+0x3B0]+0x78`, nodes with hit `+0x20` and attacker `+0x48`). If it finds the same attacker with the same hit id, it sets
  "already hit" (`[rsp+0x40]=1`). The code that then drops the damage was not traced (strong GUESS that it does).
- So **hits per target per move = number of distinct `+0xA5` ids among the rows that start** (rows with the same id are one hit,
  whatever their count, bones or windows). AI side: `0x78CE8C` at `0x78EA90` also groups an action's rows by the key
  (`+0x08` start, `+0xA5` id) and sums max power. It is an estimate, not the hit path.

### Validation
- **0x4E6E: rows 122-124 id 1 (start 96/96/98, len 11/2/9), rows 125-126 id 2 (start 143, len 15) → 2 hits. Matches the user's
  "exactly 2".** Hit 1 is active on frames 96-106, hit 2 on 143-157. 0x4E6C/0x4E6D/0x4EAC/0x4EAD/0x4EAE are the same.
- 0x4E68 → 3 hits (52-62, 93-100, 113-127). 0x4E6C → 2. Both are known multi-hitters. 0x4E74 → 1 own hit id (rows 146-148, frames
  227-251, element 90). Rows 145 and 149 have start FFFF, so the projectiles (sub7 spawns 1/2 at frame 226) carry the rest. Its
  total on-screen hit count is not proven.
- The old "power groups" candidate agreed only by chance. `+0xA5` is the real key: 0x4E68 has ids 1/2/3 with powers 30/25/40.

### Frame-meter numbers (motion frames, 60 fps clip)
| action | motion | red | hits | active windows (id:first-last frame) |
|---|---|---|---|---|
| 0x4E4E | 1000 | | 1 | 0:52-58 |
| 0x4E8E | 1000 | | 1 | 1:52-61 |
| 0x4E52/0x4E53 | 1001 | | 1 | 0:54-80 |
| 0x4E92/0x4E93 | 1001 | | 2 | 2:54-69 3:70-83 |
| 0x4E55/0x4E95 | 1010 | | 1 | 0:72-79 |
| 0x4E59/0x4E5A | 1011 | | 1 | 0:88-109 |
| 0x4E99/0x4E9A | 1011 | | 2 | 7:90-100 8:105-118 |
| 0x4E5C/0x4E61 | 1020 | | 1 | 0:97-106 |
| 0x4E9C / 0x4EA1 | 1020 | | 1 | 9 / 11:97-107 |
| 0x4E60 / 0x4EA0 | 1021 | | 1 | 0:27-32 / 10:27-35 |
| 0x4E62 / 0x4EA2 | 1022 | | 1 | 0 / 12:36-46 |
| 0x4E64/0x4EA4 | 1030 | | 1 | 6:120-173 |
| 0x4E63 | 1030 | | 1 | 6:167-173 |
| 0x4E68 | 1040 | | 3 | 1:52-62 2:93-100 3:113-127 |
| 0x4E69 | 1040 | | 2 | 2:93-100 3:113-127 |
| 0x4EA8 | 1040 | | 3 | 1:52-70 2:93-110 3:113-136 |
| 0x4EA9 | 1040 | | 2 | 2:93-110 3:113-136 |
| 0x4E6C/0x4E6D/0x4EAC/0x4EAD | 1050 | | 2 | 1:96-106 2:143-157 |
| **0x4E6E**/0x4EAE | 1050 | red | **2** | 1:96-106 2:143-157 |
| 0x4E72/0x4EB7 | 1081 | red | 3 | 1:58-60 2:84-88 3:131-140 |
| 0x4E74/0x4E75 | 1090 | | 1 (+projectiles) | 4:227-251 |
| 0x4E8D | 1104 | | 1 | 4:60-72 |
| 0x4E7C/0x4EA7 | 1062 | | 1 | 0:30-33 |
| 0x18D1 | 1061 | | 1 | 0:0-119 |
| 0x4E42/0x4E44 | 6910 | | 1 | 0:87-91 |
Moves that own 0 rows and have no started rows give 0 own hits. These include 0x4E6F, 0x4E6A/0x4E6B, 0x4E50/0x4E51,
0x4E57/0x4E58, 0x4E73, 0x4EA5 and the 107x family. 0x4E7A/0x4E78/0x4E79/0x4E89-0x4E8B/0x4EB9 have rows with start FFFF, which are
projectile-only. Their hits come from spawns or another path (not traced).

- **Startup (PROVEN as data)** = first active frame (SF convention: startup = first_active + 1 counting frame 0).
  **Active (PROVEN)** = the union of the windows above. Gaps between ids are in-move gaps.
- **Recovery (GUESSED)** = action length - last active - 1. Action length is the motion clip length from the G1A (only motion 1040 is
  known: 221 frames → 0x4E68 recovery about 93). An AI branch can end the action earlier (`+0xE0` / sub3 rules).
- **Speed (GUESSED)**: the frames are motion frames. If `sched_a` is a play-rate %, real-time startup differs between variants. For
  example, 0x4E6E runs 125 on 0-52 and 0x4E6C runs 80. That would make 0x4E6E reach frame 96 sooner in wall time. This is not proven.
  A live probe at `0x5C9F94` (log action id, slot, `+0x08`, `+0xA5`, and the ms since action start) would settle it.
