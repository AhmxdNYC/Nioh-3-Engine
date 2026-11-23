# The symbol / reverse-engineering database

This is the engineer's guide to how a name for a function in `Nioh3.exe` is born, stored, shared with Ghidra, IDA and x64dbg,
and how an address from that work ends up inside the four ASI mods.
`symbols/symbols.tsv` is the single source of truth: about 16,000 rows (RVA, kind, name, signature, description, confidence, source) for Steam v2.02.
The mods never read that file. They carry their own byte patterns (`rvas.json`, `sites.json`, `patches.json`) and find every address by pattern at game start.
Most addresses the mods use have no name yet, which makes them the best first task for a new contributor (see section 7).

Read this together with [CONTRIBUTING.md](../../CONTRIBUTING.md), [symbols/README.md](../../symbols/README.md),
[PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md) and [docs/code-map.md](../code-map.md). This page does not repeat their column rules; it explains how the pieces fit.

Conventions: paths are repo-relative and given without line numbers. "Current source" means the v1.8.39 tree. Where a file exists there but not
yet in this repository's `mod/` copy, it is marked "(source not yet in the public copy)".

## 1. The build we describe

Everything is for one executable: Steam Nioh 3 **v2.02**, PE TimeDateStamp `0x6AA7C106`, SizeOfImage `0x52FB000`.
All addresses in the database and in the mods are **RVAs** (offset from the module base), so the load address does not matter.

The shipped `Nioh3.exe` has its `.text` encrypted by the Steam DRM stub. The code is only plain after the stub has run, so:

- Static tools need a decrypted image. `tools/progress/dump_nioh3.py` reads the running game's module (read-only, `ReadProcessMemory`) and writes a
  memory-layout PE where file offset equals RVA. You make it from your own install and never share it.
- The mods wait for decryption before they scan (`[General] DecryptWaitSeconds` in `mod/Nioh3EngineFix/Nioh3EngineFix.ini`, default 60).
- A second known variant exists: the same v2.02 code with the Steam wrapper removed (SizeOfImage `0x52C1000`). `mod/shared/known_build.h`
  (source not yet in the public copy) classifies the module as Steam, stripped or unknown. The stripped case is accepted by hashing the first 64 KB of mapped `.text` (SHA-256, constant in the header).

## 2. The database: `symbols/symbols.tsv`

### Shape and numbers

Header: `rva  kind  name  signature  description  confidence  source`. Lines starting with `#` are comments. UTF-8, tab-separated, LF.
Column rules are in [symbols/README.md](../../symbols/README.md). Counts in the current source (2026-11):

| measure | value |
|---|---|
| data rows | 16,125 |
| `function` | 13,827 |
| `label` (a site inside code) | 1,125 |
| `struct_field` | 642, across 115 structs (`GfxDevice`, `RenderConfig`, `SynCtx`, `KtglContext`, `GlyphAtlas` are the largest) |
| `data` / `global` / `vtable` / `string_table` | 217 / 161 / 101 / 52 |
| confidence `high` | 10,964 |
| confidence `medium` | 4,470 |
| confidence `low` | 691 |

The public `symbols/symbols.tsv` is a snapshot of this file and can lag it by a few rows.

How the rows came about, by the `source` column: more than 8,500 rows cite one of two auto-naming scripts (RTTI-derived names such as
`Class::vfunc_N` and `ctor_or_dtor_*`, string-anchored `Str_*` names, and renderer auto-names). Roughly 1,260 rows are RTTI constructors and vtable slots and about 590 are `Str_*`.
About 5,500 rows cite a research note first (`notes/<topic>.md`, mapped to the published copies by [research/SOURCE_MAP.tsv](../../research/SOURCE_MAP.tsv)).
The auto-named rows are mostly `high` for RTTI facts and `medium` for string anchors; they say what a function is attached to, not what the game does with it.
Hand-named rows from decompilation or live probes are the valuable ones.

Overall coverage from the last recount (2026-10-24; see the progress notes under [research/](../../research/README.md) and `docs/progress/`): 12,679 of 211,206 functions
(every `.pdata` start) have a real name, 6.0 %, which is 9.3 % of code bytes. Code that shows up in CPU and GPU profiles is named; most of the rest is not.
Auto-named `Class::vfunc_N` rows are not counted as understood.

### Kinds, in practice

