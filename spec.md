# The Node Wanderer — Tutorial Stage Specification

## Project summary

Build a small, playable 2D side-scrolling platformer tutorial in C++ with Raylib. The player explores one connected world made of four regions. Platforms are the nodes in the program’s linked-list data structures: the player traverses those same nodes, and an in-game inspector reads those same nodes.

Keep the implementation beginner-readable, with explicit pointer changes and clear ownership.

## Goals

- Make singly, doubly, and circular singly linked lists tangible and explorable.
- Demonstrate that each visible platform maps to one dynamically allocated `Node`.
- Use actual list links to determine valid platform traversal and inspector output.
- Teach safe handling of a null pointer through a recoverable hazard and explicit checks.
- Deliver a complete tutorial stage without requiring an elaborate engine.

## Scope and non-goals

### In scope

- One playable tutorial stage with four connected regions.
- A simple character, walking, jumping, falling, collisions, and recovery.
- Region introductions, contextual explanations, traversal objectives, and completion feedback.
- A live node inspector that shows values and link relationships from the active region’s actual list.
- Region setup and deterministic cleanup of dynamically allocated nodes.
- Basic Raylib drawing and keyboard input.

### Out of scope

- Multiple stages, save files, accounts, networking, or a level editor.
- A general-purpose physics engine or game framework.
- Arbitrary list editing by the player, templates, smart-pointer frameworks, or complex inheritance.
- Production art, audio, controller support, or extensive settings menus.
- A separate visual-only graph or list used in place of backend nodes.

## Finalized design decisions

- **Perspective:** 2D side view with a side-scrolling camera.
- **Movement:** Walk, jump, and fall between platforms.
- **World:** One connected world divided into four tutorial regions.
- **Learning:** Hybrid: contextual explanations plus hands-on traversal puzzles.
- **Failure:** Recoverable. A fall or NULL hazard returns the player to a safe checkpoint and explains what happened.
- **Data model:** Platforms correspond directly to dynamically allocated `Node` objects.
- **Traversal:** Platform traversal follows the active list’s links. The inspector displays those same links.
- **Code style:** Small files, plain structs, and focused functions or classes.

## Resolved implementation decisions

- **Traversal Rule A (link-gated solidity):** a platform is solid only if it is the current node, `current->next`, or (doubly list only) `current->previous`. Landing on a solid platform other than the current one sets `current` to it. One function, `isPlatformSolid` in `World.cpp`, is used by both collision and drawing. Platforms are one-way: the player lands on them only from above.
- **Circular wrap:** a root hollow on the tail of Region 3 is the visible form of `tail->next`. Walking into it reads `tail->next`, a short passage carries the traveler along a great root that is drawn from the real tail to the node its `next` points to, and then the link is followed. Region 3 fits on one screen and the camera frames all of it.
- **Region transitions:** each region ends at an exit (arch, doorway, moon gate, dawn gate) that opens when its objective is complete. An exit is not a pointer. The screen fades, shows the next region's name, and sets `current = nextList.getHead()`; the lists are never linked to each other.
- **Region 2 puzzle:** reaching N4 takes the key's light, which the traveler carries back through `previous` to N2, where it lights a brazier. These are visual responses to objective steps, not extra links.
- **Region 4:** a singly list N1 → N2 → N3, with `N3->next == nullptr` and an amber hazard gap after N3. With the journal open, pressing E on N3 runs `if (current->next == nullptr)` and then calls `append()`, which creates N4 "Bridge" right at N3's edge. The bridge is drawn from that new node and assembles over about 1.6 seconds. The list owns the bridge node, so `clear()` frees it. No node is ever faked.
- **Inspector:** a field journal that shows the current node's real `next` (and `previous` in Region 2), then the whole active list walked from the head. It has no separate selection mechanism.
- **Checkpoints:** the checkpoint starts at the region head and moves to the current node at each objective step. Region 3 always restarts from the head, and its loop counter resets on a fall.
- **Checkpoint prompts:** shown as a question page followed by an answer page.
- **Replay:** clears the non-owning pointers, clears all lists, and rebuilds the stage.
- **Escape:** `SetExitKey(KEY_NULL)` makes Escape pause the game. Q quits.
- **Memory check:** a live-node counter, `g_liveNodeCount`, is printed at shutdown and must be 0.
- **Build:** CMake with `FetchContent` for Raylib 5.5, C++17. The project lives in the workspace root, not a `node-wanderer/` subfolder.
- **Art:** Raylib shapes only (no image assets), drawn into a 640x360 pixel canvas scaled 2x for every region. Each region has its own art file (`ForestArt.cpp`, `RuinsArt.cpp`, `GardenArt.cpp`, `EdgeArt.cpp`). Platforms are natural forms drawn only from node position and width; decoration has its own data and never becomes a platform. The world has no labels or pointer arrows: the next platform glows (and, in Region 2, previous glows cool), platforms that are not solid fade into mist, and the details live in the journal. The camera stays within the active region.
- **Input:** the keyboard is read once per frame into an `InputFrame` (`Input.h`), which gameplay receives as plain values.

