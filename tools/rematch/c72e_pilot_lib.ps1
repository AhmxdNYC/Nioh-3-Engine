# Read-only on import. Helpers for the single-launch C72E pilot and isolated fixtures.
Set-StrictMode -Version Latest

function C72-ReadySettled($Tracker,[long]$NowMs,[string]$Status,[bool]$Readable) {
    $good=$Readable -and (C72-Field $Status 'state') -eq '0' -and
        (C72-Field $Status 'player') -eq '1' -and (C72-Field $Status 'loading') -eq '0' -and
        $Status -match 'gate=ok(?: |$)'
    # A stalled observer is not a continuous ready observation.
    if ($null -ne $Tracker.last -and ($NowMs -lt $Tracker.last -or $NowMs-$Tracker.last -gt 1500)) { $Tracker.since=$null }
    $Tracker.last=$NowMs
    if (-not $good) { $Tracker.since=$null; return $false }
    if ($null -eq $Tracker.since) { $Tracker.since=$NowMs }
    return $NowMs-$Tracker.since -ge 12000
}
function C72-NewRefusal([string]$Before,[string]$After) {
    if (-not $After.StartsWith($Before,[StringComparison]::Ordinal)) { throw 'Add-on log changed/truncated after request; admission unconfirmed' }
    $tail=$After.Substring($Before.Length)
    $match=[regex]::Match($tail,'(?m)^\[[^\r\n]+\] Rematch: refused: ([^\r\n]+)')
    if ($match.Success) { return $match.Groups[1].Value }
    return $null
}

