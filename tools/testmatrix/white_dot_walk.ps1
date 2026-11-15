# white_dot_walk.ps1 - walk the player to the game's own white dot, then hold RB for Proceed (#106000).
# Target = newest "Rematch: white dot x y z" line in Nioh3BossRematch.log (tools/testmatrix/white_dot.py); no per-Dojo table.
# Only for a test that already holds the machine's game lock (Development live test seat). Never starts or stops the game.
# Walk: virtual pad left stick, basis solved from the last real move (the stick-right = camera-left mirror, see Temp/dojo98893).
# Fallback when the walk stalls: Training test warp to the SAME logged point (-NoWarp to report the stall instead).
param([string]$Row = "", [int]$Sec = 25, [double]$Near = 150, [switch]$NoWarp, [switch]$NoProceed,
      [string]$Game = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3", [string]$Trace = "")
$tm = $PSScriptRoot; . "$tm\vpad.ps1"
if (-not $Trace) { $Trace = Join-Path $env:TEMP "white_dot_walk.txt" }
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($Trace, $l + "`r`n"); $l }
function Pos { $s = VP-State; if ($s -match 'pos=([-\d.]+),([-\d.]+),([-\d.]+)') { return @([double]$matches[1], [double]$matches[2], [double]$matches[3]) }; return $null }
function WalkTo([double]$tx, [double]$tz, [int]$sec, [double]$near) {
    $sin = 0.0; $cos = 1.0; $have = $false
    $a = Pos; $null = VP stick L 0 1 500; Start-Sleep -Milliseconds 200; $b = Pos
    if ($a -and $b) {
        $pdx = $b[0] - $a[0]; $pdz = $b[2] - $a[2]; $blen = [math]::Sqrt($pdx * $pdx + $pdz * $pdz)
        if ($blen -gt 40) { $sin = $pdx / $blen; $cos = $pdz / $blen; $have = $true }
    }
    $t2 = Get-Date; $n = 0; $stuck = 0
    while (((Get-Date) - $t2).TotalSeconds -lt $sec) {
        $p0 = Pos; if (-not $p0) { Start-Sleep 1; continue }
        $wx = $tx - $p0[0]; $wz = $tz - $p0[2]; $dist = [math]::Sqrt($wx * $wx + $wz * $wz)
        if ($dist -lt $near) { $null = T ("at dot: {0:N0} cm, pos {1:N0},{2:N0},{3:N0}" -f $dist, $p0[0], $p0[1], $p0[2]); return $true }
        if (-not $have) { $sx = 0.0; $sy = 1.0 }
        else {
            $ux = $wx / $dist; $uz = $wz / $dist
            $sx = -$ux * $cos + $uz * $sin; $sy = $ux * $sin + $uz * $cos
            $sl = [math]::Sqrt($sx * $sx + $sy * $sy); if ($sl -gt 0.01) { $sx /= $sl; $sy /= $sl } else { $sx = 0.0; $sy = 1.0 }
        }
        if ($stuck -gt 0) {  # blocked: alternate +-34 degree kicks around the obstacle
            $kick = 0.6 * $(if ($stuck % 2) { 1 } else { -1 })
            $ks = $sx * [math]::Cos($kick) - $sy * [math]::Sin($kick); $sy = $sx * [math]::Sin($kick) + $sy * [math]::Cos($kick); $sx = $ks
        }
        $sx = [math]::Round($sx, 2); $sy = [math]::Round($sy, 2)
        $null = VP stick L $sx $sy $(if ($dist -lt 500) { 200 } else { 450 }); Start-Sleep -Milliseconds 120
        $p1 = Pos
        if ($p1) {
            $dx = $p1[0] - $p0[0]; $dz = $p1[2] - $p0[2]; $mv = [math]::Sqrt($dx * $dx + $dz * $dz); $den = $sx * $sx + $sy * $sy
            if ($mv -gt 40 -and $den -gt 0.05) {
                $sraw = ($dx * $sy + $dz * $sx) / $den; $craw = ($dz * $sy - $dx * $sx) / $den; $blen = [math]::Sqrt($sraw * $sraw + $craw * $craw)
                if ($blen -gt 1) { $sin = $sraw / $blen; $cos = $craw / $blen; $have = $true; $stuck = 0 }
            } elseif ($n -gt 1) { $stuck++ }
            if ($n % 5 -eq 0) { $null = T ("walk {0}: stick {1},{2} pos {3:N0},{4:N0} dist {5:N0} moved {6:N0}" -f $n, $sx, $sy, $p1[0], $p1[2], $dist, $mv) }
        }
        $n++
    }
    return $false
}
$log = Join-Path $Game "Nioh3BossRematch.log"
$py = Join-Path $tm "white_dot.py"
$args2 = @("-I", $py, $log); if ($Row) { $args2 += $Row }
$t = & py @args2
if ($LASTEXITCODE -ne 0 -or -not $t) { T "no white dot line for row '$Row' in $log - nothing to walk to"; "reached=nodot"; exit 1 }
$x, $y, $z = ($t -split ' ') | ForEach-Object { [double]$_ }
T ("white dot {0} {1} {2} (row {3})" -f $x, $y, $z, $(if ($Row) { $Row } else { "any" })) | Out-Null
$null = VP press R3 150; Start-Sleep 1  # drop lock-on so the stick walks instead of strafing
$how = "walk"
$at = WalkTo $x $z $Sec $Near
if (-not $at -and -not $NoWarp) {
    [IO.File]::WriteAllText((Join-Path $Game "Nioh3Training.cmd"), ("warp {0} {1} {2} 0`n" -f $x, ($y + 2), $z))
    T "walk stalled; Training test warp to the logged dot" | Out-Null; Start-Sleep 4; $how = "warp"
    $p = Pos; if ($p) { $at = [math]::Sqrt(($x - $p[0]) * ($x - $p[0]) + ($z - $p[2]) * ($z - $p[2])) -lt $Near }
}
if ($at -and -not $NoProceed) { for ($k = 0; $k -lt 3; $k++) { $null = VP press RB 2500; Start-Sleep 2 } }
"reached=$at via=$how"
