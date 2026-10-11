<#
Pinned default-versus-EngineFix-absent benchmark. External capture is identical;
loader, OptiScaler, other ASIs and shader files remain unchanged in both arms.
Manual gameplay/route prompts avoid changing default AutoContinueLoad or using
historical BEST flags. No game, lock, executable or file writes during -DryRun.
Quit through the menu when prompted. LOAD2 and latency are separate manual work.
#>
param([string]$OptimizedIni = '', [string]$AsiPath = '', [switch]$DryRun,
      [string[]]$Configs = @('STOCK','OPTIMIZED'), [string[]]$Presets = @('USER','MAX'),
      [ValidateRange(1,20)][int]$Runs = 3, [ValidateRange(1,10)][int]$StandWindows = 2,
      [ValidateRange(10,120)][int]$WindowSec = 20, [ValidateRange(60,600)][int]$SettleSec = 60,
      [switch]$SkipCombat, [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\Nioh3',
      [string]$SaveRoot = '', [string]$ConfigPath = '', [string]$OutputDir = '',
      [string]$PresentMonPath = 'C:\Users\Ahmad Hamza\nioh3-engine\tools\PresentMon.exe',
      [int]$OwnerPid = 0, [ValidateRange(1,480)][int]$MaxMinutes = 180,
      [string]$CoordinationDir = '', [switch]$SourceOwnershipConfirmed,
      [switch]$FilePrompts, [ValidateRange(1,1800)][int]$OperatorTimeoutSec = 600)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'pinned_benchmark_lib.ps1')
