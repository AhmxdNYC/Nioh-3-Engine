# RDB / LFM full index (rdb_index.py)

`py -I re\scripts_re\rdb_index.py [--sniff]` -> `D:\nioh3-re\rdb_index.tsv` (off-repo, 315,418 rows, ~18 s with --sniff).
Read-only on the game folder. Stdlib only; uses `kidshash.kh`. Type/name tables come from the RDBExplorer clone in
`data\RDBExplorer` (optional; without it types show `Unknown`). Format logic matches `data\tools\n3rdb.py` (gitignored).

## Columns
`layer archive offset size csize ktid type_hash type_name name_hash name compression head`
- **rdb** (root 241,590 + system 69,004): `archive` = `root|system/0x<fdata>.fdata` (or `.file` for 15 external entries);
  `ktid` = file KTID, `type_hash` = type KTID, `size` = uncompressed, `csize` = stored. compression = flags bits 20-25:
  zlib 192,902, zlibx (system) 69,004, none 53,490, file 15, `c3` 7.
- **lfm** (4,824, archive_00..19.bin): no KTIDs exist; `name` = 62-hex path hash (`XX/rest`), `ktid` = its first 32 bits only
  as a handle. compression = LFM flag (zlib blocks / none).
- `name`: only 11,908 rdb KTIDs resolve (rdb_names.csv). `name_hash` = kidshash(name). It never equals the KTID
  (0 of 11,908), so the file KTID is NOT the kids property hash of the name; do not try to brute-force names with kidshash.
- `head`: first 16 decoded bytes (hex) for KTIDFileBinary/BinaryFile/CSVFile/ObjectDatabaseFile/KSCLFile/AssetData/
  StreamAssetDataFile and all LFM entries. Use it to sniff formats.

## rdb type counts
G1SFile 69,505; StreamingTexContext 39,195; G1AFile 26,597; **KTIDFileBinary 25,465**; NameDatabaseFile 17,109;
**ObjectDatabaseFile (.kidsobjdb, `_DOK`) 17,086**; OBORO static 15,976; PartsModelGroupBindTable 13,337; G1CO 12,639;
OIDBindTable 12,205; MeshletModelData 11,856; OIDSQT 11,344; ModelData 10,967; MaterialGroupBindTable 10,025;
EffectData 5,997; TexContext 4,868; StaticScreenLayoutTexInfo 1,620; G1P/G1MX/G1COX 781 each; **KSCLFile 584** (UI layout, `LCSK`);
**BinaryFile 543** (GT1G 403, `_MHK` 33, MN1G 32, FP1G 31, 7 zero-head); AssetData 365; **CSVFile 2**; Landscape/River/Rig/etc. small.
Full counts: `cut -f8 rdb_index.tsv | sort | uniq -c | sort -rn`.

## Which entries look like tables (heuristic; contents not decoded)
| Wanted | Best candidates | Evidence |
|---|---|---|
| Enemy params, rewards, skills (game logic) | **LFM game scripts**: head `18 32 30 31 31-04-29...` ("2011-04-29" bytecode), 565 entries in archive_09 (110), _14 (321), _16 (132), _05 (2) | Extracted copies contain strings Mission (565), Enemy (450), Boss (110), Skill (22), Reward (11) |
| Param tables (numeric) | LFM count-prefixed blobs: head `10 00 00 00` (374, archive_01), `09 00 00 00` (133, archive_01), `03 00 00 00` (186, archive_02/10), `05 00 00 00` (58) | little-endian u32 count first; 1-2 MB for the big ones; unverified, decode next |
| Missions / stage layout | rdb `.kidsobjdb` (17,086) incl. stage DBs (Octree, ProcPlacement, PerspCam in `typescan_root.tsv`); mission logic is in the scripts above | enemy placement likely ProcPlacement-type objects |
| Text / strings | none found by type. `KTIDFileBinary` (25,465, all zero-head) are KTID-keyed blobs, BinaryFile `GT1G/MN1G/FP1G/TB1G` (LFM `TB1G` 228) are G1 containers (graphics/motion/params); text probably inside KTIDFileBinary or scripts | no `StringTable`/text type exists in TypeIDHelper |
| Battle scrolls | not typed anywhere; search KTIDFileBinary + scripts for "Scroll" (0 hits in extracted LFM) | likely a KTID-keyed table; use File Loading Logger to map the KTID live |
| CSVFile | only 2 rdb entries | check first |
| UI / animation (not tables) | LFM `G2A_` 516, `TMG_` 361, `CPSM` 68, `_S2G` 35, 0x20040022 pack (1,293, archive_03) | |

Caveat: only head sniffing and keyword grep on the existing `data\extract\lfm`; nothing is decoded. Next step: dump a
`10 00 00 00` blob and a KTIDFileBinary, and trace which KTIDs load during a boss mission.
