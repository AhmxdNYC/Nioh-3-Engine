# Shared helpers for test rounds. Dot-source:  . "$PSScriptRoot\pc2_common.ps1"
$ws  = "C:\Users\User\projects\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui  = "$ws\tools\ui.fsx"
$env:FT_ALLOW_WRITE = "1"
function Close-Game { if ($global:HG) { Note-Closed }; Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Dialog { $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d" }
function U { if ($global:HG) { Assert-Owner }; dotnet fsi $ui @args 2>&1 | Out-Null }   # harness_guard.ps1 (if loaded): owner must be alive before any input

# Is the game in actual gameplay (HUD bars visible)? Optionally keeps the screenshot.
function Test-InGame([string]$keep = "") {
    $p = if ($keep) { $keep } else { "$env:TEMP\ingame_check.png" }
    U shot $p 0 0 2560 1440
    $r = py -I "$ws\patcher\ingame.py" $p
    return ($LASTEXITCODE -eq 0)
}
# Get back into gameplay from a tip screen / pause menu / dialog left by focus theft. Returns $true if in game.
function Ensure-InGame {
    for ($i = 0; $i -lt 6; $i++) {
        U focus Nioh3
        if (Test-InGame) { return $true }
        if ($i % 2 -eq 0) { U key 20 } else { U key 1B }    # Space (continue tips) / Esc (close menus)
        Start-Sleep 4
    }
    return (Test-InGame)
}
# Benchmark only when in gameplay; marks the result line INVALID otherwise.
function Bench-Checked($label) {
    if (-not (Ensure-InGame)) { "--- $label INVALID (not in gameplay)"; return }
    "--- $label $(Get-Date -f HH:mm:ss)"
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" $label 2>&1 | Where-Object { $_ -match 'displayed|GPU busy|CPU busy' }
    if (-not (Test-InGame "$ws\captures\checks\$label.png")) { "    (left gameplay during the run -> treat as INVALID)" }
}
New-Item -ItemType Directory -Force "$ws\captures\checks" | Out-Null
