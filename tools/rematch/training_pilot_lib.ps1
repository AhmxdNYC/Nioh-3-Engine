# Pure route/approval validation and pinned import. No work on import.
function Training-CheckDependencies([string]$Repo) {
    $pins=@{
        'tools/rematch/c72e_pilot_lib.ps1'='03fd4554849f23eba0f69992b60d1723352b854cbf306ca1f1361b0525e0d94d'
        'tools/rematch/c72e_binary_lib.ps1'='ae16d3011b0966ec156aed2218eaa3e86d6a1b690279776babc29feb570617b6'
        'tools/testmatrix/harness_guard.ps1'='e2e6034c240e74806a2885ea053b23bf8d3c37aed89f0b4e9fa72b30447c6c81'
        'tools/playsession/session_lib.ps1'='98324e91ce8410edc2d519ac0855534d7e78495eb5582d297277c12fd5a248ee'
    }
    foreach ($relative in $pins.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $Repo $relative) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pins[$relative]) {
            throw "Training lifecycle dependency changed: $relative; review and repin before use"
        }
    }
}
function Training-BeginScan {
    $before=C72-Field (Invoke-C72Pipe 'row R1CBC') 'gen'
    if ($before -notmatch '^\d+$') { throw 'Pre-scan generation missing' }
    # The scan reply may already report the completed new generation.
    $null=Invoke-C72Pipe 'scan'
    return [uint64]$before
}
function Training-LogTail([string]$Before,[string]$After) {
    if (-not $After.StartsWith($Before,[StringComparison]::Ordinal)) { throw 'Addon log prefix replaced/truncated during pilot' }
    return $After.Substring($Before.Length)
}
function Training-ImportLifecycle([string]$Path) {
    $pin='a2e077a66708c2ed1278bc9bddc1c64489c5a0127d4178b6efee48f199925ea6'
    if ((C72-BinaryHash $Path) -ne $pin) { throw 'C72E lifecycle changed; review and repin before training' }
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseFile($Path,[ref]$tokens,[ref]$errors)
    if ($errors) { throw 'C72E lifecycle parse failure' }
    foreach ($name in @('Check-Asis','Assert-Owned','Invoke-C72Pipe','Shot','Close-Owned','Get-C72LaunchIdentity','Finish-C72','Kill-Registered','Guard-Fail')) {
        $found=@($ast.FindAll({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true))
        if ($found.Count -ne 1) { throw "Missing/ambiguous lifecycle function: $name" }
        # Import only definitions: never evaluate the C72E runner's top-level launch/route.
        Invoke-Expression ($found[0].Extent.Text -replace '^function\s+', 'function script:')
    }
}
function Training-QueueWitness([string]$Log) {
    return $Log -match 'Rematch: F R1CBC: queued - slot [0-2] \+4 mission 0x1CBC, \+0xC start point 67064, \+0x14 mode 2(?:\r?\n|$)'
}
function Training-CheckGate($Gate,$Manifest,$Expected) {
    if ($Gate.status -notin @('LANDING_PROXY_AND_BOSS_OBSERVED','ROOT_CONFIRMED_NATIVE_ENTRY_FIRST_CONTROL_TIME_UNKNOWN')) { throw 'C72E native entry gate not passed' }
    foreach ($key in @('saves_restored','inis_restored','binaries_restored','root_control_confirmed','native_return_confirmed')) {
        if ($Gate.$key -isnot [bool] -or -not $Gate.$key) { throw "C72E prerequisite unproven: $key" }
    }
    foreach ($name in $Expected.Keys) {
        if ($Manifest.plan.runtime_asi_hashes.$name -cne $Expected[$name]) { throw "C72E gate used different bytes: $name" }
    }
}
function Training-AtShrine([string]$Reply,[bool]$SawLoad) {
    return $SawLoad -and (C72-Field $Reply 'mission') -eq '0x1CBC' -and
        (C72-Field $Reply 'stage') -eq '67064' -and (C72-Field $Reply 'loading') -eq '0' -and
        (C72-Field $Reply 'player') -eq '1' -and (C72-Field $Reply 'infight') -eq '0' -and
        $Reply -notmatch 'gate=(game paused|not in game|.*unreadable)'
}
function Training-CheckStatus([string]$Reply) {
    foreach ($key in @('state','loading','player','mission','stage','infight','warps','spawn')) {
        if ([string]::IsNullOrWhiteSpace((C72-Field $Reply $key))) { throw "Missing telemetry: $key" }
    }
    if ((C72-Field $Reply 'warps') -ne '0' -or (C72-Field $Reply 'spawn') -ne '-') { throw 'Warp/spawn unexpectedly active' }
    if ($Reply -match 'gate=(online|co-op|.*unreadable|.*unsupported|native request uncertain)' -or (C72-Field $Reply 'fnow') -eq 'fail') { throw "Unsafe/indeterminate status: $Reply" }
    # Pending gate can mask NativeGate diagnostics in this build. This check
    # does NOT certify fresh offline/read-validity or actual player control.
}
function Training-CheckApproval($Request,$Approval) {
    foreach ($key in @('nonce','stage','owner_pid','owner_start','runner_pid','runner_start')) {
        if ([string]$Request.$key -cne [string]$Approval.$key) { throw "Stale/foreign approval: $key" }
    }
    if ([string]::IsNullOrWhiteSpace([string]$Approval.evidence)) { throw 'Root visual/stage evidence is required' }
    if ([string]$Approval.stage -notin @('ShrineRequest','EnterArena','NativeRetry','NativeReturn')) { throw 'Unknown manual stage' }
}

function Training-CandidatePath([string]$Root,[string]$Name,[string]$Layout) {
    if ($Name -notin @('Nioh3EngineFix','Nioh3BossRematch')) { throw 'Unknown candidate binary' }
    if ($Layout -eq 'Flat') { return Join-Path $Root ($Name+'.asi') }
    if ($Layout -ne 'Tree') { throw 'Unknown candidate layout' }
    return Join-Path $Root ('mod\'+$Name+'\bin\'+$Name+'.asi')
}
function Training-ReturnEvidence([string]$Before,[string]$After,[string]$Reply,$Entry) {
    if (-not $Entry -or $Entry.mission -notmatch '^0x[0-9A-Fa-f]+$' -or [string]$Entry.stage -notmatch '^\d+$') { throw 'Original entry tuple required' }
    $refused=C72-NewRefusal $Before $After
    if ($refused) { throw "Native return refused: $refused; no retry" }
    $tail=Training-LogTail $Before $After
    if ($tail -match 'Rematch: exit:[^\r\n]*EXCEPTION') { throw 'Native return uncertain; no retry' }
    $request=[regex]::Match($tail,'Rematch: exit: MissionRequestReturn\(params, 1\) from mission 0x1CBC \(retry point 67064\) -> requested')
    if (-not $request.Success) { return $false }
    $afterRequest=$tail.Substring($request.Index+$request.Length)
    $exited=$afterRequest -match ('Rematch: replay mission exited \(stage now '+[regex]::Escape([string]$Entry.stage)+', mission state \+28h '+[regex]::Escape([string]$Entry.mission)+'\)')
    return $exited -and (C72-Field $Reply 'mission') -eq $Entry.mission -and $Entry.mission -ne '0x1CBC' -and
        (C72-Field $Reply 'stage') -eq [string]$Entry.stage -and (C72-Field $Reply 'loading') -eq '0' -and
        (C72-Field $Reply 'player') -eq '1' -and (C72-Field $Reply 'state') -eq '0' -and
        (C72-Field $Reply 'warps') -eq '0' -and (C72-Field $Reply 'spawn') -eq '-' -and $Reply -match 'gate=ok(?: |$)'
}
function Training-HostedAdmission([string]$ReceiptHash,[string]$ModTree,$Hashes) {
    $expected=@{'Nioh3EngineFix.asi'='d15605a7e51f007fbbffb6e572efc5e9ec591db8fc1b59e608a4bfaffba78145';'Nioh3BossRematch.asi'='1d543289aeced5f52e3c8ff73f8417cda3057c6fb6ee20170e2b715533d5513a'}
    if ($ReceiptHash -cne '9043b96c237aecfdb63613c3be6e8b9a62394c176a64f5f11f67abe8dbcc4aff') { throw 'Hosted admission requires exact root-accepted C72E receipt' }
    if ($ModTree -cne '05a8f9e7a5ca7afd74bdf5f2cea1d5e549217898') { throw 'Hosted source mod tree changed; new review required' }
    foreach($name in $expected.Keys){if($Hashes[$name] -cne $expected[$name]){throw "Hosted candidate hash changed: $name"}}
    return @{kind='EXPLICIT_DERIVATIVE_ADMISSION_NOT_TRANSFERRED_PASS';scope='ONE_GUARDED_HOSTED_PRACTICE_PILOT_NEW_UI_UNTESTED';
        prior_receipt_sha256=$ReceiptHash;prior_source='9eeb174cb71e785e2d0b315d50a9a8fc5ed49df2';
        reviewed_new_source='3db5bc4';reviewed_code_equivalent='0c4a334';mod_tree=$ModTree;runtime_asi_hashes=$expected;
        unchanged_native_contract='ExecDirect/CallRequestById/CallReturn/TrackReplay/ReadSnap/gates/8Attempt functions/SpawnRestore/SpawnTick/MapRestoreNow/FnVerify/DoWarp/GateTick/Pump/Prepare text equal';
        changed_contract='New hosted UI and practice dispatch untested live; ToIdle additionally clears practice ownership after normal restored exit';
        exclusions='Not full training; no first-control timing, new native semantics, repeated sessions or handheld compatibility acceptance';max_launches=1}
}
function Training-HostedRequestWitness([string]$Log,[string]$Kind) {
    if($Kind -eq 'start'){return $Log -match 'Rematch: request Practice Yamagata \(native shrine\)[^\r\n]*queued from the practice menu'}
    if($Kind -eq 'return'){return $Log -match 'Rematch: request return from practice[^\r\n]*queued from the practice menu'}
    throw 'Unknown hosted request witness'
}
