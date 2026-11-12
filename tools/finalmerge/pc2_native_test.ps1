# Per catalogue row with an arena point: `fight <id>` over the add-on pipe -> the mission loads at the boss start point -> the add-on
# warps into the arena trigger and verifies (status fnow=pass: a BossBattleMode after the warp; fnow=fail: none within 8 s, after up
# to 3 warps) -> `exit` (MissionRequestReturn, as the mission menu's leave) -> next row. Results: captures\rematch\fightnow-<stamp>.csv.
#
# Safety: never touches a running game - it WAITS until Nioh3 is closed (-WaitMinutes), then takes the bridge game lock 'nioh3'
# (these runs write saves: no other device may run the game). Save folder (%LOCALAPPDATA%\KoeiTecmo\NIOH3) copied to
# captures\save-backups\<stamp>-hudtest first and restored EXACTLY (robocopy /MIR) after the game is closed. Input only through
# VirtualPad (the perf ASI's pipe; no OS input except Steam's 'Play anyway' cloud dialog via steam_cloud_click.ps1). Processes are
# closed / killed only by the exact pid registered at launch (harness_guard). Game-folder files touched (both inis, the add-on asi,
# catalogue) are restored at the end, except the add-on build + catalogue under test stay installed when -KeepBuild (default on)
# and the arena captures file (new data) is kept.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\rematch\fightnow_test.ps1 -OwnerPid <agent pid> [-Rows R1CBC,R15FF]
#        [-IncludeAtFight] [-WaitMinutes 480] [-MaxMinutes 240] [-LoadWaitSec 240] [-DryRun]
param([int]$OwnerPid = 0, [string]$Device = "pc", [string]$Plan = "", [string]$ResultsCsv = "", [string]$Row = "R7B35", [int]$MeasureSec = 60, [int]$SettleSec = 8, [int]$WarmupSec = 30, [int]$MaxRetries = 1, [int]$MaxRuns = 0,
      [int]$WaitMinutes = 480, [int]$MaxMinutes = 60, [int]$LoadWaitSec = 240, [string]$PerfAsi = "", [bool]$KeepBuild = $true, [switch]$Fast, [switch]$DryRun, [int]$ArmSec = 240, [long]$BossHp = 30, [string]$BrAsi = "", [string]$TrAsi = "")
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here\..\rematch\audit_capture_helpers.ps1"
# Fail before waiting, launching, locks or save changes; no unattended UAC prompt.
if (-not $DryRun) {
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
    $principal=New-Object Security.Principal.WindowsPrincipal($identity)
    $plu=$principal.IsInRole("Performance Log Users")  # ETW sessions are also allowed for this group (stage1 ran so)
}
$tm = Join-Path (Split-Path -Parent $here) "testmatrix"
$repo = Split-Path -Parent (Split-Path -Parent $here)
$AddonDir = Join-Path $repo "mod\Nioh3BossRematch"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$perfIni = "$GameDir\Nioh3EngineFix.ini"
$aAsi = "$GameDir\Nioh3BossRematch.asi"; $aIni = "$GameDir\Nioh3BossRematch.ini"; $aCat = "$GameDir\Nioh3BossRematch_bosses.tsv"; $aLog = "$GameDir\Nioh3BossRematch.log"
$srcCat = Join-Path $AddonDir "Nioh3BossRematch_bosses.tsv"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
# VirtualPad needs the perf ASI >= 1.7.37: the test installs this one for its run and puts the user's back afterwards
if (-not $PerfAsi) { $PerfAsi = "$GameDir\Nioh3EngineFix.asi" }
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = Join-Path $repo "captures\audit-screen\run-$Device-$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$csv = ""
$saveBak = Join-Path $repo "captures\save-backups\$stamp-soulcore"
$trace = "$out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }

# ---- wait for the user's game to be closed (never touch it) ----
$t0 = Get-Date
while (-not $DryRun -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
    if (((Get-Date) - $t0).TotalMinutes -gt $WaitMinutes) { T "Nioh3 still running after $WaitMinutes min - giving up (nothing touched)"; exit 2 }
    if ($OwnerPid -and -not (Get-Process -Id $OwnerPid -ErrorAction SilentlyContinue)) { T "owner $OwnerPid gone while waiting - exit"; exit 99 }
    Start-Sleep 30
}
T "Nioh3 is not running (waited $([math]::Round(((Get-Date) - $t0).TotalMinutes, 1)) min)"
$preSleep = if ($Fast) { 3 } else { 20 }
if (-not $DryRun) { Start-Sleep $preSleep }   # -Fast skips the long save-flush wait
if (-not $DryRun -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { T "Nioh3 started again - exit (nothing touched)"; exit 2 }

. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$uo = if ($BridgeDir) { $BridgeDir } else { Join-Path (Split-Path -Parent $repo) "universal-optimizer\agent-bridge" }
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = $Device
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3-$Device --ttl 14400 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
function Set-IniKey([string]$t, [string]$sec, [string]$key, [string]$val) {
    $nl = "`r`n"
    $m = [regex]::Match($t, "(?ms)^\[$([regex]::Escape($sec))\][^\r\n]*\r?\n(.*?)(?=^\[|\z)")
    if (-not $m.Success) { return $t.TrimEnd() + "$nl$nl[$sec]$nl$key = $val$nl" }
    $body = $m.Groups[1].Value
    $re = "(?m)^$([regex]::Escape($key))[ \t]*=.*?(\r?)$"
    if ([regex]::IsMatch($body, $re)) { $nb = [regex]::Replace($body, $re, "$key = $val`$1", 1) } else { $nb = "$key = $val$nl" + $body }
    return $t.Substring(0, $m.Groups[1].Index) + $nb + $t.Substring($m.Groups[1].Index + $m.Groups[1].Length)
}
function RMP([string]$cmd, [int]$timeoutMs = 5000) {
    if ($global:HG) { Assert-Owner }
    if ($DryRun) { T "  [dry] rm $cmd"; return "dry" }
    $p = New-Object IO.Pipes.NamedPipeClientStream(".", "nioh3bossrematch", [IO.Pipes.PipeDirection]::InOut)
    try {
        $p.Connect($timeoutMs)
        $w = New-Object IO.StreamWriter($p); $w.AutoFlush = $true; $rd = New-Object IO.StreamReader($p)
        $w.WriteLine($cmd)
        $t = $rd.ReadLineAsync(); if (-not $t.Wait($timeoutMs)) { return "error timeout" }
        return $t.Result
    } catch { return "error $($_.Exception.Message)" } finally { $p.Dispose() }
}
function F([string]$reply, [string]$key) { if ($reply -match "(?:^| )$key=(\S+)") { return $Matches[1] }; return "" }
function LastOf([string]$reply) { if ($reply -match ' last=(.*)$') { return $Matches[1] }; return "" }
function Wait-RM([scriptblock]$cond, [int]$sec, [int]$poll = 1) {
    $t0 = Get-Date
    do { $s = RMP status; if (& $cond $s) { return $s }; Start-Sleep $poll } while (((Get-Date) - $t0).TotalSeconds -lt $sec)
    return $null
}

$script:modFiles = @("dinput8.dll","Nioh3EngineFix.asi","Nioh3BossRematch.asi","Nioh3Training.asi","Nioh3CamKiProbe.asi"); $script:gamePid = 0; $script:locked = $false; $script:installed = $false; $script:saved = $false; $script:had = @{}; $script:launchN = 0
function Launch {
    Assert-Launch
    $before = VP-ForegroundInfo
    & (Join-Path $PSScriptRoot '../steam/start_autoplay.ps1')
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) {
        $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }
        $cc = "$(& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $tm 'steam_cloud_click.ps1') 2>$null | Select-Object -First 1)"
        if ($cc -like 'clicked*') { T "  Steam cloud dialog: $cc" }
        Start-Sleep 3
    }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; $script:launchN++; Register-Game; T "launch #$($script:launchN): game pid $($p.Id)"
    Start-Sleep 3; Set-AuditForeground $script:gamePid; Start-Sleep 1
    if (-not (VP-WaitPipe 180)) { throw "no VirtualPad pipe (perf ASI with VirtualPad = 1?)" }
    $ok = VP-TitleContinue 300 "$out\title_continue_$($script:launchN).txt"
    $pg = RMP ping; T "title -> Continue -> field: $ok; add-on: $pg"
    if ($pg -notmatch 'live=1') { throw "add-on not live: $pg" }
    if (-not (Wait-RM { param($x) (F $x 'player') -eq '1' -and (F $x 'loading') -eq '0' } 120)) { throw "no player after Continue" }
    Start-Sleep $(if ($Fast) { 2 } else { 5 })
    return $ok
}
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 45 s - killing pid $($p.Id)"; Stop-Process -Id $p.Id -Force }; Start-Sleep $(if ($Fast) { 2 } else { 8 }) }
    $script:gamePid = 0
    if(Test-Path "$GameDir\Nioh3Training.log"){Copy-Item "$GameDir\Nioh3Training.log" "$out\training_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue}
    if (Test-Path $aLog) { Copy-Item $aLog "$out\addon_launch$($script:launchN).log" -Force -ErrorAction SilentlyContinue }
}
function Restore {
    if ($script:installed) {
        foreach ($f in $script:modFiles) { $on = Join-Path $GameDir $f; if (-not (Test-Path $on) -and (Test-Path "$on.mod-off")) { Rename-Item "$on.mod-off" $f; T "put back parked $f" } elseif (Test-Path "$on.mod-off") { Remove-Item "$on.mod-off" -Force; T "removed stray $f.mod-off" } }
        foreach ($n in @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv")) {
            if ($KeepBuild -and $n -in @("Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv")) { continue }
            $dst = Join-Path $GameDir $n
            if ($script:had[$n]) { Copy-Item "$out\orig\$n" $dst -Force } else { Remove-Item $dst -Force -ErrorAction SilentlyContinue }
        }
        $script:installed = $false; T "game folder restored (inis, perf ASI$(if (-not $KeepBuild) { ', add-on asi, catalogue' })); arena captures kept"
    }
    if ($script:saved -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        $o = robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; $code = $LASTEXITCODE
        T "save folder restored from $saveBak (robocopy /MIR exit $code)"
        $a = Get-ChildItem $saveBak -Recurse -File | ForEach-Object { "$($_.FullName.Substring($saveBak.Length))|$($_.Length)|$((Get-FileHash $_.FullName -Algorithm SHA1).Hash)" } | Sort-Object
        $b = Get-ChildItem $saveDir -Recurse -File | ForEach-Object { "$($_.FullName.Substring($saveDir.Length))|$($_.Length)|$((Get-FileHash $_.FullName -Algorithm SHA1).Hash)" } | Sort-Object
        $same = (@(Compare-Object $a $b).Count -eq 0)
        T "save restore verified byte for byte: $same ($($a.Count) files)"
        if (-not $same) { T "!! SAVE DIFFERS FROM THE BACKUP - backup kept at $saveBak" }
        $script:saved = $false
    }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}



