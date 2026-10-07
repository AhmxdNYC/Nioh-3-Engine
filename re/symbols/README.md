# re\symbols: the persistent Nioh 3 symbol database

`symbols.tsv` is the **single source of truth** for every name we have given to something in `Nioh3.exe` (Steam v2.02,
SizeOfImage 0x52FB000, PE timestamp 0x6AA7C106). It accumulates over time toward a full decompilation. The Ghidra project,
x64dbg, IDA and our own tools all get their names from it. Never rename things only in Ghidra: those names would be lost on
the next rebuild, and nobody else would see them.

| file | what | edit? |
|---|---|---|
| `symbols.tsv` | the database (UTF-8, tab-separated, LF) | **yes**, by hand or by agents |
| `build_exports.py` | validates `symbols.tsv` and writes the three exports below | only to change the formats |
| `symbols.csv` | plain CSV copy (same columns) | generated |
| `symbols_x64dbg.txt` | x64dbg script: `lbl` + `cmt` by module RVA | generated |
| `symbols_ida.py` | IDA script (names, function comments, struct declarations) | generated |
| `apply.cmd` | applies `symbols.tsv` to the Ghidra project and **saves** it (via `re\scripts\N3ApplySymbols.java`) | no |
| `last_apply_report.txt`, `last_apply.log` | output of the last `apply.cmd` run | generated |

## Format

The header line is `rva	kind	name	signature	description	confidence	source`. Lines starting with `#` are ignored.

| column | rules |
|---|---|
| `rva` | Lower-case hex with `0x`. It is an **RVA**: image base 0x7FF74F410000 in the Ghidra project, so `FUN_7ff74f9bf410` is `0x5af410`. For `struct_field` rows this column holds the **field offset** instead. |
| `kind` | `function` (the function **entry**), `label` (a site *inside* code: a patch site, call site or loop), `global` (a variable, object or pointer slot in .data), `data` (constants, immediates, IAT slots), `vtable`, `string_table` (string, option or descriptor tables), `struct_field`. |
| `name` | Engine-style CamelCase, **unique across the whole file**. Use a verb plus an object for functions (`DynFpsGovernorTick`, `MeshletCheckRequestLoad`, `GlyphAtlasReset`). Prefix globals with `g_` (`g_GameMainPtr`, `g_CfgRenFps`). Name a site after its function plus what happens there (`RenderSubmit_FrameLatencySpin`). A `struct_field` is named `Struct::Field` (`GameMain::TargetFps`). Use identifier characters only. |
| `signature` | A C prototype for functions (`float GetDeltaTime60(void)`) or a type for data and fields: `u8 u16 u32 i32 u64 i64 f32 f64 bool ptr T*`, or arrays such as `u32[10]`. The apply script types globals and struct fields from the first token, so put the type first. |
| `description` | One line, no tabs. Say what it does or holds, the live value at dump time, and the patch or cave ids that touch it. When sources disagree, keep the more specific or later claim and name the other one (`aka ...` and `also: ...` were added when duplicate rows were merged). |
| `confidence` | `high`: verified by decompilation, a working patch or a live probe. `medium`: strong inference. `low`: a guess, or "likely". |
| `source` | The notes file plus its section, for example `notes/frame-timing.md §Governor` or `patcher/cpu_caves.fsx`. Sources are separated by `;`. |

There is one row per address, except `struct_field` rows, which are unique per (struct, offset).
A struct that is reached through a static pointer gets a `global` row for the pointer or object, plus `struct_field` rows for its members.
For example, GameMain is the object at `0x4759090`, pointed to by `[0x4759910]`. Its members include `GameMain::TargetFps` at `0x614`.

## Adding or changing symbols

1. Edit `symbols.tsv`. Add rows anywhere; ordering is cosmetic: non-struct rows come first by RVA, then struct fields grouped by struct.
   To rename something, change its name in place and keep the RVA. A new name is picked up as a rename, not as a second label.
2. Run `py re\symbols\build_exports.py`. It rejects bad kinds, bad RVAs, RVAs outside the image, non-identifier names,
   duplicate names, duplicate addresses and duplicate struct offsets, and then regenerates the exports.
3. Run `re\symbols\apply.cmd` to push the changes into the Ghidra project. Use `apply.cmd dryrun` first if you want to preview them.
   Check `function.asLabel` and the `W` lines in the report: they mean a `function` row is not at a function entry.
4. Mention the change in the notes file you took it from, if you changed the claim there.

Before writing a `function` row, check that the address is an entry point. `re\out\functions.tsv` lists the Ghidra and .pdata starts,
and `re\decomp.cmd 0x...` prints `function NAME @ start`. A patch site inside a function is a `label`, and the containing function gets its own `function` row.

