# The Node Wanderer — Visual Reference and Incorporation Guide

## Purpose

This guide turns visual references into practical art-direction and interface decisions for the tutorial stage. It is written for the coding agent implementing the current Raylib prototype. Treat the supplied screenshot as a snapshot of the current implementation, not as a finished target. The goal is to keep its clear pixel-font identity and visible node links while making the world, character, platforms, and teaching moments feel intentional and readable.

The game remains a 2D side-scrolling platformer with one connected world and four tutorial regions: singly linked, doubly linked, circular singly linked, and NULL-pointer safety. Platforms must continue to be the actual dynamically allocated backend `Node` objects. Art and UI may clarify the structure, but must never introduce a separate visual graph that disagrees with the linked list.

## Quick art direction

Use a **whimsical illustrated pixel-adventure** look: layered natural environments, crisp silhouettes, a small expressive traveler, substantial platforms, restrained glow, and a compact pixel-font interface. The screenshot already establishes a dark navy palette and geometric type. Keep those as the UI foundation, but give the world warmer light, atmospheric depth, and region-specific color accents.

The visual hierarchy should read in this order:

1. Player and safe route forward.
2. Current node and its valid outgoing link or links.
3. Region objective and short teaching prompt.
4. Optional detailed node inspector.

Avoid drawing every link with equal brightness all the time. The current screenshot’s long, thin arrows are useful for explaining the concept, but they compete with the platforms and cross the large empty space. Show the current node’s valid connection prominently, keep other links quiet, and use the inspector for exact pointer details.

## Reference 1 — Side-scrolling environment and composition

![Ori and the Blind Forest side-scrolling environment reference](https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/387290/ss_b86d0d91695cb98067565ecae7c100460063c82b.1920x1080.jpg?t=1701967651)

