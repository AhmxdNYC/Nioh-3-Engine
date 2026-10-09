<#
  test_arm_ini.ps1 - selftest for the per-arm ini isolation in pc_gpu_session.ps1 / pc2_pc_gpu_session.ps1 (bridge #988).
  Pulls Get-PristineLines/Get-IniMap/Get-ArmKeys/Test-ArmIni/Write-ArmIni out of the session script (AST, nothing launched), runs every
  arm in plan order against a temp ini, and checks: BASE has none of any item's keys; each arm differs from pristine only by its own keys;
  the guard fires on a deliberately contaminated ini. Run: powershell -NoProfile -File tools\testmatrix\test_arm_ini.ps1 [-Script pc2_pc_gpu_session.ps1]
#>
param([string]$Script = "pc_gpu_session.ps1")
$ErrorActionPreference = "Stop"
$path = Join-Path $PSScriptRoot $Script
$ast = [Management.Automation.Language.Parser]::ParseFile($path, [ref]$null, [ref]$null)
$want = "Get-PristineLines","Get-IniMap","Get-ArmKeys","Test-ArmIni","Write-ArmIni"
foreach ($f in $ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $want -contains $n.Name }, $true)) { . ([scriptblock]::Create($f.Extent.Text)) }
$asiAst = $ast.Find({ param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Left.Extent.Text -eq '$AsiItems' }, $true)
$AsiItems = & ([scriptblock]::Create($asiAst.Right.Extent.Text))
. (Join-Path $PSScriptRoot "..\playsession\session_lib.ps1") 2>$null
if (-not (Get-Command Set-IniValue -ErrorAction SilentlyContinue)) { throw "Set-IniValue not available" }
$tmp = Join-Path $env:TEMP "armini_selftest"; New-Item -ItemType Directory -Force $tmp | Out-Null
$orig = $tmp; $GP2MinRun = 3; $ini = Join-Path $tmp "live.ini"; $BaseIniSrc = Join-Path $tmp "user.ini"
function L([string]$s, [string]$c = "Gray") { }
Set-Content $BaseIniSrc "[General]`nAutoContinueLoad = 0`n[GpuOpt]`nCbvMirror = 0`n[Patches]`nAS = 0" -Encoding ASCII
$fail = 0
$plan = @("BASE") + @($AsiItems.Keys | Sort-Object) + @("BASE", "cmforce", "BASE", "cm", "BASE")
foreach ($arm in $plan) {
    Write-ArmIni $arm
    $m = Get-IniMap ([IO.File]::ReadAllLines($ini))
    if ($arm -eq "BASE") {
        if ($m["GpuOpt.CbvMirror"] -ne "0") { Write-Host "FAIL BASE CbvMirror=$($m['GpuOpt.CbvMirror'])"; $fail++ } else { Write-Host "ok   BASE clean (CbvMirror=0, AS=$($m['Patches.AS']))" }
    } else { Write-Host "ok   $arm written + guard passed" }
}
# the guard must fire on a contaminated ini
$dirty = [string[]]([IO.File]::ReadAllLines($BaseIniSrc) + @("[GpuOpt]", "CbvMirror = 2"))
$pr = [string[]][IO.File]::ReadAllLines($BaseIniSrc)
$lines = $pr; foreach ($kv in (Get-ArmKeys "BASE")) { if ($kv -match '^([^.=]+)\.([^=]+)=(.*)$') { $lines = Set-IniValue $lines $Matches[1] $Matches[2] $Matches[3] } }
$lines = Set-IniValue $lines "GpuOpt" "CbvMirror" "2"
try { Test-ArmIni "BASE" ([string[]]$lines) $pr; Write-Host "FAIL guard did not fire"; $fail++ } catch { Write-Host "ok   guard fired: $($_.Exception.Message.Substring(0, [Math]::Min(110, $_.Exception.Message.Length)))" }
if ($fail) { Write-Host "SELFTEST FAILED ($fail)"; exit 1 } else { Write-Host "SELFTEST OK ($Script)" }
