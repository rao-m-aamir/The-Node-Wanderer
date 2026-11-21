#pragma once

#include <cmath>

#include "raylib.h"
#include "Node.h"
#include "World.h"

// The world is drawn into a small pixel canvas that is scaled up 2x, so the scenery,
// the platforms, the traveler, and the effects all share one chunky pixel size.
constexpr int CANVAS_WIDTH = 640;
constexpr int CANVAS_HEIGHT = 360;
constexpr float CANVAS_SCALE = 2.0f; // world pixels per canvas pixel

inline int toCanvasX(float worldX, int cameraX) { return (int)std::floor(worldX / CANVAS_SCALE) - cameraX; }
inline int toCanvasY(float worldY) { return (int)std::floor(worldY / CANVAS_SCALE); }

// What the region art needs from gameplay. Read-only, non-owning pointers only.
struct SceneView {
    int region;             // 0..3
    int cameraX;            // camera left edge, in canvas pixels
    int scroll;             // camera offset from the region's left edge (for parallax layers)
    const Node* head;       // platforms are drawn by walking next from the active list's head
    const Node* current;    // the node the player stands on
    const Node* checkpoint; // where recovery returns the player
    const float* mistById;  // per node ID: 0 = clear, 1 = veiled in mist (not reachable now)
    ListKind kind;
    bool hasGate;
    Rectangle gateRect;     // exit opening, world coordinates
    bool gateOpen;
    float checkpointFlash;  // 1 -> 0 just after a checkpoint lights up
    float time;
    int stage;              // objective step (Region 2 relic, Region 4 repair)
    bool emphasizePrevious; // Region 2: the puzzle step that goes back through previous
    Rectangle portalRect;   // Region 3: the root hollow on the tail (world coordinates)
    float passage;          // Region 3: 0..1 while travelling through the roots, otherwise -1
    bool bridgeBuilt;       // Region 4
    float bridgeProgress;   // Region 4: 0 -> 1 while the new bridge assembles
    Rectangle hazardZone;   // Region 4: the NULL gap (world coordinates)
    Vector2 playerFeet;     // the traveler's feet, canvas coordinates (for things that follow the player)
};

// Far and middle scenery (decoration only).
void drawRegionBackground(const SceneView& view);

// One natural platform per real node, plus the things that stand on nodes
// (checkpoint lantern, exit, relic, root hollow, bridge). Platform position and
// width come from the node; whether a platform is solid comes from the links.
void drawRegionPlatforms(const SceneView& view);

// Near scenery, mist, and vignette (decoration only).
void drawRegionForeground(const SceneView& view);

// A firefly-like light that points the way along a link.
void drawWisp(Vector2 position, float strength, Color color, float time);
