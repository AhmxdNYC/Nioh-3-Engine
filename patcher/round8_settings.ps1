# Round 8: cost-vs-looks sweep. Modified preset (configs\main-240.xml, no FG, OptiScaler FSR4) as the base; each
# launch lowers ONE setting one step. Per launch: screenshot from the untouched load-in camera (for the image
# diff), 20 s PresentMon bench (GPU/CPU busy), CPU cores. The panel (running) applies the live fixes.
# Output: captures\sweep\<name>.png, captures\bench-r8-<name>.csv, task log.
$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
$ui  = "$ws\tools\ui.fsx"
$out = "$ws\captures\sweep"
New-Item -ItemType Directory -Force $out | Out-Null
$base = Get-Content "$ws\configs\main-240.xml" -Raw
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Run-Launch($name, $key, $value) {
    Close-Game
    $x = $base
    if ($key) {
        $old = [regex]::Match($x, "<$key>([^<]*)</$key>").Groups[1].Value
        $x = $x -replace "<$key>[^<]*</$key>", "<$key>$value</$key>"
        "=== $name  $key $old -> $value  $(Get-Date -f HH:mm:ss)"
    } else { "=== $name  baseline  $(Get-Date -f HH:mm:ss)" }
    [IO.File]::WriteAllText($cfg, $x)
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1" | Out-Null
    Start-Sleep 25     # panel applies the live fixes ~20 s after the window appears
    if (-not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) { "GAME EXITED"; return }
    $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d"
    dotnet fsi $ui shot "$out\$name.png" 0 0 2560 1440 2>&1 | Out-Null
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" "r8-$name" 2>&1 | Where-Object { $_ -match 'displayed|GPU busy|CPU busy' }
    & powershell -NoProfile -File "$ws\patcher\cpuuse.ps1" 10
}
$variants = @(
    @("base1", $null, $null),
    @("shadow", "SHADOW_QUAL", "STANDARD"),
    @("ao", "AMBIENT_OCCLUSION", "STANDARD"),
    @("gi", "GI", "STANDARD"),
    @("ssr", "SSR", "STANDARD"),
    @("volumetric", "VOLUMETRIC", "STANDARD"),
    @("landscape", "LANDSCAPE_QUALITY", "NORMAL"),
    @("postres", "POSTEFFECT_RESOLUTION", "STANDARD"),
    @("colorprec", "COLOR_PRECISION", "MIDDLE"),
    @("meshlet", "MESHLET_QUALITY", "STANDARD"),
    @("base2", $null, $null),
    @("grass", "GRASS_DENSITY", "NORMAL"),
    @("modelqual", "MODEL_QUAL", "STANDARD"),
    @("aniso", "ANISOTROPIC_FILTER", "8"),
    @("sss", "SSS", "OFF"),
    @("motionq", "MOTION_QUALITY", "LIGHT"),
    @("wind", "WIND_SENSITIVITY", "LOW"),
    @("vfx", "VISUAL_EFFECT", "LIMITED"),
    @("modeldraw", "MODEL_DRAW", "FEW"),
    @("motionblur", "MOTION_BLUR", "OFF"),
    @("dof", "DEPTH_OF_FIELD", "OFF"),
    @("bloom", "BLOOM", "OFF"),
    @("base3", $null, $null)
)
foreach ($v in $variants) { Run-Launch $v[0] $v[1] $v[2] }
Close-Game
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"round8 done $(Get-Date -f HH:mm:ss)"
