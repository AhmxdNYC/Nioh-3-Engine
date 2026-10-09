<#
  vs_capture.ps1 - VS1 matched VRAM / streaming capture (notes\root-cause-vram-streaming.md). Windows side (PC; pc2 with
  -RunTour tools\tour\pc2_run_tour.ps1). Same tour spots as XSYS1 so the numbers line up with the GP2 data.

  Spec: config.xml = configs\xsys-1080-high-fsr65.xml (1080p, custom HIGH, REN_RAT 65, FSR 3.1, FG OFF, VSync OFF);
        tour tools\tour\tourlist_xsys.txt, -Points 3 (spawn = real level / dense, warps = light), 1 pass, 20 s dwell, -NoProfile;
        keys Profiler.MemStats=2 (ASI >= 1.7.48: 'stats vs:' line every second), Profiler.TileStats=1, VRR.MaxFps=240.
  Output: captures\vs\<device>-<stamp>\ (run01.log, presentmon.csv, tour_points.csv, ...), zip on the bridge, [RESULT] post.
  Digest: py -I re\scripts_gpu\vs_digest.py <run01.log> [presentmon.csv]
  Run:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\vs_capture.ps1 [-Device pc2 -RunTour tools\tour\pc2_run_tour.ps1]
#>
param(
    [string]$Device = "pc", [string]$LockKey = "nioh3-pc", [string]$RunTour = "",
    [int]$Passes = 1, [int]$DwellSec = 20, [int]$Points = 3, [switch]$NoPost, [switch]$DryRun,
    # -GP2: GPUProf 2 MinRun 1 like XSYS1 (per-kernel ms for the PCIe vs contention split); -Keys: extra "Section.Key=Value;..." (e.g. "Profiler.VSForceSysUpload=1")
    [switch]$GP2, [string]$Keys = "", [string]$Tag = "",
    [string]$UoBridge = (Join-Path $env:USERPROFILE "universal-optimizer\agent-bridge")
)
$ErrorActionPreference = "Stop"
$ws = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $RunTour) { $RunTour = Join-Path $ws "tools\tour\run_tour.ps1" }
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$sfx = if ($Tag) { "-$Tag" } else { "" }
$out = Join-Path $ws "captures\vs\$Device-$stamp$sfx"
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out "vs_capture.log"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l; Add-Content $log $l }
$env:UO_BRIDGE_TOKEN_FILE = Join-Path $env:APPDATA "uo-bridge\token"; $env:UO_AGENT_ID = $Device
function Bridge([string[]]$a) { Push-Location $UoBridge; try { (& py -m @a 2>&1 | Out-String).Trim() } finally { Pop-Location } }
$cfg = Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3\config.xml"
$matched = Join-Path $ws "configs\xsys-1080-high-fsr65.xml"
$list = Join-Path $ws "tools\tour\tourlist_xsys.txt"
if (-not $DryRun -and (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { L "Nioh3 is running - refusing"; exit 2 }

L "VS1 capture on $Device (lock $LockKey, points $Points, passes $Passes x $DwellSec s)"
$cfgBak = Join-Path $out "config.orig.xml"
Copy-Item $cfg $cfgBak -Force; $origSha = (Get-FileHash $cfgBak -Algorithm SHA256).Hash
$before = @(Get-ChildItem (Join-Path $ws "captures\tour") -Directory -ErrorAction SilentlyContinue | ForEach-Object Name)
$rc = -1
try {
    if (-not $DryRun) { Copy-Item $matched $cfg -Force; L "matched config.xml installed" }
    $keys = "Profiler.MemStats=2;Profiler.TileStats=1;VRR.MaxFps=240"
    if ($GP2) { $keys += ";Profiler.GPUProf=2;Profiler.GPUProf2_MinRun=1;Profiler.GPUProf2_Top=150;Profiler.GPUProf2_SampleEvery=16;Profiler.GPUProf2_AB=0" }
    if ($Keys) { $keys += ";" + $Keys }
    L "keys: $keys"
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$RunTour`"", "-Input", "vpad", "-List", "`"$list`"", "-Points", "$Points",
           "-Passes", "$Passes", "-DwellSec", "$DwellSec", "-NoPause", "-ExtraKeys", "`"$keys`"")
    if (-not $GP2) { $a += "-NoProfile" }
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
}
$new = Get-ChildItem (Join-Path $ws "captures\tour") -Directory -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.Name } | Sort-Object Name | Select-Object -Last 1
if (-not $new) { L "no tour capture folder found"; exit 4 }
foreach ($f in @(Get-ChildItem (Join-Path $new.FullName "logs") -Filter *.log -ErrorAction SilentlyContinue)) { Copy-Item $f.FullName $out -Force }
foreach ($n in "presentmon.csv", "ini_used.ini", "tour_points.csv", "tour_report.md", "run_tour.log") {
    $f = Join-Path $new.FullName $n; if (Test-Path $f) { Copy-Item $f $out -Force }
}
$runlog = Get-ChildItem $out -Filter "run*.log" | Where-Object Name -match '^run\d+\.log$' | Sort-Object Name | Select-Object -First 1
$nvs = if ($runlog) { (Select-String -Path $runlog.FullName -Pattern "stats vs:").Count } else { 0 }
L "tour capture: $($new.FullName); 'stats vs:' lines: $nvs"
$dig = ""
if ($runlog) {
    $pm = Join-Path $out "presentmon.csv"
    $da = @("-I", (Join-Path $ws "re\scripts_gpu\vs_digest.py"), $runlog.FullName); if (Test-Path $pm) { $da += $pm }
    & py @da 2>&1 | Set-Content (Join-Path $out "vs_digest.txt")
    $dig = " Digest: " + ((Get-Content (Join-Path $out "vs_digest.txt") -TotalCount 6) -join " / ")
}
$zip = Join-Path $ws "captures\vs\vs1-$Device-$stamp$sfx.zip"
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip -Force
if (-not $NoPost -and -not $DryRun) {
    $put = Bridge @("uo_bridge", "put", $zip, "--channel", "nioh3", "--note", "VS1 VRAM/streaming capture, $Device (notes/root-cause-vram-streaming.md)")
    L "bridge put: $put"
    $null = Bridge @("uo_bridge", "send", "nioh3", "[RESULT] $Device VS1 VRAM/streaming capture $Tag done (keys $keys; run_tour exit $rc, $nvs 'stats vs:' lines). Zip on the bridge.$dig")
}
L "done"
