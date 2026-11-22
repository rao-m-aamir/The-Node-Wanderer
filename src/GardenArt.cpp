#include "ArtCommon.h"

#include <algorithm>

// Region 3, "The Endless Garden": a luminous night garden around an ancient tree and a glowing
// spring. The path climbs over the front of the tree; a current of light flows back under its
// roots from the tail's hollow to the head, so the whole circle can be seen at once.

namespace {

const Color SKY_TOP = {14, 9, 34, 255};
const Color SKY_MID = {38, 26, 76, 255};
const Color SKY_LOW = {94, 70, 134, 255};
const Color MOON = {238, 230, 255, 255};
const Color MIST = {168, 148, 218, 255};
const Color VEIL = {108, 94, 150, 255};
const Color DARK = {8, 6, 18, 255};
const Color SPRING = {120, 255, 230, 255};
const Color BLOSSOM = {255, 150, 210, 255};
const Color PETAL = {255, 214, 236, 255};
const Color WISTERIA = {176, 130, 240, 255};

const Color HILLS[4] = {{50, 38, 90, 255}, {60, 46, 102, 255}, {74, 58, 116, 255}, {96, 78, 136, 255}};
const Color HEDGE[4] = {{26, 18, 50, 255}, {36, 26, 66, 255}, {50, 38, 84, 255}, {70, 56, 106, 255}};
const Color TREE_BARK[6] = {{20, 12, 30, 255}, {36, 24, 50, 255}, {56, 38, 72, 255},
                            {82, 60, 96, 255}, {118, 92, 128, 255}, {158, 130, 162, 255}};
const Color TREE_LEAF[5] = {{18, 10, 38, 255}, {30, 20, 60, 255}, {48, 32, 88, 255}, {74, 52, 124, 255}, {108, 80, 158, 255}};
const Color STONE[6] = {{40, 34, 62, 255}, {62, 54, 90, 255}, {94, 86, 124, 255},
                        {134, 126, 162, 255}, {172, 166, 198, 255}, {210, 204, 228, 255}};
const Color MOSS[5] = {{22, 56, 60, 255}, {36, 94, 92, 255}, {70, 156, 132, 255}, {126, 214, 176, 255}, {176, 244, 210, 255}};
const Color CAP[5] = {{28, 52, 92, 255}, {46, 88, 132, 255}, {82, 156, 188, 255}, {140, 218, 226, 255}, {196, 248, 240, 255}};
const Color STALK[4] = {{96, 88, 128, 255}, {146, 138, 178, 255}, {196, 190, 218, 255}, {228, 224, 240, 255}};
const Color FERN[3] = {{8, 6, 18, 255}, {14, 10, 28, 255}, {24, 18, 44, 255}};

const Vector2 TO_MOON = {-0.6f, -0.8f};
const float TREE_X = 378.0f; // the ancient tree's trunk, behind the flower bed
const float MOON_X = 92.0f; // in open sky, below the canopy
const float MOON_Y = 108.0f;
const int SPRING_Y = 330;    // the spring's surface, under the tree's root arch

enum Layer { SKY_LAYER, HILLS_LAYER, GARDEN_LAYER, TREE_LAYER, LAYER_COUNT };

Shading stoneShading(int seed, int darkenFrom) {
    Shading s;
    s.ramp = STONE;
    s.steps = 6;
    s.toLight = TO_MOON;
    s.base = 0.48f;
    s.rim = 0.3f;
    s.bevel = 2.5f;
    s.grain = 0.1f;
    s.grainScale = 0.3f;
    s.seed = seed;
    s.darkenFrom = (float)darkenFrom;
    s.darkenRange = 70.0f;
    s.darkenAmount = 0.35f;
    return s;
}

Shading barkShading(int seed, int darkenFrom) {
    Shading s;
    s.ramp = TREE_BARK;
    s.steps = 6;
    s.toLight = TO_MOON;
    s.base = 0.4f;
    s.rim = 0.36f;
    s.bevel = 3.0f;
    s.grain = 0.1f;
    s.grainScale = 0.25f;
    s.seed = seed;
    s.darkenFrom = (float)darkenFrom;
    s.darkenRange = 80.0f;
    s.darkenAmount = 0.3f;
    return s;
}

// ---------------------------------------------------------------------------
// The current: the visible form of tail->next
// ---------------------------------------------------------------------------

// A path through a few points, smoothed (Catmull-Rom).
struct Flow {
    Vector2 points[8];
    int count = 0;
};

Vector2 flowPoint(const Flow& f, float t) {
    t = std::clamp(t, 0.0f, 1.0f) * (f.count - 1);
    int i = std::min((int)t, f.count - 2);
    float u = t - i;
    Vector2 p0 = f.points[std::max(0, i - 1)], p1 = f.points[i], p2 = f.points[i + 1], p3 = f.points[std::min(f.count - 1, i + 2)];
    float u2 = u * u, u3 = u2 * u;
    auto blend = [&](float a, float b, float c, float d) {
        return 0.5f * (2.0f * b + (-a + c) * u + (2.0f * a - 5.0f * b + 4.0f * c - d) * u2 + (-a + 3.0f * b - 3.0f * c + d) * u3);
    };
    return Vector2{blend(p0.x, p1.x, p2.x, p3.x), blend(p0.y, p1.y, p2.y, p3.y)};
}

// The current runs from the hollow on the tail to the node the tail's next points to. Its ends
// come from the real nodes, found by walking the list; its middle bends under the tree's roots.
bool returnFlow(const SceneView& view, Flow& flow) {
    const Node* head = view.head;
    const Node* tail = head;
    int steps = 0;
    while (tail != nullptr && tail->next != nullptr && tail->next != head && steps < MAX_NODES_PER_REGION) {
        tail = tail->next;
        steps++;
    }
    if (tail == nullptr || tail->next == nullptr) return false; // not a circle: no way back to draw
    const Node* target = tail->next;                            // the real link (it is the head)
    Vector2 start = {(float)toCanvasX(tail->platformPosition.x + tail->platformWidth - 18.0f, view.cameraX),
                     (float)toCanvasY(tail->platformPosition.y) - 13.0f};
    Vector2 end = {(float)toCanvasX(target->platformPosition.x + target->platformWidth - 44.0f, view.cameraX),
                   (float)toCanvasY(target->platformPosition.y) + 13.0f}; // the basin in the head's wall
    float middle = (start.x + end.x) * 0.5f;
    flow.count = 8;
    flow.points[0] = start;
    flow.points[1] = Vector2{start.x + 16.0f, start.y + 24.0f};
    flow.points[2] = Vector2{start.x + 8.0f, (float)SPRING_Y - 14.0f};
    flow.points[3] = Vector2{middle + 90.0f, (float)SPRING_Y + 4.0f};
    flow.points[4] = Vector2{middle - 40.0f, (float)SPRING_Y + 8.0f};
    flow.points[5] = Vector2{end.x + 60.0f, (float)SPRING_Y - 2.0f};
    flow.points[6] = Vector2{end.x + 24.0f, end.y + 16.0f};
    flow.points[7] = end;
    return true;
}

// ---------------------------------------------------------------------------
// Platforms
// ---------------------------------------------------------------------------

void paintBlossoms(Picture& p, int x0, int x1, int top, int count, int seed) {
    const Color colors[3] = {BLOSSOM, PETAL, WISTERIA};
    for (int i = 0; i < count; i++) {
        int x = x0 + 2 + (int)(hash01(seed * 31 + i) * (x1 - x0 - 4));
        p.set(x, top - 1, MOSS[1]);
        p.set(x, top - 2, colors[i % 3]);
        if (i % 2 == 0) p.set(x + 1, top - 2, scaleColor(colors[i % 3], 0.8f));
    }
}

void paintMossTop(Picture& p, const Mask& m, int x0, int x1, int top, int seed) {
    for (int x = x0; x < x1; x++) {
        int y = top;
        while (y < p.height && !m.at(x, y)) y++;
        if (y > top + 5) continue;
        int t = 2 + (int)(noise2(x * 0.3f, 0.0f, seed) * 3.0f);
        for (int k = 0; k <= t && m.at(x, y + k); k++) {
            p.set(x, y + k, pickRamp(MOSS, 5, 0.95f - k * 0.2f, x, y + k));
        }
        if (hash01(seed * 17 + x) < 0.12f) {
            for (int k = t + 1; k < t + 3 + (int)(hash01(seed + x) * 5.0f); k++) if (m.at(x, y + k)) p.set(x, y + k, MOSS[1]);
        }
    }
}

void paintMasonry(Picture& p, const Mask& m, int fromY, int course, int block, int seed) {
    for (int y = fromY, row = 0; y < p.height; y += course, row++) {
        for (int x = 0; x < p.width; x++) if (m.at(x, y) && m.depth[y * p.width + x] > 1.0f) p.set(x, y, STONE[1]);
        for (int x = (row % 2) * (block / 2) + (int)(hash01(seed + row) * 3.0f); x < p.width; x += block) {
            for (int k = 1; k < course; k++) if (m.at(x, y + k) && m.depth[(y + k) * p.width + x] > 1.0f) p.set(x, y + k, STONE[1]);
        }
    }
}

// N1: the garden gate's terrace, with a basin where the current arrives.
void paintGardenGate(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        int spread = (dy > 34) ? (dy - 34) / 6 : 0; // the wall steps out at its foot
        if (dy == 0) m.span(y, ox, ox + w);
        else if (dy < 4) m.span(y, ox - 1, ox + w + 1);
        else m.span(y, ox + 1 - spread, ox + w - 1 + spread);
    }
    m.measure();
    shade(p, m, stoneShading(seed, oy + 30));
    paintMasonry(p, m, oy + 6, 8, 16, seed);
    int bx = ox + w - 22, by = oy + 12; // the basin
    for (int x = -8; x <= 8; x++) {
        for (int y = 0; y < 6; y++) {
            if (y > 3 && std::abs(x) > 8 - (y - 3) * 2) continue;
            p.set(bx + x, by + y, (y == 0) ? STONE[5] : STONE[0]);
        }
    }
    for (int i = 0; i < 5; i++) { // ivy over the wall
        int x = ox + 6 + (int)(hash01(seed * 7 + i) * (w - 12));
        for (int k = 0; k < 10 + i * 4; k++) {
            int wx = x + (k / 6) % 2;
            p.set(wx, oy + 3 + k, MOSS[1]);
            if (k % 3 == 1) p.set(wx + 1, oy + 3 + k, MOSS[2]);
        }
    }
    paintMossTop(p, m, 0, p.width, oy, seed);
    paintBlossoms(p, ox + 26, ox + w, oy, 6, seed);
}

