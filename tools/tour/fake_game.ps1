<#
  fake_game.ps1 - stand-in for Nioh3.exe in run_tour.ps1 -DryRun. Writes the mod log lines the Tour mode writes (installed, ARMED,
  stage loads, "Tour P" points with GP / SP windows, heartbeat, DONE) and the progress file with the same resume rules as
  src\tour_core.h. Launch 1 "crashes" during stage 2 point 2 after scribbling on the fake save (the wrapper must detect and restore
  it); launch 2 resumes, skips the crashed point and finishes. Then it idles until it is killed, like the real game.
#>
param([string]$GameDir, [string]$SaveRoot, [int]$Launch = 1)
$log = Join-Path $GameDir "Nioh3EngineFix.log"
$tourDir = Join-Path $GameDir "Nioh3EngineFix_tour"; New-Item -ItemType Directory -Force $tourDir | Out-Null
$prog = Join-Path $tourDir "progress.txt"
Set-Content $log "" -NoNewline
function L([string]$s) { Add-Content $log ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss.fff"), $s) }
function P([string]$s) { Add-Content $prog $s }
function IniGet([string]$key, [string]$def) {
    $m = Select-String -Path (Join-Path $GameDir "Nioh3EngineFix.ini") -Pattern "^\s*$key\s*=\s*(.*)$" | Select-Object -First 1
    if ($m) { $v = $m.Matches[0].Groups[1].Value.Trim(); if ($v -ne "") { return $v } }
    return $def
}
$points = [int](IniGet "TourPointsPerStage" "3"); $passes = [int](IniGet "TourPasses" "1"); $listName = IniGet "TourList" ""
$keys = @(7356, 52, 7292)
if ($listName) { $keys = @(Get-Content (Join-Path $GameDir $listName) | ForEach-Object { ($_ -split "#")[0].Trim() } | Where-Object { $_ } | ForEach-Object { [int]($_ -split "@")[0] }) }
$keys = $keys[0..([Math]::Min(3, $keys.Count) - 1)]   # dry run: the first 4 stages are enough
L "Nioh3EngineFix 1.7.36 (dry run fake game, launch $Launch)"
# resume (same rules as tour_core.h ParseProgress)
$done = @{}; $stageDone = @{}; $open = $null
if (Test-Path $prog) {
    foreach ($ln in Get-Content $prog) {
        $f = $ln -split "\s+"
        switch ($f[0]) {
            "B" { $open = "$($f[1]) $($f[2]) $($f[3])" }
            "D" { $done["$($f[1]) $($f[2]) $($f[3])"] = 1; if ($open -eq "$($f[1]) $($f[2]) $($f[3])") { $open = $null } }
            "C" { $done["$($f[1]) $($f[2]) $($f[3])"] = 1; if ($open -eq "$($f[1]) $($f[2]) $($f[3])") { $open = $null } }
            "S" { $stageDone["$($f[1]) $($f[2])"] = 1 }
            "A" { if ($f[4] -ne "ok") { $stageDone["$($f[1]) $($f[2])"] = 1 } }
        }
    }
}
if ($open) { $done[$open] = 1; $o = $open -split " "; L ("Tour: RESUME - the previous run ended during pass {0} stage {1} point {2} (crash or kill); that point is skipped" -f ([int]$o[0] + 1), ([int]$o[1] + 1), ([int]$o[2] + 1)); P "C $open" }
L "Tour: installed - mode 1 (unlocked missions), $($keys.Count) stages, SAVES BLOCKED for this whole session"
Start-Sleep -Milliseconds 800
L "Tour: ARMED (mode 1, $($keys.Count) stages x $passes pass(es), $points points/stage, dwell 4 s); saves blocked (SB1-SB3 + guard)"
$rnd = New-Object Random (11 + $Launch)
$priv = 6400.0 + 30 * $Launch
for ($pass = 0; $pass -lt $passes; $pass++) {
    for ($st = 0; $st -lt $keys.Count; $st++) {
        if ($stageDone["$pass $st"]) { continue }
        $k = $keys[$st]
        P "L $pass $st $k"; L ("Tour: stage {0}/{1} pass {2} - loading mission {3} (unlocked 1, cleared 1, slot 0)" -f ($st + 1), $keys.Count, ($pass + 1), $k)
        Start-Sleep -Milliseconds 300
        $load = 20 + $rnd.Next(0, 15)
        L ("Tour: stage {0} arrived: request -> field {1}.0 s (incl. 3 s settle), mission now {2}, region 150, at 1 2 3" -f ($st + 1), $load, $k)
        P "A $pass $st $k ok"
        for ($pt = 0; $pt -lt $points; $pt++) {
            if ($done["$pass $st $pt"]) { continue }
            P "B $pass $st $pt"
            $t0 = [DateTimeOffset]::Now.ToUnixTimeMilliseconds()
            if ($Launch -eq 1 -and $st -eq 1 -and $pt -eq 1) {
                # simulated crash; the fake also writes into the save folder (the real mod blocks this) to test detect + restore
                $sv = Get-ChildItem (Join-Path $SaveRoot "Savedata") -Recurse -Filter SAVEDATA.BIN | Select-Object -First 1
                [IO.File]::AppendAllText($sv.FullName, "scribble")
                Set-Content (Join-Path $sv.DirectoryName "SAVEDATA.BIN.tmp") "tmp"
                L "Tour: hb state=dwell stage=2 pt=2 ticks=1234 (+1800) saves skipped user=0 backup=0 guard=0"
                exit 3
            }
            Start-Sleep -Milliseconds 250
            $gpu = 9.0 + $st * 1.5 + $rnd.NextDouble(); $fps = [Math]::Round(1000 / ($gpu + 1.5), 1); $p99 = [Math]::Round($gpu + 3 + $rnd.NextDouble() * 4 + $(if ($st -eq 2) { 9 } else { 0 }), 2)
            $priv += $rnd.Next(0, 20)
            $t1 = [DateTimeOffset]::Now.ToUnixTimeMilliseconds()
            $src = if ($pt -eq 0) { "spawn" } else { "warp" }
            L ("SP window tour {0}/{1}/{2}" -f ($pass + 1), ($st + 1), ($pt + 1))
            L "SP main [App]Game: 800 samples in 4 s from 1 thread(s), RIP in Nioh3.exe 91.0 % (the rest: system/driver code, attributed to the calling game function)"
            L ("SP main      {0:N1} %  fn 0x20B900" -f (10 + $st * 4))
            L "SP main       6.0 %  fn 0x379620"
            L "SP main       4.0 %  [ntdll.dll] <- fn 0x7D9E90"
            L "SP kidsTS     12.0 %  fn 0x469C0"
            L ("Tour P pass={0} stage={1}/{2} key={3} pt={4}/{5} src={6} warp={7} pos=1,2,3 mission={3} region=150 dwell=4.0 frames=240 fps={8} p50={9:N2} p99={10} max={11} hitch=0 cpu=6.1 main={12} private={13:F0} ws=7000 handles=2100 threads=120 syscommit=30000/60000 vram=7000/15000 nonlocal=500 load={14} warpS={15} int=0 t0={16} t1={17} label=Fake mission {3}" -f `
                ($pass + 1), ($st + 1), $keys.Count, $k, ($pt + 1), $points, $src, $(if ($pt) { 1000 + $pt } else { 0 }), $fps, ($gpu + 1.5), $p99, ($p99 + 5), (55 + $st * 10), $priv, $(if ($pt) { 0 } else { $load }), $(if ($pt) { 3.2 } else { 0 }), $t0, $t1)
            L ("GP: window tour {0}/{1}/{2}" -f ($pass + 1), ($st + 1), ($pt + 1))
            L ("GP: frame {0:N2} ms GPU (sum of top-level markers; first-to-last span {0:N2} ms), 900 markers/frame over 15 frames | begins 1, ends 1" -f $gpu)
            L ("GP:    1 {0,-44} incl {1,7:N3} ms  self {2,7:N3} ms     1.0/frame" -f "ShadowCascade", (2.0 + $st), (1.5 + $st * 0.8))
            L ("GP:    2 {0,-44} incl {1,7:N3} ms  self {2,7:N3} ms     1.0/frame" -f "VolumetricFog", 1.4, (1.2 + $(if ($st -eq 2) { 2.5 } else { 0 })))
            L ("GP:    3 {0,-44} incl {1,7:N3} ms  self {2,7:N3} ms     1.0/frame" -f "GBuffer", 2.2, 2.0)
            L ("GP2: frame {0:N2} ms GPU span (wall 16.00 ms); covered 8.00 ms (80 % of span); async-compute 0.50 ms; 300 intervals/frame" -f $gpu)
            L "GP2: top 2 of 50 (PSO shader set, marker) buckets - hash stage(s) | ms/frame | draws+dispatches/frame | intervals/frame | [marker]"
            L "GP2:    1 ps=1a2b3c4d5e6f | 1.250 ms | 40.0 | 4.0 | [GBuffer]"
            L "GP2:    2 cs=99887766aabb | 0.800 ms | 2.0 | 2.0 | [VolumetricFog]"
            P "D $pass $st $pt"
        }
        P "S $pass $st"
    }
}
L "Tour: DONE - all stages visited. Save writes skipped: user 3, backup 0; guard denials 0. Close the game now (the in-memory state is not a play state)."
P "E"
while ($true) { Start-Sleep -Seconds 1 }   # like the game: runs until it is closed
