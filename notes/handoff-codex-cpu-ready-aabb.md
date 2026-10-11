# One next pc2 CPU test: AABB, stable1.7.61

Lead follow-up 2026-10-11 after user recording finished: native source-path
control compiled and executed PASS. It reproduces uninstalled/skipped-install
ACK success with only INI preference and no live cave, then7 exact installed/OFF
set/readbacks (including no-ops), unrelated DQ/P1E7/MA unchanged and missing
row/transport fail-closed checks. Selected-source SHA256
`1bb1d7809f6a296e9171a1b7b1024c76a3096f6270d24e03178a2e3a6935d1c3`.
The runner initially failed before compiling because cmd.exe received backslash
escaped quotes around the setup path. Fixed by writing a temporary batch and
running its explicit relative path; this also handles disabled implicit current
directory executable lookup. No game, installed patch, picture or FPS claim.
The actual installed/live-state gates below remain required for device A/B.

Select **AABB only**. Historical r5-r7 logic-thread target is 0.6-0.8 ms/frame;
offline body saving is about 87%, suggesting 0.5-0.6 ms where logic constrains the
frame. These are prior-machine measurements and an estimate, **not a pc2 gain**.
DQ saves only retry overshoot (0.1-0.4 ms estimated), P1E7 0.05-0.2 ms, MA
0.05-0.15 ms. AABB preserves arithmetic order and falls back for overlapping
input/output; it does not reduce picture quality. Choose a fixed gameplay view
with CPU headroom evidence before spending the device slot; default1761's
30/45 fps field alone does not establish that it is CPU-bound.

## Exact source, defaults and proof boundary

Private branch base b54b38c7; `git diff --quiet 6abe913 b54b38c7 --
mod/Nioh3EngineFix/src` passes. Its EngineFix source therefore matches the pinned
1.7.61 source after decoder fix 6abe913. Pinned ASI SHA256:
`4734152ACAFDC13FA417C23EE60C8F6579B023D7F93FE73A763D862446A7AD00`.
Exact default INI SHA256:
`93CA1930165FB49073E0E4CB291C48164936B2B38041B14EE82486F2766B95B6`
(local INI independently matches; binary/device equality remains a run gate).
Source provenance: `notes/handoff-codex-step1.md:11-17`.

- `dllmain.cpp:434,530,1940-1945`: `[Patches] AABB=0` default means **no cave**;
  `1` installs ON; `2` installs OFF. Current-process installer requires known
  Steam v2.02, site/group/dependency checks and successful frozen write.
  Site `6EA50` is a 5-byte entry jump, dependency `6EA55..6EFCB` FNV A3C06912.
  OFF replays `mov [rsp+8],rbx`, then stock at `6EA55`.
- `caves.asm:605-673`: SSE uses the same per-axis ordered add/multiply/sign
  operations, stores xyz maximum/minimum, leaves output w lanes untouched.
  Overlap with input `[rcx,rcx+4Ch)` falls back to stock. XMM0-5 are volatile.
- Archived `re/scripts_logic/offline_out.txt:1-18` records 300,000 finite boxes,
  2,001 overlapping cases and 2,000 OFF cases with zero mismatch; 20,000
  nonfinite cases also had zero mismatch but were informational. This exercises
  the earlier F# assembled cave against copied original code, **not a new test
  of the current ASI**. Existing `test_patterns.py` checks sites/dependencies;
  `drawprep_harness.cpp` directly tests actual DQ/P1E7 assembly. Neither is a
  device performance result. Stable release full-suite PASS is historical.

## Control hazard and isolation

`menu.cpp` deliberately shows absent experimental rows as next-launch settings.
`RemoteValue` returns the live pointer when present, otherwise `g_expIni`.
`RunRemote` can return success for either; `livectl.cpp::ApplyOne` accepts that
value and discards the successful reply's `restart` / `failed` annotation.
Consequently an ACK `ok=1 fail=0 AABB=1` **alone does not prove installation**.
Missing/skipped cave plus alternating ini preferences is a false A/B.

Private reproduction: `tools/bench/test_cpu_ready_aabb_control.py`. It compiles
actual selected stable menu/LiveCtl functions in an isolated host, including
full guards/key lookup/value formatting/Remote handling/ApplyOne; only Change's
experimental arm and Value's experimental dispatch are extracted. It asserts
the two false-success paths (not installed, failed installation), seven genuine
installed set/readbacks including no-ops, untouched DQ/P1E7/MA and error paths.
It emits source digests; it cannot prove device transport timing or ASI identity.
Native execution is pending root clearance after the PC recording reservation.
Light local checks PASS: Python AST syntax and fenced PowerShell recipe parser;
neither check executes device code or establishes the native test result.
Reproduce after that clearance:
`py -I tools/bench/test_cpu_ready_aabb_control.py --setup
"C:\Users\Ahmad Hamza\nioh3-engine\tools\msvc\setup_x64.bat"`.
Donor compiler setup is read-only; generated files stay under this worktree's
ignored `mod/Nioh3EngineFix/build/cpu-ready-control` and are removed afterward.

