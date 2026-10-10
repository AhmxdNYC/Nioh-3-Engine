<#
  fcap_capture.ps1 - FCAP1: one-frame D3D12 command-stream captures at fixed tour points (notes\renderer\00-map.md section 4).
  Windows side (pc2: -Device pc2 -RunTour tools\tour\pc2_run_tour.ps1; PC: defaults). The Ally runs the same spec with its harness.
  Spec (identical on every device; same matched settings as XSYS1, notes\cross-system-compare.md):
    config.xml  configs\xsys-1080-high-fsr65.xml (1920x1080, custom HIGH, REN_RAT 65, FSR 3.1, FG OFF, VSync OFF)
    tour        tools\tour\tourlist_xsys.txt = 7356 / 52 / 7292 / 5631, spawn point only, 1 pass, 40 s dwell
    ini keys    Profiler.GPUProf=0 (FC owns the hook mask), Profiler.FrameCap=1, FrameCap_Frames=2, FrameCap_TourDelay=300,
                FrameCap_Max=8, FrameCap_MaxEvents=600000, GpuOpt.Barriers=0, VRR.MaxFps=240
    ASI         -Asi <file> (the FC build from the bridge) is put into mod\Nioh3EngineFix\bin for the run and the old one restored
  Output: captures\fcap\<device>-<stamp>\ with Nioh3EngineFix_fcap_<n>_<tour tag>.csv (one per point), the mod log, run_tour files,
  the digest (re\scripts_renderer\fcap_digest.py) -> zip on the bridge + [RESULT].
  Run:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\fcap_capture.ps1 -Device pc2 -RunTour tools\tour\pc2_run_tour.ps1 -Asi <asi> -WaitFree
#>
param(
    [string]$Device = "pc", [string]$LockKey = "", [string]$RunTour = "", [string]$Asi = "",
    [int]$DwellSec = 40, [int]$TourDelay = 300,
    [switch]$WaitFree, [int]$WaitMaxMin = 240, [switch]$NoPost, [switch]$DryRun,
    [string]$UoBridge = (Join-Path $env:USERPROFILE "universal-optimizer\agent-bridge"),
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
)
$ErrorActionPreference = "Stop"
$ws = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $LockKey) { $LockKey = "nioh3-$Device" }
if (-not $RunTour) { $RunTour = Join-Path $ws "tools\tour\run_tour.ps1" } elseif (-not [IO.Path]::IsPathRooted($RunTour)) { $RunTour = Join-Path $ws $RunTour }
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$out = Join-Path $ws "captures\fcap\$Device-$stamp"
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out "fcap_capture.log"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l; Add-Content $log $l }
$env:UO_BRIDGE_TOKEN_FILE = Join-Path $env:APPDATA "uo-bridge\token"; $env:UO_AGENT_ID = $Device
function Bridge([string[]]$a) { Push-Location $UoBridge; try { (& py -m @a 2>&1 | Out-String).Trim() } finally { Pop-Location } }
$cfg = Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3\config.xml"
$matched = Join-Path $ws "configs\xsys-1080-high-fsr65.xml"
$list = Join-Path $ws "tools\tour\tourlist_xsys.txt"
$binAsi = Join-Path $ws "mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi"
$HarnessRx = 'run_tour|ab_driver|pc_gpu_session|pc_retest|abba2?_pc|final_pc|load_pc|cost_vs_looks|loadstats_ab|pc_weakcpu|run_pc\.ps1|xsys_capture'
function Test-Free {
    $why = @()
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { $why += "Nioh3 running" }
    $st = Bridge @("uo_bridge.gamelock", "status", $LockKey)
    if ($st -notmatch "'holder':\s*None") { $why += "lock $LockKey held" }
    $h = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe' or Name='pwsh.exe'" -ErrorAction SilentlyContinue |
           Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine -match $HarnessRx })
    if ($h.Count) { $why += "harness running (pid $($h.ProcessId -join ','))" }
    return $why
}
L "FCAP1 on $Device (lock $LockKey, list $list, dwell $DwellSec s, capture $TourDelay frames into each dwell)"
if ($WaitFree -and -not $DryRun) {
    $t0 = Get-Date; $ok = 0
    while ($ok -lt 3) {
        $why = @(Test-Free)
        if ($why.Count) { L "waiting: $($why -join '; ')"; $ok = 0 } else { $ok++ }
        if (((Get-Date) - $t0).TotalMinutes -gt $WaitMaxMin) { L "gave up after $WaitMaxMin min"; exit 3 }
        if ($ok -lt 3) { [Threading.Thread]::Sleep(60000) }
    }
} elseif (-not $DryRun) {
    $why = @(Test-Free); if ($why.Count) { L "NOT free: $($why -join '; ')"; exit 2 }
}

