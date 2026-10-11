# State helpers for pinned_final_pc.ps1. Loading this file has no filesystem mutations.
# Reuse the reviewed tour SHA-256 manifest functions without executing its launcher.
$pbTour = Join-Path $PSScriptRoot '..\tour\run_tour.ps1'
$pbTokens = $null; $pbErrors = $null
$pbAst = [System.Management.Automation.Language.Parser]::ParseFile($pbTour, [ref]$pbTokens, [ref]$pbErrors)
if ($pbErrors.Count) { throw 'Cannot parse tour manifest helpers' }
foreach ($pbName in 'Get-Manifest', 'Compare-Manifest') {
    $pbFunction = $pbAst.Find({ param($n) $n -is [System.Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $pbName }, $true)
    if (-not $pbFunction) { throw "Missing tour helper $pbName" }
    Invoke-Expression $pbFunction.Extent.Text
}

function Assert-PBChild([string]$Root, [string]$Path) {
    $r = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/') + [IO.Path]::DirectorySeparatorChar
    $p = [IO.Path]::GetFullPath($Path)
    if (-not $p.StartsWith($r, [StringComparison]::OrdinalIgnoreCase)) { throw "Path outside intended root: $p" }
    return $p
}
function Assert-PBPlainTree([string]$Root) {
    if (-not (Test-Path -LiteralPath $Root -PathType Container)) { throw "Missing directory: $Root" }
    foreach ($p in @((Get-Item -LiteralPath $Root -Force)) + @(Get-ChildItem -LiteralPath $Root -Recurse -Force)) {
        if ($p.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point refused: $($p.FullName)" }
    }
}
function Assert-PBPreflight([string]$Game, [string]$Save, [string]$Config, [bool]$Running) {
    if ($Running) { throw 'Nioh3 already running; leave the existing session alone' }
    if (Test-Path -LiteralPath (Join-Path $Game 'Nioh3EngineFix.asi.stockoff')) { throw 'Stale .stockoff: restore the previous session first' }
    Assert-PBPlainTree $Save
    if (-not (Test-Path -LiteralPath $Game -PathType Container)) { throw 'Game directory missing' }
    $null = Assert-PBChild $Save $Config
    if (-not (Test-Path -LiteralPath $Config -PathType Leaf)) { throw 'Entry config.xml missing' }
    foreach ($name in 'Nioh3EngineFix.asi', 'Nioh3EngineFix.ini', 'Nioh3EngineFix.log') {
        $p = Join-Path $Game $name
        if (Test-Path -LiteralPath $p) {
            if ((Get-Item -LiteralPath $p -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point refused: $p" }
        }
    }
}
function New-PBPlan([string[]]$Configs, [string[]]$Presets, [int]$Runs) {
    if ($Runs -lt 1 -or $Runs -gt 20) { throw 'Runs must be 1..20' }
    if ($Configs.Count -ne 2 -or @($Configs | Select-Object -Unique).Count -ne 2 -or
        $Configs -notcontains 'STOCK' -or $Configs -notcontains 'OPTIMIZED') { throw 'Pinned mode requires STOCK,OPTIMIZED exactly once each' }
    if (@($Presets | Select-Object -Unique).Count -ne $Presets.Count) { throw 'Duplicate presets refused' }
    foreach ($preset in $Presets) {
        for ($run = 1; $run -le $Runs; $run++) {
            $order = $Configs.Clone()
            if ($run % 2 -eq 0) { [array]::Reverse($order) }
            foreach ($cfg in $order) { [pscustomobject]@{ cfg = $cfg; preset = $preset; run = $run; id = "final_pc_${cfg}_${preset}_r$run" } }
        }
    }
}
function New-PBState([string]$Game, [string]$Save, [string]$Output) {
    $gameFull = [IO.Path]::GetFullPath($Game); $saveFull = [IO.Path]::GetFullPath($Save)
    $outFull = [IO.Path]::GetFullPath($Output)
    foreach ($r in $gameFull, $saveFull) {
        if ($outFull -eq $r -or $outFull.StartsWith($r.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Output must be outside game/save roots' }
    }
    if (Test-Path -LiteralPath $outFull) { throw 'Use a new output directory; entry snapshots are never reused' }
    $saveBackup = Join-Path $outFull 'entry\save'; $gameBackup = Join-Path $outFull 'entry\game'
    New-Item -ItemType Directory -Path $saveBackup, $gameBackup -Force | Out-Null
    $manifest = Get-Manifest $saveFull
    foreach ($rel in $manifest.Keys) {
        $src = Assert-PBChild $saveFull (Join-Path $saveFull $rel)
        $dst = Assert-PBChild $saveBackup (Join-Path $saveBackup $rel)
        New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }
    $diff = Compare-Manifest $manifest (Get-Manifest $saveBackup)
    if ($diff.changed.Count + $diff.added.Count + $diff.removed.Count) { throw 'Entry save backup/hash verification failed' }
    $entries = @()
    foreach ($name in 'Nioh3EngineFix.asi', 'Nioh3EngineFix.ini', 'Nioh3EngineFix.log') {
        $src = Assert-PBChild $gameFull (Join-Path $gameFull $name); $exists = Test-Path -LiteralPath $src -PathType Leaf
        $sha = $null; $ticks = $null
        if ($exists) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $gameBackup $name) -Force
            $sha = (Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash
            if ($sha -ne (Get-FileHash -LiteralPath (Join-Path $gameBackup $name) -Algorithm SHA256).Hash) { throw "Entry backup differs: $name" }
            $ticks = (Get-Item -LiteralPath $src).LastWriteTimeUtc.Ticks
        }
        $entries += [pscustomobject]@{ name = $name; exists = $exists; sha = $sha; ticks = $ticks }
    }
    $state = [pscustomobject]@{ game = $gameFull; save = $saveFull; output = $outFull; files = $entries; save_manifest = $manifest }
    $state | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outFull 'entry-state.json') -Encoding UTF8
    return $state
}
function Restore-PBState($State) {
    $saveBackup = Join-Path $State.output 'entry\save'; $gameBackup = Join-Path $State.output 'entry\game'
    Assert-PBPlainTree $State.save
    $want = Get-Manifest $saveBackup
    $backupDiff = Compare-Manifest $State.save_manifest $want
    if ($backupDiff.changed.Count + $backupDiff.added.Count + $backupDiff.removed.Count) { throw 'Save backup damaged; recovery refused' }
    # Check every entry backup before performing any restore write.
    foreach ($entry in $State.files) {
        if ($entry.exists) {
            $src = Assert-PBChild $gameBackup (Join-Path $gameBackup $entry.name)
            if ((Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash -ne $entry.sha) { throw "Damaged entry backup: $($entry.name)" }
        }
    }
    $diff = Compare-Manifest $want (Get-Manifest $State.save)
    foreach ($rel in $diff.added) { Remove-Item -LiteralPath (Assert-PBChild $State.save (Join-Path $State.save $rel)) -Force }
    foreach ($rel in @($diff.changed) + @($diff.removed)) {
        $src = Assert-PBChild $saveBackup (Join-Path $saveBackup $rel)
        $dst = Assert-PBChild $State.save (Join-Path $State.save $rel)
        New-Item -ItemType Directory -Path (Split-Path -Parent $dst) -Force | Out-Null
        Copy-Item -LiteralPath $src -Destination $dst -Force
    }
    foreach ($entry in $State.files) {
        $dst = Assert-PBChild $State.game (Join-Path $State.game $entry.name)
        if ($entry.exists) {
            $src = Assert-PBChild $gameBackup (Join-Path $gameBackup $entry.name)
            if ((Get-FileHash -LiteralPath $src -Algorithm SHA256).Hash -ne $entry.sha) { throw "Damaged entry backup: $($entry.name)" }
            Copy-Item -LiteralPath $src -Destination $dst -Force
            (Get-Item -LiteralPath $dst).LastWriteTimeUtc = [DateTime]::new([long]$entry.ticks, [DateTimeKind]::Utc)
            if ((Get-FileHash -LiteralPath $dst -Algorithm SHA256).Hash -ne $entry.sha) { throw "File restore failed: $dst" }
        } elseif (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Force }
    }
    $after = Compare-Manifest $want (Get-Manifest $State.save)
    if ($after.changed.Count + $after.added.Count + $after.removed.Count) { throw 'Save restoration hash verification failed' }
}
function Read-PBState([string]$Path) {
    $state=Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    $manifest=@{}
    foreach ($property in $state.save_manifest.PSObject.Properties) { $manifest[$property.Name]=@{sha=$property.Value.sha;len=$property.Value.len} }
    $state.save_manifest=$manifest
    return $state
}
function Test-PBWindows($Windows, [int]$StandWindows, [bool]$Combat) {
    $required = @{ stand = $StandWindows; move = 1 }
    if ($Combat) { $required.combat = 1 }
    foreach ($kind in $required.Keys) {
        $w = @($Windows | Where-Object { $_.kind -eq $kind })
        if ($w.Count -ne $required[$kind] -or @($w | Where-Object { -not $_.ok -or $_.duration_s -lt 0.9 * $_.requested_s }).Count) { return $false }
    }
    return $true
}
function Get-PBCaptureDuration([string]$Csv) {
    $duration=0.0; $first=$true
    foreach ($row in Import-Csv -LiteralPath $Csv) {
        # The first interval can begin before capture armed. It cannot prove coverage.
        if ($first) { $first=$false; continue }
        $value=$row.MsBetweenPresents
        if (-not $value) { $value=$row.FrameTime }
        $number=0.0
        if ([double]::TryParse($value,[Globalization.NumberStyles]::Float,[Globalization.CultureInfo]::InvariantCulture,[ref]$number) -and $number -gt 0) { $duration += $number/1000 }
    }
    return $duration
}
function New-PBMarker([string]$Directory) {
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $path = Join-Path $Directory 'pc-benchmark-active'
    $token = [guid]::NewGuid().ToString('N')
    $stream = [IO.File]::Open($path, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes((@{ pid=$PID; token=$token; started=(Get-Date).ToString('o') } | ConvertTo-Json -Compress))
        $stream.Write($bytes,0,$bytes.Length)
    } finally { $stream.Dispose() }
    return [pscustomobject]@{ path=$path; token=$token }
}
function Remove-PBMarker($Marker) {
    $current = Get-Content -LiteralPath $Marker.path -Raw | ConvertFrom-Json
    if ($current.token -cne $Marker.token) { throw 'Benchmark marker belongs to another owner; left intact' }
    Remove-Item -LiteralPath $Marker.path -Force
}
function Wait-PBWorkerIdle([string]$StatePath, [double]$MaxSeconds, [bool]$OwnershipConfirmed, [scriptblock]$Alive = { param($p) [bool](Get-Process -Id $p -ErrorAction SilentlyContinue) }) {
    if (-not $OwnershipConfirmed) { throw 'Root source ownership must be explicitly confirmed before queueing' }
    $started=Get-Date
    while ($true) {
        if (-not (Test-Path -LiteralPath $StatePath)) { return } # absent controller + explicit root ownership
        try { $controller=Get-Content -LiteralPath $StatePath -Raw | ConvertFrom-Json } catch { throw 'Controller state unreadable; source ownership uncertain' }
        if (-not $controller.PSObject.Properties['worker_pid'] -or -not $controller.PSObject.Properties['phase']) { throw 'Controller schema unknown; source ownership uncertain' }
        if ($controller.phase -notin @('working','ready','waiting','idle','cooldown','paused','stopped','complete','blocked')) { throw 'Controller phase unknown/error; source ownership uncertain' }
        $worker=$controller.worker_pid
        if ($null -eq $worker) {
            if ($controller.phase -eq 'working') { throw 'Working controller without worker identity; source ownership uncertain' }
            return
        }
        $workerId=0
        if (-not [int]::TryParse([string]$worker,[ref]$workerId) -or $workerId -le 0) { throw 'Controller worker identity invalid' }
        # There is no start-time identity: PID reuse conservatively counts as busy.
        if (-not (& $Alive $workerId)) { return }
        if (((Get-Date)-$started).TotalSeconds -ge $MaxSeconds) { throw 'Controller worker still live; benchmark timed out before preparation' }
        [Threading.Thread]::Sleep(1000)
    }
}
