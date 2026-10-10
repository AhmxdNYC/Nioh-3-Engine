<#
  cpu_live_sched.ps1 - the live half of the pc2 CPU-bound capture (notes\cpu-bound-midrange.md). Run it while the game is IN THE
  FIELD with [Debug] LiveCtl = 1 (and RTP = 2, TG installed as usual): it does not launch or close anything.
  1. Diagnostic phase: -DiagSec seconds with nothing switched (the SP / TSTATE / JT / TS windows of the plain game).
  2. Live phase: per item (-Items, default RTP,TG; KEY or KEY=a/b, e.g. XPRE=0/2) -Pairs ABBA pairs (A = 0, B = 1, or a / b): LiveCtl toggle -> ack (gen echo, fail = 0)
     -> -WarmSec -> -WindowSec measuring window. fps per window from the mod's own frame clock (ack frames / qpc_us).
  3. Ends with every item at 0. Output in -Out: manifest.jsonl (UTC spans for PresentMon / log slicing) and report.txt (paired
     B - A fps per item, mean +- SE, and the A drift).
  Run: powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\cpu_live_sched.ps1 -Out <dir>
#>
param(
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3",
    [string]$Out = "",
    [int]$DiagSec = 90,
    [string[]]$Items = @("RTP", "TG"),
    [int]$Pairs = 2, [double]$WarmSec = 5, [int]$WindowSec = 20
)
$ErrorActionPreference = "Stop"
$Items = @($Items -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
# an item is KEY (A = 0, B = 1) or KEY=a/b (level rows, e.g. XPRE=0/2); the same KEY may appear with several b values
function IKey([string]$it) { if ($it -match '^(.+?)=([^/]+)/(.+)$') { $Matches[1] } else { $it } }
function IVal([string]$it, [int]$arm) { if ($it -match '^(.+?)=([^/]+)/(.+)$') { if ($arm) { $Matches[3] } else { $Matches[2] } } else { $arm } }
if (-not $Out) { $Out = Join-Path $env:TEMP ("cpu_live_" + (Get-Date -f yyyyMMdd-HHmm)) }
New-Item -ItemType Directory -Force $Out | Out-Null
$ctl = Join-Path $GameDir "Nioh3EngineFix_live.ini"; $ack = "$ctl.ack"
$man = Join-Path $Out "manifest.jsonl"; $rep = Join-Path $Out "report.txt"
function L([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Write-Host $l; Add-Content (Join-Path $Out "sched.log") $l }
function Utc { (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ss.fffZ") }
function Read-Ack {
    for ($i = 0; $i -lt 25; $i++) {   # retry a read that caught the ack mid-replace (pc2 finding)
        $h = @{}
        $t = ""; try { $t = [IO.File]::ReadAllText($ack) } catch {}
        foreach ($kv in ($t.Trim() -split '\s+')) { if ($kv -match '^([^=]+)=(.*)$') { $h[$Matches[1]] = $Matches[2] } }
        if ($h.ContainsKey("frames") -and $h.ContainsKey("qpc_us")) { return $h }
        Start-Sleep -Milliseconds 20
    }
    return $h
}
$script:gen = [int](Get-Date -UFormat %s) % 100000   # never equal to an older control file's Gen
function Set-Live([hashtable]$kv, [string]$tag) {
    $script:gen++
    $lines = @("[Live]", "Gen = $($script:gen)", "Tag = $tag") + @($kv.Keys | Sort-Object { $(if ($_ -eq "XPRE") { 0 } else { 1 }) }, { $_ } | ForEach-Object { "$_ = $($kv[$_])" })   # preset first: LiveCtl applies lines in order
    [IO.File]::WriteAllLines("$ctl.w", $lines); Move-Item "$ctl.w" $ctl -Force
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt 6) { $a = Read-Ack; if ($a.gen -eq "$($script:gen)") { return $a }; Start-Sleep -Milliseconds 100 }
    throw "no LiveCtl ack for gen $($script:gen) ($tag) within 6 s - is [Debug] LiveCtl = 1 and the game running?"
}
function Clock { $a = Read-Ack; return @{ f = [int64]$a.frames; q = [int64]$a.qpc_us; h33 = [int64]$a.h33; h50 = [int64]$a.h50 } }
function Add-J($o) { Add-Content -Path $man -Value ($o | ConvertTo-Json -Compress) -Encoding UTF8 }

if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { throw "Nioh3 is not running" }
$init = @{}; foreach ($it in $Items) { $init[(IKey $it)] = IVal $it 0 }
$a0 = Set-Live $init "init"
if ([int]$a0.fail -ne 0) { throw "LiveCtl refused an item at init (ack: $(([IO.File]::ReadAllText($ack))))" }
L "init ok (gen $($script:gen)): $(($init.Keys | ForEach-Object { "$_ = $($init[$_])" }) -join ', ')"

# 1. diagnostic phase
$c0 = Clock; $t0 = Utc; L "diagnostic phase $DiagSec s (nothing switched)"
Start-Sleep -Seconds $DiagSec
$c1 = Clock; $t1 = Utc
$fpsD = if ($c1.q -gt $c0.q) { ($c1.f - $c0.f) * 1e6 / ($c1.q - $c0.q) } else { 0 }
Add-J ([ordered]@{ phase = "diag"; tag = "diag"; t0_utc = $t0; t1_utc = $t1; fps = [math]::Round($fpsD, 3); frames = $c1.f - $c0.f; h33 = $c1.h33 - $c0.h33; h50 = $c1.h50 - $c0.h50 })
L ("diagnostic: {0:N2} fps (mod frame clock)" -f $fpsD)

# 2. live phase
$res = @{}
foreach ($it in $Items) {
    $order = @(); for ($p = 0; $p -lt $Pairs; $p++) { $order += $(if ($p % 2 -eq 0) { @(0, 1) } else { @(1, 0) }) }
    $res[$it] = @()
    $k = 0
    foreach ($v in $order) {
        $k++
        $kv = @{}; foreach ($o in $init.Keys) { $kv[$o] = $init[$o] }; $ik = IKey $it; $kv[$ik] = IVal $it $v   # $v = arm (0 = A, 1 = B)
        $tag = "{0}_{1}{2}" -f ($it -replace '[^\w.]', '_'), $(if ($v) { "B" } else { "A" }), $k
        $a = Set-Live $kv $tag
        $ok = [int]$a.fail -eq 0 -and "$($a[$ik])" -eq "$($kv[$ik])"
        Start-Sleep -Milliseconds ([int]($WarmSec * 1000))
        $c0 = Clock; $t0 = Utc
        Start-Sleep -Seconds $WindowSec
        $c1 = Clock; $t1 = Utc
        $fps = if ($c1.q -gt $c0.q) { ($c1.f - $c0.f) * 1e6 / ($c1.q - $c0.q) } else { 0 }
        Add-J ([ordered]@{ phase = "live"; item = $it; value = $kv[$ik]; arm = $(if ($v) { "B" } else { "A" }); tag = $tag; gen = $script:gen; ack_ok = $ok; readback = "$($a[$ik])"; t0_utc = $t0; t1_utc = $t1
                fps = [math]::Round($fps, 3); frames = $c1.f - $c0.f; h33 = $c1.h33 - $c0.h33; h50 = $c1.h50 - $c0.h50 })
        $res[$it] += , @($v, $fps, $k)
        L ("  {0}: {1} = {2} (ack {3}) -> {4:N2} fps" -f $tag, $ik, $kv[$ik], $(if ($ok) { "ok" } else { "NOT OK" }), $fps)
    }
}
$null = Set-Live $init "end"
L "end: all items back to 0"

# 3. report: pairs are consecutive windows (A B | B A | ...)
$lines = @("cpu_live_sched report  $(Get-Date -f s)", ("diagnostic phase: {0:N2} fps over {1} s" -f $fpsD, $DiagSec), "")
foreach ($it in $Items) {
    $w = $res[$it]; $d = @(); $aa = @()
    for ($i = 0; $i + 1 -lt $w.Count; $i += 2) {
        $A = @($w[$i], $w[$i + 1] | Where-Object { $_[0] -eq 0 })[0]; $B = @($w[$i], $w[$i + 1] | Where-Object { $_[0] -eq 1 })[0]
        $d += $B[1] - $A[1]; $aa += $A[1]
    }
    $n = $d.Count; $m = ($d | Measure-Object -Average).Average
    $sd = if ($n -gt 1) { [math]::Sqrt((($d | ForEach-Object { ($_ - $m) * ($_ - $m) }) | Measure-Object -Sum).Sum / ($n - 1)) } else { 0 }
    $am = ($aa | Measure-Object -Average).Average
    $lines += ("{0}: B - A = {1:+0.00;-0.00} fps ({2:+0.0;-0.0} % of A {3:N2}), SE {4:N2} over {5} pairs; windows: {6}" -f $it, $m, (100 * $m / [math]::Max(0.01, $am)), $am,
             $(if ($n -gt 1) { $sd / [math]::Sqrt($n) } else { 0 }), $n, (($w | ForEach-Object { "{0}={1:N2}" -f $_[0], $_[1] }) -join " "))
}
$lines | Set-Content $rep -Encoding UTF8
$lines | ForEach-Object { L $_ }
