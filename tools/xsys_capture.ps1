<#
  xsys_capture.ps1 - the matched cross-system GPU capture XSYS1 (notes\cross-system-compare.md), Windows side (PC; pc2 with
  -RunTour tools\tour\pc2_run_tour.ps1). The Ally runs the same spec with its own Linux harness.

  Spec (identical on every device):
    config.xml  configs\xsys-1080-high-fsr65.xml (1920x1080, custom HIGH, REN_RAT 65, in-game FSR 3.1, FG OFF, VSync OFF)
    tour        tools\tour\tourlist_xsys.txt = 7356 / 52 / 7292 / 5631, spawn point only (-Points 1), 2 passes, 30 s dwell
    GP2         GPUProf 2, GPUProf2_MinRun 1, GPUProf2_Top 150 (the ASI clamps to 100), SampleEvery 16, AB 0; SP 1 (run_tour default)
    VRR         MaxFps 240 (the 120 cap must never bind)
    shaders     ghdm REPLACED (shipped default), every other item ORIGINAL (printed, not changed, here)
    PresentMon  by run_tour (GPUBusy per point)
  Steps: optional -WaitFree (lock free + no game + no other harness for 3 checks in a row, 60 s apart), back up config.xml, install
  the matched config, run the tour (run_tour takes the lock under -LockKey), restore config.xml (sha256-verified), copy the outputs to
  captures\xsys\<device>-<stamp>\, zip + put on the bridge, post [RESULT]. If the other devices' outputs are already in
  captures\xsys\, runs tools\xsys_compare.py over all three.
  Run:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\xsys_capture.ps1 -WaitFree
#>
param(
    [string]$Device = "pc", [string]$LockKey = "nioh3-pc", [string]$RunTour = "",
    [int]$Passes = 2, [int]$DwellSec = 30,
    [switch]$WaitFree, [int]$WaitMaxMin = 480, [switch]$NoPost, [switch]$DryRun,
    [string]$UoBridge = (Join-Path $env:USERPROFILE "universal-optimizer\agent-bridge"),
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
)
$ErrorActionPreference = "Stop"
$ws = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $RunTour) { $RunTour = Join-Path $ws "tools\tour\run_tour.ps1" }
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$out = Join-Path $ws "captures\xsys\$Device-$stamp"
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out "xsys_capture.log"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l; Add-Content $log $l }
$env:UO_BRIDGE_TOKEN_FILE = Join-Path $env:APPDATA "uo-bridge\token"; $env:UO_AGENT_ID = $Device
function Bridge([string[]]$a) { Push-Location $UoBridge; try { (& py -m @a 2>&1 | Out-String).Trim() } finally { Pop-Location } }
$cfg = Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3\config.xml"
$matched = Join-Path $ws "configs\xsys-1080-high-fsr65.xml"
$list = Join-Path $ws "tools\tour\tourlist_xsys.txt"
$HarnessRx = 'run_tour|ab_driver|pc_gpu_session|pc_retest|abba2?_pc|final_pc|load_pc|cost_vs_looks|loadstats_ab|pc_weakcpu|run_pc\.ps1'

function Test-Free {
    $why = @()
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { $why += "Nioh3 running" }
    $st = Bridge @("uo_bridge.gamelock", "status", $LockKey)
    if ($st -notmatch "'holder':\s*None") { $why += "lock $LockKey held ($(([regex]::Match($st, "'holder':\s*'([^']*)'")).Groups[1].Value))" }
    $h = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe' or Name='pwsh.exe'" -ErrorAction SilentlyContinue |
           Where-Object { $_.ProcessId -ne $PID -and $_.CommandLine -match $HarnessRx })
    if ($h.Count) { $why += "harness running (pid $($h.ProcessId -join ','))" }
    $ml = Join-Path ([IO.Path]::GetTempPath()) "nioh3_measuring.lock"
    if (Test-Path $ml) { try { $j = Get-Content $ml -Raw | ConvertFrom-Json; if (Get-Process -Id $j.pid -ErrorAction SilentlyContinue) { $why += "measuring lock (pid $($j.pid))" } } catch {} }
    return $why
}

L "XSYS1 capture on $Device (lock $LockKey, $Passes passes x $DwellSec s, list $list)"
if ($WaitFree -and -not $DryRun) {
    $t0 = Get-Date; $ok = 0
    while ($ok -lt 3) {
        $why = @(Test-Free)
        if ($why.Count) { if ($ok -gt 0 -or ((Get-Date) - $t0).TotalMinutes % 15 -lt 1) { L "waiting: $($why -join '; ')" }; $ok = 0 } else { $ok++ }
        if (((Get-Date) - $t0).TotalMinutes -gt $WaitMaxMin) { L "gave up after $WaitMaxMin min"; exit 3 }
        if ($ok -lt 3) { [Threading.Thread]::Sleep(60000) }
    }
    L "machine free for 3 checks in a row - starting"
} elseif (-not $DryRun) {
    $why = @(Test-Free); if ($why.Count) { L "NOT free: $($why -join '; ')"; exit 2 }
}

