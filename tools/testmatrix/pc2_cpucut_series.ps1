param([int]$OwnerPid = 0, [string]$Dll = "", [string]$Run = "r1", [string]$Order = "A,B,B,A,C,C,A,D,D,A", [string]$BaseIni = "")
# Runs pc2_cpucut_arm.ps1 once per arm (adjacent ABBA vs A, shared A launches). Stops at the first arm that logs '!!'.
$tm = "C:\Users\User\projects\nioh3-engine\tools\testmatrix"
foreach ($a in ($Order -split ",")) {
  if ($OwnerPid -and -not (Get-Process -Id $OwnerPid -EA SilentlyContinue)) { Write-Host "owner gone, stop"; break }
  Write-Host "[$(Get-Date -f HH:mm:ss)] SERIES arm $a start"
  $o = & powershell -NoProfile -ExecutionPolicy Bypass -File "$tm\pc2_cpucut_arm.ps1" -Arm $a -Run $Run -Dll $Dll -OwnerPid $OwnerPid -BaseIni $BaseIni 2>&1 | Out-String
  Write-Host $o
  if ($o -match "!!") { Write-Host "SERIES abort after arm $a"; break }
  Start-Sleep 10
}
Write-Host "[$(Get-Date -f HH:mm:ss)] SERIES end"
