# Test hub inventory and handoff - 2026-10-11

Base: private main `8fcb81b`. New files only, branch `codex-test-hub`.
No ASI/source, workflow, installed game, save, device, bridge or public repo edits.
Recording-session constraint: no builds, dump scans, wrapper simulations or live
tests were authorized for this work. Only catalog/syntax/small fixture checks.

Deliverable: `tools/testhub/hub.py`, `test_hub.py`, `README.md`.
`py -I -B tools/testhub/hub.py run offline` is the small execution entry point.
Other lanes print inventory/commands only; the default runs nothing.

## Existing test coverage and gaps

- 39 EngineFix `run_*.py` plus one BossRematch native runner at this base.
  They include 35 EngineFix `run_*_harness.py` and separate CBS/CBF benchmarks,
  applybatch failure audit and CBV recreation witness; confirm the exact count
  with the catalog if another commit adds a runner.
- `release/check_release.py --full` loops only EngineFix `run_*_harness.py`
  plus `test_patterns.py`; it does not include BossRematch, Python stats/time,
  wrapper fixtures, benchmarks, the failure audit or recreation witness.
  `--strict` also fails warnings. Quick mode still requires a matching built ASI;
  a fresh worktree is not a release-ready artifact tree.
- Native wrappers often resolve the main checkout's ignored portable MSVC tree
  or fixed home paths; several read the large v2.02 decrypted dump and some use
  Capstone. BossRematch also checks private `tools/rematch_catalog.py`.
- `tools/testmatrix`: synthetic Python stats/time tests, PowerShell guard/INI
  and pinned-state/operator/recovery fixtures, historical matrix/ABBA, and
  final graphics/latency routines. Historical KEEP/provisional profiles are not
  a substitute for the tested current default.
- `tools/bench`: capture/source/dump audits and proofs; they are not one uniform
  pass/fail regression suite. The CPU-ready/AABB control requires a compiler.
- Map tool: `tools/tour/run_tour.ps1`; Mode 1 unlocked/cleared missions, route
  lists in `tools/tour`, crash/stall bounds, save-write blocking, backup/hash
  restore, captures and `report_tour.ps1`. `-Passes 2` is the leak route.
  `-DryRun` launches `fake_game.ps1`, uses temporary fixture trees and private
  state; it was not run during recording. Real UI mode requires Continue.
- Pinned final benchmark currently hard-requires 1.7.61 artifacts and several
  default-off gates. It must be reviewed before using a newer candidate.

## Small follow-up sequence for the lead

1. Integrate the new private files; use catalog + offline command before build.
2. After recording/device use ends: run the reviewed PowerShell fixtures, then
   native harnesses/dump checks/release checks in one controlled build slot.
   Treat witness success according to its documented meaning, not as safety.
3. Run Mode 1 main tour first, then two passes when appropriate; manual smoke
   covers menus/controllers, combat/retry, load transitions and game-speed gates.
   Keep captures/reports private and verify all entry hashes are restored.
4. Public staging is a separate curated task: first root portable CI with
   `python -I -B engine-fix/tools/gen_header.py --check`; next a vetted no-dump
   native allowlist with MSVC-on-PATH support. Public LoadStats and LoadMem
   runners already support compiler-on-PATH and use synthetic inputs; they are
   first candidates to validate in the Windows slot. GP uses a real D3D12 device.
   Do not copy this private hub or
   wrapper/helper tree wholesale. No hosted "all game tests passed" claim.

Read `notes/handoff-claude-public-repo.md` before public writes. Publish only
scrubbed summaries, original source, and approved binaries with PDB paths scrubbed;
exclude every private/game-owned input and private history.

## Validation completed in this branch

- `py -I -B tools/testhub/hub.py run offline`: PASS 5/5 (generated header,
  PresentMon stats, PresentMon time, synthetic PDB scrub and hub regression).
- Catalog/list and Python syntax passed; all 71 catalog paths exist at this base.
  Lane counts: offline 5, native 40, dump 2, wrappers 6, release 2,
  analysis 13, device 3. A plan is not an execution or success assertion.
- Public read-only `py -I -B engine-fix/tools/gen_header.py --check` passed
  (`patches_gen.h in sync`). No public files/workflows were written.
- Native builds, game-dump scans, PowerShell wrapper fixtures/simulations,
  release checks and live device routes remain unrun for this task.
