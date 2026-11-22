#include "ArtCommon.h"

#include <algorithm>

// Region 2, "The Forgotten Return": flooded ruins under a full moon. The path dips to a hall by
// the water, then climbs a ruined tower to the key's shrine; the way back down is the lesson.
// Still water reflects everything above it.

namespace {

// Night blues, moonlight from the upper left, warm fire in the braziers.
const Color SKY_TOP = {8, 12, 30, 255};
const Color SKY_MID = {22, 36, 72, 255};
const Color SKY_LOW = {64, 86, 126, 255};
const Color MOON = {232, 238, 255, 255};
const Color MIST = {140, 162, 198, 255};
const Color VEIL = {86, 102, 138, 255};
const Color DARK = {5, 8, 18, 255};
const Color WATER_TOP = {24, 44, 76, 255};
const Color WATER_DEEP = {6, 12, 28, 255};

const Color FAR_CITY[4] = {{30, 42, 72, 255}, {38, 52, 84, 255}, {50, 66, 98, 255}, {74, 92, 124, 255}};
const Color MID_STONE[5] = {{16, 24, 44, 255}, {24, 36, 60, 255}, {36, 50, 78, 255}, {54, 70, 100, 255}, {82, 100, 132, 255}};
const Color NEAR_STONE[4] = {{8, 12, 24, 255}, {14, 20, 36, 255}, {22, 30, 50, 255}, {36, 48, 72, 255}};
const Color STONE[6] = {{24, 28, 44, 255}, {46, 52, 72, 255}, {76, 84, 106, 255},
                        {114, 122, 144, 255}, {158, 166, 184, 255}, {204, 210, 224, 255}};
const Color MOSS[5] = {{22, 44, 46, 255}, {40, 74, 66, 255}, {70, 112, 90, 255}, {112, 156, 120, 255}, {162, 202, 150, 255}};
const Color ROOT = {58, 46, 44, 255};
const Color BANNER = {148, 54, 62, 255};
const Color BANNER_DARK = {98, 34, 46, 255};
const Color RELIC_GOLD = {214, 176, 96, 255};
const Color FIRE = {255, 168, 76, 255};
const Color FIRE_HOT = {255, 232, 150, 255};

const Vector2 TO_MOON = {-0.55f, -0.83f};
const int WATERLINE = 312;   // canvas y of the water surface
const float MOON_X = 236.0f; // in the sky layer (which does not scroll), clear of the columns at the start
const float MOON_Y = 62.0f;

enum Layer { SKY_LAYER, FAR_LAYER, MID_LAYER, NEAR_LAYER, HANGING_LAYER, LAYER_COUNT };
const float PARALLAX[LAYER_COUNT] = {0.0f, 0.1f, 0.35f, 0.72f, 0.9f};
const float FRONT_PARALLAX = 1.3f;

// The middle-distance arches. Bake and draw both use this, so the braziers sit on the right pillars.
struct Arch {
    float x;
    int radius;
    int springY;
    bool broken;
};
Arch archAt(int i) {
    Arch a;
    a.x = 80.0f + i * 168.0f + hash01(i * 19 + 4) * 30.0f;
    a.radius = 30 + (int)(hash01(i * 29 + 6) * 12.0f);
    a.springY = 128 + (int)(hash01(i * 31 + 8) * 34.0f);
    a.broken = hash01(i * 37 + 2) > 0.5f;
    return a;
}
const int ARCH_COUNT = 5;
const int PILLAR = 14;

Shading stoneShading(int seed, int darkenFrom) {
    Shading s;
    s.ramp = STONE;
    s.steps = 6;
    s.toLight = TO_MOON;
    s.base = 0.5f;
    s.rim = 0.32f;
    s.bevel = 2.5f;
    s.grain = 0.1f;
    s.grainScale = 0.28f;
    s.seed = seed;
    s.darkenFrom = (float)darkenFrom;
    s.darkenRange = 70.0f;
    s.darkenAmount = 0.35f;
    return s;
}

// ---------------------------------------------------------------------------
// Painting helpers
// ---------------------------------------------------------------------------

// Mortar lines with the joints offset every course, and a lit top edge on each block.
void paintMasonry(Picture& p, const Mask& m, int fromY, int toY, int course, int block, int seed) {
    for (int y = fromY, row = 0; y < toY; y += course, row++) {
        for (int x = 0; x < p.width; x++) {
            if (!m.at(x, y) || m.depth[y * p.width + x] <= 1.0f) continue;
            p.set(x, y, STONE[1]);
            if (m.at(x, y + 1) && m.depth[(y + 1) * p.width + x] > 1.0f) p.set(x, y + 1, scaleColor(p.get(x, y + 1), 1.12f));
        }
        int offset = (row % 2) * (block / 2) + (int)(hash01(seed + row) * 4.0f);
        for (int x = offset; x < p.width; x += block) {
            for (int k = 1; k < course; k++) {
                if (m.at(x, y + k) && m.depth[(y + k) * p.width + x] > 1.0f) p.set(x, y + k, STONE[1]);
            }
        }
    }
}

void paintTopMoss(Picture& p, const Mask& m, int x0, int x1, int top, int seed) {
    for (int i = 0; i < (x1 - x0) / 7; i++) {
        int x = x0 + (int)(hash01(seed * 43 + i) * (x1 - x0 - 4));
        int width = 2 + (int)(hash01(seed * 47 + i) * 6.0f);
        for (int k = 0; k < width; k++) {
            if (!m.at(x + k, top)) continue;
            p.set(x + k, top, MOSS[4]);
            p.set(x + k, top + 1, MOSS[2]);
            if ((k + i) % 3 == 0) {
                int drip = 1 + (int)(hash01(seed + i * 7 + k) * 5.0f);
                for (int d = 2; d < 2 + drip; d++) if (m.at(x + k, top + d)) p.set(x + k, top + d, MOSS[1]);
            }
        }
    }
}

void paintRoots(Picture& p, int x, int y, int length, int seed) {
    for (int k = 0; k < length; k++) {
        int wobble = (int)std::lround(std::sin(k * 0.3f + seed) * 1.2f) + (k / 9) * ((seed % 2) ? 1 : -1);
        p.set(x + wobble, y + k, ROOT);
        if (k % 5 == 3) p.set(x + wobble + 1, y + k, MOSS[1]);
    }
}

// Darker, greener stone where the water has stood.
void paintWaterStain(Picture& p, int pictureWaterline) {
    for (int y = std::max(0, pictureWaterline - 18); y < p.height; y++) {
        float t = std::clamp((y - (pictureWaterline - 18)) / 18.0f, 0.0f, 1.0f);
        for (int x = 0; x < p.width; x++) {
            Color c = p.get(x, y);
            if (c.a == 0) continue;
            Color stained = mixColor(c, MOSS[0], 0.45f * t);
            stained.a = c.a;
            p.set(x, y, stained);
        }
    }
}

// ---------------------------------------------------------------------------
// Platforms
// ---------------------------------------------------------------------------

// N1: a gatehouse of big cut blocks, with a dark arched window.
void paintGatehouse(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        int left = ox + 1, right = ox + w - 1;
        if (dy == 0) { left = ox; right = ox + w; }
        else if (dy < 4) { left = ox - 1; right = ox + w + 1; }
        else if (dy > 44) { left -= (dy - 44) / 5; right += (dy - 44) / 5; } // the foundation spreads into the water
        m.span(y, left, right);
    }
    m.measure();
    shade(p, m, stoneShading(seed, oy + 30));
    paintMasonry(p, m, oy + 5, p.height, 9, 16, seed);
    for (int x = ox; x < ox + w; x++) p.set(x, oy + 1, STONE[5]);
    for (int x = ox + 2; x < ox + w - 2; x++) { // a carved band
        p.set(x, oy + 14, STONE[4]);
        if (x % 5 == 0) p.set(x, oy + 15, STONE[1]);
    }
    int cx = ox + w / 2;
    for (int y = -6; y < 16; y++) { // the arched window
        for (int x = -6; x <= 6; x++) {
            if (y < 0 && x * x + y * y > 36) continue;
            p.set(cx + x, oy + 26 + y, (x == -6 || y == -6) ? STONE[4] : STONE[0]);
        }
    }
    paintTopMoss(p, m, ox, ox + w, oy, seed);
    paintRoots(p, ox + w - 4, oy + 2, 26, seed);
    paintRoots(p, ox + 8, oy + 3, 16, seed + 3);
    paintWaterStain(p, WATERLINE - c.top + oy);
}

