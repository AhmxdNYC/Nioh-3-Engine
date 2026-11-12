param([int]$OwnerPid = 0, [int]$Windows = 2, [int]$Sec = 60, [string]$Arm = "A", [string]$Dll = "", [string]$Run = "", [string]$BaseIni = "")
# pc2_cpucut_arm.ps1 - ONE launch = ONE arm of the CPU-cut test (ops #89928, arms per performance-pc #89932).
#   A = MB0 SN0 | B = MB1 | C = SN1 | D = MB1+SN1, all on the SAME test DLL (-Dll, #6879 161bd19d); S1 left at the stock value.
#   Based on pc2_aabb_arm.ps1: lock, save backup+restore, ini/config/DLL install+restore, FG OFF / REN_RAT 50 / FSR (CPU-bound),
#   MaxFps 240, title->Continue by VirtualPad, $Windows x $Sec s of a fixed move+RB loop at the save's spawn point.
#   Per window: PresentMon (fps, 1% low, median GPU busy ms) + process_cpu_window.py (process CPU s) -> results.csv.
$ErrorActionPreference = "Continue"
$repo = "C:\Users\User\projects\nioh3-engine"; $tm = "$repo\tools\testmatrix"
$GD = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GD\Nioh3EngineFix.ini"; $asi = "$GD\Nioh3EngineFix.asi"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"; $saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$stamp = Get-Date -f yyyyMMdd-HHmmss
if (-not $Run) { $Run = $stamp }
$out = "$repo\captures\pc2-cpucut-89928\$Run\$stamp-$Arm"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$saveBak = "$repo\captures\save-backups\$stamp-cpucut"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText("$out\trace.txt", $l + "`r`n"); Write-Host $l }
. "$tm\harness_guard.ps1"; . "$tm\vpad.ps1"
$uo = "C:\Users\User\projects\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "performance-pc2"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3-pc2 --ttl 7200 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$arms = @{ A = @{ MB = "0"; SN = "0" }; B = @{ MB = "1"; SN = "0" }; C = @{ MB = "0"; SN = "1" }; D = @{ MB = "1"; SN = "1" } }
if (-not $arms.ContainsKey($Arm)) { throw "arm $Arm not in A/B/C/D" }
$script:locked=$false; $script:saved=$false; $script:inst=$false; $script:dllInst=$false; $script:gp=0
function Close-Game { $p = if ($script:gp) { Get-Process -Id $script:gp -EA SilentlyContinue } else { $null }
  if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }; Start-Sleep 8 }; $script:gp = 0 }
