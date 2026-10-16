# Offline only: actual pure validators, AST import and read-only plan.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_binary_lib.ps1')
. (Join-Path $PSScriptRoot 'training_pilot_lib.ps1')
$script:checks=0
function Check([bool]$Value) { if (-not $Value) { throw "FAIL check $script:checks" };$script:checks++ }
function Refuses([scriptblock]$Body) { $failed=$false;try { & $Body | Out-Null } catch { $failed=$true };Check $failed }
$runner=Join-Path $PSScriptRoot 'run_training_native_pilot.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($runner,[ref]$tokens,[ref]$errors)
Check (-not $errors)
$absent=Join-Path ([IO.Path]::GetTempPath()) ('training-plan-absent-'+[guid]::NewGuid().ToString('N'))
$plan=(& $runner -DryRun -OutDir $absent -GameDir $absent -SaveRoot $absent -CandidateRoot $absent -Kit $absent)|ConvertFrom-Json
Check ($plan.mode -eq 'PLAN_ONLY_NO_MUTATION' -and $plan.launches -eq 1 -and -not $plan.warp -and -not $plan.spawn)
Check (-not (Test-Path -LiteralPath $absent))
Training-ImportLifecycle (Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1')
Check ([bool](Get-Command Finish-C72 -CommandType Function))
Check ([bool](Get-Command Guard-Fail -CommandType Function))
$line='Rematch: F R1CBC: queued - slot 1 +4 mission 0x1CBC, +0xC start point 67064, +0x14 mode 2'
Check (Training-QueueWitness $line)
foreach ($bad in @($line.Replace('67064','0'),$line.Replace('mode 2','mode 1'),$line.Replace('slot 1','slot 3'),$line.Replace('R1CBC','RC72E'))) { Check (-not (Training-QueueWitness $bad)) }
$s='state=4 loading=0 player=1 mission=0x1CBC stage=67064 infight=0 warps=0 spawn=- fnow=armed gate=native replay/return still pending'
Training-CheckStatus $s;Check (Training-AtShrine $s $true)
Check (-not (Training-AtShrine $s $false))
foreach ($bad in @($s.Replace('stage=67064','stage=1'),$s.Replace('infight=0','infight=1'),$s.Replace('player=1','player=0'),$s.Replace('loading=0','loading=1'))) { Check (-not (Training-AtShrine $bad $true)) }
foreach ($bad in @($s.Replace('warps=0','warps=1'),$s.Replace('spawn=-','spawn=active'),$s.Replace('gate=native replay/return still pending','gate=online'),$s.Replace('stage=67064',''),$s.Replace('fnow=armed','fnow=fail'))) { Refuses { Training-CheckStatus $bad } }
$request=[pscustomobject]@{nonce='unique';stage='NativeReturn';owner_pid=1;owner_start='a';runner_pid=2;runner_start='b'}
$approval=$request|Select-Object *;$approval|Add-Member evidence 'Root saw one native death/retry and shrine control'
Training-CheckApproval $request $approval;Check $true
foreach ($key in @('nonce','stage','owner_pid','owner_start','runner_pid','runner_start','evidence')) {
    $bad=$approval|Select-Object *;$bad.$key='';Refuses { Training-CheckApproval $request $bad }
}
$gate=[pscustomobject]@{status='LANDING_PROXY_AND_BOSS_OBSERVED';saves_restored=$true;inis_restored=$true;binaries_restored=$true;root_control_confirmed=$true;native_return_confirmed=$true}
$manifest=[pscustomobject]@{plan=[pscustomobject]@{runtime_asi_hashes=[pscustomobject]@{engine='123';addon='456'}}}
Training-CheckGate $gate $manifest @{engine='123';addon='456'};Check $true
$alternative=$gate|Select-Object *;$alternative.status='ROOT_CONFIRMED_NATIVE_ENTRY_FIRST_CONTROL_TIME_UNKNOWN'
Training-CheckGate $alternative $manifest @{engine='123';addon='456'};Check $true
Refuses { Training-CheckGate $gate $manifest @{engine='old';addon='456'} }
foreach ($key in @('saves_restored','inis_restored','binaries_restored','root_control_confirmed','native_return_confirmed')) {
    foreach ($value in @($false,'true')) { $bad=$gate|Select-Object *;$bad.$key=$value;Refuses { Training-CheckGate $bad $manifest @{engine='123';addon='456'} } }
}
$bad=$gate|Select-Object *;$bad.status='TIMEOUT';Refuses { Training-CheckGate $bad $manifest @{engine='123';addon='456'} }
$bad=$gate|Select-Object * -ExcludeProperty root_control_confirmed;Refuses { Training-CheckGate $bad $manifest @{engine='123';addon='456'} }
# AST proves the runner only emits one shrine and one return command; other
# traffic is read-only status/scan/row. This is not a live route test.
$calls=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -eq 'Invoke-C72Pipe'},$true))
$commands=@($calls|ForEach-Object { $_.CommandElements[1].Value })
Check (@($commands|Where-Object { $_ -eq 'shrine R1CBC' }).Count -eq 1)
Check (@($commands|Where-Object { $_ -eq 'exit' }).Count -eq 1)
Check (@($commands|Where-Object { $_ -notin @('status','scan','row R1CBC','shrine R1CBC','exit') }).Count -eq 0)
# Execute actual scan helper with the native completion interleaved before its
# scan reply. Generation must come from the earlier row, never that reply.
$script:scanGeneration=7;$script:scanCalls=New-Object 'System.Collections.Generic.List[string]'
function Invoke-C72Pipe([string]$Command) {
    $script:scanCalls.Add($Command)
    if ($Command -eq 'row R1CBC') { return "status=2 gen=$script:scanGeneration" }
    if ($Command -eq 'scan') { $script:scanGeneration++;return "ok gen=$script:scanGeneration" }
    throw 'Unexpected test command'
}
$before=Training-BeginScan
Check ($before -eq 7 -and $script:scanGeneration -eq 8)
Check (($script:scanCalls -join ',') -eq 'row R1CBC,scan')
Check ($script:scanGeneration -gt $before)
Check (-not ($before -gt $before)) # unchanged generation is never completion
$prefix="original log`r`n"
Check ((Training-LogTail $prefix ($prefix+$line)) -ceq $line)
Refuses { Training-LogTail $prefix (('x' * $prefix.Length)+$line) }
Refuses { Training-LogTail $prefix 'short' }
Refuses { Training-LogTail $prefix ($prefix.ToUpperInvariant()+$line) }
# Bind the actual runner's FreshLog adapter, so a length-only adapter regression
# cannot be hidden by an independently correct pure helper.
$fresh=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Training-FreshLog'},$true)
Invoke-Expression $fresh.Extent.Text
$script:requestLogBefore=$prefix;$addonLog='fixture';$script:logText=$prefix+$line
function C72-ReadShared([string]$Path) { return $script:logText }
Check ((Training-FreshLog) -ceq $line)
$script:logText=('x' * $prefix.Length)+$line
Refuses { Training-FreshLog }
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Training-CheckDependencies $repo;Check $true
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('training-pins-'+[guid]::NewGuid().ToString('N'))
$dependencies=@('tools/rematch/c72e_pilot_lib.ps1','tools/rematch/c72e_binary_lib.ps1','tools/testmatrix/harness_guard.ps1','tools/playsession/session_lib.ps1')
foreach ($relative in $dependencies) {
    $target=Join-Path $fixture $relative;$null=New-Item -ItemType Directory -Force -Path (Split-Path -Parent $target)
    Copy-Item -LiteralPath (Join-Path $repo $relative) -Destination $target
}
Training-CheckDependencies $fixture;Check $true
foreach ($relative in $dependencies) {
    $target=Join-Path $fixture $relative;[IO.File]::AppendAllText($target,"`n# altered fixture")
    Refuses { Training-CheckDependencies $fixture }
    Copy-Item -LiteralPath (Join-Path $repo $relative) -Destination $target -Force
}
Write-Host "PASS $script:checks offline training runner checks; no live route exercised"
# Exact original-entry return, including the valid overworld mission zero.
$entry=@{mission='0x0';stage='4019512'}
$ret='state=0 infight=0 mission=0x0 stage=4019512 loading=0 player=1 warps=0 spawn=- gate=ok'
$requested="Rematch: exit: MissionRequestReturn(params, 1) from mission 0x1CBC (retry point 67064) -> requested`n"
$exited="Rematch: replay mission exited (stage now 4019512, mission state +28h 0x0)`n"
Check (Training-ReturnEvidence 'prefix' ('prefix'+$requested+$exited) $ret $entry)
foreach($tail in @('', $requested, $exited, ($exited+$requested), ($requested.Replace('67064','0')+$exited), ($requested+$exited.Replace('4019512','1')))) {
 Check (-not (Training-ReturnEvidence 'prefix' ('prefix'+$tail) $ret $entry))
}
foreach($bad in @($ret.Replace('mission=0x0','mission=0x2'),$ret.Replace('stage=4019512','stage=9'),$ret.Replace('loading=0','loading=1'),$ret.Replace('player=1','player=0'),$ret.Replace('gate=ok','gate=native request uncertain'))) {
 Check (-not (Training-ReturnEvidence 'prefix' ('prefix'+$requested+$exited) $bad $entry))
}
Refuses { Training-ReturnEvidence 'prefix' ('foreign'+$requested+$exited) $ret $entry }
Refuses { Training-ReturnEvidence '' $requested $ret $null }
Refuses { Training-ReturnEvidence '' $requested.Replace('requested','EXCEPTION') $ret $entry }
Refuses { Training-ReturnEvidence '' '[19:00:00.000] Rematch: refused: loading' $ret $entry }
Check ((Training-CandidatePath 'D:\archive' 'Nioh3EngineFix' 'Flat') -eq 'D:\archive\Nioh3EngineFix.asi')
Check ((Training-CandidatePath 'D:\archive' 'Nioh3BossRematch' 'Tree') -eq 'D:\archive\mod\Nioh3BossRematch\bin\Nioh3BossRematch.asi')
Refuses { Training-CandidatePath 'D:\archive' 'unknown' 'Flat' }
Refuses { Training-CandidatePath 'D:\archive' 'Nioh3EngineFix' 'auto' }
Write-Host "PASS $script:checks including bound original return and explicit archive layout"
# Exercise the actual runner adapter, not only its helper.
$wait=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Training-Wait'},$true)
Invoke-Expression $wait.Extent.Text
$script:result=@{entry_tuple=$entry};$script:returnLogBefore='prefix';$script:requestLogBefore='prefix'
$script:logText='prefix'+$requested+$exited;$script:returnStatus=$ret
function Invoke-C72Pipe([string]$Command) { if($Command -ne 'status'){throw 'Unexpected mutation'};return $script:returnStatus }
Check ((Training-Wait 'return' 1) -eq $ret)
$script:logText='prefix'+'[19:00:00.000] Rematch: refused: native replay has not settled'
Refuses { Training-Wait 'shrine' 1 }
Write-Host "PASS $script:checks including actual wait adapter and fail-fast refusal"
# Hosted mode is a distinct derivative admission, never a rewritten old PASS.
$hp=(& $runner -DryRun -Hosted -OutDir $absent -GameDir $absent -SaveRoot $absent -CandidateRoot $absent -Kit $absent)|ConvertFrom-Json
Check ($hp.gate -match 'NEW_UI_UNTESTED' -and $hp.route -match 'not full training' -and $hp.versions -eq 'Engine1.7.68/addon1.4.10')
Check ($hp.runtime_asi_hashes.'Nioh3EngineFix.asi' -ne $plan.runtime_asi_hashes.'Nioh3EngineFix.asi')
Check (-not (Test-Path -LiteralPath $absent))
$receipt='9043b96c237aecfdb63613c3be6e8b9a62394c176a64f5f11f67abe8dbcc4aff';$tree='05a8f9e7a5ca7afd74bdf5f2cea1d5e549217898'
$newHashes=@{'Nioh3EngineFix.asi'=$hp.runtime_asi_hashes.'Nioh3EngineFix.asi';'Nioh3BossRematch.asi'=$hp.runtime_asi_hashes.'Nioh3BossRematch.asi'}
$admit=Training-HostedAdmission $receipt $tree $newHashes
Check ($admit.kind -eq 'EXPLICIT_DERIVATIVE_ADMISSION_NOT_TRANSFERRED_PASS' -and $admit.max_launches -eq 1)
Refuses { Training-HostedAdmission ('0'*64) $tree $newHashes }
Refuses { Training-HostedAdmission $receipt ('0'*40) $newHashes }
foreach($key in $newHashes.Keys){$bad=$newHashes.Clone();$bad[$key]='0'*64;Refuses {Training-HostedAdmission $receipt $tree $bad}}
Check (Training-HostedRequestWitness 'Rematch: request Practice Yamagata (native shrine) for R1CBC queued from the practice menu' 'start')
Check (Training-HostedRequestWitness 'Rematch: request return from practice queued from the practice menu' 'return')
Check (-not (Training-HostedRequestWitness 'Rematch: request Shrine before the boss for R1CBC queued from pipe' 'start'))
Check (-not (Training-HostedRequestWitness 'Rematch: request leave the mission (return) queued from pipe' 'return'))
# Execute actual conditional submission blocks from the runner in both modes.
$script:submitted=New-Object 'System.Collections.Generic.List[string]'
function Invoke-C72Pipe([string]$Command){$script:submitted.Add($Command);return 'queued'}
foreach($command in @('shrine R1CBC','exit')){
 $call=$calls|Where-Object{$_.CommandElements[1].Value -eq $command}|Select-Object -First 1
 $parent=$call.Parent
 while($parent -and $parent -isnot [Management.Automation.Language.IfStatementAst]){$parent=$parent.Parent}
 Check ($parent -and $parent.Extent.Text -match '^if \(\$Hosted\)')
 $Hosted=$true;Invoke-Expression $parent.Extent.Text
 Check ($script:submitted.Count -eq 0)
 $Hosted=$false;Invoke-Expression $parent.Extent.Text
 Check ($script:submitted.Count -eq 1 -and $script:submitted[0] -eq $command)
 $script:submitted.Clear()
}
Write-Host "PASS $script:checks including explicit hosted admission and no hosted pipe mutations"
