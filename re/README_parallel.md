# Parallel read access to the decompilation (agents: read this before running re\decomp.cmd)

`re\ghidra_proj` is single-user: `decomp.cmd` and `symbols\apply.cmd` serialize through `re\ghidra_proj\.n3query.mutex`.
For **read-only** work, use these first. Everything lives on D: because C: is nearly full. Full docs: **`D:\nioh3-re\README.md`**.

1. **Grep `D:\nioh3-re\decomp\`**: about 5,000 pre-decompiled functions as `<entry-rva>.c`, plus `index.tsv`, `graph.tsv` and `targets.txt`.
   They cover every function sampled in `captures\cpuprof*`, every function in `re\out\anchors.md`, `notes\*.md` tables and `symbols.tsv`,
   and the call-graph neighbours of the 300 hottest functions. No lock is needed and any number of agents can read them.
2. **`D:\nioh3-re\decomp_ro.cmd <queries>`**: same syntax as `re\decomp.cmd`, run against a read-only copy (`D:\nioh3-re\ghidra_proj_ro`)
   that has its **own** mutex. Run it at below-normal priority: `start "" /belownormal /wait /b cmd /c D:\nioh3-re\decomp_ro.cmd ...`.
3. Only use `re\decomp.cmd` (main project) when you need names or types that `symbols\apply.cmd` applied after the copy was taken
   (`D:\nioh3-re\ghidra_proj_ro\COPIED_AT.txt`).

Staleness: run `py D:\nioh3-re\apply_names.py` to overlay the current `symbols.tsv` names onto the export (seconds, no Ghidra).
After a real (non-dryrun) `symbols\apply.cmd`, run `D:\nioh3-re\refresh_ro.cmd` to re-copy the project. It holds both mutexes while it copies.
New names still go into `re\symbols\symbols.tsv` only.

## decomp_pool.cmd (use this instead of decomp_ro.cmd)

`D:\nioh3-re\decomp_pool.cmd <queries>` has the same syntax as `decomp_ro.cmd` but spreads queries over **4 read-only copies**
(`ghidra_proj_ro`, `_ro2`, `_ro3`, `_ro4`), each with its own mutex. It takes the first free copy and waits (polling) if all four are busy,
so up to 4 queries run at once. It runs Ghidra at below-normal priority itself (no `start /belownormal` wrapper needed), with a 3G heap
(`GHIDRA_HEADLESS_MAXMEM`; do not raise it) and a 10-minute wait limit. Real use is about 1.5 GB RAM per running query, so 4 at once is about 6 GB.
Batch several queries per call (startup is still around 7 to 20 s). `N3Q_OUT=path` keeps the output file.

- Agents: **use `decomp_pool.cmd` instead of `decomp_ro.cmd`**. `decomp_ro.cmd` still works, but it only uses copy 1 and contends with the pool.
- After a real (non-dryrun) `symbols\apply.cmd`, run `D:\nioh3-re\refresh_pool.cmd`. It refreshes `ghidra_proj_ro` as `refresh_ro.cmd` does, then re-copies it to `_ro2` to `_ro4`, each under its own mutex (about 1 minute per copy).
- Each copy has a `COPIED_AT.txt`.
