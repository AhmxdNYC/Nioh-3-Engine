# pc2 tool copies (second test PC: Ryzen 5 5600X, RX 6650 XT)

The main-PC scripts hardcode `C:\Users\Ahmad Hamza\...`, `D:\nioh3-re\...` and agent id `pc`. The originals are NOT edited.
`make_pc2.ps1` generates `pc2_<name>.ps1` copies next to each original (so `$PSScriptRoot`-relative paths still work) with:
- `C:\Users\Ahmad Hamza\nioh3-engine` -> `C:\Users\User\projects\nioh3-engine`; `...\universal-optimizer` -> `C:\Users\User\projects\universal-optimizer`
- `D:\nioh3-re` -> `C:\nioh3-re` (no D: drive on pc2; created, empty; save-backups / stock-stash go there)
- agent id / lock-holder checks `"pc"` -> `"pc2"`
- every cross reference between the copied files (session_lib, harness_guard stays original, run_tour, common, abba2_pc ...) -> the pc2_ copy.
Copied: testmatrix\{pc_gpu_session,abba2_pc,pc_weakcpu}, playsession\{session_lib,start_session,stop_session,monitor,test_session},
tour\run_tour, patcher\common, tools\{enter_game,notify}. harness_guard.ps1, vpad.ps1, fake_game.ps1, matrix.json need no change (used as is).
Regenerate after originals change: `powershell -NoProfile -ExecutionPolicy Bypass -File tools\pc2\make_pc2.ps1` (params -Ws -Uo -ReDir -Agent).
Generated files under tools\ are gitignored dir (`tools/`); they are force-added like the other tools.

## Run (dry runs; env first: UO_AGENT_ID=pc2, UO_BRIDGE_URL=http://192.168.1.165:8765)
- `tools\playsession\pc2_test_session.ps1` (start/stop/monitor/session_lib kit test) -> ends `DRY RUN OK`
- `tools\testmatrix\pc2_pc_gpu_session.ps1 -DryRun -UserConfig <a config.xml>` -> `DryRun OK`
- `tools\testmatrix\pc2_abba2_pc.ps1 -DryRun -Input vpad -BaseIniFile <ini>` -> `DryRun OK: ...`
- `tools\testmatrix\pc2_pc_weakcpu.ps1 -DryRun` -> `dry run done: W=dry-run ok | S=dry-run ok`
Real runs: same scripts without -DryRun (after the game is installed, ASI built, lock acquired by the caller).

## Dry-run prerequisites that are absent on pc2 (not installed, per rules)
- `mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi` + `bin\Nioh3EngineFix.ini` (build output: needs MSVC via mod\Nioh3EngineFix\build.bat, or copy from the main PC); ASI must be >= 1.7.37 for VirtualPad.
- `tools\PresentMon.exe` (not in git; PresentMon 2.x console build).
- `dotnet` SDK (`dotnet fsi tools\ui.fsx` is used for foreground / UI input; only the ui input mode and focus checks).
- A user `config.xml` (pc_gpu_session -UserConfig; default points at a main-PC capture folder). pc2 has none until the game runs once.
The dry runs above were verified with throw-away stand-ins (fake ASI/ini, a sleeping PresentMon stub, a no-op dotnet shim on PATH, a placeholder config.xml
under a temp LOCALAPPDATA); all removed afterwards. Results with real binaries only the real game can confirm.
