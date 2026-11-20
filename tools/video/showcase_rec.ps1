# showcase_rec.ps1 - record showcase footage on this PC (human #121603): public build, mod world-shading OFF vs ON with the
# mod's fps counter, plus a Boss Rematch "fight now" clip. Reuses the fightnow harness pieces (guard, VirtualPad, lock, save
# backup + byte-exact restore). Game-folder files touched are put back exactly at the end.
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\video\showcase_rec.ps1 -Rel D:\nioh3-re\showcase-121603\rel\full
#        -Out D:\nioh3-re\showcase-121603\clips [-Arms off,on] [-RematchRow R7B35] [-Sec 20]
param([string]$Rel, [string]$Out, [string[]]$Arms = @("off", "on"), [string]$RematchRow = "", [string]$TrainChr = "", [int]$Sec = 20, [int]$OwnerPid = 0)
$ErrorActionPreference = "Continue"
$repo = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path))
$tm = Join-Path $repo "tools\testmatrix"
$GameDir = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$ff = "C:\Users\Ahmad Hamza\AppData\Local\Microsoft\WinGet\Packages\Gyan.FFmpeg_Microsoft.Winget.Source_8wekyb3d8bbwe\ffmpeg-9.0.2-full_build\bin\ffmpeg.exe"
New-Item -ItemType Directory -Force "$Out\orig" | Out-Null
$stamp = Get-Date -f yyyyMMdd-HHmmss
$saveBak = "$Out\savebak-$stamp"
$trace = "$Out\trace.txt"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText($trace, $l + "`r`n"); Write-Host $l }
if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { T "Nioh3 already running - nothing touched"; exit 2 }

# ---- human activity gate: cursor still 8 s and every XInput pad idle (main PC: GetLastInputInfo is always 0) ----
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class HA {
  [StructLayout(LayoutKind.Sequential)] public struct P { public int x, y; }
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out P p);
  [StructLayout(LayoutKind.Sequential)] public struct XS { public uint pkt; public ushort b; public byte lt, rt; public short lx, ly, rx, ry; }
  [DllImport("xinput1_4.dll")] public static extern uint XInputGetState(uint i, out XS s);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c);
  [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
}
"@
function Human-Active {
    $p0 = New-Object HA+P; [void][HA]::GetCursorPos([ref]$p0)
    # a ViGEm pad on this PC bumps its packet number ~1000/s with all-zero values, so judge the VALUES, not the packet number
    for ($k = 0; $k -lt 32; $k++) {
        for ($i = 0; $i -lt 4; $i++) { $s = New-Object HA+XS
            if ([HA]::XInputGetState($i, [ref]$s) -eq 0 -and ($s.b -ne 0 -or $s.lt -gt 30 -or $s.rt -gt 30 -or [Math]::Abs([int]$s.lx) -gt 8000 -or [Math]::Abs([int]$s.ly) -gt 8000 -or [Math]::Abs([int]$s.rx) -gt 8000 -or [Math]::Abs([int]$s.ry) -gt 8000)) { return "pad $i input b=$($s.b) l=$($s.lx),$($s.ly) r=$($s.rx),$($s.ry)" } }
        Start-Sleep -Milliseconds 250
    }
    $p1 = New-Object HA+P; [void][HA]::GetCursorPos([ref]$p1)
    if ($p0.x -ne $p1.x -or $p0.y -ne $p1.y) { return "mouse moved" }
    return ""
}
$ha = Human-Active; if ($ha) { T "human active ($ha) - nothing touched"; exit 3 }
T "human idle check OK"

