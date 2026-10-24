$ErrorActionPreference='Stop'
. "$PSScriptRoot/audit_capture_helpers.ps1"
function Check($b,$m){if(-not $b){throw $m}}
function MustThrow([scriptblock]$b){$threw=$false;try{& $b}catch{$threw=$true};Check $threw 'expected failure'}
$dir=Join-Path $env:TEMP ('audit-tests-'+[guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory $dir
try{
 $log=Join-Path $dir 'training.log'
 [IO.File]::WriteAllText($log,'old HB +250 ms boss HP 99/99')
 $offset=Get-AuditLogOffset $log
 Check (-not(Test-AuditBossHeartbeat (Read-AuditLogDelta $log ([ref]$offset)))) 'old heartbeat reused'
 [IO.File]::AppendAllText($log,"`nnew HB +250 ms boss HP 123/123`n")
 Check (Test-AuditBossHeartbeat (Read-AuditLogDelta $log ([ref]$offset))) 'fresh heartbeat lost'
 Check (-not(Test-AuditBossHeartbeat ' HB +5 ms boss HP 0/123')) 'dead boss accepted'
 Check (-not(Test-AuditBossHeartbeat 'last=started R7B35 (fight now, in place)')) 'ack treated as boss'
 [IO.File]::WriteAllText($log,'truncated')
 MustThrow {Read-AuditLogDelta $log ([ref]$offset)}
 $csv=Join-Path $dir 'out.csv';$err=Join-Path $dir 'err.log'
 Set-Content $err 'Access denied'
 MustThrow {Assert-AuditCaptureResult $true 5 $csv $err}
 MustThrow {Assert-AuditCaptureResult $false -1 $csv $err}
 MustThrow {Assert-AuditCaptureResult $true 0 $csv $err}
 [IO.File]::WriteAllText($csv,'')
 MustThrow {Assert-AuditCaptureResult $true 0 $csv $err}
 Set-Content $csv 'MsBetweenPresents'
 Assert-AuditCaptureResult $true 0 $csv $err
 $a=Get-AuditPresentArgs 123 'C:/space here/out.csv' 60 'unique-session'
 Check ($a -match '--process_id 123' -and $a -match '--session_name unique-session' -and $a -match '--v1_metrics') 'capture arguments missing'
 Check ($a -notmatch 'stop_existing|restart_as_admin') 'unsafe session/UAC option'
 foreach($file in @('audit_screen_run.ps1','audit_capture_helpers.ps1')){
   $tokens=$null;$errors=$null
   $null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors)
   Check ($errors.Count -eq 0) "parse errors $file : $errors"
 }
 'PASS: fresh/dead/stale/truncated boss evidence; capture exit/timeout/missing/empty checks; scoped PM args; PowerShell parser'
}finally{
 # Only the explicitly created unique test directory; no game paths.
 Remove-Item -LiteralPath $csv,$err,$log -Force -ErrorAction SilentlyContinue
 Remove-Item -LiteralPath $dir -ErrorAction SilentlyContinue
}
