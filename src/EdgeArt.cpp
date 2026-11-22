#include "ArtCommon.h"

#include <algorithm>

// Region 4, "The Edge of Nothing": cold cliffs before dawn above a deep ravine. N3's next is
// NULL, so past its edge there is nothing: a snapped bridge, wind, and amber light rising from
// the gap. When append() creates the bridge node, the bridge is laid plank by plank from that
// real node, lanterns light along it, the wind drops, and dawn breaks over the far peaks.

namespace {

const Color SKY_TOP = {10, 14, 28, 255};
const Color SKY_MID = {34, 46, 72, 255};
const Color SKY_LOW = {88, 102, 126, 255};
const Color DAWN_TOP = {44, 46, 88, 255};
const Color DAWN_MID = {156, 106, 126, 255};
const Color DAWN_LOW = {255, 190, 132, 255};
const Color DAWN = {255, 186, 130, 255};
const Color SUN = {255, 226, 170, 255};
const Color MOON = {222, 230, 242, 255};
const Color MIST = {150, 162, 188, 255};
const Color VEIL = {92, 102, 126, 255};
const Color DARK = {6, 8, 14, 255};

const Color FAR_PEAKS[4] = {{40, 50, 74, 255}, {50, 60, 86, 255}, {66, 78, 104, 255}, {150, 160, 182, 255}};
const Color NEAR_PEAKS[4] = {{24, 30, 46, 255}, {32, 40, 58, 255}, {44, 54, 74, 255}, {96, 108, 132, 255}};
const Color SPIRE[4] = {{14, 18, 28, 255}, {22, 28, 42, 255}, {34, 42, 60, 255}, {58, 68, 92, 255}};
const Color ROCK[6] = {{34, 36, 50, 255}, {58, 60, 76, 255}, {92, 94, 108, 255},
                       {132, 132, 144, 255}, {172, 170, 178, 255}, {208, 206, 212, 255}};
const Color GRASS[5] = {{38, 58, 56, 255}, {58, 84, 68, 255}, {104, 136, 88, 255}, {168, 194, 120, 255}, {208, 224, 158, 255}};
const Color WOOD[4] = {{56, 38, 32, 255}, {84, 58, 44, 255}, {126, 90, 62, 255}, {180, 136, 92, 255}};
const Color ROPE = {196, 170, 120, 255};
const Color PINE[4] = {{10, 16, 20, 255}, {18, 28, 32, 255}, {28, 42, 44, 255}, {44, 62, 58, 255}};

const Vector2 TO_MOON = {-0.6f, -0.8f};
const float MOON_X = 150.0f;
const float MOON_Y = 58.0f;

enum Layer { NIGHT_LAYER, DAWN_LAYER, FAR_LAYER, NEAR_LAYER, SPIRE_LAYER, ROUTE_LAYER, LAYER_COUNT };
const float PARALLAX[LAYER_COUNT] = {0.0f, 0.0f, 0.08f, 0.18f, 0.42f, 1.0f};
const float FRONT_PARALLAX = 1.3f;

float easeOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// How far the morning has come: it starts as the bridge is laid and grows over a few seconds.
float dawnAmount(const SceneView& view) {
    return view.bridgeBuilt ? smooth01((view.bridgeTime - 0.6f) / 3.6f) : 0.0f;
}

// The wind is strong while the way is broken, and drops when the bridge is made.
float windStrength(const SceneView& view) {
    return 1.6f - 0.9f * dawnAmount(view);
}

// The gap's glow fades as the bridge is laid over it.
float hazardAmount(const SceneView& view) {
    return view.bridgeBuilt ? 1.0f - easeOut(view.bridgeProgress) : 1.0f;
}

Shading rockShading(int seed, int darkenFrom) {
    Shading s;
    s.ramp = ROCK;
    s.steps = 6;
    s.toLight = TO_MOON;
    s.base = 0.44f;
    s.rim = 0.34f;
    s.bevel = 2.5f;
    s.grain = 0.14f;
    s.grainScale = 0.24f;
    s.seed = seed;
    s.darkenFrom = (float)darkenFrom;
    s.darkenRange = 70.0f;
    s.darkenAmount = 0.4f;
    return s;
}

// ---------------------------------------------------------------------------
// Painting helpers
// ---------------------------------------------------------------------------

void paintGrassTop(Picture& p, const Mask& m, int x0, int x1, int top, int seed) {
    for (int x = x0; x < x1; x++) {
        int y = top;
        while (y < p.height && !m.at(x, y)) y++;
        if (y > top + 5) continue;
        int t = 2 + (int)(noise2(x * 0.35f, 0.0f, seed) * 3.0f);
        for (int k = 0; k <= t && m.at(x, y + k); k++) p.set(x, y + k, pickRamp(GRASS, 5, 0.92f - k * 0.22f, x, y + k));
    }
}

// Cracks and ledges in the rock face: short dark diagonals with a lit edge below them.
void paintStrata(Picture& p, const Mask& m, int count, int seed) {
    for (int i = 0; i < count; i++) {
        int x = (int)(hash01(seed * 13 + i) * p.width);
        int y = (int)(hash01(seed * 17 + i) * p.height);
        int length = 4 + (int)(hash01(seed * 19 + i) * 10.0f);
        for (int k = 0; k < length; k++) {
            int xx = x + k, yy = y + k / 3;
            if (!m.at(xx, yy) || m.depth[yy * p.width + xx] < 2.0f) continue;
            p.set(xx, yy, ROCK[0]);
            if (m.at(xx, yy + 1)) p.set(xx, yy + 1, ROCK[3]);
        }
    }
}

// A wind-bent pine: layers of needles swept to one side.
Picture paintPine(int height, int seed) {
    Picture p(height, height + 2);
    int base = height;
    float cx = height * 0.5f;
    for (int y = 0; y < height; y++) { // the trunk, leaning with the wind
        int x = (int)(cx + (base - y) * 0.12f);
        p.set(x, y, WOOD[0]);
        p.set(x + 1, y, WOOD[1]);
    }
    int layers = 4 + height / 14;
    for (int l = 0; l < layers; l++) {
        float y = height * 0.25f + l * (height * 0.65f / layers);
        float width = 4.0f + (y / height) * height * 0.32f;
        float x = cx + (base - y) * 0.12f;
        Mask needles(p.width, p.height);
        Vector2 tri[3] = {{x - width, y + 3.0f}, {x + width * 0.55f, y + 3.0f}, {x - width * 0.35f, y - 5.0f}};
        needles.polygon(tri, 3);
        needles.measure();
        Shading s;
        s.ramp = PINE;
        s.steps = 4;
        s.toLight = TO_MOON;
        s.base = 0.4f;
        s.rim = 0.5f;
        s.bevel = 1.5f;
        s.grain = 0.15f;
        s.seed = seed + l;
        s.outline = false;
        shade(p, needles, s);
    }
    return p;
}

// ---------------------------------------------------------------------------
// Platforms
// ---------------------------------------------------------------------------

// A rocky body under a walkable top: exact at the top, then rough sides that `left` and `right`
// push in or out as it goes down.
template <typename LeftFn, typename RightFn>
Mask rockBody(const LedgeCanvas& c, LeftFn left, RightFn right) {
    Mask m(c.picture.width, c.picture.height);
    for (int y = c.originY; y < c.picture.height; y++) {
        int dy = y - c.originY;
        if (dy == 0) m.span(y, c.originX, c.originX + c.width);
        else if (dy < 3) m.span(y, c.originX - 1, c.originX + c.width + 1);
        else m.span(y, c.originX + left(dy), c.originX + c.width + right(dy));
    }
    m.measure();
    return m;
}

// N1: a broad lookout shelf, where the path begins.
void paintLookout(LedgeCanvas& c, int seed) {
    Mask m = rockBody(
        c, [&](int dy) { return -std::min(14, dy / 3) + (int)(hash01(seed + dy / 4) * 4.0f); },
        [&](int dy) { return 2 - (int)(6.0f * std::sin(dy * 0.06f)) - (int)(hash01(seed * 3 + dy / 4) * 4.0f); });
    shade(c.picture, m, rockShading(seed, c.originY + 24));
    paintStrata(c.picture, m, 22, seed);
    paintGrassTop(c.picture, m, 0, c.picture.width, c.originY, seed);
}

// N2: a pillar of rock with an old post and a coil of rope.
void paintRopePost(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY;
    Mask m = rockBody(
        c, [&](int dy) { return 3 - std::min(8, dy / 9) + (int)(hash01(seed + dy / 5) * 3.0f); },
        [&](int dy) { return -3 + std::min(10, dy / 8) - (int)(hash01(seed * 5 + dy / 5) * 3.0f); });
    shade(p, m, rockShading(seed, oy + 20));
    paintStrata(p, m, 18, seed);
    paintGrassTop(p, m, 0, p.width, oy, seed);
    p.rect(ox + 7, oy - 11, 3, 11, WOOD[1]); // the post, with a coil of rope at its foot
    p.rect(ox + 7, oy - 11, 1, 11, WOOD[0]);
    p.rect(ox + 6, oy - 12, 5, 2, WOOD[3]);
    for (int k = 0; k < 10; k++) {
        float a = k * 0.63f;
        p.set(ox + 16 + (int)std::lround(std::cos(a) * 3.0f), oy - 2 + (int)std::lround(std::sin(a) * 1.5f), ROPE);
    }
}

// N3: the edge of the cliff. Its far face drops into the ravine where the bridge used to be.
void paintEdgeCliff(LedgeCanvas& c, int seed) {
    Mask m = rockBody(
        c, [&](int dy) { return -std::min(10, dy / 4) + (int)(hash01(seed + dy / 4) * 3.0f); },
        [&](int dy) { // the face toward the gap is jagged and falls back as it goes down
            return 1 - (int)(dy * 0.18f) - (int)(std::fabs(std::sin(dy * 0.31f)) * 3.0f) - (int)(hash01(seed * 7 + dy / 3) * 3.0f);
        });
    shade(c.picture, m, rockShading(seed, c.originY + 22));
    paintStrata(c.picture, m, 22, seed);
    paintGrassTop(c.picture, m, 0, c.picture.width - c.originX + c.width - 4, c.originY, seed);
}

void paintEdgeLedge(const Node& node, LedgeCanvas& c) {
    int seed = node.id * 37 + 11;
    switch (node.id) {
        case 1: paintLookout(c, seed); break;
        case 2: paintRopePost(c, seed); break;
        default: paintEdgeCliff(c, seed); break;
    }
}

// ---------------------------------------------------------------------------
// Scenery
// ---------------------------------------------------------------------------

float ridgeHeight(float x, float frequency, int seed) {
    auto peak = [](float v) { float f = v - std::floor(v); return 1.0f - std::fabs(f * 2.0f - 1.0f); }; // sharp-topped waves
    float h = 0.6f * peak(x * frequency + seed * 0.37f) + 0.28f * peak(x * frequency * 2.3f + 0.6f + seed) + 0.12f * peak(x * frequency * 5.7f + seed * 1.3f);
    return h + (noise2(x * 0.15f, 0.0f, seed) - 0.5f) * 0.06f;
}

void paintRidge(Picture& p, float base, float height, float frequency, int seed, const Color* ramp, bool snow) {
    for (int x = 0; x < p.width; x++) {
        float h = ridgeHeight((float)x, frequency, seed) * height;
        int top = (int)(base - h);
        float slope = ridgeHeight((float)(x - 1), frequency, seed) - ridgeHeight((float)(x + 1), frequency, seed); // > 0 faces the moon (left)
        for (int y = std::max(0, top); y < p.height; y++) {
            int d = y - top;
            Color c = ramp[0];
            if (d < 2) c = ramp[(slope > 0.0f) ? 3 : 2];               // the moonlit side of each peak catches the light
            else if (snow && d < 5 + (int)(h * 0.08f) && h > height * 0.62f) c = ramp[3]; // snow on the high peaks
            else if (d < 14 && slope > 0.0f) c = ramp[1];
            p.set(x, y, c);
        }
    }
}

void bakeEdge(RegionArt& art, const World& world) {
    const RegionInfo& info = world.getRegion(3);
    const int range = scrollRangeOf(info);

    Picture night(CANVAS_WIDTH, CANVAS_HEIGHT);
    ditherGradient(night, 0, 170, SKY_TOP, SKY_MID);
    ditherGradient(night, 170, CANVAS_HEIGHT, SKY_MID, SKY_LOW);
    for (int i = 0; i < 80; i++) {
        int x = (int)(hash01(i * 7 + 9) * CANVAS_WIDTH);
        int y = (int)(hash01(i * 13 + 3) * 150.0f);
        night.set(x, y, mixColor(SKY_MID, MOON, 0.3f + 0.65f * hash01(i * 5)));
    }
    for (int y = -12; y <= 12; y++) { // a thin crescent moon
        for (int x = -12; x <= 12; x++) {
            bool inMoon = x * x + y * y <= 11 * 11;
            bool inShadow = (x - 4) * (x - 4) + (y + 3) * (y + 3) <= 10 * 10;
            if (inMoon && !inShadow) night.set((int)MOON_X + x, (int)MOON_Y + y, MOON);
        }
    }
    storeLayer(art, night, 0.0f, 0);

    Picture dawn(CANVAS_WIDTH, CANVAS_HEIGHT);
    ditherGradient(dawn, 0, 150, DAWN_TOP, DAWN_MID);
    ditherGradient(dawn, 150, CANVAS_HEIGHT, DAWN_MID, DAWN_LOW);
    storeLayer(art, dawn, 0.0f, 0);

    Picture far(layerWidth(range, PARALLAX[FAR_LAYER]), CANVAS_HEIGHT);
    paintRidge(far, 210.0f, 92.0f, 0.009f, 3, FAR_PEAKS, true);
    fogRows(far, MIST, 150, 280, 0.2f, 0.5f);
    storeLayer(art, far, PARALLAX[FAR_LAYER], 0);

    Picture nearPeaks(layerWidth(range, PARALLAX[NEAR_LAYER]), CANVAS_HEIGHT);
    paintRidge(nearPeaks, 236.0f, 56.0f, 0.016f, 11, NEAR_PEAKS, false);
    fogRows(nearPeaks, MIST, 200, 320, 0.1f, 0.45f);
    storeLayer(art, nearPeaks, PARALLAX[NEAR_LAYER], 0);

    // Rock spires rising out of the ravine, with pines bent by the wind on top.
    Picture spires(layerWidth(range, PARALLAX[SPIRE_LAYER]), CANVAS_HEIGHT);
    Mask spireMask(spires.width, CANVAS_HEIGHT);
    std::vector<Vector2> pineSpots;
    for (int i = 0; i * 120 < spires.width + 60; i++) {
        float x = i * 120.0f + hash01(i * 17 + 2) * 50.0f;
        int width = 16 + (int)(hash01(i * 23 + 4) * 14.0f);
        int top = 186 + (int)(hash01(i * 29 + 6) * 64.0f);
        for (int y = top; y < CANVAS_HEIGHT; y++) {
            int widen = (y - top) / 10;
            int jag = (int)std::round(std::fabs(std::sin(y * 0.2f + i)) * 2.0f);
            spireMask.span(y, (int)x - width / 2 - widen + jag, (int)x + width / 2 + widen);
        }
        pineSpots.push_back(Vector2{x, (float)top});
    }
    spireMask.measure();
    Shading spireStyle;
    spireStyle.ramp = SPIRE;
    spireStyle.steps = 4;
    spireStyle.toLight = TO_MOON;
    spireStyle.base = 0.3f;
    spireStyle.rim = 0.45f;
    spireStyle.bevel = 2.0f;
    spireStyle.grain = 0.12f;
    spireStyle.seed = 40;
    spireStyle.dither = false;
    shade(spires, spireMask, spireStyle);
    fogRows(spires, MIST, 180, CANVAS_HEIGHT, 0.2f, 0.5f);
    storeLayer(art, spires, PARALLAX[SPIRE_LAYER], 0);
    for (size_t i = 0; i < pineSpots.size(); i++) {
        Picture pine = paintPine(16 + (int)(hash01((int)i * 3 + 1) * 10.0f), 60 + (int)i * 5);
        fogRows(pine, MIST, 0, pine.height, 0.15f, 0.15f);
        storeSway(art, pine, SPIRE_LAYER, false, pineSpots[i].x - pine.width * 0.5f, pineSpots[i].y - pine.height + 3.0f, 2.0f,
                  false, 4, 0.1f * (float)i);
    }

    // The far side of the ravine: a landing where the bridge will end, standing stones, and the cliff behind.
    Picture route(layerWidth(range, PARALLAX[ROUTE_LAYER]), CANVAS_HEIGHT);
    Rectangle bridge = world.getBridgeRect();
    const int landingLeft = localX(bridge.x + bridge.width, info.left) - 22;
    const int landingTop = toCanvasY(bridge.y);
    Mask cliff(route.width, CANVAS_HEIGHT);
    for (int y = landingTop; y < CANVAS_HEIGHT; y++) { // the landing and the rock below it
        int dy = y - landingTop;
        cliff.span(y, landingLeft + 2 - std::min(16, dy / 5) + (int)(hash01(dy / 4 + 7) * 3.0f), route.width);
    }
    for (int y = 90; y < landingTop; y++) { // the cliff wall rising behind the landing
        int dy = landingTop - y;
        cliff.span(y, landingLeft + 30 + dy / 8 + (int)(hash01(y / 5 + 3) * 4.0f), route.width);
    }
    cliff.measure();
    Shading cliffStyle = rockShading(70, landingTop + 20);
    cliffStyle.ramp = SPIRE; // the far side stays a dark mass, so the gate in it reads first
    cliffStyle.steps = 4;
    cliffStyle.base = 0.42f;
    cliffStyle.rim = 0.5f;
    shade(route, cliff, cliffStyle);
    paintStrata(route, cliff, 60, 71);
    paintGrassTop(route, cliff, landingLeft - 4, landingLeft + 30, landingTop, 72);
    int stoneX = localX(bridge.x + bridge.width - 50.0f, info.left); // the dawn gate stands where the bridge will end
    for (int s = 0; s < 2; s++) {
        int x = (s == 0) ? stoneX - 6 : stoneX + 20;
        route.rect(x, landingTop - 37, 6, 37, ROCK[2]);
        route.rect(x, landingTop - 37, 1, 37, ROCK[4]);
        route.rect(x + 5, landingTop - 37, 1, 37, ROCK[0]);
    }
    route.rect(stoneX - 9, landingTop - 43, 38, 6, ROCK[2]); // the lintel
    route.rect(stoneX - 9, landingTop - 43, 38, 1, ROCK[4]);
    route.rect(stoneX, landingTop - 37, 20, 37, Color{18, 22, 34, 255}); // the dark way through, until dawn
    storeLayer(art, route, PARALLAX[ROUTE_LAYER], 0);

    const Node* head = world.getHead(3);
    const Node* node = head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        LedgeCanvas canvas = makeLedgeCanvas(*node, 30, 16);
        paintEdgeLedge(*node, canvas);
        storeLedge(art, *node, canvas);
        if (node->id == 1) { // a pine leaning out over the lookout
            Picture pine = paintPine(34, 90);
            storeSway(art, pine, ROUTE_LAYER, false, localX(node->platformPosition.x, info.left) - 14.0f,
                      toCanvasY(node->platformPosition.y) - pine.height + 3.0f, 2.4f, false, 5);
        }
        node = node->next;
    }