// N2: a giant moon-cap mushroom with a pale stalk.
void paintMoonCap(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const int cx = ox + w / 2;
    Mask stalk(p.width, p.height);
    for (int y = oy + 8; y < p.height; y++) {
        int dy = y - oy;
        int half = 6 + ((dy > 46) ? (dy - 46) / 7 : 0) + ((dy >= 22 && dy <= 24) ? 3 : 0); // flared base and a ring
        stalk.span(y, cx - half, cx + half);
    }
    stalk.measure();
    Shading s;
    s.ramp = STALK;
    s.steps = 4;
    s.toLight = TO_MOON;
    s.base = 0.55f;
    s.rim = 0.4f;
    s.bevel = 4.0f;
    s.grain = 0.06f;
    s.seed = seed;
    s.darkenFrom = (float)(oy + 30);
    s.darkenRange = 80.0f;
    s.darkenAmount = 0.4f;
    shade(p, stalk, s);

    Mask cap(p.width, p.height);
    for (int y = oy; y < oy + 11; y++) {
        int dy = y - oy;
        if (dy == 0) cap.span(y, ox, ox + w);
        else if (dy < 4) cap.span(y, ox - 2, ox + w + 2);
        else if (dy < 7) cap.span(y, ox - 1 + (dy - 4), ox + w + 1 - (dy - 4));
        else cap.span(y, ox + 4 + (dy - 7) * 4, ox + w - 4 - (dy - 7) * 4); // the gills, narrowing toward the stalk
    }
    cap.measure();
    for (int y = oy; y < oy + 11; y++) {
        for (int x = 0; x < p.width; x++) {
            if (!cap.at(x, y)) continue;
            int dy = y - oy;
            float v = (dy == 0) ? 0.95f : 0.78f - dy * 0.07f;
            if (dy >= 7) v = ((x - cx) % 3 == 0) ? 0.05f : 0.25f; // gills
            p.set(x, y, pickRamp(CAP, 5, v + (noise2(x * 0.4f, y * 0.4f, seed) - 0.5f) * 0.15f, x, y));
        }
    }
    for (int i = 0; i < 7; i++) { // pale spots on the cap
        int x = ox + 4 + (int)(hash01(seed * 13 + i) * (w - 8));
        int y = oy + 2 + (i % 3);
        p.set(x, y, CAP[4]);
        p.set(x + 1, y, CAP[4]);
    }
}

