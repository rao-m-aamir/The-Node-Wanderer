#include "ArtCommon.h"

// Region 2, "The Forgotten Return": flooded ruins at blue hour, cool moonlight
// and warm braziers. Platforms are drawn only from their node's position and width;
// which ones the player can reach comes from next and previous.

namespace {

const Color SKY_TOP = {10, 16, 36, 255};
const Color SKY_MID = {28, 46, 82, 255};
const Color SKY_LOW = {76, 100, 136, 255};
const Color MOON = {226, 236, 255, 255};
const Color FAR_RUINS = {46, 64, 98, 255};
const Color MID_RUINS = {28, 40, 64, 255};
const Color MID_RUINS_LIT = {54, 72, 104, 255};
const Color MIST = {150, 178, 210, 255};
const Color VEIL = {92, 116, 148, 255};
const Color DEPTH = {16, 24, 42, 255};
const Color DARK = {6, 10, 20, 255};
const Color WATER = {34, 62, 100, 255};
const Color WATER_DEEP = {8, 14, 30, 255};

const Color STONE_LIGHT = {184, 192, 204, 255};
const Color STONE = {120, 130, 150, 255};
const Color STONE_DARK = {72, 80, 104, 255};
const Color STONE_SHADE = {48, 56, 80, 255};
const Color MOSS_LIGHT = {156, 204, 150, 255};
const Color MOSS = {94, 146, 112, 255};
const Color MOSS_DARK = {50, 92, 82, 255};
const Color ROOT = {70, 56, 52, 255};
const Color BANNER = {148, 54, 62, 255};
const Color BANNER_DARK = {100, 36, 48, 255};
const Color RELIC_GOLD = {214, 176, 96, 255};
const Color FIRE = {255, 168, 76, 255};
const Color FIRE_HOT = {255, 232, 150, 255};

const int WATERLINE = 292; // canvas y of the water surface

const LedgeMaterial RUIN_STONE = {STONE_LIGHT, STONE, STONE_DARK, 4, STONE, STONE_SHADE, STONE_LIGHT, DEPTH};

// ---------------------------------------------------------------------------
// Platform shapes, one per node ID.
// ---------------------------------------------------------------------------

enum class RuinShape { Gatehouse, BrokenArch, Capital, Shrine, Steps };

RuinShape shapeFor(const Node& node) {
    switch (node.id) {
        case 1: return RuinShape::Gatehouse;
        case 2: return RuinShape::BrokenArch;
        case 3: return RuinShape::Capital;
        case 4: return RuinShape::Shrine;
        default: return RuinShape::Steps;
    }
}

Silhouette buildRuinSilhouette(RuinShape shape, const Ledge& l) {
    Silhouette s;
    s.top = l.top;
    const int x0 = l.left;
    const int x1 = l.left + l.width;
    const int w = l.width;
    const int cx = x0 + w / 2;
    for (int dy = 0; l.top + dy < CANVAS_HEIGHT; dy++) {
        switch (shape) {
            case RuinShape::Gatehouse: { // big cut blocks on a foundation that spreads into the water
                int inset = (dy < 4) ? 0 : (dy < 30 ? 1 : 1 - (dy - 30) / 6);
                if (inset < -6) inset = -6;
                s.add(dy, x0 + inset, x1 - inset);
                break;
            }
            case RuinShape::BrokenArch: { // a bridge span with an arch underneath; its right pier broke off
                const int deck = 8;
                const int pier = 26;
                const int radius = w / 2 - pier;
                if (dy < deck) {
                    int lip = (dy >= 2 && dy < 5) ? 1 : 0; // a thin parapet band
                    s.add(dy, x0 - lip, x1 + lip);
                } else if (dy - deck < radius) {
                    float d = (float)(radius - (dy - deck)); // height above the arch's spring line
                    int half = (int)std::sqrt(std::fmax(0.0f, (float)(radius * radius) - d * d));
                    s.add(dy, x0, cx - half);
                    s.add(dy, cx + half, x1);
                } else {
                    s.add(dy, x0, x0 + pier);
                    int broken = dy - deck - radius;
                    if (broken < 26) s.add(dy, x1 - pier + broken / 3, x1 - broken / 5);
                }
                break;
            }
            case RuinShape::Capital: { // a carved column capital on a fluted shaft
                if (dy < 5) {
                    s.add(dy, x0, x1);
                } else if (dy < 13) {
                    float t = (dy - 5) / 8.0f;
                    int inset = (int)(w * 0.3f * t * t);
                    s.add(dy, x0 + inset, x1 - inset);
                } else {
                    int half = w * 20 / 100;
                    s.add(dy, cx - half, cx + half);
                }
                break;
            }
            case RuinShape::Shrine: { // an altar with two steps, on a pedestal
                if (dy < 3) s.add(dy, x0, x1);
                else if (dy < 20) s.add(dy, x0 + 4, x1 - 4);
                else if (dy < 24) s.add(dy, x0 + 1, x1 - 1);
                else if (dy < 28) s.add(dy, x0 - 2, x1 + 2);
                else {
                    int half = w * 28 / 100 + (dy - 28) / 10;
                    s.add(dy, cx - half, cx + half);
                }
                break;
            }
            case RuinShape::Steps: { // a landing with old stairs going down into the water on the left
                if (dy < 4) {
                    s.add(dy, x0, x1);
                } else {
                    int step = (dy - 4) / 6;
                    if (step > 5) step = 5;
                    s.add(dy, x0 - step * 7, x1 - 1);
                }
                break;
            }
        }
    }
    return s;
}

// ---------------------------------------------------------------------------
// Platform details
// ---------------------------------------------------------------------------

// Mortar lines between cut stones, clipped to the outline. Joints are offset on every course.
void drawMasonry(const Silhouette& s, int fromDy, int toDy, int course, int block, int seed) {
    for (int dy = fromDy; dy < toDy && dy < s.rows; dy += course) {
        paintRows(s, dy, dy + 1, depthShade(STONE_SHADE, dy, DEPTH), 1);
        int offset = ((dy / course) % 2) * (block / 2) + (seed % 5);
        for (int i = 0; i < s.count[dy]; i++) {
            const Span& span = s.spans[dy][i];
            for (int x = span.left + offset; x < span.right - 1; x += block) {
                for (int k = 1; k < course; k++) {
                    if (insideSilhouette(s, x, dy + k)) DrawRectangle(x, s.top + dy + k, 1, 1, depthShade(STONE_SHADE, dy + k, DEPTH));
                }
            }
        }
    }
}

void drawTopMoss(const Ledge& l, int count) {
    for (int i = 0; i < count; i++) {
        int x = l.left + 1 + (int)(hash01(l.seed * 43 + i) * (l.width - 6));
        int width = 2 + i % 3;
        DrawRectangle(x, l.top - 1, width, 1, MOSS_LIGHT);
        DrawRectangle(x - 1, l.top, width + 2, 1, MOSS);
        DrawRectangle(x + 1, l.top + 1, 1, 1 + i % 3, MOSS_DARK);
    }
}

// A root or vine hanging from a point and swaying a little.
void drawHangingRoot(int x, int y, int length, float time, int seed, Color color) {
    for (int k = 0; k < length; k++) {
        int sway = (int)std::round(std::sin(time * 1.2f + seed + k * 0.15f) * (k / (float)length) * 2.0f);
        DrawRectangle(x + sway + (k / 7) * ((seed % 2) ? 1 : -1), y + k, 1, 1, color);
    }
}

// A stone brazier. Lit = warm fire. `waiting` = a faint cool shimmer that marks it as the goal.
void drawBrazier(int x, int baseY, bool lit, float waiting, float time) {
    DrawRectangle(x + 3, baseY - 5, 2, 5, STONE_DARK); // stand
    DrawRectangle(x, baseY - 8, 8, 3, STONE);          // bowl
    DrawRectangle(x + 1, baseY - 5, 6, 1, STONE_DARK);
    DrawRectangle(x, baseY - 8, 8, 1, STONE_LIGHT);
    if (lit) {
        float flicker = 0.8f + 0.2f * std::sin(time * 13.0f);
        DrawCircleGradient(x + 4, baseY - 12, 26.0f, withAlpha(FIRE, 0.38f * flicker), withAlpha(FIRE, 0.0f));
        for (int i = 0; i < 4; i++) { // flame tongues
            int height = 3 + (int)((0.5f + 0.5f * std::sin(time * (9.0f + i * 2.3f) + i)) * 4.0f);
            DrawRectangle(x + 1 + i * 2, baseY - 8 - height, 2, height, (i % 2 == 0) ? FIRE : FIRE_HOT);
        }
        for (int i = 0; i < 3; i++) { // embers
            float t = std::fmod(time * 0.8f + i * 0.33f, 1.0f);
            DrawRectangle(x + 2 + i * 2 + (int)(std::sin(time * 3.0f + i) * 2.0f), baseY - 12 - (int)(t * 16.0f), 1, 1,
                          withAlpha(FIRE_HOT, 1.0f - t));
        }
    } else if (waiting > 0.0f) {
        float pulse = waiting * (0.5f + 0.5f * std::sin(time * 3.0f));
        DrawCircleGradient(x + 4, baseY - 10, 16.0f, withAlpha(COOL_LIGHT, 0.3f * pulse), withAlpha(COOL_LIGHT, 0.0f));
        DrawRectangle(x + 2, baseY - 9, 4, 1, withAlpha(COOL_LIGHT, pulse));
    }
}

// The key relic floating above the shrine (until the player reaches it).
void drawRelic(int cx, int y, float time) {
    int bob = (int)std::round(std::sin(time * 2.0f) * 2.0f);
    int top = y + bob;
    DrawCircleGradient(cx, top + 2, 16.0f, withAlpha(RELIC_GOLD, 0.4f), withAlpha(RELIC_GOLD, 0.0f));
    DrawRing(Vector2{(float)cx, (float)top}, 1.0f, 3.0f, 0.0f, 360.0f, 12, RELIC_GOLD); // bow of the key
    DrawRectangle(cx - 1, top + 2, 2, 6, RELIC_GOLD);                                     // shaft
    DrawRectangle(cx + 1, top + 5, 2, 1, RELIC_GOLD);                                     // teeth
    DrawRectangle(cx + 1, top + 7, 2, 1, RELIC_GOLD);
    for (int i = 0; i < 4; i++) { // sparkles circling it
        float angle = time * 1.5f + i * 1.5708f;
        DrawRectangle(cx + (int)(std::cos(angle) * 9.0f), top + 3 + (int)(std::sin(angle) * 5.0f), 1, 1,
                      withAlpha(FIRE_HOT, 0.8f));
    }
}

// The relic's light, carried back by the traveler while going back through previous.
void drawCarriedLight(Vector2 feet, float time) {
    float x = feet.x - 10.0f + std::sin(time * 2.0f) * 2.0f;
    float y = feet.y - 34.0f + std::sin(time * 3.1f) * 2.0f;
    DrawCircleGradient((int)x, (int)y, 10.0f, withAlpha(COOL_LIGHT, 0.45f), withAlpha(COOL_LIGHT, 0.0f));
    DrawRectangle((int)x - 1, (int)y - 1, 2, 2, WHITE);
    DrawRectangle((int)x + 2, (int)y + 1, 1, 1, withAlpha(RELIC_GOLD, 0.9f));
}

void drawRuinDetails(RuinShape shape, const Ledge& l, const Silhouette& s, const SceneView& view) {
    const int x0 = l.left;
    const int x1 = l.left + l.width;
    const int cx = x0 + l.width / 2;
    const float time = view.time;
    switch (shape) {
        case RuinShape::Gatehouse:
            drawMasonry(s, 4, 60, 9, 22, l.seed);
            paintRows(s, 13, 15, STONE_LIGHT, 2); // carved band
            for (int x = x0 + 4; x < x1 - 4; x += 5) DrawRectangle(x, l.top + 14, 2, 1, STONE_SHADE);
            DrawRectangle(cx - 5, l.top + 22, 10, 14, STONE_SHADE); // a dark arched window
            DrawCircleSector(Vector2{(float)cx, (float)(l.top + 22)}, 5.0f, 180.0f, 360.0f, 10, STONE_SHADE);
            drawTopMoss(l, 6);
            drawHangingRoot(x1 - 3, l.top + 2, 22, time, l.seed, ROOT);
            drawHangingRoot(x0 + 8, l.top + 3, 14, time, l.seed + 3, MOSS_DARK);
            break;
        case RuinShape::BrokenArch: {
            paintRows(s, 2, 3, STONE_LIGHT, 0); // parapet edge
            const int radius = l.width / 2 - 26;
            for (int i = 0; i <= 8; i++) { // voussoirs around the arch
                float angle = 3.14159f + i * 3.14159f / 8.0f;
                DrawRectangle(cx + (int)(std::cos(angle) * (radius + 3)), l.top + 8 + radius + (int)(std::sin(angle) * (radius + 3)),
                              1, 2, STONE_SHADE);
            }
            DrawRectangle(cx - 2, l.top + 8, 4, 4, STONE_LIGHT); // keystone
            drawMasonry(s, 14, 90, 8, 13, l.seed);
            drawTopMoss(l, 4);
            drawHangingRoot(x1 - 20, l.top + 8 + radius + 24, 18, time, l.seed, ROOT); // roots below the break
            drawHangingRoot(x1 - 12, l.top + 8 + radius + 20, 26, time, l.seed + 1, ROOT);
            break;
        }
        case RuinShape::Capital: {
            paintRows(s, 0, 1, STONE_LIGHT, 0);
            paintRows(s, 4, 5, STONE_SHADE, 0); // under the slab
            for (int side = 0; side < 2; side++) { // scroll carvings at the corners
                int vx = (side == 0) ? x0 + 6 : x1 - 9;
                DrawRing(Vector2{vx + 1.5f, l.top + 8.0f}, 1.0f, 2.5f, 0.0f, 360.0f, 10, STONE_SHADE);
            }
            int left = 0;
            int right = 0;
            for (int dy = 14; dy < s.rows; dy++) { // fluting on the shaft
                if (s.count[dy] == 0) continue;
                left = s.spans[dy][0].left;
                right = s.spans[dy][0].right;
                int width = right - left;
                for (int f = 1; f < 4; f++) {
                    DrawRectangle(left + width * f / 4, l.top + dy, 1, 1, depthShade(f == 3 ? STONE_LIGHT : STONE_SHADE, dy, DEPTH));
                }
            }
            for (int k = 0; k < 12; k++) DrawRectangle(cx + 4 + (k % 2) - k / 5, l.top + 20 + k, 1, 1, STONE_SHADE); // crack
            drawTopMoss(l, 3);
            drawHangingRoot(x0 + 10, l.top + 5, 16, time, l.seed, MOSS_DARK);
            break;
        }
        case RuinShape::Shrine: {
            drawMasonry(s, 3, 20, 6, 14, l.seed);
            bool relicHere = view.stage == 0;
            Color niche = relicHere ? lerpColor(STONE_SHADE, RELIC_GOLD, 0.35f) : STONE_SHADE;
            DrawRectangle(cx - 6, l.top + 6, 12, 12, niche); // carved niche
            DrawCircleSector(Vector2{(float)cx, (float)(l.top + 6)}, 6.0f, 180.0f, 360.0f, 10, niche);
            paintRows(s, 20, 21, STONE_LIGHT, 0); // step edges
            paintRows(s, 24, 25, STONE_LIGHT, 0);
            for (int x = x0 + 7; x < x1 - 7; x += 6) DrawRectangle(x, l.top + 3, 3, 1, RELIC_GOLD); // a faded gold band
            drawTopMoss(l, 3);
            if (relicHere) drawRelic(cx, l.top - 22, time);
            break;
        }
        case RuinShape::Steps: {
            for (int step = 0; step < 6; step++) { // each stair's lit edge
                int dy = 4 + step * 6;
                if (dy >= s.rows || s.count[dy] == 0) break;
                int left = s.spans[dy][0].left;
                DrawRectangle(left, l.top + dy, 7, 1, depthShade(STONE_LIGHT, dy, DEPTH));
            }
            drawMasonry(s, 4, 50, 6, 16, l.seed);
            drawTopMoss(l, 5);
            break;
        }
    }
}

// A ruined doorway around the exit. Rubble and roots block it until the objective is done.
void drawRuinDoorway(Rectangle gate, int cameraX, bool open, float time) {
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    const int cx = gx + gw / 2;

    if (open) {
        DrawCircleGradient(cx, gy + gh / 2, 46.0f, withAlpha(FIRE_HOT, 0.3f), withAlpha(FIRE_HOT, 0.0f));
        DrawRectangleGradientV(gx, gy, gw, gh, withAlpha(FIRE_HOT, 0.92f), withAlpha(RELIC_GOLD, 0.6f));
        for (int i = 0; i < 6; i++) {
            float t = std::fmod(time * 0.4f + i * 0.17f, 1.0f);
            DrawRectangle(gx + 2 + (int)(hash01(i * 5 + 21) * (gw - 4)), gy + gh - 2 - (int)(t * gh), 1, 1,
                          withAlpha(WHITE, 1.0f - t));
        }
    } else {
        DrawRectangle(gx, gy, gw, gh, DARK);
        DrawRectangle(gx + 1, gy + gh / 3, gw - 2, gh - gh / 3, STONE_DARK); // a fallen slab
        DrawRectangle(gx + 1, gy + gh / 3, gw - 2, 1, STONE);
        for (int k = 0; k < 8; k++) DrawRectangle(gx + 6 + (k % 2) + k / 3, gy + gh / 3 + 3 + k, 1, 1, STONE_SHADE);
        for (int r = 0; r < 3; r++) { // roots across the opening
            for (int x = 0; x < gw; x++) {
                int y = gy + 6 + r * 7 + (int)std::round(std::sin(x * 0.4f + r * 2.0f) * 1.5f);
                DrawRectangle(gx + x, y, 1, 1, ROOT);
            }
        }
    }

    DrawRectangle(gx - 6, gy, 6, gh, STONE); // jambs, lit on the right
    DrawRectangle(gx + gw, gy, 6, gh, STONE);
    DrawRectangle(gx - 6, gy, 1, gh, STONE_SHADE);
    DrawRectangle(gx + gw + 5, gy, 1, gh, STONE_LIGHT);
    DrawRectangle(gx - 8, gy - 6, gw + 16, 6, STONE); // lintel
    DrawRectangle(gx - 8, gy - 6, gw + 16, 1, STONE_LIGHT);
    drawTriangleSafe(Vector2{(float)(gx - 8), (float)(gy - 6)}, Vector2{(float)cx, (float)(gy - 6)},
                     Vector2{(float)(cx - 2), (float)(gy - 15)}, STONE); // half of a broken pediment
    Color rune = open ? RELIC_GOLD : STONE_SHADE;
    for (int k = 0; k < 3; k++) {
        DrawRectangle(gx - 4, gy + 6 + k * 9, 2, 2, rune);
        DrawRectangle(gx + gw + 2, gy + 6 + k * 9, 2, 2, rune);
    }
    for (int row = 0; row < 16; row++) { // a tattered banner on the left jamb
        int sway = (int)std::round(std::sin(time * 1.6f + row * 0.25f) * (row / 16.0f) * 2.0f);
        int width = (row < 13) ? 6 : 6 - (row - 12) * 2;
        DrawRectangle(gx - 13 + sway, gy - 2 + row, width, 1, (row % 5 == 0) ? BANNER_DARK : BANNER);
    }
    DrawRectangle(gx - 14, gy - 3, 8, 1, RELIC_GOLD);
}

// ---------------------------------------------------------------------------
// Background layers
// ---------------------------------------------------------------------------

void drawRuinsSky(int scroll, float time) {
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 170, SKY_TOP, SKY_MID);
    DrawRectangleGradientV(0, 170, CANVAS_WIDTH, CANVAS_HEIGHT - 170, SKY_MID, SKY_LOW);
    for (int i = 0; i < 60; i++) { // stars
        int x = (int)(hash01(i * 7 + 1) * CANVAS_WIDTH);
        int y = (int)(hash01(i * 13 + 2) * 150.0f);
        float twinkle = 0.5f + 0.5f * std::sin(time * 1.7f + i * 2.3f);
        DrawRectangle(x, y, 1, 1, withAlpha(MOON, (0.3f + 0.7f * hash01(i * 5)) * (0.4f + 0.6f * twinkle)));
    }
    int mx = 470 - (int)(scroll * 0.04f);
    int my = 62;
    DrawCircleGradient(mx, my, 130.0f, withAlpha(MOON, 0.16f), withAlpha(MOON, 0.0f));
    DrawCircleGradient(mx, my, 40.0f, withAlpha(MOON, 0.3f), withAlpha(MOON, 0.0f));
    DrawCircle(mx, my, 15.0f, MOON);
    DrawCircle(mx - 4, my - 3, 3.0f, lerpColor(MOON, SKY_LOW, 0.3f)); // craters
    DrawCircle(mx + 5, my + 4, 2.0f, lerpColor(MOON, SKY_LOW, 0.3f));
}