    // Foreground: dark rocks and grass whipped by the wind.
    Picture front(layerWidth(range, FRONT_PARALLAX), CANVAS_HEIGHT);
    Mask rocks(front.width, CANVAS_HEIGHT);
    rocks.blob(10.0f, CANVAS_HEIGHT + 8.0f, 46.0f, 5, 0.25f);
    rocks.blob(44.0f, CANVAS_HEIGHT + 18.0f, 34.0f, 6, 0.25f);
    rocks.blob((float)front.width - 60.0f, CANVAS_HEIGHT + 10.0f, 50.0f, 7, 0.25f);
    rocks.measure();
    Shading rockStyle;
    rockStyle.ramp = SPIRE;
    rockStyle.steps = 4;
    rockStyle.toLight = TO_MOON;
    rockStyle.base = 0.1f;
    rockStyle.rim = 0.3f;
    rockStyle.bevel = 2.0f;
    rockStyle.seed = 80;
    rockStyle.outline = false;
    shade(front, rocks, rockStyle);
    storeLayer(art, front, FRONT_PARALLAX, 0, true);
}

// ---------------------------------------------------------------------------
// Live pieces: the snapped anchor, the bridge, the dawn gate, the gap
// ---------------------------------------------------------------------------

// An anchor post of the old bridge. Its snapped ropes hang over the gap (`side` +1 to the right,
// -1 to the left) and swing in the wind until the bridge is made; then they are drawn up
// (reel: 0 hanging -> 1 connected). `ropeA` and `ropeB` are how far its two ropes hang.
void drawAnchorPost(int x, int top, int side, float reel, float time, float wind, int ropeA, int ropeB) {
    DrawRectangle(x, top - 10, 3, 10, WOOD[1]);
    DrawRectangle(x, top - 10, 1, 10, WOOD[0]);
    DrawRectangle(x - 1, top - 11, 5, 2, WOOD[3]);
    DrawRectangle(x + 1, top - 7, 1, 2, lerpColor(HAZARD_AMBER, WARM_LIGHT, smooth01(reel))); // a carved rune
    if (reel >= 1.0f) return;
    float hang = 1.0f - smooth01(reel);
    const int from = (side > 0) ? x + 3 : x - 1;
    for (int r = 0; r < 2; r++) {
        int length = (int)std::round(((r == 0) ? ropeA : ropeB) * hang);
        float swing = std::sin(time * 1.3f + r * 1.7f + side) * 3.0f + wind * 3.0f;
        int startY = top - 8 + r * 6;
        for (int k = 0; k < length; k++) {
            float t = k / (float)length;
            DrawRectangle(from + (int)(side * t * 6.0f + swing * t * t), startY + k, 1, 1, ROPE);
        }
        if (length > 2) { // a plank still tied to the end, turning as it swings
            int px = from + (int)(side * 6.0f + swing);
            bool edgeOn = std::sin(time * 1.1f + r * 2.3f + side) > 0.55f;
            int w = edgeOn ? 1 : 3;
            int h = (r == 0) ? 6 : 5;
            DrawRectangle(px - w / 2, startY + length, w, h, withAlpha(WOOD[2], hang));
            DrawRectangle(px - w / 2, startY + length, 1, h, withAlpha(WOOD[0], hang));
        }
    }
}

