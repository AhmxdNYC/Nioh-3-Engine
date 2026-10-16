$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'scoped_cover_native_pilot_lib.ps1')
$script:checks=0
function Check($b,[string]$why){$script:checks++;if(-not $b){throw $why}}
function Refuse([scriptblock]$b){$hit=$false;try{& $b|Out-Null}catch{$hit=$true};Check $hit 'Expected refusal'}
$e=Get-SCPExpected;$now=[datetime]::UtcNow;$out=Join-Path $env:TEMP ('scoped-cover-no-create-'+[guid]::NewGuid().ToString('N'))
$hashes=@{'Nioh3EngineFix.asi'=$e.engine_sha256;'Nioh3BossRematch.asi'=$e.addon_sha256}
$a=@{approved=$true;scope='ONE_SCOPED_COVER_WRITABLE_YAMAGATA_NATIVE_PILOT';owner_pid=43708;owner_start='2026-10-11T02:34:24.0733347-04:00';source=$e.source;mod_tree=$e.mod_tree;engine_sha256=$e.engine_sha256;addon_sha256=$e.addon_sha256;gate_receipt_sha256=$e.gate_receipt_sha256;pilot_sha256=('a'*64);library_sha256=('b'*64);backup_policy_sha256=$e.backup_policy_sha256;save_policy='CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1';out_dir=$out;nonce=('c'*32);review='offline fixture';created_utc=$now.AddMinutes(-1).ToString('o');expires_utc=$now.AddMinutes(10).ToString('o')}
function Admit($receipt){SCP-Admission $e ([pscustomobject]$receipt) ('d'*64) $e.mod_tree $hashes $e.gate_receipt_sha256 43708 $a.owner_start $out ('a'*64) ('b'*64) $now}
$r=Admit $a;Check ($r.launches -eq 1 -and $r.old_pass_transferred -is [bool] -and -not $r.old_pass_transferred) 'Valid admission/lineage'
foreach($k in @('scope','owner_pid','owner_start','source','mod_tree','engine_sha256','addon_sha256','gate_receipt_sha256','pilot_sha256','library_sha256','backup_policy_sha256','save_policy','out_dir','nonce','review')){$bad=$a.Clone();$bad[$k]='wrong';if($k -eq 'review'){$bad[$k]=''};Refuse {Admit $bad}}
$bad=$a.Clone();$bad.approved='true';Refuse {Admit $bad}
$bad=$a.Clone();$bad.expires_utc=$now.ToString('o');Refuse {Admit $bad}
$bad=$a.Clone();$bad.created_utc=$now.AddMinutes(1).ToString('o');Refuse {Admit $bad}
$bad=$a.Clone();$bad.expires_utc=$now.AddMinutes(31).ToString('o');Refuse {Admit $bad}
$bad=$a.Clone();$bad.scope='ONE_FIGHT_COVER_NATIVE_PILOT_NEW_FEATURE_UNTESTED';Refuse {Admit $bad}
$bad=$a.Clone();$bad.out_dir=$env:TEMP;Refuse {SCP-Admission $e ([pscustomobject]$bad) ('d'*64) $e.mod_tree $hashes $e.gate_receipt_sha256 43708 $a.owner_start $env:TEMP ('a'*64) ('b'*64) $now}
$pilot=Join-Path $PSScriptRoot 'scoped_cover_native_pilot.ps1';$text=Get-Content -LiteralPath $pilot -Raw
$tokens=$null;$errors=$null;$ast=[Management.Automation.Language.Parser]::ParseFile($pilot,[ref]$tokens,[ref]$errors);Check (-not $errors.Count) 'PowerShell parse'
$commands=@($ast.FindAll({param($n)$n -is [Management.Automation.Language.CommandAst]},$true))
$launch=@($commands|Where-Object {$_.GetCommandName() -eq 'Start-Process'});Check ($launch.Count -eq 1) 'Exactly one launch statement'
$ad=@($commands|Where-Object {$_.GetCommandName() -eq 'SCP-Admission'})[0]
$backup=@($commands|Where-Object {$_.GetCommandName() -eq 'C72-Backup'})[0]
$policy=@($commands|Where-Object {$_.GetCommandName() -eq 'Enable-WritableSavePolicy'})[0]
Check ($ad.Extent.StartOffset -lt $backup.Extent.StartOffset -and $backup.Extent.StartOffset -lt $policy.Extent.StartOffset -and $policy.Extent.StartOffset -lt $launch[0].Extent.StartOffset) 'Admission/backup/adapter dominate launch'
Check ($text.Contains("'AutoContinueLoad' '0'") -and -not $text.Contains("'AutoContinueLoad' '1'")) 'Global preference0'
Check ($text.Contains('$coverReady=$false;$until=[DateTime]::UtcNow.AddSeconds(200)')) 'Bounded200s observer'
Check ($text.Contains('Finish-C72;if ($script:writablePolicy -and $script:cleaned) { Close-WritableSavePolicy')) 'Pinned finalizer before backup unlock only on clean'
Check ($text.Contains('-KeepFile:(-not $script:cleaned)')) 'Failure keeps guard'
Check ($text.Contains('if (-not $WritableSaves -or -not $OutDir')) 'Run explicitly opts into writable policy'
Check ($text.Contains("$([char]36)reply=Invoke-C72Pipe 'exit'")) 'Single owned return remains explicit stage-gated'
Check ($text.Contains(". (Join-Path $([char]36)lifecycleTools 'writable_save_policy.ps1')")) 'Import exactly the hash-checked adapter path'
Check ($text.IndexOf("Original entry is the target mission") -lt $text.IndexOf("$([char]36)beforeGen=Training-BeginScan")) 'Reject same-target original entry before request'
$plan=(& $pilot -DryRun|Out-String)|ConvertFrom-Json
Check ($plan.mode -ceq 'PLAN_ONLY_NO_MUTATION' -and $plan.auto_continue_load -eq 0 -and $plan.observer_seconds -eq 200 -and $plan.expected.source -ceq $e.source) 'Actual default plan'
Check (-not (Test-Path -LiteralPath $out)) 'Plan/admission do not create output'
Write-Host "PASS $script:checks scoped admission/plan/orchestration checks; no game or approval minted"
