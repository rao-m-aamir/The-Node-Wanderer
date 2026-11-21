# The four region cards (240x135 each, the game's 16:9): one moment from each list, framed
# like the game's screen with the HUD label (top left) and the pointer-code line (bottom left).

$CardW = 240
$CardH = 135

function Card-Begin([string]$label) {
    Begin-Svg $CardW $CardH 3 $label
    $r = $CardW - 1; $b = $CardH - 1
    AddDef "<clipPath id=`"cardClip`"><path d=`"M1 0H${r}V1H${CardW}V${b}H${r}V${CardH}H1V${b}H0V1H1Z`"/></clipPath>"
    Add '<g clip-path="url(#cardClip)">'
}

function Card-End {
    Add ((Rect 0 0 $CardW 1 $UiNavy) + (Rect 0 ($CardH - 1) $CardW 1 $UiNavy) + (Rect 0 1 1 ($CardH - 2) $UiNavy) + (Rect ($CardW - 1) 1 1 ($CardH - 2) $UiNavy))
    Add '</g>'
}

# Soft dark corners, like the game's vignette.
function Vignette([int]$w, [int]$h, [double]$alpha = 0.6) {
    AddDef "<radialGradient id=`"vig`" cx=`"50%`" cy=`"45%`" r=`"72%`"><stop offset=`"0.6`" stop-color=`"$UiNavy`" stop-opacity=`"0`"/><stop offset=`"1`" stop-color=`"$UiNavy`" stop-opacity=`"$(N $alpha)`"/></radialGradient>"
    Add (Rect 0 0 $w $h 'url(#vig)')
}

# A small dark HUD card with cut corners and an accent bar. Returns markup.
function Hud-Card([double]$x, [double]$y, [double]$w, [double]$h, [string]$accent, [double]$alpha = 0.82) {
    $a = " fill-opacity=`"$(N $alpha)`""
    return (Rect ($x + 1) $y ($w - 2) $h $UiNavy $a) + (Rect $x ($y + 1) 1 ($h - 2) $UiNavy $a) +
        (Rect ($x + $w - 1) ($y + 1) 1 ($h - 2) $UiNavy $a) + (Rect ($x + 4) ($y + 4) 2 ($h - 8) $accent)
}

# Top left: "REGION n" in the region's accent color, then the place name.
function Card-Label([int]$region, [string]$place) {
    $accent = $Accents[$region - 1]
    $w = [Math]::Max((Text-Width "REGION $region"), (Text-Width $place)) + 17
    Add (Hud-Card 5 5 $w 28 $accent)
    Add (Text "REGION $region" 13 9 $accent)
    Add (Text $place 13 19 $UiText)
}

# Bottom left: the code line for the last move, like the game's toast. Returns markup.
function Card-Code([int]$region, [string]$code, [string]$color = $CodeBlue, [string]$attrs = '') {
    $w = (Text-Width $code) + 17
    return "<g$attrs>" + (Hud-Card 5 ($CardH - 20) $w 15 $Accents[$region - 1]) + (Text $code 13 ($CardH - 16) $color) + '</g>'
}

# A light that floats near the traveler and drifts toward the link to follow.
function Wisp-Svg([double]$x, [double]$y, [string]$class, [string]$color = $WarmLight) {
    return "<g transform=`"translate($(N $x) $(N $y))`"><g class=`"$class`"><g class=`"bob`">" +
        (Glow 1 1 7 'wispGlow') + (Rect 0 0 2 2 (Mix $color '#ffffff' 0.6)) + '</g></g></g>'
}

function Def-Wisp([string]$color = $WarmLight) {
    Def-Glow 'wispGlow' $color 0.55
    AddCss '@keyframes bob { 0%, 100% { transform: translateY(0px) } 50% { transform: translateY(-3px) } }'
    Anim '.bob' 'bob' 1.4 'ease-in-out'
}

