<#
  apply_wdo14.ps1 - install / exact revert / status of the WDO#14 VSM page-flag dedupe for Nioh 3 (Steam v2.02).
    shader_dx12\0x1667d3a0.file record #14 (WDO-PtSrbSsmVsm, sun-shadow resolve): the same DXIL with the repeated
    tVirtualShadowMapFlags loads behind a per-thread "same page as the last tap" test (notes\gpu-wdo14-flag-dedupe.md).
    Built by re\scripts_gpu\build_wdo14.py (proven bit-exact offline). OFF unless applied.

    powershell -NoProfile -ExecutionPolicy Bypass -File apply_wdo14.ps1            # install replacement
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_wdo14.ps1 -Revert    # restore original
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_wdo14.ps1 -Status    # show which one is installed

  Refuses while Nioh3 runs (the file is read at startup / PSO preload). SHA-256 checks everything it touches: it only
  overwrites a game file that is the known v2.02 original or our replacement (anything else = game updated: rebuild).
  The original is copied to gpu-shaders\backup\ (sha-checked) before the first overwrite; -Revert restores exactly that
  file. Steam "Verify integrity of game files" also reverts.
#>
param([switch]$Revert, [switch]$Status, [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$file = "0x1667d3a0.file"
$ho = "36bdb485805c15ea16b45321da135e98b386fa639ca76bd215aeca4648272a7c"   # v2.02 original
$hr = "60e2646cf2e46e9586013ae8ea4fb38ec902de36c176b9d9139a12c4ed328259"   # WDO14 flag dedupe
$game = Join-Path $GameDir "shader_dx12\$file"
$bak = Join-Path $here "backup\$file"
$rep = Join-Path $here "replacement\$file"
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
$hg = Get-Sha $game
$state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "REPLACEMENT" } else { "UNKNOWN($hg)" }
if ($Status) { "{0,-10} {1,-16} {2}" -f "wdo14", $file, $state; exit 0 }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running - close it first" }
if ($state -like "UNKNOWN*") { throw "$file is neither the v2.02 original nor our replacement - game updated? not touching anything" }
if ($Revert) {
  if ($state -eq "ORIGINAL") { "wdo14: $file already original"; exit 0 }
  if (-not (Test-Path -LiteralPath $bak) -or (Get-Sha $bak) -ne $ho) { throw "no valid backup for $file in $here\backup - use Steam 'Verify integrity'" }
  Copy-Item -LiteralPath $bak -Destination $game -Force
  if ((Get-Sha $game) -ne $ho) { throw "restore of $file failed" }
  "wdo14: $file reverted to the original"
} else {
  if ($state -eq "REPLACEMENT") { "wdo14: $file already installed"; exit 0 }
  if (-not (Test-Path -LiteralPath $rep) -or (Get-Sha $rep) -ne $hr) { throw "replacement $rep missing or wrong sha256 - run re\scripts_gpu\build_wdo14.py" }
  if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
  if (-not (Test-Path -LiteralPath $bak)) {
    New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
    Copy-Item -LiteralPath $game -Destination $bak
    if ((Get-Sha $bak) -ne $ho) { throw "backup of $file failed" }
  }
  Copy-Item -LiteralPath $rep -Destination $game -Force
  if ((Get-Sha $game) -ne $hr) { throw "install of $file failed" }
  "wdo14: $file installed (backup: $bak)"
}
