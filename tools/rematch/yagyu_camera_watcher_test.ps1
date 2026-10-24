[CmdletBinding()]
param([string]$EvidenceDir='D:/nioh3-re/reviews/yagyu-native-root4-20261012/preloaded-offline')
$ErrorActionPreference='Stop'
$null=New-Item -ItemType Directory -Path $EvidenceDir -Force
$here=$PSScriptRoot
foreach($name in @('yagyu_camera_watcher.ps1','yagyu_camera_witness.ps1','yagyu_camera_scope.ps1','yagyu_child_process.ps1')){
 $t=$null;$e=$null;$null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $here $name),[ref]$t,[ref]$e);if($e){throw "Parse failed $name"}
}
. (Join-Path $here 'yagyu_camera_scope.ps1')
. (Join-Path $here 'yagyu_child_process.ps1')
$path=Join-Path $EvidenceDir 'synthetic-admission.json'
$admission=@{approved=$true;scope='ONE_HOSTED_ALL_BOSS_SHORT_SWEEP';out_dir=(Join-Path $EvidenceDir 'inert-not-a-game');rows=@('R1A5E');auto_camera_return=$false;owner_pid=$PID;owner_start=(Get-Process -Id $PID).StartTime.ToString('o');expires_utc=[datetime]::UtcNow.AddMinutes(5).ToString('o')}
$admission|ConvertTo-Json|Set-Content $path -Encoding UTF8
$null=Read-YagyuCameraAdmission $path $admission.out_dir
foreach($change in @(@('approved',$false),@('scope','OTHER'),@('rows',@('R1CBC')),@('auto_camera_return',$true),@('expires_utc','2020-01-01T00:00:00Z'))){
 $bad=$admission.Clone();$bad[$change[0]]=$change[1];$bad|ConvertTo-Json|Set-Content $path -Encoding UTF8
 $refused=$false;try{$null=Read-YagyuCameraAdmission $path $admission.out_dir}catch{$refused=$true};if(-not $refused){throw 'Invalid admission accepted'}
}
$admission|ConvertTo-Json|Set-Content $path -Encoding UTF8
$refused=$false;try{$null=Read-YagyuCameraAdmission $path 'D:/wrong'}catch{$refused=$true};if(-not $refused){throw 'Wrong output accepted'}
$t=$null;$e=$null;$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $here 'yagyu_camera_watcher.ps1'),[ref]$t,[ref]$e)
foreach($name in @('Read-AwaitingSnapshot','Assert-Binding')){$fn=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $name},$true))[0];Invoke-Expression $fn.Extent.Text}
if($null -ne (Read-AwaitingSnapshot (Join-Path $EvidenceDir 'absent.json'))){throw 'Transition absence failed'}
foreach($text in @('','{broken','null')){[IO.File]::WriteAllText((Join-Path $EvidenceDir 'bad.json'),$text);$refused=$false;try{$null=Read-AwaitingSnapshot (Join-Path $EvidenceDir 'bad.json')}catch{$refused=$true};if(-not $refused){throw 'Malformed snapshot admitted'}}
$b=@{owner_pid=$admission.owner_pid;owner_start=$admission.owner_start;runner_pid=123;runner_start='r';game_pid=234;game_start='g';nonce='0123456789abcdef0123456789abcdef'}
Assert-Binding $b $b
foreach($k in @('owner_pid','owner_start','runner_pid','runner_start','game_pid','game_start','nonce')){$bad=$b.Clone();$bad[$k]='invalid';$refused=$false;try{Assert-Binding $bad $b}catch{$refused=$true};if(-not $refused){throw "Foreign $k accepted"}}
$t=$null;$e=$null;$witnessAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $here 'yagyu_camera_witness.ps1'),[ref]$t,[ref]$e)
$fn=@($witnessAst.FindAll({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Field'},$true))[0];Invoke-Expression $fn.Extent.Text
foreach($value in @('','player=1 player=0','player=1 player=1')){$refused=$false;try{$null=Field $value 'player'}catch{$refused=$true};if(-not $refused){throw 'Missing or duplicate telemetry accepted'}}
$Token=1;$Attempt=1
$good='mission=0x1A5E stage=5021112 loading=0 player=1 infight=1 row=R1A5E start=5021112 placement=5027806 observedActor=5027806 phase=4 token=1 attempt=1 armed=1 fg=1 field=1 paused=0 menu_pause=0'
$guards=@($witnessAst.FindAll({param($n)$n -is [Management.Automation.Language.ForEachStatementAst]},$true))
foreach($guard in $guards){
 $receipt=@{pad_state=$good;status_before=$good;cover=$good;pad_state_fresh=$good};$freshStatus=$good;$freshCover=$good
 Invoke-Expression $guard.Extent.Text
 $bad=[regex]::Replace($good,'=[^ ]+','=INVALID')
 $receipt=@{pad_state=$bad;status_before=$bad;cover=$bad;pad_state_fresh=$bad};$freshStatus=$bad;$freshCover=$bad
 $refused=$false;try{Invoke-Expression $guard.Extent.Text}catch{$refused=$true};if(-not $refused){throw 'Invalid native tuple accepted'}
}
$watch=[Diagnostics.Stopwatch]::StartNew();& (Join-Path $here 'yagyu_camera_witness.ps1') -PreloadOnly;$cold=$watch.Elapsed.TotalMilliseconds
$watch.Restart();& (Join-Path $here 'yagyu_camera_witness.ps1') -PreloadOnly;$warm=$watch.Elapsed.TotalMilliseconds
$refused=$false;try{[OwnedCapture]::Capture([IntPtr]::Zero,(Join-Path $EvidenceDir 'must-not-exist.png'))}catch{$refused=$true};if(-not $refused){throw 'Invalid capture handle accepted'}
$childResults=@()
foreach($i in 1..3){$r=Invoke-YagyuReviewedChild powershell.exe @('-NoProfile','-Command','Write-Output success; exit 0') (Join-Path $EvidenceDir "child$i.stdout") (Join-Path $EvidenceDir "child$i.stderr");if($r.exit_code -ne 0 -or -not $r.confirmed){throw 'Clean child rejected'};$childResults+=$r}
$refused=$false;try{$null=Invoke-YagyuReviewedChild powershell.exe @('-NoProfile','-Command','exit 7') (Join-Path $EvidenceDir 'badchild.stdout') (Join-Path $EvidenceDir 'badchild.stderr')}catch{$refused="$_" -like '*exited 7*'};if(-not $refused){throw 'Nonzero child exit not distinguished'}
$refused=$false;try{$null=Invoke-YagyuReviewedChild powershell.exe @('-NoProfile','-Command','Start-Sleep -Seconds 3') (Join-Path $EvidenceDir 'slowchild.stdout') (Join-Path $EvidenceDir 'slowchild.stderr') 50}catch{$refused="$_" -like '*timeout*'};if(-not $refused){throw 'Bounded child did not stop'}
$result=@{status='PASS_OFFLINE_ONLY';cold_preload_ms=$cold;warm_preload_ms=$warm;children=$childResults;native_game_access=$false;native_input=$false;native_capture=$false;native_acceptance=$false;limitation='Preload timing is idle-host diagnostic; does not prove live latency or survival. No fifth admission.'}
$result|ConvertTo-Json -Depth 6|Set-Content (Join-Path $EvidenceDir 'result.json') -Encoding UTF8
$result|ConvertTo-Json -Depth 6