// A distant drowned city: towers and a broken aqueduct, softened by haze.
void drawFarCity(int scroll) {
    float offset = scroll * 0.14f;
    Color window = lerpColor(FAR_RUINS, SKY_LOW, 0.5f);
    const float towerSpacing = 56.0f;
    int first = (int)std::floor(offset / towerSpacing) - 1;
    for (int i = first; i < first + 14; i++) {
        float x = i * towerSpacing + hash01(i * 7 + 3) * 24.0f - offset;
        int width = 12 + (int)(hash01(i * 11 + 5) * 12.0f);
        int height = 50 + (int)(hash01(i * 17 + 9) * 70.0f);
        int top = 232 - height;
        DrawRectangle((int)x, top, width, height, FAR_RUINS);
        DrawRectangle((int)x + width / 2, top - 6, width / 2, 6, FAR_RUINS); // broken top
        for (int w = 0; w < 3; w++) DrawRectangle((int)x + width / 2 - 1, top + 10 + w * 14, 2, 4, window);
    }
    const float pierSpacing = 34.0f; // the aqueduct
    first = (int)std::floor(offset / pierSpacing) - 1;
    for (int i = first; i < first + 22; i++) {
        if (hash01(i * 23 + 1) > 0.82f) continue; // a fallen span
        float x = i * pierSpacing - offset;
        DrawRectangle((int)x, 168, (int)pierSpacing + 1, 8, FAR_RUINS);
        DrawRectangle((int)x, 176, 6, 56, FAR_RUINS);
        DrawRing(Vector2{x + 20.0f, 190.0f}, 11.0f, 14.0f, 180.0f, 360.0f, 14, FAR_RUINS);
    }
    DrawRectangleGradientV(0, 150, CANVAS_WIDTH, 110, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.35f));
}

