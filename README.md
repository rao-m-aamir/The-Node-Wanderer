# The Node Wanderer: Tutorial Stage

A small 2D platformer for learning linked lists, written in C++17 with Raylib 5.5.
Every platform is a real `Node` allocated with `new` and owned by a linked list.
The player moves along the list's links, and the field journal (Tab) reads those same links.

## Build and run (Windows, MSVC)

Open a "Developer PowerShell for VS" (or run `VsDevCmd.bat`), then:

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
.\build\NodeWanderer.exe
```

The first configure step downloads Raylib automatically, so you need an internet connection for it.
When the game exits, the console prints `Nodes still allocated at shutdown: 0`, which shows every node was freed.

## Controls

| Action | Keys |
| --- | --- |
| Move | A / D or Left / Right |
| Jump | Space |
| Interact / continue a message | E or Enter |
| Toggle the field journal (the node inspector) | Tab |
| Pause / resume | Escape (Q quits while paused) |

## The four regions

1. **The First Connection (singly linked list):** a forest. Follow `next` from the head to the tail. Going backward fails, because no `previous` link is stored.
2. **The Forgotten Return (doubly linked list):** flooded ruins. Take the key from N4 (Key Shrine), carry its light back to N2 (Old Hall) through `previous` to light the brazier, then continue to the tail.
3. **The Endless Garden (circular singly linked list):** a moonlit garden. The hollow in the tail's root is `tail->next`; walking into it, a great root carries you back to the head. Go around once: the traversal stops when `current == start`, or at a safety limit of 12 steps.
4. **The Edge of Nothing (NULL pointer safety):** cliffs above an abyss. N3's `next` is `nullptr`, so past its edge there is only an amber glow and snapped ropes. Open the journal and press E on N3: the game checks the pointer and calls `append()`, and the bridge N4 assembles from the new node.

## Visual guide

The whole world is drawn into a 640x360 pixel canvas that is scaled up 2x, so the scenery, the platforms, the traveler, and the effects share one pixel size. All art is drawn in code with Raylib shapes; there are no image assets.

- **Platforms are natural forms that fit each place:** forest cliffs, a mossy boulder, a fallen log; ruined masonry, a broken arch, a column capital, a shrine, old stairs; a garden terrace, a moon-cap mushroom, a planter, a root knot; rock cliffs and the rebuilt rope bridge. Each one is drawn only from its node's position and width, and its look is chosen from the node's ID. The top row of every platform matches its collision exactly; roots, moss, and supports only hang below. There are no node boxes, labels, or pointer arrows in the world.
- **Links are taught through movement and light:**
  - Warm light rises from the platform that `current->next` points to.
  - In Region 2, cool light falls on the platform that `current->previous` points to.
  - A small light near the traveler drifts toward the link to follow.
  - Platforms that are not solid right now fade into mist.
  - Region 3's great root runs from the real tail to the node its `next` points to.
- **Each region has its own palette and light:** golden forest afternoon, blue-hour ruins with braziers, a violet night garden with a glowing spring, and cold cliffs before dawn. Fixing the NULL link in Region 4 brings the dawn.
- **Depth:** every region has far, middle, and near layers, mist between them, and a soft vignette. Near layers stay below and beside the route.
- **The traveler:** a 14x24-pixel explorer with a hat, a red scarf, a backpack, and a lantern. They have idle breathing, a two-step walk, a jump stretch, falling and landing poses, and a contact shadow.
- **Feedback:** dust when jumping and landing, leaves, chips, or petals when arriving on a new node, sparks when a checkpoint lantern lights, light gathering back into the traveler after a fall, and a fade with the region's name between regions.
- **Interface:**
  - Small dark cards in play: the region and objective (top left), the list as marks walked from the head (top right), the last line of pointer code, and the controls.
  - The current node is always a gold diamond, and NULL always has a stop mark, so color is never the only cue.
  - Reading happens in an explorer's field journal: the inspector, the notes (each kind has its own mark), the pause cover, and the final summary.

## Where to find things

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
