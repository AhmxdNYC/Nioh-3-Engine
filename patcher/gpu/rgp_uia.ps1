<#
  rgp_uia.ps1 - drive the RGP GUI through UI Automation (no mouse/keyboard injection; the window
  can stay minimized) to read AMD's own numbers out of RadeonGPUProfiler.exe.
  Used as ground truth / fallback for rgp_report.py.

    rgp_uia.ps1 -Trace x.rgp [-Pane "Most expensive events"] [-Out dump.txt] [-Keep]
    rgp_uia.ps1 -AttachPid 1234 -Pane "Barriers" -Out b.txt

  Panes (OVERVIEW tab list items): "Frame summary","Barriers","Context rolls","Most expensive events",
  "Render/depth targets","Pipelines". EVENTS tab items: "Wavefront occupancy","Event timing",
  "Pipeline state","Instruction timing". Dumps every UIA element (type | name | automationId tail).
#>
param(
  [string]$Trace, [int]$AttachPid = 0, [string[]]$Pane = @("Frame summary"), [string]$Out,
  [switch]$Keep, [int]$LoadWaitSec = 20,
  [string]$Rdts = "C:\Users\Ahmad Hamza\nioh3-engine\tools\rdts\RadeonDeveloperToolSuite-2026-05-28-1806"
)
Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
$A = [Windows.Automation.AutomationElement]; $TS = [Windows.Automation.TreeScope]
if ($AttachPid -eq 0) {
  $p = Start-Process "$Rdts\RadeonGPUProfiler.exe" -ArgumentList "`"$Trace`"" -WindowStyle Minimized -PassThru
  $AttachPid = $p.Id; Start-Sleep $LoadWaitSec
}
$win = $A::RootElement.FindFirst($TS::Children, (New-Object Windows.Automation.PropertyCondition($A::ProcessIdProperty, $AttachPid)))
if (-not $win) { throw "RGP window for pid $AttachPid not found" }
function Click($name) {
  $e = $win.FindFirst($TS::Descendants, (New-Object Windows.Automation.PropertyCondition($A::NameProperty, $name)))
  if (-not $e) { Write-Warning "element '$name' not found"; return }
  foreach ($pat in @([Windows.Automation.InvokePattern]::Pattern, [Windows.Automation.SelectionItemPattern]::Pattern)) {
    try { $o = $e.GetCurrentPattern($pat); if ($pat -eq [Windows.Automation.InvokePattern]::Pattern) { $o.Invoke() } else { $o.Select() }; return } catch {}
  }
}
$lines = New-Object System.Collections.Generic.List[string]
foreach ($pn in $Pane) {
  if ($pn -match '^(Wavefront occupancy|Event timing|Pipeline state|Instruction timing)$') { Click "EVENTS" } else { Click "OVERVIEW" }
  Start-Sleep 1; Click $pn; Start-Sleep 3
  $lines.Add("##### PANE: $pn")
  foreach ($e in $win.FindAll($TS::Descendants, [Windows.Automation.Condition]::TrueCondition)) {
    try {
      $c = $e.Current
      if (-not $c.IsOffscreen -or $c.ControlType.ProgrammaticName -match 'DataItem|Header|TreeItem') {
        $id = $c.AutomationId -replace '^.*\.', ''
        if ($c.Name -or $id) { $lines.Add(("{0} | {1} | {2}" -f ($c.ControlType.ProgrammaticName -replace 'ControlType\.', ''), $c.Name, $id)) }
      }
    } catch {}
  }
}
if ($Out) { $lines | Set-Content -Path $Out -Encoding utf8 } else { $lines }
if (-not $Keep) { Stop-Process -Id $AttachPid -Force -ErrorAction SilentlyContinue }
