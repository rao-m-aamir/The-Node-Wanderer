#include "ArtCommon.h"

// ---------------------------------------------------------------------------
// Which region file draws the active region.
// ---------------------------------------------------------------------------

void drawRegionBackground(const SceneView& view) {
    switch (view.region) {
        case 0: drawForestBackground(view); break;
        case 1: drawRuinsBackground(view); break;
        case 2: drawGardenBackground(view); break;
        default: drawEdgeBackground(view); break;
    }
}

void drawRegionPlatforms(const SceneView& view) {
    switch (view.region) {
        case 0: drawForestPlatforms(view); break;
        case 1: drawRuinsPlatforms(view); break;
        case 2: drawGardenPlatforms(view); break;
        default: drawEdgePlatforms(view); break;
    }
}

void drawRegionForeground(const SceneView& view) {
    switch (view.region) {
        case 0: drawForestForeground(view); break;
        case 1: drawRuinsForeground(view); break;
        case 2: drawGardenForeground(view); break;
        default: drawEdgeForeground(view); break;
    }
}

void drawWisp(Vector2 position, float strength, Color color, float time) {
    float flicker = 0.75f + 0.25f * std::sin(time * 9.0f);
    DrawCircleGradient((int)position.x, (int)position.y, 9.0f, withAlpha(color, 0.35f * flicker * strength),
                       withAlpha(color, 0.0f));
    DrawRectangle((int)position.x, (int)position.y, 2, 2, withAlpha(lerpColor(color, WHITE, 0.7f), 0.95f * strength));
}

// ---------------------------------------------------------------------------
// Silhouettes and materials
// ---------------------------------------------------------------------------

Color depthShade(Color color, int dy, Color deep) {
    float t = (dy - 20) / 100.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 0.75f) t = 0.75f;
    return lerpColor(color, deep, t);
}

void fillSilhouette(const Silhouette& shape, const LedgeMaterial& m, bool lightFromRight) {
    for (int dy = 0; dy < shape.rows; dy++) {
        for (int i = 0; i < shape.count[dy]; i++) {
            const Span& span = shape.spans[dy][i];
            int y = shape.top + dy;
            Color base = (dy == 0) ? m.rim : (dy < m.topRows - 1) ? m.top : (dy < m.topRows) ? m.underLip : m.body;
            Color shade = (dy < m.topRows) ? m.underLip : m.bodyShade;
            Color lit = (dy < m.topRows) ? m.rim : m.bodyLit;
            if (!lightFromRight) {
                Color swap = shade;
                shade = lit;
                lit = swap;
            }
            DrawRectangle(span.left, y, span.right - span.left, 1, depthShade(base, dy, m.deep));
            DrawRectangle(span.left, y, 2, 1, depthShade(shade, dy, m.deep));
            DrawRectangle(span.right - 1, y, 1, 1, depthShade(lit, dy, m.deep));
        }
    }
}

void paintRows(const Silhouette& shape, int fromDy, int toDy, Color color, int inset) {
    for (int dy = fromDy; dy < toDy && dy < shape.rows; dy++) {
        if (dy < 0) continue;
        for (int i = 0; i < shape.count[dy]; i++) {
            const Span& span = shape.spans[dy][i];
            int left = span.left + inset;
            int right = span.right - inset;
            if (right > left) DrawRectangle(left, shape.top + dy, right - left, 1, color);
        }
    }
}

bool insideSilhouette(const Silhouette& shape, int x, int dy) {
    if (dy < 0 || dy >= shape.rows) return false;
    for (int i = 0; i < shape.count[dy]; i++) {
        if (x >= shape.spans[dy][i].left && x < shape.spans[dy][i].right) return true;
    }
    return false;
}

void drawMistVeil(const Silhouette& shape, float amount, Color veil) {
    if (amount <= 0.01f) return;
    paintRows(shape, 0, shape.rows, withAlpha(veil, 0.55f * amount), 0);
}

// ---------------------------------------------------------------------------
// Link cues and small shared objects
// ---------------------------------------------------------------------------

