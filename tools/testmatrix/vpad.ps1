# vpad.ps1 - VirtualPad input backend for the PowerShell harnesses (dot-source AFTER harness_guard.ps1). ASI >= 1.7.37.
# No OS-level input at all: commands go through vpad.py to the mod's pipe \\.\pipe\nioh3vpad inside the game, so the game
# does NOT need the foreground window and the user can keep using the PC (typing / mouse never reach the game: the mod hides
# the real keyboard while Nioh3 is not foreground, VirtualPad = 1 hides the real pads, the unfocused game never reads the mouse).
# The guard still applies: Assert-Owner before every command, vpad.py itself exits 3 when the owner pid is gone, commit
# headroom / crash limit / pid files as before. notes\virtual-input.md.
$script:VpPy = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "vpad.py"
$script:VpUi = Join-Path (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)) "ui.fsx"

function VP-Owner { if ($global:HG) { @("--owner-pid", [string]$global:HG.ownerPid) } else { @() } }
# one command -> @{ ok; reply }. No pipe (game gone / VirtualPad off) or owner gone -> HARNESS-ABORT.
function VP {
    if ($global:HG) { Assert-Owner }
    $flat = @(); foreach ($a in $args) { foreach ($x in @($a)) { $flat += [string]$x } }
    $o = (py -I $script:VpPy @(VP-Owner) @flat 2>&1 | Out-String).Trim(); $code = $LASTEXITCODE
    if ($code -eq 2) { throw "HARNESS-ABORT: vpad pipe not available ($o)" }
    if ($code -eq 3) { throw "HARNESS-ABORT: owner gone (vpad.py)" }
    [pscustomobject]@{ ok = ($code -eq 0); reply = $o }
}
function VP-WaitPipe([int]$sec = 180) { $null = py -I $script:VpPy @(VP-Owner) --quiet wait-pipe $sec 2>&1; return ($LASTEXITCODE -eq 0) }
# boot screens -> title -> Continue -> field (A presses only while not in the field and not loading); $true when field=1 for 3 s
function VP-TitleContinue([int]$sec = 300, [string]$logTo = "") {
    $o = (py -I $script:VpPy @(VP-Owner) title-continue $sec 2>&1 | Out-String); $code = $LASTEXITCODE
    if ($logTo) { try { [IO.File]::AppendAllText($logTo, $o) } catch {} }
    if ($code -eq 3) { throw "HARNESS-ABORT: owner gone (vpad.py)" }
    return ($code -eq 0)
}
function VP-InGame { $null = py -I $script:VpPy --quiet in-game 2>&1; return ($LASTEXITCODE -eq 0) }
function VP-State { (VP state).reply }
# set a mod menu item directly (no menu navigation); returns the reply object (the mod logs the same 'MENU: <label> -> X' line)
function VP-MenuSet([string]$key, [string]$value) { VP menu set $key $value }

# [Debug] VirtualPad = 1 + KeepRunningUnfocused = 1 in ini text (same idea as Force-Acl)
function Force-Vpad([string]$t) {
    foreach ($kv in @(@("VirtualPad", "1"), @("KeepRunningUnfocused", "1"))) {
        $k = $kv[0]; $v = $kv[1]
        if ($t -match "(?m)^$k *=") { $t = $t -replace "(?m)^$k *=.*", "$k = $v" }
        elseif ($t -match '(?m)^\[Debug\]') { $t = $t -replace '(?m)^\[Debug\][ \t]*(\r?)$', "[Debug]`$1`n$k = $v`$1" }
        else { $t = $t + "`r`n[Debug]`r`n$k = $v`r`n" }
    }
    return $t
}

# The game window takes the foreground once when it is created. Give it back to whatever the user had (best effort; no
# input is sent - the same AttachThreadInput focus call ui.fsx uses for Nioh3).
function VP-ForegroundInfo {
    $o = (dotnet fsi $script:VpUi fg 2>&1 | Out-String)
    if ($o -match 'foreground: pid=(\d+) (\S+)') { @{ pid = [int]$Matches[1]; name = $Matches[2] } } else { $null }
}
function VP-GiveBackFocus($before) {
    if (-not $before -or $before.name -eq "Nioh3" -or $before.pid -le 0) { return }
    $now = VP-ForegroundInfo
    if ($now -and $now.name -eq "Nioh3") { $null = dotnet fsi $script:VpUi focus $before.name 2>&1 }
}

