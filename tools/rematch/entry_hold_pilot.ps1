# Dedicated local preparation. Default PLAN never launches or edits configuration.
[CmdletBinding()]
param([switch]$Run,[switch]$DryRun,[int]$OwnerPid=43708,[string]$AdmissionReceipt='',
 [string]$AdmissionSha256='',[string]$OutDir='',
 [ValidateSet('FieldReady','EnterArena','HoldMenuReviewed','ControlReviewed','NativeReturn')][string]$ApproveStage='',
 [string]$Evidence='',[switch]$VisibleMenu,[switch]$HeldWithoutBoss,[switch]$VisibleBoss,[switch]$ControlResponse,
 [ValidateRange(5,25)][int]$MaxMinutes=20,
 [string]$GameDir='C:\Program Files (x86)\Steam\steamapps\common\Nioh3',[string]$SaveRoot='',
 [string]$CandidateRoot='D:\nioh3-re\candidates\training-entry-hold-dbd6752')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'entry_hold_pilot_lib.ps1')
$plan=@{mode='PLAN_ONLY';scope='ONE_FRESH_YAMAGATA_ENTRY_HOLD_PILOT';archive=$CandidateRoot;manifest_sha256='f9847291b9a3fc9e1a212604b36fdcbe94e873bb3ae8c4fa7f590772b140a6bf';owner_pid=43708;owner_start='2026-10-11T02:34:24.0733347-04:00';route='R1CBC/67064/65092/box66599';stages=@('FieldReady','EnterArena','HoldMenuReviewed','ControlReviewed','NativeReturn');launches=1;native_factory=$false;auto_input=$false;retry=$false;requires='NEW ROOT REVIEWED ADMISSION; no old acceptance transferred'}
if(($Run -and $DryRun) -or ($ApproveStage -and ($Run -or $DryRun))){throw 'Choose plan/run/approval'}
if(-not $Run -and -not $ApproveStage){$plan|ConvertTo-Json -Depth 5;return}
if($OwnerPid -ne 43708 -or (Get-Process -Id $OwnerPid).StartTime.ToString('o') -cne '2026-10-11T02:34:24.0733347-04:00'){throw 'Exact root owner no longer available'}
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
# These sources import functions only. Hash changes require review, never automatic repinning.
$pins=@{'tools/rematch/training_pilot_lib.ps1'='50192ec2eb4f0cda36d15bb5a14b210d996fd795bb871ec5dd041476b13f1f28';'tools/rematch/c72e_pilot_lib.ps1'='03fd4554849f23eba0f69992b60d1723352b854cbf306ca1f1361b0525e0d94d';'tools/rematch/c72e_binary_lib.ps1'='ae16d3011b0966ec156aed2218eaa3e86d6a1b690279776babc29feb570617b6';'tools/rematch/run_c72e_native_pilot.ps1'='a2e077a66708c2ed1278bc9bddc1c64489c5a0127d4178b6efee48f199925ea6';'tools/playsession/session_lib.ps1'='98324e91ce8410edc2d519ac0855534d7e78495eb5582d297277c12fd5a248ee';'tools/testmatrix/harness_guard.ps1'='e2e6034c240e74806a2885ea053b23bf8d3c37aed89f0b4e9fa72b30447c6c81'}
foreach($rel in $pins.Keys){if((Get-FileHash -LiteralPath (Join-Path $repo $rel) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pins[$rel]){throw "Pilot dependency changed: $rel"}}
. (Join-Path $PSScriptRoot 'training_pilot_lib.ps1');Training-CheckDependencies $repo
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_binary_lib.ps1')
if($ApproveStage){
 $request=Get-Content -LiteralPath (C72-Child $OutDir 'awaiting.json') -Raw|ConvertFrom-Json
 if($request.stage -cne $ApproveStage -or $request.owner_pid -ne $OwnerPid){throw 'Wrong approval session/stage'}
 foreach($pair in @(@($request.runner_pid,$request.runner_start),@($request.game_pid,$request.game_start))){if((Get-Process -Id $pair[0]).StartTime.ToString('o') -cne $pair[1]){throw 'Approval identity expired'}}
 $approval=$request|Select-Object *;$approval|Add-Member evidence $Evidence
 $approval|Add-Member visible_menu ([bool]$VisibleMenu);$approval|Add-Member held_without_boss ([bool]$HeldWithoutBoss);$approval|Add-Member visible_boss ([bool]$VisibleBoss);$approval|Add-Member control_response ([bool]$ControlResponse)
 EHP-Approval $request $approval
 $dest=C72-Child $OutDir ('approve-'+$ApproveStage+'.json');$tmp=C72-Child $OutDir ([guid]::NewGuid().ToString('N')+'.tmp')
 try{C72-Json $tmp $approval;[IO.File]::Move($tmp,$dest)}finally{if(Test-Path -LiteralPath $tmp){Remove-Item -LiteralPath $tmp}}
 return
}
if(-not $OutDir -or $AdmissionSha256 -notmatch '^[a-f0-9]{64}$' -or -not $AdmissionReceipt){throw 'Fresh root admission/hash and unique OutDir required'}
$OutDir=C72-Full $OutDir
if((C72-BinaryHash $AdmissionReceipt) -cne $AdmissionSha256){throw 'Admission bytes differ'}
$hostedAdmission=Get-Content -LiteralPath $AdmissionReceipt -Raw|ConvertFrom-Json
EHP-Admission $hostedAdmission $OutDir (C72-BinaryHash $PSCommandPath) (C72-BinaryHash (Join-Path $PSScriptRoot 'entry_hold_pilot_lib.ps1')) ([datetime]::UtcNow)
if((C72-BinaryHash (Join-Path $CandidateRoot 'manifest.json')) -cne $plan.manifest_sha256){throw 'Archive manifest changed'}
$manifest=Get-Content -LiteralPath (Join-Path $CandidateRoot 'manifest.json') -Raw|ConvertFrom-Json
foreach($f in $manifest.files.psobject.Properties){if((C72-BinaryHash (C72-Child $CandidateRoot $f.Name)) -cne $f.Value){throw 'Archive artifact changed'}}
if($env:HG_MIN_HEADROOM_GB -and [double]$env:HG_MIN_HEADROOM_GB -lt 18){throw 'Requires18GB headroom'}
if(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue){throw 'Game already active'}
$hashes=@{'Nioh3EngineFix.asi'='dd8b5739062e364dfc708bd1f0f89dfc2b97477d66ae1fbfdfb6f50b0d4f2891';'Nioh3BossRematch.asi'='02d763e76037365e8814820fc1ef9d3d713710435f63099d04d617e05966d4d1'}
$runtimeHashes=@{'Nioh3EngineFix.asi'=$hostedAdmission.engine_sha256;'Nioh3BossRematch.asi'=$hostedAdmission.addon_sha256}
$catalog=Join-Path $CandidateRoot 'Nioh3BossRematch_bosses.tsv';$plan.catalog_sha256='7c0c158df9b7c5bc0ce89b6b6fee3ee384c7db94bde892eb38ad78741aad221b'
$Kit=$CandidateRoot
Add-Type 'using System;using System.Runtime.InteropServices;public static class EHPClock{[DllImport("kernel32.dll")]public static extern ulong GetTickCount64();}'
$BackupRoot='';$CaptureRoot='';$StateDir='';$StashRoot='';$OptiSrc='';$global:LockKey=''
. (Join-Path $repo 'tools\playsession\session_lib.ps1')
. (Join-Path $repo 'tools\testmatrix\harness_guard.ps1')
Training-ImportLifecycle (Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1')
$GameDir=C72-Full $GameDir;$SaveRoot=C72-Full $SaveRoot;$Kit=$CandidateRoot
$binaryPlan=@(C72-BinaryPlan $GameDir $hashes @{
    'Nioh3EngineFix.asi'=@{path=(Training-CandidatePath $CandidateRoot 'Nioh3EngineFix' 'Flat');hash=$runtimeHashes['Nioh3EngineFix.asi']};
    'Nioh3BossRematch.asi'=@{path=(Training-CandidatePath $CandidateRoot 'Nioh3BossRematch' 'Flat');hash=$runtimeHashes['Nioh3BossRematch.asi']}})
if ((C72-BinaryHash $catalog) -ne $plan.catalog_sha256) { throw 'Reviewed catalogue changed' }
if (-not (Test-Path -LiteralPath $SaveRoot -PathType Container)) { throw 'Entry save root missing' }
$engineIni=Join-Path $GameDir 'Nioh3EngineFix.ini';$rematchIni=Join-Path $GameDir 'Nioh3BossRematch.ini';$addonLog=Join-Path $GameDir 'Nioh3BossRematch.log'
foreach ($p in @($engineIni,$rematchIni)) { if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "INI missing: $p" } }
if (-not $OutDir) { $OutDir=Join-Path 'D:\nioh3-re\rematch-pilots' ('entry-hold-R1CBC-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+$PID) }
$OutDir=C72-Full $OutDir
if (Test-Path -LiteralPath $OutDir) { throw 'Session directory must be new' }
foreach ($root in @($GameDir,$SaveRoot,$Kit,$repo,$CandidateRoot)) { if ($OutDir -eq (C72-Full $root) -or (C72-Inside $root $OutDir) -or (C72-Inside $OutDir $root)) { throw 'Session overlaps protected root' } }
$null=New-Item -ItemType Directory -Path $OutDir
C72-Json (Join-Path $OutDir 'entry-hold-admission.json') $hostedAdmission
$script:HGdir=Join-Path $OutDir 'guard';$entryDir=Join-Path $OutDir 'entry-saves';$iniDir=Join-Path $OutDir 'entry-inis';$events=Join-Path $OutDir 'events.jsonl'
$script:owned=$null;$script:saveHandles=$null;$script:entry=$null;$script:touched=$false;$script:cleaned=$false;$script:binaryRecord=$null
$script:held=New-Object 'System.Collections.Generic.List[string]';$script:watch=[Diagnostics.Stopwatch]::StartNew()
$script:result=[ordered]@{route='R1CBC';feature_scope='ENTRY_HOLD_ONE_SHIPPED_ENCOUNTER';status='PREPARING';error=$null;saves_restored=$false;inis_restored=$false;binaries_restored=$false;hold_menu_reviewed=$false;root_control_confirmed=$false;native_return_confirmed=$false}
$LockId="pc-entry-hold-$PID"
function Training-FreshLog { return Training-LogTail $script:requestLogBefore (C72-ReadShared $addonLog) }
function EHP-Sample {
 Assert-Owned
 $log=C72-ReadShared $addonLog;$status=Invoke-C72Pipe 'status';Training-CheckStatus $status
 $hold=EHP-Parse (Invoke-C72Pipe 'practicehold')
 if($script:attempt -and $hold.attempt -ne $script:attempt){throw 'Practice hold attempt changed'}
 if($hold.phase -eq 4){throw 'Entrance hold abandoned; native request may still proceed'}
 if($script:requestLogBefore){$null=EHP-Log $script:requestLogBefore $log $script:summonAllowed}
 C72-Event $events 'hold-sample' (@{status=$status;hold=$hold}|ConvertTo-Json -Compress) $script:watch.Elapsed.TotalMilliseconds
 return @{log=$log;status=$status;hold=$hold}
}
function EHP-Wait([string]$Kind,[int]$Seconds=180) {
 $until=[datetime]::UtcNow.AddSeconds($Seconds);$load=$false
 do {
  $x=EHP-Sample;$h=$x.hold;$s=$x.status
  if((C72-Field $s 'loading') -eq '1'){$load=$true}
  if($Kind -eq 'shrine' -and (Training-AtShrine $s $load) -and $h.enabled -eq 1 -and $h.phase -in @(1,2) -and $h.attempt -gt $script:baselineAttempt -and $h.loadSeen -eq 1){return $x}
  if($Kind -eq 'renewed') {
   EHP-Held $h $script:attempt ([EHPClock]::GetTickCount64()) $s -Preserved
   if($h.boxAt -gt $script:reviewBoxAt -and [EHPClock]::GetTickCount64()-$h.boxAt -le 1500){EHP-Held $h $script:attempt ([EHPClock]::GetTickCount64()) $s;return $x}
  }
  if($Kind -eq 'held' -and $h.phase -eq 2){EHP-Held $h $script:attempt ([EHPClock]::GetTickCount64()) $s;return $x}
  if($Kind -eq 'released' -and $h.phase -eq 3){
   $tail=EHP-Log $script:requestLogBefore $x.log $true
   if(@([regex]::Matches($tail,'Rematch: request release Yamagata entrance[^\r\n]*queued from practice entrance menu')).Count -ne 1){throw 'Release lacks one menu Summon'}
   return $x
  }
  if($Kind -eq 'boss' -and (C72-Field $s 'mission') -eq '0x1CBC' -and (C72-Field $s 'stage') -eq '67064' -and (C72-Field $s 'loading') -eq '0' -and (C72-Field $s 'player') -eq '1' -and (C72-Field $s 'infight') -eq '1' -and $h.phase -eq 3 -and (Training-LogTail $script:summonLogBefore $x.log) -match 'arena: BossBattleMode start[^\r\n]*mission 0x1CBC[^\r\n]*65092'){return $x}
  if($Kind -eq 'return' -and (Training-ReturnEvidence $script:returnLogBefore $x.log $s $script:result.entry_tuple)){return $x}
  Start-Sleep -Milliseconds 100
 }while([datetime]::UtcNow -lt $until)
 throw "Bounded $Kind incomplete; no repeated request"
}
function EHP-WaitStage([string]$Stage) {
 $request=[ordered]@{stage=$Stage;nonce=[guid]::NewGuid().ToString('N');owner_pid=$OwnerPid;owner_start=$global:HG.ownerStart.ToString('o');runner_pid=$PID;runner_start=(Get-Process -Id $PID).StartTime.ToString('o');game_pid=$script:owned.Id;game_start=$script:owned.StartTime.ToString('o')}
 C72-Json (Join-Path $OutDir 'awaiting.json') $request
 Write-Host "WAITING ROOT: $Stage; $OutDir"
 $file=Join-Path $OutDir "approve-$Stage.json"
 do {
  Assert-Owned
  if($Stage -ne 'FieldReady'){
   $x=EHP-Sample
   if($Stage -eq 'HoldMenuReviewed'){EHP-Held $x.hold $script:attempt ([EHPClock]::GetTickCount64()) $x.status -Preserved}
  }
  if(Test-Path -LiteralPath $file){break};Start-Sleep -Milliseconds 200
 }while($true)
 $a=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json;EHP-Approval ([pscustomobject]$request) $a
 foreach($pair in @(@($request.game_pid,$request.game_start),@($request.runner_pid,$request.runner_start))){if((Get-Process -Id $pair[0]).StartTime.ToString('o') -cne $pair[1]){throw 'Approval process changed'}}
 C72-Event $events 'root-confirmation' ($a|ConvertTo-Json -Compress) $script:watch.Elapsed.TotalMilliseconds
 Remove-Item -LiteralPath (Join-Path $OutDir 'awaiting.json')
}
$script:attempt=0;$script:baselineAttempt=0;$script:summonAllowed=$false;$script:requestLogBefore=''
Init-Guard -Name entry_hold_pilot -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -MaxCrashes 1 -Cleanup { Finish-C72 }
try {
    $global:LockKey='nioh3-pc';$r=Invoke-GameLock 'acquire' 1800;if ($r.code) { throw 'PC lease unavailable' };$script:held.Add('nioh3-pc')
    Assert-Owner;if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Game appeared before preparation' }
    Add-Type -AssemblyName System.Drawing,System.Windows.Forms
    $script:entry=C72-Manifest $SaveRoot;if (-not $script:entry.Count) { throw 'Empty save tree refused' }
    C72-Backup $SaveRoot $entryDir $script:entry;$script:saveHandles=C72-LockSaves $SaveRoot $script:entry
    $null=New-Item -ItemType Directory -Path $iniDir
    foreach ($n in @('Nioh3EngineFix.ini','Nioh3BossRematch.ini')) { Copy-Item -LiteralPath (Join-Path $GameDir $n) -Destination (Join-Path $iniDir $n) }
    C72-Json (Join-Path $OutDir 'entry-manifest.json') @{plan=$plan;saves=$script:entry;admission_sha256=(C72-BinaryHash $AdmissionReceipt);binary_plan=$binaryPlan}
    $script:binaryRecord=C72-BinaryPrepare $binaryPlan (Join-Path $OutDir 'entry-binaries');C72-BinaryStage $script:binaryRecord
    $empty=Join-Path $OutDir 'empty-arenas.tsv';[IO.File]::WriteAllText($empty,"# no arena overrides`r`n")
    $lines=[IO.File]::ReadAllLines($rematchIni);C72-UniqueIni $lines @('Rematch','Remote','Menu','Debug','Training')
    foreach ($kv in @(@('Training','EntryHold','1'),@('Rematch','Enable','1'),@('Rematch','FightNow','1'),@('Rematch','FightNowWarp','0'),@('Rematch','FightNowSpawn','0'),@('Rematch','ArenaCapture','0'),@('Rematch','SelfTest',''),@('Rematch','Catalog',$catalog),@('Rematch','Arenas',$empty),@('Remote','Pipe','1'),@('Menu','Overlay','1'),@('Debug','BossLog','1'),@('Debug','RematchLog','0'))) { $lines=Set-IniValue $lines $kv[0] $kv[1] $kv[2] }
    $perf=[IO.File]::ReadAllLines($engineIni);C72-UniqueIni $perf @('General','Debug');$perf=Set-IniValue $perf 'General' 'AutoContinueLoad' '1';$perf=Set-IniValue $perf 'Debug' 'Tour' '0'
    $script:touched=$true;[IO.File]::WriteAllLines($rematchIni,$lines,(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllLines($engineIni,$perf,(New-Object Text.UTF8Encoding($false)))
    Check-Asis $runtimeHashes;Assert-Launch;if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Refusing second launch' }
    $launched=[DateTime]::Now;Start-Process -FilePath 'steam://rungameid/3681010' -WindowStyle Hidden
    $until=[DateTime]::UtcNow.AddSeconds(120)
    do { $games=@(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue);if ($games.Count -gt 1) { throw 'Multiple games appeared' };if ($games.Count -eq 1 -and (Get-C72LaunchIdentity $games[0] $launched (Join-Path $GameDir 'Nioh3.exe')) -eq 'owned') { $script:owned=$games[0];break };Start-Sleep -Milliseconds 250 } while ([DateTime]::UtcNow -lt $until)
    if (-not $script:owned) { throw 'No owned game appeared; no relaunch' }
    $global:HG.games=@(@{pid=$script:owned.Id;name='Nioh3';start=$script:owned.StartTime.ToString('o');closed=$false;counted=$false});HG-Save
    Write-Host 'Root manually selects Continue once. Waiting for explicit FieldReady confirmation.'
    EHP-WaitStage 'FieldReady'
    $readyTracker=@{since=$null;last=$null};$ready=$false;$until=[DateTime]::UtcNow.AddSeconds(180)
    do {
        $readable=$true
        try { $s=Invoke-C72Pipe 'status' } catch { $readable=$false;$s='';Assert-Owned }
        if ($readable) { Training-CheckStatus $s }
        $ready=C72-EntrySettled $readyTracker $script:watch.ElapsedMilliseconds $s $readable
        if (-not $ready) { Start-Sleep -Milliseconds 250 }
    } while (-not $ready -and [DateTime]::UtcNow -lt $until)
    if (-not $ready) { throw 'No continuously ready offline field for 12s within 180s after root confirmation' }
    $script:result.entry_tuple=@{mission=(C72-Field $s 'mission');stage=(C72-Field $s 'stage');reply=$s}
    if($script:result.entry_tuple.mission -eq '0x1CBC'){throw 'Fresh Practice requires entry outside target mission'}
    $baseline=EHP-Parse (Invoke-C72Pipe 'practicehold');if($baseline.enabled -ne 1 -or $baseline.phase -ne 0){throw 'Hold baseline is not fresh/off'};$script:baselineAttempt=$baseline.attempt
    $beforeGen=Training-BeginScan
    $cleared=$false;$until=[DateTime]::UtcNow.AddSeconds(15)
    do { $row=Invoke-C72Pipe 'row R1CBC';$gen=C72-Field $row 'gen';$cleared=(C72-Field $row 'status') -eq '2' -and $gen -match '^\d+$' -and [uint64]$gen -gt [uint64]$beforeGen;if (-not $cleared) { Start-Sleep -Milliseconds 200 } } while (-not $cleared -and [DateTime]::UtcNow -lt $until)
    if (-not $cleared -or $row -notmatch 'mission=0x1CBC\b' -or $row -notmatch 'placement=65092\b' -or $row -notmatch 'fight=67064/') { throw 'Cleared current-journey tuple not confirmed' }
    Shot 'before-practice';$script:requestLogBefore=C72-ReadShared $addonLog
    Write-Host 'ROOT: one hosted Practice Yamagata > Start at boss shrine confirmation. No pipe request is sent.'
    $x=EHP-Wait 'shrine';$script:attempt=$x.hold.attempt
    $fresh=EHP-Log $script:requestLogBefore $x.log $false
    if(-not (Training-QueueWitness $fresh) -or -not (Training-HostedRequestWitness $fresh 'start')){throw 'Exact native Practice start witness absent'}
    Shot 'shrine';EHP-WaitStage 'EnterArena'
    Write-Host 'ROOT: manually walk into entrance66599. No movement is sent.'
    $null=EHP-Wait 'held';Shot 'held-before-menu'
    Write-Host 'ROOT: open hosted Practice menu without selecting Summon. Confirm visible menu with boss still held.'
    EHP-WaitStage 'HoldMenuReviewed';$script:result.hold_menu_reviewed=$true
    $x=EHP-Sample;EHP-Held $x.hold $script:attempt ([EHPClock]::GetTickCount64()) $x.status -Preserved
    $script:reviewBoxAt=$x.hold.boxAt
    Write-Host 'ROOT: if paused, close/unpause the menu while staying at the held entrance. Wait for a NEW native box witness; then reopen and select Summon promptly only when invited. Stale paused Summon is not supported.'
    $x=EHP-Wait 'renewed' 30
    $script:summonLogBefore=$x.log;$script:summonAllowed=$true
    Write-Host 'ROOT: select Summon Yamagata exactly once now. No factory/flags/input commands are sent.'
    $null=EHP-Wait 'released';$null=EHP-Wait 'boss';Shot 'boss-after-summon'
    EHP-WaitStage 'ControlReviewed';$script:result.root_control_confirmed=$true
    EHP-WaitStage 'NativeReturn'
    $script:returnLogBefore=C72-ReadShared $addonLog
    Write-Host 'ROOT: select owned Return from practice once. No exit pipe command is sent.'
    $x=EHP-Wait 'return'
    if(-not (Training-HostedRequestWitness (Training-LogTail $script:returnLogBefore $x.log) 'return')){throw 'Return lacks owned Practice menu witness'}
    $script:result.native_return_confirmed=$true;Shot 'returned'
    $script:result.status='ENTRY_HOLD_REVIEWED_NATIVE_RETURN_OBSERVED'
} catch { $script:result.status='TRIAL_FAILED';$script:result.error="$_";Write-Warning "$_" }
finally { try { Finish-C72 } finally { Done-Guard -KeepFile:(-not $script:cleaned) } }
Write-Host "Result: $OutDir\result.json; root visual evidence governs usable practice acceptance."
if ($script:result.status -ne 'ENTRY_HOLD_REVIEWED_NATIVE_RETURN_OBSERVED' -or -not $script:cleaned) { exit 1 }