# Leaves or petals drifting down from (x, y), each on its own slow loop.
function Falling-Svg([double[]]$spots, [string[]]$colors, [string]$name) {
    AddCss "@keyframes $name { 0% { transform: translate(0px,0px); opacity: 0 } 10% { opacity: 1 } 35% { transform: translate(-7px,30px) } 65% { transform: translate(3px,62px) } 88% { opacity: 1 } 100% { transform: translate(-5px,96px); opacity: 0 } }"
    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $spots.Length; $i += 2) {
        $k = $i / 2
        Anim ".$name$k" $name (7 + ($k % 3) * 1.3) 'linear' "-$(N ($k * 2.3))s"
        $c = $colors[$k % $colors.Length]
        [void]$sb.Append("<g class=`"$name$k`">$(Rect $spots[$i] $spots[$i + 1] 2 1 $c)$(Rect ($spots[$i] + 1) ($spots[$i + 1] + 1) 1 1 (Mix $c '#000000' 0.25))</g>")
    }
    return $sb.ToString()
}

# ---------------------------------------------------------------------------
# Region 1, The First Connection: singly linked list. The traveler is on N1;
# warm light rises from N2 (current->next), and N3 is mist until it becomes next.
# ---------------------------------------------------------------------------

function Write-Forest {
    $f = $Forest
    Card-Begin 'Region 1, The First Connection: a forest where every ledge is a node of a singly linked list. The traveler stands on N1, warm light rises from N2 (the node next points to), and N3 is still mist.'
    Def-Traveler
    Def-Overlays
    Def-Wisp
    Forest-Backdrop $CardW $CardH 176 62 100 @(28, 138, 214) 124 22
    $ledges = @(
        [pscustomobject]@{ Shape = 'Cliff'; X = -6; W = 80; Top = 106; Seed = 1 }
        [pscustomobject]@{ Shape = 'Boulder'; X = 98; W = 50; Top = 98; Seed = 2 }
        [pscustomobject]@{ Shape = 'Log'; X = 174; W = 52; Top = 90; Seed = 3 }
    )
    $layers = @()
    foreach ($l in $ledges) {
        $layer = New-Layer $CardW $CardH
        Paint-ForestLedge $layer $l.Shape $l.X $l.W $l.Top $l.Seed
        Add (Layer-Svg $layer)
        $layers += $layer
    }
    Add (Mask-Svg $layers[2] $f.Veil ' fill-opacity=".62"') # not current, not next: mist
    Add (LinkGlow-Svg 98 50 98 'next')

    $canopy = New-Layer $CardW $CardH
    Paint-Canopy $canopy 146 250 300
    Add (Layer-Svg $canopy)
    AddCss '@keyframes sway { 0%, 100% { transform: rotate(-3deg) } 50% { transform: rotate(3deg) } }'
    Anim '.vine1' 'sway' 5 'ease-in-out'
    Anim '.vine2' 'sway' 6.5 'ease-in-out' '-2s'
    Add (Vine-Svg 166 5 24 11 'vine1')
    Add (Vine-Svg 201 6 34 12 'vine2')
    Add (Vine-Svg 229 4 18 13 'vine1')

    Add (Traveler-Idle 46 106)
    AddCss '@keyframes toNext { 0%, 100% { transform: translate(0px,0px) } 50% { transform: translate(30px,8px) } }'
    Anim '.toNext' 'toNext' 4 'ease-in-out'
    Add (Wisp-Svg 59 82 'toNext')
    Add (Falling-Svg @(176, 10, 208, 14, 156, 6, 224, 3) @($f.Moss, $f.MossLight, $f.Moss) 'leaf')
    Add (Fireflies-Svg 8 40 0 $CardW 46 100)
    $layer = New-Layer $CardW $CardH
    Paint-Grass $layer 74 98 6 900
    Paint-Grass $layer 148 174 7 950
    Paint-Grass $layer 226 240 8 990
    Add (Layer-Svg $layer)
    Vignette $CardW $CardH
    Card-Label 1 'The First Connection'
    Add (Card-Code 1 'current = current->next;')
    Card-End
}

# ---------------------------------------------------------------------------
# Shared stone painting for the ruins, the garden, and the edge.
# ---------------------------------------------------------------------------

# Paints a platform from row spans: $rows[dy] is @(left1, right1, left2, right2, ...).
# $pal = @(top row, base, left shade, right lit, depth color). Mortar courses every
# $course rows (0 = none), with the joints offset on alternate courses.
function Paint-Stone($layer, [int]$top, $rows, $pal, [int]$course = 0, [string]$mortar = '') {
    for ($dy = 0; $dy -lt $rows.Count; $dy++) {
        $r = $rows[$dy]
        if ($null -eq $r) { continue }
        $y = $top + $dy
        $t = [Math]::Min(0.6, $dy / 40.0)
        for ($k = 0; $k -lt $r.Length; $k += 2) {
            $l = $r[$k]; $rt = $r[$k + 1]
            if ($rt -le $l) { continue }
            $base = $pal[1]; if ($dy -eq 0) { $base = $pal[0] }
            Fill $layer $l $y ($rt - $l) 1 (Mix $base $pal[4] $t)
            Fill $layer $l $y 2 1 (Mix $pal[2] $pal[4] $t)
            Fill $layer ($rt - 1) $y 1 1 (Mix $pal[3] $pal[4] $t)
            if ($course -gt 0 -and $dy -gt 2) {
                $band = I (($dy - 3) / $course)
                if (($dy - 3) % $course -eq 0) { Fill $layer ($l + 2) $y ($rt - $l - 3) 1 (Mix $mortar $pal[4] $t) }
                else {
                    for ($x = $l + 4 + ($band % 2) * 5; $x -lt $rt - 2; $x += 10) { Dot $layer $x $y (Mix $mortar $pal[4] $t) }
                }
            }
        }
    }
}

# Moss on top and a few strands hanging down the sides.
function Paint-Moss($layer, [int]$x0, [int]$w, [int]$top, [int]$seed, $moss) {
    for ($i = 0; $i -lt (I ($w / 8)); $i++) {
        $mx = $x0 + (I ((Hash01 ($seed * 43 + $i)) * ($w - 4)))
        Fill $layer $mx $top (2 + $i % 3) 1 $moss[1]
        Dot $layer ($mx + 1) ($top - 1) $moss[0]
    }
    for ($i = 0; $i -lt (I ($w / 12)); $i++) {
        $mx = $x0 + 2 + (I ((Hash01 ($seed * 71 + $i)) * ($w - 4)))
        $len = 2 + (I ((Hash01 ($seed * 13 + $i)) * 6))
        Fill $layer $mx ($top + 1) 1 $len $moss[2]
    }
}

$Ruins = @{
    SkyTop = '#0a1024'; SkyMid = '#1c2e52'; SkyLow = '#4c6488'; Moon = '#e2ecff'
    Far = '#2e4062'; Mid = '#1c2840'; MidLit = '#364868'; Mist = '#96b2d2'; Depth = '#10182a'
    Water = '#223e64'; WaterDeep = '#080e1e'
    StoneLight = '#b8c0cc'; Stone = '#788296'; StoneDark = '#485068'; StoneShade = '#303850'
    MossLight = '#9ccc96'; Moss = '#5e9270'; MossDark = '#325c52'; Root = '#463834'
    Banner = '#94363e'; BannerDark = '#642430'; Relic = '#d6b060'; Fire = '#ffa84c'; FireHot = '#ffe896'
}

# Row spans for a ruins platform $depth rows deep (shapes from src/RuinsArt.cpp).
# Arch: a span over an arch whose right pier broke off. Capital: the top of a column.
# Shrine: a slab with a cornice on a wide base.
function Ruin-Rows([string]$shape, [int]$x0, [int]$w, [int]$depth) {
    $x1 = $x0 + $w
    $cx = $x0 + (I ($w / 2))
    $rows = New-Object 'System.Collections.Generic.List[object]'
    for ($dy = 0; $dy -lt $depth; $dy++) {
        $span = [int[]]@($x0, $x1)
        if ($shape -eq 'Arch' -and $dy -ge 6) {
            $d = 17 - $dy
            $half = 11.0
            if ($d -gt 0) { $half = [Math]::Sqrt([Math]::Max(0, 121 - $d * $d)) }
            if ($half -ge 1) {
                $openL = [int][Math]::Round($cx - $half)
                $openR = [int][Math]::Round($cx + $half)
                if ($dy -lt 13) { $span = [int[]]@($x0, $openL, $openR, $x1) }
                elseif ($dy -lt 15) { $span = [int[]]@($x0, $openL, ($openR + 3 * ($dy - 12)), ($x1 - 2 * ($dy - 12))) } # broken off
                else { $span = [int[]]@($x0, $openL) }
            }
        } elseif ($shape -eq 'Capital') {
            if ($dy -ge 3 -and $dy -lt 7) { $in = ($dy - 2) * 2; $span = [int[]]@(($x0 + $in), ($x1 - $in)) }
            elseif ($dy -ge 7 -and $dy -lt 9) { $span = [int[]]@(($x0 + 10), ($x1 - 10)) }
            elseif ($dy -ge 9) { $span = [int[]]@(($x0 + 11), ($x1 - 11)) }
        } elseif ($shape -eq 'Shrine') {
            if ($dy -eq 1) { $span = [int[]]@(($x0 - 1), ($x1 + 1)) }
            elseif ($dy -ge 2 -and $dy -lt 5) { $span = [int[]]@(($x0 + 1), ($x1 - 1)) }
            elseif ($dy -ge 5) { $span = [int[]]@(($x0 + 3), ($x1 - 3)) }
        }
        $rows.Add($span)
    }
    return , $rows
}

# A stone brazier standing on a platform top; lit ones get fire frames separately.
function Paint-Brazier($layer, [int]$cx, [int]$top, [switch]$Cold) {
    $p = $Ruins
    Fill $layer ($cx - 3) ($top - 5) 7 1 $p.StoneLight
    Fill $layer ($cx - 3) ($top - 4) 7 1 $p.Stone
    Fill $layer ($cx - 2) ($top - 3) 5 1 $p.StoneDark
    Fill $layer ($cx - 1) ($top - 2) 3 1 $p.StoneDark
    Fill $layer ($cx - 2) ($top - 1) 5 1 $p.StoneShade
    if ($Cold) { Fill $layer ($cx - 2) ($top - 6) 5 1 $p.Root; Dot $layer $cx ($top - 7) $p.Root } # cold coals
}

# Two flickering fire frames above a brazier at (cx, top), with a warm glow and embers.
function Fire-Svg([int]$cx, [int]$top) {
    $frames = @(@('..#..', '.##..', '.#h#.', '##hh#', '#hhh#'), @('...#.', '..##.', '.#h#.', '#hh##', '#hhh#'))
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append((Glow $cx ($top - 8) 18 'fireGlow' ' class="flicker"'))
    for ($f = 0; $f -lt 2; $f++) {
        $layer = New-Layer ($cx + 4) $top
        for ($r = 0; $r -lt 5; $r++) {
            for ($c = 0; $c -lt 5; $c++) {
                $code = $frames[$f][$r][$c]
                if ($code -eq '#') { Dot $layer ($cx - 2 + $c) ($top - 10 + $r) $Ruins.Fire }
                elseif ($code -eq 'h') { Dot $layer ($cx - 2 + $c) ($top - 10 + $r) $Ruins.FireHot }
            }
        }
        [void]$sb.Append((Layer-Svg $layer " class=`"fire$f`""))
    }
    for ($i = 0; $i -lt 3; $i++) {
        [void]$sb.Append("<rect class=`"ember`" style=`"animation-delay:-$(N ($i * 0.6))s`" x=`"$($cx - 1 + $i)`" y=`"$($top - 11)`" width=`"1`" height=`"1`" fill=`"$($Ruins.FireHot)`"/>")
    }
    return $sb.ToString()
}

# Far ruins on the skyline: towers and walls, some with lit openings and crenellations.
function Paint-Skyline($layer, [int]$baseY, [int]$seed, [string]$color, [string]$opening) {
    $x = -3
    $i = 0
    while ($x -lt $layer.W) {
        $w = 6 + (I ((Hash01 ($seed + $i * 7)) * 14))
        $h = 6 + (I ((Hash01 ($seed + $i * 13)) * 28))
        $kind = Hash01 ($seed + $i * 29)
        if ($kind -lt 0.25) { $h = 2 + (I ($h / 6)) } # a fallen stretch
        Fill $layer $x ($baseY - $h) $w ($layer.H) $color
        if ($kind -gt 0.8) { for ($c = $x; $c -lt $x + $w; $c += 2) { Dot $layer $c ($baseY - $h - 1) $color } }
        if ($kind -gt 0.55 -and $w -gt 9 -and $h -gt 14) { # an arched opening with sky behind it
            $ox = $x + (I ($w / 2)) - 1
            Fill $layer $ox ($baseY - $h + 5) 3 6 $opening
            Dot $layer ($ox + 1) ($baseY - $h + 4) $opening
        }
        $x += $w
        $i++
    }
}

# Midground colonnade: dark columns with a lit right edge, some broken, some joined by beams.
function Paint-Colonnade($layer, [double[]]$xs, [int]$seed) {
    $p = $Ruins
    $tops = @()
    $broken = @()
    for ($i = 0; $i -lt $xs.Length; $i++) {
        $b = (Hash01 ($seed + $i * 17)) -gt 0.6
        $t = 30 + (I ((Hash01 ($seed + $i * 11)) * 14))
        if ($b) { $t += 14 }
        elseif ($i -gt 0 -and -not $broken[$i - 1] -and (Hash01 ($seed + $i * 23)) -gt 0.4) { $t = $tops[$i - 1] } # a pair under one beam
        $tops += $t
        $broken += $b
    }
    for ($i = 0; $i -lt $xs.Length; $i++) {
        $x = [int]$xs[$i]
        $top = $tops[$i]
        Fill $layer $x $top 6 ($layer.H) $p.Mid
        Fill $layer ($x + 5) ($top + 3) 1 ($layer.H) $p.MidLit
        if ($broken[$i]) { Fill $layer ($x + 2) ($top - 2) 3 2 $p.Mid; Dot $layer ($x + 4) ($top - 3) $p.Mid; continue }
        Fill $layer ($x - 2) $top 10 2 $p.Mid # capital
        Fill $layer ($x - 2) $top 10 1 $p.MidLit
        if ($i + 1 -lt $xs.Length -and -not $broken[$i + 1] -and $tops[$i + 1] -eq $top) {
            Fill $layer ($x - 2) ($top - 3) ([int]$xs[$i + 1] - $x + 10) 3 $p.Mid # a beam to the next column
        }
    }
}

# ---------------------------------------------------------------------------
# Region 2, The Forgotten Return: doubly linked list. The traveler is on N3 with the
# key's light, facing back toward N2. Cool light falls on N2 (previous) and warm light
# rises from N4 (next), because both links are stored. No ledge is mist.
# ---------------------------------------------------------------------------

function Ruins-Backdrop {
    $p = $Ruins
    Def-Linear 'rSky' @("0 $($p.SkyTop) 1", "0.5 $($p.SkyMid) 1", "1 $($p.SkyLow) 1")
    Def-Glow 'moonWide' $p.Moon 0.16
    Def-Glow 'moonCore' $p.Moon 0.4
    Def-Linear 'rHaze' @("0 $($p.Mist) 0", "1 $($p.Mist) 0.3")
    Def-Linear 'rBand' @("0 $($p.Mist) 0", "0.5 $($p.Mist) 0.2", "1 $($p.Mist) 0")
    AddCss '@keyframes drift { 0%, 100% { transform: translateX(0px) } 50% { transform: translateX(-18px) } }'
    Anim '.drift' 'drift' 16 'ease-in-out'
    Anim '.star' 'twinkle' 3.4 'ease-in-out'

    Add (Rect 0 0 $CardW $CardH 'url(#rSky)')
    $stars = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt 16; $i++) {
        $sx = I ((Hash01 (300 + $i * 7)) * $CardW); $sy = 3 + (I ((Hash01 (300 + $i * 11)) * 48))
        [void]$stars.Append("<rect class=`"star`" style=`"animation-delay:-$(N ($i * 0.53))s`" x=`"$sx`" y=`"$sy`" width=`"1`" height=`"1`" fill=`"$($p.Moon)`"/>")
    }
    Add $stars.ToString()
    Add (Glow 196 28 70 'moonWide')
    Add (Glow 196 28 22 'moonCore')
    $sky = New-Layer $CardW $CardH
    Disc $sky 196 28 9 $p.Moon
    foreach ($c in @(@(193, 25), @(198, 31), @(200, 24), @(194, 32))) { Dot $sky $c[0] $c[1] (Mix $p.Moon $p.SkyMid 0.22) }
    Paint-Skyline $sky 100 41 $p.Far (Mix $p.Far $p.SkyLow 0.55)
    Add (Layer-Svg $sky)
    Add (Rect 0 52 $CardW 64 'url(#rHaze)')
    Add "<g class=`"drift`">$(Rect -20 84 ($CardW + 60) 16 'url(#rBand)')</g>"
    $mid = New-Layer $CardW $CardH
    Paint-Colonnade $mid @(16, 50, 84, 152, 186, 228) 77
    Add (Layer-Svg $mid)
    Add (Rect 0 70 $CardW 65 'url(#rHaze)')
}

# The shrine's posts, lintel, and the empty pedestal where the key was.
function Paint-Shrine($layer, [int]$x0, [int]$w, [int]$top) {
    $p = $Ruins
    $x1 = $x0 + $w
    foreach ($px in @(($x0 + 3), ($x1 - 7))) {
        Fill $layer $px ($top - 18) 4 18 $p.Stone
        Fill $layer $px ($top - 18) 1 18 $p.StoneDark
        Fill $layer ($px + 3) ($top - 18) 1 18 $p.StoneLight
    }
    Fill $layer ($x0 + 1) ($top - 22) ($w - 2) 4 $p.Stone # lintel
    Fill $layer ($x0 + 1) ($top - 22) ($w - 2) 1 $p.StoneLight
    Fill $layer ($x0 + 1) ($top - 19) ($w - 2) 1 $p.StoneDark
    for ($x = $x0 + 5; $x -lt $x1 - 5; $x += 6) { Fill $layer $x ($top - 21) 2 1 $p.StoneDark }
    $cx = $x0 + (I ($w / 2))
    Fill $layer ($cx - 3) ($top - 6) 7 6 $p.Stone # pedestal
    Fill $layer ($cx - 3) ($top - 6) 7 1 $p.Relic
    Fill $layer ($cx - 1) ($top - 5) 3 2 $p.StoneShade # empty: the traveler carries the key's light
}

function Write-Ruins {
    $p = $Ruins
    Card-Begin 'Region 2, The Forgotten Return: flooded ruins where the ledges form a doubly linked list. The traveler stands on N3 holding the key light; cool light falls on N2 (previous) and warm light rises from N4 (next).'
    Def-Traveler
    Def-Overlays
    Def-Wisp $CoolLight
    Def-Glow 'fireGlow' $p.Fire 0.42
    AddCss '@keyframes fireFrame { 0% { opacity: 1 } 50% { opacity: 0 } 100% { opacity: 0 } }'
    Anim '.fire0' 'fireFrame' 0.36 'steps(1,end)'
    Anim '.fire1' 'fireFrame' 0.36 'steps(1,end)' '-0.18s'
    AddCss '.fire1 { opacity: 0; }'
    AddCss '@keyframes flicker { 0%, 100% { opacity: .85 } 30% { opacity: 1 } 55% { opacity: .7 } 80% { opacity: .95 } }'
    Anim '.flicker' 'flicker' 1.1
    AddCss '@keyframes emberUp { 0% { transform: translate(0px,0px); opacity: 0 } 15% { opacity: 1 } 100% { transform: translate(2px,-16px); opacity: 0 } }'
    Anim '.ember' 'emberUp' 1.8
    Ruins-Backdrop

    $stonePal = @($p.StoneLight, $p.Stone, $p.StoneDark, $p.StoneLight, $p.Depth)
    $moss = @($p.MossLight, $p.Moss, $p.MossDark)
    $layer = New-Layer $CardW $CardH
    Paint-Stone $layer 94 (Ruin-Rows 'Arch' 6 64 41) $stonePal 6 $p.StoneShade        # N2 Old Hall
    Paint-Stone $layer 84 (Ruin-Rows 'Capital' 100 44 51) $stonePal 0                 # N3 Colonnade
    for ($dy = 9; $dy -lt 51; $dy++) { foreach ($fx in @(114, 118, 122, 126)) { Dot $layer $fx (84 + $dy) (Mix $p.StoneDark $p.Depth ([Math]::Min(0.6, $dy / 40.0))) } }
    Paint-Stone $layer 96 (Ruin-Rows 'Shrine' 170 60 39) $stonePal 6 $p.StoneShade    # N4 Key Shrine
    Paint-Shrine $layer 170 60 96
    Paint-Moss $layer 6 64 94 21 $moss
    Paint-Moss $layer 100 44 84 22 $moss
    Paint-Moss $layer 170 60 96 23 $moss
    Paint-Brazier $layer 54 94 -Cold
    Paint-Brazier $layer 216 96
    Add (Layer-Svg $layer)

    Add (LinkGlow-Svg 6 64 94 'prev' -Previous)
    Add (LinkGlow-Svg 170 60 96 'next')
    Add (Fire-Svg 216 96)

    # A banner hanging from the shrine's lintel, swaying from its top.
    $banner = New-Layer $CardW $CardH
    $rows = @('gggggg', 'dbbbbb', 'dbbbbb', 'dbggbb', 'dbggbb', 'dbbbbb', 'dbbbbb', 'dbbbbb', 'dbbbbb', 'db..bb', 'd....b')
    for ($r = 0; $r -lt $rows.Length; $r++) {
        for ($c = 0; $c -lt 6; $c++) {
            $code = $rows[$r][$c]
            $color = $null
            if ($code -ceq 'g') { $color = $p.Relic } elseif ($code -ceq 'd') { $color = $p.BannerDark } elseif ($code -ceq 'b') { $color = $p.Banner }
            if ($color) { Dot $banner (182 + $c) (78 + $r) $color }
        }
    }
    AddCss '@keyframes sway { 0%, 100% { transform: rotate(-4deg) } 50% { transform: rotate(4deg) } }'
    Anim '.banner' 'sway' 4.2 'ease-in-out'
    Add (Layer-Svg $banner ' class="banner" style="transform-origin: 185px 78px"')

    # Still water over the flooded hall, with the moon's reflection shimmering on it.
    Def-Linear 'rWater' @("0 $($p.Water) 0.82", "1 $($p.WaterDeep) 0.97")
    Add (Rect 0 116 $CardW 19 'url(#rWater)')
    Add (Rect 0 116 $CardW 1 $p.Mist ' fill-opacity=".35"')
    AddCss '@keyframes shimmer { 0%, 100% { opacity: .25; transform: translateX(0px) } 50% { opacity: .9; transform: translateX(1px) } }'
    Anim '.shimmer' 'shimmer' 2.2 'ease-in-out'
    $dashes = @(@(118, 9), @(120, 6), @(122, 8), @(125, 5), @(128, 6), @(131, 3))
    for ($i = 0; $i -lt $dashes.Count; $i++) {
        $d = $dashes[$i]
        $dx = 196 - (I ($d[1] / 2)) + (($i % 3) - 1)
        Add "<rect class=`"shimmer`" style=`"animation-delay:-$(N ($i * 0.37))s`" x=`"$dx`" y=`"$($d[0])`" width=`"$($d[1])`" height=`"1`" fill=`"$($p.Moon)`"/>"
    }
    foreach ($rp in @(@(30, 121, 7), @(80, 126, 5), @(140, 119, 6), @(150, 130, 8), @(226, 124, 5))) {
        Add "<rect class=`"shimmer`" style=`"animation-delay:-$(N ($rp[0] / 37.0))s`" x=`"$($rp[0])`" y=`"$($rp[1])`" width=`"$($rp[2])`" height=`"1`" fill=`"$($p.Mist)`" fill-opacity=`".5`"/>"
    }

    Add (Traveler-Idle 122 84 -Mirror)
    AddCss '@keyframes toPrev { 0%, 100% { transform: translate(0px,0px) } 50% { transform: translate(-26px,8px) } }'
    Anim '.toPrev' 'toPrev' 4 'ease-in-out'
    Add (Wisp-Svg 106 62 'toPrev' $CoolLight)
    Vignette $CardW $CardH
    Card-Label 2 'The Forgotten Return'
    Add (Card-Code 2 'current = current->previous;')
    Card-End
}

