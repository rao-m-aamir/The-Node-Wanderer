#pragma once

#include <cmath>
#include <vector>

#include "Art.h"
#include "DrawHelpers.h"
#include "Paint.h"
#include "Theme.h"

// Shared art: the light language, platform pictures, layers, and small live pieces that
// every region uses. Each region's own look lives in its art file.

// Light shared by every region, so the cues read the same everywhere.
inline const Color WARM_LIGHT = {255, 222, 130, 255};   // the way forward (next), lanterns
inline const Color COOL_LIGHT = {150, 210, 255, 255};   // the way back (previous, Region 2)
inline const Color SPRING_LIGHT = {120, 255, 230, 255}; // the garden's current (Region 3)
inline const Color HAZARD_AMBER = {255, 170, 50, 255};  // the missing link (Region 4)
inline const Color NEAR_BLACK = {8, 14, 18, 255};

// A platform in canvas pixels, taken from its node.
struct Ledge {
    int left;
    int top;
    int width;
    int seed; // stable per node, for repeatable details
};

inline Ledge ledgeFor(const Node& node, int cameraX) {
    return Ledge{toCanvasX(node.platformPosition.x, cameraX), toCanvasY(node.platformPosition.y),
                 (int)(node.platformWidth / CANVAS_SCALE), node.id * 37 + 11};
}

inline bool ledgeOnScreen(const Ledge& ledge) {
    return ledge.left + ledge.width > -80 && ledge.left < CANVAS_WIDTH + 80;
}

// For anything that loops with fmod (t goes 0 -> 1, then wraps): it rises from 0, holds,
// and fades back to 0, so the wrap never shows as a pop.
inline float loopEnvelope(float t) {
    float in = std::fmin(1.0f, t / 0.15f);
    float out = std::fmin(1.0f, (1.0f - t) / 0.35f);
    return std::fmax(0.0f, in * out);
}

// Link cues ease in after the player arrives on a node instead of switching on.
inline float arrivalEase(const SceneView& view) { return smooth01(view.nodeTime / 0.5f); }

// ---------------------------------------------------------------------------
// Regions: each art file fills one of these.
// ---------------------------------------------------------------------------

struct RegionPainter {
    void (*bake)(RegionArt& art, const World& world);
    void (*background)(const SceneView& view, const RegionArt& art);
    void (*platforms)(const SceneView& view, const RegionArt& art);
    void (*water)(const SceneView& view, const RegionArt& art, const Texture2D& reflection); // may be null
    void (*foreground)(const SceneView& view, const RegionArt& art);
    void (*additive)(const SceneView& view, const RegionArt& art, const SceneRenderer& renderer);
    void (*lights)(const SceneView& view, const RegionArt& art, std::vector<Light>& lights);
    Mood (*mood)(const SceneView& view);
};

extern const RegionPainter FOREST_PAINTER;
extern const RegionPainter RUINS_PAINTER;
extern const RegionPainter GARDEN_PAINTER;
extern const RegionPainter EDGE_PAINTER;

// ---------------------------------------------------------------------------
// Baking (when a region is entered)
// ---------------------------------------------------------------------------

// Region-local canvas x of a world x (the region's left edge is 0).
inline int localX(float worldX, float regionLeft) {
    return (int)std::floor(worldX / CANVAS_SCALE) - (int)std::floor(regionLeft / CANVAS_SCALE);
}

// How far the camera can scroll inside a region, in canvas pixels.
int scrollRangeOf(const RegionInfo& region);

// Width a parallax layer needs so it covers the screen at every scroll position.
inline int layerWidth(int scrollRange, float parallax) {
    return CANVAS_WIDTH + (int)std::ceil(scrollRange * parallax) + 2;
}

// A picture for one platform: the node's width plus `margin` on each side, from `above`
// rows over the walkable top down to the bottom of the canvas. The node's top-left corner
// sits at (originX, originY), and row originY is exactly the walkable top.
struct LedgeCanvas {
    Picture picture;
    int originX = 0;
    int originY = 0;
    int width = 0; // the node's width in canvas pixels
    int top = 0;   // the node's top in canvas pixels
};
LedgeCanvas makeLedgeCanvas(const Node& node, int margin, int above);
void storeLedge(RegionArt& art, const Node& node, const LedgeCanvas& canvas);
void storeLayer(RegionArt& art, const Picture& picture, float parallax, int y, bool front = false);
void storeEmptyLayer(RegionArt& art, float parallax, bool front = false); // a layer that only holds sway pieces
void storeSway(RegionArt& art, const Picture& piece, int layer, bool front, float x, float y, float sway, bool hangs,
               int bands = 4, float lag = 0.0f);

// Painting kits shared by the regions.
void paintFoliage(Picture& p, float cx, float cy, float r, const Color* ramp, int steps, Vector2 toLight, int seed,
                  float roughness = 0.35f);
void paintTrunk(Picture& p, float x, float yTop, float yBottom, float halfTop, float halfBottom, const Color* ramp,
                int steps, Vector2 toLight, int seed);

// ---------------------------------------------------------------------------
// Drawing (every frame)
// ---------------------------------------------------------------------------

void drawBackLayers(const SceneView& view, const RegionArt& art, int from, int to, float wind);
void drawFrontLayers(const SceneView& view, const RegionArt& art, float wind);

// Every node's picture, walked from the head, with the mist veil over the ones that are not solid now.
void drawLedges(const SceneView& view, const RegionArt& art, Color veil);

// The bright lip and drifting motes on a platform a link leads to (rising = next, falling = previous).
void drawLinkLip(const Ledge& ledge, Color color, float strength, float time, bool rising);
void addLinkLight(std::vector<Light>& lights, const Ledge& ledge, Color color, float strength);

// Grass along a walkable top that bends with the wind and away from the traveler.
void drawGrass(const Ledge& ledge, const SceneView& view, Color tip, Color stem, float wind, float density);

// A hanging strand (vine, moss, root) that swings more toward its tip.
void drawStrand(int x, int y, int length, Color color, Color leaf, float lean, int seed);

// A checkpoint lantern on a post, and its light.
void drawLanternPost(int x, int baseY, float flash, float time);
void addLanternLight(std::vector<Light>& lights, int x, int baseY, float flash, float time);

// Where the guiding light is heading, in canvas pixels; false if there is nowhere to go.
bool guideTarget(const SceneView& view, Vector2& target);

// A four-cornered shape with its own color at each corner, blended across (light shafts, rays).
// Corners go around the shape in order.
void drawGradientQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color ca, Color cb, Color cc, Color cd);

// Screen x of something placed at layer x in a layer with this parallax.
inline float layerToScreen(float x, int scroll, float parallax) { return x - std::floor(scroll * parallax); }
