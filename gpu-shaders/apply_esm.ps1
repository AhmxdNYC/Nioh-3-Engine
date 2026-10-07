<#
  apply_esm.ps1 - install / revert the gather-based ESM shadow-map blur (group ESM, records #2..#11) for Nioh 3
  (build_esm.py, ESM_fast.hlsl, notes\gpu-savings-round2.md). Exact revert = restore backup\0x1ca8bb39.file
  (or Steam "verify integrity").

    powershell -NoProfile -ExecutionPolicy Bypass -File apply_esm.ps1            # install replacement
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_esm.ps1 -Revert    # restore original
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_esm.ps1 -Status    # show which one is installed

  Requires the game to be closed (the file is read at startup / PSO preload). Verifies SHA-256 of
  everything it touches; refuses to overwrite an unknown file (e.g. after a game update).
#>
param([switch]$Revert, [switch]$Status)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$game = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12\0x1ca8bb39.file"
$orig = Join-Path $here "backup\0x1ca8bb39.file"
$repl = Join-Path $here "replacement\0x1ca8bb39.file"
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 $p).Hash }
foreach ($p in $orig, $repl, $game) { if (-not (Test-Path $p)) { throw "missing: $p" } }
$hg = Get-Sha $game; $ho = Get-Sha $orig; $hr = Get-Sha $repl
$state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "FAST (replacement)" } else { "UNKNOWN ($hg)" }
if ($Status) { "installed: $state"; exit 0 }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running - close it first" }
if ($state -like "UNKNOWN*") { throw "game file is neither the backed-up original nor our replacement - game updated? delete backup\0x1ca8bb39.file and re-run: py -I gpu-shaders\build_esm.py" }
if ($Revert) {
  if ($hg -ne $ho) { Copy-Item $orig $game -Force }
  "reverted -> installed: " + $(if ((Get-Sha $game) -eq $ho) { "ORIGINAL" } else { "FAILED" })
} else {
  if ($hg -ne $hr) { Copy-Item $repl $game -Force }
  "applied -> installed: " + $(if ((Get-Sha $game) -eq $hr) { "FAST (replacement)" } else { "FAILED" })
}
