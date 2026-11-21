#pragma once

#include "raylib.h"

// Small drawing utilities shared by the art files. No gameplay or list logic.

// Multiplies a color's alpha (0..1) instead of replacing it.
inline Color withAlpha(Color color, float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    color.a = (unsigned char)(color.a * alpha);
    return color;
}

// DrawTriangle needs counter-clockwise order on screen; this swaps the points if needed.
inline void drawTriangleSafe(Vector2 a, Vector2 b, Vector2 c, Color color) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0.0f) {
        Vector2 swap = b;
        b = c;
        c = swap;
    }
    DrawTriangle(a, b, c, color);
}

// Repeatable pseudo-random number in [0, 1) for placing decoration.
inline float hash01(int seed) {
    unsigned int x = (unsigned int)seed * 2654435761u;
    x ^= x >> 13;
    x *= 0x5bd1e995u;
    x ^= x >> 15;
    return (float)(x & 0xFFFFFFu) / 16777216.0f;
}
