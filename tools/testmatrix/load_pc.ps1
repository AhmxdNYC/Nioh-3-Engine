# load_pc.ps1 - PC load-test helper (guarded; see harness_guard.ps1). One action per call so a supervising agent can look at screenshots between steps.
#   -Action Launch -EP 0|1     write the load-test ini (LoadStats=3 LoadGates=1 ReadProbe=1 MemStats=1 SP=1 ReportSec=5 AutoContinueLoad=1, EP=n),
#                              launch via Steam, click past the autosave notice, leave the game at the title screen. Pid file kept.
#   -Action Shot -Out f.png    screenshot
#   -Action Click -X n -Y n | Key -Keys 20,1B | Focus | Close (clean close, notes the close so it is not counted as a crash)
param([Parameter(Mandatory)][string]$Action, [int]$EP = 0, [string]$Out = "", [int]$X = 0, [int]$Y = 0, [string[]]$Keys = @(), [int]$Wait = 0,
      [int]$OwnerPid = 0, [int]$MaxMinutes = 90)
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "C:\Users\Ahmad Hamza\nioh3-engine\patcher\common.ps1"
. "$here\harness_guard.ps1"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GameDir\Nioh3EngineFix.ini"
Init-Guard -Name "load_$Action" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes
function UG { Assert-Owner; $o = dotnet fsi $ui @args 2>&1 | Out-String
    if ($LASTEXITCODE -eq 3 -and (Try-Refocus $ui)) { $o = dotnet fsi $ui @args 2>&1 | Out-String }
    if ($LASTEXITCODE -eq 3) { Guard-Fail "input refused: $($o.Trim())" }
    Mark-Input; $o }
function Set-IniKey([string[]]$lines, [string]$sec, [string]$key, [string]$val) {
    $out = New-Object System.Collections.Generic.List[string]; $cur = ""; $done = $false; $secSeen = $false; $insAt = -1
    foreach ($l in $lines) {
        if ($l -match '^\s*\[(.+?)\]') { if ($cur -eq $sec -and -not $done -and $insAt -lt 0) { $insAt = $out.Count }; $cur = $Matches[1]; if ($cur -eq $sec) { $secSeen = $true } }
        if ($cur -eq $sec -and -not $done -and $l -match ('^\s*' + [regex]::Escape($key) + '\s*=')) { $out.Add("$key = $val"); $done = $true; continue }
        $out.Add($l)
    }
    if (-not $done) { if ($secSeen) { if ($insAt -lt 0) { $insAt = $out.Count }; $out.Insert($insAt, "$key = $val") } else { $out.Add(""); $out.Add("[$sec]"); $out.Add("$key = $val") } }
    return $out.ToArray()
}
switch ($Action) {
    "Launch" {
        $l = [IO.File]::ReadAllLines("$GameDir\Nioh3EngineFix.ini.preboss.bak")
        foreach ($t in @(@("Profiler", "LoadStats", "3"), @("Profiler", "LoadGates", "1"), @("Profiler", "ReadProbe", "1"), @("Profiler", "MemStats", "1"), @("Profiler", "SP", "1"),
                         @("Profiler", "ReportSec", "5"), @("General", "AutoContinueLoad", "1"), @("Patches", "EP", "$EP"))) { $l = Set-IniKey $l $t[0] $t[1] $t[2] }
        [IO.File]::WriteAllLines($ini, $l, (New-Object Text.UTF8Encoding($false)))
        Close-Game
        Assert-Launch
        $t0 = Get-Date
        Start-Process "steam://rungameid/3681010"
        while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0) -and ((Get-Date) - $t0).TotalSeconds -lt 180) { Start-Sleep 3 }
        Register-Game; if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { Note-Crash "no game window after launch"; Guard-Fail "no game window" }
        Start-Sleep 25
        UG focus Nioh3 | Out-Null; UG click 1280 887 | Out-Null; Start-Sleep 22
        "launched EP=$EP; boot $([int]((Get-Date)-$t0).TotalSeconds)s"
        Done-Guard -KeepFile
    }
    "Load" {   # title screen -> press -> LOAD GAME -> OK -> Yes; waits for the load to finish, saves the log segment to -Out
        $logf = "$GameDir\Nioh3EngineFix.log"; New-Item -ItemType Directory -Force (Split-Path -Parent $Out) | Out-Null
        UG focus Nioh3 | Out-Null; UG click 1280 1170 | Out-Null; Start-Sleep 6
        UG click 380 1119 | Out-Null; Start-Sleep 5
        UG key 20 | Out-Null; Start-Sleep 3
        $off = (Get-Item $logf).Length; $t0 = Get-Date
        UG key 20 | Out-Null
        $tx = ""; $ok = $false
        while (((Get-Date) - $t0).TotalSeconds -lt 240) {
            Start-Sleep 5
            $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); $null = $fs.Seek($off, "Begin"); $tx = (New-Object IO.StreamReader($fs)).ReadToEnd(); $fs.Dispose()
            if ($tx -match 'LoadGates #\d+ holder') { $ok = $true; break }
        }
        $wall = [int]((Get-Date) - $t0).TotalSeconds
        Start-Sleep 20
        $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); $null = $fs.Seek($off, "Begin"); $tx = (New-Object IO.StreamReader($fs)).ReadToEnd(); $fs.Dispose()
        Set-Content $Out $tx -Encoding UTF8
        "load finished=$ok wall(Yes->holder line)=${wall}s saved $Out"
        Done-Guard -KeepFile
    }    "Shot" { UG shot $Out 0 0 2560 1440 | Out-Null; "saved $Out"; Done-Guard }
    "Click" { UG focus Nioh3 | Out-Null; UG click $X $Y | Out-Null; if ($Wait) { Start-Sleep $Wait }; "clicked $X $Y"; Done-Guard }
    "Key" { UG focus Nioh3 | Out-Null; UG key @Keys | Out-Null; if ($Wait) { Start-Sleep $Wait }; "keys $Keys"; Done-Guard }
    "Hold" { UG focus Nioh3 | Out-Null; UG hold $Keys[0] $Wait | Out-Null; "held $($Keys[0]) $Wait ms"; Done-Guard }
    "Focus" { UG focus Nioh3; Done-Guard }
    "Close" { Note-Closed; $p = Get-Process Nioh3 -ErrorAction SilentlyContinue; foreach ($q in $p) { $null = $q.CloseMainWindow(); $q.WaitForExit(25000) | Out-Null; if (-not $q.HasExited) { Stop-Process -Id $q.Id -Force } }; "closed"; Done-Guard }
}
