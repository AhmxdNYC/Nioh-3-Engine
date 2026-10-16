$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'writable_save_policy.ps1')
Assert-WritableSaveDependencies $PSScriptRoot
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
$n=0
function Check($v){if(-not $v){throw "Fixture line $((Get-PSCallStack)[1].ScriptLineNumber)"};$script:n++}
function Refuses([scriptblock]$Action){$failed=$false;try{&$Action|Out-Null}catch{$failed=$true};Check $failed}
$base=Join-Path ([IO.Path]::GetTempPath()) ('writable-save-fixture-'+[guid]::NewGuid().ToString('N'))
$root=Join-Path $base 'live';$backup=Join-Path $base 'backup';$receipt=Join-Path $base 'policy.json'
$null=New-Item -ItemType Directory -Path $root
[IO.File]::WriteAllText((Join-Path $root 'a.sav'),'entry-a');[IO.File]::WriteAllText((Join-Path $root 'b.sav'),'entry-b')
$entry=C72-Manifest $root;C72-Backup $root $backup $entry
$handles=C72-LockSaves $root $entry
$params=@{Root=$root;Backup=$backup;Manifest=$entry;CurrentHandles=$handles;Receipt=$receipt;HeldLeases=@('nioh3-pc');AdmissionSha256=('a'*64)}
$policy=$null
try {
 Refuses {Enable-WritableSavePolicy @params}
 Refuses {[IO.File]::WriteAllText((Join-Path $root 'a.sav'),'blocked')}
 [IO.File]::WriteAllText((Join-Path $backup 'a.sav'),'corrupt')
 Refuses {Enable-WritableSavePolicy @params -OptIn};Check ($handles.Count -eq 2)
 [IO.File]::WriteAllText((Join-Path $backup 'a.sav'),'entry-a')
 # Actual publication failure leaves owned live locks held and no policy success.
 $params.Receipt=Join-Path $base 'missing-parent/policy.json'
 Refuses {Enable-WritableSavePolicy @params -OptIn};Check ($handles.Count -eq 2)
 $params.Receipt=$receipt
 $policy=Enable-WritableSavePolicy @params -OptIn
 Check ($handles.Count -eq 0 -and (Test-Path -LiteralPath $receipt))
 Refuses {[IO.File]::WriteAllText((Join-Path $backup 'a.sav'),'blocked')}
 Refuses {Close-WritableSavePolicy $policy $false}
 # Real save operations that failed under deny-write are now allowed.
 [IO.File]::WriteAllText((Join-Path $root 'a.sav'),'game-progress')
 [IO.File]::Delete((Join-Path $root 'b.sav'))
 [IO.File]::WriteAllText((Join-Path $root 'new.sav'),'new-game-file')
 # Import ACTUAL pinned finalizer; mock only process/lease/ASI surroundings.
 $tokens=$null;$errors=$null
 $ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1'),[ref]$tokens,[ref]$errors)
 Check (-not $errors)
 $fn=@($ast.FindAll({param($x)$x -is [Management.Automation.Language.FunctionDefinitionAst] -and $x.Name -eq 'Finish-C72'},$true));Check ($fn.Count -eq 1)
 Invoke-Expression $fn[0].Extent.Text
 function Close-Owned {}
 function Get-Process {param($Name,$ErrorAction) if($script:gameRemaining){return @{Id=123}};return $null}
 function Check-Asis {}
 function Invoke-GameLock {param($Action) $script:releaseCalls++;return @{code=0}}
 $script:RealSleep={param($Seconds)}
 $OutDir=$base;$SaveRoot=$root;$entryDir=$backup;$addonLog=Join-Path $base 'absent.log'
 $script:entry=$entry;$script:saveHandles=$handles;$script:owned=$null;$script:touched=$false;$script:binaryRecord=$null
 $script:held=New-Object 'System.Collections.Generic.List[string]';$script:held.Add('nioh3-pc')
 $script:cleaned=$false;$script:c72FinalState=$null;$script:result=@{status='FIXTURE';save_policy=$policy.policy}
 $script:releaseCalls=0;$script:gameRemaining=$true
 Refuses {Finish-C72}
 Check (-not $script:cleaned -and $script:held.Count -eq 1 -and $script:releaseCalls -eq 0)
 Check ([IO.File]::ReadAllText((Join-Path $root 'a.sav')) -ceq 'game-progress')
 $script:gameRemaining=$false
 # Actual deletion failure during restore retains policy, backup and cleanup evidence.
 $newLock=[IO.File]::Open((Join-Path $root 'new.sav'),'Open','Read','Read')
 try{Refuses {Finish-C72}}finally{$newLock.Dispose()}
 Check (-not $script:cleaned -and $script:held.Count -eq 1 -and $script:releaseCalls -eq 0)
 Check ((Test-Path -LiteralPath $receipt) -and (Test-Path -LiteralPath (Join-Path $base 'cleanup-failure.json')))
 Check (C72-Same $entry (C72-Manifest $backup))
 Finish-C72
 Check ($script:cleaned -and $script:held.Count -eq 0 -and $script:releaseCalls -eq 1)
 Check (C72-Same $entry (C72-Manifest $root))
 $durable=Get-Content -LiteralPath (Join-Path $base 'result.json') -Raw|ConvertFrom-Json
 Check ($durable.restoration_verified -and -not $durable.automatic_restore_permitted)
 Close-WritableSavePolicy $policy $script:cleaned
 Check ($policy.backupHandles.Count -eq 0 -and (Test-Path -LiteralPath $receipt))
 Write-Host "PASS $n writable-save checks, real temp save mutations/restoration + actual pinned finalizer; artifacts retained $base"
} finally {
 C72-UnlockSaves $handles
 if($policy){C72-UnlockSaves $policy.backupHandles}
 # Fixture artifacts retained; no recursive deletion or real game/save operations.
}
