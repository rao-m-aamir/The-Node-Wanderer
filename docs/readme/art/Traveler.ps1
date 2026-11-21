# The traveler, copied pixel for pixel from src/Player.cpp: hat, red scarf, ochre coat,
# backpack, and a lantern. Each pose becomes a <g> in defs with the feet at (0, 0).

$TravelerHead = @('...oooooo...', '..ohhhhhho..', '..oHHHHHHo..', 'oohhhhhhhhoo', 'ohhhhhhhhhho',
    '.okkSSSSSSo.', '.okssssssso.', '.okssssseso.', '.oSssssssSo.', '..oSssssSo..')
$TravelerBody = @('...orrrrrro.', 'oBBoccccRco.', 'obboccccRco.', 'obboccccccco', 'obboCcccccco',
    'obbowwwwwwso', 'obbocccccco.', '.oooCCCCCCo.')
$TravelerLegs = @{
    Stand  = @('...oppppppo.', '...oppooppo.', '...oppooppo.', '...oppooppo.', '...offoofffo', '...ooooooooo')
    Stride = @('...oppppppo.', '..oppo..oppo', '.oppo...oppo', 'oppo....oppo', 'offo....offo', 'oooo....oooo')
    Lift   = @('...oppppppo.', '..oppo.oppo.', '.oppo..oppo.', 'oppo...offo.', 'offo...oooo.', 'oooo........')
    Jump   = @('...oppppppo.', '..oppppppppo', '..offo..offo', '..oooo..oooo')
    Fall   = @('...oppppppo.', '..oppo..oppo', '.oppo...oppo', '.offo...offo', '.oooo...oooo')
    Land   = @('..oppppppppo', '.offo...offo', '.oooo...oooo')
}

# Sprite codes are case-sensitive (h = hat, H = hat band), so this is a char dictionary.
$TravelerColors = New-Object 'System.Collections.Generic.Dictionary[char,string]'
foreach ($pair in @('o#16121e', 'h#3a7a6a', 'H#96603c', 'k#5c3a2a', 's#fcdab8', 'S#d6a280', 'e#181422',
        'r#e8483c', 'R#ac2c30', 'c#e2a04e', 'C#b26e36', 'w#62402c', 'b#845a3a', 'B#d8c08e', 'p#484066',
        'f#36262a', 'L#60402c', 'G#ffe08c')) {
    $TravelerColors[$pair[0]] = $pair.Substring(1)
}

function Add-Part($px, [string[]]$rows, [int]$col, [int]$row, [bool]$blink) {
    for ($r = 0; $r -lt $rows.Length; $r++) {
        $line = $rows[$r]
        for ($c = 0; $c -lt $line.Length; $c++) {
            $code = $line[$c]
            if ($code -eq '.') { continue }
            if ($blink -and $code -ceq 'e') { $code = [char]'s' } # eye closed
            $px["$($col + $c),$($row + $r)"] = $code
        }
    }
}

