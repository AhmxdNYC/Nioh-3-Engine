# What is not in this repository, and why

This is a curated public copy of a private development repository. It contains only our own work: mod source code
and tests, installer and release scripts, documentation, our own reverse-engineering notes, symbol names, address
(RVA) tables and charts. The following were left out on purpose and must never be added (pull requests that add
them will be closed):

| Left out | Why |
|---|---|
| `Nioh3.exe`, memory dumps, game DLLs, `.pak` / `.rdb` / `.fdata` archives | Koei Tecmo's copyrighted files. Use your own install. |
| Extracted game shaders and anything built from them (`.dxil`, `.bin`, `.ll`, shader `.file` containers) | game-derived |
| Extracted game text and archive listings (string tables, RDB file lists, archive selections, `lfm` lists) | game-derived |
| Raw decompiler output (pseudo-C exports, full Ghidra function dumps, `symbols_ida.py` style generated dumps) | game-derived code listings; the notes quote only short, explained fragments |
| Captures, profiler traces with personal paths, screenshots, benchmark raw logs | personal data and size; the summaries are in `docs/` |
| Save files and save backups | personal data |
| Third-party binaries (OptiScaler, ASI loaders, other mods' DLLs, prebuilt `.lib` files) | not ours to redistribute here; release zips credit and license the ASI loader separately |
| Device / coordination tooling, logs, handoffs, tokens, user names, Steam IDs | private |

Address tables in `data/` and `symbols/` hold only RVAs, sizes, counts and our own names and descriptions: no game
bytes, no game text. Tools in `tools/progress/` rebuild local data from **your own** copy of the game into
`data/local/`, which is gitignored. Never commit or share it.

Small factual tables the mods need at run time (boss names, mission / character ids, action ids, hashes and timing
numbers in `mod/Nioh3BossRematch/*.tsv` and `mod/Nioh3Training/*.tsv`) are included because the mods cannot work
without them; they contain no game files or game text beyond names.
