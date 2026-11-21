# The hero banner and the forest pieces it shares with the Region 1 card.
# Colors and shapes follow src/ForestArt.cpp: teal shadows, warm light from behind the trees.

$Forest = @{
    SkyTop = '#0e2232'; SkyMid = '#224e56'; SkyLow = '#688e74'; Sun = '#ffe4a8'
    FarTrees = '#3c6a66'; MidTrees = '#204240'; Rim = '#567a5c'; Canopy = '#102422'; Vine = '#1e3e30'
    Mist = '#b2d0bc'; Veil = '#749c96'; Depth = '#12262c'; Dark = '#081012'
    MossLight = '#b8e270'; Moss = '#70ac4e'; MossDark = '#3a703e'
    EarthLight = '#926c4e'; Earth = '#6c4e3c'; EarthDark = '#48342e'
    StoneLight = '#a8ac9e'; Stone = '#707874'; StoneDark = '#424a50'
    BarkLight = '#9e724a'; Bark = '#6c4c36'; BarkDark = '#402c26'; Rings = '#ceaa76'; Mushroom = '#e28054'
}

# Height of a treeline at x: overlapping round crowns and a few pointed firs (treelineTop in the game).
function Treeline-Top([double]$x, [double]$spacing, [double]$baseY, [double]$minR, [double]$maxR, [int]$seed) {
    $index = [Math]::Floor($x / $spacing)
    $best = $baseY
    for ($i = $index - 2; $i -le $index + 2; $i++) {
        $center = $i * $spacing + (Hash01 ($seed + $i * 7)) * $spacing * 0.6
        $radius = $minR + (Hash01 ($seed + $i * 13)) * ($maxR - $minR)
        $ground = $baseY - (Hash01 ($seed + $i * 29)) * $radius * 0.6
        $d = [Math]::Abs($x - $center)
        if ($d -ge $radius) { continue }
        if ((Hash01 ($seed + $i * 41)) -gt 0.62) { $top = $ground - $radius * 1.7 + $d * 1.7 }
        else { $top = $ground - [Math]::Sqrt($radius * $radius - $d * $d) }
        if ($top -lt $best) { $best = $top }
    }
    return $best
}

function Paint-Treeline($layer, [double]$spacing, [double]$baseY, [double]$minR, [double]$maxR, [int]$seed, [string]$color) {
    for ($x = 0; $x -lt $layer.W; $x++) {
        $top = I (Treeline-Top $x $spacing $baseY $minR $maxR $seed)
        Fill $layer $x $top 1 ($layer.H - $top) $color
    }
}

# Midground trees: dark trunks with clustered crowns, each crown lit on its upper right.
function Paint-MidTrees($layer, [double[]]$xs, [int]$seed, [double]$crownTop, [double]$crownRange, [double]$scale) {
    $lit = Mix $Forest.MidTrees $Forest.FarTrees 0.45
    $rim = Mix $Forest.MidTrees $Forest.Rim 0.35
    $dxs = @(0.0, -0.8, 0.8, -0.4, 0.45); $dys = @(0.0, 0.3, 0.25, -0.45, -0.4); $sizes = @(1.0, 0.7, 0.75, 0.6, 0.62)
    for ($i = 0; $i -lt $xs.Length; $i++) {
        $x = $xs[$i]
        $trunk = 3 + (I ((Hash01 ($seed + $i * 23)) * 4))
        $crownY = $crownTop + (Hash01 ($seed + $i * 31)) * $crownRange
        $radius = (11 + (Hash01 ($seed + $i * 37)) * 7) * $scale
        Fill $layer (I ($x - $trunk / 2)) (I $crownY) $trunk ($layer.H) $Forest.MidTrees
        Fill $layer (I ($x + $trunk / 2 - 1)) (I ($crownY + 6)) 1 ($layer.H) $rim
        for ($c = 0; $c -lt 5; $c++) { Disc $layer ($x + $dxs[$c] * $radius + 1) ($crownY + $dys[$c] * $radius - 1) ($radius * $sizes[$c]) $lit }
        for ($c = 0; $c -lt 5; $c++) { Disc $layer ($x + $dxs[$c] * $radius) ($crownY + $dys[$c] * $radius) ($radius * $sizes[$c]) $Forest.MidTrees }
    }
}

# Dark leaf masses along the top edge between x = from and x = to.
function Paint-Canopy($layer, [double]$from, [double]$to, [int]$seed) {
    $i = 0
    for ($x = $from; $x -le $to; $x += 13) {
        $cx = $x + (Hash01 ($seed + $i * 13)) * 6
        $cy = -4 + (Hash01 ($seed + $i * 19)) * 7
        $r = 7 + (Hash01 ($seed + $i * 29)) * 6
        Disc $layer $cx $cy $r $Forest.Canopy
        Fill $layer (I ($cx - $r * 0.4)) (I ($cy + $r - 1)) 2 1 $Forest.Vine # leaf highlights
        Fill $layer (I ($cx + $r * 0.2)) (I ($cy + $r - 1)) 1 1 $Forest.Vine
        $i++
    }
}

