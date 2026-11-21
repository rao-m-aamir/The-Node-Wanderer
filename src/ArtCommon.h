#pragma once

#include "Art.h"
#include "DrawHelpers.h"
#include "Theme.h"

// Shared building blocks for the four region art files. Not used by gameplay.

// Light shared by every region, so the cues read the same everywhere.
inline const Color WARM_LIGHT = {255, 222, 130, 255};  // next link, lanterns, fireflies
inline const Color COOL_LIGHT = {150, 210, 255, 255};  // previous link (Region 2)
inline const Color HAZARD_AMBER = {255, 170, 50, 255}; // the NULL gap (Region 4)
inline const Color NEAR_BLACK = {8, 14, 18, 255};

// One horizontal run of a platform's outline.
struct Span {
    int left;
    int right;
};

// The outline of one platform, row by row below its walkable top.
// Row 0 always matches the node's collision width exactly.
struct Silhouette {
    int top = 0;  // canvas y of the walkable surface (taken from the node)
    int rows = 0; // number of rows in use
    int count[CANVAS_HEIGHT] = {};
    Span spans[CANVAS_HEIGHT][2] = {};

    void add(int dy, int left, int right) {
        if (dy < 0 || dy >= CANVAS_HEIGHT || right <= left || count[dy] >= 2) return;
        spans[dy][count[dy]] = Span{left, right};
        count[dy]++;
        if (dy + 1 > rows) rows = dy + 1;
    }
};

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
    return ledge.left + ledge.width > -60 && ledge.left < CANVAS_WIDTH + 60;
}

// Colors for filling a silhouette: a top band (moss, slab) and a shaded body.
struct LedgeMaterial {
    Color rim;       // the walkable edge (row 0)
    Color top;       // the rest of the top band
    Color underLip;  // last row of the top band
    int topRows;     // thickness of the top band
    Color body;
    Color bodyShade; // edge away from the light
    Color bodyLit;   // edge facing the light
    Color deep;      // what the body fades to far below the top
};

// Darkens a color the deeper it is below the walkable top.
Color depthShade(Color color, int dy, Color deep);

void fillSilhouette(const Silhouette& shape, const LedgeMaterial& material, bool lightFromRight);

// Paints rows [fromDy, toDy) inside the outline (soil layers, mortar lines, bark bands).
void paintRows(const Silhouette& shape, int fromDy, int toDy, Color color, int inset);

// True if (x, dy) is inside the outline.
bool insideSilhouette(const Silhouette& shape, int x, int dy);

// Mist over a platform that no permitted link leads to from the current node.
void drawMistVeil(const Silhouette& shape, float amount, Color veil);

// Next link: warm light rising from the platform. Previous link: cool light falling onto it.
void drawNextGlow(const Ledge& ledge, float time, float strength = 1.0f);
void drawPreviousGlow(const Ledge& ledge, float time, float strength);

// A small lantern on a post at the given canvas position (checkpoints).
void drawLanternPost(int x, int baseY, float flash, float time);

// Grass blades along a walkable edge.
void drawGrassTufts(const Ledge& ledge, Color light, Color mid, float time);

// Per-region art (one file per region).
void drawForestBackground(const SceneView& view);
void drawForestPlatforms(const SceneView& view);
void drawForestForeground(const SceneView& view);
void drawRuinsBackground(const SceneView& view);
void drawRuinsPlatforms(const SceneView& view);
void drawRuinsForeground(const SceneView& view);
void drawGardenBackground(const SceneView& view);
void drawGardenPlatforms(const SceneView& view);
void drawGardenForeground(const SceneView& view);
void drawEdgeBackground(const SceneView& view);
void drawEdgePlatforms(const SceneView& view);
void drawEdgeForeground(const SceneView& view);
