<#
  pc_gpu_session.ps1 - unattended PC GPU session at the USER'S real settings (notes\windows-gpu-plan.md). Runs AFTER run8.
  Phase A  GP2 per-PSO GPU budget on the tour route (tools\tour\run_tour.ps1 -Input vpad): config.xml = the user's session config
           (captures\playsession\20261008-2059-session\config.xml: 2560x1440, REN_RAT 75, XeSS input -> OptiScaler FSR4, FSR frame
           generation, VSync ON, 120 cap, HIGH), GPUProf = 2 (Top 100, 1 frame in 8), no SP. Digest: re\scripts_gpu\ally_budget.py
           gp2 / gp1 -> <session>\A_budget.txt (gameplay windows only, --min-draws).
  Phase B  launch A/B of every Windows-valid same-look item at the same config with FRAME_GENERATION OFF (-KeepFG keeps the user's FG;
           FG pacing flaps made earlier PC A/Bs INVALID) and [VRR] MaxFps 240 so the 120 cap never binds. Shader items swap the
           loose shader_dx12 file between launches (SHA-256 table = gpu-shaders\ally_shaders.sh, the single source of the hashes);
           ASI items (AS0, GIM) are ini keys. Order: BASE, then per item pair x y BASE y x BASE (each item 2 launches, every
           launch bracketed by BASE launches; the report interpolates BASE for drift). Per launch: VirtualPad title -> Continue
           (fixed spawn = the save's load point, no camera input), settle, -Windows x -WindowSec measuring windows behind the quiet
           guard (Wait-Quiet + Measure-Foreign; noisy windows are flagged and dropped), GPUProf = 2 for the per-PSO row of the
           target shader, PresentMon (GPUBusy = primary metric), one screenshot at the spawn view (only when nobody used the PC
           for 120 s; it takes the foreground for ~5 s and gives it back). Analysis: pc_gpu_report.py (verdicts + shotdiff.py).
  Safety   harness_guard (owner pid, MaxMinutes, commit headroom, max crashes, pid files, measuring lock %TEMP%\nioh3_measuring.lock),
           bridge game lock (holder 'pc'), backups of config.xml / mod ini / ASI / save folder (SHA-256 manifest) / every touched
           shader file, ALWAYS restored and verified (Restore-All, also on guard abort). Refuses while Nioh3, run8 (pc_retest_run8 /
           abba2_pc / run_tour) or a live measuring lock is active; -WaitRun8 waits for them instead (max -WaitMaxMin).
  Phase U  (-Phases U; notes\gpu-unattributed.md) where the GP2 stamped span goes that the per-PSO table does not attribute. One launch
           via run_tour: list = the single mission -UStage (default 7292 Mirage Fortress spawn, the heaviest GPU-bound point of Phase A),
           -Points 1 (spawn only), -UPasses 5 passes x -UDwell 30 s = the same spawn view re-loaded 5 times = 5 GP windows. GPUProf2_AB = 1
           with SampleEvery = 1: windows 1/3/5 stamp every frame, windows 2/4 stamp nothing (GP2 off on the GPU, PresentMon only) = ABABA
           overhead A/B in one launch; GQ = 1 (DIRECT gap census: gaps / heads / tails / unmeasured lists). Decode:
           re\scripts_gpu\gpu_unattr.py -> <session>\U_unattr.txt (idle by cause, unmeasured work by category, profiler overhead).
  Phase M  (-Phases M; notes\meshlet-frontend.md, ASI >= 1.7.47) meshlet front-end probe. Two run_tour launches at -MStage (default 7292
           Mirage Fortress spawn), -MPasses x -MDwell s each, GP2 MinRun 1 + [Profiler] MeshletProbe = 1 (per-dispatch / per-barrier
           stamps, CS invocations in alternate sampled frames, raw dump) + MpcCheck = -MpcCheckChains: M1 with the SHIPPED MPC file
           (MpcCheck = negative control, must report mismatches), M2 with the `mpc` file (MpcCheck must report none). Decode:
           re\scripts_gpu\mlp_report.py -> <session>\M_report.txt. Phase B items for the changes: mpc (file only) and mpcs (mpc file +
           [Patches] MPCS = 1).
  -DryRun  no game, no Steam, no lock, no real file changes: temp game/save folders, fake shader table, run_tour -DryRun (fake game),
           fake Phase B launches (pc_gpu_report.py fake-launch) -> the whole wrapper + report path. Read-only checks of the REAL
           replacement files, game shader states, config source and ASI build are printed. Ends with 'DryRun OK'.
  Run:     powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\pc_gpu_session.ps1 -WaitRun8
#>
param(
    [string[]]$Phases = @("A", "B"),
    [string[]]$Items = @("wdo28", "esm", "gi", "AS0", "fogpar", "wdo14", "GIM", "mpc", "mshlod"),
    [ValidateSet(2, 4)][int]$Reps = 2,
    [string]$Tag = "gpu1",
    [string]$UserConfig = "",
    [switch]$KeepFG,
    [string]$TourList = "main", [int]$TourPoints = 3, [int]$TourDwell = 12,
    [int]$SettleSec = 45, [int]$WindowSec = 20, [int]$Windows = 3, [int]$MinDraws = 1000, [int]$GP2MinRun = 4,
    [string]$UStage = "7292", [int]$UPasses = 5, [int]$UDwell = 30,
    [string]$MStage = "7292", [int]$MPasses = 3, [int]$MDwell = 30, [int]$MpcCheckChains = 400,
    [switch]$NoShots, [switch]$WaitRun8, [int]$WaitMaxMin = 240,
    [int]$OwnerPid = 0, [int]$MaxMinutes = 300, [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$ws = (Resolve-Path (Join-Path $here "..\..")).Path
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
if (-not $UserConfig) { $UserConfig = Join-Path $ws "captures\playsession\20261008-2059-session\config.xml" }
$Phases = @($Phases -split "," | ForEach-Object { $_.Trim().ToUpper() } | Where-Object { $_ })
$Items = @($Items -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$RealGame = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
if ($DryRun) {
    $dry = Join-Path $env:TEMP "nioh3-gpusess-dry-$stamp"
    $GameDir = Join-Path $dry "game"; $SaveRoot = Join-Path $dry "KoeiTecmo\NIOH3"; $BackupRoot = Join-Path $dry "save-backups"
    $sess = Join-Path $dry "session"
    $env:HG_MIN_HEADROOM_GB = "0"; $env:HG_MIN_CURRENT_GB = "0"
    $SettleSec = 0; $WindowSec = 3; $Windows = 2
} else {
    $GameDir = $RealGame; $SaveRoot = Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3"
    $sess = Join-Path $ws "captures\pc\gpu_session\$stamp-$Tag"
}
$CaptureRoot = $sess
. (Join-Path $ws "tools\playsession\session_lib.ps1")   # Set-IniValue, Get-AsiVersion, Invoke-GameLock, Get-LockHolder, Force-Acl, $PmExe, $UoBridge
. (Join-Path $here "harness_guard.ps1")
. (Join-Path $here "vpad.ps1")
if ($DryRun) { $script:MeasureLock = Join-Path $dry "nioh3_measuring.lock" }   # never touch the real lock file (run8 may hold it)
$ui = Join-Path $ws "tools\ui.fsx"
$report = Join-Path $here "pc_gpu_report.py"
$BDir = Join-Path $sess "B"; $orig = Join-Path $sess "orig"; $OrigShaders = Join-Path $orig "shaders"
New-Item -ItemType Directory -Force $sess, $BDir, $orig, $OrigShaders | Out-Null
$runLog = Join-Path $sess "session.log"
$cfg = Join-Path $SaveRoot "config.xml"; $ini = Join-Path $GameDir "Nioh3EngineFix.ini"; $asi = Join-Path $GameDir "Nioh3EngineFix.asi"
$modLog = Join-Path $GameDir "Nioh3EngineFix.log"; $binAsi = Join-Path $ws "mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi"
$ShaderDir = Join-Path $GameDir "shader_dx12"; $ReplDir = Join-Path $ws "gpu-shaders\replacement"; $RealReplDir = $ReplDir
$AsiItems = @{ AS0 = "Patches.AS0=1"; GIM = "Patches.GIMerge=1;Debug.GIProbe=1"; mpcs = "Patches.MPCS=1";
               # notes\gpu-scheduling-sync.md: VCLOUD (slot 3) / VFSCAT fog (slot 4) on the graphics queue (engine sync path) vs stock async
               vcsync = "Patches.AS=1;Tuning.AS_Slots=-3"; fogsync = "Patches.AS=1;Tuning.AS_Slots=-4";
               # notes\root-cause-vram-streaming.md: per-draw CBVs mirrored into VRAM (ASI >= 1.7.52): cm = auto (on without ReBAR), cmforce = always
               cm = "GpuOpt.CbvMirror=1"; cmforce = "GpuOpt.CbvMirror=2";
               # notes\gpu-barriers.md: D3D12 barrier batching (bb = on, bbc = hooks + counters only)
               bb = "GpuOpt.Barriers=1"; bbc = "GpuOpt.Barriers=2";
               # notes\rebar-fix-v3.md (ASI >= 1.7.57): no-ReBAR layout forced on a ReBAR PC; sbmbase = census only, sbm = scene-buffer mirror (own COPY queue), sbmd = mirror on the DIRECT queue
               sbmbase = "Profiler.MemStats=2;Profiler.VSForceSysUpload=2;Profiler.SceneBufProbe=1";
               sbm = "Profiler.MemStats=2;Profiler.VSForceSysUpload=2;Profiler.SceneBufProbe=1;GpuOpt.SceneBufMirror=2";
               sbmd = "Profiler.MemStats=2;Profiler.VSForceSysUpload=2;Profiler.SceneBufProbe=1;GpuOpt.SceneBufMirror=2;GpuOpt.SceneBufMirrorQueue=0" }
$ComboShader = @{ mpcs = "mpc" }   # ASI items that need a shader item installed in their arm (MPCS refuses without the mpc file)
$script:restored = $false; $script:lockHeld = $false; $script:modInstalled = $false; $script:cfgInstalled = $false; $script:savesBacked = $false
$script:Init = @{}; $script:pm = $null; $script:summary = [ordered]@{}

function L([string]$s, [string]$c = "Gray") { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l -ForegroundColor $c; Add-Content $runLog $l }
function Sha([string]$p) { if (Test-Path -LiteralPath $p) { (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLower() } else { "" } }
function Read-Shared([string]$path) {
    if (-not (Test-Path $path)) { return "" }
    try { $fs = [IO.File]::Open($path, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $sr.Close(); return $t } catch { return "" }
}
function Send-Bridge([string]$m) {
    if ($DryRun) { return }
    try { Push-Location $UoBridge; py -m uo_bridge send nioh3 $m 2>&1 | Out-Null } catch {} finally { Pop-Location }
}

# ---- shader table (gpu-shaders\ally_shaders.sh TABLE: name file orig repl [superseded,...]) ------------------------------------------
function Get-ShaderTable([string]$sh) {
    $t = [IO.File]::ReadAllText($sh)
    $m = [regex]::Match($t, '(?s)TABLE="(.*?)"')
    if (-not $m.Success) { throw "no TABLE in $sh" }
    $tab = @{}
    foreach ($ln in ($m.Groups[1].Value -split "`n")) {
        $c = @($ln.Trim() -split '\s+' | Where-Object { $_ })
        if ($c.Count -lt 4) { continue }
        if (-not $tab.ContainsKey($c[0])) { $tab[$c[0]] = @() }
        $tab[$c[0]] += , @{ file = $c[1]; orig = $c[2].ToLower(); repl = $c[3].ToLower(); old = $(if ($c.Count -ge 5) { @($c[4].ToLower() -split ',') } else { @() }) }
    }
    return $tab
}
$RealTab = Get-ShaderTable (Join-Path $ws "gpu-shaders\ally_shaders.sh")
foreach ($it in $Items) { if (-not $RealTab.ContainsKey($it) -and -not $AsiItems.ContainsKey($it)) { throw "unknown item '$it' (shader: $($RealTab.Keys -join ','); ASI: $($AsiItems.Keys -join ','))" } }
$ShaderItems = @(@($Items | Where-Object { $RealTab.ContainsKey($_) }) + @($Items | Where-Object { $ComboShader.ContainsKey($_) } | ForEach-Object { $ComboShader[$_] }) +
                 @(if ($Phases -contains "M") { "mpc" }) | Select-Object -Unique)

function State-Of($e, [string]$h) { if ($h -eq $e.orig) { "orig" } elseif ($h -eq $e.repl) { "repl" } elseif ($e.old -contains $h) { "superseded" } elseif (-not $h) { "missing" } else { "unknown" } }
# read-only check of the REAL files (also printed in -DryRun): replacement sha256 + what the game folder has now
function Check-RealShaders {
    $bad = @()
    foreach ($it in $ShaderItems) {
        foreach ($e in $RealTab[$it]) {
            $r = Sha (Join-Path $RealReplDir $e.file); $g = Sha (Join-Path (Join-Path $RealGame "shader_dx12") $e.file)
            $ok = ($r -eq $e.repl)
            L ("  {0,-9} {1}  replacement {2}  game now: {3}" -f $it, $e.file, $(if ($ok) { "sha OK" } else { "BAD SHA ($($r.Substring(0, [Math]::Min(8, $r.Length))))" }), (State-Of $e $g))
            if (-not $ok) { $bad += $it }
        }
    }
    return @($bad | Select-Object -Unique)
}

if ($DryRun) {   # fake table + fake files: the same swap / restore code runs on them
    $Tab = @{}; $ReplDir = Join-Path $dry "repl"; New-Item -ItemType Directory -Force $ShaderDir, $ReplDir | Out-Null
    foreach ($it in $ShaderItems) {
        $Tab[$it] = @()
        foreach ($e in $RealTab[$it]) {
            [IO.File]::WriteAllText((Join-Path $ShaderDir $e.file), "ORIG $($e.file)"); [IO.File]::WriteAllText((Join-Path $ReplDir $e.file), "REPL $($e.file)")
            $Tab[$it] += , @{ file = $e.file; orig = (Sha (Join-Path $ShaderDir $e.file)); repl = (Sha (Join-Path $ReplDir $e.file)); old = @() }
        }
    }
} else { $Tab = $RealTab }

function Set-ItemState([string]$item, [bool]$on) {
    foreach ($e in $Tab[$item]) {
        $g = Join-Path $ShaderDir $e.file
        $want = if ($on) { $e.repl } else { $e.orig }
        if ((Sha $g) -eq $want) { continue }
        $src = if ($on) { Join-Path $ReplDir $e.file } else { Join-Path $OrigShaders $e.file }
        if ((Sha $src) -ne $want) { throw "${item}: source $src has the wrong sha256" }
        Copy-Item -LiteralPath $src -Destination $g -Force
        if ((Sha $g) -ne $want) { throw "${item}: copy to $g failed" }
    }
}
function Backup-Shaders {   # snapshot + originals of every file an item touches; items whose file is in an unknown state are dropped
    $keep = @()
    foreach ($it in $ShaderItems) {
        $ok = $true
        foreach ($e in $Tab[$it]) {
            $g = Join-Path $ShaderDir $e.file; $h = Sha $g; $st = State-Of $e $h
            $dst = Join-Path $OrigShaders $e.file
            if ($st -eq "orig") { if ((Sha $dst) -ne $e.orig) { Copy-Item -LiteralPath $g -Destination $dst -Force } }
            elseif ($st -eq "repl") {
                $b = Join-Path $ws "gpu-shaders\backup\$($e.file)"
                if ((Sha $b) -eq $e.orig) { Copy-Item -LiteralPath $b -Destination $dst -Force } else { L "  $it/$($e.file): installed = replacement and no verified original in gpu-shaders\backup - item dropped" Yellow; $ok = $false }
            } else { L "  $it/$($e.file): game file is $st - item dropped (Steam verify restores it)" Yellow; $ok = $false }
            if ($ok -and -not $script:Init.ContainsKey($e.file)) { $script:Init[$e.file] = @{ sha = $h; e = $e } }
        }
        if ($ok) { $keep += $it }
    }
    return $keep
}
function Restore-Shaders {
    $bad = 0
    foreach ($f in $script:Init.Keys) {
        $x = $script:Init[$f]; $g = Join-Path $ShaderDir $f
        if ((Sha $g) -eq $x.sha) { continue }
        $src = if ($x.sha -eq $x.e.repl) { Join-Path $ReplDir $f } else { Join-Path $OrigShaders $f }
        try { Copy-Item -LiteralPath $src -Destination $g -Force } catch {}
        if ((Sha $g) -ne $x.sha) { $bad++; L "!! shader $f NOT restored - copy $src back by hand (or Steam verify)" Red }
    }
    L "shader files back as found ($($script:Init.Count) files checked, $bad failed)" $(if ($bad) { "Red" } else { "Green" })
}

# ---- save folder (Savedata*), config.xml, mod files ------------------------------------------------------------------------------------
function Get-SaveManifest {
    $m = @{}
    if (-not (Test-Path $SaveRoot)) { return $m }
    $root = (Get-Item -LiteralPath $SaveRoot -Force).FullName.TrimEnd('\')
    foreach ($f in Get-ChildItem -LiteralPath $root -Recurse -File -Force) {
        $rel = $f.FullName.Substring($root.Length + 1)
        if ($rel -notlike "Savedata*") { continue }
        $m[$rel] = (Get-FileHash $f.FullName -Algorithm SHA256).Hash
    }
    return $m
}
function Backup-Saves {
    $script:saveMan = Get-SaveManifest
    foreach ($k in $script:saveMan.Keys) { $d = Join-Path $orig "saves\$k"; New-Item -ItemType Directory -Force (Split-Path $d) | Out-Null; Copy-Item -LiteralPath (Join-Path $SaveRoot $k) -Destination $d -Force }
    foreach ($k in $script:saveMan.Keys) { if ((Get-FileHash (Join-Path $orig "saves\$k") -Algorithm SHA256).Hash -ne $script:saveMan[$k]) { throw "save backup mismatch: $k" } }
    $script:saveMan | ConvertTo-Json | Set-Content (Join-Path $orig "saves_manifest.json") -Encoding UTF8
    $script:savesBacked = $true
    L "save folder backed up ($($script:saveMan.Count) files, SHA-256 manifest)"
}
function Restore-Saves {
    if (-not $script:savesBacked) { return }
    $now = Get-SaveManifest; $n = 0
    foreach ($k in $now.Keys) { if (-not $script:saveMan.ContainsKey($k)) { Remove-Item -LiteralPath (Join-Path $SaveRoot $k) -Force; $n++ } }
    foreach ($k in $script:saveMan.Keys) {
        if (-not $now.ContainsKey($k) -or $now[$k] -ne $script:saveMan[$k]) { $d = Join-Path $SaveRoot $k; New-Item -ItemType Directory -Force (Split-Path $d) | Out-Null; Copy-Item -LiteralPath (Join-Path $orig "saves\$k") -Destination $d -Force; $n++ }
    }
    $after = Get-SaveManifest; $bad = 0
    foreach ($k in $script:saveMan.Keys) { if ($after[$k] -ne $script:saveMan[$k]) { $bad++ } }
    $bad += @($after.Keys | Where-Object { -not $script:saveMan.ContainsKey($_) }).Count
    if ($bad) { L "!! SAVE RESTORE INCOMPLETE ($bad files differ) - copy $orig\saves back by hand" Red } else { L "save folder verified byte-for-byte ($($script:saveMan.Count) files, $n restored)" Green }
}
function Install-Config([switch]$FgOff) {
    if (-not $script:cfgInstalled) { Copy-Item -LiteralPath $cfg -Destination (Join-Path $orig "config.xml") -Force; $script:cfgSha = Sha $cfg; $script:cfgInstalled = $true }
    $t = [IO.File]::ReadAllText($UserConfig)
    if ($FgOff) { $t = $t -replace "<FRAME_GENERATION>[^<]*</FRAME_GENERATION>", "<FRAME_GENERATION>OFF</FRAME_GENERATION>" }
    [IO.File]::WriteAllText($cfg, $t, (New-Object Text.UTF8Encoding($true)))
    $fg = [regex]::Match($t, "<FRAME_GENERATION>([^<]*)<").Groups[1].Value
    L "config.xml = user session config ($([regex]::Match($t, '<SCR_REZ>([^<]*)<').Groups[1].Value), REN_RAT $([regex]::Match($t, '<REN_RAT>([^<]*)<').Groups[1].Value), $([regex]::Match($t, '<RESOLUTION_SCALING>([^<]*)<').Groups[1].Value), FG $fg)"
}
function Restore-Config {
    if (-not $script:cfgInstalled) { return }
    Copy-Item -LiteralPath (Join-Path $orig "config.xml") -Destination $cfg -Force
    if ((Sha $cfg) -eq $script:cfgSha) { L "config.xml restored (sha verified)" Green } else { L "!! config.xml restore mismatch - copy $orig\config.xml back" Red }
}
function Install-Mod {
    if ($script:modInstalled) { return }
    foreach ($f in $asi, $ini) { if (Test-Path $f) { Copy-Item -LiteralPath $f -Destination (Join-Path $orig (Split-Path $f -Leaf)) -Force } }
    $script:modInstalled = $true
    if ($DryRun) { Set-Content $asi "dry asi" -Encoding ASCII } else { Copy-Item $binAsi $asi -Force }
    L "mod: bin ASI $(if ($DryRun) { '(dry)' } else { Get-AsiVersion $binAsi }) installed for Phase B"
}
function Restore-Mod {
    if (-not $script:modInstalled) { return }
    foreach ($f in $asi, $ini) { $b = Join-Path $orig (Split-Path $f -Leaf); if (Test-Path $b) { Copy-Item -LiteralPath $b -Destination $f -Force } else { Remove-Item -LiteralPath $f -Force -ErrorAction SilentlyContinue } }
    L "mod ini / ASI back as they were" Green
}
$BaseIniSrc = if (Test-Path (Join-Path $GameDir "Nioh3EngineFix.ini.preboss.bak")) { Join-Path $GameDir "Nioh3EngineFix.ini.preboss.bak" } elseif (Test-Path $ini) { $ini } else { Join-Path $ws "mod\Nioh3EngineFix\Nioh3EngineFix.ini" }
# Every arm starts from the PRISTINE ini (snapshot taken ONCE at the first arm, before any arm ini is written) + only that arm's keys.
# Bridge #988: $BaseIniSrc used to be the live $ini, which the previous arm had overwritten -> BASE arms carried CbvMirror etc.
function Get-PristineLines {
    if (-not $script:PristineLines) {
        $script:PristineLines = [string[]][IO.File]::ReadAllLines($BaseIniSrc)
        try { [IO.File]::WriteAllLines((Join-Path $orig "pristine_arm_base.ini"), $script:PristineLines, (New-Object Text.UTF8Encoding($false))) } catch {}
        L "pristine ini snapshot: $BaseIniSrc ($($script:PristineLines.Count) lines)"
    }
    return ,$script:PristineLines
}
function Get-IniMap([string[]]$lines) {
    $m = @{}; $cur = ""
    foreach ($l in $lines) { if ($l -match '^\s*\[(.+?)\]') { $cur = $Matches[1]; continue }; if ($l -match '^\s*([^;#=\s][^=]*?)\s*=\s*(.*?)\s*$') { $k = "$cur.$($Matches[1])"; if (-not $m.ContainsKey($k)) { $m[$k] = $Matches[2] } } }
    return $m
}
function Get-ArmKeys([string]$arm) {
    $keys = @("General.AutoContinueLoad=1", "Debug.VirtualPad=1", "Debug.KeepRunningUnfocused=1", "VRR.MaxFps=240",
              "Profiler.GPUProf=2", "Profiler.GPUProf2_SampleEvery=8", "Profiler.GPUProf2_Top=100", "Profiler.GPUProf2_MinRun=$GP2MinRun", "Profiler.GPUProf2_AB=0",
              "Profiler.SP=0", "Patches.AS0=0", "Patches.AS=0", "Patches.GIMerge=0", "Patches.MPCS=0", "Profiler.MeshletProbe=0", "Profiler.MpcCheck=0")
    if ($AsiItems.ContainsKey($arm)) { $keys += @($AsiItems[$arm] -split ";") }
    return $keys
}
# Guard: log the diff of the written ini vs pristine; fail the arm if any key outside the arm's own key set differs from pristine.
function Test-ArmIni([string]$arm, [string[]]$written, [string[]]$pristine) {
    $w = Get-IniMap $written; $p = Get-IniMap $pristine
    $own = @{}; foreach ($kv in (Get-ArmKeys $arm)) { if ($kv -match '^([^=]+)=') { $own[$Matches[1].Trim()] = $true } }
    $diff = @(foreach ($k in (@($w.Keys) + @($p.Keys) | Sort-Object -Unique)) { if ($w[$k] -ne $p[$k]) { "$k : $(if ($p.ContainsKey($k)) { $p[$k] } else { '<absent>' }) -> $(if ($w.ContainsKey($k)) { $w[$k] } else { '<absent>' })" } })
    L "arm $arm ini diff vs pristine ($($diff.Count) keys): $($diff -join ' | ')"
    $bad = @(foreach ($d in $diff) { $k = ($d -split ' : ')[0]; if (-not $own.ContainsKey($k)) { $d } })
    if ($bad.Count) { throw "arm $arm ini is contaminated: keys outside its own set differ from the pristine ini: $($bad -join '; ')" }
}
function Write-ArmIni([string]$arm) {
    $pristine = Get-PristineLines
    $lines = $pristine
    foreach ($kv in (Get-ArmKeys $arm)) { if ($kv -match '^([^.=]+)\.([^=]+)=(.*)$') { $lines = Set-IniValue $lines $Matches[1] $Matches[2] $Matches[3] } }
    Test-ArmIni $arm ([string[]]$lines) $pristine
    [IO.File]::WriteAllLines($ini, $lines, (New-Object Text.UTF8Encoding($false)))
}

# ---- game control ------------------------------------------------------------------------------------------------------------------
# Only the games THIS session launched (pid + start time recorded by Register-Game in the guard) - never every Nioh3 by name, and nothing
# in -DryRun. run8b (2026-10-09): a -DryRun of this script ended at 00:55:39 and its Restore-All closed the game that run8b Block A had
# launched 34 s earlier (A then aborted 'game dead/hung before L1_BASE_s1'; notes\run8b-analysis.md).
function Own-Games {
    if ($DryRun -or -not $global:HG) { return @() }
    @(foreach ($g in @($global:HG.games)) { $p = Get-Process -Id $g.pid -ErrorAction SilentlyContinue; if ($p) { try { if ($p.StartTime.ToString("o") -eq $g.start) { $p } } catch {} } })
}
function Close-Game {
    $g = @(Own-Games)
    if (-not $g.Count) { return }
    Note-Closed
    foreach ($p in $g) { try { $null = $p.CloseMainWindow() } catch {} }
    for ($i = 0; $i -lt 25 -and @(Own-Games).Count; $i++) { & $script:RealSleep -Seconds 2 }
    Own-Games | ForEach-Object { try { Stop-Process -Id $_.Id -Force } catch {} }
    & $script:RealSleep -Seconds 4
}
function Stop-Pm {
    if ($script:pm -and -not $script:pm.HasExited) {
        try { Start-Process $PmExe -ArgumentList "--terminate_existing_session --session_name Nioh3GpuAB" -WindowStyle Hidden -Wait } catch {}; & $script:RealSleep -Seconds 2
        try { Stop-Process -Id $script:pm.Id -Force -ErrorAction SilentlyContinue } catch {}
    }
    $script:pm = $null
}
function Idle-Sec { $o = dotnet fsi $ui idle 2>&1 | Out-String; if ($o -match 'idle ([\d.]+)') { [double]$Matches[1] } else { 0 } }
function Take-Shot([string]$png) {
    if ($NoShots) { return "off" }
    $idle = Idle-Sec
    if ($idle -lt 120) { return "skipped (PC in use, idle $([int]$idle) s)" }
    $before = VP-ForegroundInfo
    $null = dotnet fsi $ui focus Nioh3 2>&1; Start-Sleep 4
    $fg = VP-ForegroundInfo
    if (-not $fg -or $fg.name -ne "Nioh3") { return "skipped (could not focus Nioh3)" }
    $null = dotnet fsi $ui shot $png 0 0 2560 1440 2>&1
    VP-GiveBackFocus $before
    return $(if (Test-Path $png) { "ok" } else { "failed" })
}
# native tools (py / powershell) with stderr folded in, never terminating under EAP Stop (PS 5.1 wraps stderr lines as errors)
function Run-Native([string]$exe, [object[]]$a) { $old = $ErrorActionPreference; $ErrorActionPreference = "Continue"; try { return (& $exe @a 2>&1 | ForEach-Object { "$_" } | Out-String) } finally { $ErrorActionPreference = $old } }
function Add-Jsonl([string]$p, $o) { Add-Content -Path $p -Value ($o | ConvertTo-Json -Compress) -Encoding UTF8 }

function Run-Launch([int]$n, [string]$arm) {
    $name = "L{0:D2}_{1}" -f $n, $arm; $stem = Join-Path $BDir $name
    $rec = [ordered]@{ launch = $n; arm = $arm; ok = $false; why = ""; shot = "" }
    try {
        $need = if ($ComboShader.ContainsKey($arm)) { $ComboShader[$arm] } else { $arm }
        foreach ($it in $ShaderItems) { Set-ItemState $it ($it -eq $need) }
        Write-ArmIni $arm
        Copy-Item $ini "$stem.ini" -Force
        Set-TestStatus "launch $n/$($script:plan.Count): $arm"
        Assert-Launch
        $logOff = (Read-Shared $modLog).Length
        if (-not $DryRun) {
            if (Test-Path $PmExe) {
                $pa = "--process_name Nioh3.exe --output_file `"$stem.csv`" --v2_metrics --date_time --stop_existing_session --session_name Nioh3GpuAB --no_console_stats"
                $script:pm = Start-Process $PmExe -ArgumentList $pa -WindowStyle Hidden -PassThru; Register-Child $script:pm "PresentMon"
            }
            $fgBefore = VP-ForegroundInfo
            Start-Process "steam://rungameid/3681010"
            $t0 = Get-Date
            while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0) -and ((Get-Date) - $t0).TotalSeconds -lt 180) { $cc = "$(& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'steam_cloud_click.ps1') 2>$null | Select-Object -First 1)"; if ($cc -like 'clicked*') { Write-Host "[launch] Steam cloud dialog: $cc" }; Start-Sleep 3 }   # steam_cloud_click: 'Unable to Sync' -> Play anyway (user OK, bridge #602)
            Register-Game
            if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "the game did not start within 180 s" }
            Start-Sleep 3; VP-GiveBackFocus $fgBefore
            if (-not (VP-TitleContinue 600 "$stem.vpad.txt")) { throw "VirtualPad title -> field failed" }
            L "  $name in the field; settle $SettleSec s"
            Start-Sleep $SettleSec
        }
        for ($w = 1; $w -le $Windows; $w++) {
            if (-not $DryRun -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "the game exited during window $w" }
            $qw = if ($DryRun) { 0 } else { Wait-Quiet "$name w$w" { param($l) L $l } }
            Set-MeasureLock "measuring $name w$w"
            $t0u = (Get-Date).ToUniversalTime(); $t0l = Get-Date
            $m1 = if ($DryRun) { $null } else { Measure-Foreign 2.0 }
            Start-Sleep ([Math]::Max(1, $WindowSec - $(if ($DryRun) { 0 } else { 4 })))
            $m2 = if ($DryRun) { $null } else { Measure-Foreign 2.0 }
            $t1u = (Get-Date).ToUniversalTime(); $t1l = Get-Date
            $noisy = ($qw -lt 0) -or ($m1 -and $m1.noisy) -or ($m2 -and $m2.noisy)
            Add-Jsonl (Join-Path $BDir "manifest.jsonl") ([ordered]@{ launch = $n; arm = $arm; w = $w; t0_utc = $t0u.ToString("yyyy-MM-ddTHH:mm:ss.fffZ"); t1_utc = $t1u.ToString("yyyy-MM-ddTHH:mm:ss.fffZ")
                t0_local = $t0l.ToString("HH:mm:ss"); t1_local = $t1l.ToString("HH:mm:ss"); noisy = [bool]$noisy; foreign = $(if ($m2) { $m2.top } else { "" }) })
            if ($noisy) { L "  $name w${w}: NOISY (foreign $(if ($m2) { $m2.cores } else { '?' }) cores: $(if ($m2) { $m2.top }))" Yellow }
        }
        if ($DryRun) {
            $null = Run-Native py @("-I", $report, "fake-launch", "--dir", $BDir, "--launch", "$n", "--arm", $arm, "--shot", "1")
            $rec.shot = "fake"
        } else {
            Start-Sleep 32   # one more GP2 window (30 s) closes after the last measuring window
            $rec.shot = Take-Shot "$stem.png"
            Close-Game
            Stop-Pm
            $t = Read-Shared $modLog
            if ($t.Length -lt $logOff) { $logOff = 0 }   # the mod recreated its log at launch
            [IO.File]::WriteAllText("$stem.log", $t.Substring([Math]::Min($logOff, $t.Length)))
        }
        $rec.ok = $true
        L "  $name done (shot: $($rec.shot))" Green
    } catch {
        $rec.why = "$_"; L "  $name FAILED: $_" Red
        if (-not $DryRun) { Close-Game; Stop-Pm; $t = Read-Shared $modLog; if ($t) { [IO.File]::WriteAllText("$stem.log", $t) } }
    }
    Add-Jsonl (Join-Path $BDir "launches.jsonl") $rec
    return $rec.ok
}

function Build-Plan([string[]]$its) {
    $p = @("BASE")
    for ($i = 0; $i -lt $its.Count; $i += 2) {
        for ($r = 0; $r -lt $Reps / 2; $r++) {
            if ($i + 1 -lt $its.Count) { $x = $its[$i]; $y = $its[$i + 1]; $p += @($x, $y, "BASE", $y, $x, "BASE") }
            else { $x = $its[$i]; $p += @($x, "BASE", $x, "BASE") }
        }
    }
    return $p
}

function Restore-All {
    if ($script:restored) { return }
    $script:restored = $true
    try { Stop-TestPopup } catch {}
    try { Close-Game } catch {}
    try { Stop-Pm } catch {}
    try { Restore-Shaders } catch { L "!! Restore-Shaders: $_" Red }
    try { Restore-Mod } catch { L "!! Restore-Mod: $_" Red }
    try { Restore-Config } catch { L "!! Restore-Config: $_" Red }
    try { Restore-Saves } catch { L "!! Restore-Saves: $_" Red }
    if ($script:lockHeld) { $r = Invoke-GameLock "release"; L "game lock released ($($r.code))"; $script:lockHeld = $false }
}

function Busy-Reason {
    $r = @()
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { $r += "Nioh3 running" }
    $ml = Test-MeasureLock; if ($ml -and [int]$ml.pid -ne $PID) { $r += "measuring lock held by pid $($ml.pid) ($($ml.name))" }
    $all = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue)
    $anc = @{}; $p = $PID; for ($k = 0; $k -lt 12 -and $p; $k++) { $anc[[int]$p] = $true; $p = ($all | Where-Object { $_.ProcessId -eq $p } | Select-Object -First 1).ParentProcessId }   # our own launcher chain
    $h = @($all | Where-Object { -not $anc.ContainsKey([int]$_.ProcessId) -and $_.CommandLine -match 'pc_retest_run8|abba2_pc\.ps1|run_tour\.ps1|pc_gpu_session\.ps1' })
    if ($h.Count) { $r += "harness running: " + (($h | ForEach-Object { if ($_.CommandLine -match '(\w+\.ps1)') { $Matches[1] } }) -join ",") }
    return ($r -join "; ")
}

# =====================================================================================================================================
Init-Guard -Name "pc_gpu_session" -OwnerPid $OwnerPid -MaxMinutes ($MaxMinutes + $(if ($WaitRun8) { $WaitMaxMin } else { 0 })) -MaxCrashes 3 -Cleanup { Restore-All }
$plan = @(); $script:plan = @()
try {
    L "PC GPU session $Tag$(if ($DryRun) { ' - DRY RUN' }): phases $($Phases -join ','), items $($Items -join ','), reps $Reps, session $sess" Cyan
    # ---- preflight (read-only) ----
    $asiVer = Get-AsiVersion $binAsi
    L "bin ASI: $binAsi = $asiVer"
    if ($asiVer -eq "none") { throw "no ASI build at $binAsi" }
    if ($asiVer -ne "unknown" -and [version]$asiVer -lt [version]"1.7.37") { throw "ASI ${asiVer}: VirtualPad needs 1.7.37+" }
    if (-not (Test-Path $UserConfig)) { throw "user config not found: $UserConfig" }
    $uc = [IO.File]::ReadAllText($UserConfig)
    L ("user config: " + ((@("SCR_REZ", "REN_RAT", "RESOLUTION_SCALING", "FRAME_GENERATION", "VSYNC", "REN_FPS", "SHADOW_QUAL", "GI", "VOLUMETRIC") | ForEach-Object { "$_ " + [regex]::Match($uc, "<$_>([^<]*)<").Groups[1].Value }) -join ", "))
    if (-not (Test-Path $PmExe)) { L "PresentMon not found at $PmExe - Phase B would have no GPUBusy" Yellow }
    L "replacement files (real) and the game's current state (read-only):"
    $badItems = Check-RealShaders
    if ($badItems.Count) { L "items with a bad replacement sha dropped: $($badItems -join ',')" Yellow; $Items = @($Items | Where-Object { $badItems -notcontains $_ }); $ShaderItems = @($ShaderItems | Where-Object { $badItems -notcontains $_ }) }
    $st = ((Run-Native py @("-I", $report, "selftest")).Trim() -split "`n")[-1]; L "pc_gpu_report: $st"
    # ---- wait for run8 / anything else measuring ----
    if (-not $DryRun) {
        $tw = Get-Date
        while ($true) {
            $why = Busy-Reason
            if (-not $why) { break }
            if (-not $WaitRun8) { throw "busy: $why (use -WaitRun8 to wait)" }
            if (((Get-Date) - $tw).TotalMinutes -gt $WaitMaxMin) { throw "still busy after $WaitMaxMin min: $why" }
            Set-TestStatus "waiting for run8 / other harness: $why"
            L "waiting: $why"; Start-Sleep 60
        }
        $h = Get-LockHolder
        if ($h.ok -and $h.holder -and $h.holder -ne "pc") { throw "the game lock is held by '$($h.holder)'" }
        $r = Invoke-GameLock "acquire" (($MaxMinutes + 30) * 60)
        if ($r.code -eq 1) { throw "game lock held by another device: $($r.out)" }
        if ($r.code -eq 0) { $script:lockHeld = $true; L "game lock taken" } else { L "game lock: bridge not reachable ($($r.out)) - continuing" Yellow }
        Send-Bridge "[FYI] pc: lock taken $(Get-Date -f HH:mm) for the PC GPU session $Tag (phases $($Phases -join ','); items $($Items -join ',')). Will post [GAME-FREE] when done."
        Start-TestPopup -Name "PC GPU session $Tag" -InputMode vpad
    } else {
        L "dry run: busy check would say: $(if ($b = Busy-Reason) { $b } else { 'free' })"
        New-Item -ItemType Directory -Force (Join-Path $SaveRoot "Savedata\76561199000000000\SAVEDATA00") | Out-Null
        [IO.File]::WriteAllBytes((Join-Path $SaveRoot "Savedata\76561199000000000\SAVEDATA00\SAVEDATA.BIN"), [byte[]](1..200 | ForEach-Object { $_ % 256 }))
        Set-Content $cfg "<?xml version=`"1.0`"?><settings><FRAME_GENERATION>OFF</FRAME_GENERATION></settings>" -Encoding UTF8
        Copy-Item (Join-Path $ws "mod\Nioh3EngineFix\Nioh3EngineFix.ini") $ini; Set-Content $asi "user asi" -Encoding ASCII
        $BaseIniSrc = $ini
    }
    Set-MeasureLock "starting"
    # ---- backups ----
    Backup-Saves
    $ShaderItems = @(Backup-Shaders)
    $Items = @($Items | Where-Object { ($AsiItems.ContainsKey($_) -or $ShaderItems -contains $_) -and (-not $ComboShader.ContainsKey($_) -or $ShaderItems -contains $ComboShader[$_]) })
    L "shader snapshot: $($script:Init.Count) files; testable items: $($Items -join ',')"

    # ---- Phase A: GP2 budget on the tour route, FG as the user has it ----
    if ($Phases -contains "A") {
        Install-Config
        if (-not $DryRun) { $qw = Wait-Quiet "phase A" { param($l) L $l } }
        $gk = "Profiler.GPUProf=2;Profiler.GPUProf2_SampleEvery=8;Profiler.GPUProf2_Top=100;Profiler.GPUProf2_MinRun=$GP2MinRun;Profiler.GPUProf2_AB=0;Profiler.GPUProf_Top=60;Profiler.SP=0"
        $ta = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $ws "tools\tour\run_tour.ps1"), "-Input", "vpad", "-List", $TourList,
                "-Points", "$TourPoints", "-DwellSec", "$TourDwell", "-NoProfile", "-NoPause", "-CaptureRoot", (Join-Path $sess "A"), "-ExtraKeys", $gk,
                "-OwnerPid", "$PID", "-MaxMinutes", "90")
        if ($DryRun) { $ta += "-DryRun" } else { $ta += "-NoLock" }
        L "Phase A: run_tour ($TourList, $TourPoints points x $TourDwell s) with GP2 Top 100"
        Set-TestStatus "Phase A: GP2 budget tour"
        $o = Run-Native powershell $ta
        Set-Content (Join-Path $sess "A_console.txt") $o -Encoding UTF8
        $vd = [regex]::Match($o, "verdict: ([\w-]+)"); $cm = [regex]::Match($o, "capture folder: (.+)")
        L "Phase A tour verdict: $(if ($vd.Success) { $vd.Groups[1].Value } else { '?' }); capture $(if ($cm.Success) { $cm.Groups[1].Value.Trim() })"
        $bud = Join-Path $sess "A_budget.txt"; $txt = ""; $budPy = Join-Path $ws "re\scripts_gpu\ally_budget.py"
        if ($cm.Success) {
            foreach ($lg in @(Get-ChildItem (Join-Path $cm.Groups[1].Value.Trim() "logs") -Filter "run*.log" -ErrorAction SilentlyContinue | Sort-Object Name)) {
                $txt += "==== $($lg.Name): ally_budget gp2 --min-draws $MinDraws ====`n" + (Run-Native py @("-I", $budPy, "gp2", $lg.FullName, "--min-draws", "$MinDraws", "--top", "80"))
                $txt += "==== $($lg.Name): ally_budget gp1 ====`n" + (Run-Native py @("-I", $budPy, "gp1", $lg.FullName, "--min-span", "4"))
            }
        }
        Set-Content $bud $txt -Encoding UTF8
        $hl = @($txt -split "`n" | Where-Object { $_ -match "gameplay windows" } | Select-Object -First 1)
        $script:summary["A"] = "tour $(if ($vd.Success) { $vd.Groups[1].Value } else { '?' }); budget $bud$(if ($hl.Count) { ' - ' + $hl[0].Trim() } else { ' - no gameplay GP2 windows' + $(if ($DryRun) { ' (dry run: fake log)' }) })"
        L "Phase A: $($script:summary['A'])"
    }

    # ---- Phase U: unattributed GPU time (one launch, ABABA stamps ON/OFF at one re-loaded spawn, GQ on) ----
    if ($Phases -contains "U") {
        Install-Config
        if (-not $DryRun) { $qw = Wait-Quiet "phase U" { param($l) L $l } }
        $ul = Join-Path $sess "U_tourlist.txt"
        Set-Content $ul "$UStage   # gpu-unattributed.md: one GPU-bound spawn, re-loaded every pass (window 2 and 4 = stamps OFF)" -Encoding ASCII
        $gk = "Profiler.GPUProf=2;Profiler.GPUProf2_SampleEvery=1;Profiler.GPUProf2_AB=1;Profiler.GPUProf2_Top=100;Profiler.GPUProf2_MinRun=$GP2MinRun;" +
              "Profiler.GQ=1;Profiler.GQ_Top=20;Profiler.GPUProf_Top=60;Profiler.SP=0"
        $ta = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $ws "tools\tour\run_tour.ps1"), "-Input", "vpad", "-List", $ul,
                "-Points", "1", "-DwellSec", "$UDwell", "-Passes", "$UPasses", "-NoProfile", "-NoPause", "-CaptureRoot", (Join-Path $sess "U"),
                "-ExtraKeys", $gk, "-OwnerPid", "$PID", "-MaxMinutes", "45")
        if ($DryRun) { $ta += "-DryRun" } else { $ta += "-NoLock" }
        L "Phase U: run_tour (mission $UStage spawn, $UPasses passes x $UDwell s, GP2 A/B every frame + GQ)"
        Set-TestStatus "Phase U: unattributed GPU time"
        $o = Run-Native powershell $ta
        Set-Content (Join-Path $sess "U_console.txt") $o -Encoding UTF8
        $vd = [regex]::Match($o, "verdict: ([\w-]+)"); $cm = [regex]::Match($o, "capture folder: (.+)")
        $ut = Join-Path $sess "U_unattr.txt"; $txt = ""; $unPy = Join-Path $ws "re\scripts_gpu\gpu_unattr.py"
        if ($cm.Success) {
            $cap = $cm.Groups[1].Value.Trim(); $pmc = Join-Path $cap "presentmon.csv"
            foreach ($lg in @(Get-ChildItem (Join-Path $cap "logs") -Filter "run*.log" -ErrorAction SilentlyContinue | Sort-Object Name)) {
                $ua = @("-I", $unPy, $lg.FullName, "--windows")
                if (Test-Path $pmc) { $ua += @("--pm", $pmc) }
                $txt += "==== $($lg.Name): gpu_unattr ====`n" + (Run-Native py $ua)
            }
        }
        Set-Content $ut $txt -Encoding UTF8
        $hl = @($txt -split "`n" | Where-Object { $_ -match "^(bridge:|A/B \(stamps)" } | Select-Object -First 2)
        $script:summary["U"] = "tour $(if ($vd.Success) { $vd.Groups[1].Value } else { '?' }); $ut$(if ($hl.Count) { ' - ' + (($hl | ForEach-Object { $_.Trim() }) -join ' | ') } else { ' - no GP2 windows decoded' })"
        L "Phase U: $($script:summary['U'])"
        Send-Bridge "[RESULT] pc2 Phase U (gpu-unattributed.md) done: $($script:summary['U']). Post U_unattr.txt + the U capture folder (logs\run01.log, presentmon.csv) as one zip."
    }

    # ---- Phase M: meshlet front-end probe (notes\meshlet-frontend.md): M1 shipped MPC file, M2 the mpc file; MpcCheck in both ----
    if ($Phases -contains "M") {
        if ($ShaderItems -notcontains "mpc") { throw "Phase M needs the mpc shader item (replacement file / game file state, see above)" }
        Install-Config
        $ml = Join-Path $sess "M_tourlist.txt"
        Set-Content $ml "$MStage   # meshlet-frontend.md: one GPU-bound spawn, re-loaded every pass" -Encoding ASCII
        $mtxt = ""; $mPy = Join-Path $ws "re\scripts_gpu\mlp_report.py"
        foreach ($arm in @(@{ n = "M1"; mpc = $false }, @{ n = "M2"; mpc = $true })) {
            Set-ItemState "mpc" $arm.mpc
            if (-not $DryRun) { $qw = Wait-Quiet "phase $($arm.n)" { param($l) L $l } }
            $gk = "Profiler.GPUProf=2;Profiler.GPUProf2_SampleEvery=4;Profiler.GPUProf2_AB=0;Profiler.GPUProf2_Top=100;Profiler.GPUProf2_MinRun=1;" +
                  "Profiler.MeshletProbe=1;Profiler.MeshletProbe_Stats=1;Profiler.MeshletProbe_Top=80;Profiler.MeshletProbe_Dump=60;" +
                  "Profiler.MpcCheck=$MpcCheckChains;Profiler.MpcCheckMaxMB=64;GpuOpt.Barriers=0;Patches.MPCS=0;Profiler.GPUProf_Top=60;Profiler.SP=0"
            $ta = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", (Join-Path $ws "tools\tour\run_tour.ps1"), "-Input", "vpad", "-List", $ml,
                    "-Points", "1", "-DwellSec", "$MDwell", "-Passes", "$MPasses", "-NoProfile", "-NoPause", "-CaptureRoot", (Join-Path $sess $arm.n),
                    "-ExtraKeys", $gk, "-OwnerPid", "$PID", "-MaxMinutes", "45")
            if ($DryRun) { $ta += "-DryRun" } else { $ta += "-NoLock" }
            L "Phase $($arm.n): run_tour (mission $MStage spawn, $MPasses passes x $MDwell s, MeshletProbe + MpcCheck, MPC file $(if ($arm.mpc) { 'mpc' } else { 'SHIPPED' }))"
            Set-TestStatus "Phase $($arm.n): meshlet probe"
            $o = Run-Native powershell $ta
            Set-Content (Join-Path $sess "$($arm.n)_console.txt") $o -Encoding UTF8
            $cm = [regex]::Match($o, "capture folder: (.+)")
            if ($cm.Success) {
                $cap = $cm.Groups[1].Value.Trim()
                $csv = Join-Path $GameDir "Nioh3EngineFix_mlp.csv"
                if (Test-Path $csv) { Move-Item -LiteralPath $csv -Destination (Join-Path $cap "mlp_events.csv") -Force }
                foreach ($lg in @(Get-ChildItem (Join-Path $cap "logs") -Filter "run*.log" -ErrorAction SilentlyContinue | Sort-Object Name)) {
                    $mtxt += "==== $($arm.n) $($lg.Name): mlp_report ====`n" + (Run-Native py @("-I", $mPy, $lg.FullName))
                }
            }
        }
        Set-ItemState "mpc" $false
        $mr = Join-Path $sess "M_report.txt"
        Set-Content $mr $mtxt -Encoding UTF8
        $hl = @($mtxt -split "`n" | Where-Object { $_ -match "^(front-end|MPCK total)" } | Select-Object -First 4)
        $script:summary["M"] = "$mr$(if ($hl.Count) { ' - ' + (($hl | ForEach-Object { $_.Trim() }) -join ' | ') } else { ' - no MLP windows decoded' })"
        L "Phase M: $($script:summary['M'])"
    }

    # ---- Phase B: launch A/B per item ----
    if ($Phases -contains "B" -and $Items.Count) {
        Install-Config -FgOff:(-not $KeepFG)
        Install-Mod
        $plan = Build-Plan $Items; $script:plan = $plan
        $estMin = [Math]::Round($plan.Count * $(if ($DryRun) { 0.1 } else { (150 + $SettleSec + $Windows * ($WindowSec + 4) + 32 + 30) / 60.0 }))
        L "Phase B plan ($($plan.Count) launches, ~$estMin min): $($plan -join ' ')" Cyan
        $fails = @{}
        for ($i = 0; $i -lt $plan.Count; $i++) {
            $arm = $plan[$i]
            if ($arm -ne "BASE" -and $fails[$arm] -ge 2) { L "  skip $arm (2 failed launches)" Yellow; continue }
            $ok = Run-Launch ($i + 1) $arm
            if (-not $ok) { $fails[$arm] = 1 + [int]$fails[$arm] }
        }
        foreach ($it in $ShaderItems) { Set-ItemState $it $false }
        $rep = Join-Path $sess "B_report.txt"
        $o = Run-Native py @("-I", $report, "report", $BDir, "--min-draws", "$MinDraws", "--out", $rep)
        $v = @($o -split "`n" | Where-Object { $_ -match "^\w+: (KEEP|DROP|NEUTRAL|INVALID)" } | ForEach-Object { $_.Trim() })
        $script:summary["B"] = $(if ($v.Count) { ($v | ForEach-Object { ($_ -split "  ")[0] }) -join "; " } else { "no verdicts (see $rep)" })
        L "Phase B report: $rep"; $v | ForEach-Object { L "  $_" }
    }
} catch {
    L "!! $_" Red
    $script:summary["error"] = "$_"
} finally {
    Restore-All
    $para = "PC GPU session $Tag ($(Get-Date -f 'yyyy-MM-dd HH:mm')): " + (($script:summary.GetEnumerator() | ForEach-Object { "$($_.Key): $($_.Value)." }) -join " ") + " Folder: $sess"
    Set-Content (Join-Path $sess "summary.txt") $para -Encoding UTF8
    L "summary: $para"
    Send-Bridge "[GAME-FREE] game=nioh3 from=pc (PC GPU session $Tag finished $(Get-Date -f HH:mm); summary $sess\summary.txt)."
    if ($DryRun) {
        $chk = @()
        foreach ($f in $script:Init.Keys) { if ([IO.File]::ReadAllText((Join-Path $ShaderDir $f)) -ne "ORIG $f") { $chk += "shader $f not restored" } }
        if ((Get-Content $asi -Raw).Trim() -ne "user asi") { $chk += "ASI not restored" }
        if ((Get-Content $cfg -Raw) -notmatch "<settings><FRAME_GENERATION>OFF") { $chk += "config.xml not restored" }
        if ($Phases -contains "B") { $rf = Join-Path $sess "B_report.txt"; $vb = @(if (Test-Path $rf) { Select-String -Path $rf -Pattern "^\w+: (KEEP|DROP|NEUTRAL|INVALID)" }); if ($vb.Count -ne $Items.Count) { $chk += "report has $($vb.Count) verdicts for $($Items.Count) items" } }
        if ($Phases -contains "A" -and -not ($script:summary["A"] -match "^tour done")) { $chk += "phase A tour wrapper: $($script:summary['A'])" }
        if ($script:summary.Contains("error")) { $chk += "error: $($script:summary['error'])" }
        if ($chk.Count) { L "DryRun FAILED: $($chk -join '; ')" Red } else { L "DryRun OK (session $sess)" Green }
    }
    Done-Guard
}
