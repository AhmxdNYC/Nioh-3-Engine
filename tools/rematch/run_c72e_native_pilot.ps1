# Default: read-only plan. -Run is a bounded, ONE-launch root-coordinator experiment.
# Optional privately built ASIs are staged transactionally; no build, relaunch or forced kill.
[CmdletBinding()]
param(
    [ValidateSet('A','B')][string]$Arm = 'B', [switch]$Run, [switch]$DryRun,
    [int]$OwnerPid = 0, [ValidateRange(2,12)][int]$MaxMinutes = 10,
    [ValidateRange(30,300)][int]$TrialSeconds = 180,
    [ValidateRange(15,180)][int]$ReviewSeconds = 90,
    [string]$Kit = 'D:\nioh3-re\rematch-pilots\C72E-native334140-20261011',
    [string]$GameDir = 'C:\Program Files (x86)\Steam\steamapps\common\Nioh3',
    [string]$SaveRoot = '', [string]$OutDir = '',
    [string]$EngineCandidate = '', [string]$EngineCandidateSha256 = '',
    [string]$RematchCandidate = '', [string]$RematchCandidateSha256 = ''
)
$ErrorActionPreference = 'Stop'
if ($Run -and $DryRun) { throw '-Run and -DryRun are mutually exclusive' }
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_binary_lib.ps1')
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$BackupRoot=''; $CaptureRoot=''; $StateDir=''; $StashRoot=''; $OptiSrc=''; $global:LockKey=''
. (Join-Path $repo 'tools\playsession\session_lib.ps1')
. (Join-Path $repo 'tools\testmatrix\harness_guard.ps1')
$GameDir = C72-Full $GameDir; $SaveRoot = C72-Full $SaveRoot; $Kit = C72-Full $Kit
$catalog = Join-Path $Kit $(if ($Arm -eq 'A') { 'C72E-A-control.tsv' } else { 'C72E-B-native334140.tsv' })
$start = if ($Arm -eq 'A') { 335814 } else { 334140 }
$target = if ($Arm -eq 'A') { @(209710.140625,5880.8798828125,139370.734375) } else { @(207628.53125,6027.47998046875,142677.921875) }
$engineAsi = Join-Path $GameDir 'Nioh3EngineFix.asi'; $rematchAsi = Join-Path $GameDir 'Nioh3BossRematch.asi'
$engineIni = Join-Path $GameDir 'Nioh3EngineFix.ini'; $rematchIni = Join-Path $GameDir 'Nioh3BossRematch.ini'
$addonLog = Join-Path $GameDir 'Nioh3BossRematch.log'
$hashes = @{ 'Nioh3EngineFix.asi' = 'dd8b5739062e364dfc708bd1f0f89dfc2b97477d66ae1fbfdfb6f50b0d4f2891'
    'Nioh3BossRematch.asi' = '02d763e76037365e8814820fc1ef9d3d713710435f63099d04d617e05966d4d1' }
function Check-Asis($Expected = $hashes) {
    foreach ($n in $Expected.Keys) {
        if ((C72-BinaryHash (Join-Path $GameDir $n)) -ne $Expected[$n]) { throw "Installed $n differs from its exact expected hash" }
    }
}
$binaryPlan=@(C72-BinaryPlan $GameDir $hashes @{
    'Nioh3EngineFix.asi'=@{path=$EngineCandidate;hash=$EngineCandidateSha256}
    'Nioh3BossRematch.asi'=@{path=$RematchCandidate;hash=$RematchCandidateSha256} })