// The bridge node, drawn from its real position and width. While it is being created the planks
// are laid out from N3's edge one by one, a pier rises from the mist under its middle, and the
// rope rails go up last. Lanterns on its posts light in turn once it is whole.
void drawBridge(const Ledge& l, float progress, float bridgeTime, float time) {
    const int x1 = l.left + l.width;
    const int pierX = l.left + l.width / 2;
    float rise = easeOut((progress - 0.2f) / 0.5f);
    int pierTop = l.top + 5 + (int)((1.0f - rise) * 140.0f);
    for (int y = pierTop; y < CANVAS_HEIGHT; y++) {
        int dy = y - l.top;
        int widen = (dy > 10) ? (dy - 10) / 9 : 0;
        int left = pierX - 10 - widen + (int)std::round(std::fabs(std::sin(dy * 0.33f)) * 2.0f);
        int right = pierX + 10 + widen;
        DrawRectangle(left, y, right - left, 1, (dy % 9 == 0) ? ROCK[1] : ROCK[2]); // courses of stone
        DrawRectangle(left, y, 2, 1, ROCK[4]);
        DrawRectangle(right - 2, y, 2, 1, ROCK[0]);
    }

    const int plankCount = l.width / 5;
    float laid = progress / 0.7f * plankCount; // starts at once at N3's end, so there is always deck to step on
    for (int i = 0; i < plankCount; i++) {
        float appear = std::fmin(1.0f, (laid - i) / 1.5f);
        if (appear <= 0.0f) break;
        int drop = 0;
        if (appear < 0.75f) drop = -(int)std::round((1.0f - smooth01(appear / 0.75f)) * 9.0f);
        else if (appear < 1.0f) drop = 1; // it dips as it lands, then settles
        int x = l.left + i * 5;
        DrawRectangle(x, l.top + drop, 4, 4, WOOD[2]);
        DrawRectangle(x, l.top + drop, 4, 1, WOOD[3]);
        DrawRectangle(x, l.top + drop + 3, 4, 1, WOOD[0]);
        if (appear < 1.0f) DrawRectangle(x + 1, l.top + drop - 2, 2, 1, withAlpha(WARM_LIGHT, 1.0f - appear));
    }

    float rails = smooth01((progress - 0.72f) / 0.28f);
    if (rails <= 0.0f) return;
    const int posts[3] = {l.left + 2, pierX, x1 - 26};
    for (int p = 0; p < 3; p++) {
        DrawRectangle(posts[p], l.top - 10, 2, 10, withAlpha(WOOD[1], rails));
        DrawRectangle(posts[p], l.top - 11, 2, 1, withAlpha(WOOD[3], rails));
    }
    for (int seg = 0; seg < 2; seg++) { // rope rails sagging between the posts, with hangers
        int a = posts[seg], b = posts[seg + 1];
        for (int x = a; x <= b; x++) {
            float t = (x - a) / (float)(b - a);
            int y = l.top - 9 + (int)(4.0f * 4.0f * t * (1.0f - t));
            DrawRectangle(x, y, 1, 1, withAlpha(ROPE, rails));
            if ((x - a) % 10 == 5) DrawRectangle(x, y + 1, 1, l.top - y - 1, withAlpha(ROPE, 0.55f * rails));
        }
    }
    for (int p = 0; p < 3; p++) { // a lantern on each post, lit one after another
        float lit = smooth01((bridgeTime - 1.7f - p * 0.35f) / 0.4f);
        int lx = posts[p] - 1, ly = l.top - 17;
        DrawRectangle(lx, ly, 4, 5, withAlpha(WOOD[0], rails));
        DrawRectangle(lx + 1, ly + 1, 2, 3, withAlpha(lerpColor(WOOD[1], WARM_LIGHT, lit), rails));
        if (lit > 0.0f && lit < 1.0f) {
            for (int k = 0; k < 6; k++) {
                float angle = k * 1.047f;
                float d = lit * 8.0f;
                DrawRectangle(lx + 1 + (int)(std::cos(angle) * d), ly + 2 + (int)(std::sin(angle) * d), 1, 1, withAlpha(WARM_LIGHT, 1.0f - lit));
            }
        }
    }
    (void)time;
}

