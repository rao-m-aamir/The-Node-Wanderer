#include "ArtCommon.h"

#include <algorithm>

// Region 5, "The Lighthouse Stair": a lighthouse on the sea cliffs in the morning fog. Its stair
// has fallen. The stones of a new one wait in sockets in the tower wall: push() slides the next
// one out above the last, and pop() slides the top one back in, so the stair is the stack itself.
// Only the top stone is lit and solid; the ones under it sit in the fog until their turn. The fog
// bell hangs at the top. When it rings the fog starts to lift and boats set out for the harbor.
// The floor stone at the tower's foot is the bottom of the stack, and the sea door stands on it.

namespace {

const Color SKY_TOP = {116, 144, 176, 255};
const Color SKY_MID = {176, 186, 194, 255};
const Color SKY_LOW = {216, 204, 182, 255};
const Color SUN = {255, 244, 220, 255};
const Color FOG = {212, 214, 212, 255};
const Color SEA_FAR = {168, 184, 192, 255};
const Color SEA_NEAR = {72, 98, 116, 255};
const Color FOAM = {238, 244, 244, 255};
const Color VEIL = {150, 164, 178, 255}; // the fog over stones that cannot be reached now
const Color DARK = {26, 32, 40, 255};
const Color LAMP = {255, 224, 150, 255};
const Color DAYLIGHT = {255, 244, 214, 255};

const Color FAR_LAND[4] = {{146, 156, 168, 255}, {160, 168, 178, 255}, {176, 182, 190, 255}, {198, 202, 206, 255}};
const Color ROCK[6] = {{34, 40, 50, 255}, {52, 60, 72, 255}, {76, 86, 98, 255},
                       {106, 114, 124, 255}, {144, 150, 156, 255}, {186, 190, 192, 255}};
const Color GRASS[4] = {{62, 90, 70, 255}, {86, 118, 82, 255}, {122, 150, 96, 255}, {168, 188, 128, 255}};
const Color TOWER[5] = {{124, 118, 114, 255}, {160, 152, 144, 255}, {190, 182, 170, 255}, {212, 204, 190, 255}, {230, 224, 210, 255}};
const Color BAND[5] = {{104, 38, 36, 255}, {142, 52, 46, 255}, {180, 70, 58, 255}, {206, 98, 80, 255}, {226, 132, 110, 255}};
const Color SANDSTONE[5] = {{78, 58, 46, 255}, {120, 90, 66, 255}, {162, 124, 88, 255}, {196, 160, 114, 255}, {226, 198, 150, 255}};
const Color SEAWEED[3] = {{30, 44, 28, 255}, {54, 70, 38, 255}, {90, 102, 50, 255}};
const Color BRONZE[5] = {{70, 48, 26, 255}, {116, 82, 40, 255}, {164, 122, 58, 255}, {210, 170, 90, 255}, {244, 216, 146, 255}};
const Color IRON = {36, 38, 46, 255};
const Color IRON_LIT = {84, 88, 96, 255};
const Color GLASS = {176, 196, 204, 255};
const Color DOOR_WOOD[3] = {{56, 38, 30, 255}, {84, 58, 42, 255}, {118, 86, 60, 255}};

const Vector2 TO_SUN = {0.74f, -0.67f}; // the low morning sun, out over the sea to the right

enum Layer { SKY_LAYER, FAR_LAYER, MID_LAYER, TOWER_LAYER, LAYER_COUNT };
const float PARALLAX[LAYER_COUNT] = {0.0f, 0.1f, 0.4f, 1.0f};

const int TOWER_X = 265;    // the tower's middle (the sea door is under it), region-local canvas x
const int TOWER_BASE = 300; // where it stands on the floor stone
const int TOWER_TOP = 52;   // the gallery
const int HORIZON = 212;
const int SEA_Y = 336;      // the sea's surface around the rocks
const float SUN_X = 506.0f;
const float SUN_Y = 146.0f;
const float LAMP_X = 265.0f;
const float LAMP_Y = 32.0f;
const float BELL_X = 352.0f; // the bell hangs from an iron arm, over the highest stone
const float BELL_Y = 62.0f;

// Carvings on the stair stones, so their order can be read: a wave, a shell, a gull, a bell.
const char* const GLYPHS[4][5] = {
    {".......", ".##..##", "#..##..", ".......", "......."},
    {"..###..", ".#####.", "#.#.#.#", ".#.#.#.", "...#..."},
    {".......", "##...##", "..#.#..", "...#...", "......."},
    {"...#...", "..###..", "..###..", ".#####.", "...#..."},
};
const Color STONE_TINT[4] = {{150, 178, 198, 255}, {228, 168, 156, 255}, {218, 218, 210, 255}, {238, 196, 108, 255}};

float easeOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

// The tower narrows as it rises.
float towerHalf(float y) {
    float t = std::clamp((y - TOWER_TOP) / (float)(TOWER_BASE - TOWER_TOP), 0.0f, 1.0f);
    return 36.0f + 14.0f * t;
}

// How far the fog has lifted since the bell rang (0 .. 0.65).
float fogLift(const SceneView& view) {
    return (view.stage >= 1) ? 0.65f * smooth01((view.stageTime - 0.8f) / 7.0f) : 0.0f;
}

// How hard the bell still swings (1 just after it is rung, fading over several seconds).
float bellSwing(const SceneView& view) {
    return (view.stage >= 1) ? std::exp(-view.stageTime * 0.45f) : 0.0f;
}

// 0 -> 1 while a node created at `at` comes into view (1 for nodes that were there from the start).
float arrival(const SceneView& view, int id, float seconds) {
    if (view.createdById == nullptr || id < 0 || id > MAX_NODES_PER_REGION) return 1.0f;
    return std::clamp((view.time - view.createdById[id]) / seconds, 0.0f, 1.0f);
}

float ledgeMist(const SceneView& view, const Node& node) {
    if (view.mistById == nullptr || node.id < 0 || node.id > MAX_NODES_PER_REGION) return 0.0f;
    return view.mistById[node.id];
}

// A gull: two wings that beat, seen from the side or the front.
void drawGull(float x, float y, float span, float flap, Color color) {
    int s = (int)span;
    int lift = (int)std::lround(flap * 2.0f); // +2 wings up, -2 wings down
    for (int k = 1; k <= s; k++) {
        float t = k / (float)s;
        int dy = -(int)std::lround(lift * t) + (int)std::lround(t * t * 1.5f);
        DrawRectangle((int)x - k, (int)y + dy, 1, 1, color);
        DrawRectangle((int)x + k, (int)y + dy, 1, 1, color);
    }
    DrawRectangle((int)x, (int)y, 1, 1, color);
}

// ---------------------------------------------------------------------------
// Painting (when the region is entered)
// ---------------------------------------------------------------------------

// A stair stone: a sandstone slab with a carving on its face and a corbel that goes back into the
// wall. Painted ahead for every stone the stair can hold, from the layout push() uses.
void paintStone(LedgeCanvas& c, int id) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const int seed = id * 37 + 11;
    const Color tint = STONE_TINT[std::clamp(id - 2, 0, 3)];
    Color ramp[5];
    for (int i = 0; i < 5; i++) ramp[i] = mixColor(SANDSTONE[i], tint, 0.24f);

