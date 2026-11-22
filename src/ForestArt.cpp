#include "ArtCommon.h"

#include <algorithm>

// Region 1, "The First Connection": a deep forest in late golden light. The path climbs from
// the forest floor onto a fallen log held between two old stumps, then steps down to a sunlit
// arch. Platforms are painted from their nodes; everything else is scenery and holds nothing.

namespace {

// Teal shadows, warm light from behind the trees on the upper right.
const Color SKY_TOP = {16, 38, 54, 255};
const Color SKY_MID = {44, 92, 96, 255};
const Color SKY_GLOW = {206, 196, 134, 255};
const Color SKY_LOW = {122, 146, 116, 255};
const Color SUN = {255, 228, 168, 255};
const Color MIST = {176, 200, 184, 255};
const Color VEIL = {118, 158, 148, 255};
const Color DARK = {7, 15, 15, 255};

const Color HILLS[4] = {{104, 138, 128, 255}, {116, 150, 134, 255}, {130, 162, 140, 255}, {156, 180, 148, 255}};
const Color FAR[4] = {{44, 80, 80, 255}, {54, 94, 90, 255}, {68, 110, 100, 255}, {96, 136, 112, 255}};
const Color MID_BARK[5] = {{16, 30, 30, 255}, {24, 44, 42, 255}, {34, 60, 54, 255}, {50, 80, 66, 255}, {74, 108, 82, 255}};
const Color MID_LEAF[5] = {{18, 38, 34, 255}, {28, 56, 46, 255}, {40, 76, 58, 255}, {60, 100, 70, 255}, {94, 134, 84, 255}};
const Color GIANT_BARK[6] = {{12, 22, 22, 255}, {20, 36, 34, 255}, {30, 52, 46, 255},
                             {44, 72, 60, 255}, {64, 96, 74, 255}, {98, 130, 92, 255}};
const Color GIANT_LEAF[5] = {{10, 24, 22, 255}, {18, 38, 32, 255}, {28, 56, 42, 255}, {44, 80, 52, 255}, {72, 112, 66, 255}};
const Color NEAR_BARK[5] = {{6, 12, 12, 255}, {12, 22, 20, 255}, {20, 34, 30, 255}, {32, 50, 40, 255}, {54, 76, 56, 255}};
const Color CANOPY[5] = {{6, 14, 13, 255}, {11, 24, 21, 255}, {18, 36, 30, 255}, {28, 54, 40, 255}, {48, 82, 52, 255}};
const Color MOSS[6] = {{26, 52, 36, 255}, {48, 96, 54, 255}, {84, 146, 68, 255},
                       {130, 192, 86, 255}, {186, 228, 112, 255}, {228, 246, 158, 255}};
const Color EARTH[5] = {{34, 24, 24, 255}, {60, 42, 38, 255}, {94, 68, 52, 255}, {134, 98, 70, 255}, {170, 132, 90, 255}};
const Color STONE[6] = {{30, 36, 42, 255}, {56, 64, 70, 255}, {88, 98, 98, 255},
                        {126, 134, 124, 255}, {170, 174, 156, 255}, {212, 212, 186, 255}};
const Color BARK[6] = {{30, 20, 20, 255}, {58, 40, 34, 255}, {92, 64, 46, 255},
                       {132, 94, 62, 255}, {172, 128, 82, 255}, {206, 164, 110, 255}};
const Color RINGS[3] = {{150, 110, 70, 255}, {196, 156, 104, 255}, {228, 194, 138, 255}};
const Color FERN[4] = {{5, 12, 11, 255}, {9, 20, 17, 255}, {15, 30, 23, 255}, {26, 46, 32, 255}};
const Color MUSHROOM = {232, 132, 84, 255};
const Color FLOWER[3] = {{255, 230, 150, 255}, {255, 176, 196, 255}, {200, 236, 255, 255}};

const Vector2 TO_SUN = {0.6f, -0.8f};

// The back layers, far to near (indices into RegionArt::back).
enum Layer { SKY_LAYER, HILLS_LAYER, FAR_LAYER, MID_LAYER, GIANT_LAYER, NEAR_LAYER, CANOPY_LAYER, LAYER_COUNT };
const float PARALLAX[LAYER_COUNT] = {0.0f, 0.05f, 0.13f, 0.32f, 0.55f, 0.82f, 0.92f};
const float FRONT_PARALLAX = 1.3f;
const float GIANT_X = 370.0f; // the giant tree, in its layer (behind the fallen log at mid-scroll)

// ---------------------------------------------------------------------------
// Painting helpers
// ---------------------------------------------------------------------------

// Height of a treeline at x: overlapping round crowns and a few pointed firs.
float treelineTop(float x, float spacing, float baseY, float minRadius, float maxRadius, int seed, float firs) {
    int index = (int)std::floor(x / spacing);
    float best = baseY;
    for (int i = index - 2; i <= index + 2; i++) {
        float center = i * spacing + hash01(seed + i * 7) * spacing * 0.6f;
        float radius = minRadius + hash01(seed + i * 13) * (maxRadius - minRadius);
        float ground = baseY - hash01(seed + i * 29) * radius * 0.6f;
        float d = std::fabs(x - center);
        if (d >= radius) continue;
        bool fir = hash01(seed + i * 41) < firs;
        float top = fir ? ground - radius * 1.7f + d * 1.7f : ground - std::sqrt(radius * radius - d * d);
        best = std::min(best, top);
    }
    return best;
}

// A distant treeline: crowns against the sky, lit along their tops, darker and leafy inside.
void paintTreeline(Picture& p, float spacing, float baseY, float minR, float maxR, int seed, const Color* ramp, int steps,
                   float firs) {
    for (int x = 0; x < p.width; x++) {
        int top = (int)treelineTop((float)x, spacing, baseY, minR, maxR, seed, firs);
        int left = (int)treelineTop((float)(x - 2), spacing, baseY, minR, maxR, seed, firs);
        for (int y = std::max(0, top); y < p.height; y++) {
            float d = (float)(y - top);
            float v = 0.5f - std::fmin(d, 90.0f) * 0.003f + (fbm2(x * 0.09f, y * 0.09f, seed, 3) - 0.5f) * 0.3f;
            if (d < 2.0f) v += (left > top) ? 0.34f : 0.16f; // the side facing the sun catches more light
            int step = std::clamp((int)std::lround(std::clamp(v, 0.0f, 1.0f) * (steps - 1)), 0, steps - 1);
            p.set(x, y, ramp[step]);
        }
    }
}

// Moss along a walkable top: a bright lip, a ragged lower edge, and drips over the sides.
void paintMossTop(Picture& p, const Mask& m, int x0, int x1, int top, int seed, int thickness) {
    for (int x = x0; x < x1; x++) {
        int y = top;
        while (y < p.height && !m.at(x, y)) y++;
        if (y >= p.height || y > top + 6) continue;
        int t = thickness - 1 + (int)(noise2(x * 0.3f, 0.0f, seed) * 3.0f);
        for (int k = 0; k <= t; k++) {
            if (!m.at(x, y + k)) break;
            float v = 0.98f - k * (0.78f / (t + 1)) + (noise2(x * 0.7f, (y + k) * 0.7f, seed + 1) - 0.5f) * 0.25f;
            p.set(x, y + k, pickRamp(MOSS, 6, v, x, y + k));
        }
        if (hash01(seed * 31 + x) < 0.16f) { // a drip of moss over the edge
            int length = 2 + (int)(hash01(seed * 17 + x) * 7.0f);
            for (int k = t + 1; k < t + 1 + length; k++) {
                if (!m.at(x, y + k)) break;
                p.set(x, y + k, MOSS[(k > t + length - 2) ? 1 : 2]);
            }
        }
    }
}

void paintStones(Picture& p, const Mask& m, int count, int x0, int x1, int y0, int y1, int seed) {
    for (int i = 0; i < count; i++) {
        int x = x0 + (int)(hash01(seed + i * 7) * (x1 - x0));
        int y = y0 + (int)(hash01(seed + i * 13) * (y1 - y0));
        int w = 2 + (int)(hash01(seed + i * 5) * 4.0f);
        if (!m.at(x, y) || !m.at(x + w, y + 2)) continue;
        for (int yy = 0; yy < 2 + w / 3; yy++) {
            for (int xx = 0; xx < w; xx++) {
                if (!m.at(x + xx, y + yy)) continue;
                float v = 0.5f + (yy == 0 ? 0.25f : 0.0f) + (xx == w - 1 ? 0.12f : 0.0f) - (xx == 0 ? 0.2f : 0.0f);
                p.set(x + xx, y + yy, pickRamp(STONE, 6, v, x + xx, y + yy));
            }
        }
    }
}

void paintRoot(Picture& p, Vector2 a, Vector2 b, Vector2 c, float r0, float r1, int seed) {
    Mask m(p.width, p.height);
    m.capsule(a, b, r0, (r0 + r1) * 0.5f);
    m.capsule(b, c, (r0 + r1) * 0.5f, r1);
    m.measure();
    Shading s;
    s.ramp = BARK;
    s.steps = 6;
    s.toLight = TO_SUN;
    s.base = 0.3f;
    s.rim = 0.35f;
    s.bevel = 2.0f;
    s.grain = 0.1f;
    s.seed = seed;
    shade(p, m, s);
}

void paintFlowers(Picture& p, int x0, int x1, int top, int count, int seed) {
    for (int i = 0; i < count; i++) {
        int x = x0 + 3 + (int)(hash01(seed * 41 + i) * (x1 - x0 - 6));
        p.set(x, top - 1, MOSS[2]);
        p.set(x, top - 2, FLOWER[i % 3]);
    }
}

// Hanging moss baked into a picture (the live strands that sway are drawn on top).
void paintHangingMoss(Picture& p, int x, int y, int length, int seed) {
    for (int k = 0; k < length; k++) {
        int wobble = (int)std::lround(std::sin(k * 0.4f + seed) * 0.8f);
        p.set(x + wobble, y + k, MOSS[(k % 3 == 0) ? 2 : 1]);
    }
}

Shading earthShading(int seed, int darkenFrom) {
    Shading s;
    s.ramp = EARTH;
    s.steps = 5;
    s.toLight = TO_SUN;
    s.base = 0.45f;
    s.rim = 0.32f;
    s.bevel = 3.0f;
    s.grain = 0.16f;
    s.grainScale = 0.22f;
    s.seed = seed;
    s.darkenFrom = (float)darkenFrom;
    s.darkenRange = 70.0f;
    s.darkenAmount = 0.35f;
    return s;
}

Shading stoneShading(int seed, int darkenFrom, float base = 0.5f) {
    Shading s;
    s.ramp = STONE;
    s.steps = 6;
    s.toLight = TO_SUN;
    s.base = base;
    s.rim = 0.32f;
    s.bevel = 2.5f;
    s.grain = 0.12f;
    s.grainScale = 0.3f;
    s.seed = seed;
    s.darkenFrom = (float)darkenFrom;
    s.darkenRange = 80.0f;
    s.darkenAmount = 0.38f;
    return s;
}

// ---------------------------------------------------------------------------
// Platforms, one look per node, painted from the node's position and width
// ---------------------------------------------------------------------------

// N1: a mossy earth bank at the forest's edge, with roots and stones in its side.
void paintGroveEdge(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        int left = ox, right = ox + w;
        if (dy > 0 && dy < 4) { left = ox - 1; right = ox + w + 1; }
        if (dy >= 4) {
            float spread = std::fmin(20.0f, dy * 0.3f);
            left = ox + 1 - (int)spread + (int)((noise2(dy * 0.15f, 1.0f, seed) - 0.5f) * 4.0f);
            right = ox + w + 1 + (int)(3.0f * std::sin(dy * 0.07f + 1.0f)) + (int)((noise2(dy * 0.15f, 5.0f, seed) - 0.5f) * 4.0f);
        }
        m.span(y, left, right);
    }
    m.measure();
    shade(p, m, earthShading(seed, oy + 24));
    for (int y = oy + 8; y < p.height; y++) { // soil layers of uneven thickness
        if (std::sin(y * 0.55f + noise2(0.0f, y * 0.1f, seed) * 3.0f) < 0.85f) continue;
        for (int x = 0; x < p.width; x++) {
            if (m.at(x, y) && m.depth[y * p.width + x] > 2.0f && noise2(x * 0.08f, y * 0.3f, seed + 3) > 0.35f) {
                p.set(x, y, scaleColor(p.get(x, y), 0.8f));
            }
        }
    }
    paintStones(p, m, 16, ox - 12, ox + w + 6, oy + 8, p.height - 6, seed * 3);
    paintRoot(p, {ox + w * 0.2f, oy + 16.0f}, {ox - 6.0f, oy + 27.0f}, {ox - 15.0f, oy + 46.0f}, 3.0f, 1.0f, seed + 5);
    paintRoot(p, {ox + w - 10.0f, oy + 12.0f}, {ox + w + 7.0f, oy + 22.0f}, {ox + w + 11.0f, oy + 40.0f}, 2.5f, 1.0f, seed + 6);
    paintMossTop(p, m, 0, p.width, oy, seed, 5);
    paintFlowers(p, ox, ox + w, oy, 5, seed);
}