Reference: [Ori and the Blind Forest — Steam image](https://store.steampowered.com/app/256661394/Ori_Original_Release_Trailer/?l=japanese)

### What to borrow

- **Depth from layers:** distant silhouettes, mid-distance scenery, and foreground shapes make a side-view level feel like a place instead of a blank canvas.
- **Light as a route cue:** use a warm light source, bright plant, lantern, or node glow to draw attention toward the next traversable platform.
- **Clear play lane:** keep collision platforms and hazards in the foreground with edges that remain easy to read against the background.
- **Controlled detail:** put most decorative texture behind the route. The player should remain identifiable while moving.

### How to incorporate it

- Give each region a distinct environmental treatment while retaining a continuous horizon and shared palette: leafy green for Region 1, cool blue stone or water for Region 2, violet bioluminescent grove for Region 3, and amber warning light for Region 4.
- Use two or three parallax layers with simple shapes. If parallax is too much for the first pass, use a static gradient sky, distant silhouettes, and a few foreground plants.
- Keep platforms brighter or more saturated than the scenery immediately behind them. Node IDs should remain legible at normal game scale.
- Use lighting and color to suggest direction, not to imply extra data-structure links.
- Treat this as a mood and composition reference, not a request to reproduce Ori’s character, assets, or exact art.

## Reference 2 — Platform scale, readable path, and lively scenery

![Rayman Legends forest platforming reference](https://cdn.supersoluce.com/file/docs/docid_4f9ab4d48f152fff04000173/elemid_4ee9fac00a2fe93e0e000014/thumb_1377792563-1.jpg)

Reference: [Rayman Legends forest image](https://www.supersoluce.com/jeu/rayman-legends)

### What to borrow

- The environment has visual richness, but the traversable ground and foreground action remain easy to distinguish.
- Platforms feel broad enough to land on and are integrated into the world’s materials.
- Colorful effects can reward movement and mark points of interest without turning every surface into a control.

### How to incorporate it

- Enlarge platforms relative to the character compared with the current prototype. A beginner should be able to judge a jump and identify a landing area at a glance.
- Build node platforms from a consistent base shape and vary only a few details by region: edge trim, material, small plant or crystal accents.
- Keep the node’s ID/label on a small attached sign or badge. Do not make the entire platform a tiny floating rectangular label.
- Use a short arrival effect when the player reaches a new node: a ring, a brief pulse, or a small sparkle. This effect marks traversal success; it must not change list topology.
- Treat this as reference for readability and liveliness, not as an instruction to imitate Rayman’s exact art.

## Reference 3 — Linked-list pointer diagrams

![Linked list variants diagram showing singly, doubly, and circular connections](https://static.wixstatic.com/media/d9fce6_4b2cb3f53c054554aa553baaf02d49db~mv2.jpg/v1/fill/w_1123%2Ch_690%2Cal_c%2Cq_90/d9fce6_4b2cb3f53c054554aa553baaf02d49db~mv2.webp)

Reference: [Singly, doubly, and circular list diagram](https://sophiesu.net/ch1-singly-linked-list-%E5%8D%95%E9%93%BE%E8%A1%A8-python-%E6%95%B0%E6%8D%AE%E7%BB%93%E6%9E%84%E4%B8%8E%E7%AE%97%E6%B3%95-%E5%AD%A6%E4%B9%A0%E7%AC%94%E8%AE%B0/)

### What to borrow

- A node is a box containing a value and a pointer field; arrows show the stored relationship.
- Singly linked lists have one forward pointer and a null end.
- Doubly linked lists show both forward and backward pointers.
- A circular list visibly returns from its tail to its head.

### How to incorporate it into the game world

- Let each world platform stand for exactly one real `Node`. Its stable ID or label is the node’s value; the actual `next` and `previous` pointers determine valid traversal.
- Use a short connector ribbon or glowing directional trail between platforms. Draw it from the real list link, using the source and destination nodes’ platform positions. Never create an independent adjacency map for drawing.
- For singly linked traversal, draw one arrow from the current node toward `next`. At the tail, show a small terminal marker labeled `NULL` or a clear “end” glyph.
- For doubly linked traversal, use paired arrows or a reversible connector. Highlight only the direction currently relevant to the puzzle; let the inspector show both stored pointers.
- For circular traversal, route the final connector visibly back toward the head platform. Ensure the return path is clear on screen, with a subtle loop arc if the geometry allows it.
- For null safety, show an intentionally absent connector at the hazard. Use a warning marker and a safe checkpoint/bridge interaction; do not draw a fake destination node.

### How to incorporate it into the inspector

The inspector is a compact code-learning panel, not a second level map. When the player selects or stands on a node, show the real values in a simple format:

```text
Node N2 — Alpha
next     → N3 — Beta
previous → NULL
```

For the singly and circular regions, `previous` can be omitted or shown as unavailable; do not suggest that it is a usable link. In the doubly region, show both actual pointers. Highlight the current node and outgoing links that the player can actually follow. Display stable IDs and labels rather than memory addresses.

Reference diagrams teach the abstract pointer layout. The game should translate that idea into the same nodes the player traverses, rather than placing a separate textbook diagram over unrelated platforms.

## Reference 4 — The supplied prototype screenshot

The supplied screenshot is the baseline to improve. It shows a 1280×720 side-view prototype with a navy background, pixel-style text, a top-left region/objective banner, small rectangular platforms connected by blue arrows, and a large bottom tutorial dialogue box. The character is a simple white rounded rectangle with a dark face mark.

### Preserve

- The pixel-style typography and dark navy UI foundation.
- The explicit region number and name.
- The objective stated in plain language.
- Visible `next` link labels as a teaching device.
- A clear interaction hint for continuing dialogue.
- The central premise that the platforms are real nodes.

### Improve

- The play area currently has a lot of empty space and little environmental context. Add layered scenery and light points to establish a world and guide forward movement.
- The node platforms are too thin and small for the character. Increase their height and landing area, and give them a consistent, world-integrated shape.
- The arrows cross broad empty gaps and are visually stronger than the route. Shorten connectors, anchor them to node edges, make inactive ones dim, and highlight the current valid transition.
- The region title appears twice. Keep the region name in the HUD; use the in-world area for scenery, landmarks, and the current node instead of repeating the title.
- The bottom dialogue panel takes a large share of the screen and overlaps the platform route. Use a slimmer dialogue card, let it dismiss after the explanation, and keep the player visible while a prompt is open.
- The current player is readable as a placeholder but has little personality. Replace the block with a tiny explorer silhouette with a head, body, and a backpack or lantern, while keeping a strong outline against all backgrounds.
- Add state feedback: current node, reachable next node, completed nodes, and checkpoint state should be visually distinct.

## Shared visual system

### Character

Create one simple, original traveler that reads at small size. Start with a small idle pose, walk cycle, jump/fall pose, and landing pose. A scarf, backpack, or lantern can provide a clear silhouette and a warm accent. Keep animation frame counts modest; a readable static sprite is better than a detailed character that disappears at game scale.

### Node platforms

Each node platform should have:

- A broad collision surface with a clearly visible top edge.
- A small stable node badge (for example, `N1 · Head`).
- A current/visited/unvisited visual state.
- A region accent shared with the UI but not so bright that it obscures text.

The label, position, and platform width should come from the actual node data. If the node stores world position and width, draw from those fields; do not maintain duplicate platform coordinates elsewhere.

### Link visuals

- **Reachable current link:** bright cyan or region accent, with an arrowhead.
- **Other existing links:** thin, low-contrast line or hidden until inspection.
- **Null link:** no connector, with a small labeled endpoint marker near the source node when teaching null.
- **Circular return link:** a curved or elevated route cue that clearly returns to the head.

Use a simple render helper that receives actual source/destination nodes. It can calculate line endpoints from node platform positions. It must not decide which nodes are adjacent based on coordinates.

### UI hierarchy and layout

- Top-left: compact region badge and one-line objective.
- Near the player or current platform: unobtrusive current-node indicator.
- Bottom: short contextual message with a dismiss/continue hint; hide or collapse once read.
- Optional inspector: toggleable side panel with the selected node’s actual pointer values.
- Keep the center and lower play lane visible during traversal. Avoid persistent large panels over landing areas.

### Color roles

Use color consistently and reinforce it with shape or text so color is not the only signal:

| Role | Suggested treatment |
| --- | --- |
| Base world | Deep blue-gray, with warmer environmental light |
| Singly linked region | Leaf green with one-way arrow marks |
| Doubly linked region | Cool blue with paired arrow marks |
| Circular region | Violet with a loop motif |
| NULL lesson / warning | Amber with a broken-connector or stop glyph |
| Current node | Bright outline plus a simple marker above it |
| Completed node | Muted fill plus a check or completed state |

Colors are starting points. Maintain contrast for text and collision edges at the target display size.

## Applying the visual language to each region

### Region 1 — Forward-only forest path

Use a clear left-to-right line of broad platforms in a leafy environment. Connect only the current node to its actual `next`. Introduce the head with a distinct badge, and mark the tail’s null end when the player reaches it. Keep backward movement physically safe if needed, but do not draw or imply a `previous` pointer.

### Region 2 — Reversible stone-and-water route

Change the scenery to a cooler palette and make the route suitable for returning to a previous platform. Use paired directional marks on the actual doubly linked connections. The inspector can show `previous` and `next` simultaneously, with the currently selected direction emphasized.

### Region 3 — Circular glowing grove

Arrange platforms so the return-to-head path is visually understandable. The world layout can form an arc or loop while each platform continues to correspond to the node it represents. When the player returns to the starting node, show a clear “back at start” state and stop the puzzle traversal; avoid an unbounded automatic loop.

### Region 4 — NULL safety and recovery

Use a broken bridge, dark gap, or missing connector, accented in amber. The cue should mean “there is no node at this link,” not “the program crashed.” Offer a safe response: inspect the link, choose another valid route, or activate a recovery bridge. On a fall, fade briefly to the checkpoint and show a short explanation before control returns.

## Prototype-specific layout recommendations

For a 1280×720 reference resolution:

- Reserve a narrow top band for region and objective information.
- Keep node labels close to their platforms and readable at the final window size.
- Use the lower quarter for temporary dialogue only; collapse it after dismissal.
- Keep the player and immediate landing target inside the central play area.
- Avoid placing persistent explanations over the next platform or its connector.
- Let the camera follow horizontally; use region landmarks to communicate progress instead of placing the whole region name in the empty sky.

These are layout targets, not rigid pixel coordinates. If the window is resizable, preserve the same visual priority and keep UI anchored to the screen while nodes remain in world coordinates.

## Asset list for a first visual pass

The first implementation does not need a large art pack. Create or source only:

1. Traveler sprite or temporary drawn character with idle, walk, jump/fall, and landing states.
2. One reusable node-platform tile with current, visited, and inactive appearance variants.
3. A few background layers: sky/gradient, distant silhouettes, and foreground plants or stones.
4. Small icons for head, tail, next, previous, loop, null, checkpoint, and interaction.
5. A restrained particle/pulse effect for node arrival and checkpoint activation.
6. Pixel-readable UI font, or a fallback font with strong contrast.

Do not delay list and movement implementation while searching for final art. Use simple Raylib shapes with the same palette and hierarchy until sprites are ready.

## Implementation guidance for the coding agent

- Keep the current readable pixel UI style, but reduce its footprint and improve its hierarchy.
- Build the world art around the traversal route. Platforms must remain visually obvious and easy to land on.
- Draw platforms and link effects by iterating the active region’s actual linked list. Link endpoints must be derived from linked node pointers and node world positions.
- Keep decoration separate from collision and data structure state. Decorations may be static scenery; they must not imply traversable nodes or list relationships.
- If adding a visual current-node or visited state, store it as gameplay/progress state associated with node identity, not as a second platform graph.
- Make explanations dismissible and keep the player visible during active play.
- Prioritize a cohesive first region, then reuse the same character and platform language with new region accents.

## Acceptance criteria for the visual pass

- The screenshot’s region and objective information is retained but no longer duplicated or oversized.
- The player is recognizable against all four region backgrounds.
- Platforms are broad, readable landing surfaces with clear node labels.
- Current valid links are clear; inactive links do not overpower the route.
- Singly, doubly, circular, and NULL visual cues match the actual node pointers.
- Context panels do not obscure the main play lane for long periods.
- Each region feels distinct while remaining part of one connected world.
- The art polish does not add a fake adjacency structure: the same backend nodes drive platform positions, link highlights, traversal, and inspector content.

## Reference source note

The images above are third-party visual references for art direction and data-structure explanation. Use them to study composition, contrast, and pointer diagrams. Do not copy their characters, level art, logos, or other protected assets into the project. Create original game assets or use properly licensed assets. The supplied prototype screenshot should be compared with this guide as the current implementation snapshot.