// N2: the floor of an old hall, half sunk, on an arch that the water runs through.
void paintOldHall(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const int pier = 24;
    const int deck = 8;
    const float archCx = ox + w * 0.5f;
    const float archR = w * 0.5f - pier;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        if (dy == 0) { m.span(y, ox, ox + w); continue; }
        if (dy < deck) { m.span(y, ox - 1, ox + w + 1); continue; }
        float ay = (float)(dy - deck);
        if (ay < archR) { // the arch's underside curves up between the piers
            float half = std::sqrt(std::fmax(0.0f, archR * archR - (archR - ay) * (archR - ay)));
            m.span(y, ox, (int)(archCx - half));
            m.span(y, (int)(archCx + half), ox + w);
        } else {
            m.span(y, ox, ox + pier);
            m.span(y, ox + w - pier, ox + w);
        }
    }
    m.measure();
    shade(p, m, stoneShading(seed, oy + 24));
    paintMasonry(p, m, oy + 12, p.height, 8, 12, seed);
    for (int i = 0; i <= 8; i++) { // voussoirs around the arch
        float angle = 3.14159f + i * 3.14159f / 8.0f;
        int x = (int)(archCx + std::cos(angle) * (archR + 3.0f));
        int y = (int)(oy + deck + archR + std::sin(angle) * (archR + 3.0f));
        p.set(x, y, STONE[1]);
        p.set(x, y + 1, STONE[1]);
    }
    for (int x = ox - 1; x < ox + w + 1; x++) p.set(x, oy + 1, STONE[5]);
    paintTopMoss(p, m, ox, ox + w, oy, seed);
    paintRoots(p, (int)archCx + 6, oy + deck, 14, seed);
    paintWaterStain(p, WATERLINE - c.top + oy);
}

// N3: the capital of a great column rising out of the water.
void paintColonnade(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const int cx = ox + w / 2;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        if (dy == 0) m.span(y, ox, ox + w);
        else if (dy < 5) m.span(y, ox - 1, ox + w + 1);                     // the slab
        else if (dy < 11) m.span(y, ox + (dy - 4) * 3, ox + w - (dy - 4) * 3); // the capital narrows
        else {
            int half = 13 + (int)(1.5f * std::sin(dy * 0.03f));             // the shaft, with a slight swell
            m.span(y, cx - half, cx + half);
        }
    }
    m.measure();
    shade(p, m, stoneShading(seed, oy + 20));
    for (int x = ox - 1; x < ox + w + 1; x++) p.set(x, oy + 1, STONE[5]);
    for (int y = oy + 12; y < p.height; y++) { // fluting
        for (int f = -10; f <= 10; f += 5) {
            p.set(cx + f, y, STONE[(f > 0) ? 4 : 1]);
        }
    }
    for (int side = 0; side < 2; side++) { // scroll carvings at the corners
        int vx = side == 0 ? ox + 5 : ox + w - 6;
        for (int k = 0; k < 8; k++) {
            float a = k * 0.785f;
            p.set(vx + (int)std::lround(std::cos(a) * 2.0f), oy + 7 + (int)std::lround(std::sin(a) * 2.0f), STONE[1]);
        }
    }
    for (int k = 0; k < 14; k++) p.set(cx + 4 + (k % 2) - k / 5, oy + 22 + k, STONE[0]); // a crack
    paintTopMoss(p, m, ox, ox + w, oy, seed);
    paintRoots(p, ox + 10, oy + 5, 18, seed + 2);
    paintWaterStain(p, WATERLINE - c.top + oy);
}

// N4: the key's shrine, on top of a ruined tower.
void paintKeyShrine(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const int cx = ox + w / 2;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        if (dy == 0) m.span(y, ox, ox + w);
        else if (dy < 4) m.span(y, ox - 2, ox + w + 2); // cornice
        else {
            int inset = 3 - std::max(0, (dy - 60) / 8); // the tower widens toward the water
            int notch = (hash01(seed + dy / 6) > 0.8f) ? 2 : 0;
            m.span(y, ox + inset + notch, ox + w - inset);
        }
    }
    m.measure();
    shade(p, m, stoneShading(seed, oy + 40));
    paintMasonry(p, m, oy + 8, p.height, 9, 14, seed);
    for (int x = ox - 2; x < ox + w + 2; x++) p.set(x, oy + 1, STONE[5]);
    for (int x = ox + 6; x < ox + w - 6; x += 6) { // a faded gold band
        p.rect(x, oy + 5, 3, 1, RELIC_GOLD);
    }
    for (int y = -5; y < 12; y++) { // the empty niche where the key was kept
        for (int x = -5; x <= 5; x++) {
            if (y < 0 && x * x + y * y > 25) continue;
            p.set(cx + x, oy + 16 + y, STONE[0]);
        }
    }
    for (int i = 0; i < 2; i++) { // window slits
        int x = ox + 14 + i * (w - 30);
        p.rect(x, oy + 44, 3, 10, STONE[0]);
        p.set(x, oy + 43, STONE[1]);
    }
    paintTopMoss(p, m, ox, ox + w, oy, seed);
    paintRoots(p, ox + 4, oy + 4, 30, seed);
    paintRoots(p, ox + w - 6, oy + 4, 22, seed + 5);
    paintWaterStain(p, WATERLINE - c.top + oy);
}

