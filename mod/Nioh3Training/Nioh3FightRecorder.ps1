param([int]$GamePid = 0, [string]$OutRoot = "", [int]$MaxMinutes = 20, [int]$IntervalMs = 330, [int]$Quality = 55)
# Nioh3FightRecorder.ps1 (user #9291/#9296): started by Nioh3Training ([Record] Enable = 1) at every Boss Rematch Fight now.
# Captures the primary screen at ~3 fps, half size, jpg quality 55, into $OutRoot\rec_<stamp>\f_HHmmss_fff.jpg.
# Stops at: Nioh3Training.log 'RETURN done', the game process exiting, 90 s after a fight end ('BBM END' / 'player beaten') with no new
# 'FADE to black' / 'RETURN home stored', $MaxMinutes, or a file 'stop' in the output folder. Only one recorder runs (named mutex).
$ErrorActionPreference = "SilentlyContinue"
$m = New-Object Threading.Mutex($false, "Local\Nioh3FightRecorder"); if (-not $m.WaitOne(0)) { exit 0 }
$gd = Split-Path -Parent $MyInvocation.MyCommand.Path; if (-not $OutRoot) { $OutRoot = $gd }
$log = Join-Path $gd "Nioh3Training.log"
$dir = Join-Path $OutRoot ("rec_" + (Get-Date -f yyyyMMdd_HHmmss)); New-Item -ItemType Directory -Force $dir | Out-Null
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
$b = [Windows.Forms.SystemInformation]::PrimaryMonitorSize
$enc = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$ep = New-Object Drawing.Imaging.EncoderParameters 1; $ep.Param[0] = New-Object Drawing.Imaging.EncoderParameter([Drawing.Imaging.Encoder]::Quality, [long]$Quality)
$off = if (Test-Path $log) { (Get-Item $log).Length } else { 0 }
$end = (Get-Date).AddMinutes($MaxMinutes); $fightEnd = $null; $n = 0; $why = "max minutes"
"start $(Get-Date -f s) game pid $GamePid" | Set-Content (Join-Path $dir "rec.txt")
$bm = New-Object Drawing.Bitmap $b.Width, $b.Height; $g = [Drawing.Graphics]::FromImage($bm)
$s = New-Object Drawing.Bitmap ([int]($b.Width / 2)), ([int]($b.Height / 2)); $g2 = [Drawing.Graphics]::FromImage($s)
$g2.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::Bilinear
while ((Get-Date) -lt $end) {
    if (Test-Path (Join-Path $dir "stop")) { $why = "stop file"; break }
    if ($GamePid -and -not (Get-Process -Id $GamePid -ErrorAction SilentlyContinue)) { $why = "game exited"; break }
    if ((Test-Path $log) -and (Get-Item $log).Length -gt $off) {
        $fs = [IO.File]::Open($log, 'Open', 'Read', 'ReadWrite'); $fs.Seek($off, 'Begin') | Out-Null; $sr = New-Object IO.StreamReader($fs); $t = $sr.ReadToEnd(); $off = $fs.Position; $fs.Close()
        if ($t -match 'RETURN done') { $why = "RETURN done"; break }
        if ($t -match 'BBM END|player beaten') { $fightEnd = Get-Date }
        if ($t -match 'FADE to black \(cross-world switch\)|RETURN home stored|ARENA start') { $fightEnd = $null }
    }
    if ($fightEnd -and ((Get-Date) - $fightEnd).TotalSeconds -gt 90) { $why = "90 s after the fight ended"; break }
    try { $g.CopyFromScreen(0, 0, 0, 0, $bm.Size); $g2.DrawImage($bm, 0, 0, $s.Width, $s.Height); $s.Save((Join-Path $dir ("f_{0}.jpg" -f (Get-Date -f HHmmss_fff))), $enc, $ep); $n++ } catch { }
    Start-Sleep -Milliseconds $IntervalMs
}
$g.Dispose(); $g2.Dispose(); $bm.Dispose(); $s.Dispose()
"stop $(Get-Date -f s): $why, $n frames" | Add-Content (Join-Path $dir "rec.txt")
$m.ReleaseMutex()