// N3: a stone flower bed lodged against the ancient tree, held up by its roots.
void paintFlowerBed(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask roots(p.width, p.height);
    roots.capsule({ox + w * 0.3f, oy + 16.0f}, {ox + 6.0f, oy + 60.0f}, 7.0f, 3.0f);
    roots.capsule({ox + w * 0.65f, oy + 16.0f}, {ox + w - 4.0f, oy + 72.0f}, 8.0f, 3.0f);
    roots.capsule({ox + w * 0.5f, oy + 16.0f}, {ox + w * 0.48f, (float)p.height}, 11.0f, 15.0f);
    roots.measure();
    shade(p, roots, barkShading(seed + 1, oy + 30));
    fogRows(p, DARK, oy + 40, p.height, 0.0f, 0.55f);

    Mask bed(p.width, p.height);
    for (int y = oy; y < oy + 19; y++) {
        int dy = y - oy;
        if (dy == 0) bed.span(y, ox, ox + w);
        else if (dy < 3) bed.span(y, ox - 1, ox + w + 1);   // the rim
        else if (dy < 15) bed.span(y, ox + 1, ox + w - 1);  // the planter
        else bed.span(y, ox - 1 + (dy - 15), ox + w + 1 - (dy - 15)); // its molding
    }
    bed.measure();
    shade(p, bed, stoneShading(seed, oy + 40));
    for (int x = ox - 1; x < ox + w + 1; x++) p.set(x, oy + 1, STONE[5]);
    for (int x = ox + 5; x < ox + w - 5; x += 7) { // a carved band
        p.rect(x, oy + 8, 3, 2, STONE[4]);
        p.set(x + 1, oy + 9, STONE[1]);
    }
    for (int i = 0; i < 3; i++) { // vines trailing over the rim
        int x = ox + 6 + i * (w - 12) / 2;
        for (int k = 0; k < 6 + i * 4; k++) p.set(x + (k / 4) % 2, oy + 3 + k, MOSS[1 + (k % 2)]);
    }
    paintBlossoms(p, ox, ox + 26, oy, 6, seed);
    paintBlossoms(p, ox + w - 26, ox + w, oy, 6, seed + 3);
}

// N4: a gnarled root knot at the tail of the loop; its hollow stands at the far end.
void paintRootKnot(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        if (dy == 0) { m.span(y, ox, ox + w); continue; }
        if (dy < 4) { m.span(y, ox - 1, ox + w + 1); continue; }
        int bulge = (int)std::lround(3.0f * std::sin(std::min(dy, 22) / 22.0f * 3.14159f));
        int left = ox - bulge, right = ox + w + bulge;
        if (dy > 22) { // the knot narrows into a neck, then spreads into roots near the ground
            float neck = std::fmin(1.0f, (dy - 22) / 24.0f);
            float spread = std::fmax(0.0f, (dy - 64) / 30.0f);
            left = ox + (int)(16.0f * neck - 22.0f * spread);
            right = ox + w - (int)(10.0f * neck - 26.0f * spread);
        }
        m.span(y, left + (int)(hash01(seed + dy / 3) * 2.0f), right);
    }
    m.capsule({ox + 8.0f, oy + 20.0f}, {ox - 20.0f, oy + 70.0f}, 6.0f, 2.0f); // side roots curling out
    m.capsule({ox + w - 6.0f, oy + 24.0f}, {ox + w + 18.0f, oy + 80.0f}, 6.0f, 2.0f);
    m.measure();
    shade(p, m, barkShading(seed, oy + 26));
    for (int y = oy + 4; y < p.height; y += 3) { // bark grooves following the root
        for (int x = 0; x < p.width; x += 5) {
            int gx = x + ((y / 3) % 2) * 2;
            if (m.at(gx, y) && m.depth[y * p.width + gx] > 2.0f && hash01(seed + y * 31 + gx) < 0.6f) {
                p.set(gx, y, TREE_BARK[1]);
                p.set(gx, y + 1, TREE_BARK[1]);
            }
        }
    }
    for (int i = 0; i < 5; i++) { // runes that glow faintly in the bark
        int x = ox + 12 + i * (w - 24) / 4;
        int y = oy + 10 + (i % 2) * 5;
        p.rect(x, y, 2, 2, mixColor(SPRING, TREE_BARK[2], 0.35f));
    }
    paintMossTop(p, m, 0, p.width, oy, seed);
    paintBlossoms(p, ox + 4, ox + w - 30, oy, 4, seed);
}

void paintGardenLedge(const Node& node, LedgeCanvas& c) {
    int seed = node.id * 37 + 11;
    switch (node.id) {
        case 1: paintGardenGate(c, seed); break;
        case 2: paintMoonCap(c, seed); break;
        case 3: paintFlowerBed(c, seed); break;
        default: paintRootKnot(c, seed); break;
    }
}

// ---------------------------------------------------------------------------
// Scenery
// ---------------------------------------------------------------------------

Picture paintCrown(float r, int seed) {
    int size = (int)(r * 3.4f);
    Picture piece(size, size);
    float c = size * 0.5f;
    paintFoliage(piece, c - r * 0.75f, c + r * 0.25f, r * 0.62f, TREE_LEAF, 5, TO_MOON, seed + 1);
    paintFoliage(piece, c + r * 0.75f, c + r * 0.2f, r * 0.6f, TREE_LEAF, 5, TO_MOON, seed + 2);
    paintFoliage(piece, c, c, r, TREE_LEAF, 5, TO_MOON, seed);
    for (int i = 0; i < (int)(r * 2.0f); i++) { // blossoms scattered through the leaves
        float angle = hash01(seed * 7 + i) * 6.283f;
        float d = std::sqrt(hash01(seed * 11 + i)) * r * 1.2f;
        int x = (int)(c + std::cos(angle) * d), y = (int)(c + std::sin(angle) * d * 0.8f);
        if (!piece.painted(x, y)) continue;
        Color bloom = (i % 3 == 0) ? PETAL : BLOSSOM;
        piece.set(x, y, mixColor(bloom, TREE_LEAF[2], (y < c) ? 0.1f : 0.45f)); // brighter where the moon reaches
    }
    return piece;
}

Picture paintWisteria(int strands, int length, int seed) {
    Picture p(strands * 5 + 4, length + 2);
    for (int s = 0; s < strands; s++) {
        int len = length / 2 + (int)(hash01(seed + s * 5) * length / 2);
        for (int k = 0; k < len; k++) {
            int width = std::max(1, 3 - k * 3 / len);
            int x = 2 + s * 5 + (int)std::lround(std::sin(k * 0.2f + s) * 0.8f);
            for (int i = 0; i < width; i++) {
                Color bloom = ((k + i) % 3 == 0) ? WISTERIA : mixColor(WISTERIA, TREE_LEAF[1], 0.45f);
                p.set(x + i - width / 2, k, bloom);
            }
        }
    }
    return p;
}