# ---- "test running" popup (testpopup.fsx): status + opt-in input blocker. Off by default; the USER turns blocking on. ----
# Start-TestPopup -Name abba2 [-InputMode vpad|ui] [-NoBlock] [-MaxMin N] [-HarnessPid N]: the popup exits by itself when the harness
# or owner process ends, at -MaxMin, or when Stop-TestPopup writes DONE. Set-TestStatus "step" [endTime] updates its text.
$script:PopupFsx = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "testpopup.fsx"
$script:PopupStatus = $null; $script:Popup = $null
function Write-PopupRefs {   # #r lines for the newest installed Microsoft.WindowsDesktop.App (WinForms) next to testpopup.fsx
    $root = "C:\Program Files\dotnet\shared\Microsoft.WindowsDesktop.App"
    $d = Get-ChildItem $root -Directory -ErrorAction SilentlyContinue | Sort-Object { try { [version]$_.Name } catch { [version]"0.0" } } -Descending | Select-Object -First 1
    if (-not $d) { return $false }
    $refs = "System.Windows.Forms.dll", "System.Windows.Forms.Primitives.dll", "System.Drawing.Common.dll", "System.Private.Windows.Core.dll", "System.Private.Windows.GdiPlus.dll" |
        Where-Object { Test-Path (Join-Path $d.FullName $_) } | ForEach-Object { "#r @`"$(Join-Path $d.FullName $_)`"" }
    [IO.File]::WriteAllText((Join-Path (Split-Path -Parent $script:PopupFsx) "testpopup_refs.fsx"), (($refs -join "`r`n") + "`r`n"), (New-Object Text.UTF8Encoding($false)))
    return $true
}
function Set-TestStatus([string]$step, [datetime]$endBy = [datetime]::MinValue) {
    if (-not $script:PopupStatus) { return }
    $l2 = if ($endBy -ne [datetime]::MinValue) { $endBy.ToString("o") } elseif ($global:HG) { $global:HG.deadline.ToString("o") } else { "" }
    try { [IO.File]::WriteAllText($script:PopupStatus, "$step`r`n$l2`r`n") } catch {}
}
function Start-TestPopup([string]$Name, [string]$InputMode = "vpad", [switch]$NoBlock, [double]$MaxMin = 0, [int]$HarnessPid = $PID, [string]$StatusDir = $env:TEMP) {
    if (-not (Write-PopupRefs)) { Write-Host "test popup: no Microsoft.WindowsDesktop.App runtime - skipped"; return }
    if ($MaxMin -le 0) { $MaxMin = if ($global:HG) { [Math]::Max(1, ($global:HG.deadline - (Get-Date)).TotalMinutes) } else { 180 } }
    $script:PopupStatus = Join-Path $StatusDir "nioh3_testpopup_$HarnessPid.txt"
    Set-TestStatus "starting"
    $a = @($script:PopupFsx, "--name", $Name, "--harness-pid", $HarnessPid, "--status-file", $script:PopupStatus, "--max-min", [int]$MaxMin,
           "--input", $InputMode, "--log", (Join-Path $StatusDir "nioh3_testpopup_$HarnessPid.log"))
    if ($global:HG) { $a += @("--owner-pid", $global:HG.ownerPid) }
    if ($NoBlock) { $a += "--no-block" }
    $script:Popup = Start-Process dotnet -ArgumentList (@("fsi") + ($a | ForEach-Object { if ("$_" -match '\s') { "`"$_`"" } else { "$_" } })) -WindowStyle Hidden -PassThru
    if ($global:HG) { Register-Child $script:Popup "testpopup" }
}
function Stop-TestPopup {
    Set-TestStatus "DONE"
    if ($script:Popup -and -not $script:Popup.HasExited) { if (-not $script:Popup.WaitForExit(3000)) { Stop-Process -Id $script:Popup.Id -Force -ErrorAction SilentlyContinue } }
    $script:Popup = $null
}
