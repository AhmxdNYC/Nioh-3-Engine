param([int]$OwnerPid = 0, [string]$Run = "r1", [string]$Order = "P,O,S,S,O,Q")
# Runs pc2_sbm_arm.ps1 once per arm (ABBA), holds the game lock across arms (-KeepLock), releases once at the end.
$tm = "C:\Users\User\projects\nioh3-engine\tools\testmatrix"
foreach ($a in ($Order -split ",")) {
  if ($OwnerPid -and -not (Get-Process -Id $OwnerPid -EA SilentlyContinue)) { Write-Host "owner gone, stop"; break }
  Write-Host "[$(Get-Date -f HH:mm:ss)] SERIES arm $a start"
  $o = & powershell -NoProfile -ExecutionPolicy Bypass -File "$tm\pc2_sbm_arm.ps1" -Arm $a -Run $Run -OwnerPid $OwnerPid -KeepLock 2>&1 | Out-String
  Write-Host $o
  if ($o -match "!!") { Write-Host "SERIES abort after arm $a"; break }
  Start-Sleep 10
}
Push-Location "C:\Users\User\projects\universal-optimizer\agent-bridge"; $env:UO_AGENT_ID = "performance-pc2"; py -m uo_bridge.gamelock release nioh3-pc2 2>&1 | Out-Host; Pop-Location
Write-Host "[$(Get-Date -f HH:mm:ss)] SERIES end"