    Mask m(p.width, p.height);
    for (int y = oy; y < oy + 11; y++) { // the slab, its far corners worn round
        int dy = y - oy;
        int chip = (dy == 0) ? 2 : ((dy == 10) ? 2 : (dy == 9 ? 1 : 0));
        m.span(y, ox - 5, ox + w + 1 - chip);
    }
    for (int y = oy + 11; y < oy + 20; y++) { // the corbel under it
        int dy = y - oy - 11;
        m.span(y, ox - 5, ox + (int)(w * 0.42f) - dy * 3);
    }
    m.measure();
    Shading s;
    s.ramp = ramp;
    s.steps = 5;
    s.toLight = TO_SUN;
    s.base = 0.46f;
    s.rim = 0.3f;
    s.bevel = 2.0f;
    s.grain = 0.14f;
    s.grainScale = 0.35f;
    s.topLight = 0.2f;
    s.seed = seed;
    shade(p, m, s);
    for (int x = ox - 5; x < ox + w - 1; x++) { // the walkable top, lit by the low sun, and the slab's dark underside
        p.set(x, oy, ramp[4]);
        p.set(x, oy + 1, ramp[3]);
        if (m.at(x, oy + 10)) p.set(x, oy + 10, ramp[0]);
    }
    for (int y = oy + 11; y < oy + 20; y++) { // the corbel sits in the slab's shadow
        for (int x = ox - 5; x < ox + w; x++) {
            if (m.at(x, y)) p.set(x, y, mixColor(p.get(x, y), ramp[0], 0.45f));
        }
    }
    const char* const* glyph = GLYPHS[std::clamp(id - 2, 0, 3)];
    int gx = ox + w / 2 - 3, gy = oy + 3;
    for (int r = 0; r < 5; r++) {
        for (int k = 0; k < 7; k++) {
            if (glyph[r][k] != '#') continue;
            p.set(gx + k, gy + r, ramp[0]);                                            // the groove
            if (r == 4 || glyph[r + 1][k] != '#') p.set(gx + k, gy + r + 1, ramp[4]); // its sunlit lower edge
        }
    }
    for (int i = 0; i < 10; i++) { // lichen and salt
        int x = ox + 2 + (int)(hash01(seed * 7 + i) * (w - 4));
        int y = oy + 2 + (int)(hash01(seed * 11 + i) * 8.0f);
        if (x >= gx - 1 && x <= gx + 7 && y >= gy - 1 && y <= gy + 5) continue;
        Color c2 = (i % 3 == 0) ? Color{214, 156, 72, 255} : ((i % 3 == 1) ? Color{150, 164, 120, 255} : ramp[4]);
        if (p.painted(x, y)) p.set(x, y, c2);
    }
}

// The floor stone: a broad rock landing at the tower's foot. Flagstones by the door, then rough
// rock that runs out into the sea, wet and weedy below the tide line.
void paintFloorStone(LedgeCanvas& c) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const int seed = 1 * 37 + 11;
    const int tide = SEA_Y - c.top + oy - 5; // picture row where the wet rock begins
    Mask m(p.width, p.height);
    for (int y = oy; y < p.height; y++) {
        int dy = y - oy;
        int left = ox - 14 - std::min(10, dy / 3);
        int right = ox + w + 2 - (int)(dy * 0.35f) - (int)(std::fabs(std::sin(dy * 0.4f)) * 3.0f) - (int)(hash01(seed + dy / 3) * 3.0f);
        if (dy == 0) right = ox + w;
        m.span(y, left, right);
    }
    m.measure();
    Shading s;
    s.ramp = ROCK;
    s.steps = 6;
    s.toLight = TO_SUN;
    s.base = 0.46f;
    s.rim = 0.32f;
    s.bevel = 2.5f;
    s.grain = 0.14f;
    s.grainScale = 0.26f;
    s.topLight = 0.2f;
    s.seed = seed;
    s.darkenFrom = (float)(oy + 10);
    s.darkenRange = 40.0f;
    s.darkenAmount = 0.35f;
    shade(p, m, s);
    for (int x = ox - 14; x < ox + w; x++) { // the top: flagstones by the door, rock further out
        bool flags = x < ox + 92;
        for (int k = 0; k < (flags ? 4 : 2); k++) {
            if (!m.at(x, oy + k)) continue;
            Color top = flags ? pickRamp(TOWER, 5, 0.72f - k * 0.16f, x, oy + k) : pickRamp(ROCK, 6, 0.86f - k * 0.2f, x, oy + k);
            p.set(x, oy + k, top);
        }
        if (flags && (x - ox + 3) % 13 == 0) { // joints between the flagstones
            p.set(x, oy + 1, TOWER[0]);
            p.set(x, oy + 2, TOWER[0]);
            p.set(x, oy + 3, TOWER[1]);
        }
    }
    for (int i = 0; i < 4; i++) { // rock pools catching the sky
        int x = ox + 104 + i * 30 + (int)(hash01(seed + i * 5) * 10.0f);
        int len = 4 + (int)(hash01(seed * 3 + i) * 6.0f);
        for (int k = 0; k < len; k++) {
            if (!m.at(x + k, oy + 1)) continue;
            p.set(x + k, oy + 1, (k == 0) ? ROCK[1] : mixColor(SKY_MID, SKY_TOP, 0.4f));
        }
    }
    for (int i = 0; i < 26; i++) { // limpets and barnacles
        int x = ox - 8 + (int)(hash01(seed * 5 + i) * (w + 4));
        int y = oy + 3 + (int)(hash01(seed * 9 + i) * (tide - oy));
        if (m.at(x, y) && m.depth[y * p.width + x] > 1.5f) p.set(x, y, ROCK[5]);
    }
    for (int y = std::max(0, tide); y < p.height; y++) { // wet rock and weed below the tide line
        for (int x = 0; x < p.width; x++) {
            if (!m.at(x, y)) continue;
            float k = std::clamp((y - tide) / 6.0f, 0.0f, 1.0f);
            p.set(x, y, mixColor(p.get(x, y), SEAWEED[0], 0.55f * k));
        }
    }
    for (int i = 0; i < 18; i++) { // strands of weed hanging at the tide line
        int x = ox - 10 + (int)(hash01(seed * 13 + i) * (w + 6));
        int len = 2 + (int)(hash01(seed * 17 + i) * 5.0f);
        for (int k = 0; k < len; k++) {
            int y = tide - 2 + k;
            if (m.at(x, y)) p.set(x + ((k % 3 == 2) ? 1 : 0), y, SEAWEED[(k == 0) ? 2 : 1]);
        }
    }
}

