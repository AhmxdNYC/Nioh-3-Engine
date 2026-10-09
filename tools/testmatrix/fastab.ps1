<#
  fastab.ps1 - ONE-LAUNCH A/B (the default way to A/B anything live-capable; notes\fast-testing.md).
  One launch through tools\tour\run_tour.ps1 -Input vpad (lock, save backup + restore, ini/ASI install + restore, title ->
  Continue by VirtualPad) on a one-stage tour list: the tour parks the player at the stage's spawn (-Stage, default 7292
  Mirage Fortress, the heaviest GPU-bound PC spawn) and dwells there. While it dwells this script runs, per item, -Pairs
  paired windows in ABBA order (A B | B A | A B | B A ...), each = toggle through the mod's LiveCtl control file
  (Nioh3EngineFix_live.ini, polled every 100 ms; the ack echoes Gen + read-backs) -> -WarmSec warm-up -> -WindowSec
  measuring window (UTC span -> PresentMon GPUBusy / frame time). fastab_report.py turns the windows into paired deltas
  with a standard error and a [RESULT]-ready table (report.md).
  Items (any mix, all in the same launch):
     Hot.<set>            shader-file A/B via ShaderHot twin PSOs (A = stock bytecode bound, B = replacement bound). The set is
                          built by hotset.py from the stock / replacement .file pair (-HotTable below, or -HotSets name=orig|repl)
     <MenuKey>            any live menu row (PP, B, GF, MQ, DQ, P1E7, VSMA, LPVR, AS, ...): A = 0, B = 1
     Tuning.<Key>=a/b     a live [Tuning] value (PP_Period, VSMA_K, VSMA_Front, LPVR_N, SKYR_V, FOGT_V, CLDR_V): A = a, B = b
     <MenuKey>=a/b        numeric menu row (R1=0/2, Lat=0/1 ...)
  Relaunch-required keys (live_keys.json 'relaunch') are refused: use abba2_pc.ps1 -Arms for those.
  Config: -UserConfig (default: the user's session config.xml) with FRAME_GENERATION OFF (-KeepFG keeps it), [VRR] MaxFps 240.
  -DryRun: no game, no Steam, no lock: a fake game answers the control file like the mod and writes a fake PresentMon CSV with
  a known effect -> the whole schedule + report path. Ends with 'DryRun OK'.
  Run:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\fastab.ps1 -Items Hot.ghdm -Tag ghdm1
#>
param(
    [string[]]$Items = @("Hot.ghdm"),
    [int]$Pairs = 4, [int]$WindowSec = 20, [double]$WarmSec = 5, [int]$SettleSec = 45,
    [string]$Stage = "7292",
    [string]$Tag = "fastab",
    [string]$UserConfig = "", [switch]$KeepFG, [switch]$NoConfig, [int]$MaxFps = 240,
    [string[]]$HotSets = @(),          # extra sets: name=<orig .file>|<repl .file>
    [string]$ShaderRoot = "C:\Users\Ahmad Hamza\nioh3-engine\gpu-shaders",
    [string[]]$ExtraKeys = @(),
    [string]$LockKey = "nioh3-pc",
    [int]$OwnerPid = 0, [int]$MaxMinutes = 90, [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$wsHere = (Resolve-Path (Join-Path $here "..\..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$Items = @($Items -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$HotSets = @($HotSets -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$ExtraKeys = @($ExtraKeys | Where-Object { $_ })
if ($DryRun) {
    $dry = Join-Path $env:TEMP "nioh3-fastab-dry-$stamp"
    $GameDir = Join-Path $dry "game"; $SaveRoot = Join-Path $dry "KoeiTecmo\NIOH3"
    $sess = Join-Path $dry "session"
    New-Item -ItemType Directory -Force $GameDir, $SaveRoot | Out-Null
    $SettleSec = 1; $WindowSec = 2; $WarmSec = 0.5
}
. (Join-Path $wsHere "tools\playsession\session_lib.ps1")   # $GameDir / $SaveRoot defaults, $PmExe, $BinAsi, Get-AsiVersion, Get-LockHolder ($ws = main repo)
if (-not $DryRun) { $sess = Join-Path $ws "captures\pc\fastab\$stamp-$Tag" }
if (-not $UserConfig) { $UserConfig = Join-Path $ws "captures\playsession\20261008-2059-session\config.xml" }
$runTour = Join-Path $ws "tools\tour\run_tour.ps1"
$reportPy = Join-Path $here "fastab_report.py"
$hotPy = Join-Path $here "hotset.py"
New-Item -ItemType Directory -Force $sess | Out-Null
$runLog = Join-Path $sess "fastab.log"
$manifest = Join-Path $sess "manifest.jsonl"
$ctl = Join-Path $GameDir "Nioh3EngineFix_live.ini"; $ack = "$ctl.ack"
$hotDir = Join-Path $GameDir "Nioh3EngineFix_hot"
$progress = Join-Path $GameDir "Nioh3EngineFix_tour\progress.txt"
$cfg = Join-Path $SaveRoot "config.xml"
$script:cfgBackup = $null; $script:gen = 0; $script:hotBuilt = $false

# stock / replacement pairs of the staged shader items (gpu-shaders\ally_shaders.sh TABLE; vcloud variants: notes\gpu-volumetrics-rewrite.md)
$HotTable = [ordered]@{
    ghdm = @("backup\0x0104840c.file", "replacement\0x0104840c.file")
    esm = @("backup\0x1ca8bb39.file", "replacement\0x1ca8bb39.file")
    wdo28 = @("backup\0x1fd07cc0.file", "replacement\0x1fd07cc0.file")
    mpc = @("backup\0x29ee0902.file", "replacement\0x29ee0902.file")
    mshlod = @("backup\0x8d9282e6.file", "replacement\0x8d9282e6.file")
    gi = @("backup\0xd070067d.file", "replacement\0xd070067d.file")
    fogpar = @("backup\0xa9910db8.file", "replacement\0xa9910db8.file")
    vcu = @("backup\0x5f17ca9d.file", "vcloud\out\u\0x5f17ca9d.file")
    vcp = @("backup\0x5f17ca9d.file", "vcloud\out\p\0x5f17ca9d.file")
    vcpfu = @("backup\0x5f17ca9d.file", "vcloud\out\pfu\0x5f17ca9d.file")
    vcpfp = @("backup\0x5f17ca9d.file", "vcloud\out\pfp\0x5f17ca9d.file")
}
foreach ($h in $HotSets) { if ($h -match '^([^=]+)=(.+)\|(.+)$') { $HotTable[$Matches[1].ToLower()] = @($Matches[2], $Matches[3]) } else { throw "bad -HotSets entry '$h' (name=orig|repl)" } }

function L([string]$s, [string]$c = "Gray") { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l -ForegroundColor $c; Add-Content $runLog $l }
function Read-Shared([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    try { $fs = [IO.File]::Open($path, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close(); return $t } catch { return "" }
}
function Run-Native([string]$exe, [object[]]$a) { $old = $ErrorActionPreference; $ErrorActionPreference = "Continue"; try { return (& $exe @a 2>&1 | ForEach-Object { "$_" } | Out-String) } finally { $ErrorActionPreference = $old } }
function Add-Jsonl($o) { Add-Content -Path $manifest -Value ($o | ConvertTo-Json -Compress -Depth 4) -Encoding UTF8 }
function Utc { (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ss.fffZ") }
function Ev([string]$name) { Add-Jsonl ([ordered]@{ kind = "event"; event = $name; t_utc = (Utc) }) }

# ---- items -------------------------------------------------------------------------------------------------------------------
function Parse-Item([string]$s) {
    $key = $s; $a = "0"; $b = "1"
    if ($s -match '^(.+?)=([^/]+)/(.+)$') { $key = $Matches[1]; $a = $Matches[2]; $b = $Matches[3] }
    $kind = if ($key -like "Hot.*") { "hot" } elseif ($key -like "Tuning.*") { "tuning" } else { "menu" }
    [pscustomobject]@{ id = $s; key = $key; a = $a; b = $b; kind = $kind; set = $(if ($kind -eq "hot") { $key.Substring(4).ToLower() } else { "" }) }
}
$plan = @($Items | ForEach-Object { Parse-Item $_ })
$lk = Join-Path $here "live_keys.json"
if (Test-Path $lk) {
    $cls = (Get-Content $lk -Raw | ConvertFrom-Json).keys
    foreach ($p in $plan | Where-Object { $_.kind -eq "menu" }) {
        $e = $cls | Where-Object { $_.key -eq $p.key } | Select-Object -First 1
        if ($e -and $e.class -ne "live") { throw "$($p.key) is '$($e.class)' in live_keys.json ($($e.why)) - not a one-launch item; use abba2_pc.ps1 -Arms" }
    }
}

# ---- control file / ack ------------------------------------------------------------------------------------------------------
$script:state = @{}   # key -> current value written
function Write-Control([string]$tag) {
    $script:gen++
    $t = "[Live]`r`nGen = $($script:gen)`r`nTag = $tag`r`n" + (($script:state.Keys | Sort-Object | ForEach-Object { "$_ = $($script:state[$_])" }) -join "`r`n") + "`r`n"
    $tmp = "$ctl.w"
    [IO.File]::WriteAllText($tmp, $t, [Text.Encoding]::ASCII)
    Move-Item -LiteralPath $tmp -Destination $ctl -Force
}
function Read-Ack {
    $h = @{}
    foreach ($kv in ((Read-Shared $ack).Trim() -split '\s+')) { if ($kv -match '^([^=]+)=(.*)$') { $h[$Matches[1]] = $Matches[2] } }
    return $h
}
function Wait-Ack([double]$sec = 6) {
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt $sec) { $a = Read-Ack; if ($a.gen -eq "$($script:gen)") { return $a }; Start-Sleep -Milliseconds 100 }
    return $null
}
function Set-Values([hashtable]$kv, [string]$tag) {   # -> ack hashtable with .good = all keys applied and read back
    foreach ($k in $kv.Keys) { $script:state[$k] = $kv[$k] }
    Write-Control $tag
    $a = Wait-Ack
    if (-not $a) { L "  no ack for gen $($script:gen) ($tag) within 6 s - is LiveCtl on?" Red; return @{ good = $false } }
    $bad = @($kv.Keys | Where-Object { $a[$_] -eq "err" -or -not $a.ContainsKey($_) })
    $a.good = ([int]$a.fail -eq 0 -and $bad.Count -eq 0)
    if (-not $a.good) { L "  ack gen $($a.gen) '$tag': ok $($a.ok) fail $($a.fail); bad: $($bad -join ',') (see the mod log 'LIVE:' line)" Red }
    return $a
}
function Dwell-Ended {   # a D / L / S / E progress line after the dwell's B line = the tour moved on
    $ls = @((Read-Shared $progress) -split "`r?`n" | Where-Object { $_ })
    $k = -1; for ($i = $ls.Count - 1; $i -ge 0; $i--) { if ($ls[$i] -match '^B ') { $k = $i; break } }
    if ($k -lt 0) { return $false }
    return [bool](@($ls | Select-Object -Skip ($k + 1) | Where-Object { $_ -match '^(D|L|S|E|A)\b' }).Count)
}

# ---- dry-run fake game: answers the control file like the mod, writes the tour progress lines ---------------------------------
function Start-FakeGame([int]$dwell) {
    $fake = Join-Path $dry "fake_fastab_game.ps1"
    @'
param([string]$GameDir, [int]$Dwell)
$ctl = Join-Path $GameDir "Nioh3EngineFix_live.ini"; $ack = "$ctl.ack"; $prog = Join-Path $GameDir "Nioh3EngineFix_tour\progress.txt"
New-Item -ItemType Directory -Force (Split-Path $prog) | Out-Null
Set-Content $prog ""; $last = ""; $script:a = "gen=- tag=- ok=0 fail=0"
Start-Sleep 2
Add-Content $prog "A 0 0 7292 ok"; Add-Content $prog "B 0 0 0"
$t0 = Get-Date; $frames = 0.0; $tPrev = Get-Date; $rng = New-Object Random 9
while (((Get-Date) - $t0).TotalSeconds -lt $Dwell) {
  $t = if (Test-Path $ctl) { [IO.File]::ReadAllText($ctl) } else { "" }
  $now = Get-Date; $ms = 8.5 - $(if ($t -match '(?m)^Hot\.\w+ = 1') { 0.4 } else { 0 }) + ($rng.NextDouble() - 0.5) * 0.2
  $frames += ($now - $tPrev).TotalMilliseconds / $ms; $tPrev = $now
  $clk = " frames=$([int64]$frames) qpc_us=$([int64](($now - $t0).TotalMilliseconds * 1000)) ft_sq_us=0 h20=0 h33=0 h50=0"
  if ($t -ne $last) { $last = $t
    $g = if ($t -match 'Gen = (\S+)') { $Matches[1] } else { "-" }; $tg = if ($t -match 'Tag = (\S+)') { $Matches[1] } else { "-" }
    $kv = ""; $ok = 0; $fail = 0
    foreach ($m in [regex]::Matches($t, '(?m)^(?!Gen|Tag)(\S+) = (\S+)')) { $v = $m.Groups[2].Value; if ($m.Groups[1].Value -eq "Hot.nope") { $v = "err"; $fail++ } else { $ok++ }; $kv += " $($m.Groups[1].Value)=$v" }
    $script:a = "gen=$g tag=$tg t_apply_unix_ms=0 age_ms=0 ok=$ok fail=$fail"; $script:kv = $kv }
  [IO.File]::WriteAllText("$ack.tmp", "$($script:a)$clk$($script:kv)"); Move-Item "$ack.tmp" $ack -Force
  Start-Sleep -Milliseconds 100
}
Add-Content $prog "D 0 0 0"; Add-Content $prog "E"
'@ | Set-Content $fake -Encoding UTF8
    return Start-Process powershell -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$fake`"", "-GameDir", "`"$GameDir`"", "-Dwell", "$dwell") -WindowStyle Hidden -PassThru
}
function Write-FakePm([string]$csv) {   # GPUBusy 8 ms, Hot.* B arm -0.4 ms, slow drift, noise
    $rng = New-Object Random 3
    $rows = @(Get-Content $manifest | ForEach-Object { $_ | ConvertFrom-Json } | Where-Object { $_.kind -eq "window" })
    $sb = New-Object Text.StringBuilder
    [void]$sb.AppendLine("Application,ProcessID,CPUStartDateTime,FrameTime,GPUBusy")
    $tStart = [datetime]::Parse($rows[0].t0_utc).ToUniversalTime()
    foreach ($r in $rows) {
        $t0 = [datetime]::Parse($r.t0_utc).ToUniversalTime(); $t1 = [datetime]::Parse($r.t1_utc).ToUniversalTime()
        for ($t = $t0; $t -lt $t1; $t = $t.AddMilliseconds(8)) {
            $g = 8.0 + 0.002 * ($t - $tStart).TotalSeconds + $(if ($r.arm -eq "B" -and $r.item -like "Hot.*") { -0.4 } else { 0 }) + ($rng.NextDouble() - 0.5) * 0.8
            [void]$sb.AppendLine("Nioh3.exe,1,$($t.ToString('yyyy-MM-dd HH:mm:ss.fff')),$([Math]::Round($g + 0.5, 3)),$([Math]::Round($g, 3))")
        }
    }
    [IO.File]::WriteAllText($csv, $sb.ToString())
}

# ---- config.xml ----------------------------------------------------------------------------------------------------------------
function Install-Config {
    if ($NoConfig -or $DryRun) { L "config.xml: unchanged"; return }
    if (-not (Test-Path $UserConfig)) { throw "user config not found: $UserConfig" }
    $script:cfgBackup = Join-Path $sess "config.xml.orig"; Copy-Item -LiteralPath $cfg -Destination $script:cfgBackup -Force
    $t = [IO.File]::ReadAllText($UserConfig)
    if (-not $KeepFG) { $t = $t -replace "<FRAME_GENERATION>[^<]*</FRAME_GENERATION>", "<FRAME_GENERATION>OFF</FRAME_GENERATION>" }
    [IO.File]::WriteAllText($cfg, $t, (New-Object Text.UTF8Encoding($false)))
    L ("config.xml = $UserConfig (" + ((@("SCR_REZ", "REN_RAT", "RESOLUTION_SCALING", "FRAME_GENERATION", "REN_FPS", "SHADOW_QUAL") | ForEach-Object { "$_ " + [regex]::Match($t, "<$_>([^<]*)<").Groups[1].Value }) -join ", ") + ")")
    $script:cfgText = ((@("SCR_REZ", "REN_RAT", "RESOLUTION_SCALING", "FRAME_GENERATION") | ForEach-Object { "$_ " + [regex]::Match($t, "<$_>([^<]*)<").Groups[1].Value }) -join ", ")
}
function Restore-Config {
    if (-not $script:cfgBackup) { return }
    Copy-Item -LiteralPath $script:cfgBackup -Destination $cfg -Force; L "config.xml restored" Green
    $script:cfgBackup = $null
}

# ---- hot sets ------------------------------------------------------------------------------------------------------------------
function Build-HotSets {
    $sets = @($plan | Where-Object { $_.kind -eq "hot" } | ForEach-Object { $_.set } | Select-Object -Unique)
    if (-not $sets.Count) { return }
    if (Test-Path $hotDir) { Remove-Item $hotDir -Recurse -Force }
    New-Item -ItemType Directory -Force $hotDir | Out-Null
    $script:hotBuilt = $true
    foreach ($s in $sets) {
        if ($DryRun) { New-Item -ItemType Directory -Force (Join-Path $hotDir $s) | Out-Null; continue }
        if (-not $HotTable.Contains($s)) { throw "no stock/replacement pair for hot set '$s' (add -HotSets $s=orig|repl)" }
        $o = $HotTable[$s][0]; $r = $HotTable[$s][1]
        if (-not [IO.Path]::IsPathRooted($o)) { $o = Join-Path $ShaderRoot $o }
        if (-not [IO.Path]::IsPathRooted($r)) { $r = Join-Path $ShaderRoot $r }
        $out = Run-Native py @("-I", $hotPy, "build", "--name", $s, "--orig", $o, "--repl", $r, "--game-dir", $GameDir, "--out", $hotDir)
        L ("hotset " + ($out.Trim() -replace "`r?`n", " | "))
        if ($LASTEXITCODE -ne 0) { throw "hotset.py failed for '$s'" }
    }
}

# ---- run_tour child ------------------------------------------------------------------------------------------------------------
function Start-Tour([int]$dwell, [string]$keys) {
    $cap = Join-Path $sess "tour"; New-Item -ItemType Directory -Force $cap | Out-Null
    if ($DryRun) { return @{ proc = (Start-FakeGame $dwell); cap = $cap; out = (Join-Path $cap "console.txt") } }
    $list = Join-Path $sess "tourlist.txt"; Set-Content $list "$Stage   # fastab stage (spawn point only)" -Encoding ASCII
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$runTour`"", "-Input", "vpad", "-List", "`"$list`"", "-Points", "1",
           "-DwellSec", "$dwell", "-NoProfile", "-NoPause", "-CaptureRoot", "`"$cap`"", "-ExtraKeys", "`"$keys`"", "-OwnerPid", "$PID",
           "-MaxMinutes", "$MaxMinutes", "-StallSec", "$($dwell + 300)", "-MaxRelaunch", "0", "-LockKey", $LockKey)
    $out = Join-Path $cap "console.txt"
    $p = Start-Process powershell -ArgumentList $a -WindowStyle Hidden -PassThru -RedirectStandardOutput $out -RedirectStandardError (Join-Path $cap "console_err.txt")
    return @{ proc = $p; cap = $cap; out = $out }
}

# =================================================================================================================================
$summary = [ordered]@{}
$tr = $null; $script:pmp = $null
try {
    $nWin = 2 * $Pairs
    $dwell = [int][Math]::Ceiling($SettleSec + $plan.Count * $nWin * ($WarmSec + $WindowSec + 1.5) + 45)
    if ($dwell -gt 1800) { throw "schedule needs a $dwell s dwell (> 1800 s, the TourDwellSec limit): fewer items or pairs per launch" }
    L "fastab $Tag$(if ($DryRun) { ' - DRY RUN' }): items $($plan.id -join ', '); $Pairs pairs (ABBA) x 2 windows x $WindowSec s + $WarmSec s warm-up; stage $Stage; dwell $dwell s; session $sess" Cyan
    $asiVer = if ($DryRun) { "dry" } else { Get-AsiVersion $BinAsi }
    if (-not $DryRun) {
        if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh 3 is running - quit it first" }
        $asiText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($BinAsi))
        if ($asiText -notmatch "LiveCtl") { throw "the ASI at $BinAsi has no LiveCtl (build >= the fast-testing commit)" }
        # several PC agents share the bridge id 'pc', so a 'pc' holder may be us (run_when_idle.ps1 takes the lock before it starts
        # this script): then the only guard is that no other harness runs. Any other holder (pc2 / ally) = refuse.
        $global:LockKey = $LockKey; $h = Get-LockHolder
        if ($h.holder -and $h.holder -ne "pc") { throw "the game lock '$LockKey' is held by '$($h.holder)' - wait for it (py -m uo_bridge.gamelock status $LockKey)" }
        $par = (Get-CimInstance Win32_Process -Filter "ProcessId=$PID").ParentProcessId
        $other = @(Get-CimInstance Win32_Process | Where-Object { $_.Name -eq 'powershell.exe' -and $_.ProcessId -ne $PID -and $_.ProcessId -ne $par -and
            $_.CommandLine -match 'fastab\.ps1|run_tour|abba2_pc|pc_gpu_session|cost_vs_looks|pc_weakcpu|weakcpu_stock|final_pc|pc_retest|cm_sim_pc|vrrp_ab_pc|load_pc' -and $_.CommandLine -notmatch 'run_when_idle' })
        if ($other.Count) { throw "another test harness is running (pid $($other[0].ProcessId)): $($other[0].CommandLine.Substring(0, [Math]::Min(120, $other[0].CommandLine.Length)))" }
        L "ASI $BinAsi = $asiVer (LiveCtl present); lock '$LockKey' $(if ($h.holder) { "held by 'pc' (assumed ours, no other harness running)" } else { 'free' })"
    }
    Install-Config
    Build-HotSets
    $hk = [ordered]@{}; if (Test-Path $hotDir) { foreach ($d in Get-ChildItem $hotDir -Directory) { $hk[$d.Name] = @(Get-ChildItem $d.FullName -File | ForEach-Object { $_.BaseName -replace "^(on|off)_", "" }) } }
    Add-Jsonl ([ordered]@{ kind = "meta"; tag = $Tag; device = "pc"; asi = $asiVer; stage = $Stage; windows_per_item = $nWin; window_s = $WindowSec; warm_s = $WarmSec; items = @($plan.id); config = "$($script:cfgText)"; extra = ($ExtraKeys -join ";"); hot_keys = $hk })
    Remove-Item -LiteralPath $ctl, $ack -Force -ErrorAction SilentlyContinue
    if (-not $DryRun) { Remove-Item -LiteralPath $progress -Force -ErrorAction SilentlyContinue }
    # boot values: every item at A (Hot sets start in the installed state; the first Set-Values puts them at A)
    $keys = @("Debug.LiveCtl=1", "Debug.LiveCtlFile=Nioh3EngineFix_live.ini", "General.AutoContinueLoad=1", "Overlay.FpsCounter=0", "Overlay.FrameTime=0",
              "Profiler.GPUProf=0", "Profiler.SP=0", "Patches.GIMerge=0")
    if ($MaxFps -gt 0) { $keys += "VRR.MaxFps=$MaxFps" }
    if (@($plan | Where-Object { $_.kind -eq "hot" }).Count) { $keys += @("ShaderHot.Enabled=1", "ShaderHot.Folder=Nioh3EngineFix_hot") }
    $mx = @{}; try { foreach ($it in (Get-Content (Join-Path $here "matrix.json") -Raw | ConvertFrom-Json).items) { $mx[$it.id] = $it } } catch {}
    $baseTxt = if (Test-Path (Join-Path $GameDir "Nioh3EngineFix.ini")) { [IO.File]::ReadAllText((Join-Path $GameDir "Nioh3EngineFix.ini")) } else { "" }
    foreach ($p in $plan | Where-Object { $_.kind -eq "menu" }) {   # a live row that is not installed in the play ini: install it for the run
        if ($baseTxt -notmatch "(?m)^$([regex]::Escape($p.key))\s*=\s*[12]") { $keys += "Patches.$($p.key)=$(if ($mx[$p.key] -and $mx[$p.key].live_installed_value -eq '2') { 2 } else { 1 })" }
    }
    $keys += $ExtraKeys
    L "ini keys: $($keys -join ';')"
    Ev "launch"
    $tr = Start-Tour $dwell ($keys -join ";")
    # wait for the dwell
    $t0 = Get-Date
    while ($true) {
        if ($tr.proc.HasExited) { throw "run_tour ended before the dwell started (see $($tr.cap))" }
        if (-not $DryRun -and -not $script:pmp -and (Get-Process Nioh3 -ErrorAction SilentlyContinue) -and (Test-Path $PmExe)) {
            $pa = "--process_name Nioh3.exe --output_file `"$(Join-Path $sess 'presentmon_fastab.csv')`" --v2_metrics --date_time --stop_existing_session --session_name Nioh3FastAB --no_console_stats"
            $script:pmp = Start-Process $PmExe -ArgumentList $pa -WindowStyle Hidden -PassThru
        }
        if ((Read-Shared $progress) -match '(?m)^B \d+ \d+ \d+') { break }
        if (((Get-Date) - $t0).TotalMinutes -gt 15) { throw "no tour dwell within 15 min" }
        Start-Sleep -Milliseconds 500
    }
    Ev "field"
    L "dwell started $([int]((Get-Date) - $t0).TotalSeconds) s after the launch - settle $SettleSec s" Green
    $init = @{}; foreach ($p in $plan) { $init[$p.key] = $p.a }
    $a = Set-Values $init "init_all_A"
    if (-not $a.good) { throw "LiveCtl did not apply the initial values (ack: $(Read-Shared $ack))" }
    Start-Sleep -Seconds $SettleSec
    Ev "first_window"
    $ended = $false
    foreach ($p in $plan) {
        L "item $($p.id): $Pairs pairs, A = $($p.a), B = $($p.b)" Cyan
        for ($q = 0; $q -lt $Pairs -and -not $ended; $q++) {
            $order = if ($q % 2 -eq 0) { @("A", "B") } else { @("B", "A") }
            foreach ($arm in $order) {
                $v = if ($arm -eq "A") { $p.a } else { $p.b }
                $wtag = "{0}_q{1}_{2}" -f ($p.key -replace '[^\w.]', '_'), $q, $arm
                $ak = Set-Values @{ $p.key = $v } $wtag
                Start-Sleep -Milliseconds ([int]($WarmSec * 1000))
                $w0 = Utc; $c0 = Read-Ack   # the mod's frame clock (LiveCtl ack: frames / qpc_us / hitch counters, refreshed every 250 ms)
                Start-Sleep -Seconds $WindowSec
                $w1 = Utc; $c1 = Read-Ack
                $ended = Dwell-Ended
                if (-not $DryRun -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "the game exited during $wtag" }
                $clk = [ordered]@{}
                foreach ($k in "frames", "qpc_us", "ft_sq_us", "h20", "h33", "h50") { if ($c0.ContainsKey($k) -and $c1.ContainsKey($k)) { $clk["mod_$k"] = [double]$c1[$k] - [double]$c0[$k] } }
                Add-Jsonl ([ordered]@{ kind = "window"; item = $p.id; key = $p.key; pair = $q; arm = $arm; value = $v; a_value = $p.a; b_value = $p.b
                    tag = $wtag; gen = $script:gen; t0_utc = $w0; t1_utc = $w1; ack_ok = [bool]$ak.good; readback = $(if ($ak.ContainsKey($p.key)) { $ak[$p.key] } else { "" })
                    warm_s = $WarmSec; dwell_ended = $ended; clock = $clk })
                L ("  {0}: {1} = {2} (ack gen {3} {4})" -f $wtag, $p.key, $v, $ak.gen, $(if ($ak.good) { "ok" } else { "NOT OK" }))
                if ($ended) { L "  the tour dwell ended - schedule cut here (raise -SettleSec margin / fewer items)" Yellow; break }
            }
        }
        $null = Set-Values @{ $p.key = $p.a } "$($p.key)_back_to_A"
    }
    Ev "last_window"
    L "schedule done - waiting for the tour to finish the dwell and close the game"
    if (-not $tr.proc.HasExited) { $tr.proc.WaitForExit(([int]$dwell + 600) * 1000) | Out-Null }
    Ev "end"
    $o = Read-Shared $tr.out
    $vd = [regex]::Match($o, "verdict: ([\w-]+)")
    $summary["tour"] = if ($vd.Success) { $vd.Groups[1].Value } elseif ($DryRun) { "dry" } else { "?" }
} catch {
    L "!! $_" Red
    $summary["error"] = "$_"
    if ($tr -and -not $tr.proc.HasExited) { L "waiting for run_tour to close the game and restore (it owns the game)"; $tr.proc.WaitForExit(900000) | Out-Null }
} finally {
    if ($script:pmp -and -not $script:pmp.HasExited) {
        try { Start-Process $PmExe -ArgumentList "--terminate_existing_session --session_name Nioh3FastAB" -WindowStyle Hidden -Wait } catch {}
        Start-Sleep 2; try { Stop-Process -Id $script:pmp.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
    Remove-Item -LiteralPath $ctl, "$ctl.w", $ack -Force -ErrorAction SilentlyContinue
    if ($script:hotBuilt -and (Test-Path $hotDir)) { Remove-Item $hotDir -Recurse -Force -ErrorAction SilentlyContinue; L "hot sets removed from the game folder" }
    try { Restore-Config } catch { L "!! config restore: $_" Red }
    $tc = @(Get-ChildItem (Join-Path $sess "tour") -Directory -Filter "*-tour" -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1)
    if ($tc.Count) { foreach ($lg in @(Get-ChildItem (Join-Path $tc[0].FullName "logs") -Filter "run*.log" -ErrorAction SilentlyContinue)) { Copy-Item $lg.FullName (Join-Path $sess "mod_$($lg.Name)") -Force } }
    if ($DryRun -and (Test-Path $manifest)) { Write-FakePm (Join-Path $sess "presentmon_fastab.csv") }
    if (Test-Path $manifest) {
        $rep = Run-Native py @("-I", $reportPy, $sess)
        Set-Content (Join-Path $sess "report_console.txt") $rep -Encoding UTF8
        $res = @($rep -split "`r?`n" | Where-Object { $_ -match '^\[RESULT\]' })
        $summary["result"] = $res -join " || "
        L "report: $(Join-Path $sess 'report.md')"
        $res | ForEach-Object { L $_ Green }
    }
    $para = "fastab $Tag ($(Get-Date -f 'yyyy-MM-dd HH:mm')): " + (($summary.GetEnumerator() | ForEach-Object { "$($_.Key): $($_.Value)." }) -join " ") + " Folder: $sess"
    Set-Content (Join-Path $sess "summary.txt") $para -Encoding UTF8
    if ($DryRun) {
        $chk = @()
        $rows = @(if (Test-Path $manifest) { Get-Content $manifest | ForEach-Object { $_ | ConvertFrom-Json } | Where-Object { $_.kind -eq "window" } })
        if ($rows.Count -ne $plan.Count * 2 * $Pairs) { $chk += "manifest has $($rows.Count) window rows (want $($plan.Count * 2 * $Pairs))" }
        if (@($rows | Where-Object { -not $_.ack_ok }).Count) { $chk += "unacknowledged toggles" }
        $rp = Join-Path $sess "report.md"
        if (-not (Test-Path $rp)) { $chk += "no report.md" }
        elseif ((Get-Content $rp -Raw) -notmatch "Hot\.\w+ 0->1: fps \+[45]\.\d+ % .*frame time -0.[34]") { $chk += "report misses the fake -0.4 ms effect" }
        if ($summary.Contains("error")) { $chk += "error: $($summary['error'])" }
        if ($chk.Count) { L "DryRun FAILED: $($chk -join '; ')" Red } else { L "DryRun OK (session $sess)" Green }
    }
}
