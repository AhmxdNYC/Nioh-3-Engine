# steam_cloud_click.ps1 - clear Steam's "Unable to Sync" cloud dialog for hands-free launches by clicking "Play anyway".
# Steam's dialogs are CEF (no UI Automation buttons), so this OCRs Steam's own top-level windows with the built-in
# Windows OCR (Windows.Media.Ocr) and clicks only when ALL hold:
#   - the window belongs to steam / steamwebhelper and is visible,
#   - its OCR text contains "Unable to Sync" AND a word pair "Play anyway" (exact, case-insensitive),
#   - that window is in the foreground right before the click (brought there by SetForegroundWindow; verified),
#   - the user has been idle >= -MinIdleSec (default 20 s; 0 = skip the check).
# Clicks the centre of the "Play anyway" words, then restores the cursor position. Never types; never clicks anywhere else.
# The user decided (bridge #602/#603): save copies exist, launching without cloud sync is fine (single-player).
# Exit / output: 'clicked' (0), 'none' (1: no dialog found), 'skipped:<why>' (2).
#   -DryRun   : find + report the button rectangle, no click.   -Once is implied; callers poll it in their launch-wait loop.
param([switch]$DryRun, [int]$MinIdleSec = 20, [string]$SaveShot = '')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing, System.Runtime.WindowsRuntime
Add-Type @'
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class SCC {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint dx, uint dy, uint d, UIntPtr e);
  [DllImport("user32.dll")] static extern bool GetLastInputInfo(ref LII i);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
  [StructLayout(LayoutKind.Sequential)] struct LII { public uint cb; public uint t; }
  public static uint IdleMs() { LII i = new LII(); i.cb = 8; GetLastInputInfo(ref i); unchecked { return (uint)Environment.TickCount - i.t; } }
  public static List<IntPtr> Windows(HashSet<uint> pids) {
    var r = new List<IntPtr>();
    EnumWindows((h, l) => { uint p; GetWindowThreadProcessId(h, out p); if (pids.Contains(p) && IsWindowVisible(h)) r.Add(h); return true; }, IntPtr.Zero);
    return r;
  }
  // foreground without the Alt-tap trick (an Alt tap can open the task switcher): attach to the foreground thread's input
  public static bool Focus(IntPtr h) {
    IntPtr fg = GetForegroundWindow(); if (fg == h) return true;
    uint dummy; uint fgT = GetWindowThreadProcessId(fg, out dummy); uint me = GetCurrentThreadId();
    AttachThreadInput(me, fgT, true); BringWindowToTop(h); SetForegroundWindow(h); AttachThreadInput(me, fgT, false);
    return GetForegroundWindow() == h;
  }
}
'@
$asTask = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
function Await($op, [Type]$t) { $task = $asTask.MakeGenericMethod($t).Invoke($null, @($op)); $task.Wait(-1) | Out-Null; $task.Result }
$null = [Windows.Media.Ocr.OcrEngine, Windows.Foundation, ContentType = WindowsRuntime]
$null = [Windows.Graphics.Imaging.BitmapDecoder, Windows.Foundation, ContentType = WindowsRuntime]
$null = [Windows.Storage.StorageFile, Windows.Storage, ContentType = WindowsRuntime]
$engine = [Windows.Media.Ocr.OcrEngine]::TryCreateFromUserProfileLanguages()
if (-not $engine) { 'skipped:no OCR engine'; exit 2 }

function Ocr-Window($h) {   # returns @{ rect; lines } for the window's on-screen pixels
  $r = New-Object SCC+RECT; [void][SCC]::GetWindowRect($h, [ref]$r)
  $w = $r.R - $r.L; $ht = $r.B - $r.T; if ($w -lt 200 -or $ht -lt 100) { return $null }
  $bmp = New-Object System.Drawing.Bitmap $w, $ht
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size); $g.Dispose()
  $tmp = [IO.Path]::Combine($env:TEMP, "scc_$PID.png"); $bmp.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
  try {
    $file = Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($tmp)) ([Windows.Storage.StorageFile])
    $stream = Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
    $dec = Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
    $sb = Await ($dec.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
    $res = Await ($engine.RecognizeAsync($sb)) ([Windows.Media.Ocr.OcrResult])
    $stream.Dispose()
  } finally { if ($SaveShot) { Copy-Item $tmp $SaveShot -Force }; Remove-Item $tmp -ErrorAction SilentlyContinue }
  @{ rect = $r; lines = @($res.Lines); bmp = $bmp }
}