void drawMoonbeams(int scroll, float time) {
    float offset = scroll * 0.2f;
    for (int i = 0; i < 3; i++) {
        float topX = 420.0f + i * 60.0f - offset;
        float width = 12.0f + i * 8.0f;
        float alpha = 0.045f + 0.02f * std::sin(time * 0.5f + i * 2.1f);
        for (int segment = 0; segment < 3; segment++) {
            float y0 = segment * 100.0f;
            float y1 = y0 + 100.0f;
            float x0 = topX - 110.0f * (y0 / 300.0f);
            float x1 = topX - 110.0f * (y1 / 300.0f);
            Color light = withAlpha(MOON, alpha * (1.0f - segment * 0.3f));
            drawTriangleSafe(Vector2{x0, y0}, Vector2{x0 + width, y0}, Vector2{x1 + width, y1}, light);
            drawTriangleSafe(Vector2{x0, y0}, Vector2{x1 + width, y1}, Vector2{x1, y1}, light);
        }
    }
}

// Great broken arches in the middle distance, with banners, vines, and braziers.
void drawMidArches(int scroll, float time) {
    float offset = scroll * 0.4f;
    const float spacing = 150.0f;
    int first = (int)std::floor(offset / spacing) - 1;
    for (int i = first; i < first + 6; i++) {
        float x = i * spacing + hash01(i * 19 + 4) * 40.0f - offset;
        int radius = 34 + (int)(hash01(i * 29 + 6) * 12.0f);
        int springY = 120 + (int)(hash01(i * 31 + 8) * 40.0f);
        bool broken = hash01(i * 37 + 2) > 0.5f;
        const int pillar = 16;
        int leftX = (int)x - radius - pillar / 2;
        int rightX = (int)x + radius - pillar / 2;
        int rightTop = broken ? springY + 36 : springY;

        DrawRectangle(leftX, springY, pillar, WATERLINE - springY, MID_RUINS);
        DrawRectangle(rightX, rightTop, pillar, WATERLINE - rightTop, MID_RUINS);
        DrawRectangle(leftX + pillar - 1, springY, 1, WATERLINE - springY, MID_RUINS_LIT);
        DrawRectangle(rightX + pillar - 1, rightTop, 1, WATERLINE - rightTop, MID_RUINS_LIT);
        DrawRing(Vector2{x, (float)springY}, (float)(radius - pillar / 2), (float)(radius + pillar / 2), 180.0f,
                 broken ? 285.0f : 360.0f, 28, MID_RUINS);
        DrawRectangle(leftX - 2, springY - 4, pillar + 4, 4, MID_RUINS); // capitals
        if (!broken) {
            DrawRectangle(rightX - 2, springY - 4, pillar + 4, 4, MID_RUINS);
            for (int row = 0; row < 26; row++) { // a banner from the crown
                int sway = (int)std::round(std::sin(time * 1.3f + i + row * 0.2f) * (row / 26.0f) * 3.0f);
                int width = (row < 22) ? 10 : 10 - (row - 21) * 3;
                if (width > 0) DrawRectangle((int)x - 5 + sway, springY - radius + 4 + row, width, 1,
                                             (row % 6 == 0) ? BANNER_DARK : lerpColor(BANNER, MID_RUINS, 0.35f));
            }
        } else { // a brazier still burning on the broken pillar
            float flicker = 0.8f + 0.2f * std::sin(time * 12.0f + i);
            DrawCircleGradient(rightX + pillar / 2, rightTop - 4, 30.0f, withAlpha(FIRE, 0.28f * flicker), withAlpha(FIRE, 0.0f));
            DrawRectangle(rightX + 3, rightTop - 3, pillar - 6, 3, MID_RUINS_LIT);
            DrawRectangle(rightX + 5, rightTop - 7, pillar - 10, 4, withAlpha(FIRE, flicker));
        }
        for (int v = 0; v < 3; v++) { // vines hanging from the arch
            int vx = (int)x - radius + 10 + v * radius * 2 / 3;
            drawHangingRoot(vx, springY - radius + 8 + v * 4, 18 + v * 6, time, i * 3 + v, lerpColor(MOSS_DARK, MID_RUINS, 0.4f));
        }
    }
    DrawRectangleGradientV(0, 190, CANVAS_WIDTH, 102, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.22f));
}

