# Apply patches one at a time, waiting $Wait seconds after each; stop at the first one the game doesn't survive.
param([int]$Wait = 60)
Set-Location $PSScriptRoot
$env:FT_ALLOW_WRITE = "1"
$gamePid = (Get-Process Nioh3 -ErrorAction Stop).Id
Write-Host "tracking Nioh3 pid $gamePid"
function Survives([string]$label) {
    for ($i = 0; $i -lt $Wait; $i += 5) {
        Start-Sleep 5
        if (-not (Get-Process -Id $gamePid -ErrorAction SilentlyContinue)) { Write-Host "$label -> GAME DIED after ~$($i + 5)s"; return $false }
    }
    Write-Host "$label -> alive after ${Wait}s"; return $true
}
$steps = @(
    @{ n = "W1/W2";   s = "cpu_patches.fsx";  m = "apply"; a = "" },
    @{ n = "VRR v2";  s = "ft_vrr_patch.fsx"; m = "apply"; a = "" },
    @{ n = "cave W3"; s = "cpu_caves.fsx";    m = "apply"; a = "W3" },
    @{ n = "cave R1"; s = "cpu_caves.fsx";    m = "apply"; a = "R1" },
    @{ n = "cave L1"; s = "cpu_caves.fsx";    m = "apply"; a = "L1" }
)
foreach ($st in $steps) {
    $out = dotnet fsi $st.s $st.m $st.a 2>&1 | ForEach-Object { "$_" } | Where-Object { $_ -match 'applied|->|refus|error|fail' -and $_ -notmatch 'FS3886' } | Select-Object -First 2
    Write-Host "[$($st.n)] $($out -join ' | ')"
    if (-not (Survives $st.n)) { break }
}
