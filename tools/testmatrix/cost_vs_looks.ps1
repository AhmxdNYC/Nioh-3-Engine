<#
  cost_vs_looks.ps1 - COST vs LOOKS audit, hands-free (notes\cost-vs-looks.md). Needs ASI with [Debug] SkipFx (skipfx.cpp).
  Mode cvl (default): ONE launch through tools\tour\run_tour.ps1 -Input vpad (lock, save backup + restore, ini/ASI install + restore,
       PresentMon, title -> Continue by VirtualPad) on a short tour list (tourlist_cvl.txt: 4 stage spawns, 1 point each, long dwell).
       Extra ini keys: [Debug] SkipFx = 1, [Profiler] GPUProf_Markers = 1 (engine markers for the marker groups), OSD counters off
       (clean screenshots), GPUProf = 2 sampled 1 frame in 8, MinRun 1, Top 100 (since dbed8a1: the group's OWN GP2 cost is the cost,
       the PresentMon frame delta is reported beside it; frame delta >> own = DOWNSTREAM, not a saving; same overhead in every condition). While the tour dwells at a point this script cycles the
       effect groups of cvl_groups.json through the mod's live control file (Nioh3EngineFix_skip.ini):
           BASE, groups 1..k, BASE, groups k+1..n, BASE        (BASE = nothing skipped; the report interpolates BASE for drift)
       Per condition: write the control file, wait for the mod's ack (gen/tag), settle -SettleSec, measure -MeasureSec (window times in
       UTC -> PresentMon GPUBusy), one screenshot (primary screen; the game is focused once per point), ack counters (draws /
       dispatches actually skipped; 0 = the effect is not in this scene).
  Mode split: ONE launch, same route, no SkipFx: GPUProf = 2 with GPUProf2_MinRun = 1 (every copy / clear / resolve and every
       meshlet kernel its own GP2 row; supersedes #473 item 2) + GQ = 1 (graphics-queue gap census for the ~4 ms unattributed,
       see notes\gpu-unattributed.md). Digest: re\scripts_gpu\ally_budget.py gp2 / gp1 -> split_budget.txt, GQ lines -> gq_lines.txt.
  Mode both: split first, then cvl.
  Output: captures\pc\cvl\<stamp>-<tag>\ (manifest.jsonl, shots\, tour\<run_tour capture>, report from tools\testmatrix\cost_vs_looks.py:
       cvl_report.md + cvl_table.csv). Post cvl_report.md as [RESULT].
  -DryRun: no game, no Steam, no lock: a fake game process writes the tour progress lines and answers the control file like the mod;
       fake screenshots + fake PresentMon CSV -> the whole cycle + report path. Ends with 'DryRun OK'.
  Run (pc2): powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\pc2_cost_vs_looks.ps1 -Mode both -UserConfig configs\pc2-bench-1080-120-high.xml