// N5: a broad landing whose steps go down into the water, with the old doorway at its far end.
void paintFarSteps(LedgeCanvas& c, int seed) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        int left = ox;
        if (dy > 0 && dy < 3) left = ox - 1;
        if (dy >= 4) left = ox - std::min(5, (dy - 4) / 6 + 1) * 5; // stairs stepping down to the left
        m.span(y, left, ox + w + ((dy > 0) ? 1 : 0));
    }
    m.measure();
    shade(p, m, stoneShading(seed, oy + 30));
    for (int step = 0; step < 6; step++) { // each step's lit tread
        int y = oy + 4 + step * 6;
        int x = ox - (step + 1) * 5;
        for (int k = 0; k < 6 && step < 5; k++) p.set(x + k, y, STONE[5]);
    }
    paintMasonry(p, m, oy + 6, p.height, 8, 15, seed);
    for (int x = ox; x < ox + w; x++) p.set(x, oy + 1, STONE[5]);
    paintTopMoss(p, m, ox, ox + w - 30, oy, seed);
    paintRoots(p, ox + w - 40, oy + 3, 20, seed);
    paintWaterStain(p, WATERLINE - c.top + oy);
}

void paintRuinsLedge(const Node& node, LedgeCanvas& c) {
    int seed = node.id * 37 + 11;
    switch (node.id) {
        case 1: paintGatehouse(c, seed); break;
        case 2: paintOldHall(c, seed); break;
        case 3: paintColonnade(c, seed); break;
        case 4: paintKeyShrine(c, seed); break;
        default: paintFarSteps(c, seed); break;
    }
}

// ---------------------------------------------------------------------------
// Scenery
// ---------------------------------------------------------------------------

Picture paintBanner(int width, int length, int seed) {
    Picture p(width + 2, length + 2);
    p.rect(0, 0, width + 2, 1, RELIC_GOLD); // the rod
    for (int y = 1; y <= length; y++) {
        int cut = (y > length - 4) ? (y - (length - 4)) * 2 : 0; // a tattered point
        for (int x = 1 + cut / 2; x < width + 1 - cut / 2; x++) {
            bool stripe = (y % 7 == 3);
            Color c = (x == 1) ? BANNER_DARK : (stripe ? RELIC_GOLD : BANNER);
            if (hash01(seed * 13 + x * 31 + y) < 0.06f) continue; // moth holes
            p.set(x, y, mixColor(c, MID_STONE[1], 0.25f));
        }
    }
    return p;
}

Picture paintHangingRoots(int count, int length, int seed) {
    Picture p(count * 6 + 4, length + 2);
    for (int i = 0; i < count; i++) {
        int len = length / 2 + (int)(hash01(seed + i * 5) * length / 2);
        for (int k = 0; k < len; k++) {
            int wobble = (int)std::lround(std::sin(k * 0.25f + i * 1.3f) * 1.0f);
            p.set(2 + i * 6 + wobble, k, NEAR_STONE[0]);
            if (k < len / 2) p.set(3 + i * 6 + wobble, k, NEAR_STONE[0]);
            if (k % 6 == 4) p.set(3 + i * 6 + wobble, k, NEAR_STONE[2]);
        }
    }
    return p;
}

Picture paintReeds(int width, int height, int seed) {
    Picture p(width, height);
    for (int i = 0; i < width / 3; i++) {
        int x = 1 + i * 3 + (int)(hash01(seed + i) * 2.0f);
        int h = height / 2 + (int)(hash01(seed * 3 + i) * height / 2);
        for (int k = 0; k < h; k++) {
            int lean = (int)std::lround((k / (float)height) * (k / (float)height) * (hash01(seed + i * 7) * 4.0f - 2.0f));
            p.set(x + lean, height - 1 - k, DARK);
        }
        if (i % 3 == 0) p.rect(x - 1 + (int)(hash01(seed + i * 7) * 4.0f - 2.0f), height - h - 3, 3, 4, DARK); // a reed head
    }
    return p;
}

