<#
  run_pc.ps1 - unattended experiment matrix for Nioh 3 on the Windows PC (final phase).
  Reads matrix.json; per item: set ini/shader state (backed up, always restored), launch via Steam, reach gameplay,
  take the bridge game lock, PresentMon windows (stand 2x20 s + move 20 s), collect the mod's stats lines, restore.
  Results: tools\testmatrix\results\pc_<id>.json (+ .log, .csv captures in captures\tm\), then summary_pc.csv + verdict_pc.md.

    .\run_pc.ps1 -DryRun                      # print the plan and estimated duration, touch nothing
    .\run_pc.ps1                              # run everything not finished yet (resume)
    .\run_pc.ps1 -Only W6,GF,S_gi             # just these ids (BASE is added automatically)
    .\run_pc.ps1 -Groups 2,3                  # only these matrix groups (0 BASE,1 launch CPU,2 live,3 code bytes,4 GPU,6 shaders,7 swaps,8 measure,10 combos)
    .\run_pc.ps1 -Force                       # redo finished items
    .\run_pc.ps1 -LiveMenu                    # batch the live-switchable items in ONE launch via menu_map.json (see below)
    .\run_pc.ps1 -ReportOnly                  # just rebuild summary/verdict from results
  -LiveMenu needs tools\testmatrix\menu_map.json (generated from the mod's menu.cpp: `py -I menu_map.py -o menu_map.json`, see its header):
  per item the ui.fsx `keyseq` tokens (open F10, N Down/Up from the parked "Close menu" row, Enter = toggle, back to Close, Enter).
  After EACH toggle the mod log must show `MENU: <label> -> ON|OFF` (the expected state); otherwise that item is aborted and the rest
  of the batch is skipped (the menu cursor position is no longer known). Live items without a map entry relaunch individually.
  Never launches while the Ally holds the game: takes `py -m uo_bridge.gamelock acquire nioh3` first (skip with -NoLock for tests).
#>
param([switch]$DryRun, [switch]$Force, [switch]$LiveMenu, [switch]$ReportOnly, [switch]$NoLock,
      [string[]]$Only = @(), [int[]]$Groups = @(), [string]$Preset = "USER", [string]$CombatScript = "",
      [int]$StandWindows = 2, [int]$WindowSec = 20, [double]$NoiseFloor = 0.5, [switch]$AllowRisk)
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
. "C:\Users\Ahmad Hamza\nioh3-engine\patcher\common.ps1"        # $ws, Ensure-InGame, Test-InGame, U, Close-Game
$Device  = "pc"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$ini     = Join-Path $GameDir "Nioh3EngineFix.ini"
$logf    = Join-Path $GameDir "Nioh3EngineFix.log"
$stateD  = Join-Path $here "state";   $resD = Join-Path $here "results"; $capD = "$ws\captures\tm"
$iniOrig = Join-Path $stateD "Nioh3EngineFix.ini.orig"; $shState = Join-Path $stateD "shaders_installed.txt"
$swapAdded = Join-Path $stateD "swap_added.txt"
$uoBridge = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$M = Get-Content (Join-Path $here "matrix.json") -Raw | ConvertFrom-Json

# ---------- timing model (seconds), used by the dry run ----------
$T = @{ launch = 210; window = ($WindowSec + 8); stats = 35; close = 20; shader = 8; combat = 40; load = 70; menuItem = 105 }

# ---------- ini helpers ----------
function Set-IniValue([string[]]$lines, [string]$sec, [string]$key, [string]$val) {
    $out = New-Object System.Collections.Generic.List[string]; $cur = ""; $done = $false; $secSeen = $false; $insAt = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $l = $lines[$i]
        if ($l -match '^\s*\[(.+?)\]') { if ($cur -eq $sec -and -not $done -and $insAt -lt 0) { $insAt = $out.Count }; $cur = $Matches[1]; if ($cur -eq $sec) { $secSeen = $true } }
        if ($cur -eq $sec -and -not $done -and $l -match ('^\s*' + [regex]::Escape($key) + '\s*=')) { $out.Add("$key = $val"); $done = $true; continue }
        $out.Add($l)
    }
    if (-not $done) {
        if ($secSeen) { if ($insAt -lt 0) { $insAt = $out.Count }; $out.Insert($insAt, "$key = $val") }
        else { $out.Add(""); $out.Add("[$sec]"); $out.Add("$key = $val") }
    }
    return $out.ToArray()
}
function Write-IniState($triples) {
    $lines = [IO.File]::ReadAllLines($iniOrig)
    foreach ($t in $triples) { $lines = Set-IniValue $lines $t[0] $t[1] $t[2] }
    [IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding($false)))
}
function Get-Triples($it, [bool]$liveBatch = $false) {
    $r = New-Object System.Collections.Generic.List[object]
    foreach ($t in $M.alloff) { $r.Add(@($t[0], $t[1], $t[2])) }
    foreach ($t in $it.needs) { $r.Add(@($t[0], $t[1], $t[2])) }
    foreach ($t in $it.ini)   { $v = $t[2]; if ($liveBatch -and $it.live_installed_value) { $v = $it.live_installed_value }; $r.Add(@($t[0], $t[1], $v)) }
    return $r
}

