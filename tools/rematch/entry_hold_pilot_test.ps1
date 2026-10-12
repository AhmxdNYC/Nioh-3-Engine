$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'entry_hold_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'training_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
$n=0
function Check($v){if(-not $v){throw "Fixture line $((Get-PSCallStack)[1].ScriptLineNumber)"};$script:n++}
function Refuses([scriptblock]$b){$bad=$false;try{&$b|Out-Null}catch{$bad=$true};Check $bad}
$now=[datetime]'2026-10-12T04:00:00Z'
$a=@{approved=$true;scope='ONE_FRESH_YAMAGATA_ENTRY_HOLD_PILOT';owner_pid=43708;owner_start='2026-10-11T02:34:24.0733347-04:00';source='dbd6752b0b54c51e037a2999976ce94a348ab676';mod_tree='4f4b3c045565244917fb1b96496a0a0596b3cf0c';manifest_sha256='f9847291b9a3fc9e1a212604b36fdcbe94e873bb3ae8c4fa7f590772b140a6bf';engine_sha256='ac46084cd7169cbd3ef9ba28e26bd92707ad275bcce86421c7f02e3f58846856';addon_sha256='63a7d68142912dbfa7d4059a91a9bd2955b576db6727150b6b50b9e6e626a803';pilot_sha256='pilot';library_sha256='lib';out_dir='D:/unique';nonce=('a'*32);review='offline';created_utc='2026-10-12T03:59:00Z';expires_utc='2026-10-12T04:10:00Z'}
EHP-Admission ([pscustomobject]$a) 'D:/unique' 'pilot' 'lib' $now;Check $true
foreach($key in @('scope','owner_pid','owner_start','source','mod_tree','manifest_sha256','engine_sha256','addon_sha256','pilot_sha256','library_sha256','out_dir','nonce','review')){$bad=$a.Clone();$bad[$key]='';Refuses {EHP-Admission ([pscustomobject]$bad) 'D:/unique' 'pilot' 'lib' $now}}
foreach($pair in @(@('approved','true'),@('expires_utc','2026-10-12T03:59:00Z'),@('expires_utc','2026-10-13T04:00:00Z'),@('created_utc','2026-10-12T04:01:00Z'))){$bad=$a.Clone();$bad[$pair[0]]=$pair[1];Refuses {EHP-Admission ([pscustomobject]$bad) 'D:/unique' 'pilot' 'lib' $now}}
$raw='enabled=1 phase=2 attempt=7 loadSeen=1 arrived=1 boxAt=1000 deadline=10000 broken=0 reason=waiting'
$h=EHP-Parse $raw
$status='state=2 row=R1CBC infight=0 warps=0 spawn=- mission=0x1CBC stage=67064 loading=0 player=1 gate=rematch active'
EHP-Held $h 7 1200 $status;Check $true
foreach($pair in @(@('enabled',0),@('phase',1),@('phase',3),@('phase',4),@('attempt',8),@('loadSeen',0),@('arrived',0),@('boxAt',0),@('boxAt',1201),@('broken',1),@('deadline',1200))){$bad=$h.Clone();$bad[$pair[0]]=$pair[1];Refuses {EHP-Held $bad 7 1200 $status}}
Refuses {EHP-Held $h 7 2501 $status}
foreach($change in @(@('infight=0','infight=1'),@('mission=0x1CBC','mission=0xC72E'),@('warps=0','warps=1'),@('spawn=-','spawn=arena'),@('player=1','player=0'),@('loading=0','loading=1'))){Refuses {EHP-Held $h 7 1200 $status.Replace($change[0],$change[1])}}
foreach($key in @('enabled','phase','attempt','loadSeen','arrived','boxAt','deadline','broken')){Refuses {EHP-Parse ($raw+' '+$key+'=1')}}
$start="Rematch: request Practice Yamagata (native shrine) for R1CBC queued from the practice menu"+[char]10
$summon="Rematch: request release Yamagata entrance- queued from practice entrance menu"+[char]10
$boss="arena: BossBattleMode start mission 0x1CBC placements65092"+[char]10
Check ((EHP-Log 'prefix' ('prefix'+$start) $false) -ceq $start)
Check ((EHP-Log '' ($start+$summon+$boss) $true) -match '65092')
foreach($text in @($start+$start,$start+$summon,$start+$boss,$start+'Rematch: refused: unknown',$start+'EXCEPTION',$start+'Rematch: request Fight now queued from pipe')){Refuses {EHP-Log '' $text $false}}
Refuses {EHP-Log 'prefix' ('replaced'+$start) $false}
Refuses {EHP-Log '' ($start+$summon+$summon) $true}
$r=@{nonce='n';stage='HoldMenuReviewed';owner_pid=43708;owner_start='s';runner_pid=2;runner_start='r';game_pid=3;game_start='g'}
$approved=$r.Clone();$approved.evidence='observed';$approved.visible_menu=$true;$approved.held_without_boss=$true
EHP-Approval ([pscustomobject]$r) ([pscustomobject]$approved);Check $true
foreach($key in $r.Keys){$bad=$approved.Clone();$bad[$key]='wrong';Refuses {EHP-Approval ([pscustomobject]$r) ([pscustomobject]$bad)}}
foreach($key in @('visible_menu','held_without_boss')){$bad=$approved.Clone();$bad[$key]='true';Refuses {EHP-Approval ([pscustomobject]$r) ([pscustomobject]$bad)}}
$tokens=$null;$errors=$null;$path=Join-Path $PSScriptRoot 'entry_hold_pilot.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors);Check (-not $errors)
$fn=$ast.Find({param($x)$x -is [Management.Automation.Language.FunctionDefinitionAst] -and $x.Name -eq 'EHP-Wait'},$true)
Invoke-Expression $fn.Extent.Text
function EHP-Sample {return $script:fixture}
$script:attempt=7;$script:requestLogBefore='';$script:fixture=@{hold=@{phase=3;attempt=7};status=$status;log=$start+$summon}
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
Check ($text -match 'Training-ImportLifecycle' -and $text -match 'finally \{ try \{ Finish-C72 \} finally \{ Done-Guard')
$plan=& $path | ConvertFrom-Json;Check ($plan.mode -eq 'PLAN_ONLY' -and -not $plan.auto_input -and $plan.launches -eq 1)
# Actual runner menu-stage loop: paused native hold stays valid past 1500ms.
Add-Type 'public static class EHPClock { public static ulong GetTickCount64(){return 5000;} }'
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
 $v.evidence='fixture observed';$v.visible_menu=$true;$v.held_without_boss=$true
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
# Renewal is a NEW actual positive observation, never synthesized from review.
$script:reviewBoxAt=1000
Refuses {EHP-Wait 'renewed' 0} # preserved but old witness
$script:fixture.hold.boxAt=2000;Refuses {EHP-Wait 'renewed' 0} # new but stale
$script:fixture.hold.boxAt=4900;Check ((EHP-Wait 'renewed' 0).hold.boxAt -eq 4900)
$script:fixture.hold.attempt=8;Refuses {EHP-Wait 'renewed' 0};$script:fixture.hold.attempt=7
$script:fixture.log=$start+$summon;Refuses {EHP-Wait 'renewed' 0}
Write-Host "PASS $n entry-hold policy/actual-observer/plan checks; no native actions"
