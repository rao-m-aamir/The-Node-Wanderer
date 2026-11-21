<div align="center">

<img src="docs/readme/hero.svg" width="100%" alt="The Node Wanderer: a pixel-art traveler hops along a row of forest ledges. Warm light rises from the ledge that next points to, the other ledges fade into mist, and past the last ledge next is NULL.">

<img src="docs/readme/badges.svg" alt="C++17, Raylib 5.5, Windows and MSVC, art drawn in code">

</div>

<br>

**The Node Wanderer** is a small 2D platformer for learning linked lists, written in C++17 with Raylib 5.5.
Every platform is a real `Node` allocated with `new` and owned by a linked list.
The traveler moves along the list's links, and the field journal (<kbd>Tab</kbd>) reads those same links.

This is the Tutorial Stage: four regions, one list each.

<img src="docs/readme/divider.svg" width="100%" alt="">

## The journey

Each region is a separate list. Its exit gate starts you at the head of the next one.

<table>
<tr>
<td width="50%"><img src="docs/readme/forest.svg" width="100%" alt="Region 1, The First Connection: the traveler stands on N1 in a forest. Warm light rises from N2, the node next points to, and N3 is still mist."></td>
<td width="50%"><img src="docs/readme/ruins.svg" width="100%" alt="Region 2, The Forgotten Return: the traveler stands on N3 in flooded ruins, holding the key's light. Cool light falls on N2 (previous) and warm light rises from N4 (next)."></td>
</tr>
<tr>
<td valign="top"><b>1. The First Connection</b> · singly linked list<br>A forest. Follow <code>next</code> from the head to the tail. Going backward fails, because no <code>previous</code> link is stored.</td>
<td valign="top"><b>2. The Forgotten Return</b> · doubly linked list<br>Flooded ruins. Take the key from N4 (Key Shrine), carry its light back to N2 (Old Hall) through <code>previous</code> to light the brazier, then continue to the tail.</td>
</tr>
<tr>
<td width="50%"><img src="docs/readme/garden.svg" width="100%" alt="Region 3, The Endless Garden: the traveler stands on the tail, N4. Its next is the head, so warm light rises from N1, and a light runs along the great root from the tail back to the head."></td>
<td width="50%"><img src="docs/readme/edge.svg" width="100%" alt="Region 4, The Edge of Nothing: N3's next is NULL, so past the cliff there is only an amber glow and snapped ropes. After the check, append() creates N4 and the bridge assembles plank by plank, dawn breaks, and the traveler crosses."></td>
</tr>
<tr>
<td valign="top"><b>3. The Endless Garden</b> · circular singly linked list<br>A moonlit garden. The hollow in the tail's root is <code>tail->next</code>: walk into it and a great root carries you back to the head. Go around once; the walk stops when <code>current == start</code>, or at a safety limit of 12 steps.</td>
<td valign="top"><b>4. The Edge of Nothing</b> · NULL pointer safety<br>Cliffs above an abyss. N3's <code>next</code> is <code>nullptr</code>, so past its edge there is only an amber glow and snapped ropes. Open the journal and press <kbd>E</kbd> on N3: the game checks the pointer, calls <code>append()</code>, and the bridge N4 assembles from the new node.</td>
</tr>
</table>

## How it teaches

- **Only real links are solid (Traversal Rule A).** You can stand on the current node and on the nodes its links point to: `next` in every list, and also `previous` in the doubly linked list. Every other ledge fades into mist and cannot hold you. If you fall, you return to your last checkpoint.
- **Light shows the links.** Warm light rises from the ledge that `current->next` points to. In Region 2, cool light falls on the ledge that `current->previous` points to. A small light near the traveler drifts toward the link to follow.
- **Every move shows its code.** After each step the corner shows the line that ran, for example `current = current->next;   N2 -> N3`.
- **The field journal reads the real list.** Press <kbd>Tab</kbd> to see the node you are on, where its links point, and a walk from the head that ends at `NULL` or back at the head.
- **Color is never the only cue.** The current node is always a gold diamond, and `NULL` always has a stop mark.
- **Memory is checked.** `append()` allocates each node with `new` and `clear()` deletes it. When the game exits, the console prints `Nodes still allocated at shutdown: 0`.

<p align="center">
<img src="docs/readme/journal.svg" width="70%" alt="The field journal on Region 1, the singly linked list. It shows the node the traveler is on, where next points (NULL on the tail, with a stop mark), that previous is not stored in this list, and a walk from the head: N1 to N5, then NULL.">
</p>

<img src="docs/readme/divider.svg" width="100%" alt="">

## Play it

