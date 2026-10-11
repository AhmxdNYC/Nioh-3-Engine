# Pure route/approval validation and pinned import. No work on import.
function Training-CheckDependencies([string]$Repo) {
    $pins=@{
        'tools/rematch/c72e_pilot_lib.ps1'='8ce34b9ac175d0f58285c803ca08a57194d8cdb9aac8006943d9097f6fea52cc'
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
    $pin='36470d21a95aa4432302b5fa7ce50f446ae96139ac5bf0b049cea9164106d33f'
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
