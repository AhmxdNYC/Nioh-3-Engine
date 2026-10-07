# shinfo.ps1 <file.file> <index> [more indices...]
# Extracts DXBC #index from a shader_dx12 group file, disassembles it with the signed dxc,
# and prints the signature / buffer definitions / resource bindings header.
param([string]$File, [string]$Index)
$Idx = @($Index -split '[, ]+' | Where-Object { $_ -ne '' })
$root = "C:\Users\Ahmad Hamza\nioh3-engine"
$sd = "C:\Program Files (x86)\Steam\steamapps\common\Nioh3\shader_dx12"
if (-not (Test-Path $File)) { $File = Join-Path $sd $File }
$out = "$root\re\scripts_gpu\out"
$dxc = "$root\tools\rdts\RadeonDeveloperToolSuite-2026-05-28-1806\utils\dx12\dxc\dxc.exe"
$r = py -I "$root\re\scripts_gpu\extract_dxbc.py" $File $out @Idx
$r
foreach ($line in $r) {
  if ($line -match '^wrote (.+?\.dxbc) ') {
    $dx = $Matches[1]; $ll = [IO.Path]::ChangeExtension($dx, ".ll")
    & $dxc -dumpbin $dx 2>$null | Out-File -Encoding utf8 $ll
    $t = Get-Content $ll
    $end = ($t | Select-String -Pattern "^target datalayout" | Select-Object -First 1).LineNumber
    "===== $ll"
    $t[0..($end - 2)] | Where-Object { $_ -match '^;' -and $_ -notmatch '^; *$' }
    $t | Select-String -Pattern '^!\d+ = !\{i32 \d+, i32 \d+, i32 \d+\}$' | Select-Object -First 3 | ForEach-Object { "numthreads? " + $_.Line }
  }
}