Picture paintFern(int size, int seed) {
    Picture p(size, size);
    int fronds = 4 + (int)(hash01(seed) * 3.0f);
    for (int f = 0; f < fronds; f++) {
        float spread = (f - (fronds - 1) * 0.5f) / fronds;
        float length = size * (0.6f + 0.35f * hash01(seed * 3 + f));
        for (int k = 0; k < (int)length; k++) {
            float t = k / length;
            int x = (int)(size * 0.5f + spread * length * 1.5f * t + spread * length * 0.7f * t * t);
            int y = size - 1 - (int)(length * (t - 0.35f * t * t));
            p.set(x, y, FERN[1]);
            if (k % 3 == 1 && k > 2) {
                p.set(x - 1, y, FERN[(f % 2 == 0) ? 2 : 0]);
                p.set(x + 1, y - 1, FERN[1]);
            }
        }
    }
    return p;
}

void bakeGarden(RegionArt& art, const World& world) {
    Picture sky(CANVAS_WIDTH, CANVAS_HEIGHT);
    ditherGradient(sky, 0, 180, SKY_TOP, SKY_MID);
    ditherGradient(sky, 180, CANVAS_HEIGHT, SKY_MID, SKY_LOW);
    for (int i = 0; i < 110; i++) {
        int x = (int)(hash01(i * 7 + 3) * CANVAS_WIDTH);
        int y = (int)(hash01(i * 11 + 5) * 180.0f);
        sky.set(x, y, mixColor(SKY_MID, MOON, 0.3f + 0.6f * hash01(i * 3)));
    }
    for (int y = -20; y <= 20; y++) { // the moon, with soft seas and a darker rim
        for (int x = -20; x <= 20; x++) {
            float d = std::sqrt((float)(x * x + y * y));
            if (d > 16.5f) continue;
            float sea = fbm2((x + 60) * 0.15f, (y + 60) * 0.15f, 9, 3);
            float rim = (d / 16.5f) * (d / 16.5f);
            Color c = mixColor(MOON, SKY_LOW, 0.1f + 0.18f * rim);
            sky.set((int)MOON_X + x, (int)MOON_Y + y, (sea > 0.55f) ? mixColor(c, SKY_LOW, 0.3f) : c);
        }
    }
    storeLayer(art, sky, 0.0f, 0);

    // Rolling hills with cypresses, and an overgrown observatory on the far right.
    Picture hills(CANVAS_WIDTH, CANVAS_HEIGHT);
    for (int x = 0; x < CANVAS_WIDTH; x++) {
        float y = 214.0f + std::sin(x * 0.011f + 1.3f) * 14.0f + std::sin(x * 0.027f) * 6.0f;
        int top = (int)(std::floor(y / 2.0f) * 2.0f);
        for (int yy = top; yy < CANVAS_HEIGHT; yy++) hills.set(x, yy, HILLS[(yy - top < 2) ? 2 : 0]);
    }
    for (int i = 0; i < 14; i++) {
        int x = 20 + i * 46 + (int)(hash01(i * 13 + 2) * 20.0f);
        int height = 26 + (int)(hash01(i * 17 + 4) * 26.0f);
        int base = 214 + (int)(std::sin(x * 0.011f + 1.3f) * 14.0f);
        for (int k = 0; k < height; k++) {
            int half = (int)(5.0f * std::sin((k + 2.0f) / (height + 2.0f) * 3.14159f));
            for (int xx = -half; xx <= half; xx++) hills.set(x + xx, base - k, HILLS[(xx < 0) ? 1 : 0]);
        }
    }
    for (int y = -18; y <= 0; y++) {
        for (int x = -18; x <= 18; x++) if (x * x + y * y <= 18 * 18) hills.set(566 + x, 206 + y, HILLS[1]);
    }
    hills.rect(548, 206, 36, 12, HILLS[1]);
    hills.rect(564, 190, 3, 16, HILLS[3]);
    fogRows(hills, MIST, 180, 300, 0.25f, 0.5f);
    storeLayer(art, hills, 0.0f, 0);

    // The garden behind the route: hedges with glowing flowers, and two stone lanterns.
    Picture garden(CANVAS_WIDTH, CANVAS_HEIGHT);
    for (int i = 0; i < 10; i++) {
        float x = 10.0f + i * 68.0f + hash01(i * 7 + 3) * 20.0f;
        float y = 280.0f + hash01(i * 11 + 1) * 12.0f;
        paintFoliage(garden, x, y, 16.0f + hash01(i * 13 + 5) * 9.0f, HEDGE, 4, TO_MOON, 140 + i, 0.25f);
    }
    garden.rect(0, 290, CANVAS_WIDTH, CANVAS_HEIGHT - 290, HEDGE[0]);
    const int lanternX[2] = {168, 470};
    for (int i = 0; i < 2; i++) {
        int x = lanternX[i], base = 300;
        garden.rect(x - 6, base - 4, 12, 4, STONE[1]);
        garden.rect(x - 2, base - 16, 4, 12, STONE[1]);
        garden.rect(x - 5, base - 24, 10, 8, STONE[1]);
        garden.rect(x - 3, base - 22, 6, 4, Color{255, 214, 150, 255});
        for (int k = 0; k < 7; k++) garden.rect(x - 9 + k, base - 25 - k, 18 - k * 2, 1, STONE[2]);
    }
    fogRows(garden, MIST, 240, 320, 0.2f, 0.32f);
    storeLayer(art, garden, 0.0f, 0);

    // The ancient tree: trunk, boughs, and roots that arch over the spring.
    Picture tree(CANVAS_WIDTH, CANVAS_HEIGHT);
    paintTrunk(tree, TREE_X, 30.0f, 300.0f, 26.0f, 36.0f, TREE_BARK, 6, TO_MOON, 31);
    Mask limbs(CANVAS_WIDTH, CANVAS_HEIGHT);
    limbs.capsule({TREE_X - 14.0f, 120.0f}, {TREE_X - 120.0f, 54.0f}, 12.0f, 7.0f);
    limbs.capsule({TREE_X - 120.0f, 54.0f}, {TREE_X - 230.0f, 26.0f}, 7.0f, 3.0f);
    limbs.capsule({TREE_X + 14.0f, 104.0f}, {TREE_X + 126.0f, 50.0f}, 11.0f, 6.0f);
    limbs.capsule({TREE_X + 126.0f, 50.0f}, {TREE_X + 220.0f, 36.0f}, 6.0f, 3.0f);
    limbs.capsule({TREE_X - 18.0f, 286.0f}, {TREE_X - 70.0f, 318.0f}, 17.0f, 12.0f); // the root arch
    limbs.capsule({TREE_X - 70.0f, 318.0f}, {TREE_X - 120.0f, 364.0f}, 12.0f, 9.0f);
    limbs.capsule({TREE_X + 18.0f, 286.0f}, {TREE_X + 76.0f, 316.0f}, 16.0f, 11.0f);
    limbs.capsule({TREE_X + 76.0f, 316.0f}, {TREE_X + 128.0f, 364.0f}, 11.0f, 9.0f);
    limbs.measure();
    shade(tree, limbs, barkShading(32, 230));
    for (int y = 40; y < 300; y++) { // grooves along the trunk
        for (int g = 0; g < 5; g++) {
            int x = (int)(TREE_X - 18.0f + g * 9.0f + std::sin(y * 0.04f + g * 1.9f) * 2.0f);
            if (tree.painted(x, y) && noise2(x * 0.3f, y * 0.07f, g + 50) > 0.38f) tree.set(x, y, TREE_BARK[1]);
        }
    }
    for (int i = 0; i < 9; i++) { // faint runes in the bark, like the ones on the tail's roots
        int x = (int)TREE_X - 16 + (int)(hash01(i * 5 + 1) * 32.0f);
        int y = 80 + i * 22;
        tree.rect(x, y, 2, 2, mixColor(SPRING, TREE_BARK[2], 0.55f));
    }
    storeLayer(art, tree, 0.0f, 0);
    const float crowns[9][3] = {{40.0f, 18.0f, 30.0f},  {120.0f, 30.0f, 34.0f}, {200.0f, 10.0f, 40.0f}, {290.0f, 30.0f, 38.0f},
                                {380.0f, 6.0f, 46.0f},  {470.0f, 28.0f, 40.0f}, {552.0f, 12.0f, 38.0f}, {626.0f, 30.0f, 32.0f},
                                {340.0f, 50.0f, 26.0f}};
    for (int i = 0; i < 9; i++) {
        Picture piece = paintCrown(crowns[i][2], 200 + i * 13);
        storeSway(art, piece, TREE_LAYER, false, crowns[i][0] - piece.width * 0.5f, crowns[i][1] - piece.height * 0.5f, 1.8f,
                  true, 3, 0.2f * i);
    }
    const float wisteriaX[6] = {96.0f, 196.0f, 300.0f, 430.0f, 512.0f, 600.0f};
    for (int i = 0; i < 6; i++) {
        Picture strands = paintWisteria(3, 26 + (i % 3) * 10, 260 + i * 7);
        storeSway(art, strands, TREE_LAYER, false, wisteriaX[i], 34.0f + (i % 2) * 10.0f, 2.6f, true, 6, 0.15f * i);
    }

    const Node* head = world.getHead(2);
    const Node* node = head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        LedgeCanvas canvas = makeLedgeCanvas(*node, 30, 12);
        paintGardenLedge(*node, canvas);
        storeLedge(art, *node, canvas);
        node = node->next;
        if (node == head) break; // circular list: stop when back at the start
    }

    // Foreground: ferns in the corners and wisteria over the top edge.
    storeEmptyLayer(art, 1.0f, true);
    const float fernX[5] = {-12.0f, 24.0f, 250.0f, 560.0f, 604.0f};
    for (int i = 0; i < 5; i++) {
        int size = 36 + (int)(hash01(i * 7 + 2) * 22.0f);
        Picture fern = paintFern(size, 300 + i * 9);
        storeSway(art, fern, 0, true, fernX[i], (float)(CANVAS_HEIGHT - size + 3), 2.0f, false, 4, 0.2f * i);
    }
    const float topX[4] = {-10.0f, 70.0f, 520.0f, 590.0f};
    for (int i = 0; i < 4; i++) {
        Picture strands = paintWisteria(4, 34 + (i % 2) * 16, 330 + i * 5);
        storeSway(art, strands, 0, true, topX[i], -4.0f, 3.0f, true, 6, 0.25f * i);
    }
}