void bakeRuins(RegionArt& art, const World& world) {
    const RegionInfo& info = world.getRegion(1);
    const int range = scrollRangeOf(info);
    art.waterline = WATERLINE;

    Picture sky(CANVAS_WIDTH, CANVAS_HEIGHT);
    ditherGradient(sky, 0, 150, SKY_TOP, SKY_MID);
    ditherGradient(sky, 150, WATERLINE, SKY_MID, SKY_LOW);
    for (int i = 0; i < 90; i++) { // stars, fewer near the moon
        int x = (int)(hash01(i * 7 + 1) * CANVAS_WIDTH);
        int y = (int)(hash01(i * 13 + 2) * 170.0f);
        float dx = x - MOON_X, dy = y - MOON_Y;
        if (dx * dx + dy * dy < 70.0f * 70.0f) continue;
        sky.set(x, y, mixColor(SKY_MID, MOON, 0.35f + 0.6f * hash01(i * 5)));
    }
    for (int y = -16; y <= 16; y++) { // the moon, with its seas
        for (int x = -16; x <= 16; x++) {
            float d = std::sqrt((float)(x * x + y * y));
            if (d > 15.5f) continue;
            float sea = fbm2((x + 40) * 0.18f, (y + 40) * 0.18f, 41, 3);
            Color c = (sea > 0.58f) ? mixColor(MOON, SKY_LOW, 0.22f) : MOON;
            if (d > 14.5f) c = mixColor(MOON, SKY_LOW, 0.3f);
            sky.set((int)MOON_X + x, (int)MOON_Y + y, c);
        }
    }
    storeLayer(art, sky, PARALLAX[SKY_LAYER], 0);

    // A drowned city far away: towers with broken tops, and a long aqueduct.
    Picture far(layerWidth(range, PARALLAX[FAR_LAYER]), CANVAS_HEIGHT);
    for (int i = 0; i < 14; i++) {
        int x = (int)(i * 52.0f + hash01(i * 7 + 3) * 24.0f) - 10;
        int width = 12 + (int)(hash01(i * 11 + 5) * 12.0f);
        int top = WATERLINE - 70 - (int)(hash01(i * 17 + 9) * 80.0f);
        for (int y = top; y < WATERLINE; y++) {
            for (int xx = x; xx < x + width; xx++) {
                bool lit = xx >= x + width - 2;
                far.set(xx, y, FAR_CITY[lit ? 2 : (y < top + 4 ? 1 : 0)]);
            }
        }
        far.rect(x, top - 6, width / 2, 6, FAR_CITY[0]); // a broken top
        for (int k = 0; k < 3; k++) far.rect(x + width / 2 - 1, top + 10 + k * 14, 2, 4, FAR_CITY[3]);
    }
    for (int x = 0; x < far.width; x++) { // what is left of an aqueduct, in two broken runs
        int pier = x % 34;
        if ((x > 300 && x < 470) || x > 760 || hash01(x / 34 + 50) > 0.7f) continue; // fallen spans
        for (int y = 168; y < 176; y++) far.set(x, y, FAR_CITY[1]);
        if (pier < 6) for (int y = 176; y < WATERLINE; y++) far.set(x, y, FAR_CITY[0]);
        float dx = pier - 20.0f;
        for (int y = 176; y < 192; y++) {
            float dy = 190.0f - y;
            float d = std::sqrt(dx * dx + dy * dy);
            if (pier >= 6 && d > 11.0f && d < 14.0f) far.set(x, y, FAR_CITY[1]);
        }
    }
    fogRows(far, MIST, 120, WATERLINE, 0.45f, 0.7f);
    storeLayer(art, far, PARALLAX[FAR_LAYER], 0);

    // Great broken arches in the middle distance, standing in the water.
    Picture mid(layerWidth(range, PARALLAX[MID_LAYER]), CANVAS_HEIGHT);
    Mask midMask(mid.width, CANVAS_HEIGHT);
    for (int i = 0; i < ARCH_COUNT; i++) {
        Arch a = archAt(i);
        int leftX = (int)a.x - a.radius - PILLAR / 2;
        int rightX = (int)a.x + a.radius - PILLAR / 2;
        int rightTop = a.broken ? a.springY + 34 : a.springY;
        midMask.rect(leftX, a.springY, PILLAR, WATERLINE - a.springY + 2);
        midMask.rect(rightX, rightTop, PILLAR, WATERLINE - rightTop + 2);
        midMask.rect(leftX - 2, a.springY - 4, PILLAR + 4, 4); // capitals
        if (!a.broken) midMask.rect(rightX - 2, a.springY - 4, PILLAR + 4, 4);
        float outer = (float)(a.radius + PILLAR / 2), inner = (float)(a.radius - PILLAR / 2);
        for (int y = a.springY - (int)outer; y < a.springY; y++) {
            for (int x = (int)(a.x - outer); x <= (int)(a.x + outer); x++) {
                float dx = x + 0.5f - a.x, dy = y + 0.5f - a.springY;
                float d = std::sqrt(dx * dx + dy * dy);
                if (d < inner || d > outer) continue;
                if (a.broken && std::atan2(-dy, dx) < 1.1f) continue; // the right side of the arch has fallen
                midMask.set(x, y);
            }
        }
    }
    midMask.measure();
    Shading midStyle;
    midStyle.ramp = MID_STONE;
    midStyle.steps = 5;
    midStyle.toLight = TO_MOON;
    midStyle.base = 0.34f;
    midStyle.rim = 0.4f;
    midStyle.bevel = 2.0f;
    midStyle.grain = 0.1f;
    midStyle.seed = 33;
    midStyle.dither = false;
    shade(mid, midMask, midStyle);
    fogRows(mid, MIST, 150, WATERLINE, 0.12f, 0.4f);
    storeLayer(art, mid, PARALLAX[MID_LAYER], 0);
    for (int i = 0; i < ARCH_COUNT; i++) { // banners from the whole arches
        Arch a = archAt(i);
        if (a.broken) continue;
        Picture banner = paintBanner(9, 24 + (i % 2) * 6, 50 + i);
        storeSway(art, banner, MID_LAYER, false, a.x - 5.0f, (float)(a.springY - a.radius + 2), 2.5f, true, 6, 0.1f * i);
    }

    // Columns just behind the route, dark against the water.
    Picture nearCols(layerWidth(range, PARALLAX[NEAR_LAYER]), CANVAS_HEIGHT);
    Mask nearMask(nearCols.width, CANVAS_HEIGHT);
    // Placed in the gaps between platforms at mid-scroll, so they frame the route instead of standing behind a ledge.
    const float columnX[4] = {154.0f, 469.0f, 629.0f, 820.0f};
    const int columnTop[4] = {60, 150, 30, 190};
    for (int i = 0; i < 4; i++) {
        int top = columnTop[i];
        nearMask.rect((int)columnX[i] - 12, top, 24, WATERLINE - top + 2);
        nearMask.rect((int)columnX[i] - 15, top - 5, 30, 5);
        if (hash01(i * 7 + 71) > 0.4f) nearMask.rect((int)columnX[i] - 15, top - 9, 18, 4); // a broken upper drum
    }
    nearMask.measure();
    Shading nearStyle = midStyle;
    nearStyle.ramp = NEAR_STONE;
    nearStyle.steps = 4;
    nearStyle.base = 0.3f;
    nearStyle.seed = 72;
    shade(nearCols, nearMask, nearStyle);
    for (int i = 0; i < 4; i++) { // fluting, and the moon's light along the left edge
        for (int y = columnTop[i]; y < WATERLINE; y++) {
            if (nearMask.at((int)columnX[i] - 12, y)) nearCols.set((int)columnX[i] - 12, y, NEAR_STONE[3]);
            for (int f = -8; f <= 8; f += 4) if (nearMask.at((int)columnX[i] + f, y)) nearCols.set((int)columnX[i] + f, y, NEAR_STONE[(f > 0) ? 0 : 2]);
        }
    }
    storeLayer(art, nearCols, PARALLAX[NEAR_LAYER], 0);

    // Roots hanging from the vaults above, swaying in the draft.
    storeEmptyLayer(art, PARALLAX[HANGING_LAYER]);
    int hangWidth = layerWidth(range, PARALLAX[HANGING_LAYER]);
    for (int i = 0; i * 74 < hangWidth; i++) {
        Picture roots = paintHangingRoots(3 + i % 3, 30 + (int)(hash01(i * 9 + 1) * 40.0f), 80 + i * 7);
        storeSway(art, roots, HANGING_LAYER, false, i * 74.0f + hash01(i * 3) * 30.0f, -2.0f, 3.0f, true, 5, 0.12f * (i % 4));
    }

    const Node* head = world.getHead(1);
    const Node* node = head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        LedgeCanvas canvas = makeLedgeCanvas(*node, 30, 12);
        paintRuinsLedge(*node, canvas);
        storeLedge(art, *node, canvas);
        node = node->next;
    }

    // Foreground: a broken column at the start, roots overhead, reeds at the water's edge.
    Picture front(layerWidth(range, FRONT_PARALLAX), CANVAS_HEIGHT);
    for (int y = 60; y < CANVAS_HEIGHT; y++) {
        for (int x = 0; x < 30; x++) front.set(x, y, (x == 27) ? NEAR_STONE[2] : DARK);
    }
    front.rect(0, 52, 36, 8, DARK);
    storeLayer(art, front, FRONT_PARALLAX, 0, true);
    for (int i = 0; i * 90 < front.width; i++) {
        Picture reeds = paintReeds(30 + (int)(hash01(i * 5) * 20.0f), 30 + (int)(hash01(i * 7) * 24.0f), 120 + i);
        storeSway(art, reeds, 0, true, 40.0f + i * 90.0f + hash01(i * 3) * 40.0f, (float)(CANVAS_HEIGHT - reeds.height + 2), 2.0f,
                  false, 4, 0.15f * (i % 3));
    }
}

