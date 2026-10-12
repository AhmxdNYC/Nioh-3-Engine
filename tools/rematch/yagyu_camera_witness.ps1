[CmdletBinding()]
param([string]$RunDir,[string]$AdmissionReceipt,[string]$ExpectedAwaitHash,
 [string]$Evidence='', [uint64]$Token=1,[uint64]$Attempt=1,[switch]$PreloadOnly,[switch]$Run)
$ErrorActionPreference='Stop'
$Action='Camera'
if(-not $Run -and -not $PreloadOnly){'PLAN: admitted Yagyu run, exact awaiting hash and process/pipe checks, one camera180ms; no approval/return';return}
$identitySource='C:/Users/Ahmad Hamza/nioh3-engine/tools/rematch/camera_witness_process.cs'
if((Get-FileHash -LiteralPath $identitySource).Hash -ine 'e4879db715c036b8a3c9cdbbe6b669da7b43326c0fa7f307d5cdbb849fa6c40b'){throw 'Identity helper changed'}
$capture=Join-Path $PSScriptRoot 'yagyu_owned_capture.cs'
if((Get-FileHash -LiteralPath $capture).Hash -ine '5bac27bff0deacc01be5d2897ddfb4a4d872065e19559b55243c1529ee1888bf'){throw 'Capture helper changed'}
if(-not ('CWProcessLease' -as [type])){Add-Type -Path $identitySource}
if(-not ('OwnedCapture' -as [type])){Add-Type -Path $capture -ReferencedAssemblies System.Drawing}
if(-not ('YagyuInputNative' -as [type])){Add-Type -TypeDefinition 'using System;using System.Runtime.InteropServices; public static class YagyuInputNative { [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetNamedPipeServerProcessId(IntPtr pipe,out uint pid); [DllImport("kernel32.dll")] public static extern ulong GetTickCount64(); }'}
if($PreloadOnly){'PRELOADED: identity/capture/pipe types; no game access or input';return}
if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'yagyu_camera_scope.ps1')).Hash -ine '5053f698606f7dce76dfcd65a558e6f387712ec82c828441fd71ee8f53f77ffc'){throw 'Admission scope helper changed'}
. (Join-Path $PSScriptRoot 'yagyu_camera_scope.ps1')
$admission=Read-YagyuCameraAdmission $AdmissionReceipt $RunDir
if((Get-FileHash -LiteralPath $PSCommandPath).Hash -ine $admission.input_helper_sha256){throw 'Witness source differs from admission'}
$awaiting=Join-Path $RunDir 'awaiting.json'
$awaitHash=(Get-FileHash -LiteralPath $awaiting).Hash
if(-not $ExpectedAwaitHash -or $awaitHash -ine $ExpectedAwaitHash){throw 'Expected awaiting pin absent or changed'}
$binding=Get-Content -LiteralPath $awaiting -Raw|ConvertFrom-Json
if($binding.owner_pid -ne $admission.owner_pid -or $binding.owner_start -cne $admission.owner_start){throw 'Foreign admitted owner'}
if($binding.stage -cne 'ControlReviewed' -or $binding.row -cne 'R1A5E' -or $Token -ne 1 -or $Attempt -ne 1 -or -not $Evidence.Trim()){throw 'Exact current singleton camera scope required'}
$game=Get-Process -Id $binding.game_pid
$lease=[CWProcessLease]::new([int]$binding.owner_pid,([datetime]$binding.owner_start).ToUniversalTime().ToFileTimeUtc(),[int]$binding.runner_pid,([datetime]$binding.runner_start).ToUniversalTime().ToFileTimeUtc(),[int]$binding.game_pid,([datetime]$binding.game_start).ToUniversalTime().ToFileTimeUtc(),'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3.exe',$game.MainWindowHandle)
$game.Dispose()
$witnessClock=[Diagnostics.Stopwatch]::StartNew()
$receipt=[ordered]@{timing=[ordered]@{};native_acceptance=$false;cover_sample_is_historical=$true;action=$Action;binding=$binding;awaiting_sha256=$awaitHash;evidence=$Evidence;started_utc=[datetime]::UtcNow.ToString('o');input_attempted=$false;input_confirmed=$false;automatic_approval=$false;status='PREPARING'}
$receiptPath=Join-Path $RunDir ('owned-input-'+[guid]::NewGuid().ToString('N')+'.json')
function Check-InputIdentity {
 if($witnessClock.ElapsedMilliseconds -gt 4000){throw 'Witness4second budget expired; no further input'}
 $lease.Check()
 if((Get-FileHash -LiteralPath $awaiting).Hash -cne $awaitHash){throw 'Awaited stage changed; no input'}
}
function Invoke-OwnedInputPipe([string]$Name,[string]$Command,[bool]$InputCommand=$false){
 Check-InputIdentity
 $pipe=New-Object IO.Pipes.NamedPipeClientStream('.',$Name,[IO.Pipes.PipeDirection]::InOut)
 try{
  $pipe.Connect(500);[uint32]$server=0
  if(-not [YagyuInputNative]::GetNamedPipeServerProcessId($pipe.SafePipeHandle.DangerousGetHandle(),[ref]$server) -or $server -ne $binding.game_pid){throw 'Foreign named-pipe server'}
  Check-InputIdentity
  if($InputCommand -and ([YagyuInputNative]::GetTickCount64()-$script:freshTick -gt 750)){throw 'Current-state check expired; no input'}
  $writer=New-Object IO.StreamWriter($pipe);$writer.AutoFlush=$true;$reader=New-Object IO.StreamReader($pipe)
  if($InputCommand){$receipt.input_attempted=$true}
  $writer.WriteLine($Command);$pending=$reader.ReadLineAsync()
  if(-not $pending.Wait(2500)){throw 'Pipe reply timed out; command outcome unknown, no retry'}
  if($null -eq $pending.Result){throw 'Pipe closed; no retry'}
  return [string]$pending.Result
 }finally{$pipe.Dispose()}
}
function Field([string]$Text,[string]$Name){
 $m=[regex]::Matches($Text,'(?:^| )'+[regex]::Escape($Name)+'=([^ ]+)(?= |$)')
 if($m.Count -ne 1){throw "Missing/duplicate field $Name"};return $m[0].Groups[1].Value
}
try{
 $receipt.pad_state=Invoke-OwnedInputPipe 'nioh3vpad' 'state'
 $receipt.status_before=Invoke-OwnedInputPipe 'nioh3bossrematch' 'status'
 $receipt.cover=Invoke-OwnedInputPipe 'nioh3bossrematch' 'cover'
 foreach($pair in @(@('mission','0x1A5E'),@('stage','5021112'),@('loading','0'),@('player','1'),@('infight','1'))){if((Field $receipt.status_before $pair[0]) -ine $pair[1]){throw 'Current encounter not ready'}}
 foreach($pair in @(@('row','R1A5E'),@('mission','0x1A5E'),@('start','5021112'),@('placement','5027806'),@('observedActor','5027806'),@('phase','4'),@('token',[string]$Token),@('attempt',[string]$Attempt))){if((Field $receipt.cover $pair[0]) -ine $pair[1]){throw 'Cover transaction mismatch'}}
 foreach($field in @('armed','fg','field','player')){if((Field $receipt.pad_state $field) -cne '1'){throw 'Current live pad game state not ready'}}
 foreach($field in @('loading','paused','menu_pause')){if((Field $receipt.pad_state $field) -cne '0'){throw 'Current live pad game state blocked'}}
 $log=Get-Content -LiteralPath 'C:/Program Files (x86)/Steam/steamapps/common/Nioh3/Nioh3BossRematch.log' -Raw
 $actorPattern='cover token='+$Token+' attempt='+$Attempt+' event=existing_actor mission=0x1A5E start=5021112 actor=5027806'
 if(-not $log.Contains($actorPattern)){throw 'Exact existing_actor witness absent'}
 $receipt.existing_actor_witness=$actorPattern
 Check-InputIdentity
 $receipt.timing.before_capture_utc=[datetime]::UtcNow.ToString('o')
 $receipt.before_image=Join-Path $RunDir 'camera-before.png'
 [OwnedCapture]::Capture($lease.MainWindowHandle,$receipt.before_image)
 Check-InputIdentity
 $receipt.pad_state_fresh=Invoke-OwnedInputPipe 'nioh3vpad' 'state'
 $script:freshTick=[YagyuInputNative]::GetTickCount64()
 foreach($field in @('armed','fg','field','player')){if((Field $receipt.pad_state_fresh $field) -cne '1'){throw 'Live field changed before camera pulse'}}
 foreach($field in @('loading','paused','menu_pause')){if((Field $receipt.pad_state_fresh $field) -cne '0'){throw 'Pause/loading changed before camera pulse'}}
 $freshStatus=Invoke-OwnedInputPipe 'nioh3bossrematch' 'status'
 foreach($pair in @(@('mission','0x1A5E'),@('stage','5021112'),@('loading','0'),@('player','1'),@('infight','1'))){if((Field $freshStatus $pair[0]) -ine $pair[1]){throw 'Encounter changed during before capture'}}
 $freshCover=Invoke-OwnedInputPipe 'nioh3bossrematch' 'cover'
 foreach($pair in @(@('phase','4'),@('token',[string]$Token),@('attempt',[string]$Attempt),@('observedActor','5027806'))){if((Field $freshCover $pair[0]) -ine $pair[1]){throw 'Cover changed before camera'}}
 $receipt.timing.input_send_utc=[datetime]::UtcNow.ToString('o')
 $receipt.command='stick R 0.35 0 180'
 $receipt.reply=Invoke-OwnedInputPipe 'nioh3vpad' $receipt.command $true
 if($receipt.reply -cnotmatch '^ok stick R 11468 0 180 ms \([1-9]\d* rounds\)$'){throw 'Exact pulse acknowledgement absent/uncertain; no retry'}
 $receipt.timing.input_ack_utc=[datetime]::UtcNow.ToString('o')
 $receipt.input_confirmed=$true
 Check-InputIdentity
 $receipt.after_image=Join-Path $RunDir 'camera-after.png'
 [OwnedCapture]::Capture($lease.MainWindowHandle,$receipt.after_image)
 Check-InputIdentity
 $receipt.timing.after_capture_utc=[datetime]::UtcNow.ToString('o')
 $receipt.status_after=Invoke-OwnedInputPipe 'nioh3bossrematch' 'status'
 $receipt.pad_after=Invoke-OwnedInputPipe 'nioh3vpad' 'state'
 $receipt.cover_after=Invoke-OwnedInputPipe 'nioh3bossrematch' 'cover'
 $receipt.status='INPUT_CAPTURED_PENDING_VISUAL_REVIEW'
}catch{$receipt.status='REFUSED_OR_UNCERTAIN';$receipt.error="$_";throw}
finally{
 try{$receipt|ConvertTo-Json -Depth 8|Set-Content -LiteralPath $receiptPath -Encoding UTF8;$receipt|ConvertTo-Json -Depth 8}
 finally{$lease.Dispose()}
}
