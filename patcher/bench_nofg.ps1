$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
function Close-Game { Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12 }
function Run-One($label, $config, [switch]$Opti, [switch]$Patch) {
    Close-Game
    if ($Opti) { & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null } else { & powershell -NoProfile -File "$ws\optiscaler\apply.ps1" -Revert | Out-Null }
    $dx = Test-Path "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\dxgi.dll"
    if ($dx -ne [bool]$Opti) { "ABORT: OptiScaler present=$dx but wanted $Opti"; exit 1 }
    Copy-Item "$ws\configs\$config" $cfg -Force
    "=== $label ($config, OptiScaler dxgi.dll present=$dx, patches=$Patch) $(Get-Date -f HH:mm)"
    & powershell -NoProfile -File "$ws\tools\enter_game.ps1"
    Start-Sleep 20
    if ($Patch) { & powershell -NoProfile -File "$ws\patcher\patch_all.ps1"; Start-Sleep 10 }
    $d = dotnet fsi "$ws\tools\ui.fsx" dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d"
    & powershell -NoProfile -File "$ws\patcher\bench.ps1" $label
}
Run-One "final4-default-nofg60"  "default-nofg-60.xml"
Run-One "final5-default-nofg120" "default-nofg-120.xml"
Run-One "final6-mod-nofg"        "main-240.xml" -Opti -Patch
Close-Game
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"restored OptiScaler + main-240-fg; done $(Get-Date -f HH:mm)"

