# Chain hits: move hit count including chained follow-up pieces (2026-10-27, offline, game not touched)

Tool: `py -I tools/chain_hits.py <CHARACTER> [--all] [--depth 6]`, `--check` = Yoritomo self-check. Output: `notes/chain-hits-yoritomo.tsv`.
Builds on `notes/boss-chain.md` (branch cx-pc/boss-chain-13160), `yoritomo-action-fields.md`, `yoritomo-attack-values.md` (7:41-8:25 pm
sections, commit 998651667) and `tools/boss_sequences.py` (commit 49cc23017, not on main).

## Sub-table 3 decode (what changed vs boss-chain.md)
- Sub-table 3 is **not keyed by the source**. Its +0x00 word is the **target**. The source is the action whose packed block in sub-table 0
  lists the row: block = `u16 n1, n1 x hitbox-row index (sub-table 2), u16 n2, n2 x sub-table-3 row index`. PROVEN by layout (blocks
  cover sub-table 0 exactly) and by the three known combo links, all present as rows listed only by their source:
  **4E4E->4E52 (row 159), 4E60->4E62 (row 208), 4E53->4E54 (row 173)**. 0x4E5C->0x4E60 is both +0xE0 and a row.
- Row fields: +0x00..+0x0A six target words (only +0x00 used on Yoritomo private rows); +0x10 u16 = per-target number (4E64 always 29,
  4E68 53, 4E6E 73, 4E52/4E53 13; FFFF on 4E60->4E62/4E53->4E54), GUESS: AI request id; +0x42 s16 = blend frames (V: `0x35cf28.c:82-91`,
  <1 falls back to action +0x11A); +0x20 = 0x2FE (766, the sphere/distance condition) on 4E60->4E62/4E61. Other condition bytes not decoded.
- Shared rows (listed by up to 130 actions) are reactions and openers that can start from idle.

## Rule (INFERRED)
A **follow-up piece** is an action with a hit or attack flag that no shared row and no non-attack action points at. Yoritomo: 53 pieces,
incl. 4E52 4E54 4E59 4E60 4E61 4E62 4EB7. Standalone openers (4E53 4E64 4E68 4E6E 4E8A ...) are also private targets of attacks; that is
the AI picking another move and is not added. chain_min = own + forced +0xE0 chain; chain_max = best path over +0xE0 and private
links into pieces (cycle-safe, depth 6). Own hits = distinct hit ids (boss_action_record.py rule). A private link is a choice: the
data does not say how often it fires.

## Results
| Yoritomo | own | chain_min | chain_max | path |
|---|---|---|---|---|
| 0x4E4E | 1 | 1 | **2** | 4E4E-4E52 (4E52 1 hit; 4E54 after it has 0) |
| 0x4E55 | 1 | 1 | **2** | 4E55-4E59 |
| 0x4E6E | 2 | 2 | **2** | no follow-up |
Totals (moves shown, pieces folded in): Yoritomo 39 moves, own 36 -> min 39 / max 75. Shingen 62 moves, 92 -> 140/140 (22 moves gain,
mostly forced +0xE0 into 4E56 and 4E21->4E22; Shingen 0x4E4E stays 3). Otoroshi 65 moves, 65 -> 94/102. Yoshitsune 50 moves, 57 -> 57/57
(no gain). Sukuna (CE_RYOUMENSUKUNA_00) 7 moves, 6 -> 6/7.
Note with groups instead of hit ids 4E52 has 2 groups, which would give 4E4E = 3; the hit-id rule fits the field fact (2).

## Proven vs not
- Proven: sub-table 3 layout and source binding, the 3 known links, +0xE0 finish link, +0x42 blend read.
- Inferred: "piece" rule, that the field 2nd hit of 4E4E/4E55 is 4E52/4E59 (could be 4E53 for 4E4E, also 1 hit), that forced +0xE0
  targets with a stray hit row (Shingen 4E22 motion 11, Otoroshi 4E22/25/28 motion 31) really hit.
- Live fallback (if needed): the ACTLOG already logs transitions; count 4E4E->4E52 and 4E55->4E59 successions per run, and add the
  read-only hook at RVA 0x2B4A3C from `yoritomo-hit-source-22305.md` section 4 (one line per hit-record creation = one hit).

## Applied to the menu data (human #41382, 2026-10-27, offline)
The Boss moves menu '1 hit / 2 hits / ...' groups come from `data/boss-action-record.tsv` (`tools/boss_action_record.py`), which Training
loads at runtime as `Nioh3Training_action_records.tsv` next to the ASI (`action_records_game.h`, joined by action id + motion + anim hash in
`ArApply`, then `move_catalog.h` N3CatMerge -> kN3MoveFacts -> `move_cats.h`). Hit data is NOT compiled into any ASI (hand
`boss_move_facts.h` never seeds hits), so no ASI rebuild was needed. GAP: the 1.7.8x releases ship only the three ASIs, not this TSV.
`boss_action_record.py` now folds the best chain path of every action (`chain_hits.chain_paths`, depth 6) into hit_count /
damage_per_hit (follow-up damage appended after own) / zero_power. Rule changes vs the table above (all INFERRED):
- motions 11 / 31 (locomotion / walk) are never a piece;
- a sub-table 3 combo link only into a piece with an attack block (+0xE0 forced links still take a hit-only piece). Drops Otoroshi
  4E36/4E4E/4EF2 -> 18C7 (no attack block, 5 alternate damages at one frame = grab/throw damage) and Yoritomo 4E81/82/85/86 -> 4EA7;
- a move whose every hit has power 0 is a catch check and ends the chain (Otoroshi 4E4E -> 4E6D throw only on a landed catch).
Self-checks: Yoritomo 4E4E 200/150|210, 4E55 250/175|180, 4E6E 250/275 (2 each), Shingen 4E4E 3, Otoroshi 4E4E 1 zero-power.
All 108 characters: own 3052 -> chain 3776 hits, 534 rows gain (70 characters); per row own/chain/path in `notes/chain-hits-gains.tsv`.
Tests: `py -I tools/test_chain_hits.py` (rules + archive facts), `mod/Nioh3Training/tests/action_records_test.c <tsv>` (2 chained asserts).