// Dawn light fills the standing stones once the region is complete (open: 0 -> 1).
void drawDawnGate(Rectangle gate, int cameraX, float open, float time) {
    if (open <= 0.0f) return;
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    float light = smooth01(open);
    DrawRectangleGradientV(gx, gy, gw, gh, withAlpha(Color{255, 238, 196, 255}, 0.92f * light), withAlpha(DAWN, 0.65f * light));
    for (int i = 0; i < 6; i++) {
        float t = std::fmod(time * (0.35f + 0.1f * hash01(i * 5)) + i * 0.17f, 1.0f);
        DrawRectangle(gx + 2 + (int)(hash01(i * 7 + 2) * (gw - 4)), gy + gh - 2 - (int)(t * gh), 1, 1, withAlpha(WHITE, loopEnvelope(t) * light));
    }
}

// The NULL gap: no platform, just amber light rising from far below and embers falling into it.
void drawNullGap(const SceneView& view) {
    float hazard = hazardAmount(view);
    if (hazard <= 0.01f || view.hazardZone.width <= 0.0f) return;
    int gx = toCanvasX(view.hazardZone.x, view.cameraX);
    int gw = (int)(view.hazardZone.width / CANVAS_SCALE);
    for (int i = 0; i < 18; i++) { // embers falling away into the dark
        float t = std::fmod(view.time * (0.22f + 0.1f * hash01(i * 3 + 1)) + i * 0.055f, 1.0f);
        int x = gx + (int)(hash01(i * 13 + 5) * gw + std::sin(view.time + i) * 4.0f);
        int y = 220 + (int)(t * 150.0f);
        DrawRectangle(x, y, 1, 2, withAlpha(HAZARD_AMBER, hazard * loopEnvelope(t)));
    }
}

