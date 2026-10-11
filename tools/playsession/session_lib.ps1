# session_lib.ps1 - shared paths and helpers for start_session.ps1 / stop_session.ps1 / monitor.ps1 (tools\playsession\README.md).
# Dot-source after setting the optional overrides ($GameDir, $SaveRoot, $BackupRoot, $CaptureRoot, $StateDir) - the dry-run test points
# them at temp folders so nothing real is touched.
$ws = "C:\Users\Ahmad Hamza\nioh3-engine"
if (-not $GameDir) { $GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3" }
if (-not $SaveRoot) { $SaveRoot = Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3" }
if (-not $BackupRoot) { $BackupRoot = "D:\nioh3-re\save-backups" }
if (-not $CaptureRoot) { $CaptureRoot = Join-Path $ws "captures\playsession" }
if (-not $StateDir) { $StateDir = Join-Path $PSScriptRoot "state" }
$BinAsi = Join-Path $ws "mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi"
if ($env:NIOH3_ASI) { $BinAsi = $env:NIOH3_ASI }   # a branch-worktree build (fastab -Asi); default = the main tree's bin
$PmExe = Join-Path $ws "tools\PresentMon.exe"
# bridge lock identity: $env:NIOH3_LOCK_ID (e.g. pc-xp when several PC agents test the game), default pc
$LockId = if ($env:NIOH3_LOCK_ID) { $env:NIOH3_LOCK_ID } else { "pc" }
$PmSession = "Nioh3PlaySession"
$UoBridge = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$ActiveFile = Join-Path $StateDir "active.json"

# Capture keys written on top of the user's own play ini (everything else in that ini stays as the user had it,
# including AutoContinueLoad). All are measurement-only switches of the mod.
$CaptureKeys = @(
    @("Debug", "FnProbe", "1"),        # remaining-work 10.1 live function probe (1.7.34)
    @("Profiler", "MemStats", "1"),    # VRAM / RAM / commit every 30 s
    @("Profiler", "LoadStats", "1"),   # per-load seconds, CPU, MB read, PSOs
    @("Profiler", "LoadGates", "1"),   # which load gate held each load
    @("Patches", "CpuStats", "1"),     # MQ / PT1 / DL counters
    @("Patches", "TS", "1"),           # pad->present thread timing / latency probes
    @("Patches", "SPR", "1"),          # runtime PSO creates (stutter) every 30 s
    @("Debug", "PadLatency", "1")     # controller input -> Present / GPU-done + the game's pad poll interval (1.7.35; also turns TS on)
)

# 1.7.41: the boss loggers live in the separate add-on Nioh3BossRematch.asi; these keys go into the game folder's
# Nioh3BossRematch.ini when the add-on is installed there (backed up to orig\ and restored by STOP like the mod ini)
$AddonCaptureKeys = @(
    @("Debug", "BossLog", "1"),        # boss fight timeline
    @("Debug", "RematchLog", "1"),     # Battle Scroll / replay / retry events
    @("Debug", "InvLog", "1")          # every inventory add + its caller
)

function Set-IniValue([string[]]$lines, [string]$sec, [string]$key, [string]$val) {
    # same as tools\testmatrix\final_pc.ps1: replace key in [sec] (first copy), else insert at the end of [sec], else append the section
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

function Get-AsiVersion([string]$path) {
    if (-not (Test-Path $path)) { return "none" }
    $t = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($path))
    $m = [regex]::Match($t, 'Nioh3EngineFix (\d+\.\d+\.\d+)')
    if ($m.Success) { return $m.Groups[1].Value }
    $m = [regex]::Match($t, '\b1\.\d+\.\d+\b')
    if ($m.Success) { return $m.Value }
    return "unknown"
}

function Invoke-GameLock([string]$action, [int]$ttl = 0) {
    # returns @{ code = exit code; out = text }. Exit 0 ok, 1 held by someone else (acquire), 2 bridge error
    $env:UO_BRIDGE_TOKEN_FILE = Join-Path $env:APPDATA "uo-bridge\token"
    $env:UO_AGENT_ID = $LockId
    $lk = if ($global:LockKey) { $global:LockKey } else { "nioh3" }   # run_tour -LockKey (tour-mode runs use their own key)
    $a = @("-m", "uo_bridge.gamelock", $action, $lk)
    if ($ttl -gt 0) { $a += @("--ttl", "$ttl") }
    Push-Location $UoBridge
    try { $o = (& py @a 2>&1 | Out-String).Trim(); $c = $LASTEXITCODE } catch { $o = "$_"; $c = 2 } finally { Pop-Location }
    return @{ code = $c; out = $o }
}

function Get-LockHolder {
    $r = Invoke-GameLock "status"
    if ($r.code -ne 0) { return @{ ok = $false; holder = $null; out = $r.out } }
    $m = [regex]::Match($r.out, "'holder':\s*(None|'([^']*)')")
    $h = if ($m.Success -and $m.Groups[2].Success) { $m.Groups[2].Value } else { $null }
    return @{ ok = $m.Success; holder = $h; out = $r.out }
}

function Write-Json($obj, [string]$path) { $obj | ConvertTo-Json -Depth 6 | Set-Content -Path $path -Encoding UTF8 }

function Pause-End([switch]$NoPause) { if (-not $NoPause) { Write-Host ""; Read-Host "Press Enter to close this window" | Out-Null } }

# ---------------------------------------------------------------- STOCK mode (unmodded game) - exact allow-list, never wildcards
# Overrides for the dry run: $StashRoot, $OptiSrc (set before dot-sourcing, like $GameDir).
if (-not $StashRoot) { $StashRoot = "D:\nioh3-re\stock-stash" }
if (-not $OptiSrc) { $OptiSrc = Join-Path $ws "optiscaler\files" }
# our mod + ASI loader (names as found in the game folder; the .bak copies do not load but are listed so a stock folder is clean)
$StockModFiles = @("dinput8.dll", "Nioh3EngineFix.asi", "Nioh3EngineFix.ini", "Nioh3EngineFix.log",
    "Nioh3BossRematch.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch.log", "Nioh3BossRematch_bosses.tsv", "Nioh3BossRematch_verified.tsv",
    "Nioh3EngineFix.asi.1.7.22.bak", "Nioh3EngineFix.asi.1.7.23xmm.bak", "Nioh3EngineFix.asi.1.7.25.bak", "Nioh3EngineFix.asi.1716.bak",
    "Nioh3EngineFix.ini.crashed-abba.bak", "Nioh3EngineFix.ini.pcdefaults.bak", "Nioh3EngineFix.ini.preboss.bak")
$StockModDirs = @("Nioh3EngineFix_shaders")     # optional folder, moved file by file (every file hashed)
# OptiScaler set = the install map of optiscaler\apply.ps1 (game name -> file in optiscaler\files whose hash must match before it is stashed)
$StockOptiFiles = [ordered]@{ "dxgi.dll" = "OptiScaler.dll"; "OptiScaler.ini" = $null; "amd_fidelityfx_dx12.dll" = "amd_fidelityfx_dx12.dll";
    "amd_fidelityfx_upscaler_dx12.dll" = "amd_fidelityfx_upscaler_dx12.dll"; "amd_fidelityfx_framegeneration_dx12.dll" = "amd_fidelityfx_framegeneration_dx12.dll" }

function Get-LeftoverStash {
    # an unfinished stock session = a stash folder with manifest.json and no restored.flag
    if (-not (Test-Path $StashRoot)) { return @() }
    return @(Get-ChildItem $StashRoot -Directory -ErrorAction SilentlyContinue | Where-Object {
        (Test-Path (Join-Path $_.FullName "manifest.json")) -and -not (Test-Path (Join-Path $_.FullName "restored.flag")) } | Sort-Object Name)
}

function Get-RelFiles([string]$root, [string]$relDir) {
    # every file under root/relDir as a path relative to root (no FullName/Substring: robust against 8.3 vs long path spellings)
    foreach ($e in Get-ChildItem -LiteralPath ([IO.Path]::Combine($root, $relDir)) -Force) {
        $r = [IO.Path]::Combine($relDir, $e.Name)
        if ($e.PSIsContainer) { Get-RelFiles $root $r } else { $r }
    }
}

function Get-StockPlan([string]$game, [bool]$keepOpti) {
    $items = New-Object System.Collections.Generic.List[string]; $problems = @()
    foreach ($n in $StockModFiles) { if (Test-Path -LiteralPath (Join-Path $game $n) -PathType Leaf) { $items.Add($n) } }
    foreach ($d in $StockModDirs) {
        $dp = Join-Path $game $d
        if (Test-Path -LiteralPath $dp -PathType Container) {
            foreach ($rel in Get-RelFiles $game $d) { $items.Add($rel) }
        }
    }
    if (-not $keepOpti) {
        foreach ($kv in $StockOptiFiles.GetEnumerator()) {
            $p = Join-Path $game $kv.Key
            if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { continue }
            if ($kv.Value) {   # refuse a same-named file that is not the OptiScaler set (could be a game / driver file)
                $ref = Join-Path $OptiSrc $kv.Value
                if (Test-Path $ref) {
                    if ((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $ref -Algorithm SHA256).Hash) { $problems += "$($kv.Key) is not the OptiScaler-set file from optiscaler\files (hash differs) - left alone"; continue }
                } else { $problems += "cannot verify $($kv.Key): $ref missing - left alone"; continue }
            }
            $items.Add($kv.Key)
        }
    }
    return @{ items = $items.ToArray(); problems = $problems }
}

function Test-StockClean([string]$game, [bool]$keepOpti) {
    # what would still load a mod: the allow-list, plus ANY *.asi (the ASI loader runs it) and dinput8.dll
    $left = @()
    foreach ($n in $StockModFiles) { if (Test-Path -LiteralPath (Join-Path $game $n)) { $left += $n } }
    foreach ($d in $StockModDirs) { if (Test-Path -LiteralPath (Join-Path $game $d)) { $left += $d } }
    if (-not $keepOpti) { foreach ($k in $StockOptiFiles.Keys) { if (Test-Path -LiteralPath (Join-Path $game $k)) { $left += $k } } }
    foreach ($f in Get-ChildItem -LiteralPath $game -Filter "*.asi" -File -ErrorAction SilentlyContinue) { if ($left -notcontains $f.Name) { $left += $f.Name } }
    return $left
}

function Invoke-StockStash([string]$game, [string]$stashDir, [bool]$keepOpti) {
    # Move the allow-list into $stashDir with a manifest (name, size, sha256, original mtime). The manifest is written BEFORE any move
    # (state=stashing) so a crash half-way is restorable. Each file: copy, verify hash, delete the original.
    $plan = Get-StockPlan $game $keepOpti
    if ($plan.problems) { throw ("refusing to run STOCK, nothing moved: " + ($plan.problems -join "; ")) }
    New-Item -ItemType Directory -Force (Join-Path $stashDir "files") | Out-Null
    $ents = @()
    foreach ($rel in $plan.items) {
        $p = Join-Path $game $rel; $fi = Get-Item -LiteralPath $p
        $ents += [ordered]@{ name = $rel; size = $fi.Length; sha256 = (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash; mtime_utc = $fi.LastWriteTimeUtc.ToString("o") }
    }
    $man = [ordered]@{ created = (Get-Date).ToString("s"); game_dir = $game; keep_optiscaler = $keepOpti; state = "stashing"; files = $ents; notes = $plan.problems }
    Write-Json $man (Join-Path $stashDir "manifest.json")
    foreach ($e in $ents) {
        $src = Join-Path $game $e.name; $dst = Join-Path (Join-Path $stashDir "files") $e.name
        New-Item -ItemType Directory -Force (Split-Path $dst -Parent) | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
        if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $e.sha256) { throw "stash copy of $($e.name) does not match its hash" }
        Remove-Item -LiteralPath $src -Force
    }
    foreach ($d in $StockModDirs) {   # drop the now-empty optional dirs
        $dp = Join-Path $game $d
        if ((Test-Path -LiteralPath $dp -PathType Container) -and -not (Get-ChildItem -LiteralPath $dp -Recurse -File -Force)) { Remove-Item -LiteralPath $dp -Recurse -Force }
    }
    $left = Test-StockClean $game $keepOpti
    if ($left.Count) { throw "after stashing, the game folder still has: $($left -join ', ')" }
    $man.state = "stashed"; Write-Json $man (Join-Path $stashDir "manifest.json")
    return @{ count = $ents.Count; names = @($ents | ForEach-Object { $_.name }); problems = $plan.problems }
}

function Invoke-StockRestore([string]$stashDir) {
    # Put every manifest file back. Never overwrites a differing file that is newer than the stashed one (it stays in the stash, reported).
    # Idempotent and crash-safe: a file already back with the right hash is fine; a file in neither place is reported.
    $man = Get-Content (Join-Path $stashDir "manifest.json") -Raw | ConvertFrom-Json
    $game = $man.game_dir; $restored = @(); $already = @(); $problems = @()
    foreach ($e in @($man.files)) {
        $dst = Join-Path $game $e.name; $src = Join-Path (Join-Path $stashDir "files") $e.name
        $mt = [datetime]::Parse($e.mtime_utc, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()
        if (-not (Test-Path -LiteralPath $src)) {
            if ((Test-Path -LiteralPath $dst) -and (Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -eq $e.sha256) { $already += $e.name }
            else { $problems += "$($e.name): not in the stash and not in the game folder (crash before it was copied?)" }
            continue
        }
        if ((Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash -ne $e.sha256) { $problems += "$($e.name): stash copy hash differs from the manifest - left in the stash"; continue }
        if (Test-Path -LiteralPath $dst) {
            if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -eq $e.sha256) { Remove-Item -LiteralPath $src -Force; $already += $e.name; continue }
            if ((Get-Item -LiteralPath $dst).LastWriteTimeUtc -gt $mt) {
                $problems += "$($e.name): a different, NEWER file is in the game folder now - not overwritten; the stashed copy stays in $stashDir\files"; continue }
            $rep = Join-Path $stashDir "replaced"; New-Item -ItemType Directory -Force $rep | Out-Null
            Move-Item -LiteralPath $dst -Destination (Join-Path $rep ($e.name -replace '[\\/]', '__')) -Force
        }
        New-Item -ItemType Directory -Force (Split-Path $dst -Parent) | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
        if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $e.sha256) { $problems += "$($e.name): restore hash mismatch"; continue }
        (Get-Item -LiteralPath $dst).LastWriteTimeUtc = $mt
        Remove-Item -LiteralPath $src -Force; $restored += $e.name
    }
    if (-not $problems) {
        Set-Content (Join-Path $stashDir "restored.flag") (Get-Date).ToString("s") -Encoding ASCII
        Remove-Item -LiteralPath (Join-Path $stashDir "files") -Recurse -Force -ErrorAction SilentlyContinue
    }
    return @{ restored = $restored; already = $already; problems = $problems }
}