## Gameplay loop

1. Enter a region and receive a brief explanation of its structure.
2. Inspect nodes and identify the available link or direction.
3. Walk and jump between platforms.
4. The game checks whether the destination is reachable through the active node’s link.
5. Reaching the objective triggers a short learning checkpoint and opens the connection to the next region.
6. Falling or encountering a NULL hazard shows an explanation and restores the player at a safe checkpoint.
7. Completing all regions shows a summary and offers replay or exit.

## Controls

Show controls in the game UI.

| Action | Suggested input |
| --- | --- |
| Move left / right | A / D or Left / Right |
| Jump | Space |
| Interact / continue explanation | E or Enter |
| Toggle node inspector | Tab |
| Pause / resume | Escape |

Use configurable movement speed, jump impulse, and gravity. Do not allow another jump while airborne.

## World and tutorial regions

Place four clearly marked regions consecutively in one continuous side-scrolling world. Use visual boundaries, small gaps, or gates, while preserving a clear route forward when each objective is complete.

### Region 1 — Singly Linked List: forward traversal

- Build a linear chain of at least four nodes. Each node’s `next` points to the following node; the tail’s `next` is `nullptr`.
- Explain that traversal follows `next` in one direction and the structure has no direct backward link.
- Puzzle: start at the head and visit nodes in order to reach a checkpoint.
- Checkpoint prompt: “Which link tells you where to go next?”
- If the player tries to traverse backward, explain that no previous link is stored. Do not fabricate a backward data link.

### Region 2 — Doubly Linked List: forward and backward traversal

- Build a chain of at least four nodes with consistent `next` and `previous` links. Head `previous` and tail `next` are `nullptr`.
- Explain that traversal is available in both directions.
- Puzzle: visit a target, follow `previous` to an earlier node, then continue.
- Show both links for the selected node in the inspector. At an endpoint, explain that the corresponding pointer is null.

### Region 3 — Circular Singly Linked List: loop traversal

- Build a circular chain of at least four nodes. Each `next` points to the following node, and tail `next` points back to head.
- Explain that traversal loops and does not terminate at a null `next`.
- Puzzle: follow links until returning to the starting node. Stop by node identity or a bounded step count.
- Explain that circular traversal needs an explicit stopping condition to avoid an infinite loop.

### Region 4 — NULL pointer hazards: checks and recovery

- Present a visible broken-link or missing-platform hazard representing a null pointer. Never dereference null or treat it as a valid destination.
- Explain that `nullptr` means there is no node; check before reading a node or following a link.
- Puzzle: inspect a link, identify it as null, and choose a safe route or activate a recovery bridge.
- If the player enters the marked hazard, show a concise explanation and return them to the region checkpoint with a valid active node.
- Model a safe code check, never undefined behavior or a crash.

## Data structures and ownership

Keep the central structure explicit. A suitable starting point is:

```cpp
struct Node {
    int id;
    const char* label;
    Vector2 platformPosition; // World coordinates; this node defines its platform.
    float platformWidth;
    Node* next;
    Node* previous; // Used by the doubly linked list; nullptr otherwise.
};
```

