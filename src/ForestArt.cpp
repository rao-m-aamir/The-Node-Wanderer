#include "ForestArt.h"

#include "DrawHelpers.h"
#include "Theme.h"
#include "World.h" // MAX_NODES_PER_REGION

// Region 1 art, drawn in canvas pixels (640 x 360, shown at 2x).
// The ledges are drawn only from their node's position and width. Nothing in
// this file decides which ledges connect: that comes from the list's next pointers.

namespace {

// Forest palette: cool teal shadows, warm light coming from behind the trees.
const Color SKY_TOP = {14, 34, 50, 255};
const Color SKY_MID = {34, 78, 86, 255};
const Color SKY_LOW = {104, 142, 116, 255};
const Color SUN = {255, 228, 168, 255};
const Color FAR_TREES = {60, 106, 102, 255};
const Color MID_TREES = {32, 66, 64, 255};
const Color GIANT_BARK = {40, 68, 60, 255};
const Color GIANT_RIM = {86, 122, 92, 255};
const Color CANOPY = {16, 36, 34, 255};
const Color VINE = {30, 62, 48, 255};
const Color MIST = {178, 208, 188, 255};
const Color VEIL = {116, 156, 150, 255};
const Color DEPTH = {18, 38, 44, 255};
const Color DARK = {8, 16, 18, 255};
const Color FIREFLY = {255, 222, 130, 255};

// Ledge materials, lit from the upper right.
const Color MOSS_LIGHT = {184, 226, 112, 255};
const Color MOSS = {112, 172, 78, 255};
const Color MOSS_DARK = {58, 112, 62, 255};
const Color EARTH_LIGHT = {146, 108, 78, 255};
const Color EARTH = {108, 78, 60, 255};
const Color EARTH_DARK = {72, 52, 46, 255};
const Color STONE_LIGHT = {168, 172, 158, 255};
const Color STONE = {112, 120, 116, 255};
const Color STONE_DARK = {66, 74, 80, 255};
const Color BARK_LIGHT = {158, 114, 74, 255};
const Color BARK = {108, 76, 54, 255};
const Color BARK_DARK = {64, 44, 38, 255};
const Color RINGS = {206, 170, 118, 255};
const Color MUSHROOM = {226, 128, 84, 255};

// Darkens a ledge color the deeper it is below the walkable top (the ravine is dim).
Color depthShade(Color color, int dy) {
    float t = (dy - 20) / 100.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 0.75f) t = 0.75f;
    return lerpColor(color, DEPTH, t);
}

// ---------------------------------------------------------------------------
// Ledge shapes. Each node always gets the same look, chosen from its stable ID.
// ---------------------------------------------------------------------------

enum class LedgeShape { GrassCliff, MossyBoulder, FallenLog, OldRuin };

LedgeShape shapeFor(const Node& node) {
    switch (node.id) {
        case 2: return LedgeShape::MossyBoulder;
        case 3: return LedgeShape::FallenLog;
        case 4: return LedgeShape::OldRuin;
        default: return LedgeShape::GrassCliff; // N1 (head) and N5 (tail)
    }
}

// A ledge in canvas pixels. `top` is the walkable surface, taken from the node.
struct Ledge {
    LedgeShape shape;
    int left;
    int top;
    int width;
    int seed;
};

Ledge ledgeFor(const Node& node, int cameraX) {
    Ledge ledge;
    ledge.shape = shapeFor(node);
    ledge.left = toCanvasX(node.platformPosition.x, cameraX);
    ledge.top = toCanvasY(node.platformPosition.y);
    ledge.width = (int)(node.platformWidth / CANVAS_SCALE);
    ledge.seed = node.id * 37 + 11;
    return ledge;
}

// Left and right edge of the ledge's silhouette `dy` rows below its top.
// The top row always matches the node's collision width exactly.
// Returns false below the bottom of the shape.
bool rowSpan(const Ledge& ledge, int dy, int& left, int& right) {
    const int x0 = ledge.left;
    const int x1 = ledge.left + ledge.width;
    const float s = (float)ledge.seed;
    if (dy < 0 || ledge.top + dy >= CANVAS_HEIGHT) return false;

    switch (ledge.shape) {
        case LedgeShape::GrassCliff: {
            // Exact lip, a small undercut, then a rocky wall that spreads toward the ravine.
            int inset = 0;
            if (dy >= 4 && dy < 12) inset = (dy - 3) / 2;
            else if (dy >= 12) inset = 4 - (dy - 12) / 8;
            int wobbleLeft = 0;
            int wobbleRight = 0;
            if (dy >= 4) {
                wobbleLeft = (int)std::round(2.0f * std::sin(dy * 0.23f + s) + std::sin(dy * 0.61f + s * 1.7f));
                wobbleRight = (int)std::round(2.0f * std::sin(dy * 0.19f + s * 2.3f) + std::sin(dy * 0.57f + s));
            }
            left = x0 + inset + wobbleLeft;
            right = x1 - inset + wobbleRight;
            return true;
        }
        case LedgeShape::MossyBoulder: {
            // A lumpy boulder resting on a rough stone column.
            if (dy < 3) { left = x0; right = x1; return true; }
            if (dy < 24) {
                float t = (dy - 3) / 21.0f; // 0 under the lip, 1 at the boulder's bottom
                float curve = std::pow(t, 1.6f) * ledge.width * 0.27f;
                int lumpLeft = (int)std::round(1.5f * std::sin(dy * 0.9f + s) + std::sin(dy * 0.37f + s * 2.0f));
                int lumpRight = (int)std::round(1.5f * std::sin(dy * 0.8f + s * 3.0f));
                left = x0 - 2 + (int)curve + lumpLeft;
                right = x1 + 2 - (int)(curve * 0.85f) + lumpRight;
                return true;
            }
            int wobble = (int)std::round(std::sin(dy * 0.3f + s) * 2.0f + std::sin(dy * 0.11f + s) * 2.0f);
            int spread = (dy - 24) / 9;
            left = x0 + ledge.width * 27 / 100 - spread + wobble;
            right = x1 - ledge.width * 25 / 100 + spread + wobble;
            return true;
        }
        case LedgeShape::FallenLog: {
            // A log 16 rows thick with round ends, resting on a rock in the middle.
            if (dy < 16) {
                float d = dy - 7.5f;
                int cap = (int)std::round(8.0f - std::sqrt(std::fmax(0.0f, 64.0f - d * d)));
                left = x0 - 5 + cap;
                right = x1 + 5 - cap;
                return true;
            }
            int center = x0 + ledge.width / 2 + (ledge.seed % 7) - 3;
            int below = dy - 16;
            int half = (below < 6) ? ledge.width * 24 / 100 - below : ledge.width * 15 / 100 + below / 12;
            int wobble = (int)std::round(std::sin(dy * 0.35f + s) * 1.2f);
            left = center - half + wobble;
            right = center + half + wobble;
            return true;
        }
        case LedgeShape::OldRuin: {
            // A carved slab with a cornice, a block wall, then a broken column into the ravine.
            if (dy < 1) { left = x0; right = x1; return true; }
            if (dy < 4) { left = x0 - 2; right = x1 + 2; return true; }  // cornice
            if (dy < 8) { left = x0 + 1; right = x1 - 1; return true; }  // carved frieze
            if (dy < 30) { left = x0 + 3; right = x1 - 3; return true; } // block wall
            int center = x0 + ledge.width / 2;
            int half = ledge.width * 20 / 100 + (dy - 30) / 16;
            left = center - half;
            right = center + half;
            return true;
        }
    }
    return false;
}

