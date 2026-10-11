$ErrorActionPreference='Stop'
. "$PSScriptRoot/c72e_pilot_lib.ps1"
$n=0
function Check($v,$why) { if (-not $v) { throw $why };$script:n++ }
$before="prefix`n"
$request="[t] Rematch: exit: MissionRequestReturn(params, 1) from mission 0xC72E (retry point 334140) -> requested`n"
$exited="[t] Rematch: replay mission exited (stage now 123, mission state +28h 0x1234)`n"
$reply='state=0 mission=0x1234 loading=0 player=1 warps=0 spawn=- gate=ok'
Check (C72-ReturnEvidence $before ($before+$request+$exited) $reply 334140).confirmed 'ordered native return'
foreach ($tail in @('', $request, $exited, ($exited+$request), ($request.Replace('334140','334141')+$exited))) {
 Check (-not (C72-ReturnEvidence $before ($before+$tail) $reply 334140).confirmed) 'missing/unordered/mismatched native witness'
}
foreach ($bad in @('state=1','mission=0xC72E','mission=0x0','loading=1','player=0','warps=1','spawn=active','gate=native replay/return still pending','gate=mission queue busy')) {
 $key=$bad.Split('=')[0];$r=[regex]::Replace($reply,($key+'=\S+'),$bad)
 Check (-not (C72-ReturnEvidence $before ($before+$request+$exited) $r 334140).confirmed) $bad
}
foreach ($tail in @('[2026-10-11 19:00:00] Rematch: refused: loading', $request.Replace('requested','EXCEPTION'))) {
 $failed=$false;try { C72-ReturnEvidence $before ($before+$tail) $reply 334140 | Out-Null } catch { $failed=$true };Check $failed 'refusal/exception'
}
$failed=$false;try { C72-ReturnEvidence $before ('foreign'+$request+$exited) $reply 334140 | Out-Null } catch { $failed=$true };Check $failed 'replaced prefix'
$runner=Get-Content "$PSScriptRoot/run_c72e_native_pilot.ps1" -Raw
Check ($runner.Contains('if (-not $observed -or -not $script:result.first_observed')) 'timeout must require current observation'
Check ($runner.Contains('-or -not $script:result.native_return_confirmed -or -not $script:cleaned)')) 'final acceptance requires return'
Write-Host "PASS $n native return and timeout checks"