# A vine hanging from (x, y) that sways from its top. Returns markup.
function Vine-Svg([int]$x, [int]$y, [int]$length, [int]$seed, [string]$class) {
    $layer = New-Layer ($x + 4) ($y + $length + 1)
    for ($k = 0; $k -lt $length; $k++) {
        Dot $layer $x ($y + $k) $Forest.Vine
        if ($k % 5 -eq 2) {
            $side = 1; if ((I ($k / 5)) % 2 -eq 1) { $side = -2 }
            $leaf = $Forest.MossDark; if ((Hash01 ($seed + $k)) -gt 0.6) { $leaf = $Forest.Moss }
            Fill $layer ($x + $side) ($y + $k) 2 1 $leaf
        }
    }
    return Layer-Svg $layer " class=`"$class`" style=`"transform-origin: ${x}px ${y}px`""
}

# Outline of a forest platform $dy rows below its top: @(left, right). The top row is
# exactly the node's collision width; lips, bulges, and supports only hang below it.
function Forest-Span([string]$shape, [int]$x0, [int]$x1, [int]$dy, [int]$seed) {
    $j = I ((Hash01 ($seed * 31 + $dy)) * 2) # 0 or 1: a little unevenness
    switch ($shape) {
        'Cliff' {
            if ($dy -eq 0) { return @($x0, $x1) }
            if ($dy -lt 4) { return @(($x0 - 1), ($x1 + 1)) } # the moss lip
            $grow = I ($dy / 7)
            return @(($x0 + 1 + $j - $grow), ($x1 - 1 - $j + $grow))
        }
        'Boulder' {
            if ($dy -eq 0) { return @($x0, $x1) }
            if ($dy -lt 3) { return @(($x0 - 1), ($x1 + 1)) }
            if ($dy -lt 14) { return @(($x0 - 2 + $j), ($x1 + 2 - $j)) }
            if ($dy -lt 20) { $in = ($dy - 13) * 2; return @(($x0 + $in), ($x1 - $in)) }
            return @(($x0 + 12 + $j), ($x1 - 12 - $j)) # the rock it rests on
        }
        'Log' {
            if ($dy -lt 16) { return @($x0, $x1) }
            $in = [Math]::Max(3, 8 - (I (($dy - 16) / 3)))
            return @(($x0 + $in + $j), ($x1 - $in - $j))
        }
        'Ruin' {
            if ($dy -eq 1) { return @(($x0 - 2), ($x1 + 2)) } # cornice
            if ($dy -lt 8) { return @($x0, $x1) }
            return @(($x0 + 2), ($x1 - 2))
        }
    }
}

# @(base, shade, lit) for a row of a forest platform (materialForRow in the game).
function Forest-Material([string]$shape, [int]$dy) {
    $f = $Forest
    switch ($shape) {
        'Cliff' {
            if ($dy -eq 0) { return @($f.MossLight, $f.Moss, $f.MossLight) }
            if ($dy -lt 3) { return @($f.Moss, $f.MossDark, $f.MossLight) }
            if ($dy -lt 4) { return @($f.MossDark, $f.MossDark, $f.Moss) }
            $band = I (($dy + 4 * [Math]::Sin($dy * 0.05)) / 6) # soil layers of uneven thickness
            $base = $f.Earth
            if ($band % 3 -eq 1) { $base = Mix $f.Earth $f.EarthDark 0.55 }
            return @($base, $f.EarthDark, $f.EarthLight)
        }
        'Boulder' {
            if ($dy -eq 0) { return @($f.MossLight, $f.Moss, $f.MossLight) }
            if ($dy -lt 3) { return @($f.Moss, $f.MossDark, $f.MossLight) }
            if ($dy -lt 5) { return @($f.MossDark, (Mix $f.MossDark $f.StoneDark 0.5), $f.Moss) }
            if ($dy -lt 18) { return @($f.Stone, $f.StoneDark, $f.StoneLight) }
            return @((Mix $f.Stone $f.StoneDark 0.6), (Mix $f.StoneDark $f.Dark 0.4), $f.Stone)
        }
        'Log' {
            if ($dy -eq 0) { return @($f.MossLight, $f.Moss, $f.MossLight) }
            if ($dy -lt 2) { return @($f.Moss, $f.MossDark, $f.MossLight) }
            if ($dy -lt 5) { return @($f.BarkLight, $f.Bark, $f.BarkLight) }
            if ($dy -lt 12) { return @($f.Bark, $f.BarkDark, $f.BarkLight) }
            if ($dy -lt 16) { return @($f.BarkDark, $f.BarkDark, $f.Bark) }
            if ($dy -lt 22) { return @($f.Stone, $f.StoneDark, $f.StoneLight) } # the rock under the log
            return @($f.StoneDark, $f.StoneDark, $f.Stone)
        }
        'Ruin' {
            if ($dy -eq 0) { return @($f.StoneLight, $f.Stone, $f.StoneLight) }
            if ($dy -lt 4) { return @($f.Stone, $f.StoneDark, $f.StoneLight) }
            if ($dy -lt 8) { return @($f.StoneDark, $f.StoneDark, $f.Stone) }
            return @($f.Stone, $f.StoneDark, $f.StoneLight)
        }
    }
}

# Platforms darken toward the forest floor.
function Deep([string]$color, [int]$dy) { return Mix $color $Forest.Depth ([Math]::Min(0.6, $dy / 40.0)) }