// Material for one row of a ledge: base color, left (shadow) edge, right (lit) edge.
void materialForRow(const Ledge& ledge, int dy, Color& base, Color& shade, Color& lit) {
    switch (ledge.shape) {
        case LedgeShape::GrassCliff: {
            if (dy == 0) { base = MOSS_LIGHT; shade = MOSS; lit = MOSS_LIGHT; return; }
            if (dy < 3) { base = MOSS; shade = MOSS_DARK; lit = MOSS_LIGHT; return; }
            if (dy < 4) { base = MOSS_DARK; shade = MOSS_DARK; lit = MOSS; return; }
            // Soil layers of uneven thickness, with soft contrast.
            int layer = (int)((dy + 4.0f * std::sin(dy * 0.05f + ledge.seed)) / 11.0f);
            base = (layer % 3 == 1) ? lerpColor(EARTH, EARTH_DARK, 0.55f) : EARTH;
            shade = EARTH_DARK;
            lit = EARTH_LIGHT;
            return;
        }
        case LedgeShape::MossyBoulder:
            if (dy == 0) { base = MOSS_LIGHT; shade = MOSS; lit = MOSS_LIGHT; return; }
            if (dy < 3) { base = MOSS; shade = MOSS_DARK; lit = MOSS_LIGHT; return; }
            if (dy < 6) { base = MOSS_DARK; shade = lerpColor(MOSS_DARK, STONE_DARK, 0.5f); lit = MOSS; return; }
            if (dy < 24) { base = STONE; shade = STONE_DARK; lit = STONE_LIGHT; return; }
            base = lerpColor(STONE, STONE_DARK, 0.6f);
            shade = lerpColor(STONE_DARK, DARK, 0.4f);
            lit = STONE;
            return;
        case LedgeShape::FallenLog:
            if (dy == 0) { base = MOSS_LIGHT; shade = MOSS; lit = MOSS_LIGHT; return; }
            if (dy < 2) { base = MOSS; shade = MOSS_DARK; lit = MOSS_LIGHT; return; }
            if (dy < 5) { base = BARK_LIGHT; shade = BARK; lit = BARK_LIGHT; return; }
            if (dy < 12) { base = BARK; shade = BARK_DARK; lit = BARK_LIGHT; return; }
            if (dy < 16) { base = BARK_DARK; shade = BARK_DARK; lit = BARK; return; }
            base = (dy < 22) ? STONE : STONE_DARK; // the rock under the log
            shade = STONE_DARK;
            lit = STONE_LIGHT;
            return;
        case LedgeShape::OldRuin:
            if (dy == 0) { base = STONE_LIGHT; shade = STONE; lit = STONE_LIGHT; return; }
            if (dy < 4) { base = STONE; shade = STONE_DARK; lit = STONE_LIGHT; return; }
            if (dy < 8) { base = STONE_DARK; shade = STONE_DARK; lit = STONE; return; }
            base = (dy < 30) ? STONE : lerpColor(STONE, STONE_DARK, 0.4f);
            shade = STONE_DARK;
            lit = STONE_LIGHT;
            return;
    }
}

// Fills the silhouette row by row. Light comes from the upper right, so each row
// gets a dark left edge and a lit right edge, and the ledge darkens with depth.
void fillLedge(const Ledge& ledge) {
    for (int dy = 0; dy < CANVAS_HEIGHT; dy++) {
        int left, right;
        if (!rowSpan(ledge, dy, left, right)) break;
        if (right <= left) continue;
        Color base = STONE;
        Color shade = STONE_DARK;
        Color lit = STONE_LIGHT;
        materialForRow(ledge, dy, base, shade, lit);
        int y = ledge.top + dy;
        DrawRectangle(left, y, right - left, 1, depthShade(base, dy));
        DrawRectangle(left, y, 2, 1, depthShade(shade, dy));
        DrawRectangle(right - 1, y, 1, 1, depthShade(lit, dy));
    }
}

