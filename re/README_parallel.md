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
