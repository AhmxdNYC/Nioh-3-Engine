# Average Nioh3 process CPU (in "cores", 1.00 = one logical core fully busy) over N seconds.
param([int]$Seconds = 10)
$p = Get-Process Nioh3
$t0 = $p.TotalProcessorTime; $w0 = Get-Date
Start-Sleep $Seconds
$p.Refresh()
$cores = ($p.TotalProcessorTime - $t0).TotalSeconds / ((Get-Date) - $w0).TotalSeconds
"Nioh3 CPU: {0:N2} cores ({1:N0}% of 12 threads)" -f $cores, ($cores / 12 * 100)