// N2: a round boulder capped with moss, resting on a mound of earth.
void paintMossyRock(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask base(p.width, p.height);
    Vector2 mound[4] = {{ox + 10.0f, oy + 22.0f}, {ox + w - 8.0f, oy + 22.0f}, {ox + w + 12.0f, (float)p.height}, {ox - 10.0f, (float)p.height}};
    base.polygon(mound, 4);
    base.measure();
    shade(p, base, earthShading(seed + 2, oy + 30));
    paintStones(p, base, 10, ox - 6, ox + w + 6, oy + 34, p.height - 4, seed * 5);

    Mask rock(p.width, p.height);
    float cx = ox + w * 0.5f;
    rock.ellipse(cx, oy + 9.0f, w * 0.5f + 7.0f, 23.0f);
    rock.blob(ox + 10.0f, oy + 20.0f, 11.0f, seed, 0.25f);
    rock.blob(ox + w - 8.0f, oy + 22.0f, 10.0f, seed + 1, 0.25f);
    for (int y = 0; y < oy; y++) rock.span(y, 0, p.width, false); // the walkable top is flat and exact
    rock.span(oy, 0, p.width, false);
    rock.span(oy, ox, ox + w);
    rock.measure();
    Shading s = stoneShading(seed, oy + 18);
    s.rim = 0.45f;
    s.bevel = 6.0f; // a big round stone: wide soft shading toward its edges
    shade(p, rock, s);
    for (int i = 0; i < 2; i++) { // cracks
        int x = ox + w / 4 + i * w / 3;
        for (int k = 0; k < 10; k++) {
            if (rock.at(x + k / 3, oy + 8 + k)) p.set(x + k / 3, oy + 8 + k, STONE[0]);
        }
    }
    for (int i = 0; i < 40; i++) { // lichen on the stone
        int x = ox - 4 + (int)(hash01(seed * 19 + i) * (w + 8));
        int y = oy + 6 + (int)(hash01(seed * 23 + i) * 24.0f);
        if (rock.at(x, y) && rock.depth[y * p.width + x] > 1.5f) p.set(x, y, MOSS[(i % 3 == 0) ? 2 : 1]);
    }
    paintMossTop(p, rock, 0, p.width, oy, seed, 6);
}