function Shot([string]$name) {
    try { Add-Type -AssemblyName System.Windows.Forms, System.Drawing -ErrorAction Stop
        $b = [Windows.Forms.SystemInformation]::VirtualScreen; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
        $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $bmp.Save("$out\$name.png", [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose() } catch { T "  shot $name failed: $($_.Exception.Message)" }
}
function Combat-Pass {
    foreach ($c in @("stick L 0 1 300","press RB 80","wait 400","press RB 80","wait 400","press RB 80","wait 400","press RT 80","wait 500","stick L 0 1 500","press RB 80","wait 400","press RB 80","wait 500")) { $null = VP $c }
}
$tlog = "$GameDir\Nioh3Training.log"; $CMD = "$GameDir\Nioh3Training.cmd"
$res = "$out\result.txt"
function Rr([string]$m) { T $m; Add-Content $res $m }
function Shot([string]$name) {
    try { Add-Type -AssemblyName System.Windows.Forms, System.Drawing -ErrorAction Stop
        $b = [Windows.Forms.SystemInformation]::VirtualScreen; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
        $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $bmp.Save("$out\$name.png", [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose() } catch { T "  shot $name failed: $($_.Exception.Message)" }
}

function Menu-Open { $null = VP "key F10 120"; Start-Sleep -Milliseconds 900 }
function Menu-Close { $null = VP "key F10 120"; Start-Sleep -Milliseconds 700 }
function Pad([string]$b, [int]$n = 1) { for ($i = 0; $i -lt $n; $i++) { $null = VP "press $b 90"; Start-Sleep -Milliseconds 400 } }
function Parked { @($script:modFiles | Where-Object { Test-Path (Join-Path $GameDir "$_.mod-off") }) -join "," }
function EfLogTail([string]$pat) { if (Test-Path "$GameDir\Nioh3EngineFix.log") { (Select-String -Path "$GameDir\Nioh3EngineFix.log" -Pattern $pat | Select-Object -Last 1).Line } }
Init-Guard -Name "soulcore" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore }
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    New-Item -ItemType Directory -Force $saveBak | Out-Null
    $o = robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "saves backed up (exit $LASTEXITCODE)"
    if ($LASTEXITCODE -ge 8) { throw "save backup failed" }
    $script:saved = $true
    foreach ($n in @("Nioh3EngineFix.ini", "Nioh3EngineFix.asi", "Nioh3Training.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.asi", "Nioh3BossRematch_bosses.tsv")) {
        $p = Join-Path $GameDir $n; $script:had[$n] = Test-Path $p; if ($script:had[$n]) { Copy-Item $p "$out\orig\" -Force }
    }
    $script:installed = $true
    if ($BrAsi) { Copy-Item $BrAsi "$GameDir\Nioh3BossRematch.asi" -Force; T "installed BR $((Get-FileHash "$GameDir\Nioh3BossRematch.asi").Hash.Substring(0,8))" }
    if ($PerfAsi -ne "$GameDir\Nioh3EngineFix.asi") { Copy-Item $PerfAsi "$GameDir\Nioh3EngineFix.asi" -Force; T "installed EF $((Get-FileHash "$GameDir\Nioh3EngineFix.asi").Hash.Substring(0,8))" }
    if ($TrAsi) { Copy-Item $TrAsi "$GameDir\Nioh3Training.asi" -Force; T "installed TR $((Get-FileHash "$GameDir\Nioh3Training.asi").Hash.Substring(0,8))" }
    $aText = [IO.File]::ReadAllText($aIni)
    foreach ($kv in @(@("Rematch", "Enable", "1"), @("Remote", "Pipe", "1"), @("Debug", "BossLog", "1"), @("Debug", "InvLog", "0"), @("Debug", "NativeCapture", "1"), @("Debug", "NativeCaptureRuns", "2"), @("Debug", "NativeCaptureItem", "0"))) { $aText = Set-IniKey $aText $kv[0] $kv[1] $kv[2] }
    [IO.File]::WriteAllText($aIni, $aText, (New-Object Text.UTF8Encoding($false)))
    [IO.File]::WriteAllText($perfIni, (Force-Vpad ([IO.File]::ReadAllText("$out\orig\Nioh3EngineFix.ini"))), (New-Object Text.UTF8Encoding($false)))
    $null = Launch
    Shot "s0-field"; Rr "state: $((VP state).reply)"
    # interactive loop: the agent appends VP commands to cmd.txt (one per line; 'shot NAME', 'quit' ends); results in cmdlog.txt
    $cf = "$out\cmd.txt"; New-Item -ItemType File -Force $cf | Out-Null; $done = 0; $idle = Get-Date
    while (((Get-Date) - $idle).TotalMinutes -lt 25) {
        Assert-Owner
        $lines = @(Get-Content $cf); $quit = $false
        for (; $done -lt $lines.Count; $done++) {
            $l = $lines[$done].Trim(); if (-not $l) { continue }; $idle = Get-Date
            if ($l -eq 'quit') { $quit = $true; break }
            if ($l -like 'shot *') { Shot $l.Substring(5).Trim(); Add-Content "$out\cmdlog.txt" "$l -> ok"; continue }
            if ($l -like 'rm *') { Add-Content "$out\cmdlog.txt" "$l -> $(RMP $l.Substring(3).Trim())"; continue }
            $x = VP @($l -split " "); Add-Content "$out\cmdlog.txt" "$l -> $($x.reply)"
        }
        if ($quit) { break }
        if (-not (Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue)) { T "game gone"; break }
        Start-Sleep 1
    }
    Copy-Item "$GameDir\Nioh3Training.log" "$out\training_final.log" -Force -ErrorAction SilentlyContinue
    Copy-Item "$GameDir\Nioh3EngineFix.log" "$out\ef_final.log" -Force -ErrorAction SilentlyContinue
    Copy-Item $aLog "$out\addon_final.log" -Force -ErrorAction SilentlyContinue
} catch { T "ERROR: $($_.Exception.Message)"; Add-Content $res "ERROR: $($_.Exception.Message)" }
finally { Close-Game; Restore; T "done"; Write-Host "OUT=$out" }