# ---------- shader helpers ----------
function Invoke-Shader([string]$name, [bool]$revert) {
    $a = $M.shader_apply.$name
    $args2 = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$ws\gpu-shaders\$($a.ps1)")
    if ($a.item) { $args2 += @("-Item", $a.item) }
    if ($revert) { $args2 += "-Revert" }
    & powershell @args2 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "shader $name $(if($revert){'revert'}else{'apply'}) failed ($LASTEXITCODE)" }
}
function Get-ShState { if (Test-Path $shState) { @(Get-Content $shState | Where-Object { $_ }) } else { @() } }
function Set-Shaders([string[]]$want) {
    $have = Get-ShState
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

# ---------- plan ----------
function Test-Applies($it) {
    if ($it.enabled -eq $false) { return $false }
    if ($it.devices -notcontains $Device) { return $false }
    if ($it.wine_only -or $it.handheld_only) { return $false }
    if ($it.id -eq "BASE" -or ($Groups.Count -eq 0 -and $Only.Count -eq 0)) { return $true }
    return (($Groups -contains [int]$it.grp) -or ($Only -contains $it.id))     # -Only and -Groups are OR-ed
}
function Get-Plan {
    $items = @(); foreach ($g in 0..10) { $items += @($M.items | Where-Object { [int]$_.grp -eq $g -and (Test-Applies $_) }) }
    $plan = New-Object System.Collections.Generic.List[object]
    $base = $items | Where-Object { $_.id -eq "BASE" } | Select-Object -First 1
    $rest = @($items | Where-Object { $_.id -ne "BASE" })
    $combos = @($rest | Where-Object { $_.type -eq "combo" }); $rest = @($rest | Where-Object { $_.type -ne "combo" })
    $plan.Add(@{ id = "BASE"; it = $base; res = "BASE" })
    $half = [math]::Ceiling($rest.Count / 2); $n = 0
    if ($LiveMenu -and (Test-Path (Join-Path $here "menu_map.json"))) {
        $mm = Get-Content (Join-Path $here "menu_map.json") -Raw | ConvertFrom-Json
        $live = @($rest | Where-Object { $_.mode -eq "live" -and $mm.items.($_.id) }); $rest = @($rest | Where-Object { $_ -notin $live })
        $half = [math]::Ceiling($rest.Count / 2)
        foreach ($it in $rest) { if ($n -eq $half) { $plan.Add(@{ id = "BASE#2"; it = $base; res = "BASE#2" }) }; $plan.Add(@{ id = $it.id; it = $it; res = $it.id }); $n++ }
        if ($live.Count) { $plan.Add(@{ id = "LIVE-BATCH"; batch = $live; res = "LIVE-BATCH" }) }
    } else {
        foreach ($it in $rest) { if ($n -eq $half) { $plan.Add(@{ id = "BASE#2"; it = $base; res = "BASE#2" }) }; $plan.Add(@{ id = $it.id; it = $it; res = $it.id }); $n++ }
    }
    $plan.Add(@{ id = "BASE#3"; it = $base; res = "BASE#3" })
    foreach ($c in $combos) { $plan.Add(@{ id = $c.id; it = $c; res = $c.id }) }
    return $plan
}
function Test-Done([string]$id) {
    if ($Force) { return $false }
    $f = Join-Path $resD "${Device}_$id.json"; if (-not (Test-Path $f)) { return $false }
    $j = Get-Content $f -Raw | ConvertFrom-Json; return [bool]$j.valid
}
function Get-Est($e) {
    if ($e.batch) { return $T.launch + $T.stats + $T.close + $e.batch.Count * $T.menuItem + 2 * $T.window }
    $it = $e.it; $s = $T.launch + $T.stats + $T.close + ($StandWindows + 1) * $T.window
    if ($it.type -eq "shader" -or $it.type -eq "swap") { $s += $T.shader }
    if ($it.combat) { $s += $T.combat }; if ($it.load) { $s += $T.load }
    return $s
}

$plan = Get-Plan
if ($DryRun) {
    "== DRY RUN: nothing is launched or changed. Device=$Device Preset=$Preset game=$GameDir"
    "   windows: stand ${StandWindows}x${WindowSec}s + move 1x${WindowSec}s (+combat/load where flagged: combat needs -CombatScript, else skipped)"
    $tot = 0; $todo = 0; $i = 0
    foreach ($e in $plan) {
        $i++; $done = Test-Done $e.res; $est = Get-Est $e
        $what = if ($e.batch) { "LIVE batch: " + (($e.batch | ForEach-Object id) -join ",") }
                elseif ($e.it.type -eq "shader") { "shaders=" + ($e.it.shaders -join "+") } elseif ($e.it.type -eq "swap") { "swap=" + ($e.it.swap -join "+").Substring(0, 8) + ".." }
                elseif ($e.it.type -eq "combo") { "combo from verdict_pc.md KEEPs" } elseif ($e.it.type -eq "baseline") { "all-off" }
                else { ($e.it.ini | ForEach-Object { "$($_[0]).$($_[1])=$($_[2])" }) -join " " }
        $mode = if ($e.batch) { "menu" } elseif ($e.it.mode) { $e.it.mode } else { "launch" }
        $flag = ""; if ($e.it.risk -eq "high") { $flag = " RISK-HIGH" }
        "{0,3}. {1,-12} {2,-6} {3,-8} {4,5}s {5}{6}  {7}" -f $i, $e.id, $mode, $(if ($done) { "done" } else { "todo" }), $est, $what, $flag, $(if ($e.it.label) { "- " + $e.it.label } else { "" })
        if (-not $done) { $tot += $est; $todo++ }
    }
    "== {0} entries, {1} to do, estimated {2:N1} h ({3} min). Without -LiveMenu every live item relaunches." -f $plan.Count, $todo, ($tot / 3600), [int]($tot / 60)
    $liveN = @($M.items | Where-Object { $_.mode -eq "live" -and (Test-Applies $_) }).Count
    $save = [int](($liveN * ($T.launch + $T.stats + $T.close + ($StandWindows + 1) * $T.window) - ($T.launch + $liveN * $T.menuItem)) / 60)
    "   -LiveMenu would batch $liveN live items into one launch (needs menu_map.json), saving about $save min."
    if ($LiveMenu -and (Test-Path (Join-Path $here "menu_map.json"))) {
        $mm = Get-Content (Join-Path $here "menu_map.json") -Raw | ConvertFrom-Json
        "   menu_map.json (device $($mm.device), ini $($mm.ini)): init=$(@($mm.init).Count) tokens"
        foreach ($b in @($M.items | Where-Object { $_.mode -eq "live" -and (Test-Applies $_) })) {
            $m = $mm.items.($b.id)
            if ($m) { "     {0,-9} row#{1,-2} '{2}'  start {3}, {4} tokens, verify 'MENU: {2} -> ON|OFF'" -f $b.id, $m.index, $m.label, $m.start, @($m.keys).Count }
            else { "     {0,-9} no map entry -> relaunch ({1})" -f $b.id, $mm.skipped.($b.id) }
        }
    }
    $skipped = @($M.items | Where-Object { -not (Test-Applies $_) } | ForEach-Object id)
    "   not on this device / disabled / filtered: " + ($skipped -join " ")
    return
}

foreach ($d in $stateD, $resD, $capD) { New-Item -ItemType Directory -Force $d | Out-Null }
if ($ReportOnly) { py -I "$here\tm.py" report $resD $Device --noise-floor $NoiseFloor; return }

# ---------- live menu (menu_map.json) ----------
function Get-LogLen { if (Test-Path $logf) { (Get-Item $logf).Length } else { 0 } }
function Read-LogFrom([long]$off) {
    $fs = [IO.File]::Open($logf, "Open", "Read", "ReadWrite"); try { $null = $fs.Seek($off, "Begin"); $sr = New-Object IO.StreamReader($fs); return $sr.ReadToEnd() } finally { $fs.Dispose() }
}
function Test-MenuLine([string]$txt, [string]$label, [string]$want) { return ($txt -match ("MENU: " + [regex]::Escape($label) + " -> " + $want + "")) }
# one menu toggle + verification against the mod log line "MENU: <label> -> ON|OFF"; $false = mismatch / no line
function Invoke-MenuToggle($map, [string]$id, [string]$want) {
    $m = $map.items.$id
    if (-not $m) { "    !! menu_map has no entry for $id"; return $false }
    $off = Get-LogLen
    U keyseq @($m.keys)
    for ($k = 0; $k -lt 8; $k++) { Start-Sleep -Milliseconds 500; if (Test-MenuLine (Read-LogFrom $off) $m.label $want) { return $true } }
    "    !! $id : no 'MENU: $($m.label) -> $want' in the mod log after the toggle (item aborted)"
    return $false
}

# ---------- real run ----------
function Lock-Game {
    if ($NoLock) { return }
    Push-Location $uoBridge
    try { py -m uo_bridge.gamelock acquire nioh3 --ttl 5400 | Out-Null; if ($LASTEXITCODE -ne 0) { throw "game lock held by another device (Ally?) - ask it to release" } } finally { Pop-Location }
}
function Renew-Lock { if (-not $NoLock) { Push-Location $uoBridge; try { py -m uo_bridge.gamelock renew nioh3 --ttl 5400 | Out-Null } finally { Pop-Location } } }
function Unlock-Game { if (-not $NoLock) { Push-Location $uoBridge; try { py -m uo_bridge.gamelock release nioh3 | Out-Null } finally { Pop-Location } } }

function Start-GameToPlay {
    Close-Game
    $t0 = Get-Date
    Start-Process "steam://rungameid/$($M.steam_id)"
    while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0) -and ((Get-Date) - $t0).TotalSeconds -lt 180) { Start-Sleep 3 }
    Start-Sleep 25
    # same click path as tools\enter_game.ps1 (autosave notice -> title -> Continue -> tip -> popup)
    U focus Nioh3; U click 1280 887; Start-Sleep 22
    U focus Nioh3; U click 1280 1170; Start-Sleep 7
    U focus Nioh3; U click 380 1024; Start-Sleep 25
    U focus Nioh3; U key 20; Start-Sleep 5; U key 20; Start-Sleep 4
    $ok = Ensure-InGame
    return @{ ok = $ok; boot_s = [int]((Get-Date) - $t0).TotalSeconds }
}
function Capture-Window([string]$id, [string]$kind, [int]$n) {
    $csv = "$capD\${Device}_${id}_${kind}${n}.csv"
    if (-not (Ensure-InGame)) { return @{ kind = $kind; ok = $false; why = "not in gameplay" } }
    $pm = Start-Process "$ws\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList `
        "--process_name Nioh3.exe --output_file `"$csv`" --timed $WindowSec --terminate_after_timed --stop_existing_session --no_console_stats"
    Start-Sleep 2
    if ($kind -eq "move") {                      # camera sweep, same input path as patcher\bench.ps1
        Push-Location "$ws\tools"; U focus Nioh3
        for ($k = 0; $k -lt 4; $k++) { U pan 12 220 10; U pan -12 220 10 }
        Pop-Location
    } elseif ($kind -eq "combat" -and $CombatScript) { & $CombatScript } else { Start-Sleep ($WindowSec - 1) }
    $pm.WaitForExit(40000) | Out-Null
    if (-not (Test-InGame)) { return @{ kind = $kind; ok = $false; why = "left gameplay" } }
    $s = (py -I "$here\tm.py" stats $csv) | ConvertFrom-Json
    $w = @{ kind = $kind; ok = [bool]$s.ok }
    if ($s.ok) { $w.fps = $s.fps; $w.low1 = $s.low1; $w.p99 = $s.p99; $w.gpu_ms = $s.gpu_ms; $w.cpu_ms = $s.cpu_ms }
    return $w
}
function Measure-Item($it, [string]$resId, $boot) {
    $wins = @()
    for ($n = 1; $n -le $StandWindows; $n++) { $wins += Capture-Window $resId "stand" $n }
    $wins += Capture-Window $resId "move" 1
    if ($it.combat -and $CombatScript) { $wins += Capture-Window $resId "combat" 1 }
    if ($it.shot) { U shot "$capD\${Device}_${resId}_shot.png" 0 0 2560 1440 }
    Start-Sleep $T.stats                                      # let the mod print a 30 s stats window
    return $wins
}
function Save-Result($it, [string]$resId, $wins, $boot) {
    py -I "$here\tm.py" logtail $logf 0 "$resD\${Device}_${resId}.log" | Out-Null
    $valid = ($boot.ok -and (@($wins | Where-Object { $_.kind -eq "stand" -and $_.ok }).Count -ge 1))
    $o = [ordered]@{ id = $resId; device = $Device; type = $it.type; grp = $it.grp; risk = $it.risk; metric = $it.metric; shot = $it.shot
        preset = $Preset; when = (Get-Date -f s); boot_s = $boot.boot_s; load_s = $boot.boot_s; valid = $valid; windows = $wins }
    $o | ConvertTo-Json -Depth 6 | Set-Content "$resD\${Device}_${resId}.json" -Encoding UTF8
    "    -> $resId valid=$valid " + (($wins | Where-Object { $_.ok } | ForEach-Object { "$($_.kind)=$($_.fps)fps/p99 $($_.p99)" }) -join "  ")
}
function Resolve-Combo($it) {
    $low = if ($it.max_risk -eq "low") { "low" } else { "" }
    if (-not (Test-Path "$resD\summary_$Device.csv")) { throw "no summary_$Device.csv yet: run the singles and -ReportOnly first" }
    $tr = New-Object System.Collections.Generic.List[object]; $sh = @(); $sw = @()
    foreach ($l in (py -I "$here\tm.py" best $resD $Device (Join-Path $here "matrix.json") $low)) {
        $p = "$l".Split("|")
        if ($p[0] -eq "SHADER") { $sh += $p[1] } elseif ($p[0] -eq "SWAP") { $sw += $p[1] } elseif ($p.Count -eq 3) { $tr.Add(@($p[0], $p[1], $p[2])) }
    }
    return @{ triples = $tr; shaders = $sh; swaps = $sw }
}

# crash safety: if a previous run died with a modified ini, put the original back first
if (Test-Path $iniOrig) { Copy-Item $iniOrig $ini -Force } else { Copy-Item $ini $iniOrig -Force }
Lock-Game
try {
    foreach ($e in $plan) {
        if (Test-Done $e.res) { "skip (done) $($e.res)"; continue }
        if ($e.it.risk -eq "high" -and -not $AllowRisk -and $e.it.type -eq "asi" -and $e.it.id -ne "BASE") { "note: $($e.id) is risk=high (can corrupt / hang the GPU); a hang aborts this item only" }
        "=== $($e.res) $(Get-Date -f HH:mm:ss) $(if($e.it.label){$e.it.label})"
        Renew-Lock
        try {
            if ($e.batch) {
                $map = Get-Content (Join-Path $here "menu_map.json") -Raw | ConvertFrom-Json
                $tr = New-Object System.Collections.Generic.List[object]
                foreach ($tx in $M.alloff) { $tr.Add(@($tx[0], $tx[1], $tx[2])) }
                foreach ($b in $e.batch) { foreach ($tx in (Get-Triples $b $true)) { $tr.Add($tx) } }
                Set-Shaders @(); Set-Swaps @(); Write-IniState $tr
                $boot = Start-GameToPlay
                if ($boot.ok) {
                    U keyseq @($map.init)                # park the menu cursor on "Close menu" (fresh game: cursor on row 0)
                    Start-Sleep 1
                    $good = @()                          # items whose all-off toggle verified; once one fails the cursor is unknown -> stop
                    foreach ($b in $e.batch) {          # installed-on items start ON for value 1: toggle once to get all-off
                        if (-not $b.live_installed_value) { if (-not (Invoke-MenuToggle $map $b.id "OFF")) { "    !! menu desync, remaining live items not run"; break } }
                        $good += $b
                    }
                    if ($good.Count) {
                        $w0 = Measure-Item $good[0] "BASE-live" $boot; Save-Result $M.items[0] "BASE-live" $w0 $boot
                        foreach ($b in $good) {
                            if (-not (Invoke-MenuToggle $map $b.id "ON")) { "    !! $($b.id) not run; menu desync, rest of the batch skipped"; break }
                            $w = Measure-Item $b $b.id $boot; Save-Result $b $b.id $w $boot
                            if (-not (Invoke-MenuToggle $map $b.id "OFF")) { "    !! $($b.id) did not switch back off; rest of the batch skipped"; break }
                            Renew-Lock
                        }
                    }
                }
            } else {
                $it = $e.it; $triples = $null; $sh = @(); $sw = @()
                if ($it.type -eq "combo") { $r = Resolve-Combo $it; $sh = $r.shaders; $sw = $r.swaps
                    $triples = New-Object System.Collections.Generic.List[object]; foreach ($tx in $M.alloff) { $triples.Add(@($tx[0], $tx[1], $tx[2])) }; foreach ($tx in $r.triples) { $triples.Add($tx) } }
                else { $triples = Get-Triples $it; if ($it.shaders) { $sh = @($it.shaders) }; if ($it.swap) { $sw = @($it.swap) } }
                if ($sw.Count) { $triples.Add(@("ShaderSwap", "Enabled", "1")) }
                Set-Shaders $sh; Set-Swaps $sw; Write-IniState $triples
                $boot = Start-GameToPlay
                $wins = @(); if ($boot.ok) { $wins = Measure-Item $it $e.res $boot }
                Save-Result $it $e.res $wins $boot
            }
        } catch { "!! $($e.res) failed: $_"; "$($_.ScriptStackTrace)" }
        finally { Close-Game; Copy-Item $iniOrig $ini -Force }
        py -I "$here\tm.py" report $resD $Device --noise-floor $NoiseFloor | Out-Null
    }
} finally {
    Close-Game
    Set-Shaders @(); Set-Swaps @()
    if (Test-Path $iniOrig) { Copy-Item $iniOrig $ini -Force }
    Unlock-Game
}
py -I "$here\tm.py" report $resD $Device --noise-floor $NoiseFloor
