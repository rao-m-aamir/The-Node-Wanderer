#include "ArtCommon.h"

// Region 4, "The Edge of Nothing": cliffs above a misty abyss before dawn.
// N3's next is NULL, so past its edge there is nothing: no platform is drawn there.
// When append() creates the bridge node, the bridge assembles from that real node.

namespace {

const Color SKY_TOP = {10, 14, 28, 255};
const Color SKY_MID = {36, 48, 74, 255};
const Color SKY_LOW = {92, 106, 128, 255};
const Color DAWN = {255, 186, 130, 255};
const Color MOON = {220, 228, 240, 255};
const Color FAR_PEAKS = {44, 54, 76, 255};
const Color NEAR_PEAKS = {30, 36, 54, 255};
const Color SPIRE = {22, 28, 42, 255};
const Color SPIRE_LIT = {52, 62, 86, 255};
const Color MIST = {160, 172, 196, 255};
const Color VEIL = {96, 106, 128, 255};
const Color DEPTH = {14, 16, 28, 255};
const Color DARK = {6, 8, 14, 255};

const Color ROCK_LIGHT = {178, 176, 182, 255};
const Color ROCK = {98, 100, 114, 255};
const Color ROCK_DARK = {62, 64, 80, 255};
const Color ROCK_SHADE = {38, 40, 56, 255};
const Color GRASS_LIGHT = {172, 198, 122, 255};
const Color GRASS = {108, 140, 90, 255};
const Color GRASS_DARK = {60, 86, 70, 255};
const Color WOOD_LIGHT = {178, 134, 90, 255};
const Color WOOD = {124, 88, 60, 255};
const Color WOOD_DARK = {78, 54, 42, 255};
const Color ROPE = {196, 170, 120, 255};
const Color PINE = {28, 42, 46, 255};

const LedgeMaterial CLIFF_ROCK = {GRASS_LIGHT, GRASS, GRASS_DARK, 3, ROCK, ROCK_SHADE, ROCK_LIGHT, DEPTH};

enum class EdgeShape { Lookout, RopePillar, EdgeCliff, Bridge };

EdgeShape shapeFor(const Node& node) {
    switch (node.id) {
        case 2: return EdgeShape::RopePillar;
        case 3: return EdgeShape::EdgeCliff;
        case 4: return EdgeShape::Bridge;
        default: return EdgeShape::Lookout;
    }
}

float easeOut(float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// Rock outlines for the three cliff platforms (the bridge is drawn plank by plank instead).
Silhouette buildEdgeSilhouette(EdgeShape shape, const Ledge& l) {
    Silhouette s;
    s.top = l.top;
    const int x0 = l.left;
    const int x1 = l.left + l.width;
    const int w = l.width;
    const float seed = (float)l.seed;
    for (int dy = 0; l.top + dy < CANVAS_HEIGHT; dy++) {
        int jagLeft = (int)std::round(std::fabs(std::sin(dy * 0.31f + seed)) * 3.0f + std::sin(dy * 0.9f + seed) * 1.0f);
        int jagRight = (int)std::round(std::fabs(std::sin(dy * 0.27f + seed * 1.7f)) * 3.0f);
        switch (shape) {
            case EdgeShape::Lookout: { // a broad rocky cliff that spreads toward its base
                if (dy < 3) { s.add(dy, x0, x1); break; }
                int spread = (dy > 10) ? (dy - 10) / 7 : 0;
                s.add(dy, x0 + 2 + jagLeft - spread, x1 - 2 - jagRight + spread);
                break;
            }
            case EdgeShape::RopePillar: { // a rock pillar standing in the abyss
                if (dy < 3) { s.add(dy, x0, x1); break; }
                int inset = (dy < 10) ? (dy - 3) * (w / 4) / 7 : w / 4 - (dy - 10) / 14;
                s.add(dy, x0 + inset + jagLeft, x1 - inset - jagRight);
                break;
            }
            case EdgeShape::EdgeCliff: { // the last cliff: its right face overhangs the abyss
                if (dy < 3) { s.add(dy, x0, x1); break; }
                int spread = (dy > 10) ? (dy - 10) / 7 : 0;
                int undercut = (dy - 3) / 4;
                s.add(dy, x0 + 2 + jagLeft - spread, x1 - undercut - jagRight);
                break;
            }
            case EdgeShape::Bridge:
                break;
        }
    }
    return s;
}

// Cracks, ledges, and lit facets in the rock, clipped to the outline.
void drawRockDetails(const Ledge& l, const Silhouette& s) {
    for (int i = 0; i < 22; i++) { // short, uneven cracks instead of regular stripes
        int dy = 6 + (int)(hash01(l.seed * 3 + i * 17) * 90.0f);
        if (dy >= s.rows || s.count[dy] == 0) continue;
        const Span& span = s.spans[dy][0];
        int width = span.right - span.left;
        if (width < 12) continue;
        int x = span.left + 3 + (int)(hash01(l.seed * 5 + i * 31) * (width - 12));
        int length = 3 + (int)(hash01(l.seed * 7 + i * 13) * 8.0f);
        for (int k = 0; k < length; k++) {
            int y = dy + ((k > length / 2) ? 1 : 0);
            if (insideSilhouette(s, x + k, y)) DrawRectangle(x + k, l.top + y, 1, 1, depthShade(ROCK_SHADE, y, DEPTH));
        }
        if (insideSilhouette(s, x, dy - 1)) DrawRectangle(x, l.top + dy - 1, length, 1, depthShade(ROCK_LIGHT, dy, DEPTH)); // a lit lip above it
    }
    for (int f = 0; f < 4; f++) { // vertical fissures
        int x = l.left + 6 + (int)(hash01(l.seed * 7 + f) * (l.width - 12));
        int startDy = 5 + (int)(hash01(l.seed * 11 + f) * 20.0f);
        for (int k = 0; k < 18; k++) {
            int dx = (int)std::round(std::sin(k * 0.5f + f) * 1.0f);
            if (insideSilhouette(s, x + dx, startDy + k)) DrawRectangle(x + dx, l.top + startDy + k, 1, 1, depthShade(DARK, startDy + k, DEPTH));
        }
    }
    for (int dy = 3; dy < 60 && dy < s.rows; dy++) { // the face toward the light is a lighter facet
        if (s.count[dy] == 0) continue;
        const Span& span = s.spans[dy][0];
        int facet = 3 + (int)(std::fabs(std::sin(dy * 0.21f + l.seed)) * 6.0f);
        DrawRectangle(span.right - 1 - facet, l.top + dy, facet, 1, withAlpha(depthShade(ROCK_LIGHT, dy, DEPTH), 0.45f));
    }
}

// A wind-bent pine on the cliff, leaning out over the abyss.
void drawWindPine(int x, int baseY, float time) {
    float gust = std::fmax(0.0f, std::sin(time * 0.9f)); // now and then a stronger gust bends it further
    float sway = std::sin(time * 1.4f) * (1.0f + 1.5f * gust);
    DrawLineEx(Vector2{(float)x, (float)baseY}, Vector2{x - 6.0f + sway, baseY - 24.0f}, 2.0f, WOOD_DARK);
    for (int layer = 0; layer < 4; layer++) { // foliage layers swept by the wind, the top ones most
        float y = baseY - 10.0f - layer * 5.0f;
        float cx = x - 3.0f - layer * 1.2f + sway * (0.6f + 0.25f * layer);
        float width = 14.0f - layer * 3.0f;
        drawTriangleSafe(Vector2{cx - width, y + 2.0f}, Vector2{cx + width * 0.6f, y + 2.0f}, Vector2{cx - width * 0.3f, y - 5.0f}, PINE);
    }
}

// The old bridge anchor at the edge. Before the repair its ropes hang snapped into the abyss.
// reel: 0 = the snapped ropes hang free, rising toward 1 as they are drawn up; 1 = connected.
void drawAnchorPost(int x, int top, float reel, float time, float alpha = 1.0f) {
    DrawRectangle(x, top - 10, 3, 10, withAlpha(WOOD, alpha));
    DrawRectangle(x, top - 10, 1, 10, withAlpha(WOOD_DARK, alpha));
    DrawRectangle(x - 1, top - 11, 5, 2, withAlpha(WOOD_LIGHT, alpha));
    DrawRectangle(x + 1, top - 7, 1, 2, withAlpha(lerpColor(HAZARD_AMBER, WARM_LIGHT, smooth01(reel)), alpha)); // a carved rune
    if (reel >= 1.0f) return;
    float hang = 1.0f - smooth01(reel);
    for (int r = 0; r < 2; r++) { // snapped ropes swinging in the wind
        int length = (int)std::round(((r == 0) ? 40 : 26) * hang);
        float swing = std::sin(time * 1.3f + r * 1.7f) * 3.0f;
        int startY = top - 8 + r * 6;
        for (int k = 0; k < length; k++) {
            float t = k / (float)length;
            DrawRectangle(x + 3 + (int)(t * 6.0f + swing * t * t), startY + k, 1, 1, withAlpha(ROPE, alpha));
        }
        if (r == 0 && length > 2) { // a plank still tied to the end
            int px = x + 3 + (int)(6.0f + swing);
            DrawRectangle(px - 1, startY + length, 3, 6, withAlpha(WOOD, alpha * hang));
            DrawRectangle(px - 1, startY + length, 1, 6, withAlpha(WOOD_DARK, alpha * hang));
        }
    }
}

// The bridge node, drawn from its real position and width. While it is being created
// (progress 0 -> 1), the planks are laid out from N3's edge one by one, the far pier rises
// out of the mist, and the rope rails are strung last.
void drawBridge(const Ledge& l, float progress, float time) {
    const int x1 = l.left + l.width;
    float rise = easeOut(progress / 0.5f);
    int pierLeft = x1 - 34;
    int pierRight = x1 - 4;
    int pierTop = l.top + 4 + (int)((1.0f - rise) * 140.0f);
    for (int y = pierTop; y < CANVAS_HEIGHT; y++) {
        int dy = y - l.top;
        int widen = (dy > 10) ? (dy - 10) / 9 : 0;
        int jag = (int)std::round(std::fabs(std::sin(dy * 0.33f)) * 2.0f);
        int left = pierLeft - widen + jag;
        int right = pierRight + widen;
        DrawRectangle(left, y, right - left, 1, depthShade(ROCK, dy, DEPTH));
        DrawRectangle(left, y, 2, 1, depthShade(ROCK_SHADE, dy, DEPTH));
        DrawRectangle(right - 1, y, 1, 1, depthShade(ROCK_LIGHT, dy, DEPTH));
        int crack = left + 3 + (int)(hash01(dy * 13 + 7) * (right - left - 8));
        if (hash01(dy * 29 + 3) < 0.25f) DrawRectangle(crack, y, 3 + (int)(hash01(dy * 7) * 4.0f), 1, depthShade(ROCK_SHADE, dy, DEPTH));
    }

    const int plankCount = l.width / 5;
    float laid = progress / 0.7f * plankCount; // starts at once at the near end, so the deck is there to step on
    for (int i = 0; i < plankCount; i++) {
        float appear = std::fmin(1.0f, (laid - i) / 1.5f);
        if (appear <= 0.0f) break;
        int drop = 0; // each plank drops into place, dips a pixel as it lands, and settles
        if (appear < 0.75f) drop = -(int)std::round((1.0f - smooth01(appear / 0.75f)) * 9.0f);
        else if (appear < 1.0f) drop = 1;
        int x = l.left + i * 5;
        DrawRectangle(x, l.top + drop, 4, 4, WOOD);
        DrawRectangle(x, l.top + drop, 4, 1, WOOD_LIGHT);
        DrawRectangle(x, l.top + drop + 3, 4, 1, WOOD_DARK);
        if (appear < 1.0f) DrawRectangle(x + 1, l.top + drop - 2, 2, 1, withAlpha(WARM_LIGHT, 1.0f - appear));
    }

    float rails = smooth01((progress - 0.72f) / 0.28f);
    if (rails <= 0.0f) return;
    Color rope = withAlpha(ROPE, rails);
    int farPost = pierLeft + 4;
    drawAnchorPost(farPost, l.top, 1.0f, time, rails);
    for (int x = l.left - 2; x < farPost; x++) { // a rope rail sagging between the posts, with hangers
        float t = (x - (l.left - 2)) / (float)(farPost - (l.left - 2));
        int y = l.top - 8 + (int)(6.0f * 4.0f * t * (1.0f - t));
        DrawRectangle(x, y, 1, 1, rope);
        if ((x - l.left) % 10 == 0) DrawRectangle(x, y, 1, l.top - y, withAlpha(ROPE, 0.6f * rails));
    }
    for (int x = l.left; x < pierLeft; x++) { // the support rope under the deck
        float t = (x - l.left) / (float)(pierLeft - l.left);
        DrawRectangle(x, l.top + 5 + (int)(10.0f * 4.0f * t * (1.0f - t)), 1, 1, rope);
    }
}

// Two standing stones and a lintel at the far end of the bridge. They appear as the bridge
// finishes (appear), and dawn light fills them once the region is complete (open: 0 -> 1).
void drawDawnGate(Rectangle gate, int cameraX, float open, float appear, float time) {
    if (appear <= 0.0f) return;
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    float a = smooth01(appear);
    float light = smooth01(open);
    DrawRectangle(gx, gy, gw, gh, withAlpha(Color{20, 24, 38, 255}, 0.9f * a * (1.0f - light)));
    for (int i = 0; light < 1.0f && i < 10; i++) {
        DrawRectangle(gx + (int)(hash01(i * 3 + 1) * gw), gy + (int)(hash01(i * 5 + 2) * gh), 2, 1, withAlpha(MIST, 0.3f * a * (1.0f - light)));
    }
    if (light > 0.0f) {
        DrawCircleGradient(gx + gw / 2, gy + gh / 2, 30.0f + 16.0f * light, withAlpha(DAWN, 0.35f * a * light), withAlpha(DAWN, 0.0f));
        DrawRectangleGradientV(gx, gy, gw, gh, withAlpha(Color{255, 236, 190, 255}, 0.92f * a * light), withAlpha(DAWN, 0.6f * a * light));
        for (int i = 0; i < 6; i++) {
            float t = std::fmod(time * (0.35f + 0.1f * hash01(i * 5)) + i * 0.17f, 1.0f);
            DrawRectangle(gx + 2 + (int)(hash01(i * 7 + 2) * (gw - 4)), gy + gh - 2 - (int)(t * gh), 1, 1,
                          withAlpha(WHITE, loopEnvelope(t) * a * light));
        }
    }
    DrawRectangle(gx - 6, gy - 2, 6, gh + 2, withAlpha(ROCK, a)); // standing stones
    DrawRectangle(gx + gw, gy - 2, 6, gh + 2, withAlpha(ROCK, a));
    DrawRectangle(gx + gw + 5, gy - 2, 1, gh + 2, withAlpha(ROCK_LIGHT, a));
    DrawRectangle(gx - 6, gy - 2, 1, gh + 2, withAlpha(ROCK_SHADE, a));
    DrawRectangle(gx - 9, gy - 8, gw + 18, 6, withAlpha(ROCK, a)); // lintel
    DrawRectangle(gx - 9, gy - 8, gw + 18, 1, withAlpha(ROCK_LIGHT, a));
    Color rune = lerpColor(ROCK_SHADE, WARM_LIGHT, light);
    for (int k = 0; k < 3; k++) {
        DrawRectangle(gx - 4, gy + 5 + k * 9, 2, 2, withAlpha(rune, a));
        DrawRectangle(gx + gw + 2, gy + 5 + k * 9, 2, 2, withAlpha(rune, a));
    }
}

// The NULL gap: no platform, just an amber glow far below and embers falling into it.
// It fades as the bridge is built.
void drawNullGap(const SceneView& view) {
    float hazard = view.bridgeBuilt ? 1.0f - easeOut(view.bridgeProgress) : 1.0f;
    if (hazard <= 0.01f || view.hazardZone.width <= 0.0f) return;
    int gx = toCanvasX(view.hazardZone.x, view.cameraX);
    int gw = (int)(view.hazardZone.width / CANVAS_SCALE);
    float pulse = 0.75f + 0.25f * std::sin(view.time * 2.6f);
    DrawCircleGradient(gx + gw / 2, CANVAS_HEIGHT + 30, 180.0f, withAlpha(HAZARD_AMBER, 0.55f * hazard * pulse),
                       withAlpha(HAZARD_AMBER, 0.0f));
    // A column of light rising out of the gap, brightest at its center and soft at its sides
    // (built from thin horizontal gradients that get stronger toward the bottom). It breathes and wavers.
    float half = gw / 2.0f;
    for (int y = 232; y < CANVAS_HEIGHT; y += 4) {
        float strength = (y - 232) / (float)(CANVAS_HEIGHT - 232);
        int waver = (int)std::round(std::sin(view.time * 2.4f + y * 0.15f) * 1.5f);
        Color glow = withAlpha(HAZARD_AMBER, 0.32f * hazard * strength * (0.8f + 0.2f * pulse));
        Color clear = withAlpha(HAZARD_AMBER, 0.0f);
        DrawRectangleGradientH(gx + waver, y, (int)half, 4, clear, glow);
        DrawRectangleGradientH(gx + (int)half + waver, y, (int)half, 4, glow, clear);
    }
    for (int i = 0; i < 16; i++) { // embers falling away into the dark
        float t = std::fmod(view.time * (0.25f + 0.1f * hash01(i * 3 + 1)) + i * 0.0625f, 1.0f);
        int x = gx + (int)(hash01(i * 13 + 5) * gw + std::sin(view.time + i) * 4.0f);
        int y = 214 + (int)(t * 150.0f);
        DrawRectangle(x, y, 1, 2, withAlpha(HAZARD_AMBER, hazard * loopEnvelope(t)));
    }
}

// ---------------------------------------------------------------------------
// Background: sky before dawn, jagged peaks, rock spires, the abyss.
// ---------------------------------------------------------------------------

float dawnAmount(const SceneView& view) {
    return view.bridgeBuilt ? easeOut(view.bridgeProgress) * 0.8f : 0.0f; // the repair brings the dawn
}

void drawEdgeSky(const SceneView& view) {
    float dawn = dawnAmount(view);
    Color mid = lerpColor(SKY_MID, Color{110, 92, 112, 255}, dawn * 0.5f);
    Color low = lerpColor(SKY_LOW, DAWN, dawn * 0.55f);
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 170, SKY_TOP, mid);
    DrawRectangleGradientV(0, 170, CANVAS_WIDTH, CANVAS_HEIGHT - 170, mid, low);
    for (int i = 0; i < 50; i++) { // stars, fading as the sky warms
        int x = (int)(hash01(i * 7 + 9) * CANVAS_WIDTH);
        int y = (int)(hash01(i * 13 + 3) * 140.0f);
        float twinkle = 0.5f + 0.5f * std::sin(view.time * 1.6f + i * 2.2f);
        DrawRectangle(x, y, 1, 1, withAlpha(MOON, (1.0f - dawn) * (0.3f + 0.7f * hash01(i * 5)) * (0.4f + 0.6f * twinkle)));
    }
    int mx = 150 - (int)(view.scroll * 0.04f); // a thin crescent moon
    DrawCircleGradient(mx, 58, 60.0f, withAlpha(MOON, 0.1f * (1.0f - dawn)), withAlpha(MOON, 0.0f));
    DrawCircle(mx, 58, 11.0f, withAlpha(MOON, 1.0f - dawn * 0.6f));
    // The crescent is cut by a disc of the sky's own color at that height, so its inner edge never shows.
    DrawCircle(mx + 4, 55, 10.0f, lerpColor(SKY_TOP, mid, 55.0f / 170.0f));
    if (dawn > 0.0f) {
        DrawCircleGradient(560 - (int)(view.scroll * 0.05f), 236, 240.0f, withAlpha(DAWN, 0.45f * dawn), withAlpha(DAWN, 0.0f));
    }
}

// Two layers of jagged mountains, the far one hazier.
void drawPeaks(const SceneView& view) {
    float dawn = dawnAmount(view);
    for (int layer = 0; layer < 2; layer++) {
        float offset = std::floor(view.scroll * (layer == 0 ? 0.1f : 0.22f)); // whole pixels, so the ridges don't crawl
        float base = (layer == 0) ? 188.0f : 214.0f;
        Color color = (layer == 0) ? lerpColor(FAR_PEAKS, Color{120, 100, 120, 255}, dawn * 0.4f) : NEAR_PEAKS;
        for (int x = 0; x < CANVAS_WIDTH; x++) {
            float lx = x + offset + layer * 300.0f;
            float height = std::fabs(std::sin(lx * 0.013f + layer)) * 46.0f + std::fabs(std::sin(lx * 0.041f + 2.0f)) * 16.0f;
            int top = (int)(std::floor((base - height) / 2.0f) * 2.0f);
            DrawRectangle(x, top, 1, CANVAS_HEIGHT - top, color);
        }
        DrawRectangleGradientV(0, (int)base - 40, CANVAS_WIDTH, 90, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.25f));
        DrawRectangle(0, (int)base + 50, CANVAS_WIDTH, CANVAS_HEIGHT - (int)base - 50, withAlpha(MIST, 0.25f)); // no seam
    }
}

