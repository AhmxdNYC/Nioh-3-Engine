# pc2_pc_weakcpu.ps1 - weak-CPU simulation runs (low-end Windows PC), same runner pattern as pc_retest_run8.ps1. 2026-10-09.
#   Per-process affinity only (harness sets it on the Nioh3 process after launch, -Affinity N; reverts when the game exits; no system settings).
#   W  live quads AABB,BPO,DQ,P1E7,W1B,RPA   pc2_abba2_pc.ps1 -Live ... -LaunchFlags none -Quads 3 -Affinity 4 -MaxFps 240 -Input vpad   (~25 min)
#   S  PJ prewarm A/B (shader-compile hitches on a slow CPU)   pc2_run_tour.ps1 x4 with tourlist_pj.txt, -Affinity 4 (run8 Block P)      (~30 min)
# Same lock / ASI install from main bin / always-restore as run8. Compare W against run8 Block A (unrestricted) and S against run8 P.
# -DryRun: wrapper/analysis paths with NO game, NO lock, NO ASI swap.
param([string[]]$Blocks = @("W", "S"), [string]$Tag = "weakcpu", [int]$Affinity = 4, [switch]$DryRun, [switch]$NoN2)
$ErrorActionPreference = "Stop"
$ws = "C:\Users\User\projects\nioh3-engine"; $here = "$ws\tools\testmatrix"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $gameAsi = "$G\Nioh3EngineFix.asi"
$newAsi = "$ws\mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi"; $bak = "$ws\captures\pc\Nioh3EngineFix.asi.pre${Tag}.bak"
$br = "C:\Users\User\projects\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc2"
$ab = "$ws\captures\tm\abba2"; New-Item -ItemType Directory -Force $ab | Out-Null
$log = "$ab\${Tag}_runner.txt"
$pjDir = "$G\Nioh3EngineFix_pj"; $pjUser = "$G\Nioh3EngineFix_pj.user_$Tag"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Add-Content $log $l; Write-Host $l }
function Lock([string]$a) { Push-Location $br; try { py -m uo_bridge.gamelock $a nioh3 --ttl 7200 2>&1 | Out-String } finally { Pop-Location } }
function Send([string]$m) { Push-Location $br; try { py -m uo_bridge send nioh3 $m | Out-Null } finally { Pop-Location } }
function Renew { if (-not $DryRun) { Push-Location $br; try { py -m uo_bridge.gamelock renew nioh3 --ttl 7200 | Out-Null } finally { Pop-Location } } }

$Blocks = @($Blocks -split "," | ForEach-Object { $_.Trim().ToUpper() } | Where-Object { $_ })
foreach ($b in $Blocks) { if ($b -notin "W", "S") { throw "unknown block $b (W, S)" } }
$abbaArgs = @{
    W = @("-Live", "AABB,BPO,DQ,P1E7,W1B,RPA", "-LaunchFlags", "none", "-Quads", "3")
}
$summary = [ordered]@{}   # block -> one-line result

function Run-Abba([string]$b) {
    $rn = "${Tag}_$b"; $con = "$ab\${rn}_console.txt"
    $extra = @("-MaxFps", "240", "-Input", "vpad", "-Affinity", "$Affinity", "-RunName", $rn)
    if ($DryRun) {
        if (-not (Test-Path "$G\Nioh3EngineFix.ini.preboss.bak")) { $extra += @("-BaseIniFile", "$ws\mod\Nioh3EngineFix\bin\Nioh3EngineFix.ini") }   # game dir stashed / no preboss.bak: dry run on the repo ini
        & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\pc2_abba2_pc.ps1" @($abbaArgs[$b]) @extra -DryRun *> $con
        $ok = (Select-String -Path $con -Pattern "^DryRun OK" -Quiet)
        $miss = Select-String -Path $con -Pattern "MISSING|no menu row|throw|Exception" | Select-Object -First 3
        L "block $b DryRun: $(if ($ok -and -not $miss) { 'OK' } else { 'FAILED' }) (console $con)"
        Get-Content $con | Where-Object { $_ -match "menu rows|arms:|DryRun OK" } | ForEach-Object { L "   $_" }
        if (-not $ok -or $miss) { Get-Content $con | Select-Object -Last 15 | ForEach-Object { L "   | $_" }; throw "block $b dry run failed" }
        $summary[$b] = "dry-run ok"
        return
    }
    L "block $b start ($rn)"
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\pc2_abba2_pc.ps1" @($abbaArgs[$b]) @extra -OwnerPid $PID *>> $con
    L "block $b exit $LASTEXITCODE"
    $rd = "$ab\$rn"
    py -I "$here\abba2_report.py" $rd *> $null
    $rep = "$rd\report.txt"
    L "block $b report: $(if (Test-Path $rep) { 'written' } else { 'MISSING' })"
    if ($b -eq "J") {   # digest the JT launch (L2) with the last window's log
        $jl = Get-ChildItem $rd -Filter "L2_JT_s*.log" -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
        if ($jl) {
            $dg = (py -I "$ws\tools\gqjt_digest.py" $jl.FullName --skip 1 2>&1 | Out-String)
            Add-Content $rep "`n==== gqjt_digest.py $($jl.Name) --skip 1 ====`n$dg"
            $script:jtDigest = $dg
            L "block J digest appended ($($dg.Length) chars)"
        } else { Add-Content $rep "`nNo L2_JT_s*.log found: the JT launch did not produce a window log."; L "block J: no JT window log" }
    }
    $v = if (Test-Path $rep) { @(Select-String -Path $rep -Pattern "^(\S+): (KEEP|DROP|INVALID|NEUTRAL|MAYBE)" | ForEach-Object { $_.Line.Trim() }) } else { @() }
    $summary[$b] = $(if ($v.Count) { $v -join " ; " } else { "no verdict lines (see $rep)" })
    if ($b -eq "J" -and $script:jtDigest) {
        $dec = @($script:jtDigest -split "`n" | Where-Object { $_ -match "DECISION" } | ForEach-Object { ($_.Trim() -replace "\s+", " ") -replace "^(.{140}).*$", '$1...' })
        $bp = @(Select-String -Path $rep -Pattern "JT.*(fps|cost|INVALID)" | Select-Object -First 1 | ForEach-Object { $_.Line.Trim() })
        $summary[$b] = ($(if ($dec.Count) { $dec -join " ; " } else { "digest has no DECISION lines" }) + $(if ($v.Count) { " | probe cost: " + ($v -join " ; ") } else { "" }))
    }
    Renew
}