// N3: a fallen log held high between two old stumps, with a cut end and small mushrooms.
void paintFallenLog(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    paintTrunk(p, ox + 16.0f, oy + 10.0f, (float)p.height, 7.0f, 14.0f, BARK, 6, TO_SUN, seed + 1);
    paintTrunk(p, ox + w - 20.0f, oy + 10.0f, (float)p.height, 6.0f, 12.0f, BARK, 6, TO_SUN, seed + 2);
    fogRows(p, Color{24, 40, 36, 255}, oy + 20, p.height, 0.15f, 0.65f); // the stumps sink into the shadow below

    const int thickness = 18;
    for (int dy = 0; dy < thickness; dy++) {
        int y = oy + dy;
        int left = (dy == 0) ? ox : ox - 3 - (int)(hash01(seed * 7 + dy) * 4.0f); // splintered end
        int right = (dy == 0) ? ox + w : ox + w + 2;
        for (int x = left; x < right; x++) {
            float streak = (noise2(x * 0.13f, dy * 0.9f, seed) - 0.5f) * 0.3f;
            float v = 0.8f - dy * 0.034f + streak;
            if (dy == 0) v = 0.9f;
            if (dy == thickness - 1 || x == left) v = 0.05f;
            p.set(x, y, pickRamp(BARK, 6, v, x, y));
        }
    }
    for (int g = 0; g < 9; g++) { // bark grain
        int gx = ox + 6 + (int)(hash01(seed * 3 + g) * (w - 20));
        int gy = oy + 4 + (int)(hash01(seed * 5 + g) * 10.0f);
        for (int k = 0; k < 5 + g % 4; k++) p.set(gx + k, gy, BARK[1]);
    }
    int ringX = ox + w + 1, ringY = oy + 8;
    for (int y = -8; y <= 8; y++) { // the cut end, with its rings
        for (int x = -4; x <= 4; x++) {
            float d = std::sqrt((x / 4.5f) * (x / 4.5f) + (y / 8.5f) * (y / 8.5f));
            if (d > 1.0f) continue;
            Color ring = (d > 0.86f) ? BARK[1] : RINGS[((int)(d * 6.0f)) % 3];
            p.set(ringX + x, ringY + y, ring);
        }
    }
    for (int i = 0; i < 6; i++) { // moss patches on top
        int x = ox + 4 + (int)(hash01(seed * 23 + i) * (w - 14));
        int len = 4 + (int)(hash01(seed * 29 + i) * 9.0f);
        for (int k = 0; k < len; k++) {
            p.set(x + k, oy, MOSS[4]);
            p.set(x + k, oy + 1, MOSS[2]);
            if (k % 3 == 1) p.set(x + k, oy + 2, MOSS[1]);
        }
    }
    for (int i = 0; i < 3; i++) { // small mushrooms on the side of the log
        int x = ox + 26 + i * 6;
        int y = oy + 12 - (i % 2);
        p.rect(x, y, 4, 1, MUSHROOM);
        p.set(x + 1, y - 1, scaleColor(MUSHROOM, 1.15f));
        p.set(x + 2, y - 1, scaleColor(MUSHROOM, 1.15f));
        p.set(x + 1, y + 1, RINGS[2]);
        p.set(x + 1, y + 2, RINGS[1]);
    }
    for (int i = 0; i < 5; i++) paintHangingMoss(p, ox + 20 + i * (w - 30) / 4, oy + thickness, 4 + (int)(hash01(seed + i) * 9.0f), seed + i);
}

