<#
  apply_sweep.ps1 - install / exact revert / status of the shader-sweep replacements for Nioh 3 (Steam v2.02).
  Items (notes\gpu-shader-sweep.md):
    mpc23      shader_dx12\0x29ee0902.file  MPC #23: meshlet-culling prefix sums, 1 load round trip + LDS scan
    toonedge2  shader_dx12\0x9c481f79.file + 0x47bc5cb4.file  TOONEDGE2 #2: outline pass, 18x18 LDS tile

    powershell -NoProfile -ExecutionPolicy Bypass -File apply_sweep.ps1 -Item mpc23            # install
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_sweep.ps1 -Item mpc23 -Revert    # restore original
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_sweep.ps1 -Item all -Status      # what is installed

  Refuses while Nioh3 runs (the files are read at startup / PSO preload). Every file it touches is SHA-256 checked:
  it only overwrites a game file that is the known v2.02 original or our replacement (anything else = game updated:
  rebuild with sweep\build_sweep.py first). The original is copied to gpu-shaders\backup\ (sha-checked) before the
  first overwrite, and -Revert restores exactly that file. Steam "Verify integrity of game files" also reverts.
#>
param([ValidateSet("mpc23", "toonedge2", "all")][string]$Item = "all", [switch]$Revert, [switch]$Status,
      [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
# item, file, original sha256 (v2.02), replacement sha256
$table = @(
  @("mpc23",     "0x29ee0902.file", "42f045c370e99b80626c64e9d8084a4a855bb3baad3b04b8f634114d34b97b16", "26417c0ff41b128443ce364a69e68328588abd5f044129fe1a8c2e8dea80cec7"),
  @("toonedge2", "0x9c481f79.file", "7a8171617b2530648e12ce9dd52c091faf31cd4f1026d3b40a7fdde5788ac1ae", "d672a5d530f9d1cb49f37ee3de6f626d95e7273eb897e17de3683a8dfb2c2e6d"),
  @("toonedge2", "0x47bc5cb4.file", "07542a8322a617b7783b0c7b79580689339429c1d1e416bf70c8d0f3b0aee99a", "9a37b6e45360bb8664746687757c68ebfd33cdeb711a6459bbbe29cee111ef80")
)
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
$rows = @()
foreach ($t in $table) { if ($Item -eq "all" -or $t[0] -eq $Item) { $rows += , $t } }
if (-not $Status -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "Nioh3 is running - close it first" }
# pass 1: check everything before touching anything
$plan = @()
foreach ($r in $rows) {
  $name, $file, $ho, $hr = $r
  $game = Join-Path $GameDir "shader_dx12\$file"
  $bak = Join-Path $here "backup\$file"
  $rep = Join-Path $here "replacement\$file"
  if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
  $hg = Get-Sha $game
  $state = if ($hg -eq $ho) { "ORIGINAL" } elseif ($hg -eq $hr) { "REPLACEMENT" } else { "UNKNOWN($hg)" }
  if ($Status) { "{0,-10} {1,-16} {2}" -f $name, $file, $state; continue }
  if ($state -like "UNKNOWN*") { throw "$file is neither the v2.02 original nor our replacement - game updated? not touching anything" }
  if ($Revert) {
    if ($state -eq "REPLACEMENT") {
      if (-not (Test-Path -LiteralPath $bak) -or (Get-Sha $bak) -ne $ho) { throw "no valid backup for $file in $here\backup - use Steam 'Verify integrity'" }
    }
  } else {
    if (-not (Test-Path -LiteralPath $rep) -or (Get-Sha $rep) -ne $hr) { throw "replacement $rep missing or wrong sha256 - run sweep\build_sweep.py" }
    if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
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