// ---------------------------------------------------------------------------
// Live pieces: braziers, the key, the doorway
// ---------------------------------------------------------------------------

// A stone brazier. `ignite` counts up from 0 once it is lit: the flames grow out of the bowl
// with a flare. `waiting` = a faint cool shimmer that marks it as the goal.
void drawBrazier(int x, int baseY, float ignite, float waiting, float time) {
    DrawRectangle(x + 3, baseY - 5, 2, 5, STONE[1]);
    DrawRectangle(x, baseY - 8, 8, 3, STONE[3]);
    DrawRectangle(x + 1, baseY - 5, 6, 1, STONE[1]);
    DrawRectangle(x, baseY - 8, 8, 1, STONE[5]);
    if (waiting > 0.01f) {
        float pulse = waiting * (0.5f + 0.5f * std::sin(time * 3.0f));
        DrawRectangle(x + 2, baseY - 9, 4, 1, withAlpha(COOL_LIGHT, pulse));
        for (int i = 0; i < 3; i++) { // cool sparks waiting above the cold bowl
            float t = std::fmod(time * 0.5f + i * 0.33f, 1.0f);
            DrawRectangle(x + 2 + i * 2, baseY - 10 - (int)(t * 10.0f), 1, 1, withAlpha(COOL_LIGHT, waiting * loopEnvelope(t)));
        }
    }
    if (ignite <= 0.0f) return;
    float grow = smooth01(ignite);
    float flare = (ignite < 1.0f) ? std::sin(ignite * 3.14159f) : 0.0f;
    for (int i = 0; i < 4; i++) { // flame tongues
        float lick = 3.0f + (0.5f + 0.5f * std::sin(time * (9.0f + i * 2.3f) + i)) * 5.0f;
        int height = (int)std::lround(lick * grow + flare * 4.0f);
        if (height > 0) DrawRectangle(x + i * 2, baseY - 8 - height, 2, height, (i % 2 == 0) ? FIRE : FIRE_HOT);
    }
    DrawRectangle(x + 3, baseY - 10, 2, 2, WHITE);
    for (int i = 0; i < 4; i++) { // embers rising
        float t = std::fmod(time * (0.6f + 0.15f * i) + i * 0.27f, 1.0f);
        DrawRectangle(x + 2 + i * 2 + (int)(std::sin(time * 3.0f + i) * 2.0f), baseY - 12 - (int)(t * 20.0f), 1, 1,
                      withAlpha(FIRE_HOT, grow * loopEnvelope(t)));
    }
    for (int i = 0; flare > 0.0f && i < 8; i++) { // sparks thrown out as it catches
        float angle = i * 0.785f + 0.4f;
        float distance = ignite * 16.0f;
        DrawRectangle(x + 4 + (int)(std::cos(angle) * distance), baseY - 12 + (int)(std::sin(angle) * distance * 0.7f - ignite * 6.0f),
                      1, 1, withAlpha(FIRE_HOT, 1.0f - ignite));
    }
}

// The key, floating above the shrine until it is taken; then it rises and fades.
void drawRelic(int cx, int y, float time, float alpha) {
    int top = y + (int)std::round(std::sin(time * 2.0f) * 2.0f);
    Color gold = withAlpha(RELIC_GOLD, alpha);
    DrawRing(Vector2{(float)cx, (float)top}, 1.0f, 3.0f, 0.0f, 360.0f, 12, gold);
    DrawRectangle(cx - 1, top + 2, 2, 7, gold);
    DrawRectangle(cx + 1, top + 6, 2, 1, gold);
    DrawRectangle(cx + 1, top + 8, 2, 1, gold);
    for (int i = 0; i < 4; i++) { // sparkles circling it, dimmer behind the key
        float angle = time * 1.5f + i * 1.5708f;
        float depth = 0.55f + 0.45f * std::sin(angle);
        DrawRectangle(cx + (int)(std::cos(angle) * 9.0f), top + 3 + (int)(std::sin(angle) * 5.0f), 1, 1,
                      withAlpha(FIRE_HOT, 0.85f * depth * alpha));
    }
    float glint = std::fmod(time * 0.35f, 1.0f); // now and then a glint runs down the key
    if (glint < 0.16f) {
        float a = std::sin(glint / 0.16f * 3.14159f) * alpha;
        DrawRectangle(cx - 1, top + 2 + (int)(glint / 0.16f * 6.0f), 2, 1, withAlpha(WHITE, a));
        DrawRectangle(cx + 2, top - 3, 1, 3, withAlpha(WHITE, a * 0.8f));
        DrawRectangle(cx + 1, top - 2, 3, 1, withAlpha(WHITE, a * 0.8f));
    }
}

void drawKeyLight(Vector2 at, float alpha, float time) {
    float x = at.x + std::sin(time * 2.0f) * 2.0f;
    float y = at.y + std::sin(time * 3.1f) * 2.0f;
    DrawRectangle((int)x - 1, (int)y - 1, 2, 2, withAlpha(WHITE, alpha));
    DrawRectangle((int)x + 2, (int)y + 1, 1, 1, withAlpha(RELIC_GOLD, 0.9f * alpha));
    DrawRectangle((int)x - 3, (int)y + 2, 1, 1, withAlpha(COOL_LIGHT, 0.8f * alpha));
}