. "$tm\harness_guard.ps1"
. "$tm\vpad.ps1"
$uo = "C:\Users\Ahmad Hamza\universal-optimizer\agent-bridge"
$env:UO_BRIDGE_TOKEN_FILE = "$env:APPDATA\uo-bridge\token"; $env:UO_AGENT_ID = "pc"
function Lock([string]$a) { Push-Location $uo; try { $o = (py -m uo_bridge.gamelock $a nioh3 --ttl 3600 2>&1 | Out-String).Trim(); @{ code = $LASTEXITCODE; out = $o } } finally { Pop-Location } }
function Set-IniKey([string]$t, [string]$sec, [string]$key, [string]$val) {
    $nl = "`r`n"
    $m = [regex]::Match($t, "(?ms)^\[$([regex]::Escape($sec))\][^\r\n]*\r?\n(.*?)(?=^\[|\z)")
    if (-not $m.Success) { return $t.TrimEnd() + "$nl$nl[$sec]$nl$key = $val$nl" }
    $body = $m.Groups[1].Value
    $re = "(?m)^$([regex]::Escape($key))[ \t]*=.*?(\r?)$"
    if ([regex]::IsMatch($body, $re)) { $nb = [regex]::Replace($body, $re, "$key = $val`$1", 1) } else { $nb = "$key = $val$nl" + $body }
    return $t.Substring(0, $m.Groups[1].Index) + $nb + $t.Substring($m.Groups[1].Index + $m.Groups[1].Length)
}
function RMP([string]$cmd, [int]$timeoutMs = 5000) {
    $p = New-Object IO.Pipes.NamedPipeClientStream(".", "nioh3bossrematch", [IO.Pipes.PipeDirection]::InOut)
    try { $p.Connect($timeoutMs); $w = New-Object IO.StreamWriter($p); $w.AutoFlush = $true; $rd = New-Object IO.StreamReader($p); $w.WriteLine($cmd)
          $t = $rd.ReadLineAsync(); if (-not $t.Wait($timeoutMs)) { return "error timeout" }; return $t.Result } catch { return "error $($_.Exception.Message)" } finally { $p.Dispose() }
}
function F([string]$reply, [string]$key) { if ($reply -match "(?:^| )$key=(\S+)") { return $Matches[1] }; return "" }
function Wait-RM([scriptblock]$cond, [int]$sec, [int]$poll = 1) { $t0 = Get-Date; while (((Get-Date) - $t0).TotalSeconds -lt $sec) { $r = RMP status; if (& $cond $r) { return $r }; Start-Sleep $poll }; return $null }
# AppActivate alone leaves Steam in front: a bare Alt tap unlocks SetForegroundWindow for this process
function Force-Front {
    $p = Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue; if (-not $p) { return $false }
    $h = $p.MainWindowHandle; [HA]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [HA]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
    [void][HA]::ShowWindow($h, 9); [void][HA]::SetForegroundWindow($h); Start-Sleep -Milliseconds 700
    return ([HA]::GetForegroundWindow() -eq $h)
}
function VPS([string]$k) { $r = VP-State; if ($r -match "(?:^| )$k=(\S+)") { return $Matches[1] }; return "" }
# video (gdigrab, AMD encoder) + game sound (WASAPI loopback, tools\video\loopback_rec.py) started together, muxed into <name>.mp4
$script:APy = "D:\nioh3-re\showcase-121603\venv\Scripts\python.exe"; $script:ARec = Join-Path $repo "tools\video\loopback_rec.py"
function Start-Clip([string]$name, [int]$sec) {
    $v = Start-Process $ff -ArgumentList "-y -loglevel error -f gdigrab -framerate 60 -draw_mouse 0 -i desktop -t $sec -c:v h264_amf -b:v 40M `"$Out\${name}_v.mp4`"" -WindowStyle Hidden -PassThru
    $a = if (Test-Path $script:APy) { Start-Process $script:APy -ArgumentList "-I `"$script:ARec`" `"$Out\${name}.wav`" $sec" -WindowStyle Hidden -PassThru } else { $null }
    @{ name = $name; v = $v; a = $a; sec = $sec }
}
function End-Clip($c) {
    $c.v.WaitForExit(($c.sec + 40) * 1000) | Out-Null; if ($c.a) { $c.a.WaitForExit(($c.sec + 40) * 1000) | Out-Null }
    $n = $c.name
    if ($c.a -and (Test-Path "$Out\$n.wav")) { & $ff -y -loglevel error -i "$Out\${n}_v.mp4" -i "$Out\$n.wav" -c:v copy -c:a aac -b:a 192k -shortest "$Out\$n.mp4"; Remove-Item "$Out\${n}_v.mp4" -Force }
    else { Move-Item "$Out\${n}_v.mp4" "$Out\$n.mp4" -Force }
    T "  clip $n.mp4 (audio: $([bool]$c.a))"
}
function Rec([string]$name, [int]$sec) {
    $null = Force-Front
    End-Clip (Start-Clip $name $sec)
}
$files = @("Nioh3EngineFix.asi", "Nioh3EngineFix.ini", "Nioh3Training.asi", "Nioh3BossRematch.asi", "Nioh3BossRematch.ini", "Nioh3BossRematch_bosses.tsv", "Nioh3CamKiProbe.asi",
           "Nioh3Training_actionsets.tsv", "Nioh3Training_action_records.tsv", "Nioh3Training_arenas.tsv", "Nioh3Training_bgm.tsv")