# Fills a platform row by row: a dark left edge and a lit right edge, as in fillForestLedge.
function Fill-ForestShape($layer, [string]$shape, [int]$x0, [int]$x1, [int]$top, [int]$seed) {
    for ($dy = 0; $top + $dy -lt $layer.H; $dy++) {
        $s = Forest-Span $shape $x0 $x1 $dy $seed
        $m = Forest-Material $shape $dy
        $y = $top + $dy
        Fill $layer $s[0] $y ($s[1] - $s[0]) 1 (Deep $m[0] $dy)
        Fill $layer $s[0] $y 2 1 (Deep $m[1] $dy)
        Fill $layer ($s[1] - 1) $y 1 1 (Deep $m[2] $dy)
    }
}

# Moss strands hanging over the edge, starting $fromDy rows below the top.
function Paint-Drips($layer, [int]$x0, [int]$w, [int]$top, [int]$fromDy, [int]$count, [int]$seed) {
    for ($i = 0; $i -lt $count; $i++) {
        $x = $x0 + 2 + (I ((Hash01 ($seed * 71 + $i)) * ($w - 4)))
        $len = 2 + (I ((Hash01 ($seed * 13 + $i)) * 5))
        Fill $layer $x ($top + $fromDy) 1 $len $Forest.MossDark
        Dot $layer $x ($top + $fromDy) $Forest.Moss
    }
}

# Grass blades standing on the top edge.
function Paint-Tufts($layer, [int]$x0, [int]$w, [int]$top, [int]$count, [int]$seed) {
    for ($i = 0; $i -lt $count; $i++) {
        $x = $x0 + 1 + (I ((Hash01 ($seed * 83 + $i)) * ($w - 2)))
        $h = 1 + (I ((Hash01 ($seed * 89 + $i)) * 3))
        Fill $layer $x ($top - $h) 1 $h $Forest.Moss
        Dot $layer $x ($top - $h) $Forest.MossLight
    }
}

# Stones set in the wall between rows minDy and maxDy.
function Paint-Stones($layer, [string]$shape, [int]$x0, [int]$x1, [int]$top, [int]$count, [int]$minDy, [int]$maxDy, [int]$seed) {
    for ($i = 0; $i -lt $count; $i++) {
        $dy = $minDy + (I ((Hash01 ($seed * 29 + $i)) * ($maxDy - $minDy)))
        if ($top + $dy -ge $layer.H) { continue }
        $s = Forest-Span $shape $x0 $x1 $dy $seed
        if ($s[1] - $s[0] -lt 12) { continue }
        $x = $s[0] + 4 + (I ((Hash01 ($seed * 37 + $i)) * ($s[1] - $s[0] - 10)))
        $sw = 2 + (I ((Hash01 ($seed * 41 + $i)) * 3))
        Fill $layer $x ($top + $dy) $sw 2 (Deep $Forest.Stone $dy)
        Fill $layer $x ($top + $dy) $sw 1 (Deep $Forest.StoneLight $dy)
    }
}

# Roots poking out of the sides and curling down.
function Paint-Roots($layer, [string]$shape, [int]$x0, [int]$x1, [int]$top, [int]$count, [int]$seed) {
    for ($i = 0; $i -lt $count; $i++) {
        $dy = 6 + (I ((Hash01 ($seed * 53 + $i)) * 10))
        $s = Forest-Span $shape $x0 $x1 $dy $seed
        $onLeft = ($i % 2 -eq 0)
        $len = 5 + (I ((Hash01 ($seed * 59 + $i)) * 7))
        for ($k = 0; $k -lt $len; $k++) {
            $x = $s[1] + (I ($k / 4))
            if ($onLeft) { $x = $s[0] - 1 - (I ($k / 4)) }
            Dot $layer $x ($top + $dy + $k) (Deep $Forest.BarkDark ($dy + $k))
        }
    }
}

