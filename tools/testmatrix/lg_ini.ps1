# lg_ini.ps1 - writes tools\testmatrix\fullmod_ini_1717_lg.ini = fullmod_ini_1716.ini + the probe LG flags (queue item -2).
# No game access. Then (PC, game free): powershell -File loadstats_ab.ps1 -Ini fullmod_ini_1717_lg.ini BASE_a BASE_b
#   -> captures\pc\loadstats\loadstats_BASE_*.log; grep "LoadGates|LoadSeq|LoadStats tick" in them.
# Also run in the SAME launch: one shrine fast travel + one death respawn (native FT owners) and keep the log.
# Ally: same ini, title -> Continue x2 + 1 fast travel.
$d = Split-Path -Parent $MyInvocation.MyCommand.Path
$t = [IO.File]::ReadAllText("$d\fullmod_ini_1716.ini")
$want = [ordered]@{ LoadStats = 3; LoadGates = 1; MemStats = 1; SP = 1; ReportSec = 5; TopN = 25 }
foreach ($k in $want.Keys) {
  if ($t -match "(?m)^$k = ") { $t = $t -replace "(?m)^$k = .*", "$k = $($want[$k])" }
  else { $t = $t -replace '(?m)^\[Profiler\]', "[Profiler]`r`n$k = $($want[$k])" }
}
[IO.File]::WriteAllText("$d\fullmod_ini_1717_lg.ini", $t, (New-Object Text.UTF8Encoding($false)))
"wrote $d\fullmod_ini_1717_lg.ini"