# ---------------------------------------------------------------------------
# Region 3, The Endless Garden: circular singly linked list. The traveler is on the
# tail (N4). Its next is the head, so warm light rises from N1, and a light runs
# along the great root from the tail's hollow back to the head. N2 and N3 are mist.
# ---------------------------------------------------------------------------

$Garden = @{
    SkyTop = '#100a24'; SkyMid = '#281c4e'; SkyLow = '#5c4684'; Moon = '#ece4ff'
    Hills = '#382a60'; Mid = '#241a44'; Mist = '#aa96dc'; Veil = '#6e6096'; Depth = '#140e28'; Dark = '#080612'
    StoneLight = '#c8c2da'; Stone = '#847ca0'; StoneDark = '#544c72'; StoneShade = '#383054'
    MossLight = '#96e6be'; Moss = '#50aa8c'; MossDark = '#286460'
    CapTop = '#b0f0e8'; Cap = '#5cacc4'; CapDark = '#32628c'; Stalk = '#ccc6e0'
    BarkLight = '#9c7c88'; Bark = '#664e62'; BarkDark = '#3e2c44'
    Spring = '#78ffe6'; Blossom = '#ff96d2'; Petal = '#ffd6ec'; Wisteria = '#b082f0'
}

# A stone ring (the moon gate): lit on the upper right, shaded on the lower left, with joints.
function Paint-Ring($layer, [double]$cx, [double]$cy, [double]$r0, [double]$r1) {
    $g = $Garden
    for ($y = (I ($cy - $r1 - 1)); $y -le $cy + $r1 + 1; $y++) {
        for ($x = (I ($cx - $r1 - 1)); $x -le $cx + $r1 + 1; $x++) {
            $dx = $x + 0.5 - $cx
            $dy = $y + 0.5 - $cy
            $d = [Math]::Sqrt($dx * $dx + $dy * $dy)
            if ($d -lt $r0 -or $d -ge $r1) { continue }
            $c = $g.Stone
            $side = ($dx - $dy) / $d # +1 toward the upper right, -1 toward the lower left
            if ($d -ge $r1 - 1.3 -and $side -gt 0.3) { $c = $g.StoneLight }
            elseif ($d -ge $r1 - 1.3 -and $side -lt -0.3) { $c = $g.StoneDark }
            elseif ($d -lt $r0 + 1.3 -and $side -gt 0.2) { $c = $g.StoneShade }
            $a = [Math]::Atan2($dy, $dx) * 180 / [Math]::PI + 360
            if (($a % 30) -lt 3.2) { $c = $g.StoneShade }
            Dot $layer $x $y $c
        }
    }
}

