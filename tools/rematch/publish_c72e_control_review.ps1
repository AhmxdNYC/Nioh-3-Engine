# Explicit root report only. No game input, native commands, focus or launch.
param([Parameter(Mandatory)][string]$OutDir,[Parameter(Mandatory)][int]$OwnerPid,
    [Parameter(Mandatory)][string]$Evidence,[switch]$VisibleArena,[switch]$VisibleBoss,[switch]$ControlResponse)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
$OutDir=C72-Full $OutDir
$request=Get-Content -LiteralPath (C72-Child $OutDir 'control-review-request.json') -Raw|ConvertFrom-Json
if ($request.owner_pid -ne $OwnerPid) { throw 'Wrong root owner' }
foreach ($pair in @(@($request.owner_pid,$request.owner_start),@($request.runner_pid,$request.runner_start),@($request.game_pid,$request.game_start))) {
    $process=Get-Process -Id $pair[0] -ErrorAction Stop
    if ($process.StartTime.ToString('o') -cne $pair[1]) { throw 'Review process identity expired' }
}
$review=$request|Select-Object *
foreach ($pair in @(@('visible_arena',[bool]$VisibleArena),@('visible_boss',[bool]$VisibleBoss),@('control_response',[bool]$ControlResponse),@('evidence',$Evidence))) {
    $review|Add-Member -NotePropertyName $pair[0] -NotePropertyValue $pair[1]
}
C72-CheckControlReview $request $review
$temporary=C72-Child $OutDir ('control-review-'+[guid]::NewGuid().ToString('N')+'.tmp')
try { C72-Json $temporary $review;[IO.File]::Move($temporary,(C72-Child $OutDir 'control-review.json')) }
finally { if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary } }
Write-Host 'Explicit root control review recorded; first-controllable timing remains separate.'