function C72-Full([string]$Path) { [IO.Path]::GetFullPath($Path).TrimEnd('\') }
function C72-Inside([string]$Root, [string]$Path) {
    (C72-Full $Path).StartsWith((C72-Full $Root) + '\', [StringComparison]::OrdinalIgnoreCase)
}
function C72-Child([string]$Root, [string]$Relative) {
    $p = C72-Full (Join-Path $Root $Relative)
    if (-not (C72-Inside $Root $p)) { throw "Path escaped its root: $Relative" }
    return $p
}
function C72-Files([string]$Root) {
    # Reject junctions before walking them, including reparse files. No recursive moves/deletes.
    $rootItem = Get-Item -LiteralPath $Root -Force -ErrorAction Stop
    if ($rootItem.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse root refused: $Root" }
    $todo = New-Object 'System.Collections.Generic.Queue[string]'
    $todo.Enqueue($rootItem.FullName)
    while ($todo.Count) {
        foreach ($item in Get-ChildItem -LiteralPath $todo.Dequeue() -Force -ErrorAction Stop) {
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse child refused: $($item.FullName)" }
            if ($item.PSIsContainer) { $todo.Enqueue($item.FullName) } else { $item }
        }
    }
}
function C72-Manifest([string]$Root) {
    # Same relative-path/SHA256 method as tools/tour/run_tour.ps1 Get-Manifest.
    $full = C72-Full $Root; $m = @{}
    foreach ($f in @(C72-Files $full)) {
        $rel = $f.FullName.Substring($full.Length + 1)
        $m[$rel] = @{ length = $f.Length; sha256 = (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash.ToLowerInvariant() }
    }
    return $m
}
function C72-Same($A, $B) {
    if ($A.Count -ne $B.Count) { return $false }
    foreach ($k in $A.Keys) {
        if (-not $B.ContainsKey($k) -or $A[$k].sha256 -ne $B[$k].sha256 -or $A[$k].length -ne $B[$k].length) { return $false }
    }
    return $true
}
function C72-Backup([string]$Source, [string]$Destination, $Manifest) {
    foreach ($rel in $Manifest.Keys) {
        $dst = C72-Child $Destination $rel
        $null = New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force
        Copy-Item -LiteralPath (C72-Child $Source $rel) -Destination $dst -Force -ErrorAction Stop
    }
    if (-not (C72-Same $Manifest (C72-Manifest $Destination))) { throw 'Fresh entry backup hash verification failed' }
}
function C72-LockSaves([string]$Root, $Manifest) {
    # Deny writes and replacement/deletion to CURRENT files; do not interfere with read-only loads.
    # New temp files remain possible: the verified restoration below handles additions after exit.
    $handles = New-Object 'System.Collections.Generic.List[System.IDisposable]'
    try {
        foreach ($rel in $Manifest.Keys) {
            $handles.Add([IO.File]::Open((C72-Child $Root $rel), [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read))
        }
        if (-not (C72-Same $Manifest (C72-Manifest $Root))) { throw 'Entry files changed before save locks were established' }
        return ,$handles
    } catch { foreach ($h in $handles) { $h.Dispose() }; throw }
}
function C72-UnlockSaves($Handles) { if ($Handles) { foreach ($h in $Handles) { $h.Dispose() }; $Handles.Clear() } }
function C72-Restore([string]$Root, [string]$Backup, $Manifest) {
    # Caller MUST establish that no game remains and release this run's save handles first.
    if (-not (C72-Same $Manifest (C72-Manifest $Backup))) { throw 'Entry backup changed; refusing to restore from it' }
    $have = C72-Manifest $Root
    foreach ($rel in $have.Keys) {
        if (-not $Manifest.ContainsKey($rel)) { Remove-Item -LiteralPath (C72-Child $Root $rel) -Force -ErrorAction Stop }
    }
    foreach ($rel in $Manifest.Keys) {
        if (-not $have.ContainsKey($rel) -or $have[$rel].sha256 -ne $Manifest[$rel].sha256) {
            $dst = C72-Child $Root $rel
            $null = New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force
            Copy-Item -LiteralPath (C72-Child $Backup $rel) -Destination $dst -Force -ErrorAction Stop
        }
    }
    if (-not (C72-Same $Manifest (C72-Manifest $Root))) { throw 'Entry save restoration differs by path, length or SHA256' }
}
function C72-ReadShared([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    $stream = [IO.File]::Open($Path, 'Open', 'Read', 'ReadWrite')
    try { $reader = New-Object IO.StreamReader($stream); return $reader.ReadToEnd() } finally { $stream.Dispose() }
}
function C72-Field([string]$Reply, [string]$Key) {
    if ($Reply -match ('(?:^| )' + [regex]::Escape($Key) + '=(\S+)')) { return $Matches[1] }
    return ''
}
function C72-FirstCandidate([string]$Reply, [bool]$SawLoad, [int]$Start) {
    # Proxy ONLY: the ASI exposes pause/loading/player flags, not control-enabled or a render-frame boundary.
    return $SawLoad -and (C72-Field $Reply 'mission') -eq '0xC72E' -and
        (C72-Field $Reply 'stage') -eq "$Start" -and (C72-Field $Reply 'loading') -eq '0' -and
        (C72-Field $Reply 'player') -eq '1' -and $Reply -match 'gate=(rematch active|mission queue busy)(?: |$)'
}
function C72-Landing([string]$Reply, [double[]]$Target) {
    $xyz = (C72-Field $Reply 'pos').Split(',')
    if ($xyz.Count -ne 3) { throw 'Position telemetry is missing' }
    $p = @($xyz | ForEach-Object { [double]::Parse($_, [Globalization.CultureInfo]::InvariantCulture) })
    if (@($p | Where-Object { [double]::IsNaN($_) -or [double]::IsInfinity($_) }).Count) { throw 'Nonfinite position telemetry' }
    $dx = $p[0] - $Target[0]; $dz = $p[2] - $Target[2]
    return @{ position = $p; horizontal_m = [Math]::Sqrt($dx*$dx + $dz*$dz)/100.0
        vertical_m = [Math]::Abs($p[1]-$Target[1])/100.0; before_gate = (C72-Field $Reply 'fnow') -eq 'armed'
        near_target = $dx*$dx + $dz*$dz -le 200*200 -and [Math]::Abs($p[1]-$Target[1]) -le 300 }
}
function C72-Json([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 12), (New-Object Text.UTF8Encoding($false)))
}
function C72-UniqueIni([string[]]$Lines, [string[]]$Sections) {
    $section=''; $seen=@{}
    foreach ($line in $Lines) {
        if ($line -match '^\s*\[([^]]+)\]') { $section=$Matches[1].Trim(); continue }
        if ($section -in $Sections -and $line -match '^\s*([^;#=]+?)\s*=') {
            $key=$section+'.'+$Matches[1].Trim()
            if ($seen.ContainsKey($key)) { throw "Duplicate INI key refused: $key" }
            $seen[$key]=$true
        }
    }
}
function C72-Event([string]$Path, [string]$Kind, [string]$Reply, [double]$ElapsedMs) {
    $v = @{ utc = [DateTime]::UtcNow.ToString('o'); elapsed_ms = [Math]::Round($ElapsedMs,3); kind = $Kind; reply = $Reply }
    [IO.File]::AppendAllText($Path, ($v | ConvertTo-Json -Compress) + "`r`n", (New-Object Text.UTF8Encoding($false)))
}