Historical `pc2_abba2_pc.ps1` installs from a preboss ini, modifies graphics,
display refresh and VirtualPad setup, and leaves earlier live candidates ON.
`fastab.ps1` adds a tour/automatic continuation and other defaults. Do not use
either multi-item schedule as this candidate's isolated control. No source fix
is queued: the explicit install gate below prevents this ACK hazard.

## Runnable device recipe (lead only, after clearance)

1. Require root clearance, post-crash restoration/hash/lock-release receipt and
   independent clock-origin calibration. Recording reservation must be released.
   Lead owns the existing launch/backup/restore flow; this note authorizes no
   launch, controller, hold, display, affinity or system change.
2. Archive entry saves/config/loader/ASI/INI/shader/control-file hashes plus
   backup bytes and absence states. Use the exact pinned ASI in **both** arms;
   A is not ASI-absent. Derive a temporary INI from the pinned default INI with
   only `[Patches] AABB=2`, `[Debug] LiveCtl=1` and
   `LiveCtlFile=Nioh3EngineFix_live.ini`. DQ=P1E7=MA remain **0**; BPO and other
   optional CPU candidates remain default 0; default GPU choices remain fixed.
   Archive the derived INI and semantic diff. Do not silently import KEEP flags.
   Both arms use the same save, resolution/upscaler/render ratio, camera, shaders,
   FG state and cap. Require FG already OFF and a naturally CPU-limited view.
   If cap headroom requires a permitted game-side MaxFps change, the lead must
   record it as an identical startup value for both arms; no display changes.
3. Use the lead's cleared startup/manual Continue. Require current PID and
   fresh 1.7.61 log, known v2.02, exact ASI hash, and the exact boot line
   `AABB: SSE character AABB cave installed, switch off` plus `LIVE: ON - control
   file ...`. Any missing/skipped/failed AABB line aborts. An existing LiveCtl
   file is ignored at startup until its write timestamp/size changes; write a
   fresh generation **after** this log. Initialize AABB=0, then warm up 60 s.
4. With the only relevant control/capture writer, execute the code below in the
   device's PowerShell. `$GameDir`, `$PresentMon` and `$Output` must identify the
   verified install, existing external PresentMon binary and fresh run folder;
   `$GamePid` is the PID just verified by the lead. This code performs no launch,
   deployment, save/config edit, input, lock or shutdown action.

