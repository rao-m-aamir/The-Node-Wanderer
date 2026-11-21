# A small pixel font for the README art. Web fonts are blocked when an SVG is shown
# as an image, so every letter is drawn here: '#' is a pixel, rows are separated by
# spaces. Capitals are 7 rows tall; descenders (g, j, p, q, y) add two rows below.
# Letters have their own widths, and one empty column separates neighbors.

$Font = New-Object 'System.Collections.Generic.Dictionary[char,string[]]'

function G([char]$c, [string]$rows) {
    $r = $rows.Split(' ')
    foreach ($row in $r) {
        if ($row.Length -ne $r[0].Length) { throw "Glyph '$c' has rows of different widths" }
    }
    $Font[$c] = $r
}

G 'A' '.###. #...# #...# ##### #...# #...# #...#'
G 'B' '####. #...# #...# ####. #...# #...# ####.'
G 'C' '.###. #...# #.... #.... #.... #...# .###.'
G 'D' '####. #...# #...# #...# #...# #...# ####.'
G 'E' '##### #.... #.... ####. #.... #.... #####'
G 'F' '##### #.... #.... ####. #.... #.... #....'
G 'G' '.###. #...# #.... #.### #...# #...# .####'
G 'H' '#...# #...# #...# ##### #...# #...# #...#'
G 'I' '### .#. .#. .#. .#. .#. ###'
G 'J' '.### ...# ...# ...# ...# #..# .##.'
G 'K' '#...# #..#. #.#.. ##... #.#.. #..#. #...#'
G 'L' '#... #... #... #... #... #... ####'
G 'M' '#...# ##.## #.#.# #.#.# #...# #...# #...#'
G 'N' '#...# #...# ##..# #.#.# #..## #...# #...#'
G 'O' '.###. #...# #...# #...# #...# #...# .###.'
G 'P' '####. #...# #...# ####. #.... #.... #....'
G 'Q' '.###. #...# #...# #...# #.#.# #..#. .##.#'
G 'R' '####. #...# #...# ####. #.#.. #..#. #...#'
G 'S' '.###. #...# #.... .###. ....# #...# .###.'
G 'T' '##### ..#.. ..#.. ..#.. ..#.. ..#.. ..#..'
G 'U' '#...# #...# #...# #...# #...# #...# .###.'
G 'V' '#...# #...# #...# #...# #...# .#.#. ..#..'
G 'W' '#...# #...# #...# #.#.# #.#.# ##.## #...#'
G 'X' '#...# #...# .#.#. ..#.. .#.#. #...# #...#'
G 'Y' '#...# #...# .#.#. ..#.. ..#.. ..#.. ..#..'
G 'Z' '##### ....# ...#. ..#.. .#... #.... #####'

G 'a' '..... ..... .###. ....# .#### #...# .####'
G 'b' '#.... #.... ####. #...# #...# #...# ####.'
G 'c' '.... .... .### #... #... #... .###'
G 'd' '....# ....# .#### #...# #...# #...# .####'
G 'e' '..... ..... .###. #...# ##### #.... .###.'
G 'f' '..## .#.. #### .#.. .#.. .#.. .#..'
G 'g' '..... ..... .#### #...# #...# #...# .#### ....# .###.'
G 'h' '#.... #.... #.##. ##..# #...# #...# #...#'
G 'i' '# . # # # # #'
G 'j' '..# ... ..# ..# ..# ..# ..# #.# .#.'
G 'k' '#... #... #..# #.#. ##.. #.#. #..#'
G 'l' '# # # # # # #'
G 'm' '..... ..... ##.#. #.#.# #.#.# #.#.# #.#.#'
G 'n' '..... ..... #.##. ##..# #...# #...# #...#'
G 'o' '..... ..... .###. #...# #...# #...# .###.'
G 'p' '..... ..... ####. #...# #...# #...# ####. #.... #....'
G 'q' '..... ..... .#### #...# #...# #...# .#### ....# ....#'
G 'r' '.... .... #.## ##.. #... #... #...'
G 's' '..... ..... .#### #.... .###. ....# ####.'
G 't' '.#.. .#.. #### .#.. .#.. .#.. ..##'
G 'u' '..... ..... #...# #...# #...# #..## .##.#'
G 'v' '..... ..... #...# #...# #...# .#.#. ..#..'
G 'w' '..... ..... #...# #...# #.#.# #.#.# .#.#.'
G 'x' '..... ..... #...# .#.#. ..#.. .#.#. #...#'
G 'y' '..... ..... #...# #...# #...# #...# .#### ....# .###.'
G 'z' '..... ..... ##### ...#. ..#.. .#... #####'

G '0' '.###. #...# #..## #.#.# ##..# #...# .###.'
G '1' '.#. ##. .#. .#. .#. .#. ###'
G '2' '.###. #...# ....# ...#. ..#.. .#... #####'
G '3' '####. ....# ....# .###. ....# ....# ####.'
G '4' '...#. ..##. .#.#. #..#. ##### ...#. ...#.'
G '5' '##### #.... ####. ....# ....# #...# .###.'
G '6' '..##. .#... #.... ####. #...# #...# .###.'
G '7' '##### ....# ...#. ..#.. .#... .#... .#...'
G '8' '.###. #...# #...# .###. #...# #...# .###.'
G '9' '.###. #...# #...# .#### ....# ...#. .##..'

