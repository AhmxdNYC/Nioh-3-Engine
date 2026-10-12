# Isolated actual binary transaction + Finish-C72 failure paths. No installed files/game.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'c72e_pilot_lib.ps1')
. (Join-Path $PSScriptRoot 'c72e_binary_lib.ps1')
$tokens=$null; $errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'run_c72e_native_pilot.ps1'),[ref]$tokens,[ref]$errors)
if ($errors) { throw 'Pilot parse failed' }
$fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Finish-C72'},$true)
Invoke-Expression $fn.Extent.Text
$realReplace=${function:C72-BinaryReplace}
function C72-BinaryReplace([string]$Source,[string]$Destination,[string]$ExpectedCurrent,[string]$ExpectedNew) {
    if (($script:fault -eq 'second-stage' -and $Source -like '*candidate-b*') -or
        ($script:fault -eq 'restore' -and $Source -like '*backup*')) { throw 'Injected replacement failure' }
    & $realReplace @PSBoundParameters
}
function Close-Owned { $script:closed++; if ($script:fault -eq 'close') { throw 'Injected close failure' } }
function Get-Process { param([int]$Id,[string]$Name) if ($Id) { Microsoft.PowerShell.Management\Get-Process -Id $Id } }
function Invoke-GameLock { $script:released++; return @{code=0} }
function Check-Asis { foreach ($p in $plan) { if ((C72-BinaryHash $p.destination) -ne $p.original) { throw 'Original hash mismatch' } } }
$Root=Join-Path ([IO.Path]::GetTempPath()) ('c72e-binaries-'+[guid]::NewGuid().ToString('N'))
$names=@('Nioh3EngineFix.asi','Nioh3BossRematch.asi')
foreach ($case in @('success','second-stage','restore','foreign','close','preflight','source-changed')) {
    $OutDir=Join-Path $Root $case; $GameDir=Join-Path $OutDir 'game'
    $null=New-Item -ItemType Directory -Path $GameDir -Force
    $pins=@{}; $candidates=@{}
    for ($i=0;$i -lt 2;$i++) {
        $dst=Join-Path $GameDir $names[$i]; [IO.File]::WriteAllText($dst,"ORIGINAL-$i")
        $src=Join-Path $OutDir @('candidate-a','candidate-b')[$i]; [IO.File]::WriteAllText($src,"CANDIDATE-$i")
        $pins[$names[$i]]=C72-BinaryHash $dst
        $candidates[$names[$i]]=@{path=$src;hash=(C72-BinaryHash $src)}
    }
    $script:fault=''; $script:closed=0; $script:released=0
    $script:c72FinalState=$null # Independent transaction, not a retry of the previous fixture.
    $script:cleaned=$false; $script:touched=$false; $script:entry=$null; $script:saveHandles=$null
    $script:held=New-Object 'System.Collections.Generic.List[string]'; $script:held.Add('nioh3-pc')
    $script:result=@{status='FIXTURE';error=$null;binaries_restored=$false}
    $addonLog=Join-Path $OutDir 'absent.log'
    if ($case -eq 'preflight') {
        $candidates[$names[1]].hash='0'*64
        $rejected=$false; try { C72-BinaryPlan $GameDir $pins $candidates | Out-Null } catch { $rejected=$true }
        if (-not $rejected -or (Test-Path (Join-Path $OutDir 'backup'))) { throw 'Preflight mutated or accepted bad hash' }
        foreach ($n in $names) { if ((C72-BinaryHash (Join-Path $GameDir $n)) -ne $pins[$n]) { throw 'Preflight touched originals' } }
        Write-Host 'PASS preflight: bad candidate refused before backup/staging'; continue
    }
    $plan=@(C72-BinaryPlan $GameDir $pins $candidates)
    $script:binaryRecord=C72-BinaryPrepare $plan (Join-Path $OutDir 'backup')
    if ($case -eq 'source-changed') { [IO.File]::WriteAllText($plan[1].source,'CHANGED AFTER PREFLIGHT') }
    if ($case -eq 'second-stage') { $script:fault='second-stage' }
    $stageFailed=$false; $stageError=''; try { C72-BinaryStage $script:binaryRecord } catch { $stageFailed=$true; $stageError="$_" }
    if ($stageFailed -ne ($case -in @('second-stage','source-changed'))) { throw "Unexpected stage outcome: $case $stageError" }
    if ($stageFailed -and ((C72-BinaryHash $plan[0].destination) -ne $plan[0].candidate -or
        (C72-BinaryHash $plan[1].destination) -ne $plan[1].original)) { throw 'Partial stage was not reproduced' }
    $script:fault=if ($case -in @('restore','close')) { $case } else { '' }
    if ($case -eq 'foreign') { [IO.File]::WriteAllText($plan[0].destination,'FOREIGN OWNER') }
    $failed=$false; try { Finish-C72 } catch { $failed=$true }
    $expectedFailure=$case -in @('restore','foreign','close')
    if ($failed -ne $expectedFailure -or $script:cleaned -eq $expectedFailure -or
        $script:closed -ne 1 -or $script:released -ne [int](-not $expectedFailure)) { throw "Cleanup invariant failed: $case" }
    if (-not (Test-Path -LiteralPath $script:binaryRecord.path)) { throw 'Forgotten recovery record' }
    $receipt=Get-Content -LiteralPath $script:binaryRecord.path -Raw | ConvertFrom-Json
    if ($receipt.complete -eq $expectedFailure -or $receipt.ownerPid -ne $PID) { throw 'Recovery ownership/completion wrong' }
    if ($case -eq 'foreign' -and [IO.File]::ReadAllText($plan[0].destination) -ne 'FOREIGN OWNER') { throw 'Foreign bytes overwritten' }
    if (-not $expectedFailure) { Check-Asis }
    if ($expectedFailure -and $script:result.binaries_restored) { throw 'False restoration receipt' }
    Write-Host "PASS ${case}: owned byte restoration/recovery/lease invariants"
}
Write-Host "7 BINARY TRANSACTION FIXTURES PASS: $Root"