- `function` is an **entry point**. Check before you write one: the address must be a `.pdata` start or a real prologue.
- `label` is a spot inside a function: a patch site, a call site, a spin loop. Name it `<Function>_<what happens there>`, for example `WaitDispatcherDrainSpin_Loop`.
  The containing function gets its own `function` row.
- `global` is a variable or pointer slot in `.data`; names start with `g_`. `data` is for constants and IAT slots.
- `struct_field` rows hold the **field offset** in the `rva` column and are named `Struct::Field`. A struct reached through a static pointer gets a `global` row for the
  pointer plus `struct_field` rows. Example: the game-main object, whose target-FPS field is `GameMain::TargetFps` at offset `0x614`.
- `vtable` and `string_table` are for those objects.

Names are CamelCase, **unique across the whole file**, and use identifier characters only (`::` is allowed for RTTI names and struct fields).
One row per address; one row per (struct, offset) for fields.

### Confidence levels

| level | meaning | how to earn it |
|---|---|---|
| `high` | verified by decompilation, a working patch, or a live probe | say which in the description |
| `medium` | strong inference | string reference, call-graph position, a note that argues it |
| `low` | a guess or "likely" | fine to submit, mark it |

Raise a row only after you decompiled the code or confirmed it live, and say which. When two claims disagree, keep the more specific or later one and name the other
(`aka ...`, `also: ...`). The merge tool does this automatically (below).

## 3. Tools around the database

All of these live in `re/symbols/` in the current source; the public copy has the ones marked below.

| tool | what it does | in the public `symbols/` |
|---|---|---|
| `build_exports.py` | validates `symbols.tsv`, then writes `symbols.csv`, `symbols_x64dbg.txt`, `symbols_ida.py` | yes |
| `merge_incoming.py` | folds per-contributor files from `incoming/` into `symbols.tsv` | yes |
| `apply_renames.py` | applies rename files in place (merge never renames) | yes |
| `symbols_ida.py` | generated IDA script (names, comments, struct declarations); do not edit | no (generated; ships in the release zip) |
| `apply.cmd` + `N3ApplySymbols.java` | applies the file to a headless Ghidra project | the Java script only: `symbols/ghidra/N3ApplySymbols.java` |
| `amrita/` | name lists for gameplay data (see below) | no |

### `build_exports.py`

`py symbols/build_exports.py` (add `--check` to only validate). It rejects: bad kinds, bad RVAs, RVAs beyond `SizeOfImage` (`0x52FB000`),
names that are not identifiers, duplicate names, duplicate addresses, duplicate struct offsets, and a confidence other than high/medium/low. If it passes it rewrites the three exports.
Never edit the exports by hand.

- `symbols_x64dbg.txt`: `lbl Nioh3.exe:$5AF410, "Name"` plus `cmt` lines. `{}` and `"` are stripped from comments. Struct fields are not exported.
- `symbols_ida.py`: a list of `(rva, kind, name, comment)` tuples applied at `idaapi.get_imagebase() + rva`, so a rebased database works. It creates missing functions and declares padded C structs with `idc.parse_decls`.
- `symbols.csv`: the same columns as CSV, for your own tools.

The IDA-style dump and the CSV are generated artefacts; the release zip `nioh3-symbols-v2.02.zip` carries them. They are not committed here (see PUBLIC_EXCLUSIONS.md).

### The `incoming/` workflow (parallel contributors)

Each contributor writes only their own `incoming/<area>.tsv` with the same header and columns. Then `py re/symbols/merge_incoming.py` (`--dry` previews):

1. A new RVA is appended. For `struct_field` the key is the new `Struct::Field` name.
2. A known RVA keeps its existing name. The incoming name and description are appended to the description as `also (<file>): <name> - <desc>`, unless already mentioned. Confidence is only ever raised.
3. Rows with a bad kind, bad RVA or non-identifier name are reported and skipped.
4. A name that collides with a different RVA gets the RVA appended (`Name_<rva>`).
5. A missing `source` becomes `incoming/<file>`. Rows are re-sorted (non-struct rows by RVA, then struct fields grouped by struct).
6. Processed files move to `incoming/merged/<timestamp>_<file>`, and `build_exports.py` runs.

