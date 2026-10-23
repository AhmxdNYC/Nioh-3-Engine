# Boss action record: Yoritomo, Shingen, Otoroshi

2026-10-22, offline. The game was not launched. Board task 71. The notes named in the brief, `notes/boss-movesets.md` and `notes/boss-action-ids.md`, are not in this tree. The 47/104 row pair is not a count in those missing files. The counts below were read from the three action-field CSVs.

The action id from Training `ActionLog` (commit `39265534`) is the value returned by RVA `0x358528`. Those 15 bytes in `re/Nioh3.dump.exe` are `48 8B 81 E8 00 00 00 48 85 C0 74 03 8B 00 C3`: `mov rax, [rcx+0xE8]`; if that pointer is not null, `mov eax, [rax]`; `ret`. The file row for that id is a different object. The hit-setup path uses `[[state+0xE8]+0x20]`, a 336-byte row whose id is the u16 at `+0x0C` (`notes/yoritomo-hit-source-22305.md`).

## Record the id indexes

Each boss has one LFM action file. Sub-table 1 is an array of 336-byte (`0x150`) rows. The id is the u16 at `+0x0C`. There is no second copy of that table in the exe.

| off | what it is | status |
|---|---|---|
| +0x0C u16 | action id | proven. It matches the ActLog id. |
| +0x20 u16 | motion number | proven as a motion id. 1000-1199 is the attack-motion band on these files. It is not a duration. |
| +0x00 qword bit 45 (`+0x04` bit `0x2000`) | great-attack / unblockable flag | proven on Yoritomo: file bit -> state bit 11 -> hit `+0x13C` bit 10 (`IsAttackGreatAttack`). |
| +0xE0 u16 | next action when the move finishes | proven on Yoritomo against the ActLog. `0x2` is idle. A self id loops. |
| +0xF0 u32 | action-object hash | proven as a hash in that character's setting DB. The name database is not in the installed package, so this is not a display name. |
| +0x13C / +0x142 u16 | attack-block words | set (not `FFFF`) only on some attacks. This marks an attack block. It is not a hit count. |
| +0x40 event slots | frame, length, event id | the frame numbers are event marks inside the move. They are not startup / active / recovery. |
| hit count | none | no field is the number of hits. `0x5C9348` returns one hitbox row for `(action id, slot)`. The slot count is not a hit count. |
| damage | not on this row | Yoritomo's base damage is hitbox-row word `+0x10` times `0.01` times `500`. The action row does not hold that word. |
| duration | not on this row | logged run time does not match any u16 here. Length is in the motion clip. |

## Filled values

`has_attack_block` is 1 when `+0x13C` or `+0x142` is not `FFFF`. Great-attack means `flags04` bit `0x2000`.

| boss | chr | rows | unique ids | motion-kind attack | attack-block | great-attack bit | idle `0x2` |
|---|---:|---:|---:|---:|---:|---:|---|
| Minamoto no Yoritomo | 16102 | 173 | 173 | 96 | 88 | 19 | next `0x2`, hash `8db9745e`, motion 0 |
| Takeda Shingen | 42450 | 136 | 136 | 57 | 0 | 13 | next `0x2`, hash `e50e95a8`, motion 0 |
| Otoroshi | 371449 | 141 | 141 | 22 | 39 | 1 | next `0x2`, hash `56704158`, motion 0 |

One filled attack row each:

- Yoritomo `0x4E6E`: motion 1050, kind attack, next `0x2`, hash `e999d024`, great-attack bit set (`flags04` `0x2200`), attack-block 1, phase marks `5@13 1@83 0@92 6@112`. ActLog median run 3586 ms on the rows that have a log. That time is not stored in the row.
- Shingen `0x4E4E`: motion 1030, kind attack, next `0x2`, hash `d049c7ec`, great-attack bit clear (`flags04` `0x0100`), attack-block 0. Shingen's great-attack example is `0x4E6B`: motion 1010, kind attack, next `0x2`, hash `b5d464ea`, `flags04` `0x2200`, attack-block still 0.
- Otoroshi `0x4E4E`: motion 1012, kind attack, next `0x2`, hash `0f6bedd8`, great-attack bit clear, attack-block 1, phase marks `1@55 0@114`. The only great-attack row is `0x4F56`: motion 11042, kind other, next `0x4E53`, hash `b94187fa`, `flags04` `0x2200`, attack-block 1, phase marks `4@12 0@36 2@62 0@103`.

Sources: `nioh3-ryomen-16809/notes/yoritomo-action-fields.csv` (sha256 `db246290...`), `nioh3-coverage-16710/notes/shingen-action-fields.csv` (`40121e11...`), `nioh3-coverage-16710/notes/otoroshi-action-fields.csv` (`123dbcb9...`). Same files as `nioh3-sort-20999/notes/boss-batch-16858/boss-master-coverage.csv`.