# A whole forest platform with the details of its shape (drawForestDetails in the game).
function Paint-ForestLedge($layer, [string]$shape, [int]$x0, [int]$w, [int]$top, [int]$seed) {
    $x1 = $x0 + $w
    $f = $Forest
    Fill-ForestShape $layer $shape $x0 $x1 $top $seed
    switch ($shape) {
        'Cliff' {
            Paint-Drips $layer $x0 $w $top 4 (I ($w / 6)) $seed
            Paint-Stones $layer 'Cliff' $x0 $x1 $top 6 8 24 $seed
            Paint-Roots $layer 'Cliff' $x0 $x1 $top 2 $seed
            Paint-Tufts $layer $x0 $w $top (I ($w / 5)) $seed
        }
        'Boulder' {
            Paint-Drips $layer $x0 $w $top 5 (I ($w / 7)) $seed
            for ($i = 0; $i -lt 2; $i++) { # cracks
                $cx = $x0 + (I ($w / 4)) + $i * (I ($w / 3))
                for ($k = 0; $k -lt 8; $k++) { Dot $layer ($cx + (I ($k / 3))) ($top + 7 + $k) $f.StoneDark }
            }
            for ($i = 0; $i -lt 10; $i++) { # moss and lichen clinging to the rock
                $dy = 6 + (I ((Hash01 ($seed * 17 + $i)) * 12))
                $s = Forest-Span 'Boulder' $x0 $x1 $dy $seed
                $lx = $s[0] + 2 + (I ((Hash01 ($seed * 19 + $i)) * ($s[1] - $s[0] - 5)))
                $c = $f.MossDark; if ($i % 4 -eq 0) { $c = $f.Moss }
                Fill $layer $lx ($top + $dy) (2 + $i % 3) 1 (Deep $c $dy)
            }
            for ($i = 0; $i -lt 4; $i++) { # lit chips on the upper right
                $s = Forest-Span 'Boulder' $x0 $x1 (6 + $i * 3) $seed
                Fill $layer ($s[1] - 4 - ($i % 2) * 3) ($top + 6 + $i * 3) 2 1 $f.StoneLight
            }
            Paint-Tufts $layer $x0 $w $top (I ($w / 8)) $seed
        }
        'Log' {
            foreach ($g in @(6, 9, 12)) { # bark grain
                for ($x = $x0 + 3; $x -lt $x1 - 9; $x++) {
                    if ((Hash01 ($seed * 7 + $g * 1000 + $x)) -lt 0.55) { Dot $layer $x ($top + $g) $f.BarkDark }
                }
            }
            Fill $layer ($x0 + (I ($w * 0.4))) ($top + 8) 3 2 $f.BarkDark # knot
            Dot $layer ($x0 + (I ($w * 0.4)) + 1) ($top + 8) $f.BarkLight
            $rx = $x1 - 4; $ry = $top + 8 # the cut end with tree rings
            Disc $layer $rx $ry 7 $f.BarkDark
            Disc $layer $rx $ry 6 $f.Rings
            Disc $layer $rx $ry 4.6 $f.BarkLight
            Disc $layer $rx $ry 3.6 $f.Rings
            Disc $layer $rx $ry 2 $f.BarkLight
            Dot $layer $rx $ry $f.Bark
            for ($i = 0; $i -lt 4; $i++) { Fill $layer ($x0 - 3 - ($i % 2) * 2) ($top + 3 + $i * 3) (2 + $i % 2) 1 $f.BarkDark }
            for ($i = 0; $i -lt 6; $i++) { Fill $layer ($x0 + (I ((Hash01 ($seed * 23 + $i)) * ($w - 12)))) ($top + 2) 3 1 $f.Moss }
            for ($i = 0; $i -lt 3; $i++) { # small mushrooms on the side
                $mx = $x0 + 8 + $i * 5; $my = $top + 13 - ($i % 2)
                Fill $layer $mx $my 3 1 $f.Mushroom
                Fill $layer ($mx + 1) ($my + 1) 1 2 $f.Rings
            }
        }
    }
    if ($shape -eq 'Ruin') { Paint-ForestRuin $layer $x0 $w $top $seed }
}

# The old ruin: cornice, carved frieze, block courses, a crack, ivy, and moss on the slab.
function Paint-ForestRuin($layer, [int]$x0, [int]$w, [int]$top, [int]$seed) {
    $x1 = $x0 + $w
    $f = $Forest
    Fill $layer ($x0 - 2) ($top + 1) ($w + 4) 1 $f.StoneLight
    for ($x = $x0 + 3; $x -lt $x1 - 3; $x += 4) { Fill $layer $x ($top + 5) 2 2 $f.Stone }
    for ($course = 0; $course -lt 3; $course++) {
        $dy = 9 + $course * 7
        if ($top + $dy -ge $layer.H) { break }
        Fill $layer ($x0 + 3) ($top + $dy) ($w - 6) 1 (Deep $f.StoneDark $dy)
        for ($x = $x0 + 5 + ($course % 2) * 7; $x -lt $x1 - 4; $x += 14) { Fill $layer $x ($top + $dy) 1 7 (Deep $f.StoneDark $dy) }
    }
    for ($k = 0; $k -lt 9; $k++) { Dot $layer ($x0 + (I ($w * 0.6)) + ($k % 3) - (I ($k / 4))) ($top + 10 + $k) $f.Dark }
    for ($i = 0; $i -lt 3; $i++) { # ivy over the face
        $ix = $x0 + 4 + (I ((Hash01 ($seed * 61 + $i)) * ($w - 8)))
        $len = 6 + (I ((Hash01 ($seed * 67 + $i)) * 10))
        for ($k = 0; $k -lt $len; $k++) {
            Dot $layer $ix ($top + 1 + $k) $f.MossDark
            if ($k % 3 -eq 1) { $side = 1; if ((I ($k / 3)) % 2 -eq 1) { $side = -1 }; Dot $layer ($ix + $side) ($top + 1 + $k) $f.Moss }
        }
    }
    for ($i = 0; $i -lt 5; $i++) { # moss on the slab
        $mx = $x0 + (I ((Hash01 ($seed * 43 + $i)) * ($w - 4)))
        Fill $layer $mx $top 4 1 $f.Moss
        Fill $layer ($mx + 1) ($top - 1) 2 1 $f.MossLight
    }
}

# ---------------------------------------------------------------------------
# Animated overlays shared by the scenes.
# ---------------------------------------------------------------------------

