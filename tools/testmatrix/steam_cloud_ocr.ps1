# Bitmap-only OCR helpers. No window discovery, screen capture, focus or input.
# Caller supplies the initialized WinRT engine and Await helper.
function SCC-OcrBitmap($Bitmap) {
    $temporary=Join-Path ([IO.Path]::GetTempPath()) ('scc-ocr-'+[guid]::NewGuid().ToString('N')+'.png')
    $stream=$null; $software=$null
    try {
        $Bitmap.Save($temporary,[Drawing.Imaging.ImageFormat]::Png)
        $file=Await ([Windows.Storage.StorageFile]::GetFileFromPathAsync($temporary)) ([Windows.Storage.StorageFile])
        $stream=Await ($file.OpenAsync([Windows.Storage.FileAccessMode]::Read)) ([Windows.Storage.Streams.IRandomAccessStream])
        $decoder=Await ([Windows.Graphics.Imaging.BitmapDecoder]::CreateAsync($stream)) ([Windows.Graphics.Imaging.BitmapDecoder])
        $software=Await ($decoder.GetSoftwareBitmapAsync()) ([Windows.Graphics.Imaging.SoftwareBitmap])
        $result=Await ($engine.RecognizeAsync($software)) ([Windows.Media.Ocr.OcrResult])
        return @($result.Lines)
    } finally {
        if ($software) { $software.Dispose() }
        if ($stream) { $stream.Dispose() }
        if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary }
    }
}
function SCC-EnhancedButton($Observation) {
    $button=SCC-Button $Observation
    if ($button) { return $button }
    $header=@($Observation.lines | Where-Object { $_.Text -match '(?i)^\s*Unable to Sync\s*$' })
    if ($header.Count -ne 1) { return $null }
    $words=@($header[0].Words)
    if (-not $words.Count) { return $null }
    # Region derives from recognized heading geometry, never remembered click coordinates.
    $left=[Math]::Max(0,[int](($words | ForEach-Object { $_.BoundingRect.X } | Measure-Object -Minimum).Minimum)-80)
    $top=[Math]::Max(0,[int](($words | ForEach-Object { $_.BoundingRect.Y } | Measure-Object -Minimum).Minimum)-40)
    $width=[Math]::Min(1000,$Observation.bmp.Width-$left)
    $height=[Math]::Min(450,$Observation.bmp.Height-$top)
    if ($width -le 0 -or $height -le 0) { return $null }
    $scale=2
    if ($width*$scale -gt [Windows.Media.Ocr.OcrEngine]::MaxImageDimension -or
        $height*$scale -gt [Windows.Media.Ocr.OcrEngine]::MaxImageDimension) { return $null }
    $crop=New-Object Drawing.Bitmap ($width*$scale),($height*$scale)
    $graphics=[Drawing.Graphics]::FromImage($crop)
    try {
        $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.DrawImage($Observation.bmp,(New-Object Drawing.Rectangle 0,0,($width*$scale),($height*$scale)),
            (New-Object Drawing.Rectangle $left,$top,$width,$height),[Drawing.GraphicsUnit]::Pixel)
        $lines=@(SCC-OcrBitmap $crop)
        # Require heading again in this crop, then exact adjacent Play/anyway words.
        if (($lines.Text -join ' | ') -notmatch '(?i)\bUnable to Sync\b') { return $null }
        $local=SCC-Button @{rect=@{L=0;T=0};lines=$lines}
        if (-not $local) { return $null }
        return @{x=[int]($Observation.rect.L+$left+$local.x/$scale)
            y=[int]($Observation.rect.T+$top+$local.y/$scale);how='heading crop OCR 2x'}
    } finally { $graphics.Dispose(); $crop.Dispose() }
}