// ---------------------------------------------------------------------------
// Ledge details: grass, moss, stones, roots, bark, carvings.
// ---------------------------------------------------------------------------

// Grass blades on top of a walkable lip. They fade toward the mist when veiled.
void drawGrassTufts(const Ledge& ledge, float mist, float time) {
    Color light = lerpColor(MOSS_LIGHT, VEIL, mist * 0.6f);
    Color mid = lerpColor(MOSS, VEIL, mist * 0.6f);
    for (int x = ledge.left + 1; x < ledge.left + ledge.width - 1; x += 3) {
        float h = hash01(ledge.seed * 131 + x);
        if (h < 0.35f) continue;
        int height = 1 + (int)(h * 3.0f);
        int sway = (h > 0.8f && std::sin(time * 2.0f + x * 0.3f) > 0.3f) ? 1 : 0;
        DrawRectangle(x + sway, ledge.top - height, 1, height, (h > 0.7f) ? light : mid);
    }
}

// Moss strands hanging over the edge, starting `fromDy` rows below the top.
void drawMossDrips(const Ledge& ledge, int fromDy, int count) {
    for (int i = 0; i < count; i++) {
        int x = ledge.left + 2 + (int)(hash01(ledge.seed * 71 + i) * (ledge.width - 4));
        int length = 2 + (int)(hash01(ledge.seed * 13 + i) * 6.0f);
        DrawRectangle(x, ledge.top + fromDy, 1, length, MOSS_DARK);
        DrawRectangle(x, ledge.top + fromDy, 1, 1, MOSS);
    }
}

void drawEmbeddedStones(const Ledge& ledge, int count, int minDy, int maxDy) {
    for (int i = 0; i < count; i++) {
        int dy = minDy + (int)(hash01(ledge.seed * 29 + i) * (maxDy - minDy));
        int left, right;
        if (!rowSpan(ledge, dy, left, right) || right - left < 12) continue;
        int x = left + 4 + (int)(hash01(ledge.seed * 37 + i) * (right - left - 10));
        int width = 2 + (int)(hash01(ledge.seed * 41 + i) * 3.0f);
        DrawRectangle(x, ledge.top + dy, width, 2, depthShade(STONE, dy));
        DrawRectangle(x, ledge.top + dy, width, 1, depthShade(STONE_LIGHT, dy));
    }
}

// Roots poking out of the wall and curling down.
void drawHangingRoots(const Ledge& ledge, int count, float time) {
    for (int i = 0; i < count; i++) {
        int dy = 6 + (int)(hash01(ledge.seed * 53 + i) * 30.0f);
        int left, right;
        if (!rowSpan(ledge, dy, left, right)) continue;
        bool onLeft = (i % 2 == 0);
        int x = onLeft ? left : right - 1;
        int length = 6 + (int)(hash01(ledge.seed * 59 + i) * 10.0f);
        int sway = (int)std::round(std::sin(time * 1.3f + i) * 1.0f);
        for (int k = 0; k < length; k++) {
            int dx = onLeft ? -(k / 4) : (k / 4);
            DrawRectangle(x + dx + (k > length / 2 ? sway : 0), ledge.top + dy + k, 1, 1,
                          depthShade(BARK_DARK, dy + k));
        }
    }
}

void drawBoulderDetails(const Ledge& ledge) {
    drawMossDrips(ledge, 5, ledge.width / 7);
    for (int i = 0; i < 2; i++) { // cracks
        int x = ledge.left + ledge.width / 4 + i * ledge.width / 3;
        for (int k = 0; k < 9; k++) {
            DrawRectangle(x + k / 3, ledge.top + 9 + k, 1, 1, STONE_DARK);
        }
    }
    for (int i = 0; i < 14; i++) { // moss and lichen clinging to the rock
        int dy = 6 + (int)(hash01(ledge.seed * 17 + i) * 30.0f);
        int left, right;
        if (!rowSpan(ledge, dy, left, right) || right - left < 8) continue;
        int x = left + 2 + (int)(hash01(ledge.seed * 19 + i) * (right - left - 5));
        DrawRectangle(x, ledge.top + dy, 2 + (i % 3), 1, depthShade((i % 4 == 0) ? MOSS : MOSS_DARK, dy));
    }
    for (int i = 0; i < 5; i++) { // a few lit chips on the upper right
        int dy = 7 + i * 3;
        int left, right;
        if (!rowSpan(ledge, dy, left, right)) continue;
        DrawRectangle(right - 4 - (i % 2) * 3, ledge.top + dy, 2, 1, STONE_LIGHT);
    }
}