float ledgeMist(const SceneView& view, const Node& node) {
    if (view.mistById == nullptr || node.id < 0 || node.id > MAX_NODES_PER_REGION) return 0.0f;
    return view.mistById[node.id];
}

// ---------------------------------------------------------------------------
// Region 4 drawing
// ---------------------------------------------------------------------------

void drawEdgeBackground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    const float dawn = dawnAmount(view);
    const float wind = windStrength(view);
    drawBackLayers(view, art, NIGHT_LAYER, DAWN_LAYER, wind);
    for (int i = 0; i < 30; i++) { // stars that twinkle, fading as the sky warms
        int x = (int)(hash01(i * 17 + 3) * CANVAS_WIDTH);
        int y = 4 + (int)(hash01(i * 19 + 5) * 140.0f);
        float twinkle = 0.5f + 0.5f * std::sin(time * (1.3f + hash01(i)) + i * 2.2f);
        DrawRectangle(x, y, 1, 1, withAlpha(MOON, (0.25f + 0.6f * twinkle) * (1.0f - dawn)));
    }
    if (dawn > 0.0f) DrawTexture(art.back[DAWN_LAYER].texture, 0, 0, withAlpha(WHITE, dawn));
    if (dawn > 0.0f) { // the sun rising behind the far peaks
        float sx = 405.0f - view.scroll * 0.04f;
        float sy = 232.0f - 52.0f * dawn;
        DrawCircle((int)sx, (int)sy, 16.0f, withAlpha(SUN, dawn));
    }
    for (int i = 0; i < 6; i++) { // clouds blown across the sky
        float x = std::fmod(i * 160.0f + time * (6.0f + 2.0f * i) * wind - view.scroll * 0.05f + 4000.0f, 960.0f) - 160.0f;
        int y = 40 + (int)(hash01(i * 9 + 1) * 90.0f);
        Color cloud = lerpColor(Color{40, 50, 74, 255}, Color{240, 170, 150, 255}, dawn);
        for (int k = 0; k < 4; k++) {
            DrawEllipse((int)x + k * 22, y + (k % 2) * 3, 26.0f - k * 3.0f, 6.0f, withAlpha(cloud, 0.32f));
        }
        DrawEllipse((int)x + 30, y - 3, 30.0f, 3.0f, withAlpha(lerpColor(Color{70, 82, 108, 255}, Color{255, 214, 170, 255}, dawn), 0.25f));
    }
    drawBackLayers(view, art, FAR_LAYER, SPIRE_LAYER, wind);
    for (int i = 0; i < 4; i++) { // mist banks drifting between the spires
        float x = std::fmod(i * 230.0f + time * 3.0f * wind - view.scroll * 0.3f + 4000.0f, 1000.0f) - 180.0f;
        DrawEllipse((int)x, 214 + i * 18, 130.0f, 12.0f, withAlpha(lerpColor(MIST, DAWN, dawn * 0.6f), 0.08f));
    }
    drawBackLayers(view, art, SPIRE_LAYER, ROUTE_LAYER, wind);
    DrawRectangleGradientV(0, 250, CANVAS_WIDTH, CANVAS_HEIGHT - 250, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.85f)); // the abyss
    drawNullGap(view);
    drawBackLayers(view, art, ROUTE_LAYER, LAYER_COUNT, wind);
}

