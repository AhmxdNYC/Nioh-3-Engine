$ErrorActionPreference = "Continue"
$tm = "C:\Users\Ahmad Hamza\nioh3-engine\tools\testmatrix"
. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$out = "C:\Users\Ahmad Hamza\nioh3-engine\captures\merged-verify-20261023"; New-Item -ItemType Directory -Force $out | Out-Null
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; Add-Content "$out\trace.txt" $l; Write-Host $l }
function RMP([string]$cmd, [int]$timeoutMs = 5000) {
    $p = New-Object IO.Pipes.NamedPipeClientStream(".", "nioh3bossrematch", [IO.Pipes.PipeDirection]::InOut)
    try { $p.Connect($timeoutMs); $w = New-Object IO.StreamWriter($p); $w.AutoFlush = $true; $rd = New-Object IO.StreamReader($p)
        $w.WriteLine($cmd); $t = $rd.ReadLineAsync(); if (-not $t.Wait($timeoutMs)) { return "error timeout" }; return $t.Result
    } catch { return "error $($_.Exception.Message)" } finally { $p.Dispose() }
}
function F([string]$reply, [string]$key) { if ($reply -match "(?:^| )$key=(\S+)") { return $Matches[1] }; return "" }
function Shot([string]$name) {
    Add-Type -AssemblyName System.Windows.Forms, System.Drawing
    $b = [Windows.Forms.SystemInformation]::VirtualScreen; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
    $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $bmp.Save("$out\$name.png", [Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
}
if (-not (VP-WaitPipe 240)) { T "no VirtualPad pipe"; exit 1 }
$ok = VP-TitleContinue 300 "$out\title_continue.txt"; T "title->continue: $ok"
$t0 = Get-Date; $s = ""
do { $s = RMP status; Start-Sleep 2 } while (((F $s 'player') -ne '1' -or (F $s 'loading') -ne '0') -and ((Get-Date) - $t0).TotalSeconds -lt 150)
T "status: $s"
Start-Sleep 5
Shot "field_padstyle"
$q = RMP "fight R7B35"; T "fight: $q"
Start-Sleep 25
$st = RMP status; T "status in fight: $st"
Shot "fight_1"
$null = VP "press RB 80"; Start-Sleep 1; $null = VP "press RB 80"; Start-Sleep 1
Shot "fight_2_pressed"
$null = VP "hold RB"; Start-Sleep 1; Shot "fight_3_holdRB"; $null = VP "release RB"
T "done"