// Rock spires rising from the abyss, with wind-bent pines and mist between them.
void drawSpires(const SceneView& view) {
    float offset = std::floor(view.scroll * 0.45f);
    const float spacing = 120.0f;
    int first = (int)std::floor(offset / spacing) - 1;
    for (int i = first; i < first + 8; i++) {
        float x = std::floor(i * spacing + hash01(i * 17 + 2) * 50.0f - offset);
        int width = 18 + (int)(hash01(i * 23 + 4) * 16.0f);
        int top = 150 + (int)(hash01(i * 29 + 6) * 70.0f);
        for (int y = top; y < CANVAS_HEIGHT; y++) {
            int widen = (y - top) / 10;
            int jag = (int)std::round(std::fabs(std::sin(y * 0.2f + i)) * 2.0f);
            DrawRectangle((int)x - width / 2 - widen + jag, y, width + widen * 2 - jag, 1, SPIRE);
            DrawRectangle((int)x + width / 2 + widen - 1, y, 1, 1, SPIRE_LIT);
        }
        drawWindPine((int)x, top, view.time + i);
    }
    for (int i = 0; i < 4; i++) { // mist banks drifting between the spires
        float x = std::fmod(i * 230.0f + view.time * 3.0f - view.scroll * 0.5f + 4000.0f, 920.0f) - 140.0f;
        DrawEllipse((int)x, 210 + i * 18, 130.0f, 12.0f, withAlpha(MIST, 0.08f));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Region 4 drawing
// ---------------------------------------------------------------------------

void drawEdgeBackground(const SceneView& view) {
    drawEdgeSky(view);
    drawPeaks(view);
    drawSpires(view);
    DrawRectangleGradientV(0, 250, CANVAS_WIDTH, CANVAS_HEIGHT - 250, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.85f)); // the abyss
    drawNullGap(view);
}

void drawEdgePlatforms(const SceneView& view) {
    // The platform the current node's next points to, checked for NULL before use.
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;

    const Node* node = view.head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            float mist = 0.0f;
            if (view.mistById != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION) {
                mist = view.mistById[node->id];
            }
            EdgeShape shape = shapeFor(*node);
            if (shape == EdgeShape::Bridge) {
                drawBridge(ledge, view.bridgeProgress, view.time);
            } else {
                Silhouette outline = buildEdgeSilhouette(shape, ledge);
                fillSilhouette(outline, CLIFF_ROCK, true);
                drawRockDetails(ledge, outline);
                drawGrassTufts(ledge, GRASS_LIGHT, GRASS, view.time);
                if (shape == EdgeShape::Lookout) drawWindPine(ledge.left + 3, ledge.top, view.time);
                if (shape == EdgeShape::RopePillar) { // an old wooden post with a coil of rope
                    DrawRectangle(ledge.left + 8, ledge.top - 9, 3, 9, WOOD);
                    DrawRectangle(ledge.left + 7, ledge.top - 10, 5, 2, WOOD_LIGHT);
                    DrawRing(Vector2{ledge.left + 16.0f, ledge.top - 2.0f}, 1.0f, 3.0f, 0.0f, 360.0f, 10, ROPE);
                }
                if (shape == EdgeShape::EdgeCliff) {
                    // The anchor of the old bridge. Its snapped ropes are drawn up as the new bridge is made.
                    float reel = view.bridgeBuilt ? std::fmin(1.0f, view.bridgeProgress / 0.35f) : 0.0f;
                    drawAnchorPost(ledge.left + ledge.width - 5, ledge.top, reel, view.time);
                    // While next is NULL, amber light from the gap catches the cliff's open face.
                    float hazard = view.bridgeBuilt ? 1.0f - easeOut(view.bridgeProgress) : 1.0f;
                    for (int dy = 2; hazard > 0.0f && dy < 90 && dy < outline.rows; dy++) {
                        if (outline.count[dy] == 0) continue;
                        int right = outline.spans[dy][0].right;
                        DrawRectangle(right - 3, ledge.top + dy, 3, 1, withAlpha(HAZARD_AMBER, 0.55f * hazard * (1.0f - dy / 90.0f)));
                    }
                }
                drawMistVeil(outline, mist, VEIL);
            }
            if (node == next) {
                // The new bridge only glows once there are planks to glow on.
                float strength = arrivalEase(view);
                if (shape == EdgeShape::Bridge) strength *= smooth01((view.bridgeProgress - 0.25f) / 0.6f);
                drawNextGlow(ledge, view.time, strength);
            }
        }
        node = node->next;
        steps++;
    }

    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        int x = (view.checkpoint->id == 1) ? ledge.left + 14 : ledge.left + 6; // beside the pine on the lookout
        drawLanternPost(x, ledge.top, view.checkpointFlash, view.time);
    }
    if (view.hasGate) {
        drawDawnGate(view.gateRect, view.cameraX, view.gateProgress, (view.bridgeProgress - 0.7f) / 0.3f, view.time);
    }
}

