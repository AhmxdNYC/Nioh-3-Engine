param([string]$Ini="fullmod_ini_1716.ini",[string[]]$Variants,[int]$OwnerPid=0,[int]$MaxMinutes=90)
. "C:\Users\Ahmad Hamza\nioh3-engine\patcher\common.ps1"
. "$PSScriptRoot\harness_guard.ps1"
Init-Guard -Name "loadstats_ab" -OwnerPid $OwnerPid -MaxMinutes $MaxMinutes -Cleanup { Close-Game; Copy-Item "$ws\tools\testmatrix\fullmod_ini_1716.ini" "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3EngineFix.ini" -Force }
$g="C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $base="$ws\tools\testmatrix\$Ini"
$out="$ws\captures\pc\loadstats"; New-Item -ItemType Directory -Force $out | Out-Null
foreach($v in $Variants){
  $name=$v.Split('_')[0]
  $t=[IO.File]::ReadAllText($base)
  if($Ini -notmatch '_lg'){ $t=$t -replace '(?m)^LoadStats = \d','LoadStats = 1' }
  foreach($k in 'GL1','LC','LW'){ $t=$t -replace "(?m)^$k = \d","$k = $(if($name -eq $k){1}else{0})" }
  $t = Force-Acl $t   # off by default since 1.7.33
  [IO.File]::WriteAllText("$g\Nioh3EngineFix.ini",$t,(New-Object Text.UTF8Encoding($false)))
  Close-Game
  Assert-Launch
  & (Join-Path $PSScriptRoot '../steam/start_autoplay.ps1')
  Start-Process "steam://rungameid/3681010"
  $t0=Get-Date
  while (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0) -and ((Get-Date) - $t0).TotalSeconds -lt 180) { $cc = "$(& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'steam_cloud_click.ps1') 2>$null | Select-Object -First 1)"; if ($cc -like 'clicked*') { Write-Host "[launch] Steam cloud dialog: $cc" }; Start-Sleep 3 }   # steam_cloud_click: 'Unable to Sync' -> Play anyway (user OK, bridge #602)
  Register-Game; if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { Note-Crash "no game window after launch" }
  Start-Sleep 25
  U focus Nioh3; U click 1280 887; Start-Sleep 22
  U focus Nioh3; U click 1280 1170; Start-Sleep 7
  U focus Nioh3; U click 380 1024; Start-Sleep 25
  U focus Nioh3; U key 20; Start-Sleep 5; U key 20; Start-Sleep 4
  "$name ingame=" + (Ensure-InGame) + " boot=" + [int]((Get-Date)-$t0).TotalSeconds
  Start-Sleep 15
  Copy-Item "$g\Nioh3EngineFix.log" "$out\loadstats_$v.log" -Force
  Close-Game
}
Copy-Item "$ws\tools\testmatrix\fullmod_ini_1716.ini" "$g\Nioh3EngineFix.ini" -Force
Done-Guard