#>
param(
    [ValidateSet("cvl", "split", "both")][string]$Mode = "cvl",
    [string]$Tag = "cvl1",
    [string]$UserConfig = "",
    [string]$TourList = "",
    [string]$Groups = "",
    [string[]]$Only = @(),             # group ids to run (default all, file order)
    [switch]$NoRisky,                  # leave out the groups marked risky (culling / FSR / compose)
    [double]$SettleSec = 2.0, [double]$MeasureSec = 3.0,
    [int]$SplitDwell = 25,
    [switch]$NoShots, [int]$ShotIdleSec = 60,
    [int]$OwnerPid = 0, [int]$MaxMinutes = 120, [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$ws = (Resolve-Path (Join-Path $here "..\..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
if (-not $Groups) { $Groups = Join-Path $here "cvl_groups.json" }
if (-not $TourList) { $TourList = Join-Path $ws "tools\tour\tourlist_cvl.txt" }
if ($DryRun) {
    $dry = Join-Path $env:TEMP "nioh3-cvl-dry-$stamp"
    $GameDir = Join-Path $dry "game"; $SaveRoot = Join-Path $dry "KoeiTecmo\NIOH3"
    $sess = Join-Path $dry "session"
    New-Item -ItemType Directory -Force $GameDir, $SaveRoot | Out-Null
} else {
    $sess = Join-Path $ws "captures\pc\cvl\$stamp-$Tag"
}
. (Join-Path $ws "tools\playsession\session_lib.ps1")   # $GameDir / $SaveRoot defaults, Get-AsiVersion, $UoBridge
$ui = Join-Path $ws "tools\ui.fsx"
$runTour = Join-Path $ws "tools\tour\run_tour.ps1"
$reportPy = Join-Path $here "cost_vs_looks.py"
$shots = Join-Path $sess "shots"
New-Item -ItemType Directory -Force $sess, $shots | Out-Null
$runLog = Join-Path $sess "cvl.log"
$manifest = Join-Path $sess "manifest.jsonl"
$ctl = Join-Path $GameDir "Nioh3EngineFix_skip.ini"; $ack = "$ctl.ack"
$progress = Join-Path $GameDir "Nioh3EngineFix_tour\progress.txt"
$cfg = Join-Path $SaveRoot "config.xml"
$script:cfgBackup = $null
$script:summary = [ordered]@{}

function L([string]$s, [string]$c = "Gray") { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l -ForegroundColor $c; Add-Content $runLog $l }
function Read-Shared([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    try { $fs = [IO.File]::Open($path, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close(); return $t } catch { return "" }
}
function Run-Native([string]$exe, [object[]]$a) { $old = $ErrorActionPreference; $ErrorActionPreference = "Continue"; try { return (& $exe @a 2>&1 | ForEach-Object { "$_" } | Out-String) } finally { $ErrorActionPreference = $old } }
function Add-Jsonl([string]$p, $o) { Add-Content -Path $p -Value ($o | ConvertTo-Json -Compress -Depth 4) -Encoding UTF8 }
function Utc { (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ss.fffZ") }

# ---- control file / ack ----------------------------------------------------------------------------------------------------------
function Write-Control([string]$tag, [string[]]$pso, [string[]]$mk, [int]$clearMode = 0, [string]$clear = "0,0,0,0") {
    $t = "[SkipFx]`r`nTag = $tag`r`nPSO = $($pso -join ',')`r`nMarker = $($mk -join ',')`r`nClearMode = $clearMode`r`nClear = $clear`r`n"
    $tmp = "$ctl.w"
    [IO.File]::WriteAllText($tmp, $t, [Text.Encoding]::ASCII)
    Move-Item -LiteralPath $tmp -Destination $ctl -Force
}
function Read-Ack {
    $t = (Read-Shared $ack).Trim()
    $h = @{}
    foreach ($kv in ($t -split '\s+')) { if ($kv -match '^([^=]+)=(.*)$') { $h[$Matches[1]] = $Matches[2] } }
    return $h
}
function Wait-Ack([string]$tag, [double]$sec = 4) {
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt $sec) { $a = Read-Ack; if ($a.tag -eq $tag) { return $a }; Start-Sleep -Milliseconds 100 }
    return $null
}

# ---- screenshots ---------------------------------------------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
if (-not ("CvlDpi" -as [type])) { Add-Type -Name CvlDpi -Namespace "" -MemberDefinition '[DllImport("user32.dll")] public static extern bool SetProcessDPIAware();' }
$null = [CvlDpi]::SetProcessDPIAware()
function Shot([string]$png) {
    if ($DryRun) { return Fake-Shot $png }
    try {
        $b = [Windows.Forms.Screen]::PrimaryScreen.Bounds
        $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
        $g = [Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($b.Location, [Drawing.Point]::Empty, $b.Size)
        $bmp.Save($png, [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
        return "ok"
    } catch { return "failed: $_" }
}
function Idle-Sec { if ($DryRun) { return 999 }; $o = Run-Native dotnet @("fsi", $ui, "idle"); if ($o -match 'idle ([\d.]+)') { [double]$Matches[1] } else { 0 } }
function Fg-Name { if ($DryRun) { return "Nioh3" }; $o = Run-Native dotnet @("fsi", $ui, "fg"); if ($o -match 'foreground: pid=\d+ (\S+)') { $Matches[1] } else { "" } }
function Focus-Game { if ($DryRun) { return $true }; $null = Run-Native dotnet @("fsi", $ui, "focus", "Nioh3"); Start-Sleep 2; return ((Fg-Name) -eq "Nioh3") }

# ---- dry-run fakes ---------------------------------------------------------------------------------------------------------------
$script:fakeRng = New-Object Random 11
function Fake-Shot([string]$png) {   # a fixed gradient scene + per-group change (so the report has something to rank)
    $bmp = New-Object Drawing.Bitmap 320, 180
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.Clear([Drawing.Color]::FromArgb(60, 90, 120))
    $g.FillRectangle([Drawing.Brushes]::DarkGreen, 0, 120, 320, 60)
    $name = [IO.Path]::GetFileNameWithoutExtension($png)
    if ($name -match 'FOG_all|SKY_all') { $g.FillRectangle([Drawing.Brushes]::Black, 0, 0, 320, 110) }
    elseif ($name -match 'SSFOG') { $g.FillRectangle([Drawing.Brushes]::Gray, 0, 60, 320, 30) }
    elseif ($name -match 'PB2') { $g.FillRectangle([Drawing.Brushes]::White, 100, 100, 80, 60) }
    for ($i = 0; $i -lt 40; $i++) { $bmp.SetPixel($script:fakeRng.Next(320), $script:fakeRng.Next(180), [Drawing.Color]::White) }   # noise
    $bmp.Save($png, [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
    return "ok"
}
function Start-FakeGame([int]$points, [int]$dwell) {
    # writes progress B lines + answers the control file like the mod (ack with fake counts), then exits
    $fake = Join-Path $dry "fake_cvl_game.ps1"
    @'
param([string]$GameDir, [int]$Points, [int]$Dwell)
$ctl = Join-Path $GameDir "Nioh3EngineFix_skip.ini"; $ack = "$ctl.ack"; $prog = Join-Path $GameDir "Nioh3EngineFix_tour\progress.txt"
New-Item -ItemType Directory -Force (Split-Path $prog) | Out-Null
Set-Content $prog "" ; $gen = 0; $last = ""
function Pump { $t = if (Test-Path $ctl) { [IO.File]::ReadAllText($ctl) } else { "" }
  if ($t -ne $script:last) { $script:last = $t; $script:gen++; $script:tag = if ($t -match 'Tag = (\S+)') { $Matches[1] } else { "-" }; $script:t0 = Get-Date }
  if ($script:tag) { $n = if ($t -match 'PSO = \S|Marker = \S') { [int]((Get-Date) - $script:t0).TotalMilliseconds } else { 0 }
    $a = "gen=$($script:gen) tag=$($script:tag) t_apply_unix_ms=0 age_ms=0 draws=$n dispatches=0 ei=0 mesh=0 clears=0 sets=1 begins=5 on=1 tracking=1"
    [IO.File]::WriteAllText("$ack.tmp", $a); Move-Item "$ack.tmp" $ack -Force } }
Start-Sleep 2
for ($p = 0; $p -lt $Points; $p++) {
  Add-Content $prog "A 0 $p 7356 ok"; Add-Content $prog "B 0 $p 0"
  $t0 = Get-Date; while (((Get-Date) - $t0).TotalSeconds -lt $Dwell) { Pump; Start-Sleep -Milliseconds 100 }
  Add-Content $prog "D 0 $p 0"
}
Add-Content $prog "E"
'@ | Set-Content $fake -Encoding UTF8
    return Start-Process powershell -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$fake`"", "-GameDir", "`"$GameDir`"", "-Points", "$points", "-Dwell", "$dwell") -WindowStyle Hidden -PassThru
}
function Write-FakePm([string]$csv) {   # GPUBusy = 23 ms minus the group's gp2_ms (from cvl_groups.json) during its window, + noise
    $rows = @(Get-Content $manifest | ForEach-Object { $_ | ConvertFrom-Json })
    $ms = @{}; foreach ($g in $script:groupDefs) { $ms[$g.id] = [double]$(if ($g.gp2_ms) { $g.gp2_ms } else { 0 }) }
    $sb = New-Object Text.StringBuilder
    [void]$sb.AppendLine("Application,ProcessID,SwapChainAddress,PresentRuntime,SyncInterval,PresentFlags,AllowsTearing,PresentMode,CPUStartDateTime,FrameTime,CPUBusy,CPUWait,GPULatency,GPUTime,GPUBusy,GPUWait")
    foreach ($r in $rows) {
        $t0 = [datetime]::Parse($r.t0_utc).ToUniversalTime(); $t1 = [datetime]::Parse($r.t1_utc).ToUniversalTime()
        $base = 23.0 - 0.1 * $r.point
        $gb = $base - $(if ($ms.ContainsKey($r.cond)) { $ms[$r.cond] } else { 0 })
        for ($t = $t0; $t -lt $t1; $t = $t.AddMilliseconds(25)) {
            $v = $gb + ($script:fakeRng.NextDouble() - 0.5) * 0.6
            [void]$sb.AppendLine("Nioh3.exe,1,0,DXGI,0,0,1,Composed,$($t.ToString('yyyy-MM-dd HH:mm:ss.fff')),25.0,5,20,1,$([Math]::Round($v,3)),$([Math]::Round($v,3)),0")
        }
    }
    [IO.File]::WriteAllText($csv, $sb.ToString())
}

# ---- config.xml -----------------------------------------------------------------------------------------------------------------
function Install-Config {
    if (-not $UserConfig) { L "config.xml: unchanged (no -UserConfig)"; return }
    $src = if ([IO.Path]::IsPathRooted($UserConfig)) { $UserConfig } else { Join-Path $ws $UserConfig }
    if (-not (Test-Path $src)) { throw "user config not found: $src" }
    if (Test-Path $cfg) { $script:cfgBackup = Join-Path $sess "config.xml.orig"; Copy-Item -LiteralPath $cfg -Destination $script:cfgBackup -Force }
    else { $script:cfgBackup = "" }
    Copy-Item -LiteralPath $src -Destination $cfg -Force
    $t = [IO.File]::ReadAllText($cfg)
    L ("config.xml = $src (" + ((@("SCR_REZ", "REN_RAT", "RESOLUTION_SCALING", "FRAME_GENERATION", "VOLUMETRIC", "GI", "SHADOW_QUAL") | ForEach-Object { "$_ " + [regex]::Match($t, "<$_>([^<]*)<").Groups[1].Value }) -join ", ") + ")")
}
function Restore-Config {
    if ($null -eq $script:cfgBackup) { return }
    if ($script:cfgBackup) { Copy-Item -LiteralPath $script:cfgBackup -Destination $cfg -Force; L "config.xml restored" Green }
    else { Remove-Item -LiteralPath $cfg -Force -ErrorAction SilentlyContinue }
    $script:cfgBackup = $null
}

# ---- one tour launch (run_tour.ps1 in a child process) ----------------------------------------------------------------------------
function Start-Tour([string]$sub, [int]$dwell, [string]$keys, [int]$points) {
    $cap = Join-Path $sess $sub; New-Item -ItemType Directory -Force $cap | Out-Null
    if ($DryRun) { return @{ proc = (Start-FakeGame $points $dwell); cap = $cap; out = (Join-Path $cap "console.txt") } }
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$runTour`"", "-Input", "vpad", "-List", "`"$TourList`"", "-Points", "1",
           "-DwellSec", "$dwell", "-NoProfile", "-NoPause", "-CaptureRoot", "`"$cap`"", "-ExtraKeys", "`"$keys`"", "-OwnerPid", "$PID",
           "-MaxMinutes", "$MaxMinutes", "-StallSec", "$($dwell + 300)")
    $out = Join-Path $cap "console.txt"
    $p = Start-Process powershell -ArgumentList $a -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError (Join-Path $cap "console_err.txt")
    return @{ proc = $p; cap = $cap; out = $out }
}
function Tour-Capture($t) {   # run_tour's own capture folder (<cap>\<stamp>-tour)
    if ($DryRun) { return $t.cap }
    $d = @(Get-ChildItem $t.cap -Directory -Filter "*-tour" -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1)
    if ($d.Count) { $d[0].FullName } else { $null }
}

function Dwell-Ended([string]$pline) {   # a D / L / S / A / E progress line after this point's B line = the tour moved on
    $ls = @((Read-Shared $progress) -split "`r?`n" | Where-Object { $_ })
    $k = [Array]::LastIndexOf($ls, $pline)
    if ($k -lt 0) { return $false }
    return [bool](@($ls | Select-Object -Skip ($k + 1) | Where-Object { $_ -match '^(D|L|S|E|A)\b' }).Count)
}
# ---- the cycle at one tour point ---------------------------------------------------------------------------------------------------
function Run-Point([int]$n, [string]$pline, $plan) {
    $doShots = -not $NoShots
    if ($doShots) {
        $idle = Idle-Sec
        if ($idle -lt $ShotIdleSec) { L "  point ${n}: PC in use (idle $([int]$idle) s) - no screenshots at this point" Yellow; $doShots = $false }
        elseif (-not (Focus-Game)) { L "  point ${n}: could not focus Nioh3 - shots may show another window" Yellow }
    }
    $script:fg = if ($doShots) { Fg-Name } else { "" }
    $i = 0
    foreach ($c in $plan) {
        $i++
        $tag = "{0}_{1:D2}_{2}" -f $n, $i, $c.id
        Write-Control $tag $c.pso $c.markers ([int]$c.clearMode) ([string]$c.clear)
        $a0 = Wait-Ack $tag
        if (-not $a0) { L "  $tag : no ack from the mod in 4 s (SkipFx not running?)" Red }
        Start-Sleep -Milliseconds ([int]($SettleSec * 1000))
        $t0 = Utc; $a1 = Read-Ack
        Start-Sleep -Milliseconds ([int]($MeasureSec * 1000))
        $t1 = Utc; $a2 = Read-Ack
        $png = Join-Path $shots "$tag.png"
        $sh = if ($doShots) { Shot $png } else { "off" }
        $ended = Dwell-Ended $pline
        $d = { param($k) if ($a2 -and $a1 -and $a2.tag -eq $tag -and $a1.tag -eq $tag) { [int64]$a2[$k] - [int64]$a1[$k] } else { -1 } }
        Add-Jsonl $manifest ([ordered]@{ point = $n; progress = $pline; idx = $i; cond = $c.id; tag = $tag; t0_utc = $t0; t1_utc = $t1
            shot = $(if ($sh -eq "ok") { "shots/$tag.png" } else { "" }); shot_state = $sh; acked = [bool]$a0; dwell_ended = $ended; fg = $script:fg
            skipped_draws = (& $d "draws"); skipped_dispatches = (& $d "dispatches"); skipped_ei = (& $d "ei"); skipped_mesh = (& $d "mesh")
            sets = $(if ($a2) { [int]$a2.sets } else { -1 }); measure_s = $MeasureSec })
        if (-not $DryRun -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "the game exited during point $n ($tag)" }
        if ($ended) { L "  point ${n}: the tour dwell ended at $tag - remaining conditions skipped (raise the dwell)" Yellow; break }
    }
    Write-Control "idle_$n" @() @()
}

function Build-Plan {
    $all = @($script:groupDefs | Where-Object { (-not $Only.Count -or $Only -contains $_.id) -and -not ($NoRisky -and $_.risky) })
    $base = [pscustomobject]@{ id = "BASE"; pso = @(); markers = @(); clearMode = 0; clear = "0,0,0,0" }
    $mid = [Math]::Ceiling($all.Count / 2)
    $p = @($base) + @($all | Select-Object -First $mid) + @($base) + @($all | Select-Object -Skip $mid) + @($base)
    return $p
}

# =====================================================================================================================================
$t = Get-Content $Groups -Raw | ConvertFrom-Json
$script:groupDefs = @($t.groups | ForEach-Object { $_ | Add-Member -NotePropertyName clearMode -NotePropertyValue $(if ($_.PSObject.Properties["clear_mode"]) { $_.clear_mode } else { 0 }) -PassThru -Force |
    Add-Member -NotePropertyName clear -NotePropertyValue $(if ($_.PSObject.Properties["clear"]) { $_.clear } else { "0,0,0,0" }) -PassThru -Force })
$Only = @($Only -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
Copy-Item $Groups (Join-Path $sess "cvl_groups.json") -Force
try {
    L "cost-vs-looks $Tag$(if ($DryRun) { ' - DRY RUN' }): mode $Mode, $($script:groupDefs.Count) groups in the file, settle $SettleSec s + measure $MeasureSec s, session $sess" Cyan
    if (-not $DryRun) {
        if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh 3 is running - quit it first" }
        $v = Get-AsiVersion $BinAsi
        if ($v -eq "none") { throw "no ASI build at $BinAsi" }
        if (-not ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($BinAsi)) -match "SkipFx")) { throw "the ASI at $BinAsi has no SkipFx (build >= the skipfx commit)" }
        L "ASI $BinAsi = $v (SkipFx present)"
        if (-not (Test-Path $TourList)) { throw "tour list not found: $TourList" }
    }
    Install-Config
    $nPoints = if ($DryRun) { 2 } else { @(Get-Content $TourList | Where-Object { $_ -match '^\s*\d' }).Count }
    $common = "General.AutoContinueLoad=1;Overlay.FpsCounter=0;Overlay.FrameTime=0;VRR.MaxFps=240;Patches.GIMerge=0"

    # ---- split: GP2 MinRun 1 + GQ ----
    if ($Mode -in "split", "both" -and -not $DryRun) {
        $k = "$common;Debug.SkipFx=0;Profiler.GPUProf=2;Profiler.GPUProf2_SampleEvery=8;Profiler.GPUProf2_Top=100;Profiler.GPUProf2_MinRun=1;Profiler.GPUProf2_AB=0;Profiler.GPUProf_Top=60;Profiler.GQ=1;Profiler.GQ_Top=8;Profiler.SP=0"
        L "split launch: GP2 MinRun 1 + GQ, $nPoints points x $SplitDwell s" Cyan
        $tr = Start-Tour "split" $SplitDwell $k $nPoints
        $tr.proc.WaitForExit()
        $o = Read-Shared $tr.out
        $vd = [regex]::Match($o, "verdict: ([\w-]+)")
        $tc = Tour-Capture $tr
        $txt = ""; $gq = ""
        $budPy = Join-Path $ws "re\scripts_gpu\ally_budget.py"
        if ($tc) {
            foreach ($lg in @(Get-ChildItem (Join-Path $tc "logs") -Filter "run*.log" -ErrorAction SilentlyContinue | Sort-Object Name)) {
                $txt += "==== $($lg.Name): ally_budget gp2 --min-draws 1000 (MinRun 1) ====`n" + (Run-Native py @("-I", $budPy, "gp2", $lg.FullName, "--min-draws", "1000", "--top", "120"))
                $txt += "==== $($lg.Name): ally_budget gp1 ====`n" + (Run-Native py @("-I", $budPy, "gp1", $lg.FullName, "--min-span", "4"))
                $gq += (@(Get-Content $lg.FullName | Where-Object { $_ -match "GQ: |GP2: frame " }) -join "`n") + "`n"
            }
        }
        Set-Content (Join-Path $sess "split_budget.txt") $txt -Encoding UTF8
        Set-Content (Join-Path $sess "gq_lines.txt") $gq -Encoding UTF8
        $script:summary["split"] = "tour $(if ($vd.Success) { $vd.Groups[1].Value } else { '?' }); split_budget.txt + gq_lines.txt"
        L "split: $($script:summary['split'])"
    }

    # ---- cvl: skip cycle ----
    if ($Mode -in "cvl", "both") {
        $plan = @(Build-Plan)
        $per = $SettleSec + $MeasureSec + 1.2   # + ack wait + screenshot
        $dwell = [int][Math]::Ceiling($plan.Count * $per + 30 + $(if ($NoShots) { 0 } else { 6 }))
        if ($DryRun) { $SettleSec = 0.3; $MeasureSec = 0.6; $dwell = [int][Math]::Ceiling($plan.Count * 1.5 + 3) }
        L "cvl plan per point ($($plan.Count) conditions, dwell $dwell s, $nPoints points): $(($plan | ForEach-Object { $_.id }) -join ' ')" Cyan
        Remove-Item -LiteralPath $ctl, $ack -Force -ErrorAction SilentlyContinue
        if (-not $DryRun) { Remove-Item -LiteralPath $progress -Force -ErrorAction SilentlyContinue }   # no stale B lines from an earlier tour
        $k ="$common;Debug.SkipFx=1;Debug.SkipFxFile=Nioh3EngineFix_skip.ini;Debug.SkipPSO=;Debug.SkipMarker=;Profiler.GPUProf=2;Profiler.GPUProf2_SampleEvery=8;Profiler.GPUProf2_Top=100;Profiler.GPUProf2_MinRun=1;Profiler.GPUProf2_AB=0;Profiler.GPUProf_Markers=1;Profiler.SP=0"   # dbed8a1: GP2 on (1 frame in 8) = the groups' OWN per-PSO cost next to the frame delta (cost_vs_looks.py own_costs)
        $tr = Start-Tour "tour" $dwell $k $nPoints
        $seen = 0; $n = 0; $tStart = Get-Date; $pmN = 0; $script:cpm = $null
        while (-not $tr.proc.HasExited) {
            Start-Sleep -Milliseconds 300
            # own PresentMon (session Nioh3Cvl) beside run_tour's, restarted if it stops (#560: #126's PresentMon stopped after 6 min)
            if (-not $DryRun -and (Test-Path $PmExe) -and (Get-Process Nioh3 -ErrorAction SilentlyContinue) -and (-not $script:cpm -or $script:cpm.HasExited)) {
                $pmN++
                if ($pmN -gt 1) { L "own PresentMon stopped - restart $pmN" Yellow }
                $pa = "--process_name Nioh3.exe --output_file `"$(Join-Path $sess "presentmon_cvl_$pmN.csv")`" --v2_metrics --date_time --stop_existing_session --session_name Nioh3Cvl --no_console_stats"
                $script:cpm = Start-Process $PmExe -ArgumentList $pa -WindowStyle Hidden -PassThru
            }
            $pl = @((Read-Shared $progress) -split "`r?`n" | Where-Object { $_ -match '^B \d+ \d+ \d+' })
            if ($pl.Count -gt $seen) {
                $seen = $pl.Count; $n++
                L "point $n dwell started ($($pl[-1])) - cycling $($plan.Count) conditions" Green
                try { Run-Point $n $pl[-1] $plan } catch { L "  point $n aborted: $_" Red; if ("$_" -match "exited") { break } }
                L "point $n done"
            }
            if (((Get-Date) - $tStart).TotalMinutes -gt $MaxMinutes) { L "max minutes reached" Red; break }
        }
        if (-not $tr.proc.HasExited) { $tr.proc.WaitForExit(600000) | Out-Null }
        if ($script:cpm -and -not $script:cpm.HasExited) {
            try { Start-Process $PmExe -ArgumentList "--terminate_existing_session --session_name Nioh3Cvl" -WindowStyle Hidden -Wait } catch {}
            Start-Sleep 2; try { Stop-Process -Id $script:cpm.Id -Force -ErrorAction SilentlyContinue } catch {}
        }
        $o = Read-Shared $tr.out
        $vd = [regex]::Match($o, "verdict: ([\w-]+)")
        $tc = Tour-Capture $tr
        L "tour finished: $(if ($vd.Success) { $vd.Groups[1].Value } elseif ($DryRun) { 'dry' } else { '?' }); capture $tc"
        $pm = if ($DryRun) { Join-Path $sess "presentmon.csv" } elseif ($tc) { Join-Path $tc "presentmon.csv" } else { "" }
        if ($DryRun) { Write-FakePm $pm }
        if ($pm -and (Test-Path $pm)) { Copy-Item $pm (Join-Path $sess "presentmon.csv") -Force -ErrorAction SilentlyContinue }
        if ($tc -and -not $DryRun) { foreach ($lg in @(Get-ChildItem (Join-Path $tc "logs") -Filter "run*.log" -ErrorAction SilentlyContinue)) { Copy-Item $lg.FullName (Join-Path $sess "mod_$($lg.Name)") -Force } }
        $rep = Run-Native py @("-I", $reportPy, "report", $sess)
        Set-Content (Join-Path $sess "report_console.txt") $rep -Encoding UTF8
        L "report: $((($rep.Trim()) -split "`n")[-1])"
        $script:summary["cvl"] = "$n points; report $(Join-Path $sess 'cvl_report.md')"
    }
} catch {
    L "!! $_" Red
    $script:summary["error"] = "$_"
} finally {
    Remove-Item -LiteralPath $ctl, "$ctl.w" -Force -ErrorAction SilentlyContinue
    try { Restore-Config } catch { L "!! config restore: $_" Red }
    $para = "cost-vs-looks $Tag ($(Get-Date -f 'yyyy-MM-dd HH:mm')): " + (($script:summary.GetEnumerator() | ForEach-Object { "$($_.Key): $($_.Value)." }) -join " ") + " Folder: $sess"
    Set-Content (Join-Path $sess "summary.txt") $para -Encoding UTF8
    L "summary: $para"
    if ($DryRun) {
        $chk = @()
        $rows = @(if (Test-Path $manifest) { Get-Content $manifest })
        if ($rows.Count -lt 4) { $chk += "manifest has $($rows.Count) rows" }
        if (-not (Test-Path (Join-Path $sess "cvl_report.md"))) { $chk += "no cvl_report.md" }
        elseif (-not ((Get-Content (Join-Path $sess "cvl_report.md") -Raw) -match "## Ranked[\s\S]*\d\. \*\*")) { $chk += "report has no ranked rows" }
        if ($script:summary.Contains("error")) { $chk += "error: $($script:summary['error'])" }
        if ($chk.Count) { L "DryRun FAILED: $($chk -join '; ')" Red } else { L "DryRun OK (session $sess)" Green }
    }
}
