param([int]$OwnerPid = 0, [int]$Windows = 2, [int]$Sec = 60, [string]$Arm = "O", [string]$Run = "", [switch]$KeepLock)
# pc2_sbm_arm.ps1 - ONE launch = ONE arm of the pc2 SceneBufMirror GPU/PCIe A/B (#96044). Copy of pc2_pcm_arm.ps1.
#   O = SceneBufMirror 0 (stock 1.8.13)  S = SceneBufMirror 1 (auto, no-ReBAR) on its own COPY queue.
#   P / Q = profile arms (O / S + SceneBufProbe 1, GPUProf 2, MemStats 2): pass + scene-buffer census, not for fps. User settings: FSR, REN_RAT 65, FG OFF, MaxFps 240.
#   Lock, save backup+restore, EF ini/config restore, title->Continue by VirtualPad, game-window spawn shot (Steam minimized),
#   $Windows x $Sec s of a fixed move+RB loop. PresentMon + process CPU per window -> results.csv.
$ErrorActionPreference = "Continue"
$repo = "C:\Users\User\projects\nioh3-engine"; $tm = "$repo\tools\testmatrix"
$GD = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GD\Nioh3EngineFix.ini"; $asi = "$GD\Nioh3EngineFix.asi"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"; $saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$stamp = Get-Date -f yyyyMMdd-HHmmss
if (-not $Run) { $Run = $stamp }
$out = "$repo\captures\pc2-sbm-96044\$Run\$stamp-$Arm"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$saveBak = "$repo\captures\save-backups\$stamp-sbm"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText("$out\trace.txt", $l + "`r`n"); Write-Host $l }
. "$tm\harness_guard.ps1"; . "$tm\vpad.ps1"
$uo = "C:\Users\User\projects\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "performance-pc2"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3-pc2 --ttl 7200 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$base = @(@("SceneBufMirrorQueue","1"),@("SceneBufProbe","0"),@("GPUProf","0"),@("MemStats","0"))
$arms = @{ O = @(,@("SceneBufMirror","0")); S = @(,@("SceneBufMirror","1"))
  P = @(@("SceneBufMirror","0"),@("SceneBufProbe","1"),@("GPUProf","2"),@("MemStats","2")); Q = @(@("SceneBufMirror","1"),@("SceneBufProbe","1"),@("GPUProf","2"),@("MemStats","2")) }
if (-not $arms.ContainsKey($Arm)) { throw "arm $Arm not in O/S/P/Q" }
$script:optiPut=@()
$script:locked=$false; $script:saved=$false; $script:inst=$false; $script:optiPut=@(); $script:gp=0
function Close-Game { $p = if ($script:gp) { Get-Process -Id $script:gp -EA SilentlyContinue } else { $null }
  if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }; Start-Sleep 8 }; $script:gp = 0 }
