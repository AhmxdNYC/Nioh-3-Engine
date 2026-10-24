# pj_bench_105_pc.ps1 - board #105: PJ/PJH OFF vs ON on THIS PC (RX 9070) via tools\tour\run_tour.ps1 (PC-native paths).
# Arms N (PJ=0), J (PJH=1 cold, empty journal), W (PJH=1 warm, -WaitPrewarm), N2 (PJ=0). Installed user build is used (NIOH3_ASI).
$ErrorActionPreference = "Stop"
$ws = "C:\Users\Ahmad Hamza\nioh3-engine"; $G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$br = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$bk = "$ws\captures\pc\play-20261022-pre-pjbench"; $out = "$ws\captures\pc\pjbench105"
$log = "$out\driver.log"; $marker = "C:\Users\Ahmad Hamza\AppData\Local\uo-agentd\pc\test-active"
$pjDir = "$G\Nioh3EngineFix_pj"; $pjUser = "$G\Nioh3EngineFix_pj.user_pjbench"
New-Item -ItemType Directory -Force $bk, $out | Out-Null
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; try { Add-Content $log $l -ErrorAction Stop } catch {}; Write-Host $l }
function Lock([string]$a) { Push-Location $br; try { $env:UO_AGENT_ID = "pc"; (py -m uo_bridge.gamelock $a nioh3 --ttl 7200 2>&1 | Out-String).Trim() } finally { Pop-Location } }
$files = "Nioh3EngineFix.asi", "Nioh3EngineFix.ini", "Nioh3Training.asi", "Nioh3Training.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch.ini"
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 running" }
Set-Content $marker "pj_bench_105 pid $PID $(Get-Date -f s)"
$hash = @{}
foreach ($f in $files) { Copy-Item "$G\$f" $bk -Force; $hash[$f] = (Get-FileHash "$G\$f").Hash }
$hash | ConvertTo-Json | Set-Content "$bk\hashes.json"
Copy-Item "$env:LOCALAPPDATA\KoeiTecmo\NIOH3" "$bk\saves" -Recurse -Force
L "backup done -> $bk"
L ("lock acquire: " + (Lock "acquire"))
$env:NIOH3_ASI = "$bk\Nioh3EngineFix.asi"
$caps = @{}
try {
    if (Test-Path $pjDir) { Move-Item $pjDir $pjUser; L "user journal moved aside" }
    $arms = @(@("N", "Patches.PJ=0;Patches.SPR=1", $false), @("J", "Patches.PJH=1;Patches.SPR=1", $false),
              @("W", "Patches.PJH=1;Patches.SPR=1", $true), @("N2", "Patches.PJ=0;Patches.SPR=1", $false))
    foreach ($a in $arms) {
        $cr = "$out\$($a[0])"
        $args2 = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$ws\tools\tour\run_tour.ps1", "-Input", "vpad", "-List", "$ws\tools\tour\tourlist_pj.txt",
                   "-Points", "2", "-DwellSec", "4", "-NoProfile", "-NoPause", "-NoLock", "-CaptureRoot", $cr, "-ExtraKeys", $a[1], "-OwnerPid", "$PID")
        if ($a[2]) { $args2 += "-WaitPrewarm" }
        L "arm $($a[0]) start ($($a[1]))"
        & powershell @args2 *>> "$out\console_$($a[0]).txt"
        L "arm $($a[0]) exit $LASTEXITCODE"
        $cd = Get-ChildItem $cr -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
        if ($cd) { $caps[$a[0]] = $cd.FullName }
        L ("lock renew: " + (Lock "renew"))
    }
} finally {
    Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 5
    if (Test-Path $pjDir) { Remove-Item $pjDir -Recurse -Force; L "test journal removed" }
    if (Test-Path $pjUser) { Move-Item $pjUser $pjDir; L "user journal restored" }
    foreach ($f in $files) { Copy-Item "$bk\$f" "$G\$f" -Force; $ok = (Get-FileHash "$G\$f").Hash -eq $hash[$f]; L "restore $f hash ok=$ok" }
    $items = @(); foreach ($k in "N", "J", "W", "N2") { if ($caps[$k]) { $items += "$k=$($caps[$k])" } }
    if ($items.Count) { py -I "$ws\tools\pj_hitch.py" --out "$out\pj_hitch_report.txt" @items *>> $log }
    L ("lock release: " + (Lock "release"))
    Remove-Item $marker -Force -ErrorAction SilentlyContinue
    Start-Process powershell -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', 'C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1', '-Minutes', '15' -WindowStyle Hidden
    Start-Process "steam://rungameid/3681010"
    L "user build restored + game relaunched; ALL DONE"
}
