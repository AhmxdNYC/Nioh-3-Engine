# upscaler_sidecar.ps1 - per-arm CPU% + screenshot for upscaler_ab.ps1 (#95480). Read-only: watches the arm's abba2 text log,
# reads Nioh3 CPU time from "cap check OK" to the s3 window end, then grabs one screenshot (after the last window, outside
# every measured span). Run: powershell -File upscaler_sidecar.ps1 -AbbaTxt <Lx-ARM.abba2.txt> -Out <dir> -Name <arm>
param([string]$AbbaTxt, [string]$Out, [string]$Name, [int]$TimeoutMin = 15)
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$end = (Get-Date).AddMinutes($TimeoutMin)
function Has($pat) { if (-not (Test-Path $AbbaTxt)) { return $false }; return [bool]((Get-Content $AbbaTxt -Raw -ErrorAction SilentlyContinue) -match $pat) }
while (-not (Has 'cap check OK')) { if ((Get-Date) -gt $end) { "timeout before cap check"; exit 1 }; Start-Sleep -Milliseconds 500 }
$p = Get-Process Nioh3 -ErrorAction Stop | Select-Object -First 1
$c0 = $p.TotalProcessorTime.TotalSeconds; $t0 = Get-Date
while (-not (Has 'window L\d+_BASE_s3 done')) { if ((Get-Date) -gt $end) { "timeout before s3"; exit 1 }; Start-Sleep -Milliseconds 300 }
$p.Refresh(); $c1 = $p.TotalProcessorTime.TotalSeconds; $t1 = Get-Date
$cores = ($c1 - $c0) / ($t1 - $t0).TotalSeconds; $n = [Environment]::ProcessorCount
$png = Join-Path $Out "$Name.png"
Add-Type -Namespace W -Name U -MemberDefinition '[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f); [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r); public struct RECT { public int L, T, R, B; } public delegate bool EP(IntPtr h, IntPtr l); [DllImport("user32.dll")] public static extern bool EnumWindows(EP cb, IntPtr l); [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid); [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h); public static IntPtr Find(uint want) { IntPtr best = IntPtr.Zero; int area = 0; EnumWindows((h, l) => { uint pid; GetWindowThreadProcessId(h, out pid); RECT r; if (pid == want && IsWindowVisible(h) && GetClientRect(h, out r) && r.R * r.B > area) { area = r.R * r.B; best = h; } return true; }, IntPtr.Zero); return best; }'
try {   # PrintWindow(PW_RENDERFULLCONTENT): grabs the game window even when it is not in front (abba2 measures unfocused)
    $h = [W.U]::Find([uint32]$p.Id); if ($h -eq [IntPtr]::Zero) { throw "no window handle" }; $r = New-Object W.U+RECT; [void][W.U]::GetClientRect($h, [ref]$r)
    $bmp = New-Object Drawing.Bitmap ([Math]::Max(1, $r.R)), ([Math]::Max(1, $r.B)); $g = [Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
    $ok = [W.U]::PrintWindow($h, $dc, 3); $g.ReleaseHdc($dc); $bmp.Save($png); $g.Dispose(); $bmp.Dispose()
    if (-not $ok) { $png = "PrintWindow failed (h=$h rect=$($r.R)x$($r.B))" }
} catch { $png = "shot failed: $_" }
$line = "{0} pid {1} span {2:N0}s cores {3:N2} cpu% {4:N1} (of {5} logical) shot {6}" -f $Name, $p.Id, ($t1 - $t0).TotalSeconds, $cores, (100 * $cores / $n), $n, $png
Add-Content (Join-Path $Out "cpu.txt") $line; $line
