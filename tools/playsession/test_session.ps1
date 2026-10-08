<#
  test_session.ps1 - dry run of the whole kit WITHOUT the game (fake game/save folders in %TEMP%; the real game folder, saves and
  lock are never changed). Checks: lock refusal (reads the real bridge status: refuses if another device holds it), START (save
  backup, ini = user ini + capture keys, ASI from bin\, PresentMon + monitor running), double START refused, a fake "Nioh3.exe"
  (a copy of PING.EXE) seen by the monitor + its mod log copied at exit, STOP refused while it runs, STOP restores byte-identical
  files and writes report.md, summarize.py --selftest. Ends with DRY RUN OK.
    powershell -NoProfile -ExecutionPolicy Bypass -File tools\playsession\test_session.ps1
#>
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$root = Join-Path $env:TEMP ("playsession_dry_" + (Get-Date -Format "HHmmss"))
$g = Join-Path $root "game"; $s = Join-Path $root "save"; $b = Join-Path $root "backups"; $c = Join-Path $root "captures"; $st = Join-Path $root "state"
New-Item -ItemType Directory -Force $g, $s, (Join-Path $s "Savedata\1\SAVEDATA00"), $b, $c, $st | Out-Null
$realGame = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$userIni = if (Test-Path "$realGame\Nioh3EngineFix.ini") { "$realGame\Nioh3EngineFix.ini" } else { "C:\Users\Ahmad Hamza\nioh3-engine\mod\Nioh3EngineFix\Nioh3EngineFix.ini" }
Copy-Item $userIni "$g\Nioh3EngineFix.ini"
[IO.File]::WriteAllBytes("$g\Nioh3EngineFix.asi", [Text.Encoding]::ASCII.GetBytes("MZ fake Nioh3EngineFix 1.7.0 (built test)"))
[IO.File]::WriteAllBytes("$s\Savedata\1\SAVEDATA00\SAVEDATA.BIN", (1..4096 | ForEach-Object { [byte]($_ % 251) }))
if (Test-Path "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml") { Copy-Item "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml" "$s\config.xml" }
$h0 = @{ ini = (Get-FileHash "$g\Nioh3EngineFix.ini").Hash; asi = (Get-FileHash "$g\Nioh3EngineFix.asi").Hash }
$fails = @()
function Ck([bool]$cond, [string]$msg) { if ($cond) { Write-Host "ok   $msg" -ForegroundColor Green } else { Write-Host "FAIL $msg" -ForegroundColor Red; $script:fails += $msg } }
$common = @("-NoPause", "-GameDir", $g, "-SaveRoot", $s, "-BackupRoot", $b, "-CaptureRoot", $c, "-StateDir", $st)
function Run([string]$script, [string[]]$extra) {
    $o = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $here $script) @extra 2>&1 | Out-String
    return @{ code = $LASTEXITCODE; out = $o }
}

# 1. lock refusal against the real bridge (status only; acquire is never reached when another device holds it)
. (Join-Path $here "session_lib.ps1")
$hl = Get-LockHolder
if ($hl.ok -and $hl.holder -and $hl.holder -ne "pc") {
    $r = Run "start_session.ps1" $common
    Ck ($r.code -eq 1 -and $r.out -match "in use on '$($hl.holder)'") "START refused while '$($hl.holder)' holds the game lock"
    Ck ((Get-FileHash "$g\Nioh3EngineFix.ini").Hash -eq $h0.ini -and -not (Test-Path (Join-Path $st "active.json"))) "refusal changed nothing"
} else { Write-Host "skip lock-refusal test (lock holder: '$($hl.holder)', bridge ok=$($hl.ok))" -ForegroundColor Yellow }

# 2. START (no lock)
$r = Run "start_session.ps1" ($common + "-NoLock")
Ck ($r.code -eq 0 -and $r.out -match "READY") "START ok"
if ($r.code -ne 0) { Write-Host $r.out }
$A = Get-Content (Join-Path $st "active.json") -Raw | ConvertFrom-Json
$sess = $A.session_dir
$ini = Get-Content "$g\Nioh3EngineFix.ini"
foreach ($k in $CaptureKeys) { Ck ([bool]($ini -match ("^\s*" + $k[1] + "\s*=\s*" + $k[2] + "\s*$"))) "ini has $($k[1]) = $($k[2])" }
$u = Get-Content $userIni
$acl = ($u | Where-Object { $_ -match '^\s*AutoContinueLoad\s*=' }) -join ""
Ck ((($ini | Where-Object { $_ -match '^\s*AutoContinueLoad\s*=' }) -join "") -eq $acl) "AutoContinueLoad kept as the user had it ($acl)"
$diff = Compare-Object $u $ini | Where-Object { $_.SideIndicator -eq "<=" -and $_.InputObject -notmatch ('^\s*(' + (($CaptureKeys | ForEach-Object { $_[1] }) -join "|") + ')\s*=') }
Ck (-not $diff) "every other line of the user's ini unchanged"
Ck ((Get-FileHash "$g\Nioh3EngineFix.asi").Hash -eq (Get-FileHash $BinAsi).Hash) "ASI from bin\ installed ($(Get-AsiVersion $BinAsi))"
Ck ((Get-ChildItem $A.save_backup -Recurse -File).Count -eq (Get-ChildItem $s -Recurse -File).Count) "save backup complete"
Ck ([bool](Get-Process -Id $A.pm_pid -ErrorAction SilentlyContinue)) "PresentMon running"
Ck ([bool](Get-Process -Id $A.mon_pid -ErrorAction SilentlyContinue)) "monitor running"

