$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'camera_witness_lib.ps1')
$s=@{token=5;mission='0x1CBC';start=67064;placement=65092;phase=4;attempt=3;sample=1000;deadline=90000}
foreach($k in @('presented','issued','bossEntry','placementEntry','control','algo','life','playerAlive')){$s[$k]=1}
$n=0
function Require($v){if(-not $v){throw 'Fixture failed'};$script:n++}
Require (CW-Ready $s 5 1200)
foreach($change in @(@('token',6),@('mission','0xC72E'),@('placement',1),@('start',1),@('phase',5),@('phase',6),@('attempt',0),@('sample',0),@('sample',1300),@('sample',449),@('deadline',999))){$bad=$s.Clone();$bad[$change[0]]=$change[1];$failed=$false;try{$null=CW-Ready $bad 5 1200}catch{$failed=$true};Require $failed}
foreach($k in @('presented','issued','bossEntry','placementEntry','control','algo','life','playerAlive')){$bad=$s.Clone();$bad[$k]=0;$failed=$false;try{$null=CW-Ready $bad 5 1200}catch{$failed=$true};Require $failed}
$old=$s.Clone();$old.token=4;Require (-not (CW-Ready $old 5 1200))
$loading=$s.Clone();$loading.phase=3;Require (-not (CW-Ready $loading 5 1200))
foreach($failAt in @(1,2)){
 $script:checks=0;$script:pulses=0;$script:shots=0;$failed=$false
 try{CW-CaptureOnce { $script:checks++;if($script:checks -eq $failAt){throw 'identity/foreground/freshness failure'} } {$script:shots++} {$script:pulses++;'ok stick R 1 0 180 ms (3 rounds)'}|Out-Null}catch{$failed=$true}
 Require ($failed -and $script:pulses -eq 0 -and $script:shots -eq $failAt-1)
}
$script:pulses=0;$script:labels=@();$null=CW-CaptureOnce {} {param($label)$script:labels+=$label} {$script:pulses++;'ok stick R 11468 0 180 ms (3 rounds)'}
Require ($script:pulses -eq 1 -and ($script:labels -join ',') -eq 'before,after')
$script:pulses=0;$script:labels=@();$failed=$false
try{CW-CaptureOnce {} {param($label)$script:labels+=$label} {$script:pulses++;'err timeout'}|Out-Null}catch{$failed=$true}
Require ($failed -and $script:pulses -eq 1 -and ($script:labels -join ',') -eq 'before')
$tokens=$null;$errors=$null;$null=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run_camera_witness.ps1'),[ref]$tokens,[ref]$errors);Require (-not $errors)
Write-Host "PASS $n camera witness policy/orchestration checks; no native actions"
