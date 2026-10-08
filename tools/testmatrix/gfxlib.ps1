# gfxlib.ps1 - library for gfx.ps1 / final_pc.ps1 (dot-source; has no parameters so it cannot clobber the caller's variables).
# Edits only <KEY>value</KEY> elements of the game's config.xml, in place, keys/values validated against presets.json.
$gfxHere  = Split-Path -Parent $MyInvocation.MyCommand.Path
$gfxState = Join-Path $gfxHere "state"
$PM       = Get-Content (Join-Path $gfxHere "presets.json") -Raw | ConvertFrom-Json
function Init-Gfx([string]$Config = "") {
    # $Config = another file (tests): it gets its own backup marker so the real one is never touched
    $script:gfxCfg  = if ($Config) { $Config } else { Join-Path $env:LOCALAPPDATA "KoeiTecmo\NIOH3\config.xml" }
    $script:gfxOrig = Join-Path $gfxState $(if ($Config) { "config.xml.orig.custom" } else { "config.xml.orig" })
}
Init-Gfx

function Get-Preset([string]$name) {
    # resolves _base chains; returns an ordered hashtable KEY -> value ("USER" -> empty = leave config as backed up)
    if ($name -eq "USER") { return [ordered]@{} }
    $p = $PM.presets.$name
    if (-not $p) { throw "unknown preset '$name' (known: USER, $(($PM.presets.PSObject.Properties.Name) -join ', '))" }
    $r = if ($p._base) { Get-Preset $p._base } else { [ordered]@{} }
    foreach ($pr in $p.PSObject.Properties) { if (-not $pr.Name.StartsWith("_")) { $r[$pr.Name] = [string]$pr.Value } }
    return $r
}
function Test-GfxValue([string]$key, [string]$val) {
    $lv = $PM.levels.$key
    if (-not $lv) { throw "key '$key' is not a sweepable graphics key (presets.json levels)" }
    if ($lv -notcontains $val) { throw "value '$val' not a known level of $key (known: $($lv -join ' '))" }
}
function Read-Gfx { [IO.File]::ReadAllText($gfxCfg) }
function Get-GfxValue([string]$xml, [string]$key) { $m = [regex]::Match($xml, "<$key>([^<]*)</$key>"); if ($m.Success) { $m.Groups[1].Value } else { $null } }
function Backup-Gfx {
    New-Item -ItemType Directory -Force $gfxState | Out-Null
    if (-not (Test-Path $gfxOrig)) { Copy-Item $gfxCfg $gfxOrig -Force; "backup: $gfxCfg -> $gfxOrig" }
    else { "backup: kept existing $gfxOrig (an earlier run did not restore; the original is preserved)" }
    New-Item -ItemType Directory -Force (Join-Path $gfxState "gfx_backups") | Out-Null
    Copy-Item $gfxCfg (Join-Path $gfxState ("gfx_backups\config_" + (Get-Date -f yyyyMMdd_HHmmss) + ".xml")) -Force
}
function Restore-Gfx {
    if (Test-Path $gfxOrig) { Copy-Item $gfxOrig $gfxCfg -Force; Remove-Item $gfxOrig -Force; "restored $gfxCfg from backup" } else { "no backup marker: nothing to restore" }
}
# $kv = ordered hashtable KEY -> value. Returns the list of "KEY old -> new" changes; writes unless $dry.
function Apply-Gfx($kv, [bool]$dry) {
    $xml = Read-Gfx; $ch = @()
    foreach ($k in $kv.Keys) {
        Test-GfxValue $k $kv[$k]
        $old = Get-GfxValue $xml $k
        if ($null -eq $old) { throw "config.xml has no <$k> element (game never wrote it?)" }
        if ($old -ne $kv[$k]) { $ch += "$k $old -> $($kv[$k])"; $xml = [regex]::Replace($xml, "<$k>[^<]*</$k>", "<$k>$($kv[$k])</$k>") }
    }
    $pv = Get-GfxValue $xml "PRESET"
    if ($kv.Keys.Count -gt 0 -and $null -ne $pv -and $pv -ne "CUSTOM") { $ch += "PRESET $pv -> CUSTOM"; $xml = [regex]::Replace($xml, "<PRESET>[^<]*</PRESET>", "<PRESET>CUSTOM</PRESET>") }
    if (-not $dry -and $ch.Count) { [IO.File]::WriteAllText($gfxCfg, $xml, (New-Object Text.UTF8Encoding($false))) }
    return $ch
}
# OFF|FSR for the OPTIMIZED+FG config (single key, same safety).
function Set-FgMode([string]$mode, [bool]$dry) {
    $k = $PM.fg_key[0]; $xml = Read-Gfx; $old = Get-GfxValue $xml $k
    if ($old -eq $mode) { return @() }
    if (-not $dry) { [IO.File]::WriteAllText($gfxCfg, [regex]::Replace($xml, "<$k>[^<]*</$k>", "<$k>$mode</$k>"), (New-Object Text.UTF8Encoding($false))) }
    return @("$k $old -> $mode")
}
# every (key,value) of the requested sweep keys on top of the base preset; cells equal to the base value are marked base
function Get-SweepCells([string[]]$which) {
    $keys = @(); foreach ($w in $which) { if ($w -eq "ALL") { $keys += $PM.levels.PSObject.Properties.Name } elseif ($w -eq "cheap") { $keys += $PM.sweep_groups.cheap } else { $keys += $w } }
    $base = Get-Preset $PM.base_preset; $cells = @()
    foreach ($k in ($keys | Select-Object -Unique)) { foreach ($v in $PM.levels.$k) { $cells += [pscustomobject]@{ key = $k; value = $v; base = ($base[$k] -eq $v); id = "SW_$k=$v" } } }
    return $cells
}
