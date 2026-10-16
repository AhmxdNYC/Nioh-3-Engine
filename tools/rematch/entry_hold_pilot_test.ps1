$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'entry_hold_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'training_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
Add-Type 'public static class EHPClock { public static ulong GetTickCount64(){return 5000;} }'
$n=0
function Check($v){if(-not $v){throw "Fixture line $((Get-PSCallStack)[1].ScriptLineNumber)"};$script:n++}
function Refuses([scriptblock]$b){$bad=$false;try{&$b|Out-Null}catch{$bad=$true};Check $bad}
$now=[datetime]'2026-10-12T04:00:00Z'
$a=@{approved=$true;engine_entry_sha256=('b'*64);addon_entry_sha256=('c'*64);save_policy='CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1';scope='ONE_FRESH_YAMAGATA_SUMMON_INTENT_PILOT';owner_pid=43708;owner_start='2026-10-11T02:34:24.0733347-04:00';source='c6d9abe271e7539ab8354c695ce084c4e2d10c8a';mod_tree='670a24d6e444ad2829facd0d23a903dc001e6e95';manifest_sha256='56ed97e041d1a8ec1ccd3e406c79ebba0cbcb7f3c44f455916556e5487085ca7';engine_sha256='4f6cccf1365c5659d323fcaedcc00726234b03912875889b1165670cae5eebe8';addon_sha256='5efd90ee4792f896e75c229dbefbaf8c79a37fa3f66dc67119d35e941d0992cc';pilot_sha256='pilot';library_sha256='lib';out_dir='D:/unique';nonce=('a'*32);review='offline';created_utc='2026-10-12T03:59:00Z';expires_utc='2026-10-12T04:10:00Z'}
EHP-Admission ([pscustomobject]$a) 'D:/unique' 'pilot' 'lib' $now;Check $true
foreach($key in @('scope','owner_pid','owner_start','source','mod_tree','manifest_sha256','engine_sha256','addon_sha256','pilot_sha256','library_sha256','out_dir','nonce','review','engine_entry_sha256','addon_entry_sha256','save_policy')){$bad=$a.Clone();$bad[$key]='';Refuses {EHP-Admission ([pscustomobject]$bad) 'D:/unique' 'pilot' 'lib' $now}}
foreach($pair in @(@('approved','true'),@('expires_utc','2026-10-12T03:59:00Z'),@('expires_utc','2026-10-13T04:00:00Z'),@('created_utc','2026-10-12T04:01:00Z'))){$bad=$a.Clone();$bad[$pair[0]]=$pair[1];Refuses {EHP-Admission ([pscustomobject]$bad) 'D:/unique' 'pilot' 'lib' $now}}
$raw='enabled=1 phase=2 attempt=7 loadSeen=1 arrived=1 boxAt=1000 deadline=10000 pending=0 intentDeadline=0 broken=0 reason=waiting'
$h=EHP-Parse $raw
$status='state=2 row=R1CBC infight=0 warps=0 spawn=- mission=0x1CBC stage=67064 loading=0 player=1 gate=rematch active'
EHP-Held $h 7 1200 $status;Check $true
foreach($pair in @(@('enabled',0),@('phase',1),@('phase',3),@('phase',4),@('attempt',8),@('loadSeen',0),@('arrived',0),@('boxAt',0),@('boxAt',1201),@('broken',1),@('deadline',1200))){$bad=$h.Clone();$bad[$pair[0]]=$pair[1];Refuses {EHP-Held $bad 7 1200 $status}}
Refuses {EHP-Held $h 7 2501 $status}
foreach($change in @(@('infight=0','infight=1'),@('mission=0x1CBC','mission=0xC72E'),@('warps=0','warps=1'),@('spawn=-','spawn=arena'),@('player=1','player=0'),@('loading=0','loading=1'))){Refuses {EHP-Held $h 7 1200 $status.Replace($change[0],$change[1])}}
foreach($key in @('enabled','phase','attempt','loadSeen','arrived','boxAt','deadline','pending','intentDeadline','broken')){Refuses {EHP-Parse ($raw+' '+$key+'=1')}}
$start="Rematch: request Practice Yamagata (native shrine) for R1CBC queued from the practice menu"+[char]10
$summon="Rematch: request release Yamagata entrance- queued from practice entrance menu"+[char]10
$boss="arena: BossBattleMode start mission 0x1CBC placements65092"+[char]10
Check ((EHP-Log 'prefix' ('prefix'+$start) $false) -ceq $start)
Check ((EHP-Log '' ($start+$summon+$boss) $true) -match '65092')
foreach($text in @($start+$start,$start+$summon,$start+$boss,$start+'Rematch: refused: unknown',$start+'EXCEPTION',$start+'Rematch: request Fight now queued from pipe')){Refuses {EHP-Log '' $text $false}}
Refuses {EHP-Log 'prefix' ('replaced'+$start) $false}
Refuses {EHP-Log '' ($start+$summon+$summon) $true}
$r=@{nonce='n';stage='HoldMenuReviewed';owner_pid=43708;owner_start='s';runner_pid=2;runner_start='r';game_pid=3;game_start='g'}
$approved=$r.Clone();$approved.evidence='observed';$approved.visible_menu=$true;$approved.held_without_boss=$true;$approved.menu_observed_seconds=2
EHP-Approval ([pscustomobject]$r) ([pscustomobject]$approved);Check $true
foreach($key in $r.Keys){$bad=$approved.Clone();$bad[$key]='wrong';Refuses {EHP-Approval ([pscustomobject]$r) ([pscustomobject]$bad)}}
foreach($key in @('visible_menu','held_without_boss')){$bad=$approved.Clone();$bad[$key]='true';Refuses {EHP-Approval ([pscustomobject]$r) ([pscustomobject]$bad)}}
$tokens=$null;$errors=$null;$path=Join-Path $PSScriptRoot 'entry_hold_pilot.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors);Check (-not $errors)
$fn=$ast.Find({param($x)$x -is [Management.Automation.Language.FunctionDefinitionAst] -and $x.Name -eq 'EHP-Wait'},$true)
Invoke-Expression $fn.Extent.Text
function EHP-Sample {return $script:fixture}
$script:attempt=7;$script:requestLogBefore='';$script:fixture=@{hold=(EHP-Parse $raw.Replace('phase=2','phase=3'));status=$status;log=$start+$summon}
Check ((EHP-Wait 'released' 0).hold.attempt -eq 7)
$script:fixture.log=$start;Refuses {EHP-Wait 'released' 0}
$script:returnLogBefore='';$script:result=@{entry_tuple=@{mission='0x0';stage='4019512'}}
$return="Rematch: exit: MissionRequestReturn(params, 1) from mission 0x1CBC (retry point 67064) -> requested"+[char]10+"Rematch: replay mission exited (stage now 4019512, mission state +28h 0x0)"
$script:fixture=@{hold=@{phase=3};log=$return;status='state=0 row=- mission=0x0 stage=4019512 loading=0 player=1 infight=0 warps=0 spawn=- gate=ok'}
Check ((EHP-Wait 'return' 0).status -match 'gate=ok')
$script:fixture.status=$script:fixture.status.Replace('spawn=-','spawn=arena');Refuses {EHP-Wait 'return' 0}
$text=$ast.Extent.Text
Check ($text.IndexOf('$log=C72-ReadShared $addonLog') -lt $text.IndexOf('$status=Invoke-C72Pipe'))
Check ($text -notmatch "Invoke-C72Pipe '(fight|shrine|exit|practice|summon)'")
Check ($text -match 'Training-ImportLifecycle' -and $text -match 'finally \{ try \{ Finish-C72;.*Close-WritableSavePolicy.*\} finally \{ Done-Guard')
$plan=& $path | ConvertFrom-Json;Check ($plan.mode -eq 'PLAN_ONLY' -and -not $plan.auto_input -and $plan.launches -eq 1)
# Actual runner menu-stage loop: paused native hold stays valid past 1500ms.
$stageFn=$ast.Find({param($x)$x -is [Management.Automation.Language.FunctionDefinitionAst] -and $x.Name -eq 'EHP-WaitStage'},$true)
Invoke-Expression $stageFn.Extent.Text
$OwnerPid=$PID;$script:owned=Get-Process -Id $PID
$global:HG=@{ownerStart=$script:owned.StartTime};$OutDir=(Join-Path $env:TEMP 'inert-entry-hold-fixture');$events='inert'
$script:watch=[Diagnostics.Stopwatch]::StartNew();$script:attempt=7
function Assert-Owned {}
function C72-Json($Path,$Value){$script:stageRequest=$Value}
function C72-Event {}
function Test-Path {return $true}
function Remove-Item {}
function Get-Content {
 $v=@{};foreach($k in $script:stageRequest.Keys){$v[$k]=$script:stageRequest[$k]}
 $v.evidence='fixture observed';$v.visible_menu=$true;$v.held_without_boss=$true;$v.menu_observed_seconds=2
 return ($v|ConvertTo-Json)
}
$script:fixture=@{hold=(EHP-Parse $raw);status=$status.Replace('gate=rematch active','gate=game paused');log=$start}
function EHP-Sample { $null=EHP-Log '' $script:fixture.log $false;return $script:fixture }
EHP-WaitStage 'HoldMenuReviewed';Check $true
foreach($pair in @(@('attempt',8),@('phase',4),@('phase',0),@('deadline',5000),@('broken',1),@('boxAt',5001))){
 $prior=$script:fixture.hold[$pair[0]];$script:fixture.hold[$pair[0]]=$pair[1]
 Refuses {EHP-WaitStage 'HoldMenuReviewed'};$script:fixture.hold[$pair[0]]=$prior
}
$script:fixture.status=$status.Replace('gate=rematch active','gate=mission state unreadable')
Refuses {EHP-WaitStage 'HoldMenuReviewed'};$script:fixture.status=$status
$script:fixture.log=$start+$summon;Refuses {EHP-WaitStage 'HoldMenuReviewed'};$script:fixture.log=$start
# New paired build deliberately permits a stale paused-menu witness to queue.
$intentHold=EHP-Parse $raw
$tracker=@{deadline=0}
Check ((EHP-Intent $intentHold 7 5000 $status $tracker) -eq 'waiting')
$intentHold.pending=1;$intentHold.intentDeadline=9000
Check ((EHP-Intent $intentHold 7 5000 $status $tracker) -eq 'pending')
foreach($pair in @(@('attempt',8),@('phase',1),@('phase',4),@('intentDeadline',5000),@('intentDeadline',10001),@('broken',1))){
 $bad=$intentHold.Clone();$bad[$pair[0]]=$pair[1];Refuses {EHP-Intent $bad 7 5000 $status $tracker}
}
$bad=$intentHold.Clone();$bad.intentDeadline=9500;Refuses {EHP-Intent $bad 7 5000 $status $tracker}
$bad=$intentHold.Clone();$bad.pending=0;$bad.intentDeadline=0;Refuses {EHP-Intent $bad 7 5000 $status $tracker}
$released=$bad.Clone();$released.phase=3;$released.boxAt=0
Check ((EHP-Intent $released 7 5000 $status $tracker) -eq 'released')
Check ((EHP-Intent $released 7 5000 $status @{deadline=0}) -eq 'released') # pending can be missed
$bad=$released.Clone();$bad.pending=1;$bad.intentDeadline=9000;Refuses {EHP-Intent $bad 7 5000 $status $tracker}
# Exercise actual observer: unique request required even if release was too fast to sample pending.
function EHP-Sample {return $script:fixture}
$script:fixture=@{hold=$released;status=$status;log=$start+$summon}
Check ((EHP-Wait 'released' 0).hold.phase -eq 3)
$script:fixture.hold=$bad;Refuses {EHP-Wait 'released' 0}
$script:fixture.hold=$released;$script:fixture.log=$start;Refuses {EHP-Wait 'released' 0}
$script:fixture.log=$start+$summon+$summon;Refuses {EHP-Wait 'released' 0}
foreach($seconds in @(0,1.99,601,'2',[double]::NaN)){$bad=$approved.Clone();$bad.menu_observed_seconds=$seconds;Refuses {EHP-Approval ([pscustomobject]$r) ([pscustomobject]$bad)}}
# Actual read interleaving: log sample precedes release but next sample catches
# its unique queue event. The native release must not fail on the first sample.
$script:samples=0
function EHP-Sample {
 $script:samples++
 return @{hold=$released;status=$status;log=($start+$(if($script:samples -gt 1){$summon}else{''}))}
}
Check ((EHP-Wait 'released' 1).hold.phase -eq 3 -and $script:samples -eq 2)
$create="BossLog #0: Character::Create placement 65092 (arg2 0, flag 1, arg4 0) t=12.345 s +0.0 s"+[char]10
$encounter="Rematch: arena: BossBattleMode start - player now 1 2 3; mission 0x1CBC, retry point 67064, placements created: 65092(-1.1s)"+[char]10
Check (EHP-BossEvidence '' ($summon+$create+$encounter))
Check (-not(EHP-BossEvidence '' ($summon+$encounter))) # old ring entry alone
Check (-not(EHP-BossEvidence '' ($create+$summon+$encounter)))
Check (-not(EHP-BossEvidence '' ($summon+$encounter+$create)))
Check (-not(EHP-BossEvidence $create ($create+$summon+$encounter))) # pre-Summon creation
Check (-not(EHP-BossEvidence '' ($summon+$create.Replace('65092','650920')+$encounter)))
Check (-not(EHP-BossEvidence '' ($summon+$create+$encounter.Replace('67064','670640'))))
Refuses {EHP-BossEvidence '' ($summon+$summon+$create+$encounter)}
Refuses {EHP-BossEvidence 'prefix' ($summon+$create+$encounter)}
Check ($text -match "'VirtualPad' '0'" -and $text -match 'Enable-WritableSavePolicy')
Check ($text -match "'RematchLog','1'")
Check ($text -notmatch "EHP-Wait 'renewed'")
Write-Host "PASS $n combined-entry-hold policy/actual-observer/plan checks; no native actions"
