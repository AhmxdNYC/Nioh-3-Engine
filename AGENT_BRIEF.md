# Nioh 3 agent brief (read first; keep briefs short and point here)
- Goal: real 60 fps on ROG Ally (Proton) at the user's settings / max 1080p, low input delay; PC gains are a bonus. Ally is GPU-bound (99 %): CPU fixes are fps-neutral there; GPU shader/pass savings move fps (GHDM +8.7 % at MAX).
- Scope: game-side only (ASI mod, shaders, game data, game's own API calls). Never system/hardware tweaks (TDP, boost, env vars, gamescope). Never strip DRM or edit Nioh3.exe on disk. No secrets in files.
- Same look: no quality reductions; prove equivalence offline (bit-exact or within output quantization).
- Tools: decomp D:\nioh3-re\decomp (grep), re\symbols\symbols.tsv (+ incoming/<area>.tsv; merge_incoming.py has no --help), re\Nioh3.dump.exe, Ghidra headless <=4 GB heap, C: >= 3 GB free. Game not runnable on PC while the Ally plays.
- ASI: mod\Nioh3EngineFix (build.bat, tests\test_patterns.py must end ALL N SITES OK). Only ONE agent edits src at a time; others add new files + integration notes.
- Deliverables: notes\<topic>.md (concise), MODLOG.md entry, commit only own files ("Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"), push. Don't launch the game; don't use the bridge.
- Token rules: universal-optimizer\docs\token-efficiency.md (read narrowly, digest logs with scripts, short reports).