// ---------------------------------------------------------------------------
// Live pieces
// ---------------------------------------------------------------------------

// The current: a ribbon of light with waves running along it and petals riding it.
void drawCurrent(const Flow& flow, float time, float surge) {
    float length = 0.0f; // sample it densely enough that the ribbon has no gaps
    for (int i = 1; i <= 64; i++) {
        Vector2 a = flowPoint(flow, (i - 1) / 64.0f), b = flowPoint(flow, i / 64.0f);
        length += std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    }
    const int samples = (int)(length * 1.6f) + 2;
    for (int i = 0; i < samples; i++) {
        float t = i / (float)(samples - 1);
        Vector2 p = flowPoint(flow, t);
        Vector2 q = flowPoint(flow, std::fmin(1.0f, t + 0.004f));
        float dx = q.x - p.x, dy = q.y - p.y, d = std::sqrt(dx * dx + dy * dy) + 0.001f;
        Vector2 side = {-dy / d, dx / d};
        float wave = 0.5f + 0.5f * std::sin(t * length * 0.12f - time * 4.0f); // bright swells running along it
        float edgeFade = std::fmin(1.0f, std::fmin(t, 1.0f - t) * 14.0f);
        Color core = lerpColor(SPRING, WHITE, 0.2f + 0.5f * wave * surge);
        for (int w = -3; w <= 3; w++) {
            int across = std::abs(w);
            float a = (across == 3 ? 0.18f : (across == 2 ? 0.45f : (across == 1 ? 0.8f : 1.0f))) * edgeFade;
            Color c = (across >= 2) ? SPRING : core;
            DrawRectangle((int)std::floor(p.x + side.x * w), (int)std::floor(p.y + side.y * w), 1, 1, withAlpha(c, a));
        }
    }
    for (int i = 0; i < 26; i++) { // petals riding the current
        float speed = 0.07f + 0.04f * hash01(i * 3 + 1);
        float t = std::fmod(time * speed + hash01(i * 7 + 2), 1.0f);
        Vector2 p = flowPoint(flow, t);
        float drift = std::sin(time * 2.0f + i) * 2.0f;
        Color petal = (i % 3 == 0) ? WISTERIA : ((i % 3 == 1) ? PETAL : BLOSSOM);
        bool flat = std::cos(time * 3.0f + i * 1.7f) > 0.0f;
        DrawRectangle((int)p.x, (int)(p.y + drift), flat ? 2 : 1, flat ? 1 : 2, withAlpha(petal, loopEnvelope(t)));
    }
}