void drawNextGlow(const Ledge& ledge, float time, float strength) {
    float pulse = strength * (0.6f + 0.4f * std::sin(time * 3.0f));
    DrawRectangleGradientV(ledge.left, ledge.top - 18, ledge.width, 18, withAlpha(WARM_LIGHT, 0.0f),
                           withAlpha(WARM_LIGHT, 0.22f * pulse));
    DrawRectangle(ledge.left, ledge.top, ledge.width, 1, withAlpha(WARM_LIGHT, 0.55f * pulse));
    for (int i = 0; i < 6; i++) { // motes rising from the surface
        float t = std::fmod(time * 0.5f + i * 0.17f, 1.0f);
        int x = ledge.left + 6 + (int)(hash01(ledge.seed * 3 + i) * (ledge.width - 12));
        DrawRectangle(x, ledge.top - 2 - (int)(t * 18.0f), 1, 1, withAlpha(WARM_LIGHT, pulse * (1.0f - t)));
    }
}

void drawPreviousGlow(const Ledge& ledge, float time, float strength) {
    float pulse = strength * (0.6f + 0.4f * std::sin(time * 2.2f + 1.0f));
    DrawRectangleGradientV(ledge.left, ledge.top - 14, ledge.width, 14, withAlpha(COOL_LIGHT, 0.0f),
                           withAlpha(COOL_LIGHT, 0.2f * pulse));
    DrawRectangle(ledge.left, ledge.top, ledge.width, 1, withAlpha(COOL_LIGHT, 0.6f * pulse));
    for (int i = 0; i < 6; i++) { // sparkles drifting down onto the surface (the opposite motion to next)
        float t = std::fmod(time * 0.45f + i * 0.19f, 1.0f);
        int x = ledge.left + 6 + (int)(hash01(ledge.seed * 5 + i) * (ledge.width - 12));
        DrawRectangle(x, ledge.top - 20 + (int)(t * 18.0f), 1, 1, withAlpha(COOL_LIGHT, pulse * t));
    }
}

void drawLanternPost(int x, int baseY, float flash, float time) {
    const Color wood = {64, 44, 38, 255};
    DrawRectangle(x, baseY - 18, 2, 18, wood);    // post
    DrawRectangle(x, baseY - 18, 7, 1, wood);     // arm
    DrawRectangle(x + 6, baseY - 17, 1, 2, wood); // hook

    float flicker = 0.85f + 0.15f * std::sin(time * 11.0f) * std::sin(time * 6.3f);
    int lx = x + 4;
    int ly = baseY - 15;
    DrawCircleGradient(lx + 2, ly + 3, 20.0f + 18.0f * flash, withAlpha(WARM_LIGHT, 0.3f * flicker + 0.35f * flash),
                       withAlpha(WARM_LIGHT, 0.0f));
    DrawRectangle(lx + 1, ly - 1, 3, 1, wood); // cap
    DrawRectangle(lx, ly, 5, 6, wood);         // frame
    DrawRectangle(lx + 1, ly + 1, 3, 4, (flicker > 0.9f) ? WARM_LIGHT : Color{255, 196, 100, 255});
    DrawRectangle(lx + 2, ly + 2, 1, 2, WHITE); // flame

    for (int i = 0; flash > 0.0f && i < 10; i++) { // sparks when the checkpoint lights up
        float angle = i * 0.628f + 0.3f;
        float distance = (1.0f - flash) * 16.0f;
        DrawRectangle(lx + 2 + (int)(std::cos(angle) * distance),
                      ly + 3 + (int)(std::sin(angle) * distance - (1.0f - flash) * 6.0f), 1, 1,
                      withAlpha(WARM_LIGHT, flash));
    }
}

void drawGrassTufts(const Ledge& ledge, Color light, Color mid, float time) {
    for (int x = ledge.left + 1; x < ledge.left + ledge.width - 1; x += 3) {
        float h = hash01(ledge.seed * 131 + x);
        if (h < 0.35f) continue;
        int height = 1 + (int)(h * 3.0f);
        int sway = (h > 0.8f && std::sin(time * 2.0f + x * 0.3f) > 0.3f) ? 1 : 0;
        DrawRectangle(x + sway, ledge.top - height, 1, height, (h > 0.7f) ? light : mid);
    }
}
