# Boss drops: reward-set table decode (static, 2026-10-08)

Game not run, mod not touched. Output: `notes\boss-drops.csv` (186 tier-1 bosses of boss-enemy-params.csv; 55 have a reward-set link, 131 have +0xA8 = 0). Result is PARTIAL: the join chain is proven, the "drop lot / probability / per-difficulty" semantics are NOT in these tables.

## Table `archive_03\23\DAC95BA0...` (84 rows x 84 B, header 00220420,84)
u32 columns: [0] cum offset into a variable-length side blob (+10..17/row; blob not located) · [1] second cum offset · [2] 1250/6500/5251/1470/.../0xFFFFFFFF (amrita-like value; -1 on all boss-flagged rows) · **[3] reward-set key (= enemy +0xA8)** · [4],[6],[7],[8] random-looking 16-24 bit values (hash/seed, unproven) · **[5] flag 0/1/3** (1/3 = boss rows, rows 48-66,70-77,83) · **[9],[10],[11] triple 1/5/7, 2/10/15, 5/25/37** (scales with boss importance: minor foes / mid / main yokai bosses; probably chance or level steps, unproven) · **[12] hash = item-record w74** (49 distinct, all found) · [13..20] small id/size pairs, [20]=4000 const.

## Item side: `archive_03\2E\D4E1DB3B...` (416 B records, 3362)
[12] resolves to a record of category w25 = 900000000 (119 records, idx 3169..3287, w0 54963..56419, w1 131590..134149; **w2/w3 name keys = 0xFFFFFFFF, so no static English name**). Fields: w4=w5 (species/type 2..56), w20 (2..32, rarity/level candidate), w33 (64/416/896/928), w34/w36 (400..2100 / 250..1300 numbers), w74 hash. Hash 3051336956 (0xB5DFB4FC) is a shared default used by 6 records (idx 3203, 3287, ...) and by 19 reward rows = generic fallback item. 9 reward rows have unique items (e.g. Great Tengu -> idx 3220, Hino-enma etc. via the default).
Name: the only soul-core text is the template `{} Soul Core` (item text group 8F\676FE6, id 17639921) and `Mortal Soul Core` (19868437); the CSV `item_name` is therefore INFERRED as "<boss> Soul Core" (confidence L). Two w26/w27 text keys (17844063, 18269809) exist but their English string is empty.

## CSV columns
char_id, boss_name, reward_set_id (= +0xA8; 0 = none), item_id (= item w1), item_name (inferred), chance (EMPTY: no probability field proven), qty_level (reward-set triple col9/10/11), condition (boss-flagged = col5, generic fallback marker), confidence (L / n/a), then diagnostics: rs_row, rs_amrita_col2, rs_hash_col12, item_rec_idx, item_w0, item_species_w4, item_rarity_w20.

## Not resolved
Drop probabilities, quantities, per-difficulty/NG+ rows (no such columns found; per-variant differences are separate enemy records), the side blob behind cols 0/1 (likely the variable-length lot list), real item names (likely generated from the enemy name at runtime). Main-story bosses with +0xA8 = 0 (131) get rewards from scripts/mission tables (boss-rewards.md). Next: runtime hook on AddDropTable/ForceDropItem to read table id -> items.
