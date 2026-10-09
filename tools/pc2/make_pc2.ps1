# Generates pc2_*.ps1 copies of the main-PC test tools with pc2 paths. Originals are never modified.
# Re-run after the originals change:  powershell -File tools\pc2\make_pc2.ps1
# Overrides (edit here or pass params): repo root, bridge repo, local re dir, agent id.
param([string]$Ws = "C:\Users\User\projects\nioh3-engine", [string]$Uo = "C:\Users\User\projects\universal-optimizer",
      [string]$ReDir = "C:\nioh3-re", [string]$Agent = "pc2")
$ErrorActionPreference = "Stop"
$tools = Split-Path -Parent $PSScriptRoot
$files = @{ testmatrix = "pc_gpu_session.ps1","abba2_pc.ps1","pc_weakcpu.ps1","cost_vs_looks.ps1","samelook_ab.ps1"; tour = "run_tour.ps1"; "..\patcher" = "common.ps1"; "." = "enter_game.ps1","notify.ps1"; playsession = "session_lib.ps1","start_session.ps1","stop_session.ps1","monitor.ps1","test_session.ps1" }
$all = $files.Values | % { $_ }
foreach ($d in $files.Keys) { foreach ($f in $files[$d]) {
  $t = [IO.File]::ReadAllText("$tools\$d\$f")
  $t = $t.Replace('C:\Users\Ahmad Hamza\nioh3-engine', $Ws).Replace('C:\Users\Ahmad Hamza\universal-optimizer', $Uo).Replace('D:\nioh3-re', $ReDir)
  $t = [regex]::Replace($t, '(-ne|-eq) "pc"', "`$1 `"$Agent`"")
  $t = $t.Replace('$env:UO_AGENT_ID = "pc"', "`$env:UO_AGENT_ID = `"$Agent`"")
  foreach ($n in $all) { $t = [regex]::Replace($t, '(?<![\w])' + [regex]::Escape($n), "pc2_$n") }
  $t = $t.Replace("pc2_pc2_", "pc2_")
  $t = $t.Replace('[string]$LockKey = "nioh3"', '[string]$LockKey = "nioh3-pc2"')   # pc2 tours: own lock key (#576)
  [IO.File]::WriteAllText("$tools\$d\pc2_$f", $t, (New-Object Text.UTF8Encoding $true))
  "wrote $d\pc2_$f"
} }