# 3. second START refused
$r = Run "start_session.ps1" ($common + "-NoLock")
Ck ($r.code -eq 1 -and $r.out -match "already running") "second START refused"

# 4. fake game run: PING.EXE copied as Nioh3.exe, a fake mod log in the fake game folder
Copy-Item "$env:WINDIR\System32\PING.EXE" (Join-Path $root "Nioh3.exe")
@("[20:00:00.000] Nioh3EngineFix 1.7.34 (built Oct  8 2026) - native port of the live-tested Nioh 3 engine patches",
  "[20:00:01.000] FnProbe: 14/14 functions hooked (default 10.1 list; measurement only; report every 30 s, args of the first 8 hits, callers 1 in 16 hits)") |
    Set-Content "$g\Nioh3EngineFix.log" -Encoding ASCII
$fake = Start-Process (Join-Path $root "Nioh3.exe") -ArgumentList "-n 14 127.0.0.1" -WindowStyle Hidden -PassThru
Start-Sleep -Seconds 7
$r = Run "stop_session.ps1" @("-NoPause", "-StateDir", $st)
Ck ($r.code -eq 1 -and $r.out -match "still running") "STOP refused while Nioh3.exe runs"
Ck ((Get-FileHash "$g\Nioh3EngineFix.asi").Hash -eq (Get-FileHash $BinAsi).Hash) "refused STOP changed nothing"
$fake.WaitForExit(30000) | Out-Null
Start-Sleep -Seconds 9
$evs = Get-Content (Join-Path $sess "events.txt") -ErrorAction SilentlyContinue
Ck ([bool]($evs -match "game start run 1") -and [bool]($evs -match "game exit run 1 code 0; mod log copied")) "monitor saw the run and copied the mod log"
$rows = @(Import-Csv (Join-Path $sess "sysmon.csv"))
Ck ($rows.Count -ge 3 -and @($rows | Where-Object { $_.game_running -eq "1" }).Count -ge 1 -and @($rows | Where-Object { $_.cpu_total_pct -ne "" }).Count -ge 3) "sysmon.csv rows ($($rows.Count), game_running seen)"
Ck (@($rows | Where-Object { $_.gpu3d_total_pct -ne "" -and $_.vram_adapter_mb -ne "" }).Count -ge 1) "GPU counters present"

# 5. STOP
$r = Run "stop_session.ps1" @("-NoPause", "-StateDir", $st)
Ck ($r.code -eq 0 -and $r.out -match "Done. Report") "STOP ok"
if ($r.code -ne 0) { Write-Host $r.out }
Ck ((Get-FileHash "$g\Nioh3EngineFix.ini").Hash -eq $h0.ini) "ini restored byte-identical"
Ck ((Get-FileHash "$g\Nioh3EngineFix.asi").Hash -eq $h0.asi) "ASI restored byte-identical"
Ck (-not (Test-Path (Join-Path $st "active.json"))) "session closed"
Ck (-not (Get-Process -Id $A.pm_pid -ErrorAction SilentlyContinue) -and -not (Get-Process -Id $A.mon_pid -ErrorAction SilentlyContinue)) "PresentMon + monitor stopped"
Ck (Test-Path (Join-Path $sess "report.md")) "report.md written"
Ck ((Get-Content (Join-Path $sess "report.md") -Raw) -match "FnProbe: 14/14") "report reads the copied mod log"

# 6. summarizer against fake capture files with every input
$o = & py -I (Join-Path $here "summarize.py") --selftest 2>&1 | Out-String
Ck ($o -match "SELFTEST OK") "summarize.py --selftest"

if ($fails.Count) { Write-Host "$($fails.Count) FAILED (kept $root)" -ForegroundColor Red; exit 1 }
Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue
Write-Host "DRY RUN OK" -ForegroundColor Green