// N4: the top of an old ruined wall, overgrown with ivy.
void paintOldRuin(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        int left = ox + 2, right = ox + w - 2;
        if (dy == 0) { left = ox; right = ox + w; }
        else if (dy < 4) { left = ox - 2; right = ox + w + 2; } // cornice
        else if (dy > 42) {                                       // broken lower courses spreading into rubble
            left = ox + 2 - (int)((dy - 42) * 0.25f) + (int)(hash01(seed + dy / 4) * 5.0f);
            right = ox + w - 2 + (int)((dy - 42) * 0.35f) - (int)(hash01(seed * 3 + dy / 4) * 5.0f);
        }
        m.span(y, left, right);
    }
    m.measure();
    shade(p, m, stoneShading(seed, oy + 30));
    for (int course = 0; course < 12; course++) { // mortar: courses, with the joints offset each course
        int y = oy + 5 + course * 8;
        for (int x = 0; x < p.width; x++) if (m.at(x, y) && m.depth[y * p.width + x] > 1.0f) p.set(x, y, STONE[1]);
        for (int x = ox + 4 + (course % 2) * 7; x < ox + w + 10; x += 14) {
            for (int k = 1; k < 8; k++) if (m.at(x, y + k) && m.depth[(y + k) * p.width + x] > 1.0f) p.set(x, y + k, STONE[1]);
        }
    }
    for (int x = ox - 2; x < ox + w + 2; x++) p.set(x, oy + 1, STONE[5]); // the cornice catches the light
    int wx = ox + (int)(w * 0.32f), wy = oy + 20;
    for (int y = -5; y < 14; y++) { // a dark arched window
        for (int x = -5; x <= 5; x++) {
            if (y < 0 && x * x + y * y > 25) continue;
            p.set(wx + x, wy + y, (x == 5 || y == 13) ? STONE[2] : STONE[0]);
        }
    }
    for (int k = 0; k < 12; k++) p.set(ox + w * 3 / 5 + (k % 3) - k / 4, oy + 10 + k, STONE[0]); // a crack
    for (int i = 0; i < 6; i++) { // ivy over the face
        int x = ox + 4 + (int)(hash01(seed * 61 + i) * (w - 8));
        int length = 8 + (int)(hash01(seed * 67 + i) * 22.0f);
        for (int k = 0; k < length; k++) {
            int sway = (int)std::lround(std::sin(k * 0.25f + i) * 1.2f);
            p.set(x + sway, oy + 2 + k, MOSS[1]);
            if (k % 3 == 1) p.set(x + sway + ((k / 3) % 2 == 0 ? 1 : -1), oy + 2 + k, MOSS[2 + (k % 2)]);
        }
    }
    paintMossTop(p, m, ox, ox + w, oy, seed, 2);
}

// N5: a mossy cliff that carries the stone arch at its far end.
void paintArchCliff(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        int left = ox, right = ox + w;
        if (dy > 0 && dy < 4) { left = ox - 1; right = ox + w + 1; }
        if (dy >= 4) {
            left = ox + 2 + (int)(6.0f * std::sin(dy * 0.05f)) - (int)(dy * 0.15f) + (int)(hash01(seed + dy / 3) * 3.0f);
            right = ox + w + 2 + (int)(dy * 0.12f) - (int)(hash01(seed * 7 + dy / 3) * 3.0f);
        }
        m.span(y, left, right);
    }
    m.measure();
    Shading s = stoneShading(seed, oy + 26, 0.46f);
    shade(p, m, s);
    for (int i = 0; i < 9; i++) { // rock facets: short diagonal shadows
        int x = ox + (int)(hash01(seed * 13 + i) * w);
        int y = oy + 10 + (int)(hash01(seed * 17 + i) * 60.0f);
        for (int k = 0; k < 6; k++) if (m.at(x + k, y + k) && m.depth[(y + k) * p.width + x + k] > 1.5f) p.set(x + k, y + k, STONE[1]);
    }
    for (int i = 0; i < 70; i++) { // moss growing down the face
        int x = ox - 8 + (int)(hash01(seed * 19 + i) * (w + 16));
        int y = oy + 4 + (int)(hash01(seed * 23 + i) * hash01(seed * 29 + i) * 70.0f);
        if (m.at(x, y)) p.set(x, y, MOSS[1 + i % 2]);
    }
    paintStones(p, m, 8, ox, ox + w, oy + 20, p.height - 6, seed * 7);
    paintRoot(p, {ox + 12.0f, oy + 10.0f}, {ox - 4.0f, oy + 20.0f}, {ox - 8.0f, oy + 36.0f}, 2.5f, 1.0f, seed + 4);
    paintMossTop(p, m, 0, p.width, oy, seed, 5);
    paintFlowers(p, ox, ox + w - 30, oy, 6, seed + 9);
}

void paintForestLedge(const Node& node, LedgeCanvas& c) {
    int seed = node.id * 37 + 11;
    switch (node.id) {
        case 1: paintGroveEdge(c, seed); break;
        case 2: paintMossyRock(c, seed); break;
        case 3: paintFallenLog(c, seed); break;
        case 4: paintOldRuin(c, seed); break;
        default: paintArchCliff(c, seed); break;
    }
}

// ---------------------------------------------------------------------------
// Scenery
// ---------------------------------------------------------------------------

// A fern: fronds curving out from one root, each with small leaflets.
Picture paintFern(int size, int seed) {
    Picture p(size, size);
    Mask m(size, size);
    int fronds = 5 + (int)(hash01(seed) * 3.0f);
    for (int f = 0; f < fronds; f++) {
        float spread = (f - (fronds - 1) * 0.5f) / fronds; // -0.5 .. 0.5
        float length = size * (0.55f + 0.35f * hash01(seed * 3 + f));
        Vector2 previous = {size * 0.5f, (float)size};
        for (int k = 1; k <= 10; k++) {
            float t = k / 10.0f;
            Vector2 point = {size * 0.5f + spread * length * 1.6f * t + spread * length * 0.6f * t * t,
                             size - length * (t - 0.45f * t * t * (std::fabs(spread) * 2.0f))};
            m.capsule(previous, point, 1.2f, 0.6f);
            if (k % 2 == 0 && k < 10) { // leaflets on both sides
                m.capsule(point, Vector2{point.x - 3.0f, point.y - 1.5f}, 0.8f, 0.5f);
                m.capsule(point, Vector2{point.x + 3.0f, point.y - 1.5f}, 0.8f, 0.5f);
            }
            previous = point;
        }
    }
    m.measure();
    Shading s;
    s.ramp = FERN;
    s.steps = 4;
    s.toLight = TO_SUN;
    s.base = 0.35f;
    s.rim = 0.5f;
    s.bevel = 1.5f;
    s.grain = 0.05f;
    s.outline = false;
    s.seed = seed;
    shade(p, m, s);
    return p;
}