The build uses MSVC, CMake, and Ninja on Windows.
Open a "Developer PowerShell for VS" (or run `VsDevCmd.bat`), then:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
.\build\NodeWanderer.exe
```

The first configure step downloads Raylib, so it needs an internet connection.

| Action | Keys |
| --- | --- |
| Move | <kbd>A</kbd> <kbd>D</kbd> or <kbd>←</kbd> <kbd>→</kbd> |
| Jump | <kbd>Space</kbd> |
| Interact, continue a message | <kbd>E</kbd> or <kbd>Enter</kbd> |
| Open or close the field journal | <kbd>Tab</kbd> |
| Pause and resume | <kbd>Esc</kbd> (<kbd>Q</kbd> quits while paused) |
| On the final summary | <kbd>R</kbd> plays again, <kbd>Q</kbd> or <kbd>Esc</kbd> exits |

<img src="docs/readme/divider.svg" width="100%" alt="">

## Under the hood

There is one structure for platforms. The game draws, collides with, and inspects these exact objects (`src/Node.h`):

```cpp
struct Node {
    int id;                   // stable ID shown in the game (N1, N2, ...)
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

### Where to find things

| What | File |
| --- | --- |
| `Node` definition | `src/Node.h` |
| List operations (`append`, `clear`) | `src/SinglyLinkedList.cpp`, `src/DoublyLinkedList.cpp`, `src/CircularSinglyLinkedList.cpp` |
| Region setup, link checks, Traversal Rule A (`isPlatformSolid`) | `src/World.cpp` |
| Player-to-node traversal (`resolveLandings`, `followLink`), objectives, recovery, transitions | `src/Game.cpp` |
| Keyboard input, read once per frame | `src/Input.h` |
| Player movement and the traveler pixel sprite | `src/Player.cpp` |
| How a frame of the world is drawn, and the wisp hint | `src/GameScene.cpp` |
| HUD, field journal (inspector), notes, pause, summary | `src/GameUi.cpp` |
| Shared art helpers: platform outlines, materials, mist, link glows, lanterns | `src/ArtCommon.h`, `src/ArtCommon.cpp` |
| Region art (platforms from nodes, plus decoration) | `src/ForestArt.cpp`, `src/RuinsArt.cpp`, `src/GardenArt.cpp`, `src/EdgeArt.cpp` |
| Particles (dust, leaves, sparks) | `src/Effects.cpp` |
| Interface colors | `src/Theme.h` |
| The animated art in this README, and the script that draws it | `docs/readme/` |

## The art

The whole world is drawn into a 640x360 pixel canvas that is scaled up 2x, so the scenery, the platforms, the traveler, and the effects share one pixel size. All art is drawn in code with Raylib shapes; there are no image assets.

- **Platforms are natural forms that fit each place:** forest cliffs, a mossy boulder, a fallen log; ruined masonry, a broken arch, a column capital, a shrine, old stairs; a garden terrace, a moon-cap mushroom, a planter, a root knot; rock cliffs and the rebuilt rope bridge. Each one is drawn only from its node's position and width, and its look is chosen from the node's ID. The top row of every platform matches its collision exactly. There are no node boxes, labels, or pointer arrows in the world.
- **Each region has its own palette and light:** golden forest afternoon, blue-hour ruins with braziers, a violet night garden with a glowing spring, and cold cliffs before dawn. Fixing the NULL link in Region 4 brings the dawn.
- **Depth:** every region has far, middle, and near layers, mist between them, and a soft vignette. Near layers stay below and beside the route.
- **The traveler:** a 14x24-pixel explorer with a hat, a red scarf, a backpack, and a lantern, with idle breathing, a two-step walk, a jump stretch, falling and landing poses, and a contact shadow.
- **Feedback:** dust when jumping and landing; leaves, chips, or petals when arriving on a new node; sparks when a checkpoint lantern lights; light gathering back into the traveler after a fall; and a fade with the region's name between regions.
- **Interface:** small dark cards in play (the region and objective, the list as marks walked from the head, the last line of pointer code, and the controls), and an explorer's field journal for reading: the inspector, the notes, the pause cover, and the final summary.

The animated pictures in this README are drawn in code as well. `docs/readme/make-art.ps1` paints them pixel by pixel with the game's palettes and the traveler sprite from `src/Player.cpp`, then writes them as SVG with CSS and SMIL animation. Each one loops, and holds still when the system asks for reduced motion. To redraw them after changing the art:

```powershell
powershell -ExecutionPolicy Bypass -File docs/readme/make-art.ps1          # all files
powershell -ExecutionPolicy Bypass -File docs/readme/make-art.ps1 -Only hero,edge
```

<br>

<p align="center">
<img src="docs/readme/footer.svg" width="100%" alt="Thanks for wandering. The traveler walks across a strip of forest floor at night.">
</p>
