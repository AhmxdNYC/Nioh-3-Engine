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
$PmExe = Join-Path $ws "tools\PresentMon.exe"
$PmSession = "Nioh3PlaySession"
$UoBridge = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$ActiveFile = Join-Path $StateDir "active.json"

# Capture keys written on top of the user's own play ini (everything else in that ini stays as the user had it,
# including AutoContinueLoad). All are measurement-only switches of the mod.
$CaptureKeys = @(
    @("Debug", "FnProbe", "1"),        # remaining-work 10.1 live function probe (1.7.34)
    @("Debug", "BossLog", "1"),        # boss fight timeline
    @("Debug", "RematchLog", "1"),     # Battle Scroll / replay / retry events
    @("Profiler", "MemStats", "1"),    # VRAM / RAM / commit every 30 s
    @("Profiler", "LoadStats", "1"),   # per-load seconds, CPU, MB read, PSOs
    @("Profiler", "LoadGates", "1"),   # which load gate held each load
    @("Patches", "CpuStats", "1"),     # MQ / PT1 / DL counters
    @("Patches", "TS", "1"),           # pad->present thread timing / latency probes
    @("Patches", "SPR", "1"),          # runtime PSO creates (stutter) every 30 s
    @("Debug", "PadLatency", "1")     # controller input -> Present / GPU-done + the game's pad poll interval (1.7.35; also turns TS on)
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
    $env:UO_AGENT_ID = "pc"
    $a = @("-m", "uo_bridge.gamelock", $action, "nioh3")
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
