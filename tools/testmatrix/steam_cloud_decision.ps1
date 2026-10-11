# Pure decision helpers: no process, screen, focus or input calls.
function SCC-RevealDecision([bool]$Owned,[bool]$ExpectedIdentity,[bool]$LaunchTitle,[bool]$Foreground,[bool]$DryRun,[uint32]$IdleMs,[int]$MinIdleSec) {
    if (-not $Owned -or -not $ExpectedIdentity -or -not $LaunchTitle) { return 'refuse-reveal' }
    if ($Foreground) { return 'already-foreground' }
    if ($DryRun) { return 'dryrun-reveal' }
    if ($MinIdleSec -gt 0 -and $IdleMs -lt ([long]$MinIdleSec * 1000)) { return 'user-active' }
    return 'reveal-once'
}
function SCC-Decision([bool]$Owned,[bool]$Foreground,[bool]$Header,[bool]$Button,[bool]$DryRun,[uint32]$IdleMs,[int]$MinIdleSec) {
    if (-not $Owned) { return 'foreign' }
    # CopyFromScreen cannot attribute background-window pixels to that HWND.
    if (-not $Foreground) { return 'unverified-background' }
    if (-not $Header) { return 'no-dialog' }
    if (-not $Button) { return 'no-exact-button' }
    if ($DryRun) { return 'dryrun' }
    if ($MinIdleSec -gt 0 -and $IdleMs -lt ([long]$MinIdleSec * 1000)) { return 'user-active' }
    return 'click-candidate'
}
function SCC-Button($Observation) {
    foreach ($line in $Observation.lines) {
        $words=@($line.Words)
        for ($i=0; $i -lt $words.Count-1; $i++) {
            if ($words[$i].Text -ieq 'Play' -and $words[$i+1].Text -ieq 'anyway') {
                $a=$words[$i].BoundingRect; $b=$words[$i+1].BoundingRect
                return @{x=[int]($Observation.rect.L+($a.X+$b.X+$b.Width)/2)
                    y=[int]($Observation.rect.T+$a.Y+$a.Height/2)}
            }
        }
    }
    return $null
}
