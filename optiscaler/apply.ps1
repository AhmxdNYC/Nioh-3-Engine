# Install / remove OptiScaler (FSR4 via XeSS inputs) for Nioh 3.
# usage: powershell -File apply.ps1 [-Revert]
# Only adds files the game does not ship; never overwrites a game file.
param([switch]$Revert)
$game = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$src  = "C:\Users\Ahmad Hamza\nioh3-engine\optiscaler\files"
$map  = [ordered]@{
    "OptiScaler.dll"                          = "dxgi.dll"
    "OptiScaler.ini"                          = "OptiScaler.ini"
    "amd_fidelityfx_dx12.dll"                 = "amd_fidelityfx_dx12.dll"
    "amd_fidelityfx_upscaler_dx12.dll"        = "amd_fidelityfx_upscaler_dx12.dll"
    "amd_fidelityfx_framegeneration_dx12.dll" = "amd_fidelityfx_framegeneration_dx12.dll"
}
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Close Nioh 3 first." }
foreach ($kv in $map.GetEnumerator()) {
    $dst = Join-Path $game $kv.Value
    if ($Revert) {
        if (Test-Path $dst) { Remove-Item $dst; "removed $($kv.Value)" }
    } else {
        if ((Test-Path $dst) -and -not (Test-Path "$src\.installed")) { throw "$($kv.Value) already exists in the game folder - refusing to overwrite." }
        Copy-Item (Join-Path $src $kv.Key) $dst -Force; "installed $($kv.Value)"
    }
}
if ($Revert) { Remove-Item "$src\.installed" -ErrorAction SilentlyContinue } else { New-Item -ItemType File -Force "$src\.installed" | Out-Null }
