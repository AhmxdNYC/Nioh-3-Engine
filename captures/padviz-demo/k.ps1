param([string]$keys = "", [int]$gap = 350, [int]$hold = 70)
# Keys to the Nioh3 window (based on captures/frame-meter-20261021/freeze/t3.ps1). Tokens: KEY, KEY*n, wNNN (sleep), shot:NAME, LMB, RMB, hold:KEY:ms
Add-Type @"
using System; using System.Runtime.InteropServices;
public class FK9 {
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte sc, uint fl, UIntPtr ex);
 [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint c, uint t);
 [DllImport("user32.dll")] public static extern void mouse_event(uint f,uint dx,uint dy,uint d,UIntPtr e);
}
"@
$D = "C:\Users\Ahmad Hamza\nioh3-engine\captures\padviz-demo"
$ff = "C:/Users/Ahmad Hamza/AppData/Local/Microsoft/WinGet/Packages/Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe/ffmpeg-9.0.2-full_build/bin/ffmpeg.exe"
$map = @{F6=0x75;PGDN=0x22;PGUP=0x21;F8=0x77;F9=0x78;F10=0x79;UP=0x26;DOWN=0x28;LEFT=0x25;RIGHT=0x27;ENTER=0x0D;ESC=0x1B;BACK=0x08;SPACE=0x20;W=0x57;A=0x41;S=0x53;D=0x44;E=0x45;Q=0x51;F=0x46;R=0x52;TAB=0x09;SHIFT=0x10;CTRL=0x11;ALT=0x12;X=0x58;C=0x43;V=0x56;G=0x47;Z=0x5A;T=0x54;I=0x49;O=0x4F;P=0x50;K=0x4B;L=0x4C;J=0x4A;H=0x48;N1=0x31;N2=0x32;N3=0x33;N4=0x34}
$p = Get-Process Nioh3 | Select-Object -First 1
[FK9]::SetForegroundWindow($p.MainWindowHandle) | Out-Null; Start-Sleep -Milliseconds 250
function Press($vk, $ms) { $sc = [FK9]::MapVirtualKey($vk, 0); $ex = 0; if (@(0x26,0x28,0x25,0x27,0x21,0x22) -contains $vk) { $ex = 1 }
  [FK9]::keybd_event($vk, $sc, $ex, [UIntPtr]::Zero); Start-Sleep -Milliseconds $ms; [FK9]::keybd_event($vk, $sc, ($ex -bor 2), [UIntPtr]::Zero) }
foreach ($k in ($keys -split ',' | Where-Object { $_ })) {
  if ($k -match '^w(\d+)$') { Start-Sleep -Milliseconds ([int]$Matches[1]); continue }
  if ($k -match '^shot:(.+)$') { & $ff -y -loglevel error -f gdigrab -framerate 5 -i desktop -frames:v 1 -vf scale=1920:1080 "$D\$($Matches[1]).jpg"; continue }
  if ($k -match '^hold:(\w+):(\d+)$') { Press $map[$Matches[1].ToUpper()] ([int]$Matches[2]); Start-Sleep -Milliseconds $gap; continue }
  $n = 1; if ($k -match '^(\w+)\*(\d+)$') { $k = $Matches[1]; $n = [int]$Matches[2] }
  for ($i = 0; $i -lt $n; $i++) {
    if ($k -eq 'LMB') { [FK9]::mouse_event(2,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds $hold; [FK9]::mouse_event(4,0,0,0,[UIntPtr]::Zero) }
    elseif ($k -eq 'RMB') { [FK9]::mouse_event(8,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds $hold; [FK9]::mouse_event(16,0,0,0,[UIntPtr]::Zero) }
    else { Press $map[$k.ToUpper()] $hold }
    Start-Sleep -Milliseconds $gap
  }
}
"ok"
