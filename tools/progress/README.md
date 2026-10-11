# Progress charts and the code map

These scripts redraw every chart in `docs/progress/` and the purpose-region table behind
[docs/code-map.md](../../docs/code-map.md). They use this repository and **your own copy of the game**. Nothing from the
game is published here. The published tables in `data/` hold only addresses, sizes, our own names and the
few fields of our own profiler captures that the charts need.

## Prerequisites

- **64-bit Python 3.11 or newer**. `py -I` below is the Windows Python launcher in isolated mode. The chart scripts also run
  elsewhere with `python3`, but making the dump needs Windows.
- `numpy`, for `regions.py` only: `py -m pip install -r tools\progress\requirements.txt`. The other scripts need only the standard library.
- **Nioh 3 from Steam, v2.02** (PE timestamp `0x6AA7C106`). Every RVA in `data/` and `symbols/` is for that build.
- A dump of the running game: `data\local\Nioh3.dump.exe` (see below). `data\local\` is gitignored. Never commit or share anything in it: a dump of the running game also contains your Windows user name, your Steam ID and save paths.

## Make the dump (once per game build)

```
py -I tools\progress\dump_nioh3.py
```

1. Turn off every mod (ASI loaders, patchers, our engine fix), so that the code you dump is the game's own.
2. Start the game from Steam and wait for the title screen.
3. Run the command. It takes a few seconds and writes about 87 MB.

The Nioh3.exe on disk is wrapped by Steam's DRM stub, which decrypts the code only in memory. The script opens the game with read-only
access (`PROCESS_VM_READ`). It copies the module image page by page with `ReadProcessMemory`, so that file offset ==
RVA. Then it patches the PE header so other tools agree: `ImageBase` = the load address, and each section's raw offset and size
equal its virtual address and size. It never writes to the game process. At the end it checks that `.text` is decrypted and that
the build matches `data\progress_meta.tsv`. If either check fails, it prints a warning.

**Status:** the header-patching step was verified offline (it rebuilds our own dump byte for byte), but the script
has not yet been run against a live game. If it fails or the checks warn, please open an issue with its output.

To keep the dump somewhere else, pass `--dump PATH` to any script, or set `NIOH3_DUMP`. `NIOH3_LOCAL` moves the whole
local folder (the dump and `strings.tsv`).

## One command per chart

Run these from the repository root. The times were measured with the dump on an SSD.

| output | command | time |
|---|---|---|
| `data\regions.tsv` (purpose regions; the blocks of the full-game chart and the table in docs/code-map.md) | `py -I tools\progress\regions.py --md` | ~17 s (+1 s the first time, to build `data\local\strings.tsv`) |
| `docs\progress\full_game.svg`, `performance.svg`, `gpu.svg`, `badges.md` | `py -I tools\progress\progress_treemap.py` | ~2 s |
| `docs\progress\renderer_understood.svg`, `renderer_badge.svg`, `renderer_table.md` | `py -I tools\progress\renderer_progress.py` | ~1 s |
| `docs\progress\renderer.svg` | `py -I tools\progress\progress_chart.py` | ~1 s |
| coverage numbers only (no file) | `py -I tools\progress\perf_coverage.py --self` | ~1 s |

- Order: run `regions.py` first only if you want new regions. `progress_treemap.py` reads `data\regions.tsv` as published.
  After a symbols merge, rerun all three chart scripts.
- `--check` (progress_treemap, renderer_progress) prints the numbers and writes nothing. `--out DIR` (or
  `NIOH3_PROGRESS_OUT`) writes the charts somewhere other than `docs\progress`.
- `regions.py --why 0xRVA` shows the evidence for one region. `--survey` prints a per-MB evidence digest.
  These two print game strings to your terminal, which is fine. Do not paste them into the repository.
- `perf_coverage.py` options: `--precise`, `--list N` / `--incl N` (top unnamed functions), `--gpu`, `--tsv FILE`,
  `--before` (ignore pending `symbols\incoming\*.tsv`).

## What is in `data/`

| file | contents | made by |
|---|---|---|
| `functions.tsv` | rva and size of every function Ghidra found | our Ghidra export |
| `leaf_ranges.tsv` | code with no `.pdata` entry, mapped to the function that owns it | our disassembly of the `.pdata` gaps |
| `imports.tsv` | imports: DLL, function, IAT rva, calling functions | our Ghidra export |
| `string_xrefs.tsv` | for each game string that has references: rva, encoding, referencing functions, pointer slots. **No text.** | our Ghidra script |
| `partition.tsv` | renderer partition: rva, subsystem, depth, size, region | our renderer map |
| `area_overrides.tsv` | manual area overrides for the treemaps | hand-written |
| `regions.tsv` | purpose regions; string evidence is written as `s@0xRVA`, never as text | `regions.py` |
| `gpu_note_rvas.tsv` | function starts cited in our GPU notes, with citation counts | our notes |
| `progress_meta.tsv` | GPU passes mapped, renderer packages that have a note, game-build identity | our notes |
| `profiles/ally/sp_*.txt` | ROG Ally sampling profiler: thread-group sample counts and `fn 0xRVA` rows | our ASI profiler |
| `profiles/pc/cpuprof-*` | PC sampler: per-thread sample totals, leaf RIPs, `Nioh3+RVA` rows | our external sampler |
| `profiles/gpu/*` | one Ally GPU capture (per-PSO GP2 rows), our index of its shader digests, our names for exe-embedded shaders | our GPU profiler |

**Local only (`data\local\`, gitignored).** Your dump, plus `strings.tsv`, which `make_strings.py` builds from the dump.
It contains the game's text, so it is never published. `regions.py` builds it automatically on first use. On v2.02 it is
byte-identical to our table.

Not ported, because their outputs are published as tables: the Ghidra export scripts (`functions.tsv`, `imports.tsv`, string
references), the leaf-range disassembler (`leaf_ranges.tsv`) and the renderer partition builder (`partition.tsv`).
`refresh_data.py` is for maintainers only. It rebuilds `data/` from the private working tree.

## Expected results

On v2.02 with the published `data/`, every file in `docs\progress\` should come out identical to the committed one. The only exception is the date
in `renderer_understood.svg`, which is the day you ran it. If yours differ, the usual causes are another game build (the scripts warn about this) or
changes to your `symbols\symbols.tsv`. `.data` in a dump holds live values, so `regions.tsv` from your own dump can differ slightly from ours.