# Builds one pose the way drawTraveler does: scarf tail, legs, body, head, then the lantern.
# bob moves the head and body, lift and swing move the lantern, droop bends the scarf.
function Build-Pose([string]$legsName, [int]$bob = 0, [int]$lift = 0, [int]$swing = 0,
                    [double]$droop = 0.45, [double]$time = 0, [switch]$Blink) {
    $legs = $TravelerLegs[$legsName]
    $px = New-Object 'System.Collections.Generic.Dictionary[string,char]'
    $bodyTop = 10 + $bob

    $tail = @(0, 0, 0, 0, 0)
    for ($i = 1; $i -le 4; $i++) {
        $wave = [Math]::Sin($time * 9 + $i * 1.3) * 0.6 * ($i / 4)
        $tail[$i] = $bodyTop + [int][Math]::Round($droop * $i + $wave, [MidpointRounding]::AwayFromZero)
    }
    for ($i = 1; $i -le 4; $i++) { # outline first, so it never covers the cloth
        $thick = 1
        if ($i -lt 3) { $thick = 2 }
        $px["$(3 - $i),$($tail[$i] - 1)"] = 'o'
        $px["$(3 - $i),$($tail[$i] + $thick)"] = 'o'
    }
    for ($i = 1; $i -le 4; $i++) {
        $px["$(3 - $i),$($tail[$i])"] = 'r'
        if ($i -lt 3) { $px["$(3 - $i),$($tail[$i] + 1)"] = 'R' }
    }

    Add-Part $px $legs 0 18 $false
    Add-Part $px $TravelerBody 0 $bodyTop $false
    Add-Part $px $TravelerHead 1 $bob $Blink.IsPresent

    $top = $bodyTop + 6 + $lift # lantern ring, frame, and light
    $left = 11 + $swing
    $px["$($left + 1),$top"] = 'L'
    for ($r = 1; $r -le 3; $r++) {
        $px["$left,$($top + $r)"] = 'L'
        $px["$($left + 1),$($top + $r)"] = 'L'
        if ($r -lt 3) { $px["$($left + 1),$($top + $r)"] = 'G' }
        $px["$($left + 2),$($top + $r)"] = 'L'
    }
    return [pscustomobject]@{ Pixels = $px; Height = 18 + $legs.Length }
}

# Adds a pose to defs as <g id> with the feet at (0, 0), plus the soft contact shadow.
function Def-Pose([string]$id, $pose, [switch]$Shadow) {
    $byColor = New-Object 'System.Collections.Generic.SortedDictionary[string,System.Text.StringBuilder]'
    $h = $pose.Height
    for ($row = -3; $row -lt $h; $row++) {
        $col = -2
        while ($col -le 15) {
            $code = [char]0
            if (-not $pose.Pixels.TryGetValue("$col,$row", [ref]$code)) { $col++; continue }
            $start = $col
            $col++
            $next = [char]0
            while ($col -le 15 -and $pose.Pixels.TryGetValue("$col,$row", [ref]$next) -and $next -ceq $code) { $col++ }
            $color = $TravelerColors[$code]
            $sb = $null
            if (-not $byColor.TryGetValue($color, [ref]$sb)) {
                $sb = New-Object System.Text.StringBuilder
                $byColor[$color] = $sb
            }
            [void]$sb.Append("M$($start - 7) $($row - $h)h$($col - $start)v1h-$($col - $start)z")
        }
    }
    $out = New-Object System.Text.StringBuilder
    [void]$out.Append("<g id=`"$id`">")
    if ($Shadow) { [void]$out.Append('<rect x="-6" y="0" width="12" height="1" fill="#000" fill-opacity=".35"/>') }
    foreach ($e in $byColor.GetEnumerator()) { [void]$out.Append("<path fill=`"$($e.Key)`" d=`"$($e.Value.ToString())`"/>") }
    [void]$out.Append('</g>')
    AddDef $out.ToString()
}

