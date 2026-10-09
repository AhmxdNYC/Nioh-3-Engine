<#
  pc2_run_tour.ps1 - the Tour stability / performance run (notes\tour-mode.md). No input is ever sent to the game.
  1. refuses if Nioh3 runs or another device holds the bridge game lock; takes the lock (harness_guard: owner pid, commit headroom,
     max 2 crashes, MaxMinutes)
  2. backs up %LOCALAPPDATA%\KoeiTecmo\NIOH3 to C:\nioh3-re\save-backups\<stamp>-tour + a SHA-256 manifest
  3. installs the current ASI build + the user's ini with the tour keys ([Debug] Tour/TourDwellSec/TourPointsPerStage/TourPasses/TourList,
     [General] AutoContinueLoad = 1, [Profiler] MemStats/LoadStats/LoadGates = 1, GPUProf = 2 sampled 1-in-16, SP = 1 at 5 ms)
  4. starts PresentMon, launches the game through Steam and waits: YOU press Continue at the title once per launch; the mod arms
     itself in the field and tours. The script watches the mod log: "Tour: DONE" -> closes the game; game gone without DONE -> crash,
     relaunch (the mod resumes after the crashed point; max 2); no tour progress for -StallSec -> kill + relaunch.
  5. collects logs / progress / PresentMon into captures\tour\<stamp>, compares the save folder with the manifest (reports any change),
     restores every changed / removed / added file byte-for-byte and re-verifies all hashes, puts the user's ini + ASI back,
     releases the lock and writes tour_report.md (summarize_tour.py).
  -DryRun: no game, no Steam, no lock: temp folders and tools\tour\fake_game.ps1 (writes tour log lines, crashes once mid-tour,
  scribbles on the fake save) - tests the whole wrapper path offline.
