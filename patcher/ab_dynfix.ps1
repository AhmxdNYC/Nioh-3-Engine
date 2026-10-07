$ws  = "C:\Users\Ahmad Hamza\nioh3-engine"
$cfg = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3\config.xml"
Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" -Revert | Out-Null
if (Test-Path "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\dxgi.dll") { "ABORT optiscaler present"; exit 1 }
Copy-Item "$ws\configs\default-nofg-120.xml" $cfg -Force
& powershell -NoProfile -File "$ws\tools\enter_game.ps1"
Start-Sleep 20
function B($l) { "--- $l $(Get-Date -f HH:mm:ss)"; dotnet fsi "$ws\patcher\pace_sample.fsx" 5 2>&1 | Where-Object { $_ -match 'cfg=|x fps' } | Select-Object -First 4; & powershell -NoProfile -File "$ws\patcher\bench.ps1" $l }
B "fix-stock1"
& powershell -NoProfile -File "$ws\patcher\patch_all.ps1"; Start-Sleep 8
B "fix-patched1"
& powershell -NoProfile -File "$ws\patcher\patch_all.ps1" -Revert; Start-Sleep 8
B "fix-stock2"
& powershell -NoProfile -File "$ws\patcher\patch_all.ps1"; Start-Sleep 8
B "fix-patched2"
$d = dotnet fsi "$ws\tools\ui.fsx" dialog Nioh3 2>&1 | Select-String 'dialogs:'; "$d"
Get-Process Nioh3 -ErrorAction SilentlyContinue | Stop-Process -Force; Start-Sleep 12
& powershell -NoProfile -File "$ws\optiscaler\apply.ps1" | Out-Null
Copy-Item "$ws\configs\main-240-fg.xml" $cfg -Force
"restored OptiScaler + main-240-fg"
