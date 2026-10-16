# Pure fixtures only: importing this library cannot enumerate/focus/click windows.
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'steam_cloud_decision.ps1')
$checks=0
function Check($Actual,$Expected,[string]$Name) {
    if ($Actual -ne $Expected) { throw "FAIL ${Name}: $Actual != $Expected" }
    $script:checks++
}
Check (SCC-RevealDecision $true $true $true $false $false 20000 20) 'reveal-once' 'covered explicitly owned launch may be raised'
Check (SCC-RevealDecision $true $true $true $false $true 20000 20) 'dryrun-reveal' 'DryRun never focuses'
Check (SCC-RevealDecision $true $true $true $false $false 19999 20) 'user-active' 'default idle gate'
Check (SCC-RevealDecision $true $false $true $false $false 20000 20) 'refuse-reveal' 'wrong PID'
Check (SCC-RevealDecision $false $true $true $false $false 20000 20) 'refuse-reveal' 'foreign window'
Check (SCC-RevealDecision $true $true $false $false $false 20000 20) 'refuse-reveal' 'Steam main/settings not raised'
Check (SCC-Decision $true $false $true $true $false 20000 20) 'unverified-background' 'covered pixels cannot authorize click'
Check (SCC-Decision $true $true $true $true $false 20000 20) 'click-candidate' 'fresh foreground exact OCR'
Check (SCC-Decision $true $true $false $false $false 20000 20) 'no-dialog' 'blank CEF after reveal still refused'
Check (SCC-Decision $true $true $false $true $false 20000 20) 'no-dialog' 'button without header refused'
Check (SCC-Decision $true $true $true $false $false 20000 20) 'no-exact-button' 'header plus color not enough'
Check (SCC-Decision $true $true $true $true $false 19999 20) 'user-active' 'fresh click idle gate'
Check (SCC-Decision $true $true $true $true $true 0 20) 'dryrun' 'read-only plan allowed while active'
Check (SCC-Decision $true $true $true $true $false 0 0) 'click-candidate' 'explicit idle override retained'
function Word([string]$Text,[int]$X) { @{Text=$Text;BoundingRect=@{X=$X;Y=20;Width=40;Height=10}} }
$o=@{rect=@{L=100;T=200};lines=@(@{Words=@((Word 'Play' 10),(Word 'anyway' 55))})}
$b=SCC-Button $o
Check $b.x 152 'exact word geometry x'
Check $b.y 225 'exact word geometry y'
$o.lines=@(@{Words=@((Word 'Cancel' 100))})
Check ([bool](SCC-Button $o)) $false 'no blue-button fallback'
$o.lines=@(@{Words=@((Word 'Play' 10),(Word 'something' 55),(Word 'anyway' 100))})
Check ([bool](SCC-Button $o)) $false 'nonadjacent phrase rejected'
$tokens=$null; $errors=$null
$path=Join-Path $PSScriptRoot 'steam_cloud_click.ps1'
$null=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
Check @($errors).Count 0 'helper parses'
$source=[IO.File]::ReadAllText($path)
Check ($source -match 'GetPixel|blue button left') $false 'color fallback removed from actual source'
Check ($source.IndexOf('$candidate=Read-Candidate $h $pids $false') -gt $source.IndexOf('if (-not [SCC]::Focus($h))')) $true 'actual source rereads after focus'
Write-Host "$checks PURE DECISION/INTEGRATION CHECKS PASS; no live UI acceptance"
