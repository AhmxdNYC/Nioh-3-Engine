# vpad_live.ps1 - live check of VirtualPad + KeepRunningUnfocused (ASI >= 1.7.37) with the game NOT in the foreground.
#   lock -> backup ini/asi (+ copy of the save folder) -> test ini (VirtualPad = 1, KeepRunningUnfocused = 1, AutoContinueLoad = 1)
#   -> launch, hand the foreground back -> title -> Continue -> field through the pipe -> toggle one menu item (Show frame time)
#   on and back -> stick forward 1.5 s (player position must change) -> close normally -> restore -> release the lock.
#   No SendInput anywhere. Guard: harness_guard.ps1 (owner pid, MaxMinutes, commit headroom, crash limit, pid files).
#   powershell -File vpad_live.ps1 -OwnerPid <agent pid> [-AsiPath <built asi>]
param([int]$OwnerPid = 0, [int]$MaxMinutes = 25, [string]$AsiPath = "")
$ErrorActionPreference = "Continue"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$here\harness_guard.ps1"
. "$here\vpad.ps1"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GameDir\Nioh3EngineFix.ini"; $asi = "$GameDir\Nioh3EngineFix.asi"; $logf = "$GameDir\Nioh3EngineFix.log"
if (-not $AsiPath) { $AsiPath = Join-Path $here "..\..\mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi" }
$stamp = Get-Date -f yyyyMMdd_HHmmss
$out = "C:\Users\Ahmad Hamza\nioh3-engine\captures\vpad\$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$trace = "$out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
function Log-From([long]$off) { $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); try { $null = $fs.Seek($off, "Begin"); (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() } }
$script:gamePid = 0; $script:locked = $false; $script:installed = $false
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(30000) | Out-Null; if (-not $p.HasExited) { T "!! game did not exit in 30 s - killing"; Stop-Process -Id $p.Id -Force }; Start-Sleep 5 }
    $script:gamePid = 0
}
function Restore {
    if ($script:installed) {
        if (Test-Path "$out\orig\Nioh3EngineFix.ini") { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force }
        if (Test-Path "$out\orig\Nioh3EngineFix.asi") { Copy-Item "$out\orig\Nioh3EngineFix.asi" $asi -Force }
        $script:installed = $false; T "ini + asi restored"
    }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}
Init-Guard -Name "vpad_live" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Restore; Stop-TestPopup }
Start-TestPopup -Name "VirtualPad live check" -InputMode vpad
$result = [ordered]@{}
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running" }
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    Copy-Item $ini "$out\orig\" -Force; Copy-Item $asi "$out\orig\" -Force
    $sb = "D:\nioh3-re\save-backups\$stamp-vpad"; New-Item -ItemType Directory -Force $sb | Out-Null
    Copy-Item "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\*" $sb -Recurse -Force; T "save folder copied to $sb"
    $t = [IO.File]::ReadAllText($ini); $t = Force-Acl (Force-Vpad $t)
    $script:installed = $true
    [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false))); Copy-Item $AsiPath $asi -Force
    T "installed $AsiPath with VirtualPad = 1, KeepRunningUnfocused = 1, AutoContinueLoad = 1"
    # ---- launch ----
    Assert-Launch
    $before = VP-ForegroundInfo; T "foreground before launch: $($before.name)"
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }; Start-Sleep 3 }
    if (-not $p) { Note-Crash "no window"; throw "no game window after 180 s" }
    $script:gamePid = $p.Id; Register-Game; T "game pid $($p.Id)"
    Start-Sleep 3; VP-GiveBackFocus $before; Start-Sleep 1
    $fg = VP-ForegroundInfo; T "foreground after give-back: $($fg.name)"; $result.fg_after_launch = $fg.name
    if (-not (VP-WaitPipe 180)) { throw "no pipe" }
    T "ping: $((VP ping).reply)"
    # ---- title -> Continue -> field ----
    $tc0 = Get-Date
    $ok = VP-TitleContinue 300 "$out\title_continue.txt"
    $result.title_continue = $ok; $result.title_sec = [int]((Get-Date) - $tc0).TotalSeconds
    $fg = VP-ForegroundInfo; $result.fg_in_field = $fg.name
    T "title-continue: $ok in $($result.title_sec) s, foreground now $($fg.name)"
    T "state: $(VP-State)"
    if (-not $ok) { throw "not in the field" }
    Start-Sleep 5
    # ---- menu item via the pipe ----
    $off = (Get-Item $logf).Length
    $g = VP menu get FrameTime; T "menu get FrameTime: $($g.reply)"
    $s1 = VP menu set FrameTime 1; T "menu set FrameTime 1: $($s1.reply)"
    Start-Sleep 2
    $s0 = VP menu set FrameTime 0; T "menu set FrameTime 0: $($s0.reply)"
    Start-Sleep 1
    $tx = Log-From $off
    $result.menu_on_line = [bool]($tx -match "MENU: Show frame time -> ON"); $result.menu_off_line = [bool]($tx -match "MENU: Show frame time -> OFF")
    T "mod log: 'Show frame time -> ON' $($result.menu_on_line), '-> OFF' $($result.menu_off_line)"
    # ---- stick ----
    $a = VP-State; T "before stick: $a"
    $st = VP stick L 0 1 1500; T "stick: $($st.reply)"
    Start-Sleep 1
    $b = VP-State; T "after stick: $b"
    $pa = if ($a -match 'pos=([-\d.]+),([-\d.]+),([-\d.]+)') { @([double]$Matches[1], [double]$Matches[2], [double]$Matches[3]) } else { $null }
    $pb = if ($b -match 'pos=([-\d.]+),([-\d.]+),([-\d.]+)') { @([double]$Matches[1], [double]$Matches[2], [double]$Matches[3]) } else { $null }
    if ($pa -and $pb) { $d = [Math]::Sqrt([Math]::Pow($pb[0] - $pa[0], 2) + [Math]::Pow($pb[2] - $pa[2], 2)); $result.moved = [Math]::Round($d, 2); T "player moved $([Math]::Round($d,2)) units (x/z)" }
    $result.fg_end = (VP-ForegroundInfo).name
    T "state end: $(VP-State)"
} catch {
    T "!! $($_.Exception.Message)"; $result.error = $_.Exception.Message
} finally {
    Close-Game
    Stop-TestPopup
    $tx = Log-From 0
    $keep = ($tx -split "`n" | Where-Object { $_ -match 'VPad|KeepRunningUnfocused|MENU: (Show frame time|hooked|ready)|Nioh3EngineFix 1\.' }) -join "`n"
    Set-Content "$out\modlog_vpad.txt" $keep
    Copy-Item $logf "$out\Nioh3EngineFix.log" -Force -ErrorAction SilentlyContinue
    Restore
    Done-Guard
    $result | ConvertTo-Json | Set-Content "$out\result.json"
    T "result: $($result | ConvertTo-Json -Compress)"
    T "out: $out"
}