# Rolling hills, then dark round shrubs in front of them.
function Paint-Hills($layer, [int]$baseY, [int]$seed) {
    for ($x = 0; $x -lt $layer.W; $x++) {
        $top = I ($baseY - 9 * [Math]::Sin($x * 0.026 + 1.2) - 5 * [Math]::Sin($x * 0.071 + $seed))
        Fill $layer $x $top 1 ($layer.H - $top) $Garden.Hills
    }
    for ($i = 0; $i -lt 9; $i++) {
        $bx = (Hash01 ($seed + $i * 13)) * $layer.W
        $r = 6 + (Hash01 ($seed + $i * 7)) * 8
        Disc $layer $bx ($baseY + 10 + (Hash01 ($seed + $i * 3)) * 8) $r $Garden.Mid
    }
}

# Garden platforms (GardenShape in the game): Terrace, MoonCap, Planter, RootKnot.
function Paint-GardenLedge($layer, [string]$shape, [int]$x0, [int]$w, [int]$top, [int]$seed) {
    $g = $Garden
    $x1 = $x0 + $w
    $cx = $x0 + (I ($w / 2))
    for ($dy = 0; $top + $dy -lt $layer.H; $dy++) {
        $j = I ((Hash01 ($seed * 31 + $dy)) * 2)
        $l = $x0; $r = $x1
        $m = @($g.Stone, $g.StoneDark, $g.StoneLight)
        if ($shape -eq 'Terrace') {
            if ($dy -eq 0) { $m = @($g.MossLight, $g.Moss, $g.MossLight) }
            elseif ($dy -lt 3) { $m = @($g.Moss, $g.MossDark, $g.MossLight) }
            elseif ($dy -ge 4) { $l = $x0 + 1 - (I ([Math]::Max(0, $dy - 24) / 5)); $r = $x1 - 1 + (I ([Math]::Max(0, $dy - 24) / 5)) }
        } elseif ($shape -eq 'MoonCap') {
            if ($dy -eq 0) { $m = @($g.CapTop, $g.Cap, $g.CapTop) }
            elseif ($dy -lt 3) { $l = $x0 - 1; $r = $x1 + 1; $m = @($g.Cap, $g.CapDark, $g.CapTop) }
            elseif ($dy -lt 5) { $m = @($g.Cap, $g.CapDark, $g.Cap) }
            elseif ($dy -lt 6) { $l = $x0 + 2; $r = $x1 - 2; $m = @($g.CapDark, $g.CapDark, $g.Cap) }
            else {
                $half = 4 + (I ([Math]::Max(0, $dy - 22) / 5))
                $l = $cx - $half; $r = $cx + $half
                $m = @($g.Stalk, (Mix $g.Stalk $g.StoneDark 0.5), $g.Stalk)
            }
        } elseif ($shape -eq 'Planter') {
            if ($dy -eq 0) { $m = @($g.StoneLight, $g.Stone, $g.StoneLight) }
            elseif ($dy -lt 10) { $l = $x0 + 1; $r = $x1 - 1 }
            else { $l = $x0 + 8 + $j; $r = $x1 - 8 - $j }
        } else { # RootKnot
            if ($dy -eq 0) { $m = @($g.MossLight, $g.Moss, $g.MossLight) }
            elseif ($dy -lt 3) { $m = @($g.Moss, $g.MossDark, $g.MossLight) }
            else {
                $bulge = [int][Math]::Round(2 * [Math]::Sin([Math]::Min($dy, 20) / 20.0 * [Math]::PI))
                $l = $x0 - $bulge + $j; $r = $x1 + $bulge - $j
                if ($dy -ge 20) { $l = $x0 + 3 - (I (($dy - 20) / 3)); $r = $x1 - 1 + (I (($dy - 20) / 4)) }
                $m = @($g.Bark, $g.BarkDark, $g.BarkLight)
            }
        }
        $t = [Math]::Min(0.6, $dy / 40.0)
        $y = $top + $dy
        Fill $layer $l $y ($r - $l) 1 (Mix $m[0] $g.Depth $t)
        Fill $layer $l $y 2 1 (Mix $m[1] $g.Depth $t)
        Fill $layer ($r - 1) $y 1 1 (Mix $m[2] $g.Depth $t)
        if ($shape -ne 'MoonCap' -and $shape -ne 'RootKnot' -and $dy -gt 4 -and $dy % 6 -eq 5) { Fill $layer ($l + 2) $y ($r - $l - 3) 1 (Mix $g.StoneShade $g.Depth $t) }
        if ($shape -eq 'RootKnot' -and $dy -gt 3 -and $dy % 3 -eq 0) { # bark grain
            for ($gx = $l + 3 + ($dy % 2); $gx -lt $r - 2; $gx += 5) { Dot $layer $gx $y (Mix $g.BarkDark $g.Depth $t) }
        }
    }
}

