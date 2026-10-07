# Apply (default) or revert every engine patch on the running Nioh3, in a safe order.
# usage: powershell -File patch_all.ps1 [-Revert]
param([switch]$Revert)
Set-Location $PSScriptRoot
$env:FT_ALLOW_WRITE = "1"
function Run($script, $mode, $arg) {
    $out = dotnet fsi $script $mode $arg 2>&1 | ForEach-Object { "$_" } |
        Where-Object { $_ -match 'DYNAMIC_FPS|applied|revert|patched|->|MISMATCH|UNKNOWN|refus|fail|error' -and $_ -notmatch 'FS3886|RemoteException' }
    "[$script $mode] " + ($out -join ' | ')
}
if ($Revert) {
    Run cpu_caves.fsx revert "W3,R1,L1"
    Run ft_vrr_patch.fsx revert ""
    Run cpu_patches.fsx revert ""
} else {
    Run cpu_patches.fsx apply ""
    Run ft_vrr_patch.fsx apply ""
    Run cpu_caves.fsx apply "W3,R1,L1"
}
"game responding: $((Get-Process Nioh3).Responding)"
