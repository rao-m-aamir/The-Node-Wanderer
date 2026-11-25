#pragma once

#include <cmath>
#include <vector>

#include "raylib.h"
#include "Lighting.h"
#include "Node.h"
#include "World.h"

// The world is drawn into a small pixel canvas that is scaled up 2x, so the scenery,
// the platforms, the traveler, and the effects all share one chunky pixel size.
constexpr int CANVAS_WIDTH = 640;
constexpr int CANVAS_HEIGHT = 360;
constexpr float CANVAS_SCALE = 2.0f; // world pixels per canvas pixel

inline int toCanvasX(float worldX, int cameraX) { return (int)std::floor(worldX / CANVAS_SCALE) - cameraX; }
inline int toCanvasY(float worldY) { return (int)std::floor(worldY / CANVAS_SCALE); }

// How long the art takes to show a node arriving or leaving in Regions 5 and 6 (the node itself
// exists, or is gone, from the moment push, pop, enqueue, or dequeue runs).
constexpr float STONE_SLIDE_SECONDS = 0.35f; // Region 5: a stone sliding out of the wall or back in
constexpr float BOAT_ARRIVE_SECONDS = 1.1f;  // Region 6: a boat rowing in to the rear of the line
constexpr float BOAT_LEAVE_SECONDS = 2.4f;   // Region 6: a boat that left the line rowing away

// A platform node that was just deleted (a stone popped off the stack, a boat that left the
// queue), kept by ID so the art can show it going. Only its rectangle is kept, never the node.
struct RemovedPiece {
    bool active = false;
    Rectangle rect = {}; // where its platform was, world coordinates
    float at = -100.0f;  // when it was removed (SceneView::time clock)
};

// What the region art needs from gameplay. Read-only, non-owning pointers only.
struct SceneView {
    int region;             // 0..5
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
    float gateProgress;     // 0 while the exit is closed, then 0 -> 1 as it opens
    float checkpointFlash;  // 1 -> 0 just after a checkpoint lights up
    float time;
    int stage;              // objective step (Region 2 key and brazier, Region 4 repair)
    float stageTime;        // seconds since the objective step changed
    float nodeTime;         // seconds since the player arrived on the current node
    bool emphasizePrevious; // Region 2: the step that goes back through previous
    Rectangle portalRect;   // Region 3: the hollow on the tail (world coordinates)
    float passage;          // Region 3: 0..1 while the current carries the traveler, otherwise -1
    bool bridgeBuilt;       // Region 4
    float bridgeProgress;   // Region 4: 0 -> 1 while the new bridge assembles
    float bridgeTime;       // Region 4: seconds since the bridge node was appended
    Rectangle hazardZone;   // Region 4: the NULL gap (world coordinates)
    const float* createdById;          // Regions 5-6: when each node ID was created (time clock; very early = from the start)
    const RemovedPiece* removedById;   // Regions 5-6: nodes just deleted (popped stones, boats that left), by ID
    bool moreToCome;        // Region 6: another boat can still be called
    Vector2 playerFeet;     // the traveler's feet, canvas coordinates
    float playerSpeed;      // horizontal speed, world pixels per second (signed)
    bool playerAirborne;
    const Node* guide;      // the node the guiding light points to (next, or previous on the way back)
    bool guideToGate;       // ...or the open exit, when the path has ended
    Color guideColor;
};

// 0 -> 1 with a gentle start and end, for easing state changes in the art.
inline float smooth01(float t) {
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// The wind at canvas position x: a steady breeze plus gusts that roll across the scene from
// left to right, so foliage, banners, and grass move in waves instead of on their own.
// About 0..1.4 at strength 1; positive blows to the right.
inline float windAt(float x, float time, float strength = 1.0f) {
    float breeze = 0.35f + 0.12f * std::sin(time * 0.37f + x * 0.004f);
    float wave = std::sin(time * 1.15f - x * 0.013f); // a gust front moving right
    float gust = (wave > 0.0f) ? wave * wave * wave : 0.0f;
    float gustiness = 0.55f + 0.45f * std::sin(time * 0.11f + 1.7f);
    return strength * (breeze + 0.95f * gust * gustiness);
}

// ---------------------------------------------------------------------------
// Baked art: painted once when a region is entered (see Paint.h), drawn every frame.
// ---------------------------------------------------------------------------

struct ArtLayer {
    Texture2D texture = {};
    float parallax = 0.0f; // 0 = fixed to the screen, 1 = moves with the route
    int y = 0;             // canvas y of its top edge
};

// Scenery that moves with the wind: drawn in horizontal slices that lean further the
// farther they are from where the piece is anchored, so it bends instead of sliding.
struct SwayPiece {
    Texture2D texture = {};
    int layer = 0;          // index in RegionArt::back (or front, if `front`)
    bool front = false;
    float x = 0.0f;         // top-left in its layer
    float y = 0.0f;
    float sway = 1.5f;      // canvas pixels it leans at full wind
    bool hangs = false;     // anchored at the top (vines, wisteria, banners) instead of the bottom
    int bands = 4;
    float lag = 0.0f;       // seconds it trails the wind by
};

// A platform's picture, painted from its node's position and width.
struct LedgeArt {
    int nodeId = -1;
    Texture2D picture = {};
    Texture2D mask = {};    // white silhouette, tinted for the mist veil
    int dx = 0;             // picture top-left relative to the node's canvas top-left
    int dy = 0;
};

struct RegionArt {
    int region = -1;
    std::vector<ArtLayer> back;  // far to near, behind the route
    std::vector<ArtLayer> front; // in front of the route
    std::vector<SwayPiece> sway;
    std::vector<LedgeArt> ledges;
    std::vector<Texture2D> extra; // region-specific pictures (props), unloaded with the rest
    std::vector<Vector2> spots;   // region-specific places that live pieces need (region-local canvas pixels)
    int waterline = -1;           // canvas y of a reflecting water surface, or -1

    void clear();
};

// Paints the region's art from its real list (ledges are painted by walking it from the head).
void bakeRegionArt(RegionArt& art, int region, const World& world);

// One frame of the region, in order. Water (if any) is drawn after the traveler, from a copy
// of the canvas so far, so it can reflect everything above it.
void drawRegionBackground(const SceneView& view, const RegionArt& art);
void drawRegionPlatforms(const SceneView& view, const RegionArt& art);
void drawRegionWater(const SceneView& view, const RegionArt& art, const Texture2D& reflection);
void drawRegionForeground(const SceneView& view, const RegionArt& art);
void drawRegionAdditive(const SceneView& view, const RegionArt& art, const SceneRenderer& renderer); // light shafts, glow
void addRegionLights(const SceneView& view, const RegionArt& art, std::vector<Light>& lights);
Mood regionMood(const SceneView& view);

// The guiding light: a stream of motes from the traveler toward where the link leads.
void drawGuideStream(const SceneView& view);
