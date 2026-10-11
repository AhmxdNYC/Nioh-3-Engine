# Nioh 3 symbol database

`symbols.tsv` gives names to functions, globals and data in `Nioh3.exe` from **Steam v2.02** (SizeOfImage `0x52FB000`, PE timestamp `0x6AA7C106`). It has about 16,000 rows. It grows over time. It is the single source of truth. The other files are generated from it.

These addresses are for v2.02 only. After a game update, re-derive them before reuse. The byte patterns in the engine fix's `patches.json` are a good starting point.

Some rows are guesses. Check the `confidence` column.

## Files

| file | what | edit? |
|---|---|---|
| `symbols.tsv` | the database (UTF-8, tab-separated, LF) | yes |
| `build_exports.py` | validates `symbols.tsv`, then writes the exports below | only to change formats |
| `symbols.csv` | CSV copy, same columns | generated (in the release zip) |
| `symbols_x64dbg.txt` | x64dbg script (labels and comments by module RVA) | generated (in the release zip) |
| `symbols_ida.py` | IDA script (names, comments, struct declarations) | generated (in the release zip) |
| `ghidra/N3ApplySymbols.java` | Ghidra script that applies `symbols.tsv` | no |

## Column format

The header is `rva	kind	name	signature	description	confidence	source`. Lines starting with `#` are ignored.

| column | rules |
|---|---|
| `rva` | Lower-case hex with `0x`. An **RVA**, not a full address. Add your module base. In Ghidra with image base `0x7FF74F410000`, `FUN_7ff74f9bf410` is `0x5af410`. For `struct_field` rows it holds the **field offset**. |
| `kind` | `function` (an entry point), `label` (a spot inside code, such as a patch site), `global` (a variable or pointer slot), `data` (constants, IAT slots), `vtable`, `string_table`, `struct_field`. |
| `name` | Engine-style CamelCase. **Unique in the whole file.** Functions: verb plus object, such as `DynFpsGovernorTick`. Globals start with `g_`. A label is the function name plus what happens there. A struct field is `Struct::Field`. Identifier characters only. |
| `signature` | A C prototype for functions, or a type for data: `u8 u16 u32 i32 u64 i64 f32 f64 bool ptr T*`, or an array like `u32[10]`. Type first. |
| `description` | One line, no tabs. What it does or holds, and which patches touch it. |
| `confidence` | `high`: confirmed by decompilation, a working patch or a live probe. `medium`: strong inference. `low`: a guess. |
| `source` | Where the claim comes from. Separate several with `;`. |

There is one row per address. `struct_field` rows are unique per struct and offset. A struct reached through a static pointer gets a `global` row for the pointer and `struct_field` rows for its members.

## Using the exports

**x64dbg.** Attach to the running game. Run `scriptload "path\to\symbols_x64dbg.txt"` then `scriptrun`, or use the Script tab. Lines look like `lbl Nioh3.exe:$5AF410, "DynFpsGovernorTick"`. Struct fields are not exported to x64dbg.

**IDA 7.x to 9.x.** File, Script file, `symbols_ida.py`. It applies names and comments at `imagebase + rva`, so a rebased database works. It creates missing functions for `function` rows and declares the structs.

**Ghidra.** Copy `ghidra/N3ApplySymbols.java` into a Ghidra script folder, run it from the Script Manager and pick `symbols.tsv`, then save the program. It names functions (creating them where needed), places labels, adds comments and builds the structs under `/N3`. It is safe to re-run; it only replaces its own labels and comments. Headless: `-postScript N3ApplySymbols.java <path\to\symbols.tsv>`.

**Your own tools.** Read `symbols.tsv` or `symbols.csv` instead of hard-coding addresses.

## Contribute

Setup and what to work on: [../CONTRIBUTING.md](../CONTRIBUTING.md).

1. Edit `symbols.tsv`. Add rows anywhere. Ordering is cosmetic. To rename, change the name in place and keep the RVA.
2. Before adding a `function` row, check that the address is an entry point. A spot inside a function is a `label`. The containing function gets its own `function` row.
3. Run:

```
py build_exports.py
```

`build_exports.py` rejects:

- bad kinds
- bad RVAs
- RVAs outside the image
- names that are not identifiers
- duplicate names
- duplicate addresses
- duplicate struct offsets

Then it regenerates `symbols.csv`, `symbols_x64dbg.txt` and `symbols_ida.py`.

4. Send a pull request with `symbols.tsv` and the regenerated exports. Say in the `description` how you checked the row, and put the reference in `source`.

Rules for changes:

- When two claims disagree, keep the more specific or later one and name the other (`aka ...`, `also: ...`).
- Raise `low` to `medium` or `high` only after decompiling the code or confirming it with a live probe. Say which one in the description.
- Do not delete rows without a reason in the pull request.
