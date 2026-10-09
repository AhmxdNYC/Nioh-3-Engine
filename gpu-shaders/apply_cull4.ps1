<#
  apply_cull4.ps1 - install / exact revert / status of the round-4 meshlet-culling rewrite for Nioh 3 (Steam v2.02).
  notes\gpu-rewrite-round4.md. Item lodu = shader_dx12\0x8d9282e6.file with MshLod#2 (63c07c21, the per-cluster LOD
  kernel) given a wave-uniform fast path: when all lanes of a wave work on one mesh (WaveActiveAllEqual), the G1M
  offset rows are loaded once with a scalar index and the bindless G1M buffer needs no waterfall; otherwise the shipped
  code runs. Every UAV write / atomic / wave op is unchanged (proven: meshcull\verify_lod.py, meshcull\results\).

    powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_cull4.ps1            # install
    powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_cull4.ps1 -Revert    # exact restore
    powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_cull4.ps1 -Status

  Files come from gpu-shaders\replacement\cull4\ (meshcull\build_cull4.py) or, on another PC, gpu-shaders\replacement\
  (meshvs\splice_msv.py <cull4_records.zip> --flat). Table = gpu-shaders\meshcull\cull4_manifest.tsv.
  SAME FILE as the 'mshlod' item (MshLod#3): if mshlod is installed this script refuses (UNKNOWN sha); revert it first.
  Refuses while Nioh3 runs. Originals are backed up to gpu-shaders\backup\cull4\ (sha-checked). Steam verify also reverts.
#>
param([ValidateSet("lodu")][string]$Item = "lodu", [switch]$Revert, [switch]$Status,
      [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$man = Join-Path $here "meshcull\cull4_manifest.tsv"
if (-not (Test-Path -LiteralPath $man)) { throw "manifest $man missing" }
$rows = New-Object System.Collections.ArrayList   # (a one-row pipeline would unroll the row into its fields)
foreach ($l in (Get-Content -LiteralPath $man)) { if ($l -and -not $l.StartsWith("#")) { [void]$rows.Add([string[]]($l -split "`t")) } }
function Get-Sha($p) { if (Test-Path -LiteralPath $p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() } else { "" } }
if (-not $Status -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "Nioh3 is running - close it first" }
$plan = @()
foreach ($r in $rows) {
  $file, $ho, $hr = $r[0], $r[1], $r[2]
  $game = Join-Path $GameDir "shader_dx12\$file"
  $bak = Join-Path $here "backup\cull4\$file"
  $rep = Join-Path $here "replacement\cull4\$file"
  if ((Get-Sha $rep) -ne $hr) { $rep = Join-Path $here "replacement\$file" }
  if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
  $hg = Get-Sha $game
  $state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "REPLACEMENT" } else { "UNKNOWN" }
  if ($Status) { "{0,-18} {1}" -f $file, $state; continue }
  if ($state -eq "UNKNOWN") { throw "$file ($hg) is neither the v2.02 original nor the $Item replacement (mshlod installed, or game updated) - not touching anything" }
  if ($Revert) {
    if ($state -ne "ORIGINAL" -and (Get-Sha $bak) -ne $ho) { throw "no valid backup for $file - use Steam 'Verify integrity'" }
  } else {
    if ((Get-Sha $rep) -ne $hr) { throw "replacement $rep missing or wrong sha256 - run meshcull\build_cull4.py or meshvs\splice_msv.py <cull4_records.zip> --flat" }
    if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
  }
  $plan += , @($file, $ho, $hr, $game, $bak, $rep, $state)
}
if ($Status) { exit 0 }
$n = 0
foreach ($p in $plan) {
  $file, $ho, $hr, $game, $bak, $rep, $state = $p
  if ($Revert) {
    if ($state -eq "ORIGINAL") { continue }
    Copy-Item -LiteralPath $bak -Destination $game -Force
    if ((Get-Sha $game) -ne $ho) { throw "restore of $file failed" }
  } else {
    if ($state -eq "REPLACEMENT") { continue }
    if (-not (Test-Path -LiteralPath $bak)) {
      New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
      Copy-Item -LiteralPath $game -Destination $bak
      if ((Get-Sha $bak) -ne $ho) { throw "backup of $file failed" }
    }
    Copy-Item -LiteralPath $rep -Destination $game -Force
    if ((Get-Sha $game) -ne $hr) { throw "install of $file failed" }
  }
  $n++
}
"{0}: {1} {2} of {3} files" -f $Item, $(if ($Revert) { "reverted" } else { "installed" }), $n, $rows.Count
