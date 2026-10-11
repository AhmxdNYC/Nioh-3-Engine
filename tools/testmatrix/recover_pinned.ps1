# Root-owned recovery after a guarded process exits without running finally.
param([Parameter(Mandatory)][string]$StatePath, [switch]$SourceOwnershipConfirmed)
$ErrorActionPreference='Stop'
if (-not $SourceOwnershipConfirmed) { throw 'Root ownership confirmation required for recovery' }
. (Join-Path $PSScriptRoot 'pinned_benchmark_lib.ps1')
. (Join-Path $PSScriptRoot 'pinned_operator.ps1')
$entry=Read-PBState $StatePath
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw 'Quit the game before recovery' }
if (-not $entry.coordinator_marker -or -not (Test-Path -LiteralPath $entry.coordinator_marker.path)) { throw 'Owned recovery marker missing; refuse stale-state restore' }
$current=Get-Content -LiteralPath $entry.coordinator_marker.path -Raw | ConvertFrom-Json
if ($current.token -cne $entry.coordinator_marker.token) { throw 'Recovery marker belongs to another run' }
Wait-PBWorkerIdle (Join-Path (Split-Path -Parent $entry.coordinator_marker.path) 'state.json') 60 $true
$bridgeDir='C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge'
$locked=$false
try {
    Push-Location $bridgeDir
    try { py -m uo_bridge.gamelock acquire nioh3 --ttl 600 | Out-Null; if ($LASTEXITCODE -ne 0) { throw 'Recovery game lock unavailable' }; $locked=$true }
    finally { Pop-Location }
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw 'Game started during recovery; leave state intact' }
    Restore-PBState $entry
    Push-Location $bridgeDir
    try { py -m uo_bridge.gamelock release nioh3 | Out-Null; if ($LASTEXITCODE -ne 0) { throw 'Recovery game lock release failed' }; $locked=$false }
    finally { Pop-Location }
    Remove-PBMarker $entry.coordinator_marker
    [IO.File]::WriteAllText((Join-Path $entry.output 'restored.txt'),'Entry save/ASI/INI/log hashes verified by root recovery')
    $operator=Join-Path $entry.output 'operator'
    if (Test-Path -LiteralPath $operator) { Write-PBOperatorJson (Join-Path $operator 'finished.json') @{restored=$true;complete=$false;recovered=$true;finished=(Get-Date).ToString('o')} }
    'Entry hashes restored; owned marker and game lock released'
} finally {
    if ($locked) {
        Push-Location $bridgeDir
        try { py -m uo_bridge.gamelock release nioh3 | Out-Null }
        finally { Pop-Location }
    }
}
