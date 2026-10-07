# Watch the game for $Seconds; report a crash if the process exits OR shows its "Fatal error" dialog
# (the game process stays alive while that dialog is up, so a plain process check isn't enough).
param([int]$Seconds = 300, [string]$Label = "soak")
$ui = "C:\Users\Ahmad Hamza\nioh3-engine\tools\ui.fsx"
$gamePid = (Get-Process Nioh3 -ErrorAction Stop).Id
for ($t = 0; $t -lt $Seconds; $t += 15) {
    Start-Sleep 15
    if (-not (Get-Process -Id $gamePid -ErrorAction SilentlyContinue)) { "$Label -> GAME EXITED after ~$($t + 15)s"; exit 1 }
    $d = dotnet fsi $ui dialog Nioh3 2>&1 | Select-String 'dialogs:'
    if ($d -and $d.Line -notmatch 'dialogs: 0') { "$Label -> CRASH DIALOG after ~$($t + 15)s: $($d.Line)"; exit 1 }
}
"$Label -> OK for ${Seconds}s"
