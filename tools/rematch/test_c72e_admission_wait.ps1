# Fake monotonic clock + actual observation helpers. No game/pipe/file mutations.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
$checks=0
function Check([bool]$Value) { if (-not $Value) { throw "FAIL $script:checks" }; $script:checks++ }
$good='state=0 player=1 loading=0 gate=ok'
$track=@{since=$null;last=$null}
for ($t=0;$t -lt 12000;$t+=1000) { Check (-not (C72-ReadySettled $track $t $good $true)) }
Check (C72-ReadySettled $track 12000 $good $true)
foreach ($bad in @('state=1 player=1 loading=0 gate=ok','state=0 player=0 loading=0 gate=ok',
    'state=0 player=1 loading=1 gate=ok','state=0 player=1 gate=ok','state=0 player=1 loading=0 gate=mission queue busy')) {
    $track=@{since=0;last=11000}
    Check (-not (C72-ReadySettled $track 12000 $bad $true))
    Check ($null -eq $track.since)
    Check (-not (C72-ReadySettled $track 13000 $good $true))
}
$track=@{since=0;last=11000}
Check (-not (C72-ReadySettled $track 12000 $good $false))
Check ($null -eq $track.since)
$track=@{since=0;last=1000}
Check (-not (C72-ReadySettled $track 13000 $good $true))
Check ($track.since -eq 13000)
$track=@{since=100;last=11000}
Check (-not (C72-ReadySettled $track 50 $good $true))
$old="[00:00:00] Rematch: refused: old refusal`r`n"
Check ($null -eq (C72-NewRefusal $old $old))
$queued=$old+"[00:00:01] Rematch: request Fight now queued from pipe`r`n"
Check ($null -eq (C72-NewRefusal $old $queued)) # queue ACK is not native admission
$refused=$queued+"[00:00:06] Rematch: refused: mission queue busy`r`n"
Check ((C72-NewRefusal $old $refused) -eq 'mission queue busy')
$failed=$false;try { C72-NewRefusal $old 'rotated log' | Out-Null } catch { $failed=$true };Check $failed
$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1'))
Check ($source.IndexOf('C72-ReadySettled $readyTracker') -lt $source.IndexOf("Invoke-C72Pipe 'scan'"))
Check ($source.IndexOf('$requestLogBefore=C72-ReadShared') -lt $source.IndexOf("Invoke-C72Pipe 'fight RC72E'"))
Check ($source -match "request_disposition='QUEUED_ACK_ONLY'")
Check ($source -match "request_disposition='REFUSED_AFTER_QUEUE_ACK'")
Check ($source -match 'AddSeconds\(180\)')
Check (([regex]::Matches($source,"Invoke-C72Pipe 'fight RC72E'")).Count -eq 1)
Write-Host "$checks READY-SETTLE / NEW-REFUSAL CHECKS PASS; queue acknowledgement is not native-call/landing proof"