void drawLogDetails(const Ledge& ledge) {
    const int x0 = ledge.left;
    const int x1 = ledge.left + ledge.width;
    const int grainRows[3] = {6, 9, 12};
    for (int g = 0; g < 3; g++) { // bark grain
        int left, right;
        if (!rowSpan(ledge, grainRows[g], left, right)) continue;
        for (int x = left + 3; x < right - 3; x++) {
            if (hash01(ledge.seed * 7 + g * 1000 + x) < 0.55f) {
                DrawRectangle(x, ledge.top + grainRows[g], 1, 1, BARK_DARK);
            }
        }
    }
    DrawEllipse(x0 + ledge.width * 2 / 3, ledge.top + 9, 3.0f, 2.0f, BARK_DARK); // knot
    DrawRectangle(x0 + ledge.width * 2 / 3, ledge.top + 9, 1, 1, BARK_LIGHT);

    // Cut end with tree rings on the right, a splintered end on the left.
    int ringX = x1 - 3;
    int ringY = ledge.top + 7;
    DrawCircle(ringX, ringY, 7.0f, RINGS);
    DrawCircleLines(ringX, ringY, 5.0f, BARK_LIGHT);
    DrawCircleLines(ringX, ringY, 3.0f, BARK_LIGHT);
    DrawCircleLines(ringX, ringY, 7.0f, BARK_DARK);
    DrawRectangle(ringX, ringY, 1, 1, BARK);
    for (int i = 0; i < 4; i++) {
        DrawRectangle(x0 - 5 - (i % 2) * 2, ledge.top + 3 + i * 3, 2 + (i % 2), 1, BARK_DARK);
    }

    for (int i = 0; i < 6; i++) { // moss patches on top
        int x = x0 + (int)(hash01(ledge.seed * 23 + i) * (ledge.width - 3));
        DrawRectangle(x, ledge.top + 2, 3, 1, MOSS);
    }
    for (int i = 0; i < 3; i++) { // small mushrooms on the side
        int x = x0 + 10 + i * 5;
        int y = ledge.top + 12 - (i % 2);
        DrawRectangle(x, y, 3, 1, MUSHROOM);
        DrawRectangle(x + 1, y + 1, 1, 2, RINGS);
    }
}

void drawRuinDetails(const Ledge& ledge, float time) {
    const int x0 = ledge.left;
    const int x1 = ledge.left + ledge.width;
    DrawRectangle(x0 - 2, ledge.top + 1, ledge.width + 4, 1, STONE_LIGHT); // cornice edge
    for (int x = x0 + 3; x < x1 - 3; x += 4) {                             // carved frieze
        DrawRectangle(x, ledge.top + 5, 2, 2, STONE);
    }
    for (int course = 0; course < 3; course++) { // block wall: mortar lines, joints offset per course
        int dy = 8 + course * 7;
        DrawRectangle(x0 + 3, ledge.top + dy, ledge.width - 6, 1, STONE_DARK);
        for (int x = x0 + 3 + (course % 2) * 9; x < x1 - 3; x += 18) {
            DrawRectangle(x, ledge.top + dy, 1, 7, STONE_DARK);
        }
    }
    for (int k = 0; k < 10; k++) { // a crack
        DrawRectangle(x0 + ledge.width * 3 / 5 + (k % 3) - k / 4, ledge.top + 9 + k, 1, 1, DARK);
    }

    int left, right;
    for (int dy = 30; rowSpan(ledge, dy, left, right); dy++) { // fluted column
        int width = right - left;
        DrawRectangle(left + width / 4, ledge.top + dy, 1, 1, depthShade(STONE_DARK, dy));
        DrawRectangle(left + width / 2, ledge.top + dy, 1, 1, depthShade(STONE_DARK, dy));
        DrawRectangle(left + width * 3 / 4, ledge.top + dy, 1, 1, depthShade(STONE_LIGHT, dy));
    }

    for (int i = 0; i < 4; i++) { // ivy hanging over the face
        int x = x0 + 4 + (int)(hash01(ledge.seed * 61 + i) * (ledge.width - 8));
        int length = 8 + (int)(hash01(ledge.seed * 67 + i) * 14.0f);
        for (int k = 0; k < length; k++) {
            int sway = (int)std::round(std::sin(time * 1.1f + i + k * 0.2f) * 0.6f);
            DrawRectangle(x + sway, ledge.top + 1 + k, 1, 1, MOSS_DARK);
            if (k % 3 == 1) DrawRectangle(x + sway + ((k / 3) % 2 == 0 ? 1 : -1), ledge.top + 1 + k, 1, 1, MOSS);
        }
    }
    for (int i = 0; i < 7; i++) { // moss on the slab
        int x = x0 + (int)(hash01(ledge.seed * 43 + i) * (ledge.width - 4));
        DrawRectangle(x, ledge.top, 4, 1, MOSS);
        DrawRectangle(x + 1, ledge.top - 1, 2, 1, MOSS_LIGHT);
    }
}

void drawLedgeDetails(const Ledge& ledge, float mist, float time) {
    switch (ledge.shape) {
        case LedgeShape::GrassCliff:
            drawMossDrips(ledge, 4, ledge.width / 7);
            drawEmbeddedStones(ledge, 16, 8, 90);
            drawHangingRoots(ledge, 4, time);
            drawGrassTufts(ledge, mist, time);
            break;
        case LedgeShape::MossyBoulder:
            drawBoulderDetails(ledge);
            drawGrassTufts(ledge, mist, time);
            break;
        case LedgeShape::FallenLog:
            drawLogDetails(ledge);
            break;
        case LedgeShape::OldRuin:
            drawRuinDetails(ledge, time);
            break;
    }
}

// ---------------------------------------------------------------------------
// Traversal cues, checkpoint lantern, and exit arch.
// ---------------------------------------------------------------------------

// Soft warm light rising from the ledge that current->next points to.
void drawNextGlow(const Ledge& ledge, float time) {
    float pulse = 0.6f + 0.4f * std::sin(time * 3.0f);
    DrawRectangleGradientV(ledge.left, ledge.top - 18, ledge.width, 18, withAlpha(FIREFLY, 0.0f),
                           withAlpha(FIREFLY, 0.22f * pulse));
    DrawRectangle(ledge.left, ledge.top, ledge.width, 1, withAlpha(FIREFLY, 0.55f * pulse));
    for (int i = 0; i < 6; i++) { // motes drifting up
        float t = std::fmod(time * 0.5f + i * 0.17f, 1.0f);
        int x = ledge.left + 6 + (int)(hash01(ledge.seed * 3 + i) * (ledge.width - 12));
        int y = ledge.top - 2 - (int)(t * 18.0f);
        DrawRectangle(x, y, 1, 1, withAlpha(FIREFLY, pulse * (1.0f - t)));
    }
}

