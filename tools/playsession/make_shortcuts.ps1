# make_shortcuts.ps1 - (re)creates the two desktop shortcuts "Nioh3 Session START" and "Nioh3 Session STOP" (visible PowerShell windows).
$desk = [Environment]::GetFolderPath("Desktop")
$ps = Join-Path $env:WINDIR "System32\WindowsPowerShell\v1.0\powershell.exe"
$sh = New-Object -ComObject WScript.Shell
foreach ($x in @(@("Nioh3 Session START", "start_session.ps1", "Click BEFORE starting Nioh 3: save backup + capture on"),
                 @("Nioh3 Session STOP", "stop_session.ps1", "Click AFTER quitting Nioh 3: capture off, mod files back, report"))) {
    $l = $sh.CreateShortcut((Join-Path $desk ($x[0] + ".lnk")))
    $l.TargetPath = $ps
    $l.Arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$(Join-Path $PSScriptRoot $x[1])`""
    $l.WorkingDirectory = $PSScriptRoot
    $l.Description = $x[2]
    $l.IconLocation = "$ps,0"
    $l.WindowStyle = 1
    $l.Save()
    "created $(Join-Path $desk ($x[0] + '.lnk'))"
}
