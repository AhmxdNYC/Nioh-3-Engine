<# Launch the installed game for a user recording. No mod/config installation,
   capture, input automation, automatic relaunch, game termination or save restore.
   Uses the existing launch guard and shared Steam-account game lock. #>
param([switch]$Watch, [string]$StatePath = '', [string]$GameLockKey = 'nioh3-pc', [switch]$MemoryTelemetryOnly)
$ErrorActionPreference = 'Stop'
$recordingRoot = Join-Path $env:LOCALAPPDATA 'nioh3-codex'
if (-not $StatePath) { $StatePath = Join-Path $recordingRoot 'user-recording.json' }
. (Join-Path $PSScriptRoot 'session_lib.ps1')
$LockId = 'codex-user-recording'
$global:LockKey = $GameLockKey

if ($Watch) {
    $recordingState = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
    $recordingState.watcherPid = $PID
    $recordingState | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $StatePath -Encoding UTF8
    $recordingStart = [datetime]::Parse($recordingState.gameStart)
    while ($true) {
        $recordingGame = Get-Process -Id $recordingState.gamePid -ErrorAction SilentlyContinue
        if (-not $recordingGame -or $recordingGame.StartTime.ToUniversalTime() -ne $recordingStart.ToUniversalTime()) { break }
        $renewal = Invoke-GameLock 'renew' 3600
        if ($renewal.code -ne 0) {
            Add-Content -LiteralPath (Join-Path $recordingRoot 'user-recording-watch.log') -Value ('Lock renewal failed at ' + (Get-Date -Format o))
        }
        Start-Sleep -Seconds 30
    }
    $release = Invoke-GameLock 'release'
    $recordingState.phase = if ($release.code -eq 0) { 'game-exited' } else { 'game-exited-lock-release-failed' }
    $recordingState | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $StatePath -Encoding UTF8
    exit $(if ($release.code -eq 0) { 0 } else { 1 })
}

if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Nioh3 is already running; no second launch.' }
if (Get-Process -Name 'ghidra*','java' -ErrorAction SilentlyContinue) { throw 'Analysis process present; no game launch.' }
if (-not (Test-Path -LiteralPath (Join-Path $GameDir 'Nioh3.exe'))) { throw 'Installed game executable missing.' }
if (Test-Path -LiteralPath $StatePath) {
    $previousRecording = Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json
    if ($previousRecording.phase -ne 'game-exited') { throw 'Previous recording ownership needs inspection.' }
}
New-Item -ItemType Directory -Path $recordingRoot -Force | Out-Null
$leaseHeld = $false
$launchIssued = $false
. (Join-Path $PSScriptRoot '..\testmatrix\harness_guard.ps1')
$launchHeadroom = if ($MemoryTelemetryOnly) { 0 } else { 18 }
if ($MemoryTelemetryOnly) { $env:HG_MIN_HEADROOM_GB = '0'; $env:HG_MIN_CURRENT_GB = '0' }
Init-Guard -Name 'user_recording_launch' -OwnerPid $PID -MaxMinutes 10 -MinHeadroomGB $launchHeadroom -Cleanup {
    if ($leaseHeld -and -not $launchIssued) { $released = Invoke-GameLock 'release'; $leaseHeld = ($released.code -ne 0) }
}
try {
    Assert-Launch
    $acquire = Invoke-GameLock 'acquire' 3600
    if ($acquire.code -ne 0) { throw ('Shared game lock unavailable: ' + $acquire.out) }
    $leaseHeld = $true
    if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Nioh3 appeared during preparation; no second launch.' }
    if (Get-Process -Name 'ghidra*','java' -ErrorAction SilentlyContinue) { throw 'Analysis process appeared during preparation.' }

    $recordingBackup = Join-Path $BackupRoot ((Get-Date -Format 'yyyyMMdd-HHmmss') + '-user-recording')
    if (Test-Path -LiteralPath $recordingBackup) { throw 'Recording backup already exists; refusing overwrite.' }
    New-Item -ItemType Directory -Path $recordingBackup | Out-Null
    if (Test-Path -LiteralPath $SaveRoot) { Copy-Item -LiteralPath $SaveRoot -Destination (Join-Path $recordingBackup 'saves') -Recurse }
    $installedHashes = @()
    foreach ($recordingFile in @('Nioh3EngineFix.asi','Nioh3EngineFix.ini','Nioh3BossRematch.asi','Nioh3BossRematch.ini')) {
        $recordingPath = Join-Path $GameDir $recordingFile
        if (Test-Path -LiteralPath $recordingPath) {
            Copy-Item -LiteralPath $recordingPath -Destination (Join-Path $recordingBackup $recordingFile)
            $installedHashes += [pscustomobject]@{ name = $recordingFile; sha256 = (Get-FileHash -LiteralPath $recordingPath -Algorithm SHA256).Hash }
        }
    }
    $installedHashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $recordingBackup 'installed-hashes.json') -Encoding UTF8
    $saveHashes = @(Get-ChildItem -LiteralPath $SaveRoot -File -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
        [pscustomobject]@{ relative = $_.FullName.Substring($SaveRoot.Length).TrimStart('\'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
    $saveHashes | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $recordingBackup 'entry-save-hashes.json') -Encoding UTF8
    foreach ($entry in $saveHashes) {
        $copyPath = Join-Path (Join-Path $recordingBackup 'saves') $entry.relative
        if ((Get-FileHash -LiteralPath $copyPath -Algorithm SHA256).Hash -ne $entry.sha256) { throw 'Save backup hash mismatch.' }
    }

    $recordingState = [ordered]@{ phase = 'launch-pending'; gamePid = $null; gameStart = $null; watcherPid = $null; backup = $recordingBackup; started = (Get-Date -Format o); owner = $LockId }
    $recordingState | ConvertTo-Json | Set-Content -LiteralPath $StatePath -Encoding UTF8
    $launchIssued = $true
    Start-Process -FilePath 'C:\Program Files (x86)\Steam\steam.exe' -ArgumentList '-applaunch 3681010' -WindowStyle Hidden
    $startDeadline = (Get-Date).AddSeconds(120)
    do {
        Start-Sleep -Seconds 2
        $recordingGame = @(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue)
    } while ($recordingGame.Count -eq 0 -and (Get-Date) -lt $startDeadline)
    if ($recordingGame.Count -ne 1) { throw 'Game startup unconfirmed; retain lock and inspect before any retry.' }
    $recordingState.phase = 'running'
    $recordingState.gamePid = $recordingGame[0].Id
    $recordingState.gameStart = $recordingGame[0].StartTime.ToUniversalTime().ToString('o')
    $recordingState | ConvertTo-Json | Set-Content -LiteralPath $StatePath -Encoding UTF8
    $watchArgs = '-NoProfile -ExecutionPolicy Bypass -File "{0}" -Watch -StatePath "{1}" -GameLockKey "{2}"' -f $PSCommandPath, $StatePath, $GameLockKey
    $recordingWatcher = Start-Process powershell -ArgumentList $watchArgs -WindowStyle Hidden -PassThru
    Write-Output ('RECORDING READY: Nioh3 PID ' + $recordingGame[0].Id + '; installed files preserved; backup ' + $recordingBackup)
} finally {
    if ($leaseHeld -and -not $launchIssued) { $released = Invoke-GameLock 'release'; $leaseHeld = ($released.code -ne 0) }
    Done-Guard
}
