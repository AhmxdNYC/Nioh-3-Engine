param([Parameter(Mandatory=$true)][string]$GameDir,
      [Parameter(Mandatory=$true)][string]$Output,
      [Parameter(Mandatory=$true)][ValidateSet('A','B')][string]$Arm,
      [Parameter(Mandatory=$true)][string]$Scene,
      [ValidateRange(10,30)][int]$Seconds=25)
$ErrorActionPreference='Stop'
$expected=if($Arm -eq 'A'){'c32b5ae870a80f68f8fced65e85ff4306a81ba2ab5dddb6983aff80c652c817a'}else{'d8511aa0bea0d778de9e8d29342470cec644fe907559415ec0bafdfc9f0844d5'}
$target=Join-Path $GameDir 'shader_dx12\0xb4751b0f.file'
if((Get-FileHash -LiteralPath $target).Hash.ToLower() -ne $expected){throw 'Wrong arm installed'}
$games=@(Get-Process Nioh3 -ErrorAction SilentlyContinue)
if($games.Count -ne 1){throw 'Exactly one live Nioh3 process required'}
$gp=$games[0];$started=$gp.StartTime.ToUniversalTime().ToString('o')
if(Test-Path -LiteralPath $Output){throw 'Output already exists'}
New-Item -ItemType Directory -Path $Output | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;using System.Runtime.InteropServices;
public class PB2Shot {
 [StructLayout(LayoutKind.Sequential)]public struct RECT{public int L,T,R,B;}
 [DllImport("user32.dll")]public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")]public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32.dll")]public static extern bool GetWindowRect(IntPtr w,out RECT r);
 [DllImport("user32.dll")]public static extern bool SetProcessDPIAware();
}
'@
[void][PB2Shot]::SetProcessDPIAware()
function Shot([string]$Name){
 $hw=[PB2Shot]::GetForegroundWindow();[uint32]$foregroundPid=0
 [void][PB2Shot]::GetWindowThreadProcessId($hw,[ref]$foregroundPid)
 if($foregroundPid -ne $gp.Id){throw 'Game must already be foreground; no automatic focus/input'}
 $rect=New-Object PB2Shot+RECT
 if(-not [PB2Shot]::GetWindowRect($hw,[ref]$rect)){throw 'Cannot locate game window'}
 $bmp=New-Object Drawing.Bitmap ($rect.R-$rect.L),($rect.B-$rect.T)
 $graphics=[Drawing.Graphics]::FromImage($bmp)
 try{$graphics.CopyFromScreen($rect.L,$rect.T,0,0,$bmp.Size);$bmp.Save((Join-Path $Output $Name),[Drawing.Imaging.ImageFormat]::Png)}
 finally{$graphics.Dispose();$bmp.Dispose()}
}
$session='PB2_'+[guid]::NewGuid().ToString('N')
$csv=Join-Path $Output 'frames.csv'
Shot 'before.png'
$argsLine="--process_id $($gp.Id) --output_file `"$csv`" --v2_metrics --date_time --timed $Seconds --terminate_after_timed --session_name $session --no_console_stats"
$pm=Start-Process (Join-Path $PSScriptRoot 'PresentMon.exe') -ArgumentList $argsLine -WindowStyle Hidden -PassThru
try{
 if(-not $pm.WaitForExit(($Seconds+10)*1000)){Stop-Process -Id $pm.Id;throw 'Collector timed out'}
 if($pm.ExitCode -ne 0 -or -not(Test-Path -LiteralPath $csv)){throw 'PresentMon failed'}
 $now=Get-Process -Id $gp.Id -ErrorAction Stop
 if($now.StartTime.ToUniversalTime().ToString('o') -ne $started){throw 'Game process replaced'}
 if((Get-FileHash -LiteralPath $target).Hash.ToLower() -ne $expected){throw 'Shader changed during window'}
 Shot 'after.png'
 [ordered]@{arm=$Arm;pid=$gp.Id;process_start_utc=$started;shader_sha256=$expected;seconds=$Seconds;session=$session;scene_operator_label=$Scene;scene_match_verified=$false;candidate_exercised_verified=$false;performance_verdict='UNASSESSED';native_visual_verdict='UNASSESSED'} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Output 'window.json')
}finally{if(-not $pm.HasExited){Stop-Process -Id $pm.Id}}