```powershell
$ctl = Join-Path $GameDir 'Nioh3EngineFix_live.ini'
$ack = "$ctl.ack"
$run = (New-Guid).ToString('N')
$script:gen = 0
$script:lastValue = 0 # required fresh boot log proves installed/OFF
$nativeLog = Join-Path $GameDir 'Nioh3EngineFix.log'
$cpuProcess = Get-Process -Id $GamePid -ErrorAction Stop
New-Item -ItemType Directory -Path $Output -ErrorAction Stop | Out-Null
function Set-Aabb([int]$value, [string]$label) {
    if ($cpuProcess.HasExited) { throw 'Game exited' }
    $logOffset = [IO.File]::ReadAllText($nativeLog).Length
    $script:gen++
    $tag = "cpu_$($run)_$label"
    $tmp = "$ctl.$run.tmp"
    [IO.File]::WriteAllText($tmp, "[Live]`r`nGen=$script:gen`r`nTag=$tag`r`nAABB=$value`r`n", [Text.Encoding]::ASCII)
    Move-Item -LiteralPath $tmp -Destination $ctl -Force
    $deadline = [DateTime]::UtcNow.AddSeconds(6)
    do {
        $a = @{}
        try { $line = [IO.File]::ReadAllText($ack) } catch { $line = '' }
        foreach ($word in ($line.Trim() -split '\s+')) {
            if ($word -match '^([^=]+)=(.*)$') { $a[$Matches[1]] = $Matches[2] }
        }
        if ($a.gen -eq "$script:gen" -and $a.tag -eq $tag) {
            if ($a.ok -ne '1' -or $a.fail -ne '0' -or $a.AABB -ne "$value") { throw "Bad ACK: $line" }
            if ($value -ne $script:lastValue) {
                $newLog = [IO.File]::ReadAllText($nativeLog).Substring($logOffset)
                $word = if ($value) { 'ON' } else { 'OFF' }
                $expected = [regex]::Escape("MENU: Faster bounding boxes (AABB) -> $word")
                if ($newLog -notmatch ($expected + '(?:\r?\n|$)')) { throw "No exact live MENU transition: $newLog" }
            }
            $script:lastValue = $value
            Add-Content -LiteralPath (Join-Path $Output 'acks.txt') -Value $line
            return $a
        }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $deadline)
    throw "No fresh exact ACK: $tag"
}
$null = Set-Aabb 0 'init'
Start-Sleep -Seconds 60
# Three isolated ON/OFF/OFF/ON quads: each quad supplies B/A and A/B pairs.
for ($q=1; $q -le 3; $q++) {
    $position=0
    foreach ($value in 1,0,0,1) {
        $position++
        $label="q${q}_p${position}_v$value"
        $beginAck = Set-Aabb $value $label
        Start-Sleep -Seconds 5
        $csv = Join-Path $Output "$label.csv"
        $pmArgs=@('--process_name','Nioh3.exe','--output_file',('"'+$csv+'"'),
            '--timed','20','--terminate_after_timed','--no_console_stats','--v2_metrics')
        $pmProcess=Start-Process -FilePath $PresentMon -ArgumentList $pmArgs -WindowStyle Hidden -PassThru
        if (-not $pmProcess.WaitForExit(30000)) { throw 'PresentMon timeout; lead handles exact capture PID' }
        if ($pmProcess.ExitCode -ne 0 -or $cpuProcess.HasExited) { throw 'Capture/game failure' }
        $endAck=Set-Aabb $value "${label}_end"
        if ([long]$endAck.frames -le [long]$beginAck.frames) { throw 'No advancing game frames' }
        # Preserve CSVs; lead rejects contaminated quads during the audit below.
    }
}
$null = Set-Aabb 0 'restore_off'
```

5. First true ON and OFF must also yield fresh native lines
   `MENU: Faster bounding boxes (AABB) -> ON` / `-> OFF`, with no `restart`,
   `(ini)` or failure marker. A no-op need not emit MENU; its fresh matching
   Gen/Tag and exact `AABB` value are required. LiveCtl polls every 100 ms and
   ACK rewrites every 250 ms in this source (header's 500 ms is stale).
   ACK reports the last apply, not a continuous query of the live switch;
   exclusivity is necessary. Never press Save during the run (OFF Save retains
   AABB=2 but also writes other menu state).
6. External primary metric: PresentMon v2 **FrameTime/present cadence**,
   FPS=1000/mean(FrameTime), mean FrameTime, nearest-rank p99, GPU Busy and
   CPU Busy. Filter exact PID and one normal game swapchain/present mode;
   require at least 18 s of captured FrameTime per requested 20 s file, finite
   positive intervals, no dropped/missing-capture indication. Keep complete
   original files, Gen/Tag/ACKs/log and metadata. Per-window file membership
   avoids guessing the earlier +14 h mapping; calibration still precedes launch.
   ACK frames are a liveness cross-check, not the performance metric. Busy
   time includes spins and does not prove logic-thread savings.
7. Require no cap compression and clear baseline GPU headroom (for example,
   GPU Busy below 80% of FrameTime; reject rather than infer a CPU win near
   GPU saturation). Reject a quad on OFF/OFF FPS shift >1%, ON/ON >1.5%,
   GPU Busy shift >3%, scene/input/FG/mode changes or any install/ACK gap.
   Report valid quad deltas as units; windows/frames are not independent runs.
   Pilot success: median FPS improvement >=1%, every valid quad >0, p99 not
   worse >2%; fewer than three valid quads needs another cleared run. Same
   scene validation plus stock/cave arithmetic evidence supports unchanged
   picture; fixed-view performance does not prove combat safety or latency.
8. At any failure stop measurement; attempt AABB=0 only while the same process
   responds. Preserve exact PID/start identity and logs. Lead performs graceful
   exit and restores exact entry bytes/absence states for ASI/INI/control/ACK,
   saves/config and shader/loader files, then publishes hash and lock-release
   receipts. No name-wide kill, speculative relaunch or automatic retry. An
   exited/crashed process requires fresh clearance after recovery.

Missing gates: native execution of the new control reproduction; stable binary
and current-process install on pc2; recovered prior session and calibrated clock;
released recording reservation/new-launch clearance; naturally CPU-bound,
uncapped same-picture view; actual AABB-only PresentMon quads and movement/combat
observation. No measured gain, source patch, deployment or KEEP claim here.