G ' ' '...'
G '.' '. . . . . . #'
G ',' '.. .. .. .. .. .# .# #.'
G ':' '. . . # . . #'
G ';' '.. .. .. .# .. .# .# #.'
G '-' '... ... ... ### ... ... ...'
G '=' '.... .... #### .... #### .... ....'
G '+' '..... ..#.. ..#.. ##### ..#.. ..#.. .....'
G '>' '... #.. .#. ..# .#. #.. ...'
G '<' '... ..# .#. #.. .#. ..# ...'
G '(' '.# #. #. #. #. #. .#'
G ')' '#. .# .# .# .# .# #.'
G '/' '..# ..# .#. .#. .#. #.. #..'
G "'" '# # . . . . .'
G '!' '# # # # # . #'
G '?' '.###. #...# ....# ...#. ..#.. ..... ..#..'
G '&' '.##.. #..#. #.#.. .#... #.#.# #..#. .##.#'
G '_' '..... ..... ..... ..... ..... ..... ..... #####'
G ([char]0x2192) '..... ..#.. ...#. ##### ...#. ..#.. .....' # "->" is drawn as one arrow

# The SVG shape of one glyph: one path, one rectangle per run of pixels in a row.
function Glyph-Def([int]$code) {
    $rows = $Font[[char]$code]
    $d = New-Object System.Text.StringBuilder
    for ($r = 0; $r -lt $rows.Length; $r++) {
        $row = $rows[$r]
        $c = 0
        while ($c -lt $row.Length) {
            if ($row[$c] -ne '#') { $c++; continue }
            $start = $c
            while ($c -lt $row.Length -and $row[$c] -eq '#') { $c++ }
            [void]$d.Append("M$start ${r}h$($c - $start)v1h-$($c - $start)z")
        }
    }
    return "<path id=`"g$code`" d=`"$($d.ToString())`"/>"
}

function Text-Chars([string]$s) { return $s.Replace('->', [string][char]0x2192).ToCharArray() }

# Width in pixels of a line of text at scale 1.
function Text-Width([string]$s) {
    $w = -1
    foreach ($ch in (Text-Chars $s)) {
        if (-not $Font.ContainsKey($ch)) { throw "No pixel glyph for '$ch' in: $s" }
        $w += $Font[$ch][0].Length + 1
    }
    return [Math]::Max(0, $w)
}

# <use> elements that spell $s from x = 0, with the top of the capitals at y = 0.
function Text-Uses([string]$s) {
    $sb = New-Object System.Text.StringBuilder
    $x = 0
    foreach ($ch in (Text-Chars $s)) {
        if (-not $Font.ContainsKey($ch)) { throw "No pixel glyph for '$ch' in: $s" }
        if ($ch -ne ' ') {
            [void]$script:Doc.Glyphs.Add([int]$ch)
            [void]$sb.Append("<use href=`"#g$([int]$ch)`" x=`"$x`"/>")
        }
        $x += $Font[$ch][0].Length + 1
    }
    return $sb.ToString()
}

# Pixel text with its top-left corner at (x, y).
function Text([string]$s, [double]$x, [double]$y, [string]$fill, [double]$scale = 1, [string]$attrs = '') {
    $t = "translate($(N $x) $(N $y))"
    if ($scale -ne 1) { $t += " scale($(N $scale))" }
    return "<g transform=`"$t`" fill=`"$fill`"$attrs>$(Text-Uses $s)</g>"
}

# Text centered on x.
function Text-Center([string]$s, [double]$cx, [double]$y, [string]$fill, [double]$scale = 1, [string]$attrs = '') {
    $x = [Math]::Round($cx - (Text-Width $s) * $scale / 2)
    return Text $s $x $y $fill $scale $attrs
}

# Defines text once so it can be stamped several times (for outlines). Returns its width.
function Def-Text([string]$id, [string]$s, [double]$scale = 1) {
    $t = ''
    if ($scale -ne 1) { $t = " transform=`"scale($(N $scale))`"" }
    AddDef "<g id=`"$id`"$t>$(Text-Uses $s)</g>"
    return (Text-Width $s) * $scale
}

function Stamp([string]$id, [double]$x, [double]$y, [string]$fill, [string]$attrs = '') {
    return "<use href=`"#$id`" x=`"$(N $x)`" y=`"$(N $y)`" fill=`"$fill`"$attrs/>"
}

# Defined text with a one-pixel dark outline and a drop shadow one pixel lower.
function Outlined([string]$id, [double]$x, [double]$y, [string]$fill, [string]$outline) {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.Append("<g fill=`"$outline`">")
    $dxs = @(-1, 0, 1, -1, 1, -1, 0, 1, -1, 0, 1)
    $dys = @(-1, -1, -1, 0, 0, 1, 1, 1, 2, 2, 2)
    for ($k = 0; $k -lt $dxs.Length; $k++) {
        [void]$sb.Append("<use href=`"#$id`" x=`"$(N ($x + $dxs[$k]))`" y=`"$(N ($y + $dys[$k]))`"/>")
    }
    [void]$sb.Append('</g>')
    [void]$sb.Append((Stamp $id $x $y $fill))
    return $sb.ToString()
}
