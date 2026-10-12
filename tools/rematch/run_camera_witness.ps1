param([Parameter(Mandatory)][string]$RunDir,[Parameter(Mandatory)][uint64]$Token,[Parameter(Mandatory)][int]$OwnerPid,[switch]$Run)
$ErrorActionPreference='Stop'
if(-not $Run){'PLAN: fresh exact R1CBC cover token; guarded foreground; before/after PNG; one stick R 0.35 0 180; no approval/return/retry.';return}
. (Join-Path $PSScriptRoot 'camera_witness_lib.ps1')
. (Join-Path $PSScriptRoot 'fight_cover_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
Add-Type -AssemblyName System.Drawing,System.Windows.Forms
Add-Type @"
using System;using System.Runtime.InteropServices;
public static class CWNative {
 [DllImport("kernel32.dll")] public static extern ulong GetTickCount64();
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetNamedPipeServerProcessId(IntPtr h,out uint id);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint id);
}
"@
$records=@(Get-ChildItem -LiteralPath (Join-Path $RunDir 'guard') -Filter '*.json')
if($records.Count -ne 1){throw 'Expected one pilot guard'}
$recordPath=$records[0].FullName;$initial=Get-Content -LiteralPath $recordPath -Raw|ConvertFrom-Json
if($initial.ownerPid -ne $OwnerPid -or @($initial.games).Count -ne 1){throw 'Owner/game binding mismatch'}
function CW-Guard {
 $r=Get-Content -LiteralPath $recordPath -Raw|ConvertFrom-Json
 foreach($k in @('ownerPid','ownerStart','harnessPid','harnessStart','deadline')){if([string]$r.$k -cne [string]$initial.$k){throw "Guard changed $k"}}
 foreach($pair in @(@($r.ownerPid,$r.ownerStart),@($r.harnessPid,$r.harnessStart))){$p=Get-Process -Id $pair[0];if($p.StartTime -ne [DateTime]$pair[1]){throw 'Owner/harness identity changed'}}
 if([DateTime]::Now -ge [DateTime]$r.deadline -or @($r.games).Count -ne 1){throw 'Pilot expired/game count changed'}
 $g=$r.games[0];if($g.closed -or $g.pid -ne $initial.games[0].pid -or $g.start -cne $initial.games[0].start){throw 'Game record changed'}
 $games=@(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue)
 if($games.Count -ne 1){throw 'Expected exactly one game'}
 $p=$games[0]
 if($p.Id -ne $g.pid -or $p.StartTime -ne [DateTime]$g.start -or $p.Path -ine 'C:\Program Files (x86)\Steam\steamapps\common\Nioh3\Nioh3.exe'){throw 'Game identity mismatch'}
 $h=$p.MainWindowHandle;[uint32]$windowPid=0
 $null=[CWNative]::GetWindowThreadProcessId($h,[ref]$windowPid)
 if(-not $h -or $windowPid -ne $p.Id -or [CWNative]::GetForegroundWindow() -ne $h){throw 'Owned game is not foreground; no focus performed'}
 return $p
}
function CW-Pipe([string]$Name,[string]$Command,[int]$Timeout=500){
 $game=CW-Guard;$pipe=New-Object IO.Pipes.NamedPipeClientStream('.', $Name, [IO.Pipes.PipeDirection]::InOut)
 try{
  $pipe.Connect(300);[uint32]$server=0
  if(-not [CWNative]::GetNamedPipeServerProcessId($pipe.SafePipeHandle.DangerousGetHandle(),[ref]$server) -or $server -ne $game.Id){throw 'Foreign pipe server'}
  $null=CW-Guard
  $writer=New-Object IO.StreamWriter($pipe);$writer.AutoFlush=$true
  $reader=New-Object IO.StreamReader($pipe)
  if($Command -eq 'stick R 0.35 0 180' -and -not (CW-Ready $script:armedSample $Token ([CWNative]::GetTickCount64()))){throw 'Input readiness expired during connect'}
  $writer.WriteLine($Command);$pending=$reader.ReadLineAsync()
  if(-not $pending.Wait($Timeout)){throw 'Pipe timeout; no retry'}
  return $pending.Result
 }finally{$pipe.Dispose()}
}
function CW-Snapshot {
 $reply=CW-Pipe 'nioh3bossrematch' 'cover';$s=FCP-Parse $reply
 $ready=CW-Ready $s $Token ([CWNative]::GetTickCount64())
 return @{ready=$ready;state=$s;raw=$reply}
}
$until=[DateTime]::UtcNow.AddSeconds(100)
do{$snapshot=CW-Snapshot;if(-not $snapshot.ready){Start-Sleep -Milliseconds 100}}while(-not $snapshot.ready -and [DateTime]::UtcNow -lt $until)
if(-not $snapshot.ready){throw 'Ready witness timeout; no input'}
$attempt=$snapshot.state.attempt
$output=Join-Path $RunDir ('camera-witness-'+[guid]::NewGuid().ToString('N'));$null=New-Item -ItemType Directory -Path $output
$receipt=@{token=$Token;attempt=$attempt;game_pid=$initial.games[0].pid;game_start=$initial.games[0].start;started_utc=[DateTime]::UtcNow.ToString('o');status='UNCONFIRMED';automatic_approval=$false;sample=$snapshot.state.sample}
try{
 $reply=CW-CaptureOnce -Check {
  $current=CW-Snapshot
  if(-not $current.ready -or $current.state.attempt -ne $attempt){throw 'Ready attempt changed'}
  $status=CW-Pipe 'nioh3bossrematch' 'status';FCP-CheckStatus $status
  foreach($kv in @(@('mission','0x1CBC'),@('loading','0'),@('player','1'),@('infight','1'))){if((C72-Field $status $kv[0]) -cne $kv[1]){throw 'Current game state not ready'}}
  $script:armedSample=$current.state
  if(-not (CW-Ready $current.state $Token ([CWNative]::GetTickCount64()))){throw 'Ready sample expired'}
 } -Shot {
  param($label)
  $game=CW-Guard;$bounds=[Windows.Forms.Screen]::FromHandle($game.MainWindowHandle).Bounds
  $bitmap=New-Object Drawing.Bitmap($bounds.Width,$bounds.Height);$graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.CopyFromScreen($bounds.X,$bounds.Y,0,0,$bounds.Size);$null=CW-Guard;$bitmap.Save((Join-Path $output "$label.png"),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
 } -Pulse {CW-Pipe 'nioh3vpad' 'stick R 0.35 0 180' 3800}
 $receipt.status='CAMERA_COMMAND_COMPLETED_REQUIRES_ROOT_VISUAL_REVIEW';$receipt.reply=$reply
}catch{$receipt.error="$_";throw}finally{$receipt.finished_utc=[DateTime]::UtcNow.ToString('o');$receipt|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $output 'receipt.json') -Encoding UTF8}
Write-Host "Review $output now; stage approval remains manual."
