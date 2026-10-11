# Offline temp fixtures only. Never calls the bridge or a game/graphics executable.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'pinned_benchmark_lib.ps1')
$script:checks = 0
function Check([bool]$Condition, [string]$Message) { if (-not $Condition) { throw "FAIL: $Message" }; $script:checks++ }
function Throws([scriptblock]$Body, [string]$Message) { $failed=$false; try { & $Body } catch { $failed=$true }; Check $failed $Message }
$testRoot = Join-Path $env:TEMP ('nioh3-pinned-test-' + [guid]::NewGuid().ToString('N'))
$game = Join-Path $testRoot 'game'; $save = Join-Path $testRoot 'save'; $out = Join-Path $testRoot 'run'
New-Item -ItemType Directory -Path $game,$save -Force | Out-Null
try {
    Set-Content -LiteralPath (Join-Path $game 'Nioh3EngineFix.asi') -Value 'original old installed ASI' -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $game 'Nioh3EngineFix.ini') -Value 'entry ini, not stale orig' -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $game 'dxgi.dll') -Value 'unrelated loader unchanged' -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $save 'config.xml') -Value '<FRAME_GENERATION>OFF</FRAME_GENERATION>' -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $save 'slot.sav') -Value 'SAVE ORIGINAL' -Encoding ASCII
    $entryGame = Get-Manifest $game; $entrySave = Get-Manifest $save
    Assert-PBPreflight $game $save (Join-Path $save 'config.xml') $false
    Throws { Assert-PBPreflight $game $save (Join-Path $save 'config.xml') $true } 'existing game refused'
    Set-Content -LiteralPath (Join-Path $game 'Nioh3EngineFix.asi.stockoff') -Value 'leftover'
    Throws { Assert-PBPreflight $game $save (Join-Path $save 'config.xml') $false } 'stale stockoff refused'
    Remove-Item -LiteralPath (Join-Path $game 'Nioh3EngineFix.asi.stockoff')
    Throws { Assert-PBChild $save (Join-Path $game 'escape') } 'outside-root operation refused'
    $state = New-PBState $game $save $out
    # Model a failure after shader/config setup and partial gameplay/autosave.
    Remove-Item -LiteralPath (Join-Path $game 'Nioh3EngineFix.asi')
    Set-Content -LiteralPath (Join-Path $game 'Nioh3EngineFix.ini') -Value 'pinned temp configuration'
    Set-Content -LiteralPath (Join-Path $game 'Nioh3EngineFix.log') -Value 'new capture log'
    Remove-Item -LiteralPath (Join-Path $save 'slot.sav')
    Set-Content -LiteralPath (Join-Path $save 'config.xml') -Value 'changed during gameplay'
    Set-Content -LiteralPath (Join-Path $save 'new.sav') -Value 'new autosave'
    Restore-PBState $state
    $diff = Compare-Manifest $entrySave (Get-Manifest $save)
    Check (($diff.changed.Count+$diff.added.Count+$diff.removed.Count) -eq 0) 'changed/deleted/added save files restored by hashes'
    $diff = Compare-Manifest $entryGame (Get-Manifest $game)
    Check (($diff.changed.Count+$diff.added.Count+$diff.removed.Count) -eq 0) 'ASI/INI/log/unchanged loader restored exactly'
    Set-Content -LiteralPath (Join-Path $save 'slot.sav') -Value 'crash/restart changed save'
    Restore-PBState (Read-PBState (Join-Path $out 'entry-state.json'))
    $diff = Compare-Manifest $entrySave (Get-Manifest $save)
    Check (($diff.changed.Count+$diff.added.Count+$diff.removed.Count) -eq 0) 'persisted entry manifest supports recovery after shell loss'
    Set-Content -LiteralPath (Join-Path $out 'entry\save\slot.sav') -Value 'corrupted backup'
    Throws { Restore-PBState $state } 'corrupt backup cannot be accepted as recovery'
    $plan = @(New-PBPlan @('STOCK','OPTIMIZED') @('USER') 3)
    Check (($plan.cfg -join ',') -eq 'STOCK,OPTIMIZED,OPTIMIZED,STOCK,STOCK,OPTIMIZED') 'three balanced pairs AB|BA|AB'
    Check (@($plan.id | Select-Object -Unique).Count -eq 6) 'balanced repeated arms have unique capture IDs'
    $windows = @([pscustomobject]@{kind='stand';ok=$true;requested_s=20;duration_s=20},
        [pscustomobject]@{kind='stand';ok=$true;requested_s=20;duration_s=20},
        [pscustomobject]@{kind='move';ok=$true;requested_s=20;duration_s=20},
        [pscustomobject]@{kind='combat';ok=$true;requested_s=25;duration_s=25})
    Check (Test-PBWindows $windows 2 $true) 'complete capture accepted'
    Check (-not (Test-PBWindows $windows[0..2] 2 $true)) 'missing combat rejected'
    $windows[1].duration_s=5
    Check (-not (Test-PBWindows $windows 2 $true)) 'short second stand rejected despite other good windows'
    $windows[1].duration_s=20; $windows[2].ok=$false
    Check (-not (Test-PBWindows $windows 2 $true)) 'failed move rejected'
    $csvFixture=Join-Path $testRoot 'short.csv'
    Set-Content -LiteralPath $csvFixture -Value "MsBetweenPresents`n20000`n100`n100" -Encoding ASCII
    Check ((Get-PBCaptureDuration $csvFixture) -eq .2) 'pre-capture first interval cannot disguise a short CSV'
    $coord=Join-Path $testRoot 'coordinator'
    $marker=New-PBMarker $coord
    $markerHash=(Get-FileHash -LiteralPath $marker.path -Algorithm SHA256).Hash
    Throws { New-PBMarker $coord } 'active marker exclusive creation refuses second benchmark'
    Check ((Get-FileHash -LiteralPath $marker.path -Algorithm SHA256).Hash -eq $markerHash) 'existing marker never overwritten'
    Throws { Remove-PBMarker ([pscustomobject]@{path=$marker.path;token='foreign'}) } 'foreign marker cannot be removed'
    $controller=Join-Path $coord 'state.json'
    Set-Content -LiteralPath $controller -Value '{"phase":"working","worker_pid":1234}'
    Throws { Wait-PBWorkerIdle $controller 0 $true {param($p) $true} } 'live controller worker blocks before preparation'
    Set-Content -LiteralPath $controller -Value 'broken-json'
    Throws { Wait-PBWorkerIdle $controller 0 $true } 'unknown controller ownership blocks'
    Set-Content -LiteralPath $controller -Value '{"phase":"unknown","worker_pid":null}'
    Throws { Wait-PBWorkerIdle $controller 0 $true } 'unknown phase blocked even with no worker PID'
    Set-Content -LiteralPath $controller -Value '{"phase":"ready","worker_pid":null}'
    Wait-PBWorkerIdle $controller 0 $true
    Throws { Wait-PBWorkerIdle $controller 0 $false } 'explicit root source ownership required'
    Remove-PBMarker $marker
    Check (-not (Test-Path -LiteralPath $marker.path)) 'only owned marker removed after verified restore'
    $fakeAsi=Join-Path $testRoot 'pinned.asi'; $fakeIni=Join-Path $testRoot 'pinned.ini'
    Set-Content -LiteralPath $fakeAsi -Value 'Nioh3EngineFix 1.7.61' -Encoding ASCII
    Set-Content -LiteralPath $fakeIni -Value "[GpuOpt]`nFrameOverlap=0`n[ShaderSwap]`nEnabled=0`n[ShaderHot]`nEnabled=0" -Encoding ASCII
    $before=Get-Manifest $testRoot
    $dryOut=Join-Path $testRoot 'dry-should-not-exist'
    $text = & (Join-Path $PSScriptRoot 'pinned_final_pc.ps1') -DryRun -OptimizedIni $fakeIni -AsiPath $fakeAsi `
        -GameDir $game -SaveRoot $save -OutputDir $dryOut | Out-String
    Check ($text -match 'DRY RUN') 'actual driver dry-run executes the plan'
    $diff=Compare-Manifest $before (Get-Manifest $testRoot)
    Check (($diff.changed.Count+$diff.added.Count+$diff.removed.Count) -eq 0 -and -not (Test-Path $dryOut)) 'dry-run creates/modifies no fixture file or output directory'
    $text = & (Join-Path $PSScriptRoot 'final_pc.ps1') -DryRun -OptimizedIni $fakeIni -AsiPath $fakeAsi -NoLatency | Out-String
    Check ($text -match 'DRY RUN' -and $text -notmatch 'KEEP set now') 'legacy entry forwards pinned dry-run without historical BEST'
    foreach ($name in 'final_pc.ps1','pinned_final_pc.ps1','pinned_benchmark_lib.ps1','test_pinned_benchmark.ps1') {
        $tokens=$null; $errors=$null
        $null=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $name),[ref]$tokens,[ref]$errors)
        Check ($errors.Count -eq 0) "PowerShell parse: $name"
    }
    'ALL '+$script:checks+' PINNED BENCHMARK CHECKS PASSED'
} finally {
    # Explicitly checked temp workspace; no shell mixing or computed recursive delete elsewhere.
    $resolved=[IO.Path]::GetFullPath($testRoot)
    if ($resolved.StartsWith([IO.Path]::GetFullPath($env:TEMP).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolved) -like 'nioh3-pinned-test-*') { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
