# autoname: automatic evidence-based function names

Script: `re\scripts_re\autoname.py` (stdlib, ~70 s). Output: `re\symbols\incoming\autoname.tsv` (4,738 NEW rows, merge left to main session).
Rule: skips any function whose .pdata primary start already has a non-auto name (symbols.tsv + all incoming) and any RVA already present as a row. Names are made unique (`_<RVA>` suffix on collisions).

## Method and counts
| rule | what | rows | conf |
|---|---|---|---|
| rtti vfunc | 204 RTTI COLs (222 type descriptors) -> vtable (qword == image base + COL); slot functions named `<Class>::vfunc_<slot>`, skipped when shared by several classes | 860 | high |
| rtti ctor | function that `lea`s a vtable -> `<Class>::ctor_or_dtor` | 845 | high |
| thunk / wrap | `jmp [IAT]`, `jmp rel32` to a named fn, `sub rsp,28/call X/add rsp,28/ret` to a named fn or an import -> `<T>_thunk`, `Wrap_<T>` (only 18 found; very few targets are named yet) | 18 | high |
| lib | curated CRT `R60xx` / zlib inflate error strings | 1 | high |
| str, qualified-name string (`A::b`) | function name taken from the string | 1,191 med / 194 low | medium/low |
| str, single distinctive string | `Str_<string>` or `Src_<file>` (`__FILE__`) | 597 med / 791 low | medium/low |
| str, several strings | most specific string, always low | 241 | low |
Totals: high 1,724, medium 1,788, low 1,226. Medium = one string referenced by <=2 functions, function has <=4 string refs and <=8 KB; anything with >12 string refs is skipped.
Only 117-222 RTTI types exist in the exe (no `ktgl::` vtables except CZlibDecompressInputStream, ktul::CMemoryMonitorAllocator), so rule 1 is small; renderconfig-field registration sites are skipped (they are one big registrar, not per-field functions).

## Precision (hand check, 30 random rows that have a decompile, seed 11)
- high (11 rows, vtable slots / zlib): 11/11 consistent with the decompile (RTTI is exact; `vfunc_N` is only a slot label).
- medium (14 rows): 13 right (string is the function's own log/marker/property text, e.g. PCL_SubmitStart/End, slPCLSetMarker, GenerateSceneHiZ), 1 wrong-ish (`Str_encountered_in`, a log-glue fragment; a filter for short lowercase fragments was added after the check).
- low (5 rows): 1-2 right, rest big functions where the string is one of many (ColorModulate, CopyMRLRScene, WaveManager) - treat low as a hint only.
Overall about 83-90 % useful (25 clearly right, 1 wrong, 4 unverifiable). Medium+high ~96 %. Recommendation: merge high and medium, merge low only if you want hints.

## Coverage
Whole-game named functions (`progress_treemap.py`): 1,399 / 211,206 = 0.66 % now. Merging all 4,738 rows: 6,137 = 2.91 %; high+medium only (3,512): 4,911 = 2.33 %.
Note: the "stuck at 1393" report - the 192 D3D12 rows were already in symbols.tsv (swept in by an earlier merge) so they were already counted. A real bug was found though: `load_syms` keyed rows by exact RVA, so rows not on the .pdata primary start were missed; it now folds with `pc.fold` (+6 -> 1,399).
