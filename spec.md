# The Node Wanderer — Tutorial Stage Specification

## Project summary

Build a small, playable 2D side-scrolling fantasy platformer in C++ with Raylib, with its own music and sound. A traveler with a hat, a red scarf, a backpack, and a lantern crosses six places: a deep forest, flooded ruins, a night garden, the cliffs at the edge of the world, a lighthouse on the sea cliffs, and a harbor. Underneath the adventure, every place is a linked list: the ledges, stones, and boats the traveler stands on are the list's nodes, the ways between them are its links, and a field journal reads those same nodes. The last two places are a stack and a queue built from linked nodes, so their lists grow and shrink while the traveler plays.

The game should feel inviting and alive first. The data structures are taught through what the player does and sees, and explained in detail in the field journal.

Keep the implementation beginner-readable, with explicit pointer changes and clear ownership.

## Goals

- Make singly, doubly, and circular singly linked lists tangible and explorable.
- Use actual list links to determine valid platform traversal and journal output.
- Teach safe handling of a null pointer through a recoverable hazard and explicit checks.
- Show a stack and a queue as linked lists that change only at their ends: `push` and `pop` at the top; `enqueue` at the rear and `dequeue` at the front. Their nodes are created and deleted during play.
- Order the places so that each adds one idea to the last: `next`, then `previous`, a loop, a null check and a first insertion, insertion and removal at one end, then at both ends.
- Give the tutorial the look, sound, and feel of a finished fantasy platformer: layered, lit, animated scenes, a memorable traveler, a route that flows through each place, and a gentle score and soundscape for each place.
- Deliver a complete tutorial stage without requiring an elaborate engine.

## Scope and non-goals

### In scope

- One playable tutorial stage with six connected regions.
- A traveler who walks, jumps, falls, lands, and recovers, with expressive animation.
- Original visual assets and a complete art pass across all six regions: layered scenery, platforms that belong to their environment, lighting, reflections, particles, and animation. Assets may be painted by code at load time, drawn as pixel sprites in code, or added as image files.
- Region introductions, short teaching moments, objectives, transitions, and an ending.
- Music, ambience, and sound effects for the whole journey, from verified open-license sources (or original cues), with music and sound volume controls and a mute option.
- A field journal that shows values and links from the active region's actual list.
- Region setup and deterministic cleanup of dynamically allocated nodes.
- Keyboard input.

### Out of scope

