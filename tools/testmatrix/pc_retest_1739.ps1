# pc_retest_1739.ps1 - PC re-test per notes\pc-cpu-next.md s5 (Blocks A, C, B) on ASI 1.7.39, 2026-10-08.
# Takes the nioh3 game lock, installs bin\Nioh3EngineFix.asi (game ASI backed up), runs abba2_pc.ps1 per block,
# writes each report, then ALWAYS restores the game ASI (abba2 restores ini + config.xml itself), releases the lock
# and posts GAME-FREE. -Blocks picks/reorders blocks (default A,C,B).
param([string[]]$Blocks = @("A", "C", "B"), [string]$Tag = "run7")
$ErrorActionPreference = "Stop"
$ws = "C:\Users\Ahmad Hamza\nioh3-engine"; $here = "$ws\tools\testmatrix"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $gameAsi = "$G\Nioh3EngineFix.asi"
$newAsi = "$ws\mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi"; $bak = "$ws\captures\pc\Nioh3EngineFix.asi.pre1739.bak"
$br = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
$log = "$ws\captures\tm\abba2\${Tag}_runner.txt"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Add-Content $log $l; Write-Host $l }
function Lock([string]$a) { Push-Location $br; try { py -m uo_bridge.gamelock $a nioh3 --ttl 5400 2>&1 | Out-String } finally { Pop-Location } }
function Send([string]$m) { Push-Location $br; try { py -m uo_bridge send nioh3 $m | Out-Null } finally { Pop-Location } }

$Blocks = @($Blocks -split "," | ForEach-Object { $_.Trim().ToUpper() } | Where-Object { $_ })
$args_ = @{
    A = @("-Live", "AABB,DQ,P1E7", "-LaunchFlags", "none", "-Quads", "3")
    C = @("-Arms", "BASE,MA,BASE,MA,BASE", "-Live", "none")
    B = @("-Arms", "BASE,DQ,P1E7,BASE,P1E7,DQ,BASE", "-Live", "none")
}
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is already running" }
$st = Lock "status"; L "lock status: $($st.Trim())"
if ($st -match "'holder': '(\w+)'" -and $Matches[1] -ne "pc") { throw "game lock held by $($Matches[1])" }
L ("acquire: " + (Lock "acquire").Trim())
Send "[FYI] pc: lock taken $(Get-Date -f HH:mm) for the PC re-test on ASI 1.7.39 (pc-cpu-next.md s5: blocks $($Blocks -join ','); ~80 min). Will post [GAME-FREE] when done."
try {
    if (-not (Test-Path $bak)) { Copy-Item $gameAsi $bak; L "game ASI backed up -> $bak" }
    Copy-Item $newAsi $gameAsi -Force; L "installed 1.7.39 ASI ($((Get-Item $gameAsi).Length) bytes)"
    foreach ($b in $Blocks) {
        $rn = "${Tag}_$b"; L "block $b start ($rn)"
        & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\abba2_pc.ps1" @($args_[$b]) -MaxFps 240 -Input vpad -RunName $rn -OwnerPid $PID *>> "$ws\captures\tm\abba2\${rn}_console.txt"
        L "block $b exit $LASTEXITCODE"
        py -I "$here\abba2_report.py" "$ws\captures\tm\abba2\$rn" *> $null
        L "block $b report: $(if (Test-Path "$ws\captures\tm\abba2\$rn\report.txt") { 'written' } else { 'MISSING' })"
        Push-Location $br; try { py -m uo_bridge.gamelock renew nioh3 --ttl 5400 | Out-Null } finally { Pop-Location }
    }
} finally {
    Get-Process Nioh3 -ErrorAction SilentlyContinue | ForEach-Object { L "game still running pid $($_.Id) - waiting 60 s"; $_.WaitForExit(60000) | Out-Null }
    if (Test-Path $bak) { Copy-Item $bak $gameAsi -Force; L "game ASI restored from $bak" }
    L ("release: " + (Lock "release").Trim())
    Send "[GAME-FREE] game=nioh3 from=pc (PC re-test $Tag finished $(Get-Date -f HH:mm))."
    L "done"
}
