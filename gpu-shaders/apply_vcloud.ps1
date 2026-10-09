<#
  apply_vcloud.ps1 - install / exact revert / status of the VCLOUD replacement for Nioh 3 (Steam v2.02).
    vcloud  shader_dx12\0x5f17ca9d.file record 7 = CS b76874bb (DrawSky async cloud ray march), notes\gpu-volumetrics-rewrite.md.
            The 4 light marches (2 layers x sun/moon, 4 steps each) are if-converted and their 12 mask/base/detail
            samples + sky-ambient/transmittance lookups are issued in 2 batches instead of one texture round trip
            per step. Same arithmetic; the other 7 records of the file byte-identical.
    -Variant u  (default, Windows) no pins: same expressions, dxc may reassociate (max rel. diff 1.3e-3 on extreme
                synthetic scenes, fp16-equal on 99.9 %); fewest instructions (gfx1102: 124 VGPR / 4 waves, = shipped).
    -Variant p  726 max(x, x) pins: bit-exact to the shipped DXIL on the interpreter (512 hold-out seeds); the Ally
                (RADV folds the pins) build. On Windows +17 % VALU.
    -Variant pfu / pfp  = u / p + both main ray-march loops software-pipelined: step i+1's mask + base samples are
                issued at the top of step i (prologue for step 0). pfp is bit-exact (interpreter, 768 seeds).

    powershell -NoProfile -ExecutionPolicy Bypass -File apply_vcloud.ps1 [-Variant u|p|pfu|pfp]   # install
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_vcloud.ps1 -Revert          # restore original
    powershell -NoProfile -ExecutionPolicy Bypass -File apply_vcloud.ps1 -Status          # what is installed

  Refuses while Nioh3 runs. Only overwrites a game file that is the known v2.02 original or one of our variants
  (anything else = game updated: rebuild with vcloud\build_vcloud.py). The original is copied to gpu-shaders\backup\
  (sha-checked) before the first overwrite; -Revert restores exactly that file. Steam "Verify integrity" also reverts.
  Files: vcloud\out\<variant>\0x5f17ca9d.file (or -Pack <dir> holding the <variant>\ folders).
#>
param([ValidateSet("u", "p", "pfu", "pfp")][string]$Variant = "u", [switch]$Revert, [switch]$Status, [string]$Pack = "",
      [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$file = "0x5f17ca9d.file"
$ho = "d92866c1ca2e3cb1025eba3f46ca4569d9e877701f90bdbb54e0cc383a843e71"
$hv = @{ u = "abf3ab1a9697cb5214a83ccbeae16ebb3a030042b90c92e3080059ec6a5edbcd";
         p = "fb596ee6cfc37acf512d9d3118056e09cf858562a4e27bd7b658ff283b47a110";
         pfu = "1ec99309d055cf32da5dd4cc4076e45b48c53ab46afbce87f29b9b11aba1f650";
         pfp = "732eede2c09e643414e7383945eb141ed518d1e57399d454dd0d0fbbef444ba4" }
function Get-Sha($p) { (Get-FileHash -Algorithm SHA256 -LiteralPath $p).Hash.ToLower() }
$game = Join-Path $GameDir "shader_dx12\$file"
$bak = Join-Path $here "backup\$file"
$src = if ($Pack) { $Pack } else { Join-Path $here "vcloud\out" }
$rep = Join-Path $src "$Variant\$file"
if (-not (Test-Path -LiteralPath $game)) { throw "missing game file: $game" }
$hg = Get-Sha $game
$state = "UNKNOWN($hg)"
if ($hg -eq $ho) { $state = "ORIGINAL" } else { foreach ($k in $hv.Keys) { if ($hg -eq $hv[$k]) { $state = "REPLACEMENT-$k" } } }
if ($Status) { "vcloud     {0,-16} {1}" -f $file, $state; exit 0 }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running - close it first" }
if ($state -like "UNKNOWN*") { throw "$file is neither the v2.02 original nor our replacement - game updated? not touching anything" }
if ($Revert) {
  if ($state -eq "ORIGINAL") { "vcloud: $file already original"; exit 0 }
  if (-not (Test-Path -LiteralPath $bak) -or (Get-Sha $bak) -ne $ho) { throw "no valid backup for $file in $here\backup - use Steam 'Verify integrity'" }
  Copy-Item -LiteralPath $bak -Destination $game -Force
  if ((Get-Sha $game) -ne $ho) { throw "restore of $file failed" }
  "vcloud: $file reverted to the original"
} else {
  if ($state -eq "REPLACEMENT-$Variant") { "vcloud: $file variant $Variant already installed"; exit 0 }
  if (-not (Test-Path -LiteralPath $rep) -or (Get-Sha $rep) -ne $hv[$Variant]) { throw "replacement $rep missing or wrong sha256 - run vcloud\build_vcloud.py" }
  if ((Test-Path -LiteralPath $bak) -and (Get-Sha $bak) -ne $ho) { throw "backup $bak exists but is not the v2.02 original - move it away and retry" }
  if (-not (Test-Path -LiteralPath $bak)) {
    if ($state -ne "ORIGINAL") { throw "a variant is installed and there is no backup - use Steam 'Verify integrity' first" }
    New-Item -ItemType Directory -Force (Split-Path $bak) | Out-Null
    Copy-Item -LiteralPath $game -Destination $bak
    if ((Get-Sha $bak) -ne $ho) { throw "backup of $file failed" }
  }
  Copy-Item -LiteralPath $rep -Destination $game -Force
  if ((Get-Sha $game) -ne $hv[$Variant]) { throw "install of $file failed" }
  "vcloud: $file variant $Variant installed (backup: $bak)"
}