function Garden-Details($layer, [string]$shape, [int]$x0, [int]$w, [int]$top, [int]$seed) {
    $g = $Garden
    $x1 = $x0 + $w
    $moss = @($g.MossLight, $g.Moss, $g.MossDark)
    if ($shape -eq 'Terrace') { Paint-Moss $layer $x0 $w $top $seed $moss }
    if ($shape -eq 'MoonCap') {
        for ($i = 0; $i -lt 5; $i++) { Fill $layer ($x0 + 3 + (I ((Hash01 ($seed * 5 + $i)) * ($w - 8)))) ($top + 1 + ($i % 3)) 2 1 $g.CapTop }
        for ($gx = $x0 + 4; $gx -lt $x1 - 4; $gx += 3) { Dot $layer $gx ($top + 5) $g.Depth } # gills
    }
    if ($shape -eq 'Planter') {
        Fill $layer ($x0 + 3) ($top + 4) ($w - 6) 1 $g.StoneShade # carved band
        for ($x = $x0 + 6; $x -lt $x1 - 6; $x += 6) { Dot $layer $x ($top + 6) $g.StoneLight }
        for ($i = 0; $i -lt 7; $i++) { # flowers growing from the bed
            $fx = $x0 + 3 + (I ((Hash01 ($seed * 9 + $i)) * ($w - 6)))
            $sh = 1 + (I ((Hash01 ($seed * 11 + $i)) * 3))
            $head = @($g.Blossom, $g.Petal, $g.Wisteria)[$i % 3]
            Fill $layer $fx ($top - $sh) 1 $sh $g.MossDark
            Fill $layer ($fx - 1) ($top - $sh - 1) 3 1 $head
            Dot $layer $fx ($top - $sh - 2) $head
        }
    }
    if ($shape -eq 'RootKnot') {
        Paint-Moss $layer $x0 $w $top $seed $moss
        for ($dy = 6; $dy -lt 17; $dy++) { # the hollow in the tail's root: where tail->next leads
            $half = 4
            if ($dy -lt 11) { $half = [int][Math]::Round(4.5 * [Math]::Sqrt([Math]::Max(0, 1 - [Math]::Pow((11 - $dy) / 5.0, 2)))) }
            if ($half -lt 1) { continue }
            Fill $layer ($x0 + 8 - $half) ($top + $dy) (2 * $half) 1 $g.Dark
            Dot $layer ($x0 + 8 + $half) ($top + $dy) $g.BarkLight # the lit rim
            Dot $layer ($x0 + 7 - $half) ($top + $dy) $g.BarkDark
        }
        Fill $layer ($x0 + 3) ($top + 17) 11 1 $g.BarkLight # the worn sill
    }
}

# The great root from the tail's hollow to the head: a thick cubic curve with a lit top.
# $p = @(x0, y0, x1, y1, x2, y2, x3, y3).
function Paint-Root($layer, [double[]]$p) {
    $g = $Garden
    $pts = @()
    for ($k = 0; $k -le 200; $k++) {
        $t = $k / 200.0
        $u = 1 - $t
        $x = $u * $u * $u * $p[0] + 3 * $u * $u * $t * $p[2] + 3 * $u * $t * $t * $p[4] + $t * $t * $t * $p[6]
        $y = $u * $u * $u * $p[1] + 3 * $u * $u * $t * $p[3] + 3 * $u * $t * $t * $p[5] + $t * $t * $t * $p[7]
        $pts += , @($x, $y)
    }
    foreach ($q in $pts) { Disc $layer $q[0] $q[1] 3.2 $g.BarkDark }
    foreach ($q in $pts) { Disc $layer $q[0] ($q[1] - 0.5) 2.1 $g.Bark }
    foreach ($q in $pts) { Dot $layer (I $q[0]) (I ($q[1] - 2.2)) $g.BarkLight }
    for ($k = 12; $k -lt $pts.Count; $k += 23) { Fill $layer (I $pts[$k][0]) (I ($pts[$k][1] - 3)) 3 1 $g.Moss }
}

