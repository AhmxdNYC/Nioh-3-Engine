$ErrorActionPreference='Stop'
. "$PSScriptRoot/c72e_pilot_lib.ps1"
$n=0
function Check($v,$why) { if (-not $v) { throw $why };$script:n++ }
$before="prefix`n"
$request="[t] Rematch: exit: MissionRequestReturn(params, 1) from mission 0xC72E (retry point 334140) -> requested`n"
$exited="[t] Rematch: replay mission exited (stage now 123, mission state +28h 0x1234)`n"
$reply='state=0 stage=123 mission=0x1234 loading=0 player=1 warps=0 spawn=- gate=ok'
Check (C72-ReturnEvidence $before ($before+$request+$exited) $reply 334140 @{mission='0x1234';stage='123'}).confirmed 'ordered native return'
foreach ($tail in @('', $request, $exited, ($exited+$request), ($request.Replace('334140','334141')+$exited))) {
 Check (-not (C72-ReturnEvidence $before ($before+$tail) $reply 334140 @{mission='0x1234';stage='123'}).confirmed) 'missing/unordered/mismatched native witness'
}
foreach ($bad in @('state=1','mission=0xC72E','mission=0x0','loading=1','player=0','warps=1','spawn=active','gate=native replay/return still pending','gate=mission queue busy')) {
 $key=$bad.Split('=')[0];$r=[regex]::Replace($reply,($key+'=\S+'),$bad)
 Check (-not (C72-ReturnEvidence $before ($before+$request+$exited) $r 334140 @{mission='0x1234';stage='123'}).confirmed) $bad
}
foreach ($tail in @('[2026-10-11 19:00:00] Rematch: refused: loading', $request.Replace('requested','EXCEPTION'))) {
 $failed=$false;try { C72-ReturnEvidence $before ($before+$tail) $reply 334140 @{mission='0x1234';stage='123'} | Out-Null } catch { $failed=$true };Check $failed 'refusal/exception'
}
$failed=$false;try { C72-ReturnEvidence $before ('foreign'+$request+$exited) $reply 334140 @{mission='0x1234';stage='123'} | Out-Null } catch { $failed=$true };Check $failed 'replaced prefix'
$runner=Get-Content "$PSScriptRoot/run_c72e_native_pilot.ps1" -Raw
Check ($runner.Contains('if (-not $observed -or -not $script:result.first_observed')) 'timeout must require current observation'
Check ($runner.Contains('-or -not $script:result.native_return_confirmed -or -not $script:cleaned)')) 'final acceptance requires return'
Write-Host "PASS $n native return and timeout checks"
$zero=@{mission='0x0';stage='4019512'}
$zeroReply=$reply.Replace('mission=0x1234','mission=0x0').Replace('stage=123','stage=4019512')
$zeroLog=$exited.Replace('stage now 123','stage now 4019512').Replace('0x1234','0x0')
Check (C72-ReturnEvidence $before ($before+$request+$zeroLog) $zeroReply 334140 $zero).confirmed 'bound zero mission entry accepted'
Check (-not (C72-ReturnEvidence $before ($before+$request+$zeroLog) $zeroReply 334140 @{mission='0x1234';stage='4019512'}).confirmed) 'zero not bound entry refused'
Check (-not (C72-ReturnEvidence $before ($before+$request+$zeroLog) $zeroReply 334140 @{mission='0x0';stage='999'}).confirmed) 'wrong entry stage refused'
$failed=$false;try { C72-ReturnEvidence $before ($before+$request+$zeroLog) $zeroReply 334140 $null | Out-Null } catch { $failed=$true };Check $failed 'missing original binding refused'
$tracker=@{since=$null;last=$null}
for ($ms=0;$ms -le 11000;$ms+=1000) { Check (-not (C72-EntrySettled $tracker $ms $zeroReply $true)) 'entry still settling' }
Check (-not (C72-EntrySettled $tracker 12000 ($zeroReply.Replace('4019512','999')) $true)) 'tuple change resets settle'
Write-Host "PASS $n including exact-entry zero mission and settle-reset checks"