function Restore {
  if (Test-Path "$GD\Nioh3EngineFix.log") { Copy-Item "$GD\Nioh3EngineFix.log" "$out\" -Force }
  if ($script:inst) { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force; Copy-Item "$out\orig\config.xml" $cfg -Force; $script:inst=$false; T "EF ini + config.xml restored" }
  if ($script:optiPut.Count -and -not (Get-Process Nioh3 -EA SilentlyContinue)) {
    foreach ($f in @(Get-ChildItem $GD -Filter "OptiScaler*.log" -EA SilentlyContinue)) { Copy-Item $f.FullName "$out\" -Force; Remove-Item $f.FullName -Force }
    foreach ($d in $script:optiPut) { Remove-Item (Join-Path $GD $d) -Force -EA SilentlyContinue }
    T "OptiScaler removed: $($script:optiPut -join ',')"; $script:optiPut = @() }
  if ($script:saved -and -not (Get-Process Nioh3 -EA SilentlyContinue)) { robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "save restored robocopy $LASTEXITCODE"; $script:saved=$false }
  if ($script:locked -and -not $KeepLock) { $r = Lock "release"; $script:locked=$false; T "lock released: $($r.out)" }
}
Init-Guard -Name "sbm96044" -OwnerPid $OwnerPid -MaxMinutes 12 -Cleanup { Close-Game; Restore }
try {
  if (Get-Process Nioh3 -EA SilentlyContinue) { throw "Nioh3 running" }
  $r = Lock "acquire"; if ($r.code -ne 0 -and $r.out -match "cannot reach bridge") { T "bridge down: local lock only" } elseif ($r.code -ne 0) { throw "lock: $($r.out)" } else { $script:locked=$true; T "lock: $($r.out)" }
  New-Item -ItemType Directory -Force $saveBak | Out-Null; robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saved=$true
  Copy-Item $ini "$out\orig\" -Force; Copy-Item $cfg "$out\orig\" -Force; $script:inst=$true
  T "arm $Arm EF $((Get-FileHash $asi).Hash.Substring(0,8)) $(($arms[$Arm] | % { $_ -join '=' }) -join ' ')"
  $t = [IO.File]::ReadAllText($ini)
  foreach ($kv in (@(@("AutoContinueLoad","1"),@("CpuStats","1"),@("ReportSec","10"),@("MaxFps","240"),@("PCM","0")) + $base + $arms[$Arm])) { $t = $t -replace "(?m)^$($kv[0]) *=.*", "$($kv[0]) = $($kv[1])" }
  $t = Force-Vpad $t
  [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false))); Copy-Item $ini "$out\test.ini"
  $c = [IO.File]::ReadAllText($cfg)
  foreach ($kv in @(@("FRAME_GENERATION","OFF"),@("REN_RAT","65"),@("RESOLUTION_SCALING","FSR"))) { $c = $c -replace "<$($kv[0])>[^<]*</$($kv[0])>", "<$($kv[0])>$($kv[1])</$($kv[0])>" }
  [IO.File]::WriteAllText($cfg, $c, (New-Object Text.UTF8Encoding($false)))
  T "ini: $((Select-String -Path $ini -Pattern '^(PCM|SceneBufMirror|SceneBufMirrorQueue|SceneBufProbe|GPUProf|MemStats) *=' | % { $_.Line.Trim() }) -join ' | ')"
  T "config: $((Select-String -Path $cfg -Pattern '<(FRAME_GENERATION|REN_RAT|RESOLUTION_SCALING|REN_FPS)>' -AllMatches | % { $_.Line.Trim() }) -join ' ')"
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
  # #95578: same scene per arm - minimize Steam windows, game to front, grab only the game window, before any movement
  T "shot: $(py -I "$tm\shot_game.py" $script:gp "$out\spawn.png" 2>&1)"
  $ol = Get-ChildItem $GD -Filter "OptiScaler*.log" -EA SilentlyContinue | Select -First 1
  if ($ol) { $h = Select-String -Path $ol.FullName -Pattern 'FSR4|FSR 4|Int8|INT8|FSR3|FSR 3|FfxApi|XeSS|upscaler' | Select -First 15 | % { $_.Line.Trim() }; T "opti log: $($h -join ' || ')" }
  
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
    $fps = [math]::Round($ms.Count / $s, 2); $srt = @($ms | Sort -Descending); $k = [math]::Max(1,[int][math]::Ceiling($ms.Count*0.01))
    $low1 = [math]::Round(1000.0 / (($srt | Select -First $k | Measure -Average).Average), 2); $p99 = [math]::Round($srt[$k-1], 2)
    $gpu = if ($gcol) { $g = @($rows | % { [double]$_.$gcol } | Sort); [math]::Round($g[[int]($g.Count/2)], 3) } else { -1 }
    $cj = if (Test-Path "$out\$tag-cpu.json") { Get-Content "$out\$tag-cpu.json" | Select -Last 1 } else { "" }
    T "WIN $tag fps=$fps low1=$low1 p99ms=$p99 gpu_ms=$gpu n=$($ms.Count) cpu=$cj state=$(VP-State)"
    "$Arm,$w,$fps,$low1,$p99,$gpu,$($ms.Count),`"$($cj -replace '"',"'")`"" | Add-Content "$out\results.csv"
  }
} catch { T "!! $($_.Exception.Message)" }
finally { Close-Game; Restore; Done-Guard; T "done" }