# CSS and gradients for link glows, motes, fireflies, and mist wisps. Call once per SVG.
function Def-Overlays([string]$warm = $WarmLight) {
    Def-Linear 'riseWarm' @("0 $warm 0", "1 $warm 0.2")
    Def-Linear 'fallCool' @("0 $CoolLight 0", "1 $CoolLight 0.3")
    Def-Glow 'ffGlow' $warm 0.4
    AddCss '@keyframes moteUp { 0% { transform: translateY(0); opacity: 0 } 15% { opacity: 1 } 100% { transform: translateY(-22px); opacity: 0 } }'
    AddCss '@keyframes moteDown { 0% { transform: translateY(-24px); opacity: 0 } 20% { opacity: 1 } 100% { transform: translateY(0); opacity: 0 } }'
    Anim '.mote' 'moteUp' 2.4
    Anim '.dmote' 'moteDown' 2.6
    AddCss '.k1 { animation-delay: -0.6s; } .k2 { animation-delay: -1.2s; } .k3 { animation-delay: -1.8s; }'
    AddCss '@keyframes pulse { 0%, 100% { opacity: .7 } 50% { opacity: 1 } }'
    Anim '.pulse' 'pulse' 1.8 'ease-in-out'
    AddCss '@keyframes ffA { 0%, 100% { transform: translate(0px,0px) } 25% { transform: translate(6px,-4px) } 50% { transform: translate(10px,1px) } 75% { transform: translate(4px,4px) } }'
    AddCss '@keyframes ffB { 0%, 100% { transform: translate(0px,0px) } 30% { transform: translate(-5px,3px) } 60% { transform: translate(-9px,-3px) } 80% { transform: translate(-3px,-5px) } }'
    AddCss '@keyframes twinkle { 0%, 100% { opacity: .2 } 50% { opacity: 1 } }'
    Anim '.ffa' 'ffA' 7 'ease-in-out'
    Anim '.ffb' 'ffB' 9 'ease-in-out'
    Anim '.fft' 'twinkle' 2.6 'ease-in-out'
    AddCss '@keyframes wisp { 0%, 100% { transform: translateX(0px) } 50% { transform: translateX(-10px) } }'
    Anim '.wisp' 'wisp' 9 'ease-in-out'
}

# Light along the platform that a link points to: warm with motes rising for next,
# cool with motes falling onto it for previous (Region 2). Three nested bands give the
# beam soft, stepped edges and a brighter middle.
function LinkGlow-Svg([int]$x0, [int]$w, [int]$top, [string]$class, [switch]$Previous) {
    $color = $WarmLight; $grad = 'riseWarm'; $mote = 'mote'; $h = 30
    if ($Previous) { $color = $CoolLight; $grad = 'fallCool'; $mote = 'dmote'; $h = 40 }
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("<g class=`"$class`"><g class=`"pulse`">")
    for ($k = 0; $k -lt 3; $k++) {
        $inset = $k * [Math]::Max(3, (I ($w / 10)))
        if ($w - 2 * $inset -lt 6) { break }
        [void]$sb.Append((Rect ($x0 + $inset) ($top - $h + $k * 6) ($w - 2 * $inset) ($h - $k * 6) "url(#$grad)"))
    }
    [void]$sb.Append((Rect $x0 $top $w 1 $color ' fill-opacity=".6"'))
    [void]$sb.Append('</g>')
    for ($i = 0; $i -lt 4; $i++) {
        $mx = $x0 + 3 + (I ((Hash01 ($x0 * 3 + $i * 17)) * ($w - 6)))
        [void]$sb.Append("<rect class=`"$mote k$i`" x=`"$mx`" y=`"$($top - 2)`" width=`"1`" height=`"1`" fill=`"$color`"/>")
    }
    [void]$sb.Append('</g>')
    return $sb.ToString()
}

# Fireflies between (x0, y0) and (x1, y1): a soft glow and one bright pixel each.
function Fireflies-Svg([int]$count, [int]$seed, [int]$x0, [int]$x1, [int]$y0, [int]$y1) {
    $sb = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $count; $i++) {
        $x = I ($x0 + (Hash01 ($seed + $i * 11)) * ($x1 - $x0))
        $y = I ($y0 + (Hash01 ($seed + $i * 7)) * ($y1 - $y0))
        $drift = 'ffa'; if ($i % 2 -eq 1) { $drift = 'ffb' }
        [void]$sb.Append("<g transform=`"translate($x $y)`"><g class=`"$drift`" style=`"animation-delay:-$(N ($i * 1.3))s`">")
        [void]$sb.Append("<g class=`"fft`" style=`"animation-delay:-$(N ($i * 0.7))s`">$(Glow 0.5 0.5 4 'ffGlow')")
        [void]$sb.Append("<rect width=`"1`" height=`"1`" fill=`"$WarmLight`"/></g></g></g>")
    }
    return $sb.ToString()
}

# Dust kicked up at (x, y) at second t of the loop, plus a few leaves when $leaves is given.
function Puff-Svg([int]$n, [double]$x, [double]$y, [double]$t, [double]$loop, [string]$dust, [string[]]$leaves = @()) {
    $e = $t + 0.45
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("<g transform=`"translate($(N $x) $(N $y))`">")
    foreach ($side in @(@('l', -1), @('r', 1))) {
        $name = "pf$($side[0])$n"
        $s = $side[1]
        Keyframes $name $loop @(0, 'opacity:0;transform:translate(0px,0px)', $t, 'opacity:.85;transform:translate(0px,0px)',
            $e, "opacity:0;transform:translate($(6 * $s)px,-2px)", $loop, "opacity:0;transform:translate($(6 * $s)px,-2px)")
        Anim ".$name" $name $loop
        AddCss ".$name { opacity: 0; }"
        $a = 1; $b = 4; $c = 1
        if ($s -lt 0) { $a = -3; $b = -5; $c = -2 }
        [void]$sb.Append("<g class=`"$name`" fill=`"$dust`"><rect x=`"$a`" y=`"-1`" width=`"2`" height=`"1`"/>")
        [void]$sb.Append("<rect x=`"$b`" y=`"-2`" width=`"1`" height=`"1`"/><rect x=`"$c`" y=`"-3`" width=`"1`" height=`"1`"/></g>")
    }
    for ($i = 0; $i -lt $leaves.Length; $i++) {
        $name = "pk$n$i"
        $dx = 2 + $i * 2
        if ($i % 2 -eq 0) { $dx = -$dx }
        Keyframes $name $loop @(0, 'opacity:0;transform:translate(0px,0px)', $t, 'opacity:1;transform:translate(0px,-1px)',
            ($t + 0.3), "opacity:1;transform:translate($($dx)px,-7px)", ($t + 1.0), "opacity:0;transform:translate($([Math]::Round($dx * 1.6))px,1px)",
            $loop, "opacity:0;transform:translate($([Math]::Round($dx * 1.6))px,1px)")
        Anim ".$name" $name $loop
        AddCss ".$name { opacity: 0; }"
        [void]$sb.Append("<rect class=`"$name`" x=`"0`" y=`"-2`" width=`"2`" height=`"1`" fill=`"$($leaves[$i])`"/>")
    }
    [void]$sb.Append('</g>')
    return $sb.ToString()
}

