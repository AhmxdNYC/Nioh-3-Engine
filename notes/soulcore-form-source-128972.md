# Soul core form gap: where the form id comes from, and the real live core swap (static, 2026-11-21, pc/1 #128972)

Static only, no game. Ghidra RO copy (D:/nioh3-re/decomp_ro.cmd) + the decomp export + the script-name table in
re/Nioh3.dump.exe (file offset = RVA, game 2.02). S = read from code, I = inference. Builds on soulcore-A-body-select.md.

## 1. 217D8B4 equips GUARDIAN SPIRITS, not soul cores (S)
- Script native table (16-byte rows {name*, fn*}, 0x397e770..0x3982a70, 1072 rows): `Player::EquipGuardianSpirit` = 0x23A7FF4,
  `Player::AddGuardianSpirit` = 0x23A7714, `Player::IsEquipGuardianSpirit` = 0x23A9F70 (rows at 0x397eb70 / 0x397eb30 / 0x397eb20).
- 0x23A7FF4: if Has(pd, id) 374BD8 then rec = 3748E0([g+0x3F0], id) (the spirit record table, 0x100-byte rows) and
  rec+0xFC bit0 -> **217D8B4(pd, 0, id)**, bit1 -> 217D8B4(pd, 1, id). 0x23A7714 calls 2153E7C(pd, id) = Add.
- 217D8B4(pd, slot<2, id): writes [[pd]+0x100+slot*4] = id, then 2174F44(), 1CA66A4(), 842FAC(pd,id,0), and sound/notice
  70275C(...,0x4B,1). Slot 0/1 = samurai/ninja guardian. Getter 2FEA68 is the same field.
- So the 10-29 run (583ab0fd, 857f76f0) swapped the guardian spirit (0x111 / 0xF / 0x4B are spirit-record ids), not a soul core.

## 2. The Nioh Tenshin form id [state+A68] comes from the WEAPON (S)
- Only writer path: 74C1DC(state, clear) -> SetForm 74C2FC -> 74C608 (writes A68 + action table state+0x1D8).
- 74C1DC: ext = [[state+0xE0]+0x3A0]; set = [ext+0x590+0x1EE0] (0/1); slot = (byte [ext+0x590+set*0xF70+0xF68] == 0);
  item = F65D8(ext+0x590, slot) = ext+0x590+set*0xF70+slot*0xE8 (equipment slots 0..16, 0xE8 each; slot 0/1 = the two weapons);
  def = HashIndexFindRecord1A0([g+0x68], item id); **formId = def+0x58** (default 0x3C2E). If the action mgr is busy
  (2F98A4 false + 13582C(def,0x1A)): formId = 74C9B0(chr) = [chr+0xE8]+0x124.
- Reader: vfunc 0x555B00 (`mov eax,[rcx+0xA68]`, ptr 0x38FE810) in the Tenshin input 2E6C34 picks the activation action.
- Form -> pack table: static std::map built at 0x11793F0 (17 rows, keys 0x3C2E/0x1909/.../0x33C9 = weapon-type form ids),
  lookup 0x11793D0 (pack key 0x600000000 + map[id]).
- **No core -> form table exists.** Neither the guardian slot nor the soul core feeds A68. Same weapon = 0x1909 for every
  core, which is exactly the 10-29 result.

## 3. What a soul core actually is: a d-pad item shortcut (S)
- Player input 2F8A04 -> 2F8B3C (quick item use): for 20 pad buttons (table 0x3BF93A0, 5 dwords per row, column 55A330()),
  shortcut index i -> rec = F66C8([0x4751530], i, set 55A41C(chr)) -> 2FA940/2FA970 reads the u16 item id from the shortcut
  map at **[[0x4751530]]+0x23B7E0** (hash map keyed by set, entry+0x14+i*2, 16 slots), then finds the inventory record with
  that id (inv+0x927D8, 0x5DC rows of 0xE8; fallback 506798).
- Gate 236361C(ctrl,&f,id), then **1178DF0(ctrl, id)**: ctrl+9B0 = id, itemdef+0x54 (res54 = yokai charId) != 0 ->
  FF0BA8(chr+0xA9A0, res54) preloads the yokai, 3FE73C -> 1FF8EC0 -> 682C00 spawns it (soulcore-swap-A-la.md section 3).
- So the summoned yokai = res54 of the item id in the pressed d-pad shortcut slot, read fresh on every press. The game keeps
  no other copy on this path.

## 4. Proposed live swap call (universal, no per-core rows)
The game's own `Item::SetShortCut(slot, itemId)` = 0x23A210C. Its body, for set 0 and 1:
**215BE6C(unused, slot, set, itemId)** -> 21828B0([0x4751530], set, slot, itemId) (u16 store into the map above), then
byte [0x45C6F2F] = 1 (shortcut HUD refresh) and 845338([[0x45B9E40]]+0xB8, 0x49F5) (notice).
- Call on the game thread: `for set in 0,1: 215BE6C(0, slot, set, coreItemId)`. slot = the d-pad shortcut index (0..15)
  that holds the current core; find it like Item::FindShortCut 0x23A191C (loop 2FA940(inv, i, set) == old id).
- Requirement: the new core item must be in the inventory (F66C8 resolves the id to a record; no record = press does nothing).
  The core list = inventory records whose itemdef+0x54 != 0. No per-core table needed.
- No form refresh is needed: the next d-pad press reads the new id and spawns its yokai (res54). The Tenshin form stays
  weapon-driven.
- Unproven live: that the HUD icon updates from [0x45C6F2F] alone; that a cooldown is per item (a fresh core may be usable
  at once). Check with one live call + one press.

## 5. Shrine equip = the active-core update, and why the 11-21 live write showed count 0 (static, pc/1 #129171)
Sources: watch log D:/nioh3-re/live-sc129137/watch.log (human's shrine equip, tid 25776 = game thread) + Ghidra RO decomp.
inv = [Nioh3.exe+0x4751530] (the value, rcx 7FF74CF714E0 in the log). slot s = soul core slot 0..7 (5529D8(inv,s)).
Shrine handler 0x1DECB78 (s = [menu+0x8D0], rec = new core's inventory record, 0xE8 bytes, +0x28 serial):
1. `sc = 216B9D0(inv, s)`: d-pad shortcut index 0..15 that holds the OLD core's usable id (96285C(9649AC(oldId), s>1)), -1 = none. Must run BEFORE the write.
2. `2285C48(s, rec)` must be true (core valid for this slot); else no equip.
3. `21607A4(inv, rec)`: if rec is already in another slot, unequip it there (54C6A8 -> 21607D8(inv, other, 1)).
4. `2183744(inv, s, rec)`: slot = 54CF94(inv, rec+0x28) (the write seen at +2183790), then **54B734(inv) rebuilds the usable-core list
   inv+0x1E0AD0 (200 x 0xE8, counts)** (552DD6 copy, counter A52F88), then 723C20(), 1CA64E4(s).
5. `215F85C(inv)`: drops invalid shortcut entries.
6. **`if (sc != -1) 2182968(inv, sc, s)`**: puts the NEW core's usable id into d-pad shortcut sc (21828B0 store for the current set).
7. `34EDF8(0xF53E7B0C, 0)` (menu event). For the HUD icon also set byte [0x45C6F2F] = 1 as Item::SetShortCut does.
Why count 0: only the slot write/rebuild ran. The rebuild no longer makes the old core's usable item, but the d-pad shortcut still
held the old usable id (step 6 skipped), so the HUD showed the old core with 0. Fix = steps 1 and 6 around the equip, same thread.