## What this does not claim

Hit count, damage, and a display name are not columns of the 336-byte row. Duration is not a column either. Shingen has 57 attack-motion rows and zero attack-block rows, so the attack-block words are not how every boss marks an attack.

## Hitbox rows (damage)

2026-10-22, offline. The game was not launched. The damage table is sub-table 2 of the same LFM action file. Each row is 192 bytes. The size of that sub-table divides by 192 on all three files, and the packed-record tail of sub-table 0 walks to the end with nothing left over.

| boss | file | hitbox rows | actions that own a row | attack motions 1000-1199 | of those, with a row |
|---|---|---:|---:|---:|---:|
| Yoritomo | `archive_01/FA/5FBD421B...CFFA54` | 182 | 50 | 96 | 48 |
| Shingen | `archive_01/E5/D7F89184...94F6DE` | 194 | 67 | 57 | 51 |
| Otoroshi | `archive_01/17/7C2A9D27...4EED78` | 162 | 69 | 22 | 19 |

The motion number does not select a hitbox row. It is only the u16 at action-row `+0x20`. Several action ids can share one motion, and each action has its own list.

The link is the action-row index:

1. Action row `r` (the 336-byte row whose id is at `+0x0C`) has a packed record in sub-table 0, in the same order as the action rows.
2. That record is a u16 count, then that many u16 indexes.
3. Each index picks one 192-byte row in sub-table 2.
4. The lookup `0x5C9348` takes `(action id, slot)` and returns that one row. It does not take the motion number.

Base damage is the word at hitbox-row `+0x10`, times `0.01`, times `500` (the same as times 5). That formula is the Yoritomo getter `GetAttackBaseDamage` at RVA `0x23C785C`. Shingen and Otoroshi use the same row size and the same word. Their numbers below were not checked against a live health drop.

One motion, different lists:

- Yoritomo motion 1050. `0x4E6E` owns rows 122-126: power 50, 50, 50, 55, 55 (base 250, then 275). `0x4E6F` is the same motion and owns 0 rows.
- Shingen motion 1030. `0x4E4E` owns rows 48-52: power 17, 17, 48, 48, 48 (base 85, then 240). `0x4E73` on that motion owns five rows at 12 and 42. `0x4E8B` owns seven, the last two at power 0.
- Otoroshi motion 1012. `0x4E4E` owns rows 32-34, and all three words at `+0x10` are 0. `0x4EED` and `0x4E72` on that same motion each own one row at power 105 (base 525).

A power of 0 is what the file stores. It is not a claim that the swing does no damage. Element is the word at `+0x12`. These examples are element 0. The row count is still not a hit count.

## Hit count is not on the ActLog object

2026-10-22, offline. The game was not launched. The dump is `re/Nioh3.dump.exe`, 87,011,328 bytes. RVA `0x358528` is still the 15 bytes `48 8B 81 E8 00 00 00 48 85 C0 74 03 8B 00 C3`. It loads the object at `state+0xE8` and returns that object's first dword. That dword is the action id. It is not a hit count, and it is not a combo counter.

The same dump has no `HitCount`, `hit_count`, `ComboCount`, `combo`, or `HitNum` string. The only `GetHit` string is `Collision::Attack::Hit::GetHitEffLocalPos`.

There is no state field that already holds the number of hits. A display has to count.

What it reads:

1. The action id, from `0x358528` (the first dword of `state+0xE8`). That is all ActLog has today.
2. That action's hitbox rows, through the index list in the section above.
3. On each row, byte `+0xA5`. Getter `0x2B46E0` returns that byte, unless hit-record `+0x13C` bit 15 or 23 forces `0xC8` or `0xC9`. Byte `0xFF` becomes 0. The read is `0x2B5B98`.
4. The hit count is the number of different `+0xA5` values among rows whose start frame (`+0x08`) is not `FFFF`. Rows that share an id are one hit. The number of rows is the wrong number.

Checked this session:

| boss | action | motion | rows | distinct ids | ids |
|---|---:|---:|---:|---:|---|
| Yoritomo | `0x4E6E` | 1050 | 5 | 2 | 1, 2 |
| Shingen | `0x4E4E` | 1030 | 5 | 3 | 1, 2, 3 |
| Otoroshi | `0x4E4E` | 1012 | 3 | 1 | 0 |

Attack-motion actions that have at least one started row, so this count exists in the file: Yoritomo 41 of 96, Shingen 50 of 57, Otoroshi 19 of 22. The rest own no started row. Yoritomo `0x4E6F` is motion 1050 and owns 0 rows, so this read says 0. A sibling or a projectile can still hit. That path is not a field on `state+0xE8`.

A "hits so far" number also needs the current motion frame, and then counts only the ids whose start frame has been reached. A "hits that landed" number is not on this object. The file count is how many hits the move is built to throw.