function Run-Tour([string]$label, [string]$keys, [switch]$Wait) {
    $capRoot = "$ab\${Tag}_S\$label"
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$ws\tools\tour\pc2_run_tour.ps1", "-Input", "vpad", "-List", "$ws\tools\tour\tourlist_pj.txt",
           "-Points", "2", "-DwellSec", "4", "-NoProfile", "-NoPause", "-CaptureRoot", $capRoot, "-ExtraKeys", $keys, "-Affinity", "$Affinity", "-OwnerPid", "$PID")
    if ($Wait) { $a += "-WaitPrewarm" }
    if ($DryRun) { $a += @("-DryRun") } else { $a += @("-NoLock") }
    L "S/$label start: ExtraKeys '$keys'$(if ($Wait) { ' + title held for the prewarm' })"
    if ($DryRun) {   # run_tour -DryRun builds its own temp game dir and keeps the capture there; read the ini it used from the Say-log
        $o = (& powershell @a 2>&1 | Out-String)
        $okk = $o -match "verdict: done"
        $iniOk = $true
        foreach ($kv in ($keys -split ";")) { if ($kv -match "^(\w+)\.(\w+)=(.*)$") { } }
        L "S/${label} DryRun: $(if ($okk) { 'tour wrapper OK (verdict done)' } else { 'FAILED' }); ExtraKeys applied line: $((($o -split "`n" | Where-Object { $_ -match 'mod: .* installed with' } | Select-Object -First 1)).Trim())"
        if (-not $okk) { ($o -split "`n" | Select-Object -Last 20) | ForEach-Object { L "   | $_" }; throw "S/$label dry run failed" }
        $m = [regex]::Match($o, "capture folder: (.+)"); if ($m.Success) { $script:dryCaps[$label] = $m.Groups[1].Value.Trim() }
        return
    }
    & powershell @a *>> "$ab\${Tag}_S_console.txt"
    L "S/$label exit $LASTEXITCODE"
    $cd = Get-ChildItem $capRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
    if ($cd) { $script:caps[$label] = $cd.FullName } else { L "S/${label}: no capture dir under $capRoot" }
    Renew
}

$script:caps = @{}; $script:dryCaps = @{}; $script:jtDigest = ""
function Run-S {
    $pd = "$ab\${Tag}_S"; New-Item -ItemType Directory -Force $pd | Out-Null
    $spr = "Patches.SPR=1"
    if (-not $DryRun) {
        if (Test-Path $pjDir) { Move-Item $pjDir $pjUser; L "user's PJ journal folder moved to $pjUser" }
        Remove-Item $pjDir -Recurse -Force -ErrorAction SilentlyContinue   # J starts from an empty journal
    }
    try {
        Run-Tour "J" "Patches.PJ=1;$spr"
        Run-Tour "N" "Patches.PJ=0;$spr"
        Run-Tour "W" "Patches.PJ=1;$spr" -Wait
        if (-not $NoN2) { Run-Tour "N2" "Patches.PJ=0;$spr" }
    } finally {
        if (-not $DryRun) {
            if (Test-Path $pjDir) { $sz = (Get-ChildItem $pjDir -Recurse -File | Measure-Object Length -Sum).Sum; L "test journal folder: $([int]($sz / 1KB)) KB - removed"; Remove-Item $pjDir -Recurse -Force -ErrorAction SilentlyContinue }
            if (Test-Path $pjUser) { Move-Item $pjUser $pjDir; L "user's PJ journal folder restored" }
        }
    }
    $items = @(); foreach ($k in "J", "N", "W", "N2") { $c = if ($DryRun) { $script:dryCaps[$k] } else { $script:caps[$k] }; if ($c) { $items += "$k=$c" } }
    $rep = if ($DryRun) { "$env:TEMP\${Tag}_S_dry_report.txt" } else { "$pd\report.txt" }
    if ($items.Count) {
        $txt = (py -I "$ws\tools\pj_hitch.py" --out $rep @items 2>&1 | Out-String)
        L "S report: $(if (Test-Path $rep) { $rep } else { 'MISSING' })"
        $vl = @($txt -split "`n" | Where-Object { $_ -match "^PJ: " } | ForEach-Object { $_.Trim() })
        $summary["S"] = $(if ($DryRun) { "dry-run ok (analyzer ran on fake captures: $(($vl -join ' ') -replace '^(.{80}).*$','$1'))" } elseif ($vl.Count) { $vl -join " ; " } else { "no verdict (see $rep)" })
    } else { $summary["S"] = "no captures"; L "S: no captures to analyse" }
}