void drawEdgePlatforms(const SceneView& view, const RegionArt& art) {
    const float wind = windStrength(view);
    drawLedges(view, art, VEIL);

    const Node* next = (view.current != nullptr) ? view.current->next : nullptr; // checked for NULL before use
    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            float mist = ledgeMist(view, *node);
            if (node->id == 4) {
                drawBridge(ledge, view.bridgeProgress, view.bridgeTime, view.time);
            } else {
                drawGrass(ledge, view, lerpColor(GRASS[4], VEIL, mist * 0.7f), lerpColor(GRASS[2], VEIL, mist * 0.7f), wind, 0.4f);
            }
            if (node->id == 3) {
                float reel = view.bridgeBuilt ? std::fmin(1.0f, view.bridgeProgress / 0.35f) : 0.0f;
                drawAnchorPost(ledge.left + ledge.width - 5, ledge.top, 1, reel, view.time, windAt((float)(ledge.left + view.cameraX), view.time, wind), 40, 26);
                float hazard = hazardAmount(view); // amber light from the gap on the cliff's open face
                for (int dy = 2; hazard > 0.0f && dy < 70; dy++) {
                    int right = ledge.left + ledge.width + 1 - (int)(dy * 0.18f) - (int)(std::fabs(std::sin(dy * 0.31f)) * 3.0f);
                    DrawRectangle(right - 3, ledge.top + dy, 3, 1, withAlpha(HAZARD_AMBER, 0.5f * hazard * (1.0f - dy / 70.0f)));
                }
            }
            if (node == next) {
                float strength = arrivalEase(view); // the new bridge only glows once there are planks to glow on
                if (node->id == 4) strength *= smooth01((view.bridgeProgress - 0.25f) / 0.6f);
                drawLinkLip(ledge, WARM_LIGHT, strength, view.time, true);
            }
        }
        node = node->next;
    }

    if (view.hazardZone.width > 0.0f) { // the other half of the snapped bridge, on the far landing
        int farX = toCanvasX(view.hazardZone.x + view.hazardZone.width, view.cameraX) - 20;
        int farTop = toCanvasY(view.hazardZone.y - 10.0f);
        float reel = view.bridgeBuilt ? std::fmin(1.0f, view.bridgeProgress / 0.35f) : 0.0f;
        if (farX > -20 && farX < CANVAS_WIDTH + 20) {
            drawAnchorPost(farX, farTop, -1, reel, view.time, windAt((float)(farX + view.cameraX), view.time, wind), 52, 34);
        }
    }
    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        int x = (view.checkpoint->id == 1) ? ledge.left + 22 : ledge.left + 6; // beside the pine on the lookout
        drawLanternPost(x, ledge.top, view.checkpointFlash, view.time);
    }
    if (view.hasGate) drawDawnGate(view.gateRect, view.cameraX, view.gateProgress, view.time);

    float dawn = dawnAmount(view);
    if (dawn > 0.0f) { // birds leaving the far cliff as the light comes
        float t = view.bridgeTime - 2.4f;
        for (int i = 0; t > 0.0f && i < 7; i++) {
            float bt = t - i * 0.18f;
            if (bt <= 0.0f || bt > 6.0f) continue;
            float x = 620.0f - view.scroll + 30.0f * hash01(i * 3) - bt * (34.0f + 8.0f * i);
            float y = 190.0f + 20.0f * hash01(i * 5) - bt * (16.0f + 3.0f * i) + std::sin(bt * 3.0f + i) * 3.0f;
            bool wingsUp = std::sin(view.time * 14.0f + i * 1.3f) > 0.0f;
            Color bird = withAlpha(DARK, std::fmin(1.0f, (6.0f - bt) / 1.5f));
            DrawRectangle((int)x, (int)y, 1, 1, bird);
            DrawRectangle((int)x - 2, (int)y + (wingsUp ? -1 : 1), 2, 1, bird);
            DrawRectangle((int)x + 1, (int)y + (wingsUp ? -1 : 1), 2, 1, bird);
        }
    }
}

