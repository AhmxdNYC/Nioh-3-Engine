# Engineer onboarding

Start here if you are joining the project to work on the code. Read in this order:

| Doc | What it covers |
|---|---|
| [SETUP.md](SETUP.md) | Toolchain, building each of the four ASI mods, game folder layout, ini settings, logs, installing a dev build and rolling back, how releases are packaged. Includes a first-hour checklist. |
| [SYMBOLS.md](SYMBOLS.md) | The symbol / reverse-engineering database (`symbols/symbols.tsv`): format, how names are added and merged, Ghidra/IDA/x64dbg exports, and how addresses reach the mods through byte patterns. |
| [LIVE-TO-RE.md](LIVE-TO-RE.md) | From a live symptom, a Player log line, or the overlay to the code: find the value, break on the write in x64dbg, open the RVA in Ghidra, add a `symbols.tsv` row, then reach it from a mod by byte pattern. Worked example: charged-attack action id `0x629d`. |
| [performance-features.md](performance-features.md) | Every EngineFix performance feature: what ships on, why, and where the code is. The settings table is generated from the ini. |
| [performance-measurement.md](performance-measurement.md) | How fps, CPU, and load numbers are measured, the A/B method, and the checks that keep a result honest. |
| [FEATURES-BOSS-TRAINING.md](FEATURES-BOSS-TRAINING.md) | Every gameplay feature: Boss Match / rematch, Fight now, Train in arena, the boss catalog, Training mode, move controls, frame meter, style meter, soul cores and transformation, Player log. |
| [FEATURES-ENGINEFIX.md](FEATURES-ENGINEFIX.md) | Every Nioh3EngineFix feature (performance, pacing, loading, PSO/shader work, overlays, menu, reports, updater) plus Nioh3CamKiProbe. |
| [TESTS.md](TESTS.md) | Offline tests and harnesses, the base-vs-candidate regression rule, the no-hardcode audit, and the live in-game test scripts. |
| [TODO.md](../../TODO.md) | Open work for contributors, by area. |
| [PLAY-AS-BOSS.md](PLAY-AS-BOSS.md) | Starter brief for a "play as a boss" mod: what is known about spawning, actors, input, forcing actions, AI, the game's own transformation mechanic and the camera, plus suggested approaches and the missing reverse engineering. |

Also read [CONTRIBUTING.md](../../CONTRIBUTING.md), [PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md) and
[docs/subsystems/](../subsystems/).

**Note on the source.** These docs describe release 1.8.39, and `mod/` in this repository is synced to that release.
A few pages still mention an "older snapshot": that predates the sync. Some project tools outside `mod/` (for example
the boss catalog generator `tools/rematch_catalog.py` and `tools/setup/`) are not published yet.
