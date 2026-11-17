# pc2/1 #112985 (#98 v2 no-harm): HoldWindowFix 0/1 x uncapped(MaxFps60)/45 cap, 40 x 'press Y 1000', EF #8260 21df02a2, restart per arm, one lock across arms.
param([string]$Arms = "A60,H60,A45,H45", [int]$N = 40)
$ErrorActionPreference = 'Continue'
$G = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $W = "C:\nioh3-re\t98v2"; $repo = "C:\Users\User\projects\nioh3-engine"
$uo = "C:\Users\User\projects\universal-optimizer\agent-bridge"; $vp = "$repo\tools\testmatrix\vpad.py"
$dd = "C:\Users\User\AppData\Local\uo-agentd\pc2"; $saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"; $bak = "$W\bak"
$inis = @("Nioh3EngineFix.ini","Nioh3CamKiProbe.ini","Nioh3Training.ini","Nioh3BossRematch.ini","Nioh3Training_learned.ini")
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; try { Add-Content "$W\trace.txt" $l -ErrorAction Stop } catch {}; Write-Host $l }
function Lock([string]$a) { Push-Location $uo; try { $env:UO_AGENT_ID='pc2'; (py -m uo_bridge.gamelock $a nioh3-pc2 --ttl 7200 2>&1 | Out-String).Trim() } finally { Pop-Location } }
function SetKey([string]$t, [string]$sec, [string]$k, [string]$v) {
    if ($t -match "(?m)^$k *=") { return ($t -replace "(?m)^$k *=.*", "$k = $v") }
    return ($t -replace "(?m)^\[$sec\][ \t]*(\r?)$", "[$sec]`$1`n$k = $v`$1")
}
function St() { return (py -I $vp state 2>&1 | Out-String).Trim() }
function KillGame() { $q = Get-Process Nioh3 -ErrorAction SilentlyContinue; if ($q) { Stop-Process -Id $q.Id -Force; Start-Sleep 6 } }
function H8([string]$f) { (Get-FileHash $f).Hash.Substring(0,8).ToLower() }
$hw = @{ A60 = "0"; H60 = "1"; A45 = "0"; H45 = "1" }; $mx = @{ A60 = "120"; H60 = "120"; A45 = "45"; H45 = "45" }; $mn = @{ A60 = "48"; H60 = "48"; A45 = "30"; H45 = "30" }
$locked = $false; $saved = $false; $inst = $false
try {
    if (Get-Process Nioh3 -ErrorAction SilentlyContinue) { throw "Nioh3 already running" }
    $o = Lock "acquire"; T "lock: $o"; if ($o -notmatch "acquired|already") { throw "lock not acquired: $o" }; $locked = $true
    Set-Content "$dd\test-active" "pc2/1 #112985 #98 v2 no-harm"
    New-Item -ItemType Directory -Force "$bak\saves" | Out-Null
    Copy-Item "$G\Nioh3EngineFix.asi" "$bak\" -Force; foreach ($f in $inis) { if (Test-Path "$G\$f") { Copy-Item "$G\$f" "$bak\" -Force } }
    $null = robocopy $saveDir "$bak\saves" /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "saves backed up exit $LASTEXITCODE; EF $(H8 "$bak\Nioh3EngineFix.asi") ini $(H8 "$bak\Nioh3EngineFix.ini")"
    if ($LASTEXITCODE -ge 8) { throw "save backup failed" }; $saved = $true; $inst = $true
    foreach ($arm in $Arms.Split(',')) {
        if (Test-Path "$W\STOP") { throw "STOP" }
        for ($try = 1; $try -le 2; $try++) {
            $out = "$W\arm$arm"; New-Item -ItemType Directory -Force $out | Out-Null
            $null = robocopy "$bak\saves" $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "===== arm $arm try $try HoldWindowFix=$($hw[$arm]) Min120=$($mn[$arm]) Max120=$($mx[$arm]) (saves reset exit $LASTEXITCODE)"
            Copy-Item "$W\dl\Nioh3EngineFix.asi" "$G\Nioh3EngineFix.asi" -Force
            $t = [IO.File]::ReadAllText("$bak\Nioh3EngineFix.ini")
            foreach ($kv in @(@("Debug","VirtualPad","1"),@("Debug","KeepRunningUnfocused","1"),@("General","StartupPopup","0"),@("General","AutoContinueLoad","1"),
                              @("VRR","Tier120","1"),@("VRR","MaxFps","60"),@("VRR","GovTrace","1"),@("VRR","Tier120Floor","0"),@("VRR","GovFpsFix","0"),@("VRR","HoldWindowFix",$hw[$arm]),@("VRR","Min120",$mn[$arm]),@("VRR","Max120",$mx[$arm]))) { $t = SetKey $t $kv[0] $kv[1] $kv[2] }
            [IO.File]::WriteAllText("$G\Nioh3EngineFix.ini", $t, (New-Object Text.UTF8Encoding($false)))
            $k = ([regex]::Matches($t, "(?m)^(VirtualPad|AutoContinueLoad|Tier120|Tier120Floor|GovFpsFix|GovTrace|MaxFps|HoldWindowFix|Min120|Max120) *=.*$") | % { $_.Value.Trim() }) -join "; "
            T "EF $(H8 "$G\Nioh3EngineFix.asi") ini: $k"
            Remove-Item "$G\Nioh3EngineFix.log" -Force -ErrorAction SilentlyContinue
            Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File','C:/Users/User/projects/universal-optimizer/tools/steam/steam_autoplay.ps1','-Minutes','15'
            Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
            while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -ErrorAction SilentlyContinue | Select-Object -First 1; if ($p) { break }; Start-Sleep 1 }
            if (-not $p) { throw "no launch" }
            Set-Content "$W\game.pid" $p.Id; T "launched pid $($p.Id)"
            $null = py -I $vp wait-pipe 180; T "pipe exit $LASTEXITCODE"
            Start-Sleep 40; T "press A -> $((py -I $vp press A 150 2>&1 | Out-String).Trim()) | $(St)"; Start-Sleep 15
            $null = py -I $vp title-continue 300; $tc = $LASTEXITCODE; T "title-continue exit $tc | $(St)"
            if ($tc -ne 0) { Copy-Item "$G\Nioh3EngineFix.log" "$out\EF_fail$try.log" -Force -ErrorAction SilentlyContinue; KillGame; continue }
            Start-Sleep 10; T "sampler start | $(St)"
            py -I "$W\hold.py" $arm $N $out *>&1 | % { T "  $_" }
            $alive = [bool](Get-Process -Id $p.Id -ErrorAction SilentlyContinue); T "sampler done, game alive $alive"
            KillGame; Copy-Item "$G\Nioh3EngineFix.log" "$out\EF.log" -Force -ErrorAction SilentlyContinue
            T "EF.log: $((Select-String -Path "$out\EF.log" -Pattern 'hold window|HoldWindowFix|tier 120' | Select-Object -First 4 | % { $_.Line.Trim() }) -join ' | ')"
            break
        }
    }
} catch { T "ERROR $_" }
finally {
    KillGame
    if ($inst) { Copy-Item "$bak\Nioh3EngineFix.asi" "$G\" -Force; foreach ($f in $inis) { if (Test-Path "$bak\$f") { Copy-Item "$bak\$f" "$G\" -Force } }
                 T "restored EF $(H8 "$G\Nioh3EngineFix.asi") ini $(H8 "$G\Nioh3EngineFix.ini")" }
    if ($saved) { $null = robocopy "$bak\saves" $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH; T "saves restored exit $LASTEXITCODE" }
    if ($locked) { $o = Lock "release"; T "lock release: $o" }
    Remove-Item "$dd\test-active" -ErrorAction SilentlyContinue
    T "SERIES END"
}
