Add-Type @"
using System; using System.Runtime.InteropServices;
public static class I { [StructLayout(LayoutKind.Sequential)] public struct L { public uint s; public uint t; }
[DllImport("user32.dll")] public static extern bool GetLastInputInfo(ref L l);
[StructLayout(LayoutKind.Sequential)] public struct X { public uint p; public ushort b; public byte lt, rt; public short a,c,d,e; }
[DllImport("xinput1_4.dll")] public static extern uint XInputGetState(uint i, out X x); }
"@
$l = New-Object I+L; $l.s = 8; [I]::GetLastInputInfo([ref]$l) | Out-Null
"idle_s=" + [math]::Round(([Environment]::TickCount - $l.t)/1000)
$a = @(); for ($i=0;$i -lt 4;$i++){ $x = New-Object I+X; $r=[I]::XInputGetState($i,[ref]$x); $a += "${i}:${r}:$($x.p)" }
Start-Sleep 5
$b = @(); for ($i=0;$i -lt 4;$i++){ $x = New-Object I+X; $r=[I]::XInputGetState($i,[ref]$x); $b += "${i}:${r}:$($x.p)" }
"pads t0=" + ($a -join ",") + " t5=" + ($b -join ",")