. (Join-Path $PSScriptRoot 'pinned_operator.ps1')
. (Join-Path $PSScriptRoot 'gfxlib.ps1')
if (-not $SaveRoot) { $SaveRoot = Join-Path $env:LOCALAPPDATA 'KoeiTecmo\NIOH3' }
if (-not $ConfigPath) { $ConfigPath = Join-Path $SaveRoot 'config.xml' }
Init-Gfx $ConfigPath
if (-not $OutputDir) { $OutputDir = Join-Path $PSScriptRoot ("results\pinned_" + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
if (-not $CoordinationDir) { $CoordinationDir = Join-Path $env:LOCALAPPDATA 'nioh3-codex' }
$Configs = @($Configs | ForEach-Object { $_ -split ',' })
$Presets = @($Presets | ForEach-Object { $_ -split ',' })
$plan = @(New-PBPlan $Configs $Presets $Runs)
foreach ($preset in $Presets) { $null = Get-Preset $preset }
foreach ($path in $OptimizedIni, $AsiPath) {
    if (-not $path -or -not (Test-Path -LiteralPath $path -PathType Leaf)) { throw 'Provide existing -OptimizedIni and -AsiPath' }
}
$OptimizedIni = (Get-Item -LiteralPath $OptimizedIni).FullName
$AsiPath = (Get-Item -LiteralPath $AsiPath).FullName
$iniHash = (Get-FileHash -LiteralPath $OptimizedIni -Algorithm SHA256).Hash
$asiHash = (Get-FileHash -LiteralPath $AsiPath -Algorithm SHA256).Hash
$versionText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($AsiPath))
$version = [regex]::Match($versionText, 'Nioh3EngineFix (\d+\.\d+\.\d+)').Groups[1].Value
if ($version -ne '1.7.61') { throw "Expected tested 1.7.61, got '$version'" }
$iniText = [IO.File]::ReadAllText($OptimizedIni)
foreach ($gate in @(@('GpuOpt','FrameOverlap'), @('ShaderSwap','Enabled'), @('ShaderHot','Enabled'))) {
    $section = [regex]::Match($iniText, '(?ms)^\[' + $gate[0] + '\]\s*\r?\n(.*?)(?=^\[|\z)').Groups[1].Value
    if ($section -notmatch ('(?m)^\s*' + $gate[1] + '\s*=\s*0\s*(?:;[^\r\n]*)?$')) { throw "Pinned mode requires [$($gate[0])] $($gate[1])=0" }
}
$asi = Join-Path $GameDir 'Nioh3EngineFix.asi'; $ini = Join-Path $GameDir 'Nioh3EngineFix.ini'
if ($AsiPath -eq [IO.Path]::GetFullPath($asi) -or $OptimizedIni -eq [IO.Path]::GetFullPath($ini)) { throw 'Pinned input must be outside the destination game files' }
foreach ($inputPath in $AsiPath,$OptimizedIni) {
    if ($inputPath.StartsWith([IO.Path]::GetFullPath($GameDir).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Pinned inputs must be outside the game directory; another source ASI there would contaminate STOCK'
    }
}
$header = [ordered]@{ baseline = 'EngineFix absent; loader/OptiScaler/other ASIs/shaders unchanged'; version = $version;
    asi = $AsiPath; asi_sha256 = $asiHash; optimized_ini = $OptimizedIni; ini_sha256 = $iniHash;
    fg = 'OFF in both arms'; settle_s = $SettleSec; stand_windows = $StandWindows; window_s = $WindowSec;
    combat_s = 25; full_protocol = $false; window_protocol_complete = (-not $SkipCombat); output = $OutputDir;
    file_prompts=[bool]$FilePrompts; operator_directory=(Join-Path $OutputDir 'operator') }
if ($DryRun) {
    $header | ConvertTo-Json
    $plan | Format-Table id,cfg,preset,run | Out-String
    'DRY RUN: no game/lock/executable/file mutation; current-entry snapshots will replace no shared .orig backup.'
    return
}
# Refuse before contacting the lock service or taking any snapshot.
Assert-PBPreflight $GameDir $SaveRoot $ConfigPath ([bool](Get-Process Nioh3 -ErrorAction SilentlyContinue))
if (-not (Test-Path -LiteralPath $PresentMonPath -PathType Leaf)) { throw 'PresentMon missing' }
if (Test-Path -LiteralPath $OutputDir) { throw 'Output directory already exists; choose a fresh directory' }
$bridgeDir = 'C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge'
$script:pbLocked = $false; $script:pbState = $null; $script:pbRestoreOk = $false; $script:pbMarker = $null
$script:pbOperator = $null
$script:pbSuccess = $false
function PB-Ask([string]$Stage, [string]$Prompt, $Context) {
    if (-not $FilePrompts) { return (Read-Host $Prompt) }
    $answer = Wait-PBOperatorAnswer $script:pbOperator $Stage $Prompt $Context $OperatorTimeoutSec { Assert-Owner }
    return (Assert-PBOperatorAnswer $Stage $answer)
}
function PB-Lock([string]$Action) {
    Push-Location $bridgeDir
    try { py -m uo_bridge.gamelock $Action nioh3 --ttl 14400 | Out-Null; if ($LASTEXITCODE -ne 0) { throw "Game lock $Action failed" } }
    finally { Pop-Location }
}
function PB-Unlock {
    if ($script:pbLocked) {
        Push-Location $bridgeDir
        try { py -m uo_bridge.gamelock release nioh3 | Out-Null; if ($LASTEXITCODE -ne 0) { throw 'Game lock release failed' }; $script:pbLocked = $false }
        finally { Pop-Location }
    }
}
function PB-Recover {
    # On failure stop only the process identities registered by this driver.
    # Never kill or overwrite files under an unrelated existing game.
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) {
        if ($global:HG) {
            foreach ($owned in @($global:HG.games)) {
                $process = Get-Process -Id $owned.pid -ErrorAction SilentlyContinue
                if ($process -and $process.StartTime.ToString('o') -eq $owned.start) { $null=$process.CloseMainWindow() }
            }
        }
        for ($i=0; $i -lt 10 -and (Get-Process Nioh3 -ErrorAction SilentlyContinue); $i++) { [Threading.Thread]::Sleep(1000) }
        if ((Get-Process Nioh3 -ErrorAction SilentlyContinue) -and $global:HG) { Kill-Registered }
        if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Unowned game still running; quit it, then recover $OutputDir entry-state.json; lock retained" }
    }
    if ($script:pbState) { Restore-PBState $script:pbState; $script:pbRestoreOk = $true }
    PB-Unlock
    # harness_guard may call this cleanup then Environment.Exit, bypassing finally.
    # Mark successful restoration and release our hold here, on that path too.
    if ($script:pbRestoreOk -or -not $script:pbState) {
        if ($script:pbRestoreOk) { 'Entry save/ASI/INI/log hashes restored' | Set-Content -LiteralPath (Join-Path $OutputDir 'restored.txt') }
        if ($script:pbMarker) { Remove-PBMarker $script:pbMarker; $script:pbMarker=$null }
    }
    if ($script:pbOperator) { Write-PBOperatorJson (Join-Path $script:pbOperator.directory 'finished.json') @{session=$script:pbOperator.session;restored=$script:pbRestoreOk;complete=$script:pbSuccess;finished=(Get-Date).ToString('o')} }
}
function PB-RequireQuit {
    $null = PB-Ask 'quit' 'Quit Nioh3 through its menu; acknowledge only after it exits' @{cell=$cell.id}
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw 'Game remains running; restoration deferred, lock retained' }
    Note-Closed
}
function PB-Capture([string]$Id, [string]$Kind, [int]$Number) {
    $seconds = if ($Kind -eq 'combat') { 25 } else { $WindowSec }
    $null = PB-Ask 'window-ready' "Ready in gameplay for $Kind $Number (${seconds}s); same view/route/fight. Acknowledge after observation" @{cell=$Id;kind=$Kind;number=$Number;seconds=$seconds}
    Assert-Owner
    if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw 'Game exited before capture' }
    $csv = Join-Path $OutputDir "${Id}_${Kind}${Number}.csv"
    $args = @('--process_name','Nioh3.exe','--output_file',('"' + $csv + '"'),'--timed',"$seconds",
        '--terminate_after_timed','--stop_existing_session','--no_console_stats','--v2_metrics')
    $begin = Get-Date
    $pm = Start-Process -FilePath $PresentMonPath -ArgumentList $args -WindowStyle Hidden -PassThru
    Register-Child $pm 'PresentMon'
    while (-not $pm.HasExited) { Assert-Owner; Start-Sleep 1; $pm.Refresh(); if (((Get-Date)-$begin).TotalSeconds -gt $seconds+30) { throw 'PresentMon timeout' } }
    $summary = py -I (Join-Path $PSScriptRoot 'tm.py') stats $csv | ConvertFrom-Json
    if ($LASTEXITCODE -ne 0) { throw 'Capture statistics failed' }
    # Sum captured present intervals, not wall-clock wait time: a short CSV cannot pass.
    $duration = Get-PBCaptureDuration $csv
    $confirmed = PB-Ask 'window-confirm' 'Was the entire window gameplay with the required identical view/route/fight? Answer YES only after observation' @{cell=$Id;kind=$Kind;number=$Number;csv=$csv}
    $window = [ordered]@{ kind = $Kind; n = $Number; requested_s = $seconds; duration_s = $duration;
        ok = ($pm.ExitCode -eq 0 -and [bool]$summary.ok -and $duration -ge .9*$seconds -and
            [bool](Get-Process Nioh3 -ErrorAction SilentlyContinue) -and $confirmed -ceq 'YES');
        csv = $csv; t0 = $begin.ToString('o'); t1 = (Get-Date).ToString('o') }
    foreach ($property in $summary.PSObject.Properties) { if ($property.Name -ne 'ok') { $window[$property.Name] = $property.Value } }
    return [pscustomobject]$window
}
try {
    $script:pbMarker = New-PBMarker $CoordinationDir
    Wait-PBWorkerIdle (Join-Path $CoordinationDir 'state.json') ($MaxMinutes*60) ([bool]$SourceOwnershipConfirmed)
    PB-Lock acquire; $script:pbLocked = $true
    # Recheck after lock acquisition, before the first game/save file mutation.
    Assert-PBPreflight $GameDir $SaveRoot $ConfigPath ([bool](Get-Process Nioh3 -ErrorAction SilentlyContinue))
    $script:pbState = New-PBState $GameDir $SaveRoot $OutputDir
    $script:pbState | Add-Member -NotePropertyName coordinator_marker -NotePropertyValue $script:pbMarker
    $script:pbState | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputDir 'entry-state.json') -Encoding UTF8
    $header | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDir 'manifest.json') -Encoding UTF8
    if ($FilePrompts) { $script:pbOperator = New-PBOperator (Join-Path $OutputDir 'operator') }
    . (Join-Path $PSScriptRoot 'harness_guard.ps1')
    Init-Guard -Name 'pinned_final_pc' -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { PB-Recover }
    foreach ($cell in $plan) {
        Assert-Owner; PB-Lock renew
        Wait-PBWorkerIdle (Join-Path $CoordinationDir 'state.json') ($MaxMinutes*60) ([bool]$SourceOwnershipConfirmed)
        Assert-PBPreflight $GameDir $SaveRoot $ConfigPath ([bool](Get-Process Nioh3 -ErrorAction SilentlyContinue))
        Restore-PBState $script:pbState # same entry save/settings for every launch
        if ($asiHash -ne (Get-FileHash -LiteralPath $AsiPath -Algorithm SHA256).Hash -or
            $iniHash -ne (Get-FileHash -LiteralPath $OptimizedIni -Algorithm SHA256).Hash) { throw 'Pinned input changed during benchmark' }
        $null = Apply-Gfx (Get-Preset $cell.preset) $false
        $null = Set-FgMode 'OFF' $false
        if ($cell.cfg -eq 'STOCK') { if (Test-Path -LiteralPath $asi) { Remove-Item -LiteralPath $asi -Force } }
        else { Copy-Item -LiteralPath $AsiPath -Destination $asi -Force; Copy-Item -LiteralPath $OptimizedIni -Destination $ini -Force }
        $graphics = [IO.File]::ReadAllText($ConfigPath)
        $fgConfigured = Assert-PBFgOff $graphics
        Assert-Launch
        $started = Get-Date; Start-Process 'steam://rungameid/3681010'
        while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
            Assert-Owner; Start-Sleep 1
            if (((Get-Date)-$started).TotalSeconds -gt 180) { throw 'Game launch timeout' }
        }
        Register-Game
        $null = PB-Ask 'gameplay-ready' "$($cell.id): load SAME save/view. Acknowledge after observing controllable HUD gameplay" @{cell=$cell.id;cfg=$cell.cfg;preset=$cell.preset}
        Assert-Owner
        if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw 'Game never reached gameplay' }
        $load1 = ((Get-Date)-$started).TotalSeconds
        Start-Sleep $SettleSec
        $windows = @()
        for ($n=1; $n -le $StandWindows; $n++) { $windows += PB-Capture $cell.id 'stand' $n }
        $windows += PB-Capture $cell.id 'move' 1
        if (-not $SkipCombat) { $windows += PB-Capture $cell.id 'combat' 1 }
        $valid = Test-PBWindows $windows $StandWindows (-not $SkipCombat)
        [ordered]@{ id=$cell.id; device='pc'; cfg=$cell.cfg; preset=$cell.preset; run=$cell.run;
            valid=$valid; full_protocol=$false; window_protocol_complete=(-not $SkipCombat); windows=$windows; load1_s=$load1; load2_s=$null; hitches=$null;
            input=$header; graphics_xml=$graphics;fg_configured=$fgConfigured; timing='LOAD1 includes operator acknowledgement; LOAD2/hitches pending manual protocol' } |
            ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputDir ($cell.id+'.json')) -Encoding UTF8
        PB-RequireQuit
        $log = Join-Path $GameDir 'Nioh3EngineFix.log'
        if ($cell.cfg -eq 'OPTIMIZED' -and (Test-Path -LiteralPath $log)) { Copy-Item -LiteralPath $log -Destination (Join-Path $OutputDir ($cell.id+'.log')) }
        Restore-PBState $script:pbState
        if (-not $valid) { throw 'Required window invalid; stop instead of reporting a partial benchmark as valid' }
    }
    $script:pbSuccess=$true
} finally {
    PB-Recover
    if ($global:HG) { Done-Guard }
}