$script:had = @{}; $script:installed = $false; $script:saved = $false; $script:locked = $false; $script:gamePid = 0
function Close-Game {
    $p = if ($script:gamePid) { Get-Process -Id $script:gamePid -ErrorAction SilentlyContinue } else { $null }
    if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { T "!! killing pid $($p.Id)"; Stop-Process -Id $p.Id -Force }; Start-Sleep 8 }
    $script:gamePid = 0
}
function Restore {
    if ($script:installed) {
        foreach ($n in $files) { $d = Join-Path $GameDir $n; if ($script:had[$n]) { Copy-Item "$Out\orig\$n" $d -Force } else { Remove-Item $d -Force -ErrorAction SilentlyContinue } }
        $script:installed = $false
        T "game folder restored: $(($files | Where-Object { $_ -like '*.asi' } | ForEach-Object { "$_ " + (Get-FileHash (Join-Path $GameDir $_)).Hash.Substring(0,8).ToLower() }) -join ', ')"
    }
    if ($script:saved -and -not (Get-Process Nioh3 -ErrorAction SilentlyContinue)) {
        $null = robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH
        $a = Get-ChildItem $saveBak -Recurse -File | ForEach-Object { "$($_.FullName.Substring($saveBak.Length))|$((Get-FileHash $_.FullName -Algorithm SHA1).Hash)" } | Sort-Object
        $b = Get-ChildItem $saveDir -Recurse -File | ForEach-Object { "$($_.FullName.Substring($saveDir.Length))|$((Get-FileHash $_.FullName -Algorithm SHA1).Hash)" } | Sort-Object
        T "save restored, byte-exact: $(@(Compare-Object $a $b).Count -eq 0) ($($a.Count) files)"; $script:saved = $false
    }
    if ($script:locked) { $r = Lock "release"; $script:locked = $false; T "lock released: $($r.out)" }
}
function Launch {
    Assert-Launch
    $before = VP-ForegroundInfo
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"C:\Users\Ahmad Hamza\universal-optimizer\tools\steam\steam_autoplay.ps1`" -Minutes 5" -WindowStyle Hidden
    Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
    while (((Get-Date) - $t0).TotalSeconds -lt 180) {
        $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1; if ($p) { break }
        $null = & powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $tm 'steam_cloud_click.ps1') 2>$null
        Start-Sleep 3
    }
    if (-not $p) { throw "no game window after 180 s" }
    $script:gamePid = $p.Id; Register-Game; T "game pid $($p.Id)"
    Start-Sleep 5; T "  front: $(Force-Front)"
    if (-not (VP-WaitPipe 180)) { throw "no VirtualPad pipe" }
    $ok = VP-TitleContinue 300 "$Out\title_$stamp.txt"; T "title -> Continue -> field: $ok"
    # vpad's helper stops pressing when it thinks frames stalled; the load-tip page still needs A: press until field=1
    $t1 = Get-Date
    while ((VPS 'field') -ne '1' -and ((Get-Date) - $t1).TotalSeconds -lt 120) { $null = VP press A 120; Start-Sleep 3 }   # the load-tip page reports loading=1 too
    T "  field=$(VPS 'field') player=$(VPS 'player')"
    Start-Sleep 4
    # the fps counter is drawn into the game's picture: bring the game to the front for the capture
    T "  front: $(Force-Front)"; Start-Sleep 2
}

