#pragma once

#include "raylib.h"

// Interface colors. Colors only: no gameplay or list data lives here.

// Each region's accent: `accent` for text on dark cards, `ink` for writing on journal paper.
struct RegionStyle {
    Color accent;
    Color ink;
};

inline const RegionStyle REGION_STYLES[6] = {
    {{150, 220, 110, 255}, {46, 108, 52, 255}},  // Region 1: forest green
    {{130, 190, 255, 255}, {40, 82, 150, 255}},  // Region 2: moonlit blue
    {{190, 160, 255, 255}, {98, 64, 160, 255}},  // Region 3: garden violet
    {{255, 186, 80, 255}, {158, 94, 18, 255}},   // Region 4: dawn amber
    {{255, 150, 128, 255}, {170, 62, 50, 255}},  // Region 5: lighthouse red
    {{96, 220, 204, 255}, {18, 116, 112, 255}},  // Region 6: harbor teal
};

// Dark cards for the in-play HUD (they lean slightly green so they sit well in every scene).
inline const Color UI_NAVY = {10, 20, 28, 255};
inline const Color UI_TEXT = {236, 238, 248, 255};
inline const Color UI_DIM_TEXT = {150, 156, 180, 255};
inline const Color OBJECTIVE_YELLOW = {255, 222, 120, 255};
inline const Color CURRENT_GOLD = {255, 205, 70, 255};
inline const Color NULL_AMBER = {255, 176, 50, 255};
inline const Color CHECKPOINT_CREAM = {255, 240, 200, 255};
inline const Color OUTLINE_DARK = {14, 12, 24, 255};

// The explorer's field journal (inspector, notes, pause, and the final summary).
inline const Color PARCHMENT = {238, 224, 192, 255};
inline const Color PARCHMENT_SHADE = {212, 192, 152, 255};
inline const Color INK = {48, 36, 30, 255};
inline const Color INK_SOFT = {112, 90, 72, 255};
inline const Color INK_NULL = {168, 60, 30, 255}; // NULL is always written in this red-brown, with a stop mark
inline const Color LEATHER = {86, 52, 38, 255};
inline const Color LEATHER_DARK = {54, 32, 24, 255};
inline const Color GILT = {222, 186, 110, 255};

inline Color lerpColor(Color a, Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), (unsigned char)(a.a + (b.a - a.a) * t)};
}