void drawEdgeForeground(const SceneView& view) {
    const int scroll = view.scroll;
    const float time = view.time;

    for (int i = 0; i < 5; i++) { // low mist in front of the cliff bases
        float x = std::fmod(i * 210.0f + time * 6.0f - scroll * 1.1f + 4000.0f, 1000.0f) - 160.0f;
        DrawEllipse((int)x, 296 + (i % 3) * 16, 120.0f + (i % 2) * 40.0f, 12.0f, withAlpha(MIST, 0.12f));
    }
    DrawRectangleGradientV(0, 300, CANVAS_WIDTH, 60, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.7f));

    for (int i = 0; i < 18; i++) { // leaves tumbling down into the abyss on the wind
        float t = std::fmod(time * 0.08f + hash01(i * 5 + 3), 1.0f);
        float x = std::fmod(hash01(i * 11 + 1) * 800.0f + t * 260.0f - scroll * 0.9f + 4000.0f, 800.0f) - 80.0f;
        float y = 40.0f + t * 330.0f + std::sin(time * 2.0f + i) * 6.0f;
        bool flip = std::sin(time * 6.0f + i) > 0.0f; // tumbling
        Color leaf = (i % 2 == 0) ? Color{196, 140, 70, 255} : Color{150, 110, 60, 255};
        DrawRectangle((int)x, (int)y, flip ? 2 : 1, flip ? 1 : 2, withAlpha(leaf, 0.85f * loopEnvelope(t)));
    }
    for (int i = 0; i < 6; i++) { // gusts of wind
        float t = std::fmod(time * 0.5f + i * 0.17f, 1.0f);
        float x = -80.0f + t * 800.0f;
        int y = 80 + (int)(hash01(i * 7 + 2) * 190.0f);
        DrawRectangle((int)x, y, 26 + i * 4, 1, withAlpha(MIST, 0.12f * std::sin(t * 3.14159f)));
    }

    float offset = scroll * 1.3f;
    for (int i = 0; i < 30; i++) { // dark grass whipped by the wind
        float x = i * 34.0f + hash01(i * 3 + 7) * 12.0f - offset;
        if (x < -20.0f || x > CANVAS_WIDTH + 20.0f) continue;
        for (int b = 0; b < 3; b++) {
            float height = 10.0f + hash01(i * 11 + b) * 20.0f;
            float lean = 6.0f + std::sin(time * 2.2f + i + b) * 3.0f; // the wind pushes them right
            drawTriangleSafe(Vector2{x + b * 4.0f - 2.0f, CANVAS_HEIGHT + 2.0f}, Vector2{x + b * 4.0f + 2.0f, CANVAS_HEIGHT + 2.0f},
                             Vector2{x + b * 4.0f + lean, CANVAS_HEIGHT - height}, DARK);
        }
    }
    const float rockLayerX[2] = {10.0f, 990.0f};
    for (int i = 0; i < 2; i++) { // big dark rocks framing the scene
        float x = rockLayerX[i] - offset;
        if (x < -60.0f || x > CANVAS_WIDTH + 60.0f) continue;
        DrawCircle((int)x, CANVAS_HEIGHT + 10, 46.0f, DARK);
        DrawCircle((int)x + 30, CANVAS_HEIGHT + 20, 34.0f, DARK);
    }

    DrawRectangleGradientH(0, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.5f), withAlpha(DARK, 0.0f));
    DrawRectangleGradientH(CANVAS_WIDTH - 70, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.5f));
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 44, withAlpha(DARK, 0.5f), withAlpha(DARK, 0.0f));
}
