param([string]$LifecyclePath='C:/Users/Ahmad Hamza/nioh3-engine/tools/rematch/run_c72e_native_pilot.ps1',[switch]$ExpectOriginalDefect)
$ErrorActionPreference='Stop'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($LifecyclePath,[ref]$tokens,[ref]$errors)
if($errors){throw 'Lifecycle parse failed'}
$fn=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Finish-C72'},$true))
if($fn.Count -ne 1){throw 'Missing/ambiguous actual finalizer'}
Invoke-Expression $fn[0].Extent.Text
# Every external side effect is mocked; no saves, game, INIs, locks or receipts touched.
$script:trace=New-Object 'System.Collections.Generic.List[string]'
function Close-Owned {$script:trace.Add('close')}
function Get-Process {param($Name,$ErrorAction) return $null}
function Test-Path {param($LiteralPath) return Microsoft.PowerShell.Management\Test-Path -LiteralPath $LiteralPath}
function C72-UnlockSaves {param($Handles) $script:trace.Add('unlock')}
function C72-Restore {param($Root,$Backup,$Manifest) $script:trace.Add('restore')}
function C72-Manifest {param($Root) return @('entry')}
function C72-Same {param($A,$B) return $true}
function Check-Asis {$script:trace.Add('verify-binaries')}
function Invoke-GameLock {param($Action) $script:trace.Add('release');$script:releaseKeys.Add($global:LockKey);return @{code=[int]($script:failRelease -eq $global:LockKey)}}
function C72-Json {param($Path,$Value)
    if($script:failReceipt){$script:trace.Add('receipt-fail');[IO.File]::WriteAllText($Path,'{"partial":');throw 'INJECTED partial result persistence failure'}
    if($script:corruptReceipt){$script:trace.Add('receipt-corrupt');[IO.File]::WriteAllText($Path,'{}');return}
    $script:trace.Add('receipt-write');[IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 12))
}
$script:RealSleep={param($Seconds)}
$OutDir=Join-Path ([IO.Path]::GetTempPath()) ('c72-finalizer-review-'+[guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path $OutDir
$addonLog=Join-Path $OutDir 'absent-addon.log';$SaveRoot='mock-save';$entryDir='mock-backup'
$script:failReceipt=$true;$script:corruptReceipt=$false;$script:failRelease='';$script:releaseKeys=New-Object 'System.Collections.Generic.List[string]'
$script:c72FinalState=$null
$script:cleaned=$false;$script:owned=$null;$script:saveHandles=$null;$script:entry=@('entry');$script:touched=$false;$script:binaryRecord=$null
$script:held=New-Object 'System.Collections.Generic.List[string]';$script:held.Add('mock-owned-lock')
$script:result=@{status='TRIAL_FAILED';saves_restored=$false;inis_restored=$false;binaries_restored=$false}
$prior='{"prior":"do-not-overwrite-on-failure"}'
[IO.File]::WriteAllText((Join-Path $OutDir 'result.json'),$prior)
$failed=$false;try{Finish-C72}catch{$failed=$true}
if(-not $failed){throw 'Injected failure did not propagate'}
$released=$script:trace.Contains('release')
if($ExpectOriginalDefect){
    if(-not $released -or -not $script:cleaned -or $script:held.Count -ne 0){throw 'Original defect not reproduced'}
    Write-Host ('REPRODUCED actual Finish-C72 receipt failure after lease release/cleaned: '+($script:trace -join ', '))
} else {
    if($released -or $script:cleaned -or $script:held.Count -ne 1){throw 'Receipt failure released recovery ownership'}
    Write-Host ('PASS actual Finish-C72 receipt failure retains lease/guard: '+($script:trace -join ', '))
    if([IO.File]::ReadAllText((Join-Path $OutDir 'result.json')) -cne $prior){throw 'Failed temporary publication changed prior receipt'}
    # Exercise actual atomic publisher failure, not only its JSON writer.
    $script:failReceipt=$false
    $locked=[IO.File]::Open((Join-Path $OutDir 'result.json'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
    $publishFailed=$false
    try {try{Finish-C72}catch{$publishFailed=$true}} finally {$locked.Dispose()}
    if(-not $publishFailed -or $script:cleaned -or $script:trace.Contains('release') -or $script:held.Count -ne 1){throw 'Failed actual publication did not preserve ownership'}
    if([IO.File]::ReadAllText((Join-Path $OutDir 'result.json')) -cne $prior){throw 'Failed actual publication damaged prior receipt'}
    Write-Host 'PASS actual locked-target publication failure preserves prior receipt and ownership'
    $script:corruptReceipt=$true;$readbackFailed=$false
    try{Finish-C72}catch{$readbackFailed=$true}
    $script:corruptReceipt=$false
    if(-not $readbackFailed -or $script:cleaned -or $script:trace.Contains('release')){throw 'Malformed completed receipt was admitted'}
    if([IO.File]::ReadAllText((Join-Path $OutDir 'result.json')) -cne $prior){throw 'Malformed completed receipt replaced prior receipt'}
    Write-Host 'PASS completed JSON missing restoration fields fails actual readback before publication/release'
    # Retry the same restored transaction: first successful release must never
    # permit re-restoration over later progress, even when another release fails.
    $script:failReceipt=$false;$script:held.Add('second-lock');$script:failRelease='mock-owned-lock'
    $beforeRestores=@($script:trace|Where-Object {$_ -eq 'restore'}).Count
    $failed=$false;$failureText='';try{Finish-C72}catch{$failed=$true;$failureText="$_"}
    if(-not $failed -or $script:cleaned -or $script:held.Count -ne 1 -or $script:held[0] -ne 'mock-owned-lock'){throw "Partial release did not retain only remaining ownership: $failureText; trace=$($script:trace -join ',')"}
    $durable=Get-Content -LiteralPath (Join-Path $OutDir 'result.json') -Raw|ConvertFrom-Json
    if($durable.restoration_verified -ne $true -or $durable.automatic_restore_permitted -ne $false -or $durable.cleanup_stage -cne 'RESTORATION_VERIFIED_LEASE_RELEASE_PENDING'){throw 'Missing durable no-restoration-again state'}
    $savedReceipt=[IO.File]::ReadAllText((Join-Path $OutDir 'result.json'))
    $script:failRelease='';Finish-C72
    if(-not $script:cleaned -or $script:held.Count){throw 'Remaining release did not complete'}
    if(@($script:trace|Where-Object {$_ -eq 'restore'}).Count -ne $beforeRestores){throw 'Cleanup retry restored stale saves again'}
    if(@($script:releaseKeys|Where-Object {$_ -eq 'second-lock'}).Count -ne 1){throw 'Already released lease was touched again'}
    if([IO.File]::ReadAllText((Join-Path $OutDir 'result.json')) -cne $savedReceipt){throw 'Post-release cleanup rewrote durable restoration state'}
    $traceCount=$script:trace.Count;Finish-C72
    if($script:trace.Count -ne $traceCount){throw 'Completed finalizer was not idempotent'}
    Write-Host 'PASS prior receipt preservation, durable restoration-before-release, partial release retry, no stale restore, no released-lease retry, completed idempotence'
}
Write-Host "Temporary fixture retained: $OutDir"
