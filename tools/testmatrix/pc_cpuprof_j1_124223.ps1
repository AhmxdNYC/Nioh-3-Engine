param([int]$OwnerPid = 0, [int]$Sec = 30, [int]$J1 = 0, [string]$Run = "")
# pc_cpuprof_j1_124223.ps1 - Main PC CPU sampler A/B arm: J1 forced to $J1 (TASK #124223). Measure only. Copy of pc_cpuprof_124164.ps1.
#   Current installed build + the user's own ini/config (only VirtualPad forced on for title->Continue, restored after).
#   Windows: S1, S2 = standing $Sec s (no input), M1 = moving $Sec s (stick loop, no attacks).
#   Per window: patcher\cpuprof.fsx (self/inclusive/spin flags) + PresentMon fps. Lock, save backup, restore.
$ErrorActionPreference = "Continue"
$repo = "C:\Users\Ahmad Hamza\nioh3-engine"; $tm = "$repo\tools\testmatrix"
$GD = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GD\Nioh3EngineFix.ini"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"; $saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = "$repo\captures\cpuprof-124223\$(if ($Run) { $Run } else { $stamp })\J1-$J1"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$saveBak = "$repo\captures\save-backups\$stamp-cpuprof"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText("$out\trace.txt", $l + "`r`n"); Write-Host $l }
. "$tm\harness_guard.ps1"; . "$tm\vpad.ps1"
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "performance-pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
$script:locked=$false; $script:saved=$false; $script:inst=$false; $script:gp=0
function Close-Game { $p = if ($script:gp) { Get-Process -Id $script:gp -EA SilentlyContinue } else { $null }
  if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }; Start-Sleep 8 }; $script:gp = 0 }
function Restore {
  if (Test-Path "$GD\Nioh3EngineFix.log") { Copy-Item "$GD\Nioh3EngineFix.log" "$out\" -Force }
  if ($script:inst) { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force; Copy-Item "$out\orig\config.xml" $cfg -Force; $script:inst=$false; T "ini+config restored $((Get-FileHash $ini).Hash.Substring(0,8))" }
  if ($script:saved -and -not (Get-Process Nioh3 -EA SilentlyContinue)) { robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "save restored robocopy $LASTEXITCODE"; $script:saved=$false }
  if ($script:locked) { $r = Lock "release"; $script:locked=$false; T "lock released: $($r.out)" }
}
function Fps([string]$csv) {
  if (-not (Test-Path $csv)) { return "nocsv" }
  $rows = Import-Csv $csv; if (-not $rows) { return "empty" }; $names = $rows[0].PSObject.Properties.Name
  $col = if ($names -contains 'MsBetweenPresents') {'MsBetweenPresents'} else {'msBetweenPresents'}
  $ms = @($rows | % { [double]$_.$col } | ? { $_ -gt 0 }); $s = ($ms | Measure -Sum).Sum / 1000.0
  $srt = $ms | Sort -Descending; $k = [math]::Max(1,[int][math]::Ceiling($ms.Count*0.01))
  "fps={0:N1} low1={1:N1} n={2}" -f ($ms.Count / $s), (1000.0 / (($srt | Select -First $k | Measure -Average).Average)), $ms.Count
}
Init-Guard -Name "cpuprof124223" -OwnerPid $OwnerPid -MaxMinutes 14 -Cleanup { Close-Game; Restore }
try {
  if (Get-Process Nioh3 -EA SilentlyContinue) { throw "Nioh3 running" }
  $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked=$true; T "lock: $($r.out)"
  New-Item -ItemType Directory -Force $saveBak | Out-Null; robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saved=$true
  Copy-Item $ini "$out\orig\" -Force; Copy-Item $cfg "$out\orig\" -Force; $script:inst=$true
  foreach ($f in "Nioh3EngineFix.asi","Nioh3Training.asi","Nioh3BossRematch.asi","Nioh3CamKiProbe.asi") { if (Test-Path "$GD\$f") { T "$f $((Get-FileHash "$GD\$f").Hash.Substring(0,8))" } }
  $t = Force-Vpad ([IO.File]::ReadAllText($ini)); $t = [regex]::Replace($t, '(?m)^J1 *=[^\r\n]*', "J1 =$J1")
  [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false)))
  $c = [IO.File]::ReadAllText($cfg)
  T "config: $((@('FRAME_GENERATION','REN_RAT','RESOLUTION_SCALING','REN_FPS','VSYNC','RESOLUTION') | % { if ($c -match "<$_>([^<]*)</$_>") { "$_=$($Matches[1])" } }) -join ' ')"
  T "ini: $((Select-String -Path $ini -Pattern '^(MaxFps|VirtualPad|CpuStats|FileLinger|FWP|VrsCeiling|J1|W6|XHALF\w*) *=' | % { $_.Line.Trim() }) -join ' | ')"
  Assert-Launch
  Start-Process powershell -WindowStyle Hidden -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-File","`"C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1`"","-Minutes","15"
  Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
  while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -EA SilentlyContinue | ? MainWindowHandle -ne 0 | Select -First 1; if ($p) { break }; Start-Sleep 3 }
  if (-not $p) { throw "no window" }
  $script:gp = $p.Id; Register-Game; T "game pid $($p.Id)"
  if (-not (VP-WaitPipe 180)) { throw "no vpad pipe" }
  $ok = VP-TitleContinue 300 "$out\title.txt"; T "title->field $ok"; if (-not $ok) { throw "title-continue failed" }
  Start-Sleep 20
  T "state: $(VP-State)"
  py -I -c "from PIL import ImageGrab; ImageGrab.grab().save(r'$out\spawn.png')" 2>&1 | Out-Null
  foreach ($w in "S1","S2","M1") {
    $csv = "$out\$w-pm.csv"
    $pm = Start-Process "$repo\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -RedirectStandardError "$out\$w-pm.err" -RedirectStandardOutput "$out\$w-pm.out" -ArgumentList "--process_id $($script:gp) --output_file `"$csv`" --timed $Sec --terminate_after_timed --stop_existing_session --no_console_stats"
    Register-Child $pm "PresentMon"
    $cp = Start-Process dotnet -PassThru -WindowStyle Hidden -WorkingDirectory "$repo\patcher" -RedirectStandardOutput "$out\$w-prof.out" -RedirectStandardError "$out\$w-prof.err" -ArgumentList "fsi","cpuprof.fsx","$Sec","`"$out\cpuprof-$w.txt`""
    Register-Child $cp "cpuprof"
    $t1 = Get-Date
    while (-not $cp.HasExited -and ((Get-Date) - $t1).TotalSeconds -lt ($Sec + 90)) {
      if ($w -like "M*") { foreach ($cmd in @("stick L 0 1 1500","stick L 1 0 1000","stick L 0 -1 1500","stick L -1 0 1000")) { $null = VP $cmd } } else { Start-Sleep 1 }
    }
    if (-not $cp.HasExited) { T "cpuprof $w overran, stopping"; Stop-Process -Id $cp.Id -Force }
    $pm.WaitForExit(30000) | Out-Null
    T "WIN $w $(Fps $csv) prof=$(Test-Path "$out\cpuprof-$w.txt") state=$(VP-State)"
  }
} catch { T "!! $($_.Exception.Message)" }
finally { Close-Game; Restore; Done-Guard; T "done" }