// The sea far off: fog over a pale horizon, a headland on the left, and the harbor's coast on the
// right, faint, where the boats will go.
void paintFar(Picture& far) {
    for (int y = HORIZON; y < CANVAS_HEIGHT; y++) {
        float t = (y - HORIZON) / (float)(CANVAS_HEIGHT - HORIZON);
        Color row = mixColor(SEA_FAR, SEA_NEAR, std::pow(t, 0.8f));
        for (int x = 0; x < far.width; x++) {
            Color c = row;
            float swell = noise2(x * 0.045f, y * 0.9f, 31);
            if (swell > 0.68f) c = mixColor(c, FOAM, 0.18f + 0.25f * (1.0f - t));
            else if (swell < 0.3f) c = mixColor(c, SEA_NEAR, 0.12f);
            far.set(x, y, c);
        }
    }
    for (int x = 0; x < far.width; x++) far.set(x, HORIZON, mixColor(SEA_FAR, FOAM, 0.45f)); // the horizon's line of light
    for (int x = 0; x < 190; x++) { // a headland on the left, fading into the fog
        float h = 58.0f * smooth01((190.0f - x) / 150.0f) + (noise2(x * 0.08f, 0.0f, 7) - 0.5f) * 8.0f;
        int top = HORIZON - (int)h;
        for (int y = top; y < HORIZON; y++) far.set(x, y, FAR_LAND[(y - top < 1) ? 2 : 1]);
    }
    for (int x = 470; x < far.width; x++) { // the harbor's coast on the right, low and far
        float h = 10.0f + 8.0f * smooth01((x - 470.0f) / 120.0f) + (noise2(x * 0.1f, 0.0f, 9) - 0.5f) * 4.0f;
        int top = HORIZON - (int)h;
        for (int y = top; y < HORIZON; y++) far.set(x, y, FAR_LAND[(y - top < 2) ? 2 : 0]);
    }
    for (int i = 0; i < 9; i++) { // white houses and a harbor light, very small
        int x = 520 + i * 11 + (int)(hash01(i * 7 + 1) * 5.0f);
        int y = HORIZON - 9 - (int)(hash01(i * 5 + 2) * 6.0f);
        far.rect(x, y, 3, 2, FAR_LAND[3]);
        far.set(x, y - 1, Color{176, 128, 112, 255});
    }
    far.rect(500, HORIZON - 16, 2, 9, FAR_LAND[3]);
    fogRows(far, FOG, 120, 250, 0.62f, 0.25f);
}

// Sea stacks standing in the water, streaked white by the birds that nest on them.
void paintMid(Picture& mid) {
    Mask m(mid.width, CANVAS_HEIGHT);
    struct Stack { float x; int top; int half; };
    const Stack stacks[3] = {{438.0f, 258, 13}, {470.0f, 296, 8}, {590.0f, 280, 11}};
    for (int i = 0; i < 3; i++) {
        for (int y = stacks[i].top; y < CANVAS_HEIGHT; y++) {
            int dy = y - stacks[i].top;
            int half = stacks[i].half + dy / 9 + (int)std::round(std::sin(y * 0.3f + i) * 1.5f);
            m.span(y, (int)stacks[i].x - half, (int)stacks[i].x + half + (dy < 3 ? -2 : 0));
        }
    }
    m.measure();
    Shading s;
    s.ramp = ROCK;
    s.steps = 6;
    s.toLight = TO_SUN;
    s.base = 0.34f;
    s.rim = 0.4f;
    s.bevel = 2.0f;
    s.grain = 0.15f;
    s.seed = 41;
    s.dither = false;
    shade(mid, m, s);
    for (int i = 0; i < 40; i++) { // white streaks
        int x = (int)(stacks[i % 3].x) - stacks[i % 3].half + (int)(hash01(i * 7 + 2) * stacks[i % 3].half * 2.0f);
        int y = stacks[i % 3].top + 2 + (int)(hash01(i * 3 + 5) * 26.0f);
        for (int k = 0; k < 3; k++) {
            if (m.at(x, y + k)) mid.set(x, y + k, ROCK[5]);
        }
    }
    fogRows(mid, FOG, 240, CANVAS_HEIGHT, 0.38f, 0.18f);
}

