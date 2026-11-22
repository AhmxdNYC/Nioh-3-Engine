param([string]$Rows = "R1CBC", [int]$HumanPid = 0)
# arena_walk_run.ps1 (human #129550: keep this): live runner for arena_walk_test.py, first PASS R1CBC #129539. Usage: -Rows R1CBC,R15FF [-HumanPid N only when the human said go]. Closes that game, lock, save backup, VirtualPad on,
# launch, title->Continue, walk test, close, restore ini + saves, release lock, relaunch for the human (VirtualPad off).
$ErrorActionPreference = "Continue"
$wt = (Resolve-Path "$PSScriptRoot\..\..").Path; $repo = $wt; $tm = "$repo\tools\testmatrix"
$GD = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GD\Nioh3EngineFix.ini"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"; $stamp = Get-Date -f yyyyMMdd-HHmmss
$out = "$repo\captures\rematch\arenawalk-run-$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$saveBak = "$repo\captures\save-backups\$stamp-arenawalk"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText("$out\trace.txt", $l + "`r`n"); Write-Host $l }
$env:HG_MIN_HEADROOM_GB = "8"
. "$tm\harness_guard.ps1"; . "$tm\vpad.ps1"
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$script:locked=$false; $script:saved=$false; $script:iniSaved=$false; $script:brSaved=$false; $script:gp=0
function Close-Game { $p = if ($script:gp) { Get-Process -Id $script:gp -EA SilentlyContinue } else { $null }
  if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }; Start-Sleep 8 }; $script:gp = 0 }
function Restore {
  foreach ($f in "Nioh3Training.log","Nioh3BossRematch.log","Nioh3EngineFix.log") { if (Test-Path "$GD\$f") { Copy-Item "$GD\$f" "$out\" -Force } }
  if ($script:iniSaved) { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force; $script:iniSaved=$false; T "ini restored $((Get-FileHash $ini).Hash.Substring(0,8))" }
  if ($script:brSaved -and -not (Get-Process Nioh3 -EA SilentlyContinue)) { Copy-Item "$out\orig\Nioh3BossRematch.asi" "$GD\Nioh3BossRematch.asi" -Force; Copy-Item "$out\orig\Nioh3BossRematch.ini" "$GD\Nioh3BossRematch.ini" -Force; $script:brSaved=$false; T "BR restored $((Get-FileHash "$GD\Nioh3BossRematch.asi").Hash.Substring(0,8))" }
  if ($script:saved -and -not (Get-Process Nioh3 -EA SilentlyContinue)) { robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "save restored robocopy $LASTEXITCODE"; $script:saved=$false }
  if ($script:locked) { $r = Lock "release"; $script:locked=$false; T "lock released: $($r.out)" }
}
function Launch { Start-Process powershell -WindowStyle Hidden -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-File","`"C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1`"","-Minutes","15"
  Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
  while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -EA SilentlyContinue | ? MainWindowHandle -ne 0 | Select -First 1; if ($p) { break }; Start-Sleep 3 }
  return $p }
Init-Guard -Name "arenawalk" -MaxMinutes 14 -Cleanup { Close-Game; Restore }
try {
  $h = Get-Process -Id $HumanPid -EA SilentlyContinue
  if ($h) { T "closing human game pid $HumanPid (-HumanPid: only when the human said go)"; $null = $h.CloseMainWindow(); $h.WaitForExit(45000) | Out-Null; if (-not $h.HasExited) { Stop-Process -Id $HumanPid -Force }; Start-Sleep 8 }
  if (Get-Process Nioh3 -EA SilentlyContinue) { throw "Nioh3 still running" }
  $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked=$true; T "lock: $($r.out)"
  New-Item -ItemType Directory -Force $saveBak | Out-Null; robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saved=$true; T "saves backed up $saveBak"
  Copy-Item $ini "$out\orig\" -Force; $script:iniSaved=$true
  Copy-Item "$GD\Nioh3BossRematch.asi" "$out\orig\" -Force; Copy-Item "$GD\Nioh3BossRematch.ini" "$out\orig\" -Force; $script:brSaved=$true
  $bi = [IO.File]::ReadAllText("$GD\Nioh3BossRematch.ini") -replace '(?m)^Pipe = 0', 'Pipe = 1'; [IO.File]::WriteAllText("$GD\Nioh3BossRematch.ini", $bi, (New-Object Text.UTF8Encoding($false)))
  T "BR ini: $((Select-String -Path "$GD\Nioh3BossRematch.ini" -Pattern '^Pipe *=' | % { $_.Line.Trim() }) -join ',')"
  [IO.File]::WriteAllText($ini, (Force-Vpad ([IO.File]::ReadAllText($ini))), (New-Object Text.UTF8Encoding($false)))
  foreach ($f in "Nioh3Training.asi","Nioh3BossRematch.asi","Nioh3EngineFix.asi","Nioh3Training_arenas.tsv","Nioh3BossRematch_bosses.tsv") { T "$f $((Get-FileHash "$GD\$f").Hash.Substring(0,8))" }
  Assert-Launch
  $p = Launch; if (-not $p) { throw "no window" }
  $script:gp = $p.Id; Register-Game; T "game pid $($p.Id)"
  if (-not (VP-WaitPipe 180)) { throw "no vpad pipe" }
  $ok = VP-TitleContinue 300 "$out\title.txt"; T "title->field $ok"; if (-not $ok) { throw "title-continue failed" }
  Start-Sleep 15
  Push-Location $wt
  $o = (py -I tools\rematch\arena_walk_test.py --i-own-the-game --owner-pid $PID --rows $Rows --walk-sec 30 --repeat 1 2>&1 | Out-String)
  Pop-Location
  [IO.File]::WriteAllText("$out\walk.txt", $o); T "WALK:`r`n$o"
  Get-ChildItem "$wt\captures\rematch\arenawalk-*.csv" -EA SilentlyContinue | % { Copy-Item $_.FullName "$out\" -Force }
} catch { T "ERROR: $_" } finally {
  Close-Game; Restore
  # relaunch for the human, VirtualPad off (ini restored)
  if (-not (Get-Process Nioh3 -EA SilentlyContinue)) { $p = Launch; if ($p) { T "relaunched for human pid $($p.Id) vpad=$((Select-String -Path $ini -Pattern '^VirtualPad *=' | % { $_.Line.Trim() }) -join ',')" } else { T "relaunch: no window" } }
  T "done"
}