// Mist over a ledge that no permitted link leads to from the current node.
void drawMistVeil(const Ledge& ledge, float amount) {
    if (amount <= 0.01f) return;
    Color veil = withAlpha(VEIL, 0.55f * amount);
    for (int dy = 0; dy < CANVAS_HEIGHT; dy++) {
        int left, right;
        if (!rowSpan(ledge, dy, left, right)) break;
        if (right > left) DrawRectangle(left, ledge.top + dy, right - left, 1, veil);
    }
}

// A lantern on a post at the left end of the checkpoint ledge.
void drawCheckpointLantern(const Ledge& ledge, float flash, float time) {
    int postX = ledge.left + 6;
    int base = ledge.top;
    DrawRectangle(postX, base - 18, 2, 18, BARK_DARK);    // post
    DrawRectangle(postX, base - 18, 7, 1, BARK_DARK);     // arm
    DrawRectangle(postX + 6, base - 17, 1, 2, BARK_DARK); // hook

    float flicker = 0.85f + 0.15f * std::sin(time * 11.0f) * std::sin(time * 6.3f);
    int lx = postX + 4;
    int ly = base - 15;
    DrawCircleGradient(lx + 2, ly + 3, 20.0f + 18.0f * flash, withAlpha(FIREFLY, 0.3f * flicker + 0.35f * flash),
                       withAlpha(FIREFLY, 0.0f));
    DrawRectangle(lx + 1, ly - 1, 3, 1, BARK_DARK); // cap
    DrawRectangle(lx, ly, 5, 6, BARK_DARK);         // frame
    DrawRectangle(lx + 1, ly + 1, 3, 4, (flicker > 0.9f) ? FIREFLY : Color{255, 196, 100, 255});
    DrawRectangle(lx + 2, ly + 2, 1, 2, WHITE);     // flame

    for (int i = 0; flash > 0.0f && i < 10; i++) { // sparks when the checkpoint lights up
        float angle = i * 0.628f + 0.3f;
        float distance = (1.0f - flash) * 16.0f;
        DrawRectangle(lx + 2 + (int)(std::cos(angle) * distance),
                      ly + 3 + (int)(std::sin(angle) * distance - (1.0f - flash) * 6.0f), 1, 1, withAlpha(FIREFLY, flash));
    }
}

// An old stone arch around the exit. Vines block it until the objective is done.
void drawExitArch(Rectangle gate, int cameraX, bool open, float time) {
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    const int bottom = gy + gh;
    const int radius = gw / 2;
    const int pillarTop = gy + radius;
    const Vector2 center = {(float)(gx + radius), (float)pillarTop};

    // The opening: warm light when open, dark leaves when closed.
    if (open) {
        DrawCircleGradient(gx + radius, gy + gh / 2, 44.0f, withAlpha(FIREFLY, 0.35f), withAlpha(FIREFLY, 0.0f));
        DrawCircleSector(center, (float)radius, 180.0f, 360.0f, 16, withAlpha(SUN, 0.9f));
        DrawRectangleGradientV(gx, pillarTop, gw, bottom - pillarTop, withAlpha(SUN, 0.9f), withAlpha(MOSS_LIGHT, 0.6f));
        for (int i = 0; i < 6; i++) { // light motes drifting out
            float t = std::fmod(time * 0.4f + i * 0.17f, 1.0f);
            int x = gx + 2 + (int)(hash01(i * 5 + 9) * (gw - 4));
            DrawRectangle(x, bottom - 2 - (int)(t * gh), 1, 1, withAlpha(WHITE, 1.0f - t));
        }
    } else {
        Color shade = {16, 30, 28, 255};
        DrawCircleSector(center, (float)radius, 180.0f, 360.0f, 16, shade);
        DrawRectangle(gx, pillarTop, gw, bottom - pillarTop, shade);
        for (int i = 0; i < 28; i++) {
            int x = gx + (int)(hash01(i * 7 + 1) * (gw - 1));
            int y = gy + 1 + (int)(hash01(i * 11 + 3) * (gh - 2));
            float dx = x - center.x;
            float dy = y - center.y;
            if (y < pillarTop && dx * dx + dy * dy > radius * radius) continue; // stay inside the arch
            DrawRectangle(x, y, 2, 1, (i % 3 == 0) ? MOSS : MOSS_DARK);
        }
    }

    // Stone frame: two pillars and a round arch, lit on the right.
    DrawRectangle(gx - 5, pillarTop, 5, bottom - pillarTop, STONE);
    DrawRectangle(gx + gw, pillarTop, 5, bottom - pillarTop, STONE);
    DrawRectangle(gx - 5, pillarTop, 1, bottom - pillarTop, STONE_DARK);
    DrawRectangle(gx + gw + 4, pillarTop, 1, bottom - pillarTop, STONE_LIGHT);
    DrawRing(center, (float)radius, radius + 5.0f, 180.0f, 360.0f, 20, STONE);
    DrawRing(center, radius + 4.0f, radius + 5.0f, 270.0f, 360.0f, 10, STONE_LIGHT);
    DrawRectangle((int)center.x - 2, gy - 6, 5, 6, STONE_LIGHT); // keystone
    DrawRectangle(gx - 6, bottom - 3, 7, 3, STONE_DARK);         // pillar bases
    DrawRectangle(gx + gw - 1, bottom - 3, 7, 3, STONE_DARK);
    Color rune = open ? FIREFLY : STONE_DARK;
    for (int k = 0; k < 3; k++) {
        DrawRectangle(gx - 3, pillarTop + 4 + k * 6, 1, 3, rune);
        DrawRectangle(gx + gw + 2, pillarTop + 4 + k * 6, 1, 3, rune);
    }
    for (int i = 0; i < 5; i++) { // moss on top of the arch
        DrawRectangle((int)center.x - 8 + i * 4, gy - 5 + (i == 0 || i == 4 ? 2 : 0), 3, 1, MOSS);
    }
}

