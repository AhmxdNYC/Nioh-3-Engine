<#
  export_renderer.ps1 - bulk C export of the renderer function set (D:\nioh3-re\decomp\targets.txt, written from
  re\scripts_renderer\partition.tsv) in chunks of -ChunkMin minutes, so the read-only copy's mutex is released between
  chunks and queued decomp_ro.cmd queries of other agents get in. Then a retry pass for timeouts with a long limit, then
  apply_names.py. Log: D:\nioh3-re\decomp\export_renderer.log
#>
param([int]$Threads = 6, [int]$Timeout = 120, [int]$ChunkMin = 12, [int]$RetryTimeout = 600, [int]$MaxChunks = 30)
$log = "D:\nioh3-re\decomp\export_renderer.log"
function L([string]$s) { Add-Content $log ("[{0}] {1}" -f (Get-Date -f "yyyy-MM-dd HH:mm:ss"), $s) }
$t0 = Get-Date
L "start: threads=$Threads timeout=$Timeout chunk=$ChunkMin min"
for ($i = 1; $i -le $MaxChunks; $i++) {
    $before = (Get-Content D:\nioh3-re\decomp\index.tsv | Measure-Object -Line).Lines
    cmd /c "D:\nioh3-re\export_decomp.cmd threads=$Threads timeout=$Timeout minutes=$ChunkMin" 2>&1 | Out-Null
    $after = (Get-Content D:\nioh3-re\decomp\index.tsv | Measure-Object -Line).Lines
    $last = (Select-String -Path D:\nioh3-re\decomp\export_log.txt -Pattern "^=== " | Select-Object -Last 2 | ForEach-Object Line) -join " | "
    L "chunk ${i}: index $before -> $after rows; $last"
    if ($last -match "toExport=0\b" -or $after -eq $before) { break }
    Start-Sleep -Seconds 20   # let queued decomp_ro.cmd queries take the mutex
}
L "main pass done in $([int]((Get-Date) - $t0).TotalMinutes) min; retry pass (timeouts) timeout=$RetryTimeout threads=2"
cmd /c "D:\nioh3-re\export_decomp.cmd retry=1 threads=2 timeout=$RetryTimeout minutes=40" 2>&1 | Out-Null
$last = (Select-String -Path D:\nioh3-re\decomp\export_log.txt -Pattern "^=== " | Select-Object -Last 2 | ForEach-Object Line) -join " | "
L "retry: $last"
py -I D:\nioh3-re\apply_names.py 2>&1 | Select-Object -Last 3 | ForEach-Object { L "apply_names: $_" }
L "ALL DONE in $([int]((Get-Date) - $t0).TotalMinutes) min"
