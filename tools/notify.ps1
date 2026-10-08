# Nioh 3 wrapper around universal-optimizer\tools\notify (project key "nioh3").
#   .\tools\notify.ps1 progress [--text ..] [--done ..]     (counts notes\remaining-work.md)
#   .\tools\notify.ps1 win --title .. --before .. --after .. --device ally --version 1.7.25
param([Parameter(Mandatory, Position=0)][string]$Type, [Parameter(ValueFromRemainingArguments)][string[]]$Rest)
$py = 'C:\Users\Ahmad Hamza\universal-optimizer\tools\notify\notify.py'
$extra = @('--project', 'nioh3')
if ($Type -eq 'progress' -and ($Rest -notcontains '--percent') -and ($Rest -notcontains '--json')) { $extra += @('--from-md', '--file', 'notes/remaining-work.md') }
py -I $py $Type @extra @Rest
