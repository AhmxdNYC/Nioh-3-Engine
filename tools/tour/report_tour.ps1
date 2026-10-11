# Run the offline reporter without treating its diagnostic stderr as a PowerShell
# terminating error. Its exit code, rather than the output stream, decides success.
param([Parameter(Mandatory = $true)][string]$Capture)

$pythonExe = (Get-Command py -CommandType Application -ErrorAction Stop).Source
$previousPreference = $ErrorActionPreference
try {
    $ErrorActionPreference = 'Continue'
    $report = & $pythonExe -I (Join-Path $PSScriptRoot 'summarize_tour.py') $Capture 2>&1 |
        ForEach-Object { "$_" } | Out-String
    $reportExitCode = $LASTEXITCODE
} finally {
    $ErrorActionPreference = $previousPreference
}
if ($null -eq $reportExitCode -or $reportExitCode -ne 0) {
    throw "summarize_tour.py exit ${reportExitCode}: $($report.Trim())"
}
$report.Trim()
