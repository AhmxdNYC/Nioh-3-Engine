$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'pinned_operator.ps1')
. (Join-Path $PSScriptRoot 'pinned_benchmark_lib.ps1')
$script:checks=0
function Check([bool]$Condition,[string]$Name) { if (-not $Condition) { throw "FAIL $Name" }; $script:checks++ }
function MustThrow([scriptblock]$Body,[string]$Name) { $threw=$false; try { &$Body } catch { $threw=$true }; Check $threw $Name }
$tempBase=[IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\')+'\'
$fixture=Join-Path $tempBase ('nioh3-operator-'+[guid]::NewGuid().ToString('N'))
try {
    $op=New-PBOperator $fixture
    Check (Test-Path -LiteralPath (Join-Path $fixture 'session.json')) 'session persisted'
    MustThrow { New-PBOperator $fixture | Out-Null } 'second operator refuses existing session'
    $request=@{protocol=1;session=$op.session;sequence=7;nonce='current'}
    $good=@{protocol=1;session=$op.session;sequence=7;nonce='current';answer='READY';observed=$true;evidence='fixture observation'}
    Check (Test-PBOperatorReply $request $good) 'current observed reply'
    foreach ($bad in @(@{nonce='old'},@{session='foreign'},@{sequence=6},@{protocol=2},@{observed=$false},@{observed='true'},@{evidence=' '},@{answer=7})) {
        $reply=$good.Clone(); foreach ($key in $bad.Keys) { $reply[$key]=$bad[$key] }
        Check (-not (Test-PBOperatorReply $request $reply)) ('reject '+($bad.Keys -join ','))
    }
    Check (-not (Test-PBOperatorReply $request $null)) 'missing reply'
    Check ((Assert-PBOperatorAnswer 'gameplay-ready' 'READY') -ceq 'READY') 'observed ready accepted'
    Check ((Assert-PBOperatorAnswer 'window-confirm' 'NO') -ceq 'NO') 'negative window preserved'
    Check ((Assert-PBOperatorAnswer 'quit' 'DONE') -ceq 'DONE') 'observed quit accepted'
    MustThrow { Assert-PBOperatorAnswer 'gameplay-ready' 'YES' } 'blind yes cannot acknowledge readiness'
    MustThrow { Assert-PBOperatorAnswer 'quit' 'READY' } 'readiness cannot acknowledge quit'
    MustThrow { Assert-PBOperatorAnswer 'window-confirm' 'ABORT' } 'operator abort'
    Check ((Assert-PBFgOff '<config><FRAME_GENERATION>OFF</FRAME_GENERATION></config>') -ceq 'OFF') 'configured FG OFF readback'
    foreach ($bad in '<config/>','<FRAME_GENERATION>ON</FRAME_GENERATION>','<FRAME_GENERATION>off</FRAME_GENERATION>','<FRAME_GENERATION>OFF</FRAME_GENERATION><FRAME_GENERATION>OFF</FRAME_GENERATION>') {
        MustThrow { Assert-PBFgOff $bad } 'invalid FG config blocks launch'
    }
    $script:polls=0
    $answer=Wait-PBOperatorAnswer $op 'gameplay-ready' 'fixture prompt' @{cell='A1'} 3 {
        $script:polls++
        $current=Get-Content -LiteralPath (Join-Path $fixture 'request.json') -Raw | ConvertFrom-Json
        $reply=@{protocol=1;session=$current.session;sequence=$current.sequence;nonce=$current.nonce;answer='READY';observed=$true;evidence='fixture observed HUD'}
        if ($script:polls -eq 1) { $reply.nonce='stale' }
        Write-PBOperatorJson (Join-Path $fixture 'response.json') $reply
    }
    Check ($script:polls -eq 2 -and $answer -ceq 'READY') 'actual waiter ignores stale answer'
    $answered=Get-Content -LiteralPath (Join-Path $fixture 'request.json') -Raw | ConvertFrom-Json
    Check ($answered.state -eq 'answered' -and $answered.context.cell -eq 'A1') 'accepted request archived with context'
    Check (Test-Path -LiteralPath (Join-Path $fixture 'answered_1.json')) 'request response pair retained'
    MustThrow { Wait-PBOperatorAnswer $op 'quit' 'fixture quit' @{} 0 {} | Out-Null } 'unanswered timeout'
    MustThrow { Wait-PBOperatorAnswer $op 'quit' 'fixture quit' @{} 3 { throw 'owner gone' } | Out-Null } 'owner loss interrupts waiter'
    $script:polls=0
    $answer=Wait-PBOperatorAnswer $op 'window-confirm' 'fixture confirmation' @{} 3 {
        $script:polls++
        $current=Get-Content -LiteralPath (Join-Path $fixture 'request.json') -Raw | ConvertFrom-Json
        if ($script:polls -eq 1) { [IO.File]::WriteAllText((Join-Path $fixture 'response.json'),'{partial') }
        else { Write-PBOperatorJson (Join-Path $fixture 'response.json') @{protocol=1;session=$current.session;sequence=$current.sequence;nonce=$current.nonce;answer='NO';observed=$true;evidence='fixture invalid gameplay'} }
    }
    Check ($script:polls -eq 2 -and $answer -ceq 'NO') 'malformed partial response cannot acknowledge'
    foreach ($file in 'pinned_operator.ps1','pinned_final_pc.ps1','test_pinned_operator.ps1') {
        $tokens=$null;$errors=$null
        $null=[System.Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors)
        Check ($errors.Count -eq 0) ('parse '+$file)
    }
    "ALL $script:checks OBSERVED OPERATOR CHECKS PASSED"
} finally {
    $resolved=[IO.Path]::GetFullPath($fixture)
    if (-not $resolved.StartsWith($tempBase,[StringComparison]::OrdinalIgnoreCase) -or (Split-Path -Leaf $resolved) -notlike 'nioh3-operator-*') { throw 'Fixture cleanup escaped temporary root' }
    if (Test-Path -LiteralPath $resolved) { Remove-Item -LiteralPath $resolved -Recurse -Force }
}