# ---------------------------------------------------------------------------
# hero.svg: the traveler walks Region 1's list along next, one ledge at a time.
# Only the current node and current->next are solid (Traversal Rule A); the other
# ledges fade into mist. At the tail, next is NULL.
# ---------------------------------------------------------------------------

$HeroLedges = @(
    [pscustomobject]@{ Shape = 'Cliff'; X = 16; W = 68; Top = 104; Seed = 1 }
    [pscustomobject]@{ Shape = 'Boulder'; X = 104; W = 50; Top = 98; Seed = 2 }
    [pscustomobject]@{ Shape = 'Log'; X = 174; W = 52; Top = 92; Seed = 3 }
    [pscustomobject]@{ Shape = 'Ruin'; X = 246; W = 52; Top = 100; Seed = 4 }
)

function Forest-Backdrop([int]$w, [int]$h, [double]$sunX, [double]$sunY, [double]$treeBase, [double[]]$midXs, [double]$shaftX, [double]$crownTop) {
    $f = $Forest
    Def-Linear 'hSky' @("0 $($f.SkyTop) 1", "0.55 $($f.SkyMid) 1", "1 $($f.SkyLow) 1")
    Def-Glow 'hSunWide' $f.Sun 0.26
    Def-Glow 'hSunCore' $f.Sun 0.34
    Def-Linear 'hHaze' @("0 $($f.Mist) 0", "1 $($f.Mist) 0.4")
    Def-Linear 'hBand' @("0 $($f.Mist) 0", "0.5 $($f.Mist) 0.24", "1 $($f.Mist) 0")
    Def-Linear 'hShaft' @("0 $($f.Sun) 0.13", "1 $($f.Sun) 0")
    Def-Linear 'hFloor' @("0 $($f.Depth) 0", "1 $($f.Depth) 0.9")
    AddCss '@keyframes sunPulse { 0%, 100% { opacity: .8 } 50% { opacity: 1 } }'
    Anim '.sunPulse' 'sunPulse' 6 'ease-in-out'
    AddCss '@keyframes shaft { 0%, 100% { opacity: .5 } 50% { opacity: 1 } }'
    Anim '.shaft' 'shaft' 7 'ease-in-out'
    AddCss '.s1 { animation-delay: -1.8s; } .s2 { animation-delay: -3.5s; } .s3 { animation-delay: -5.1s; }'
    AddCss '@keyframes drift { 0%, 100% { transform: translateX(0px) } 50% { transform: translateX(-18px) } }'
    Anim '.drift' 'drift' 16 'ease-in-out'

    Add (Rect 0 0 $w $h 'url(#hSky)')
    Add (Glow $sunX $sunY 150 'hSunWide')
    Add (Glow $sunX $sunY 52 'hSunCore' ' class="sunPulse"')
    $far = New-Layer $w $h
    Paint-Treeline $far 13 $treeBase 8 15 101 $f.FarTrees
    Add (Layer-Svg $far)
    Add (Rect 0 ($treeBase - 48) $w ($h - $treeBase + 48) 'url(#hHaze)')
    $slant = [Math]::Round($h * 0.45)
    for ($i = 0; $i -lt 4; $i++) { # slanted beams that fade as they fall
        $tx = $shaftX + $i * 32
        $sw = 7 + $i * 3
        Add "<polygon class=`"shaft s$i`" points=`"$tx,0 $($tx + $sw),0 $($tx + $sw - $slant),$h $($tx - $slant),$h`" fill=`"url(#hShaft)`"/>"
    }
    Add "<g class=`"drift`">$(Rect -20 ($treeBase - 18) ($w + 60) 16 'url(#hBand)')</g>"
    $mid = New-Layer $w $h
    Paint-MidTrees $mid $midXs 7 $crownTop 22 0.9
    Add (Layer-Svg $mid)
    Add (Rect 0 ($h - 66) $w 66 'url(#hHaze)' ' opacity=".6"')
    Add (Rect 0 ($h - 28) $w 28 'url(#hFloor)')
}

function Hero-Backdrop { Forest-Backdrop 320 128 226 76 98 @(20, 98, 186, 300) 190 20 }

# The four ledges, their mist veils and next-link light, the NULL mark, and the canopy.
function Hero-Ledges([double]$loop) {
    $layers = @()
    foreach ($l in $HeroLedges) {
        $layer = New-Layer 320 128
        Paint-ForestLedge $layer $l.Shape $l.X $l.W $l.Top $l.Seed
        Add (Layer-Svg $layer)
        $layers += $layer
    }

    # Mist over every ledge that is neither current nor current->next.
    $veils = @(@(2.3, 9.6), @(4.4, 9.6), @(0, 2.3, 6.6, 10), @(0, 4.4, 9.6, 10))
    for ($k = 0; $k -lt 4; $k++) {
        Window-Keyframes "mv$k" $loop $veils[$k] 0.35
        Anim ".mv$k" "mv$k" $loop
        Add (Mask-Svg $layers[$k] $Forest.Veil " class=`"mv$k`" fill-opacity=`".62`"")
    }

    # Warm light rises from the ledge that current->next points to.
    $next = @($null, @(0, 2.3, 9.7, 10), @(2.3, 4.4), @(4.4, 6.6))
    for ($k = 1; $k -lt 4; $k++) {
        Window-Keyframes "nx$k" $loop $next[$k]
        Anim ".nx$k" "nx$k" $loop
        $l = $HeroLedges[$k]
        Add (LinkGlow-Svg $l.X $l.W $l.Top "nx$k")
    }

    # On the tail, next is NULL: an amber glow past the edge and a stop mark.
    Def-Glow 'hNull' $Hazard 0.4
    Window-Keyframes 'nul' $loop @(6.6, 9.4)
    Anim '.nul' 'nul' $loop
    AddCss '.mv0, .mv1, .nx2, .nx3, .nul { opacity: 0; }'
    $stop = New-Layer 320 128
    Icon $stop 305 84 '..###.. .#####. ####### ####### ####### .#####. ..###..' $NullAmber
    Fill $stop 306 87 5 1 $UiNavy
    [void](Def-Text 'nullText' 'NULL')
    Add ("<g class=`"nul`">" + (Glow 309 98 22 'hNull') + (Layer-Svg $stop) + (Outlined 'nullText' 298 74 $NullAmber $UiNavy) + '</g>')

    $canopy = New-Layer 320 128
    Paint-Canopy $canopy -8 50 300
    Paint-Canopy $canopy 266 330 400
    Add (Layer-Svg $canopy)
    AddCss '@keyframes sway { 0%, 100% { transform: rotate(-3deg) } 50% { transform: rotate(3deg) } }'
    Anim '.vine1' 'sway' 5 'ease-in-out'
    Anim '.vine2' 'sway' 6.5 'ease-in-out' '-2s'
    Add (Vine-Svg 10 6 26 11 'vine1')
    Add (Vine-Svg 34 5 16 12 'vine2')
    Add (Vine-Svg 290 6 30 13 'vine2')
    Add (Vine-Svg 309 4 19 14 'vine1')
}

# The traveler's 10-second walk: N1 -> N2 -> N3 -> N4 (the tail), then a fade and a restart.
# Each row: pose, start, end, from x, from y, to x, to y. Feet positions are ledge tops.
function Hero-Traveler([double]$loop) {
    $segs = @(
        @('idle', 0.0, 0.5, 40, 104, 40, 104), @('walk', 0.5, 1.6, 40, 104, 78, 104),
        @('jump', 1.6, 2.3, 78, 104, 114, 98), @('land', 2.3, 2.45, 114, 98, 114, 98),
        @('walk', 2.45, 3.7, 114, 98, 148, 98), @('jump', 3.7, 4.4, 148, 98, 184, 92),
        @('land', 4.4, 4.55, 184, 92, 184, 92), @('walk', 4.55, 5.9, 184, 92, 220, 92),
        @('jump', 5.9, 6.6, 220, 92, 256, 100), @('land', 6.6, 6.75, 256, 100, 256, 100),
        @('walk', 6.75, 7.4, 256, 100, 272, 100), @('idle', 7.4, $loop, 272, 100, 272, 100)
    )
    Window-Keyframes 'hwisp' $loop @(0.3, 6.6) 0.5
    Anim '.hwisp' 'hwisp' $loop
    AddCss '@keyframes bob { 0%, 100% { transform: translateY(0px) } 50% { transform: translateY(-3px) } }'
    Anim '.bob' 'bob' 1.4 'ease-in-out'
    Def-Glow 'wispGlow' $WarmLight 0.55

    # A small light floats ahead of the traveler, toward next, until the tail.
    $wisp = '<g class="hwisp"><g class="bob">' + (Glow 15 -21 7 'wispGlow') + (Rect 14 -22 2 2 '#fff4c8') + '</g></g>'
    Add (Traveler-Route 'tr' $segs $loop @(0, 0.3, 9.2, 9.6) -extra $wisp)
}

# Dark grass in the foreground, only below and beside the route.
function Paint-Grass($layer, [int]$x0, [int]$x1, [int]$maxH, [int]$seed) {
    for ($x = $x0; $x -lt $x1; $x++) {
        if ((Hash01 ($seed + $x * 3)) -lt 0.3) { continue }
        $fromEnd = [Math]::Min($x - $x0, $x1 - 1 - $x) # shorter blades at the ends of a clump
        $h = 1 + (I ((Hash01 ($seed + $x * 7)) * [Math]::Min($maxH, 2 + $fromEnd * 1.5)))
        Fill $layer $x ($layer.H - $h) 1 $h $Forest.Canopy
        if ((Hash01 ($seed + $x * 5)) -gt 0.72) { Dot $layer $x ($layer.H - $h) $Forest.Vine }
    }
}

function Hero-Front([double]$loop) {
    $dust = Mix $Forest.EarthLight $Forest.Mist 0.5
    Add (Puff-Svg 1 78 104 1.6 $loop $dust)
    Add (Puff-Svg 2 114 98 2.3 $loop $dust @($Forest.Moss, $Forest.MossLight))      # leaves from the mossy rock
    Add (Puff-Svg 3 148 98 3.7 $loop $dust)
    Add (Puff-Svg 4 184 92 4.4 $loop $dust @($Forest.Rings, $Forest.BarkLight))     # bark chips from the log
    Add (Puff-Svg 5 220 92 5.9 $loop $dust)
    Add (Puff-Svg 6 256 100 6.6 $loop $dust @($Forest.StoneLight, $Forest.Moss))    # stone chips from the ruin
    Add (Fireflies-Svg 10 500 0 320 46 96)

    AddCss '@keyframes grass { 0%, 100% { transform: skewX(-4deg) } 50% { transform: skewX(4deg) } }'
    $clumps = @(@(0, 20, 8), @(86, 104, 6), @(154, 175, 7), @(226, 247, 6), @(303, 320, 9))
    for ($i = 0; $i -lt $clumps.Count; $i++) {
        $c = $clumps[$i]
        $layer = New-Layer 320 128
        Paint-Grass $layer $c[0] $c[1] $c[2] (900 + $i * 50)
        Anim ".gr$i" 'grass' (4 + $i * 0.7) 'ease-in-out'
        Add (Layer-Svg $layer " class=`"gr$i`" style=`"transform-origin: $([int](($c[0] + $c[1]) / 2))px 128px`"")
    }
    AddDef "<radialGradient id=`"hVig`" cx=`"50%`" cy=`"42%`" r=`"72%`"><stop offset=`"0.6`" stop-color=`"$UiNavy`" stop-opacity=`"0`"/><stop offset=`"1`" stop-color=`"$UiNavy`" stop-opacity=`"0.6`"/></radialGradient>"
    Add (Rect 0 0 320 128 'url(#hVig)')
}

function Hero-Title {
    Def-Linear 'hGold' @('0 #fff3c4 1', '0.2857 #fff3c4 1', '0.2857 #ffde78 1', '0.5714 #ffde78 1', '0.5714 #f2b04e 1', '1 #f2b04e 1') 'gradientUnits="userSpaceOnUse" x1="0" y1="0" x2="0" y2="7"'
    $tw = Def-Text 'hTitle' 'THE NODE WANDERER' 2
    $tx = [Math]::Round((320 - $tw) / 2)
    Add (Outlined 'hTitle' $tx 11 'url(#hGold)' $UiNavy)
    $sw = Def-Text 'hSub' 'Every ledge is a real linked-list node.'
    Add (Outlined 'hSub' ([Math]::Round((320 - $sw) / 2)) 32 $Accents[0] $UiNavy)

    # A glint crosses the title now and then.
    AddDef "<mask id=`"hTitleMask`" maskUnits=`"userSpaceOnUse`" x=`"0`" y=`"0`" width=`"320`" height=`"40`">$(Stamp 'hTitle' $tx 11 '#ffffff')</mask>"
    Keyframes 'tglint' 8 @(0, 'transform:translateX(0px)', 5.5, 'transform:translateX(0px)', 6.6, "transform:translateX($($tw + 32)px)", 8, "transform:translateX($($tw + 32)px)")
    Anim '.tglint' 'tglint' 8
    Add "<g mask=`"url(#hTitleMask)`"><polygon class=`"tglint`" points=`"$($tx - 12),8 $($tx - 4),8 $($tx - 16),30 $($tx - 24),30`" fill=`"#ffffff`" fill-opacity=`".55`"/></g>"
}

function Write-Hero {
    $loop = 10.0
    Begin-Svg 320 128 3 'The Node Wanderer: the traveler walks a linked list of forest ledges, following next from the head to the tail, where next is NULL.'
    Def-Traveler
    Def-Overlays
    Hero-Backdrop
    Hero-Ledges $loop
    Hero-Traveler $loop
    Hero-Front $loop
    Hero-Title
}