$pids = New-Object 'System.Collections.Generic.HashSet[uint32]'
Get-Process steam, steamwebhelper -ErrorAction SilentlyContinue | ForEach-Object { [void]$pids.Add([uint32]$_.Id) }
if ($pids.Count -eq 0) { 'none'; exit 1 }
foreach ($h in [SCC]::Windows($pids)) {
  $o = Ocr-Window $h; if (-not $o) { continue }
  $all = ($o.lines | ForEach-Object { $_.Text }) -join ' | '
  if ($all -notmatch '(?i)unable to sync') { continue }
  $btn = $null
  foreach ($ln in $o.lines) {
    $ws = @($ln.Words)
    for ($i = 0; $i -lt $ws.Count - 1; $i++) {
      if ($ws[$i].Text -match '^(?i)play$' -and $ws[$i + 1].Text -match '^(?i)anyway$') {
        $a = $ws[$i].BoundingRect; $b = $ws[$i + 1].BoundingRect
        $btn = @{ x = [int]($o.rect.L + ($a.X + $b.X + $b.Width) / 2); y = [int]($o.rect.T + $a.Y + $a.Height / 2) }
      }
    }
  }
  if (-not $btn) {   # OCR often misses white-on-blue 'Play anyway': find 'Cancel', then the blue button left of it on the same row
    foreach ($ln in $o.lines) { foreach ($wd in @($ln.Words)) { if ($wd.Text -match '^(?i)cancel$') {
      $cy = [int]($wd.BoundingRect.Y + $wd.BoundingRect.Height / 2); $x = [int]$wd.BoundingRect.X - 4; $run = @()
      for (; $x -gt [Math]::Max(0, $wd.BoundingRect.X - 500); $x--) {
        $c = $o.bmp.GetPixel($x, $cy); $blue = ($c.B -gt 170 -and $c.R -lt 110 -and $c.B -gt $c.G + 40)
        if ($blue) { $run += $x } elseif ($run.Count -gt 40) { break }
      }
      if ($run.Count -gt 40) { $btn = @{ x = [int]($o.rect.L + (($run | Measure-Object -Minimum -Maximum) | ForEach-Object { ($_.Minimum + $_.Maximum) / 2 })); y = [int]($o.rect.T + $cy); how = "blue button left of Cancel ($($run.Count) px)" } }
    } } }
  }
  $o.bmp.Dispose()
  if (-not $btn) { "skipped:dialog found but no 'Play anyway' button (text: $all)"; exit 2 }
  if ($DryRun) { "dryrun: would click 'Play anyway' at $($btn.x),$($btn.y) [$($btn.how)] in window 0x$('{0:X}' -f [int64]$h) (text: $all)"; exit 0 }
  if ($MinIdleSec -gt 0 -and [SCC]::IdleMs() -lt $MinIdleSec * 1000) { "skipped:user active (idle $([int]([SCC]::IdleMs()/1000)) s < $MinIdleSec s)"; exit 2 }
  if (-not [SCC]::Focus($h)) { 'skipped:could not bring the Steam dialog to the foreground'; exit 2 }
  Start-Sleep -Milliseconds 300
  if ([SCC]::GetForegroundWindow() -ne $h) { 'skipped:Steam dialog lost the foreground'; exit 2 }
  $old = New-Object SCC+POINT; [void][SCC]::GetCursorPos([ref]$old)
  [void][SCC]::SetCursorPos($btn.x, $btn.y); Start-Sleep -Milliseconds 80
  [SCC]::mouse_event(0x0002, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 40; [SCC]::mouse_event(0x0004, 0, 0, 0, [UIntPtr]::Zero)
  Start-Sleep -Milliseconds 80; [void][SCC]::SetCursorPos($old.X, $old.Y)
  "clicked 'Play anyway' at $($btn.x),$($btn.y)"; exit 0
}
'none'; exit 1
