# Pure policy; no mutations on import. Root alone creates reviewed admission.
function EHP-Admission($a,[string]$Out,[string]$PilotHash,[string]$LibHash,[datetime]$Now) {
 if($a.approved -isnot [bool] -or -not $a.approved -or $a.scope -cne 'ONE_FRESH_YAMAGATA_ENTRY_HOLD_PILOT') {throw 'No fresh entry-hold admission'}
 $pins=@{owner_pid='43708';owner_start='2026-10-11T02:34:24.0733347-04:00';source='dbd6752b0b54c51e037a2999976ce94a348ab676';mod_tree='4f4b3c045565244917fb1b96496a0a0596b3cf0c';manifest_sha256='f9847291b9a3fc9e1a212604b36fdcbe94e873bb3ae8c4fa7f590772b140a6bf';engine_sha256='ac46084cd7169cbd3ef9ba28e26bd92707ad275bcce86421c7f02e3f58846856';addon_sha256='63a7d68142912dbfa7d4059a91a9bd2955b576db6727150b6b50b9e6e626a803';pilot_sha256=$PilotHash;library_sha256=$LibHash;out_dir=$Out}
 foreach($k in $pins.Keys){if([string]$a.$k -cne $pins[$k]){throw "Admission mismatch: $k"}}
 if([string]$a.nonce -notmatch '^[a-f0-9]{32}$' -or [string]::IsNullOrWhiteSpace([string]$a.review)) {throw 'Admission nonce/review required'}
 $created=[datetime]$a.created_utc;$expires=[datetime]$a.expires_utc
 if($created.ToUniversalTime() -gt $Now.ToUniversalTime() -or $expires.ToUniversalTime() -le $Now.ToUniversalTime() -or ($expires-$created).TotalMinutes -gt 30){throw 'Admission expired/future/unbounded'}
}
function EHP-Parse([string]$Reply) {
 $v=@{}
 foreach($k in @('enabled','phase','attempt','loadSeen','arrived','boxAt','deadline','broken')) {
  $m=@([regex]::Matches($Reply,"(?:^| )$k=(\d+)(?= |$)"));if($m.Count -ne 1){throw "Missing/ambiguous hold $k"};$v[$k]=[uint64]$m[0].Groups[1].Value
 }
 if($v.phase -gt 4){throw 'Unknown hold phase'}
 foreach($k in @('enabled','loadSeen','arrived','broken')){if($v[$k] -gt 1){throw 'Invalid hold bit'}}
 return $v
}
function EHP-Held($h,[uint64]$Attempt,[uint64]$Now,[string]$Status,[switch]$Preserved) {
 if($h.enabled -ne 1 -or $h.attempt -ne $Attempt -or -not $Attempt -or $h.phase -ne 2 -or $h.broken -ne 0 -or $h.loadSeen -ne 1 -or $h.arrived -ne 1 -or -not $h.boxAt -or $h.boxAt -gt $Now -or (-not $Preserved -and $Now-$h.boxAt -gt 1500) -or $Now -ge $h.deadline){throw 'Entrance hold not currently waiting; no Summon'}
 Training-CheckStatus $Status
 foreach($kv in @(@('mission','0x1CBC'),@('stage','67064'),@('loading','0'),@('player','1'),@('infight','0'))){if((C72-Field $Status $kv[0]) -cne $kv[1]){throw 'Held destination changed'}}
}
function EHP-Log([string]$Before,[string]$After,[bool]$Released) {
 $tail=Training-LogTail $Before $After
 if($tail -match 'EXCEPTION|quarantin|Rematch: refused:|request cancel entrance hold'){throw 'Native failure/cancel; no retry'}
 $starts=@([regex]::Matches($tail,'Rematch: request Practice Yamagata \(native shrine\)[^\r\n]*queued from the practice menu'))
 if($starts.Count -gt 1){throw 'Second Practice request refused'}
 $summons=@([regex]::Matches($tail,'Rematch: request release Yamagata entrance[^\r\n]*queued from practice entrance menu'))
 if($summons.Count -gt 1 -or (-not $Released -and $summons.Count)){throw 'Early/multiple Summon'}
 if(-not $Released -and $tail -match 'arena: BossBattleMode start[^\r\n]*mission 0x1CBC'){throw 'Boss began before explicit Summon stage'}
 $returns=@([regex]::Matches($tail,'Rematch: request return from practice[^\r\n]*queued from the practice menu'))
 if($returns.Count -gt 1){throw 'Second owned return refused'}
 foreach($q in [regex]::Matches($tail,'Rematch: request [^\r\n]*')){
  if($q.Value -notmatch '^Rematch: request (Practice Yamagata \(native shrine\).*queued from the practice menu|release Yamagata entrance.*queued from practice entrance menu|return from practice.*queued from the practice menu)$'){throw 'Foreign request in owned Practice window'}
 }
 return $tail
}
function EHP-Approval($Request,$Approval) {
 foreach($k in @('nonce','stage','owner_pid','owner_start','runner_pid','runner_start','game_pid','game_start')){if([string]::IsNullOrWhiteSpace([string]$Request.$k) -or [string]$Request.$k -cne [string]$Approval.$k){throw "Foreign/stale approval: $k"}}
 if([string]::IsNullOrWhiteSpace([string]$Approval.evidence)){throw 'Explicit root evidence required'}
 if($Approval.stage -notin @('FieldReady','EnterArena','HoldMenuReviewed','ControlReviewed','NativeReturn')){throw 'Unknown stage'}
 $bits=switch($Approval.stage){'EnterArena'{@('control_response')};'HoldMenuReviewed'{@('visible_menu','held_without_boss')};'ControlReviewed'{@('visible_boss','control_response')};default{@()}}
 foreach($k in $bits){if($Approval.$k -isnot [bool] -or -not $Approval.$k){throw "Missing root observation: $k"}}
}