// The headland the tower stands on, and the tower itself: white stone with red bands, slit
// windows, the sea door at its foot, the sockets the stair stones slide out of, the gallery and
// the lamp room, and the iron arm the fog bell hangs from.
void paintTowerLayer(Picture& p, const World& world) {
    Mask land(p.width, CANVAS_HEIGHT);
    for (int x = 0; x < 262; x++) {
        float top = 268.0f + ((x > 140) ? (x - 140) * 0.27f : 0.0f) + (noise2(x * 0.07f, 0.0f, 3) - 0.5f) * 6.0f;
        land.span((int)top, x, x + 1);
        for (int y = (int)top; y < CANVAS_HEIGHT; y++) land.set(x, y);
    }
    land.measure();
    Shading rock;
    rock.ramp = ROCK;
    rock.steps = 6;
    rock.toLight = TO_SUN;
    rock.base = 0.4f;
    rock.rim = 0.36f;
    rock.bevel = 3.0f;
    rock.grain = 0.16f;
    rock.grainScale = 0.22f;
    rock.seed = 12;
    rock.darkenFrom = 300.0f;
    rock.darkenRange = 50.0f;
    rock.darkenAmount = 0.3f;
    shade(p, land, rock);
    for (int x = 0; x < 262; x++) { // grass over the top, and a worn path toward the door
        int y = 0;
        while (y < CANVAS_HEIGHT && !land.at(x, y)) y++;
        int depth = 3 + (int)(noise2(x * 0.3f, 0.0f, 5) * 4.0f);
        bool path = x > 150 && x < 240 && std::fabs(noise2(x * 0.05f, 1.0f, 8) - 0.5f) < 0.2f;
        for (int k = 0; k < depth; k++) {
            Color c = path ? pickRamp(SANDSTONE, 5, 0.62f - k * 0.12f, x, y + k) : pickRamp(GRASS, 4, 0.9f - k * 0.22f, x, y + k);
            p.set(x, y + k, c);
        }
        if (!path && hash01(x * 13 + 7) > 0.55f) p.set(x, y - 1, GRASS[(x % 3 == 0) ? 3 : 2]); // tufts
    }

    // The tower: a tapered drum, lit from the right by the morning sun.
    for (int y = TOWER_TOP; y < TOWER_BASE; y++) {
        float half = towerHalf((float)y);
        int left = (int)std::floor(TOWER_X - half), right = (int)std::ceil(TOWER_X + half);
        bool band = (y >= 104 && y < 128) || (y >= 186 && y < 210);
        bool plinth = y >= TOWER_BASE - 14;
        for (int x = left; x < right; x++) {
            float u = (x + 0.5f - TOWER_X) / half; // -1 .. 1 across the drum
            float round = std::sqrt(std::fmax(0.0f, 1.0f - u * u));
            float value = 0.22f + 0.34f * round + 0.34f * (u * 0.5f + 0.5f) + (noise2(x * 0.4f, y * 0.4f, 17) - 0.5f) * 0.1f;
            const Color* ramp = band ? BAND : TOWER;
            if (plinth) value -= 0.12f;
            Color c = pickRamp(ramp, 5, value, x, y);
            if (y % 8 == 0) c = mixColor(c, ramp[0], 0.35f);                                 // courses of stone
            if (y % 8 != 0 && ((x + (y / 8) * 5) % 11) == 0) c = mixColor(c, ramp[0], 0.25f); // the joints
            if (x == left) c = ramp[0];
            p.set(x, y, c);
        }
    }
    for (int y = 140; y < 300; y++) { // salt and lichen low on the shaded side
        for (int x = TOWER_X - 50; x < TOWER_X - 10; x++) {
            if (!p.painted(x, y) || fbm2(x * 0.2f, y * 0.06f, 23, 3) < 0.64f) continue;
            p.set(x, y, mixColor(p.get(x, y), Color{132, 146, 118, 255}, 0.5f));
        }
    }
    const int windows[3][2] = {{TOWER_X - 7, 78}, {TOWER_X + 9, 150}, {TOWER_X - 5, 232}};
    for (const auto& win : windows) { // slit windows, warm from the lamp's stair inside
        for (int y = 0; y < 9; y++) {
            for (int x = 0; x < 3; x++) {
                if (y == 0 && x != 1) continue; // an arched top
                p.set(win[0] + x, win[1] + y, (y < 2) ? Color{120, 84, 52, 255} : Color{68, 52, 44, 255});
            }
        }
        p.rect(win[0] - 1, win[1] + 9, 5, 1, TOWER[4]); // a sill
    }
    for (int y = 254; y < 300; y++) { // the sea door's arched frame (the door itself is drawn live)
        for (int x = TOWER_X - 14; x <= TOWER_X + 13; x++) {
            float dx = x + 0.5f - TOWER_X, dy = y + 0.5f - 268.0f;
            bool inArch = (y >= 268) ? std::fabs(dx) < 13.0f : (dx * dx + dy * dy < 13.0f * 13.0f);
            bool inOpening = (y >= 268) ? std::fabs(dx) < 10.0f : (dx * dx + dy * dy < 10.0f * 10.0f);
            if (inArch && !inOpening) p.set(x, y, ((x + y) % 5 == 0) ? TOWER[1] : TOWER[2]);
            if (inOpening) p.set(x, y, DARK);
        }
    }
    for (int id = 2; id <= World::STONE_SLOTS; id++) { // the sockets the stair stones slide out of
        Rectangle slot = world.getStoneSlot(id);
        int top = toCanvasY(slot.y);
        int wall = (int)std::ceil(TOWER_X + towerHalf((float)top));
        const Color tint = STONE_TINT[id - 2];
        for (int y = top - 1; y < top + 12; y++) {
            for (int x = wall - 4; x < wall; x++) {
                bool rim = y == top - 1 || y == top + 11 || x == wall - 4;
                p.set(x, y, rim ? IRON : mixColor(SANDSTONE[1], tint, 0.3f)); // the stone's end, waiting in the wall
            }
        }
    }
    // The gallery: an iron walkway with a railing.
    for (int x = TOWER_X - 44; x <= TOWER_X + 44; x++) {
        p.set(x, 50, IRON);
        p.set(x, 51, IRON);
        p.set(x, 52, (x % 2 == 0) ? IRON : IRON_LIT);
        if ((x - TOWER_X) % 4 == 0) {
            for (int y = 42; y < 50; y++) p.set(x, y, IRON);
        }
        p.set(x, 41, IRON_LIT);
    }
    for (int k = 0; k < 6; k++) { // brackets under the walkway
        int x = TOWER_X - 38 + k * 15;
        for (int d = 0; d < 6; d++) p.set(x + ((x < TOWER_X) ? d : -d) / 2, 53 + d, IRON);
    }
    // The lamp room: iron frame and glass, the lamp inside (its light is drawn live).
    for (int y = 22; y < 42; y++) {
        for (int x = TOWER_X - 20; x <= TOWER_X + 20; x++) {
            bool frame = (x - (TOWER_X - 20)) % 8 == 0 || y == 22 || y == 41;
            float lit = (x - TOWER_X) / 20.0f;
            p.set(x, y, frame ? IRON : mixColor(GLASS, SUN, 0.15f + 0.2f * lit));
        }
    }
    for (int y = 27; y < 38; y++) { // the lens
        for (int x = TOWER_X - 5; x <= TOWER_X + 5; x++) {
            if ((x - TOWER_X) * (x - TOWER_X) + (y - 32) * (y - 32) * 2 > 40) continue;
            p.set(x, y, ((x + y) % 3 == 0) ? Color{255, 236, 170, 255} : Color{236, 190, 110, 255});
        }
    }
    for (int y = 10; y < 22; y++) { // the dome, and a vent ball on top
        float half = 22.0f * std::sqrt(std::fmax(0.0f, 1.0f - ((21.5f - y) / 12.0f) * ((21.5f - y) / 12.0f)));
        for (int x = (int)(TOWER_X - half); x <= (int)(TOWER_X + half); x++) {
            float u = (x - TOWER_X) / std::fmax(1.0f, half);
            p.set(x, y, pickRamp(BAND, 5, 0.25f + 0.45f * (u * 0.5f + 0.5f), x, y));
        }
    }
    for (int y = 5; y < 10; y++) {
        for (int x = TOWER_X - 2; x <= TOWER_X + 2; x++) {
            if ((x - TOWER_X) * (x - TOWER_X) + (y - 7) * (y - 7) <= 5) p.set(x, y, (x > TOWER_X) ? IRON_LIT : IRON);
        }
    }
    p.rect(TOWER_X, 1, 1, 5, IRON); // the weathervane
    p.rect(TOWER_X + 1, 2, 4, 1, IRON);
    // The bell's iron arm: from the wall under the gallery, out over the highest stone.
    const int armY = 57;
    for (int x = TOWER_X + 36; x <= (int)BELL_X + 2; x++) {
        p.set(x, armY, IRON);
        p.set(x, armY - 1, IRON_LIT);
    }
    for (int k = 0; k < 9; k++) { // a strut back to the wall
        p.set(TOWER_X + 37 + k * 2, armY + 9 - k, IRON);
        p.set(TOWER_X + 38 + k * 2, armY + 9 - k, IRON);
    }
    p.rect((int)BELL_X - 1, armY + 1, 3, 3, IRON); // the hanger
}

