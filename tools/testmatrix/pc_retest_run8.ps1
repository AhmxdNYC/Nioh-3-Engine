# pc_retest_run8.ps1 - PC re-test run8 on ASI 1.7.40 (main: BPO row, AABB=2 fixed), 2026-10-08. One go, no fiddling.
#   A  live quads  AABB,BPO,DQ,P1E7,W1B,RPA        abba2_pc.ps1 -Live ... -LaunchFlags none -Quads 3        (pc-cpu-next.md s5 Block A)
#   C  MA launch arms                          abba2_pc.ps1 -Arms BASE,MA,BASE,MA,BASE -Live none        (Block C)
#   J  JT probe, one launch, log-only          abba2_pc.ps1 -Arms BASE,JT,BASE -Live none ; tools\gqjt_digest.py on the JT launch log
#   P  PJ prewarm A/B on a fixed tour route    tools\tour\run_tour.ps1 x4 (tourlist_pj.txt: 5 stages, 2 points x 4 s), SPR = 1, vpad:
#        J  PJ=1  empty journal    -> records the route's PSOs                         (not scored)
#        N  PJ=0                   -> baseline: first-use PSO compiles on the route
#        W  PJ=1  prewarm          -> title held until 'PJ: prewarm done', then the same route
#        N2 PJ=0                   -> brackets W (route / driver drift; N vs N2 > 30 % on creates = INVALID)
#      Metric (tools\pj_hitch.py): rendered frames > 33 ms in 30 s SPR windows whose cumulative '>=1 ms runtime gfx PSO creates'
#      grew (= PSO-attributed hitches, count and ms), next to all route hitches and the SPR create totals; level loads excluded.
# Takes the nioh3 game lock, installs main's bin\Nioh3EngineFix.asi (game ASI backed up), runs the blocks in the order given, writes each
# block's report.txt, ALWAYS restores the game ASI (+ the user's PJ journal folder), releases the lock, posts GAME-FREE, and writes
# captures\tm\abba2\<Tag>_summary.txt (one paragraph). -DryRun: every block's wrapper/analysis path with NO game, NO lock, NO ASI swap.
# Order: A first (primary evidence), then J, C, P (P is the longest and least critical). Est. ~85 min incl. boots (A 25, J 15, C 25, P 30).
param([string[]]$Blocks = @("A", "J", "C", "P"), [string]$Tag = "run8", [switch]$DryRun, [switch]$NoN2)
$ErrorActionPreference = "Stop"
$ws = "C:\Users\Ahmad Hamza\nioh3-engine"; $here = "$ws\tools\testmatrix"
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $gameAsi = "$G\Nioh3EngineFix.asi"
$newAsi = "$ws\mod\Nioh3EngineFix\bin\Nioh3EngineFix.asi"; $bak = "$ws\captures\pc\Nioh3EngineFix.asi.pre${Tag}.bak"
$br = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
$ab = "$ws\captures\tm\abba2"; New-Item -ItemType Directory -Force $ab | Out-Null
$log = "$ab\${Tag}_runner.txt"
$pjDir = "$G\Nioh3EngineFix_pj"; $pjUser = "$G\Nioh3EngineFix_pj.user_$Tag"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Add-Content $log $l; Write-Host $l }
function Lock([string]$a) { Push-Location $br; try { py -m uo_bridge.gamelock $a nioh3 --ttl 7200 2>&1 | Out-String } finally { Pop-Location } }
function Send([string]$m) { Push-Location $br; try { py -m uo_bridge send nioh3 $m | Out-Null } finally { Pop-Location } }
function Renew { if (-not $DryRun) { Push-Location $br; try { py -m uo_bridge.gamelock renew nioh3 --ttl 7200 | Out-Null } finally { Pop-Location } } }

$Blocks = @($Blocks -split "," | ForEach-Object { $_.Trim().ToUpper() } | Where-Object { $_ })
foreach ($b in $Blocks) { if ($b -notin "A", "C", "J", "P") { throw "unknown block $b (A, C, J, P)" } }
$abbaArgs = @{
    A = @("-Live", "AABB,BPO,DQ,P1E7,W1B,RPA", "-LaunchFlags", "none", "-Quads", "3")
    C = @("-Arms", "BASE,MA,BASE,MA,BASE", "-Live", "none")
    J = @("-Arms", "BASE,JT,BASE", "-Live", "none")
}
$summary = [ordered]@{}   # block -> one-line result

