param([int]$OwnerPid = 0, [string]$Dll = "D:/nioh3-re/live123670/Nioh3EngineFix-resinfo-145.asi", [string]$Want = "4861177B")
# pc_resinfo_145.ps1 (TASK #123670, GH #145): ONE launch with the ResInfoGuard EngineFix only.
# Lock is held by the caller (pc). Backs up saves + EF asi/ini, installs the test EF, title->Continue (a level load),
# then 75 s in the field. Pass = log 'RESINFOGUARD: installed', no crash file, game alive after the load. Restores everything.
$ErrorActionPreference = "Continue"
$repo = "C:\Users\Ahmad Hamza\nioh3-engine"; $tm = "$repo\tools\testmatrix"
$GD = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3"; $ini = "$GD\Nioh3EngineFix.ini"; $asi = "$GD\Nioh3EngineFix.asi"
$saveDir = "$env:LOCALAPPDATA\KoeiTecmo\NIOH3"
$stamp = Get-Date -f yyyyMMdd-HHmmss
$out = "$repo\captures\pc-resinfo-123670\$stamp"; New-Item -ItemType Directory -Force "$out\orig" | Out-Null
$saveBak = "$repo\captures\save-backups\$stamp-resinfo145"
function T([string]$s) { $l = "[{0}] {1}" -f (Get-Date -f HH:mm:ss), $s; [IO.File]::AppendAllText("$out\trace.txt", $l + "`r`n"); Write-Host $l }
. "$tm\harness_guard.ps1"; . "$tm\vpad.ps1"
$script:saved=$false; $script:inst=$false; $script:gp=0
function Close-Game { $p = if ($script:gp) { Get-Process -Id $script:gp -EA SilentlyContinue } else { $null }
  if ($p) { Note-Closed; $null = $p.CloseMainWindow(); $p.WaitForExit(45000) | Out-Null; if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }; Start-Sleep 8 }; $script:gp = 0 }
function Restore {
  if (Test-Path "$GD\Nioh3EngineFix.log") { Copy-Item "$GD\Nioh3EngineFix.log" "$out\" -Force }
  if (Get-Process Nioh3 -EA SilentlyContinue) { T "!! Nioh3 still running, restore skipped"; return }
  if ($script:inst) { Copy-Item "$out\orig\Nioh3EngineFix.ini" $ini -Force; Copy-Item "$out\orig\Nioh3EngineFix.asi" $asi -Force; $script:inst=$false; T "EF restored $((Get-FileHash $asi).Hash.Substring(0,8)) + ini" }
  if ($script:saved) { robocopy $saveBak $saveDir /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; T "save restored robocopy $LASTEXITCODE"; $script:saved=$false }
}
Init-Guard -Name "resinfo123670" -OwnerPid $OwnerPid -MaxMinutes 10 -Cleanup { Close-Game; Restore }
try {
  if (Get-Process Nioh3 -EA SilentlyContinue) { throw "Nioh3 running" }
  $h = (Get-FileHash $Dll).Hash; if (-not $h.StartsWith($Want)) { throw "test dll sha $h != $Want" }
  New-Item -ItemType Directory -Force $saveBak | Out-Null; robocopy $saveDir $saveBak /MIR /R:2 /W:2 /NFL /NDL /NP /NJH | Out-Null; $script:saved=$true; T "saves backed up to $saveBak"
  Copy-Item $ini "$out\orig\" -Force; Copy-Item $asi "$out\orig\" -Force; $script:inst=$true
  $crashBefore = @(Get-ChildItem "$GD\Nioh3EngineFix-crash-2*.txt" -EA SilentlyContinue | % Name)
  Copy-Item $Dll $asi -Force; T "test EF $((Get-FileHash $asi).Hash.Substring(0,8)) installed (orig $((Get-FileHash "$out\orig\Nioh3EngineFix.asi").Hash.Substring(0,8)) saved)"
  $t = [IO.File]::ReadAllText($ini); $t = $t -replace "(?m)^AutoContinueLoad *=.*", "AutoContinueLoad = 1"; $t = Force-Vpad $t
  [IO.File]::WriteAllText($ini, $t, (New-Object Text.UTF8Encoding($false))); Copy-Item $ini "$out\test.ini"
  Assert-Launch
  Start-Process powershell -WindowStyle Hidden -ArgumentList "-NoProfile","-ExecutionPolicy","Bypass","-File","`"C:/Users/Ahmad Hamza/universal-optimizer/tools/steam/steam_autoplay.ps1`"","-Minutes","15"
  Start-Process "steam://rungameid/3681010"; $t0 = Get-Date; $p = $null
  while (((Get-Date) - $t0).TotalSeconds -lt 180) { $p = Get-Process Nioh3 -EA SilentlyContinue | ? MainWindowHandle -ne 0 | Select -First 1; if ($p) { break }; Start-Sleep 3 }
  if (-not $p) { throw "no window" }
  $script:gp = $p.Id; Register-Game; T "game pid $($p.Id)"
  if (-not (VP-WaitPipe 180)) { throw "no vpad pipe" }
  $ok = VP-TitleContinue 300 "$out\title.txt"; T "title->field $ok"
  if (-not $ok) { py -I -c "from PIL import ImageGrab; ImageGrab.grab().save(r'$out\stuck.png')" 2>&1 | Out-Null; throw "title-continue failed" }
  Start-Sleep 75
  $alive = [bool](Get-Process -Id $script:gp -EA SilentlyContinue); T "alive after load+75s: $alive state: $(VP-State)"
  py -I -c "from PIL import ImageGrab; ImageGrab.grab().save(r'$out\field.png')" 2>&1 | Out-Null
  $g = Select-String -Path "$GD\Nioh3EngineFix.log" -Pattern 'RESINFOGUARD' | Select -First 10 | % { $_.Line.Trim() }
  T "log RESINFOGUARD: $($g -join ' || ')"
  $bp = Select-String -Path "$GD\Nioh3EngineFix.log" -Pattern '18cbdf7|18CBDF7' | Select -First 5 | % { $_.Line.Trim() }
  T "log 0x18cbdf7 hits: $($bp.Count) $($bp -join ' || ')"
  $crashNew = @(Get-ChildItem "$GD\Nioh3EngineFix-crash-2*.txt" -EA SilentlyContinue | % Name | ? { $crashBefore -notcontains $_ })
  T "new crash files: $($crashNew.Count) $($crashNew -join ',')"
} catch { T "!! $($_.Exception.Message)" }
finally { Close-Game; Restore; Done-Guard; T "done" }
