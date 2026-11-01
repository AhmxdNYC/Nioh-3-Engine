# Builds dist/Nioh3-Setup.exe with the given full-install zip embedded (human #60944).
#   powershell -ExecutionPolicy Bypass -File tools/setup/build-setup.ps1 -Zip Nioh3-1.7.90-full-install.zip
param([Parameter(Mandatory = $true)][string]$Zip, [string]$Python = '')
$ErrorActionPreference = 'Stop'
$Zip = (Resolve-Path -LiteralPath $Zip).Path
Set-Location $PSScriptRoot
$repo = (Resolve-Path ..\..).Path
& py -3 -I "$repo/release/full_install_zip.py" check $Zip
if ($LASTEXITCODE -ne 0) { throw 'Payload is not a complete full-install zip' }
if (!$Python) {
    $Python = Join-Path $PSScriptRoot '.venv-build/Scripts/python.exe'
    if (!(Test-Path -LiteralPath $Python)) {
        py -m venv .venv-build
        if ($LASTEXITCODE -ne 0) { throw 'Build venv creation failed' }
        & $Python -m pip install 'pyinstaller==6.16.0'
        if ($LASTEXITCODE -ne 0) { throw 'PyInstaller installation failed' }
    }
}
$stage = Join-Path $PSScriptRoot 'stage'
New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item -LiteralPath $Zip (Join-Path $stage 'payload.zip') -Force
$version = [regex]::Match([IO.Path]::GetFileName($Zip), '\d+\.\d+\.\d+').Value
Set-Content -LiteralPath (Join-Path $stage 'version.txt') -Value $version -Encoding ascii
& $Python -m PyInstaller --clean --noconfirm --onefile --console --name Nioh3-Setup `
    --paths "$repo/release" --paths "$repo/tools/updater" `
    --add-data "$stage/payload.zip;." --add-data "$stage/version.txt;." setup.py
if ($LASTEXITCODE -ne 0) { throw 'Setup executable build failed' }
Write-Host "Ready: $PSScriptRoot/dist/Nioh3-Setup.exe ($version)"
