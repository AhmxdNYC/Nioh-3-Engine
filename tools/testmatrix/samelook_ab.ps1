<#
  samelook_ab.ps1 - hands-free A/B of the same-look package (ASI >= 1.7.49; notes\same-look-package.md). Tour mode only (saves
  blocked), one launch through tools\tour\run_tour.ps1 -Input vpad (lock, save backup + restore, ini/ASI install + restore,
  PresentMon, title -> Continue by VirtualPad). Every switch is a live menu row driven through the VirtualPad pipe
  (`vpad.py menu set <KEY> 0|1`), so A and B run in the same launch, at the same spots.
  Ini: [Patches] VSMA / LPVR / SKYR / FOGT / CLDR = 2 (installed, switch off) + the [Tuning] values; OSD off; GPUProf off.
  Route: -TourList (default tourlist_cvl.txt: 4 stage spawns, 1 point each = the same view every pass), -Passes (default 4).
  Per stage load (progress 'L pass stage'): VSMA = ABBA[pass] xor (stage & 1)  -> every stage arrives twice with VSMA on and
       twice off; the virtual shadow map is rebuilt from scratch after the load (the arrival burst).
  Per dwell (progress 'B'):
    1. arrival shot (+ a 3 s static window) with the VSMA state of that load: the shadows the engine marked valid under VSMA
       vs under stock (any stale / missing shadow from delayed requests would persist here until the tile's lifespan ends);
    2. knob cycle (static view): BASE, LPVR, SKYR, FOGT, CLDR, BASE (reversed on odd passes): set on, settle, measure, shot,
       set off;
    3. movement A/B (VSMA live): VSMA off / on / on / off, each = left stick forward -MoveMs then back -MoveMs (the clipmaps
       scroll, new tiles stream in = the movement bursts); one PresentMon window per run.
  Output: captures\pc\samelook\<stamp>-<tag>\ (manifest.jsonl, shots\, presentmon*.csv, mod logs, report from samelook_report.py).
  Run (PC): powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\samelook_ab.ps1 -LockKey nioh3-pc
#>
param(
    [string]$Tag = "sl1",
    [string]$TourList = "",
    [int]$Passes = 4,
    [string]$LockKey = "nioh3-pc",
    [double]$SettleSec = 1.5, [double]$MeasureSec = 3.0, [int]$MoveMs = 2500,
    [string]$Knobs = "LPVR,SKYR,FOGT,CLDR",
    [string]$TuningKeys = "Tuning.VSMA_K=2;Tuning.VSMA_Front=2;Tuning.VSMA_First=0;Tuning.LPVR_N=4;Tuning.SKYR_V=4.0;Tuning.FOGT_V=0.5;Tuning.CLDR_V=0.5",
    [string]$UserConfig = "",
    [switch]$NoShots, [int]$ShotIdleSec = 60, [switch]$NoMove,
    [int]$OwnerPid = 0, [int]$MaxMinutes = 90
)
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$ws = (Resolve-Path (Join-Path $here "..\..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
if (-not $TourList) { $TourList = Join-Path $ws "tools\tour\tourlist_cvl.txt" }
$sess = Join-Path $ws "captures\pc\samelook\$stamp-$Tag"
. (Join-Path $ws "tools\playsession\session_lib.ps1")   # $GameDir / $SaveRoot, Get-AsiVersion, $PmExe, $BinAsi
. (Join-Path $here "vpad.ps1")
$ui = Join-Path $ws "tools\ui.fsx"
$runTour = Join-Path $ws "tools\tour\run_tour.ps1"
$reportPy = Join-Path $here "samelook_report.py"
$shots = Join-Path $sess "shots"
New-Item -ItemType Directory -Force $sess, $shots | Out-Null
$runLog = Join-Path $sess "samelook.log"
$manifest = Join-Path $sess "manifest.jsonl"
$progress = Join-Path $GameDir "Nioh3EngineFix_tour\progress.txt"
$cfg = Join-Path $SaveRoot "config.xml"
$script:cfgBackup = $null
$knobList = @($Knobs -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })

function L([string]$s, [string]$c = "Gray") { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l -ForegroundColor $c; Add-Content $runLog $l }
function Read-Shared([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    try { $fs = [IO.File]::Open($path, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close(); return $t } catch { return "" }
}
function Run-Native([string]$exe, [object[]]$a) { $old = $ErrorActionPreference; $ErrorActionPreference = "Continue"; try { return (& $exe @a 2>&1 | ForEach-Object { "$_" } | Out-String) } finally { $ErrorActionPreference = $old } }
function Add-Jsonl([string]$p, $o) { Add-Content -Path $p -Value ($o | ConvertTo-Json -Compress -Depth 4) -Encoding UTF8 }
function Utc { (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ss.fffZ") }
function Set-Row([string]$key, [int]$v) {
    try { $r = VP menu set $key $v; if (-not $r.ok) { L "  menu set $key $v failed: $($r.reply)" Yellow }; return $r.ok } catch { L "  menu set $key $v : $_" Red; return $false }
}

# ---- screenshots (primary screen; only when nobody used the PC for -ShotIdleSec) ------------------------------------------------
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
if (-not ("SlDpi" -as [type])) { Add-Type -Name SlDpi -Namespace "" -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();' }
$null = [SlDpi]::SetProcessDPIAware()
function Shot([string]$png) {
    try {
        $b = [Windows.Forms.Screen]::PrimaryScreen.Bounds
        $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
        $g = [Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size)
        $bmp.Save($png, [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
        return "ok"
    } catch { return "failed: $_" }
}
function Idle-Sec { $o = Run-Native dotnet @("fsi", $ui, "idle"); if ($o -match 'idle ([\d.]+)') { [double]$Matches[1] } else { 0 } }
function Fg-Name { $o = Run-Native dotnet @("fsi", $ui, "fg"); if ($o -match 'foreground: pid=\d+ (\S+)') { $Matches[1] } else { "" } }
function Focus-Game { $null = Run-Native dotnet @("fsi", $ui, "focus", "Nioh3"); Start-Sleep 2; return ((Fg-Name) -eq "Nioh3") }

function Install-Config {
    if (-not $UserConfig) { L "config.xml: unchanged (no -UserConfig)"; return }
    $src = if ([IO.Path]::IsPathRooted($UserConfig)) { $UserConfig } else { Join-Path $ws $UserConfig }
    if (Test-Path $cfg) { $script:cfgBackup = Join-Path $sess "config.xml.orig"; Copy-Item -LiteralPath $cfg -Destination $script:cfgBackup -Force } else { $script:cfgBackup = "" }
    Copy-Item -LiteralPath $src -Destination $cfg -Force
    L "config.xml = $src"
}
function Restore-Config {
    if ($null -eq $script:cfgBackup) { return }
    if ($script:cfgBackup) { Copy-Item -LiteralPath $script:cfgBackup -Destination $cfg -Force; L "config.xml restored" Green }
    $script:cfgBackup = $null
}

function Measure-Window([string]$kind, [string]$cond, [int]$pass, [int]$stage, [string]$pline, [scriptblock]$during, [string]$shotName) {
    $t0 = Utc
    & $during
    $t1 = Utc
    $sh = "off"
    if ($shotName -and $script:doShots) { $sh = Shot (Join-Path $shots "$shotName.png") }
    Add-Jsonl $manifest ([ordered]@{ kind = $kind; cond = $cond; pass = $pass; stage = $stage; progress = $pline; t0_utc = $t0; t1_utc = $t1
        shot = $(if ($sh -eq "ok") { "shots/$shotName.png" } else { "" }); shot_state = $sh; fg = $script:fg })
}

function Run-Point([int]$pass, [int]$stage, [string]$pline) {
    $script:doShots = -not $NoShots
    if ($script:doShots) {
        $idle = Idle-Sec
        if ($idle -lt $ShotIdleSec) { L "  pass $pass stage ${stage}: PC in use (idle $([int]$idle) s) - no screenshots here" Yellow; $script:doShots = $false }
        elseif (-not (Focus-Game)) { L "  could not focus Nioh3 - shots may show another window" Yellow }
    }
    $script:fg = if ($script:doShots) { Fg-Name } else { "" }
    $v = $script:vsmaArrive
    # 1. arrival (static): the VSMA state of this load
    Start-Sleep -Milliseconds ([int]($SettleSec * 1000))
    Measure-Window "arrive" "VSMA$v" $pass $stage $pline { Start-Sleep -Milliseconds ([int]($MeasureSec * 1000)) } ("p{0}_s{1}_arrive_VSMA{2}" -f $pass, $stage, $v)
    # 2. knob cycle (static view)
    $rev = [string[]]$knobList.Clone(); [array]::Reverse($rev)
    $order = @("BASE") + $(if ($pass % 2) { $rev } else { $knobList }) + @("BASE")
    $i = 0
    foreach ($k in $order) {
        $i++
        if ($k -ne "BASE") { $null = Set-Row $k 1 }
        Start-Sleep -Milliseconds ([int]($SettleSec * 1000))
        Measure-Window "knob" $k $pass $stage $pline { Start-Sleep -Milliseconds ([int]($MeasureSec * 1000)) } ("p{0}_s{1}_k{2:D2}_{3}" -f $pass, $stage, $i, $k)
        if ($k -ne "BASE") { $null = Set-Row $k 0 }
        if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "the game exited" }
    }
    # 3. movement A/B with VSMA live
    if (-not $NoMove) {
        $j = 0
        foreach ($m in @(0, 1, 1, 0)) {
            $j++
            $null = Set-Row "VSMA" $m
            Start-Sleep -Milliseconds 300
            Measure-Window "move" "VSMA$m" $pass $stage $pline {
                $null = VP stick L 0 1 $MoveMs
                $null = VP stick L 0 -1 $MoveMs
            } ""
            L ("  pass $pass stage ${stage}: move {0} VSMA {1} done" -f $j, $m)
        }
        $null = Set-Row "VSMA" $v
    }
}

$script:summary = [ordered]@{}
$abba = @(0, 1, 1, 0)
try {
    L "same-look A/B $Tag : list $TourList, $Passes passes, knobs $($knobList -join ' '), move $MoveMs ms x2, lock $LockKey, session $sess" Cyan
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh 3 is running - quit it first" }
    $ver = Get-AsiVersion $BinAsi
    if (-not ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($BinAsi)) -match "VSMA: VSM waiting-tile")) { throw "the ASI at $BinAsi has no VSMA (needs >= 1.7.49)" }
    L "ASI $BinAsi = $ver"
    Install-Config
    $nStages = @(Get-Content $TourList | Where-Object { $_ -match '^\s*\d' }).Count
    $perPoint = $SettleSec + $MeasureSec + 2 + ($knobList.Count + 2) * ($SettleSec + $MeasureSec + 1.5) + $(if ($NoMove) { 0 } else { 4 * (2 * $MoveMs / 1000.0 + 1.5) })
    $dwell = [int][Math]::Ceiling($perPoint + 20)
    $keys = "General.AutoContinueLoad=1;Overlay.FpsCounter=0;Overlay.FrameTime=0;VRR.MaxFps=240;Profiler.GPUProf=0;Profiler.SP=0;" +
            "Patches.VSMA=2;Patches.LPVR=2;Patches.SKYR=2;Patches.FOGT=2;Patches.CLDR=2;$TuningKeys"
    L "dwell $dwell s per point ($nStages stages x $Passes passes); keys: $keys"
    Remove-Item -LiteralPath $progress -Force -ErrorAction SilentlyContinue
    $cap = Join-Path $sess "tour"; New-Item -ItemType Directory -Force $cap | Out-Null
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$runTour`"", "-Input", "vpad", "-List", "`"$TourList`"", "-Points", "1",
           "-Passes", "$Passes", "-DwellSec", "$dwell", "-NoProfile", "-NoPause", "-CaptureRoot", "`"$cap`"", "-ExtraKeys", "`"$keys`"",
           "-OwnerPid", "$PID", "-MaxMinutes", "$MaxMinutes", "-StallSec", "$($dwell + 300)", "-LockKey", $LockKey)
    $out = Join-Path $cap "console.txt"
    $tr = Start-Process powershell -ArgumentList $a -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError (Join-Path $cap "console_err.txt")
    $seenL = 0; $seenB = 0; $tStart = Get-Date; $pmN = 0; $script:cpm = $null; $script:vsmaArrive = 0; $points = 0
    while (-not $tr.HasExited) {
        Start-Sleep -Milliseconds 300
        if ((Test-Path $PmExe) -and (Get-Process Nioh3 -ErrorAction SilentlyContinue) -and (-not $script:cpm -or $script:cpm.HasExited)) {
            $pmN++
            if ($pmN -gt 1) { L "own PresentMon stopped - restart $pmN" Yellow }
            $pa = "--process_name Nioh3.exe --output_file `"$(Join-Path $sess "presentmon_sl_$pmN.csv")`" --v2_metrics --date_time --stop_existing_session --session_name Nioh3SameLook --no_console_stats"
            $script:cpm = Start-Process $PmExe -ArgumentList $pa -WindowStyle Hidden -PassThru
        }
        $ls = @((Read-Shared $progress) -split "`r?`n" | Where-Object { $_ })
        $lLines = @($ls | Where-Object { $_ -match '^L \d+ \d+ \d+' })
        if ($lLines.Count -gt $seenL) {
            $seenL = $lLines.Count
            $m = [regex]::Match($lLines[-1], '^L (\d+) (\d+)')
            $p = [int]$m.Groups[1].Value; $s = [int]$m.Groups[2].Value
            $script:vsmaArrive = $abba[$p % 4] -bxor ($s -band 1)
            $ok = Set-Row "VSMA" $script:vsmaArrive
            foreach ($k in $knobList) { $null = Set-Row $k 0 }
            L "load pass $p stage $s ($($lLines[-1])): VSMA $($script:vsmaArrive) for the arrival$(if (-not $ok) { ' (menu set FAILED)' })" Green
        }
        $bLines = @($ls | Where-Object { $_ -match '^B \d+ \d+ \d+' })
        if ($bLines.Count -gt $seenB) {
            $seenB = $bLines.Count
            $m = [regex]::Match($bLines[-1], '^B (\d+) (\d+)')
            $p = [int]$m.Groups[1].Value; $s = [int]$m.Groups[2].Value
            L "dwell pass $p stage $s started ($($bLines[-1]))" Green
            try { Run-Point $p $s $bLines[-1]; $points++ } catch { L "  point aborted: $_" Red; if ("$_" -match "exited|ABORT") { break } }
            L "dwell pass $p stage $s done"
        }
        if (((Get-Date) - $tStart).TotalMinutes -gt $MaxMinutes) { L "max minutes reached" Red; break }
    }
    if (-not $tr.HasExited) { $tr.WaitForExit(900000) | Out-Null }
    if ($script:cpm -and -not $script:cpm.HasExited) {
        try { Start-Process $PmExe -ArgumentList "--terminate_existing_session --session_name Nioh3SameLook" -WindowStyle Hidden -Wait } catch {}
        Start-Sleep 2; try { Stop-Process -Id $script:cpm.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
    $o = Read-Shared $out
    $vd = [regex]::Match($o, "verdict: ([\w-]+)")
    $tc = @(Get-ChildItem $cap -Directory -Filter "*-tour" -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1)
    if ($tc.Count) {
        $tc = $tc[0].FullName
        if (Test-Path (Join-Path $tc "presentmon.csv")) { Copy-Item (Join-Path $tc "presentmon.csv") (Join-Path $sess "presentmon_tour.csv") -Force }
        foreach ($lg in @(Get-ChildItem (Join-Path $tc "logs") -Filter "run*.log" -ErrorAction SilentlyContinue)) { Copy-Item $lg.FullName (Join-Path $sess "mod_$($lg.Name)") -Force }
    }
    L "tour finished: $(if ($vd.Success) { $vd.Groups[1].Value } else { '?' }); $points points measured"
    $rep = Run-Native py @("-I", $reportPy, "report", $sess)
    Set-Content (Join-Path $sess "report_console.txt") $rep -Encoding UTF8
    L "report: $((($rep.Trim()) -split "`n")[-1])"
    $script:summary["tour"] = "$(if ($vd.Success) { $vd.Groups[1].Value } else { '?' }), $points points"
} catch {
    L "!! $_" Red
    $script:summary["error"] = "$_"
} finally {
    try { Restore-Config } catch { L "!! config restore: $_" Red }
    $para = "same-look $Tag ($(Get-Date -f 'yyyy-MM-dd HH:mm')): " + (($script:summary.GetEnumerator() | ForEach-Object { "$($_.Key): $($_.Value)." }) -join " ") + " Folder: $sess"
    Set-Content (Join-Path $sess "summary.txt") $para -Encoding UTF8
    L "summary: $para"
}
