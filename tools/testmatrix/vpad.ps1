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
