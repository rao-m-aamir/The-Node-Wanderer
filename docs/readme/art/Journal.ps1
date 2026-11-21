# journal.svg: the field journal (the game's node inspector) on Region 1, as drawn by
# Game::drawInspector in src/GameUi.cpp. It steps through N1..N5 every 2 seconds and
# shows the same lines the game reads from the real links.

$JournalNodes = @('Grove Edge', 'Mossy Rock', 'Fallen Log', 'Old Ruin', 'Arch Cliff')

# The octagonal stop mark drawn next to every NULL, so NULL is readable without color.
function Stop-Mark($layer, [int]$x, [int]$y, [string]$color, [string]$bar) {
    Icon $layer $x $y '..###.. .#####. ####### ####### ####### .#####. ..###..' $color
    Fill $layer ($x + 1) ($y + 3) 5 1 $bar
}

# A key cap followed by an action, e.g. [TAB] close. Returns markup and the width via $script:HintW.
function Key-Hint([double]$x, [double]$y, [string]$key, [string]$action, [string]$cap, [string]$keyInk, [string]$actionInk) {
    $kw = (Text-Width $key) + 6
    $script:HintW = $kw + 4 + (Text-Width $action)
    return (Rect ($x + 1) $y ($kw - 2) 11 $cap) + (Rect $x ($y + 1) $kw 9 $cap) + (Text $key ($x + 3) ($y + 2) $keyInk) + (Text $action ($x + $kw + 4) ($y + 2) $actionInk)
}

