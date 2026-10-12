$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'camera_witness_lib.ps1')
$s=@{token=5;mission='0x1CBC';start=67064;placement=65092;phase=4;attempt=3;sample=1000;deadline=90000}
foreach($k in @('presented','issued','bossEntry','placementEntry','control','algo','life','playerAlive')){$s[$k]=1}
$n=0
function Require($v){if(-not $v){throw ("Fixture failed at line {0}; fault={1}" -f (Get-PSCallStack)[1].ScriptLineNumber,$script:fault)};$script:n++}
Require (CW-Ready $s 5 1200)
foreach($change in @(@('token',6),@('mission','0xC72E'),@('placement',1),@('start',1),@('phase',5),@('phase',6),@('attempt',0),@('sample',0),@('sample',1300),@('sample',449),@('deadline',999))){$bad=$s.Clone();$bad[$change[0]]=$change[1];$failed=$false;try{$null=CW-Ready $bad 5 1200}catch{$failed=$true};Require $failed}
foreach($k in @('presented','issued','bossEntry','placementEntry','control','algo','life','playerAlive')){$bad=$s.Clone();$bad[$k]=0;$failed=$false;try{$null=CW-Ready $bad 5 1200}catch{$failed=$true};Require $failed}
$old=$s.Clone();$old.token=4;Require (-not (CW-Ready $old 5 1200))
$loading=$s.Clone();$loading.phase=3;Require (-not (CW-Ready $loading 5 1200))

class CWTestImage {
 [string]$Label
 [int]$Disposals=0
 [bool]$FailDispose=$false
 CWTestImage([string]$label){$this.Label=$label}
 [void]Dispose(){ $this.Disposals++;if($this.FailDispose){throw 'dispose fault'} }
}
# Actual orchestration with captured-object identity, ordering and failures, no drawing/input.
foreach($fault in @('none','check1','before','check2','pulse','reply','after','save-before','save-after','dispose-before','dispose-after')) {
 $script:events=@();$script:checks=0;$script:pulses=0;$script:images=@();$script:fault=$fault
 $state=@{};$failed=$false;$result=$null
 try {
  $result=CW-CaptureOnce -Check {
   $script:checks++;$script:events+='check'+$script:checks
   if($script:fault -eq ('check'+$script:checks)){throw 'identity/foreground/freshness fault'}
  } -Shot {
   param($label)
   $script:events+='capture-'+$label
   if($script:fault -eq $label){throw 'capture fault'}
   $image=[CWTestImage]::new($label);$image.FailDispose=$script:fault -eq ('dispose-'+$label)
   $script:images+=,$image
   return $image
  } -Pulse {
   $script:pulses++;$script:events+='pulse'
   if($script:fault -eq 'pulse'){throw 'timeout after possible send'}
   if($script:fault -eq 'reply'){return 'err timeout'}
   return 'ok stick R 11468 0 180 ms (3 rounds)'
  } -Save {
   param($image,$label)
   $script:events+='encode-'+$label
   Require ($image.Label -ceq $label -and $image.Disposals -eq 0)
   Require ([object]::ReferenceEquals($image,$script:images[$(if($label -eq 'before'){0}else{1})]))
   Require ($script:pulses -eq 1 -and $script:images.Count -eq 2)
   if($script:fault -eq ('save-'+$label)){throw 'PNG encoding failed'}
  } -State $state
 } catch { $failed=$true }
 Require ($failed -eq ($fault -ne 'none'))
 Require ($script:pulses -eq $(if($fault -in @('check1','before','check2')){0}else{1}))
 foreach($image in $script:images){Require ($image.Disposals -eq 1)}
 if($fault -in @('save-before','save-after','after','pulse','reply','dispose-before','dispose-after')){Require ($null -eq $result -and $state.pulse_attempted)}
 if($fault -in @('save-before','save-after')){Require (-not $state.images_saved -and $state.pulse_confirmed)}
 if($fault -eq 'none'){
  Require (($script:events -join ',') -ceq 'check1,capture-before,check2,pulse,capture-after,encode-before,encode-after')
  Require ($state.images_saved -and $state.pulse_confirmed -and $result -match '^ok stick')
 }
}
# Encoding may exceed the freshness window: it occurs only after the one confirmed pulse and both captures.
$script:clock=1200;$script:pulses=0;$script:encoded=0;$state=@{}
$null=CW-CaptureOnce -Check {$null=CW-Ready $s 5 $script:clock} -Shot {param($label)[CWTestImage]::new($label)} -Pulse {$script:pulses++;'ok stick R 11468 0 180 ms (3 rounds)'} -Save {$script:clock+=1300;$script:encoded++} -State $state
Require ($script:pulses -eq 1 -and $script:encoded -eq 2 -and $script:clock -gt 1750 -and $state.images_saved)
# The original 750ms bound is unchanged; slow capture fails before pulse, slow encode occurs after it.
Require (CW-Ready $s 5 1750)
$failed=$false;try{$null=CW-Ready $s 5 1751}catch{$failed=$true};Require $failed
$script:clock=1200;$script:pulses=0;$script:slowImage=[CWTestImage]::new('before');$failed=$false
try{CW-CaptureOnce -Check {$null=CW-Ready $s 5 $script:clock} -Shot {$script:clock+=600;return $script:slowImage} -Pulse {$script:pulses++} -Save {}|Out-Null}catch{$failed=$true}
Require ($failed -and $script:pulses -eq 0 -and $script:slowImage.Disposals -eq 1)
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run_camera_witness.ps1'),[ref]$tokens,[ref]$errors);Require (-not $errors)
$call=$ast.Find({param($node)$node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -eq 'CW-CaptureOnce'},$true)
Require ($null -ne $call)
$blocks=@{}
for($i=0;$i -lt $call.CommandElements.Count-1;$i++){
 $arg=$call.CommandElements[$i]
 if($arg -is [Management.Automation.Language.CommandParameterAst]){$blocks[$arg.ParameterName]=$call.CommandElements[$i+1].Extent.Text}
}
Require ($blocks.Shot -match 'CopyFromScreen' -and $blocks.Shot -notmatch '\.Save\(')
Require ($blocks.Save -match '\.Save\(' -and $blocks.Save -notmatch 'CW-Pipe|CW-Guard')
Require ($blocks.Shot -match '\$bitmap.Dispose\(\)' -and $blocks.Shot -match '\$graphics.Dispose\(\)')
Require ($blocks.Pulse -ceq "{CW-Pipe 'nioh3vpad' 'stick R 0.35 0 180' 3800}")
Require ($ast.Extent.Text -match 'Input readiness expired during connect' -and $ast.Extent.Text -match 'FAILED_AFTER_PULSE_ATTEMPT_NO_RETRY')
Write-Host "PASS $n camera witness policy/orchestration checks; no native actions"
