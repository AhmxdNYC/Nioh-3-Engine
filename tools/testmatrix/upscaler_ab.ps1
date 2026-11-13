<#
  upscaler_ab.ps1 - relaunch A/B of UPSCALER arms (task #95420, open.md "better upscaler at lower internal res").
  Each arm = one abba2_pc.ps1 launch (-Arms BASE -Live none) with its own config.xml RESOLUTION_SCALING/REN_RAT and its own
  OptiScaler.ini [FSR] keys. Plan and history: notes/upscaler-swap-re.md section 5 + notes/upscaler-candidate-95420.md.
  Arm syntax  NAME:UPSCALER:RENRAT:OPTI   OPTI = off (game FSR3.1 fork, OptiScaler idle) | fsr4 (Fsr4Update=true) |
                                                 int8 (Fsr4Update=true + Fsr4ForceEnableInt8=true, RDNA2/3)
  Default (Main PC, RDNA4): F3:FSR:75:off, F4:XESS:75:fsr4, F4lo:XESS:65:fsr4 in order F3 F4 F4lo F4lo F4 F3.
  OptiScaler logging is switched on per arm (LogLevel 1) and the lines naming the upscaler are copied to arm.log, so every
  arm PROVES which upscaler ran (the Fsr4Update=false since 10-24 question). config.xml + OptiScaler.ini are backed up once
  and restored at the end (also on error). Caller holds the game lock (abba2 renews it).
  Run:  powershell -NoProfile -ExecutionPolicy Bypass -File tools\testmatrix\upscaler_ab.ps1 -Tag up1   [-DryRun]
#>
param(
    [string[]]$Arms = @("F3:FSR:75:off", "F4:XESS:75:fsr4", "F4lo:XESS:65:fsr4"),
    [string]$Order = "F3,F4,F4lo,F4lo,F4,F3",
    [string]$Tag = "upscaler", [int]$WindowSec = 20, [int]$MaxFps = 240, [int]$OwnerPid = 0, [int]$MaxMinutes = 40,
    [string]$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3",
    [string]$SaveRoot = (Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3"),
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$abba = Join-Path $PSScriptRoot "abba2_pc.ps1"
$out = Join-Path (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path ("captures\pc\upscaler\" + (Get-Date -Format "yyyyMMdd-HHmm") + "-$Tag")
New-Item -ItemType Directory -Force $out | Out-Null
$log = Join-Path $out "upscaler_ab.log"
function L($m) { $l = "$(Get-Date -Format HH:mm:ss) $m"; Add-Content $log $l; Write-Host $l }

$def = @{}
foreach ($a in ($Arms -split ",")) {
    $p = $a.Trim() -split ":"
    if ($p.Count -ne 4 -or $p[1] -notin @("FSR", "XESS") -or $p[2] -notmatch '^\d+$' -or [int]$p[2] % 5 -or $p[3] -notin @("off", "fsr4", "int8")) {
        throw "bad arm '$a' (NAME:FSR|XESS:RENRAT(step 5):off|fsr4|int8)" }
    if ($p[1] -eq "XESS" -and $p[3] -eq "off") { throw "arm $($p[0]): XESS with OptiScaler off measures Intel XeSS, not FSR4" }
    $def[$p[0]] = @{ up = $p[1]; rr = [int]$p[2]; opti = $p[3] }
}
$seq = @($Order -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ })
foreach ($s in $seq) { if (-not $def[$s]) { throw "order names unknown arm $s" } }

$opti = Join-Path $GameDir "OptiScaler.ini"; $cfg = Join-Path $SaveRoot "config.xml"
if (-not $DryRun) {
    if (-not (Test-Path (Join-Path $GameDir "dxgi.dll"))) { throw "OptiScaler (dxgi.dll) not installed: run optiscaler\apply.ps1 first" }
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 is running - close it (by exact PID, after the leftovers check) first" }
    Copy-Item $opti (Join-Path $out "OptiScaler.ini.orig") -Force
    Copy-Item $cfg (Join-Path $out "config.xml.orig") -Force
}
function Set-Ini([string]$path, [hashtable]$kv) {
    $t = [IO.File]::ReadAllText($path)
    foreach ($k in $kv.Keys) {
        if ($t -notmatch "(?m)^$k=") { throw "$path has no $k=" }
        $t = [regex]::Replace($t, "(?m)^$k=.*$", "$k=$($kv[$k])")
    }
    [IO.File]::WriteAllText($path, $t, (New-Object Text.UTF8Encoding $true))
}
function Restore {
    if ($DryRun) { return }
    Copy-Item (Join-Path $out "OptiScaler.ini.orig") $opti -Force
    Copy-Item (Join-Path $out "config.xml.orig") $cfg -Force
    L "restored OptiScaler.ini + config.xml"
}
try {
    if (-not $DryRun) {   # abba2 refuses REN_FPS < 120; it restores config.xml to this state, Restore puts the user's back
        $c = [IO.File]::ReadAllText($cfg)
        if ($c -match "<REN_FPS>(\d+)<" -and [int]$Matches[1] -lt 120) { $c = $c -replace "<REN_FPS>\d+<", "<REN_FPS>120<"; [IO.File]::WriteAllText($cfg, $c); L "REN_FPS -> 120 for the run" }
    }
    $L = 0
    foreach ($s in $seq) {
        $L++; $a = $def[$s]
        $kv = @{ Fsr4Update = $(if ($a.opti -eq "off") { "false" } else { "true" })
                 Fsr4ForceEnableInt8 = $(if ($a.opti -eq "int8") { "true" } else { "auto" })
                 LogLevel = "2"; LogToFile = "true" }   # 2 = info: 1 (debug) logs ~20 lines per frame, ~1.8 ms CPU (#95480)
        L "L$L arm $s : RESOLUTION_SCALING=$($a.up) REN_RAT=$($a.rr) OptiScaler $($a.opti) ($(($kv.Keys | Sort-Object | ForEach-Object { "$_=$($kv[$_])" }) -join ' '))"
        if (-not $DryRun) {
            Set-Ini $opti $kv
            Get-ChildItem $GameDir -Filter "OptiScaler*.log" -ErrorAction SilentlyContinue | Remove-Item -Force
        }
        $args2 = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $abba, "-Arms", "BASE", "-Live", "none", "-LaunchFlags", "none",
                   "-RenRat", $a.rr, "-Upscaler", $a.up, "-WindowSec", $WindowSec, "-MaxFps", $MaxFps, "-Input", "vpad",
                   "-RunName", "$Tag-L$L-$s", "-MaxMinutes", $MaxMinutes, "-OwnerPid", $(if ($OwnerPid) { $OwnerPid } else { $PID }))
        if ($DryRun) { $args2 += "-DryRun" }
        $ErrorActionPreference = "Continue"   # abba2's python children write to stderr; do not turn that into a throw
        & powershell @args2 *> (Join-Path $out "L$L-$s.abba2.txt"); $rc = $LASTEXITCODE
        $ErrorActionPreference = "Stop"
        L "L$L arm $s abba2 exit $rc"
        if ($rc -ne 0) { throw "abba2 L$L $s failed (exit $rc) - see L$L-$s.abba2.txt" }
        $ol = Get-ChildItem $GameDir -Filter "OptiScaler*.log" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($ol) {
            $hit = Select-String -Path $ol.FullName -Pattern "FSR4|FSR 4|Int8|INT8|FSR3|FSR 3|FfxApi|upscaler|XeSS" -SimpleMatch:$false | Select-Object -First 40
            ($hit | ForEach-Object { $_.Line }) | Set-Content (Join-Path $out "L$L-$s.opti.txt")
            L ("L$L arm $s OptiScaler: " + (($hit | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join " | "))
        } else { L "L$L arm $s OptiScaler: no log (expected for an off arm; for fsr4/int8 arms = OptiScaler never ran, arm INVALID)" }
    }
} finally { Restore }
L "done; abba2 outputs: captures\pc\abba2\*$Tag-L*; compare arm fps / GPU busy with abba2_report.py per run"
if ($DryRun) { L "DryRun OK" }
