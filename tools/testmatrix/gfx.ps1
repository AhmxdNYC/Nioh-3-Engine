<#
  gfx.ps1 - graphics preset / per-setting switcher for the game's config.xml (final phase, PC; the Ally uses the same keys).
  Edits only <KEY>value</KEY> elements named in presets.json, in place (encoding/line endings kept, no BOM written), after backing
  the original up to tools\testmatrix\state\config.xml.orig (created once, crash-safe: an existing .orig is never overwritten).
  Run with:  powershell -NoProfile -ExecutionPolicy Bypass -File gfx.ps1 ...

    -Action List                              presets + sweepable keys
    -Action Show   -Preset HIGH               resolved key list and the current value of each
    -Action Apply  -Preset HIGH [-DryRun]     backup (if none) + write the preset; USER = put the backed-up file back
    -Action Apply  -Set GI=HIGH,REN_RAT=85 [-Preset MAX] [-DryRun]   preset (optional), then single overrides
    -Action SweepPlan [-Sweep cheap|ALL|GI,SSR]   the per-setting sweep cells (every level once on top of the base preset)
    -Action Restore                           put the backup back, remove the .orig marker
    -Config <path>   operate on another file (tests). The game must be closed while applying (it rewrites config.xml on exit).
  Library functions (Get-Preset / Apply-Gfx / Set-FgMode / Backup-Gfx / Restore-Gfx / Get-SweepCells) live in gfxlib.ps1.
#>
param([ValidateSet("List", "Show", "Apply", "SweepPlan", "Restore", "Backup")][string]$Action = "List",
      [string]$Preset = "", [string[]]$Set = @(), [string[]]$Sweep = @("cheap"), [string]$Config = "", [switch]$DryRun)
. (Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) "gfxlib.ps1")
Init-Gfx $Config
switch ($Action) {
    "List" { "presets: USER (current config)  " + (($PM.presets.PSObject.Properties.Name) -join "  "); "sweep keys: " + (($PM.levels.PSObject.Properties | ForEach-Object { "$($_.Name)[$($_.Value -join ',')]" }) -join "  ") }
    "Show" {
        $kv = Get-Preset $(if ($Preset) { $Preset } else { "USER" }); $xml = Read-Gfx
        foreach ($k in $kv.Keys) { "{0,-24} {1,-10} -> {2}" -f $k, (Get-GfxValue $xml $k), $kv[$k] }
        "(config: $gfxCfg)"
    }
    "Backup" { Backup-Gfx }
    "Restore" { Restore-Gfx }
    "SweepPlan" { $c = Get-SweepCells $Sweep; $c | ForEach-Object { "{0,-34} {1}" -f $_.id, $(if ($_.base) { "(= base preset $($PM.base_preset): reuse the base cell)" } else { "" }) }; "$($c.Count) cells ($(@($c | Where-Object { -not $_.base }).Count) non-base)" }
    "Apply" {
        $kv = Get-Preset $(if ($Preset) { $Preset } else { "USER" })
        foreach ($s in $Set) { foreach ($pair in ($s -split ",")) { $a = $pair -split "=", 2; if ($a.Count -ne 2) { throw "bad -Set item '$pair' (KEY=VALUE)" }; $kv[$a[0]] = $a[1] } }
        if ($DryRun) { "== DRY RUN (config untouched): $gfxCfg"; $ch = Apply-Gfx $kv $true; if ($ch.Count) { $ch } else { "(no change)" }; return }
        Backup-Gfx
        if ($Preset -eq "USER" -and $Set.Count -eq 0) { Copy-Item $gfxOrig $gfxCfg -Force; "USER: backed-up config in place" }
        else { Copy-Item $gfxOrig $gfxCfg -Force; $ch = Apply-Gfx $kv $false; if ($ch.Count) { $ch } else { "(no change)" } }
    }
}