$runtimeHashes=@{}; foreach ($p in $binaryPlan) { $runtimeHashes[$p.name]=$p.candidate }
$manifest = Get-Content -LiteralPath (Join-Path $Kit 'manifest.json') -Raw | ConvertFrom-Json
foreach ($p in $manifest.artifacts.PSObject.Properties) {
    if ((Get-FileHash -LiteralPath (C72-Child $Kit $p.Name) -Algorithm SHA256).Hash.ToLowerInvariant() -ne $p.Value) { throw "Pilot kit artifact changed: $($p.Name)" }
}
if ($manifest.candidate_sha256 -ne '4025b610a6babf9e7d3b18f3b3af2aab69c1edc5f2a0baf4519d30ce71895c24') { throw 'Unexpected candidate manifest' }
$selectedHash = if ($Arm -eq 'A') { '7c0c158df9b7c5bc0ce89b6b6fee3ee384c7db94bde892eb38ad78741aad221b' } else { '4025b610a6babf9e7d3b18f3b3af2aab69c1edc5f2a0baf4519d30ce71895c24' }
if ((Get-FileHash -LiteralPath $catalog -Algorithm SHA256).Hash.ToLowerInvariant() -ne $selectedHash) { throw 'Selected catalogue differs from the reviewed kit' }
Check-Asis
foreach ($p in $engineIni,$rematchIni) { if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "Required original INI missing: $p" } }
$engineBytes = [IO.File]::ReadAllBytes($binaryPlan[0].source)
if ([Text.Encoding]::Unicode.GetString($engineBytes) -notmatch 'AutoContinueLoad' -and
    [Text.Encoding]::Unicode.GetString($engineBytes,1,$engineBytes.Length-1) -notmatch 'AutoContinueLoad') { throw 'Installed binary lacks the verified AutoContinueLoad key' }
$plan = [ordered]@{ mode = 'PLAN_ONLY_NO_MUTATION'; arm = $Arm; start = $start; catalog = $catalog
    installed_asi_hashes = $hashes; runtime_asi_hashes=$runtimeHashes; binary_plan=$binaryPlan
    auto_continue = 'General.AutoContinueLoad=1 (post-load tips only; candidate behavior requires separate review)'
    title = 'Root manually selects Continue once; no automated title input'
    saves = 'Fresh CURRENT entry SHA256 backup; read-only sharing locks deny current file overwrite/delete; exact post-exit restoration includes new files'
    locks = @('nioh3-pc'); launches = 1; relaunch = $false; kill = $false
    telemetry = 'Earliest loading=0/player proxy, native start and expected boss event recorded separately; pending gate masks pause/read validity. Control-enabled is NOT exposed.'
    root_review = "Explicit nonce/process-bound arena, boss and control-response review required within $ReviewSeconds seconds before one native return; first-controllable timing remains unknown."
    verdict = 'First controllable frame requires root visual confirmation; F PASS is never instant proof'
    screenshots = 'Host screenshots before request, first observed player and BossBattleMode; root visual review required. No focus or input commands.' }
if (-not $Run) { $plan | ConvertTo-Json -Depth 5; return }
if ($OwnerPid -le 0) { throw 'Explicit -OwnerPid of the live root coordinator is required for -Run' }
if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Game already running; nothing touched' }
if (-not (Test-Path -LiteralPath $SaveRoot -PathType Container)) { throw 'Current save root does not exist' }
if (-not $OutDir) { $OutDir = Join-Path 'D:\nioh3-re\rematch-pilots' ("live-C72E-$Arm-" + (Get-Date -Format yyyyMMdd-HHmmss) + "-$PID") }
$OutDir = C72-Full $OutDir
if (Test-Path -LiteralPath $OutDir) { throw 'Live output directory must be NEW' }
foreach ($r in $GameDir,$SaveRoot,$Kit,$repo) {
    if ($OutDir -eq $r -or (C72-Inside $r $OutDir) -or (C72-Inside $OutDir $r)) { throw "Output overlaps a protected root: $r" }
}
$null = New-Item -ItemType Directory -Path $OutDir
$script:HGdir = Join-Path $OutDir 'guard'
$script:owned = $null; $script:saveHandles = $null; $script:entry = $null; $script:touched = $false; $script:cleaned = $false
$script:binaryRecord = $null
$script:held = New-Object 'System.Collections.Generic.List[string]'
$script:result = [ordered]@{ arm=$Arm; start=$start; status='PREPARING'; instant_verdict='UNCONFIRMED_CONTROL_NOT_EXPOSED'
    started_utc=[DateTime]::UtcNow.ToString('o'); first_observed=$null; native_start=$null; battle_observed=$false; battle_evidence=$null; native_return_confirmed=$false; return_disposition='NOT_SENT'; root_control_confirmed=$false; control_review=$null; saves_restored=$false; inis_restored=$false; binaries_restored=$false; request_disposition='NOT_SENT'; error=$null }
