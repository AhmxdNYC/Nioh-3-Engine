<#
  apply_msv.ps1 - install / exact revert / status of MSV (meshlet-VS slot-chain scalarization) for Nioh 3 (Steam v2.02).
  notes\gpu-rewrite-round3.md. The meshlet vertex shaders load a ~7-deep chain of buffer values that only depend on the
  meshlet slot (vid/384); MSV evaluates that chain once per distinct slot in the wave (WaveReadLaneFirst waterfall), so the
  driver issues it as scalar (SMEM) loads instead of 64-lane vector loads. Output is identical by construction (proven
  per record in the DXIL interpreter, meshvs\verify_msv.py).

    -Set hot : 7 files  (the 3 meshlet VS that rank in the pc2 RDNA2 GP2 budget: PB2 1559714c / 70d0ee9f, PB2TREE2 f311c8e2)
    -Set all : 187 files (every meshlet VS: G-buffer PB2/PB2TREE2, shadow SM/SMTREE2, DEPTH; 1616 unique shaders)

    powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_msv.ps1 -Set hot            # install
    powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_msv.ps1 -Set hot -Revert    # exact restore
    powershell -NoProfile -ExecutionPolicy Bypass -File gpu-shaders\apply_msv.ps1 -Set all -Status    # per-file state

  Files come from gpu-shaders\replacement\msv_<set>\ (build_msv.py, or on another PC: meshvs\splice_msv.py <pack.zip>).
  Table = gpu-shaders\meshvs\msv_manifest_<set>.tsv (file, sha256 v2.02 original, sha256 replacement).
  Refuses while Nioh3 runs. Checks EVERY file before touching any: a game file must be the v2.02 original or our
  replacement. Shared files: 0xfd4f418f.file is also the 'pbvs' item (apply_materials.ps1) - if pbvs is installed this
  script refuses (UNKNOWN sha); revert pbvs first. Originals are backed up to gpu-shaders\backup\msv\ (sha-checked);
  -Revert restores exactly those. Steam "Verify integrity of game files" also reverts.
#>
param([ValidateSet("hot", "all")][string]$Set = "hot", [switch]$Revert, [switch]$Status,
      [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$man = Join-Path $here "meshvs\msv_manifest_$Set.tsv"
if (-not (Test-Path -LiteralPath $man)) { throw "manifest $man missing" }
$rows = Get-Content -LiteralPath $man | Where-Object { $_ -and -not $_.StartsWith("#") } | ForEach-Object { , ($_ -split "`t") }
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
if (-not $Status -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "Nioh3 is running - close it first" }
$plan = @(); $cnt = @{}
foreach ($r in $rows) {
  $file, $ho, $hr = $r[0], $r[1], $r[2]
  $game = Join-Path $GameDir "shader_dx12\$file"
  $bak = Join-Path $here "backup\msv\$file"
  $rep = Join-Path $here "replacement\msv_$Set\$file"
  if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
  $hg = Get-Sha $game
  $state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "REPLACEMENT" } else { "UNKNOWN" }
  $cnt[$state] = 1 + [int]$cnt[$state]
  if ($Status) { if ($state -ne "ORIGINAL" -or $rows.Count -le 10) { "{0,-18} {1}" -f $file, $state }; continue }
  if ($state -eq "UNKNOWN") { throw "$file ($hg) is neither the v2.02 original nor the msv_$Set replacement (other item installed, other MSV set, or game updated) - not touching anything" }
  if ($Revert) {
    if ($state -ne "ORIGINAL" -and (-not (Test-Path -LiteralPath $bak) -or (Get-Sha $bak) -ne $ho)) { throw "no valid backup for $file - use Steam 'Verify integrity'" }
  } else {
    if (-not (Test-Path -LiteralPath $rep) -or (Get-Sha $rep) -ne $hr) { throw "replacement $rep missing or wrong sha256 - run meshvs\build_msv.py --set $Set or meshvs\splice_msv.py" }
    if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
  }
  $plan += , @($file, $ho, $hr, $game, $bak, $rep, $state)
}
if ($Status) { "msv_{0}: {1} files: {2}" -f $Set, $rows.Count, (($cnt.GetEnumerator() | ForEach-Object { "$($_.Key)=$($_.Value)" }) -join " "); exit 0 }
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
"msv_{0}: {1} {2} of {3} files" -f $Set, $(if ($Revert) { "reverted" } else { "installed" }), $n, $rows.Count
