# Shared pieces for the README art: numbers, colors, pixel layers, and the SVG writer.
# Dot-sourced by make-art.ps1. Nothing here is random, so every run writes identical files.

$Inv = [Globalization.CultureInfo]::InvariantCulture

# Numbers in SVG always use a dot, whatever the system culture.
function N([double]$v) { return $v.ToString('0.###', $Inv) }

# Floor to an int (a plain [int] cast would round half to even).
function I([double]$v) { return [int][Math]::Floor($v) }

# Repeatable pseudo-random value in [0, 1) for an integer seed.
function Hash01([int]$n) {
    $x = [Math]::Sin($n * 12.9898 + 78.233) * 43758.5453
    return $x - [Math]::Floor($x)
}

# CSS percentage of a time on a loop, e.g. Pct 2.5 10 -> "25%".
function Pct([double]$t, [double]$loop) { return (N ([Math]::Min(100.0, [Math]::Max(0.0, $t / $loop * 100)))) + '%' }

# ---------------------------------------------------------------------------
# Colors are '#rrggbb' strings; layers store them as ints.
# ---------------------------------------------------------------------------

$script:ColorInts = New-Object 'System.Collections.Generic.Dictionary[string,int]'

function ColorInt([string]$hex) {
    $value = 0
    if (-not $script:ColorInts.TryGetValue($hex, [ref]$value)) {
        $value = [Convert]::ToInt32($hex.Substring(1), 16)
        $script:ColorInts[$hex] = $value
    }
    return $value
}

function HexOf([int]$v) { return '#' + $v.ToString('x6') }

# Blends two colors: t = 0 gives a, t = 1 gives b.
function Mix([string]$a, [string]$b, [double]$t) {
    $x = ColorInt $a
    $y = ColorInt $b
    $r = [int][Math]::Round((($x -shr 16) -band 255) * (1 - $t) + (($y -shr 16) -band 255) * $t)
    $g = [int][Math]::Round((($x -shr 8) -band 255) * (1 - $t) + (($y -shr 8) -band 255) * $t)
    $bl = [int][Math]::Round(($x -band 255) * (1 - $t) + ($y -band 255) * $t)
    return HexOf (($r -shl 16) -bor ($g -shl 8) -bor $bl)
}

# ---------------------------------------------------------------------------
# Pixel layers: a grid of opaque colors (-1 = empty), painted in order like the
# game's canvas, then written out as one <path> per color.
# ---------------------------------------------------------------------------

function New-Layer([int]$w, [int]$h) {
    $cells = New-Object 'int[]' ($w * $h)
    for ($i = 0; $i -lt $cells.Length; $i++) { $cells[$i] = -1 }
    return [pscustomobject]@{ W = $w; H = $h; Cells = $cells }
}

# Fills a rectangle of pixels, clipped to the layer.
function Fill($layer, [int]$x, [int]$y, [int]$w, [int]$h, [string]$color) {
    $c = ColorInt $color
    $x0 = [Math]::Max(0, $x); $x1 = [Math]::Min($layer.W, $x + $w)
    $y0 = [Math]::Max(0, $y); $y1 = [Math]::Min($layer.H, $y + $h)
    $cells = $layer.Cells
    $lw = $layer.W
    for ($yy = $y0; $yy -lt $y1; $yy++) {
        $row = $yy * $lw
        for ($xx = $x0; $xx -lt $x1; $xx++) { $cells[$row + $xx] = $c }
    }
}

function Dot($layer, [int]$x, [int]$y, [string]$color) { Fill $layer $x $y 1 1 $color }

# Color of a pixel, or $null when it is empty or outside the layer.
function Get-Pixel($layer, [int]$x, [int]$y) {
    if ($x -lt 0 -or $y -lt 0 -or $x -ge $layer.W -or $y -ge $layer.H) { return $null }
    $c = $layer.Cells[$y * $layer.W + $x]
    if ($c -lt 0) { return $null }
    return HexOf $c
}