## Applying to Ghidra

```
re\symbols\apply.cmd            :: apply + save (takes re\ghidra_proj\.n3query.mutex like decomp.cmd; waits up to 60 min)
re\symbols\apply.cmd dryrun     :: preview only: the project is opened -readOnly and the transaction is rolled back
re\symbols\apply.cmd other.tsv  :: apply another file in the same format
re\decomp.cmd 0x5af410          :: check: the header line and callers/callees now show the new names
```

`apply.cmd` runs `analyzeHeadless re\ghidra_proj Nioh3 -process Nioh3.dump.exe -noanalysis -postScript N3ApplySymbols.java`.
It does not use `-readOnly`, so the project is saved, and the heap is pinned to `GHIDRA_HEADLESS_MAXMEM=4G`.
It shares the query mutex, so it never runs at the same time as an agent's `decomp.cmd`. **Never delete the mutex while another java.exe query is running.**
The project is locked while it runs (about 1–2 min), so do not open it in the Ghidra GUI at the same time.
**Disk:** saving writes a new copy of the program DB, a `db.N.gbf` file of about 1.2 GB, before it releases the old one.
`apply.cmd` therefore refuses to start with less than 3 GB free on C:, returning exit 4.
If the save fails anyway, it returns exit 5, and the project is left exactly as it was because the old DB files are kept.
Exit codes: 2 = mutex busy for 60 min, 3 = the script failed, 4 = low disk, 5 = the save failed.

What `N3ApplySymbols.java` does:
- **function**: renames the function at the RVA (USER_DEFINED) and writes the plate comment.
  If there is no function there and the address is clearly code (a primary .pdata entry, or an existing instruction that no function covers), it creates the function.
  If the RVA lies inside another function, it places a label instead and adds a warning.
  If the function already had a meaningful name that is not ours (for example a demangled CRT name), that name is kept as a secondary label.
- **label / global / data / vtable / string_table**: creates the label, or renames the label our previous run created, and makes it primary unless the primary is a function or a user name.
  It sets the EOL comment. `global` and `data` rows with a simple type get that data type, but only on bytes that are still undefined.
- **struct_field**: creates or extends the structure `/N3/<Struct>` in the program's data types, and names, types and comments the field.
  A field that overlaps an existing field at a different offset is skipped with a warning.
  To use a struct, retype a pointer to it in the decompiler, for example the result of `*(u64*)0x4759910` to `GameMain *`.
- Every comment is wrapped in a `[N3sym] <name> ... [/N3sym]` block. Re-running replaces only that block, and any other comment text is kept.
  This makes the script **idempotent**, and it **never deletes** anything.

## Other tools

```
py re\symbols\build_exports.py        :: regenerate symbols.csv / symbols_x64dbg.txt / symbols_ida.py
```
- **x64dbg**: start the game normally, attach, then run `scriptload "C:\Users\Ahmad Hamza\nioh3-engine\re\symbols\symbols_x64dbg.txt"` and `scriptrun`, or use the Script tab's Load and Run buttons.
  The lines are `lbl Nioh3.exe:$5AF410, "DynFpsGovernorTick"`. In x64dbg, `module:$rva` means an RVA within that module, and numbers are hex.
  `{}` and `"` are removed from comments, because x64dbg would treat them as string formatting. Struct fields are not exported to x64dbg.
- **IDA 7.x–9.x**: File > Script file... > `symbols_ida.py`. Names and comments are applied at `idaapi.get_imagebase() + rva`, so a rebased database also works.
  It creates missing functions for `function` rows, and calls `idc.parse_decls` on the struct declarations, which are padded C structs.
- **Our own tools** (F# patchers, cheat tables, notes) should read `symbols.csv` or `symbols.tsv` instead of hard-coding names.

## Caveats

- These RVAs are for **v2.02 only**. After a game update, re-derive the addresses (for example with the AOB patterns in `mod\Nioh3EngineFix\patches.json`) before reusing any names.
- `low` rows are hypotheses. Raise a row to `medium` or `high` only after decompiling it or confirming it with a live probe, and say which you did in the description.

## Contributing from parallel work (incoming/)
Each agent or session writes only its own file `incoming\<area>.tsv` (same header and columns as `symbols.tsv`).
`py re\symbols\merge_incoming.py` folds them in (new RVAs appended; known RVAs keep their name and get an
"also (<file>): ..." note; confidence only goes up), moves the processed files to `incoming\merged\`, and
regenerates the exports. Then run `re\symbols\apply.cmd` to push the names and comments into the Ghidra project
(needs >= 3 GB free on C:). `--dry` shows what would change.
