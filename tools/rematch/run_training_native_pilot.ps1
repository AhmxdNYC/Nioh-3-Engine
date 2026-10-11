# Default PLAN is read-only. One guarded launch, one shrine request, manual native gameplay.
[CmdletBinding()]
param([switch]$Run,[switch]$DryRun,[int]$OwnerPid=0,
    [ValidateSet('ShrineRequest','EnterArena','NativeRetry','NativeReturn')][string]$ApproveStage='',
    [string]$Evidence='',[string]$OutDir='',[string]$RequestGateReceipt='',
    [ValidateRange(5,25)][int]$MaxMinutes=20,
    [string]$GameDir='C:\Program Files (x86)\Steam\steamapps\common\Nioh3',
    [string]$SaveRoot='',
    [string]$Kit='D:\nioh3-re\rematch-pilots\C72E-native334140-20261011',
    [string]$CandidateRoot='D:\nioh3-wt\codex-settings-menu-v4')
$ErrorActionPreference='Stop'
if (($Run -and $DryRun) -or ($ApproveStage -and ($Run -or $DryRun))) { throw 'Choose plan, Run, or one stage approval' }
. (Join-Path $PSScriptRoot 'training_pilot_lib.ps1')
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Training-CheckDependencies $repo
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_binary_lib.ps1')
$hashes=@{'Nioh3EngineFix.asi'='dd8b5739062e364dfc708bd1f0f89dfc2b97477d66ae1fbfdfb6f50b0d4f2891';'Nioh3BossRematch.asi'='02d763e76037365e8814820fc1ef9d3d713710435f63099d04d617e05966d4d1'}
$runtimeHashes=@{'Nioh3EngineFix.asi'='6890c0605e3346d915f7f2263337716ea1ddad6c0b4ea36efebb2c4d4aa8e32d';'Nioh3BossRematch.asi'='50484e3b0e39c5621fb7b829122f92380db017fd87d64ed0e2f96a1acb3b9b07'}
$catalog=Join-Path $Kit 'C72E-A-control.tsv'
$plan=[ordered]@{mode='PLAN_ONLY_NO_MUTATION';route='shrine R1CBC';mission='0x1CBC';start=67064;placement=65092;trigger='box66599, manual walk';
    versions='Engine1.7.67/addon1.4.9';runtime_asi_hashes=$runtimeHashes;entry_asi_hashes=$hashes;
    catalog=$catalog;catalog_sha256='7c0c158df9b7c5bc0ce89b6b6fee3ee384c7db94bde892eb38ad78741aad221b';
    launches=1;relaunch=$false;warp=$false;spawn=$false;automatic_movement=$false;automatic_fighting=$false;
    stages=@('Root approves ShrineRequest after Continue','Root confirms shrine control then approves EnterArena','Root confirms visible Yamagata/combat then approves one NativeRetry','Root confirms native death/retry at67064 then approves NativeReturn');
    gate='Requires completed C72E native-entry receipt, root_control_confirmed and native_return_confirmed true and all restoration true. No live readiness implied by plan.';
    recovery='Reuses pinned C72E finalizer/abort path; fresh entry save locks and exact restore, retained recovery record on failure';
    limits='Status/position never proves control; pending gate masks fresh network diagnostics. Native wrappers remain authoritative.'}