// A small tuft of sea grass for the headland (it bends with the breeze).
Picture paintTuft(int height, int seed) {
    Picture p(9, height + 1);
    for (int b = 0; b < 4; b++) {
        int h = height - (int)(hash01(seed * 7 + b) * height * 0.5f);
        int x = 1 + b * 2;
        for (int k = 0; k < h; k++) p.set(x + ((k > h / 2 && b % 2 == 0) ? 1 : 0), height - k, GRASS[(k == h - 1) ? 3 : ((k > h / 2) ? 2 : 1)]);
    }
    return p;
}

void bakeLighthouse(RegionArt& art, const World& world) {
    const RegionInfo& info = world.getRegion(4);
    const int range = scrollRangeOf(info); // 0: the whole stair fits on one screen

    Picture sky(CANVAS_WIDTH, CANVAS_HEIGHT);
    ditherGradient(sky, 0, 110, SKY_TOP, SKY_MID);
    ditherGradient(sky, 110, CANVAS_HEIGHT, SKY_MID, SKY_LOW);
    for (int y = -15; y <= 15; y++) { // the sun, a pale disc in the fog
        for (int x = -15; x <= 15; x++) {
            float d = std::sqrt((float)(x * x + y * y));
            if (d > 14.5f) continue;
            sky.set((int)SUN_X + x, (int)SUN_Y + y, mixColor(SUN, SKY_LOW, 0.12f + 0.3f * (d / 14.5f) * (d / 14.5f)));
        }
    }
    storeLayer(art, sky, PARALLAX[SKY_LAYER], 0);

    Picture far(layerWidth(range, PARALLAX[FAR_LAYER]), CANVAS_HEIGHT);
    paintFar(far);
    storeLayer(art, far, PARALLAX[FAR_LAYER], 0);

    Picture mid(layerWidth(range, PARALLAX[MID_LAYER]), CANVAS_HEIGHT);
    paintMid(mid);
    storeLayer(art, mid, PARALLAX[MID_LAYER], 0);

    Picture tower(layerWidth(range, PARALLAX[TOWER_LAYER]), CANVAS_HEIGHT);
    paintTowerLayer(tower, world);
    storeLayer(art, tower, PARALLAX[TOWER_LAYER], 0);
    for (int i = 0; i < 7; i++) {
        Picture tuft = paintTuft(5 + (int)(hash01(i * 3 + 1) * 4.0f), 30 + i);
        float x = 12.0f + i * 31.0f + hash01(i * 5) * 12.0f;
        float top = 268.0f + ((x > 140.0f) ? (x - 140.0f) * 0.27f : 0.0f) + (noise2(x * 0.07f, 0.0f, 3) - 0.5f) * 6.0f;
        storeSway(art, tuft, TOWER_LAYER, false, x, top - tuft.height + 2.0f, 1.4f, false, 3, 0.1f * i);
    }

    // The floor stone and every stair stone, by ID. The floor stone exists now; the others are
    // painted ahead from the layout push() will give them, and drawn only while they exist.
    const Node* top = world.getHead(4);
    for (int id = 1; id <= World::STONE_SLOTS; id++) {
        Rectangle slot = world.getStoneSlot(id);
        for (const Node* node = top; node != nullptr; node = node->next) { // a stone that exists: use its real node
            if (node->id == id) slot = getPlatformRect(*node);
        }
        LedgeCanvas c;
        c.width = (int)(slot.width / CANVAS_SCALE);
        c.top = toCanvasY(slot.y);
        if (id == 1) {
            c.originX = 24;
            c.originY = 8;
            c.picture = Picture(c.width + 48, CANVAS_HEIGHT - c.top + c.originY);
            paintFloorStone(c);
        } else {
            c.originX = 6;
            c.originY = 3;
            c.picture = Picture(c.width + 12, 26);
            paintStone(c, id);
        }
        storeLedgeById(art, id, c);
    }

    // Foreground: dark rocks in the surf at the bottom corners.
    Picture front(layerWidth(range, 1.1f), CANVAS_HEIGHT);
    Mask rocks(front.width, CANVAS_HEIGHT);
    rocks.blob(18.0f, CANVAS_HEIGHT + 6.0f, 34.0f, 5, 0.3f);
    rocks.blob(56.0f, CANVAS_HEIGHT + 14.0f, 22.0f, 6, 0.3f);
    rocks.blob((float)front.width - 30.0f, CANVAS_HEIGHT + 8.0f, 36.0f, 7, 0.3f);
    rocks.measure();
    Shading rockStyle;
    rockStyle.ramp = ROCK;
    rockStyle.steps = 6;
    rockStyle.toLight = TO_SUN;
    rockStyle.base = 0.12f;
    rockStyle.rim = 0.34f;
    rockStyle.bevel = 2.0f;
    rockStyle.seed = 80;
    rockStyle.outline = false;
    shade(front, rocks, rockStyle);
    storeLayer(art, front, 1.1f, 0, true);
}

// ---------------------------------------------------------------------------
// Live pieces: the stones sliding, the bell, the sea door, the sea
// ---------------------------------------------------------------------------

// A stair stone sliding out of its socket (out 0 -> 1). Only the part outside the wall is drawn.
void drawStone(const LedgeArt& art, int towerX, int left, int top, float out, float mist) {
    if (out <= 0.0f) return;
    const int x0 = left + art.dx; // the picture's left edge when the stone is all the way out
    const int y0 = top + art.dy;
    const int wall = (int)std::ceil(towerX + towerHalf((float)top));
    const int hidden = std::max(0, wall - x0);
    const int travel = art.picture.width - hidden;
    const int shift = (int)std::lround((1.0f - easeOut(out)) * travel);
    const int srcX = hidden + shift;
    const int srcW = art.picture.width - srcX;
    if (srcW <= 0) return;
    Rectangle source = {(float)srcX, 0.0f, (float)srcW, (float)art.picture.height};
    DrawTextureRec(art.picture, source, Vector2{(float)wall, (float)y0}, WHITE);
    if (mist > 0.01f) DrawTextureRec(art.mask, source, Vector2{(float)wall, (float)y0}, withAlpha(VEIL, 0.55f * mist));
}