Picture paintCrown(float r, const Color* ramp, int seed, float fog, Color fogColor) {
    int size = (int)(r * 3.4f);
    Picture piece(size, size);
    float c = size * 0.5f;
    paintFoliage(piece, c - r * 0.72f, c + r * 0.3f, r * 0.62f, ramp, 5, TO_SUN, seed + 1);
    paintFoliage(piece, c + r * 0.74f, c + r * 0.26f, r * 0.6f, ramp, 5, TO_SUN, seed + 2);
    paintFoliage(piece, c, c, r, ramp, 5, TO_SUN, seed);
    paintFoliage(piece, c + r * 0.2f, c - r * 0.55f, r * 0.55f, ramp, 5, TO_SUN, seed + 3);
    if (fog > 0.0f) fogRows(piece, fogColor, 0, size, fog, fog);
    return piece;
}

void bakeForest(RegionArt& art, const World& world) {
    const RegionInfo& info = world.getRegion(0);
    const int range = scrollRangeOf(info);

    // Sky: deep teal overhead, warm haze low down where the sun is behind the trees.
    Picture sky(CANVAS_WIDTH, CANVAS_HEIGHT);
    ditherGradient(sky, 0, 130, SKY_TOP, SKY_MID);
    ditherGradient(sky, 130, 238, SKY_MID, SKY_GLOW);
    ditherGradient(sky, 238, CANVAS_HEIGHT, SKY_GLOW, SKY_LOW);
    storeLayer(art, sky, PARALLAX[SKY_LAYER], 0);

    Picture hills(layerWidth(range, PARALLAX[HILLS_LAYER]), CANVAS_HEIGHT);
    paintTreeline(hills, 34.0f, 216.0f, 20.0f, 36.0f, 207, HILLS, 4, 0.0f);
    fogRows(hills, Color{156, 172, 140, 255}, 150, 300, 0.3f, 0.55f);
    storeLayer(art, hills, PARALLAX[HILLS_LAYER], 0);

    Picture far(layerWidth(range, PARALLAX[FAR_LAYER]), CANVAS_HEIGHT);
    paintTreeline(far, 24.0f, 240.0f, 16.0f, 30.0f, 101, FAR, 4, 0.3f);
    fogRows(far, MIST, 210, 340, 0.12f, 0.5f);
    storeLayer(art, far, PARALLAX[FAR_LAYER], 0);

    // Middle trees: trunks in the layer, crowns as sway pieces, so gusts visibly roll through them.
    Picture mid(layerWidth(range, PARALLAX[MID_LAYER]), CANVAS_HEIGHT);
    const Color midFog = {74, 116, 102, 255};
    struct Crown { float x, y, r; int seed; };
    std::vector<Crown> crowns;
    for (int i = 0; i < 12; i++) {
        float x = 30.0f + i * 92.0f + hash01(i * 17 + 5) * 36.0f;
        if (x > mid.width + 20.0f) break;
        float crownY = 46.0f + hash01(i * 31 + 9) * 46.0f;
        float half = 3.0f + hash01(i * 23 + 7) * 2.5f;
        paintTrunk(mid, x, crownY, (float)CANVAS_HEIGHT, half, half + 4.0f, MID_BARK, 5, TO_SUN, 300 + i);
        crowns.push_back({x, crownY, 21.0f + hash01(i * 37 + 3) * 11.0f, 400 + i * 7});
    }
    fogRows(mid, midFog, 100, 340, 0.3f, 0.55f);
    storeLayer(art, mid, PARALLAX[MID_LAYER], 0);
    for (const Crown& c : crowns) {
        Picture piece = paintCrown(c.r, MID_LEAF, c.seed, 0.42f, midFog);
        storeSway(art, piece, MID_LAYER, false, c.x - piece.width * 0.5f, c.y - piece.height * 0.5f, 1.6f, false, 4,
                  hash01(c.seed) * 0.4f);
    }

    // The landmark: a giant old tree with a glowing hollow, its branches reaching off the top.
    Picture giant(layerWidth(range, PARALLAX[GIANT_LAYER]), CANVAS_HEIGHT);
    paintTrunk(giant, GIANT_X, -10.0f, (float)CANVAS_HEIGHT, 24.0f, 50.0f, GIANT_BARK, 6, TO_SUN, 77);
    Mask limbs(giant.width, CANVAS_HEIGHT);
    limbs.capsule({GIANT_X - 12.0f, 76.0f}, {GIANT_X - 124.0f, -24.0f}, 11.0f, 6.0f);
    limbs.capsule({GIANT_X + 14.0f, 60.0f}, {GIANT_X + 146.0f, -20.0f}, 10.0f, 5.0f);
    limbs.capsule({GIANT_X - 70.0f, 30.0f}, {GIANT_X - 160.0f, 26.0f}, 5.0f, 2.5f);
    limbs.capsule({GIANT_X - 34.0f, 300.0f}, {GIANT_X - 96.0f, 362.0f}, 12.0f, 5.0f); // roots
    limbs.capsule({GIANT_X + 36.0f, 296.0f}, {GIANT_X + 104.0f, 362.0f}, 12.0f, 5.0f);
    limbs.measure();
    Shading limbStyle;
    limbStyle.ramp = GIANT_BARK;
    limbStyle.steps = 6;
    limbStyle.toLight = TO_SUN;
    limbStyle.base = 0.36f;
    limbStyle.rim = 0.36f;
    limbStyle.bevel = 4.0f;
    limbStyle.seed = 78;
    shade(giant, limbs, limbStyle);
    for (int g = 0; g < 6; g++) { // bark grooves
        for (int y = 0; y < CANVAS_HEIGHT; y++) {
            int x = (int)(GIANT_X - 20.0f + g * 8.0f + std::sin(y * 0.05f + g * 1.7f) * 2.0f);
            if (giant.painted(x, y) && noise2(x * 0.3f, y * 0.08f, g + 90) > 0.35f) giant.set(x, y, GIANT_BARK[1]);
        }
    }
    const Color giantMoss = mixColor(GIANT_BARK[3], MOSS[1], 0.55f);
    for (int y = 40; y < CANVAS_HEIGHT; y++) { // long streaks of moss down the shaded side
        for (int x = (int)GIANT_X - 52; x < (int)GIANT_X - 6; x++) {
            if (giant.painted(x, y) && fbm2(x * 0.16f, y * 0.022f, 95, 3) > 0.64f) giant.set(x, y, giantMoss);
        }
    }
    for (int y = -12; y <= 12; y++) { // the hollow
        for (int x = -8; x <= 8; x++) {
            if ((x / 8.0f) * (x / 8.0f) + (y / 12.0f) * (y / 12.0f) > 1.0f) continue;
            giant.set((int)GIANT_X - 6 + x, 168 + y, (x > 5) ? GIANT_BARK[4] : GIANT_BARK[0]);
        }
    }
    fogRows(giant, midFog, 140, 360, 0.15f, 0.42f);
    storeLayer(art, giant, PARALLAX[GIANT_LAYER], 0);
    const float giantCrowns[4][3] = {{GIANT_X - 130.0f, -6.0f, 34.0f}, {GIANT_X - 30.0f, -14.0f, 38.0f},
                                     {GIANT_X + 80.0f, -10.0f, 36.0f}, {GIANT_X + 170.0f, -2.0f, 28.0f}};
    for (int i = 0; i < 4; i++) {
        Picture piece = paintCrown(giantCrowns[i][2], GIANT_LEAF, 500 + i * 9, 0.12f, midFog);
        storeSway(art, piece, GIANT_LAYER, false, giantCrowns[i][0] - piece.width * 0.5f,
                  giantCrowns[i][1] - piece.height * 0.5f, 2.0f, true, 3, 0.15f * i);
    }

    // Near trunks, just behind the route, for depth.
    Picture nearTrees(layerWidth(range, PARALLAX[NEAR_LAYER]), CANVAS_HEIGHT);
    const float nearX[3] = {36.0f, 478.0f, 808.0f};
    for (int i = 0; i < 3; i++) {
        paintTrunk(nearTrees, nearX[i], -10.0f, (float)CANVAS_HEIGHT, 9.0f, 17.0f, NEAR_BARK, 5, TO_SUN, 600 + i);
        for (int k = 0; k < 4; k++) {
            paintHangingMoss(nearTrees, (int)nearX[i] - 6 + k * 4, 30 + k * 23, 8 + k * 3, 610 + i * 5 + k);
        }
    }
    storeLayer(art, nearTrees, PARALLAX[NEAR_LAYER], 0);

    // The canopy along the top: dark leaf masses that hang and sway.
    storeEmptyLayer(art, PARALLAX[CANOPY_LAYER]);
    int canopyWidth = layerWidth(range, PARALLAX[CANOPY_LAYER]);
    for (int i = 0; i * 56 < canopyWidth + 40; i++) {
        float r = 22.0f + hash01(i * 29 + 3) * 12.0f;
        float x = i * 56.0f - 24.0f + hash01(i * 13 + 1) * 18.0f;
        float y = -10.0f + hash01(i * 19 + 2) * 12.0f;
        Picture piece = paintCrown(r, CANOPY, 700 + i * 11, 0.0f, DARK);
        storeSway(art, piece, CANOPY_LAYER, false, x - piece.width * 0.5f, y - piece.height * 0.5f, 2.2f, true, 3,
                  hash01(i * 5) * 0.3f);
    }

    // Platforms: walk the real list from its head and paint each node's ledge.
    const Node* head = world.getHead(0);
    const Node* node = head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        LedgeCanvas canvas = makeLedgeCanvas(*node, 30, 12);
        paintForestLedge(*node, canvas);
        storeLedge(art, *node, canvas);
        node = node->next;
    }

    // Foreground: undergrowth along the bottom and two dark trunks framing the way.
    Picture front(layerWidth(range, FRONT_PARALLAX), CANVAS_HEIGHT);
    for (int x = 0; x < front.width; x++) {
        int height = 8 + (int)(fbm2(x * 0.03f, 0.0f, 900, 3) * 18.0f);
        for (int y = CANVAS_HEIGHT - height; y < CANVAS_HEIGHT; y++) front.set(x, y, FERN[(y == CANVAS_HEIGHT - height) ? 2 : 0]);
    }
    const float trunkX[2] = {6.0f, (float)front.width - 40.0f};
    for (int i = 0; i < 2; i++) paintTrunk(front, trunkX[i], -10.0f, (float)CANVAS_HEIGHT, 14.0f, 20.0f, FERN, 4, TO_SUN, 950 + i);
    storeLayer(art, front, FRONT_PARALLAX, 0, true);
    for (int i = 0; i * 64 < front.width; i++) {
        int size = 40 + (int)(hash01(i * 7 + 1) * 26.0f);
        Picture fern = paintFern(size, 960 + i * 5);
        float x = i * 64.0f + hash01(i * 3 + 2) * 30.0f - size * 0.5f;
        storeSway(art, fern, 0, true, x, (float)(CANVAS_HEIGHT - size + 4), 2.6f, false, 5, hash01(i * 11) * 0.3f);
    }
}