function Restore {
  if (Test-Path "$GD\Nioh3EngineFix.log") { Copy-Item "$GD\Nioh3EngineFix.log" "$out\" -Force }
  if ($script:inst) { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force; Copy-Item "$out\orig\config.xml" $cfg -Force; $script:inst=$false; T "ini+config restored" }
  if ($script:dllInst -and -not (Get-Process Nioh3 -EA SilentlyContinue)) { Copy-Item "$out\orig\Nioh3EngineFix.asi" $asi -Force; $script:dllInst=$false; T "stock DLL restored $((Get-FileHash $asi).Hash.Substring(0,8))" }
  if ($script:saved -and -not (Get-Process Nioh3 -EA SilentlyContinue)) { robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "save restored robocopy $LASTEXITCODE"; $script:saved=$false }
  if ($script:locked) { $r = Lock "release"; $script:locked=$false; T "lock released: $($r.out)" }
}
function Set-Key([string]$t, [string]$k, [string]$v) {
  if ($t -match "(?m)^$k *=") { return ($t -replace "(?m)^$k *=.*", "$k = $v") }
  return ($t -replace "(?m)^\[Patches\]\s*$", "[Patches]`r`n$k = $v")
}
Init-Guard -Name "cpucut89928" -OwnerPid $OwnerPid -MaxMinutes 12 -Cleanup { Close-Game; Restore }
try {
  if (Get-Process Nioh3 -EA SilentlyContinue) { throw "Nioh3 running" }
  if (-not (Test-Path $Dll)) { throw "no test dll $Dll" }
  $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked=$true; T "lock: $($r.out)"
  New-Item -ItemType Directory -Force $saveBak | Out-Null; robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saved=$true
  Copy-Item $ini "$out\orig\" -Force; Copy-Item $cfg "$out\orig\" -Force; Copy-Item $asi "$out\orig\" -Force; $script:inst=$true
  Copy-Item $Dll $asi -Force; $script:dllInst=$true; T "arm $Arm test DLL $((Get-FileHash $asi).Hash.Substring(0,8)) installed (stock $((Get-FileHash "$out\orig\Nioh3EngineFix.asi").Hash.Substring(0,8)) saved)"
  $t = if ($BaseIni) { [IO.File]::ReadAllText($BaseIni) } else { [IO.File]::ReadAllText($ini) }
  if ($BaseIni) { T "base ini $BaseIni $((Get-FileHash $BaseIni).Hash.Substring(0,12))" }
  if ($t -notmatch "(?m)^\[Patches\]") { throw "no [Patches] section" }
  foreach ($kv in @(@("AutoContinueLoad","1"),@("CpuStats","1"),@("ReportSec","10"),@("MaxFps","240"))) { $t = $t -replace "(?m)^$($kv[0]) *=.*", "$($kv[0]) = $($kv[1])" }
  $t = Set-Key $t "MB" $arms[$Arm].MB; $t = Set-Key $t "SN" $arms[$Arm].SN
  $t = Force-Vpad $t
  [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false))); Copy-Item $ini "$out\test.ini"
  T "ini: $((Select-String -Path "$out\test.ini" -Pattern '^(MB|SN|S1|MA|MaxFps) *=' | % { $_.Line.Trim() }) -join ' | ')"
  $c = [IO.File]::ReadAllText($cfg)
  foreach ($kv in @(@("FRAME_GENERATION","OFF"),@("REN_RAT","50"),@("RESOLUTION_SCALING","FSR"))) { $c = $c -replace "<$($kv[0])>[^<]*</$($kv[0])>", "<$($kv[0])>$($kv[1])</$($kv[0])>" }
  [IO.File]::WriteAllText($cfg, $c, (New-Object Text.UTF8Encoding($false)))
  Assert-Launch
  Start-Process powershell -WindowStyle Hidden -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-File","C:/Users/User/projects/universal-optimizer/tools/steam/steam_autoplay.ps1","-Minutes","15"
  Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
  while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -EA SilentlyContinue | ? MainWindowHandle -ne 0 | Select -First 1; if ($p) { break }; Start-Sleep 3 }
  if (-not $p) { throw "no window" }
  $script:gp = $p.Id; Register-Game; T "game pid $($p.Id)"
  if (-not (VP-WaitPipe 180)) { throw "no vpad pipe" }
  $ok = VP-TitleContinue 300 "$out\title.txt"; T "title->field $ok"; if (-not $ok) { throw "title-continue failed" }
  Start-Sleep 20
  T "state: $(VP-State)"
  py -I -c "from PIL import ImageGrab; ImageGrab.grab().save(r'$out\spawn.png')" 2>&1 | Out-Null
  $lg = Select-String -Path "$GD\Nioh3EngineFix.log" -Pattern '(^|\W)(MB|SN)(:|\b.*(applied|patch|site|cave|on|ok))' | Select -First 8 | % { $_.Line.Trim() }
  T "log MB/SN: $($lg -join ' || ')"
  foreach ($w in 1..$Windows) {
    $tag = "$Arm-$w"; $csv = "$out\$tag.csv"
    $pm = Start-Process "$repo\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -ArgumentList "--process_name Nioh3.exe --output_file `"$csv`" --timed $Sec --terminate_after_timed --stop_existing_session --no_console_stats"
    Register-Child $pm "PresentMon"
    $cp = Start-Process py -PassThru -WindowStyle Hidden -RedirectStandardOutput "$out\$tag-cpu.out" -ArgumentList "-I `"$tm\process_cpu_window.py`" --pid $($script:gp) --seconds $Sec --out `"$out\$tag-cpu.json`" --label $tag"
    Register-Child $cp "cpu"
    $t1 = Get-Date
    while (((Get-Date) - $t1).TotalSeconds -lt $Sec) {
      foreach ($cmd in @("stick L 0 1 1500","press RB 80","wait 400","press RB 80","wait 400","stick L 1 0 1000","press RB 80","wait 400","stick L 0 -1 1500","press RB 80","wait 400","stick L -1 0 1000")) { $null = VP $cmd }
    }
    $pm.WaitForExit(30000) | Out-Null; $cp.WaitForExit(30000) | Out-Null
    $rows = Import-Csv $csv; $names = $rows[0].PSObject.Properties.Name
    $col = if ($names -contains 'MsBetweenPresents') {'MsBetweenPresents'} else {'msBetweenPresents'}
    $gcol = @('MsGPUBusy','GPUBusy','msGPUActive','MsGPUActive') | ? { $names -contains $_ } | Select -First 1
    $ms = @($rows | % { [double]$_.$col } | ? { $_ -gt 0 }); $s = ($ms | Measure -Sum).Sum / 1000.0
    $fps = [math]::Round($ms.Count / $s, 2); $srt = $ms | Sort -Descending; $k = [math]::Max(1,[int][math]::Ceiling($ms.Count*0.01))
    $low1 = [math]::Round(1000.0 / (($srt | Select -First $k | Measure -Average).Average), 2)
    $gpu = if ($gcol) { $g = @($rows | % { [double]$_.$gcol } | Sort); [math]::Round($g[[int]($g.Count/2)], 3) } else { -1 }
    $cj = if (Test-Path "$out\$tag-cpu.json") { Get-Content "$out\$tag-cpu.json" | Select -Last 1 } else { "" }
    T "WIN $tag fps=$fps low1=$low1 gpu_ms=$gpu n=$($ms.Count) cpu=$cj state=$(VP-State)"
    "$Arm,$w,$fps,$low1,$gpu,$($ms.Count),`"$($cj -replace '"',"'")`"" | Add-Content "$out\results.csv"
  }
} catch { T "!! $($_.Exception.Message)" }
finally { Close-Game; Restore; Done-Guard; T "done" }