function Write-Garden {
    $g = $Garden
    Card-Begin 'Region 3, The Endless Garden: a moonlit garden where the ledges form a circular singly linked list. The traveler stands on the tail, N4; its next is the head, so warm light rises from N1 and a light runs along the great root from the tail back to the head.'
    Def-Traveler
    Def-Overlays
    Def-Wisp
    Def-Linear 'gSky' @("0 $($g.SkyTop) 1", "0.55 $($g.SkyMid) 1", "1 $($g.SkyLow) 1")
    Def-Glow 'gMoon' $g.Moon 0.3
    Def-Glow 'gSpring' $g.Spring 0.32
    Def-Glow 'gHollow' $WarmLight 0.5
    Def-Glow 'rootMote' $WarmLight 0.6
    Def-Linear 'gHaze' @("0 $($g.Mist) 0", "1 $($g.Mist) 0.28")
    Anim '.star' 'twinkle' 3.4 'ease-in-out'

    Add (Rect 0 0 $CardW $CardH 'url(#gSky)')
    $stars = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt 18; $i++) {
        $sx = I ((Hash01 (700 + $i * 7)) * $CardW); $sy = 3 + (I ((Hash01 (700 + $i * 11)) * 56))
        [void]$stars.Append("<rect class=`"star`" style=`"animation-delay:-$(N ($i * 0.47))s`" x=`"$sx`" y=`"$sy`" width=`"1`" height=`"1`" fill=`"$($g.Moon)`"/>")
    }
    Add $stars.ToString()
    Add (Glow 150 52 46 'gMoon')
    $back = New-Layer $CardW $CardH
    Disc $back 150 52 10 $g.Moon
    foreach ($c in @(@(146, 49), @(153, 55), @(155, 48))) { Dot $back $c[0] $c[1] (Mix $g.Moon $g.SkyMid 0.2) }
    Paint-Hills $back 94 5
    Paint-Ring $back 150 60 20 26 # the moon gate
    Fill $back 127 82 6 14 $g.Stone; Fill $back 167 82 6 14 $g.Stone
    Fill $back 127 82 1 14 $g.StoneDark; Fill $back 172 82 1 14 $g.StoneLight
    for ($i = 0; $i -lt 3; $i++) { Disc $back (204 + $i * 13) (36 - ($i % 2) * 6) (11 - $i) (Mix $g.Blossom $g.Mid 0.4) } # blossom tree
    Fill $back 219 40 4 95 $g.BarkDark
    for ($i = 0; $i -lt 26; $i++) { Dot $back (194 + (I ((Hash01 (40 + $i)) * 44))) (26 + (I ((Hash01 (80 + $i)) * 22))) @($g.Blossom, $g.Petal)[$i % 2] }
    Add (Layer-Svg $back)
    Add (Rect 0 64 $CardW 71 'url(#gHaze)')

    # Wisteria hanging from the blossom tree, swaying.
    AddCss '@keyframes sway { 0%, 100% { transform: rotate(-4deg) } 50% { transform: rotate(4deg) } }'
    for ($i = 0; $i -lt 4; $i++) {
        $wx = 198 + $i * 11; $wy = 40 + ($i % 2) * 3; $len = 8 + ($i % 3) * 3
        $wl = New-Layer $CardW $CardH
        for ($k = 0; $k -lt $len; $k++) { Fill $wl ($wx - [Math]::Max(0, 1 - (I ($k / 4)))) ($wy + $k) ([Math]::Max(1, 3 - (I ($k / 4)))) 1 @($g.Wisteria, (Mix $g.Wisteria $g.Mid 0.35))[$k % 2] }
        Anim ".wis$i" 'sway' (3.6 + $i * 0.5) 'ease-in-out'
        Add (Layer-Svg $wl " class=`"wis$i`" style=`"transform-origin: ${wx}px ${wy}px`"")
    }

    # The glowing spring below the garden, with motes rising from it.
    Add (Glow 138 121 44 'gSpring' ' class="pulse"')
    $sp = New-Layer $CardW $CardH
    foreach ($row in @(@(119, 128, 148), @(120, 122, 154), @(121, 118, 158), @(122, 118, 158), @(123, 120, 156), @(124, 124, 152))) { Fill $sp $row[1] $row[0] ($row[2] - $row[1]) 1 $g.Spring }
    Fill $sp 126 121 22 1 (Mix $g.Spring '#ffffff' 0.55)
    Add (Layer-Svg $sp)
    for ($i = 0; $i -lt 5; $i++) { Add "<rect class=`"mote`" style=`"animation-delay:-$(N ($i * 0.5))s`" x=`"$(120 + $i * 8)`" y=`"118`" width=`"1`" height=`"1`" fill=`"$($g.Spring)`"/>" }

    # N2 and N3 sit behind the root and are mist: from the tail, only N4 and its next (N1) are solid.
    $ledges = @(
        @('Terrace', 6, 44, 98, 31), @('MoonCap', 66, 38, 88, 32), @('Planter', 120, 42, 96, 33), @('RootKnot', 182, 44, 86, 34)
    )
    $layers = @()
    foreach ($l in $ledges) {
        $layer = New-Layer $CardW $CardH
        Paint-GardenLedge $layer $l[0] $l[1] $l[2] $l[3] $l[4]
        Garden-Details $layer $l[0] $l[1] $l[2] $l[3] $l[4]
        $layers += $layer
    }
    foreach ($k in @(1, 2)) {
        Add (Layer-Svg $layers[$k])
        Add (Mask-Svg $layers[$k] $g.Veil ' fill-opacity=".62"')
    }
    $rootPath = @(188, 102, 158, 116, 80, 116, 40, 104)
    $root = New-Layer $CardW $CardH
    Paint-Root $root $rootPath
    Add (Layer-Svg $root)
    Add (Layer-Svg $layers[0])
    Add (Layer-Svg $layers[3])
    Add (Glow 190 97 9 'gHollow' ' class="pulse"')
    Add (LinkGlow-Svg 6 44 98 'next') # tail->next is the head

    # A light runs along the great root, from the tail's hollow to the head.
    $d = "M$($rootPath[0]) $($rootPath[1]) C$($rootPath[2]) $($rootPath[3]) $($rootPath[4]) $($rootPath[5]) $($rootPath[6]) $($rootPath[7])"
    Add ("<g class=`"smil`" opacity=`"0`">" + (Glow 0 0 7 'rootMote') + (Rect -1 -1 2 2 '#fff4c8') +
        "<animateMotion dur=`"3.6s`" repeatCount=`"indefinite`" path=`"$d`" keyPoints=`"0;1;1`" keyTimes=`"0;0.8;1`" calcMode=`"linear`"/>" +
        '<animate attributeName="opacity" values="0;1;1;0;0" keyTimes="0;0.08;0.72;0.8;1" dur="3.6s" repeatCount="indefinite"/></g>')

    Add (Traveler-Idle 208 86 -Mirror)
    AddCss '@keyframes toHollow { 0%, 100% { transform: translate(0px,0px) } 50% { transform: translate(-14px,12px) } }'
    Anim '.toHollow' 'toHollow' 4 'ease-in-out'
    Add (Wisp-Svg 194 64 'toHollow')
    Add (Falling-Svg @(206, 44, 222, 40, 232, 48, 196, 50) @($g.Petal, $g.Blossom) 'petal')
    Vignette $CardW $CardH
    Card-Label 3 'The Endless Garden'
    Add (Card-Code 3 'tail->next == head')
    Card-End
}

# ---------------------------------------------------------------------------
# Region 4, The Edge of Nothing: NULL pointer safety. N3's next is NULL, so past its edge
# there is only an amber glow and snapped ropes. After the check, append() creates N4 and
# the bridge assembles plank by plank; the dawn comes up and the traveler crosses.
# ---------------------------------------------------------------------------

$Edge = @{
    SkyTop = '#0a0e1c'; SkyMid = '#24304a'; SkyLow = '#5c6a80'; Dawn = '#ffba82'; Moon = '#dce4f0'
    FarPeaks = '#2c364c'; NearPeaks = '#1e2436'; Spire = '#161c2a'; SpireLit = '#343e56'
    Mist = '#a0acc4'; Depth = '#0e101c'; Dark = '#06080e'
    RockLight = '#b2b0b6'; Rock = '#626472'; RockDark = '#3e4050'; RockShade = '#262838'
    GrassLight = '#acc67a'; Grass = '#6c8c5a'; GrassDark = '#3c5646'
    WoodLight = '#b2865a'; Wood = '#7c583c'; WoodDark = '#4e362a'; Rope = '#c4aa78'; Pine = '#1c2a2e'
}

