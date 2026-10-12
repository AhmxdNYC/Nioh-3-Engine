# PLAN/DryRun is read-only. Exact frozen admission, one owned game only.
[CmdletBinding()]
param([switch]$Run,[switch]$DryRun,[int]$OwnerPid=0,
 [ValidateSet('FieldReady','FightReady','ControlReviewed','NativeReturn')][string]$ApproveStage='',
 [switch]$VisibleArena,[switch]$VisibleBoss,[switch]$ControlResponse,
 [string]$Evidence='',[string]$OutDir='',[string]$RequestGateReceipt='',[string]$AdmissionReceipt='',
 [ValidateRange(5,25)][int]$MaxMinutes=20,
 [string]$GameDir='C:\Program Files (x86)\Steam\steamapps\common\Nioh3',[string]$SaveRoot='',
 [string]$Kit='D:\nioh3-re\rematch-pilots\C72E-native334140-20261011',
 [string]$CandidateRoot='D:\nioh3-wt\codex-settings-menu-v4',[string]$LifecycleRepo='')
$ErrorActionPreference='Stop'
if (($Run -and $DryRun) -or ($ApproveStage -and ($Run -or $DryRun))) { throw 'Choose plan, Run or approval' }
. (Join-Path $PSScriptRoot 'fight_cover_pilot_lib.ps1')
$expected=Get-FCPExpected
$plan=[ordered]@{mode='PLAN_ONLY_NO_MUTATION';route='Hosted Fight now R1CBC';mission='0x1CBC';start=67064;placement=65092;
 admission='EXACT_PRIVATE_PILOT_PINS_REQUIRE_OWNER_BOUND_RECEIPT';expected=$expected;launches=1;relaunch=$false;retry=$false;
 arena_override_bytes='# no arena overrides\r\n (UTF8 no BOM)';arena_override_sha256='d05722d1edae92d39bdb8a1a1fe6bac59fc92550c38f8593aa04d2087d50b171';arena_source='PINNED_CATALOGUE_R1CBC_337691_10244_206714_YAW_MINUS89.2_BOX66599';
 catalog_sha256='7c0c158df9b7c5bc0ce89b6b6fee3ee384c7db94bde892eb38ad78741aad221b';
 stages=@('Root Continue;12s ready settle;fresh clear scan','Root navigates to final confirmation;FightReady arms bounded observation','Root hosted Fight now;observe drawn ACK then native request','Exact same-token/attempt ready;root visual/control confirmation','One native exit;exact original entry;owned restoration');
 recovery='Pinned existing C72 finalizer, current entry backups, retained guard on any failure';limits='No prior PASS transferred; telemetry alone is not seamless/control acceptance'}