This is why a merge never replaces a name. To **rename**, use `apply_renames.py`:
`py -I re/symbols/apply_renames.py incoming/<x>-renames.tsv`. A renames row overwrites the non-empty columns of the row with the same RVA and records the old name as `(was <old>)`
in the description. A row with kind `DELETE` removes a bogus row (not a struct field). An RVA not present yet is added. Applied files move to `incoming/merged/`.

In the public repository you can skip the incoming workflow: edit `symbols/symbols.tsv` directly and open a pull request ([CONTRIBUTING.md](../../CONTRIBUTING.md)).
Small PRs avoid merge conflicts in a file this size.

### Applying to Ghidra

`N3ApplySymbols.java` (public copy: `symbols/ghidra/N3ApplySymbols.java`) takes `<symbols.tsv> [report.txt] [dryrun]`. Per kind:

- `function`: renames the function at the RVA (USER_DEFINED), writes a plate comment, and creates the function if the address is clearly code. If the RVA is inside another function it places a label and warns. A previous non-default name that is not ours is kept as a secondary label.
- `label`, `global`, `data`, `vtable`, `string_table`: create or rename our own label and set an EOL comment. Simple scalar or pointer types are applied only on bytes that are still undefined.
- `struct_field`: builds or extends the structure `/N3/<Struct>` and names, types and comments the field. A field overlapping an existing one at another offset is skipped with a warning.

Every comment is wrapped in a `[N3sym] <name> ... [/N3sym]` block; re-running replaces only that block. The script never deletes anything. Run it as the first step after loading a dump into Ghidra,
and check the `W` lines in the report: they mean a `function` row is not at a function entry. In the GUI, run it from Script Manager and pick the TSV; save the program afterwards.
`dryrun` rolls the transaction back so you can preview.

Never rename only in the Ghidra database. Those names are lost on the next rebuild and nobody else sees them; put them in `symbols.tsv`.

The project's own headless wrapper (`apply.cmd`, current source only) runs `analyzeHeadless ... -noanalysis -postScript N3ApplySymbols.java`, and refuses to start with under 3 GB free on the project drive,
because saving writes a new copy of the program database (about 1.2 GB) before dropping the old one. You do not need it; the Java script alone does the work.

### The decompiler index (generic description)

Besides the TSV, the maintainers keep a private, regenerated **per-function index of the decompiled exe**: one row per function with its rva, name, size, callers and callees, and a status.
It is built from the Ghidra project plus the `symbols.tsv` names, and is used to find unnamed callers and callees of a named function quickly and to count progress.
It is derived from game code and is not published. You can build the equivalent locally from your own dump (Ghidra's function export, or `tools/progress/`, which makes function lists from `.pdata`;
see `tools/progress/README.md`). The charts in `docs/progress/` and `data/regions.tsv` come from data of that kind.

### The `amrita/` gameplay tables

`re/symbols/amrita/` (current source only; not published) holds name lists for gameplay records: items (2,784 rows), gear effects (3,604), skills (1,100), soul cores (118),
transformation effects (176), guardians, equipment slots, armor sets, enemies, and 22 save-field hashes. They were parsed from public community web tools and describe the 2.01 build, so a few rows may differ on 2.02.
What a contributor needs to know:

- Gameplay records use **16-bit param ids** (the u16 at item+0 and effect slot+0 in the save). They are **not** KTIDs and do not appear in the RDB/KOD symbol databases.
- `save_field_hashes.tsv` holds the 32-bit field-name hashes used by the save serializer; each row lists the section layout and the exe read/write sites. The source strings of those hashes are unknown
  (a dictionary attack with the engine property hash over a large vocabulary found only chance collisions).
- They feed the save-format and parameter-table work, not the exe symbol DB. Game text tables are excluded from this repository (PUBLIC_EXCLUSIONS.md).

## 4. From database to mods: addresses found by pattern

Rule of the codebase: **no hard-coded game address in a mod**. Every `base + 0xRVA` goes through a pattern table that works on the known build and also on a changed build.
Humans use the database; the mods use these artefacts instead.

| artefact | mod | records | generated header |
|---|---|---|---|
| `rvas.json` | EngineFix, BossRematch, Training, CamKiProbe | every fixed RVA the code reads or hooks | `src/rvas_gen.h` |
| `sites.json` | BossRematch | hook, call, native and global sites with relocation info | `src/sites_gen.h` |
| `patches.json` | EngineFix | code patches (bytes, call, jmp, vptr, ripload, check) | `src/patches_gen.h` |

Counts in the current source:

