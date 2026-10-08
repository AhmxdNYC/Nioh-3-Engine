<#
  apply_materials.ps1 - install / exact revert / status of the material-shader LOAD-DEDUPE replacements for Nioh 3 (Steam v2.02).
  Items (notes\gpu-materials-trim.md) - LOAD-DEDUPE: a vertex shader re-loads, right after writing outputs, the same
  read-only buffer dwords it loaded a few instructions earlier; the repeat is removed and the first value reused:
    trvs   shader_dx12\0x52c39db0.file  PB2TREE2 tree vertex shaders (#0-#3): 3 repeated instance-matrix loads each
    pbvs   shader_dx12\0xfd4f418f.file  PB2 meshlet vertex shaders (14 records): 5 repeated loads each

    powershell -NoProfile -ExecutionPolicy Bypass -File apply_materials.ps1 -Item trvs              # install
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_materials.ps1 -Item trvs -Revert      # restore original
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_materials.ps1 -Item all -Status      # what is installed

  Refuses while Nioh3 runs (the files are read at startup / PSO preload). Every file it touches is SHA-256 checked:
  it only overwrites a game file that is the known v2.02 original or our replacement (anything else = game updated:
  rebuild with materials\build_dedupe.py first). The original is copied to gpu-shaders\backup\ (sha-checked) before the
  first overwrite, and -Revert restores exactly that file. Steam "Verify integrity of game files" also reverts.
#>
param([ValidateSet("trvs", "pbvs", "all")][string]$Item = "all", [switch]$Revert, [switch]$Status,
      [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# item, file, original sha256 (v2.02), replacement sha256, superseded sha256s (older builds of our replacement)
$table = @(
  @("trvs", "0x52c39db0.file", "18f82522d1ba8cad1aaf7341ff3708bf40a1dc51eec072fe61205d95ce49cb91", "64679acf288bf5eb310a5415c68362b9ba750103d78f256a1c46f9af3cd33d2b", @()),
  @("pbvs", "0xfd4f418f.file", "806b2d7f343e67b5899585e80f116134fdf1af322191ccbee060443c8cd9da40", "a323fe723f5ee7223dd45a21782144f9d13508033229a7b954e8c8257d2d1a30", @())
)
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
$rows = @()
foreach ($t in $table) { if ($Item -eq "all" -or $t[0] -eq $Item) { $rows += , $t } }
if (-not $Status -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "Nioh3 is running - close it first" }
# pass 1: check everything before touching anything
$plan = @()
foreach ($r in $rows) {
  $name, $file, $ho, $hr, $old = $r
  $game = Join-Path $GameDir "shader_dx12\$file"
  $bak = Join-Path $here "backup\$file"
  $rep = Join-Path $here "replacement\$file"
  if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
  $hg = Get-Sha $game
  $state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "REPLACEMENT" } elseif ($old -contains $hg) { "SUPERSEDED" } else { "UNKNOWN($hg)" }
  if ($Status) { "{0,-10} {1,-16} {2}" -f $name, $file, $state; continue }
  if ($state -like "UNKNOWN*") { throw "$file is neither the v2.02 original nor our replacement - game updated? not touching anything" }
  if ($Revert) {
    if ($state -ne "ORIGINAL") {
      if (-not (Test-Path -LiteralPath $bak) -or (Get-Sha $bak) -ne $ho) { throw "no valid backup for $file in $here\backup - use Steam 'Verify integrity'" }
    }
  } else {
    if (-not (Test-Path -LiteralPath $rep) -or (Get-Sha $rep) -ne $hr) { throw "replacement $rep missing or wrong sha256 - run materials\build_dedupe.py" }
    if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
    if ($state -eq "SUPERSEDED" -and -not (Test-Path -LiteralPath $bak)) { throw "an older build of our $file is installed but there is no backup of the original - use Steam 'Verify integrity' first" }
  }
  $plan += , @($name, $file, $ho, $hr, $game, $bak, $rep, $state)
}
if ($Status) { exit 0 }
# pass 2: act
foreach ($p in $plan) {
  $name, $file, $ho, $hr, $game, $bak, $rep, $state = $p
  if ($Revert) {
    if ($state -eq "ORIGINAL") { "{0}: {1} already original" -f $name, $file; continue }
    Copy-Item -LiteralPath $bak -Destination $game -Force
    if ((Get-Sha $game) -ne $ho) { throw "restore of $file failed" }
    "{0}: {1} reverted to the original" -f $name, $file
  } else {
    if ($state -eq "REPLACEMENT") { "{0}: {1} already installed" -f $name, $file; continue }
    if (-not (Test-Path -LiteralPath $bak)) {
      New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
      Copy-Item -LiteralPath $game -Destination $bak
      if ((Get-Sha $bak) -ne $ho) { throw "backup of $file failed" }
    }
    Copy-Item -LiteralPath $rep -Destination $game -Force
    if ((Get-Sha $game) -ne $hr) { throw "install of $file failed" }
    "{0}: {1} installed (backup: {2})" -f $name, $file, $bak
  }
}