// The fog bell, swinging from its hanger after it is rung. `swing` fades from 1 to 0.
void drawBell(float x, float y, float swing, float time) {
    float angle = swing * 0.45f * std::sin(time * 5.4f);
    const int rows = 15;
    for (int r = 0; r < rows; r++) {
        float half = (r < 2) ? 2.0f : ((r < 5) ? 3.5f + (r - 2) * 0.5f : ((r < 11) ? 5.0f + (r - 5) * 0.15f : 6.0f + (r - 10) * 0.6f));
        float cx = x + std::sin(angle) * r;
        float cy = y + std::cos(angle) * r;
        int left = (int)std::lround(cx - half), right = (int)std::lround(cx + half);
        for (int px = left; px <= right; px++) {
            float u = (px + 0.5f - cx) / std::fmax(1.0f, half);
            float value = 0.3f + 0.45f * (u * 0.5f + 0.5f) + ((std::fabs(u - 0.35f) < 0.14f) ? 0.3f : 0.0f);
            if (r == rows - 1) value -= 0.25f; // the lip in shadow
            int step = std::clamp((int)std::lround(value * 4.0f), 0, 4);
            DrawRectangle(px, (int)cy, 1, 1, BRONZE[step]);
        }
    }
    float clapper = angle * 1.6f; // the clapper lags behind the bell
    DrawRectangle((int)std::lround(x + std::sin(clapper) * 16.0f) - 1, (int)std::lround(y + std::cos(clapper) * 16.0f), 3, 2, IRON);
}

// The sea door in the tower's foot. It swings open (0 -> 1) once the stair has been taken down,
// and the daylight from the far side of the tower comes through it.
void drawSeaDoor(Rectangle gate, int cameraX, float open, float time) {
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    const int cx = gx + gw / 2;
    auto inside = [&](int x, int y) { // the arched opening
        float dx = x + 0.5f - cx, dy = y + 0.5f - (gy + 3.0f);
        return (y >= gy + 3) ? std::fabs(dx) < 10.0f : (dx * dx + dy * dy < 100.0f);
    };
    float light = smooth01(open);
    for (int y = gy - 8; y < gy + gh; y++) {
        for (int x = cx - 10; x < cx + 10; x++) {
            if (!inside(x, y)) continue;
            if (light > 0.0f) DrawRectangle(x, y, 1, 1, withAlpha(lerpColor(DAYLIGHT, LAMP, (y - gy + 8.0f) / (gh + 8.0f)), light));
        }
    }
    float panel = 1.0f - 0.85f * light; // the door swings in on its left hinge
    int doorRight = cx - 10 + (int)std::lround(20.0f * panel);
    for (int y = gy - 8; y < gy + gh; y++) {
        for (int x = cx - 10; x < doorRight; x++) {
            if (!inside(x, y)) continue;
            int plank = (x - (cx - 10)) * 4 / std::max(1, doorRight - (cx - 10));
            Color wood = DOOR_WOOD[(plank % 2 == 0) ? 1 : 2];
            if ((x - (cx - 10)) % 5 == 0) wood = DOOR_WOOD[0];
            if (y == gy + 4 || y == gy + gh - 8) wood = IRON; // iron bands
            DrawRectangle(x, y, 1, 1, lerpColor(wood, DOOR_WOOD[0], light * 0.6f));
        }
    }
    if (open <= 0.0f) {
        DrawRectangle(cx - 9, gy + 14, 18, 2, IRON); // the bar across it, until the stair is down
        DrawRectangle(cx + 5, gy + 18, 2, 2, IRON_LIT);
    }
    for (int i = 0; light > 0.0f && i < 6; i++) { // motes in the light
        float t = std::fmod(time * (0.3f + 0.1f * hash01(i * 3)) + i * 0.17f, 1.0f);
        int x = cx - 8 + (int)(hash01(i * 5 + 3) * 16.0f);
        DrawRectangle(x, gy + gh - 2 - (int)(t * gh), 1, 1, withAlpha(WHITE, light * loopEnvelope(t)));
    }
}

// The sea where it meets the rocks: a sheet of water over their feet, swell lines, and surf that
// washes up and drains away.
void drawNearSea(const SceneView& view, float lift) {
    const float time = view.time;
    for (int y = SEA_Y; y < CANVAS_HEIGHT; y++) {
        float k = (y - SEA_Y) / (float)(CANVAS_HEIGHT - SEA_Y);
        DrawRectangle(0, y, CANVAS_WIDTH, 1, withAlpha(lerpColor(Color{96, 124, 140, 255}, SEA_NEAR, k), 0.55f + 0.35f * k));
    }
    for (int i = 0; i < 26; i++) { // light on the swell
        float x = std::fmod(hash01(i * 7 + 2) * 700.0f + time * (6.0f + (i % 3) * 3.0f) + 3500.0f, 700.0f) - 30.0f;
        int y = SEA_Y + 2 + (int)(hash01(i * 11 + 5) * (CANVAS_HEIGHT - SEA_Y - 3));
        float shimmer = 0.5f + 0.5f * std::sin(time * (1.2f + 0.5f * hash01(i * 3)) + i * 2.1f);
        DrawRectangle((int)x, y, 5 + (int)(hash01(i * 13) * 10.0f), 1, withAlpha(FOAM, (0.1f + 0.16f * shimmer) * (1.0f + 0.4f * lift)));
    }
    // Surf along the rocks: it runs up, foams, and drains back, in waves that roll in from the right.
    const int surfX[3][2] = {{0, 250}, {470, 500}, {560, 620}};
    for (int s = 0; s < 3; s++) {
        float wave = 0.5f + 0.5f * std::sin(time * 0.9f - s * 1.3f);
        int rise = (int)std::lround(wave * 3.0f);
        for (int x = surfX[s][0]; x < surfX[s][1]; x++) {
            float n = noise2(x * 0.15f, time * 0.4f, 60 + s);
            if (n < 0.35f) continue;
            DrawRectangle(x, SEA_Y - rise + (int)(n * 2.0f), 1, 1 + (int)(wave * 2.0f), withAlpha(FOAM, 0.55f + 0.35f * n));
        }
    }
    for (int x = 250; x < 480; x++) { // foam along the floor stone's foot
        float n = noise2(x * 0.12f, time * 0.5f, 70);
        int y = SEA_Y - 1 - (int)((0.5f + 0.5f * std::sin(time * 0.9f - 0.7f)) * 2.0f);
        if (n > 0.42f) DrawRectangle(x, y + (int)(n * 2.0f), 1, 1, withAlpha(FOAM, 0.4f + 0.5f * n));
    }
}

// ---------------------------------------------------------------------------
// Region 5 drawing
// ---------------------------------------------------------------------------