| file | entries | breakdown |
|---|---|---|
| `mod/Nioh3EngineFix/rvas.json` | 269 | 210 code, 44 global, 11 iat, 4 none |
| `mod/Nioh3BossRematch/rvas.json` | 121 | 113 code, 8 global |
| `mod/Nioh3Training/rvas.json` | 229 | 205 code, 22 global, 1 iat, 1 none |
| `mod/Nioh3CamKiProbe/rvas.json` | 18 | 10 code, 8 global |
| `mod/Nioh3BossRematch/sites.json` | 138 | 51 native, 48 hook, 17 check, 11 global, 8 fn, 3 call |
| `mod/Nioh3EngineFix/patches.json` | 299 | 98 check, 94 call, 72 bytes, 25 jmp, 9 ripload, 1 vptr; 82 groups |

The copies in this repository's `mod/` come from an older source copy (2026-11-02) and have fewer entries. The tables are generated from a decrypted dump, so anyone with a dump can regenerate them.

### `rvas.json` and `Grva()`

`rvas.json` is **generated** by `mod/<mod>/tools/gen_rvas.py` (one per mod; the Training one reuses the EngineFix module). The generator takes the list of fixed RVAs from
`tools/check_fixed_rvas.py`, which scans `mod/*/src` for `base + 0x...`, `base + kName`, address tables, loops over tables, and helper functions that add a base to a parameter.
For each RVA it derives a byte pattern from the dump that is unique in the executable sections, with rip-relative displacements and branch targets wildcarded. It then writes `src/rvas_gen.h`.

An entry has a `kind`:

| kind | resolution |
|---|---|
| `code` | the bytes at `rva - offset` match `pattern` exactly once in executable sections; result = hit + `offset` |
| `global` | a rip-relative anchor instruction is found by pattern; result = anchor + `insn_len` + disp32 + `addend` (`disp_off` says where the disp32 sits) |
| `iat` | an import slot found by DLL and function name (`KERNEL32.dll!Sleep`, or by ordinal `XINPUT1_4.dll!#2`) in the import directory; no pattern |
| `none` | no unique pattern exists (`why` says so); known build only, 0 elsewhere |

Each entry also lists `used_in`, the source files that use it. That makes `rvas.json` the answer to "which game functions do the mods use" (section 7).

At run time, `mod/Nioh3EngineFix/src/gamerva.h` defines `Grva(rva)`. On the known build it returns the RVA unchanged: zero cost, behaviour unchanged.
On another build it calls the pattern lookup in `src/gamerva.cpp` and returns the RVA on that build, or 0 if the pattern is missing or not unique. An RVA that is not in `rvas.json` also returns 0: the table never guesses.
Callers then read nothing and the feature stays off instead of writing to a wrong address. The same files exist per mod: `mod/Nioh3BossRematch/src/gamerva.{h,cpp}`,
`mod/Nioh3CamKiProbe/src/gamerva.{h,cpp}`, and, for the C code of Training, `mod/Nioh3Training/src/gamerva.h`.
`gamerva::Init` runs right after the PE header is read; `gamerva::Verify` runs once `.text` is decrypted (BossRematch: once the player exists), checks every entry at its v2.02 place and logs
one line starting `game RVAs (rvas.json, N entries)` with counts of verified, hooked-over, import and missing entries.
Hooked-over means a jump (`E9`, `CC`, or `FF 25`) already sits at the site, so another module's hook is tolerated.

Build-time guards: `tools/check_fixed_rvas.py` lists every fixed address that is not pattern-resolved and fails unless it is in `tools/fixed_rvas_allow.txt`
(used for profiler label tables that are never read as addresses). `py -I mod/<mod>/tools/gen_rvas.py --check` fails if `rvas_gen.h` is stale or an RVA is missing from the JSON.
`tools/wrap_grva.py` was the one-shot rewrite that wrapped existing uses in `Grva(...)`, and `tools/aob_audit.py` proposes unique patterns for a list of fixed RVAs (both current source only).

### `sites.json` (BossRematch)

BossRematch hooks and calls many game functions, so its table is richer. `mod/Nioh3BossRematch/tools/derive_sites.py` (a `SPEC` list of name, group, kind, RVA, head) writes
`sites.json` and `src/sites_gen.h`. Kinds:

