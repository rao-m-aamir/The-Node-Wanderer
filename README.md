<div align="center">

<img src="docs/readme/hero.svg" width="100%" alt="The Node Wanderer: a pixel-art traveler hops along a row of forest ledges. Warm light rises from the ledge that next points to, the other ledges fade into mist, and past the last ledge next is NULL.">

<img src="docs/readme/badges.svg" alt="C++17, Raylib 5.5, Windows and MSVC, art drawn in code">

</div>

<br>

**The Node Wanderer** is a short pixel-art adventure. A traveler with a hat, a red scarf, and a lantern crosses four places: a deep forest, drowned ruins under the moon, a night garden that never ends, and the cliffs where the world runs out.
Follow the warm light from ledge to ledge, carry a key's light home, walk a garden that loops back on itself, and mend a bridge over nothing.

It is also a game about linked lists. Each place is built from one list, the ledges you stand on are its nodes, and you can only step where a link leads.
Your field journal (<kbd>Tab</kbd>) shows how it works. The game is written in C++17 with Raylib 5.5.

<img src="docs/readme/divider.svg" width="100%" alt="">

## The journey

<table>
<tr>
<td width="50%"><img src="docs/readme/forest.svg" width="100%" alt="Region 1, The First Connection: the traveler stands on N1 in a forest. Warm light rises from N2, the node next points to, and N3 is still mist."></td>
<td width="50%"><img src="docs/readme/ruins.svg" width="100%" alt="Region 2, The Forgotten Return: the traveler stands on N3 in flooded ruins, holding the key's light. Cool light falls on N2 (previous) and warm light rises from N4 (next)."></td>
</tr>
<tr>
<td valign="top"><b>1. The First Connection</b> · singly linked list<br>A deep forest in late golden light. Only the ledge ahead will hold you: follow the warm light up through the trees, over a fallen log, and down to a sunlit arch. There is no way back, because each ledge only knows the one after it.</td>
<td valign="top"><b>2. The Forgotten Return</b> · doubly linked list<br>Flooded halls under a full moon. Climb a ruined tower to the Key Shrine, carry the key's light back down to the Old Hall to wake its brazier, then go on. Here every stone also remembers the one before it.</td>
</tr>
<tr>
<td width="50%"><img src="docs/readme/garden.svg" width="100%" alt="Region 3, The Endless Garden: the traveler stands on the tail, N4. Its next is the head, so warm light rises from N1, and a glowing current carries petals from the hollow in the tail back to the head."></td>
<td width="50%"><img src="docs/readme/edge.svg" width="100%" alt="Region 4, The Edge of Nothing: N3's next is NULL, so past the cliff there is only an amber glow and snapped ropes. After the check, append() creates N4 and the bridge assembles plank by plank, dawn breaks, and the traveler crosses."></td>
</tr>
<tr>
<td valign="top"><b>3. The Endless Garden</b> · circular singly linked list<br>A luminous night garden around an ancient tree. The last terrace flows back to the first: step into the hollow at the end of the path and a glowing current carries you home. Walk the circle once, and stop when you are back where you began.</td>
<td valign="top"><b>4. The Edge of Nothing</b> · NULL pointer safety<br>Cold cliffs before dawn. Past the last cliff the bridge has fallen, and there is nothing to stand on. Look before you step, then mend the way: the bridge is laid plank by plank, lanterns light along it, and dawn breaks over the peaks.</td>
</tr>
</table>

## How it teaches

- **You can only stand where a link leads.** The ledge you are on and the ledges its links point to are solid: `next` in every place, and `previous` too in the drowned ruins. Every other ledge sleeps in mist. If you fall, your lantern brings you back to the last checkpoint.
- **Light shows the way.** Warm light rises from the ledge that `next` points to, and a stream of motes drifts from the traveler toward it. In the ruins, cool light marks the way back through `previous`. In the garden, the current is the link from the tail back to the head. At the edge, amber light marks the link that is missing.
- **Two voices.** While you explore, the game speaks plainly about where you are and what to do. The first time you use each kind of link, a slip from the journal names it with one line of code, such as `current = current->next;`. Ordinary steps show no code.
- **The field journal reads the real list.** Press <kbd>Tab</kbd> to see the node you are on, where its links point, the whole list walked from its head to `NULL` (or back to the start), and what the place teaches.
- **Each place ends with a question.** Reaching the goal asks a short question in the journal, answers it, and opens the way on.
- **Color is never the only cue.** The current node is always a gold diamond, and `NULL` always has a stop mark.