// ---------------------------------------------------------------------------
// Background layers, far to near. Each layer scrolls at a fraction of the camera.
// ---------------------------------------------------------------------------

// Height of a treeline at layer position x: overlapping round crowns and a few pointed firs.
float treelineTop(float x, float spacing, float baseY, float minRadius, float maxRadius, int seed) {
    int index = (int)std::floor(x / spacing);
    float best = baseY;
    for (int i = index - 2; i <= index + 2; i++) {
        float center = i * spacing + hash01(seed + i * 7) * spacing * 0.6f;
        float radius = minRadius + hash01(seed + i * 13) * (maxRadius - minRadius);
        float ground = baseY - hash01(seed + i * 29) * radius * 0.6f;
        float d = std::fabs(x - center);
        if (d >= radius) continue;
        bool fir = hash01(seed + i * 41) > 0.62f;
        float top = fir ? ground - radius * 1.7f + d * 1.7f : ground - std::sqrt(radius * radius - d * d);
        if (top < best) best = top;
    }
    return best;
}

void drawSkyAndFarForest(int cameraX) {
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 150, SKY_TOP, SKY_MID);
    DrawRectangleGradientV(0, 150, CANVAS_WIDTH, CANVAS_HEIGHT - 150, SKY_MID, SKY_LOW);

    float offset = cameraX * 0.12f;
    for (int x = 0; x < CANVAS_WIDTH; x++) {
        int top = (int)treelineTop(x + offset, 24.0f, 196.0f, 16.0f, 30.0f, 101);
        DrawRectangle(x, top, 1, CANVAS_HEIGHT - top, FAR_TREES);
    }
    DrawRectangleGradientV(0, 130, CANVAS_WIDTH, 120, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.4f)); // haze

    // Warm light from behind the trees blooms over the distant forest.
    int sunX = 450 - (int)(cameraX * 0.05f);
    DrawCircleGradient(sunX, 140, 270.0f, withAlpha(SUN, 0.26f), withAlpha(SUN, 0.0f));
    DrawCircleGradient(sunX, 140, 90.0f, withAlpha(SUN, 0.32f), withAlpha(SUN, 0.0f));
}

// Slanted beams of light that fade as they fall.
void drawLightShafts(int cameraX, float time) {
    float offset = cameraX * 0.2f;
    for (int i = 0; i < 4; i++) {
        float topX = 330.0f + i * 70.0f - offset;
        float width = 16.0f + i * 6.0f;
        float slant = -130.0f;
        float alpha = 0.06f + 0.025f * std::sin(time * 0.6f + i * 1.7f);
        for (int segment = 0; segment < 3; segment++) {
            float y0 = segment * 100.0f;
            float y1 = y0 + 100.0f;
            float x0 = topX + slant * (y0 / 300.0f);
            float x1 = topX + slant * (y1 / 300.0f);
            Color light = withAlpha(SUN, alpha * (1.0f - segment * 0.3f));
            drawTriangleSafe(Vector2{x0, y0}, Vector2{x0 + width, y0}, Vector2{x1 + width, y1}, light);
            drawTriangleSafe(Vector2{x0, y0}, Vector2{x1 + width, y1}, Vector2{x1, y1}, light);
        }
    }
}

// Midground trees: dark trunks with round crowns, lit on the right side.
void drawMidTrees(int cameraX) {
    float offset = cameraX * 0.35f;
    const float spacing = 96.0f;
    int first = (int)std::floor(offset / spacing) - 1;
    for (int i = first; i < first + 9; i++) {
        float x = i * spacing + hash01(i * 17 + 5) * 40.0f - offset;
        int trunkWidth = 6 + (int)(hash01(i * 23 + 7) * 6.0f);
        int crownY = 40 + (int)(hash01(i * 31 + 9) * 50.0f);
        float radius = 22.0f + hash01(i * 37 + 3) * 14.0f;
        DrawRectangle((int)x - trunkWidth / 2, crownY, trunkWidth, CANVAS_HEIGHT - crownY, MID_TREES);
        DrawRectangle((int)x + trunkWidth / 2 - 1, crownY + 10, 1, CANVAS_HEIGHT - crownY - 10,
                      withAlpha(GIANT_RIM, 0.25f));
        // A clustered crown. Each lighter circle sits up and to the right of its dark twin,
        // so a thin lit edge shows where the light comes from.
        Color lit = lerpColor(MID_TREES, FAR_TREES, 0.45f);
        const float dxs[5] = {0.0f, -0.8f, 0.8f, -0.4f, 0.45f};
        const float dys[5] = {0.0f, 0.3f, 0.25f, -0.45f, -0.4f};
        const float sizes[5] = {1.0f, 0.7f, 0.75f, 0.6f, 0.62f};
        for (int c = 0; c < 5; c++) {
            float r = radius * sizes[c];
            DrawCircle((int)(x + dxs[c] * radius + 2.0f), (int)(crownY + dys[c] * radius - 2.0f), r, lit);
        }
        for (int c = 0; c < 5; c++) {
            DrawCircle((int)(x + dxs[c] * radius), (int)(crownY + dys[c] * radius), radius * sizes[c], MID_TREES);
        }
    }
    DrawRectangleGradientV(0, 170, CANVAS_WIDTH, 110, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.26f));
}

