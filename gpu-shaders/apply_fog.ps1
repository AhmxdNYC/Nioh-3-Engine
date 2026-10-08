<#
  apply_fog.ps1 - install / exact revert / status of the volumetric-fog replacement for Nioh 3 (Steam v2.02).
    fog  shader_dx12\0xa9910db8.file  NOW the 'fogpar' build (2026-10-08): only item 1 below (merged early-exit VSM
         search; no prefetch / tail reorder), see notes\ally-gpu-budget.md s6. Was: VFSCAT #235 (volumetric fog froxel scattering): one merged early-exit
         virtual-shadow-map level search for the 3 sun samples, software-pipelined clustered light list, tail
         loads issued together. Bit-exact to the shipped DXIL on the interpreter (notes\gpu-fog-rewrite.md); every
         other record of the file byte-identical. Meant for the ROG Ally (latency-bound); PC gain is model-only.

    powershell -NoProfile -ExecutionPolicy Bypass -File apply_fog.ps1            # install
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_fog.ps1 -Revert    # restore original
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_fog.ps1 -Status    # what is installed

  Refuses while Nioh3 runs. Only overwrites a game file that is the known v2.02 original or our replacement (anything
  else = game updated: rebuild with fog\mkfog.py + gi\mkfast.py vf235 + fog\build_fog.py). The original is copied to
  gpu-shaders\backup\ (sha-checked) before the first overwrite; -Revert restores exactly that file. Steam "Verify
  integrity" also reverts.
#>
param([switch]$Revert, [switch]$Status,
      [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$file = "0xa9910db8.file"
$ho = "4784aabe40fdb10646babde1df734123ce713dc56089a55b4efa0db43bf4f993"
$hr = "81007b3f2bc066e4f4951b9bc4bac37be29bdb934efbc9fe83275d65b767334e"   # fogpar (2026-10-08, fog\build_fogpar.py)
$hold = "ae04ee6ac2920c62cac9d694428f8105e42c45ef9c34b13426dead6d97190e6d" # superseded full rewrite (dropped: slower on the Ally)
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
$game = Join-Path $GameDir "shader_dx12\$file"
$bak = Join-Path $here "backup\$file"
$rep = Join-Path $here "replacement\$file"
if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
$hg = Get-Sha $game
$state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "REPLACEMENT" } elseif ($hg -eq $hold) { "SUPERSEDED" } else { "UNKNOWN($hg)" }
if ($Status) { "fog        {0,-16} {1}" -f $file, $state; exit 0 }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running - close it first" }
if ($state -like "UNKNOWN*") { throw "$file is neither the v2.02 original nor our replacement - game updated? not touching anything" }
if ($Revert) {
  if ($state -eq "ORIGINAL") { "fog: $file already original"; exit 0 }
  if (-not (Test-Path -LiteralPath $bak) -or (Get-Sha $bak) -ne $ho) { throw "no valid backup for $file in $here\backup - use Steam 'Verify integrity'" }
  Copy-Item -LiteralPath $bak -Destination $game -Force
  if ((Get-Sha $game) -ne $ho) { throw "restore of $file failed" }
  "fog: $file reverted to the original"
} else {
  if ($state -eq "REPLACEMENT") { "fog: $file already installed"; exit 0 }
  if (-not (Test-Path -LiteralPath $rep) -or (Get-Sha $rep) -ne $hr) { throw "replacement $rep missing or wrong sha256 - run fog\build_fogpar.py" }
  if ($state -eq "SUPERSEDED" -and -not ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -eq $ho)) { throw "old fog replacement installed and no valid backup - use Steam 'Verify integrity' first" }
  if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
  if (-not (Test-Path -LiteralPath $bak)) {
    New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
    Copy-Item -LiteralPath $game -Destination $bak
    if ((Get-Sha $bak) -ne $ho) { throw "backup of $file failed" }
  }
  Copy-Item -LiteralPath $rep -Destination $game -Force
  if ((Get-Sha $game) -ne $hr) { throw "install of $file failed" }
  "fog: $file installed (backup: $bak)"
}