// ---------------------------------------------------------------------------
// Live pieces
// ---------------------------------------------------------------------------

// The old stone arch around the exit. Leaves block it until the path's end is reached; then
// they fall away while warm light fills the arch (open: 0 closed -> 1 open).
void drawExitArch(Rectangle gate, int cameraX, float open, float time) {
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    const int bottom = gy + gh;
    const int radius = gw / 2;
    const int pillarTop = gy + radius;
    const Vector2 center = {(float)(gx + radius), (float)pillarTop};
    const float light = smooth01(open);

    if (light > 0.0f) {
        DrawCircleSector(center, (float)radius, 180.0f, 360.0f, 16, withAlpha(SUN, 0.92f * light));
        DrawRectangleGradientV(gx, pillarTop, gw, bottom - pillarTop, withAlpha(SUN, 0.92f * light), withAlpha(MOSS[4], 0.7f * light));
        for (int i = 0; i < 6; i++) {
            float t = std::fmod(time * (0.35f + 0.1f * hash01(i * 3)) + i * 0.17f, 1.0f);
            int x = gx + 2 + (int)(hash01(i * 5 + 9) * (gw - 4));
            DrawRectangle(x, bottom - 2 - (int)(t * gh), 1, 1, withAlpha(WHITE, light * loopEnvelope(t)));
        }
    }
    if (open < 1.0f) { // dark leaves fill the arch, then drop and fade as it opens
        const Color shade = {16, 30, 28, 255};
        float cover = 1.0f - light;
        DrawCircleSector(center, (float)radius, 180.0f, 360.0f, 16, withAlpha(shade, cover));
        DrawRectangle(gx, pillarTop, gw, bottom - pillarTop, withAlpha(shade, cover));
        for (int i = 0; i < 30; i++) {
            int x = gx + (int)(hash01(i * 7 + 1) * (gw - 1));
            int y = gy + 1 + (int)(hash01(i * 11 + 3) * (gh - 2));
            float dx = x - center.x, dy = y - center.y;
            if (y < pillarTop && dx * dx + dy * dy > radius * radius) continue;
            float fall = open * open * (14.0f + hash01(i * 13 + 5) * 22.0f);
            int drift = (int)std::lround(std::sin(open * 6.0f + i) * open * 4.0f);
            float alpha = std::fmax(0.0f, 1.0f - open * (1.2f + hash01(i * 3 + 2)));
            DrawRectangle(x + drift, y + (int)fall, 2, 1, withAlpha((i % 3 == 0) ? MOSS[2] : MOSS[1], alpha));
        }
    }
    DrawRectangle(gx - 5, pillarTop, 5, bottom - pillarTop, STONE[3]); // pillars, lit on the right
    DrawRectangle(gx + gw, pillarTop, 5, bottom - pillarTop, STONE[3]);
    DrawRectangle(gx - 5, pillarTop, 1, bottom - pillarTop, STONE[1]);
    DrawRectangle(gx + gw + 4, pillarTop, 1, bottom - pillarTop, STONE[5]);
    DrawRing(center, (float)radius, radius + 5.0f, 180.0f, 360.0f, 20, STONE[3]);
    DrawRing(center, radius + 4.0f, radius + 5.0f, 270.0f, 360.0f, 10, STONE[5]);
    DrawRectangle((int)center.x - 2, gy - 6, 5, 6, STONE[4]); // keystone
    DrawRectangle(gx - 6, bottom - 3, 7, 3, STONE[1]);
    DrawRectangle(gx + gw - 1, bottom - 3, 7, 3, STONE[1]);
    Color rune = lerpColor(STONE[1], WARM_LIGHT, smooth01(open * 1.6f - 0.3f));
    for (int k = 0; k < 3; k++) {
        DrawRectangle(gx - 3, pillarTop + 4 + k * 6, 1, 3, rune);
        DrawRectangle(gx + gw + 2, pillarTop + 4 + k * 6, 1, 3, rune);
    }
    for (int i = 0; i < 6; i++) { // moss and a few leaves on top of the arch
        DrawRectangle((int)center.x - 10 + i * 4, gy - 5 + (i == 0 || i == 5 ? 2 : 0), 3, 1, MOSS[3]);
    }
}