void drawLighthouseBackground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    const float lift = fogLift(view);
    drawBackLayers(view, art, SKY_LAYER, FAR_LAYER, 0.8f);
    drawBackLayers(view, art, FAR_LAYER, MID_LAYER, 0.8f);
    for (int i = 0; i < 6; i++) { // fog banks drifting over the far sea, thinning once the bell has rung
        float x = std::fmod(i * 170.0f - time * (2.5f + 0.6f * i) + 4000.0f, 1020.0f) - 190.0f;
        int y = 150 + (int)(hash01(i * 9 + 1) * 80.0f);
        DrawEllipse((int)x, y, 150.0f + 30.0f * (i % 3), 14.0f + 4.0f * (i % 2), withAlpha(FOG, (0.34f - 0.1f * (i % 2)) * (1.0f - lift)));
    }
    for (int i = 0; lift > 0.01f && i < 5; i++) { // boats setting out for the harbor, called by the bell
        float t = (view.stageTime - 2.0f - i * 1.6f) / 24.0f;
        if (t <= 0.0f || t > 1.0f) continue;
        float x = 200.0f + t * 330.0f + i * 14.0f;
        int y = HORIZON + 6 + (i % 3) * 4;
        Color boat = withAlpha(Color{88, 96, 110, 255}, std::fmin(1.0f, lift * 2.5f) * std::fmin(1.0f, (1.0f - t) * 6.0f));
        DrawRectangle((int)x - 2, y, 5, 1, boat);
        drawTriangleSafe(Vector2{x, (float)y - 5}, Vector2{x, (float)y}, Vector2{x + 3.0f, (float)y}, withAlpha(Color{236, 232, 220, 255}, boat.a / 255.0f));
    }
    for (int i = 0; i < 5; i++) { // gulls far out over the sea
        float a = time * (0.25f + 0.05f * i) + i * 1.7f;
        float x = 470.0f + std::cos(a) * (40.0f + i * 9.0f);
        float y = 175.0f + std::sin(a * 1.3f) * 14.0f + i * 5.0f;
        drawGull(x, y, 2.0f, std::sin(time * 6.0f + i * 1.9f), withAlpha(Color{112, 118, 128, 255}, 0.7f));
    }
    drawBackLayers(view, art, MID_LAYER, TOWER_LAYER, 0.8f);
    for (int i = 0; i < 3; i++) { // spray bursting against the sea stacks
        float t = std::fmod(time * 0.32f + i * 0.37f, 1.0f);
        float x = layerToScreen((i == 0) ? 438.0f : ((i == 1) ? 470.0f : 590.0f), view.scroll, PARALLAX[MID_LAYER]);
        for (int k = 0; k < 6; k++) {
            float px = x - 10.0f + k * 4.0f + std::sin(k * 1.7f + i) * 2.0f;
            float py = SEA_Y - 2.0f - std::sin(t * 3.14159f) * (8.0f + k % 3 * 3.0f);
            DrawRectangle((int)px, (int)py, 1, 1, withAlpha(FOAM, 0.6f * std::sin(t * 3.14159f)));
        }
    }
    for (int i = 0; i < 4; i++) { // nearer fog, between the sea stacks and the tower
        float x = std::fmod(i * 260.0f - time * 4.0f + 4000.0f, 1100.0f) - 220.0f;
        DrawEllipse((int)x, 248 + i * 14, 160.0f, 10.0f, withAlpha(FOG, 0.16f * (1.0f - lift)));
    }
    drawBackLayers(view, art, TOWER_LAYER, LAYER_COUNT, 1.0f);
}

void drawLighthousePlatforms(const SceneView& view, const RegionArt& art) {
    const int towerX = (int)layerToScreen((float)TOWER_X, view.scroll, PARALLAX[TOWER_LAYER]);
    if (view.hasGate) drawSeaDoor(view.gateRect, view.cameraX, view.gateProgress, view.time);

    // The floor stone and the stones of the stair, walked from the top down.
    const Node* top = view.head;
    const Node* node = top;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        const LedgeArt* ledge = findLedgeArt(art, node->id);
        if (ledge != nullptr) {
            Ledge l = ledgeFor(*node, view.cameraX);
            float mist = ledgeMist(view, *node);
            if (node->id == 1) {
                DrawTexture(ledge->picture, l.left + ledge->dx, l.top + ledge->dy, WHITE);
                if (mist > 0.01f) DrawTexture(ledge->mask, l.left + ledge->dx, l.top + ledge->dy, withAlpha(VEIL, 0.45f * mist));
            } else {
                drawStone(*ledge, towerX, l.left, l.top, arrival(view, node->id, STONE_SLIDE_SECONDS), mist);
            }
        }
        node = node->next;
    }
    for (int id = 2; view.removedById != nullptr && id <= World::STONE_SLOTS; id++) { // stones just popped, sliding home
        const RemovedPiece& piece = view.removedById[id];
        float t = (view.time - piece.at) / STONE_SLIDE_SECONDS;
        if (!piece.active || t >= 1.0f) continue;
        const LedgeArt* ledge = findLedgeArt(art, id);
        if (ledge != nullptr) drawStone(*ledge, towerX, toCanvasX(piece.rect.x, view.cameraX), toCanvasY(piece.rect.y), 1.0f - t, 0.0f);
    }

    // Light on the stone you can reach: a stone just pushed glows warm; after the bell, the stone
    // under the top glows cool, where the next pop will set you down.
    const float arrive = arrivalEase(view);
    if (top != nullptr && view.current != nullptr && top != view.current) {
        drawLinkLip(ledgeFor(*top, view.cameraX), WARM_LIGHT, arrival(view, top->id, STONE_SLIDE_SECONDS) * arrive, view.time, true);
    } else if (view.stage >= 1 && !view.gateOpen && view.current != nullptr && view.current->next != nullptr) {
        drawLinkLip(ledgeFor(*view.current->next, view.cameraX), COOL_LIGHT, 0.6f * arrive, view.time, false);
    }

    // The keeper's lantern on the floor stone.
    for (const Node* n = top; n != nullptr; n = n->next) {
        if (n->id != 1) continue;
        Ledge floor = ledgeFor(*n, view.cameraX);
        drawLanternPost(floor.left + 196, floor.top, view.checkpoint == n ? view.checkpointFlash : 0.0f, view.time);
    }

    drawBell((float)towerX + (BELL_X - TOWER_X), BELL_Y, bellSwing(view), view.stageTime);
}

