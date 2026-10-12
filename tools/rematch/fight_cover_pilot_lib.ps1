# Pure validators; no work on import. Frozen candidate pins intentionally unset.
function Get-FCPExpected {
    return @{mod_tree='';engine_sha256='';addon_sha256='';gate_receipt_sha256='';admission_receipt_sha256=''}
}
function FCP-CheckDependencies([string]$Repo) {
    $p=Join-Path $Repo 'tools/rematch/training_pilot_lib.ps1'
    if ((Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant() -cne '0e66ba321ce0beb0c3af2ffa96c9386e14bdd3938c28a5cae271a5b59bfccc36') { throw 'Reviewed training library changed' }
}
function FCP-Admission($Expected,$Receipt,[string]$ReceiptHash,[string]$Tree,[hashtable]$Hashes,[string]$GateHash,[int]$OwnerPid,[string]$OwnerStart) {
    if ($Expected.mod_tree -notmatch '^[0-9a-f]{40}$') { throw 'Frozen cover candidate admission unset; no launch authorized' }
    foreach ($k in @('engine_sha256','addon_sha256','gate_receipt_sha256','admission_receipt_sha256')) { if ($Expected[$k] -notmatch '^[0-9a-f]{64}$') { throw "Admission pin unset: $k" } }
    if ($ReceiptHash -cne $Expected.admission_receipt_sha256 -or $GateHash -cne $Expected.gate_receipt_sha256 -or $Tree -cne $Expected.mod_tree -or $Hashes['Nioh3EngineFix.asi'] -cne $Expected.engine_sha256 -or $Hashes['Nioh3BossRematch.asi'] -cne $Expected.addon_sha256) { throw 'Cover candidate/receipt/source lineage mismatch' }
    if ($Receipt.approved -isnot [bool] -or -not $Receipt.approved -or $Receipt.scope -cne 'ONE_FIGHT_COVER_NATIVE_PILOT_NEW_FEATURE_UNTESTED' -or $Receipt.owner_pid -ne $OwnerPid -or $Receipt.owner_start -cne $OwnerStart -or $Receipt.mod_tree -cne $Tree -or $Receipt.engine_sha256 -cne $Expected.engine_sha256 -or $Receipt.addon_sha256 -cne $Expected.addon_sha256) { throw 'Explicit cover admission missing or mismatched' }
    return @{scope=$Receipt.scope;mod_tree=$Tree;gate_receipt_sha256=$GateHash;admission_receipt_sha256=$ReceiptHash;runtime_hashes=$Hashes;old_pass_transferred=$false;launches=1}
}
function FCP-Parse([string]$Reply) {
    $r=@{}
    foreach ($key in @('token','phase','attempt','start','placement','presented','issued','bossEntry','placementEntry','deadline','control','algo','life','playerAlive','sample')) {
        $m=[regex]::Matches($Reply,"(?:^| )$key=(\d+)(?= |$)")
        if ($m.Count -ne 1) { throw "Cover field missing/ambiguous: $key" }
        $r[$key]=[uint64]$m[0].Groups[1].Value
    }
    $m=[regex]::Matches($Reply,'(?:^| )mission=(0x[0-9A-Fa-f]+)(?= |$)')
    if ($m.Count -ne 1) { throw 'Cover mission missing/ambiguous' };$r.mission=$m[0].Groups[1].Value
    if ($r.phase -gt 6) { throw 'Unknown cover phase' }
    foreach ($k in @('presented','issued','bossEntry','placementEntry','control','algo','life','playerAlive')) { if ($r[$k] -gt 1) { throw "Invalid cover bit: $k" } }
    return $r
}
function FCP-Evidence([uint64]$BaselineToken,[string]$Before,[string]$After,[string]$Reply) {
    if (-not $After.StartsWith($Before,[StringComparison]::Ordinal)) { throw 'Cover log replaced/truncated' }
    $s=FCP-Parse $Reply;$fresh=$After.Substring($Before.Length)
    if ($s.token -le $BaselineToken) { return $false }
    if ($s.mission -ine '0x1CBC' -or $s.start -ne 67064 -or $s.placement -ne 65092) { throw 'Cover route tuple mismatch' }
    if ($s.phase -in @(5,6)) { throw 'Cover failed/cancelled release is not success' }
    $events=@([regex]::Matches($fresh,'Rematch: cover token=(\d+) phase=(\d+) attempt=(\d+) event=(\w+)([^\r\n]*)'))
    $step=0;$attempt=[uint64]0
    foreach ($e in $events) {
        if ([uint64]$e.Groups[1].Value -ne $s.token) { throw 'Unexpected cover token within request window; no retry' }
        $phase=[int]$e.Groups[2].Value;$a=[uint64]$e.Groups[3].Value;$event=$e.Groups[4].Value
        if ($phase -in @(5,6)) { throw 'Cover failure event retained; cannot promote later ready' }
        if ($event -eq 'begin') {
            if ($step -ne 0 -or $phase -ne 1 -or $a -ne 0 -or $e.Groups[5].Value -notmatch '\bmission=0x1CBC start=67064 placement=65092\b') { throw 'Cover begin mismatch' };$step=1
        } elseif ($event -eq 'present_ack') {
            if ($step -ne 1 -or $phase -ne 2 -or $a -ne 0) { throw 'Cover ACK missing/out of order' };$step=2
        } elseif ($event -eq 'native_post') {
            if ($step -ne 2 -or $phase -ne 2 -or -not $a) { throw 'Native post without drawn-cover ACK' };$attempt=$a;$step=3
        } elseif ($event -eq 'phase' -and $phase -eq 4) {
            if ($step -ne 3 -or $a -ne $attempt) { throw 'Ready without same-attempt post' };$step=4
        } elseif ($step -ge 3 -and $a -ne $attempt) { throw 'Cover native attempt changed' }
    }
    if ($s.phase -ne 4) { return $false }
    if ($step -ne 4 -or $s.attempt -ne $attempt -or $s.presented -ne 1 -or $s.issued -ne 1 -or $s.bossEntry -ne 1 -or $s.placementEntry -ne 1 -or $s.control -ne 1 -or $s.algo -ne 1 -or $s.life -ne 1 -or $s.playerAlive -ne 1 -or -not $s.sample -or $s.sample -gt $s.deadline) { throw 'Ready snapshot lacks exact ordered witnesses' }
    return $true
}
function FCP-Complete($Result) {
    foreach ($key in @('root_control_confirmed','native_return_confirmed','saves_restored','inis_restored','binaries_restored')) { if ($Result[$key] -isnot [bool] -or -not $Result[$key]) { return $false } }
    return $Result.status -ceq 'FIGHT_COVER_REVIEWED_RETURN_OBSERVED'
}
function FCP-CheckApproval($Request,$Approval) {
    foreach ($key in @('nonce','stage','owner_pid','owner_start','runner_pid','runner_start','game_pid','game_start')) {
        if ([string]::IsNullOrWhiteSpace([string]$Request.$key) -or [string]$Request.$key -cne [string]$Approval.$key) { throw "Stale/foreign cover approval: $key" }
    }
    if ([string]::IsNullOrWhiteSpace([string]$Approval.evidence)) { throw 'Root visual/control evidence required' }
    if ($Approval.stage -eq 'ControlReviewed') { foreach ($k in @('visible_arena','visible_boss','control_response')) { if ($Approval.$k -isnot [bool] -or -not $Approval.$k) { throw "Root confirmation missing: $k" } } }
    if ($Approval.stage -notin @('FieldReady','ControlReviewed','NativeReturn')) { throw 'Unknown cover approval stage' }
}
function FCP-CheckStatus([string]$Reply) {
    foreach ($key in @('state','loading','player','mission','stage','infight','warps','spawn')) {
        if ([string]::IsNullOrWhiteSpace((C72-Field $Reply $key))) { throw "Missing cover route telemetry: $key" }
    }
    # Actual SpawnLast literals: failed550/566/583/1526/1534; recovery560; applied589; pending967; start/arena1550.
    if ((C72-Field $Reply 'spawn') -in @('failed','recovery')) { throw 'Spawn failure or unresolved restoration ownership' }
    if ((C72-Field $Reply 'warps') -notmatch '^[0-3]$' -or (C72-Field $Reply 'spawn') -notin @('-','pending','applied','arena','start')) { throw 'Unknown/failed fight warp or spawn state' }
    if ($Reply -match 'gate=(online|co-op|.*unreadable|.*unsupported|native request uncertain)' -or (C72-Field $Reply 'fnow') -eq 'fail') { throw "Unsafe/indeterminate cover status: $Reply" }
}
function FCP-ReturnSettled($Tracker,[long]$NowMs,[string]$Reply) {
    $good=(C72-Field $Reply 'mission') -eq '0x1CBC' -and (C72-Field $Reply 'stage') -eq '67064' -and (C72-Field $Reply 'loading') -eq '0' -and (C72-Field $Reply 'player') -eq '1' -and $Reply -match 'gate=(rematch active|mission queue busy|native replay/return still pending)(?: |$)'
    if ($null -ne $Tracker.last -and ($NowMs -lt $Tracker.last -or $NowMs-$Tracker.last -gt 1500)) { $Tracker.since=$null }
    $Tracker.last=$NowMs
    if (-not $good) { $Tracker.since=$null;return $false }
    if ($null -eq $Tracker.since) { $Tracker.since=$NowMs }
    return $NowMs-$Tracker.since -ge 4000 # telemetry proxy; authoritative native ExitGate can still refuse
}
