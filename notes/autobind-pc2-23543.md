# Auto-binding action files (pc2 #23543, offline, 2026-10-22)

Step 1 - file -> character, fully automatic (`tools/auto_bind.py`, output `notes/boss-action-bind-auto.tsv`):
every archive_01 file with the 16-sub-table action layout (363 files) is matched to the setting DB(s) (ObjectDatabaseFile in root.rdb, read via data/rdb_list.tsv + package/*.fdata)
that contain >= 50% of its action object hashes (+0xF0). Result: **257 of 363 files bound** (256 to a named character), 94 -> 257 vs the old CSV (which missed e.g. CN_JUBE 490-row file,
CE_ENENRA, CE_GOZUKI, CE_WAIRA, CE_IPPONDATARA main file). Runs in ~55 s, no per-character input.
Step 2 - boss name -> character (`tools/auto_bind_names.py [--eval]`): codex key (boss-codex.csv) / exact name-token / unique fuzzy spelling match; no hand dict.
Result: **37 of 83 boss names resolve**. Scored against the old hand dict (37 entries): 26 identical, 3 different (Takeda Shingen CN_ vs CE_ file, Great Tengu TENGU vs OTENGU,
Gaki Chief GAKI vs GAKIDAISHO: both candidates are real files), 8 missed (Mikoshi, Himiko, Hanzo, Munenori, Kasha, Nuppeppo, Joro-gumo, Oni = non-obvious naming).
Hard limit: **~44 boss names (Kagetoki, Kondo, Hijikata, Ito Ittosai, Fuma Kotaro, Okiku, Ibaraki Doji, Demon of X, Enki-like DLC duelists...) have no character of their own
with an action file** - human duelists use shared CP_HUMAN_* / CN_ movesets selected by weapon, so a name-based offline binding cannot resolve them. The numeric boss chr id -> character
key table is not in any extracted data (runtime only). Real fix = runtime: read the spawned boss's own action set ([[actor+3B0]+8]+0x178, notes/boss-action-table-re.md) and look up its
action object hashes in the auto-bind table (hash -> file), which needs no names at all. NOT built (needs the game).
