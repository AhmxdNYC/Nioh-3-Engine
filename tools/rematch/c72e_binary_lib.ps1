# No mutation on import or plan. Binary transactions require an already-held PC lease.
function C72-BinaryHash([string]$Path) {
    $item=Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Not a regular binary: $Path" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function C72-BinaryPlan([string]$GameDir, $Pins, $Candidates) {
    foreach ($name in @('Nioh3EngineFix.asi','Nioh3BossRematch.asi')) {
        $destination=Join-Path $GameDir $name
        if ((C72-BinaryHash $destination) -ne $Pins[$name]) { throw "Original binary pin mismatch: $name" }
        $candidate=$Candidates[$name]
        if ([bool]$candidate.path -ne [bool]$candidate.hash) { throw "Candidate path and SHA256 must be supplied together: $name" }
        $source=$destination; $hash=$Pins[$name]
        if ($candidate.path) {
            if ($candidate.hash -notmatch '^[a-fA-F0-9]{64}$') { throw "Invalid candidate SHA256: $name" }
            $source=C72-Full $candidate.path; $hash=$candidate.hash.ToLowerInvariant()
            if ($source -eq (C72-Full $destination)) { throw "Candidate must be external: $name" }
            if ((C72-BinaryHash $source) -ne $hash) { throw "Candidate SHA256 mismatch: $name" }
        }
        [pscustomobject]@{ name=$name; destination=$destination; source=$source; original=$Pins[$name]; candidate=$hash }
    }
}
function C72-BinaryJournal($Record) {
    $temporary=$Record.path+'.new'
    [IO.File]::WriteAllText($temporary,($Record | ConvertTo-Json -Depth 8))
    if (Test-Path -LiteralPath $Record.path) { [IO.File]::Replace($temporary,$Record.path,[NullString]::Value) }
    else { [IO.File]::Move($temporary,$Record.path) }
}
function C72-BinaryPrepare($Plan,[string]$Directory) {
    if (Test-Path -LiteralPath $Directory) { throw 'Binary recovery directory must be new' }
    $null=New-Item -ItemType Directory -Path $Directory
    $entries=@()
    foreach ($p in $Plan) {
        # Back up BOTH originals before the first stage. A failed backup cannot touch installation.
        if ((C72-BinaryHash $p.destination) -ne $p.original) { throw "Original changed before backup: $($p.name)" }
        $backup=Join-Path $Directory $p.name
        [IO.File]::Copy($p.destination,$backup,$false)
        if ((C72-BinaryHash $backup) -ne $p.original) { throw "Binary backup mismatch: $($p.name)" }
        $entries+= [pscustomobject]@{ name=$p.name; destination=$p.destination; source=$p.source; backup=$backup
            original=$p.original; candidate=$p.candidate; stageAttempted=$false; restored=$false }
    }
    $record=@{ path=(Join-Path $Directory 'recovery.json'); ownerPid=$PID
        ownerStart=(Get-Process -Id $PID).StartTime.ToString('o'); complete=$false; entries=$entries }
    C72-BinaryJournal $record
    return $record
}
function C72-BinaryReplace([string]$Source,[string]$Destination,[string]$ExpectedCurrent,[string]$ExpectedNew) {
    # Same-directory replace is atomic on NTFS. No non-atomic overwrite fallback.
    $temporary=$Destination+'.c72e-'+[guid]::NewGuid().ToString('N')+'.tmp'
    try {
        if ((C72-BinaryHash $Source) -ne $ExpectedNew) { throw 'Replacement source hash mismatch' }
        [IO.File]::Copy($Source,$temporary,$false)
        if ((C72-BinaryHash $temporary) -ne $ExpectedNew) { throw 'Replacement copy hash mismatch' }
        if ((C72-BinaryHash $Destination) -ne $ExpectedCurrent) { throw 'Foreign destination bytes; replacement refused' }
        [IO.File]::Replace($temporary,$Destination,[NullString]::Value)
        if ((C72-BinaryHash $Destination) -ne $ExpectedNew) { throw 'Replacement verification failed' }
    } finally {
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
    }
}
function C72-BinaryStage($Record) {
    foreach ($entry in $Record.entries) {
        if ($entry.candidate -eq $entry.original) { continue }
        # Persist intent before Replace: even an exception after replacement is recoverable.
        $entry.stageAttempted=$true; C72-BinaryJournal $Record
        C72-BinaryReplace $entry.source $entry.destination $entry.original $entry.candidate
    }
}
function C72-BinaryRestore($Record) {
    foreach ($entry in $Record.entries) {
        if ((C72-BinaryHash $entry.backup) -ne $entry.original) { throw "Recovery backup changed: $($entry.name)" }
        $current=C72-BinaryHash $entry.destination
        if ($current -ne $entry.original) {
            if (-not $entry.stageAttempted -or $current -ne $entry.candidate) { throw "Foreign binary preserved: $($entry.name)" }
            C72-BinaryReplace $entry.backup $entry.destination $entry.candidate $entry.original
        }
        $entry.restored=$true; C72-BinaryJournal $Record
    }
    foreach ($entry in $Record.entries) {
        if ((C72-BinaryHash $entry.destination) -ne $entry.original) { throw 'Binary changed after restore' }
    }
    $Record.complete=$true; C72-BinaryJournal $Record
    # Keep the journal and backups even after success as exact restoration receipts.
}
