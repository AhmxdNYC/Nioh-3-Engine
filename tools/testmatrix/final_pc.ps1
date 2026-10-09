<#
  final_pc.ps1 - final-benchmark driver for the Windows PC (notes\final-benchmark-protocol.md, "User additions").
  Cells = config x graphics preset (x run), plus an optional per-setting sweep. Per cell/run: set graphics (gfxlib.ps1), set the mod
  state, launch via Steam, reach gameplay, take the bridge game lock, capture PresentMon 2.x windows, restore everything.
  Run with: powershell -NoProfile -ExecutionPolicy Bypass -File final_pc.ps1 ...

  Configs (-Configs):
    STOCK         Nioh3EngineFix.asi moved aside, original shaders/swaps, FRAME_GENERATION=OFF
    OPTIMIZED     mod ALL-OFF + the matrix KEEP set (tm.py best on summary_pc.csv; -KeepSet low = BEST_lowrisk), KEEP shaders/swaps, FG OFF
    OPTIMIZED+FG  OPTIMIZED with FRAME_GENERATION=FSR (config.xml; Ally: never, FG crashes Proton)
  Presets (-Presets): USER (config.xml as found), MAX, HIGH, MID, LOW, A_MAX_MINUS_INVISIBLE, B_BALANCED, C_FLOOR (presets.json).
  Sweep (-Sweep cheap|ALL|GI,SSR, config -SweepConfig): each level of each key once on top of the base preset (MAX), -SweepRuns runs.
  Windows per run: STAND x2, MOVE x1, COMBAT x1 (needs -CombatScript), LATENCY x1 (-LatencySec, -Presses scripted clicks/keys; -NoLatency
  skips; sweeps never take it). LOAD2/hitches are manual (the protocol's door timing) and can be typed into the result json.
  Results: results\final_pc_<cfg>_<preset>[_<sweep>]_r<n>.json + captures\tm\final_*.csv, env snapshot results\final_pc_env.json.
  After the session:  -ReportOnly [-Hwinfo f.csv] [-Rtss f.csv]  ingests sensor logs, writes results_table_pc.md + latency_pc.md.

    -DryRun   print the plan, per-cell config.xml changes (read-only), PresentMon capabilities, estimated time; touches nothing.
  Order is interleaved (run -> preset -> config) so thermal drift hits all configs equally. Game lock, crash-safe restore of
  config.xml / the .asi / shaders in `finally`.
#>
param([switch]$DryRun, [switch]$Force, [switch]$ReportOnly, [switch]$NoLock, [switch]$NoLatency, [switch]$PcLatency,
      [string[]]$Configs = @("STOCK", "OPTIMIZED", "OPTIMIZED+FG"), [string[]]$Presets = @("USER", "MAX"),
      [string[]]$Sweep = @(), [string]$SweepConfig = "OPTIMIZED", [int]$SweepRuns = 1, [int]$Runs = 3,
      [int]$StandWindows = 2, [int]$WindowSec = 20, [int]$LatencySec = 30, [int]$Presses = 25, [ValidateSet("click", "key")][string]$PressKind = "click",
      [string]$KeepSet = "full", [string]$CombatScript = "", [string]$Hwinfo = "", [string]$Rtss = "", [string]$ConfigPath = "", [int]$OwnerPid = 0, [int]$MaxMinutes = 90)   # OwnerPid 0 = parent process; see harness_guard.ps1
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
foreach ($n in "Configs", "Presets", "Sweep") { Set-Variable $n @((Get-Variable $n).Value | ForEach-Object { $_ -split "," } | Where-Object { $_ }) }   # -File passes "a,b" as one string
$ws = "C:\Users\Ahmad Hamza\nioh3-engine"
. (Join-Path $here "gfxlib.ps1"); Init-Gfx $ConfigPath
$Device = "pc"; $GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$asi = Join-Path $GameDir "Nioh3EngineFix.asi"; $asiOff = "$asi.stockoff"
$ini = Join-Path $GameDir "Nioh3EngineFix.ini"; $logf = Join-Path $GameDir "Nioh3EngineFix.log"
$stateD = Join-Path $here "state"; $resD = Join-Path $here "results"; $capD = "$ws\captures\tm"
$iniOrig = Join-Path $stateD "Nioh3EngineFix.ini.orig"; $shState = Join-Path $stateD "shaders_installed.txt"; $swapAdded = Join-Path $stateD "swap_added.txt"
$pmExe = "$ws\tools\PresentMon.exe"
$uoBridge = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$M = Get-Content (Join-Path $here "matrix.json") -Raw | ConvertFrom-Json
$T = @{ launch = 210; window = ($WindowSec + 8); stats = 35; close = 25; shader = 8; combat = 40; swap = 10 }

function Slug([string]$s) { return ($s -replace '\+', 'plus' -replace '[^A-Za-z0-9=_.-]', '-') }
function Get-PmInfo {
    $o = (cmd /c "`"$pmExe`" --help 2>&1" | Out-String)      # PresentMon prints its banner on stderr
    $v = [regex]::Match($o, 'PresentMon (\d+\.\d+\.\d+)').Groups[1].Value
    $ver = if ($v) { [version]$v } else { [version]"0.0.0" }
    return [pscustomobject]@{ version = $v; v2 = ($o -match '--v2_metrics'); pcl = ($o -match '--track_pc_latency'); input = ($o -match '--no_track_input'); ok = ($ver -ge [version]"2.3.0") }
}
function New-Cells {
    $cells = New-Object System.Collections.Generic.List[object]
    foreach ($r in 1..$Runs) { foreach ($p in $Presets) { foreach ($c in $Configs) {
        $cells.Add([pscustomobject]@{ cfg = $c; preset = $p; sweep = $null; run = $r; latency = (-not $NoLatency) }) } } }
    if ($Sweep.Count) {
        $base = (Get-Preset $PM.base_preset)
        foreach ($r in 1..$SweepRuns) { foreach ($sc in (Get-SweepCells $Sweep)) { if ($sc.base) { continue }
            $cells.Add([pscustomobject]@{ cfg = $SweepConfig; preset = $PM.base_preset; sweep = @{ key = $sc.key; value = $sc.value }; run = $r; latency = $false }) } }
    }
    return $cells
}
function Get-CellId($c) { $s = if ($c.sweep) { "_SW-" + (Slug "$($c.sweep.key)=$($c.sweep.value)") } else { "" }; return "final_${Device}_$(Slug $c.cfg)_$(Slug $c.preset)${s}_r$($c.run)" }
function Get-CellGfx($c) {
    $kv = Get-Preset $c.preset
    if ($c.sweep) { $kv[$c.sweep.key] = $c.sweep.value }
    return $kv
}
function Test-Done($c) {
    if ($Force) { return $false }
    $f = Join-Path $resD ((Get-CellId $c) + ".json"); if (-not (Test-Path $f)) { return $false }
    return [bool]((Get-Content $f -Raw | ConvertFrom-Json).valid)
}
function Get-Est($c) {
    $s = $T.launch + $T.stats + $T.close + ($StandWindows + 1) * $T.window + $(if ($CombatScript) { $T.combat } else { 0 })
    if ($c.latency) { $s += $LatencySec + 8 }
    if ($c.cfg -ne "OPTIMIZED+FG" -and $c.cfg -ne "OPTIMIZED") { $s += $T.shader }
    return $s
}

# ---------------- plan / dry run ----------------
$cells = New-Cells
if ($ReportOnly) {
    if ($Hwinfo -or $Rtss) { $a = @("ingest", $resD, $Device); if ($Hwinfo) { $a += @("--hwinfo", $Hwinfo) }; if ($Rtss) { $a += @("--rtss", $Rtss) }; py -I "$here\tm.py" @a }
    py -I "$here\tm.py" table $resD $Device | Select-Object -Last 3; py -I "$here\tm.py" latency $resD $Device | Out-Null
    "-> $resD\results_table_$Device.md, $resD\latency_$Device.md"; return
}
if ($DryRun) {
    $pmi = Get-PmInfo
    "== DRY RUN: nothing launched, no file written. Device=$Device game=$GameDir"
    "   PresentMon: $pmExe -> version '$($pmi.version)'  --v2_metrics=$($pmi.v2)  input tracking=$($pmi.input)  --track_pc_latency=$($pmi.pcl)" + $(if ($pmi.ok) { "  (OK: AllInput/Click->photon columns come from --v2_metrics)" } else { "  (TOO OLD for latency columns: need >= 2.3, get the newest PresentMon 2.x release)" })
    "   MsPCLatency needs the game to emit PC Latency (Reflex/PCL) markers: " + $(if ($PcLatency) { "-PcLatency set: adds --track_pc_latency (beta)" } else { "not requested (add -PcLatency to try; blank column is expected if the game emits none)" })
    "   sensors: HWiNFO/RTSS logs are NOT started by this script (start the logging by hand, same clock); ingest with -ReportOnly -Hwinfo <csv> -Rtss <csv>"
    "   configs=$($Configs -join ',') presets=$($Presets -join ',') runs=$Runs sweep=$($Sweep -join ',') ($SweepConfig x$SweepRuns) keepset=$KeepSet"
    $haveSum = Test-Path (Join-Path $resD "summary_pc.csv")
    if (($Configs -contains "OPTIMIZED") -or ($Configs -contains "OPTIMIZED+FG")) {
        if ($haveSum) { "   KEEP set now (tm.py best): " + ((py -I "$here\tm.py" best $resD $Device (Join-Path $here "matrix.json") $(if ($KeepSet -eq "low") { "low" } else { "" })) -join " ; ") }
        else { "   !! results\summary_pc.csv missing: OPTIMIZED cells need the matrix run (run_pc.ps1) + report first; a real run would stop" }
    }
    $tot = 0; $i = 0; $todo = 0
    foreach ($c in $cells) {
        $i++; $done = Test-Done $c; $est = Get-Est $c
        $kv = Get-CellGfx $c
        $ch = @(); if (Test-Path $gfxCfg) { try { $ch = @(Apply-Gfx $kv $true) } catch { $ch = @("!! $_") } }
        $fg = if ($c.cfg -eq "OPTIMIZED+FG") { " FG=" + $PM.fg_key[2] } else { " FG=" + $PM.fg_key[1] }
        "{0,3}. {1,-30} {2,-13} {3,-22} {4,-5} {5,5}s lat={6} gfx[{7}]{8}" -f $i, (Get-CellId $c).Replace("final_pc_", ""), $c.cfg, $c.preset, $(if ($done) { "done" } else { "todo" }), $est, $c.latency, $ch.Count, $fg
        if ($ch.Count -and $ch.Count -le 12 -and ($i -le 6 -or $c.sweep)) { "       " + ($ch -join "; ") }
        if (-not $done) { $tot += $est; $todo++ }
    }
    "== {0} cells, {1} to do, estimated {2:N1} h. Presets USER/MAX vs the current config.xml shown as 'gfx[n changes]'." -f $cells.Count, $todo, ($tot / 3600)
    "   latency windows: $Presses scripted $PressKind presses in ${LatencySec}s; PresentMon args: --process_name Nioh3.exe --timed $LatencySec --terminate_after_timed --stop_existing_session --no_console_stats --v2_metrics$(if($PcLatency){' --track_pc_latency'})"
    return
}

# ---------------- real run (helpers duplicated from run_pc.ps1 on purpose: that script may be mid-matrix) ----------------
. "$ws\patcher\common.ps1"          # $ws, Ensure-InGame, Test-InGame, U, Close-Game
foreach ($d in $stateD, $resD, $capD) { New-Item -ItemType Directory -Force $d | Out-Null }
. "$here\harness_guard.ps1"
Init-Guard -Name "final_pc" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; if (Test-Path $asiOff) { Move-Item $asiOff $asi -Force }; Set-Shaders @(); Set-Swaps @(); if (Test-Path $iniOrig) { Copy-Item $iniOrig $ini -Force }; Restore-Gfx | Out-Null; Unlock-Game }
function Set-IniValue([string[]]$lines, [string]$sec, [string]$key, [string]$val) {
    $out = New-Object System.Collections.Generic.List[string]; $cur = ""; $done = $false; $secSeen = $false; $insAt = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $l = $lines[$i]
        if ($l -match '^\s*\[(.+?)\]') { if ($cur -eq $sec -and -not $done -and $insAt -lt 0) { $insAt = $out.Count }; $cur = $Matches[1]; if ($cur -eq $sec) { $secSeen = $true } }
        if ($cur -eq $sec -and -not $done -and $l -match ('^\s*' + [regex]::Escape($key) + '\s*=')) { $out.Add("$key = $val"); $done = $true; continue }
        $out.Add($l)
    }
    if (-not $done) { if ($secSeen) { if ($insAt -lt 0) { $insAt = $out.Count }; $out.Insert($insAt, "$key = $val") } else { $out.Add(""); $out.Add("[$sec]"); $out.Add("$key = $val") } }
    return $out.ToArray()
}
function Write-IniState($triples) {
    $lines = [IO.File]::ReadAllLines($iniOrig)
    $lines = Set-IniValue $lines "General" "AutoContinueLoad" "1"   # off by default since 1.7.33; later triples may override
    foreach ($t in $triples) { $lines = Set-IniValue $lines $t[0] $t[1] $t[2] }
    [IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding($false)))
}
function Invoke-Shader([string]$name, [bool]$revert) {
    $a = $M.shader_apply.$name
    $args2 = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$ws\gpu-shaders\$($a.ps1)")
    if ($a.item) { $args2 += @("-Item", $a.item) }
    if ($revert) { $args2 += "-Revert" }
    & powershell @args2 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "shader $name failed ($LASTEXITCODE)" }
}
function Set-Shaders([string[]]$want) {
    $have = if (Test-Path $shState) { @(Get-Content $shState | Where-Object { $_ }) } else { @() }
    foreach ($s in $have) { if ($want -notcontains $s) { Invoke-Shader $s $true } }
    foreach ($s in $want) { if ($have -notcontains $s) { Invoke-Shader $s $false } }
    Set-Content $shState $want
}
function Set-Swaps([string[]]$digests) {
    $dst = Join-Path $GameDir "Nioh3EngineFix_shaders"
    if (Test-Path $swapAdded) { foreach ($f in Get-Content $swapAdded) { if ($f) { Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $dst $f) } }; Remove-Item $swapAdded -Force }
    if ($digests.Count -eq 0) { return }
    New-Item -ItemType Directory -Force $dst | Out-Null
    foreach ($d in $digests) { Copy-Item "$ws\$($M.swap_dir)\$d.dxbc" $dst -Force; Add-Content $swapAdded "$d.dxbc" }
}
function Resolve-Keep {
    if (-not (Test-Path "$resD\summary_pc.csv")) { throw "results\summary_pc.csv missing: run the matrix (run_pc.ps1) first so a KEEP set exists" }
    $tr = New-Object System.Collections.Generic.List[object]; $sh = @(); $sw = @()
    foreach ($tx in $M.alloff) { $tr.Add(@($tx[0], $tx[1], $tx[2])) }
    foreach ($l in (py -I "$here\tm.py" best $resD $Device (Join-Path $here "matrix.json") $(if ($KeepSet -eq "low") { "low" } else { "" }))) {
        $p = "$l".Split("|")
        if ($p[0] -eq "SHADER") { $sh += $p[1] } elseif ($p[0] -eq "SWAP") { $sw += $p[1] } elseif ($p.Count -eq 3) { $tr.Add(@($p[0], $p[1], $p[2])) }
    }
    if ($sw.Count) { $tr.Add(@("ShaderSwap", "Enabled", "1")) }
    return @{ triples = $tr; shaders = $sh; swaps = $sw }
}
function Lock-Game { if ($NoLock) { return }; Push-Location $uoBridge; try { py -m uo_bridge.gamelock acquire nioh3 --ttl 5400 | Out-Null; if ($LASTEXITCODE -ne 0) { throw "game lock held by another device" } } finally { Pop-Location } }
function Renew-Lock { if (-not $NoLock) { Push-Location $uoBridge; try { py -m uo_bridge.gamelock renew nioh3 --ttl 5400 | Out-Null } finally { Pop-Location } } }
function Unlock-Game { if (-not $NoLock) { Push-Location $uoBridge; try { py -m uo_bridge.gamelock release nioh3 | Out-Null } finally { Pop-Location } } }
function Start-GameToPlay {
    Close-Game; Assert-Launch; $t0 = Get-Date
    Start-Process "steam://rungameid/$($M.steam_id)"
    while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0) -and ((Get-Date) - $t0).TotalSeconds -lt 180) { $cc = "$(& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'steam_cloud_click.ps1') 2>$null | Select-Object -First 1)"; if ($cc -like 'clicked*') { Write-Host "[launch] Steam cloud dialog: $cc" }; Start-Sleep 3 }   # steam_cloud_click: 'Unable to Sync' -> Play anyway (user OK, bridge #602)
    Register-Game; if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { Note-Crash "no game window after launch" }
    Start-Sleep 25
    U focus Nioh3; U click 1280 887; Start-Sleep 22
    U focus Nioh3; U click 1280 1170; Start-Sleep 7
    U focus Nioh3; U click 380 1024; Start-Sleep 25
    U focus Nioh3; U key 20; Start-Sleep 5; U key 20; Start-Sleep 4
    return @{ ok = (Ensure-InGame); boot_s = [int]((Get-Date) - $t0).TotalSeconds }
}

# scripted input for the latency window: real mouse/keyboard events via user32 (fast, no dotnet startup per press)
Add-Type -Namespace W32 -Name In -MemberDefinition @'
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, System.UIntPtr e);
[System.Runtime.InteropServices.DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, System.UIntPtr e);
'@
function Send-Press { if ($PressKind -eq "click") { [W32.In]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 40; [W32.In]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero) }
                      else { [W32.In]::keybd_event(0x20, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 40; [W32.In]::keybd_event(0x20, 0, 2, [UIntPtr]::Zero) } }

function Capture-Window([string]$id, [string]$kind, [int]$n) {
    $csv = "$capD\${id}_${kind}${n}.csv"
    if (-not (Ensure-InGame)) { return @{ kind = $kind; ok = $false; why = "not in gameplay" } }
    $sec = if ($kind -eq "latency") { $LatencySec } else { $WindowSec }
    $a = "--process_name Nioh3.exe --output_file `"$csv`" --timed $sec --terminate_after_timed --stop_existing_session --no_console_stats --v2_metrics"
    if ($kind -eq "latency" -and $PcLatency) { $a += " --track_pc_latency" }
    $t0 = Get-Date
    $pmp = Start-Process $pmExe -PassThru -WindowStyle Hidden -ArgumentList $a; Register-Child $pmp "PresentMon"
    Start-Sleep 2
    if ($kind -eq "move") { Push-Location "$ws\tools"; U focus Nioh3; for ($k = 0; $k -lt 4; $k++) { U pan 12 220 10; U pan -12 220 10 }; Pop-Location }
    elseif ($kind -eq "combat" -and $CombatScript) { & $CombatScript }
    elseif ($kind -eq "latency") { U focus Nioh3; $gap = [int](($LatencySec - 4) * 1000 / $Presses); for ($k = 0; $k -lt $Presses; $k++) { Send-Press; Start-Sleep -Milliseconds $gap } }
    else { Start-Sleep ($sec - 1) }
    $pmp.WaitForExit(($sec + 20) * 1000) | Out-Null
    $t1 = Get-Date
    if (-not (Test-InGame)) { return @{ kind = $kind; ok = $false; why = "left gameplay" } }
    $s = (py -I "$here\tm.py" stats $csv) | ConvertFrom-Json
    $w = [ordered]@{ kind = $kind; ok = [bool]$s.ok; t0 = $t0.ToString("s"); t1 = $t1.ToString("s"); csv = $csv }
    if ($s.ok) { foreach ($p in $s.PSObject.Properties) { $w[$p.Name] = $p.Value } }
    return $w
}
function Measure-Cell($c, [string]$id) {
    $wins = @()
    for ($n = 1; $n -le $StandWindows; $n++) { $wins += Capture-Window $id "stand" $n }
    $wins += Capture-Window $id "move" 1
    if ($CombatScript) { $wins += Capture-Window $id "combat" 1 }
    if ($c.latency) { $wins += Capture-Window $id "latency" 1 }
    Start-Sleep $T.stats
    return $wins
}

# env snapshot (driver, PresentMon, game files) once per session
$drv = (Get-CimInstance Win32_VideoController | Select-Object -First 1)
[ordered]@{ when = (Get-Date -f s); gpu = $drv.Name; driver = $drv.DriverVersion; presentmon = (Get-PmInfo).version; game_exe_mtime = (Get-Item "$GameDir\Nioh3.exe").LastWriteTime.ToString("s")
            os = (Get-CimInstance Win32_OperatingSystem).Version; configs = $Configs; presets = $Presets; keepset = $KeepSet; fps_cap = "REN_FPS in config.xml"
            note = "also record: HWiNFO/RTSS version, power plan, ambient temp" } | ConvertTo-Json | Set-Content "$resD\final_pc_env.json" -Encoding UTF8

if (Test-Path $iniOrig) { Copy-Item $iniOrig $ini -Force } else { Copy-Item $ini $iniOrig -Force }
Backup-Gfx | Out-Null
Lock-Game
try {
    foreach ($c in $cells) {
        $id = Get-CellId $c
        if (Test-Done $c) { "skip (done) $id"; continue }
        "=== $id $(Get-Date -f HH:mm:ss)"
        Renew-Lock
        try {
            Close-Game
            if (Test-Path $asiOff) { Move-Item $asiOff $asi -Force }                  # always start from "mod present"
            Copy-Item $gfxOrig $gfxCfg -Force
            $null = Apply-Gfx (Get-CellGfx $c) $false
            $null = Set-FgMode $(if ($c.cfg -eq "OPTIMIZED+FG") { $PM.fg_key[2] } else { $PM.fg_key[1] }) $false
            if ($c.cfg -eq "STOCK") {
                Set-Shaders @(); Set-Swaps @(); Copy-Item $iniOrig $ini -Force
                Move-Item $asi $asiOff -Force
            } else {
                $k = Resolve-Keep; Set-Shaders $k.shaders; Set-Swaps $k.swaps; Write-IniState $k.triples
            }
            $boot = Start-GameToPlay
            $wins = @(); if ($boot.ok) { $wins = Measure-Cell $c $id }
            py -I "$here\tm.py" logtail $logf 0 "$resD\$id.log" | Out-Null
            $valid = ($boot.ok -and (@($wins | Where-Object { $_.kind -eq "stand" -and $_.ok }).Count -ge 1))
            [ordered]@{ id = $id; device = $Device; cfg = $c.cfg; preset = $c.preset; sweep = $c.sweep; run = $c.run; when = (Get-Date -f s); load1_s = $boot.boot_s
                        load2_s = $null; hitches = $null; valid = $valid; windows = $wins } | ConvertTo-Json -Depth 6 | Set-Content "$resD\$id.json" -Encoding UTF8
            "    -> valid=$valid " + (($wins | Where-Object { $_.ok } | ForEach-Object { "$($_.kind)=$($_.fps)fps" + $(if ($_.lat_aip) { "/aip $($_.lat_aip)ms" }) }) -join "  ")
        } catch { "!! $id failed: $_" }
        finally { Close-Game; if (Test-Path $asiOff) { Move-Item $asiOff $asi -Force }; Copy-Item $iniOrig $ini -Force; if (Test-Path $gfxOrig) { Copy-Item $gfxOrig $gfxCfg -Force } }
    }
} finally {
    Close-Game
    if (Test-Path $asiOff) { Move-Item $asiOff $asi -Force }
    Set-Shaders @(); Set-Swaps @()
    if (Test-Path $iniOrig) { Copy-Item $iniOrig $ini -Force }
    Restore-Gfx | Out-Null
    Unlock-Game
    Done-Guard
}
if ($Hwinfo -or $Rtss) { $a = @("ingest", $resD, $Device); if ($Hwinfo) { $a += @("--hwinfo", $Hwinfo) }; if ($Rtss) { $a += @("--rtss", $Rtss) }; py -I "$here\tm.py" @a }
py -I "$here\tm.py" table $resD $Device | Select-Object -Last 1; py -I "$here\tm.py" latency $resD $Device | Out-Null