float ledgeMist(const SceneView& view, const Node& node) {
    if (view.mistById == nullptr || node.id < 0 || node.id > MAX_NODES_PER_REGION) return 0.0f;
    return view.mistById[node.id];
}

// ---------------------------------------------------------------------------
// Region 1 drawing
// ---------------------------------------------------------------------------

void drawForestBackground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    drawBackLayers(view, art, SKY_LAYER, MID_LAYER, 0.8f);
    for (int i = 0; i < 4; i++) { // haze drifting between the far and the middle trees
        float x = std::fmod(i * 260.0f + time * 3.0f - view.scroll * 0.2f + 4000.0f, 1040.0f) - 200.0f;
        DrawEllipse((int)x, 238 + (i % 2) * 14, 150.0f, 14.0f, withAlpha(MIST, 0.07f));
    }
    drawBackLayers(view, art, MID_LAYER, NEAR_LAYER, 1.0f);
    DrawRectangleGradientV(0, 200, CANVAS_WIDTH, 100, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.16f));
    DrawRectangle(0, 300, CANVAS_WIDTH, CANVAS_HEIGHT - 300, withAlpha(MIST, 0.16f));
    drawBackLayers(view, art, NEAR_LAYER, LAYER_COUNT, 1.1f);

    // Vines hanging from the canopy, swaying with the gusts that move the leaves.
    for (int i = 0; i < 18; i++) {
        float lx = i * 56.0f - 10.0f + hash01(i * 13 + 1) * 18.0f;
        int x = (int)layerToScreen(lx, view.scroll, PARALLAX[CANOPY_LAYER]);
        if (x < -10 || x > CANVAS_WIDTH + 10 || i % 3 == 1) continue;
        int length = 24 + (int)(hash01(i * 47 + 5) * 54.0f);
        float lean = windAt(lx, time - 0.2f, 1.1f) * 4.0f;
        drawStrand(x, 14 + (int)(hash01(i * 19 + 2) * 10.0f), length, CANOPY[3], MOSS[1], lean, i);
    }
}

void drawForestPlatforms(const SceneView& view, const RegionArt& art) {
    drawLedges(view, art, VEIL);

    const Node* next = (view.current != nullptr) ? view.current->next : nullptr; // checked before use
    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            float mist = ledgeMist(view, *node);
            Color tip = lerpColor(MOSS[5], VEIL, mist * 0.7f);
            Color stem = lerpColor(MOSS[3], VEIL, mist * 0.7f);
            if (node->id != 3 && node->id != 4) drawGrass(ledge, view, tip, stem, 1.0f, node->id == 2 ? 0.25f : 0.45f);
            if (node->id == 3) { // moss strands swinging under the log
                for (int i = 0; i < 4; i++) {
                    int x = ledge.left + 14 + i * (ledge.width - 24) / 3;
                    float lean = windAt((float)(x + view.cameraX), view.time, 1.0f) * 3.0f;
                    drawStrand(x, ledge.top + 18, 8 + (int)(hash01(i * 7 + 3) * 12.0f), lerpColor(MOSS[1], VEIL, mist), MOSS[2], lean, i);
                }
            }
            if (node == next) drawLinkLip(ledge, WARM_LIGHT, arrivalEase(view), view.time, true);
        }
        node = node->next;
        if (node == view.head) break;
    }

    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        drawLanternPost(ledge.left + 6, ledge.top, view.checkpointFlash, view.time);
    }
    if (view.hasGate) drawExitArch(view.gateRect, view.cameraX, view.gateProgress, view.time);
}