# ---------------------------------------------------------------------------------------------------------------------------------
if ($DryRun) {
    L "DRY RUN ($Tag): blocks $($Blocks -join ',') - no game, no lock, no ASI swap"
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { L "note: Nioh3 is running (the Ally does not matter here; dry run only)" }
    L ("bin ASI: $newAsi " + $(if (Test-Path $newAsi) { "$((Get-Item $newAsi).Length) bytes, $((Get-Item $newAsi).LastWriteTime)" } else { "MISSING" }))
    py -I "$ws\tools\pj_hitch.py" --selftest | Select-Object -Last 1 | ForEach-Object { L "pj_hitch: $_" }
    py -I "$ws\tools\gqjt_digest.py" --selftest | Select-Object -Last 1 | ForEach-Object { L "gqjt_digest: $_" }
    foreach ($b in $Blocks) { if ($b -eq "S") { Run-S } else { Run-Abba $b } }
    L "dry run done: $(($summary.GetEnumerator() | ForEach-Object { $_.Key + '=' + $_.Value }) -join ' | ')"
    return
}

if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is already running" }
if (-not (Test-Path "$G\Nioh3EngineFix.ini") -and -not (Test-Path "$G\Nioh3EngineFix.ini.preboss.bak")) { throw "no Nioh3EngineFix.ini in the game dir: it looks stashed by a vanilla play session (tools\playsession\pc2_stop_session.ps1 restores it)" }
if (-not (Test-Path "$G\Nioh3EngineFix.ini.preboss.bak")) { Copy-Item "$G\Nioh3EngineFix.ini" "$G\Nioh3EngineFix.ini.preboss.bak"; L "preboss.bak created from the installed ini (abba2 base + restore point)" }
if (-not (Test-Path $newAsi)) { throw "no ASI build at $newAsi" }
$st = Lock "status"; L "lock status: $($st.Trim())"
if ($st -match "'holder': '(\w+)'" -and $Matches[1] -ne "pc2") { throw "game lock held by $($Matches[1])" }
L ("acquire: " + (Lock "acquire").Trim())
Send "[FYI] pc: lock taken $(Get-Date -f HH:mm) for weak-CPU run $Tag, affinity $Affinity (blocks $($Blocks -join ','); ~55 min). Will post [GAME-FREE] when done."
try {
    if (-not (Test-Path $bak)) { Copy-Item $gameAsi $bak; L "game ASI backed up -> $bak" }
    Copy-Item $newAsi $gameAsi -Force; L "installed main ASI ($((Get-Item $gameAsi).Length) bytes)"
    foreach ($b in $Blocks) {
        try { if ($b -eq "S") { Run-S } else { Run-Abba $b } }
        catch { L "block $b FAILED: $_"; $summary[$b] = "FAILED: $_" }
    }
} finally {
    Get-Process Nioh3 -ErrorAction SilentlyContinue | ForEach-Object { L "game still running pid $($_.Id) - waiting 60 s"; $_.WaitForExit(60000) | Out-Null }
    if (Test-Path $bak) { Copy-Item $bak $gameAsi -Force; L "game ASI restored from $bak" }
    if (Test-Path $pjUser) { if (-not (Test-Path $pjDir)) { Move-Item $pjUser $pjDir; L "user's PJ journal folder restored" } }
    $para = "weakcpu affinity $Affinity (ASI main bin, $(Get-Date -f 'yyyy-MM-dd HH:mm')): " + (($summary.GetEnumerator() | ForEach-Object { "Block $($_.Key): $($_.Value)." }) -join " ") +
            " Reports: captures\tm\abba2\${Tag}_{W,S}\report.txt; runner log ${Tag}_runner.txt."
    Set-Content "$ab\${Tag}_summary.txt" $para -Encoding UTF8
    L "summary: $ab\${Tag}_summary.txt"
    L ("release: " + (Lock "release").Trim())
    Send "[GAME-FREE] game=nioh3 from=pc (weak-CPU run $Tag finished $(Get-Date -f HH:mm); summary captures\tm\abba2\${Tag}_summary.txt)."
    L "done"
}
