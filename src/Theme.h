#pragma once

#include "raylib.h"

// Colors only. No gameplay or list data lives here.

// Per-region palette: scenery, platform material, and accent.
struct RegionTheme {
    Color skyTop;
    Color skyHorizon;
    Color farHills;     // distant silhouettes
    Color midScenery;   // mid-distance props (trees, pillars, mushrooms, ruins)
    Color accent;       // region accent: UI badge, reachable link, platform trim
    Color platformTop;  // walkable top edge
    Color platformBody;
    Color platformDark; // underside
    Color light;        // warm environmental light
};

inline const RegionTheme REGION_THEMES[4] = {
    // Region 1: leafy forest (singly linked)
    {{22, 34, 54, 255}, {72, 98, 92, 255}, {40, 66, 70, 255}, {26, 50, 46, 255},
     {130, 220, 110, 255}, {120, 200, 90, 255}, {120, 86, 58, 255}, {74, 52, 38, 255}, {255, 214, 140, 255}},
    // Region 2: cool stone and water (doubly linked)
    {{16, 30, 58, 255}, {58, 94, 128, 255}, {40, 64, 98, 255}, {28, 46, 76, 255},
     {110, 200, 255, 255}, {176, 200, 220, 255}, {98, 112, 134, 255}, {60, 68, 88, 255}, {200, 230, 255, 255}},
    // Region 3: violet bioluminescent grove (circular)
    {{26, 18, 50, 255}, {86, 58, 116, 255}, {58, 40, 90, 255}, {42, 28, 70, 255},
     {200, 130, 255, 255}, {200, 150, 255, 255}, {92, 62, 128, 255}, {56, 36, 84, 255}, {170, 255, 230, 255}},
    // Region 4: amber warning ruins (NULL safety)
    {{32, 22, 30, 255}, {116, 72, 50, 255}, {72, 46, 44, 255}, {52, 34, 36, 255},
     {255, 186, 70, 255}, {230, 190, 120, 255}, {116, 90, 70, 255}, {70, 52, 44, 255}, {255, 200, 120, 255}},
};

// Shared UI and state colors. The navy leans slightly green so it sits well in the forest.
inline const Color UI_NAVY = {10, 20, 28, 255};
inline const Color UI_TEXT = {236, 238, 248, 255};
inline const Color UI_DIM_TEXT = {150, 156, 180, 255};
inline const Color OBJECTIVE_YELLOW = {255, 222, 120, 255};
inline const Color CURRENT_GOLD = {255, 205, 70, 255};
inline const Color PREVIOUS_ORANGE = {255, 160, 90, 255};
inline const Color NULL_AMBER = {255, 176, 50, 255};
inline const Color CHECKPOINT_CREAM = {255, 240, 200, 255};
inline const Color OUTLINE_DARK = {14, 12, 24, 255};

inline Color lerpColor(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
