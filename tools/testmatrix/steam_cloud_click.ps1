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
# Background CopyFromScreen pixels are untrusted. Optional ExpectedLaunchHwnd/Pid
# identify root's pending launch; only a Steam Launching/Unable to Sync title may
# be raised once after the idle gate. DryRun never raises. Fresh foreground OCR
# must still identify both exact texts; a blank CEF window remains unconfirmed.
param([switch]$DryRun, [ValidateRange(0,2147483)][int]$MinIdleSec = 20, [string]$SaveShot = '',
  [long]$ExpectedLaunchHwnd = 0, [int]$ExpectedLaunchPid = 0)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'steam_cloud_decision.ps1')
if (($ExpectedLaunchHwnd -ne 0) -ne ($ExpectedLaunchPid -ne 0)) { throw 'Expected launch HWND and PID must be supplied together' }
Add-Type -AssemblyName System.Drawing, System.Runtime.WindowsRuntime
Add-Type @'
using System; using System.Text; using System.Runtime.InteropServices; using System.Collections.Generic;
public static class SCC {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetWindowText(IntPtr h, StringBuilder text, int size);
  public static string Title(IntPtr h) { var s=new StringBuilder(1024); GetWindowText(h,s,s.Capacity); return s.ToString(); }
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

function Steam-Owned($h,$pids) {
  [uint32]$windowPid=0
  [void][SCC]::GetWindowThreadProcessId($h,[ref]$windowPid)
  if (-not $pids.Contains($windowPid) -or -not [SCC]::IsWindowVisible($h)) { return $false }
  if ($ExpectedLaunchHwnd -and ($h -ne [IntPtr]$ExpectedLaunchHwnd -or $windowPid -ne $ExpectedLaunchPid)) { return $false }
  $p=Get-Process -Id $windowPid -ErrorAction SilentlyContinue
  return $p -and $p.ProcessName -in @('steam','steamwebhelper')
}
function Read-Candidate($h,$pids,[bool]$PlanOnly) {
  $owned=Steam-Owned $h $pids
  $foreground=[SCC]::GetForegroundWindow() -eq $h
  if (-not $owned -or -not $foreground) {
    return @{decision=(SCC-Decision $owned $foreground $false $false $PlanOnly 0 $MinIdleSec);button=$null;text=''}
  }
  $o=Ocr-Window $h
  if (-not $o) { return @{decision='no-dialog';button=$null;text=''} }
  try {
    $all=($o.lines | ForEach-Object { $_.Text }) -join ' | '
    $button=SCC-Button $o
    # OCR can outlast a foreground change. Never attribute those pixels to Steam.
    $owned=Steam-Owned $h $pids
    $foreground=[SCC]::GetForegroundWindow() -eq $h
    $decision=SCC-Decision $owned $foreground ($all -match '(?i)unable to sync') ([bool]$button) $PlanOnly ([SCC]::IdleMs()) $MinIdleSec
    return @{decision=$decision;button=$button;text=$all}
  } finally { $o.bmp.Dispose() }
}
$pids = New-Object 'System.Collections.Generic.HashSet[uint32]'
Get-Process steam, steamwebhelper -ErrorAction SilentlyContinue | ForEach-Object { [void]$pids.Add([uint32]$_.Id) }
if ($pids.Count -eq 0) { 'none'; exit 1 }
if ($ExpectedLaunchHwnd) {
  # Root must supply the HWND/PID from its own pending launch. Never pick Steam main/settings by OCR.
  $expected=[IntPtr]$ExpectedLaunchHwnd; [uint32]$windowPid=0
  [void][SCC]::GetWindowThreadProcessId($expected,[ref]$windowPid)
  $reveal=SCC-RevealDecision (Steam-Owned $expected $pids) ($windowPid -eq $ExpectedLaunchPid) `
    ([SCC]::Title($expected) -match '^(?i)(Launching\b|Unable to Sync\b)') `
    ([SCC]::GetForegroundWindow() -eq $expected) ([bool]$DryRun) ([SCC]::IdleMs()) $MinIdleSec
  if ($reveal -eq 'dryrun-reveal') { 'dryrun: would raise the explicitly identified Steam launch window once; dialog/button remain unconfirmed'; exit 2 }
  if ($reveal -eq 'reveal-once') {
    if (-not [SCC]::Focus($expected)) { 'skipped:expected Steam launch window could not be raised'; exit 2 }
    Start-Sleep -Milliseconds 300
  } elseif ($reveal -ne 'already-foreground') { "skipped:$reveal"; exit 2 }
}
$unverified=0
foreach ($h in [SCC]::Windows($pids)) {
  if ($ExpectedLaunchHwnd -and $h -ne [IntPtr]$ExpectedLaunchHwnd) { continue }
  $candidate=Read-Candidate $h $pids ([bool]$DryRun)
  if ($candidate.decision -eq 'unverified-background') { $unverified++; continue }
  if ($candidate.decision -in @('foreign','no-dialog')) { continue }
  if ($candidate.decision -eq 'dryrun') {
    "dryrun: exact 'Play anyway' at $($candidate.button.x),$($candidate.button.y) in foreground Steam window 0x$('{0:X}' -f [int64]$h)"; exit 0
  }
  if ($candidate.decision -ne 'click-candidate') { "skipped:$($candidate.decision)"; exit 2 }
  if (-not [SCC]::Focus($h)) { 'skipped:could not verify foreground Steam dialog'; exit 2 }
  Start-Sleep -Milliseconds 300
  # Re-read after focus/wait; previous rectangles are never used for input.
  $candidate=Read-Candidate $h $pids $false
  if ($candidate.decision -ne 'click-candidate') { "skipped:recheck $($candidate.decision)"; exit 2 }
  $btn=$candidate.button
  $old=New-Object SCC+POINT; [void][SCC]::GetCursorPos([ref]$old)
  try {
    [void][SCC]::SetCursorPos($btn.x,$btn.y); Start-Sleep -Milliseconds 80
    if (-not (Steam-Owned $h $pids) -or [SCC]::GetForegroundWindow() -ne $h) { 'skipped:Steam dialog lost ownership/foreground'; exit 2 }
    [SCC]::mouse_event(0x0002,0,0,0,[UIntPtr]::Zero); Start-Sleep -Milliseconds 40
    [SCC]::mouse_event(0x0004,0,0,0,[UIntPtr]::Zero)
  } finally { [void][SCC]::SetCursorPos($old.X,$old.Y) }
  "clicked 'Play anyway' at $($btn.x),$($btn.y)"; exit 0
}
if ($unverified) { "skipped:$unverified background Steam windows cannot be verified by screen OCR; covered or blank dialogs are unconfirmed"; exit 2 }
'none'; exit 1
