# Pure policy; no mutations on import. Root alone creates reviewed admission.
function EHP-Admission($a,[string]$Out,[string]$PilotHash,[string]$LibHash,[datetime]$Now) {
 if($a.approved -isnot [bool] -or -not $a.approved -or $a.scope -cne 'ONE_FRESH_YAMAGATA_SUMMON_INTENT_PILOT') {throw 'No fresh entry-hold admission'}
 $pins=@{owner_pid='43708';owner_start='2026-10-11T02:34:24.0733347-04:00';source='c6d9abe271e7539ab8354c695ce084c4e2d10c8a';mod_tree='670a24d6e444ad2829facd0d23a903dc001e6e95';manifest_sha256='56ed97e041d1a8ec1ccd3e406c79ebba0cbcb7f3c44f455916556e5487085ca7';engine_sha256='4f6cccf1365c5659d323fcaedcc00726234b03912875889b1165670cae5eebe8';addon_sha256='5efd90ee4792f896e75c229dbefbaf8c79a37fa3f66dc67119d35e941d0992cc';pilot_sha256=$PilotHash;library_sha256=$LibHash;out_dir=$Out}
 foreach($k in $pins.Keys){if([string]$a.$k -cne $pins[$k]){throw "Admission mismatch: $k"}}
 foreach($k in @('engine_entry_sha256','addon_entry_sha256')){if([string]$a.$k -cnotmatch '^[a-f0-9]{64}$'){throw "Fresh reviewed entry binary hash required: $k"}}
 if($a.save_policy -cne 'CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1'){throw 'Explicit writable current-entry admission required'}
 if([string]$a.nonce -notmatch '^[a-f0-9]{32}$' -or [string]::IsNullOrWhiteSpace([string]$a.review)) {throw 'Admission nonce/review required'}
 $created=[datetime]$a.created_utc;$expires=[datetime]$a.expires_utc
 if($created.ToUniversalTime() -gt $Now.ToUniversalTime() -or $expires.ToUniversalTime() -le $Now.ToUniversalTime() -or ($expires-$created).TotalMinutes -gt 30){throw 'Admission expired/future/unbounded'}
}
function EHP-Parse([string]$Reply) {
 $v=@{}
 foreach($k in @('enabled','phase','attempt','loadSeen','arrived','boxAt','deadline','pending','intentDeadline','broken')) {
  $m=@([regex]::Matches($Reply,"(?:^| )$k=(\d+)(?= |$)"));if($m.Count -ne 1){throw "Missing/ambiguous hold $k"};$v[$k]=[uint64]$m[0].Groups[1].Value
 }
 if($v.phase -gt 4){throw 'Unknown hold phase'}
 foreach($k in @('enabled','loadSeen','arrived','pending','broken')){if($v[$k] -gt 1){throw 'Invalid hold bit'}}
 return $v
}
function EHP-Held($h,[uint64]$Attempt,[uint64]$Now,[string]$Status,[switch]$Preserved) {
 if($h.enabled -ne 1 -or $h.attempt -ne $Attempt -or -not $Attempt -or $h.phase -ne 2 -or $h.broken -ne 0 -or $h.loadSeen -ne 1 -or $h.arrived -ne 1 -or -not $h.boxAt -or $h.boxAt -gt $Now -or (-not $Preserved -and $Now-$h.boxAt -gt 1500) -or $h.pending -ne 0 -or $h.intentDeadline -ne 0 -or $Now -ge $h.deadline){throw 'Entrance hold not currently waiting; no Summon'}
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
 if($Approval.stage -eq 'HoldMenuReviewed' -and ($Approval.menu_observed_seconds -isnot [ValueType] -or [double]::IsNaN([double]$Approval.menu_observed_seconds) -or [double]$Approval.menu_observed_seconds -lt 2 -or [double]$Approval.menu_observed_seconds -gt 600)){throw 'Observe held paused menu for at least2 seconds'}
 $bits=switch($Approval.stage){'EnterArena'{@('control_response')};'HoldMenuReviewed'{@('visible_menu','held_without_boss')};'ControlReviewed'{@('visible_boss','control_response')};default{@()}}
 foreach($k in $bits){if($Approval.$k -isnot [bool] -or -not $Approval.$k){throw "Missing root observation: $k"}}
}

# Samples may miss the short pending state. A released sample is valid only with
# the same attempt, cleared intent and separately checked unique native request.
function EHP-Intent($h,[uint64]$Attempt,[uint64]$Now,[string]$Status,$Tracker) {
 if($h.enabled -ne 1 -or -not $Attempt -or $h.attempt -ne $Attempt -or $h.loadSeen -ne 1 -or $h.arrived -ne 1){throw 'Summon ownership changed'}
 Training-CheckStatus $Status
 if($h.phase -eq 3){if($h.pending -ne 0 -or $h.intentDeadline -ne 0){throw 'Released intent was not consumed'};return 'released'}
 if($h.phase -ne 2){throw 'Summon left held entrance without native release'}
 $held=$h.Clone();$held.pending=0;$held.intentDeadline=0
 EHP-Held $held $Attempt $Now $Status -Preserved
 if($h.pending -eq 0){
  if($h.intentDeadline -ne 0 -or $Tracker.deadline){throw 'Summon intent expired or was cancelled; no retry'}
  return 'waiting'
 }
 if($h.pending -ne 1 -or $h.intentDeadline -le $Now -or $h.intentDeadline -gt $h.deadline -or $h.intentDeadline-$Now -gt 30000){throw 'Summon intent is expired/unbounded'}
 if($Tracker.deadline -and $Tracker.deadline -ne $h.intentDeadline){throw 'Summon intent renewed unexpectedly'}
 $Tracker.deadline=$h.intentDeadline
 return 'pending'
}
function EHP-BossEvidence([string]$Before,[string]$After) {
 $tail=EHP-Log $Before $After $true
 $queue=[regex]::Match($tail,'Rematch: request release Yamagata entrance[^\r\n]*queued from practice entrance menu')
 $create=[regex]::Match($tail,'BossLog #\d+: Character::Create placement 65092 \(arg2 \d+, flag \d+, arg4 \d+\) t=\d+(?:\.\d+)? s')
 $battle=[regex]::Match($tail,'arena: BossBattleMode start[^\r\n]*mission 0x1CBC, retry point 67064, placements created:[^\r\n]*\b65092\(')
 # Create is an entry-hook receipt, not proof the native factory returned a
 # usable actor. Explicit root visible-boss/control approval remains required.
 return $queue.Success -and $create.Success -and $battle.Success -and $queue.Index -lt $create.Index -and $create.Index -lt $battle.Index
}