function Run-Abba([string]$b) {
    $rn = "${Tag}_$b"; $con = "$ab\${rn}_console.txt"
    $extra = @("-MaxFps", "240", "-Input", "vpad", "-RunName", $rn)
    if ($DryRun) {
        if (-not (Test-Path "$G\Nioh3EngineFix.ini.preboss.bak")) { $extra += @("-BaseIniFile", "$ws\mod\Nioh3EngineFix\bin\Nioh3EngineFix.ini") }   # game dir stashed / no preboss.bak: dry run on the repo ini
        & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\abba2_pc.ps1" @($abbaArgs[$b]) @extra -DryRun *> $con
        $ok = (Select-String -Path $con -Pattern "^DryRun OK" -Quiet)
        $miss = Select-String -Path $con -Pattern "MISSING|no menu row|throw|Exception" | Select-Object -First 3
        L "block $b DryRun: $(if ($ok -and -not $miss) { 'OK' } else { 'FAILED' }) (console $con)"
        Get-Content $con | Where-Object { $_ -match "menu rows|arms:|DryRun OK" } | ForEach-Object { L "   $_" }
        if (-not $ok -or $miss) { Get-Content $con | Select-Object -Last 15 | ForEach-Object { L "   | $_" }; throw "block $b dry run failed" }
        $summary[$b] = "dry-run ok"
        return
    }
    L "block $b start ($rn)"
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$here\abba2_pc.ps1" @($abbaArgs[$b]) @extra -OwnerPid $PID *>> $con
    L "block $b exit $LASTEXITCODE"
    $rd = "$ab\$rn"
    $ea = $ErrorActionPreference; $ErrorActionPreference = "Continue"   # stderr of a failing report must not abort the block (run8b A: block A FAILED: Traceback)
    try { $ro = (py -I "$herebba2_report.py" $rd 2>&1 | Out-String); if ($LASTEXITCODE -ne 0) { L "block $b report script failed: $((($ro -split "`n") | Select-Object -Last 3) -join ' | ')" } } finally { $ErrorActionPreference = $ea }
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
    $capRoot = "$ab\${Tag}_P\$label"
    $a = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$ws\tools\tour\run_tour.ps1", "-Input", "vpad", "-List", "$ws\tools\tour\tourlist_pj.txt",
           "-Points", "2", "-DwellSec", "4", "-NoProfile", "-NoPause", "-CaptureRoot", $capRoot, "-ExtraKeys", $keys, "-OwnerPid", "$PID")
    if ($Wait) { $a += "-WaitPrewarm" }
    if ($DryRun) { $a += @("-DryRun") } else { $a += @("-NoLock") }
    L "P/$label start: ExtraKeys '$keys'$(if ($Wait) { ' + title held for the prewarm' })"
    if ($DryRun) {   # run_tour -DryRun builds its own temp game dir and keeps the capture there; read the ini it used from the Say-log
        $o = (& powershell @a 2>&1 | Out-String)
        $okk = $o -match "verdict: done"
        $iniOk = $true
        foreach ($kv in ($keys -split ";")) { if ($kv -match "^(\w+)\.(\w+)=(.*)$") { } }
        L "P/${label} DryRun: $(if ($okk) { 'tour wrapper OK (verdict done)' } else { 'FAILED' }); ExtraKeys applied line: $((($o -split "`n" | Where-Object { $_ -match 'mod: .* installed with' } | Select-Object -First 1)).Trim())"
        if (-not $okk) { ($o -split "`n" | Select-Object -Last 20) | ForEach-Object { L "   | $_" }; throw "P/$label dry run failed" }
        $m = [regex]::Match($o, "capture folder: (.+)"); if ($m.Success) { $script:dryCaps[$label] = $m.Groups[1].Value.Trim() }
        return
    }
    & powershell @a *>> "$ab\${Tag}_P_console.txt"
    L "P/$label exit $LASTEXITCODE"
    $cd = Get-ChildItem $capRoot -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
    if ($cd) { $script:caps[$label] = $cd.FullName } else { L "P/${label}: no capture dir under $capRoot" }
    Renew
}

$script:caps = @{}; $script:dryCaps = @{}; $script:jtDigest = ""
function Run-P {
    $pd = "$ab\${Tag}_P"; New-Item -ItemType Directory -Force $pd | Out-Null
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
    $rep = if ($DryRun) { "$env:TEMP\${Tag}_P_dry_report.txt" } else { "$pd\report.txt" }
    if ($items.Count) {
        $txt = (py -I "$ws\tools\pj_hitch.py" --out $rep @items 2>&1 | Out-String)
        L "P report: $(if (Test-Path $rep) { $rep } else { 'MISSING' })"
        $vl = @($txt -split "`n" | Where-Object { $_ -match "^PJ: " } | ForEach-Object { $_.Trim() })
        $summary["P"] = $(if ($DryRun) { "dry-run ok (analyzer ran on fake captures: $(($vl -join ' ') -replace '^(.{80}).*$','$1'))" } elseif ($vl.Count) { $vl -join " ; " } else { "no verdict (see $rep)" })
    } else { $summary["P"] = "no captures"; L "P: no captures to analyse" }
}

# ---------------------------------------------------------------------------------------------------------------------------------
if ($DryRun) {
    L "DRY RUN ($Tag): blocks $($Blocks -join ',') - no game, no lock, no ASI swap"
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { L "note: Nioh3 is running (the Ally does not matter here; dry run only)" }
    L ("bin ASI: $newAsi " + $(if (Test-Path $newAsi) { "$((Get-Item $newAsi).Length) bytes, $((Get-Item $newAsi).LastWriteTime)" } else { "MISSING" }))
    py -I "$ws\tools\pj_hitch.py" --selftest | Select-Object -Last 1 | ForEach-Object { L "pj_hitch: $_" }
    py -I "$ws\tools\gqjt_digest.py" --selftest | Select-Object -Last 1 | ForEach-Object { L "gqjt_digest: $_" }
    foreach ($b in $Blocks) { if ($b -eq "P") { Run-P } else { Run-Abba $b } }
    L "dry run done: $(($summary.GetEnumerator() | ForEach-Object { $_.Key + '=' + $_.Value }) -join ' | ')"
    return
}

if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is already running" }
if (-not (Test-Path "$G\Nioh3EngineFix.ini") -and -not (Test-Path "$G\Nioh3EngineFix.ini.preboss.bak")) { throw "no Nioh3EngineFix.ini in the game dir: it looks stashed by a vanilla play session (tools\playsession\stop_session.ps1 restores it)" }
if (-not (Test-Path "$G\Nioh3EngineFix.ini.preboss.bak")) { Copy-Item "$G\Nioh3EngineFix.ini" "$G\Nioh3EngineFix.ini.preboss.bak"; L "preboss.bak created from the installed ini (abba2 base + restore point)" }
if (-not (Test-Path $newAsi)) { throw "no ASI build at $newAsi" }
$st = Lock "status"; L "lock status: $($st.Trim())"
if ($st -match "'holder': '(\w+)'" -and $Matches[1] -ne "pc") { throw "game lock held by $($Matches[1])" }
L ("acquire: " + (Lock "acquire").Trim())
Send "[FYI] pc: lock taken $(Get-Date -f HH:mm) for PC re-test $Tag on ASI 1.7.40 (blocks $($Blocks -join ','); ~85 min). Will post [GAME-FREE] when done."
try {
    if (-not (Test-Path $bak)) { Copy-Item $gameAsi $bak; L "game ASI backed up -> $bak" }
    Copy-Item $newAsi $gameAsi -Force; L "installed main ASI ($((Get-Item $gameAsi).Length) bytes)"
    foreach ($b in $Blocks) {
        try { if ($b -eq "P") { Run-P } else { Run-Abba $b } }
        catch { L "block $b FAILED: $_"; $summary[$b] = "FAILED: $_" }
    }
} finally {
    Get-Process Nioh3 -ErrorAction SilentlyContinue | ForEach-Object { L "game still running pid $($_.Id) - waiting 60 s"; $_.WaitForExit(60000) | Out-Null }
    if (Test-Path $bak) { Copy-Item $bak $gameAsi -Force; L "game ASI restored from $bak" }
    if (Test-Path $pjUser) { if (-not (Test-Path $pjDir)) { Move-Item $pjUser $pjDir; L "user's PJ journal folder restored" } }
    $para = "run8 (ASI 1.7.40 main, $(Get-Date -f 'yyyy-MM-dd HH:mm')): " + (($summary.GetEnumerator() | ForEach-Object { "Block $($_.Key): $($_.Value)." }) -join " ") +
            " Reports: captures\tm\abba2\${Tag}_{A,C,J,P}\report.txt; runner log ${Tag}_runner.txt."
    Set-Content "$ab\${Tag}_summary.txt" $para -Encoding UTF8
    L "summary: $ab\${Tag}_summary.txt"
    L ("release: " + (Lock "release").Trim())
    Send "[GAME-FREE] game=nioh3 from=pc (PC re-test $Tag finished $(Get-Date -f HH:mm); summary captures\tm\abba2\${Tag}_summary.txt)."
    L "done"
}
