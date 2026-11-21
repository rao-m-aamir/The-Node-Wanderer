# The Node Wanderer: Tutorial Stage

A small 2D platformer for learning linked lists, written in C++17 with Raylib 5.5.
Every platform is a real `Node` allocated with `new` and owned by a linked list.
The player moves along the list's links, and the inspector (Tab) reads those same links.

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
| Toggle node inspector | Tab |
| Pause / resume | Escape (Q quits while paused) |

## The four regions

1. **Singly linked list:** follow `next` from the head to the tail. Going backward fails, because no `previous` link is stored.
2. **Doubly linked list:** go forward to N4, use `previous` to go back to N2, then continue to the tail.
3. **Circular singly linked list:** the tail's portal is `tail->next`, which leads to the head. Go around the loop once. The traversal stops when `current == start`, or at a safety limit of 12 steps.
4. **NULL pointer hazards:** N3's `next` is `nullptr`, shown as an amber gap with a broken bridge. Open the inspector and press E on N3. The game checks the pointer and calls `append()`, which creates a new node N4 across the gap. Jump onto it to reach the exit.

## Visual design

Region 1 is the polished vertical slice. It is drawn into a 640x360 pixel canvas that is scaled up 2x, so the scenery, the ledges and the traveler share one pixel size. All art is drawn in code with Raylib shapes; there are no image assets.

- **Ledges are natural forms:** a grassy cliff, a mossy boulder, a fallen log, an old ruin and an arch cliff. Each one is drawn only from its node's position and width, and its look is chosen from the node's ID. The world has no node boxes, labels or pointer arrows.
- **The links are taught through movement:**
  - The ledge that `current->next` points to glows softly.
  - A firefly near the traveler drifts toward it.
  - Ledges that are not solid right now fade into mist, including the ones behind you, because a singly list has no way back.
- **Interface:** a small top-left card shows the region and the objective. A top-right row of pips shows the list walked from the head: gold is the current node, filled pips are visited, and the row ends at `NULL`. The code line (`current = current->next;`), the inspector (Tab) and short message cards explain the pointers.
- **Feedback:** landing on a new node kicks up leaves and light, the checkpoint lantern lights up with sparks, and the exit arch is blocked by vines until the objective is done.

Regions 2-4 still use the earlier diagram-style look (platform badges and link arrows). They share the new traveler sprite and interface. The camera stays inside the active region.

## Where to find things

| What | File |
| --- | --- |
| `Node` definition | `src/Node.h` |
| List operations (`append`, `clear`) | `src/SinglyLinkedList.cpp`, `src/DoublyLinkedList.cpp`, `src/CircularSinglyLinkedList.cpp` |
| Region setup, link checks, Traversal Rule A (`isPlatformSolid`), Regions 2-4 platform and link drawing | `src/World.cpp` |
| Region 1 forest art: ledges, exit arch, lantern, background and foreground layers | `src/ForestArt.cpp` |
| Which renderer draws the active region, the firefly hint, the progress pips | `src/GameScene.cpp` |
| Player movement and the traveler pixel sprite | `src/Player.cpp` |
| Player-to-node traversal (`resolveLandings`, `followLink`), objectives, recovery, HUD, inspector | `src/Game.cpp` |
| Region colors | `src/Theme.h` |
| Regions 2-4 background scenery (decoration only) | `src/Scenery.cpp` |
