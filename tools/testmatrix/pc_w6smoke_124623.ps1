param([int]$OwnerPid = 0, [int]$Sec = 60, [string]$Asi = "", [string]$Run = "")
# pc_w6smoke_124623.ps1 - Main PC 60 s smoke of a candidate EF -Asi with W6 = -1 (TASK #124623); user config, MaxFps 240, PresentMon walk; restores EF + ini + saves + lock. -CfgFile = config.xml (MAX + FG on or off), -Hot = folder with the hotset.py "ghdm" set. LiveCtl Hot.ghdm 0 = stock, 1 = fast (installed), ABBA standing then moving; PresentMon fps + cpuprof cores per window; screenshots of first A/B standing. Lock, save backup, ini/config/hot restore.
#   Current installed build + the user's own ini/config (only VirtualPad forced on for title->Continue, restored after).
#   Windows: S1, S2 = standing $Sec s (no input), M1 = moving $Sec s (stick loop, no attacks).
#   Per window: patcher\cpuprof.fsx (self/inclusive/spin flags) + PresentMon fps. Lock, save backup, restore.
$ErrorActionPreference = "Continue"
$repo = "C:\Users\Ahmad Hamza\nioh3-engine"; $tm = "$repo\tools\testmatrix"
$GD = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GD\Nioh3EngineFix.ini"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"; $saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = "$repo\captures\w6smoke-124623\$(if ($Run) { $Run } else { $stamp })"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
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
  if ($script:inst) { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force; Copy-Item "$out\orig\Nioh3EngineFix.asi" "$GD\Nioh3EngineFix.asi" -Force; T "EF restored $((Get-FileHash "$GD\Nioh3EngineFix.asi").Hash.Substring(0,8))"; $script:inst=$false; T "ini+config restored $((Get-FileHash $ini).Hash.Substring(0,8))" }
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
$ctl = "$GD\Nioh3EngineFix_live.ini"; $ack = "$ctl.ack"; $hotDir = "$GD\Nioh3EngineFix_hot"; $script:gen = 0
function Live([string]$v, [string]$tag) {
  $script:gen++; $tmp = "$ctl.w"
  [IO.File]::WriteAllText($tmp, "[Live]`r`nGen = $($script:gen)`r`nTag = $tag`r`nHot.ghdm = $v`r`n", [Text.Encoding]::ASCII); Move-Item -LiteralPath $tmp -Destination $ctl -Force
  $t0 = Get-Date
  while (((Get-Date) - $t0).TotalSeconds -lt 8) {
    $h = @{}; try { foreach ($kv in ([IO.File]::ReadAllText($ack).Trim() -split '\s+')) { if ($kv -match '^([^=]+)=(.*)$') { $h[$Matches[1]] = $Matches[2] } } } catch {}
    if ($h.gen -eq "$($script:gen)") { return "gen=$($h.gen) ok=$($h.ok) fail=$($h.fail) Hot.ghdm=$($h['Hot.ghdm'])" }
    Start-Sleep -Milliseconds 100 }
  return "NOACK"
}
. "$repo\tools\playsession\session_lib.ps1"
Init-Guard -Name "w6smoke124623" -OwnerPid $OwnerPid -MaxMinutes 14 -Cleanup { Close-Game; Restore }
try {
  if (Get-Process Nioh3 -EA SilentlyContinue) { throw "Nioh3 running" }
  $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked=$true; T "lock: $($r.out)"
  New-Item -ItemType Directory -Force $saveBak | Out-Null; robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saved=$true
  Copy-Item $ini "$out\orig\" -Force; Copy-Item $cfg "$out\orig\" -Force; Copy-Item "$GD\Nioh3EngineFix.asi" "$out\orig\" -Force; $script:inst=$true
  T "orig EF $((Get-FileHash "$out\orig\Nioh3EngineFix.asi").Hash.Substring(0,8))"
  Copy-Item $Asi "$GD\Nioh3EngineFix.asi" -Force; T "installed EF $((Get-FileHash "$GD\Nioh3EngineFix.asi").Hash.Substring(0,8))"
  foreach ($f in "Nioh3Training.asi","Nioh3BossRematch.asi","Nioh3CamKiProbe.asi") { if (Test-Path "$GD\$f") { T "$f $((Get-FileHash "$GD\$f").Hash.Substring(0,8))" } }
  $lines = (Force-Vpad ([IO.File]::ReadAllText($ini))) -split "`r?`n"
  foreach ($k in @(@("Patches","W6","-1"),@("VRR","MaxFps","240"))) { $lines = Set-IniValue $lines $k[0] $k[1] $k[2] }
  [IO.File]::WriteAllText($ini, ($lines -join "`r`n"), (New-Object Text.UTF8Encoding($false)))
  $c = [IO.File]::ReadAllText($cfg)
  T "config: $((@('FRAME_GENERATION','REN_RAT','RESOLUTION_SCALING','REN_FPS','VSYNC','SHADOW_QUAL','GI','SCR_REZ') | % { if ($c -match "<$_>([^<]*)</$_>") { "$_=$($Matches[1])" } }) -join ' ')"
  T "ini: $((Select-String -Path $ini -Pattern '^(MaxFps|VirtualPad|W6|LoadOnlyCpuFixes) *=' | % { $_.Line.Trim() }) -join ' | ')"
  Assert-Launch
  Start-Process powershell -WindowStyle Hidden -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-File","`"C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1`"","-Minutes","15"
  Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
  while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -EA SilentlyContinue | ? MainWindowHandle -ne 0 | Select -First 1; if ($p) { break }; Start-Sleep 3 }
  if (-not $p) { throw "no window" }
  $script:gp = $p.Id; Register-Game; T "game pid $($p.Id)"
  if (-not (VP-WaitPipe 180)) { throw "no vpad pipe" }
  $ok = VP-TitleContinue 300 "$out\title.txt"; T "title->field $ok"; if (-not $ok) { throw "title-continue failed" }
  Start-Sleep 20
  if (-not ("U.W" -as [type])) { Add-Type -Name W -Namespace U -MemberDefinition '[DllImport("user32.dll")] public static extern bool ShowWindow(System.IntPtr h, int c); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(System.IntPtr h);' }
  Get-Process steamwebhelper, steam -EA SilentlyContinue | ? { $_.MainWindowHandle -ne 0 -and $_.MainWindowTitle } | % { $null = [U.W]::ShowWindow($_.MainWindowHandle, 6) }
  $null = [U.W]::SetForegroundWindow($p.MainWindowHandle); Start-Sleep 3
  T "state: $(VP-State)"
  $csv = "$out\walk-pm.csv"
  $pm = Start-Process "$repo\tools\PresentMon.exe" -PassThru -WindowStyle Hidden -RedirectStandardError "$out\pm.err" -RedirectStandardOutput "$out\pm.out" -ArgumentList "--process_id $($script:gp) --output_file `"$csv`" --timed $Sec --terminate_after_timed --stop_existing_session --no_console_stats"
  Register-Child $pm "PresentMon"
  $t1 = Get-Date
  while (((Get-Date) - $t1).TotalSeconds -lt ($Sec + 2)) { foreach ($cmd in @("stick L 0 1 1500","stick L 1 0 1000","stick L 0 -1 1500","stick L -1 0 1000")) { $null = VP $cmd } }
  $pm.WaitForExit(30000) | Out-Null
  T "WALK $(Fps $csv) alive=$([bool](Get-Process -Id $script:gp -EA SilentlyContinue))"
  $L = "$GD\Nioh3EngineFix.log"
  T "log auto: $((Select-String -Path $L -Pattern 'auto W6|W6:' | Select -First 3 | % { $_.Line.Trim() }) -join ' || ')"
  T "log exp: $((Select-String -Path $L -Pattern 'stats/s exp: W6' | Select -Last 2 | % { $_.Line.Trim() }) -join ' || ')"
} catch { T "!! $($_.Exception.Message)" }
finally { Close-Game; Restore; Done-Guard; T "done" }
