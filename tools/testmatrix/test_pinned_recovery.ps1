$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'pinned_benchmark_lib.ps1')
. (Join-Path $PSScriptRoot 'pinned_operator.ps1')
$tokens=$null;$errors=$null
$ast=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'pinned_final_pc.ps1'),[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'Driver parse failed' }
$function=$ast.Find({param($node) $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'PB-Recover'},$true)
if (-not $function) { throw 'Actual driver recovery function missing' }
Invoke-Expression $function.Extent.Text
# Bind the actual recovery body to offline process/lease boundaries only.
function Get-Process { return $null }
function PB-Unlock { if ($script:leaseFailure) { throw 'fixture release failure' }; $script:pbLocked=$false }
$global:HG=$null
$script:checks=0
function Check([bool]$Condition,[string]$Name) { if (-not $Condition) { throw "FAIL $Name" }; $script:checks++ }
function MustThrow([scriptblock]$Body,[string]$Name) { $threw=$false; try { &$Body } catch { $threw=$true }; Check $threw $Name }
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
$fixture=Join-Path $tempBase ('nioh3-recovery-'+[guid]::NewGuid().ToString('N'))
try {
    foreach ($case in 'success','damaged-backup','lease-failure') {
        $caseRoot=Join-Path $fixture $case
        $game=Join-Path $caseRoot 'game';$save=Join-Path $caseRoot 'save';$OutputDir=Join-Path $caseRoot 'out'
        New-Item -ItemType Directory -Path $game,$save -Force | Out-Null
        [IO.File]::WriteAllText((Join-Path $game 'Nioh3EngineFix.asi'),'entry-asi')
        [IO.File]::WriteAllText((Join-Path $save 'save.bin'),'entry-save')
        $script:pbState=New-PBState $game $save $OutputDir
        $script:pbMarker=New-PBMarker (Join-Path $caseRoot 'coord')
        $markerPath=$script:pbMarker.path
        $script:pbOperator=New-PBOperator (Join-Path $OutputDir 'operator')
        $script:pbRestoreOk=$false;$script:pbSuccess=$false;$script:pbLocked=$true
        $script:leaseFailure=($case -eq 'lease-failure')
        [IO.File]::WriteAllText((Join-Path $game 'Nioh3EngineFix.asi'),'test-asi')
        [IO.File]::WriteAllText((Join-Path $save 'save.bin'),'test-save')
        if ($case -eq 'damaged-backup') {
            [IO.File]::WriteAllText((Join-Path $OutputDir 'entry\save\save.bin'),'damaged')
            MustThrow { PB-Recover } 'damaged backup stops actual recovery'
            Check ((Test-Path -LiteralPath $markerPath) -and $script:pbLocked) 'damage retains hold and lease'
            Check ([IO.File]::ReadAllText((Join-Path $game 'Nioh3EngineFix.asi')) -eq 'test-asi') 'damage detected before restore write'
            Check (-not (Test-Path -LiteralPath (Join-Path $OutputDir 'operator\finished.json'))) 'damage cannot report restored'
        } elseif ($case -eq 'lease-failure') {
            MustThrow { PB-Recover } 'lease release failure stops completion'
            Check ((Test-Path -LiteralPath $markerPath) -and $script:pbLocked) 'lease failure retains hold'
            Check (-not (Test-Path -LiteralPath (Join-Path $OutputDir 'operator\finished.json'))) 'lease failure cannot report completion'
        } else {
            # This is the cleanup callback itself; no finally block is executed.
            PB-Recover
            Check ([IO.File]::ReadAllText((Join-Path $game 'Nioh3EngineFix.asi')) -eq 'entry-asi') 'callback restores entry ASI'
            Check ([IO.File]::ReadAllText((Join-Path $save 'save.bin')) -eq 'entry-save') 'callback restores entry save'
            Check (-not (Test-Path -LiteralPath $markerPath) -and -not $script:pbLocked) 'callback releases hold and lease without finally'
            $finished=Get-Content -LiteralPath (Join-Path $OutputDir 'operator\finished.json') -Raw | ConvertFrom-Json
            Check ($finished.restored -and -not $finished.complete) 'guard abort reports restoration without benchmark success'
            PB-Recover
            Check ($script:pbMarker -eq $null) 'second callback is idempotent'
        }
    }
    $script:pbState=$null;$script:pbOperator=$null;$script:pbRestoreOk=$false;$script:leaseFailure=$false
    $OutputDir=Join-Path $fixture 'not-prepared'
    $script:pbMarker=New-PBMarker (Join-Path $fixture 'preparation-coord')
    $markerPath=$script:pbMarker.path
    PB-Recover
    Check (-not (Test-Path -LiteralPath $markerPath) -and -not (Test-Path -LiteralPath $OutputDir)) 'preparation failure releases owned hold without output writes'
    $tokens=$null;$errors=$null
    $null=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'recover_pinned.ps1'),[ref]$tokens,[ref]$errors)
    Check ($errors.Count -eq 0) 'root recovery entry parses'
    "ALL $script:checks ACTUAL RECOVERY CALLBACK CHECKS PASSED"
} finally {
    $resolved=[IO.Path]::GetFullPath($fixture)
    if (-not $resolved.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'nioh3-recovery-*') { throw 'Fixture cleanup escaped temporary root' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