- Multiple stages, save files, accounts, networking, or a level editor.
- A general-purpose physics engine, game framework, or complex rendering framework.
- Arbitrary list editing by the player (the operations the player runs are fixed ones at fixed places: the bridge's `append`, the stair's `push` and `pop`, and the harbor's `enqueue`, with `dequeue` when the traveler leaves a boat), templates, smart-pointer frameworks, or complex inheritance.
- Controller support, extensive settings menus, or saved settings.
- Voice acting, an audio middleware engine, or a dynamic music system beyond crossfades between loops.
- A separate visual-only graph or list used in place of backend nodes.

## Finalized design decisions

- **Perspective:** 2D side view with a side-scrolling camera.
- **Movement:** Walk, jump, and fall between platforms.
- **World:** One connected world divided into six tutorial regions, in the order a student meets the ideas: singly, doubly, and circular lists, NULL safety, then a stack and a queue built from linked nodes.
- **Learning:** Hybrid: hands-on traversal puzzles, short teaching moments, and the field journal.
- **Failure:** Recoverable. A fall or NULL hazard returns the player to a safe checkpoint and says what happened.
- **Data model:** Platforms correspond directly to dynamically allocated `Node` objects.
- **Traversal:** Platform traversal follows the active list's links. The journal displays those same links.
- **Code style:** Small files, plain structs, and focused functions or classes.

## Resolved implementation decisions

- **Traversal Rule A (link-gated solidity):** a platform is solid only if it is the current node, `current->next`, or (doubly list only) `current->previous`. In the stack, the only other solid platform is the top (the list's head): the stones under it wait their turn. Landing on a solid platform other than the current one sets `current` to it. One function, `isPlatformSolid(current, candidate, kind, head)` in `World.cpp`, is used by both collision and drawing. Platforms are one-way: the player lands on them only from above.
- **Layout:** node positions describe a flowing route that rises, dips, and turns with each place, not a level row. List order decides which platform is reachable; it does not require platforms to sit in a line. Every jump the route needs (forward, and backward where Region 2 requires it) is checked at startup by simulating the player's real movement, and a failed check is reported like a failed link check.
- **Camera:** follows the traveler horizontally, stays inside the active region, and leans toward the platform the guiding light points to, so the eye is carried along the way forward. Region 3 fits on one screen and the camera frames all of it.
- **Circular wrap:** a hollow in the tail's roots in Region 3 is the visible form of `tail->next`. A luminous current of water and petals flows from that hollow, under the garden's ancient tree, back to the head. Its path is computed from the real tail and the node its `next` points to. Walking into the hollow reads `tail->next`, the current carries the traveler along that path, and then the link is followed.
- **Region transitions:** each region ends at an exit (arch, doorway, moon gate, dawn gate, sea door, the ship's cabin door) that opens over a moment when its objective is complete. An exit is not a pointer and the lists are never linked to each other. Between regions the screen fades to a short atmospheric beat that names the next place, and the game moves to the next list's head. Transitions show no code.
- **Region 2 puzzle:** reaching N4 takes the key's light, which the traveler carries back through `previous` to N2, where it lights a brazier. These are visual responses to objective steps, not extra links.
- **Region 4:** a singly list N1 → N2 → N3, with `N3->next == nullptr` and an amber hazard gap after N3. With the journal open, pressing E on N3 runs `if (current->next == nullptr)` and then calls `append()`, which creates N4 "Bridge" right at N3's edge. The bridge is drawn from that new node and assembles plank by plank; the repair brings the dawn. The list owns the bridge node, so `clear()` frees it. No node is ever faked.
- **Region 5 (stack):** a `LinkedStack` whose bottom node is the floor stone at the foot of a lighthouse. Standing on the top, E runs `push()`: a new stone slides out of its socket in the tower wall above the old top, becomes the new top, and the traveler climbs onto it. On the highest stone, E rings the fog bell. Then S (or Down) runs `pop()`: the top stone slides back into the wall and is deleted, and the traveler drops onto the stone under it, which is the new top. Popping the floor stone is refused, because its `next` is NULL, and a callout says why. Standing on the floor stone after the bell has rung opens the sea door.
- **Region 6 (queue):** a `LinkedQueue` of boats with a front and a rear pointer; three boats wait at the start. E runs `enqueue()` from anywhere: the next boat rows in to the rear, because the queue keeps a pointer to its rear and never walks the line. The traveler steps along `next` from the front; once they have left the front boat, `dequeue()` deletes it and it rows away. The ship comes in last, and reaching it opens the cabin door.
- **Deleting nodes during play:** before `pop()` or `dequeue()` deletes a node, every non-owning pointer leaves it: the traveler's current node, the checkpoint, and the region's exit. Animation and art keep node IDs and rectangles (a removed node's last rectangle and the time it left), never pointers. Nodes that do not exist yet are painted ahead from the layout table that `push()` and `enqueue()` use, and drawn by ID once they exist.
- **Notes for the stack and the queue:** a new stone or boat plays its note as it arrives, and the same note again, softer, as it leaves. The stair's tune therefore comes back reversed (last in, first out) and the harbor's in the order it came (first in, first out).
- **Field journal:** shows the current node's real `next` (and `previous` in Region 2), the whole active list walked from the head, and a short note that connects the list to what the player just did. It is the place for node IDs, pointer names, and code. It has no separate selection mechanism.
- **Checkpoints:** the checkpoint starts at the region head and moves to the current node at each objective step. Region 3 restarts from the head during the first trip around, and its step counter resets on such a fall. On the stair, the checkpoint is the top stone, the only one that holds.
- **Checkpoint prompts:** shown as a journal question followed by an answer.
- **Replay:** clears the non-owning pointers, clears all lists, and rebuilds the stage.
- **Escape:** `SetExitKey(KEY_NULL)` makes Escape pause the game. Q quits.
- **Memory check:** a live-node counter, `g_liveNodeCount`, is printed to the console at shutdown and must be 0. It is a developer diagnostic and is not shown on the player's completion screen.
- **Build:** CMake with `FetchContent` for Raylib 5.5, C++17. The project lives in the workspace root, not a `node-wanderer/` subfolder. Raylib is a static library and the MSVC runtime is linked statically (`/MT`), so the exe only uses Windows system DLLs. All art and shaders are built into the executable. The music and sounds are files in `assets/audio`: the build copies the folder beside the exe, and `cmake --install build --prefix dist --component game` makes a folder to share (the exe and `assets/audio`). The game finds the folder beside the exe or in the working folder; without it, or without an audio device, the game runs silently and says so in the console.
- **Input:** the keyboard is read once per frame into an `InputFrame` (`Input.h`), which gameplay receives as plain values.
- **Display:** every frame is laid out at 1280x720 (`Display.h`) and drawn 1:1 in the window. F11 or Alt+Enter, anywhere, or the pause screen's Mode row, switches to exclusive fullscreen at the resolution of the monitor the window is on, and back. In fullscreen the frame fills the screen at 16:9 with black bars: it is drawn one whole step larger than the screen needs (2x for 1920x1080, 3x for 2560x1440 and up) into an off-screen frame and scaled down, which smooths text and shapes while the pixel art stays exact at the usual resolutions. Screens beyond 3x are scaled up with a sharp bilinear shader. Fullscreen uses the monitor's fastest refresh rate at that resolution, so the frame cap follows the refresh rate there (60 in the window); movement is integrated exactly, so play is the same at any frame rate. The cursor is hidden in fullscreen. The choice lasts for the session.

## Art direction

The game is "lantern-lit storybook pixel art": crisp pixels on a 640x360 canvas scaled 2x, painterly dithered gradients, layered depth, and soft, dynamic light.

### Assets and rendering

- Each region's scenery and platforms are painted pixel by pixel when the region loads (`Paint.cpp` and the six region art files) and kept as textures. Moving parts (water, fire, foliage in the wind, the bridge, the stones and boats, gates, particles) are drawn live each frame.
- The traveler and small props are pixel sprites written in code.
- Lighting: a light map darkens the scene away from light sources and lights from above, every light source also glows, and a final pass adds gentle bloom, a per-region color grade, and a vignette. The shaders are embedded in the source; if they fail to load, the game still runs without the final pass.
- All art is original and made for this project. The audio comes from open-license sources (CC0 and CC BY 4.0), and every file's sources, licenses, attribution, and edits are listed in `assets/audio/CREDITS.md`, with copies of the licenses in `assets/audio/licenses/`. Any external asset added later must be recorded the same way.

### Shared language

- **Light shows the way.** Warm gold light rises from the platform `current->next` points to, and a stream of motes drifts from the traveler toward it. In Region 2, cool silver light marks the way back through `previous`. In Region 3, the teal current is the link from the tail back to the head. In Region 4, amber light marks the missing link; the repair turns it into dawn. In Region 5, warm light rises from the new top stone. In Region 6, it rises from the boat `current->next` points to, and the lamp on the next empty post shows where `enqueue` will moor the next boat.
- **The route reads first.** Every walkable top has a bright lip and a shadow below it, and the route is the most saturated, highest-contrast layer. Background layers fade toward each region's fog color with distance. Platforms that are not solid right now are veiled in mist and lift out of it when a link reaches them.
- **Platforms belong to their place.** Mossy banks, boulders, and logs; ruined halls, columns, and stairs; garden terraces, giant mushrooms, and roots; cliff shelves and a rope bridge; stones set into a lighthouse wall; rowing boats and a ship. Each platform is drawn from its node's position and width, and its top matches its collision exactly. Decoration never becomes a platform.
- **Depth.** Sky, far, middle, and near layers move at different rates behind the route; dark foreground silhouettes frame the view from the edges without covering the route.
- **Motion with purpose.** Wind gusts roll across canopies, grass, and banners; water flows under the ruins and reflects the scene; petals ride the garden's current; the wind drops and the light changes when the bridge is repaired; fog drifts past the lighthouse and its bell swings; boats row in to the rear of the line and away from its front. Plants bend away from the traveler, and the lantern lights what is near. Small flickers support the scene but are not its main source of life.
- **No diagram in the world.** No pointer arrows, node boxes, IDs, or code labels in the landscape.

### Regions

1. **The First Connection** (singly linked): a deep, welcoming forest in late golden light, with layered trees, a giant old tree, roots, mossy stone, light shafts, and drifting motes. The route climbs from the forest floor onto a fallen log high between the trunks, then steps down to a sunlit arch.
2. **The Forgotten Return** (doubly linked): flooded ruins under a full moon, with broken arches, stone halls, braziers, and still water that reflects everything. The route dips to a hall by the water and climbs a ruined tower to the key's shrine; the way back down is the lesson.
3. **The Endless Garden** (circular): a luminous night garden around an ancient tree and a glowing spring. The route climbs over the front of the tree, and the current flows back under its roots to the start, so the whole circle is visible on one screen.
4. **The Edge of Nothing** (NULL safety): cold cliffs before dawn above a deep ravine, with a snapped bridge, wind, and amber light rising from the gap. The repaired bridge assembles plank by plank, lanterns light along it, and dawn breaks over the far peaks.
5. **The Lighthouse Stair** (stack): a lighthouse on sea cliffs in morning fog, with surf on the rocks below, gulls, and a fog bell on an iron arm near the top. The stair is the stack: stones slide out of the tower wall one on top of another, and back in, the last one first.
6. **The Patient Harbor** (queue): a bright harbor below a hill town, with a breakwater and its light and a line of boats on clear water. Boats row in at the rear and away from the front, and the line ends at a ship.

### Interface

- Small, warm overlays that never cover the traveler or the route: the place and objective at the top left, a journal hint at the top right, and controls at the start of the journey and on the pause screen.
- Reading happens in an explorer's field journal: the journal itself, notes, the pause cover, and the ending.

## Sound

The sound is a warm, gentle extension of the traveler and the places: it supports exploration, makes important moments satisfying, and leaves room for the notes and the journal.

### Music and ambience

- **One score:** related pieces by one composer (piano, strings, and soft synth pads), each cut into a seamless loop. The forest is warm and sparse, the ruins spacious and reflective, the garden luminous and dreamlike, the edge restrained and expectant, the lighthouse calm and misty, and the harbor warm and folk-like. Mending the bridge brings a hopeful dawn piece, and the journey's end has its own.
- **Ambience for each place:** soft forest air, birds, and leaves; drips, still water, and faint firelight in the flooded halls; night insects in the garden; wind across the cliffs, with the deep draft of the ravine growing near the broken bridge, then a calmer morning wood after the repair; surf on the rocks below the lighthouse, with a thin breeze; water against the quay and boats creaking in the harbor. Beds sit well under the music and loop without seams.
- **Transitions:** the old music and ambience fade with the screen, and the new place's music and ambience fade in during the dark beat that names it, with a short motif in the place's key. Nothing cuts off abruptly.
- **Under notes and menus:** the music dips a little under journal notes and the open journal, and further on the pause screen, then returns.

### Effects

- **The traveler:** footsteps that follow the walking rhythm, with a sound for each surface (grass, earth, stone, wet stone, wood, a soft mushroom cap) and slight variation in pitch and level; a rustle on jumping; landing on the surface, with a thud after a real drop; a rush of air on a long fall; a splash into water.
- **Links:** each link followed plays a note in the key of the place's music, rising along `next`; in Region 2 a cooler note falls back along `previous`; in Region 3 the current home through the tail's `next` ends on a full chord. On the stair, each stone plays its note when `push()` sets it and the same note, softer, when `pop()` takes it, so the tune comes back reversed; in the harbor, a boat's note plays when it moors, when the traveler steps onto it, and, softer, when it rows away.
- **The world:** taking the key's light, the brazier catching, checkpoint lanterns, gates opening (one chord for each place), walking through an exit, recovering from a fall, the edge's gust, and the journey's end.
- **Region 4:** the gap is tense but never startling (a low swell from the depth, with no hard attack); the repair swells, planks land one after another as the bridge is drawn, the pier rises, the rope rails tighten, lanterns light in turn on notes of the dawn's key, and the wind drops.
- **Regions 5 and 6:** stones grind out of the wall and lock (`push`), grind back in and seat (`pop`), and the bottom stone only thuds when it will not move; the fog bell strikes twice and rings on; a boat rows in and knocks against its post (`enqueue`), and the front boat rows away (`dequeue`).
- **The journal and interface:** opening and closing the journal, turning a note's page, dismissing a note, the first-time callouts, the pause cover, and the volume controls each have a quiet paper, leather, or tick sound.
- Each one-shot plays once when its event happens, never while a state stays true. Repeated sounds vary, and a few sounds of each place (drips, chimes, creaks, pebbles, gulls) play at random intervals.

### Mix

- Levels are set from measurements of the running game: the rewards of a place reach the level of the music and ambience, link notes sit a little under it, interface sounds lower, and footsteps about 9 dB under it.
- Sounds are placed left or right by where they happen on screen.
- A soft limiter at the end of the mix rounds off rare peaks instead of clipping, and a voice limit and short per-sound cooldowns stop pile-ups and double triggers.

## Writing

There are two voices:

- **While exploring:** short, natural lines about what the traveler sees or can do. Introductions, objectives, recovery notes, transitions, and the ending are written in this voice. They do not mention memory, `new`, ownership, or code.
- **In the field journal:** clear explanations of `next`, `previous`, `NULL`, and traversal, connected to what the player just did. A brief callout appears the first time each kind of link is used, with a short code line, and points to the journal for detail. Ordinary steps do not show code.

A circular traversal normally stops when it returns to its starting node. The maximum step count is a safety fallback, not an equivalent stopping rule.

## Gameplay loop

1. Enter a place and read a short welcome.
2. Look for the light, or open the journal, to find the way forward.
3. Walk and jump between platforms.
4. The game checks whether the destination is reachable through the active node's link.
5. Reaching the objective asks a short journal question, answers it, and opens the way to the next place.
6. Falling or stepping into the NULL hazard says what happened and restores the player at a safe checkpoint.
7. Completing all regions shows the ending and offers replay or exit.

## Controls

Show controls at the start of the journey and on the pause screen.

| Action | Input |
| --- | --- |
| Move left / right | A / D or Left / Right |
| Jump | Space |
| Interact (mend the bridge, set a stone, ring the bell, call a boat) / continue a note | E or Enter |
| Take the top stone off (Region 5) | S or Down |
| Open or close the field journal | Tab |
| Pause / resume | Escape (Q quits while paused) |
| Sound off / on | M |
| Fullscreen / window | F11 or Alt+Enter |
| Change music or sound volume, or the screen mode | On the pause screen: W / S or Up / Down choose, A / D or Left / Right change |

Use configurable movement speed, jump impulse, and gravity. Do not allow another jump while airborne.

## World and tutorial regions

Place six regions consecutively in one continuous side-scrolling world, each ending at its exit.

### Region 1 — Singly Linked List: forward traversal

- Build a linear chain of at least four nodes. Each node's `next` points to the following node; the tail's `next` is `nullptr`.
- Through play and the journal, show that traversal follows `next` in one direction and that no backward link exists.
- Puzzle: start at the head and visit nodes in order to reach the exit.
- Checkpoint prompt: "Which link tells you where to go next?"
- If the player tries to go backward, explain that no previous link is kept. Do not fabricate a backward data link.

### Region 2 — Doubly Linked List: forward and backward traversal

- Build a chain of at least four nodes with consistent `next` and `previous` links. Head `previous` and tail `next` are `nullptr`.
- Traversal is available in both directions.
- Puzzle: visit a target, follow `previous` to an earlier node, then continue.
- The journal shows both links for the current node. At an endpoint, it shows that the corresponding pointer is null.

### Region 3 — Circular Singly Linked List: loop traversal

- Build a circular chain of at least four nodes. Each `next` points to the following node, and tail `next` points back to head.
- Traversal loops and never reaches a null `next`.
- Puzzle: follow links until returning to the starting node. The walk stops when `current == start`; a bounded step count is a safety fallback.
- Explain that a circular traversal needs an explicit stopping condition to avoid an infinite loop.

### Region 4 — NULL pointer hazards: checks and recovery

- Present a visible missing-link hazard representing a null pointer. Never dereference null or treat it as a valid destination.
- Explain that `nullptr` means there is no node; check before reading a node or following a link.
- Puzzle: inspect a link, identify it as null, and repair the way by appending a node.
- If the player enters the hazard, explain briefly and return them to the checkpoint with a valid active node.
- Model a safe code check, never undefined behavior or a crash.

### Region 5 — Stack: push and pop at the top

- Build a stack as a linked list whose head is the top. The floor stone is the bottom, and its `next` is `nullptr`. `push` allocates a stone and sets `stone->next = top; top = stone;`. `pop` saves `top->next`, deletes the old top, and makes the saved node the top.
- Only the top (and the node the traveler stands on) is solid: the stones under it wait their turn.
- Puzzle: push stones and climb to the fog bell, ring it, then pop the stones top first back down to the floor, where the sea door opens.
- Popping the bottom stone is refused with an explanation: there is nothing under it, and its `next == nullptr` says so.
- The journal shows the top, the walk from the top to NULL, and the code of push and pop.

### Region 6 — Queue: enqueue at the rear, dequeue at the front

- Build a queue as a linked list with a front and a rear pointer. `enqueue` allocates a boat and links it after the rear, or makes it the front and the rear when the queue is empty. `dequeue` deletes the front and sets the rear to `nullptr` when the queue becomes empty.
- Traversal follows `next` from the front. Leaving the front boat dequeues it.
- Puzzle: call boats to the rear and cross the line to the ship, which comes in last.
- The journal shows the front, the rear, the walk from the front to NULL, and the code of enqueue and dequeue.

## Data structures and ownership

Keep the central structure explicit:

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

Raylib's `Vector2` is stored directly in the node so platform position is not duplicated in a visual-only platform model. Static string labels are enough for this stage.

Use a small owning list class for each structure, with focused operations:

- `SinglyLinkedList`: `head`, `append`, `getHead`, `clear`.
- `DoublyLinkedList`: `head`, `tail`, `append`, `getHead`, `clear`.
- `CircularSinglyLinkedList`: `head`, `tail`, `append`, `getHead`, `clear`.
- `LinkedStack`: `top`, `push`, `pop`, `getTop`, `isEmpty`, `clear`.
- `LinkedQueue`: `front`, `rear`, `enqueue`, `dequeue`, `getFront`, `getRear`, `isEmpty`, `clear`.

Each list owns every node it creates. Allocate with `new` in append, push, and enqueue operations, and delete each owned node exactly once. Each list's destructor calls `clear()`. Copying owning list objects is disabled. Keep ownership and link manipulation visible to beginners.

Implement operations plainly:

- Append allocates a node and updates the previous tail link or links.
- Push links the new node in front of the top. Pop and dequeue save the next pointer before deleting; dequeue also clears the rear when the queue becomes empty.
- Forward traversal follows `next` only after checking it.
- Backward traversal follows `previous` only after checking it.
- Circular traversal stops on returning to its start, with a safe maximum step count as a fallback.
- Cleanup stores the next pointer before deleting the current node. Circular cleanup breaks the cycle first so it terminates.

Avoid generic abstractions that hide pointer changes. Add short comments to explain meaningful link assignments.

## Single source of truth: node, platform, traversal, journal

The active region's list is the gameplay source of truth. There is no separate platform-node graph.

- Draw every platform from fields on its real `Node`. Platform art may be painted ahead of time, but it is painted from the node's position and width by walking the real list, and its walkable top matches the node's collision surface.
- Keep non-owning `Node*` references to the active head and the player's current node. They point into the owning region list.
- For a valid traversal, obtain the destination by reading `currentNode->next` or `currentNode->previous` after a null check.
- A successful landing updates the player's current node only if the destination is reachable through the active structure's permitted link. Never infer adjacency from screen coordinates.
- The journal walks from the active head and reads actual IDs, labels, and links. It displays targets as stable IDs and labels, not raw addresses.
- Rendering may iterate the list to draw nodes, but must not create another set of nodes. Temporary traversal pointers are non-owning.
- Switching regions selects that region's list and head. The lists are not linked to each other.
- A shared `Node` type is used. For non-doubly-linked regions, `previous` stays `nullptr`.

## Player and platform interaction

Use a small `Player` struct with position, velocity, size, grounded state, and a non-owning pointer to the current node. Animation state is kept apart from physics and never affects collision.

- Use world coordinates for node positions and player movement.
- Use Raylib rectangle checks for landing and hazard detection.
- Apply gravity each frame; apply the jump impulse only while grounded. The jump arc does not depend on the frame rate.
- Derive collision geometry from the node's `platformPosition` and `platformWidth`.
- Accept a landing as a list traversal only if its destination is reachable from the current node via an allowed link.
- On an ordinary fall, restore the last safe checkpoint. In the null-hazard region, explain the missing link as well.
- Null must not represent "the player is on a platform." Before gameplay resumes from recovery, assign a valid checkpoint node.

## Memory safety

- Check pointers for `nullptr` before dereferencing.
- Never delete through the player, renderer, journal, or other non-owning pointer.
- Delete every allocated node exactly once.
- Never use node pointers after their owning list is cleared or destroyed. Baked platform art keeps node IDs and positions, never node pointers.
- Give circular traversal and cleanup explicit termination conditions.
- Reset player and checkpoint references before destroying their owner.
- Before `pop()` or `dequeue()` deletes a node, move every non-owning pointer off it (the traveler's current node, the checkpoint, the region's exit).
- Do not represent a missing destination with a fake node.

## Audio implementation

- Raylib's audio module (miniaudio) is used. `main` opens the audio device after the window and before the game loads any sound, and closes it after the game has unloaded every sound and stream.
- `GameAudio` (`Audio.h` / `Audio.cpp`) owns every music stream and sound. Music and ambience are streams (Ogg Vorbis, 48 kHz) with large buffers, so painting a new place cannot starve them; effects are decoded sounds, each with an alias so a sound that is still ringing is not cut off when it plays again.
- `GameSound.cpp` decides which music, ambience, and effects fit each place and state. Gameplay calls the audio module only from its event handlers; drawing functions only read the settings.
- Streams are updated every frame, also while paused. Pausing dips the music and lets the world's sounds glide to silence and wait; interface sounds keep playing. Volume changes, muting, and stopping always glide (no clicks): streams fade, every playing sound's level is eased each frame, and a master gain at the end of the mix glides on mute and before shutdown.
- Replay forgets scheduled sounds and starts the first place's music again. Completion fades into the finale.
- If the device or a file cannot be opened, the game logs a warning naming it and continues; without the folder it is silent.
- The pause screen has a small settings panel: music and sound levels (in steps of 10%, shown as notches and a percentage), a mute switch, and the screen mode (window or fullscreen, with the resolution in use). While a level changes, it plays at that level so it can be heard. M mutes and unmutes anywhere. Settings last for the session.
- `tools/audio/build_audio.py` rebuilds every file in `assets/audio` from its sources and writes `CREDITS.md`; `tools/audio/check_audio.py` measures every file and checks each loop's seam.

## Folder structure

```text
README.md
spec.md
CMakeLists.txt
src/
  main.cpp                 window, loop, shutdown memory check
  Node.h                   the Node struct and the live-node counter
  SinglyLinkedList.*       DoublyLinkedList.*   CircularSinglyLinkedList.*
  LinkedStack.*            LinkedQueue.*        the stack and the queue (Regions 5 and 6)
  World.h / World.cpp      region lists, layout, Rule A, link checks
  Game.h / Game.cpp        rules: traversal, objectives, recovery, transitions, camera
  Player.h / Player.cpp    movement, the jump check, and the traveler sprite and animation
  Input.h                  keyboard read once per frame
  GameScene.cpp            composing a frame: layers, platforms, traveler, lights, post pass
  GameUi.cpp               HUD, callouts, field journal, notes, pause and its sound panel, ending
  GameSound.cpp            which music, ambience, and effects fit each place and event
  Audio.h / Audio.cpp      the audio module: loading, streams, effects, fades, volume, mute
  Art.h / ArtCommon.*      shared art types and helpers (layers, ledges, glows, wind)
  Paint.h / Paint.cpp      CPU painting: masks, shading, noise, dithering
  Lighting.h / Lighting.cpp  render targets, light map, bloom and grade shader
  Display.h / Display.cpp  the 1280x720 frame, the window and exclusive fullscreen, crisp scaling
  ForestArt.cpp  RuinsArt.cpp  GardenArt.cpp  EdgeArt.cpp  LighthouseArt.cpp  HarborArt.cpp   one file per region
  Effects.*                particles
  Theme.h  DrawHelpers.h   interface colors and small helpers
assets/audio/              music/, ambience/, sfx/ (Ogg Vorbis), CREDITS.md, licenses/
tools/audio/               build_audio.py (rebuilds assets/audio and its credits), check_audio.py
docs/readme/               documentation art for the README (not used by the game)
```

## Acceptance criteria

- The project builds and starts as a Raylib C++ application.
- The player walks, jumps, falls, lands on platforms, and the camera follows.
- The world has six recognizable regions in the specified order, each with its own palette, materials, landmark, and motion, at the same level of finish.
- Each region's route varies in height and spacing, and every required jump is reachable.
- Singly linked traversal follows only `next`; tail `next` is null.
- Doubly linked traversal follows both links; head `previous` and tail `next` are null.
- Circular traversal returns to head and ends via its stopping rule, with a step limit only as a safety fallback.
- The null-hazard puzzle checks safely, explains null, and allows recovery without crashing.
- The stack grows and shrinks only at its top: push creates the new top over the old one, pop deletes the top, the bottom cannot be popped, and only the top holds the traveler.
- The queue adds at the rear and removes from the front: a new boat is linked after the rear without walking the list, the boat that came first leaves first, and the rear is cleared when the queue empties.
- No non-owning pointer is left pointing at a node that pop or dequeue deletes.
- Every visible platform is drawn from a real dynamically allocated `Node` owned by its list.
- Platform traversal and the journal read the same nodes and links. There is no parallel fake platform graph or journal-only link representation.
- Player, renderer, and journal hold only non-owning node pointers.
- All allocated nodes are released once, with no dangling references during play or shutdown.
- Exploring text is written in an adventure voice; technical teaching lives in the journal and first-time callouts.
- The completion screen shows no memory diagnostics.
- Music and ambience change smoothly with the six places, with no audible loop seams or abrupt cuts.
- Important player, world, journal, and objective events have restrained, fitting sound; repeated sounds vary and do not pile up.
- Pause, transitions, replay, completion, mute, fullscreen, and the volume controls behave correctly, and the game runs silently when the audio assets or device are unavailable.
- Every sound and stream is unloaded before the audio device closes.
- Every third-party audio asset has a verified license and complete attribution in `assets/audio/CREDITS.md`.
- A beginner can find and understand the node definition, list operations, region setup, and player-to-node interaction without navigating a large framework.

## Instructions to the coding agent

Do not replace linked lists with arrays, vectors, coordinate-based adjacency, or a visual mock graph for gameplay. Arrays or vectors may hold art and fixed UI data, but platform traversal and the journal must consult actual nodes and links.

Use plain C++ and explicit pointer checks. Add short comments where links are connected, where circular traversal stops, and where nodes are freed. Prefer a few small, understandable operations over a generic framework. Visual quality, sound, and the player's experience have high priority: art direct, implement, inspect the running game at 1280x720, listen to it, and refine every region, as long as the art and sound stay presentation and the single-source-of-truth design holds.