// Still water below the route, with the moon's reflection.
void drawWaterBack(int scroll, float time) {
    DrawRectangleGradientV(0, WATERLINE, CANVAS_WIDTH, CANVAS_HEIGHT - WATERLINE, WATER, WATER_DEEP);
    int mx = 470 - (int)(scroll * 0.04f);
    for (int y = WATERLINE + 1; y < CANVAS_HEIGHT; y += 2) {
        float depth = (y - WATERLINE) / (float)(CANVAS_HEIGHT - WATERLINE);
        int width = 10 - (int)(depth * 7.0f);
        int wobble = (int)std::round(std::sin(time * 2.0f + y * 0.35f) * 2.0f);
        DrawRectangle(mx - width / 2 + wobble, y, width, 1, withAlpha(MOON, 0.4f * (1.0f - depth)));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Region 2 drawing
// ---------------------------------------------------------------------------

void drawRuinsBackground(const SceneView& view) {
    drawRuinsSky(view.scroll, view.time);
    drawFarCity(view.scroll);
    drawMoonbeams(view.scroll, view.time);
    drawMidArches(view.scroll, view.time);
    drawWaterBack(view.scroll, view.time);
}

void drawRuinsPlatforms(const SceneView& view) {
    // Both links of the current node, each checked for NULL before use.
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;
    const Node* previous = (view.current != nullptr) ? view.current->previous : nullptr;

    // Walk the real list from its head, with a bound.
    const Node* node = view.head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            float mist = 0.0f;
            if (view.mistById != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION) {
                mist = view.mistById[node->id];
            }
            RuinShape shape = shapeFor(*node);
            Silhouette outline = buildRuinSilhouette(shape, ledge);
            fillSilhouette(outline, RUIN_STONE, true);
            drawRuinDetails(shape, ledge, outline, view);
            drawMistVeil(outline, mist, VEIL);

            // The Old Hall's brazier: cold at first, waiting while you carry the light back, then lit.
            if (node->id == 2) {
                drawBrazier(ledge.left + ledge.width - 14, ledge.top, view.stage >= 2, view.stage == 1 ? 1.0f : 0.0f,
                            view.time);
            }
            if (node == next) drawNextGlow(ledge, view.time, view.emphasizePrevious ? 0.45f : 1.0f);
            if (node == previous) drawPreviousGlow(ledge, view.time, view.emphasizePrevious ? 1.0f : 0.55f);
        }
        node = node->next;
        steps++;
    }

    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        drawLanternPost(ledge.left + 6, ledge.top, view.checkpointFlash, view.time);
    }
    if (view.stage == 1) {
        drawCarriedLight(view.playerFeet, view.time);
    }
    if (view.hasGate) {
        drawRuinDoorway(view.gateRect, view.cameraX, view.gateOpen, view.time);
    }
}

