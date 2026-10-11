# Contributing

Help is welcome, especially with **decompiling**: giving names and descriptions to the game's code in
[`symbols/symbols.tsv`](symbols/symbols.tsv). Bug reports and hardware test results for the mods are welcome too.

## Ground rules

- **Never upload game files.** That includes `Nioh3.exe`, memory dumps, extracted archives, shaders or saves.
  Share addresses, names, byte patterns and descriptions only.
- Everything is for **Steam Nioh 3 v2.02** (PE timestamp `0x6AA7C106`, SizeOfImage `0x52FB000`).
  Addresses from any other version do not belong in `symbols.tsv`.
- Be honest about certainty. A guess is fine as long as it is marked `low`.

## Decompiling: get set up

1. **Your own copy of the game, v2.02.** The shipped `Nioh3.exe` is wrapped in Steam's DRM stub, so disassemblers
   see encrypted code. Analyse a decrypted copy made from **your own install**: run
   `tools\progress\dump_nioh3.py` while the game sits at the title screen. It writes a memory-layout dump
   (file offset equals RVA), the same layout this project uses. Keep it on your machine; do not share it.
2. **A disassembler.** Ghidra (free), IDA, or x64dbg for live debugging.
3. **Load the existing names** so you start from what is already known:
   - Ghidra: copy [`symbols/ghidra/N3ApplySymbols.java`](symbols/ghidra/N3ApplySymbols.java) into your script folder,
     run it from the Script Manager, pick `symbols.tsv`, then save. Re-running it is safe; it updates its own labels.
   - IDA / x64dbg: use the exports in the `nioh3-symbols-v2.02.zip` release (or build them with
     `py symbols/build_exports.py`).

Rows are **RVAs** (offsets from the image base), so they work whatever base your tool picks.

## Catch up on what we know

- [`research/`](research/README.md): the notes behind the names. Most `source` entries in `symbols.tsv` cite a
  `notes/x.md` path; [`research/SOURCE_MAP.tsv`](research/SOURCE_MAP.tsv) maps those to the published notes
  (about 91 % of cited rows resolve). Start with [`research/renderer/00-map.md`](research/renderer/00-map.md).
- [`docs/subsystems/`](docs/subsystems/) and [`docs/code-map.md`](docs/code-map.md): short summaries per area.

## Regenerate the charts yourself

Every chart in `docs/progress/` and the region table behind `docs/code-map.md` can be rebuilt from this repo plus
your own copy of the game:

1. With the game at the title screen (mods off), run `py -I tools\progress\dump_nioh3.py`. It writes
   `data\local\Nioh3.dump.exe`, which is gitignored. **Never commit or share it**: it contains your Windows user
   name and Steam ID.
2. `py -m pip install -r tools\progress\requirements.txt` (numpy, only `regions.py` needs it).
3. Run `py -I tools\progress\progress_treemap.py`, `renderer_progress.py` and `progress_chart.py` (1-2 s each).
   `regions.py` (about 17 s) rebuilds the code map. Include the regenerated charts in your pull request.

Details: [tools/progress/README.md](tools/progress/README.md). The same dump works as the input for Ghidra or IDA.

## What to work on

Names that explain where the frame time goes are worth the most. The least understood parts of the renderer
(by code size, as of 2026-10-10) are:

| package | understood |
|---|--:|
| meshlet / geometry | 8.5 % |
| materials | 9.4 % |
| effects | 11.3 % |
| post-processing | 11.6 % |
| backend (D3D12) | 12.1 % |
| UI | 12.9 % |

[`docs/code-map.md`](docs/code-map.md) shows which address ranges hold which subsystem, and
[`docs/subsystems/`](docs/subsystems/) describes the parts we already understand, with their key addresses.
Gameplay systems (AI, missions, items, the save format) are open territory too.

Good first contributions:

- Replace a placeholder name (`PauseSpin_*`, `MainHot_*`, `ScalarDeletingDtor_*` stubs) with a real one once you know what the code does.
- Raise a `low` row to `medium` or `high` after you verify it, and say how in the description.
- Add `struct_field` rows for a structure you mapped.
- Name the callers or callees of an already-named hot function.

## Sending a change

1. Edit `symbols/symbols.tsv`. The column rules are in [`symbols/README.md`](symbols/README.md). Names are unique
   CamelCase; one row per address.
2. Run `py symbols/build_exports.py`. It rejects bad rows (duplicate names or addresses, RVAs outside the image,
   bad kinds) and regenerates the exports.
3. Open a pull request. In each row's `description` say what the code does, and in `source` say how you know
   (for example `decompiled`, `x64dbg breakpoint`, or a link to your write-up). Small PRs review faster.

Longer findings (how a subsystem works, a structure layout, a performance discovery) can go in `docs/subsystems/`
as a Markdown file in the same style as the existing ones.

## Mod bugs and hardware reports

Open an issue with your GPU, CPU, OS (Windows or Proton), mod version, and `Nioh3EngineFix.log` or
`Nioh3BossRematch.log`. Results from **NVIDIA, Intel Arc and Intel CPUs** are especially useful, since nobody on the
project has tested them yet.
