<#
  apply_gi.ps1 - install / exact revert / status of the GI-gather replacement for Nioh 3 (Steam v2.02).
    gi   shader_dx12\0xd070067d.file  RefProbe #121 (diffuse probe GI gather): the 8-corner trilinear probe loop is
         unrolled + if-converted so all corners' reads issue together (memory-level parallelism). Bit-exact to the
         shipped DXIL on the interpreter (notes\gpu-gi-rewrite.md); every other record of the file byte-identical.

    powershell -NoProfile -ExecutionPolicy Bypass -File apply_gi.ps1            # install
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_gi.ps1 -Revert    # restore original
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_gi.ps1 -Status    # what is installed

  Refuses while Nioh3 runs. Only overwrites a game file that is the known v2.02 original or our replacement (anything
  else = game updated: rebuild with gi\mkfast.py + gi\build_gi.py). The original is copied to gpu-shaders\backup\
  (sha-checked) before the first overwrite; -Revert restores exactly that file. Steam "Verify integrity" also reverts.
#>
param([switch]$Revert, [switch]$Status,
      [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$file = "0xd070067d.file"
$ho = "d3cbb484ee3380d337641c8c893aa152f2c8d053afb37f49091609b1d87dc372"
$hr = "c952f39a76bc09668953b1bc483e1434f1cbe121bfe9e907d6f49fc601e0fb97"
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
$game = Join-Path $GameDir "shader_dx12\$file"
$bak = Join-Path $here "backup\$file"
$rep = Join-Path $here "replacement\$file"
if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
$hg = Get-Sha $game
$state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "REPLACEMENT" } else { "UNKNOWN($hg)" }
if ($Status) { "gi         {0,-16} {1}" -f $file, $state; exit 0 }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running - close it first" }
if ($state -like "UNKNOWN*") { throw "$file is neither the v2.02 original nor our replacement - game updated? not touching anything" }
if ($Revert) {
  if ($state -eq "ORIGINAL") { "gi: $file already original"; exit 0 }
  if (-not (Test-Path -LiteralPath $bak) -or (Get-Sha $bak) -ne $ho) { throw "no valid backup for $file in $here\backup - use Steam 'Verify integrity'" }
  Copy-Item -LiteralPath $bak -Destination $game -Force
  if ((Get-Sha $game) -ne $ho) { throw "restore of $file failed" }
  "gi: $file reverted to the original"
} else {
  if ($state -eq "REPLACEMENT") { "gi: $file already installed"; exit 0 }
  if (-not (Test-Path -LiteralPath $rep) -or (Get-Sha $rep) -ne $hr) { throw "replacement $rep missing or wrong sha256 - run gi\build_gi.py" }
  if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
  if (-not (Test-Path -LiteralPath $bak)) {
    New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
    Copy-Item -LiteralPath $game -Destination $bak
    if ((Get-Sha $bak) -ne $ho) { throw "backup of $file failed" }
  }
  Copy-Item -LiteralPath $rep -Destination $game -Force
  if ((Get-Sha $game) -ne $hr) { throw "install of $file failed" }
  "gi: $file installed (backup: $bak)"
}
