# Fallback for callers that genuinely need a child. Preloaded watcher uses no child.
function Invoke-YagyuReviewedChild([string]$Exe,[string[]]$Arguments,[string]$Stdout,[string]$Stderr,[int]$TimeoutMs=15000){
 $child=Start-Process -FilePath $Exe -ArgumentList $Arguments -WindowStyle Hidden -PassThru -RedirectStandardOutput $Stdout -RedirectStandardError $Stderr
 try{
  # Windows PowerShell loses redirected-process ExitCode unless Handle is retained before exit.
  $retainedHandle=$child.Handle
  if(-not $child.WaitForExit($TimeoutMs)){$child.Kill();$child.WaitForExit();throw 'Owned child timeout; outcome unknown; no retry'}
  $child.Refresh();$exitCode=$child.ExitCode
  if($null -eq $exitCode){throw 'Owned child exit code UNKNOWN; inspect exact receipt; no retry'}
  if($exitCode -ne 0){throw "Owned child exited $exitCode; no retry"}
  return @{exit_code=[int]$exitCode;confirmed=$true}
 }finally{$child.Dispose()}
}