// The hollow at the tail end: an arching root around a dark opening that glows as the traveler nears it.
void drawRootHollow(Rectangle portal, const SceneView& view, bool ready) {
    int px = toCanvasX(portal.x, view.cameraX);
    int py = toCanvasY(portal.y);
    int pw = (int)(portal.width / CANVAS_SCALE);
    int ph = (int)(portal.height / CANVAS_SCALE);
    Vector2 center = {(float)(px + pw / 2), (float)(py + ph / 2 + 2)};
    float pulse = 0.5f + 0.5f * std::sin(view.time * 3.0f);
    float closeness = ready ? smooth01(1.0f - std::fabs(view.playerFeet.x - center.x) / 90.0f) * 0.7f + 0.3f : 0.0f;
    DrawEllipse((int)center.x, (int)center.y, 7.0f, 16.0f, DARK);
    DrawEllipse((int)center.x, (int)center.y + 2, 5.0f, 12.0f, withAlpha(SPRING, 0.2f + (0.3f + 0.3f * pulse) * closeness));
    DrawRing(Vector2{center.x, center.y - 4.0f}, 7.0f, 11.0f, 180.0f, 360.0f, 16, TREE_BARK[2]);
    DrawRing(Vector2{center.x, center.y - 4.0f}, 10.0f, 11.0f, 270.0f, 360.0f, 8, TREE_BARK[4]);
    DrawRectangle((int)center.x - 11, (int)center.y - 4, 4, 22, TREE_BARK[2]);
    DrawRectangle((int)center.x + 7, (int)center.y - 4, 4, 22, TREE_BARK[1]);
    for (int i = 0; closeness > 0.0f && i < 4; i++) { // petals drifting into the hollow
        float t = std::fmod(view.time * 0.5f + i * 0.25f, 1.0f);
        DrawRectangle((int)(center.x - 22.0f + t * 20.0f), (int)(center.y - 14.0f + std::sin(t * 6.0f + i) * 4.0f), 2, 1,
                      withAlpha(PETAL, closeness * loopEnvelope(t)));
    }
}

// A round moon gate on the head. Wisteria hangs in it until the loop is complete; then it
// parts and draws up while the spring's light fills the ring (open: 0 -> 1).
void drawMoonGate(Rectangle gate, int cameraX, float open, float time) {
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    const float radius = 17.0f;
    const Vector2 center = {(float)(gx + gw / 2), (float)(gy + gh) - radius - 2.0f};
    const float light = smooth01(open);
    DrawCircleV(center, radius - 3.0f, withAlpha(Color{24, 16, 46, 255}, 1.0f - light));
    if (light > 0.0f) {
        DrawCircleV(center, radius - 3.0f, withAlpha(SPRING, 0.75f * light));
        DrawCircleV(center, (radius - 7.0f) * (0.4f + 0.6f * light), withAlpha(WHITE, 0.5f * light));
        for (int i = 0; i < 6; i++) {
            float t = std::fmod(time * (0.35f + 0.1f * hash01(i * 7)) + i * 0.17f, 1.0f);
            DrawRectangle((int)center.x - 8 + (int)(hash01(i * 5 + 1) * 16.0f), (int)(center.y + 10.0f - t * 24.0f), 1, 1,
                          withAlpha(WHITE, light * loopEnvelope(t)));
        }
    }
    if (open < 1.0f) {
        for (int strand = 0; strand < 7; strand++) { // the curtain sways, then parts to the sides
            int part = (int)std::round((strand - 3) * light * 3.0f);
            int x = (int)center.x - 10 + strand * 3 + part;
            int length = (int)std::round((14 + (strand % 3) * 5) * (1.0f - 0.85f * light));
            for (int k = 0; k < length; k++) {
                int sway = (int)std::round(std::sin(time * 1.5f + strand + k * 0.1f) * (k / 18.0f) * 1.5f);
                Color bloom = (k % 3 == 0) ? WISTERIA : lerpColor(WISTERIA, DARK, 0.4f);
                DrawRectangle(x + sway, (int)center.y - 12 + k, 1, 1, withAlpha(bloom, 1.0f - light * 0.6f));
            }
        }
    }
    DrawRing(center, radius - 3.0f, radius + 2.0f, 0.0f, 360.0f, 40, STONE[3]);
    DrawRing(center, radius + 1.0f, radius + 2.0f, 180.0f, 270.0f, 12, STONE[5]); // lit on the moon's side
    for (int i = 0; i < 6; i++) { // vines and blossoms on the ring
        float angle = 3.4f + i * 0.45f;
        int x = (int)(center.x + std::cos(angle) * radius);
        int y = (int)(center.y + std::sin(angle) * radius);
        DrawRectangle(x, y, 2, 2, MOSS[1]);
        if (i % 2 == 0) DrawRectangle(x, y - 1, 1, 1, BLOSSOM);
    }
}

float ledgeMist(const SceneView& view, const Node& node) {
    if (view.mistById == nullptr || node.id < 0 || node.id > MAX_NODES_PER_REGION) return 0.0f;
    return view.mistById[node.id];
}

// The current brightens while it carries the traveler, and when the traveler stands at the hollow.
float currentSurge(const SceneView& view) {
    float surge = 0.35f;
    if (view.current != nullptr && view.current->next == view.head) surge = 0.65f;
    if (view.passage >= 0.0f) surge = 1.0f;
    return surge;
}

// ---------------------------------------------------------------------------
// Region 3 drawing
// ---------------------------------------------------------------------------

