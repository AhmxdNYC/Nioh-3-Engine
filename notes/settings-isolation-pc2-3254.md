# Settings isolation patch (Task 3254, 2026-10-14)

Branch `pc2/settings-isolation-3254`, based on `556deac`. Isolated worktree:
`C:/Users/User/projects/nioh3-settings-isolation-3254`.

The FG row now displays the actual setting (including OFF) and ignores activation/left/right input.
Removed the game XML writer and all six call sites, two native FG setter calls, the dirty-byte store,
startup/exit recovery, marker operations, and their dedicated process-exit import hooks.
Save to ini continues to write EngineFix-owned settings. Read-only OSD/present telemetry is unchanged.
Legacy FrameGenToggle=3 retains the independent FGC copy-job crash guard under Proton;
it cannot switch/pause FG or write game settings. The default is now 0. Old markers/backups are ignored.
Runtime renderer/presentation controls outside this FG-settings scope are unchanged.

## Offline checks

- Source contracts: PASS. Reject removed writer/setter/hook names and addresses, direct file I/O in menu,
  missing read-only dispatch, or loss of own-INI saving/telemetry/FGC guard.
- Negative controls: PASS (3/3): original pre-patch source, writable FG row, and reintroduced setter rejected.
- `git diff --check`: PASS.
- Build: BLOCKED (`ERROR: no MSVC toolchain found`). No build artifact produced.
- Required pattern check: BLOCKED (local `re/Nioh3.dump.exe` absent).
- Native preservation harness: ADDED, NOT RUN here. No runtime preservation pass is claimed.

The full harness links the actual menu translation unit and normal mod objects into a console executable;
it does not load the ASI, call DLL attach/Worker, install hooks, or launch the game.
64 cases cover FrameGenToggle 0/1/2/3, Windows/Proton host flag, OFF/DLSS/FSR/XeSS, and absent/stale marker.
Synthetic image memory and temporary files are checked after Configure, FG input, own-INI saving and
process-detach/exit. A simulated user edit after startup changes resolution/window mode; byte equality
and SHA256 must preserve that newer file. Own WorkerCap save must persist. Old backup/marker bytes stay intact.
Startup/exit hook removal is also checked statically because those removed entry points cannot be called.

Run from `mod/Nioh3EngineFix` in an x64 MSVC developer prompt (or pass `--setup <vcvars64.bat>`):

```
py -I tests/run_settings_preservation.py
py -I tests/test_patterns.py <path-to-decrypted-Nioh3.dump.exe>
```

Source-only fallback: `py -I tests/run_settings_preservation.py --source-only`.
Build log: `mod/Nioh3EngineFix/build/settings-preservation/build.log`.
No game launch, installed-bin modification, game-config access or save access occurred.