#>
param(
    [ValidateSet(1, 2)][int]$Mode = 1,
    [int]$DwellSec = 4, [int]$Points = 3, [int]$Passes = 1,
    [string]$List = "main",            # main | full | builtin | <path to a list file>
    [switch]$Resume,                   # keep Nioh3EngineFix_tour\progress.txt (continue an interrupted tour)
    [switch]$NoProfile,                # no GP / SP windows (frame / memory lines only)
    [switch]$DryRun, [switch]$NoLock, [switch]$NoPause,
    [int]$StallSec = 240, [int]$ArmWaitMin = 15, [int]$MaxRelaunch = 2, [int]$MaxMinutes = 120, [int]$OwnerPid = 0,
    [string]$GameDir = "", [string]$SaveRoot = "", [string]$BackupRoot = "", [string]$CaptureRoot = "",
    # -Input vpad (ASI >= 1.7.37): the title / Continue step is driven through the mod's VirtualPad pipe (tools\testmatrix\vpad.py,
    # VirtualPad = 1 + KeepRunningUnfocused = 1) - no key press needed from you, the game does not need the foreground.
    # Default ui = you press Continue at the title (unchanged).
    [Alias("Input")][ValidateSet("ui", "vpad")][string]$InputMode = "ui",
    # extra ini keys "Section.Key=Value" joined by ";" (e.g. "Patches.PJ=1;Patches.SPR=1"); applied after the tour keys (pc_retest_run8.ps1 Block P)
    [string[]]$ExtraKeys = @(),
    # hold the vpad title -> Continue step until the PJ prewarm has finished (log 'PJ: prewarm done' / refused / nothing to do), max -WaitPrewarmSec; stutter.md 6.1 B2
    [switch]$WaitPrewarm, [int]$WaitPrewarmSec = 300,
    # weak-CPU simulation: pin the Nioh3 process to N logical CPUs (distinct physical cores first), per process, harness-set
    [int]$Affinity = 0
)
$vp = $InputMode -eq "vpad"
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$ws = (Resolve-Path (Join-Path $here "..\..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
if ($DryRun) {
    $dry = Join-Path $env:TEMP "nioh3-tour-dry-$stamp"
    if (-not $GameDir) { $GameDir = Join-Path $dry "game" }
    if (-not $SaveRoot) { $SaveRoot = Join-Path $dry "KoeiTecmo\NIOH3" }
    if (-not $BackupRoot) { $BackupRoot = Join-Path $dry "save-backups" }
    if (-not $CaptureRoot) { $CaptureRoot = Join-Path $dry "captures" }
    $NoLock = $true
    $env:HG_MIN_HEADROOM_GB = "0"; $env:HG_MIN_CURRENT_GB = "0"
    foreach ($d in $GameDir, (Join-Path $SaveRoot "Savedata\76561199000000000\SAVEDATA00"), (Join-Path $SaveRoot "Savedata_backup")) { New-Item -ItemType Directory -Force $d | Out-Null }
    $rnd = New-Object byte[] 65536; (New-Object Random 7).NextBytes($rnd)
    [IO.File]::WriteAllBytes((Join-Path $SaveRoot "Savedata\76561199000000000\SAVEDATA00\SAVEDATA.BIN"), $rnd)
    [IO.File]::WriteAllBytes((Join-Path $SaveRoot "Savedata\76561199000000000\SAVEDATA00\BACKUP.BIN"), $rnd)
    Set-Content (Join-Path $SaveRoot "config.xml") "<config><PRESET>3</PRESET></config>" -Encoding UTF8
    Copy-Item (Join-Path $ws "mod\Nioh3EngineFix\Nioh3EngineFix.ini") (Join-Path $GameDir "Nioh3EngineFix.ini")
    Set-Content (Join-Path $GameDir "Nioh3EngineFix.asi") "old asi" -Encoding ASCII
}
if (-not $CaptureRoot) { $CaptureRoot = Join-Path $ws "captures\tour" }
$StateDir = Join-Path $here "state"
. (Join-Path $ws "tools\playsession\pc2_session_lib.ps1")   # Set-IniValue, Get-AsiVersion, Invoke-GameLock, Get-LockHolder ($GameDir/$SaveRoot/$BackupRoot defaults)
. (Join-Path $ws "tools\testmatrix\harness_guard.ps1")
. (Join-Path $here "..\testmatrix\vpad.ps1")
function Say([string]$s, [string]$c = "Gray") { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l -ForegroundColor $c; if ($script:runLog) { Add-Content $script:runLog $l } }

$cap = Join-Path $CaptureRoot "$stamp-tour"
$logsDir = Join-Path $cap "logs"; $orig = Join-Path $cap "orig"
New-Item -ItemType Directory -Force $logsDir, $orig, $StateDir | Out-Null
$script:runLog = Join-Path $cap "run_tour.log"
$asi = Join-Path $GameDir "Nioh3EngineFix.asi"; $ini = Join-Path $GameDir "Nioh3EngineFix.ini"; $modLog = Join-Path $GameDir "Nioh3EngineFix.log"
$tourDir = Join-Path $GameDir "Nioh3EngineFix_tour"; $progress = Join-Path $tourDir "progress.txt"
$bk = Join-Path $BackupRoot "$stamp-tour"
$script:lockHeld = $false; $script:modInstalled = $false; $script:saveBacked = $false; $script:finished = $false
$script:game = $null; $script:pm = $null; $script:logN = 0; $script:verdict = "aborted"

function Get-Manifest([string]$root) {
    $m = @{}
    if (-not (Test-Path $root)) { return $m }
    $rootFull = (Get-Item -LiteralPath $root -Force).FullName.TrimEnd('\')   # children's FullName starts with exactly this string
    foreach ($f in Get-ChildItem -LiteralPath $rootFull -Recurse -File -Force) {
        $rel = $f.FullName.Substring($rootFull.Length + 1)
        $m[$rel] = @{ len = $f.Length; sha = (Get-FileHash $f.FullName -Algorithm SHA256).Hash }
    }
    return $m
}
function Compare-Manifest($want, $have) {
    $chg = @(); $add = @(); $del = @()
    foreach ($k in $want.Keys) { if (-not $have.ContainsKey($k)) { $del += $k } elseif ($have[$k].sha -ne $want[$k].sha) { $chg += $k } }
    foreach ($k in $have.Keys) { if (-not $want.ContainsKey($k)) { $add += $k } }
    return @{ changed = $chg; added = $add; removed = $del }
}
function Read-Shared([string]$path) {   # the mod keeps its log open with deny-write: read with ReadWrite sharing
    if (-not (Test-Path $path)) { return "" }
    try { $fs = [IO.File]::Open($path, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close(); return $t } catch { return "" }
}
function Save-ModLog {
    $t = Read-Shared $modLog
    if ($t -and $t -ne $script:lastSaved) { $script:lastSaved = $t; $script:logN++; Set-Content -Path (Join-Path $logsDir ("run{0:D2}.log" -f $script:logN)) -Value $t -Encoding UTF8 -NoNewline }
}
function Game-Alive { return ($script:game -and -not $script:game.HasExited) }
function Close-TourGame([switch]$Kill) {
    if (-not (Game-Alive)) { return }
    Note-Closed
    if (-not $Kill -and -not $DryRun) {
        try { $null = $script:game.CloseMainWindow() } catch {}
        for ($i = 0; $i -lt 30 -and (Game-Alive); $i++) { & $script:RealSleep -Seconds 2 }
    }
    if (Game-Alive) { try { Stop-Process -Id $script:game.Id -Force } catch {} ; & $script:RealSleep -Seconds 3 }
}
function Launch-Game([int]$n) {
    Assert-Launch
    if ($DryRun) {
        $fa = "-NoProfile -ExecutionPolicy Bypass -File `"$(Join-Path $here 'fake_game.ps1')`" -GameDir `"$GameDir`" -SaveRoot `"$SaveRoot`" -Launch $n"
        $script:game = Start-Process powershell -ArgumentList $fa -WindowStyle Hidden -PassThru
        Register-Child $script:game "fake_game"
        Say "dry run: fake game launched (pid $($script:game.Id), launch $n)"
        return
    }
    $t0 = Get-Date
    $fgBefore = if ($vp) { VP-ForegroundInfo } else { $null }
    Start-Process "steam://rungameid/3681010"
    while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0) -and ((Get-Date) - $t0).TotalSeconds -lt 180) { $cc = "$(& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot '..	estmatrix\steam_cloud_click.ps1') 2>$null | Select-Object -First 1)"; if ($cc -like 'clicked*') { Write-Host "[launch] Steam cloud dialog: $cc" }; Start-Sleep 3 }   # steam_cloud_click: 'Unable to Sync' -> Play anyway (user OK, bridge #602)
    $script:game = Get-Process Nioh3 -ErrorAction SilentlyContinue | Select-Object -First 1
    Register-Game
    if (-not $script:game) { Guard-Fail "the game did not start within 180 s" }
    if ($Affinity -gt 0) { Set-WeakAffinity $Affinity { param($m) Say $m } }
    if ($WaitPrewarm -and -not $DryRun) {
        $tw = Get-Date; $pw = ""
        while (((Get-Date) - $tw).TotalSeconds -lt $WaitPrewarmSec) {
            Start-Sleep 3; $lt = Read-Shared $modLog
            if ($lt -match "(?m)PJ: prewarm (done|skipped|stopped)[^
]*") { $pw = $Matches[0]; break }
            if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { break }
        }
        Say ("PJ prewarm wait: " + $(if ($pw) { $pw } else { "no prewarm line within $WaitPrewarmSec s - continuing" })) Cyan
    }
    if ($vp) {   # title -> Continue -> field through the mod's pipe, in the background (the watch loop below keeps running)
        Start-Sleep 3; VP-GiveBackFocus $fgBefore
        $tc = Join-Path $cap ("title_continue_{0:D2}.txt" -f $n)
        $script:vpTc = Start-Process py -ArgumentList @("-I", "`"$script:VpPy`"", "--owner-pid", $global:HG.ownerPid, "title-continue", "600") -WindowStyle Hidden -PassThru -RedirectStandardOutput $tc
        Register-Child $script:vpTc "vpad-title"
        Say "game started (pid $($script:game.Id), launch $n). VirtualPad drives the title -> Continue -> field (log: $tc); hands off." Cyan
        return
    }
    Say "game started (pid $($script:game.Id), launch $n). PRESS CONTINUE AT THE TITLE and load your save - nothing else; the tour starts by itself." Cyan
}
function Finish-Run {
    if ($script:finished) { return }
    $script:finished = $true
    try { Stop-TestPopup } catch {}
    try { Close-TourGame } catch {}
    try { Save-ModLog } catch {}
    if ($script:pm -and -not $script:pm.HasExited) { try { Start-Process $PmExe -ArgumentList "--terminate_existing_session --session_name Nioh3Tour" -WindowStyle Hidden -Wait } catch {}; & $script:RealSleep -Seconds 2; try { Stop-Process -Id $script:pm.Id -Force -ErrorAction SilentlyContinue } catch {} }
    if (Test-Path $progress) { Copy-Item $progress (Join-Path $cap "progress.txt") -Force }
    # ---- save: compare, restore, verify ----
    if ($script:saveBacked) {
        $want = Get-Content (Join-Path $bk "manifest.json") -Raw | ConvertFrom-Json
        $wantH = @{}; foreach ($p in $want.PSObject.Properties) { $wantH[$p.Name] = @{ len = $p.Value.len; sha = $p.Value.sha } }
        $diff = Compare-Manifest $wantH (Get-Manifest $SaveRoot)
        $n = $diff.changed.Count + $diff.added.Count + $diff.removed.Count
        if ($n) {
            Say ("SAVE FOLDER CHANGED during the run: {0} changed, {1} added, {2} removed ({3})" -f $diff.changed.Count, $diff.added.Count, $diff.removed.Count, (($diff.changed + $diff.added + $diff.removed) -join ", ")) Yellow
            if (-not $DryRun) { Say "If Steam shows a cloud conflict for Nioh 3 at the next start, keep the LOCAL files (they are the restored ones)." Yellow }
        } else { Say "save folder unchanged (the tour's save block held)" Green }
        foreach ($k in $diff.added) { Remove-Item (Join-Path $SaveRoot $k) -Force }
        foreach ($k in @($diff.changed) + @($diff.removed)) { $dst = Join-Path $SaveRoot $k; New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null; Copy-Item (Join-Path $bk "files\$k") $dst -Force }
        $after = Compare-Manifest $wantH (Get-Manifest $SaveRoot)
        $bad = $after.changed.Count + $after.added.Count + $after.removed.Count
        if ($bad) { Say "!! RESTORE INCOMPLETE ($bad files differ) - copy $bk\files back by hand" Red; $script:verdict = "restore-failed" }
        else { Say ("save folder restored and verified byte-for-byte ({0} files, {1} were restored)" -f $wantH.Count, $n) Green }
        @{ changed = $diff.changed; added = $diff.added; removed = $diff.removed; verified = ($bad -eq 0) } | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $cap "save_check.json") -Encoding UTF8
    }
    # ---- mod files back ----
    if ($script:modInstalled) {
        if (Test-Path (Join-Path $orig "Nioh3EngineFix.ini")) { Copy-Item (Join-Path $orig "Nioh3EngineFix.ini") $ini -Force } else { Remove-Item $ini -Force -ErrorAction SilentlyContinue }
        if (Test-Path (Join-Path $orig "Nioh3EngineFix.asi")) { Copy-Item (Join-Path $orig "Nioh3EngineFix.asi") $asi -Force } else { Remove-Item $asi -Force -ErrorAction SilentlyContinue }
        Say "your Nioh3EngineFix.ini / .asi are back as they were (Tour = 0)"
    }
    if ($script:lockHeld) { $null = Invoke-GameLock "release"; Say "game lock released" }
    try {
        $rep = & py -I (Join-Path $here "summarize_tour.py") $cap 2>&1 | Out-String
        Say "report: $($rep.Trim())" Green
    } catch { Say "summarize_tour.py failed: $_" Yellow }
    Say "verdict: $($script:verdict); capture folder: $cap" Cyan
}

Init-Guard -Name "run_tour" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -MaxCrashes ($MaxRelaunch + 1) -Cleanup { Finish-Run }
if (-not $DryRun) { Start-TestPopup -Name "Tour (mode $Mode, list $List)" -InputMode $InputMode }   # status popup; blocking only if the user clicks it
try {
    Say "Nioh 3 tour - mode $Mode, list $List, $Points points x $DwellSec s, $Passes pass(es)$(if ($DryRun) { ' - DRY RUN' })" Cyan
    if (-not $DryRun -and (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue)) { throw "Nioh 3 is running - quit it first" }
    $binAsi = Join-Path $ws "mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi"
    if (-not $DryRun -and -not (Test-Path $binAsi)) { throw "no mod build at $binAsi" }
    $asiVer = if ($DryRun) { "dry" } else { Get-AsiVersion $binAsi }
    if (-not $DryRun -and $asiVer -ne "unknown" -and [version]$asiVer -lt [version]"1.7.36") { throw "the build is $asiVer; Tour needs 1.7.36+" }
    if ($vp -and -not $DryRun -and $asiVer -ne "unknown" -and [version]$asiVer -lt [version]"1.7.37") { throw "the build is $asiVer; -Input vpad needs 1.7.37+" }
    if (-not $NoLock) {
        $h = Get-LockHolder
        if ($h.ok -and $h.holder -and $h.holder -ne "pc2") { throw "the game is in use on '$($h.holder)' (bridge game lock)" }
        $r = Invoke-GameLock "acquire" ([Math]::Max(3600, $MaxMinutes * 60))
        if ($r.code -eq 1) { throw "the game lock is held by another device: $($r.out)" }
        if ($r.code -eq 0) { $script:lockHeld = $true; Say "game lock taken" } else { Say "game lock: bridge not reachable ($($r.out)) - continuing" Yellow }
    }
    # ---- 2. save backup + manifest ----
    if (-not (Test-Path $SaveRoot)) { throw "save folder not found: $SaveRoot" }
    New-Item -ItemType Directory -Force (Join-Path $bk "files") | Out-Null
    Copy-Item -Path (Join-Path $SaveRoot "*") -Destination (Join-Path $bk "files") -Recurse -Force
    $man = Get-Manifest $SaveRoot
    $chk = Compare-Manifest $man (Get-Manifest (Join-Path $bk "files"))
    if ($chk.changed.Count + $chk.added.Count + $chk.removed.Count) { throw "save backup does not match the save folder" }
    $man | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $bk "manifest.json") -Encoding UTF8
    $script:saveBacked = $true
    Say ("save backup: {0} ({1} files, {2:N0} MB, SHA-256 manifest)" -f $bk, $man.Count, (($man.Values | ForEach-Object { $_.len } | Measure-Object -Sum).Sum / 1MB)) Green
    # ---- 3. mod + ini ----
    if (Test-Path $asi) { Copy-Item $asi (Join-Path $orig "Nioh3EngineFix.asi") -Force }
    if (Test-Path $ini) { Copy-Item $ini (Join-Path $orig "Nioh3EngineFix.ini") -Force }
    $baseIni = if (Test-Path $ini) { $ini } else { Join-Path $ws "mod\Nioh3EngineFix\Nioh3EngineFix.ini" }
    $lines = [IO.File]::ReadAllLines($baseIni)
    $listFile = ""
    switch ($List) {
        "builtin" { $listFile = "" }
        "main" { $listFile = Join-Path $here "tourlist_main.txt" }
        "full" { $listFile = Join-Path $here "tourlist_full.txt" }
        default { $listFile = (Resolve-Path $List).Path }
    }
    $listName = ""
    if ($listFile) { $listName = "Nioh3EngineFix_tourlist.txt"; Copy-Item $listFile (Join-Path $GameDir $listName) -Force; Copy-Item $listFile (Join-Path $cap "tourlist.txt") -Force }
    $keys = @(@("Debug", "Tour", "$Mode"), @("Debug", "TourDwellSec", "$DwellSec"), @("Debug", "TourPointsPerStage", "$Points"),
              @("Debug", "TourPasses", "$Passes"), @("Debug", "TourList", $listName), @("General", "AutoContinueLoad", "1"),
              @("Profiler", "MemStats", "1"), @("Profiler", "LoadStats", "1"), @("Profiler", "LoadGates", "1"))
    if ($vp) { $keys += @(@("Debug", "VirtualPad", "1"), @("Debug", "KeepRunningUnfocused", "1")) }
    if (-not $NoProfile) {
        $keys += @(@("Profiler", "GPUProf", "2"), @("Profiler", "GPUProf2_SampleEvery", "16"), @("Profiler", "GPUProf2_AB", "0"),
                   @("Profiler", "GPUProf2_MinRun", "8"), @("Profiler", "SP", "1"), @("Profiler", "IntervalUs", "5000"),
                   @("Profiler", "Workers", "1"), @("Profiler", "StartDelaySec", "0"))
    }
    foreach ($x in @($ExtraKeys -split ";" | ForEach-Object { $_.Trim() } | Where-Object { $_ })) { if ($x -match '^([^.=]+)\.([^=]+)=(.*)$') { $keys += ,@($Matches[1], $Matches[2].Trim(), $Matches[3].Trim()) } else { throw "bad -ExtraKeys entry '$x' (want Section.Key=Value)" } }
    foreach ($k in $keys) { $lines = Set-IniValue $lines $k[0] $k[1] $k[2] }
    $script:modInstalled = $true
    [IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding($false)))
    if (-not $DryRun) { Copy-Item $binAsi $asi -Force }
    Copy-Item $ini (Join-Path $cap "ini_used.ini") -Force
    New-Item -ItemType Directory -Force $tourDir | Out-Null
    if (-not $Resume -and (Test-Path $progress)) { Remove-Item $progress -Force; Say "progress.txt cleared (fresh tour; -Resume keeps it)" }
    Say "mod: $asiVer installed with $($keys.Count) tour keys (Tour = $Mode, saves blocked while the game runs)"
    # ---- 4. PresentMon ----
    if (-not $DryRun -and (Test-Path $PmExe)) {
        $pmArgs = "--process_name Nioh3.exe --output_file `"$(Join-Path $cap 'presentmon.csv')`" --v2_metrics --date_time --stop_existing_session --session_name Nioh3Tour --no_console_stats"
        $script:pm = Start-Process $PmExe -ArgumentList $pmArgs -WindowStyle Hidden -PassThru
        Register-Child $script:pm "PresentMon"
        Say "PresentMon recording"
    }
    # ---- launch + watch ----
    $launch = 0; $armedAny = $false
    while ($true) {
        $launch++
        if ($launch -gt $MaxRelaunch + 1) { Say "not relaunching: $MaxRelaunch relaunches used" Red; $script:verdict = "gave-up"; break }
        Set-TestStatus "launch $launch - touring"
        Launch-Game $launch
        $tLaunch = Get-Date; $tProgress = Get-Date; $lastSig = ""; $armed = $false; $outcome = ""
        while ($true) {
            Start-Sleep 2
            $t = Read-Shared $modLog
            if ($DryRun -and -not $t) { $t = "" }
            $sig = ([regex]::Matches($t, "Tour P |Tour: stage \d+ arrived|Tour: stage \d+/\d+ pass").Count)
            if ("$sig" -ne $lastSig) { $lastSig = "$sig"; $tProgress = Get-Date }
            if (-not $armed -and $t -match "Tour: ARMED") { $armed = $true; $armedAny = $true; $tProgress = Get-Date; Say "tour armed - touring (hands off)" Green }
            if ($t -match "Tour: DONE|tour already finished") { $outcome = "done"; break }
            if ($t -match "Tour: DISARMED") { $outcome = "disarmed"; break }
            if ($t -match "Tour: not installed|Tour: the TourList is empty|tour does not run") { $outcome = "not-installed"; break }
            if (-not (Game-Alive)) { $outcome = "exited"; break }
            if (-not $armed -and ((Get-Date) - $tLaunch).TotalMinutes -gt $ArmWaitMin) { $outcome = "never-armed"; break }
            if ($armed -and ((Get-Date) - $tProgress).TotalSeconds -gt $StallSec) { $outcome = "stall"; break }
        }
        Say "launch $launch ended: $outcome"
        if ($outcome -eq "done") { Close-TourGame; Save-ModLog; $script:verdict = "done"; break }
        if ($outcome -in "disarmed", "not-installed", "never-armed") { Close-TourGame; Save-ModLog; $script:verdict = $outcome; break }
        if ($outcome -eq "stall") { Close-TourGame -Kill; Note-Crash "tour stalled $StallSec s"; Save-ModLog; Say "killed the stalled game; the next launch resumes after the stalled point" Yellow; continue }
        # exited without DONE = crash (Assert-Launch counts the vanished pid); the mod resumes from progress.txt
        Save-ModLog
        if ((Test-Path $progress) -and ((Get-Content $progress -Raw) -match "(?m)^E\s*$")) { $script:verdict = "done"; break }
        Say "the game exited without finishing the tour (crash?) - relaunching; the crashed point is skipped" Yellow
        if ($DryRun) { Note-Crash "fake game exit" }
    }
} catch {
    Say "!! $_" Red
    $script:verdict = "error: $_"
} finally {
    Finish-Run
    Done-Guard
}
if (-not $NoPause -and -not $DryRun) { Read-Host "Press Enter to close" | Out-Null }
if ($script:verdict -eq "done") { exit 0 } else { exit 1 }
