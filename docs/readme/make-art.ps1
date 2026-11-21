# Regenerates the animated pixel-art SVGs used by README.md.
#
#   powershell -ExecutionPolicy Bypass -File docs/readme/make-art.ps1
#
# The art is drawn in code, like the game: the scripts in art/ paint pixels into layers
# and write them as SVG rectangles, with CSS and SMIL animation. Colors and the
# traveler sprite are copied from src/ (Theme.h, the region art files, Player.cpp).
# Pass one or more names (hero, forest, ruins, garden, edge, journal, badges, divider,
# footer) to rebuild only those files.

param([string[]]$Only = @())

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2
$Only = @($Only | ForEach-Object { $_.Split(',') } | Where-Object { $_ }) # "-Only a,b" arrives as one string via -File

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
foreach ($part in @('Core', 'Font', 'Traveler', 'Hero', 'Regions', 'Journal', 'Extras')) {
    . (Join-Path $here "art\$part.ps1")
}

$targets = [ordered]@{
    hero    = { Write-Hero }
    forest  = { Write-Forest }
    ruins   = { Write-Ruins }
    garden  = { Write-Garden }
    edge    = { Write-Edge }
    journal = { Write-Journal }
    badges  = { Write-Badges }
    divider = { Write-Divider }
    footer  = { Write-Footer }
}

foreach ($name in $targets.Keys) {
    if ($Only.Count -gt 0 -and $Only -notcontains $name) { continue }
    & $targets[$name]
    Save-Svg (Join-Path $here "$name.svg")
}