# Adds every pose, the lantern glow, and the shared idle and walk cycles to the current SVG.
function Def-Traveler {
    Def-Pose 'tv-stand' (Build-Pose 'Stand' -time 0.3) -Shadow
    Def-Pose 'tv-breathe' (Build-Pose 'Stand' -bob 1 -time 1.1) -Shadow
    Def-Pose 'tv-blink' (Build-Pose 'Stand' -time 0.3 -Blink) -Shadow
    Def-Pose 'tv-walk0' (Build-Pose 'Stride' -swing 1 -droop 0 -time 0) -Shadow
    Def-Pose 'tv-walk1' (Build-Pose 'Stand' -bob -1 -droop 0 -time 0.12) -Shadow
    Def-Pose 'tv-walk2' (Build-Pose 'Lift' -droop 0 -time 0.24) -Shadow
    Def-Pose 'tv-walk3' (Build-Pose 'Stand' -bob -1 -droop 0 -time 0.36) -Shadow
    Def-Pose 'tv-jump' (Build-Pose 'Jump' -bob -1 -lift -1 -droop 0.7 -time 0.2)
    Def-Pose 'tv-fall' (Build-Pose 'Fall' -lift -2 -droop -0.5 -time 0.5)
    Def-Pose 'tv-land' (Build-Pose 'Land' -lift 1 -time 0.1) -Shadow
    Def-Glow 'tv-light' '#ffe08c' 0.24

    # Lantern flicker, like 0.85 + 0.15 sin(13t) sin(7t) in the game.
    Keyframes 'tvFlicker' 1.7 @(0, 'opacity:1', 0.3, 'opacity:.8', 0.5, 'opacity:.95', 0.8, 'opacity:.72',
        1.1, 'opacity:1', 1.4, 'opacity:.86', 1.7, 'opacity:1')
    Anim '.tv-glow' 'tvFlicker' 1.7

    # Idle: breathe every 0.8 s and blink once per cycle.
    Switch-Keyframes 'tvI0' 3.2 @(0, 0.8, 1.6, 1.9, 2.02, 2.4)
    Switch-Keyframes 'tvI1' 3.2 @(0.8, 1.6, 2.4, 3.2)
    Switch-Keyframes 'tvI2' 3.2 @(1.9, 2.02)
    AddCss '.tv-i1, .tv-i2, .tv-w1, .tv-w2, .tv-w3 { opacity: 0; }'
    Anim '.tv-i0' 'tvI0' 3.2 'steps(1,end)'
    Anim '.tv-i1' 'tvI1' 3.2 'steps(1,end)'
    Anim '.tv-i2' 'tvI2' 3.2 'steps(1,end)'

    # Walk: four frames, 0.12 s each (stride, passing, other stride, passing).
    AddCss '@keyframes tvWalk { 0% { opacity:1 } 25% { opacity:0 } 100% { opacity:0 } }'
    Anim '.tv-w0' 'tvWalk' 0.48 'steps(1,end)'
    Anim '.tv-w1' 'tvWalk' 0.48 'steps(1,end)' '-0.36s'
    Anim '.tv-w2' 'tvWalk' 0.48 'steps(1,end)' '-0.24s'
    Anim '.tv-w3' 'tvWalk' 0.48 'steps(1,end)' '-0.12s'
}