if ($ApproveStage) {
    if (-not $OutDir -or $OwnerPid -le 0 -or -not $Evidence.Trim()) { throw 'Approval needs session OutDir, explicit root OwnerPid and evidence' }
    $OutDir=C72-Full $OutDir
    if ((Get-Item -LiteralPath $OutDir).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse session refused' }
    $request=Get-Content -LiteralPath (C72-Child $OutDir 'awaiting.json') -Raw | ConvertFrom-Json
    if ($request.stage -ne $ApproveStage -or $request.owner_pid -ne $OwnerPid) { throw 'Wrong awaited stage/owner' }
    foreach ($pair in @(@($request.owner_pid,$request.owner_start),@($request.runner_pid,$request.runner_start))) {
        $p=Get-Process -Id $pair[0] -ErrorAction Stop
        if ($p.StartTime.ToString('o') -cne $pair[1]) { throw 'Approval process identity expired' }
    }
    $approval=$request | Select-Object *; $approval | Add-Member -NotePropertyName evidence -NotePropertyValue $Evidence
    Training-CheckApproval $request $approval
    $path=C72-Child $OutDir ("approve-$ApproveStage.json")
    $temporary=C72-Child $OutDir ('approval-'+[guid]::NewGuid().ToString('N')+'.tmp')
    $stream=[IO.File]::Open($temporary,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try { $bytes=[Text.Encoding]::UTF8.GetBytes(($approval|ConvertTo-Json));$stream.Write($bytes,0,$bytes.Length) } finally { $stream.Dispose() }
    # Publish the complete receipt atomically; Move refuses an existing approval.
    try { [IO.File]::Move($temporary,$path) } finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
    Write-Host "Recorded explicit root stage confirmation: $ApproveStage";return
}
if (-not $Run) { $plan|ConvertTo-Json -Depth 7;return }
if ($OwnerPid -le 0 -or -not $RequestGateReceipt) { throw 'Run requires explicit root OwnerPid and completed C72E gate receipt' }
if ($env:HG_MIN_HEADROOM_GB -and [double]$env:HG_MIN_HEADROOM_GB -lt 18) { throw 'Training requires at least 18 GB launch headroom' }
if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Game already active; nothing touched' }
$gate=Get-Content -LiteralPath $RequestGateReceipt -Raw|ConvertFrom-Json
$gateManifest=Get-Content -LiteralPath (Join-Path (Split-Path -Parent (C72-Full $RequestGateReceipt)) 'entry-manifest.json') -Raw|ConvertFrom-Json
Training-CheckGate $gate $gateManifest $runtimeHashes
$BackupRoot='';$CaptureRoot='';$StateDir='';$StashRoot='';$OptiSrc='';$global:LockKey=''
. (Join-Path $repo 'tools\playsession\session_lib.ps1')
. (Join-Path $repo 'tools\testmatrix\harness_guard.ps1')
Training-ImportLifecycle (Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1')
$GameDir=C72-Full $GameDir;$SaveRoot=C72-Full $SaveRoot;$Kit=C72-Full $Kit
$binaryPlan=@(C72-BinaryPlan $GameDir $hashes @{
    'Nioh3EngineFix.asi'=@{path=(Join-Path $CandidateRoot 'mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi');hash=$runtimeHashes['Nioh3EngineFix.asi']};
    'Nioh3BossRematch.asi'=@{path=(Join-Path $CandidateRoot 'mod\Nioh3BossRematch\bin\Nioh3BossRematch.asi');hash=$runtimeHashes['Nioh3BossRematch.asi']}})
if ((C72-BinaryHash $catalog) -ne $plan.catalog_sha256) { throw 'Reviewed catalogue changed' }
if (-not (Test-Path -LiteralPath $SaveRoot -PathType Container)) { throw 'Entry save root missing' }
$engineIni=Join-Path $GameDir 'Nioh3EngineFix.ini';$rematchIni=Join-Path $GameDir 'Nioh3BossRematch.ini';$addonLog=Join-Path $GameDir 'Nioh3BossRematch.log'
foreach ($p in @($engineIni,$rematchIni)) { if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "INI missing: $p" } }
if (-not $OutDir) { $OutDir=Join-Path 'D:\nioh3-re\rematch-pilots' ('training-R1CBC-'+(Get-Date -Format yyyyMMdd-HHmmss)+'-'+$PID) }
$OutDir=C72-Full $OutDir
if (Test-Path -LiteralPath $OutDir) { throw 'Session directory must be new' }
foreach ($root in @($GameDir,$SaveRoot,$Kit,$repo,$CandidateRoot)) { if ($OutDir -eq (C72-Full $root) -or (C72-Inside $root $OutDir) -or (C72-Inside $OutDir $root)) { throw 'Session overlaps protected root' } }
$null=New-Item -ItemType Directory -Path $OutDir
$script:HGdir=Join-Path $OutDir 'guard';$entryDir=Join-Path $OutDir 'entry-saves';$iniDir=Join-Path $OutDir 'entry-inis';$events=Join-Path $OutDir 'events.jsonl'
$script:owned=$null;$script:saveHandles=$null;$script:entry=$null;$script:touched=$false;$script:cleaned=$false;$script:binaryRecord=$null
$script:held=New-Object 'System.Collections.Generic.List[string]';$script:watch=[Diagnostics.Stopwatch]::StartNew()
$script:result=[ordered]@{route='R1CBC';status='PREPARING';error=$null;saves_restored=$false;inis_restored=$false;binaries_restored=$false;control_verdict='ROOT_CONFIRMATIONS_REQUIRED';retry_observed=$false;return_observed=$false}
$LockId="pc-training-$PID"
function Training-FreshLog { return Training-LogTail $script:requestLogBefore (C72-ReadShared $addonLog) }
function Training-WaitStage([string]$Stage) {
    $request=[ordered]@{stage=$Stage;nonce=[guid]::NewGuid().ToString('N');owner_pid=$OwnerPid;owner_start=$global:HG.ownerStart.ToString('o');runner_pid=$PID;runner_start=(Get-Process -Id $PID).StartTime.ToString('o')}
    C72-Json (Join-Path $OutDir 'awaiting.json') $request
    Write-Host "WAITING FOR ROOT: $Stage; session $OutDir. No automated movement, fighting or retry."
    $file=Join-Path $OutDir "approve-$Stage.json"
    while (-not (Test-Path -LiteralPath $file)) {
        Assert-Owned
        # Title/initial pipe readiness is not a field-state failure. Root must
        # confirm Continue completed; the first request then requires gate=ok.
        if ($Stage -ne 'ShrineRequest') { $s=Invoke-C72Pipe 'status';Training-CheckStatus $s }
        Start-Sleep -Milliseconds 250
    }
    $a=Get-Content -LiteralPath $file -Raw|ConvertFrom-Json;Training-CheckApproval ([pscustomobject]$request) $a
    C72-Event $events 'root-confirmation' ($a|ConvertTo-Json -Compress) $script:watch.Elapsed.TotalMilliseconds
    Remove-Item -LiteralPath (Join-Path $OutDir 'awaiting.json')
}
function Training-Wait([string]$Kind,[int]$Seconds=180) {
    $until=[DateTime]::UtcNow.AddSeconds($Seconds);$sawLoad=$false
    do {
        $s=Invoke-C72Pipe 'status';Training-CheckStatus $s
        if ((C72-Field $s 'loading') -eq '1') { $sawLoad=$true }
        if ($Kind -eq 'shrine' -and (Training-AtShrine $s $sawLoad)) { return $s }
        if ($Kind -eq 'boss' -and (C72-Field $s 'mission') -eq '0x1CBC' -and (C72-Field $s 'infight') -eq '1' -and $s -match 'placements=\[[^\]]*\b65092(?:\(|\s|\])') { return $s }
        if ($Kind -eq 'return' -and (C72-Field $s 'mission') -ne '0x1CBC' -and (C72-Field $s 'loading') -eq '0' -and (C72-Field $s 'player') -eq '1' -and (C72-Field $s 'state') -eq '0' -and $s -match 'gate=ok(?: |$)') { return $s }
        Start-Sleep -Milliseconds 100
    } while ([DateTime]::UtcNow -lt $until)
    throw "Bounded $Kind observation incomplete; no second request"
}
Init-Guard -Name training_native_pilot -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -MaxCrashes 1 -Cleanup { Finish-C72 }
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
    $empty=Join-Path $OutDir 'empty-arenas.tsv';[IO.File]::WriteAllText($empty,"# no arena overrides`r`n")
    $lines=[IO.File]::ReadAllLines($rematchIni);C72-UniqueIni $lines @('Rematch','Remote','Menu','Debug')
    foreach ($kv in @(@('Rematch','Enable','1'),@('Rematch','FightNow','1'),@('Rematch','FightNowWarp','0'),@('Rematch','FightNowSpawn','0'),@('Rematch','ArenaCapture','0'),@('Rematch','SelfTest',''),@('Rematch','Catalog',$catalog),@('Rematch','Arenas',$empty),@('Remote','Pipe','1'),@('Menu','Overlay','0'),@('Debug','BossLog','1'),@('Debug','RematchLog','0'))) { $lines=Set-IniValue $lines $kv[0] $kv[1] $kv[2] }
    $perf=[IO.File]::ReadAllLines($engineIni);C72-UniqueIni $perf @('General','Debug');$perf=Set-IniValue $perf 'General' 'AutoContinueLoad' '1';$perf=Set-IniValue $perf 'Debug' 'Tour' '0'
    $script:touched=$true;[IO.File]::WriteAllLines($rematchIni,$lines,(New-Object Text.UTF8Encoding($false)));[IO.File]::WriteAllLines($engineIni,$perf,(New-Object Text.UTF8Encoding($false)))
    Check-Asis $runtimeHashes;Assert-Launch;if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Refusing second launch' }
    $launched=[DateTime]::Now;Start-Process -FilePath 'steam://rungameid/3681010' -WindowStyle Hidden
    $until=[DateTime]::UtcNow.AddSeconds(120)
    do { $games=@(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue);if ($games.Count -gt 1) { throw 'Multiple games appeared' };if ($games.Count -eq 1 -and (Get-C72LaunchIdentity $games[0] $launched (Join-Path $GameDir 'Nioh3.exe')) -eq 'owned') { $script:owned=$games[0];break };Start-Sleep -Milliseconds 250 } while ([DateTime]::UtcNow -lt $until)
    if (-not $script:owned) { throw 'No owned game appeared; no relaunch' }
    $global:HG.games=@(@{pid=$script:owned.Id;name='Nioh3';start=$script:owned.StartTime.ToString('o');closed=$false;counted=$false});HG-Save
    Write-Host 'Root manually selects Continue once. Waiting for explicit ShrineRequest confirmation.'
    Training-WaitStage 'ShrineRequest'
    $readyTracker=@{since=$null;last=$null};$ready=$false;$until=[DateTime]::UtcNow.AddSeconds(180)
    do {
        $readable=$true
        try { $s=Invoke-C72Pipe 'status' } catch { $readable=$false;$s='';Assert-Owned }
        if ($readable) { Training-CheckStatus $s }
        $ready=C72-ReadySettled $readyTracker $script:watch.ElapsedMilliseconds $s $readable
        if (-not $ready) { Start-Sleep -Milliseconds 250 }
    } while (-not $ready -and [DateTime]::UtcNow -lt $until)
    if (-not $ready) { throw 'No continuously ready offline field for 12s within 180s after root confirmation' }
    $beforeGen=Training-BeginScan
    $cleared=$false;$until=[DateTime]::UtcNow.AddSeconds(15)
    do { $row=Invoke-C72Pipe 'row R1CBC';$gen=C72-Field $row 'gen';$cleared=(C72-Field $row 'status') -eq '2' -and $gen -match '^\d+$' -and [uint64]$gen -gt [uint64]$beforeGen;if (-not $cleared) { Start-Sleep -Milliseconds 200 } } while (-not $cleared -and [DateTime]::UtcNow -lt $until)
    if (-not $cleared -or $row -notmatch 'mission=0x1CBC\b' -or $row -notmatch 'placement=65092\b' -or $row -notmatch 'fight=67064/') { throw 'Cleared current-journey tuple not confirmed' }
    Shot 'before-shrine';$script:requestLogBefore=C72-ReadShared $addonLog
    $reply=Invoke-C72Pipe 'shrine R1CBC';if ($reply -notmatch '^queued') { throw "Shrine submission refused: $reply" }
    $null=Training-Wait 'shrine';if (-not (Training-QueueWitness (Training-FreshLog))) { throw 'Exact mission/start/mode queue witness absent' };Shot 'shrine-observed'
    Training-WaitStage 'EnterArena' # root confirms visible shrine/control before manual walking
    Write-Host 'Root/player may now walk into native box66599. The runner sends no movement.'
    $null=Training-Wait 'boss';if ((Training-FreshLog) -notmatch 'arena: BossBattleMode start[^\r\n]*mission 0x1CBC[^\r\n]*65092') { throw 'Fresh Yamagata battle event absent' };Shot 'native-boss-observed'
    Training-WaitStage 'NativeRetry' # explicit permission for ONE manual native death/retry
    Write-Host 'Root/player performs one native death/retry. No retry/input command is sent.'
    $null=Training-Wait 'shrine';$script:result.retry_observed=$true;Shot 'native-retry-shrine'
    Training-WaitStage 'NativeReturn' # root confirms death/retry and controllable67064 landing
    $s=Invoke-C72Pipe 'status';Training-CheckStatus $s
    if (-not (Training-AtShrine $s $true)) { throw 'Retry shrine state changed before native return' }
    $reply=Invoke-C72Pipe 'exit';if ($reply -notmatch '^queued') { throw "Native return submission refused: $reply" }
    $null=Training-Wait 'return';$script:result.return_observed=$true;Shot 'returned'
    $script:result.status='MANUAL_NATIVE_ROUTE_COMPLETED_REVIEW_REQUIRED'
} catch { $script:result.status='TRIAL_FAILED';$script:result.error="$_";Write-Warning "$_" }
finally { try { Finish-C72 } finally { Done-Guard -KeepFile:(-not $script:cleaned) } }
Write-Host "Result: $OutDir\result.json; root visual evidence governs usable practice acceptance."
if ($script:result.status -ne 'MANUAL_NATIVE_ROUTE_COMPLETED_REVIEW_REQUIRED' -or -not $script:cleaned) { exit 1 }
