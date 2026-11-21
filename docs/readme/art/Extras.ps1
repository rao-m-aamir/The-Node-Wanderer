# Small pieces for the README: the badge row, the vine divider, and the footer strip.

# A pill with a one-pixel border and cut corners.
function Pill($layer, [int]$x, [int]$y, [int]$w, [int]$h, [string]$border, [string]$fill) {
    Fill $layer ($x + 1) $y ($w - 2) $h $border
    Fill $layer $x ($y + 1) $w ($h - 2) $border
    Fill $layer ($x + 1) ($y + 1) ($w - 2) ($h - 2) $fill
}

# A small icon from rows of '#', in the same format as the font.
function Icon($layer, [int]$x, [int]$y, [string]$rows, [string]$color) {
    $r = $rows.Split(' ')
    for ($j = 0; $j -lt $r.Length; $j++) {
        for ($i = 0; $i -lt $r[$j].Length; $i++) {
            if ($r[$j][$i] -eq '#') { Dot $layer ($x + $i) ($y + $j) $color }
        }
    }
}

function Write-Badges {
    $badges = @(
        @('C++17', $Accents[1], '..#.. .###. ##### .###. ..#..'),
        @('RAYLIB 5.5', $Accents[0], '##### #...# #.#.# #...# #####'),
        @('WINDOWS + MSVC', $Accents[2], '##.## ##.## ..... ##.## ##.##'),
        @('ART DRAWN IN CODE', $Accents[3], '..#.. ..#.. ##.## ..#.. ..#..')
    )
    $gap = 5
    $h = 15
    $widths = @($badges | ForEach-Object { 13 + (Text-Width $_[0]) + 6 })
    $total = ($widths | Measure-Object -Sum).Sum + $gap * ($badges.Count - 1)
    Begin-Svg ($total + 2) ($h + 2) 2 'Badges: C++17, Raylib 5.5, Windows and MSVC, art drawn in code'
    $layer = New-Layer ($total + 2) ($h + 2)
    $loop = 7.0
    $x = 1
    for ($k = 0; $k -lt $badges.Count; $k++) {
        $b = $badges[$k]
        $w = $widths[$k]
        Fill $layer ($x + 2) ($h + 1) ($w - 2) 1 '#000000'   # a hard shadow under each pill
        Pill $layer $x 1 $w $h $b[1] $UiNavy
        Fill $layer ($x + 2) 2 ($w - 4) 1 (Mix $UiNavy $b[1] 0.25) # a lit top edge
        Icon $layer ($x + 4) 6 $b[2] $b[1]
        $b += $x
        $badges[$k] = $b
        $x += $w + $gap
    }
    Add (Layer-Svg $layer)
    for ($k = 0; $k -lt $badges.Count; $k++) {
        $b = $badges[$k]
        $bx = $b[3]
        $w = $widths[$k]
        Add (Text $b[0] ($bx + 13) 5 $UiText)
        # A glint that sweeps across each badge in turn.
        AddDef "<clipPath id=`"bc$k`"><rect x=`"$($bx + 1)`" y=`"2`" width=`"$($w - 2)`" height=`"$($h - 2)`"/></clipPath>"
        $t0 = 0.6 + $k * 0.55
        Keyframes "glint$k" $loop @(0, "transform:translateX($($bx - 8)px)", $t0, "transform:translateX($($bx - 8)px)",
            ($t0 + 0.6), "transform:translateX($($bx + $w + 6)px)", $loop, "transform:translateX($($bx + $w + 6)px)")
        Anim ".glint$k" "glint$k" $loop
        AddCss ".glint$k { transform: translateX(-40px); }"
        Add "<g clip-path=`"url(#bc$k)`"><polygon class=`"glint$k`" points=`"3,1 6,1 2,16 -1,16`" fill=`"#ffffff`" fill-opacity=`".28`"/></g>"
    }
}

# A mossy vine with leaves and a gold node diamond in the middle; a firefly drifts along it.
# The background stays transparent, so it sits on GitHub's light and dark themes alike.
function Write-Divider {
    $W = 320; $H = 14
    Begin-Svg $W $H 3 'A vine divider with a firefly'
    $f = $Forest
    $layer = New-Layer $W $H
    $vy = { param($x) 7 + 2.2 * [Math]::Sin($x * [Math]::PI * 2 / 80) }
    $prev = [int][Math]::Round((& $vy 0))
    for ($x = 0; $x -lt $W; $x++) {
        $y = [int][Math]::Round((& $vy $x))
        $y0 = [Math]::Min($y, $prev); $y1 = [Math]::Max($y, $prev)
        Fill $layer $x $y0 1 ($y1 - $y0 + 1) $f.MossDark
        Dot $layer $x ($y0 - 1) $f.Moss # the lit top of the vine
        if ($x % 9 -eq 4) { # leaves, alternating above and below
            $side = -1; if ((I ($x / 9)) % 2 -eq 1) { $side = 1 }
            Fill $layer ($x - 1) ($y + 2 * $side) 3 1 $f.Moss
            Dot $layer ($x + $side) ($y + 2 * $side) $f.MossLight
            Dot $layer $x ($y + $side) $f.Moss
        }
        if ($x % 37 -eq 18) { Dot $layer $x ($y - 2) $WarmLight } # buds
        $prev = $y
    }
    Fill $layer 155 3 9 9 $UiNavy # the node diamond, outlined so it reads on light and dark pages
    Icon $layer 155 3 '....#.... ...###... ..#####.. .#######. ######### .#######. ..#####.. ...###... ....#....' $UiNavy
    for ($x = 155; $x -lt 164; $x++) { for ($y = 3; $y -lt 12; $y++) { if ([Math]::Abs($x - 159) + [Math]::Abs($y - 7) -gt 4) { $layer.Cells[$y * $W + $x] = -1 } } }
    Icon $layer 156 4 '...#... ..###.. .#####. ####### .#####. ..###.. ...#...' $CurrentGold
    AddDef '<linearGradient id="dvFade" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#000"/><stop offset="0.14" stop-color="#fff"/><stop offset="0.86" stop-color="#fff"/><stop offset="1" stop-color="#000"/></linearGradient>'
    AddDef "<mask id=`"dvMask`" maskUnits=`"userSpaceOnUse`" x=`"0`" y=`"0`" width=`"$W`" height=`"$H`"><rect width=`"$W`" height=`"$H`" fill=`"url(#dvFade)`"/></mask>"
    Add (Layer-Svg $layer ' mask="url(#dvMask)"')

    $d = New-Object System.Text.StringBuilder
    [void]$d.Append('M20 ' + (N ((& $vy 20) - 3)))
    for ($x = 24; $x -le 300; $x += 4) { [void]$d.Append(' L' + $x + ' ' + (N ((& $vy $x) - 3))) }
    Def-Glow 'dvGlow' $WarmLight 0.55
    Add ('<g class="smil" opacity="0">' + (Glow 0 0 5 'dvGlow') + (Rect -0.5 -0.5 1 1 '#fff4c8') +
        "<animateMotion dur=`"14s`" repeatCount=`"indefinite`" path=`"$($d.ToString())`" keyPoints=`"0;1;0`" keyTimes=`"0;0.5;1`" calcMode=`"linear`"/>" +
        '<animate attributeName="opacity" values="0;1;.4;1;.5;1;0" keyTimes="0;.06;.3;.5;.7;.94;1" dur="14s" repeatCount="indefinite"/></g>')
}

# A small night strip of forest floor: the traveler walks across under fireflies.
function Write-Footer {
    $W = 320; $H = 56; $loop = 16.0; $gy = 46 # the ground's top row
    $f = $Forest
    Begin-Svg $W $H 3 'Thanks for wandering. The traveler walks across a strip of forest floor at night.'
    Def-Traveler
    Def-Overlays
    Def-Wisp
    AddDef "<clipPath id=`"ftClip`"><path d=`"M2 0H$($W - 2)V1H$($W - 1)V2H${W}V$($H - 2)H$($W - 1)V$($H - 1)H$($W - 2)V${H}H2V$($H - 1)H1V$($H - 2)H0V2H1V1H2Z`"/></clipPath>"
    Def-Linear 'ftSky' @("0 $UiNavy 1", "1 $($f.SkyTop) 1")
    Add '<g clip-path="url(#ftClip)">'
    Add (Rect 0 0 $W $H 'url(#ftSky)')

    $back = New-Layer $W $H
    Paint-Treeline $back 11 ($gy + 6) 6 11 505 (Mix $f.MidTrees $UiNavy 0.45)
    Add (Layer-Svg $back)
    $ground = New-Layer $W $H
    for ($x = 0; $x -lt $W; $x++) {
        $bump = I ((Hash01 (2000 + (I ($x / 6)))) * 2)
        Fill $ground $x $gy 1 1 $f.MossLight
        Fill $ground $x ($gy + 1) 1 2 $f.Moss
        Fill $ground $x ($gy + 3) 1 1 $f.MossDark
        Fill $ground $x ($gy + 4 + $bump) 1 ($H - $gy - 4) (Mix $f.Earth $f.Depth 0.3)
    }
    Paint-Tufts $ground 0 $W $gy 40 2100
    for ($i = 0; $i -lt 12; $i++) { # pebbles
        $px = 6 + (I ((Hash01 (2200 + $i * 7)) * ($W - 12)))
        Fill $ground $px ($gy + 6 + $i % 3) 2 1 (Mix $f.Stone $f.Depth 0.3)
    }
    foreach ($mx in @(42, 131, 262)) { Fill $ground $mx ($gy - 3) 3 1 $f.Mushroom; Fill $ground ($mx + 1) ($gy - 2) 1 2 $f.Rings }
    Add (Layer-Svg $ground)
    Add (Fireflies-Svg 9 1200 0 $W 20 40)

    $segs = @(, @('walk', 0, $loop, -14, $gy, 334, $gy))
    $wisp = '<g class="bob">' + (Glow 15 -21 7 'wispGlow') + (Rect 14 -22 2 2 '#fff4c8') + '</g>'
    Add (Traveler-Route 'ft' $segs $loop @(0, 0.05, 15.9, $loop) -extra $wisp -still @(62, $gy))

    $tw = Def-Text 'ftText' 'Thanks for wandering.'
    $tx = [Math]::Round(($W - $tw) / 2)
    Add (Outlined 'ftText' $tx 8 $Parchment $UiNavy)
    $orn = New-Layer $W $H
    Icon $orn ($tx - 12) 9 '..#.. .###. ##### .###. ..#..' $CurrentGold
    Icon $orn ($tx + $tw + 7) 9 '..#.. .###. ##### .###. ..#..' $CurrentGold
    Add (Layer-Svg $orn ' class="pulse"')
    Add '</g>'
}
