$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$python = Join-Path $PSScriptRoot '.venv-build/Scripts/python.exe'
if (!(Test-Path -LiteralPath $python)) {
    py -m venv .venv-build
    if ($LASTEXITCODE -ne 0) { throw 'Build venv creation failed' }
}
& $python -m pip install 'pyinstaller==6.16.0'
if ($LASTEXITCODE -ne 0) { throw 'PyInstaller installation failed' }
& $python -m PyInstaller --clean --noconfirm --onefile --console --name Nioh3Updater updater.py
if ($LASTEXITCODE -ne 0) { throw 'Updater executable build failed' }
Write-Host "Ready: $PSScriptRoot/dist/Nioh3Updater.exe"