# A jagged ridgeline: the highest of several peaks at each column, with pale tips.
function Paint-Peaks($layer, [int]$baseY, [int]$count, [double]$minH, [double]$maxH, [int]$seed, [string]$color, [string]$tip) {
    $peaks = @()
    for ($i = 0; $i -lt $count; $i++) {
        $peaks += , @(((Hash01 ($seed + $i * 7)) * ($layer.W + 40) - 20), ($minH + (Hash01 ($seed + $i * 13)) * ($maxH - $minH)), (18 + (Hash01 ($seed + $i * 29)) * 26))
    }
    for ($x = 0; $x -lt $layer.W; $x++) {
        $best = 0.0
        foreach ($p in $peaks) {
            $hgt = $p[1] - [Math]::Abs($x - $p[0]) * $p[1] / $p[2]
            $hgt += ((Hash01 ($seed + $x * 3)) - 0.5) * 1.6 # rough rock
            if ($hgt -gt $best) { $best = $hgt }
        }
        $top = I ($baseY - $best)
        Fill $layer $x $top 1 ($layer.H - $top) $color
        if ($best -gt $maxH * 0.62) { Fill $layer $x $top 1 2 $tip }
    }
}

# Small pines standing on a ridge at the given x positions.
function Paint-Pines($layer, [double[]]$xs, [int]$baseY, [int]$seed) {
    for ($i = 0; $i -lt $xs.Length; $i++) {
        $h = 7 + (I ((Hash01 ($seed + $i * 5)) * 7))
        for ($r = 0; $r -lt $h; $r++) {
            $half = I (($r + 1) / 2.4)
            Fill $layer ([int]$xs[$i] - $half) ($baseY - $h + $r) (2 * $half + 1) 1 $Edge.Pine
        }
        Fill $layer ([int]$xs[$i]) $baseY 1 3 $Edge.Pine
    }
}

# A cliff of rock with a grassy top. The face toward the gap is jagged and lit or shaded
# by side: $face = 'right' for a cliff whose edge is on its right.
function Paint-Cliff($layer, [int]$x0, [int]$x1, [int]$top, [string]$face, [int]$seed) {
    $e = $Edge
    for ($dy = 0; $top + $dy -lt $layer.H; $dy++) {
        $jag = I ((Hash01 ($seed * 17 + (I ($dy / 2)))) * 3)
        $back = 0
        if ($dy -gt 5) { $back = 1 + (I (($dy - 5) / 5)) + $jag }
        $l = $x0; $r = $x1
        if ($face -eq 'right') { $r = $x1 - $back } else { $l = $x0 + $back }
        $m = @($e.Rock, $e.RockShade, $e.RockLight)
        if ($dy -eq 0) { $m = @($e.GrassLight, $e.Grass, $e.GrassLight) }
        elseif ($dy -lt 3) { $m = @($e.Grass, $e.GrassDark, $e.GrassLight) }
        elseif ($dy -lt 4) { $m = @($e.GrassDark, $e.GrassDark, $e.Grass) }
        elseif ((($dy + $jag) % 6) -eq 0) { $m = @($e.RockDark, $e.RockShade, $e.Rock) } # strata
        $t = [Math]::Min(0.65, $dy / 36.0)
        $y = $top + $dy
        Fill $layer $l $y ($r - $l) 1 (Mix $m[0] $e.Depth $t)
        Fill $layer $l $y 2 1 (Mix $m[1] $e.Depth $t)
        Fill $layer ($r - 1) $y 1 1 (Mix $m[2] $e.Depth $t)
    }
    for ($i = 0; $i -lt (I (($x1 - $x0) / 6)); $i++) { # grass blades
        $gx = $x0 + 1 + (I ((Hash01 ($seed * 83 + $i)) * ($x1 - $x0 - 2)))
        $h = 1 + (I ((Hash01 ($seed * 89 + $i)) * 3))
        Fill $layer $gx ($top - $h) 1 $h $e.Grass
        Dot $layer $gx ($top - $h) $e.GrassLight
    }
}

# A wooden post standing on a ledge top at x.
function Paint-Post($layer, [int]$x, [int]$top) {
    Fill $layer $x ($top - 12) 3 12 $Edge.Wood
    Fill $layer $x ($top - 12) 1 12 $Edge.WoodDark
    Fill $layer ($x + 2) ($top - 12) 1 12 $Edge.WoodLight
    Fill $layer ($x - 1) ($top - 13) 5 1 $Edge.WoodDark
}

# A rope from (x0, y0) to (x1, y1) that sags by $sag pixels in the middle.
function Paint-Rope($layer, [double]$x0, [double]$y0, [double]$x1, [double]$y1, [double]$sag, [string]$color) {
    $steps = [int][Math]::Max([Math]::Abs($x1 - $x0), [Math]::Abs($y1 - $y0)) + 1
    for ($k = 0; $k -le $steps; $k++) {
        $t = $k / $steps
        Dot $layer (I ($x0 + ($x1 - $x0) * $t)) (I ($y0 + ($y1 - $y0) * $t + $sag * 4 * $t * (1 - $t))) $color
    }
}

