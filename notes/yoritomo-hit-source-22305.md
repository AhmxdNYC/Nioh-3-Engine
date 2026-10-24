# Yoritomo (chr 16102): where hit count, unblockable and damage come from (#22305/#22306)

2026-10-21, offline only. The game was running and was not touched. No installed file changed. Nothing committed.
Builds on `nioh3-sort-20999/notes/yoritomo-attack-values.md` (hitbox table, row ownership, damage path), `yoritomo-action-fields.md`,
`yoritomo-frame-fields.md`, and `.handoff/yoritomo-training-stop-21839.md`.

Data file: archive_01 `FA/5FBD421BF96B8514DA056584B9507A22DC7956B1793995A113FBA714CFFA54` (sha256 `89836dd4...`).
Exe: `re/Nioh3.dump.exe` (file offset = RVA, image base 0x7ff74f410000).
New tools (read-only): `tools/chr_hit_groups.py` (per-action table), `tools/n3dis.py`, `tools/n3dispscan.py`, `tools/n3condcase.py`.
Output: `notes/yoritomo-hit-groups-22305.tsv` (94 actions: unblockable flag, flag bits, owned hitbox rows, row groups).

## Verdict
- **Unblockable / "great attack": FOUND in the static data, with the code path proven.** It is the action-row u16 at `+0x04`, bit `0x2000`
  (qword `+0x00` bit 45). It is set on **19 Yoritomo actions**, all of them attacks.
  `0x4E50 0x4E51 0x4E57 0x4E58 0x4E6A 0x4E6B 0x4E6E 0x4E6F 0x4E72 0x4E73 0x4E90 0x4E91 0x4E97 0x4E98 0x4EAA 0x4EAB 0x4EAE 0x4EAF 0x4EB7`.
- **The move the user calls "unblockable double attack" is `0x4E6E`.** The live Training log says the drill is forcing
  `0x4E6E` (142 forced plays from 11:05:03 to 11:20:39). `0x4E6E` has the flag. Its hitbox rows form **2 groups** (power 50 x3 rows, then
  55 x2 rows). The same motion's guardable variants `0x4E6C` and `0x4E6D` do not have the flag. Their rows have the same shape at lower power (42, 47).
- **Damage: proven formula, two live numbers fit it.** Base damage = hitbox row `+0x10` x 0.01 x 500 (= x5). Element = `+0x12`, the same way.
- **Hit count: candidate only.** The count of hitbox-row *groups* fits 0x4E6E (2), but 44 attacks own 0 rows, including the multi-hitter `0x4EA5`.
  The actual per-swing count is a runtime event (see "Live probe").
- **Startup / active / recovery: not in static data (unchanged).** The probe below gives them from real frames.

## 1. Proven (numeric / code evidence)

### 1a. Unblockable flag chain (static row -> runtime -> hit -> script API)
1. Script API strings and their native functions (table entries `0x397A980..0x397AA10`):
   - `Collision::Damage::Hit::IsAttackGreatAttack` -> RVA `0x23C7EA0`: `test dword [hit+0x13C], 0x400` (hit from `0x2E6074`).
   - `...IsAttackPowerAttack` -> `0x23C7F20`: `[hit+0x13C] & 2`. `...IsAttackAttackSkill` -> `0x23C7B18`: `& 0x40`.
   - `...GetAttackBaseDamage` -> `0x23C785C`: `word[[hit]+0x10] * 0.01 * 500`. It adds `word[[hit]+0x12]` the same way when the type at `hit+0x14` / row byte `+0x20` is one of a list.
   - `...IsAttackCanCounterGreatAttack` (`0x23C7C1C`) and `...IsAttackSekiBreak` (`0x23C7FA0`) are stubs that always return 0.
2. The hit record is filled by **RVA `0x2B4A3C`** (`rcx` = hit record, `rdx` = attacker, `r8d` -> `hit+0x16C`, `r9` = hitbox-row object -> `hit+0`).
   At `0x2B4FEE..0x2B5000` it calls `0x4893B8(attacker, 0x75)`. If that is true, it does `bts [hit+0x13C], 10`.
   At `0x2B5098..0x2B50B0` it does the same check for the parent (`[r13+0x3B0]`), so a shot inherits the flag. Other bits filled in the same block:
   cond 0x2D->bit0, 0x2E->bit1 (PowerAttack), 0x33->bit2, 0x32->bit3, 0x30->bit4, 0x31->bit5, 0x2F->bit6 (AttackSkill), 0xEC->bit22,
   0xFC->bit25, 0xF5->bit26.
3. `0x4893B8` -> state test `0x2E9948(state, id, curAction)`. Here `rbx` = `[[state+0xE8]+0x20]` = **the current action's 336-byte file row**.
   Condition **0x75** = `state+0x1060` bitset, bit 11 (`0x2EABAF`). It was resolved with `tools/n3condcase.py`.
4. The bitset `state+0x1060` is set through `0xFC1A20(bitset, bit, value)` (119 bits). The only constant-bit-11 set is **RVA `0x4F617A`**, inside
   the action-start function `0x4F54C4`:
   `rcx = [[state+0xE8]+0x20]; test qword [rcx], 0x200000000000; je clear; set bit 11; clear bit 12` (else it clears bit 11 at `0x4F6199`).
   So **file action row qword[+0] bit 45 -> state bit 11 -> hit +0x13C bit 10 = IsAttackGreatAttack.**
