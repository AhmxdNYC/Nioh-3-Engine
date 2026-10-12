# Pure camera witness policy. No native actions on import.
function CW-Ready($s,[uint64]$Token,[uint64]$Now) {
 if ($s.token -lt $Token) { return $false }
 if ($s.token -ne $Token -or $s.mission -cne '0x1CBC' -or $s.start -ne 67064 -or $s.placement -ne 65092) { throw 'Foreign cover transaction' }
 if ($s.phase -in @(5,6)) { throw 'Cover failed/cancelled' }
 if ($s.phase -ne 4) { return $false }
 foreach($k in @('presented','issued','bossEntry','placementEntry','control','algo','life','playerAlive')) { if($s[$k] -ne 1){throw "Missing witness $k"} }
 if(-not $s.attempt -or -not $s.sample -or $s.sample -gt $Now -or $Now-$s.sample -gt 750 -or $s.sample -gt $s.deadline){throw 'Stale/invalid ready sample'}
 return $true
}
function CW-CaptureOnce([scriptblock]$Check,[scriptblock]$Shot,[scriptblock]$Pulse,[scriptblock]$Save,[hashtable]$State=@{}) {
 $before=$null;$after=$null
 $State.pulse_attempted=$false;$State.pulse_confirmed=$false;$State.images_saved=$false
 try {
  & $Check
  $before=& $Shot 'before'
  if($null -eq $before){throw 'Before capture unavailable; no input'}
  & $Check # Capture time cannot turn historical readiness into fresh permission.
  $State.pulse_attempted=$true
  $reply=& $Pulse
  if($reply -notmatch '^ok stick R .* 180 ms \([1-9]\d* rounds\)$'){throw "Camera pulse not confirmed: $reply"}
  $State.pulse_confirmed=$true
  $after=& $Shot 'after'
  if($null -eq $after){throw 'After capture unavailable; no retry'}
  # Preserve both original captures; PNG encoding is outside the freshness/input window.
  & $Save $before 'before'
  & $Save $after 'after'
  $State.images_saved=$true
  return $reply
 } finally {
  try { if($null -ne $after){$after.Dispose()} }
  finally { if($null -ne $before){$before.Dispose()} }
 }
}
