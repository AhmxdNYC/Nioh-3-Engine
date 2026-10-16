$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
$checks=0
function Check([bool]$Value) { if (-not $Value) { throw "FAIL $script:checks at line $($MyInvocation.ScriptLineNumber)" };$script:checks++ }
$start=334140;$target=@(207628.53125,6027.47998046875,142677.921875)
$o=@{saw_load=$false;native_start=$null;battle=$null;first_player=$null}
$trace='D:/nioh3-re/rematch-pilots/live-C72E-B-20261011-191415-5360/events.jsonl'
$statuses=@(Get-Content -LiteralPath $trace|ForEach-Object { $_|ConvertFrom-Json }|Where-Object { $_.kind -eq 'status' -and $_.elapsed_ms -lt 30000 })
# Retained real trace ends at loading1/infight1. It must never complete/close the observation.
foreach ($event in $statuses) { Check (-not (C72-ObserveEntry $o $event.reply $start $target $event.elapsed_ms)) }
Check ([bool]$o.native_start);Check ([bool]$o.battle);Check ([bool]$o.first_player)
Check ($o.first_player.before_gate -and $o.first_player.near_target)
$battle=$o.battle
# Explicitly synthetic continuation: first post-intro player moved from requested point.
$after='state=2 mission=0xC72E stage=334140 loading=0 player=1 infight=1 fnow=pass warps=0 spawn=- pos=207073,6026,142372 gate=native replay/return still pending placements=[333550(0s)]'
Check (C72-ObserveEntry $o $after $start $target 20000)
Check ($o.battle -eq $battle);Check ($o.first_player.before_gate);Check ($o.first_player.near_target)
Check ($o.first_player.control -eq 'UNCONFIRMED')
$earliest=$o.first_player
Check (C72-ObserveEntry $o ($after.Replace('207073','207628')) $start $target 21000)
Check ($o.first_player -eq $earliest)
$late=@{saw_load=$true;native_start=$null;battle=$battle;first_player=$null}
Check (C72-ObserveEntry $late $after $start $target 20000)
Check (-not $late.first_player.before_gate -and -not $late.first_player.near_target)
$bad=@{saw_load=$true;native_start=$null;battle=$null;first_player=$null}
Check (-not (C72-ObserveEntry $bad ($after.Replace('0xC72E','0x1111')) $start $target 21000))
Check (-not $bad.battle -and -not $bad.first_player)
$r=[pscustomobject]@{nonce='abc';owner_pid=1;owner_start='a';runner_pid=2;runner_start='b';game_pid=3;game_start='c'}
$a=$r|Select-Object *;$a|Add-Member visible_arena $true;$a|Add-Member visible_boss $true;$a|Add-Member control_response $true;$a|Add-Member evidence 'Explicit synthetic root evidence'
C72-CheckControlReview $r $a;Check $true
foreach ($key in @('nonce','owner_pid','owner_start','runner_pid','runner_start','game_pid','game_start','evidence','visible_arena','visible_boss','control_response')) {
    $broken=$a|Select-Object *;$broken.$key='';$refused=$false;try { C72-CheckControlReview $r $broken } catch { $refused=$true };Check $refused
}
$tracker=@{since=$null;last=$null}
foreach ($ms in 0,1000,2000,3000) { Check (-not (C72-ReturnProxySettled $tracker $ms $after $start)) }
Check (C72-ReturnProxySettled $tracker 4000 $after $start)
Check (-not (C72-ReturnProxySettled $tracker 5000 ($after.Replace('loading=0','loading=1')) $start))
$runner=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1'))
Check ($runner.IndexOf('C72-CheckControlReview ([pscustomobject]$reviewRequest)') -lt $runner.IndexOf("Invoke-C72Pipe 'exit'"))
Check ($runner -match 'if \(\$observed\) \{ break \}')
Write-Host "$checks ordered trace/review/proxy checks PASS; retained trace has pre-intro proxy but no post-intro/control observation, continuation is synthetic"