// The landmark: a giant old tree behind the middle of the route.
void drawGiantTree(int cameraX, float time) {
    float cx = 360.0f - cameraX * 0.5f;
    Color shadow = lerpColor(GIANT_BARK, DARK, 0.4f);
    for (int y = 0; y < CANVAS_HEIGHT; y++) {
        float half = 22.0f + std::sin(y * 0.045f) * 2.0f;
        if (y > 230) half += (y - 230) * 0.45f; // roots flare out at the base
        if (y < 40) half += (40 - y) * 0.5f;    // the trunk opens into branches
        int left = (int)(cx - half);
        int right = (int)(cx + half);
        DrawRectangle(left, y, right - left, 1, GIANT_BARK);
        DrawRectangle(left, y, 3, 1, shadow);
        DrawRectangle(right - 3, y, 3, 1, GIANT_RIM); // the side facing the light
    }
    for (int g = 0; g < 4; g++) { // bark grooves
        float gx = cx - 12.0f + g * 8.0f;
        for (int y = 20; y < CANVAS_HEIGHT; y++) {
            int wiggle = (int)std::round(std::sin(y * 0.08f + g * 1.9f) * 1.5f);
            if (hash01(g * 977 + y) < 0.8f) DrawRectangle((int)gx + wiggle, y, 1, 1, shadow);
        }
    }
    DrawEllipse((int)cx - 4, 150, 7.0f, 11.0f, lerpColor(GIANT_BARK, DARK, 0.6f)); // a hollow, faintly lit
    DrawCircleGradient((int)cx - 4, 152, 10.0f, withAlpha(FIREFLY, 0.12f + 0.05f * std::sin(time * 1.5f)),
                       withAlpha(FIREFLY, 0.0f));
    DrawLineEx(Vector2{cx - 10.0f, 40.0f}, Vector2{cx - 90.0f, -10.0f}, 10.0f, GIANT_BARK);
    DrawLineEx(Vector2{cx + 10.0f, 36.0f}, Vector2{cx + 100.0f, -6.0f}, 9.0f, GIANT_BARK);
    for (int i = 0; i < 6; i++) { // moss hanging from the left branch
        float t = 0.2f + i * 0.13f;
        int length = 6 + (int)(hash01(i * 3 + 77) * 12.0f);
        DrawRectangle((int)(cx - 10.0f - 80.0f * t), (int)(40.0f - 50.0f * t) + 4, 1, length, VINE);
    }
}

// Dark leaf masses along the top edge, with vines hanging from them (kept well above the route).
void drawCanopy(int cameraX, float time) {
    float offset = cameraX * 0.6f;
    const float spacing = 34.0f;
    int first = (int)std::floor(offset / spacing) - 2;
    for (int i = first; i < first + 24; i++) {
        float x = i * spacing - offset + hash01(i * 13 + 1) * 12.0f;
        float y = -8.0f + hash01(i * 19 + 2) * 16.0f;
        float radius = 16.0f + hash01(i * 29 + 3) * 14.0f;
        DrawCircle((int)x, (int)y, radius, CANOPY);
        DrawRectangle((int)(x - radius * 0.4f), (int)(y + radius - 2.0f), 3, 1, VINE); // leaf highlights
        DrawRectangle((int)(x + radius * 0.2f), (int)(y + radius - 1.0f), 2, 1, VINE);

        if (i % 2 != 0) continue;
        int length = 30 + (int)(hash01(i * 47 + 5) * 60.0f); // a hanging vine
        int startY = (int)(y + radius * 0.6f);
        for (int k = 0; k < length; k++) {
            float sway = std::sin(time * 1.2f + i + k * 0.04f) * 3.0f * (k / (float)length);
            int vx = (int)(x + sway);
            DrawRectangle(vx, startY + k, 1, 1, VINE);
            if (k % 5 == 2) DrawRectangle(vx + ((k / 5) % 2 == 0 ? 1 : -2), startY + k, 2, 1, MOSS_DARK);
        }
    }
}

void drawFireflies(int cameraX, float time) {
    float offset = cameraX * 0.9f;
    for (int i = 0; i < 28; i++) {
        float x = std::fmod(hash01(i * 11 + 4) * 1100.0f - offset + 4400.0f, 1100.0f) - 100.0f;
        x += std::sin(time * 0.5f + i * 1.3f) * 10.0f;
        float y = 70.0f + hash01(i * 7 + 2) * 200.0f + std::sin(time * 0.7f + i) * 8.0f;
        float twinkle = 0.5f + 0.5f * std::sin(time * 2.3f + i * 2.1f);
        DrawCircleGradient((int)x, (int)y, 5.0f, withAlpha(FIREFLY, 0.3f * twinkle), withAlpha(FIREFLY, 0.0f));
        DrawRectangle((int)x, (int)y, 1, 1, withAlpha(FIREFLY, 0.4f + 0.6f * twinkle));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Public drawing functions
// ---------------------------------------------------------------------------

void drawForestBackground(int cameraX, float time) {
    drawSkyAndFarForest(cameraX);
    drawLightShafts(cameraX, time);
    drawMidTrees(cameraX);
    drawGiantTree(cameraX, time);
    drawCanopy(cameraX, time);
    drawFireflies(cameraX, time);
    // Mist just behind the route separates the ledges from the trees.
    DrawRectangleGradientV(0, 215, CANVAS_WIDTH, 80, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.2f));
}

void drawForestLedges(const ForestView& view) {
    // The ledge the current node's next points to. Checked for NULL before reading it.
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;

    // Walk the real list from its head. The bound and the head check keep the loop finite.
    const Node* node = view.head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        bool onScreen = ledge.left + ledge.width > -40 && ledge.left < CANVAS_WIDTH + 40;
        if (onScreen) {
            float mist = 0.0f;
            if (view.mistById != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION) {
                mist = view.mistById[node->id];
            }
            fillLedge(ledge);
            drawLedgeDetails(ledge, mist, view.time);
            drawMistVeil(ledge, mist);
            if (node == next) drawNextGlow(ledge, view.time);
        }
        node = node->next;
        steps++;
        if (node == view.head) break; // circular list: back at the start
    }

    if (view.checkpoint != nullptr) {
        drawCheckpointLantern(ledgeFor(*view.checkpoint, view.cameraX), view.checkpointFlash, view.time);
    }
    if (view.hasGate) {
        drawExitArch(view.gateRect, view.cameraX, view.gateOpen, view.time);
    }
}

