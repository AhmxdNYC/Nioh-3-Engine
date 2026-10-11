param([Parameter(Mandatory=$true)][string]$RunDir)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/c72e_pilot_lib.ps1"
$names=@('result.json','events.jsonl','addon.log','control-review-request.json','control-review.json')
$hashes=@{};foreach($name in $names){$hashes[$name]=(Get-FileHash -LiteralPath (Join-Path $RunDir $name) -Algorithm SHA256).Hash}
$r=Get-Content (Join-Path $RunDir 'result.json') -Raw|ConvertFrom-Json
foreach($key in @('root_control_confirmed','saves_restored','inis_restored','binaries_restored')) { if($r.$key -isnot [bool] -or -not $r.$key){throw "Missing final $key"} }
$req=Get-Content (Join-Path $RunDir 'control-review-request.json') -Raw|ConvertFrom-Json
$review=Get-Content (Join-Path $RunDir 'control-review.json') -Raw|ConvertFrom-Json
C72-CheckControlReview $req $review
$events=@(Get-Content (Join-Path $RunDir 'events.jsonl')|ForEach-Object{$_|ConvertFrom-Json})
$fights=@($events|Where-Object kind -eq 'fight RC72E');$exits=@($events|Where-Object kind -eq 'exit')
if($fights.Count -ne 1 -or $exits.Count -ne 1 -or $fights[0].reply -notmatch '^queued' -or $exits[0].reply -notmatch '^queued'){throw 'Exactly one queued fight and exit required'}
$fightTime=[datetime]$fights[0].utc;$exitTime=[datetime]$exits[0].utc
$tracker=@{since=$null;last=$null};$entry=$null
foreach($e in $events){
 if([datetime]$e.utc -ge $fightTime){break}
 if($e.kind -ne 'status'){continue}
 if(C72-EntrySettled $tracker ([long]$e.elapsed_ms) $e.reply $true){$entry=@{mission=(C72-Field $e.reply 'mission');stage=(C72-Field $e.reply 'stage');reply=$e.reply}} else {$entry=$null}
}
if(-not $entry){throw 'No stable original entry tuple'}
# Bound native log to this single exit using its wall-clock timestamp. Refuse a
# midnight-spanning replay here rather than infer a date from time-only log lines.
$localExit=$exitTime.ToLocalTime();$limit=$localExit.AddSeconds(90)
if($localExit.Date -ne $limit.Date){throw 'Time-only log crosses midnight; manual correlation required'}
$tailLines=@();foreach($line in Get-Content (Join-Path $RunDir 'addon.log')){
 if($line -match '^\[(\d\d:\d\d:\d\d\.\d{3})\]'){
  $stamp=$localExit.Date+[timespan]::Parse($Matches[1])
  if($stamp -ge $localExit -and $stamp -le $limit){$tailLines+=$line}
 }
}
$tail=$tailLines -join "`n";$proof=$null
foreach($e in $events){
 $when=[datetime]$e.utc
 if($e.kind -ne 'status' -or $when -lt $exitTime -or $when -gt $exitTime.AddSeconds(90)){continue}
 # Do not credit a telemetry snapshot from before the native exit log.
 $available=@($tailLines|Where-Object{$_ -match '^\[([^]]+)\]' -and ($localExit.Date+[timespan]::Parse($Matches[1])) -le $when.ToLocalTime()}) -join "`n"
 $candidate=C72-ReturnEvidence '' $available $e.reply $r.start $entry
 if($candidate.confirmed){$proof=$candidate;break}
}
if(-not $proof){throw 'No ordered native return and exact entry telemetry witness'}
foreach($name in $names){if((Get-FileHash -LiteralPath (Join-Path $RunDir $name) -Algorithm SHA256).Hash -ne $hashes[$name]){throw 'Evidence changed during offline review'}}
$out=Join-Path $RunDir 'offline-return-review.json'
if(Test-Path -LiteralPath $out){throw 'Offline receipt already exists; preserve it'}
C72-Json $out @{status='OFFLINE_RETURN_EVIDENCE_READY_FOR_ROOT_REVIEW';original_result_preserved=$true;input_sha256=$hashes;entry_tuple=$entry;native_return_confirmed=$true;native_return_evidence=$proof;root_control_confirmed=$true;restoration_confirmed=$true;instant_verdict=$r.instant_verdict;control_review=$review}
Write-Host "PASS offline ordered return to original entry; original result preserved: $out"
