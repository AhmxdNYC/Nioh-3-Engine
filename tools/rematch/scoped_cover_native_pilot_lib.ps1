# Pure validators + reviewed strict oracle, no runtime mutation on import.
$scpOracle=Join-Path $PSScriptRoot 'fight_cover_pilot_lib.ps1'
if((Get-FileHash -LiteralPath $scpOracle -Algorithm SHA256).Hash.ToLowerInvariant() -cne 'e48f9155566a60d23fc2ffaa51cf19973474d2108e1108eda03889c26822a4f5'){throw 'Reviewed FightCover oracle changed'}
. $scpOracle
function Get-SCPExpected {
 return @{source='1ffa5696b746a7806958bef2ab4bfe1a9003fb53';mod_tree='65365b00e9e2d770de334bce33850ec3f3ad2899';engine_sha256='f6f9ba6162103f06403a11307af1c01e54487bf6f5d02aa7e12b5742935c13ab';addon_sha256='ccb7dc8c6d8b4779e405f42c79939584f63e1ef10b3044406d6e3ed4471136df';gate_receipt_sha256='9043b96c237aecfdb63613c3be6e8b9a62394c176a64f5f11f67abe8dbcc4aff';backup_policy_sha256='ba3779bca245e093dc7520e1e9e0ba300f755dfb4622b69cdd86c76d68852d82'}
}
function SCP-CheckDependencies([string]$Repo) {
 FCP-CheckDependencies $Repo
 $path=Join-Path $Repo 'tools/rematch/writable_save_policy.ps1'
 if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() -cne (Get-SCPExpected).backup_policy_sha256){throw 'Writable backup policy changed'}
}
function SCP-Admission($Expected,$Receipt,[string]$ReceiptHash,[string]$Tree,[hashtable]$Hashes,[string]$GateHash,[int]$OwnerPid,[string]$OwnerStart,[string]$Out,[string]$PilotHash,[string]$LibHash,[datetime]$Now) {
 if($Receipt.approved -isnot [bool] -or -not $Receipt.approved -or $Receipt.scope -cne 'ONE_SCOPED_COVER_WRITABLE_YAMAGATA_NATIVE_PILOT'){throw 'Fresh scoped-cover admission required'}
 if($OwnerPid -ne 43708 -or $OwnerStart -cne '2026-10-11T02:34:24.0733347-04:00'){throw 'Exact root owner required'}
 if($Tree -cne $Expected.mod_tree -or $GateHash -cne $Expected.gate_receipt_sha256 -or $Hashes['Nioh3EngineFix.asi'] -cne $Expected.engine_sha256 -or $Hashes['Nioh3BossRematch.asi'] -cne $Expected.addon_sha256){throw 'Candidate or prerequisite lineage mismatch'}
 $pins=@{owner_pid=[string]$OwnerPid;owner_start=$OwnerStart;source=$Expected.source;mod_tree=$Tree;engine_sha256=$Expected.engine_sha256;addon_sha256=$Expected.addon_sha256;gate_receipt_sha256=$GateHash;pilot_sha256=$PilotHash;library_sha256=$LibHash;backup_policy_sha256=$Expected.backup_policy_sha256;save_policy='CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1';out_dir=$Out}
 foreach($k in $pins.Keys){if([string]$Receipt.$k -cne [string]$pins[$k]){throw "Admission mismatch: $k"}}
 foreach($h in @($ReceiptHash,$PilotHash,$LibHash)){if($h -cnotmatch '^[a-f0-9]{64}$'){throw 'Invalid receipt/tool hash'}}
 if([string]$Receipt.nonce -cnotmatch '^[a-f0-9]{32}$' -or [string]::IsNullOrWhiteSpace([string]$Receipt.review)){throw 'Fresh nonce and root review required'}
 $created=([datetime]$Receipt.created_utc).ToUniversalTime();$expires=([datetime]$Receipt.expires_utc).ToUniversalTime();$nowUtc=$Now.ToUniversalTime()
 if($created -gt $nowUtc -or $expires -le $nowUtc -or ($expires-$created).TotalMinutes -gt 30){throw 'Admission expired/future/unbounded'}
 if(-not $Out -or (Test-Path -LiteralPath $Out)){throw 'Admission requires a new exact output directory'}
 return @{scope=$Receipt.scope;source=$Expected.source;mod_tree=$Tree;gate_receipt_sha256=$GateHash;admission_receipt_sha256=$ReceiptHash;runtime_hashes=$Hashes;old_pass_transferred=$false;launches=1;nonce=$Receipt.nonce;backup_policy_sha256=$Expected.backup_policy_sha256;save_policy=$pins.save_policy}
}