$entryDir = Join-Path $OutDir 'entry-saves'; $iniDir = Join-Path $OutDir 'entry-inis'
$events = Join-Path $OutDir 'events.jsonl'; $script:watch = [Diagnostics.Stopwatch]::StartNew()
$LockId = "pc-c72e-$PID"
function Assert-Owned {
    Assert-Owner
    if (-not $script:owned) { throw 'No owned game process' }
    $p = Get-Process -Id $script:owned.Id -ErrorAction SilentlyContinue
    if (-not $p -or $p.StartTime -ne $script:owned.StartTime) { throw 'Owned game exited or PID was reused; no relaunch' }
    if (@(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue).Count -ne 1) { throw 'Unexpected additional game process' }
}
function Invoke-C72Pipe([string]$Command) {
    Assert-Owned
    $pipe = New-Object IO.Pipes.NamedPipeClientStream('.', 'nioh3bossrematch', [IO.Pipes.PipeDirection]::InOut)
    try {
        $pipe.Connect(500)
        $writer = New-Object IO.StreamWriter($pipe); $writer.AutoFlush = $true
        $reader = New-Object IO.StreamReader($pipe); $writer.WriteLine($Command)
        $pending = $reader.ReadLineAsync()
        if (-not $pending.Wait(500)) { throw 'Rematch pipe response timeout' }
        C72-Event $events $Command $pending.Result $script:watch.Elapsed.TotalMilliseconds
        return $pending.Result
    } finally { $pipe.Dispose() }
}
function Shot([string]$Stage) {
    # Host screenshot only: never focuses the window or sends input. Monitor may include other windows.
    Assert-Owned
    $bounds=[Windows.Forms.Screen]::FromHandle($script:owned.MainWindowHandle).Bounds
    $bitmap=New-Object Drawing.Bitmap($bounds.Width,$bounds.Height)
    $graphics=[Drawing.Graphics]::FromImage($bitmap)
    $began=$script:watch.Elapsed.TotalMilliseconds
    try {
        $graphics.CopyFromScreen($bounds.X,$bounds.Y,0,0,$bounds.Size)
        $path=Join-Path $OutDir "$Stage.png"
        $bitmap.Save($path,[Drawing.Imaging.ImageFormat]::Png)
        C72-Event $events 'screenshot' "$Stage.png capture_began_ms=$began" $script:watch.Elapsed.TotalMilliseconds
    } finally { $graphics.Dispose(); $bitmap.Dispose() }
}
function Close-Owned {
    if (-not $script:owned) { return }
    $p = Get-Process -Id $script:owned.Id -ErrorAction SilentlyContinue
    if ($p -and $p.StartTime -eq $script:owned.StartTime) {
        $null = $p.CloseMainWindow()
        if (-not $p.WaitForExit(30000)) { throw 'Owned game did not close gracefully; NO forced kill or restoration while running' }
        Note-Closed
    }
    $script:owned = $null
}
function Get-C72LaunchIdentity($Process, [DateTime]$Launched, [string]$ExpectedPath) {
    # New process objects can precede readable image metadata. Requery in the
    # existing bounded loop; never adopt based on process name/PID alone.
    try { $started=$Process.StartTime; $path=$Process.Path } catch { return 'pending' }
    if (-not $started -or [string]::IsNullOrWhiteSpace($path)) { return 'pending' }
    if ($started -lt $Launched.AddSeconds(-1)) { throw 'Process predates this launch' }
    if ((C72-Full $path) -ne $ExpectedPath) { throw 'New process does not belong to this launch' }
    return 'owned'
}
function Finish-C72 {
    if ($script:cleaned) { return }
    try {
        Close-Owned
        if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'A game remains; restoration deferred' }
        if (Test-Path -LiteralPath $addonLog) { [IO.File]::WriteAllText((Join-Path $OutDir 'addon.log'), (C72-ReadShared $addonLog)) }
        C72-UnlockSaves $script:saveHandles
        if ($script:entry) {
            C72-Restore $SaveRoot $entryDir $script:entry
            & $script:RealSleep -Seconds 2
            if (-not (C72-Same $script:entry (C72-Manifest $SaveRoot))) { throw 'Save tree changed after restoration' }
            $script:result.saves_restored = $true
        }
        if ($script:touched) {
            foreach ($n in @('Nioh3EngineFix.ini','Nioh3BossRematch.ini')) {
                Copy-Item -LiteralPath (Join-Path $iniDir $n) -Destination (Join-Path $GameDir $n) -Force
                if ((Get-FileHash -LiteralPath (Join-Path $iniDir $n)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $GameDir $n)).Hash) { throw "INI restoration failed: $n" }
            }
            $script:result.inis_restored = $true
        }
        if ($script:binaryRecord) { C72-BinaryRestore $script:binaryRecord }
        Check-Asis
        $script:result.binaries_restored=$true
        for ($i=$script:held.Count-1; $i -ge 0; $i--) {
            $key=$script:held[$i]; $global:LockKey=$key; $r=Invoke-GameLock 'release'
            if ($r.code) { throw "Lock release failed: $key" }
        }
        $script:held.Clear(); $script:cleaned=$true
    } catch { $script:result.status='RESTORE_OR_CLOSE_FAILED'; $script:result.error="$_"; throw }
    finally { C72-Json (Join-Path $OutDir 'result.json') $script:result }
}
# Guard's default forced-kill policy is overridden for this pilot. Exact owned process, graceful only.
function Kill-Registered { try { Close-Owned } catch { Write-Warning "$_" } }
# The shared abort path deletes its record even when cleanup throws and closes again.
# This pilot must retain recovery ownership until Finish-C72 verifies all restoration.
function Guard-Fail([string]$why, [int]$code = 99) {
    if (-not $global:HG) { Write-Host "GUARD-FAIL (no guard): $why"; [Environment]::Exit($code) }
    if ($global:HG.aborting) { [Environment]::Exit($code) }
    $global:HG.aborting = $true
    HG-Log "ABORT: $why"
    try { if ($global:HG.cleanup) { & $global:HG.cleanup } } catch { HG-Log "cleanup error; recovery record retained: $_" }
    if ($script:cleaned) {
        Clear-MeasureLock
        Remove-Item -LiteralPath $global:HG.file -Force -ErrorAction SilentlyContinue
    }
    # No second close: it could succeed after failed cleanup without restoring saves/INIs.
    [Environment]::Exit($code)
}
Init-Guard -Name c72e_native_pilot -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -MaxCrashes 1 -Cleanup { Finish-C72 }
try {
    foreach ($key in @('nioh3-pc')) {
        $global:LockKey=$key; $r=Invoke-GameLock 'acquire' 1800
        if ($r.code) { throw "Shared lock '$key' unavailable: $($r.out)" }
        $script:held.Add($key)
    }
    Assert-Owner
    if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Game appeared during lock acquisition; nothing installed' }
    Add-Type -AssemblyName System.Drawing,System.Windows.Forms # framework assemblies; no source compilation or new native module
    $script:entry = C72-Manifest $SaveRoot
    if (-not $script:entry.Count) { throw 'Empty entry save tree refused' }
    C72-Backup $SaveRoot $entryDir $script:entry
    $script:saveHandles = C72-LockSaves $SaveRoot $script:entry
    $null = New-Item -ItemType Directory -Path $iniDir
    foreach ($n in @('Nioh3EngineFix.ini','Nioh3BossRematch.ini')) { Copy-Item -LiteralPath (Join-Path $GameDir $n) -Destination (Join-Path $iniDir $n) }
    C72-Json (Join-Path $OutDir 'entry-manifest.json') @{ captured_utc=[DateTime]::UtcNow.ToString('o'); save_root=$SaveRoot; saves=$script:entry; asis=$hashes; plan=$plan }
    if ($EngineCandidate -or $RematchCandidate) {
        $script:binaryRecord=C72-BinaryPrepare $binaryPlan (Join-Path $OutDir 'entry-binaries')
        C72-BinaryStage $script:binaryRecord
    }
    $empty = Join-Path $OutDir 'empty-arenas.tsv'
    Copy-Item -LiteralPath (Join-Path $Kit 'C72E-pilot-empty-arenas.tsv') -Destination $empty
    $lines = [IO.File]::ReadAllLines($rematchIni)
    C72-UniqueIni $lines @('Rematch','Remote','Menu','Debug')
    foreach ($kv in @(@('Rematch','Enable','1'),@('Rematch','FightNow','1'),@('Rematch','FightNowWarp','0'),@('Rematch','FightNowSpawn','0'),
        @('Rematch','ArenaCapture','0'),@('Rematch','SelfTest',''),@('Rematch','Catalog',$catalog),@('Rematch','Arenas',$empty),
        @('Remote','Pipe','1'),@('Menu','Overlay','0'),@('Debug','BossLog','1'),@('Debug','RematchLog','0'))) { $lines = Set-IniValue $lines $kv[0] $kv[1] $kv[2] }
    $perf = [IO.File]::ReadAllLines($engineIni)
    C72-UniqueIni $perf @('General','Debug')
    $perf = Set-IniValue $perf 'General' 'AutoContinueLoad' '1'
    $perf = Set-IniValue $perf 'Debug' 'Tour' '0'
    $script:touched=$true
    [IO.File]::WriteAllLines($rematchIni,$lines,(New-Object Text.UTF8Encoding($false)))
    [IO.File]::WriteAllLines($engineIni,$perf,(New-Object Text.UTF8Encoding($false)))
    Check-Asis $runtimeHashes; Assert-Launch
    if (Get-Process -Name Nioh3 -ErrorAction SilentlyContinue) { throw 'Game appeared before launch; refusing second launch' }
    $launched=[DateTime]::Now
    Start-Process -FilePath 'steam://rungameid/3681010' -WindowStyle Hidden
    $until=[DateTime]::Now.AddSeconds(120)
    do {
        $games=@(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue)
        if ($games.Count -gt 1) { throw 'More than one game process appeared' }
        if ($games.Count -eq 1) {
            if ((Get-C72LaunchIdentity $games[0] $launched (Join-Path $GameDir 'Nioh3.exe')) -eq 'owned') {
                $script:owned=$games[0]; break
            }
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::Now -lt $until)
    if (-not $script:owned) { throw 'No owned game appeared; no relaunch' }
    $global:HG.games=@(@{pid=$script:owned.Id;name='Nioh3';start=$script:owned.StartTime.ToString('o');closed=$false;counted=$false}); HG-Save
    Write-Host "Owned game $($script:owned.Id): root selects Continue once and records first landing. No automated title input."
    $until=[DateTime]::Now.AddSeconds(180); $ready=$false
    $readyTracker=@{since=$null;last=$null}
    do {
        try { $s=Invoke-C72Pipe 'status'; $ready=C72-EntrySettled $readyTracker $script:watch.ElapsedMilliseconds $s $true }
        catch { $ready=C72-EntrySettled $readyTracker $script:watch.ElapsedMilliseconds '' $false; Assert-Owned }
        if (-not $ready) { Start-Sleep -Milliseconds 250 }
    } while (-not $ready -and [DateTime]::Now -lt $until)
    if (-not $ready) { throw 'No continuously ready offline field for12s within180s after manual Continue' }
    $script:result.entry_tuple=@{mission=(C72-Field $s 'mission');stage=(C72-Field $s 'stage');reply=$s}
    $null=Invoke-C72Pipe 'scan'; $cleared=$false; $until=[DateTime]::Now.AddSeconds(15)
    do { $s=Invoke-C72Pipe 'row RC72E'; $cleared=(C72-Field $s 'status') -eq '2'; if (-not $cleared) { Start-Sleep -Milliseconds 200 } } while (-not $cleared -and [DateTime]::Now -lt $until)
    if (-not $cleared) { throw 'RC72E is not cleared on this journey' }
    Shot 'before-request'
    $requestLogBefore=C72-ReadShared $addonLog
    $q=Invoke-C72Pipe 'fight RC72E'; if ($q -notmatch '^queued') { throw "Fight request refused: $q" }
    $script:result.request_disposition='QUEUED_ACK_ONLY'
    $script:watch.Restart(); $entryObservation=@{saw_load=$false;native_start=$null;battle=$null;first_player=$null}
    $script:result.status='OBSERVING'; $until=[DateTime]::Now.AddSeconds($TrialSeconds)
    do {
        $refusal=C72-NewRefusal $requestLogBefore (C72-ReadShared $addonLog)
        if ($refusal) {
            $script:result.request_disposition='REFUSED_AFTER_QUEUE_ACK'
            throw "Native admission refused after queue acknowledgement: $refusal; no retry"
        }
        $s=Invoke-C72Pipe 'status'
        if ($s -match 'gate=(online|co-op session)' -or (C72-Field $s 'warps') -ne '0' -or (C72-Field $s 'spawn') -ne '-') { throw "Unexpected network/warp/spawn state: $s" }
        $observed=C72-ObserveEntry $entryObservation $s $start $target $script:watch.Elapsed.TotalMilliseconds
        $script:result.native_start=$entryObservation.native_start
        $script:result.battle_observed=[bool]$entryObservation.battle
        $script:result.battle_evidence=$entryObservation.battle
        if (-not $script:result.first_observed -and $entryObservation.first_player) {
            $land=$entryObservation.first_player
            $script:result.first_observed=$land; C72-Json (Join-Path $OutDir 'first-observed.json') $land
            Shot 'first-observed-player'
            Write-Host "FIRST OBSERVED PLAYER PROXY: $($land.position -join ',') before_gate=$($land.before_gate) near_target=$($land.near_target). Root visual capture required."
        }
        if ($observed) { break } # battle during intro is retained, never an early completion
        if ((C72-Field $s 'fnow') -eq 'fail') { throw 'Native gate/encounter verifier failed' }
        Start-Sleep -Milliseconds 25
    } while ([DateTime]::Now -lt $until)
    if (-not $observed -or -not $script:result.first_observed -or -not $script:result.battle_observed) { throw 'Bounded observation incomplete' }
    Shot 'boss-observed'
    $log=(C72-ReadShared $addonLog)
    $null=C72-NewRefusal $requestLogBefore $log # verifies the pre-request log prefix still belongs to this run
    $log=$log.Substring($requestLogBefore.Length)
    if ($log -notmatch 'arena: BossBattleMode start[^\r\n]*mission 0xC72E[^\r\n]*333550') { throw 'BossBattleMode log lacks the expected mission/placement' }
    $reviewRequest=@{nonce=[guid]::NewGuid().ToString('N');owner_pid=$OwnerPid;owner_start=$global:HG.ownerStart.ToString('o');
        runner_pid=$PID;runner_start=(Get-Process -Id $PID).StartTime.ToString('o');game_pid=$script:owned.Id;game_start=$script:owned.StartTime.ToString('o')}
    C72-Json (Join-Path $OutDir 'control-review-request.json') $reviewRequest
    C72-Json (Join-Path $OutDir 'result.json') $script:result
    Write-Host "WAITING FOR ROOT CONTROL REVIEW ($ReviewSeconds seconds): confirm visible arena, boss and actual control response with publish_c72e_control_review.ps1. First-player telemetry is not control proof."
    $reviewPath=Join-Path $OutDir 'control-review.json';$reviewUntil=[DateTime]::UtcNow.AddSeconds($ReviewSeconds)
    do { Assert-Owned;if (Test-Path -LiteralPath $reviewPath) { break };Start-Sleep -Milliseconds 250 } while ([DateTime]::UtcNow -lt $reviewUntil)
    if (-not (Test-Path -LiteralPath $reviewPath)) { throw 'Root control review absent within bound; control remains unconfirmed' }
    $review=Get-Content -LiteralPath $reviewPath -Raw|ConvertFrom-Json;C72-CheckControlReview ([pscustomobject]$reviewRequest) $review
    $script:result.root_control_confirmed=$true;$script:result.control_review=$review
    $returnTracker=@{since=$null;last=$null};$returnReady=$false;$returnUntil=[DateTime]::UtcNow.AddSeconds(15)
    do {
        $s=Invoke-C72Pipe 'status';$returnReady=C72-ReturnProxySettled $returnTracker $script:watch.ElapsedMilliseconds $s $start
        if (-not $returnReady) { Start-Sleep -Milliseconds 250 }
    } while (-not $returnReady -and [DateTime]::UtcNow -lt $returnUntil)
    if (-not $returnReady) { throw 'Return observation proxy did not settle; no return command sent' }
    $script:result.return_gate_evidence='FOUR_SECOND_TELEMETRY_PROXY_NATIVE_WRAPPER_AUTHORITATIVE'
    $script:result.status=if ($script:result.first_observed.before_gate -and $script:result.first_observed.near_target) { 'LANDING_PROXY_AND_BOSS_OBSERVED' } else { 'ROOT_CONFIRMED_NATIVE_ENTRY_FIRST_CONTROL_TIME_UNKNOWN' }
    $returnLogBefore=C72-ReadShared $addonLog
    $exitAck=Invoke-C72Pipe 'exit' # One native mission return; no retry or relaunch.
    $script:result.return_ack=$exitAck
    if ($exitAck -notmatch '^queued') { $script:result.return_disposition='COMMAND_REFUSED';throw "Return command refused: $exitAck" }
    $script:result.return_disposition='QUEUED_ACK_ONLY'
    $returnUntil=[DateTime]::UtcNow.AddSeconds(90)
    do {
        $s=Invoke-C72Pipe 'status'
        $returnLog=C72-ReadShared $addonLog
        try { $returnEvidence=C72-ReturnEvidence $returnLogBefore $returnLog $s $start $script:result.entry_tuple }
        catch { $script:result.return_disposition='REFUSED_OR_UNCERTAIN';throw }
        $script:result.return_disposition=$returnEvidence.disposition
        if ($returnEvidence.confirmed) {
            $script:result.native_return_confirmed=$true
            $script:result.native_return_evidence=$returnEvidence
            Shot 'native-return-observed'
            break
        }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $returnUntil)
    if (-not $script:result.native_return_confirmed) { throw 'Native return not observed within90s; no retry' }
} catch { $script:result.status='TRIAL_FAILED'; $script:result.error="$_"; Write-Warning "$_" }
finally {
    try { Finish-C72 } finally { Done-Guard -KeepFile:(-not $script:cleaned) }
}
Write-Host "Result: $(Join-Path $OutDir 'result.json'); instant entry remains unconfirmed until root checks visual evidence."
if ($script:result.status -notin @('LANDING_PROXY_AND_BOSS_OBSERVED','ROOT_CONFIRMED_NATIVE_ENTRY_FIRST_CONTROL_TIME_UNKNOWN') -or -not $script:result.root_control_confirmed -or -not $script:result.native_return_confirmed -or -not $script:cleaned) { exit 1 }
