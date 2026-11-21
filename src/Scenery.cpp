#include "Scenery.h"

#include <cmath>

#include "DrawHelpers.h"
#include "Theme.h"

// Everything in this file is decoration. Positions come from fixed formulas,
// never from the lists, and nothing here can be stood on or followed.

namespace {

int regionAt(const World& world, float worldX) {
    for (int i = 0; i < REGION_COUNT; i++) {
        if (worldX < world.getRegion(i).right) return i;
    }
    return REGION_COUNT - 1;
}

// Region palette at a world position, blended across region boundaries.
RegionTheme themeAt(const World& world, float worldX) {
    const float BAND = 220.0f;
    int index = regionAt(world, worldX);
    int other = index;
    float t = 0.0f;
    const RegionInfo& region = world.getRegion(index);
    if (index < REGION_COUNT - 1 && region.right - worldX < BAND) {
        other = index + 1;
        t = 0.5f * (1.0f - (region.right - worldX) / BAND);
    } else if (index > 0 && worldX - region.left < BAND) {
        other = index - 1;
        t = 0.5f * (1.0f - (worldX - region.left) / BAND);
    }
    const RegionTheme& a = REGION_THEMES[index];
    const RegionTheme& b = REGION_THEMES[other];
    RegionTheme mixed;
    mixed.skyTop = lerpColor(a.skyTop, b.skyTop, t);
    mixed.skyHorizon = lerpColor(a.skyHorizon, b.skyHorizon, t);
    mixed.farHills = lerpColor(a.farHills, b.farHills, t);
    mixed.midScenery = lerpColor(a.midScenery, b.midScenery, t);
    mixed.accent = lerpColor(a.accent, b.accent, t);
    mixed.platformTop = lerpColor(a.platformTop, b.platformTop, t);
    mixed.platformBody = lerpColor(a.platformBody, b.platformBody, t);
    mixed.platformDark = lerpColor(a.platformDark, b.platformDark, t);
    mixed.light = lerpColor(a.light, b.light, t);
    return mixed;
}

float ridgeHeight(float x, float seed) {
    return std::sin(x * 0.004f + seed) * 40.0f + std::sin(x * 0.011f + seed * 2.0f) * 18.0f +
           std::sin(x * 0.023f + seed * 3.0f) * 7.0f;
}

void drawLineArrow(Vector2 from, Vector2 to, Color color) {
    DrawLineEx(from, to, 3.0f, color);
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f) return;
    dx /= length;
    dy /= length;
    DrawLineEx(to, Vector2{to.x - dx * 7.0f - dy * 5.0f, to.y - dy * 7.0f + dx * 5.0f}, 3.0f, color);
    DrawLineEx(to, Vector2{to.x - dx * 7.0f + dy * 5.0f, to.y - dy * 7.0f - dx * 5.0f}, 3.0f, color);
}