// A ruined doorway around the exit. A fallen slab and roots block it until the end of the path is
// reached; then the slab sinks away and warm light comes through (open: 0 -> 1).
void drawRuinDoorway(Rectangle gate, int cameraX, float open, float time) {
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    const int cx = gx + gw / 2;
    const float light = smooth01(open);

    DrawRectangle(gx, gy, gw, gh, withAlpha(DARK, 1.0f - light));
    if (light > 0.0f) {
        DrawRectangleGradientV(gx, gy, gw, gh, withAlpha(FIRE_HOT, 0.92f * light), withAlpha(RELIC_GOLD, 0.65f * light));
        for (int i = 0; i < 6; i++) {
            float t = std::fmod(time * (0.35f + 0.1f * hash01(i * 3 + 4)) + i * 0.17f, 1.0f);
            DrawRectangle(gx + 2 + (int)(hash01(i * 5 + 21) * (gw - 4)), gy + gh - 2 - (int)(t * gh), 1, 1,
                          withAlpha(WHITE, light * loopEnvelope(t)));
        }
    }
    if (open < 1.0f) {
        int sink = (int)std::round(open * open * (gh - gh / 3));
        int slabTop = gy + gh / 3 + sink;
        int slabHeight = gh - gh / 3 - sink;
        if (slabHeight > 0) {
            DrawRectangle(gx + 1, slabTop, gw - 2, slabHeight, STONE[1]);
            DrawRectangle(gx + 1, slabTop, gw - 2, 1, STONE[3]);
            for (int k = 0; k < 8 && 3 + k < slabHeight; k++) DrawRectangle(gx + 6 + (k % 2) + k / 3, slabTop + 3 + k, 1, 1, STONE[0]);
        }
        float roots = 1.0f - smooth01(open * 1.5f);
        for (int r = 0; roots > 0.0f && r < 3; r++) {
            for (int x = 0; x < gw; x++) {
                DrawRectangle(gx + x, gy + 6 + r * 7 + (int)std::round(std::sin(x * 0.4f + r * 2.0f) * 1.5f), 1, 1, withAlpha(ROOT, roots));
            }
        }
        float dust = std::sin(open * 3.14159f);
        for (int i = 0; dust > 0.01f && i < 6; i++) {
            DrawRectangle(gx + (int)(hash01(i * 7 + 3) * gw), gy + gh - 2 - (int)(hash01(i * 5 + 1) * 5.0f * open), 1, 1,
                          withAlpha(STONE[4], 0.7f * dust));
        }
    }
    DrawRectangle(gx - 6, gy, 6, gh, STONE[3]); // jambs, lit on the left (the moon side)
    DrawRectangle(gx + gw, gy, 6, gh, STONE[2]);
    DrawRectangle(gx - 6, gy, 1, gh, STONE[5]);
    DrawRectangle(gx + gw + 5, gy, 1, gh, STONE[0]);
    DrawRectangle(gx - 8, gy - 6, gw + 16, 6, STONE[3]); // lintel
    DrawRectangle(gx - 8, gy - 6, gw + 16, 1, STONE[5]);
    drawTriangleSafe(Vector2{(float)(gx - 8), (float)(gy - 6)}, Vector2{(float)cx, (float)(gy - 6)},
                     Vector2{(float)(cx - 2), (float)(gy - 15)}, STONE[3]); // half of a broken pediment
    Color rune = lerpColor(STONE[1], RELIC_GOLD, smooth01(open * 1.6f - 0.3f));
    for (int k = 0; k < 3; k++) {
        DrawRectangle(gx - 4, gy + 6 + k * 9, 2, 2, rune);
        DrawRectangle(gx + gw + 2, gy + 6 + k * 9, 2, 2, rune);
    }
    for (int row = 0; row < 16; row++) { // a tattered banner on the jamb
        int sway = (int)std::round(std::sin(time * 1.6f + row * 0.25f) * (row / 16.0f) * 2.0f);
        int width = (row < 13) ? 6 : 6 - (row - 12) * 2;
        DrawRectangle(gx - 13 + sway, gy - 2 + row, width, 1, (row % 5 == 0) ? BANNER_DARK : BANNER);
    }
    DrawRectangle(gx - 14, gy - 3, 8, 1, RELIC_GOLD);
}

float ledgeMist(const SceneView& view, const Node& node) {
    if (view.mistById == nullptr || node.id < 0 || node.id > MAX_NODES_PER_REGION) return 0.0f;
    return view.mistById[node.id];
}

// How strongly the back-step is shown: previous bright and next dim while the key's light is carried.
float previousEmphasis(const SceneView& view) {
    float emphasis = view.emphasizePrevious ? 1.0f : 0.0f;
    if (view.stage == 1 || view.stage == 2) {
        float k = smooth01(view.stageTime / 0.6f);
        emphasis = (1.0f - k) * (1.0f - emphasis) + k * emphasis;
    }
    return emphasis;
}

// When the key is taken, a wave of cool light runs back along the path: N4 -> N3 -> N2.
float returnWave(const SceneView& view, const Node& node) {
    if (view.stage != 1 || view.stageTime > 3.0f) return 0.0f;
    float delay = (node.id == 3) ? 0.35f : (node.id == 2 ? 0.8f : -1.0f);
    if (delay < 0.0f) return 0.0f;
    float t = view.stageTime - delay;
    return (t > 0.0f && t < 1.4f) ? std::sin(t / 1.4f * 3.14159f) : 0.0f;
}

// ---------------------------------------------------------------------------
// Region 2 drawing
// ---------------------------------------------------------------------------

void drawRuinsBackground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    drawBackLayers(view, art, SKY_LAYER, FAR_LAYER, 0.0f);
    for (int i = 0; i < 24; i++) { // a few stars that twinkle
        int x = (int)(hash01(i * 17 + 3) * CANVAS_WIDTH);
        int y = 6 + (int)(hash01(i * 19 + 5) * 150.0f);
        float twinkle = 0.5f + 0.5f * std::sin(time * (1.2f + hash01(i)) + i * 2.3f);
        DrawRectangle(x, y, 1, 1, withAlpha(MOON, 0.25f + 0.6f * twinkle));
    }
    drawBackLayers(view, art, FAR_LAYER, MID_LAYER, 0.0f);
    for (int i = 0; i < 4; i++) { // haze drifting over the drowned city
        float x = std::fmod(i * 250.0f + time * 3.0f - view.scroll * 0.2f + 4000.0f, 1000.0f) - 180.0f;
        DrawEllipse((int)x, 250 + (i % 2) * 22, 160.0f, 12.0f, withAlpha(MIST, 0.06f));
    }
    drawBackLayers(view, art, MID_LAYER, NEAR_LAYER, 0.8f);
    for (int i = 0; i < ARCH_COUNT; i++) { // braziers still burning on the broken pillars
        Arch a = archAt(i);
        if (!a.broken) continue;
        float bx = layerToScreen(a.x + a.radius, view.scroll, PARALLAX[MID_LAYER]);
        int top = a.springY + 34 - 4;
        for (int f = 0; f < 3; f++) {
            int height = 2 + (int)((0.5f + 0.5f * std::sin(time * (8.0f + f * 2.9f) + i + f * 1.7f)) * 4.0f);
            DrawRectangle((int)bx - 3 + f * 2, top - height, 2, height, (f == 1) ? FIRE_HOT : FIRE);
        }
        for (int e = 0; e < 2; e++) {
            float t = std::fmod(time * (0.6f + 0.2f * e) + i * 0.37f + e * 0.5f, 1.0f);
            DrawRectangle((int)bx - 1 + e * 2 + (int)(std::sin(time * 2.0f + e + i) * 2.0f), top - 6 - (int)(t * 18.0f), 1, 1,
                          withAlpha(FIRE_HOT, loopEnvelope(t)));
        }
    }
    DrawRectangleGradientV(0, 190, CANVAS_WIDTH, WATERLINE - 190, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.18f));
    drawBackLayers(view, art, NEAR_LAYER, LAYER_COUNT, 0.9f);
}

