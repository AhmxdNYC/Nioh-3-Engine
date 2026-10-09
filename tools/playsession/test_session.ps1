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
$common = @("-NoPause", "-NoPopup", "-GameDir", $g, "-SaveRoot", $s, "-BackupRoot", $b, "-CaptureRoot", $c, "-StateDir", $st)
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
@("[20:00:00.000] Nioh3EngineFix 1.7.35 (built Oct  8 2026) - native port of the live-tested Nioh 3 engine patches",
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

# 5b. STOCK mode on a fake game folder (stash / restore round trip, hashes, leftover-stash refusal, crash-safe STOP, newer-file refusal, -KeepOptiScaler)
$g2 = Join-Path $root "game2"; $st2 = Join-Path $root "state2"; $sh2 = Join-Path $root "stash"; $os2 = Join-Path $root "optisrc"; $c2 = Join-Path $root "captures2"
New-Item -ItemType Directory -Force $g2, $st2, $sh2, $os2, $c2, "$g2\Nioh3EngineFix_shaders\sub" | Out-Null
$StashRoot = $sh2      # the dot-sourced lib uses this for Get-LeftoverStash
$rb = New-Object Random 7
function Put([string]$path, [int]$n) { $by = New-Object byte[] $n; $rb.NextBytes($by); [IO.File]::WriteAllBytes($path, $by); (Get-Item $path).LastWriteTimeUtc = [datetime]"2026-01-01T10:00:00Z" }
$modNames = @("dinput8.dll", "Nioh3EngineFix.asi", "Nioh3EngineFix.ini", "Nioh3EngineFix.log", "Nioh3EngineFix.asi.1.7.25.bak", "Nioh3EngineFix.ini.preboss.bak", "Nioh3EngineFix_shaders\a.bin", "Nioh3EngineFix_shaders\sub\b.bin")
$optNames = @("dxgi.dll", "OptiScaler.ini", "amd_fidelityfx_dx12.dll", "amd_fidelityfx_upscaler_dx12.dll", "amd_fidelityfx_framegeneration_dx12.dll")
$gameNames = @("Nioh3.exe", "amd_ags_x64.dll", "libxess.dll", "data.pak")      # Steam-shipped files: must never move
foreach ($n in ($modNames + $optNames + $gameNames)) { Put (Join-Path $g2 $n) (2000 + $rb.Next(5000)) }
# OptiScaler reference copies (optiscaler\files layout) so the stash can verify dxgi.dll / amd dlls are really the OptiScaler set
Copy-Item "$g2\dxgi.dll" "$os2\OptiScaler.dll"; foreach ($n in $optNames[2..4]) { Copy-Item "$g2\$n" "$os2\$n" }
function Snap([string]$dir) { $h = @{}; foreach ($rel in Get-RelFiles $dir "") { $h[$rel] = (Get-FileHash -LiteralPath ([IO.Path]::Combine($dir, $rel))).Hash }; return $h }
function SameSnap($a, $b) { if ($a.Count -ne $b.Count) { return $false }; foreach ($k in $a.Keys) { if ($a[$k] -ne $b[$k]) { return $false } }; return $true }
$snap0 = Snap $g2
$mt0 = (Get-Item "$g2\Nioh3EngineFix.asi").LastWriteTimeUtc
$sc = @("-NoPause", "-NoPopup", "-NoLock", "-GameDir", $g2, "-SaveRoot", $s, "-BackupRoot", $b, "-CaptureRoot", $c2, "-StateDir", $st2, "-StashRoot", $sh2, "-OptiSrc", $os2)
$sp = @("-NoPause", "-StateDir", $st2, "-StashRoot", $sh2, "-OptiSrc", $os2)
function Gone([string[]]$names) { foreach ($n in $names) { if (Test-Path -LiteralPath "$g2\$n") { return $false } }; return $true }
function Kill-Capture($act) { foreach ($id in @($act.pm_pid, $act.mon_pid)) { Stop-Process -Id $id -Force -ErrorAction SilentlyContinue }; Start-Process $PmExe -ArgumentList "--terminate_existing_session --session_name $PmSession" -Wait -WindowStyle Hidden }

# (a) START -Stock
$r = Run "start_session.ps1" ($sc + "-Stock")
Ck ($r.code -eq 0 -and $r.out -match "READY \(STOCK") "STOCK START ok"
if ($r.code -ne 0) { Write-Host $r.out }
$A2 = Get-Content (Join-Path $st2 "active.json") -Raw | ConvertFrom-Json
Ck ($A2.mode -eq "stock" -and -not $A2.keep_optiscaler) "session state records mode=stock"
Ck ((Gone ($modNames + $optNames)) -and -not (Get-ChildItem $g2 -Filter "*.asi" -File) -and -not (Test-Path "$g2\Nioh3EngineFix_shaders")) "game folder has no mod / OptiScaler file, no *.asi, no dinput8.dll"
Ck (@($gameNames | Where-Object { (Get-FileHash "$g2\$_").Hash -ne $snap0[$_] }).Count -eq 0 -and (Get-ChildItem $g2 -Recurse -File).Count -eq $gameNames.Count) "game files untouched, nothing else moved"
$man = Get-Content (Join-Path $A2.stash_dir "manifest.json") -Raw | ConvertFrom-Json
Ck ($man.files.Count -eq ($modNames.Count + $optNames.Count) -and -not @($man.files | Where-Object { -not $_.sha256 -or $_.size -le 0 -or $snap0[$_.name] -ne $_.sha256 }).Count -and $man.state -eq "stashed") "manifest lists name/size/sha256 for all $($man.files.Count) files"
Ck ((Test-Path (Join-Path $A2.session_dir "stash_manifest.json")) -and [bool](Get-Process -Id $A2.pm_pid -ErrorAction SilentlyContinue)) "manifest copied to session; PresentMon running"
# (b) second START refused; (c) crash: kill capture, drop the state file -> START refuses on the leftover stash, STOP (no state) restores
$r = Run "start_session.ps1" ($sc + "-Stock")
Ck ($r.code -eq 1 -and $r.out -match "already running") "STOCK second START refused"
Kill-Capture $A2; Remove-Item (Join-Path $st2 "active.json") -Force
$r = Run "start_session.ps1" ($sc + "-Stock")
Ck ($r.code -eq 1 -and $r.out -match "unfinished STOCK session") "START refuses while an unfinished stock stash exists (after a crash)"
$r = Run "start_session.ps1" $sc
Ck ($r.code -eq 1 -and $r.out -match "unfinished STOCK session") "normal START refuses too"
$r = Run "stop_session.ps1" $sp
Ck ($r.code -eq 0 -and $r.out -match "STOCK stash restored: $($modNames.Count + $optNames.Count) file") "STOP after a crash (no state) restores the stash"
Ck (SameSnap $snap0 (Snap $g2)) "all files back byte-identical (sha256), nothing extra"
Ck ((Get-Item "$g2\Nioh3EngineFix.asi").LastWriteTimeUtc -eq $mt0) "original timestamps restored"
Ck ((Test-Path (Join-Path $A2.stash_dir "restored.flag")) -and -not (Test-Path (Join-Path $A2.stash_dir "files"))) "stash marked restored"
# (d) normal stock cycle with a report; the stock report says there is no mod log
$r = Run "start_session.ps1" ($sc + "-Stock"); Ck ($r.code -eq 0) "STOCK START again after restore"
$A3 = Get-Content (Join-Path $st2 "active.json") -Raw | ConvertFrom-Json
Start-Sleep -Seconds 3
$r = Run "stop_session.ps1" $sp
Ck ($r.code -eq 0 -and $r.out -match "Done. Report" -and $r.out -match "STOCK stash restored") "STOCK STOP ok"
Ck (SameSnap $snap0 (Snap $g2)) "round trip: game folder identical to before START"
$rep = Get-Content (Join-Path $A3.session_dir "report.md") -Raw
Ck ($rep -match "STOCK session" -and $rep -notmatch "## FnProbe" -and $rep -notmatch "mod log \(modlog") "stock report: PresentMon only, says so"
# (e) refuse to overwrite a newer, different file
$r = Run "start_session.ps1" ($sc + "-Stock"); $A4 = Get-Content (Join-Path $st2 "active.json") -Raw | ConvertFrom-Json
[IO.File]::WriteAllBytes("$g2\dinput8.dll", [byte[]](1..50))      # a different, newer dinput8.dll appears while stashed
$r = Run "stop_session.ps1" $sp
Ck ($r.out -match "NEWER file" -and $r.out -match "Stash kept") "STOP reports a newer different file instead of overwriting"
Ck ((Get-Item "$g2\dinput8.dll").Length -eq 50 -and (Test-Path "$($A4.stash_dir)\files\dinput8.dll") -and -not (Test-Path (Join-Path $A4.stash_dir "restored.flag"))) "newer file kept, stashed copy kept, not marked restored"
Ck ((Get-FileHash "$g2\Nioh3EngineFix.asi").Hash -eq $snap0["Nioh3EngineFix.asi"]) "other files were still restored"
$r = Run "start_session.ps1" ($sc + "-Stock"); Ck ($r.code -eq 1 -and $r.out -match "unfinished STOCK") "START still refuses (stash not fully restored)"
Remove-Item "$g2\dinput8.dll" -Force
$r = Run "stop_session.ps1" $sp
Ck ($r.code -eq 0 -and (SameSnap $snap0 (Snap $g2))) "after the newer file is gone, STOP completes the restore"
# (f) -KeepOptiScaler: only our mod + dinput8
$r = Run "start_session.ps1" ($sc + "-Stock" + "-KeepOptiScaler"); Ck ($r.code -eq 0) "STOCK -KeepOptiScaler START ok"
$A5 = Get-Content (Join-Path $st2 "active.json") -Raw | ConvertFrom-Json
Ck ((Gone $modNames) -and -not (@($optNames | Where-Object { -not (Test-Path "$g2\$_") }).Count) -and $A5.keep_optiscaler) "only mod + dinput8 stashed, OptiScaler files stay"
$r = Run "stop_session.ps1" $sp
Ck ($r.code -eq 0 -and (SameSnap $snap0 (Snap $g2))) "KeepOptiScaler round trip identical"
# (g) a dxgi.dll that is not the OptiScaler one makes START refuse before moving anything; a stray *.asi (would still load the mod) too
[IO.File]::WriteAllBytes("$g2\dxgi.dll", [byte[]](9..99)); (Get-Item "$g2\dxgi.dll").LastWriteTimeUtc = [datetime]"2026-01-01T10:00:00Z"
$snap1 = Snap $g2
$r = Run "start_session.ps1" ($sc + "-Stock")
Ck ($r.code -eq 1 -and $r.out -match "NOT STARTED" -and $r.out -match "not the OptiScaler-set file" -and (SameSnap $snap1 (Snap $g2)) -and -not @(Get-LeftoverStash).Count) "foreign dxgi.dll: START refuses, nothing moved"
Copy-Item "$os2\OptiScaler.dll" "$g2\dxgi.dll" -Force; (Get-Item "$g2\dxgi.dll").LastWriteTimeUtc = [datetime]"2026-01-01T10:00:00Z"
$snap1 = Snap $g2; [IO.File]::WriteAllBytes("$g2\Other.asi", [byte[]](1..9)); $snap1["Other.asi"] = (Get-FileHash "$g2\Other.asi").Hash
$r = Run "start_session.ps1" ($sc + "-Stock")
Ck ($r.code -eq 1 -and $r.out -match "NOT STARTED" -and $r.out -match "Other.asi") "an unlisted *.asi makes START fail"
Ck ((SameSnap $snap1 (Snap $g2)) -and -not (Test-Path (Join-Path $st2 "active.json")) -and -not @(Get-LeftoverStash).Count) "failed START rolled everything back (files identical, no active state, no leftover stash)"

# 6. summarizer against fake capture files with every input
$o = & py -I (Join-Path $here "summarize.py") --selftest 2>&1 | Out-String
Ck ($o -match "SELFTEST OK") "summarize.py --selftest"

if ($fails.Count) { Write-Host "$($fails.Count) FAILED (kept $root)" -ForegroundColor Red; exit 1 }
Remove-Item $root -Recurse -Force -ErrorAction SilentlyContinue
Write-Host "DRY RUN OK" -ForegroundColor Green
