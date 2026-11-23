# To-do list

Open work for contributors, newest priorities first. Each item links to the doc that explains the background.
Pick one, open an issue or draft pull request saying you are on it, and send changes as a pull request
(main is protected: 1 approving review). Read [docs/engineer/README.md](docs/engineer/README.md) first.

## Now

- [ ] **Symbol database catch-up.** `symbols/symbols.tsv` was last fed on 2026-11-12. Findings since then live only in
      notes: the hold-attack input path, Train-in-arena / Fight-now placement, and the Player log readers. Add them as
      rows (format and rules: [SYMBOLS.md](docs/engineer/SYMBOLS.md) section 6). Maintainer is doing this; coordinate first.
- [ ] **First task for a new engineer:** name the functions the mods already use
      ([SYMBOLS.md](docs/engineer/SYMBOLS.md) section 7). Every patched site in `rvas.json` / `sites.json` / `patches.json`
      should have a real name.
- [ ] **Walk through one live-to-code trace yourself** using [LIVE-TO-RE.md](docs/engineer/LIVE-TO-RE.md), then fix
      anything in that page that did not match what you saw.

## Build and tests (make the repo usable outside the original machine)

- [ ] One-command build for all four mods; remove the hard-coded `tools/msvc/setup_x64.bat` and Steam paths from test
      runners (`NIOH3_DIR`-style overrides). [SETUP.md](docs/engineer/SETUP.md) §12, [TESTS.md](docs/engineer/TESTS.md).
- [ ] Single source list for EngineFix (`build.bat` vs `CMakeLists.txt`); replace the outdated GitHub workflow.
- [ ] Hosted CI lane with the dump-free checks (`gen_header.py --check`, load harnesses). [TESTS.md](docs/engineer/TESTS.md).
- [ ] Make `mod/settings/test_catalog.py` pass on a shallow clone.
- [ ] Remove stale packaging scripts (`release/package_final.py`, `release/package.py`).

## Game-update safety

- [ ] Pattern-resolve the remaining raw fixed RVAs (`tools/check_fixed_rvas.py`, `tools/fixed_rvas_allow.txt`).
- [ ] Unique patterns for the `rvas.json` entries with kind `none`.
- [ ] Tool to carry symbol names to a new game build by pattern. [SYMBOLS.md](docs/engineer/SYMBOLS.md) §8.

## Boss Match / Training

- [ ] Live-verify the unconfirmed catalogue rows (58 of 121 have no `confirmed` date; H / D / E0 route classes) with
      `verify_all.ps1`. [FEATURES-BOSS-TRAINING.md](docs/engineer/FEATURES-BOSS-TRAINING.md) §9.
- [ ] Staged fights: prove every box station fires in a replay; Death Veil "bar only, wave never starts".
- [ ] Native actor uniqueness and mission wave ordering (Training logs "UNPROVEN").
- [ ] Player log gaps: per-character time scale, buff ids and enemy speed are not logged yet.

## Performance (same picture, lower CPU / higher fps)

- [ ] End-to-end A/B of the shipped bundle against stock; replace estimated gains with measured ones
      (`B`, `PP`, `MA`, `LK`, `BPO`, `W1B`, `RPA`, `P1E7`, `GL1`). Method: [performance-measurement.md](docs/engineer/performance-measurement.md).
- [ ] Test on hardware we have never run: NVIDIA, Intel Arc, Intel CPUs. [FEATURES-ENGINEFIX.md](docs/engineer/FEATURES-ENGINEFIX.md) §15.
- [ ] Frame overlap: fix the offline lifetime audit, then measure.

## Play as a boss (new mod, research stage)

- [ ] Map the game's AI input-injection path and input object; build the transactional spawn adapter.
      Full list: [PLAY-AS-BOSS.md](docs/engineer/PLAY-AS-BOSS.md) §11.