void drawRuinsPlatforms(const SceneView& view, const RegionArt& art) {
    drawLedges(view, art, VEIL);

    const Node* next = (view.current != nullptr) ? view.current->next : nullptr; // each link checked before use
    const Node* previous = (view.current != nullptr) ? view.current->previous : nullptr;
    const float emphasis = previousEmphasis(view);
    const float arrival = arrivalEase(view);
    Vector2 brazier = {0.0f, 0.0f};
    bool brazierSeen = false;

    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            if (node->id == 2) { // the Old Hall's brazier: cold, then waiting for the key's light, then lit
                int bx = ledge.left + ledge.width - 14;
                float ignite = (view.stage >= 2) ? view.stageTime / 0.8f : 0.0f;
                float waiting = (view.stage == 1) ? 1.0f : ((view.stage == 2) ? std::fmax(0.0f, 1.0f - view.stageTime / 0.4f) : 0.0f);
                drawBrazier(bx, ledge.top, ignite, waiting, view.time);
                brazier = Vector2{(float)(bx + 4), (float)(ledge.top - 11)};
                brazierSeen = true;
            }
            if (node->id == 4) { // the key above the shrine; it rises and fades when taken
                float relic = (view.stage == 0) ? 1.0f : ((view.stage == 1) ? 1.0f - smooth01(view.stageTime / 0.6f) : 0.0f);
                if (relic > 0.0f) drawRelic(ledge.left + ledge.width / 2, ledge.top - 24 - (int)((1.0f - relic) * 12.0f), view.time, relic);
            }
            if (node == next) drawLinkLip(ledge, WARM_LIGHT, (1.0f - 0.55f * emphasis) * arrival, view.time, true);
            if (node == previous) drawLinkLip(ledge, COOL_LIGHT, (0.5f + 0.5f * emphasis) * arrival, view.time, false);
            float wave = returnWave(view, *node);
            if (wave > 0.0f) drawLinkLip(ledge, COOL_LIGHT, wave, view.time, false);
        }
        node = node->next;
    }

    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        drawLanternPost(ledge.left + 6, ledge.top, view.checkpointFlash, view.time);
    }
    // The key's light travels with the traveler, and at N2 it flies into the brazier.
    Vector2 carried = {view.playerFeet.x - 10.0f, view.playerFeet.y - 32.0f};
    if (view.stage == 1) {
        drawKeyLight(carried, smooth01(view.stageTime / 0.5f), view.time);
    } else if (view.stage == 2 && brazierSeen && view.stageTime < 0.35f) {
        float k = smooth01(view.stageTime / 0.35f);
        Vector2 at = {carried.x + (brazier.x - carried.x) * k, carried.y + (brazier.y - carried.y) * k - std::sin(k * 3.14159f) * 10.0f};
        drawKeyLight(at, 1.0f - 0.4f * k, view.time);
    }
    if (view.hasGate) drawRuinDoorway(view.gateRect, view.cameraX, view.gateProgress, view.time);
}

// The water: the scene above, reflected and rippling, with the current flowing to the right.
void drawRuinsWater(const SceneView& view, const RegionArt&, const Texture2D& reflection) {
    const float time = view.time;
    const int depth = CANVAS_HEIGHT - WATERLINE;
    DrawRectangleGradientV(0, WATERLINE, CANVAS_WIDTH, depth, WATER_TOP, WATER_DEEP);
    for (int y = WATERLINE; y < CANVAS_HEIGHT; y++) {
        int k = y - WATERLINE;
        int row = WATERLINE - 1 - k; // the row of the scene mirrored at this depth
        if (row < 0) break;
        float wave = std::sin(time * 1.7f + y * 0.55f) + 0.5f * std::sin(time * 2.9f - y * 0.9f);
        int wobble = (int)std::lround(wave * (0.6f + k * 0.07f));
        float fade = 1.0f - (float)k / depth;
        Color tint = {150, 172, 214, (unsigned char)(255.0f * (0.62f * fade + 0.12f))};
        DrawTextureRec(reflection, Rectangle{(float)wobble, (float)(CANVAS_HEIGHT - 1 - row), (float)CANVAS_WIDTH, 1.0f},
                       Vector2{0.0f, (float)y}, tint);
    }
    DrawRectangle(0, WATERLINE, CANVAS_WIDTH, 1, withAlpha(MOON, 0.3f));
    for (int i = 0; i < 40; i++) { // ripples catching the moonlight, drifting with the current
        float x = std::fmod(hash01(i * 7 + 2) * 760.0f + time * (5.0f + (i % 3) * 2.5f) - view.scroll + 3800.0f, 760.0f) - 60.0f;
        int y = WATERLINE + 2 + (int)(hash01(i * 11 + 5) * (depth - 3));
        float shimmer = 0.5f + 0.5f * std::sin(time * (1.4f + 0.6f * hash01(i * 3)) + i * 2.3f);
        int width = 4 + (int)(hash01(i * 13 + 1) * 12.0f) + (int)std::round(shimmer * 2.0f);
        DrawRectangle((int)x, y, width, 1, withAlpha(MOON, 0.06f + 0.12f * shimmer));
        if (i % 7 == 0) { // a floating leaf, bobbing on the ripple
            DrawRectangle((int)x + 2, y - 1 + (int)std::round(std::sin(time * 1.7f + i) * 0.6f), 2, 1, MOSS[2]);
        }
    }
    for (int y = WATERLINE + 1; y < CANVAS_HEIGHT; y += 2) { // the moon's path on the water
        float k = (y - WATERLINE) / (float)depth;
        int width = 3 + (int)(k * 9.0f);
        int wobble = (int)std::round(std::sin(time * 2.0f + y * 0.35f) * (1.0f + k * 3.0f));
        DrawRectangle((int)MOON_X - width / 2 + wobble, y, width, 1, withAlpha(MOON, 0.32f * (1.0f - k)));
    }
    for (int i = 0; i < 5; i++) { // low mist over the water
        float x = std::fmod(i * 190.0f + time * 4.0f - view.scroll * 1.05f + 4000.0f, 990.0f) - 170.0f;
        DrawEllipse((int)x, WATERLINE - 3 + (i % 2) * 6, 100.0f + (i % 3) * 30.0f, 8.0f, withAlpha(MIST, 0.1f));
    }
}

