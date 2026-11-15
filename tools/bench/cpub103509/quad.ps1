param([string]$Key, [string]$Order = "A,B,B,A", [string]$Run = "")
# #103509: A = full CPU bundle, B = bundle minus $Key. One launch per letter.
$all = @("RD","M2S","MW","J1","P1E7","DQ")
$ka = ($all | % { "$_=1" }) -join ","
$kb = ($all | % { if ($_ -eq $Key) { "$_=0" } else { "$_=1" } }) -join ","
if (-not $Run) { $Run = "q-$Key" }
$d = "D:\nioh3-re\cpub103509"
foreach ($a in ($Order -split ",")) {
  $k = if ($a -eq "A") { $ka } else { $kb }
  $steam = "C:\Program Files (x86)\Steam\steam.exe"   # AppError_16 WaitingPrevProcess after a close: fresh Steam per launch
  & $steam -shutdown; $t0 = Get-Date; while ((Get-Process steam -EA SilentlyContinue) -and ((Get-Date) - $t0).TotalSeconds -lt 60) { Start-Sleep 2 }
  Start-Process $steam -ArgumentList "-silent"; Start-Sleep 30
  Write-Host "[$(Get-Date -f HH:mm:ss)] QUAD $Key arm $a keys $k"
  $o = & powershell -NoProfile -ExecutionPolicy Bypass -File "$d\arm.ps1" -Arm $a -Keys $k -Dll "$d\EF.asi" -BaseIni "$d\base.ini" -Tag "pc-cpub-103509" -Run $Run -MaxFps 60 -RenFps 60 -Windows 2 -Sec 60 -OwnerPid $PID 2>&1 | Out-String
  Write-Host $o
  if ($o -match "!!|MissingArgument|ParameterBinding") { Write-Host "QUAD abort after $a"; break }
  Start-Sleep 10
}
Write-Host "[$(Get-Date -f HH:mm:ss)] QUAD end"