void drawLighthouseForeground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    const float lift = fogLift(view);
    drawNearSea(view, lift);
    for (int i = 0; i < 12; i++) { // spray drifting up off the rocks
        float t = std::fmod(time * (0.2f + 0.05f * (i % 4)) + hash01(i * 7) , 1.0f);
        float x = std::fmod(hash01(i * 11 + 3) * 640.0f + t * 40.0f, 640.0f);
        float y = SEA_Y - t * 40.0f;
        DrawRectangle((int)x, (int)y, 1, 1, withAlpha(FOAM, 0.5f * loopEnvelope(t)));
    }
    for (int i = 0; i < 5; i++) { // fog drifting past in front
        float x = std::fmod(i * 230.0f - time * (7.0f + i) + 4000.0f, 1100.0f) - 220.0f;
        int y = 60 + (int)(hash01(i * 13 + 2) * 230.0f);
        DrawEllipse((int)x, y, 120.0f + 40.0f * (i % 2), 9.0f, withAlpha(FOG, 0.12f * (1.0f - 0.8f * lift)));
    }
    for (int i = 0; i < 3; i++) { // gulls wheeling around the tower
        float a = time * (0.45f + 0.08f * i) + i * 2.1f;
        float x = TOWER_X + std::cos(a) * (70.0f + i * 18.0f);
        float y = 70.0f + std::sin(a) * (16.0f + i * 6.0f) + i * 22.0f;
        bool behind = std::sin(a) < -0.2f && std::fabs(x - TOWER_X) < 40.0f; // hidden while they pass behind the tower
        if (!behind) drawGull(x, y, 3.0f, std::sin(time * 7.0f + i * 2.3f), Color{246, 246, 242, 255});
    }
    drawFrontLayers(view, art, 1.0f);
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 40, withAlpha(FOG, 0.25f * (1.0f - lift)), withAlpha(FOG, 0.0f));
}

void drawLighthouseAdditive(const SceneView& view, const RegionArt&, const SceneRenderer& renderer) {
    const float time = view.time;
    const float lift = fogLift(view);
    const float towerX = layerToScreen((float)TOWER_X, view.scroll, PARALLAX[TOWER_LAYER]);
    renderer.drawSoftLight(Vector2{SUN_X, SUN_Y}, 150.0f, SUN, 0.1f + 0.14f * lift);
    renderer.drawSoftLight(Vector2{SUN_X, SUN_Y}, 36.0f, SUN, 0.18f + 0.18f * lift);

    // The lamp's two beams turning: long rays through the fog to either side, and a flash each
    // time one faces you. They fade as the fog lifts.
    const Vector2 lamp = {towerX + (LAMP_X - TOWER_X), LAMP_Y};
    float phase = time * 0.8f;
    float fog = 1.0f - 0.7f * lift;
    for (int side = 0; side < 2; side++) {
        float reach = std::sin(phase) * (side == 0 ? 1.0f : -1.0f);
        if (reach <= 0.0f) continue;
        float dir = (side == 0) ? 1.0f : -1.0f;
        float length = 560.0f * (0.35f + 0.65f * reach);
        Vector2 a = {lamp.x + dir * length, lamp.y - 16.0f * reach - 4.0f};
        Vector2 b = {lamp.x + dir * length, lamp.y + 36.0f * reach + 6.0f};
        Vector2 top = {lamp.x + dir * 4.0f, lamp.y - 4.0f};
        Vector2 bottom = {lamp.x + dir * 4.0f, lamp.y + 5.0f};
        drawGradientQuad(top, bottom, b, a, withAlpha(LAMP, 0.34f * fog * reach), withAlpha(LAMP, 0.34f * fog * reach),
                         withAlpha(LAMP, 0.0f), withAlpha(LAMP, 0.0f));
    }
    float flash = std::pow(std::fabs(std::cos(phase)), 12.0f);
    renderer.drawSoftLight(lamp, 26.0f + 30.0f * flash, LAMP, 0.45f + 0.45f * flash * fog);

    float swing = bellSwing(view);
    if (swing > 0.02f) { // the bell's ring, a shimmer that spreads and fades
        Vector2 bell = {towerX + (BELL_X - TOWER_X), BELL_Y + 8.0f};
        for (int k = 0; k < 3; k++) {
            float t = std::fmod(view.stageTime * 0.9f + k / 3.0f, 1.0f);
            renderer.drawSoftLight(bell, 10.0f + t * 70.0f, WARM_LIGHT, 0.18f * swing * (1.0f - t));
        }
    }
    if (view.hasGate && view.gateProgress > 0.0f) {
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height * 0.6f);
        renderer.drawSoftLight(Vector2{x, y}, 50.0f, DAYLIGHT, 0.35f * smooth01(view.gateProgress));
    }
}

void addLighthouseLights(const SceneView& view, const RegionArt&, std::vector<Light>& lights) {
    const float time = view.time;
    const float towerX = layerToScreen((float)TOWER_X, view.scroll, PARALLAX[TOWER_LAYER]);
    float flicker = 0.92f + 0.08f * std::sin(time * 9.0f);
    lights.push_back(Light{Vector2{towerX + (LAMP_X - TOWER_X), LAMP_Y}, 120.0f, LAMP, 0.45f * flicker, 12.0f, 0.55f});
    const Node* top = view.head;
    if (top != nullptr && view.current != nullptr && top != view.current) {
        addLinkLight(lights, ledgeFor(*top, view.cameraX), WARM_LIGHT, arrival(view, top->id, STONE_SLIDE_SECONDS) * arrivalEase(view));
    }
    for (const Node* n = top; n != nullptr; n = n->next) {
        if (n->id != 1) continue;
        Ledge floor = ledgeFor(*n, view.cameraX);
        addLanternLight(lights, floor.left + 196, floor.top, view.checkpoint == n ? view.checkpointFlash : 0.0f, time);
    }
    if (view.hasGate && view.gateProgress > 0.0f) {
        float light = smooth01(view.gateProgress);
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height / 2.0f);
        lights.push_back(Light{Vector2{x, y}, 80.0f * light, DAYLIGHT, 0.55f * light, 22.0f * light, 0.45f * light});
    }
}

Mood lighthouseMood(const SceneView& view) {
    float lift = fogLift(view);
    Mood mood;
    mood.skyLight = WHITE;
    mood.groundLight = lerpColor(Color{186, 188, 190, 255}, Color{214, 212, 204, 255}, lift);
    mood.lightFrom = 60;
    mood.lightTo = 320;
    mood.shadowTint = Vector3{0.95f, 0.99f, 1.06f};
    mood.highlightTint = Vector3{1.03f, 1.0f, 0.95f};
    mood.saturation = 0.94f + 0.12f * lift;
    mood.bloom = 0.36f;
    mood.vignette = 0.32f;
    return mood;
}

} // namespace

const RegionPainter LIGHTHOUSE_PAINTER = {bakeLighthouse, drawLighthouseBackground, drawLighthousePlatforms, nullptr,
                                          drawLighthouseForeground, drawLighthouseAdditive, addLighthouseLights, lighthouseMood};