void drawEdgeForeground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    const float wind = windStrength(view);
    for (int i = 0; i < 5; i++) { // low mist rolling in front of the cliff bases
        float x = std::fmod(i * 210.0f + time * 6.0f * wind - view.scroll * 1.1f + 4000.0f, 1040.0f) - 200.0f;
        DrawEllipse((int)x, 296 + (i % 3) * 16, 120.0f + (i % 2) * 40.0f, 12.0f, withAlpha(MIST, 0.12f));
    }
    DrawRectangleGradientV(0, 300, CANVAS_WIDTH, 60, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.7f));
    for (int i = 0; i < 18; i++) { // leaves tumbling down into the ravine on the wind
        float t = std::fmod(time * 0.08f + hash01(i * 5 + 3), 1.0f);
        float x = std::fmod(hash01(i * 11 + 1) * 800.0f + t * 260.0f * wind - view.scroll * 0.9f + 4000.0f, 800.0f) - 80.0f;
        float y = 40.0f + t * 330.0f + std::sin(time * 2.0f + i) * 6.0f;
        bool flip = std::sin(time * 6.0f + i) > 0.0f;
        Color leaf = (i % 2 == 0) ? Color{196, 140, 70, 255} : Color{150, 110, 60, 255};
        DrawRectangle((int)x, (int)y, flip ? 2 : 1, flip ? 1 : 2, withAlpha(leaf, 0.85f * loopEnvelope(t)));
    }
    for (int i = 0; i < 7; i++) { // streaks of wind
        float t = std::fmod(time * 0.5f * wind + i * 0.17f, 1.0f);
        float x = -80.0f + t * 800.0f;
        int y = 70 + (int)(hash01(i * 7 + 2) * 200.0f);
        DrawRectangle((int)x, y, 26 + i * 4, 1, withAlpha(MIST, 0.14f * std::sin(t * 3.14159f) * std::fmin(1.0f, wind)));
    }
    drawFrontLayers(view, art, wind);
    float offset = view.scroll * FRONT_PARALLAX;
    for (int i = 0; i < 30; i++) { // dark grass whipped by the wind
        float x = i * 34.0f + hash01(i * 3 + 7) * 12.0f - offset;
        if (x < -20.0f || x > CANVAS_WIDTH + 20.0f) continue;
        for (int b = 0; b < 3; b++) {
            float height = 10.0f + hash01(i * 11 + b) * 18.0f;
            float lean = 3.0f + windAt(x, time, wind) * 4.0f + std::sin(time * 2.2f + i + b) * 1.5f;
            drawTriangleSafe(Vector2{x + b * 4.0f - 2.0f, CANVAS_HEIGHT + 2.0f}, Vector2{x + b * 4.0f + 2.0f, CANVAS_HEIGHT + 2.0f},
                             Vector2{x + b * 4.0f + lean, CANVAS_HEIGHT - height}, DARK);
        }
    }
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 44, withAlpha(DARK, 0.5f), withAlpha(DARK, 0.0f));
}