Raylib’s `Vector2` can be stored directly in the node so platform position is not duplicated in a visual-only platform model. Static string labels are enough for this stage; `std::string` is also reasonable if needed.

Use a small owning list class for each structure, or distinct clear functions, with focused operations:

- `SinglyLinkedList`: `head`, `append`, `getHead`, `clear`.
- `DoublyLinkedList`: `head`, `tail`, `append`, `getHead`, `clear`.
- `CircularSinglyLinkedList`: `head`, `tail`, `append`, `getHead`, `clear`.

Each list owns every node it creates. Allocate with `new` in append/build operations and delete each owned node exactly once. Give each list a destructor that calls `clear()`, or use one explicit cleanup owner; do not combine strategies in a way that double-frees. Disable copying of owning list objects unless copy behavior is deliberately implemented. Keep ownership and link manipulation visible to beginners.

Implement operations plainly:

- Append allocates a node and updates the previous tail link or links.
- Forward traversal follows `next` only after checking it.
- Backward traversal follows `previous` only after checking it.
- Circular traversal stops on returning to its start or reaching a safe maximum step count.
- Cleanup stores the next pointer before deleting the current node. Circular cleanup must break the cycle or use a bounded count so it terminates.

Avoid generic abstractions that hide pointer changes. Add short comments to explain meaningful link assignments.

## Single source of truth: node, platform, traversal, inspector

The active region’s list is the gameplay source of truth. There must be no separate platform-node graph.

- Draw every platform from fields on its real `Node`.
- Keep non-owning `Node*` references to the active head and player’s current node. They must point into the owning region list.
- For a valid traversal, obtain the destination by reading `currentNode->next` or `currentNode->previous` after a null check.
- A successful landing updates the player’s current node only if the destination is reachable through the active structure’s permitted link. Never infer adjacency from screen coordinates.
- The inspector walks from the active head and reads actual IDs, labels, and links. Display targets as stable IDs or labels, not raw addresses.
- Rendering may iterate the list to draw nodes, but must not create another set of nodes. Temporary traversal pointers are non-owning.
- Switching regions selects that region’s list and head. Do not invent links between distinct structures unless explicitly teaching such a link.
- A shared `Node` type is fine. For non-doubly-linked regions, `previous` stays `nullptr`.

## Player and platform interaction

Use a small `Player` struct with position, velocity, size, grounded state, and a non-owning pointer to the current node. Keep physics and list ownership separate.

- Use world coordinates for node positions and player movement.
- Use Raylib rectangle checks for landing and hazard detection.
- Apply gravity each frame; apply the jump impulse only while grounded.
- Follow the player horizontally with a camera clamped to world bounds.
- Derive collision geometry from the node’s `platformPosition` and `platformWidth`.
- Accept a landing as a list traversal only if its destination is reachable from the current node via an allowed link.
- Choose one clear traversal rule, such as an interaction at a neighboring platform or a constrained landing rule, document it, and use it consistently.
- On an ordinary fall, restore the last safe checkpoint. In the null-hazard region, explain the missing link as well.
- Null must not represent “the player is on a platform.” Before gameplay resumes from recovery, assign a valid checkpoint node.

## Learning checkpoints and interface

Keep explanations short and actionable; do not obscure the player or inspector. Include:

- Region name and current objective.
- A brief contextual explanation upon entering each structure.
- A toggleable live inspector showing node ID/label and relevant `next` / `previous` target or “NULL”.
- A checkpoint prompt after each puzzle explaining the concept practiced.
- A recovery message explaining what happened and a safe action.
- A completion screen summarizing singly linked, doubly linked, circular, and null-check concepts.

The inspector reads actual backend links. It may highlight the current node and linked destinations but must not synthesize links.

## Region construction and cleanup

Construct each region explicitly in a stage or world setup function. Give nodes stable IDs, readable labels, and world-space platform positions. Keep the layout simple enough that list order is apparent.