if (-not $Run -and -not $ApproveStage) { $plan|ConvertTo-Json -Depth 7;return }
if (-not $LifecycleRepo) { $LifecycleRepo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot) }
$repo=[IO.Path]::GetFullPath($LifecycleRepo);$lifecycleTools=Join-Path $repo 'tools/rematch'
FCP-CheckDependencies $repo
. (Join-Path $lifecycleTools 'training_pilot_lib.ps1')
Training-CheckDependencies $repo
. (Join-Path $lifecycleTools 'c72e_pilot_lib.ps1')
. (Join-Path $lifecycleTools 'c72e_binary_lib.ps1')
if ($ApproveStage) {
    if (-not $OutDir -or $OwnerPid -le 0 -or -not $Evidence.Trim()) { throw 'Approval needs session OutDir, explicit root OwnerPid and evidence' }
    $OutDir=C72-Full $OutDir
    if ((Get-Item -LiteralPath $OutDir).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse session refused' }
    $request=Get-Content -LiteralPath (C72-Child $OutDir 'awaiting.json') -Raw | ConvertFrom-Json
    if ($request.stage -ne $ApproveStage -or $request.owner_pid -ne $OwnerPid) { throw 'Wrong awaited stage/owner' }
    foreach ($pair in @(@($request.owner_pid,$request.owner_start),@($request.runner_pid,$request.runner_start),@($request.game_pid,$request.game_start))) {
        $p=Get-Process -Id $pair[0] -ErrorAction Stop
        if ($p.StartTime.ToString('o') -cne $pair[1]) { throw 'Approval process identity expired' }
    }
    $approval=$request | Select-Object *; $approval | Add-Member -NotePropertyName evidence -NotePropertyValue $Evidence
    $approval | Add-Member visible_arena ([bool]$VisibleArena);$approval | Add-Member visible_boss ([bool]$VisibleBoss);$approval | Add-Member control_response ([bool]$ControlResponse)
    FCP-CheckApproval $request $approval
    $path=C72-Child $OutDir ("approve-$ApproveStage.json")
    $temporary=C72-Child $OutDir ('approval-'+[guid]::NewGuid().ToString('N')+'.tmp')
    $stream=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $bytes=[Text.Encoding]::UTF8.GetBytes(($approval|ConvertTo-Json));$stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
    # Publish the complete receipt atomically; Move refuses an existing approval.
    try { [IO.File]::Move($temporary,$path) } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
    Write-Host "Recorded explicit root stage confirmation: $ApproveStage";return
}

if ($OwnerPid -le 0 -or -not $RequestGateReceipt -or -not $AdmissionReceipt) { throw 'Explicit owner, old native gate and NEW admission receipt required' }
$owner=Get-Process -Id $OwnerPid -ErrorAction Stop
$runtimeHashes=@{'Nioh3EngineFix.asi'=$expected.engine_sha256;'Nioh3BossRematch.asi'=$expected.addon_sha256}
$hashes=@{'Nioh3EngineFix.asi'='dd8b5739062e364dfc708bd1f0f89dfc2b97477d66ae1fbfdfb6f50b0d4f2891';'Nioh3BossRematch.asi'='02d763e76037365e8814820fc1ef9d3d713710435f63099d04d617e05966d4d1'}
$gateHashes=@{'Nioh3EngineFix.asi'='6890c0605e3346d915f7f2263337716ea1ddad6c0b4ea36efebb2c4d4aa8e32d';'Nioh3BossRematch.asi'='50484e3b0e39c5621fb7b829122f92380db017fd87d64ed0e2f96a1acb3b9b07'}
$modTree=(& git -C $CandidateRoot rev-parse HEAD:mod).Trim();if ($LASTEXITCODE -ne 0) { throw 'Candidate mod tree unreadable' }
& git -C $CandidateRoot diff --quiet HEAD -- mod;if ($LASTEXITCODE -ne 0) { throw 'Candidate source changed' }
$hostedAdmission=FCP-Admission $expected (Get-Content -LiteralPath $AdmissionReceipt -Raw|ConvertFrom-Json) (C72-BinaryHash $AdmissionReceipt) $modTree $runtimeHashes (C72-BinaryHash $RequestGateReceipt) $OwnerPid $owner.StartTime.ToString('o')
if ($env:HG_MIN_HEADROOM_GB -and [double]$env:HG_MIN_HEADROOM_GB -lt 18) { throw 'Requires18GB launch headroom' }
if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Game already active; nothing touched' }
$gate=Get-Content -LiteralPath $RequestGateReceipt -Raw|ConvertFrom-Json
$gateManifest=Get-Content -LiteralPath (Join-Path (Split-Path -Parent (C72-Full $RequestGateReceipt)) 'entry-manifest.json') -Raw|ConvertFrom-Json
Training-CheckGate $gate $gateManifest $gateHashes
$catalog=Join-Path $Kit 'C72E-A-control.tsv'
$BackupRoot='';$CaptureRoot='';$StateDir='';$StashRoot='';$OptiSrc='';$global:LockKey=''
. (Join-Path $repo 'tools\playsession\session_lib.ps1')
. (Join-Path $repo 'tools\testmatrix\harness_guard.ps1')
Training-ImportLifecycle (Join-Path $lifecycleTools 'run_c72e_native_pilot.ps1')
$GameDir=C72-Full $GameDir;$SaveRoot=C72-Full $SaveRoot;$Kit=C72-Full $Kit
$binaryPlan=@(C72-BinaryPlan $GameDir $hashes @{
    'Nioh3EngineFix.asi'=@{path=(Training-CandidatePath $CandidateRoot 'Nioh3EngineFix' 'Tree');hash=$runtimeHashes['Nioh3EngineFix.asi']};
    'Nioh3BossRematch.asi'=@{path=(Training-CandidatePath $CandidateRoot 'Nioh3BossRematch' 'Tree');hash=$runtimeHashes['Nioh3BossRematch.asi']}})
if ((C72-BinaryHash $catalog) -ne $plan.catalog_sha256) { throw 'Reviewed catalogue changed' }
if (-not (Test-Path -LiteralPath $SaveRoot -PathType Container)) { throw 'Entry save root missing' }
$engineIni=Join-Path $GameDir 'Nioh3EngineFix.ini';$rematchIni=Join-Path $GameDir 'Nioh3BossRematch.ini';$addonLog=Join-Path $GameDir 'Nioh3BossRematch.log'
foreach ($p in @($engineIni,$rematchIni)) { if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "INI missing: $p" } }
if (-not $OutDir) { $OutDir=Join-Path 'D:\nioh3-re\rematch-pilots' ('fight-cover-R1CBC-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+$PID) }
$OutDir=C72-Full $OutDir
if (Test-Path -LiteralPath $OutDir) { throw 'Session directory must be new' }
foreach ($root in @($GameDir,$SaveRoot,$Kit,$repo,$CandidateRoot)) { if ($OutDir -eq (C72-Full $root) -or (C72-Inside $root $OutDir) -or (C72-Inside $OutDir $root)) { throw 'Session overlaps protected root' } }
$null=New-Item -ItemType Directory -Path $OutDir
if ($true) { $hostedAdmission.arena_override_sha256=$plan.arena_override_sha256;$hostedAdmission.arena_source=$plan.arena_source;$hostedAdmission.runner_pid=$PID;$hostedAdmission.owner_pid=$OwnerPid;$hostedAdmission.session=$OutDir;C72-Json (Join-Path $OutDir 'cover-admission.json') $hostedAdmission }
$script:HGdir=Join-Path $OutDir 'guard';$entryDir=Join-Path $OutDir 'entry-saves';$iniDir=Join-Path $OutDir 'entry-inis';$events=Join-Path $OutDir 'events.jsonl'
$script:owned=$null;$script:saveHandles=$null;$script:entry=$null;$script:touched=$false;$script:cleaned=$false;$script:binaryRecord=$null
$script:held=New-Object 'System.Collections.Generic.List[string]';$script:watch=[Diagnostics.Stopwatch]::StartNew()
$script:result=[ordered]@{route='R1CBC';hosted_fight_cover=$true;feature_scope='SEAMLESS_FIGHT_COVER_UNTESTED';ui_verdict='NEW_FEATURE_UNTESTED_ROOT_EVIDENCE_REQUIRED';status='PREPARING';error=$null;saves_restored=$false;inis_restored=$false;binaries_restored=$false;control_verdict='ROOT_CONFIRMATIONS_REQUIRED';root_control_confirmed=$false;native_return_confirmed=$false}
$LockId="pc-fight-cover-$PID"
function Training-FreshLog { return Training-LogTail $script:requestLogBefore (C72-ReadShared $addonLog) }
function Training-WaitStage([string]$Stage) {
    $request=[ordered]@{stage=$Stage;nonce=[guid]::NewGuid().ToString('N');owner_pid=$OwnerPid;owner_start=$global:HG.ownerStart.ToString('o');runner_pid=$PID;runner_start=(Get-Process -Id $PID).StartTime.ToString('o');game_pid=$script:owned.Id;game_start=$script:owned.StartTime.ToString('o')}
    C72-Json (Join-Path $OutDir 'awaiting.json') $request
    Write-Host "WAITING FOR ROOT: $Stage; session $OutDir. No automated movement, fighting or retry."
    $file=Join-Path $OutDir "approve-$Stage.json"
    while (-not (Test-Path -LiteralPath $file)) {
        Assert-Owned
        # Title/initial pipe readiness is not a field-state failure. Root must
        # confirm Continue completed; the first request then requires gate=ok.
        if ($Stage -ne 'FieldReady') { $s=Invoke-C72Pipe 'status';FCP-CheckStatus $s }
        if ($Stage -eq 'FightReady') { FCP-CheckPreSubmit $baseline (Invoke-C72Pipe 'cover') }
        Start-Sleep -Milliseconds 250
    }
    $a=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json;FCP-CheckApproval ([pscustomobject]$request) $a;if ($a.game_pid -ne $script:owned.Id -or $a.game_start -cne $script:owned.StartTime.ToString('o')) { throw 'Approval game identity mismatch' }
    Assert-Owned
    if ($Stage -eq 'FightReady') { FCP-CheckPreSubmit $baseline (Invoke-C72Pipe 'cover') }
    C72-Event $events 'root-confirmation' ($a|ConvertTo-Json -Compress) $script:watch.Elapsed.TotalMilliseconds
    Remove-Item -LiteralPath (Join-Path $OutDir 'awaiting.json')
}
function Training-Wait([string]$Kind,[int]$Seconds=180) {
    $until=[DateTime]::UtcNow.AddSeconds($Seconds);$sawLoad=$false
    do {
        $s=Invoke-C72Pipe 'status';FCP-CheckStatus $s
        $refused=C72-NewRefusal $script:requestLogBefore (C72-ReadShared $addonLog)
        if ($refused) { throw "Native request refused: $refused; no retry" }
        if ((C72-Field $s 'loading') -eq '1') { $sawLoad=$true }
        if ($Kind -eq 'return' -and (Training-ReturnEvidence $script:returnLogBefore (C72-ReadShared $addonLog) $s $script:result.entry_tuple)) { return $s }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $until)
    throw "Bounded $Kind observation incomplete; no second request"
}
Init-Guard -Name fight_cover_native_pilot -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -MaxCrashes 1 -Cleanup { Finish-C72 }
try {
    $global:LockKey='nioh3-pc';$r=Invoke-GameLock 'acquire' 1800;if ($r.code) { throw 'PC lease unavailable' };$script:held.Add('nioh3-pc')
    Assert-Owner;if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Game appeared before preparation' }
    Add-Type -AssemblyName System.Drawing,System.Windows.Forms
    $script:entry=C72-Manifest $SaveRoot;if (-not $script:entry.Count) { throw 'Empty save tree refused' }
    C72-Backup $SaveRoot $entryDir $script:entry;$script:saveHandles=C72-LockSaves $SaveRoot $script:entry
    $null=New-Item -ItemType Directory -Path $iniDir
    foreach ($n in @('Nioh3EngineFix.ini','Nioh3BossRematch.ini')) { Copy-Item -LiteralPath (Join-Path $GameDir $n) -Destination (Join-Path $iniDir $n) }
    C72-Json (Join-Path $OutDir 'entry-manifest.json') @{plan=$plan;saves=$script:entry;gate_receipt_sha256=(C72-BinaryHash $RequestGateReceipt);binary_plan=$binaryPlan}
    $script:binaryRecord=C72-BinaryPrepare $binaryPlan (Join-Path $OutDir 'entry-binaries');C72-BinaryStage $script:binaryRecord
    $empty=Join-Path $OutDir 'empty-arenas.tsv';[IO.File]::WriteAllText($empty,"# no arena overrides`r`n",(New-Object Text.UTF8Encoding($false)));if ((C72-BinaryHash $empty) -cne $plan.arena_override_sha256) { throw 'Header-only arena override bytes changed' }
    $lines=[IO.File]::ReadAllLines($rematchIni);C72-UniqueIni $lines @('Rematch','Remote','Menu','Debug')
    foreach ($kv in @(@('Rematch','Enable','1'),@('Rematch','FightNow','1'),@('Rematch','FightNowWarp','1'),@('Rematch','FightNowSpawn','1'),@('Rematch','ArenaCapture','0'),@('Rematch','SelfTest',''),@('Rematch','Catalog',$catalog),@('Rematch','Arenas',$empty),@('Remote','Pipe','1'),@('Menu','Overlay','1'),@('Debug','BossLog','1'),@('Debug','RematchLog','0'))) { $lines=Set-IniValue $lines $kv[0] $kv[1] $kv[2] }
    $perf=[IO.File]::ReadAllLines($engineIni);C72-UniqueIni $perf @('General','Debug');$perf=Set-IniValue $perf 'General' 'AutoContinueLoad' '1';$perf=Set-IniValue $perf 'Debug' 'Tour' '0'
    $script:touched=$true;[IO.File]::WriteAllLines($rematchIni,$lines,(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllLines($engineIni,$perf,(New-Object Text.UTF8Encoding($false)))
    Check-Asis $runtimeHashes;Assert-Launch;if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Refusing second launch' }
    $launched=[DateTime]::Now;Start-Process -FilePath 'steam://rungameid/3681010' -WindowStyle Hidden
    $until=[DateTime]::UtcNow.AddSeconds(120)
    do { $games=@(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue);if ($games.Count -gt 1) { throw 'Multiple games appeared' };if ($games.Count -eq 1 -and (Get-C72LaunchIdentity $games[0] $launched (Join-Path $GameDir 'Nioh3.exe')) -eq 'owned') { $script:owned=$games[0];break };Start-Sleep -Milliseconds 250 } while ([DateTime]::UtcNow -lt $until)
    if (-not $script:owned) { throw 'No owned game appeared; no relaunch' }
    $global:HG.games=@(@{pid=$script:owned.Id;name='Nioh3';start=$script:owned.StartTime.ToString('o');closed=$false;counted=$false});HG-Save
    Write-Host 'Root manually selects Continue once. Waiting for explicit FieldReady confirmation.'
    Training-WaitStage 'FieldReady'
    $readyTracker=@{since=$null;last=$null};$ready=$false;$until=[DateTime]::UtcNow.AddSeconds(180)
    do {
        $readable=$true
        try { $s=Invoke-C72Pipe 'status' } catch { $readable=$false;$s='';Assert-Owned }
        if ($readable) { FCP-CheckStatus $s }
        $ready=C72-EntrySettled $readyTracker $script:watch.ElapsedMilliseconds $s $readable
        if (-not $ready) { Start-Sleep -Milliseconds 250 }
    } while (-not $ready -and [DateTime]::UtcNow -lt $until)
    if (-not $ready) { throw 'No continuously ready offline field for 12s within 180s after root confirmation' }
    $script:result.entry_tuple=@{mission=(C72-Field $s 'mission');stage=(C72-Field $s 'stage');reply=$s}
    $beforeGen=Training-BeginScan
    $cleared=$false;$until=[DateTime]::UtcNow.AddSeconds(15)
    do { $row=Invoke-C72Pipe 'row R1CBC';$gen=C72-Field $row 'gen';$cleared=(C72-Field $row 'status') -eq '2' -and $gen -match '^\d+$' -and [uint64]$gen -gt [uint64]$beforeGen;if (-not $cleared) { Start-Sleep -Milliseconds 200 } } while (-not $cleared -and [DateTime]::UtcNow -lt $until)
    if (-not $cleared -or $row -notmatch 'mission=0x1CBC\b' -or $row -notmatch 'placement=65092\b' -or $row -notmatch 'fight=67064/') { throw 'Cleared current-journey tuple not confirmed' }
    Shot 'before-fight-cover';$baseline=FCP-Parse (Invoke-C72Pipe 'cover')
    if ($baseline.phase -notin @(0,4,5,6)) { throw 'Existing cover transaction active' }
    $script:requestLogBefore=C72-ReadShared $addonLog
    Write-Host 'Root: navigate to the final Yamagata Fight now confirmation without confirming. Record FightReady, then confirm once after observation is armed.'
    Training-WaitStage 'FightReady'
    Write-Host 'Root: open hosted Boss Rematch > Yamagata > Fight now and confirm once. Observing now; no fight pipe command.'
    $coverReady=$false;$until=[DateTime]::UtcNow.AddSeconds(180)
    do {
        $cover=Invoke-C72Pipe 'cover';$s=Invoke-C72Pipe 'status';FCP-CheckStatus $s
        C72-Event $events 'cover' $cover $script:watch.Elapsed.TotalMilliseconds
        $log=C72-ReadShared $addonLog
        $refused=C72-NewRefusal $script:requestLogBefore $log;if ($refused) { throw "Native request refused: $refused; no retry" }
        $coverReady=FCP-Evidence $baseline.token $script:requestLogBefore $log $cover
        if (-not $coverReady) { Start-Sleep -Milliseconds 100 }
    } while (-not $coverReady -and [DateTime]::UtcNow -lt $until)
    if (-not $coverReady) { throw 'Bounded cover readiness incomplete; no retry' }
    if ((C72-Field $s 'mission') -ne '0x1CBC' -or (C72-Field $s 'loading') -ne '0' -or (C72-Field $s 'player') -ne '1' -or (C72-Field $s 'infight') -ne '1') { throw 'Ready cover lacks current mission/player/boss state' }
    $script:result.cover_ready=$cover;Shot 'cover-ready-boss'
    Training-WaitStage 'ControlReviewed'
    if (-not (FCP-Evidence $baseline.token $script:requestLogBefore (C72-ReadShared $addonLog) (Invoke-C72Pipe 'cover'))) { throw 'Cover readiness lost before root confirmation' }
    # Phase4 retains a historical native sample. Recheck current basic state independently; root confirms actual control.
    $current=Invoke-C72Pipe 'status';FCP-CheckStatus $current
    if ((C72-Field $current 'mission') -ne '0x1CBC' -or (C72-Field $current 'loading') -ne '0' -or (C72-Field $current 'player') -ne '1' -or (C72-Field $current 'infight') -ne '1') { throw 'Current field state changed during root review' }
    $script:result.root_review_current_status=$current;$script:result.cover_sample_is_historical=$true
    $script:result.root_control_confirmed=$true
    Training-WaitStage 'NativeReturn'
    $rt=@{since=$null;last=$null};$rs=$false;$ru=[DateTime]::UtcNow.AddSeconds(30)
    do { $s=Invoke-C72Pipe 'status';FCP-CheckStatus $s;$rs=FCP-ReturnSettled $rt $script:watch.ElapsedMilliseconds $s;if (-not $rs) { Start-Sleep -Milliseconds 100 } } while (-not $rs -and [DateTime]::UtcNow -lt $ru)
    if (-not $rs) { throw 'Native return telemetry proxy did not settle; no submission' }
    # Native authoritative ExitGate remains final admission; one submission only.
    $script:returnLogBefore=C72-ReadShared $addonLog
    $reply=Invoke-C72Pipe 'exit';if ($reply -notmatch '^queued') { throw "Native return submission refused: $reply" }
    $null=Training-Wait 'return';$script:result.native_return_confirmed=$true;Shot 'returned'
    $script:result.status='FIGHT_COVER_REVIEWED_RETURN_OBSERVED'
} catch { $script:result.status='TRIAL_FAILED';$script:result.error="$_";Write-Warning "$_" }
finally { try { Finish-C72 } finally { Done-Guard -KeepFile:(-not $script:cleaned) } }
Write-Host "Result: $OutDir/result.json; frozen new feature requires root evidence."
if (-not (FCP-Complete $script:result) -or -not $script:cleaned) { exit 1 }
