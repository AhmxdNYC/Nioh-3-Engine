# ONE COMMAND: time every owned soul core in one launch on the regular player (human #134924).
# Installs the given Boss Rematch ASI only (Training untouched), backs up saves, holds the game lock + marker,
# runs survey_cores.py, writes OUT\table.md, restores ASI/ini/saves and releases the lock.
# Usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\soulcore\run_survey.ps1 -Asi PATH\Nioh3BossRematch.asi -Seat pc/3 [-Out DIR]
# The ASI must have the "spirit scequip/scstate/gauge" pipe (pc2/sc-survey-134412 e93268d3 or later).
param(
  [Parameter(Mandatory = $true)][string]$Asi,
  [Parameter(Mandatory = $true)][string]$Seat,
  [string]$Out = "",
  [int]$MaxMinutes = 70
)
$ErrorActionPreference = "Continue"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not (Test-Path $Asi)) { throw "no ASI at $Asi" }
$Asi = (Resolve-Path $Asi).Path
if (-not $Out) { $Out = "D:\nioh3-wt\sc-survey-" + (Get-Date -f "MMdd-HHmm") }
$out = $Out
New-Item -ItemType Directory -Force $out | Out-Null
$trace = Join-Path $out "launch.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f "HH:mm:ss"), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
$tm = "C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix"
. "$tm\harness_guard.ps1"
$saveDir = Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3"
$saveBak = Join-Path $out "savebak"
$marker = "C:\Users\Ahmad Hamza\AppData\Local\uo-agentd\pc\test-active"
$script:gamePid = 0
$script:gotLock = $false
$script:installed = $false
$script:saved = $false
function Restore-All {
  if ($script:gamePid) {
    Note-Closed
    $p = Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue
    if ($p) { T "close $($p.Id)"; Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; $p.WaitForExit(25000) | Out-Null }
    Start-Sleep 5
    $script:gamePid = 0
  }
  if ($script:installed) {
    Copy-Item (Join-Path $out "Nioh3BossRematch.asi.bak") (Join-Path $G "Nioh3BossRematch.asi") -Force
    Copy-Item (Join-Path $out "Nioh3EngineFix.ini.bak") (Join-Path $G "Nioh3EngineFix.ini") -Force
    T "asi and ini restored"
    $script:installed = $false
  }
  if ($script:saved) {
    robocopy $saveBak $saveDir /MIR /R:1 /W:1 /NFL /NDL /NP /NJH | Out-Null
    T "saves restored robocopy $LASTEXITCODE"
    $script:saved = $false
  }
  if (Test-Path $marker) {
    $cur = Get-Content $marker -Raw -ErrorAction SilentlyContinue
    if ($cur -match [regex]::Escape($Seat)) { Remove-Item $marker -Force -ErrorAction SilentlyContinue; T "marker cleared" }
  }
  if ($script:gotLock) { py -m uo_bridge.gamelock release nioh3 --device pc | Out-Host; $script:gotLock = $false; T "lock released" }
}
Init-Guard -Name "scsurvey-onecmd" -OwnerPid $PID -MaxMinutes $MaxMinutes -MinHeadroomGB 8 -Cleanup { Restore-All }
try {
  if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 already running" }
  $acq = py -m uo_bridge.gamelock acquire nioh3 --device pc --ttl 5400 2>&1 | Out-String
  T "lock: $acq"
  if ($LASTEXITCODE -ne 0) {
    $st = py -m uo_bridge.gamelock status nioh3 2>&1 | Out-String
    T "lock status: $st"
    if ($st -notmatch "'holder': 'pc'") { throw "lock not acquired" }
    T "lock already held by this pc"
  }
  $script:gotLock = $true
  New-Item -ItemType Directory -Force (Split-Path $marker) | Out-Null
  [IO.File]::WriteAllText($marker, "$Seat soul-core-survey`r`n")
  Copy-Item (Join-Path $G "Nioh3BossRematch.asi") (Join-Path $out "Nioh3BossRematch.asi.bak") -Force
  Copy-Item (Join-Path $G "Nioh3EngineFix.ini") (Join-Path $out "Nioh3EngineFix.ini.bak") -Force
  robocopy $saveDir $saveBak /MIR /R:1 /W:1 /NFL /NDL /NP /NJH | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "save backup failed $LASTEXITCODE" }
  $script:saved = $true
  T "saves backed up"
  Copy-Item $Asi (Join-Path $G "Nioh3BossRematch.asi") -Force
  $ini = [IO.File]::ReadAllText((Join-Path $G "Nioh3EngineFix.ini"))
  $ini2 = [regex]::Replace($ini, '(?s)(\[Debug\].*?VirtualPad\s*=\s*)0', '${1}1', 1)
  if ($ini2 -eq $ini) { T "WARN VirtualPad under Debug was not 0" } else { T "Debug VirtualPad set to 1" }
  [IO.File]::WriteAllText((Join-Path $G "Nioh3EngineFix.ini"), $ini2)
  $script:installed = $true
  $hash = (Get-FileHash (Join-Path $G "Nioh3BossRematch.asi") -Algorithm SHA256).Hash.Substring(0, 8)
  T "installed BR $hash training left as-is"
  Assert-Launch
  Start-Process powershell -ArgumentList '-NoProfile -ExecutionPolicy Bypass -File "C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1" -Minutes 15' -WindowStyle Hidden
  Start-Process -FilePath "C:\Program Files (x86)\Steam\steam.exe" -ArgumentList "-applaunch","3681010"
  $t0 = Get-Date; $p = $null
  while (((Get-Date) - $t0).TotalSeconds -lt 180) {
    $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if ($p) { break }
    Start-Sleep 3
  }
  if (-not $p) { Note-Crash "no window"; throw "no game window" }
  $script:gamePid = $p.Id
  Register-Game
  T "pid $($p.Id)"
  $vp = "C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix\vpad.py"
  $null = py -I $vp --owner-pid $PID wait-pipe 180
  if ($LASTEXITCODE -ne 0) { throw "no vpad pipe" }
  $null = py -I $vp --owner-pid $PID title-continue 300
  if ($LASTEXITCODE -ne 0) { throw "not in the field" }
  T "in the field"
  Start-Sleep 6
  py -I (Join-Path $here "survey_cores.py") $out
  T "survey exit $LASTEXITCODE"
  $tbl = Join-Path $out "table.md"
  py -I (Join-Path $here "table.py") (Join-Path $out "cores.jsonl") | Out-File -Encoding utf8 $tbl
  T "table $tbl"
} catch {
  T "!! $($_.Exception.Message)"
} finally {
  Restore-All
  Done-Guard
  T "launch finished"
}
