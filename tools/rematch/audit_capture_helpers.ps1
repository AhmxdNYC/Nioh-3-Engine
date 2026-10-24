# Pure parsing + incremental evidence helpers; no processes are launched when dot-sourced.
function Get-AuditLogOffset([string]$Path) {
    if (Test-Path -LiteralPath $Path) { return [long](Get-Item -LiteralPath $Path).Length }
    return [long]0
}
function Read-AuditLogDelta([string]$Path, [ref]$Offset) {
    if (-not (Test-Path -LiteralPath $Path)) { return '' }
    $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        if ($stream.Length -lt $Offset.Value) { throw 'AUDIT-FATAL: Training log truncated during fight; cannot verify current boss' }
        $null = $stream.Seek($Offset.Value, [IO.SeekOrigin]::Begin)
        $reader = New-Object IO.StreamReader($stream)
        try { $text = $reader.ReadToEnd(); $Offset.Value = $stream.Position; return $text } finally { $reader.Dispose() }
    } finally { $stream.Dispose() }
}
function Test-AuditBossHeartbeat([string]$Text) {
    return [regex]::IsMatch($Text, ' HB \+\d+ ms boss HP [1-9]\d*/[1-9]\d*')
}
function Get-AuditPresentArgs([int]$GameProcessId,[string]$Csv,[int]$Seconds,[string]$Session) {
    return "--process_id $GameProcessId --output_file `"$Csv`" --timed $Seconds --terminate_after_timed --session_name $Session --v1_metrics --no_console_stats"
}
function Assert-AuditCaptureResult([bool]$Exited,[int]$Code,[string]$Csv,[string]$ErrorLog) {
    $detail = if(Test-Path -LiteralPath $ErrorLog){(Get-Content -LiteralPath $ErrorLog -Tail 12) -join ' '}else{'no stderr'}
    if(-not $Exited){throw "AUDIT-FATAL: PresentMon did not exit within timeout; $detail"}
    if($Code -ne 0){throw "AUDIT-FATAL: PresentMon exit $Code; $detail"}
    if(-not(Test-Path -LiteralPath $Csv)){throw "AUDIT-FATAL: PresentMon produced no CSV; $detail"}
    if((Get-Item -LiteralPath $Csv).Length -eq 0){throw 'AUDIT-FATAL: PresentMon CSV empty'}
}
function Assert-AuditForeground([int]$GameProcessId) {
    [uint32]$foregroundPid=0
    $null=[AuditFocus]::GetWindowThreadProcessId([AuditFocus]::GetForegroundWindow(),[ref]$foregroundPid)
    if($foregroundPid -ne $GameProcessId){throw "AUDIT-FATAL: game lost foreground (wanted $GameProcessId, got $foregroundPid); sample invalid"}
}
function Set-AuditForeground([int]$GameProcessId) {
    # Focus only the registered game PID, never Steam or another process.
    if(-not ('AuditFocus' -as [type])) {
        Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices;
public static class AuditFocus {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr h,int n);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk,byte sc,uint fl,UIntPtr ex);
}
"@
    }
    $game=Get-Process -Id $GameProcessId -ErrorAction Stop
    # Steam's launch splash can hold the foreground for a few seconds: retry (ALT tap lifts the focus-steal lock) before failing
    for($i=0;$i -lt 10;$i++){
        $game.Refresh()
        $null=[AuditFocus]::ShowWindowAsync($game.MainWindowHandle,9)
        [AuditFocus]::keybd_event(0x12,0,0,[UIntPtr]::Zero); [AuditFocus]::keybd_event(0x12,0,2,[UIntPtr]::Zero)
        $null=[AuditFocus]::SetForegroundWindow($game.MainWindowHandle)
        Start-Sleep -Milliseconds 500
        [uint32]$fp=0; $null=[AuditFocus]::GetWindowThreadProcessId([AuditFocus]::GetForegroundWindow(),[ref]$fp)
        if($fp -eq $GameProcessId){break}
    }
    Assert-AuditForeground $GameProcessId
}