void drawRuinsForeground(const SceneView& view) {
    const int scroll = view.scroll;
    const float time = view.time;

    // The water covers the platforms' feet, with ripples and floating leaves.
    DrawRectangleGradientV(0, WATERLINE, CANVAS_WIDTH, CANVAS_HEIGHT - WATERLINE, withAlpha(WATER, 0.55f),
                           withAlpha(WATER_DEEP, 0.9f));
    DrawRectangle(0, WATERLINE, CANVAS_WIDTH, 1, withAlpha(MOON, 0.22f));
    for (int i = 0; i < 34; i++) {
        float x = std::fmod(hash01(i * 7 + 2) * 760.0f + time * (4.0f + (i % 3) * 2.0f) - scroll + 3800.0f, 760.0f) - 60.0f;
        int y = WATERLINE + 3 + (int)(hash01(i * 11 + 5) * 60.0f);
        int width = 5 + (int)(hash01(i * 13 + 1) * 12.0f);
        DrawRectangle((int)x, y, width, 1, withAlpha(STONE_LIGHT, 0.16f));
        if (i % 6 == 0) DrawRectangle((int)x + 2, y - 1, 2, 1, withAlpha(MOSS, 0.8f)); // a floating leaf
    }
    for (int i = 0; i < 5; i++) { // low mist over the water
        float x = std::fmod(i * 190.0f + time * 4.0f - scroll * 1.05f + 4000.0f, 950.0f) - 150.0f;
        DrawEllipse((int)x, WATERLINE - 4 + (i % 2) * 6, 100.0f + (i % 3) * 30.0f, 8.0f, withAlpha(MIST, 0.1f));
    }

    // Dust floating in the moonlight.
    for (int i = 0; i < 22; i++) {
        float x = std::fmod(hash01(i * 17 + 3) * 900.0f - scroll * 0.9f + time * 3.0f + 3600.0f, 900.0f) - 100.0f;
        float y = 60.0f + hash01(i * 5 + 8) * 200.0f + std::sin(time * 0.6f + i) * 6.0f;
        DrawRectangle((int)x, (int)y, 1, 1, withAlpha(MOON, 0.25f + 0.25f * std::sin(time * 2.0f + i)));
    }

    // Near silhouettes: a broken column at the start, hanging roots, and reeds.
    float offset = scroll * 1.3f;
    {
        float x = 14.0f - offset;
        if (x > -40.0f) {
            DrawRectangle((int)x - 14, 70, 28, CANVAS_HEIGHT, DARK);
            DrawRectangle((int)x - 18, 62, 36, 8, DARK);
            DrawRectangle((int)x + 12, 70, 2, CANVAS_HEIGHT, withAlpha(MID_RUINS_LIT, 0.6f));
        }
    }
    for (int i = 0; i < 12; i++) {
        float x = i * 70.0f + hash01(i * 3 + 7) * 30.0f - scroll * 1.15f;
        if (x < -20.0f || x > CANVAS_WIDTH + 20.0f) continue;
        drawHangingRoot((int)x, 0, 24 + (int)(hash01(i * 9 + 1) * 40.0f), time, i, DARK);
    }
    for (int i = 0; i < 30; i++) {
        float x = i * 36.0f + hash01(i * 5 + 4) * 14.0f - offset;
        if (x < -10.0f || x > CANVAS_WIDTH + 10.0f) continue;
        int height = 12 + (int)(hash01(i * 7 + 9) * 22.0f);
        int sway = (int)std::round(std::sin(time * 1.4f + i) * 1.5f);
        DrawLineEx(Vector2{x, CANVAS_HEIGHT + 2.0f}, Vector2{x + sway, (float)(CANVAS_HEIGHT - height)}, 2.0f, DARK);
        DrawRectangle((int)x + sway - 1, CANVAS_HEIGHT - height - 4, 3, 5, DARK); // reed head
    }

    DrawRectangleGradientH(0, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.5f), withAlpha(DARK, 0.0f));
    DrawRectangleGradientH(CANVAS_WIDTH - 70, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.5f));
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 50, withAlpha(DARK, 0.55f), withAlpha(DARK, 0.0f));
}
