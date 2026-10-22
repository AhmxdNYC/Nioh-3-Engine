# pc2_pj_bench.ps1 - PJ/PJH prewarm OFF vs ON route bench on pc2 (board task #106). Port of pc_retest_run8.ps1 Block P.
#   J PJ=1 empty journal (record, not scored) ; N PJ=0 ; W PJ=1 + title held for prewarm ; N2 PJ=0. Metric: tools\pj_hitch.py.
param([string]$Tag = "pj106", [switch]$NoN2)
$ErrorActionPreference = "Stop"
$ws = "C:\Users\User\projects\nioh3-engine"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $gameAsi = "$G\Nioh3EngineFix.asi"
$bak = "$ws\captures\pc2-$Tag\Nioh3EngineFix.asi.bak"; $ini = "$G\Nioh3EngineFix.ini"; $iniBak = "$ws\captures\pc2-$Tag\Nioh3EngineFix.ini.bak"
$out = "$ws\captures\pc2-$Tag"; New-Item -ItemType Directory -Force $out | Out-Null
$log = "$out\runner.txt"
$pjDir = "$G\Nioh3EngineFix_pj"; $pjUser = "$G\Nioh3EngineFix_pj.user_$Tag"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Add-Content $log $l; Write-Host $l }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is already running" }
Copy-Item $gameAsi $bak -Force; Copy-Item $ini $iniBak -Force
$caps = @{}
function Run-Tour([string]$label, [string]$keys, [switch]$Wait) {
    $capRoot = "$out\$label"
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$ws\tools\tour\pc2_run_tour.ps1", "-Input", "vpad", "-List", "$ws\tools\tour\tourlist_pj.txt",
           "-Points", "2", "-DwellSec", "4", "-NoProfile", "-NoPause", "-CaptureRoot", $capRoot, "-ExtraKeys", $keys, "-OwnerPid", "$PID")
    if ($Wait) { $a += "-WaitPrewarm" }
    L "$label start: $keys"
    & powershell @a *>> "$out\console.txt"
    L "$label exit $LASTEXITCODE"
    $cd = Get-ChildItem $capRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
    if ($cd) { $script:caps[$label] = $cd.FullName } else { L "${label}: no capture dir" }
}
try {
    if (Test-Path $pjDir) { Move-Item $pjDir $pjUser; L "user PJ journal moved" }
    $spr = "Patches.SPR=1"
    Run-Tour "J" "Patches.PJ=1;$spr"
    Run-Tour "N" "Patches.PJ=0;$spr"
    Run-Tour "W" "Patches.PJ=1;$spr" -Wait
    if (-not $NoN2) { Run-Tour "N2" "Patches.PJ=0;$spr" }
} finally {
    Get-Process Nioh3 -ErrorAction SilentlyContinue | ForEach-Object { $_.WaitForExit(60000) | Out-Null }
    if (Test-Path $pjDir) { Remove-Item $pjDir -Recurse -Force -ErrorAction SilentlyContinue }
    if (Test-Path $pjUser) { Move-Item $pjUser $pjDir; L "user PJ journal restored" }
    Copy-Item $bak $gameAsi -Force; Copy-Item $iniBak $ini -Force; L "asi+ini restored"
    $items = @(); foreach ($k in "J", "N", "W", "N2") { if ($caps[$k]) { $items += "$k=$($caps[$k])" } }
    if ($items.Count) { py -I "$ws\tools\pj_hitch.py" --out "$out\report.txt" @items | Out-Null; L "report $out\report.txt" }
    L "done"
}
