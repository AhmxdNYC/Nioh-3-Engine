# Observed file prompts for a device worker whose shell cannot use Read-Host.
# Loading this file is read-only. Responses are data, never commands to execute.
function New-PBOperator([string]$Directory) {
    New-Item -ItemType Directory -Path $Directory -Force | Out-Null
    $session = [guid]::NewGuid().ToString('N')
    $state = [pscustomobject]@{ directory=$Directory; session=$session; sequence=0 }
    $stream = [IO.File]::Open((Join-Path $Directory 'session.json'), [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::Read)
    try {
        $bytes = [Text.Encoding]::UTF8.GetBytes((@{protocol=1;session=$session;pid=$PID;started=(Get-Date).ToString('o')} | ConvertTo-Json -Compress))
        $stream.Write($bytes,0,$bytes.Length)
    } finally { $stream.Dispose() }
    return $state
}
function Write-PBOperatorJson([string]$Path, $Value) {
    $temp = $Path + '.' + [guid]::NewGuid().ToString('N') + '.tmp'
    [IO.File]::WriteAllText($temp, ($Value | ConvertTo-Json -Depth 8), (New-Object Text.UTF8Encoding($false)))
    Move-Item -LiteralPath $temp -Destination $Path -Force
}
function Test-PBOperatorReply($Request, $Reply) {
    if (-not $Reply -or $Reply.protocol -ne 1 -or $Reply.session -cne $Request.session -or
        $Reply.nonce -cne $Request.nonce -or [string]$Reply.sequence -cne [string]$Request.sequence -or
        $Reply.answer -isnot [string]) { return $false }
    # A screenshot/observation is required for every acknowledgement. This is an
    # operator attestation, not automated proof that the screenshot shows gameplay.
    if ($Reply.observed -ne $true -or $Reply.observed -isnot [bool] -or
        $Reply.evidence -isnot [string] -or [string]::IsNullOrWhiteSpace($Reply.evidence)) { return $false }
    return $true
}
function Assert-PBOperatorAnswer([string]$Stage,[string]$Answer) {
    if ($Answer -ceq 'ABORT') { throw 'Operator aborted; restore entry state' }
    $allowed = if ($Stage -eq 'window-confirm') { @('YES','NO') } elseif ($Stage -eq 'quit') { @('DONE') } else { @('READY') }
    if ($allowed -cnotcontains $Answer) { throw "Invalid observed answer for $Stage" }
    return $Answer
}
function Wait-PBOperatorAnswer($Operator, [string]$Stage, [string]$Prompt, $Context,
                               [int]$TimeoutSec, [scriptblock]$CheckOwner) {
    $Operator.sequence++
    $request = [ordered]@{ protocol=1;session=$Operator.session;sequence=$Operator.sequence;
        nonce=[guid]::NewGuid().ToString('N');stage=$Stage;prompt=$Prompt;context=$Context;
        created=(Get-Date).ToString('o');state='waiting' }
    $path = Join-Path $Operator.directory 'request.json'
    $replyPath = Join-Path $Operator.directory 'response.json'
    Write-PBOperatorJson $path $request
    $deadline=(Get-Date).AddSeconds($TimeoutSec)
    while ($true) {
        & $CheckOwner
        if (Test-Path -LiteralPath $replyPath) {
            $reply=$null
            try { $reply=Get-Content -LiteralPath $replyPath -Raw | ConvertFrom-Json } catch { }
            if (Test-PBOperatorReply $request $reply) {
                $request.state='answered'
                $request.answer=$reply.answer
                $request.evidence=$reply.evidence
                $request.answered=(Get-Date).ToString('o')
                Write-PBOperatorJson $path $request
                $archive=Join-Path $Operator.directory ('answered_' + $request.sequence + '.json')
                Write-PBOperatorJson $archive @{request=$request;response=$reply}
                return $reply.answer
            }
        }
        if ((Get-Date) -ge $deadline) { throw "Observed operator response timed out at $Stage; recovery required" }
        Start-Sleep -Milliseconds 250
    }
}
