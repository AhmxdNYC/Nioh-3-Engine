$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'test_camera_witness.ps1')
# Read-only retained-handle identity tests target this test process, never a game.
Add-Type -Path (Join-Path $PSScriptRoot 'camera_witness_process.cs')
$self=Get-Process -Id $PID
$created=$self.StartTime.ToUniversalTime().ToFileTimeUtc();$exe=$self.Path
Require ([CWProcessLease]::CheckIdentity($PID,$created,$exe))
foreach($bad in @(@(($created+1),$exe),@($created,($exe+'.foreign')))){$failed=$false;try{$null=[CWProcessLease]::CheckIdentity($PID,$bad[0],$bad[1])}catch{$failed=$true};Require $failed}
$self.Dispose()
$initial=@{ownerPid=1;ownerStart='2026-10-12T00:00:00Z';harnessPid=2;harnessStart='2026-10-12T00:00:01Z';deadline='2026-10-12T01:00:00Z';games=@(@{pid=3;start='2026-10-12T00:00:02Z';closed=$false})}
CW-CheckGuardRecord $initial $initial ([DateTime]'2026-10-12T00:30:00Z');Require $true
foreach($k in @('ownerPid','ownerStart','harnessPid','harnessStart','deadline')){
 $bad=$initial.Clone();$bad[$k]='changed';$failed=$false;try{CW-CheckGuardRecord $bad $initial ([DateTime]'2026-10-12T00:30:00Z')}catch{$failed=$true};Require $failed
}
foreach($k in @('pid','start','closed')){
 $bad=$initial.Clone();$bad.games=@($initial.games[0].Clone());$bad.games[0][$k]=if($k -eq 'closed'){$true}else{'changed'}
 $failed=$false;try{CW-CheckGuardRecord $bad $initial ([DateTime]'2026-10-12T00:30:00Z')}catch{$failed=$true};Require $failed
}
$failed=$false;try{CW-CheckGuardRecord $initial $initial ([DateTime]$initial.deadline)}catch{$failed=$true};Require $failed
$bad=$initial.Clone();$bad.games=@();$failed=$false;try{CW-CheckGuardRecord $bad $initial ([DateTime]'2026-10-12T00:30:00Z')}catch{$failed=$true};Require $failed
# Execute the runner's ACTUAL Check callback. Terminal native sample never refreshes.
# Simulate only latency of the transport/identity boundary; no pipe or input operation.
Add-Type 'public static class CWNative { public static ulong Tick=1000; public static ulong GetTickCount64(){return Tick;} }'
$Token=5;$attempt=3
$actualCheck=[scriptblock]::Create($blocks.Check.Substring(1,$blocks.Check.Length-2))
function CW-Snapshot {
 [CWNative]::Tick+=$script:guardCost*2
 $ready=CW-Ready $s $Token ([CWNative]::GetTickCount64())
 return @{ready=$ready;state=$s;raw='fixture'}
}
function CW-Pipe {param($name,$command)
 [CWNative]::Tick+=$script:guardCost*2
 return 'mission=0x1CBC loading=0 player=1 infight=1'
}
function FCP-CheckStatus {param($status)if($status -cne 'mission=0x1CBC loading=0 player=1 infight=1'){throw 'bad fixture status'}}
function C72-Field {param($status,$key)if($status -match "(?:^| )$key=([^ ]+)"){return $Matches[1]};throw 'missing fixture field'}
foreach($cost in @(200,2)){
 [CWNative]::Tick=1000;$script:guardCost=$cost;$script:pulses=0;$receipt=@{};$failed=$false
 try{
  CW-CaptureOnce -Check $actualCheck -Shot {param($label)[CWNative]::Tick+=80;[CWTestImage]::new($label)} -Pulse {
   [CWNative]::Tick+=$script:guardCost*2
   if(-not (CW-Ready $script:armedSample $Token ([CWNative]::GetTickCount64()))){throw 'input stale'}
   $script:pulses++;'ok stick R 11468 0 180 ms (3 rounds)'
  } -Save {} -State $receipt|Out-Null
 }catch{$failed=$true}
 Require ($failed -eq ($cost -eq 200));Require ($script:pulses -eq $(if($cost -eq 200){0}else{1}))
}
# Even fast guards cannot rescue a late pulse: actual pre-write policy still refuses.
[CWNative]::Tick=1000;$script:guardCost=2;$script:pulses=0;$receipt=@{};$failed=$false
try{CW-CaptureOnce -Check $actualCheck -Shot {param($label)[CWTestImage]::new($label)} -Pulse {
 [CWNative]::Tick=1751;$null=CW-Ready $script:armedSample $Token ([CWNative]::GetTickCount64());$script:pulses++
} -Save {} -State $receipt|Out-Null}catch{$failed=$true}
Require ($failed -and $script:pulses -eq 0)
$guard=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq 'CW-Guard'},$true)
Require ($guard.Extent.Text -notmatch 'Get-Process' -and $guard.Extent.Text -match '\$lease.Check\(\)' -and $guard.Extent.Text -match 'CW-CheckGuardRecord')
Require ($ast.Extent.Text -match 'finally \{ \$lease.Dispose\(\) \}')
Write-Host "PASS $n total camera policy, actual-callback latency and retained self-identity checks; no game/input"
