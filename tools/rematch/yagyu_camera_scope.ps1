function Read-YagyuCameraAdmission([string]$Path,[string]$RunDir){
 $a=Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json
 if($a.approved -isnot [bool] -or -not $a.approved -or $a.scope -cne 'ONE_HOSTED_ALL_BOSS_SHORT_SWEEP'){throw 'Camera admission absent'}
 if([IO.Path]::GetFullPath($RunDir).TrimEnd('\') -cne [IO.Path]::GetFullPath([string]$a.out_dir).TrimEnd('\')){throw 'Camera run differs from admitted output'}
 if(@($a.rows).Count -ne 1 -or $a.rows[0] -cne 'R1A5E' -or $a.auto_camera_return -isnot [bool] -or $a.auto_camera_return){throw 'Camera singleton/manual-return scope mismatch'}
 if(-not $a.owner_pid -or -not $a.owner_start -or -not $a.expires_utc -or [datetime]::UtcNow -ge ([datetime]$a.expires_utc).ToUniversalTime()){throw 'Camera admission expired or owner absent'}
 return $a
}
