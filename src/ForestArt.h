#pragma once

#include <cmath>

#include "raylib.h"
#include "Node.h"

// Region 1 is drawn into a small pixel canvas that is scaled up 2x, so the
// scenery, the ledges, and the traveler all share one chunky pixel size.
constexpr int CANVAS_WIDTH = 640;
constexpr int CANVAS_HEIGHT = 360;
constexpr float CANVAS_SCALE = 2.0f; // world pixels per canvas pixel

inline int toCanvasX(float worldX, int cameraX) { return (int)std::floor(worldX / CANVAS_SCALE) - cameraX; }
inline int toCanvasY(float worldY) { return (int)std::floor(worldY / CANVAS_SCALE); }

// What the forest renderer needs from gameplay. Read-only, non-owning pointers.
struct ForestView {
    int cameraX;            // camera left edge, in canvas pixels
    const Node* head;       // ledges are drawn by walking next from Region 1's head
    const Node* current;    // the node the player stands on
    const Node* checkpoint; // where recovery returns the player (it gets the lantern)
    const float* mistById;  // per node ID: 0 = clear, 1 = veiled in mist (not reachable now)
    bool hasGate;
    Rectangle gateRect;     // the exit arch opening, in world coordinates
    bool gateOpen;
    float checkpointFlash;  // 1 -> 0 just after the checkpoint lantern lights
    float time;
};

// Sky, sunlight, distant trees, the giant tree, and hanging vines (behind the route).
void drawForestBackground(int cameraX, float time);

// One natural ledge per real node, plus the checkpoint lantern and the exit arch.
// A ledge's look comes from its node's ID; whether it is solid comes from next.
void drawForestLedges(const ForestView& view);

// Ravine mist, dark framing plants, and a soft vignette (in front of the route).
void drawForestForeground(int cameraX, float time);

// A firefly that points the way to the ledge current->next leads to.
void drawForestWisp(Vector2 position, float strength, float time);

// Leaves and light where the player lands on a new ledge. t goes from 0 to 1.
void drawArrivalBurst(Vector2 position, float t);
