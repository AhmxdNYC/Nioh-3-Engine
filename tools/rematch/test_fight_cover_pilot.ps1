param([string]$LifecycleRepo='C:\Users\Ahmad Hamza\nioh3-engine')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'fight_cover_pilot_lib.ps1')
FCP-CheckDependencies $LifecycleRepo
. (Join-Path $LifecycleRepo 'tools/rematch/training_pilot_lib.ps1')
Training-CheckDependencies $LifecycleRepo
. (Join-Path $LifecycleRepo 'tools/rematch/c72e_pilot_lib.ps1')
. (Join-Path $LifecycleRepo 'tools/rematch/c72e_binary_lib.ps1')
$script:n=0
function Check($Value,[string]$Name) { $script:n++;if (-not $Value) { throw "FAIL $Name" } }
function Reject([scriptblock]$Fn,[string]$Name) { $failed=$false;try { &$Fn } catch { $failed=$true };Check $failed $Name }
$before="old log`n"
$begin='Rematch: cover token=42 phase=1 attempt=0 event=begin mission=0x1CBC start=67064 placement=65092'
$ack='Rematch: cover token=42 phase=2 attempt=0 event=present_ack'
$post='Rematch: cover token=42 phase=2 attempt=7 event=native_post'
$ready='Rematch: cover token=42 phase=4 attempt=7 event=phase reason=ready'
$log=$before+(@($begin,$ack,$post,$ready)-join "`n")
$reply='token=42 phase=4 attempt=7 mission=0x1CBC start=67064 placement=65092 presented=1 issued=1 bossEntry=1 placementEntry=1 deadline=123456 control=1 algo=1 life=1 playerAlive=1 sample=123000 reason=ready'
Check (FCP-Evidence 41 $before $log $reply) 'exact ordered cover evidence'
Check (-not (FCP-Evidence 42 $before $log $reply)) 'old token not new entry'
Reject { FCP-Evidence 41 $before ($before+(@($begin,$post,$ready)-join "`n")) $reply } 'missing ACK'
Reject { FCP-Evidence 41 $before ($before+(@($begin,$post,$ack,$ready)-join "`n")) $reply } 'post before ACK'
Reject { FCP-Evidence 41 $before ($log.Replace('phase=4 attempt=7','phase=4 attempt=8')) $reply } 'attempt changed'
Reject { FCP-Evidence 41 $before ($log.Replace('token=42 phase=2','token=43 phase=2')) $reply } 'foreign token'
Reject { FCP-Evidence 41 $before $log ($reply.Replace('placement=65092','placement=1')) } 'wrong placement'
foreach ($bit in @('presented','issued','bossEntry','placementEntry','control','algo','life','playerAlive')) { Reject { FCP-Evidence 41 $before $log ($reply.Replace("$bit=1","$bit=0")) } "missing $bit" }
foreach ($phase in @(5,6)) { Reject { FCP-Evidence 41 $before $log ($reply.Replace('phase=4',"phase=$phase")) } "failure/cancel phase$phase" }
Reject { FCP-Evidence 41 $before ($before+"Rematch: cover token=42 phase=5 attempt=7 event=phase reason=failed`n"+$log.Substring($before.Length)) $reply } 'failure cannot become PASS'
Reject { FCP-Evidence 41 $before 'replacement' $reply } 'log replacement'
Reject { FCP-Parse ($reply+' token=42') } 'duplicate field'
$req=[pscustomobject]@{nonce='nonce';stage='ControlReviewed';owner_pid=1;owner_start='a';runner_pid=2;runner_start='b';game_pid=3;game_start='c'}
$a=$req|Select-Object *;$a|Add-Member evidence 'visible boss, cover disappeared, camera/walk response';foreach ($k in @('visible_arena','visible_boss','control_response')) {$a|Add-Member $k $true}
FCP-CheckApproval $req $a;Check $true 'exact root review'
foreach ($k in @('nonce','stage','owner_pid','owner_start','runner_pid','runner_start','game_pid','game_start')) {
    $bad=$a|Select-Object *;$bad.$k='foreign';Reject { FCP-CheckApproval $req $bad } "approval $k"
}
$unset=@{mod_tree='';engine_sha256='';addon_sha256='';gate_receipt_sha256='';admission_receipt_sha256=''}
Reject { FCP-Admission $unset @{} '' '' @{} '' 1 'a' } 'unset frozen admission'
Reject { FCP-Admission (Get-FCPExpected) @{} '' '' @{} '' 1 'a' } 'missing actual receipt'
$expected=@{mod_tree=('a'*40);engine_sha256=('b'*64);addon_sha256=('c'*64);gate_receipt_sha256=('d'*64);admission_receipt_sha256=('e'*64)}
$ad=@{approved=$true;scope='ONE_FIGHT_COVER_NATIVE_PILOT_NEW_FEATURE_UNTESTED';owner_pid=1;owner_start='a';mod_tree=$expected.mod_tree;engine_sha256=$expected.engine_sha256;addon_sha256=$expected.addon_sha256}
$hashes=@{'Nioh3EngineFix.asi'=$expected.engine_sha256;'Nioh3BossRematch.asi'=$expected.addon_sha256}
$proof=FCP-Admission $expected $ad $expected.admission_receipt_sha256 $expected.mod_tree $hashes $expected.gate_receipt_sha256 1 'a';Check (-not $proof.old_pass_transferred) 'new admission distinct'
Reject { FCP-Admission $expected $ad $expected.admission_receipt_sha256 ('f'*40) $hashes $expected.gate_receipt_sha256 1 'a' } 'mod lineage changed'
Reject { FCP-Admission $expected $ad $expected.admission_receipt_sha256 $expected.mod_tree $hashes ('f'*64) 1 'a' } 'old receipt mismatch'
# Exact native return helper accepts mission zero ONLY when bound to original entry.
$entry=@{mission='0x0';stage='4019512'}
$return="Rematch: exit: MissionRequestReturn(params, 1) from mission 0x1CBC (retry point 67064) -> requested`nRematch: replay mission exited (stage now 4019512, mission state +28h 0x0)`n"
$status='state=0 loading=0 player=1 mission=0x0 stage=4019512 infight=0 warps=0 spawn=- gate=ok'
Check (Training-ReturnEvidence '' $return $status $entry) 'exact original zero tuple'
Check (-not (Training-ReturnEvidence '' $return ($status.Replace('4019512','1')) $entry)) 'wrong return stage'
Check (-not (Training-ReturnEvidence '' '' $status $entry)) 'no native return event'
# Exercise actual runner outer finally: cleanup failure must preserve guard ownership.
$tokens=$null;$errors=$null;$path=Join-Path $PSScriptRoot 'run_fight_cover_pilot.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors);Check (-not $errors) 'runner parses'
$outer=@($ast.EndBlock.Statements|Where-Object { $_ -is [Management.Automation.Language.TryStatementAst] })[-1]
$finally=[scriptblock]::Create(($outer.Finally.Statements|ForEach-Object {$_.Extent.Text}) -join "`n")
function Finish-C72 { throw 'fixture restoration failed' }
function Done-Guard { param([switch]$KeepFile) $script:kept=[bool]$KeepFile }
$script:cleaned=$false;$script:kept=$false;try { &$finally } catch {};Check $script:kept 'failed finalizer retains guard'
function Finish-C72 { $script:cleaned=$true }
$script:cleaned=$false;$script:kept=$true;&$finally;Check (-not $script:kept) 'successful restoration releases guard'
$r=@{status='FIGHT_COVER_REVIEWED_RETURN_OBSERVED';root_control_confirmed=$true;native_return_confirmed=$true;saves_restored=$true;inis_restored=$true;binaries_restored=$true}
Check (FCP-Complete $r) 'complete only with all receipts'
foreach ($k in @('root_control_confirmed','native_return_confirmed','saves_restored','inis_restored','binaries_restored')) {$bad=$r.Clone();$bad[$k]=$false;Check (-not (FCP-Complete $bad)) "incomplete $k"}
$plan=& $path -DryRun | ConvertFrom-Json;Check ($plan.mode -eq 'PLAN_ONLY_NO_MUTATION' -and $plan.expected.mod_tree -eq (Get-FCPExpected).mod_tree) 'dryrun remains read-only with frozen pins'
foreach ($k in @('visible_arena','visible_boss','control_response')) {$bad=$a|Select-Object *;$bad.$k=$false;Reject {FCP-CheckApproval $req $bad} "missing visual $k"}
$t=@{since=$null;last=$null};$s='mission=0x1CBC stage=67064 loading=0 player=1 gate=rematch active'
foreach ($ms in @(0,1000,2000,3000)) { Check (-not (FCP-ReturnSettled $t $ms $s)) 'return settles4s' };Check (FCP-ReturnSettled $t 4000 $s) 'return proxysettled'
Check (-not (FCP-ReturnSettled $t 4100 ($s.Replace('loading=0','loading=1')))) 'loading resetsreturn'
foreach ($v in @('failed','recovery','invented')) { Reject {FCP-CheckStatus ("state=3 loading=0 player=1 mission=0x1CBC stage=67064 infight=1 warps=0 spawn=$v gate=rematch active")} "spawn $v" }
foreach ($v in @('-','pending','applied','arena','start')) {FCP-CheckStatus ("state=3 loading=0 player=1 mission=0x1CBC stage=67064 infight=1 warps=1 spawn=$v gate=rematch active");Check $true "native spawn $v"}
$baseline=FCP-Parse $reply
FCP-CheckPreSubmit $baseline $reply;Check $true 'navigation preserves original baseline'
foreach ($pair in @(@('token=42','token=43'),@('phase=4','phase=1'),@('attempt=7','attempt=8'),@('mission=0x1CBC','mission=0xC72E'),@('sample=123000','sample=123001'))) {
    Reject { FCP-CheckPreSubmit $baseline ($reply.Replace($pair[0],$pair[1])) } 'native event during navigation refused'
}
$navReq=$req|Select-Object *;$navReq.stage='FightReady';$navApproval=$a|Select-Object *;$navApproval.stage='FightReady'
FCP-CheckApproval $navReq $navApproval;Check $true 'owned FightReady accepted'
Reject { FCP-CheckApproval $req $navApproval } 'FightReady cannot approve control'
$navApproval.nonce='previous-run';Reject { FCP-CheckApproval $navReq $navApproval } 'stale navigation approval'
$source=[IO.File]::ReadAllText($path)
Check ($source.IndexOf('$script:requestLogBefore=C72-ReadShared $addonLog') -lt $source.IndexOf("Training-WaitStage 'FightReady'") -and $source.IndexOf("Training-WaitStage 'FightReady'") -lt $source.IndexOf('$coverReady=$false;$until=')) 'capture baseline then manual stage then one fight deadline'
Check ($source.Contains("if (`$Stage -eq 'FightReady') { FCP-CheckPreSubmit `$baseline (Invoke-C72Pipe 'cover') }") -and $source.Contains('Assert-Owned')) 'navigation continuously guarded and rejects early submit'
Write-Host "PASS $script:n fight cover pilot checks"
