# Boss / enemy PARAMETER record (decoded, static, 2026-10-08)

Game not run, mod not touched. Output: `notes\boss-enemy-params.csv` (all 1022 records, one row each; columns below). Source: `data\extract\lfm\archive_03\C4\E1CBAE0841F7ECE8...` (943,314 B): header `00 22 04 20`, u32 count @+4 = 1022, records 920 B from file offset 8 (1022*920+8 = 940,248; the trailing 3,066 B = 3 B per record, constant `1B 5A 00` pattern, record word0 = running offset into it). Common LFM table layout: **word0 = cumulative offset into a side blob, then fields**; many sibling tables in archive_03 use the same `00220420, n, stride` header and are joinable on keys (below). Confidence tags: H = cross-table proven, M = strong statistical/value pattern, L = guess.

## Field map (record-relative byte offsets, u32 unless noted)
| off | meaning | conf | evidence |
|---|---|---|---|
| +0x00 | cumulative offset into 3 B/record tail (0,3,6,...) | H | exact arithmetic progression |
| +0x04 | cumulative offset into an external variable-length list (0,8,29,53,...,; Shingen 108) | L | monotone, no target found |
| +0x08 | **char_id** (unique, 20680..51475, ascending) | H | boss-names.md |
| +0x18 | **base HP** (0..21000) | M | grunts 1000/1060/1450/1900, yokai mobs 480-1900, humans 9000, major bosses 7000-21000 (Shingen/Ibaraki 17000, Honda 13000/9000/7000, Hanzo 10000/7000); 261 recs >=3000. Never seen as a 'level' scaled value: this is the raw base, level scaling not in this record |
| +0x1C,+0x20,+0x24,+0x28,+0x2C | percent-style multipliers (100 / 1300 / 100 / 95,195,.. / 1000 default) | L | near-constant columns; +0x28 differs by band (195 main bosses, 95/115/140) |
| +0x10, +0x9C | 100 (50 on 3/29 rows) | L | constant |
| +0x60 | 620 (170/700/90 on a few) | L | constant |
| **+0x64** | **record key** (unique 100..999088, hash-like) | H | targets of +0x1E4/+0x1E8 |
| +0x68..+0x78 | flag bitfields (e.g. 0x8A4038, 0x100000 ...) | L | low-cardinality (121/78/29/26/56 distinct) |
| +0x80/+0x84/+0x88/+0x8C/+0x90/+0x94/+0x98 | small enums: size/AI-class/etc. (+0x88 nonzero on 50 recs only; +0x94 in {48,112,64,96}) | L | histograms |
| **+0xA0** | **name text id** (0 = unnamed) | H | boss-names.md, 960 resolve in EN group 7662000 |
| **+0xA4** | **enemy master id** -> `archive_03\B0\AB7C3E87...` col1 (268 rows x 20 B: [0] cum, [1] id, [2] family seq, [3] name key, [4] flags 0/1/3/256/257). 163 distinct values, 163/163 present | H | exact set containment; shared by variants of one boss (Honda 59966 on 3 recs, Hanzo 37970/11280, Shingen 32150/51399). 0 on 275 recs (mostly ids >=43000 and generic mobs) |
| **+0xA8** | key -> `archive_03\23\DAC95BA0...` col3 (84 rows x 84 B), 79/79 | H (link) / L (meaning) | table has per-row ids + item-category hash in col12 (u32, 48/48 found in item table `2E\D4E1DB3B` col 74). **Best candidate for the enemy -> reward/drop link**; not resolved to concrete items (see below) |
| +0xAC | 0/30/45/50 | L | |
| +0xB0..+0xD4 | 12 percent factors (100 default, 80/120 on a few): per-damage-type/element taken? | L | |
| +0xD8 | 15000 (372 recs) or small id 30/38/45/53/60/68/83/150: matches table `37\2EE466...` (14/14) | M (link) | 15000 = default/none; value band tracks char_id band (53/83 humans in 31000-42999, 60 in 22500-25500, 150 in 49000+) |
| +0xE0..+0x10C | 12 u16-ish ids -> table `F0\B57C9DE7...` (99 x 116 B, float tuning rows); +0xE4 in {16201 (832 recs), 239, 12608, 9454} | H (link) | 5-12/5-12 distinct values all present |
| +0x110 | 1..10000 (1000 default; bosses 1600-2000, 500 on some) | L | candidate Ki/stamina pool; unproven |
| +0x114,+0x118,+0x11C,+0x120 | 0..100 / 100 or 10000 / 1 / 1000 or 150 | L | bosses mostly 10000 and 150 |
| +0x124,+0x128 | ids (+0x124 -> `5F\FA7D6BB0` 16/16; 6409 on 267 recs) | M | |
| +0x12C..+0x13C | ids on only 17/120/11/2/3 recs (boss-only) | L | |
| +0x14C..+0x15C | 5 ids, nonzero on 57-72 recs, ~85% found in `D0\D8B53841` (930x96) / `8E\77FA98BE` (288x68) | M (link) | sparse = special/boss-only attachments |
| +0x160..+0x188 | enemy attack/guard-gauge style numbers (+0x178: 18000 Shingen/Ibaraki, 9000 Honda/Hanzo, 320/735/240 mobs; +0x17C 1200/180/50; +0x180 600/90/40) | L | bosses cluster at 9000/18000 |
| **+0x19C,+0x1A0,+0x1A4,+0x1A8** | keys into combat-behaviour set table `archive_03\23\5B761F23...` (528 rows x 432 B, word0 = key). 207/158/108 distinct, **100% found** | H (link) | same table also holds the +0x1AC/+0x1B0 constants (0x48278, 0x57C83 on 1018 recs). Probably the move/skill/AI parameter sets (**AI id candidate**) |
| +0x1B8/+0x1BC/+0x1C0 | enums 0..15 (2/6/11/3 = species class?) | L | tracks band: 6 = main bosses, 11 = ext block |
| +0x1CC | key -> `72\0F8A8F11...` col3 (235x32), 167/167 | H (link) | |
| +0x1D0 | key -> `08\8F254529...` col0 (435x152, mostly floats = hit/motion tuning), 117/117 | H (link) | |
| +0x1D8 | key -> `A4\E6103C62...` col0 (87x52, float tuning), 62/62 | H (link) | |
| **+0x1E4,+0x1E8** | **link to another record's +0x64** (48 / 6 recs); resolves to partner char_id | H | Nuribotoke 21008<->21027, Kajiwara 22248<->23167, Mikoshi Nyudo trio 26296/26312/26343, Gaki -> Gaki, Scampuss chains. Matches the exe binder `Chara::GetLinkEnemyID` (= linked/summoned/partner enemy). CSV: link_1e4_char/link_1e8_char |
| +0x1F4 | key -> `6C\7CE6AC...` (96 rows x ~512 B), 39/39 | H (link) | |
| +0x1F8..+0x234 | 16-bit ids (motion/model/effect set refs); no containment found | L | **model id / music id NOT found here** (model is via CE_/CS_ char-key hash -> MotorCharacterSetting per boss-char-assets.md; BGM is script-driven per boss-bgm-events.md) |
| +0x23C | key -> `61\1AF149C5...` col5 (188x24), 172/172 | H (link) | |
| +0x248,+0x24C,+0x254,+0x258 | u32 kids-style hashes; +0x248 (933/936) and +0x254 (821) are in `BC\A7FA73E0...` col3 (268 rows twin of the master table) | H (link) | +248/+254 include character-name hashes: Yamagata307259=CN_MASAKAGE and598115=CE_MASAKAGE_00, directly verified2026-10-11 in `training-yamagata-asset-binding.md`. Earlier zero-match comparison used loader IDs from stage_names, so its non-character-hash inference was invalid. Neighboring-field semantics remain open. |
| +0x264..+0x2BC | float32 tuning (100.0 / 55.0 / 250 / 120 ...) | M | e.g. 0x42C80000 = 100.0 |
| +0x2C8..+0x2E8 | float scales 1.0 (0.98..1.4 on few) | M | |
| +0x30C..+0x394 | packed u16 pairs (0x00640064 = 100|100 ...) | L | |

