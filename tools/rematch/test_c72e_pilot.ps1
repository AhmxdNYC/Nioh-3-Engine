# Lightweight isolated fixtures: no game, bridge, installed writes, source compilation or ASI changes.
param([string]$FixtureRoot='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
$script:checks=0
function Check([bool]$Condition,[string]$Name) { if (-not $Condition) { throw "FAIL: $Name" }; $script:checks++; Write-Host "PASS: $Name" }
if (-not $FixtureRoot) { $FixtureRoot=Join-Path ([IO.Path]::GetTempPath()) ('c72e-fixture-'+[guid]::NewGuid().ToString('N')) }
$FixtureRoot=C72-Full $FixtureRoot
if (Test-Path -LiteralPath $FixtureRoot) { throw 'Fixture directory must be new' }
$live=Join-Path $FixtureRoot 'live';$backup=Join-Path $FixtureRoot 'backup'
$null=New-Item -ItemType Directory -Path (Join-Path $live 'Savedata\test\SAVEDATA00') -Force
$save=Join-Path $live 'Savedata\test\SAVEDATA00\SAVEDATA.BIN'
[IO.File]::WriteAllText($save,'CURRENT_USER_ENTRY_AFTER_PLAY')
[IO.File]::WriteAllText((Join-Path $live 'config.xml'),'original current config')
$entry=C72-Manifest $live
C72-Backup $live $backup $entry
Check (C72-Same $entry (C72-Manifest $backup)) 'Fresh entry backup hashes match'
$handles=$null
try {
    $handles=C72-LockSaves $live $entry
    Check ([IO.File]::ReadAllText($save) -eq 'CURRENT_USER_ENTRY_AFTER_PLAY') 'Sharing locks permit read-only loading'
    $denied=$false;try { [IO.File]::WriteAllText($save,'trial progress') } catch [IO.IOException] { $denied=$true }
    Check $denied 'Sharing locks deny overwrite'
    $denied=$false;try { [IO.File]::Delete($save) } catch [IO.IOException] { $denied=$true }
    Check $denied 'Sharing locks deny deletion/replacement'
    [IO.File]::WriteAllText((Join-Path $live 'new-save-temp.bin'),'possible new file')
    Check (-not (C72-Same $entry (C72-Manifest $live))) 'Manifest detects new files while original saves remain locked'
} finally { C72-UnlockSaves $handles }
[IO.File]::WriteAllText($save,'mutation after releasing fixture locks')
Remove-Item -LiteralPath (Join-Path $live 'config.xml')
C72-Restore $live $backup $entry
Check (C72-Same $entry (C72-Manifest $live)) 'Exact restore handles changed/missing/added files'
$denied=$false;try { $null=C72-Child $live '..\escaped.bin' } catch { $denied=$true }
Check $denied 'Computed child path escape refused'
[IO.File]::WriteAllText((Join-Path $backup 'config.xml'),'tampered backup')
$denied=$false;try { C72-Restore $live $backup $entry } catch { $denied=$true }
Check $denied 'Tampered backup refused before restoration'
$native='state=2 row=RC72E fights=0 infight=0 emaki=0 loading=0 player=1 stage=334140 mission=0xC72E fnow=armed warps=0 loadms=0 spawn=- pos=207629,6027,142678 gate=rematch active placements=[] last=started'
Check (C72-FirstCandidate $native $true 334140) 'Native first observed proxy after load accepted'
Check (-not (C72-FirstCandidate $native $false 334140)) 'Pre-load status is never treated as landing'
Check (-not (C72-FirstCandidate ($native.Replace('gate=rematch active','gate=game paused')) $true 334140)) 'Paused player status rejected'
Check (-not (C72-FirstCandidate ($native.Replace('0xC72E','0x0034')) $true 334140)) 'Wrong mission status rejected'
$land=C72-Landing $native @(207628.53125,6027.47998,142677.921875)
Check ($land.before_gate -and $land.near_target) 'Native target proxy is separate from eventual fight'
$land=C72-Landing ($native.Replace('fnow=armed','fnow=pass')) @(207629,6027,142678)
Check (-not $land.before_gate) 'Existing F PASS cannot prove pre-gate landing'
$land=C72-Landing ($native.Replace('207629,6027,142678','209710,5881,139371')) @(207629,6027,142678)
Check (-not $land.near_target) 'Shrine landing fails native target criterion'
$land=C72-Landing ($native.Replace('207629,6027,142678','207073,6026,142372')) @(207629,6027,142678)
Check (-not $land.near_target) 'Later script destination is not confused with native first landing'
$denied=$false;try { $null=C72-Landing ($native.Replace('207629,6027,142678','NaN,6027,142678')) @(207629,6027,142678) } catch { $denied=$true }
Check $denied 'Nonfinite telemetry rejected'
$denied=$false;try { C72-UniqueIni @('[Rematch]','FightNow=0','FightNow=1') @('Rematch') } catch { $denied=$true }
Check $denied 'Duplicate INI keys refused'
foreach ($file in @('c72e_pilot_lib.ps1','run_c72e_native_pilot.ps1')) {
    $tokens=$null;$errors=$null
    $null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors)
    Check (-not $errors) "PowerShell parser: $file"
}
Write-Host "ALL $($script:checks) FIXTURE CHECKS PASSED; artifacts retained at $FixtureRoot"
