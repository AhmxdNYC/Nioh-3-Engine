[CmdletBinding()]
param([Parameter(Mandatory)][string]$RunDir,[Parameter(Mandatory)][string]$AdmissionReceipt,[switch]$Run)
$ErrorActionPreference='Stop'
if(-not $Run){'PLAN: wait <=600s for exact admitted ControlReviewed; one Camera180ms; capture before/after; never approve/return';return}
if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'yagyu_camera_scope.ps1')).Hash -ine '5053f698606f7dce76dfcd65a558e6f387712ec82c828441fd71ee8f53f77ffc'){throw 'Admission scope helper changed'}
. (Join-Path $PSScriptRoot 'yagyu_camera_scope.ps1')
$admission=Read-YagyuCameraAdmission $AdmissionReceipt $RunDir
if((Get-FileHash -LiteralPath $PSCommandPath).Hash -ine $admission.camera_watcher_sha256){throw 'Watcher source differs from admission'}
$admissionHash=(Get-FileHash -LiteralPath $AdmissionReceipt).Hash
$await=Join-Path $RunDir 'awaiting.json'
$initial=Get-Content -LiteralPath $await -Raw|ConvertFrom-Json
function Read-AwaitingSnapshot([string]$Path){
 # FileNotFound alone is the driver's expected FieldReady -> ControlReviewed gap.
 # A malformed file, directory disappearance, denied access or any other IO error is fatal.
 try{$bytes=[IO.File]::ReadAllBytes($Path)}catch [IO.FileNotFoundException]{return $null}
 $text=[Text.Encoding]::UTF8.GetString($bytes).TrimStart([char]0xFEFF)
 if(-not $text.Trim()){throw 'Empty awaiting file'}
 $value=$text|ConvertFrom-Json
 if($null -eq $value){throw 'Null awaiting binding'}
 $sha=[Security.Cryptography.SHA256]::Create()
 try{$hash=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','')}finally{$sha.Dispose()}
 return @{binding=$value;hash=$hash}
}
function Assert-Binding($current,$first){
 foreach($k in @('owner_pid','owner_start','runner_pid','runner_start','game_pid','game_start')){if([string]$current.$k -cne [string]$first.$k){throw "Binding changed $k"}}
 if($current.owner_pid -ne $admission.owner_pid -or $current.owner_start -cne $admission.owner_start){throw 'Foreign owner'}
 if(-not $current.nonce -or $current.nonce -notmatch '^[0-9a-f]{32}$'){throw 'Bad nonce'}
}
Assert-Binding $initial $initial
if($initial.stage -cne 'FieldReady' -or $initial.row -cne 'ENTRY'){throw 'Watcher must start before FieldReady approval'}
$identity='C:/Users/Ahmad Hamza/nioh3-engine/tools/rematch/camera_witness_process.cs'
if((Get-FileHash -LiteralPath $identity).Hash -ine 'e4879db715c036b8a3c9cdbbe6b669da7b43326c0fa7f307d5cdbb849fa6c40b'){throw 'Identity source changed'}
Add-Type -Path $identity
$game=Get-Process -Id $initial.game_pid
$lease=[CWProcessLease]::new([int]$initial.owner_pid,([datetime]$initial.owner_start).ToUniversalTime().ToFileTimeUtc(),[int]$initial.runner_pid,([datetime]$initial.runner_start).ToUniversalTime().ToFileTimeUtc(),[int]$initial.game_pid,([datetime]$initial.game_start).ToUniversalTime().ToFileTimeUtc(),'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3.exe',$game.MainWindowHandle)
$game.Dispose()
try{
$helper=Join-Path $PSScriptRoot 'yagyu_camera_witness.ps1'
$helperHash=(Get-FileHash -LiteralPath $helper).Hash
if($helperHash -ine $admission.input_helper_sha256){throw 'Witness differs from admission'}
& $helper -PreloadOnly
 $initial|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $RunDir 'camera-watcher-ready.json') -Encoding UTF8
 $deadline=[datetime]::UtcNow.AddSeconds(600)
 while([datetime]::UtcNow -lt $deadline){
  $lease.Check()
  if(Test-Path -LiteralPath (Join-Path $RunDir 'result.json')){throw 'Runner finalized before camera'}
  $snapshot=Read-AwaitingSnapshot $await
  if($null -eq $snapshot){Start-Sleep -Milliseconds 200;continue}
  $hash=$snapshot.hash;$b=$snapshot.binding
  Assert-Binding $b $initial
  if($b.stage -ceq 'ControlReviewed' -and $b.row -ceq 'R1A5E'){
   if($b.nonce -ceq $initial.nonce){throw 'Control nonce reused'}
   if((Get-FileHash -LiteralPath $helper).Hash -cne $helperHash -or (Get-FileHash -LiteralPath $AdmissionReceipt).Hash -cne $admissionHash){throw 'Reviewed source/admission changed'}
   $null=Read-YagyuCameraAdmission $AdmissionReceipt $RunDir
   & $helper -RunDir $RunDir -AdmissionReceipt $AdmissionReceipt -ExpectedAwaitHash $hash -Token 1 -Attempt 1 -Evidence 'Root-authorized bounded preloaded camera witness; no stage approval or return' -Run
   'CAMERA_CAPTURED_NO_APPROVAL: camera-before.png camera-after.png; root must review and approve; no native return sent'
   return
  }
  if($b.stage -cne 'FieldReady' -or $b.row -cne 'ENTRY' -or $b.nonce -cne $initial.nonce){throw 'Unexpected awaited stage'}
  Start-Sleep -Milliseconds 200
 }
 throw 'Watcher deadline; no input'
}finally{$lease.Dispose()}
