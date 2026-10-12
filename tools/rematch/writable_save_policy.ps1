# Opt-in adapter only. Import has no side effects; C72 defaults remain unchanged.
function Assert-WritableSaveDependencies([string]$Directory) {
 $pins=@{'c72e_pilot_lib.ps1'='03fd4554849f23eba0f69992b60d1723352b854cbf306ca1f1361b0525e0d94d';'run_c72e_native_pilot.ps1'='a2e077a66708c2ed1278bc9bddc1c64489c5a0127d4178b6efee48f199925ea6'}
 foreach($name in $pins.Keys){if((Get-FileHash -LiteralPath (Join-Path $Directory $name) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $pins[$name]){throw "Writable-save dependency changed: $name"}}
}
function Enable-WritableSavePolicy {
 [CmdletBinding()]
 param([switch]$OptIn,[Parameter(Mandatory)][string]$Root,[Parameter(Mandatory)][string]$Backup,
  [Parameter(Mandatory)]$Manifest,[Parameter(Mandatory)]$CurrentHandles,
  [Parameter(Mandatory)][string]$Receipt,[Parameter(Mandatory)]$HeldLeases,
  [Parameter(Mandatory)][string]$AdmissionSha256)
 if(-not $OptIn){throw 'Explicit writable-current-entry policy opt-in required'}
 Assert-WritableSaveDependencies $PSScriptRoot
 if($AdmissionSha256 -cnotmatch '^[a-f0-9]{64}$'){throw 'New reviewed admission hash required'}
 if($HeldLeases.Count -ne 1 -or $HeldLeases[0] -cne 'nioh3-pc'){throw 'Requires caller-owned PC lease'}
 if(Get-Process -Name Nioh3 -ErrorAction SilentlyContinue){throw 'Establish writable policy before game launch'}
 $rootFull=C72-Full $Root;$backupFull=C72-Full $Backup;$receiptFull=C72-Full $Receipt
 if($rootFull -eq $backupFull -or (C72-Inside $rootFull $backupFull) -or (C72-Inside $backupFull $rootFull)){throw 'Backup and live saves overlap'}
 foreach($path in @($rootFull,$backupFull)){if((C72-Inside $path $receiptFull) -or $path -eq $receiptFull){throw 'Policy receipt overlaps save census'}}
 if(Test-Path -LiteralPath $receiptFull){throw 'Policy receipt must be new'}
 if(-not $Manifest.Count -or $CurrentHandles.Count -ne $Manifest.Count){throw 'Exact owned save handle census required'}
 $seen=@{}
 foreach($h in $CurrentHandles){
  if($h -isnot [IO.FileStream] -or -not $h.CanRead -or $h.CanWrite){throw 'Expected owned read-only file handles'}
  $name=C72-Full $h.Name
  if(-not (C72-Inside $rootFull $name)){throw 'Foreign handle refused'}
  $rel=$name.Substring($rootFull.Length+1)
  if(-not $Manifest.ContainsKey($rel) -or $seen.ContainsKey($rel)){throw 'Foreign/duplicate save handle refused'}
  $seen[$rel]=$true
 }
 $backupHandles=$null;$temporary=$receiptFull+'.'+[guid]::NewGuid().ToString('N')+'.tmp'
 try {
  $backupHandles=C72-LockSaves $backupFull $Manifest
  if(-not (C72-Same $Manifest (C72-Manifest $rootFull))){throw 'Live entry changed before writable handoff'}
  $record=[ordered]@{policy='CURRENT_ENTRY_WRITABLE_REVERT_AFTER_OWNED_EXIT_V1';state='VERIFIED_BACKUP_BEFORE_OWN_HANDLE_RELEASE';
   owner_pid=$PID;owner_start=(Get-Process -Id $PID).StartTime.ToString('o');created_utc=[datetime]::UtcNow.ToString('o');
   admission_sha256=$AdmissionSha256;save_root=$rootFull;backup_root=$backupFull;entry_manifest=$Manifest;
   automatic_restore_permitted=$false;requires='Pinned finalizer, no remaining game, retained PC lease; review ownership after restart'}
  C72-Json $temporary $record
  $stream=[IO.File]::Open($temporary,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
  try{$stream.Flush($true)}finally{$stream.Dispose()}
  $check=Get-Content -LiteralPath $temporary -Raw|ConvertFrom-Json
  if($check.policy -cne $record.policy -or $check.state -cne $record.state -or $check.admission_sha256 -cne $AdmissionSha256 -or
     $check.save_root -cne $rootFull -or $check.backup_root -cne $backupFull){throw 'Policy receipt readback failed'}
  $readManifest=@{}
  foreach($property in $check.entry_manifest.psobject.Properties){$readManifest[$property.Name]=@{length=$property.Value.length;sha256=$property.Value.sha256}}
  if(-not (C72-Same $Manifest $readManifest) -or $check.automatic_restore_permitted -isnot [bool] -or $check.automatic_restore_permitted){throw 'Policy receipt census/ownership readback failed'}
  [IO.File]::Move($temporary,$receiptFull)
  C72-UnlockSaves $CurrentHandles
  return @{policy=$record.policy;receipt=$receiptFull;backupHandles=$backupHandles;entry=$Manifest}
 } catch {
  if($backupHandles){C72-UnlockSaves $backupHandles}
  throw
 } finally {
  if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force -ErrorAction SilentlyContinue}
 }
}
function Close-WritableSavePolicy($Policy,[bool]$Cleaned) {
 if(-not $Cleaned){throw 'Keep backup protection until pinned cleanup succeeds; recovery evidence retained'}
 if($Policy){C72-UnlockSaves $Policy.backupHandles}
 # Never delete backup or policy/recovery receipts.
}