function Write-Journal {
    $W = 252; $H = 150; $loop = 10.0
    $regionInk = $RegionInks[0] # (not $ink: PowerShell names ignore case, and $Ink is the journal's ink)
    Begin-Svg $W $H 3 'The field journal on Region 1 (singly linked list): it shows the node the traveler is on, where next points, that previous is not stored, and a walk from the head that stops at NULL.'
    $paper = New-Layer $W $H
    Fill $paper 8 9 240 138 '#000000'                      # shadow (drawn transparent below)
    $shadowSvg = Layer-Svg $paper ' fill-opacity=".3"'
    $paper = New-Layer $W $H
    Fill $paper 5 4 238 140 $ParchmentShade                # the darker edge
    Fill $paper 4 5 240 138 $ParchmentShade
    Fill $paper 6 5 236 138 $Parchment
    Fill $paper 5 6 238 136 $Parchment
    for ($i = 0; $i -lt 76; $i++) {                        # fibers in the paper
        $fx = 8 + (I ((Hash01 (1000 + $i * 7)) * 230)); $fy = 8 + (I ((Hash01 (1000 + $i * 13)) * 132))
        Fill $paper $fx $fy (1 + $i % 3) 1 (Mix $Parchment $ParchmentShade 0.55)
    }
    $tabW = (Text-Width 'FIELD JOURNAL') + 10
    Fill $paper 13 8 ($tabW - 2) 13 $Leather                # the journal's leather tab
    Fill $paper 12 9 $tabW 11 $Leather
    Fill $paper 13 20 ($tabW - 2) 1 $LeatherDark
    Fill $paper 14 47 224 1 (Mix $Parchment $InkSoft 0.25) # a ruled line under the title
    Fill $paper 14 97 224 1 (Mix $Parchment $InkSoft 0.5)   # divider before the walk
    Add $shadowSvg
    Add (Layer-Svg $paper)
    Add (Text 'FIELD JOURNAL' 17 11 $Gilt)

    # A ribbon bookmark in the region's ink, and the [TAB] close hint beside it.
    $rib = New-Layer $W $H
    Fill $rib 222 2 6 20 $regionInk
    Fill $rib 222 2 1 20 (Mix $regionInk '#000000' 0.35)
    Fill $rib 222 22 2 1 $regionInk; Fill $rib 226 22 2 1 $regionInk; Dot $rib 222 23 $regionInk; Dot $rib 227 23 $regionInk
    AddCss '@keyframes ribbon { 0%, 100% { transform: rotate(-3deg) } 50% { transform: rotate(3deg) } }'
    Anim '.ribbon' 'ribbon' 5 'ease-in-out'
    Add (Layer-Svg $rib ' class="ribbon" style="transform-origin: 225px 2px"')
    $hintW = (Text-Width 'TAB') + 10 + (Text-Width 'close')
    Add (Key-Hint (216 - $hintW) 9 'TAB' 'close' $Ink $Parchment $InkSoft)

    Add (Text 'Singly Linked List' 14 25 $regionInk 2)
    Add (Text 'YOU ARE ON' 14 51 $InkSoft)
    $marks = New-Layer $W $H
    Icon $marks 14 61 '...#... ..###.. .#####. ####### .#####. ..###.. ...#...' $CurrentGold # the HUD's current-node diamond
    Add (Layer-Svg $marks ' class="pulse"')
    AddCss '@keyframes pulse { 0%, 100% { opacity: .75 } 50% { opacity: 1 } }'
    Anim '.pulse' 'pulse' 1.6 'ease-in-out'
    Add (Text 'next' 14 73 $regionInk)
    Add (Text '->' 58 73 $InkSoft)
    Add (Text 'previous' 14 85 $InkSoft 1 ' fill-opacity=".6"')
    Add (Text 'not stored in this list' 70 85 $InkSoft)

    # One group per node: what the journal says while the traveler stands there.
    for ($k = 1; $k -le 5; $k++) {
        Switch-Keyframes "st$k" $loop @((($k - 1) * 2.0), ($k * 2.0))
        Anim ".st$k" "st$k" $loop 'steps(1,end)'
        $sb = New-Object System.Text.StringBuilder
        [void]$sb.Append("<g class=`"st$k`">")
        [void]$sb.Append((Text "N$k - $($JournalNodes[$k - 1])" 25 61 $Ink))
        if ($k -lt 5) {
            [void]$sb.Append((Text "N$($k + 1) - $($JournalNodes[$k])" 70 73 $Ink))
            [void]$sb.Append((Text 'follow' (236 - (Text-Width 'follow')) 73 $regionInk))
        } else {
            [void]$sb.Append((Text 'NULL' 70 73 $InkNull))
            $stop = New-Layer $W $H
            Stop-Mark $stop (70 + (Text-Width 'NULL') + 4) 73 $InkNull $Parchment
            [void]$sb.Append((Layer-Svg $stop))
            [void]$sb.Append((Text 'next is NULL: this is the tail.' 14 133 $Ink))
        }
        [void]$sb.Append('</g>')
        Add $sb.ToString()
    }
    AddCss '.st2, .st3, .st4, .st5 { opacity: 0; }'

    # The walk from the head, read through next until NULL. A gold box marks the current node.
    Add (Text 'WALK FROM HEAD' 14 101 $InkSoft)
    $tokens = @('N1', '->', 'N2', '->', 'N3', '->', 'N4', '->', 'N5', '->', 'NULL')
    $x = 14
    $centers = @()
    $walk = New-Object System.Text.StringBuilder
    foreach ($t in $tokens) {
        $tw = Text-Width $t
        $color = $Ink
        if ($t -eq '->') { $color = $InkSoft } elseif ($t -eq 'NULL') { $color = $InkNull }
        if ($t -like 'N?') { $centers += $x + $tw / 2.0 }
        [void]$walk.Append((Text $t $x 112 $color))
        $x += $tw + 6
    }
    $stop = New-Layer $W $H
    Stop-Mark $stop ($x - 2) 112 $InkNull $Parchment
    $frames = New-Object System.Collections.ArrayList
    for ($k = 0; $k -lt 5; $k++) {
        $at = "transform:translateX($(N ($centers[$k] - 8))px)"
        if ($k -gt 0) { [void]$frames.AddRange(@(($k * 2.0), $at)) } else { [void]$frames.AddRange(@(0, $at)) }
        if ($k -lt 4) { [void]$frames.AddRange(@(($k * 2.0 + 1.7), $at)) } else { [void]$frames.AddRange(@($loop, $at)) }
    }
    Keyframes 'curbox' $loop $frames
    Anim '.curbox' 'curbox' $loop 'ease-in-out'
    AddCss ".curbox { transform: translateX($(N ($centers[0] - 8))px); }"
    Add ('<g class="curbox" opacity=".55">' + (Rect 1 110 14 11 $CurrentGold) + (Rect 0 111 16 9 $CurrentGold) + '</g>')
    Add $walk.ToString()
    Add (Layer-Svg $stop)
    $note = 'stopped: next == NULL (the end of the list)'
    if ((Text-Width $note) -gt 222) { Write-Warning "journal note is $((Text-Width $note)) px wide" }
    Add (Text $note 14 123 $InkSoft)
}