// Mid-distance prop: tree, stone pillar, giant mushroom, or ruined tower.
void drawProp(int region, float x, float base, float height, int seed, float time) {
    const RegionTheme& theme = REGION_THEMES[region];
    Color color = theme.midScenery;
    Color dark = lerpColor(color, OUTLINE_DARK, 0.35f);
    float scale = 0.8f + hash01(seed * 5 + 3) * 0.5f;
    float top = base - height;

    switch (region) {
        case 0: // tree
            DrawRectangle((int)(x - 7.0f), (int)top, 14, (int)height, dark);
            DrawCircleV(Vector2{x, top}, 46.0f * scale, color);
            DrawCircleV(Vector2{x - 30.0f * scale, top + 18.0f}, 32.0f * scale, color);
            DrawCircleV(Vector2{x + 30.0f * scale, top + 16.0f}, 34.0f * scale, color);
            break;
        case 1: // stone pillar
            DrawRectangle((int)(x - 17.0f), (int)top, 34, (int)height, color);
            DrawRectangle((int)(x - 24.0f), (int)(top - 10.0f), 48, 12, lerpColor(color, theme.farHills, 0.5f));
            for (float y = top + 24.0f; y < base; y += 30.0f) {
                DrawRectangle((int)(x - 17.0f), (int)y, 34, 2, dark);
            }
            break;
        case 2: { // giant mushroom with glowing spots
            DrawRectangle((int)(x - 8.0f), (int)top, 16, (int)height, dark);
            DrawCircleSector(Vector2{x, top + 4.0f}, 54.0f * scale, 180.0f, 360.0f, 20, color);
            for (int i = 0; i < 3; i++) {
                float glow = 0.35f + 0.25f * std::sin(time * 2.0f + seed + i * 2.0f);
                Vector2 spot = {x - 26.0f * scale + i * 26.0f * scale, top - 14.0f * scale + (i % 2) * 8.0f};
                DrawCircleV(spot, 4.0f, Fade(theme.light, glow));
            }
            break;
        }
        case 3: { // ruined tower with a blinking amber warning beacon
            DrawRectangle((int)(x - 20.0f), (int)top, 40, (int)height, color);
            DrawRectangle((int)(x - 20.0f), (int)(top - 10.0f), 12, 10, color);
            DrawRectangle((int)(x + 6.0f), (int)(top - 6.0f), 14, 6, color);
            DrawRectangle((int)(x - 4.0f), (int)(top + 30.0f), 8, 18, dark);
            bool on = std::sin(time * 3.0f + seed) > 0.2f;
            Vector2 beacon = {x, top - 18.0f};
            DrawCircleGradient((int)beacon.x, (int)beacon.y, 22.0f, Fade(theme.accent, on ? 0.55f : 0.12f),
                               Fade(theme.accent, 0.0f));
            DrawCircleV(beacon, 4.0f, Fade(theme.accent, on ? 1.0f : 0.4f));
            break;
        }
        default:
            break;
    }
}

// Region landmark: a numbered sign with the region's motif, at the region's start.
void drawLandmark(int region, float x, float time) {
    const RegionTheme& theme = REGION_THEMES[region];
    DrawRectangle((int)(x - 5.0f), 366, 10, 720 - 366, lerpColor(theme.platformDark, OUTLINE_DARK, 0.3f));

    Rectangle board = {x - 34.0f, 322.0f, 68.0f, 46.0f};
    float pulse = 0.14f + 0.06f * std::sin(time * 2.0f + region);
    DrawCircleGradient((int)x, (int)(board.y + 23.0f), 70.0f, Fade(theme.accent, pulse), Fade(theme.accent, 0.0f));
    DrawRectangleRounded(board, 0.25f, 4, Fade(UI_NAVY, 0.92f));
    DrawRectangleRoundedLinesEx(board, 0.25f, 4, 2.0f, theme.accent);
    DrawText(TextFormat("%d", region + 1), (int)(board.x + 8.0f), (int)(board.y + 13.0f), 20, theme.accent);

    Vector2 icon = {board.x + 44.0f, board.y + 23.0f};
    switch (region) {
        case 0: // one-way: next only
            drawLineArrow(Vector2{icon.x - 12.0f, icon.y}, Vector2{icon.x + 12.0f, icon.y}, theme.accent);
            break;
        case 1: // two-way: next and previous
            drawLineArrow(Vector2{icon.x - 12.0f, icon.y - 5.0f}, Vector2{icon.x + 12.0f, icon.y - 5.0f}, theme.accent);
            drawLineArrow(Vector2{icon.x + 12.0f, icon.y + 5.0f}, Vector2{icon.x - 12.0f, icon.y + 5.0f}, PREVIOUS_ORANGE);
            break;
        case 2: // loop
            DrawRing(icon, 7.0f, 10.0f, 0.0f, 300.0f, 24, theme.accent);
            DrawCircleV(Vector2{icon.x + 8.5f, icon.y - 5.0f}, 3.0f, theme.accent);
            break;
        case 3: // stop: NULL
            DrawPoly(icon, 8, 12.0f, 22.5f, theme.accent);
            DrawRectangle((int)(icon.x - 6.0f), (int)(icon.y - 1.5f), 12, 3, OUTLINE_DARK);
            break;
        default:
            break;
    }
}

} // namespace

