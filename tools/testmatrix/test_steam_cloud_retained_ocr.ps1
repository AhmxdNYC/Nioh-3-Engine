param([string]$Image='D:/nioh3-re/steam-inspection-20261011/play-anyway-foreground.png')
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing,System.Runtime.WindowsRuntime
. (Join-Path $PSScriptRoot 'steam_cloud_decision.ps1')
. (Join-Path $PSScriptRoot 'steam_cloud_ocr.ps1')
# Use actual helper initialization, excluding its P/Invoke/window/input portions.
$source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'steam_cloud_click.ps1'))
$begin=$source.IndexOf('$asTask =')
$end=$source.IndexOf('function Ocr-Window')
Invoke-Expression $source.Substring($begin,$end-$begin)
$bitmap=[Drawing.Bitmap]::FromFile($Image)
try {
    $o=@{rect=@{L=0;T=0};bmp=$bitmap;lines=@(SCC-OcrBitmap $bitmap)}
    if (($o.lines.Text -join ' | ') -notmatch '(?i)Unable to Sync') { throw 'Retained header not recognized' }
    $base=SCC-Button $o
    $button=SCC-EnhancedButton $o
    if (-not $button) { throw 'Enhanced OCR did not recover exact button' }
    # Assert recovered text geometry falls inside the manually reviewed retained button.
    # These coordinates are test assertions only, never inputs to production detection.
    if ($button.x -lt 1006 -or $button.x -gt 1126 -or $button.y -lt 591 -or $button.y -gt 623) { throw 'Recovered geometry misses retained button' }
    $missingHeader=@{rect=$o.rect;bmp=$bitmap;lines=@()}
    if (SCC-EnhancedButton $missingHeader) { throw 'Missing-header image accepted' }
    $o.rect=@{L=250;T=-40}
    $offset=SCC-EnhancedButton $o
    if ($offset.x -ne $button.x+250 -or $offset.y -ne $button.y-40) { throw 'Crop/window coordinate conversion failed' }
    Write-Host "RETAINED IMAGE PASS: baselineExact=$([bool]$base), enhancedExact=true, centre=$($button.x),$($button.y); missing header refused; translated window coordinates PASS. No live UI actions."
    Write-Host "SHA256=$((Get-FileHash -LiteralPath $Image -Algorithm SHA256).Hash)"
} finally { $bitmap.Dispose() }