void drawEdgeAdditive(const SceneView& view, const RegionArt&, const SceneRenderer& renderer) {
    const float dawn = dawnAmount(view);
    const float hazard = hazardAmount(view);
    renderer.drawSoftLight(Vector2{MOON_X, MOON_Y}, 70.0f, MOON, 0.14f * (1.0f - dawn));
    if (hazard > 0.01f && view.hazardZone.width > 0.0f) { // a column of amber light rising out of the gap
        float gx = (float)toCanvasX(view.hazardZone.x + view.hazardZone.width * 0.5f, view.cameraX);
        float pulse = 0.8f + 0.2f * std::sin(view.time * 2.4f);
        renderer.drawSoftLight(Vector2{gx, CANVAS_HEIGHT + 20.0f}, 190.0f, HAZARD_AMBER, 0.5f * hazard * pulse);
        float half = view.hazardZone.width / CANVAS_SCALE * 0.5f;
        for (int k = 0; k < 6; k++) { // soft swells of light climbing out of the depth, drifting on the wind
            float rise = std::fmod(view.time * 0.12f + k / 6.0f, 1.0f);
            float y = CANVAS_HEIGHT + 30.0f - rise * 170.0f;
            float x = gx + std::sin(view.time * 0.5f + k * 1.9f) * half * 0.35f + rise * 18.0f;
            renderer.drawSoftLight(Vector2{x, y}, half * (0.9f - rise * 0.45f), HAZARD_AMBER, 0.16f * hazard * pulse * loopEnvelope(rise));
        }
    }
    if (dawn > 0.0f) { // dawn light from behind the peaks, and rays across the ravine
        float sx = 405.0f - view.scroll * 0.04f;
        float sy = 232.0f - 52.0f * dawn;
        renderer.drawSoftLight(Vector2{sx, sy}, 280.0f, DAWN, 0.24f * dawn);
        renderer.drawSoftLight(Vector2{sx, sy}, 46.0f, SUN, 0.32f * dawn);
        for (int i = 0; i < 6; i++) {
            float angle = 3.55f + i * 0.42f + std::sin(view.time * 0.2f + i) * 0.02f;
            Vector2 far = {sx + std::cos(angle) * 700.0f, sy + std::sin(angle) * 700.0f};
            Vector2 farB = {sx + std::cos(angle + 0.05f) * 700.0f, sy + std::sin(angle + 0.05f) * 700.0f};
            drawGradientQuad(Vector2{sx, sy}, Vector2{sx, sy}, farB, far, withAlpha(SUN, 0.08f * dawn), withAlpha(SUN, 0.08f * dawn),
                             withAlpha(SUN, 0.0f), withAlpha(SUN, 0.0f));
        }
    }
}

void addEdgeLights(const SceneView& view, const RegionArt&, std::vector<Light>& lights) {
    const float time = view.time;
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;
    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            if (node == next) {
                float strength = arrivalEase(view);
                if (node->id == 4) strength *= smooth01((view.bridgeProgress - 0.25f) / 0.6f);
                addLinkLight(lights, ledge, WARM_LIGHT, strength);
            }
            if (node->id == 4) { // the lanterns along the bridge
                const int posts[3] = {ledge.left + 2, ledge.left + ledge.width / 2, ledge.left + ledge.width - 26};
                for (int p = 0; p < 3; p++) {
                    float lit = smooth01((view.bridgeTime - 1.7f - p * 0.35f) / 0.4f);
                    if (lit > 0.0f) lights.push_back(Light{Vector2{posts[p] + 1.0f, ledge.top - 14.0f}, 52.0f, WARM_LIGHT, 0.5f * lit, 7.0f, 0.5f * lit});
                }
            }
        }
        node = node->next;
    }
    float hazard = hazardAmount(view);
    if (hazard > 0.01f && view.hazardZone.width > 0.0f) {
        float gx = (float)toCanvasX(view.hazardZone.x + view.hazardZone.width * 0.5f, view.cameraX);
        lights.push_back(Light{Vector2{gx, 330.0f}, 200.0f, HAZARD_AMBER, 0.55f * hazard * (0.85f + 0.15f * std::sin(time * 2.4f)), 0.0f, 0.0f});
    }
    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        int x = (view.checkpoint->id == 1) ? ledge.left + 22 : ledge.left + 6;
        addLanternLight(lights, x, ledge.top, view.checkpointFlash, time);
    }
    if (view.hasGate && view.gateProgress > 0.0f) {
        float light = smooth01(view.gateProgress);
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height / 2.0f);
        lights.push_back(Light{Vector2{x, y}, 90.0f * light, DAWN, 0.6f * light, 26.0f * light, 0.5f * light});
    }
}

Mood edgeMood(const SceneView& view) {
    float dawn = dawnAmount(view);
    Mood mood;
    mood.skyLight = WHITE;
    mood.groundLight = lerpColor(Color{118, 126, 156, 255}, Color{226, 196, 182, 255}, dawn);
    mood.lightFrom = 90;
    mood.lightTo = 300;
    mood.shadowTint = Vector3{0.88f + 0.12f * dawn, 0.94f - 0.02f * dawn, 1.12f - 0.18f * dawn};
    mood.highlightTint = Vector3{1.0f + 0.1f * dawn, 0.98f + 0.04f * dawn, 1.0f - 0.1f * dawn};
    mood.saturation = 0.94f + 0.16f * dawn;
    mood.bloom = 0.65f + 0.2f * dawn;
    mood.vignette = 0.45f - 0.08f * dawn;
    return mood;
}

} // namespace

const RegionPainter EDGE_PAINTER = {bakeEdge, drawEdgeBackground, drawEdgePlatforms, nullptr,
                                    drawEdgeForeground, drawEdgeAdditive, addEdgeLights, edgeMood};