Init-Guard -Name "showcase_rec" -OwnerPid $OwnerPid -MaxMinutes 25 -Cleanup { Close-Game; Restore }
try {
    $r = Lock "acquire"; if ($r.code -ne 0) { throw "lock: $($r.out)" }; $script:locked = $true; T "lock: $($r.out)"
    $null = robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; $script:saved = $true; T "saves backed up -> $saveBak"
    foreach ($n in $files) { $s = Join-Path $GameDir $n; $script:had[$n] = Test-Path $s; if ($script:had[$n]) { Copy-Item $s "$Out\orig\$n" -Force } }
    foreach ($n in $files) { if ($n -ne "Nioh3EngineFix.ini") { Copy-Item (Join-Path $Rel $n) (Join-Path $GameDir $n) -Force } }
    $bri = "$GameDir\Nioh3BossRematch.ini"; if (Test-Path $bri) { [IO.File]::WriteAllText($bri, (Set-IniKey (Get-Content $bri -Raw) "Remote" "Pipe" "1")) }
    $script:installed = $true
    T "installed release: $(($files | Where-Object { $_ -like '*.asi' } | ForEach-Object { "$_ " + (Get-FileHash (Join-Path $GameDir $_)).Hash.Substring(0,8).ToLower() }) -join ', ')"
    $base = Get-Content (Join-Path $Rel "Nioh3EngineFix.ini") -Raw
    $base = Set-IniKey $base "Debug" "VirtualPad" "1"; $base = Set-IniKey $base "General" "StartupPopup" "0"; $base = Set-IniKey $base "Overlay" "FpsCounter" "1"
    foreach ($arm in $Arms) {
        $ini = $base
        if ($arm -eq "on") { $ini = Set-IniKey $ini "Perf" "VrsCeiling" "1"; $ini = Set-IniKey $ini "Perf" "VrsWideExclude" "6" }
        else { $ini = Set-IniKey $ini "Perf" "VrsCeiling" "0"; $ini = Set-IniKey $ini "Perf" "VrsWideExclude" "0" }
        [IO.File]::WriteAllText("$GameDir\Nioh3EngineFix.ini", $ini); T "arm ${arm}: ini written"
        Launch
        Rec "field_${arm}_stand" $Sec
        $null = Force-Front
        $job = Start-Clip "field_${arm}_move" $Sec
        Start-Sleep 1; $null = VP stick L 0 1 ([int](($Sec - 2) * 1000)); End-Clip $job
        if ($TrainChr -and $arm -eq $Arms[-1]) {
            # Training in the field via the add-on's command file (as Temp/live119487/toad.ps1): summon, no-death, repeat last move
            $tc = "$GameDir\Nioh3Training.cmd"
            function TCmd([string]$c) { $w = 0; while ((Test-Path $tc) -and $w -lt 50) { Start-Sleep -Milliseconds 200; $w++ }; [IO.File]::WriteAllText($tc, "$c`r`n"); T "  train cmd: $c" }
            $null = Force-Front
            $job = Start-Clip "training_$TrainChr" 55
            TCmd "nodeath 1"; Start-Sleep 2; TCmd "summon $TrainChr"; Start-Sleep 15; TCmd "replast $TrainChr 1"
            End-Clip $job
            TCmd "replast $TrainChr 0"; Start-Sleep 2; TCmd "despawn all"; Start-Sleep 5
            Remove-Item $tc -Force -ErrorAction SilentlyContinue
        }
        if ($RematchRow -and $arm -eq $Arms[-1]) {
            $job = Start-Clip "rematch_$RematchRow" 75
            T "  rematch fight $RematchRow -> $(RMP "fight $RematchRow")"
            $e = Wait-RM { param($x) (F $x 'fnow') -in @('pass', 'fail') } 70 1; T "  rematch status: $e"
            if ($e) { $null = VP stick L 0 1 4000 }
            End-Clip $job
        }
        Close-Game
    }
} catch { T "ERROR: $($_.Exception.Message)" } finally { Close-Game; Restore; Done-Guard; T "done" }