void drawGardenBackground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    drawBackLayers(view, art, SKY_LAYER, HILLS_LAYER, 0.0f);
    for (int i = 0; i < 30; i++) { // stars that twinkle
        int x = (int)(hash01(i * 17 + 3) * CANVAS_WIDTH);
        int y = 4 + (int)(hash01(i * 19 + 5) * 160.0f);
        float twinkle = 0.5f + 0.5f * std::sin(time * (1.1f + hash01(i)) + i * 1.9f);
        DrawRectangle(x, y, 1, 1, withAlpha(MOON, 0.2f + 0.65f * twinkle));
    }
    drawBackLayers(view, art, HILLS_LAYER, TREE_LAYER, 0.6f);
    drawBackLayers(view, art, TREE_LAYER, LAYER_COUNT, 0.9f);

    // Fruit lanterns hanging from the boughs, swinging gently.
    for (int i = 0; i < 16; i++) {
        float x = 70.0f + hash01(i * 17 + 5) * 500.0f;
        float y = 52.0f + hash01(i * 19 + 7) * 44.0f;
        float swing = std::sin(time * 1.2f + i) * 1.5f + windAt(x, time, 0.6f) * 1.2f;
        Color glow = (i % 3 == 0) ? BLOSSOM : SPRING;
        DrawLineEx(Vector2{x, y - 8.0f}, Vector2{x + swing, y}, 1.0f, TREE_LEAF[1]);
        DrawRectangle((int)(x + swing) - 1, (int)y, 2, 3, glow);
    }

    // The spring under the root arch: bright water with light rising from it.
    const float springLeft = TREE_X - 98.0f, springRight = TREE_X + 104.0f;
    const float springMiddle = (springLeft + springRight) * 0.5f;
    for (int y = SPRING_Y; y < CANVAS_HEIGHT; y++) { // a pool that fades out toward its banks
        float depth = (y - SPRING_Y) / (float)(CANVAS_HEIGHT - SPRING_Y);
        int half = (int)((springRight - springLeft) * 0.5f * (1.0f - 0.25f * depth));
        Color water = lerpColor(lerpColor(SPRING, WHITE, 0.1f), Color{24, 46, 80, 255}, depth * 0.85f);
        DrawRectangleGradientH((int)springMiddle - half, y, half, 1, withAlpha(water, 0.0f), withAlpha(water, 0.9f));
        DrawRectangleGradientH((int)springMiddle, y, half, 1, withAlpha(water, 0.9f), withAlpha(water, 0.0f));
    }
    DrawRectangleGradientH((int)springLeft + 10, SPRING_Y, (int)(springMiddle - springLeft) - 10, 1, withAlpha(WHITE, 0.0f), withAlpha(WHITE, 0.7f));
    DrawRectangleGradientH((int)springMiddle, SPRING_Y, (int)(springRight - springMiddle) - 10, 1, withAlpha(WHITE, 0.7f), withAlpha(WHITE, 0.0f));
    for (int i = 0; i < 18; i++) { // shimmer on the surface, fading in and out
        float cycle = (hash01(i * 5 + 3) + time * (0.02f + 0.01f * (i % 4)));
        float t = cycle - std::floor(cycle);
        float x = springLeft + 4.0f + t * (springRight - springLeft - 12.0f);
        int y = SPRING_Y + 3 + (int)(hash01(i * 7 + 1) * 24.0f);
        DrawRectangle((int)x, y, 3 + (int)(hash01(i * 11) * 7.0f), 1, withAlpha(WHITE, 0.3f * loopEnvelope(t)));
    }
    for (int i = 0; i < 5; i++) { // lotus flowers bobbing on the water
        int x = (int)springLeft + 20 + i * 38 + (int)(hash01(i * 3 + 9) * 12.0f);
        int y = SPRING_Y + 5 + (i % 2) * 6 + (int)std::round(std::sin(time * 1.3f + i) * 1.0f);
        DrawRectangle(x - 2, y, 5, 1, MOSS[2]);
        DrawRectangle(x - 1, y - 1, 3, 1, BLOSSOM);
        DrawRectangle(x, y - 2, 1, 1, PETAL);
    }
}

void drawGardenPlatforms(const SceneView& view, const RegionArt& art) {
    drawLedges(view, art, VEIL);

    Flow flow;
    bool circle = returnFlow(view, flow);
    if (circle) drawCurrent(flow, view.time, currentSurge(view));

    const Node* next = (view.current != nullptr) ? view.current->next : nullptr; // never NULL in a circle, still checked
    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        float mist = ledgeMist(view, *node);
        if (node->id == 1 || node->id == 4) {
            drawGrass(ledge, view, lerpColor(MOSS[4], VEIL, mist * 0.7f), lerpColor(MOSS[2], VEIL, mist * 0.7f), 0.6f, 0.35f);
        }
        if (node->id == 3) { // flowers in the bed, nodding in the breeze
            for (int i = 0; i < 9; i++) {
                int x = ledge.left + 28 + i * (ledge.width - 56) / 8;
                int lean = (int)std::lround(windAt((float)x, view.time, 0.6f) * 1.5f);
                Color bloom = lerpColor((i % 3 == 0) ? WISTERIA : ((i % 2 == 0) ? BLOSSOM : PETAL), VEIL, mist * 0.7f);
                DrawRectangle(x, ledge.top - 2, 1, 2, lerpColor(MOSS[2], VEIL, mist * 0.7f));
                DrawRectangle(x + lean, ledge.top - 3, 1, 1, bloom);
            }
        }
        // The tail's next is the head: that link is the current, so the head gets no lip.
        if (node == next && node != view.head) drawLinkLip(ledge, WARM_LIGHT, arrivalEase(view), view.time, true);
        node = node->next;
        if (node == view.head) break; // stop condition: back at the start
    }

    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        drawLanternPost(ledge.left + ledge.width - 14, ledge.top, view.checkpointFlash, view.time);
    }
    bool onTail = view.current != nullptr && view.current->next == view.head;
    drawRootHollow(view.portalRect, view, onTail);
    if (view.hasGate) drawMoonGate(view.gateRect, view.cameraX, view.gateProgress, view.time);

    if (view.passage >= 0.0f && circle) { // the traveler, as a mote of light, carried by the current
        Vector2 p = flowPoint(flow, std::fmin(1.0f, view.passage / 0.92f));
        if (view.passage > 0.92f) { // the last hop up onto the head
            const Node* head = view.head;
            Vector2 top = {(float)toCanvasX(head->platformPosition.x + head->platformWidth / 2.0f, view.cameraX),
                           (float)toCanvasY(head->platformPosition.y) - 6.0f};
            float k = (view.passage - 0.92f) / 0.08f;
            p = Vector2{p.x + (top.x - p.x) * k, p.y + (top.y - p.y) * k - std::sin(k * 3.14159f) * 12.0f};
        }
        DrawRectangle((int)p.x - 1, (int)p.y - 1, 3, 3, WHITE);
        for (int k = 1; k <= 7; k++) { // a trail of petals
            float behind = std::fmin(1.0f, view.passage / 0.92f) - k * 0.02f;
            if (behind < 0.0f) break;
            Vector2 b = flowPoint(flow, behind);
            DrawRectangle((int)b.x + (int)(std::sin(view.time * 8.0f + k) * 2.0f), (int)b.y + k / 2, 2, 1, withAlpha(PETAL, 1.0f - k / 8.0f));
        }
    }
}