| kind | meaning |
|---|---|
| `hook` | function entry hooked with a relocated prologue of `head` bytes; the head must be whole instructions with no rip-relative or branch inside |
| `native` | a hook found by the **script native name** (for example `Mission::BossBattleMode`): the engine's script-native table maps name to function, and the head bytes must still equal the v2.02 bytes |
| `fn` | a function the add-on calls (entry) |
| `call` | an `E8 rel32` call site the add-on retargets; the original callee is read from the rel32 at run time |
| `global` | a `.data` pointer or array resolved from a rip-relative reference; target = ip + len + disp32 + addend |
| `check` | a code reference that proves struct offsets; resolved, never patched |

Each site has a `group` (`PUMP`, `FIGHT`, `BOSSLOG`, `RLOG`, `KEEPGEAR`, `STAGED` and others). A group self-disables when any of its sites fails to resolve. Resolution is in `mod/Nioh3BossRematch/src/sites.cpp`:
a site is checked at its v2.02 RVA first (fast path); otherwise, or if that fails, the executable sections are scanned and the pattern must match **exactly once**.
A site already hooked by another module still resolves because the head is skipped in the match, and `hooks.cpp` chains onto the other module's jump.
Soul-core sites (groups `SC`, `ACANCEL`) are compiled out of the release line by `BR_SOULCORE=0` in `addon.h`.

`mod/Nioh3BossRematch/tests/test_sites.py` verifies the table offline against a dump. The checks include a patch simulation: 16 bytes are inserted in the middle of `.text` so every later address and rip target moves,
and every later site must still be found exactly once, 16 bytes later.

### `patches.json` (EngineFix)

`mod/Nioh3EngineFix/patches.json` is the single source for the in-memory code patches; `src/patches_gen.h` is generated from it by `mod/Nioh3EngineFix/tools/gen_header.py`
(`--check` for staleness). Each site has: `id` (`W1`, `W2a`, `P3a`...), `group` (the atomicity group), `rva`, `len`, `orig` (the original bytes), `kind`, optional `new`, `pattern` and `offset`.

| kind | what the patcher writes |
|---|---|
| `bytes` | an in-place byte replacement of the same length |
| `call` | an `E8` retargeted through a near trampoline to a cave in `src/*.asm` (`callAt` = E8 position; bytes before it are NOPs) |
| `ripload` | a 6-byte `opcode modrm rel32` load re-pointed at a near data slot the mod owns |
| `jmp` | `E9 rel32` to a cave trampoline plus NOP padding (experimental) |
| `vptr` | an aligned 8-byte pointer in `.rdata` re-pointed at a cave; verified by value (known build only) |
| `check` | verified, never written: proves the original code is what the caves assume |