<p align="center">
<img src="docs/readme/journal.svg" width="70%" alt="The field journal on Region 1, the singly linked list. It shows the node the traveler is on, where next points (NULL on the tail, with a stop mark), that previous is not stored in this list, and a walk from the head: N1 to N5, then NULL.">
</p>

<img src="docs/readme/divider.svg" width="100%" alt="">

## Play it

The build uses MSVC, CMake, and Ninja on Windows.
Open a "Developer PowerShell for VS" (or run `VsDevCmd.bat`), then:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\NodeWanderer.exe
```

The first configure step downloads Raylib, so it needs an internet connection.
The result is one self-contained executable: Raylib, the MSVC runtime, the art code, and the shaders are all built into `NodeWanderer.exe`, so it runs on another Windows PC without installing anything.
For a debug build, where a failed startup check also stops the program, use `-DCMAKE_BUILD_TYPE=Debug`.

| Action | Keys |
| --- | --- |
| Walk | <kbd>A</kbd> <kbd>D</kbd> or <kbd>←</kbd> <kbd>→</kbd> |
| Jump | <kbd>Space</kbd> |
| Interact, turn a note's page | <kbd>E</kbd> or <kbd>Enter</kbd> |
| Open or close the field journal | <kbd>Tab</kbd> |
| Pause and resume (the pause screen lists the controls) | <kbd>Esc</kbd> (<kbd>Q</kbd> quits while paused) |
| At the journey's end | <kbd>R</kbd> walks again, <kbd>Q</kbd> or <kbd>Esc</kbd> exits |

<img src="docs/readme/divider.svg" width="100%" alt="">

## Under the hood

Every platform is a `Node` that its region's list allocates with `new`. The game draws, collides with, and inspects these exact objects; there is no second, visual-only set of platforms (`src/Node.h`):

```cpp
struct Node {
    int id;                   // stable ID shown in the journal (N1, N2, ...)
    const char* label;        // short readable name
    Vector2 platformPosition; // world coordinates of the platform's top-left corner
    float platformWidth;
    Node* next;               // the following node, or nullptr
    Node* previous;           // used only by the doubly linked list; nullptr otherwise
};
```

Collision and drawing both ask the same function which platforms are solid, so what you see is what the links allow (`src/World.cpp`):

```cpp
bool isPlatformSolid(const Node* current, const Node* candidate, ListKind kind) {
    if (current == nullptr || candidate == nullptr) {
        return false;
    }
    if (candidate == current) {
        return true; // you can always stand on the node you are at
    }
    if (current->next != nullptr && candidate == current->next) {
        return true; // every list kind allows following next
    }
    if (kind == ListKind::Doubly && current->previous != nullptr && candidate == current->previous) {
        return true; // only a doubly linked list allows following previous
    }
    return false;
}
```

- **Landing is traversal.** Landing on a solid platform other than the current one moves `current` along the link that leads there (`followLink` in `src/Game.cpp`). Nothing is ever inferred from screen positions.
- **The places are separate lists.** An exit hands the traveler to the next list's head; the lists are never linked to each other.
- **The garden's stop rule.** The walk around the circular list stops when `current == start`. A limit of 12 steps (`MAX_LOOP_STEPS`) is only a safety fallback in case the start is never reached.
- **The edge's check.** With the journal open, pressing <kbd>E</kbd> on N3 runs `if (current->next == nullptr)` and then `append()`, which creates N4 "Bridge" at N3's edge. The bridge is drawn from that new node, and the list frees it like any other.
- **Startup checks.** The game checks every link of every list, and simulates every jump the route needs with the real movement code. The console reports 8 link checks and 17 jump checks; a failed one is reported there and stops a debug build.
- **Memory is checked.** `append()` allocates each node and `clear()` deletes it exactly once (a circular list breaks its cycle first). Platform art keeps node IDs and positions, never node pointers. When the game exits, the console prints `Nodes still allocated at shutdown: 0 (expected 0)`.

### Where to find things

| What | File |
| --- | --- |
| `Node` definition and the live-node counter | `src/Node.h` |
| List operations (`append`, `clear`) | `src/SinglyLinkedList.cpp`, `src/DoublyLinkedList.cpp`, `src/CircularSinglyLinkedList.cpp` |
| Region setup and layout, link checks, Traversal Rule A (`isPlatformSolid`) | `src/World.cpp` |
| Player-to-node traversal (`resolveLandings`, `followLink`), objectives, recovery, transitions, camera, the jump check | `src/Game.cpp` |
| Keyboard input, read once per frame | `src/Input.h` |
| Movement, the jump simulation, the traveler sprite, and its scarf and lantern | `src/Player.cpp` |
| Composing a frame: layers, platforms, traveler, water, lights, and the final pass | `src/GameScene.cpp` |
| HUD, first-time callouts, field journal, notes, pause, and the ending | `src/GameUi.cpp` |
| Shared art types and helpers: layers, wind, swaying pieces, ledges, link glows, lanterns, the guiding stream | `src/Art.h`, `src/ArtCommon.h`, `src/ArtCommon.cpp` |
| Painting on the CPU: masks, shading toward a light, noise, dithering | `src/Paint.h`, `src/Paint.cpp` |
| Render targets, the light map, and the bloom and color-grade shader | `src/Lighting.h`, `src/Lighting.cpp` |
| Each place's art, painted from its list's nodes, plus its live motion | `src/ForestArt.cpp`, `src/RuinsArt.cpp`, `src/GardenArt.cpp`, `src/EdgeArt.cpp` |
| Particles (dust, leaves, sparks, splashes) | `src/Effects.cpp` |
| Interface colors | `src/Theme.h` |
| The animated art in this README, and the script that draws it | `docs/readme/` |

## The art

The world is drawn into a 640x360 pixel canvas that is scaled up 2x, so the scenery, the platforms, the traveler, and the effects share one pixel size. Everything is original and made in code; there are no image files.

- **Painted when a place loads.** Each place's sky, far and near scenery, and every platform are painted pixel by pixel on the CPU when the traveler arrives (`src/Paint.cpp` and the four region files): shapes are shaded toward the light, textured with noise, and blended with dithered gradients, then kept as textures. Moving parts are drawn live every frame: water, fire, foliage in the wind, the current, the bridge, the gates, and particles.
- **Light.** A light map darkens everything away from light and lights the scene from above; lanterns, braziers, the moon, and the sun each glow; and a final shader pass adds gentle bloom, a color grade for each place, and a vignette. The shaders are embedded in the source. If they cannot be compiled, the game still runs without the final pass.
- **Four places, four moods.** A deep forest in late golden light, with a giant old tree, roots, mossy stone, light shafts, and drifting motes. Flooded ruins under a full moon, with broken arches, stone halls, braziers, and still water that reflects everything above it. A violet night garden around an ancient tree, with a glowing spring and the current that flows back under its roots. Cold cliffs before dawn above a deep ravine, with a snapped bridge, wind, and amber light rising from the gap.
- **Platforms belong to their place:** mossy banks, boulders, and a fallen log; ruined halls, a colonnade, a shrine, and old steps; a garden terrace, a giant moon-cap mushroom, a flower bed, and a root hollow; cliff shelves and the rebuilt rope bridge. Each one is painted from its node's position and width, and its top matches its collision exactly. There are no node boxes, labels, or pointer arrows in the world.
- **Motion with purpose.** Gusts of wind roll across canopies, grass, and banners; water ripples under the ruins and throws back the firelight; petals ride the garden's current; when the bridge is mended, the wind drops, lanterns light along it, birds take off, and dawn breaks.
- **The traveler** is a 14x24-pixel explorer with a hat, a red scarf, a backpack, and a lantern. They breathe when idle, walk in two steps, stretch into a jump, and bend their knees on landing. The scarf is a small chain of points that trails behind, streams when walking or in a gust, and ripples; the lantern swings like a pendulum when the traveler starts and stops.
- **Feedback:** dust at each footstep, jump, and landing; leaves, stone chips, or petals on arriving at a new ledge; a splash in the ruins' water; sparks when a checkpoint lantern lights; light gathering back into the traveler after a fall; and between places, a dark moment that names the place ahead before it fades in.
- **Interface:** in play, a small card with the place and what to do, and a hint for the journal and the pause screen; the controls appear at the start of the journey. Reading happens in an explorer's field journal: the journal itself, its notes, the pause cover, and the last spread at the journey's end.

The animated pictures in this README are documentation art, drawn in code as well. `docs/readme/make-art.ps1` paints them pixel by pixel with the game's palettes and the traveler sprite from `src/Player.cpp`, then writes them as SVG with CSS and SMIL animation. Each one loops. To redraw them after changing the art:

```powershell
powershell -ExecutionPolicy Bypass -File docs/readme/make-art.ps1          # all files
powershell -ExecutionPolicy Bypass -File docs/readme/make-art.ps1 -Only hero,edge
```

<br>

<p align="center">
<img src="docs/readme/footer.svg" width="100%" alt="Thanks for wandering. The traveler walks across a strip of forest floor at night.">
</p>