void drawGardenForeground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    for (int i = 0; i < 24; i++) { // petals drifting down through the garden, turning as they fall
        float t = std::fmod(time * 0.06f + hash01(i * 7 + 1), 1.0f);
        float x = std::fmod(hash01(i * 13 + 2) * 700.0f + t * 120.0f, 700.0f) - 30.0f + std::sin(time + i) * 8.0f;
        float y = t * 380.0f - 10.0f;
        bool flat = std::cos(time * 3.2f + i * 1.9f) > 0.0f;
        DrawRectangle((int)x, (int)y, flat ? 2 : 1, flat ? 1 : 2, withAlpha((i % 3 == 0) ? WISTERIA : PETAL, 0.75f * loopEnvelope(t)));
    }
    for (int i = 0; i < 16; i++) { // fireflies
        float x = hash01(i * 5 + 9) * CANVAS_WIDTH + std::sin(time * 0.6f + i * 1.4f) * 12.0f;
        float y = 120.0f + hash01(i * 3 + 4) * 160.0f + std::sin(time * 0.8f + i) * 8.0f;
        float twinkle = 0.5f + 0.5f * std::sin(time * 2.4f + i * 2.0f);
        DrawRectangle((int)x, (int)y, 1, 1, withAlpha(SPRING, 0.4f + 0.6f * twinkle));
    }
    drawFrontLayers(view, art, 0.8f);
    DrawRectangleGradientH(0, 0, 60, CANVAS_HEIGHT, withAlpha(DARK, 0.45f), withAlpha(DARK, 0.0f));
    DrawRectangleGradientH(CANVAS_WIDTH - 60, 0, 60, CANVAS_HEIGHT, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.45f));
}

void drawGardenAdditive(const SceneView& view, const RegionArt&, const SceneRenderer& renderer) {
    const float time = view.time;
    renderer.drawSoftLight(Vector2{MOON_X, MOON_Y}, 130.0f, MOON, 0.16f);
    renderer.drawSoftLight(Vector2{MOON_X, MOON_Y}, 46.0f, MOON, 0.28f);
    for (int x = 0; x < CANVAS_WIDTH; x += 2) { // a faint ribbon of night light across the sky
        float wave = std::sin(x * 0.012f + time * 0.15f) * 14.0f + std::sin(x * 0.031f) * 5.0f;
        int y = 50 + (int)wave;
        DrawRectangleGradientV(x, y, 2, 30, withAlpha(SPRING, 0.0f), withAlpha(SPRING, 0.04f));
        DrawRectangleGradientV(x, y + 30, 2, 16, withAlpha(SPRING, 0.04f), withAlpha(WISTERIA, 0.0f));
    }
    for (int i = 0; i < 16; i++) { // the fruit lanterns' glow
        float x = 70.0f + hash01(i * 17 + 5) * 500.0f + std::sin(time * 1.2f + i) * 1.5f;
        float y = 54.0f + hash01(i * 19 + 7) * 44.0f;
        renderer.drawSoftLight(Vector2{x, y}, 7.0f, (i % 3 == 0) ? BLOSSOM : SPRING, 0.35f);
    }
    for (int i = 0; i < 16; i++) { // the fireflies' glow
        float x = hash01(i * 5 + 9) * CANVAS_WIDTH + std::sin(time * 0.6f + i * 1.4f) * 12.0f;
        float y = 120.0f + hash01(i * 3 + 4) * 160.0f + std::sin(time * 0.8f + i) * 8.0f;
        float twinkle = 0.5f + 0.5f * std::sin(time * 2.4f + i * 2.0f);
        renderer.drawSoftLight(Vector2{x + 0.5f, y + 0.5f}, 6.0f, SPRING, 0.35f * twinkle);
    }
}

void addGardenLights(const SceneView& view, const RegionArt&, std::vector<Light>& lights) {
    const float time = view.time;
    lights.push_back(Light{Vector2{TREE_X, (float)SPRING_Y + 10.0f}, 150.0f, SPRING, 0.42f + 0.05f * std::sin(time * 1.3f), 60.0f, 0.18f});
    Flow flow;
    if (returnFlow(view, flow)) { // light along the current, swelling as waves pass
        float surge = currentSurge(view);
        for (int i = 1; i < 7; i++) {
            float t = i / 7.0f;
            float wave = 0.6f + 0.4f * std::sin(t * 46.0f - time * 5.0f);
            lights.push_back(Light{flowPoint(flow, t), 46.0f, SPRING, 0.3f * surge * wave, 10.0f, 0.12f * surge});
        }
        if (view.passage >= 0.0f) {
            Vector2 p = flowPoint(flow, std::fmin(1.0f, view.passage / 0.92f));
            lights.push_back(Light{p, 60.0f, Color{220, 255, 245, 255}, 0.6f, 14.0f, 0.7f});
        }
    }
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;
    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (node == next && node != view.head) addLinkLight(lights, ledge, WARM_LIGHT, arrivalEase(view));
        if (node->id == 2) { // the moon-cap glows from its pale spots
            float pulse = 0.5f + 0.5f * std::sin(time * 1.8f);
            lights.push_back(Light{Vector2{ledge.left + ledge.width * 0.5f, ledge.top + 3.0f}, 40.0f, SPRING, 0.22f + 0.1f * pulse, 16.0f, 0.12f + 0.08f * pulse});
        }
        if (node->id == 4) {
            bool onTail = view.current == node;
            float glow = onTail ? 0.55f : 0.25f;
            Vector2 hollow = {(float)toCanvasX(view.portalRect.x + view.portalRect.width / 2.0f, view.cameraX),
                              (float)toCanvasY(view.portalRect.y + view.portalRect.height / 2.0f)};
            lights.push_back(Light{hollow, 42.0f, SPRING, glow, 12.0f, glow * 0.6f});
        }
        node = node->next;
        if (node == view.head) break;
    }
    const int lanternX[2] = {168, 470};
    for (int i = 0; i < 2; i++) {
        float flicker = 0.85f + 0.15f * std::sin(time * 9.0f + i * 2.0f);
        lights.push_back(Light{Vector2{(float)lanternX[i], 278.0f}, 34.0f, WARM_LIGHT, 0.4f * flicker, 7.0f, 0.4f * flicker});
    }
    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        addLanternLight(lights, ledge.left + ledge.width - 14, ledge.top, view.checkpointFlash, time);
    }
    if (view.hasGate && view.gateProgress > 0.0f) {
        float light = smooth01(view.gateProgress);
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height) - 19.0f;
        lights.push_back(Light{Vector2{x, y}, 70.0f * light, SPRING, 0.55f * light, 22.0f * light, 0.5f * light});
    }
}

Mood gardenMood(const SceneView&) {
    Mood mood;
    mood.skyLight = WHITE;
    mood.groundLight = Color{136, 120, 176, 255};
    mood.lightFrom = 90;
    mood.lightTo = 300;
    mood.shadowTint = Vector3{0.92f, 0.86f, 1.1f};
    mood.highlightTint = Vector3{1.04f, 0.99f, 1.06f};
    mood.saturation = 1.08f;
    mood.bloom = 0.85f;
    mood.vignette = 0.42f;
    return mood;
}

} // namespace

const RegionPainter GARDEN_PAINTER = {bakeGarden, drawGardenBackground, drawGardenPlatforms, nullptr,
                                      drawGardenForeground, drawGardenAdditive, addGardenLights, gardenMood};
