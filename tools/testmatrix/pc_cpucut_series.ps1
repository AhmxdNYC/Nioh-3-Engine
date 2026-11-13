param([int]$OwnerPid = 0, [string]$Dll = "", [string]$Run = "r1", [string]$Order = "A,B,B,A,C,C,A,D,D,A", [string]$BaseIni = "", [string]$KeysA = "", [string]$KeysB = "", [string]$Tag = "pc-cpucut-91440", [string[]]$ExtraArgs = @())
# Runs pc_cpucut_arm.ps1 once per arm (adjacent ABBA vs A, shared A launches). Stops at the first arm that logs '!!'.
$tm = "C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix"
foreach ($a in ($Order -split ",")) {
  if ($OwnerPid -and -not (Get-Process -Id $OwnerPid -EA SilentlyContinue)) { Write-Host "owner gone, stop"; break }
  Write-Host "[$(Get-Date -f HH:mm:ss)] SERIES arm $a start"
  $k = if ($a -eq "A") { $KeysA } elseif ($a -eq "B") { $KeysB } else { "" }
  $al = @("-NoProfile","-ExecutionPolicy","Bypass","-File","$tm\pc_cpucut_arm.ps1","-Arm",$a,"-Run",$Run,"-Dll",$Dll,"-OwnerPid",$OwnerPid,"-Tag",$Tag) + $ExtraArgs
  if ($BaseIni) { $al += @("-BaseIni",$BaseIni) }; if ($k) { $al += @("-Keys",$k) }
  $o = & powershell @al 2>&1 | Out-String
  Write-Host $o
  if ($o -match "!!|MissingArgument|ParameterBinding") { Write-Host "SERIES abort after arm $a"; break }
  Start-Sleep 10
}
Write-Host "[$(Get-Date -f HH:mm:ss)] SERIES end"