function Edge-Backdrop {
    $e = $Edge
    Def-Linear 'eSky' @("0 $($e.SkyTop) 1", "0.55 $($e.SkyMid) 1", "1 $($e.SkyLow) 1")
    Def-Linear 'eDawn' @("0 $($e.Dawn) 0", "0.45 $($e.Dawn) 0.2", "1 $($e.Dawn) 0.8")
    Def-Glow 'eSun' $e.Dawn 0.5
    Def-Linear 'eHaze' @("0 $($e.Mist) 0", "1 $($e.Mist) 0.3")
    Def-Linear 'eBand' @("0 $($e.Mist) 0", "0.5 $($e.Mist) 0.22", "1 $($e.Mist) 0")
    Def-Glow 'eMoon' $e.Moon 0.25
    AddCss '@keyframes drift { 0%, 100% { transform: translateX(0px) } 50% { transform: translateX(-18px) } }'
    Anim '.drift' 'drift' 16 'ease-in-out'
    Anim '.star' 'twinkle' 3.4 'ease-in-out'

    Add (Rect 0 0 $CardW $CardH 'url(#eSky)')
    Add ('<g class="dawn">' + (Rect 0 0 $CardW $CardH 'url(#eDawn)') + (Glow 196 96 110 'eSun') + '</g>')
    $stars = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt 14; $i++) {
        $sx = I ((Hash01 (900 + $i * 7)) * $CardW); $sy = 3 + (I ((Hash01 (900 + $i * 11)) * 44))
        [void]$stars.Append("<rect class=`"star`" style=`"animation-delay:-$(N ($i * 0.61))s`" x=`"$sx`" y=`"$sy`" width=`"1`" height=`"1`" fill=`"$($e.Moon)`"/>")
    }
    Add ("<g class=`"night`">" + $stars.ToString() + (Glow 206 26 24 'eMoon') + '</g>')
    $back = New-Layer $CardW $CardH
    Disc $back 206 26 5.5 $e.Moon
    Paint-Peaks $back 100 7 22 46 61 $e.FarPeaks (Mix $e.FarPeaks $e.Moon 0.35)
    Add (Layer-Svg $back)
    Add (Rect 0 50 $CardW 60 'url(#eHaze)')
    $near = New-Layer $CardW $CardH
    Paint-Peaks $near 112 8 14 30 67 $e.NearPeaks $e.SpireLit
    Paint-Pines $near @(30, 38, 184, 196, 214, 226) 100 5
    Add (Layer-Svg $near)
    Add "<g class=`"drift`">$(Rect -20 100 ($CardW + 60) 18 'url(#eBand)')</g>"
}

# The prompt the game shows above the traveler on N3: a key cap, then the action.
function Prompt-Svg([double]$cx, [double]$y, [string]$key, [string]$action, [string]$attrs) {
    $kw = (Text-Width $key) + 6
    $w = $kw + 4 + (Text-Width $action) + 10
    $x = [Math]::Round($cx - $w / 2)
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("<g$attrs>")
    [void]$sb.Append((Rect ($x + 1) $y ($w - 2) 15 $UiNavy ' fill-opacity=".92"'))
    [void]$sb.Append((Rect ($x + 1) $y ($w - 2) 1 $NullAmber) + (Rect ($x + 1) ($y + 14) ($w - 2) 1 $NullAmber) + (Rect $x ($y + 1) 1 13 $NullAmber) + (Rect ($x + $w - 1) ($y + 1) 1 13 $NullAmber))
    [void]$sb.Append((Rect ($x + 5) ($y + 3) $kw 9 $UiText) + (Text $key ($x + 8) ($y + 4) $UiNavy))
    [void]$sb.Append((Text $action ($x + 5 + $kw + 4) ($y + 4) $NullAmber))
    [void]$sb.Append('</g>')
    return $sb.ToString()
}

function Write-Edge {
    $e = $Edge
    $loop = 8.0
    Card-Begin 'Region 4, The Edge of Nothing: N3 next is NULL, so past the cliff edge there is only an amber glow and snapped ropes. After the check, append() creates N4, the bridge assembles plank by plank, dawn breaks, and the traveler crosses.'
    Def-Traveler
    Def-Overlays
    Def-Glow 'eHazard' $Hazard 0.45
    Def-Linear 'eAbyss' @("0 $Hazard 0", "1 $Hazard 0.34")
    Def-Glow 'eGate' $e.Dawn 0.5
    Window-Keyframes 'dawn' $loop @(4.8, 7.3) 0.8
    Anim '.dawn' 'dawn' $loop
    Window-Keyframes 'night' $loop @(0, 4.8, 7.6, 8) 0.8
    Anim '.night' 'night' $loop
    Window-Keyframes 'hazard' $loop @(0, 4.6, 7.6, 8) 0.5
    Anim '.hazard' 'hazard' $loop
    Window-Keyframes 'snapped' $loop @(0, 4.4, 7.7, 8) 0.25
    Anim '.snapped' 'snapped' $loop
    Window-Keyframes 'rails' $loop @(4.4, 7.6) 0.3
    Anim '.rails' 'rails' $loop
    Switch-Keyframes 'prompt' $loop @(0.3, 2.4)
    Anim '.prompt' 'prompt' $loop 'steps(1,end)'
    Switch-Keyframes 'code1' $loop @(0, 2.4, 7.6, 8)
    Anim '.code1' 'code1' $loop 'steps(1,end)'
    Switch-Keyframes 'code2' $loop @(2.4, 7.6)
    Anim '.code2' 'code2' $loop 'steps(1,end)'
    Keyframes 'pier' $loop @(0, 'transform:translateY(50px);opacity:1', 4.3, 'transform:translateY(50px);opacity:1', 4.9, 'transform:translateY(0px);opacity:1',
        7.6, 'transform:translateY(0px);opacity:1', 7.9, 'transform:translateY(0px);opacity:0', $loop, 'transform:translateY(50px);opacity:0')
    Anim '.pier' 'pier' $loop
    AddCss '.dawn, .rails, .code2, .plank { opacity: 0; } .pier { transform: translateY(50px); }'
    Edge-Backdrop

    # The abyss under the gap glows amber while N3->next is NULL; embers drift up out of it.
    $hz = New-Object System.Text.StringBuilder
    [void]$hz.Append('<g class="hazard">' + (Rect 96 88 64 47 'url(#eAbyss)') + (Glow 128 122 48 'eHazard'))
    for ($i = 0; $i -lt 6; $i++) {
        $c = $Hazard; if ($i % 2 -eq 1) { $c = $Ruins.FireHot }
        [void]$hz.Append("<rect class=`"ember`" style=`"animation-delay:-$(N ($i * 0.45))s`" x=`"$(102 + $i * 10)`" y=`"128`" width=`"1`" height=`"1`" fill=`"$c`"/>")
    }
    [void]$hz.Append('</g>')
    AddCss '@keyframes emberRise { 0% { transform: translate(0px,0px); opacity: 0 } 15% { opacity: 1 } 100% { transform: translate(3px,-40px); opacity: 0 } }'
    Anim '.ember' 'emberRise' 2.7
    Add $hz.ToString()

    $ground = New-Layer $CardW $CardH
    Paint-Cliff $ground -4 96 88 'right' 3 # N3, the Edge
    Paint-Cliff $ground 160 244 88 'left' 9 # the far side, with the dawn gate
    Paint-Post $ground 90 88
    Paint-Post $ground 162 88
    foreach ($gx in @(198, 222)) { # the dawn gate
        Fill $ground $gx 62 5 26 $e.Rock
        Fill $ground $gx 62 1 26 $e.RockShade
        Fill $ground ($gx + 4) 62 1 26 $e.RockLight
    }
    Fill $ground 195 58 35 4 $e.Rock
    Fill $ground 195 58 35 1 $e.RockLight
    Fill $ground 195 61 35 1 $e.RockShade
    Add (Layer-Svg $ground)
    Add (Glow 212 76 20 'eGate' ' class="dawn"')

    $snapped = New-Layer $CardW $CardH # ropes that used to hold a bridge
    Paint-Rope $snapped 92 77 99 99 3 $e.Rope
    Paint-Rope $snapped 163 77 155 96 -3 $e.Rope
    Dot $snapped 100 100 $e.Rope; Dot $snapped 98 101 $e.Rope; Dot $snapped 154 97 $e.Rope
    Add (Layer-Svg $snapped ' class="snapped"')

    # append() creates N4: the planks arrive one by one, then the rails, then the piers rise.
    $pier = New-Layer $CardW $CardH
    foreach ($px in @(117, 140)) {
        Fill $pier $px 91 3 44 $e.Wood
        Fill $pier $px 91 1 44 $e.WoodDark
        Fill $pier ($px + 2) 91 1 44 $e.WoodLight
    }
    Fill $pier 117 100 26 2 $e.WoodDark
    Add (Layer-Svg $pier ' class="pier"')
    for ($k = 0; $k -lt 8; $k++) {
        $x = 96 + $k * 8
        $tk = 2.6 + $k * 0.22
        Keyframes "plk$k" $loop @(0, 'opacity:0;transform:translateY(-5px)', $tk, 'opacity:0;transform:translateY(-5px)', ($tk + 0.2), 'opacity:1;transform:translateY(0px)',
            7.6, 'opacity:1;transform:translateY(0px)', 7.9, 'opacity:0;transform:translateY(0px)', $loop, 'opacity:0;transform:translateY(-5px)')
        Anim ".plk$k" "plk$k" $loop
        Add ("<g class=`"plank plk$k`">" + (Rect $x 88 7 1 $e.WoodLight) + (Rect $x 89 7 1 $e.Wood) + (Rect $x 90 7 1 $e.WoodDark) + (Rect ($x + 1) 89 1 1 $e.WoodDark) + '</g>')
    }
    $rails = New-Layer $CardW $CardH
    Paint-Rope $rails 92 77 163 77 6 $e.Rope
    for ($k = 0; $k -lt 8; $k += 2) {
        $hx = 99 + $k * 8
        $t = ($hx - 92) / 71.0
        $ry = I (77 + 6 * 4 * $t * (1 - $t))
        Fill $rails $hx ($ry + 1) 1 (87 - $ry) $e.Rope
    }
    Add (Layer-Svg $rails ' class="rails"')

    Add (Puff-Svg 1 96 87 2.4 $loop $WarmLight @($WarmLight, '#ffffff', $e.Dawn)) # sparks as append() runs
    $segs = @(@('idle', 0, 5.0, 76, 88, 76, 88), @('walk', 5.0, 7.0, 76, 88, 178, 88), @('idle', 7.0, $loop, 178, 88, 178, 88))
    Add (Traveler-Route 'tv4' $segs $loop @(0, 0.25, 7.2, 7.6))
    Add (Prompt-Svg 80 49 'E' 'check & repair N3->next' ' class="prompt"')

    Vignette $CardW $CardH
    Card-Label 4 'The Edge of Nothing'
    $t1 = 'N3->next == nullptr'
    $w1 = (Text-Width $t1) + 28
    $stop = New-Layer $CardW $CardH
    Icon $stop (13 + (Text-Width $t1) + 4) ($CardH - 15) '.###. ##### ##### ##### .###.' $NullAmber
    Fill $stop (13 + (Text-Width $t1) + 5) ($CardH - 13) 3 1 $UiNavy
    Add ('<g class="code1">' + (Hud-Card 5 ($CardH - 20) $w1 15 $Accents[3]) + (Text $t1 13 ($CardH - 16) $NullAmber) + (Layer-Svg $stop) + '</g>')
    Add (Card-Code 4 'append(): N3->next = N4' $CodeBlue ' class="code2"')
    Card-End
}
