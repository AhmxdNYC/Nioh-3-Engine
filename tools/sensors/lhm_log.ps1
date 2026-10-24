# Logs LibreHardwareMonitor sensors (needs LHM running elevated with Options > Remote Web Server > Run, port 8085) to CSV.
# usage: lhm_log.ps1 -Out file.csv [-Seconds 600] [-Interval 1]
# A file named "<Out>.stop" ends the loop early. If LHM is down the script keeps waiting and does not fail the bench.
param([string]$Out,[int]$Seconds=600,[double]$Interval=1)
function Walk($n,$path,$acc){ $p = if($n.Text){"$path/$($n.Text)"}else{$path}
  if($n.Value -and $n.Type){ $acc[$p+" ["+$n.Type+"]"]=($n.Value -replace '[^\d\.\-]','') }
  foreach($c in $n.Children){ Walk $c $p $acc } }
$end=(Get-Date).AddSeconds($Seconds); $hdr=$false
while((Get-Date) -lt $end){
  if ($Out -and (Test-Path "$Out.stop")) { break }
  try {
    $j=Invoke-RestMethod http://localhost:8085/data.json -TimeoutSec 3
    $a=[ordered]@{}; Walk $j "" $a
    $row=[ordered]@{ts=(Get-Date).ToString('o')}; foreach($k in $a.Keys){$row[$k]=$a[$k]}
    $o=[pscustomobject]$row
    if(-not $hdr){ $o|Export-Csv $Out -NoTypeInformation; $hdr=$true } else { $o|Export-Csv $Out -NoTypeInformation -Append }
  } catch { }
  Start-Sleep -Milliseconds ([int]($Interval*1000)) }