Other fields: `callee` (expected `E8` target RVA), `iat_name` / `iat_disp_at` (an `FF 15` slot that must resolve to a named import), `supersedes`, and the top-level `deps`
(extra original code a group's caves rely on, compared exactly or by FNV-1a hash on the known build before the group is written).

How a patch **self-locates on any game file**, in `Resolve()` in `mod/Nioh3EngineFix/src/dllmain.cpp`:

1. Parse the site's pattern and scan the executable sections. It must match **exactly once**; zero or several matches skip the patch and log why.
2. The site address is the match start plus `offset` (the distance from the pattern start to the site inside it).
3. On the known build the resulting RVA must equal the recorded `rva`, and the bytes must equal `orig` exactly.
4. On an unknown build the RVA may differ, and bytes at **wildcarded** positions inside the site may differ; everything else must still match.
5. `vptr` sites and `deps` checks are known-build only.
6. Groups are all-or-nothing where a partial install would be dangerous (for example the VRR group: a partial install would run the game at the wrong speed). Each group is gated by its own ini switch.

On an unknown build, `mod/Nioh3EngineFix/src/dllmain.cpp` logs `unknown build: each patch is installed only when its own bytes match`. Early-boot patches that must be written before pools or the renderer are sized
(`P12`, `AS0`, `AS`, readfix, worker cap) are skipped, because the code cannot be pattern-scanned in time. The `[General] AllowUnknownBuild` key in `Nioh3EngineFix.ini` is parsed,
but we found no other use of it in the source (unverified at runtime).

`mod/Nioh3EngineFix/tests/test_patterns.py` verifies `patches.json` against a decrypted dump: build identity, each pattern unique and resolving to the expected RVA, dump bytes equal to `orig`,
no overlapping sites, replacement shapes, values the mod reads from original bytes (the IAT slot of `PeekMessageA`, callee RVAs), `vptr` slots, `deps` ranges, and that `src/patches_gen.h` matches the JSON.

### Generated gameplay tables

Not address data, but generated the same way: `*_gen.h` files in `mod/Nioh3BossRematch/src/` (`native_branch_entrances_gen.h`, `native_entry_gates_gen.h`, `native_entry_waves_gen.h`) and
`mod/Nioh3Training/src/` (`fallback_rosters_gen.h`, `native_entry_boxes_gen.h`, `native_entry_sequences_gen.h`, `native_no_boss_mode_gen.h`, `style_atlas_gen.h`) are written by scripts in `tools/`
(`gen_native_entry_gates.py`, `gen_native_branch_entrances.py`, `gen_native_entry_boxes.py`, `gen_native_no_boss_mode.py`, `gen_fallback_rosters.py`, and others; current source only).
Treat the headers as output: edit the generator or its inputs.

## 5. How the public repo, CONTRIBUTING and the database relate

| piece | role |
|---|---|
| `symbols/symbols.tsv` | the names; what you edit |
| `symbols/build_exports.py`, `merge_incoming.py`, `apply_renames.py` | validate, merge, rename |
| `symbols/ghidra/N3ApplySymbols.java` | load the names into your own Ghidra copy |
| `tools/progress/dump_nioh3.py` | make the decrypted dump from your own install |
| `tools/progress/*` and `data/` | rebuild coverage charts from the dump |
| `research/` and `research/SOURCE_MAP.tsv` | the notes that `source` columns cite (about 91 % of cited rows resolve) |
| `docs/subsystems/`, `docs/code-map.md` | per-area summaries and the address-range map |
| `mod/*/rvas.json`, `sites.json`, `patches.json` | the patterns each mod ships; also a starting point for re-deriving addresses |

The loop for a contributor (the longer version is in CONTRIBUTING.md):

1. Dump your own v2.02 image at the title screen with mods off (`py -I tools/progress/dump_nioh3.py`).
2. Open it in Ghidra, IDA or x64dbg. Load existing names via `N3ApplySymbols.java` or the generated exports.
3. Name something. Edit `symbols/symbols.tsv`, run `py symbols/build_exports.py`, and open a PR with your evidence in `description` and `source`.
4. Do not upload game files, dumps, decompiler listings or game text tables. RVAs, names, byte patterns and short explanations are fine.

## 6. How to add a name properly

1. **Find a real entry point.** A function row must sit at a function start. The decrypted dump's `.pdata` gives the starts; the Ghidra function list gives them too. A patch site in the middle is a `label`.
2. **Check whether it is already there.** `grep -i '^0x5af410' symbols/symbols.tsv`. Many addresses already have an auto name; you are replacing it with a better one (keep the RVA, change `name`).
3. **Pick the name.** Verb plus object for functions (`DynFpsGovernorTick`, `MeshletCheckRequestLoad`, `GlyphAtlasReset`), `g_` for globals, `Struct::Field` for members. It must be unique in the file; search first.
4. **Write the signature.** A C prototype for functions; for data the type first (`u8 u16 u32 i32 u64 i64 f32 f64 bool ptr T*`, `u32[10]`).
5. **Write the description on one line, no tabs.** What it does, the live value if it is a global, and which mod patch or site touches it (use the id from `patches.json` or the name from `sites.json`).
6. **Set the confidence honestly** and say what you did (decompiled, live probe, working patch). A guess is `low`.
7. **Cite `source`.** A research note section (`research/<topic>.md`), or `decompiled`, `x64dbg breakpoint`, or a link to your write-up. Separate several with `;`.
8. **Validate.** `py symbols/build_exports.py` and fix every error. Commit `symbols.tsv`; the exports are regenerated by maintainers and the release process.
9. **If the name concerns a mod address**, leave the mod tables alone: you are adding a name, not moving an address.

Examples of good first renames (from [CONTRIBUTING.md](../../CONTRIBUTING.md)): replace placeholder names (`PauseSpin_*`, `MainHot_*`, `ScalarDeletingDtor_*`), raise a `low` row after verifying it, add
`struct_field` rows for a structure you mapped, name the callers or callees of an already-named hot function.

## 7. Your first task: name the functions the mods already use

The mods touch a bounded set of game addresses. Those are where a name saves the most future work, because each one is already tied to a behaviour and a patch.

1. List the used addresses and their users:
   ```
   grep -c '"rva"' mod/*/rvas.json
   grep -B1 -A8 '"rva": "0x3EB04"' mod/Nioh3EngineFix/rvas.json    # the entry plus used_in
   ```
   `rvas_gen.h` has the same table with the file name in a trailing comment. For hooks, read `mod/Nioh3BossRematch/sites.json` (`name`, `group`, `native`; RVAs there are decimal).
   For patches, read `mod/Nioh3EngineFix/patches.json` (`id`, `group`).
2. For each RVA, check whether a row exists: `grep -i '^0x3eb04' symbols/symbols.tsv`.
3. If a row exists but has a placeholder name, replace it. If none exists, decompile the code in your dump, understand it, and add one. A call site inside a larger function becomes a `label` plus a `function` row for the container.

The size of the opportunity, from the current source: the four `rvas.json` files plus `sites.json` and `patches.json` reference **947 distinct RVAs**. **345** of them have a row at exactly that address
(334 with a name that is not an auto-name), so about **600** addresses used by the mods have no row at that RVA. Many of these are call sites inside a function whose entry is named, but a large share are unnamed
functions or data slots. The count is exact-RVA only and was made by comparing the files offline, so treat it as an order of magnitude.

Good places to start:

- BossRematch `native` sites: the script-native name already tells you what the function does (`Mission::BossBattleMode`, `Character::IsDeathGroupEnemy`, `Item::AddItem`); the database may only have an anonymous row or none.
- EngineFix entries whose `used_in` is `loadstats.cpp`, `thrsched.cpp`, `drawprep.cpp`, `savefiles.cpp` or `menu.cpp`: each is a well-defined subsystem with a research note.
- `global` entries in `rvas.json`: add `global` and `struct_field` rows.

## 8. Open problems / known gaps

From source comments, the symbol README, the audit tooling and the research notes (there is no separate tech-debt file for the symbol work):

- **Coverage.** 6.0 % of functions have real names (2026-10-24 recount); about 90 % of code bytes are unnamed. The least understood renderer packages by code size are meshlet/geometry (8.5 %), materials (9.4 %), effects (11.3 %),
  post-processing (11.6 %), backend (12.1 %) and UI (12.9 %), per CONTRIBUTING.md. The biggest unnamed blocks are generated animation-rig code, menu and item text, and online/P2P code.
- **Auto-names are not understanding.** More than 8,500 rows come from auto-naming; they say what a function is attached to (a vtable, a string), not its behaviour. The low tier has 691 rows still to verify.
- **Single build.** Every RVA is for v2.02. After a game update the names in `symbols.tsv` are wrong until re-derived. The `rvas.json` patterns and BossRematch `sites.json` self-locate; the database itself does not,
  and there is no tool yet that carries names across builds by pattern.
- **Entries without a unique pattern.** Four EngineFix entries and one Training entry in `rvas.json` have kind `none` (with a `why`), so on an unknown build those features read nothing. One data slot has no rip-relative reference in the 0x400 bytes below it.
- **Known-build-only patch groups.** Experimental CPU-power patches (`jmp`, `vptr`) and early-boot patches (`P12`, `AS0`, `AS`, readfix, worker cap) cannot be pattern-scanned in time and are skipped on a changed build.
- **`AllowUnknownBuild`** is read from the ini but we found no other reference in the source (unverified). The ini text says the mod refuses unknown builds, while the log says it installs patches whose bytes match.
- **Pattern buffers are fixed at 160 bytes** in the `Grva` lookup (`gamerva.cpp`; `gamerva.h` for Training). The longest current pattern is 97 tokens, so there is margin, but a longer generated pattern would be cut off (unverified; check the `gen_rvas.py` output if you extend it).
- **Stale docs.** `docs/code-map.md` says itself that it was written from an older region map, and some region labels are known to be wrong (keyword rules also match substrings). This repository's `mod/` copy lags the current source.
- **Source citations resolve imperfectly.** About 9 % of `source` citations do not map to a published note in `research/SOURCE_MAP.tsv`.
- **The save-field hash names** in the gameplay tables are unknown (section 3).
- **File ordering is cosmetic.** A merge re-sorts the whole file, so large diffs in a PR are usually just that. Keep edits minimal and do not reformat.
