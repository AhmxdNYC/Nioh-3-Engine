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
function CW-CaptureOnce([scriptblock]$Check,[scriptblock]$Shot,[scriptblock]$Pulse) {
 & $Check
 & $Shot 'before'
 & $Check # Screenshot time must not turn historical readiness into fresh permission.
 $reply=& $Pulse
 if($reply -notmatch '^ok stick R .* 180 ms \([1-9]\d* rounds\)$'){throw "Camera pulse not confirmed: $reply"}
 & $Shot 'after'
 return $reply
}