void drawBackdrop(const World& world, float cameraX, float time) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const float cameraCenter = cameraX + width / 2.0f;

    // Sky: vertical gradient per column, blended between regions.
    for (int sx = 0; sx < width; sx += 8) {
        RegionTheme theme = themeAt(world, cameraX + sx + 4.0f);
        DrawRectangleGradientV(sx, 0, 8, 560, theme.skyTop, theme.skyHorizon);
        DrawRectangleGradientV(sx, 560, 8, height - 560, theme.skyHorizon, lerpColor(theme.skyHorizon, OUTLINE_DARK, 0.7f));
    }

    // Warm light source, far away (it does not scroll).
    RegionTheme here = themeAt(world, cameraCenter);
    DrawCircleGradient(760, 200, 280.0f, Fade(here.light, 0.16f), Fade(here.light, 0.0f));
    DrawCircleGradient(760, 200, 64.0f, Fade(here.light, 0.45f), Fade(here.light, 0.0f));
    DrawCircleV(Vector2{760.0f, 200.0f}, 24.0f, Fade(here.light, 0.8f));

    // Two distant ridges with slow parallax, stepped like pixel art.
    for (int sx = 0; sx < width; sx += 8) {
        RegionTheme theme = themeAt(world, cameraX + sx + 4.0f);
        float farY = std::floor((400.0f + ridgeHeight(cameraX * 0.15f + sx, 1.3f)) / 4.0f) * 4.0f;
        DrawRectangle(sx, (int)farY, 8, height - (int)farY, theme.farHills);
        float nearY = std::floor((520.0f + ridgeHeight(cameraX * 0.3f + sx, 4.1f) * 0.8f) / 4.0f) * 4.0f;
        DrawRectangle(sx, (int)nearY, 8, height - (int)nearY, lerpColor(theme.farHills, theme.midScenery, 0.6f));
    }

    // Mid-distance props (parallax 0.6), each tied to the region it starts in.
    const float propParallax = 0.6f;
    const float propSpacing = 290.0f;
    int propCount = (int)(WORLD_WIDTH / propSpacing) + 2;
    for (int i = 0; i < propCount; i++) {
        float homeX = 100.0f + i * propSpacing + (hash01(i * 7 + 1) - 0.5f) * 120.0f;
        float sx = width / 2.0f + propParallax * (homeX - cameraCenter);
        if (sx < -160.0f || sx > width + 160.0f) continue;
        drawProp(regionAt(world, homeX), sx, 640.0f, 140.0f + hash01(i * 11 + 5) * 130.0f, i, time);
    }

    // Region 2 water below the route, faded at the region's edges.
    const RegionInfo& waterRegion = world.getRegion(1);
    const Color water = {44, 98, 150, 255};
    for (int sx = 0; sx < width; sx += 8) {
        float worldX = cameraX + sx;
        float edgeDistance = std::fmin(worldX - waterRegion.left, waterRegion.right - worldX);
        if (edgeDistance <= 0.0f) continue;
        float alpha = std::fmin(edgeDistance / 160.0f, 1.0f);
        DrawRectangleGradientV(sx, 628, 8, height - 628, Fade(water, 0.75f * alpha), Fade(water, 0.0f));
    }
    for (int k = 0; k < 40; k++) {
        float worldX = waterRegion.left + 120.0f + hash01(k * 5) * (waterRegion.right - waterRegion.left - 240.0f);
        float sx = worldX - cameraX + std::sin(time * 1.5f + k) * 6.0f;
        if (sx < -40.0f || sx > width + 40.0f) continue;
        float y = 636.0f + hash01(k * 11) * 60.0f;
        DrawRectangle((int)sx, (int)y, 14 + (int)(hash01(k * 3) * 16.0f), 2, Fade(REGION_THEMES[1].light, 0.35f));
    }

    // Light motes: fireflies, sparkles, spores, and embers.
    const float moteParallax = 0.85f;
    for (int i = 0; i < 80; i++) {
        float homeX = i * 86.0f + hash01(i * 17) * 60.0f;
        float sx = width / 2.0f + moteParallax * (homeX - cameraCenter) + std::sin(time * 0.5f + i * 1.7f) * 16.0f;
        if (sx < -20.0f || sx > width + 20.0f) continue;
        int region = regionAt(world, homeX);
        float y = 120.0f + hash01(i * 3 + 1) * 400.0f + std::sin(time * 0.8f + i) * 12.0f;
        float twinkle = 0.5f + 0.5f * std::sin(time * 2.5f + i * 2.1f);
        Color color = (region == 3) ? REGION_THEMES[3].accent : REGION_THEMES[region].light;
        DrawCircleGradient((int)sx, (int)y, 8.0f, Fade(color, 0.25f * twinkle), Fade(color, 0.0f));
        DrawRectangle((int)sx - 1, (int)y - 1, 2, 2, Fade(color, 0.35f + 0.55f * twinkle));
    }

    // Region landmarks at each region's start.
    for (int i = 0; i < REGION_COUNT; i++) {
        float sx = world.getRegion(i).left + 40.0f - cameraX;
        if (sx < -100.0f || sx > width + 100.0f) continue;
        drawLandmark(i, sx, time);
    }

    // Fog at the bottom: falling down there is not a path.
    DrawRectangleGradientV(0, 600, width, height - 600, Fade(OUTLINE_DARK, 0.0f), Fade(OUTLINE_DARK, 0.65f));
}