## What is NOT in the record (negative results)
- **No ki field identified** with confidence (+0x110 and +0x178 are candidates). **No level / NG+ / difficulty scaling field**: HP is a flat base; per-variant differences are separate records (below). No explicit boss / HP-bar flag: the gauge is script-driven (`UI::ShowBossGauge`, boss-fights.md). Boss tiering here is derived: tier 1 = HP >= 7000 and named (186 records, 115 name-groups); tier 2 = HP 3000-6999 and named (67).
- **Drop / reward id:** no field in this record equals an item key or hash. Item table `2E\D4E1DB3B` has no back-reference to an enemy. The only join that lands near items is +0xA8 -> `23\DAC95BA0` col12 -> item table col74 (a category/group hash shared by several items, e.g. 0xB5DFB4FC = 2 items; 0xED7FB07D = 1 item), i.e. a *drop-set / reward group* link, not an item id. Resolving DAC95BA0 col2 (1250/6500/5251/... = likely currency/amrita value) and cols 13-20 would be the next step. Most tier-1 bosses have +0xA8 = 0, so main-story boss rewards are probably granted by mission scripts (boss-loot.md). Treat as unresolved.
- Music: not in this table. Model: not in this table.

## Variant grouping (CSV columns `group`, `group_size`, `variant_rank`, `hp_pct_of_group_max`, `band`)
Group key = name_text_id (else master id). Variants of one boss share name id, master id (+0xA4) and behaviour keys; they differ in HP and a few tuning fields. char_id band is the only available discriminator for *where* a copy is used (HYPOTHESES, not proven; the other agents' mission/arena data must confirm):
- **A 20680-22499 main-story originals**: highest HP (Shingen 17000, Ibaraki 17000, Hiruko 17000, Sukuna 18000-21000, Daidara 20000, Demons of Pride/Envy 20000).
- **C 23000-23999 flat 7000-11000 copies of the same bosses** (Honda 23189=9000 and 23208=7000, Yoshitsune 23350, Yoritomo 23372, Yoshinobu 23394, Takasugi 23145, Kunimatsu 23110, Hanzo 23249/23290, Jubei 23706 all flat 7000) with the same master id: alternate/rematch/duel copy, plausible Battle Scroll / replay tier.
- **D 24000-25499 flat 9000 named humans** (Kondo/Hijikata/Saito 2 copies each 22-25 ids apart, Goemon, Yoichi, Tomoe, Shinran, ...): duelist/side-boss tier.
- **E 26000-30999 yokai**: Gaki Chief x21 (27120..28555, all HP 8800, same master 65434 = 21 near-identical encounter copies), Mikoshi Nyudo trio, Gaki/Thorned Gaki chains (linked).
- **G 43000-51475, +0xA4 = 0 in most**: second-generation block with duplicates of main bosses at the original HP: Ibaraki 46806 (master 2233), Hiruko 46927 (master 5035), Otoroshi 46775, Great Tengu 46899, Enenra 45930, Demon of Obsession 46239, Daidara 48614/48636, Yoshitsune 22077, Kodama/Sudama 47654-47668 (9999). Candidates for DLC / alternate-difficulty / Eternal Rift variants; the fixed ids 49000+ (+0xD8 = 150) look like a later DLC batch.
- Same-name pairs to compare when verifying story vs Scroll: Ibaraki 21175/21213 (+ 46806), Shingen 20899 (master 32150) vs 22199 (master 51399, HP 13000), Hiruko 21134/21153/22510/46927, Honda 22616/23189/23208, Hanzo 22762/22799/22836/22873 (sub-forms 3000-5000 HP) vs 23230/23249/23290, Kasha 21809/21816/21841, Mezuki 21429/21439/21467, Gozuki 21584..45084, Sukuna 21715/21728/21762.
- Full per-record data (HP, master id/family/flags, +0xA8, behaviour keys, link partners, tuning columns) is in the CSV.

## Caveats
Offsets and links are static analysis; HP meaning (+0x18) is inferred from value distribution only. ID bands for variant use are hypotheses. Everything here is reproducible with plain struct parsing of the files named above.
