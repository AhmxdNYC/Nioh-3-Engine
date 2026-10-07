<#
  rgp_daemon.ps1 - headless RGP capture daemon for Nioh 3 (Windows PowerShell 5.1, no native code).

  MUST be started BEFORE Nioh3.exe is launched: RadeonDeveloperPanelCLI hosts the developer-mode
  router; the D3D12 driver only enables SQTT tracing for a process that connects to that router
  when it creates its device. (Do NOT also run RadeonDeveloperServiceCLI - the Panel CLI then fails
  with "Failed to create router".)

  Control files (all in captures\rgp\):
    RGP_TRIGGER   create it (any content) during gameplay -> daemon sends "c" to the CLI -> new .rgp
    RGP_STOP      create it -> daemon sends "q", then kills the CLI if needed, and exits
  Status files:
    daemon.log    daemon events;  cli.log  raw CLI stdout/stderr;  daemon.state  one word
    (WAITING | CAPTURING | IDLE | STOPPED | CLI_EXITED)

  Usage:
    powershell -NoProfile -ExecutionPolicy Bypass -File rgp_daemon.ps1 [-SqttBuffer high] [-AutoFrame N]
#>
param(
  [string]$Rdts = "C:\Users\Ahmad Hamza\nioh3-engine\tools\rdts\RadeonDeveloperToolSuite-2026-05-28-1806",
  [string]$OutDir = "C:\Users\Ahmad Hamza\nioh3-engine\captures\rgp",
  [string]$Process = "Nioh3",
  [ValidateSet("minimum","low","default","high","maximum")][string]$SqttBuffer = "high",
  [int]$AutoFrame = -1,            # >=0: let the driver auto-capture absolute frame N instead of triggers
  [switch]$InstructionTracing,     # off by default: big files, not needed for per-event timing
  [int]$CaptureTimeoutSec = 180
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$log = Join-Path $OutDir "daemon.log"; $cliLog = Join-Path $OutDir "cli.log"; $stateF = Join-Path $OutDir "daemon.state"
$trig = Join-Path $OutDir "RGP_TRIGGER"; $stop = Join-Path $OutDir "RGP_STOP"
function L($m) { $s = "{0:yyyy-MM-dd HH:mm:ss} {1}" -f (Get-Date), $m; Add-Content -Path $log -Value $s -Encoding utf8 }
function S($s) { Set-Content -Path $stateF -Value $s -Encoding ascii }
Remove-Item $trig, $stop -ErrorAction SilentlyContinue

$exe = Join-Path $Rdts "RadeonDeveloperPanelCLI.exe"
$cliArgs = "-m profiling -p $Process --rgp-sqtt-buffer-size $SqttBuffer --verbose"
if ($InstructionTracing) { $cliArgs += " --rgp-instruction-tracing" }
if ($AutoFrame -ge 0) { $cliArgs += " --rgp-auto-capture=frame:$AutoFrame" }
# cmd wrapper: stdout/stderr go straight to a file (no pipe deadlocks); we keep the stdin pipe.
$psi = New-Object Diagnostics.ProcessStartInfo "cmd.exe", ("/c `"`"{0}`" {1} > `"{2}`" 2>&1`"" -f $exe, $cliArgs, $cliLog)
$psi.WorkingDirectory = $OutDir   # CLI default output name <mode>_<timestamp>.rgp lands here
$psi.UseShellExecute = $false; $psi.RedirectStandardInput = $true; $psi.CreateNoWindow = $true
$p = [Diagnostics.Process]::Start($psi)
L "started CLI (cmd pid $($p.Id)): $exe $cliArgs"
S "WAITING"

function Get-Rgps { @(Get-ChildItem $OutDir -Filter *.rgp -ErrorAction SilentlyContinue) }
$known = @{}; Get-Rgps | ForEach-Object { $known[$_.FullName] = 1 }
function Kill-Cli {
  Get-Process RadeonDeveloperPanelCLI -ErrorAction SilentlyContinue | Stop-Process -Force -Confirm:$false -ErrorAction SilentlyContinue
  if (-not $p.HasExited) { try { $p.Kill() } catch {} }
}

$capStart = $null; $attempts = 0
while ($true) {
  Start-Sleep -Milliseconds 500
  if ($p.HasExited) { L "CLI exited (code $($p.ExitCode))"; S "CLI_EXITED"; break }
  if (Test-Path $stop) {
    Remove-Item $stop -ErrorAction SilentlyContinue
    L "stop requested"; try { $p.StandardInput.WriteLine("q"); $p.StandardInput.Flush() } catch {}
    Start-Sleep 3; Kill-Cli; S "STOPPED"; L "stopped"; break
  }
  # new trace files?
  foreach ($f in Get-Rgps) {
    if (-not $known.ContainsKey($f.FullName)) {
      Start-Sleep 2  # let the writer finish
      $len1 = (Get-Item $f.FullName).Length; Start-Sleep 2; $len2 = (Get-Item $f.FullName).Length
      if ($len1 -ne $len2) { continue }
      $known[$f.FullName] = 1
      L ("NEW TRACE {0} ({1:N1} MB)" -f $f.FullName, ($len2 / 1MB))
      $capStart = $null; $attempts = 0; S "IDLE"
    }
  }
  if ($capStart -and ((Get-Date) - $capStart).TotalSeconds -gt $CaptureTimeoutSec) {
    if ($attempts -lt 3) { L "no trace after ${CaptureTimeoutSec}s - re-sending 'c'"; $p.StandardInput.WriteLine("c"); $p.StandardInput.Flush(); $capStart = Get-Date; $attempts++ }
    else { L "capture failed after retries - see cli.log"; $capStart = $null; $attempts = 0; S "IDLE" }
  }
  if ((Test-Path $trig) -and -not $capStart) {
    Remove-Item $trig -ErrorAction SilentlyContinue
    L "trigger -> sending 'c'"
    $p.StandardInput.WriteLine("c"); $p.StandardInput.Flush()
    $capStart = Get-Date; $attempts = 1; S "CAPTURING"
  }
}