$asiBak = $null
if ($Asi) {
    if (-not (Test-Path $Asi)) { L "ASI not found: $Asi"; exit 5 }
    if (Test-Path $binAsi) { $asiBak = Join-Path $out "bin_asi.orig"; Copy-Item $binAsi $asiBak -Force }
    Copy-Item $Asi $binAsi -Force
    L ("FC ASI installed into bin: sha256 " + (Get-FileHash $binAsi -Algorithm SHA256).Hash.Substring(0, 16))
}
Get-ChildItem $GameDir -Filter "Nioh3EngineFix_fcap_*.csv" -ErrorAction SilentlyContinue | Remove-Item -Force
$cfgBak = Join-Path $out "config.orig.xml"
Copy-Item $cfg $cfgBak -Force; $origSha = (Get-FileHash $cfgBak -Algorithm SHA256).Hash
$before = @(Get-ChildItem (Join-Path $ws "captures\tour") -Directory -ErrorAction SilentlyContinue | ForEach-Object Name)
$rc = -1
try {
    if (-not $DryRun) { Copy-Item $matched $cfg -Force; L "matched config.xml installed" }
    $keys = "Profiler.GPUProf=0;Profiler.FrameCap=1;Profiler.FrameCap_Frames=2;Profiler.FrameCap_TourDelay=$TourDelay;Profiler.FrameCap_Max=8;" +
            "Profiler.FrameCap_MaxEvents=600000;GpuOpt.Barriers=0;VRR.MaxFps=240"
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$RunTour`"", "-Input", "vpad", "-List", "`"$list`"", "-Points", "1",
           "-Passes", "1", "-DwellSec", "$DwellSec", "-NoPause", "-ExtraKeys", "`"$keys`"")
    if ((Get-Content $RunTour -Raw) -match '\$LockKey') { $a += @("-LockKey", $LockKey) }
    if ($DryRun) { $a += "-DryRun" }
    L "run: $RunTour $($a[5..($a.Count-1)] -join ' ')"
    $p = Start-Process powershell -ArgumentList $a -PassThru -Wait -WindowStyle Hidden
    $rc = $p.ExitCode; L "run_tour exit $rc"
} finally {
    if (-not $DryRun) {
        Copy-Item $cfgBak $cfg -Force
        $now = (Get-FileHash $cfg -Algorithm SHA256).Hash
        L ("config.xml restored: " + $(if ($now -eq $origSha) { "sha256 OK" } else { "SHA MISMATCH ($now vs $origSha)" }))
    }
    if ($Asi) { if ($asiBak) { Copy-Item $asiBak $binAsi -Force } else { Remove-Item $binAsi -Force -ErrorAction SilentlyContinue }; L "bin ASI restored" }
}

$csv = @(Get-ChildItem $GameDir -Filter "Nioh3EngineFix_fcap_*.csv" -ErrorAction SilentlyContinue)
foreach ($f in $csv) { Move-Item $f.FullName $out -Force }
L "captures: $($csv.Count) file(s)"
$new = Get-ChildItem (Join-Path $ws "captures\tour") -Directory -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.Name } | Sort-Object Name | Select-Object -Last 1
if ($new) {
    foreach ($f in @(Get-ChildItem (Join-Path $new.FullName "logs") -Filter *.log -ErrorAction SilentlyContinue)) { Copy-Item $f.FullName $out -Force }
    foreach ($n in "presentmon.csv", "ini_used.ini", "tour_points.csv", "tour_report.md", "run_tour.log") {
        $f = Join-Path $new.FullName $n; if (Test-Path $f) { Copy-Item $f $out -Force }
    }
}
$dig = Join-Path $out "fcap_digest.md"
if ($csv.Count) { & py -I (Join-Path $ws "re\scripts_renderer\fcap_digest.py") (Join-Path $out "Nioh3EngineFix_fcap_*.csv") --device $Device 2>&1 | Set-Content $dig; L "digest: $dig" }
Set-Content (Join-Path $out "device.txt") "device=$Device`nstamp=$stamp`nrun_tour_exit=$rc`ncaptures=$($csv.Count)"
$zip = Join-Path $ws "captures\fcap\fcap1-$Device-$stamp.zip"
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip -Force
if (-not $NoPost -and -not $DryRun) {
    $put = Bridge @("uo_bridge", "put", $zip, "--channel", "nioh3", "--note", "FCAP1 D3D12 frame captures, $Device (notes/renderer/00-map.md 4)")
    L "bridge put: $put"
    $null = Bridge @("uo_bridge", "send", "nioh3", "[RESULT] $Device FCAP1 frame capture done (run_tour exit $rc, $($csv.Count) capture file(s)). Zip $(Split-Path $zip -Leaf) on the bridge (file above); digest inside (fcap_digest.md).")
}
L "done"