void drawForestForeground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    const int scroll = view.scroll;

    for (int i = 0; i < 6; i++) { // mist drifting low through the forest floor
        float x = std::fmod(i * 170.0f + time * 5.0f - scroll * 1.05f + 4000.0f, 1020.0f) - 190.0f;
        DrawEllipse((int)x, 300 + (i % 3) * 14, 100.0f + (i % 2) * 40.0f, 10.0f, withAlpha(MIST, 0.1f));
    }

    for (int i = 0; i < 16; i++) { // leaves drifting down through the light, tumbling as they fall
        float cycle = time * (0.045f + 0.03f * hash01(i * 3 + 1)) + hash01(i * 7 + 2);
        float t = cycle - std::floor(cycle);
        int pass = (int)std::floor(cycle);
        float x = std::fmod(hash01(i * 11 + pass * 5) * 760.0f + t * 60.0f - scroll * 1.0f + 7600.0f, 760.0f) - 60.0f;
        x += std::sin(time * (1.1f + 0.4f * hash01(i)) + i) * 12.0f + windAt(x, time, 1.0f) * 6.0f;
        float y = -10.0f + t * 330.0f;
        bool flat = std::cos(time * 3.6f + i * 1.7f) > 0.0f;
        Color leaf = (i % 3 == 0) ? MOSS[4] : ((i % 3 == 1) ? MOSS[3] : lerpColor(MOSS[4], SUN, 0.5f));
        DrawRectangle((int)x, (int)y, flat ? 2 : 1, flat ? 1 : 2, withAlpha(leaf, 0.9f * loopEnvelope(t)));
    }

    for (int i = 0; i < 22; i++) { // fireflies, low among the roots and ferns
        float pace = 0.7f + 0.6f * hash01(i * 3 + 1);
        float x = std::fmod(hash01(i * 11 + 4) * 1100.0f - scroll * 0.95f + 4400.0f, 1100.0f) - 100.0f;
        x += std::sin(time * 0.5f * pace + i * 1.3f) * 12.0f;
        float y = 150.0f + hash01(i * 7 + 2) * 160.0f + std::sin(time * 0.7f * pace + i) * 9.0f;
        float twinkle = 0.5f + 0.5f * std::sin(time * 2.3f * pace + i * 2.1f);
        DrawRectangle((int)x, (int)y, 1, 1, withAlpha(WARM_LIGHT, 0.35f + 0.65f * twinkle));
    }

    drawFrontLayers(view, art, 1.2f);
    DrawRectangleGradientV(0, 318, CANVAS_WIDTH, 42, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.7f));
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 40, withAlpha(DARK, 0.45f), withAlpha(DARK, 0.0f));
}

void drawForestAdditive(const SceneView& view, const RegionArt&, const SceneRenderer& renderer) {
    const float time = view.time;
    const float sunX = 440.0f - view.scroll * 0.05f;
    renderer.drawSoftLight(Vector2{sunX, 140.0f}, 250.0f, SUN, 0.15f); // the sun behind the trees
    renderer.drawSoftLight(Vector2{sunX, 140.0f}, 90.0f, SUN, 0.2f);

    // Shafts of light slanting down through gaps in the canopy; they flicker as the leaves move.
    for (int i = 0; i < 4; i++) {
        float top = 330.0f + i * 84.0f - view.scroll * 0.25f;
        float width = 10.0f + 8.0f * hash01(i * 5 + 1);
        float slant = -120.0f;
        float leaves = 0.7f + 0.3f * std::sin(time * 0.6f + i * 1.7f) - 0.25f * windAt(top, time, 1.0f);
        Color topColor = withAlpha(SUN, std::fmax(0.0f, 0.075f * leaves));
        Color clear = withAlpha(SUN, 0.0f);
        drawGradientQuad(Vector2{top, 20.0f}, Vector2{top + width, 20.0f}, Vector2{top + width + slant, 300.0f},
                         Vector2{top + slant, 300.0f}, topColor, topColor, clear, clear);
        for (int k = 0; k < 6; k++) { // dust motes turning in the light
            float t = std::fmod(time * 0.05f * (1.0f + hash01(i * 9 + k)) + hash01(i * 13 + k), 1.0f);
            float y = 30.0f + t * 240.0f;
            float x = top + slant * ((y - 20.0f) / 280.0f) + width * hash01(i * 17 + k) + std::sin(time + k) * 3.0f;
            DrawRectangle((int)x, (int)y, 1, 1, withAlpha(SUN, 0.5f * loopEnvelope(t) * leaves));
        }
    }

    for (int i = 0; i < 22; i++) { // the fireflies' glow
        float pace = 0.7f + 0.6f * hash01(i * 3 + 1);
        float x = std::fmod(hash01(i * 11 + 4) * 1100.0f - view.scroll * 0.95f + 4400.0f, 1100.0f) - 100.0f;
        x += std::sin(time * 0.5f * pace + i * 1.3f) * 12.0f;
        float y = 150.0f + hash01(i * 7 + 2) * 160.0f + std::sin(time * 0.7f * pace + i) * 9.0f;
        float twinkle = 0.5f + 0.5f * std::sin(time * 2.3f * pace + i * 2.1f);
        renderer.drawSoftLight(Vector2{x + 0.5f, y + 0.5f}, 6.0f, WARM_LIGHT, 0.35f * twinkle);
    }
}

void addForestLights(const SceneView& view, const RegionArt&, std::vector<Light>& lights) {
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;
    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            if (node == next) addLinkLight(lights, ledge, WARM_LIGHT, arrivalEase(view));
            if (node->id == 3) { // the mushrooms on the log glow softly
                Light glow = {Vector2{ledge.left + 34.0f, ledge.top + 12.0f}, 26.0f, Color{255, 170, 110, 255}, 0.3f, 6.0f, 0.25f};
                lights.push_back(glow);
            }
        }
        node = node->next;
        if (node == view.head) break;
    }
    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        addLanternLight(lights, ledge.left + 6, ledge.top, view.checkpointFlash, view.time);
    }
    float hollowX = layerToScreen(GIANT_X - 6.0f, view.scroll, PARALLAX[GIANT_LAYER]);
    lights.push_back(Light{Vector2{hollowX, 168.0f}, 30.0f, WARM_LIGHT, 0.22f + 0.06f * std::sin(view.time * 1.5f), 8.0f, 0.18f});
    if (view.hasGate && view.gateProgress > 0.0f) {
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height / 2.0f);
        float light = smooth01(view.gateProgress);
        lights.push_back(Light{Vector2{x, y}, 80.0f * light, SUN, 0.6f * light, 26.0f * light, 0.45f * light});
    }
}

Mood forestMood(const SceneView&) {
    Mood mood;
    mood.skyLight = WHITE;
    mood.groundLight = Color{196, 202, 184, 255};
    mood.lightFrom = 110;
    mood.lightTo = 330;
    mood.shadowTint = Vector3{0.9f, 0.97f, 1.03f};
    mood.highlightTint = Vector3{1.08f, 1.03f, 0.9f};
    mood.saturation = 1.06f;
    mood.bloom = 0.6f;
    mood.vignette = 0.38f;
    return mood;
}

} // namespace

const RegionPainter FOREST_PAINTER = {bakeForest, drawForestBackground, drawForestPlatforms, nullptr,
                                      drawForestForeground, drawForestAdditive, addForestLights, forestMood};