# shader state (report only)
$tab = [regex]::Match([IO.File]::ReadAllText((Join-Path $ws "gpu-shaders\ally_shaders.sh")), '(?s)TABLE="(.*?)"').Groups[1].Value
$state = @{}; $hc = @{}
foreach ($ln in ($tab -split "`n")) {
    $c = @($ln.Trim() -split '\s+' | Where-Object { $_ }); if ($c.Count -lt 4) { continue }
    $p = Join-Path $GameDir "shader_dx12\$($c[1])"
    if (-not $hc.ContainsKey($p)) { $hc[$p] = if (Test-Path $p) { (Get-FileHash $p -Algorithm SHA256).Hash.ToLower() } else { "" } }
    $s = if ($hc[$p] -eq $c[2]) { "orig" } elseif ($hc[$p] -eq $c[3]) { "repl" } else { "other" }
    if (-not $state.ContainsKey($c[0]) -or $s -ne "orig") { $state[$c[0]] = $s }
}
$shaderLine = ($state.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join " "
L "shader items: $shaderLine"
if (($state.GetEnumerator() | Where-Object { $_.Name -ne "ghdm" -and $_.Value -ne "orig" }).Count) { L "WARNING: items other than ghdm are not ORIGINAL - rows of those shaders will not match across devices" }

# config swap
$cfgBak = Join-Path $out "config.orig.xml"
Copy-Item $cfg $cfgBak -Force; $origSha = (Get-FileHash $cfgBak -Algorithm SHA256).Hash
Copy-Item $matched (Join-Path $out "config.xsys.xml") -Force
$before = @(Get-ChildItem (Join-Path $ws "captures\tour") -Directory -ErrorAction SilentlyContinue | ForEach-Object Name)
$rc = -1
try {
    if (-not $DryRun) { Copy-Item $matched $cfg -Force; L "matched config.xml installed" }
    $keys = "Profiler.GPUProf=2;Profiler.GPUProf2_MinRun=1;Profiler.GPUProf2_Top=150;Profiler.GPUProf2_SampleEvery=16;Profiler.GPUProf2_AB=0;VRR.MaxFps=240"
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$RunTour`"", "-Input", "vpad", "-List", "`"$list`"", "-Points", "1",
           "-Passes", "$Passes", "-DwellSec", "$DwellSec", "-NoPause", "-ExtraKeys", "`"$keys`"")
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
L "tour capture: $($new.FullName)"
foreach ($f in @(Get-ChildItem (Join-Path $new.FullName "logs") -Filter *.log -ErrorAction SilentlyContinue)) { Copy-Item $f.FullName $out -Force }
foreach ($n in "presentmon.csv", "ini_used.ini", "tour_points.csv", "tour_report.md", "run_tour.log") {
    $f = Join-Path $new.FullName $n; if (Test-Path $f) { Copy-Item $f $out -Force }
}
Set-Content (Join-Path $out "device.txt") "device=$Device`nstamp=$stamp`nrun_tour_exit=$rc`nshaders=$shaderLine`nsource=$($new.FullName)"
$runlog = Get-ChildItem $out -Filter "run*.log" | Where-Object Name -match '^run\d+\.log$' | Sort-Object Name | Select-Object -First 1
$nwin = if ($runlog) { (Select-String -Path $runlog.FullName -Pattern "GP2: frame .*PSO stamps ON").Count } else { 0 }
L "GP2 windows in $($runlog.Name): $nwin"

$zip = Join-Path $ws "captures\xsys\xsys1-$Device-$stamp.zip"
Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip -Force
$cmp = ""
$mine = Join-Path $out $runlog.Name
$others = @{}
foreach ($d in "pc", "pc2", "ally") {
    if ($d -eq $Device) { continue }
    $o = Get-ChildItem (Join-Path $ws "captures\xsys") -Directory -Filter "$d-*" -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
    if ($o) { $r = Get-ChildItem $o.FullName -Filter "*.log" | Where-Object Name -notmatch 'xsys_capture' | Sort-Object Length -Descending | Select-Object -First 1; if ($r) { $others[$d] = $r.FullName } }
}
if ($others.Count -eq 2 -and $runlog) {
    $ins = @("--in", "pc2=$(if ($Device -eq 'pc2') { $mine } else { $others['pc2'] })", "--in", "pc=$(if ($Device -eq 'pc') { $mine } else { $others['pc'] })",
             "--in", "ally=$(if ($Device -eq 'ally') { $mine } else { $others['ally'] })")
    $cmpOut = Join-Path $out "xsys_compare.txt"
    & py -I (Join-Path $ws "tools\xsys_compare.py") @ins --top 40 2>&1 | Set-Content $cmpOut
    $cmp = " Comparison over all three: $cmpOut"
    L "xsys_compare written: $cmpOut"
}
if (-not $NoPost -and -not $DryRun) {
    $put = Bridge @("uo_bridge", "put", $zip, "--channel", "nioh3", "--note", "XSYS1 matched capture, $Device (notes/cross-system-compare.md)")
    L "bridge put: $put"
    $null = Bridge @("uo_bridge", "send", "nioh3", "[RESULT] $Device XSYS1 matched GPU capture done (run_tour exit $rc, $nwin GP2 windows, shaders: $shaderLine). Zip on the bridge (file above).$cmp")
}
L "done"