# A filled pixel circle: the pixels whose centers lie inside the circle.
function Disc($layer, [double]$cx, [double]$cy, [double]$r, [string]$color) {
    $top = I ($cy - $r - 1)
    $bottom = I ($cy + $r + 1)
    for ($y = $top; $y -le $bottom; $y++) {
        $d = ($y + 0.5) - $cy
        $s = $r * $r - $d * $d
        if ($s -lt 0) { continue }
        $half = [Math]::Sqrt($s)
        $x0 = [int][Math]::Ceiling($cx - $half - 0.5)
        $x1 = I ($cx + $half - 0.5)
        if ($x1 -ge $x0) { Fill $layer $x0 $y ($x1 - $x0 + 1) 1 $color }
    }
}

# Writes a layer as SVG. Equal colors next to each other in a row become one span, and
# equal spans in consecutive rows become one rectangle. Returns '' for an empty layer.
function Layer-Svg($layer, [string]$attrs = '') {
    $w = $layer.W; $h = $layer.H; $cells = $layer.Cells
    $paths = New-Object 'System.Collections.Generic.SortedDictionary[int,System.Text.StringBuilder]'
    $open = New-Object 'System.Collections.Generic.Dictionary[long,int[]]' # key -> (first row, rows)
    for ($y = 0; $y -le $h; $y++) {
        $seen = New-Object 'System.Collections.Generic.HashSet[long]'
        if ($y -lt $h) {
            $row = $y * $w
            $x = 0
            while ($x -lt $w) {
                $c = $cells[$row + $x]
                $start = $x
                $x++
                while ($x -lt $w -and $cells[$row + $x] -eq $c) { $x++ }
                if ($c -lt 0) { continue }
                $key = ([long]$c -shl 24) -bor ([long]$start -shl 12) -bor [long]($x - $start)
                $run = $null
                if ($open.TryGetValue($key, [ref]$run)) { $run[1]++ } else { $open[$key] = [int[]]@($y, 1) }
                [void]$seen.Add($key)
            }
        }
        $closing = @($open.Keys | Where-Object { -not $seen.Contains($_) })
        foreach ($key in $closing) {
            $run = $open[$key]
            $c = [int]($key -shr 24)
            $sx = [int](($key -shr 12) -band 4095)
            $len = [int]($key -band 4095)
            $sb = $null
            if (-not $paths.TryGetValue($c, [ref]$sb)) {
                $sb = New-Object System.Text.StringBuilder
                $paths[$c] = $sb
            }
            [void]$sb.Append("M$sx $($run[0])h${len}v$($run[1])h-${len}z")
            [void]$open.Remove($key)
        }
    }
    if ($paths.Count -eq 0) { return '' }
    $out = New-Object System.Text.StringBuilder
    [void]$out.Append("<g$attrs>")
    foreach ($entry in $paths.GetEnumerator()) {
        [void]$out.Append("`n<path fill=`"$(HexOf $entry.Key)`" d=`"$($entry.Value.ToString())`"/>")
    }
    [void]$out.Append("`n</g>")
    return $out.ToString()
}

# ---------------------------------------------------------------------------
# The SVG being written. CSS, defs, and body are collected separately and joined
# by Save-Svg; the pixel font adds only the glyphs a file actually uses.
# ---------------------------------------------------------------------------

function Begin-Svg([int]$w, [int]$h, [int]$scale, [string]$label) {
    $script:Doc = [pscustomobject]@{
        W = $w; H = $h; Scale = $scale; Label = $label
        Css = New-Object System.Text.StringBuilder
        Defs = New-Object System.Text.StringBuilder
        Body = New-Object System.Text.StringBuilder
        Glyphs = New-Object 'System.Collections.Generic.SortedSet[int]'
    }
}

function Add([string]$s) { if ($s) { [void]$script:Doc.Body.Append($s).Append("`n") } }
function AddDef([string]$s) { [void]$script:Doc.Defs.Append($s).Append("`n") }
function AddCss([string]$s) { [void]$script:Doc.Css.Append($s).Append("`n") }

function Save-Svg([string]$path) {
    $d = $script:Doc
    $label = [System.Security.SecurityElement]::Escape($d.Label)
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("<svg xmlns=`"http://www.w3.org/2000/svg`" viewBox=`"0 0 $($d.W) $($d.H)`" width=`"$($d.W * $d.Scale)`" height=`"$($d.H * $d.Scale)`" shape-rendering=`"crispEdges`" role=`"img`" aria-label=`"$label`">`n")
    [void]$sb.Append("<title>$label</title>`n<style>`n")
    [void]$sb.Append($d.Css.ToString())
    [void]$sb.Append("@media (prefers-reduced-motion: reduce) { * { animation: none !important; } .smil { display: none; } }`n")
    [void]$sb.Append("</style>`n<defs>`n")
    foreach ($code in $d.Glyphs) { [void]$sb.Append((Glyph-Def $code)).Append("`n") }
    [void]$sb.Append($d.Defs.ToString()).Append("</defs>`n")
    [void]$sb.Append($d.Body.ToString()).Append("</svg>`n")
    [System.IO.File]::WriteAllText($path, $sb.ToString(), (New-Object System.Text.UTF8Encoding $false))
    Write-Host ('wrote {0,-12} {1,7} bytes' -f (Split-Path -Leaf $path), $sb.Length)
}

# Small element builders. They return markup; pass it to Add.
function Rect([double]$x, [double]$y, [double]$w, [double]$h, [string]$fill, [string]$attrs = '') {
    return "<rect x=`"$(N $x)`" y=`"$(N $y)`" width=`"$(N $w)`" height=`"$(N $h)`" fill=`"$fill`"$attrs/>"
}

function Glow([double]$cx, [double]$cy, [double]$r, [string]$gradient, [string]$attrs = '') {
    return "<circle cx=`"$(N $cx)`" cy=`"$(N $cy)`" r=`"$(N $r)`" fill=`"url(#$gradient)`"$attrs/>"
}

# A linear gradient; each stop is 'offset color opacity', e.g. '0.5 #ffde82 0.4'.
function Def-Linear([string]$id, [string[]]$stops, [string]$dir = 'x1="0" y1="0" x2="0" y2="1"') {
    $s = New-Object System.Text.StringBuilder
    foreach ($stop in $stops) {
        $p = $stop.Split(' ')
        [void]$s.Append("<stop offset=`"$($p[0])`" stop-color=`"$($p[1])`" stop-opacity=`"$($p[2])`"/>")
    }
    AddDef "<linearGradient id=`"$id`" $dir>$($s.ToString())</linearGradient>"
}

# A round glow that fades evenly from the middle to the edge, like DrawCircleGradient.
function Def-Glow([string]$id, [string]$color, [double]$alpha) {
    AddDef "<radialGradient id=`"$id`"><stop offset=`"0`" stop-color=`"$color`" stop-opacity=`"$(N $alpha)`"/><stop offset=`"1`" stop-color=`"$color`" stop-opacity=`"0`"/></radialGradient>"
}

# ---------------------------------------------------------------------------
# CSS animation helpers. Times are in seconds on a loop of $loop seconds.
# ---------------------------------------------------------------------------

# Keyframes from a flat list of (seconds, css) pairs.
function Keyframes([string]$name, [double]$loop, $frames) {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("@keyframes $name {")
    for ($i = 0; $i -lt $frames.Count; $i += 2) {
        [void]$sb.Append(" $(Pct $frames[$i] $loop) { $($frames[$i + 1]) }")
    }
    [void]$sb.Append(' }')
    AddCss $sb.ToString()
}

# Opacity keyframes: $on inside the [start, end) windows, 0 elsewhere.
# Each window fades in over $fade seconds after its start and out after its end.
function Window-Keyframes([string]$name, [double]$loop, [double[]]$windows, [double]$fade = 0.3, [string]$on = '1') {
    $f = New-Object System.Collections.ArrayList
    $last = -1.0
    if ($windows[0] -gt 0) { [void]$f.AddRange(@(0, 'opacity:0')); $last = 0 }
    for ($i = 0; $i -lt $windows.Length; $i += 2) {
        $a = $windows[$i]
        $b = $windows[$i + 1]
        if ($a -le 0) { [void]$f.AddRange(@(0, "opacity:$on")) }
        else { [void]$f.AddRange(@($a, 'opacity:0', ([Math]::Min($a + $fade, $b)), "opacity:$on")) }
        if ($b -ge $loop) { [void]$f.AddRange(@($loop, "opacity:$on")); $last = $loop }
        else {
            $end = [Math]::Min($b + $fade, $loop)
            [void]$f.AddRange(@($b, "opacity:$on", $end, 'opacity:0'))
            $last = $end
        }
    }
    if ($last -lt $loop) { [void]$f.AddRange(@($loop, 'opacity:0')) }
    Keyframes $name $loop $f
}

# Attaches a looping animation to a CSS selector.
function Anim([string]$selector, [string]$name, [double]$seconds, [string]$timing = 'linear', [string]$more = '') {
    $rule = "$selector { animation: $name $(N $seconds)s $timing infinite"
    if ($more) { $rule += " $more" }
    AddCss ($rule + '; }')
}

# Opacity keyframes that switch instantly (pair with steps(1,end)): 1 inside the windows.
function Switch-Keyframes([string]$name, [double]$loop, [double[]]$windows) {
    $f = New-Object System.Collections.ArrayList
    if ($windows[0] -le 0) { [void]$f.AddRange(@(0, 'opacity:1')) } else { [void]$f.AddRange(@(0, 'opacity:0')) }
    for ($i = 0; $i -lt $windows.Length; $i += 2) {
        if ($windows[$i] -gt 0) { [void]$f.AddRange(@($windows[$i], 'opacity:1')) }
        if ($windows[$i + 1] -lt $loop) { [void]$f.AddRange(@($windows[$i + 1], 'opacity:0')) }
    }
    [void]$f.AddRange(@($loop, 'opacity:0'))
    Keyframes $name $loop $f
}

# ---------------------------------------------------------------------------
# Interface colors from src/Theme.h and src/GameUi.cpp.
# ---------------------------------------------------------------------------

$UiNavy = '#0a141c'
$UiText = '#eceef8'
$UiDim = '#969cb4'
$Objective = '#ffde78'
$CurrentGold = '#ffcd46'
$NullAmber = '#ffb032'
$CodeBlue = '#96e6ff'
$Parchment = '#eee0c0'
$ParchmentShade = '#d4c098'
$Ink = '#30241e'
$InkSoft = '#705a48'
$InkNull = '#a83c1e'
$Leather = '#563426'
$LeatherDark = '#362018'
$Gilt = '#deba6e'
$WarmLight = '#ffde82'
$CoolLight = '#96d2ff'
$Hazard = '#ffaa32'
$Accents = @('#96dc6e', '#82beff', '#bea0ff', '#ffba50')   # region accent, regions 1-4
$RegionInks = @('#2e6c34', '#285296', '#6240a0', '#9e5e12') # region ink on parchment

# The silhouette of a layer in one color, e.g. the mist veil over a platform.
function Mask-Svg($layer, [string]$color, [string]$attrs = '') {
    $copy = New-Layer $layer.W $layer.H
    $c = ColorInt $color
    $src = $layer.Cells
    $dst = $copy.Cells
    for ($i = 0; $i -lt $src.Length; $i++) { if ($src[$i] -ge 0) { $dst[$i] = $c } }
    return Layer-Svg $copy $attrs
}