5. The row bits right beside it are also static: qword[+0] bits 40/41/42/43 -> hit bits 0/1/3/2 (conditions 0x2D/0x2E/0x32/0x33).
   On Yoritomo, every flagged action has bit 41 (= **PowerAttack**) and not bit 40. The one exception is 0x4E57/0x4E97, which have bit 40 instead.
   Normal attacks have bit 40.

### 1b. Live match with the move playing now
- `Nioh3Training.log` (copied read-only): `DRILL chr 16102 forced 0x4E6E` x142 (11:05:03-11:20:39). The user says this move is unblockable and hits twice.
- `0x4E6E` row: great_attack=1, bits40-43 = `0100`. Hitbox rows 122-126: `50/0/12 x3` then `55/0/12 x2` (`+0x10/+0x12/+0x14`). Each group starts with a
  `+0x28=22` capsule row.
- Player-HP drops during that drill (heartbeat every 2 s, full refill afterward): **2658->2211 (447)** at 11:15:46 and **2658->2167 (491)** at 11:20:30.
  491/447 = **1.098**, and power 55/50 = **1.100**. Each drop fits **one hit** of group 1 and of group 2 at the same defense factor (x1.79 of base 250/275).
  The older `0x4E68` drop of 221 fits power 25 (row 93) at x1.77. These ratios fit, but they do not prove the groups are hits.

### 1c. Other numbers
- Hitbox row ownership (from the earlier note): sub-table 0 packed records. Re-checked here for all 173 rows (`tools/chr_hit_groups.py`).
- Group-count distribution over the 94 actions with an attack block or rows: 0 groups 44, 1 group 23, 2 groups 16, 3 groups 9, 4 groups 2.

## 2. Candidates (not proven)
- **Hit count = number of row groups** (a run of owned rows with equal power/element/type). Fits 0x4E6E = 2, 0x4E6C/0x4E6D = 2 (known "hits repeatedly"),
  and 0x4E74 = 2 (4 element rows `0/90/17` for the spawned objects, plus 1 physical `55`). It fails on actions that own **0 rows**: 44 actions, including
  `0x4EA5` (known multi-hit) and the flagged `0x4E6F 0x4E6A 0x4E6B 0x4E50 ...`. The lookup `0x5C9348` takes **(action id, slot)** from the attack command.
  A move with 0 rows probably uses a sibling's rows (for example `0x4E6F` -> `0x4E6E`). That is not proven.
- Row `+0x14` type: 12 on almost all melee rows, 17 on spawn and element rows, 4 on 0x4E7A's middle group, 65535 on 0x18D1, 0x4E7C and 0x4EA7. Perhaps attack kind / hit reaction.
- Row dword `+0x04` = `0x04000000` on the first row of some element groups (0x4E72 row 144, 0x4E74 row 145). Unknown.
- "Great attack" = the in-game unblockable / perilous kind. The name and the 0x4E6E match support this. The guard-side reader was not found: the only
  other reader of bit 10 is `0x1A62920`, which sets a stats byte. A guard decision through a copied flag word was not traced.
  Grabs are a different path (box flag +0x4C, `notes/grapple-finisher-paths-9133.md`).

## 3. Still unknown
- Exact hit count per swing for the 44 zero-row actions, and which slot each command uses (`0x5C9348` slot from the command / `+0x75C` byte).
- Startup / active / recovery frames. Not in the action row, sub-tables, or motion dope tracks (earlier notes).
- Whether the guard code treats bit 10 as "cannot guard" or as "guard breaks / big Ki damage". It is still the only static flag that marks 0x4E6E.
- Ki damage: the row candidates `+0x1C/+0x1E` (earlier note) are not tied to a reader.

## 4. Live probe (read-only logging in the Training mod; no game-memory writes)
Hook **RVA `0x2B4A3C`** (hit-record setup) with a pass-through detour. Log after the original returns (`rcx` saved = hit record `H`, `rdx` = attacker,
`r9` = row object `R`):
- attacker chr id (already resolved by ActLog), current action id = `u16 [[[state+0xE8]+0x20]+0x0C]` (state = `[attacker+8]`)
- `dword [H+0x13C]` -> bit 10 = unblockable/great, bit 1 = power, bit 6 = skill; `word [R+0x10]`, `word [R+0x12]`, `word [R+0x14]` (power/element/type)
- motion frame at that moment (ActLog already has the action start; ms since start / 16.67 gives a frame), and `dword [H+0x16C]` (r8d, unknown id)
Then **one log line per hit-record creation = one hit window opening**. Lines per action run = hit count. The first line's frame = startup. Pair it with
`Collision::Attack::Delete` / `DeleteAll` (script table `0x397A790` / `0x397A7A0`) or the record's release to get active length. Recovery =
action end (ActLog `ran N ms`) minus the last active frame.
A cheaper first step: log `[[state+0xE8]+0x20]` qword[+0] bit 45 at ActLog action start. That needs no hook and shows the unblockable label live.
Cross-check: every 0x4E6E run should log 2 creations with power 50 then 55 and bit 10 set. 0x4E6C should log the same shape with bit 10 clear.