# A standing traveler with the feet at (x, y): breathing, blinking, lantern flickering.
# -Mirror faces left, the way the game mirrors the sprite.
function Traveler-Idle([double]$x, [double]$y, [string]$attrs = '', [switch]$Mirror) {
    $flip = ''
    if ($Mirror) { $flip = ' scale(-1 1)' }
    return "<g transform=`"translate($(N $x) $(N $y))$flip`"$attrs>" + (Glow 5.5 -5.5 15 'tv-light' ' class="tv-glow"') +
        '<use href="#tv-stand" class="tv-i0"/><use href="#tv-breathe" class="tv-i1"/><use href="#tv-blink" class="tv-i2"/></g>'
}

# The four walking frames around the feet at (0, 0); wrap them in a moving group.
function Traveler-WalkFrames {
    return '<use href="#tv-walk0" class="tv-w0"/><use href="#tv-walk1" class="tv-w1"/>' +
        '<use href="#tv-walk2" class="tv-w2"/><use href="#tv-walk3" class="tv-w3"/>'
}

# Animates the traveler along a route on a loop. Each segment is
# @(pose, start, end, from x, from y, to x, to y) with the feet on the ground; poses are
# idle, walk, jump (drawn as a parabola, rising then falling), and land.
# $fade = @(fade-in start, fade-in end, fade-out start, fade-out end). Returns markup;
# $extra goes inside the moving group (for a wisp). The sampled path is left in $LastRoute.
function Traveler-Route([string]$id, $segs, [double]$loop, [double[]]$fade, [switch]$Mirror, [string]$extra = '', [double]$arc = 14, [double[]]$still = $null) {
    $move = New-Object System.Collections.ArrayList
    $points = New-Object System.Collections.ArrayList
    $windows = @{ idle = @(); walk = @(); jump = @(); fall = @(); land = @() }
    foreach ($s in $segs) {
        $kind = $s[0]; $t0 = [double]$s[1]; $t1 = [double]$s[2]
        $at = "transform:translate($($s[3])px,$($s[4])px)"
        if ($kind -eq 'walk') { $at += ";animation-timing-function:steps($([Math]::Abs($s[5] - $s[3])),end)" } # a pixel per step
        [void]$move.AddRange(@($t0, $at))
        [void]$points.Add(@($t0, [double]$s[3], [double]$s[4]))
        if ($kind -eq 'jump') {
            for ($k = 1; $k -lt 8; $k++) {
                $u = $k / 8
                $x = $s[3] + ($s[5] - $s[3]) * $u
                $y = $s[4] + ($s[6] - $s[4]) * $u - $arc * 4 * $u * (1 - $u)
                $t = $t0 + ($t1 - $t0) * $u
                [void]$move.AddRange(@($t, "transform:translate($(N $x)px,$(N $y)px)"))
                [void]$points.Add(@($t, $x, $y))
            }
            $windows.jump += @($t0, (($t0 + $t1) / 2))
            $windows.fall += @((($t0 + $t1) / 2), $t1)
        } else {
            $windows[$kind] += @($t0, $t1)
        }
    }
    $last = $segs[$segs.Count - 1]
    [void]$move.AddRange(@($loop, "transform:translate($($last[5])px,$($last[6])px)"))
    [void]$points.Add(@($loop, [double]$last[5], [double]$last[6]))
    $script:LastRoute = $points
    Keyframes "$id-move" $loop $move
    Anim ".$id-move" "$id-move" $loop
    Keyframes "$id-fade" $loop @(0, 'opacity:0', $fade[0], 'opacity:0', $fade[1], 'opacity:1', $fade[2], 'opacity:1', $fade[3], 'opacity:0', $loop, 'opacity:0')
    Anim ".$id-fade" "$id-fade" $loop
    if ($null -eq $still) { $still = @([double]$segs[0][3], [double]$segs[0][4]) } # where it stands with reduced motion
    $hidden = @('idle', 'walk', 'jump', 'fall', 'land' | Where-Object { $_ -ne $segs[0][0] } | ForEach-Object { ".$id-$_" }) -join ', '
    AddCss ".$id-move { transform: translate($(N $still[0])px,$(N $still[1])px); } $hidden { opacity: 0; }"

    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append((Glow 5.5 -5.5 15 'tv-light' ' class="tv-glow"'))
    $uses = @{ idle = '<use href="#tv-stand" class="tv-i0"/><use href="#tv-breathe" class="tv-i1"/><use href="#tv-blink" class="tv-i2"/>'
        walk = (Traveler-WalkFrames); jump = '<use href="#tv-jump"/>'; fall = '<use href="#tv-fall"/>'; land = '<use href="#tv-land"/>' }
    foreach ($kind in @('idle', 'walk', 'jump', 'fall', 'land')) {
        if ($windows[$kind].Count -eq 0) { continue }
        Switch-Keyframes "$id-$kind" $loop ([double[]]$windows[$kind])
        Anim ".$id-$kind" "$id-$kind" $loop 'steps(1,end)'
        [void]$sb.Append("<g class=`"$id-$kind`">$($uses[$kind])</g>")
    }
    $inner = $sb.ToString()
    if ($Mirror) { $inner = "<g transform=`"scale(-1 1)`">$inner</g>" } # faces left, like the game's mirrored sprite
    return "<g class=`"$id-move`"><g class=`"$id-fade`">$inner$extra</g></g>"
}
