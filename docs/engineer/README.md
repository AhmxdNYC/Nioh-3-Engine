# Engineer onboarding

Start here if you are joining the project to work on the code. Read in this order:

| Doc | What it covers |
|---|---|
| [SETUP.md](SETUP.md) | Toolchain, building each of the four ASI mods, game folder layout, ini settings, logs, installing a dev build and rolling back, how releases are packaged. Includes a first-hour checklist. |
| [SYMBOLS.md](SYMBOLS.md) | The symbol / reverse-engineering database (`symbols/symbols.tsv`): format, how names are added and merged, Ghidra/IDA/x64dbg exports, and how addresses reach the mods through byte patterns. |
| [FEATURES-BOSS-TRAINING.md](FEATURES-BOSS-TRAINING.md) | Every gameplay feature: Boss Match / rematch, Fight now, Train in arena, the boss catalog, Training mode, move controls, frame meter, style meter, soul cores and transformation, Player log. |
| [FEATURES-ENGINEFIX.md](FEATURES-ENGINEFIX.md) | Every Nioh3EngineFix feature (performance, pacing, loading, PSO/shader work, overlays, menu, reports, updater) plus Nioh3CamKiProbe. |
| [TESTS.md](TESTS.md) | Offline tests and harnesses, the base-vs-candidate regression rule, the no-hardcode audit, and the live in-game test scripts. |
| [PLAY-AS-BOSS.md](PLAY-AS-BOSS.md) | Starter brief for a "play as a boss" mod: what is known about spawning, actors, input, forcing actions, AI, the game's own transformation mechanic and the camera, plus suggested approaches and the missing reverse engineering. |

Also read [CONTRIBUTING.md](../../CONTRIBUTING.md), [PUBLIC_EXCLUSIONS.md](../../PUBLIC_EXCLUSIONS.md) and
[docs/subsystems/](../subsystems/).

**Note on the source.** These docs describe the current release (1.8.39). The `mod/` folder in this repository is an
older snapshot (2026-11-02), so features marked *(source not yet in the public copy)* are described here but their code
is not in this repository yet.
