# Pinned default benchmark driver

Private Windows tooling, no game run during implementation. The existing
`final_pc.ps1 -OptimizedIni ... -AsiPath ... -NoLatency` forwards to the new
`pinned_final_pc.ps1`; the historical BEST/sweep path is not used. This separate
driver avoids stale shared `.ini.orig` / gfx backups and old driver game kills.

Control: **EngineFix absent**, with loader/OptiScaler, other ASIs, disk shaders
and external PresentMon instrumentation unchanged. This is not a vanilla-game
claim. OPTIMIZED installs the named 1.7.61 ASI and exact INI bytes; no automatic
flags, auto-continue, probes, affinity, graphics-quality reductions or shaders.
FG OFF in both arms. Input hashes and full graphics XML are recorded per cell.

The queued path exclusively creates `%LOCALAPPDATA%/nioh3-codex/pc-benchmark-active`,
then waits for a live controller worker PID to finish (PID reuse is busy).
Unknown controller state is blocked. Explicit root source ownership confirmation
is required even when controller state is absent. Lock acquisition and a second
existing-game/stale-stockoff check precede snapshots or game/save preparation.
Only registered PID/start-time identities may be stopped on exceptional cleanup.
Normal runs prompt for menu exit. A failed restore retains the marker and backups.

Entry save backups reuse the existing tour Get-Manifest/Compare-Manifest functions
without executing the tour. Every launch starts from the same entry save/settings;
changed, removed and added files are restored and SHA-256 verified. ASI/INI/log
presence, bytes and file timestamps are restored. Shared historical backup markers
are ignored. Recovery snapshots are retained in a unique output directory.

Root must hold automatic source work and wait for the Ally shared-account launch
to end. Run from the selected checkout, with a live owner shell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\final_pc.ps1 `
  -OptimizedIni "C:\Users\Ahmad Hamza\nioh3-engine\mod\Nioh3EngineFix\bin\Nioh3EngineFix.ini" `
  -AsiPath "C:\Users\Ahmad Hamza\nioh3-engine\mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi" `
  -Configs STOCK,OPTIMIZED -Presets USER,MAX -Runs 3 -StandWindows 2 `
  -WindowSec 20 -NoLatency -OwnerPid $PID -MaxMinutes 180 -SourceOwnershipConfirmed
```

Append `-DryRun` for a mutation-free plan: no process/lock/executable invocation,
no marker or output writes. Direct `pinned_final_pc.ps1` additionally accepts
`-OutputDir`, `-GameDir`, `-SaveRoot`, `-CoordinationDir` and `-SkipCombat`.

Operator drives the same save/view/route/fight and acknowledges gameplay. Each
preset uses A1 B1 | B2 A2 | A3 B3; 60 s gameplay settle, two STAND and one MOVE
windows of 20 s, COMBAT 25 s. Every required window must pass capture/exit/gameplay
confirmation and 90% duration from captured intervals (first interval excluded).
Any invalid window aborts after restoration. No repeated local smoke is needed.

Limitations: manual prompts require an attentive device worker; LOAD1 includes
its acknowledgement delay. LOAD2/hitches and latency remain manual work; result
`full_protocol=false` until those are separately completed. Graphics preset MAX
uses the PC presets table; confirm the user's resolution/upscaler/cap beforehand.
This driver does not prove visual equality or statistical significance by itself.
If abrupt termination bypasses finally, quit the game, inspect the retained
snapshot/marker and use `Read-PBState` + `Restore-PBState` from
`pinned_benchmark_lib.ps1` under root ownership/lock. Remove only that state's
owned `coordinator_marker` after all restoration hashes succeed; release the lock.

Validation: `powershell -NoProfile -ExecutionPolicy Bypass -File
tools\testmatrix\test_pinned_benchmark.ps1` runs only temporary fixtures. It checks
restoration after changed/deleted/new saves and temporary ASI/INI/log installation,
persisted recovery, damaged backups, existing-game/stale-stockoff guards, ABBA IDs,
partial windows, first-interval false coverage, actual dry-run no writes, exclusive
marker ownership and controller blocking; also parses all four PowerShell files.