Recommended setup:

1. Create the region’s owning list.
2. Append nodes in intended traversal order with positions and labels.
3. For the circular region, connect tail to head after construction.
4. Add simple debug checks for expected boundary links.
5. Set the player’s region and current node to that list’s head.

Keep all four lists alive for the stage lifetime, or clear a region only after moving all non-owning references elsewhere. Do not delete nodes while the player, renderer, inspector, or checkpoint still references them. Destroy each owning list once at shutdown.

## Memory safety

- Check pointers for `nullptr` before dereferencing.
- Never delete through the player, renderer, inspector, or other non-owning pointer.
- Delete every allocated node exactly once.
- Never use node pointers after their owning list is cleared or destroyed.
- Give circular traversal and cleanup explicit termination conditions.
- Reset player and checkpoint references before destroying their owner.
- Do not represent a missing destination with a fake node.
- Keep rendering and inspector reads within each owner’s lifetime.

## Suggested folder structure

Keep the project small; merge files if that makes the first implementation clearer.

```text
node-wanderer/
  README.md
  CMakeLists.txt
  src/
    main.cpp
    Game.h
    Game.cpp
    Node.h
    SinglyLinkedList.h
    SinglyLinkedList.cpp
    DoublyLinkedList.h
    DoublyLinkedList.cpp
    CircularSinglyLinkedList.h
    CircularSinglyLinkedList.cpp
    Player.h
    Player.cpp
    World.h
    World.cpp
```

Keep Raylib drawing and input in game/player/world code. Keep list operations independent of drawing where practical, while allowing `Node` to hold the small amount of platform layout data required for the single-source-of-truth design.

## Implementation phases

1. Build a minimal Raylib app: window, background, input, clean close.
2. Implement `Node` and the singly linked list: append, traverse, clear, and draw by iterating the list.
3. Add player movement, gravity, jumping, landing, camera follow, and checkpoint recovery.
4. Complete Region 1 with forward traversal and contextual teaching.
5. Add the doubly linked list and Region 2 in both directions.
6. Add circular list traversal and Region 3 with an explicit stop condition.
7. Add Region 4 with safe null checks, a marked hazard, recovery, and explanation.
8. Add the live inspector and checkpoints; ensure both use the actual active list.
9. Polish transitions, completion screen, control hints, and visual hierarchy.

Keep the project buildable at each phase and the data-structure operations easy to find.

## Acceptance criteria

- The project builds and starts as a Raylib C++ application.
- The player walks, jumps, falls, lands on platforms, and uses a horizontal side camera.
- The world has four connected, recognizable regions in the specified order.
- Singly linked traversal follows only `next`; tail `next` is null.
- Doubly linked traversal follows both links; head `previous` and tail `next` are null.
- Circular traversal returns to head and ends only via an explicit stopping rule.
- The null-hazard puzzle checks safely, explains null, and allows recovery without crashing.
- Every visible platform is drawn from a real dynamically allocated `Node` owned by its list.
- Platform traversal and live inspection read the same nodes and links.
- There is no parallel fake platform graph or inspector-only link representation.
- Player, renderer, and inspector hold only non-owning node pointers.
- All allocated nodes are released once, including circular-list nodes, with no dangling references during play or shutdown.
- Names, focused operations, and comments make pointer updates understandable.
- A beginner can find and understand the node definition, list operations, region setup, and player-to-node interaction without navigating a large framework.

## Instructions to the coding agent

Start with the smallest buildable Raylib project and implement the data model before visual polish. Keep the tutorial playable at each phase. Do not replace linked lists with arrays, vectors, coordinate-based adjacency, or a visual mock graph for gameplay. Arrays or vectors may support unrelated fixed UI data, but platform traversal and the inspector must consult actual nodes and links.

Use plain C++ and explicit pointer checks. Add short comments where links are connected, where circular traversal stops, and where nodes are freed. Prefer a few small, understandable operations over a generic data-structure framework. Once the structures, gameplay, recovery, and inspector work together, make modest visual improvements that preserve readability and the single-source-of-truth design.
