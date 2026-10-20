# menu_smoke_pc2.ps1 - install the pc/menu-combined-final ASIs, launch, title->field via VirtualPad, then serve commands from <out>\cmd.txt
# (one VP command per line; 'shot name', 'sleep N', 'log', 'quit' extra). Restores everything on 'quit'. Task #13353.
param([int]$OwnerPid = 0, [int]$MaxMinutes = 90, [string]$Src = "C:\Users\User\projects\nioh3-engine\captures\pc2-menufix2\dl", [string]$Out = "")
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here\harness_guard.ps1"
. "$here\vpad.ps1"
$repo = Split-Path -Parent (Split-Path -Parent $here)
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$stamp = Get-Date -f yyyyMMdd_HHmmss
if (-not $Out) { $Out = Join-Path $repo "captures\pc2-menufix2-run-$stamp" }
New-Item -ItemType Directory -Force "$Out\orig" | Out-Null
$trace = "$Out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$uo = "C:\Users\User\projects\universal-optimizer"
$env:UO_AGENT_ID = "pc2"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$files = "Nioh3EngineFix.asi","Nioh3EngineFix.ini","Nioh3BossRematch.asi","Nioh3BossRematch_bosses.tsv","Nioh3Training.asi","Nioh3Training.ini","Nioh3Training_arenas.tsv","Nioh3Training_bgm.tsv","Nioh3Training_actions.tsv","Nioh3Training.log","Nioh3EngineFix.log","Nioh3BossRematch.log"
$script:gamePid = 0; $script:locked = $false; $script:installed = $false
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(30000) | Out-Null; if (-not $p.HasExited) { T "game did not exit in 30 s - killing exact pid"; Stop-Process -Id $p.Id -Force }; Start-Sleep 5 }
    $script:gamePid = 0
}
function Restore {
    foreach ($f in "Nioh3Training.log","Nioh3EngineFix.log","Nioh3BossRematch.log") { Copy-Item "$G\$f" "$Out\$f" -Force -ErrorAction SilentlyContinue }
    if ($script:installed) {
        foreach ($f in $files) { if (Test-Path "$Out\orig\$f") { Copy-Item "$Out\orig\$f" "$G\$f" -Force } elseif (Test-Path "$Out\orig\$f.absent") { Remove-Item "$G\$f" -Force -ErrorAction SilentlyContinue } }
        robocopy "$Out\saves" $saveDir /MIR /NFL /NDL /NJH /NJS /NP | Out-Null
        $script:installed = $false; T "game files + saves restored"
    }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
    Remove-Item "C:\Users\User\AppData\Local\uo-agentd\pc2\test-active" -Force -ErrorAction SilentlyContinue
}
Init-Guard -Name "menu_smoke" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore }
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    Set-Content "C:\Users\User\AppData\Local\uo-agentd\pc2\test-active" (Get-Date -f s)
    foreach ($f in $files) { if (Test-Path "$G\$f") { Copy-Item "$G\$f" "$Out\orig\" -Force } else { Set-Content "$Out\orig\$f.absent" "" } }
    robocopy $saveDir "$Out\saves" /MIR /NFL /NDL /NJH /NJS /NP | Out-Null; T "saves backed up to $Out\saves"
    $script:installed = $true
    foreach ($f in "Nioh3EngineFix.asi","Nioh3BossRematch.asi","Nioh3BossRematch_bosses.tsv","Nioh3Training.asi") { Copy-Item "$Src\$f" "$G\$f" -Force }
    $m = $Src
    foreach ($f in "Nioh3Training_arenas.tsv","Nioh3Training_bgm.tsv","Nioh3Training.ini") { if (Test-Path "$m\$f") { Copy-Item "$m\$f" "$G\$f" -Force } }
    Remove-Item "$G\Nioh3Training.log","$G\Nioh3EngineFix.log","$G\Nioh3BossRematch.log" -Force -ErrorAction SilentlyContinue
    $ini = "$G\Nioh3EngineFix.ini"
    $t = [IO.File]::ReadAllText($ini); $t = Force-Acl (Force-Vpad $t)
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
    T "installed (existing EngineFix/Rematch inis kept, VirtualPad on); Training ini = branch default"
    Assert-Launch
    $before = VP-ForegroundInfo
    & (Join-Path $PSScriptRoot '../steam/start_autoplay.ps1')
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; Register-Game; T "game pid $($p.Id)"
    Start-Sleep 3; VP-GiveBackFocus $before
    if (-not (VP-WaitPipe 180)) { throw "no pipe" }
    T "ping: $((VP ping).reply)"
    $ok = VP-TitleContinue 300 "$Out\title_continue.txt"; T "title-continue: $ok"; T "state: $(VP-State)"
    if (-not $ok) { throw "not in the field" }
    "ready" | Set-Content "$Out\READY"
    T "READY - serving $Out\cmd.txt"
    $cmdf = "$Out\cmd.txt"; $done = 0; $quit = $false
    while (-not $quit) {
        Assert-Owner
        if (-not (Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue)) { T "GAME PROCESS GONE (crash?)"; Note-Crash "game exited"; break }
        if (Test-Path $cmdf) {
            $lines = @(Get-Content $cmdf)
            while ($done -lt $lines.Count) {
                $ln = $lines[$done].Trim(); $done++
                if (-not $ln -or $ln.StartsWith("#")) { continue }
                $w = $ln -split "\s+", 2
                switch ($w[0]) {
                    "quit"  { $quit = $true; T "quit requested"; break }
                    "sleep" { Start-Sleep ([int]$w[1]); "ok" | Add-Content "$Out\cmd.out"; break }
                    "shot"  { $f = "$Out\$($w[1]).png"; dotnet fsi (Join-Path $repo "tools\ui.fsx") shot $f 2>&1 | Out-Null; "$ln -> $f" | Add-Content "$Out\cmd.out"; break }
                    default { $rep = (Invoke-Expression "VP $ln"); "$ln -> $($rep.reply)" | Add-Content "$Out\cmd.out" }
                }
                if ($quit) { break }
            }
        }
        Start-Sleep 1
    }
} catch { T "!! $($_.Exception.Message)" }
finally { Close-Game; Restore; Done-Guard; T "out: $Out" }
