# Soul core item -> seal spawn chr id (pc2/0, task #81523, 2026-11-07, live read on pc2)

Game: Steam build in use 11-07 (EF b7eeb478, BR 8aeb68a2, TR f57937d1), pc2 save. Read-only ReadProcessMemory; saves restored byte for byte.
No hardcoded per-core table: the game's own param tables give the link.

## Chain (RVAs for the current exe; all reads are plain memory)
GameData = qword [exe+0x45B9E30] (a static object inside the image). Each param table: wrapper = qword [GameData+off];
header = qword [wrapper+0]; header+0 u32 magic 0x20042200, header+4 u32 count, rows from header+8.

1. **Core item row** - item table [GameData+0x68] (same table the game's F4DBC lookup uses), stride 0x1A0, count 3362.
   - row+0x152 u16 = item id (the save/inventory u16 id; the "w84 hash" in notes/items.md is really id<<16).
   - row+0x64 u32 = category; soul cores = 900000000 (119 rows).
   - **row+0x180 u16 = soul-core param key** (unique per core; e.g. EB60 -> 59C4, BD34 -> D619, DFFB -> D51A).
   - (row+0x128 is a hash that only sometimes matches; 37 cores share b5dfb4fc - do NOT key on it.)
2. **Soul-core table** [GameData+0x118], stride 0x54, count 84 (118 of 119 cores map; cores can share a row):
   - +0x0C u32 key (= core row+0x180)
   - +0x18 u32 **Yang item = Summoning Seal item id** (e.g. 9C04)
   - +0x1C u32 Yin item (talisman, e.g. 10DE Carnage Talisman)
   - +0x24/+0x28/+0x2C costs/levels (= amrita n_raw, e.g. 5,25,37); +0x10,+0x14,+0x20,+0x30 not decoded.
3. **Seal item row** (item table again, id = Yang item): row+0x64 = 700000000, **row+0x54 u32 = chr spawn id** = the desc[0] BR's
   seal hook logs ("FormKey seal spawn hit ... desc[0]="). (+0x114 = shared effect A839, as in #53039.)

## Proof (pc2, live, 11-07 ~6:25-6:36 pm EST)
| core (item) | key +0x180 | yang seal | seal row+0x54 | BR log at press |
|---|---|---|---|---|
| EB60 Ibaraki Doji | 59C4 | 9C04 | **5129B** | LEFT press: `seal spawn hit ... desc[0]=5129B` |
| BD34 Maelstrom Oni-bi | D619 | FC9F | **A2ACB** | DOWN press: `seal spawn hit ... desc[0]=A2ACB` |
| DFFB Yasha | D51A | C09D | **5B402** | (matches ally #81149 Yasha Yang -> 5B402) |
Full dump of the 84 rows: notes/soulcore-core-to-seal-81523.tsv. Re-run: `py tools/re/core_chain.py EB60 BD34` with the game running
(prints the chain from the exe global; needs PROCESS_VM_READ only).

## Equipped core -> core item id (not proven live here)
Save schema (amrita, #81522): soulCores AB0E9943 = 10 x {u32 0x36BFE4B6, u32 ?, u32 inventory index | FFFFFFFF}, entries 0-1 = Yang;
inventory record 240 B (0xF0), +0 u16 item id. Live I found the core's inventory records (0xF0 stride, several copies:
main + presets; on pc2 EB60 and BD34 are 138 slots apart) but did NOT locate the runtime Onmyo Box index array, so the index ->
record step is schema only. In-mod alternative that needs no index: walk the inventory for cat-900000000 items is not enough
(all owned cores), so the menu still needs the Yang entries; or learn per d-pad side at the seal hit (desc[0]) and name it via
the reverse map seal chr id -> core item(s) from steps 1-3.

## For the menu (cx-pc)
- Build the map once after the tables are loaded (title or field): for every item row with cat 900000000, key=+0x180 -> soul-core
  row -> yang id -> seal row+0x54. 84 rows, trivial cost; no memory scan needed (two pointer derefs from the exe global).
- Core name: no text on core rows (name id 0); the game uses template "{} Soul Core" + yokai name. Not resolved here.
- RVA 0x45B9E30 is for this exe; for other builds, find it via the F4DBC caller's global (notes/soulcore-ghidra-questions.md q6).