void drawForestForeground(int cameraX, float time) {
    // Ravine mist: the ledges' bases disappear into it.
    DrawRectangleGradientV(0, 268, CANVAS_WIDTH, 92, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.32f));
    for (int i = 0; i < 6; i++) { // slow drifting mist banks
        float x = std::fmod(i * 170.0f + time * 5.0f - cameraX * 1.05f + 4000.0f, 900.0f) - 130.0f;
        DrawEllipse((int)x, 300 + (i % 3) * 14, 90.0f + (i % 2) * 40.0f, 10.0f, withAlpha(MIST, 0.1f));
    }
    DrawRectangleGradientV(0, 318, CANVAS_WIDTH, 42, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.75f));

    // Dark grass and ferns along the bottom edge (nearer than the route, so they scroll faster).
    float offset = cameraX * 1.3f;
    for (int i = 0; i < 34; i++) {
        float x = i * 34.0f + hash01(i * 5 + 1) * 12.0f - offset;
        if (x < -30.0f || x > CANVAS_WIDTH + 30.0f) continue;
        int blades = 3 + (int)(hash01(i * 7 + 2) * 3.0f);
        for (int b = 0; b < blades; b++) { // tapered leaves
            float height = 14.0f + hash01(i * 11 + b) * 26.0f;
            float lean = (b - blades / 2.0f) * 5.0f + std::sin(time * 1.6f + i + b) * 1.5f;
            float baseX = x + b * 4.0f;
            drawTriangleSafe(Vector2{baseX - 2.0f, (float)CANVAS_HEIGHT + 2.0f}, Vector2{baseX + 2.0f, (float)CANVAS_HEIGHT + 2.0f},
                             Vector2{baseX + lean, CANVAS_HEIGHT - height}, DARK);
        }
    }

    // Two big trunks frame the start and the end of the region.
    const float trunkLayerX[2] = {6.0f, 976.0f};
    for (int i = 0; i < 2; i++) {
        float x = trunkLayerX[i] - offset;
        if (x < -40.0f || x > CANVAS_WIDTH + 40.0f) continue;
        DrawRectangle((int)x - 12, 0, 24, CANVAS_HEIGHT, DARK);
        DrawRectangle((int)x + 10, 0, 2, CANVAS_HEIGHT, withAlpha(VINE, 0.8f));
        DrawCircle((int)x, 4, 34.0f, DARK);
    }

    // Leaves overhanging the top edge.
    for (int i = 0; i < 14; i++) {
        float x = i * 80.0f + hash01(i * 5 + 3) * 30.0f - cameraX * 1.15f;
        if (x < -40.0f || x > CANVAS_WIDTH + 40.0f) continue;
        float y = -10.0f + hash01(i * 9 + 4) * 8.0f;
        DrawCircle((int)x, (int)y, 18.0f + hash01(i * 13 + 5) * 10.0f, DARK);
        for (int k = 0; k < 3; k++) {
            DrawRectangle((int)x - 10 + k * 9, (int)y + 16 + (k % 2) * 3, 2, 3, DARK);
        }
    }

    // Soft vignette keeps the eye on the middle of the screen.
    DrawRectangleGradientH(0, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.45f), withAlpha(DARK, 0.0f));
    DrawRectangleGradientH(CANVAS_WIDTH - 70, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.45f));
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 50, withAlpha(DARK, 0.5f), withAlpha(DARK, 0.0f));
}

void drawForestWisp(Vector2 position, float strength, float time) {
    float flicker = 0.75f + 0.25f * std::sin(time * 9.0f);
    DrawCircleGradient((int)position.x, (int)position.y, 9.0f, withAlpha(FIREFLY, 0.35f * flicker * strength),
                       withAlpha(FIREFLY, 0.0f));
    DrawRectangle((int)position.x, (int)position.y, 2, 2, withAlpha(Color{255, 250, 225, 255}, 0.95f * strength));
}

void drawArrivalBurst(Vector2 position, float t) {
    float fade = 1.0f - t;
    DrawRing(position, 3.0f + t * 14.0f, 4.0f + t * 14.0f, 180.0f, 360.0f, 20, withAlpha(FIREFLY, 0.6f * fade));
    for (int i = 0; i < 9; i++) { // leaves and sparks kicked up, then falling
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        float speed = 10.0f + hash01(i * 7) * 14.0f;
        float x = position.x + side * speed * t * (0.6f + hash01(i * 3) * 0.8f);
        float y = position.y - (14.0f + hash01(i * 11) * 10.0f) * t + 22.0f * t * t;
        Color color = (i % 3 == 0) ? FIREFLY : ((i % 3 == 1) ? MOSS_LIGHT : MOSS);
        DrawRectangle((int)x, (int)y, (i % 2 == 0) ? 2 : 1, 1, withAlpha(color, fade));
    }
}