void drawRuinsForeground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    for (int i = 0; i < 26; i++) { // dust floating in the moonlight
        float x = std::fmod(hash01(i * 17 + 3) * 900.0f - view.scroll * 0.9f + time * 3.0f + 3600.0f, 900.0f) - 100.0f;
        float y = 50.0f + hash01(i * 5 + 8) * 220.0f + std::sin(time * 0.6f + i) * 6.0f;
        DrawRectangle((int)x, (int)y, 1, 1, withAlpha(MOON, 0.2f + 0.25f * std::sin(time * 2.0f + i)));
    }
    drawFrontLayers(view, art, 0.8f);
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 50, withAlpha(DARK, 0.55f), withAlpha(DARK, 0.0f));
}

void drawRuinsAdditive(const SceneView& view, const RegionArt&, const SceneRenderer& renderer) {
    const float time = view.time;
    renderer.drawSoftLight(Vector2{MOON_X, MOON_Y}, 150.0f, MOON, 0.18f);
    renderer.drawSoftLight(Vector2{MOON_X, MOON_Y}, 44.0f, MOON, 0.3f);
    for (int i = 0; i < 3; i++) { // moonbeams slanting down through the broken vaults
        float top = 120.0f + i * 110.0f - view.scroll * 0.2f;
        float width = 14.0f + i * 7.0f;
        float alpha = 0.05f + 0.02f * std::sin(time * 0.5f + i * 2.1f);
        Color topColor = withAlpha(MOON, alpha);
        Color clear = withAlpha(MOON, 0.0f);
        drawGradientQuad(Vector2{top, 0.0f}, Vector2{top + width, 0.0f}, Vector2{top + width + 110.0f, (float)WATERLINE},
                         Vector2{top + 110.0f, (float)WATERLINE}, topColor, topColor, clear, clear);
    }
}

void addRuinsLights(const SceneView& view, const RegionArt&, std::vector<Light>& lights) {
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;
    const Node* previous = (view.current != nullptr) ? view.current->previous : nullptr;
    const float emphasis = previousEmphasis(view);
    const float arrival = arrivalEase(view);
    const float time = view.time;
    const Node* node = view.head;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            if (node == next) addLinkLight(lights, ledge, WARM_LIGHT, (1.0f - 0.55f * emphasis) * arrival);
            if (node == previous) addLinkLight(lights, ledge, COOL_LIGHT, (0.5f + 0.5f * emphasis) * arrival);
            float wave = returnWave(view, *node);
            if (wave > 0.0f) addLinkLight(lights, ledge, COOL_LIGHT, wave);
            if (node->id == 2 && view.stage >= 2) {
                float grow = smooth01(view.stageTime / 0.8f);
                float flare = (view.stageTime < 0.8f) ? std::sin(view.stageTime / 0.8f * 3.14159f) : 0.0f;
                float flicker = 0.85f + 0.1f * std::sin(time * 13.0f) + 0.05f * std::sin(time * 21.7f);
                lights.push_back(Light{Vector2{ledge.left + ledge.width - 10.0f, ledge.top - 12.0f}, 70.0f + 30.0f * flare,
                                       FIRE, (0.62f * grow + 0.4f * flare) * flicker, 12.0f + 10.0f * flare, 0.5f * grow + 0.4f * flare});
            } else if (node->id == 2 && view.stage == 1) {
                lights.push_back(Light{Vector2{ledge.left + ledge.width - 10.0f, ledge.top - 10.0f}, 26.0f, COOL_LIGHT, 0.3f, 6.0f, 0.25f});
            }
            if (node->id == 4) {
                float relic = (view.stage == 0) ? 1.0f : ((view.stage == 1) ? 1.0f - smooth01(view.stageTime / 0.6f) : 0.0f);
                if (relic > 0.0f) {
                    lights.push_back(Light{Vector2{ledge.left + ledge.width * 0.5f, ledge.top - 20.0f}, 44.0f, RELIC_GOLD,
                                           0.45f * relic, 10.0f, (0.35f + 0.1f * std::sin(time * 2.6f)) * relic});
                }
            }
        }
        node = node->next;
    }
    for (int i = 0; i < ARCH_COUNT; i++) {
        Arch a = archAt(i);
        if (!a.broken) continue;
        float bx = layerToScreen(a.x + a.radius, view.scroll, PARALLAX[MID_LAYER]);
        float flicker = 0.8f + 0.12f * std::sin(time * 12.0f + i) + 0.08f * std::sin(time * 19.3f + i * 2.7f);
        lights.push_back(Light{Vector2{bx, a.springY + 26.0f}, 40.0f, FIRE, 0.35f * flicker, 9.0f, 0.4f * flicker});
    }
    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        addLanternLight(lights, ledge.left + 6, ledge.top, view.checkpointFlash, time);
    }
    if (view.stage == 1) {
        float a = smooth01(view.stageTime / 0.5f);
        lights.push_back(Light{Vector2{view.playerFeet.x - 10.0f, view.playerFeet.y - 32.0f}, 40.0f, COOL_LIGHT, 0.45f * a, 8.0f, 0.5f * a});
    }
    if (view.hasGate && view.gateProgress > 0.0f) {
        float light = smooth01(view.gateProgress);
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height / 2.0f);
        lights.push_back(Light{Vector2{x, y}, 80.0f * light, FIRE_HOT, 0.55f * light, 24.0f * light, 0.45f * light});
    }
}

Mood ruinsMood(const SceneView&) {
    Mood mood;
    mood.skyLight = WHITE;
    mood.groundLight = Color{118, 130, 170, 255};
    mood.lightFrom = 90;
    mood.lightTo = 300;
    mood.shadowTint = Vector3{0.86f, 0.94f, 1.12f};
    mood.highlightTint = Vector3{1.07f, 1.0f, 0.92f};
    mood.saturation = 1.02f;
    mood.bloom = 0.75f;
    mood.vignette = 0.42f;
    return mood;
}

} // namespace

const RegionPainter RUINS_PAINTER = {bakeRuins, drawRuinsBackground, drawRuinsPlatforms, drawRuinsWater,
                                     drawRuinsForeground, drawRuinsAdditive, addRuinsLights, ruinsMood};
