# Actual Finish-C72 and Guard-Fail in isolated child processes; no game/bridge access.
param([string]$Case = '', [string]$Root = '')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
function Import-Function([string]$Path, [string]$Name) {
    $tokens=$null; $errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($Path,[ref]$tokens,[ref]$errors)
    if ($errors) { throw "Parse failure: $Path" }
    $fn=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $Name},$true)
    if ($fn) { Invoke-Expression ($fn.Extent.Text -replace '^function\s+', 'function script:') }
}
if ($Case) {
    . (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
    Import-Function (Join-Path $repo 'tools\testmatrix\harness_guard.ps1') 'Guard-Fail'
    $pilot=Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1'
    Import-Function $pilot 'Guard-Fail'
    Import-Function $pilot 'Finish-C72'
    function HG-Log([string]$Text) { Write-Host $Text }
    function Clear-MeasureLock {}
    function Kill-Registered { [IO.File]::AppendAllText((Join-Path $Root 'kill-called'),'called') }
    function Close-Owned {
        [IO.File]::AppendAllText((Join-Path $Root 'close-count'),'1')
        if ($Case -eq 'close-failure') { throw 'injected graceful-close failure' }
    }
    function Get-Process { if ($Case -eq 'foreign-game') { return @{Id=789} } }
    function Check-Asis {}
    function Invoke-GameLock {
        [IO.File]::AppendAllText((Join-Path $Root 'released-locks'),$global:LockKey+"`n")
        return @{code=0}
    }
    $OutDir=$Root; $SaveRoot=Join-Path $Root 'saves'; $entryDir=Join-Path $Root 'backup'
    $GameDir=Join-Path $Root 'game'; $iniDir=Join-Path $Root 'inis'; $addonLog=Join-Path $Root 'absent.log'
    foreach ($dir in @($SaveRoot,$GameDir,$iniDir)) { $null=New-Item -ItemType Directory -Path $dir }
    [IO.File]::WriteAllText((Join-Path $SaveRoot 'save.bin'),'CURRENT_ENTRY')
    $script:entry=C72-Manifest $SaveRoot
    C72-Backup $SaveRoot $entryDir $script:entry
    $script:saveHandles=C72-LockSaves $SaveRoot $script:entry
    [IO.File]::WriteAllText((Join-Path $SaveRoot 'added.bin'),'TRIAL')
    if ($Case -eq 'restore-failure') { [IO.File]::WriteAllText((Join-Path $entryDir 'save.bin'),'TAMPERED') }
    foreach ($name in @('Nioh3EngineFix.ini','Nioh3BossRematch.ini')) {
        [IO.File]::WriteAllText((Join-Path $iniDir $name),'ORIGINAL')
        [IO.File]::WriteAllText((Join-Path $GameDir $name),'OVERLAY')
    }
    $script:cleaned=$false; $script:touched=$true
    $script:held=New-Object 'System.Collections.Generic.List[string]'
    $script:held.Add('nioh3-pc'); $script:held.Add('nioh3')
    $script:RealSleep=Get-Command Start-Sleep -CommandType Cmdlet
    $script:result=@{status='TRIAL_FAILED';error='injected abort';saves_restored=$false;inis_restored=$false}
    $global:HG=@{aborting=$false;file=(Join-Path $Root 'guard.json');cleanup={
        [IO.File]::AppendAllText((Join-Path $Root 'cleanup-count'),'1')
        Finish-C72
    }}
    [IO.File]::WriteAllText($global:HG.file,'{"ownerPid":123,"games":[{"pid":456,"start":"fixture"}]}')
    Guard-Fail 'injected owner loss/deadline' 99
    throw 'Abort unexpectedly returned'
}
$Root=Join-Path ([IO.Path]::GetTempPath()) ('c72e-abort-'+[guid]::NewGuid().ToString('N'))
foreach ($scenario in @('close-failure','foreign-game','restore-failure','success')) {
    $dir=Join-Path $Root $scenario
    $null=New-Item -ItemType Directory -Path $dir -Force
    & powershell -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath -Case $scenario -Root $dir
    if ($LASTEXITCODE -ne 99) { throw "FAIL: $scenario exit code $LASTEXITCODE" }
    foreach ($name in @('cleanup-count','close-count')) {
        if ([IO.File]::ReadAllText((Join-Path $dir $name)) -ne '1') { throw "FAIL: $name must run exactly once" }
    }
    $failed=$scenario -ne 'success'
    $exists=Test-Path -LiteralPath (Join-Path $dir 'guard.json')
    if ($exists -ne $failed) { throw "FAIL: $scenario recovery record retention=$exists" }
    if (Test-Path -LiteralPath (Join-Path $dir 'kill-called')) { throw 'FAIL: second close/kill after cleanup' }
    if ($exists) {
        $record=Get-Content -LiteralPath (Join-Path $dir 'guard.json') -Raw | ConvertFrom-Json
        if ($record.ownerPid -ne 123 -or $record.games[0].pid -ne 456 -or $record.games[0].start -ne 'fixture') { throw 'FAIL: ownership record changed' }
    }
    $result=Get-Content -LiteralPath (Join-Path $dir 'result.json') -Raw | ConvertFrom-Json
    if ($result.saves_restored -eq $failed -or $result.inis_restored -eq $failed) { throw 'FAIL: restoration receipt' }
    if ((Test-Path -LiteralPath (Join-Path $dir 'released-locks')) -eq $failed) { throw 'FAIL: release before restoration' }
    if ([IO.File]::ReadAllText((Join-Path $dir 'saves\save.bin')) -ne 'CURRENT_ENTRY') { throw 'FAIL: current progress changed' }
    if ((Test-Path -LiteralPath (Join-Path $dir 'saves\added.bin')) -ne $failed) { throw 'FAIL: restoration crossed failed-close/backup gate' }
    foreach ($name in @('Nioh3EngineFix.ini','Nioh3BossRematch.ini')) {
        $expected=if ($failed) { 'OVERLAY' } else { 'ORIGINAL' }
        if ([IO.File]::ReadAllText((Join-Path $dir ('game\'+$name))) -ne $expected) { throw 'FAIL: INI restoration order' }
    }
    Write-Host "PASS: $scenario exits99; cleanup/close once; ownership, save, INI, shared-lock invariants hold"
}
Write-Host "ABORT REGRESSION PASS; fixtures: $Root"