void drawForeground(const World& world, float cameraX, float time) {
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const float cameraCenter = cameraX + width / 2.0f;
    const float parallax = 1.3f;
    const float spacing = 64.0f;

    int count = (int)(WORLD_WIDTH / spacing) + 12;
    for (int i = 0; i < count; i++) {
        float homeX = i * spacing + hash01(i * 19) * 40.0f - 200.0f;
        float sx = width / 2.0f + parallax * (homeX - cameraCenter);
        if (sx < -40.0f || sx > width + 40.0f) continue;

        int region = regionAt(world, homeX);
        const RegionTheme& theme = REGION_THEMES[region];
        Color color = lerpColor(theme.midScenery, OUTLINE_DARK, 0.65f);
        float tall = 16.0f + hash01(i * 13) * 30.0f;
        float base = (float)height;

        switch (region) {
            case 0: // grass blades
                for (int b = 0; b < 3; b++) {
                    float sway = std::sin(time * 2.0f + i + b) * 2.0f;
                    float bladeHeight = tall - b * 5.0f;
                    DrawRectangle((int)(sx + b * 6.0f - 6.0f + sway), (int)(base - bladeHeight), 3, (int)bladeHeight, color);
                }
                break;
            case 1: // reeds
                DrawRectangle((int)sx, (int)(base - tall - 10.0f), 2, (int)(tall + 10.0f), color);
                DrawRectangle((int)sx - 1, (int)(base - tall - 14.0f), 4, 10, color);
                DrawRectangle((int)(sx + 7.0f), (int)(base - tall), 2, (int)tall, color);
                break;
            case 2: // crystals with a faint glint
                DrawRectangle((int)(sx - 4.0f), (int)(base - tall), 8, (int)tall, color);
                DrawRectangle((int)(sx + 5.0f), (int)(base - tall * 0.6f), 6, (int)(tall * 0.6f), color);
                DrawRectangle((int)(sx - 4.0f), (int)(base - tall), 8, 2, Fade(theme.accent, 0.45f));
                break;
            case 3: // rubble
                DrawRectangleRounded(Rectangle{sx - 14.0f, base - tall * 0.6f, 28.0f, tall * 0.6f}, 0.5f, 4, color);
                break;
            default:
                break;
        }
    }
}
