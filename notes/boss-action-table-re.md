# Boss action table - static RE (pc2 #71, 2026-10-18, partial)

Method: capstone on the runtime dump (`Nioh3.dump.exe`, RVAs), no game run, no Ghidra on pc2. Scripts: scratchpad only (disasm + `[reg+0xE8]` -> `[x+0x20]` field-use histogram over the action modules 0x340000-0x3A0000, 0x1FF0000-0x2060000, 0x2360000, 0x2770000-0x27D0000, 0x490000, 0x66C000, 0x9C0000).

## Proven chain
- `state` = `[[actor+3B0]+8]` (action state object).
- `state+0x178` = action set. `49B054(set, key)` scans the set's groups: `49B804` = group count (0x1A, or 0x17 variant), `49B720` = group index map (tables at 0x3BDBA70/0x3BDBAD0); group = `[set + idx*8]`; `49D19C(group, key)` linearly searches the group's array (`group+0x190` ptr, `group+0x198` u16 count) for the entry whose **s16 at +0 == key**.
- That entry is the **request-map record**: `+0` s16 key, `+2` u16 mapped action id (what `2367454` returns, FFFF on miss), `+4` byte flags (bit 1 tested by `66C330`). So it maps key -> action id; it holds no damage.
- `state+0xE8` = **current action object** (set at `35E148` from `49B688(set,...)`; null = none). `358528` returns its `dword [+0]` = action id (FFFF if null); `state+0x318` is a second index compared with it (`3583E0`).
- `action+0x20` -> **action param block**, used in 87 places. Fields seen: `+0x00` qword flags (bits 40 etc.), `+0x18` qword flag bits (4,7,0x33-0x35,0x37,0x38 = permission/cancel-style gates, `357xxx`), `+0xDC` s16 (sign bit = none; written to `[ctx+0x106]`), `+0xF8`/`+0x110` qword flags, `+0x3E` s8, `+0x2A/+0x2C/+0x2E` s16, `+0x34`, `+0x14E` s16.
- Also on the action object itself: `+0x4C,+0x50,+0x54,+0x64,+0x80,+0x94,+0xBA,+0x120` (sizes known, meaning not).

## NOT found
Hit count, damage, anim/name hash and duration fields are **not** in this chain's readers (flag/gate fields dominate). Damage is applied from the collision attack side (`Collision::Attack::*`, `Collision::Damage::Hit::GetAttackBaseDamage` script bindings in strings.tsv at 0x3bf1838), so the next step is: from `GetAttackBaseDamage`'s binding, find the attack-param struct and how it is picked per action (probably via action+0xDC or a motion/attack-id lookup), then name the fields. Duration is better measured live (ActionLog already gives avg ms per id, see boss-action-ids.md).
