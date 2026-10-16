# Actual helper functions only; no process discovery, launch, input or installed edits.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
$tokens=$null;$errors=$null
$path=Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if ($errors) { throw 'Parse error' }
foreach ($name in @('Get-C72LaunchIdentity')) {
    $fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true)
    Invoke-Expression $fn.Extent.Text
}
$checks=0
function Check([bool]$Value) { if (-not $Value) { throw "FAIL check $script:checks" };$script:checks++ }
$launched=[DateTime]'2026-10-11T18:55:40';$expected='C:\fixture\Nioh3.exe'
$oldFailed=$false; try { C72-Full '' | Out-Null } catch { $oldFailed=$true }
Check $oldFailed # actual old call failed before identity could be adopted
Check ((Get-C72LaunchIdentity @{StartTime=$launched;Path=''} $launched $expected) -eq 'pending')
Check ((Get-C72LaunchIdentity @{StartTime=$launched;Path=$null} $launched $expected) -eq 'pending')
$unreadable=New-Object PSObject
$unreadable | Add-Member ScriptProperty StartTime { throw 'metadata unavailable' }
Check ((Get-C72LaunchIdentity $unreadable $launched $expected) -eq 'pending')
# Requery sequence: two unknown observations do not adopt; exact third does.
$adoptions=0
foreach ($imagePath in @('',$null,$expected)) {
    if ((Get-C72LaunchIdentity @{StartTime=$launched;Path=$imagePath} $launched $expected) -eq 'owned') { $adoptions++ }
}
Check ($adoptions -eq 1)
foreach ($process in @(@{StartTime=$launched.AddSeconds(-10);Path=$expected},@{StartTime=$launched;Path='C:\foreign\Nioh3.exe'})) {
    $refused=$false; try { Get-C72LaunchIdentity $process $launched $expected | Out-Null } catch { $refused=$true }
    Check $refused
}
$root=Join-Path ([IO.Path]::GetTempPath()) ('c72e-finalize-'+[guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path $root
function Finish-C72 { if ($script:fail) { throw 'game remains or restoration failed' }; $script:cleaned=$script:complete }
$guard=Join-Path (Split-Path -Parent $PSScriptRoot) 'testmatrix\harness_guard.ps1'
$guardAst=[Management.Automation.Language.Parser]::ParseFile($guard,[ref]$tokens,[ref]$errors)
$doneFn=$guardAst.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Done-Guard'},$true)
Invoke-Expression $doneFn.Extent.Text
function HG-Log { $script:done++ }
function Clear-MeasureLock {}
$source=[IO.File]::ReadAllText($path)
$outer=$source.Substring([regex]::Match($source,'(?m)^finally \{').Index)
$outer=$outer.Substring(0,$outer.IndexOf('Write-Host'))
Invoke-Expression ('function Test-Finally { try {} '+$outer+' }')
foreach ($scenario in @('failure','incomplete','success')) {
    $script:cleaned=$false;$script:done=0;$script:fail=$scenario -eq 'failure';$script:complete=$scenario -eq 'success'
    $record=Join-Path $root ($scenario+'.json');[IO.File]::WriteAllText($record,'recovery owner fixture')
    $global:HG=@{file=$record;ownerPid=123}
    try { Test-Finally } catch { if (-not $script:fail) { throw } }
    Check ((Test-Path -LiteralPath $record) -eq ($scenario -ne 'success'))
    Check ($script:done -eq 1)
    Check (-not $global:HG) # shared Done-Guard clears memory even while retaining its file
}
Check ($source -match 'Done-Guard -KeepFile:\(-not \$script:cleaned\)')
Check ($source -match 'Get-C72LaunchIdentity \$games\[0\]')
Write-Host "$checks ACTUAL HELPER / FINALIZATION CHECKS PASS; no live actions"
