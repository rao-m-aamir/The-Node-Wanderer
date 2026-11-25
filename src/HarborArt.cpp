#include "ArtCommon.h"

#include <algorithm>

// Region 6, "The Patient Harbor": a bright late morning in the harbor below the lighthouse. The
// bell has called the boats. Each one that is called rows in to the end of the line (enqueue at
// the rear), and the boat the traveler steps off rows home (dequeue at the front), so the line
// creeps across the harbor toward the ship that waits last of all. Everything that moves is drawn
// from the real queue: the boats from its nodes, the ones leaving from what was kept of them.

namespace {

const Color SKY_TOP = {84, 148, 214, 255};
const Color SKY_MID = {142, 196, 236, 255};
const Color SKY_LOW = {214, 234, 242, 255};
const Color SUN = {255, 250, 228, 255};
const Color SEA_FAR = {60, 120, 170, 255};
const Color SEA_NEAR = {64, 150, 172, 255};
const Color WATER_TOP = {74, 166, 186, 255};
const Color WATER_DEEP = {22, 80, 118, 255};
const Color FOAM = {240, 250, 250, 255};
const Color VEIL = {214, 232, 238, 255}; // sea haze over boats that cannot be reached now
const Color DARK = {20, 30, 40, 255};
const Color CLOUD[4] = {{186, 204, 222, 255}, {212, 226, 238, 255}, {234, 242, 248, 255}, {252, 254, 255, 255}};
const Color HAZE = {196, 220, 234, 255};

const Color WOOD[5] = {{56, 38, 28, 255}, {86, 60, 42, 255}, {122, 88, 60, 255}, {164, 124, 86, 255}, {204, 168, 120, 255}};
const Color STONE[5] = {{96, 104, 110, 255}, {128, 136, 140, 255}, {160, 166, 166, 255}, {192, 196, 192, 255}, {222, 224, 218, 255}};
const Color WALL[4] = {{196, 190, 178, 255}, {222, 216, 202, 255}, {240, 236, 224, 255}, {252, 250, 242, 255}};
const Color ROOF[4] = {{136, 62, 44, 255}, {172, 84, 56, 255}, {204, 112, 72, 255}, {226, 146, 98, 255}};
const Color HILL[4] = {{86, 122, 74, 255}, {108, 142, 82, 255}, {140, 166, 96, 255}, {178, 190, 120, 255}};
const Color TREE[4] = {{40, 70, 52, 255}, {56, 92, 62, 255}, {78, 118, 74, 255}, {110, 146, 88, 255}};
const Color SAIL = {240, 232, 212, 255};
const Color SAIL_SHADE = {206, 194, 170, 255};
const Color ROPE = {150, 126, 92, 255};
const Color LAMP_WARM = {255, 214, 140, 255};

// Each boat's paint: hull, its lighter top, and the rail.
struct BoatPaint {
    Color hull, hullLight, rail;
    bool mast;
};
const BoatPaint BOATS[6] = {
    {{178, 58, 48, 255}, {214, 96, 74, 255}, {240, 232, 218, 255}, false}, // the red skiff
    {{52, 96, 160, 255}, {86, 136, 196, 255}, {232, 222, 196, 255}, false}, // the blue dory
    {{56, 120, 88, 255}, {92, 158, 112, 255}, {210, 190, 140, 255}, false}, // the green punt
    {{200, 150, 48, 255}, {226, 184, 84, 255}, {120, 76, 48, 255}, false},  // the yellow gig
    {{226, 226, 220, 255}, {246, 246, 240, 255}, {60, 104, 150, 255}, true}, // the white yawl
    {{112, 78, 54, 255}, {150, 110, 76, 255}, {196, 168, 118, 255}, true},   // the old lugger
};

const Vector2 TO_SUN = {0.5f, -0.86f}; // the sun high over the sea, a little to the right

enum Layer { SKY_LAYER, FAR_LAYER, MID_LAYER, ROUTE_LAYER, LAYER_COUNT };
const float PARALLAX[LAYER_COUNT] = {0.0f, 0.08f, 0.3f, 1.0f};
const float FRONT_PARALLAX = 1.25f;

const int WATERLINE = 308; // canvas y of the harbor's surface (world 616)
const int HORIZON = 196;
const float SUN_X = 560.0f;
const float SUN_Y = 34.0f;
const int SHIP_ID = 7;
const int BREAKWATER_END = 600; // mid-layer x of the harbor light, where the harbor opens to the sea

float easeOut(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return 1.0f - (1.0f - t) * (1.0f - t);
}

float easeIn(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t;
}

// The town hill's outline (rows of the mid layer): low at the far left, where the cape with the
// lighthouse shows behind it, level over the town, then down to the harbor.
float hillTopAt(float x) {
    return 150.0f + ((x < 50.0f) ? (50.0f - x) * 0.9f : 0.0f) + ((x > 120.0f) ? (x - 120.0f) * 0.55f : 0.0f);
}

float ledgeMist(const SceneView& view, const Node& node) {
    if (view.mistById == nullptr || node.id < 0 || node.id > MAX_NODES_PER_REGION) return 0.0f;
    return view.mistById[node.id];
}

// How long a node of this ID takes to come in: the ship is slower than the boats.
float arriveSeconds(int id) {
    return (id == SHIP_ID) ? BOAT_ARRIVE_SECONDS * 1.6f : BOAT_ARRIVE_SECONDS;
}

float arrival(const SceneView& view, int id) {
    if (view.createdById == nullptr || id < 0 || id > MAX_NODES_PER_REGION) return 1.0f;
    return std::clamp((view.time - view.createdById[id]) / arriveSeconds(id), 0.0f, 1.0f);
}

void drawGull(float x, float y, float span, float flap, Color color) {
    int s = (int)span;
    int lift = (int)std::lround(flap * 2.0f);
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

// A small wooden boat: rail, planked hull with a dark stripe at the water, an oar shipped over the
// side, and for the two sailing boats a mast with the sail furled. The walkable top is the rail.
void paintBoat(LedgeCanvas& c, int id) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const BoatPaint& paint = BOATS[std::clamp(id - 1, 0, 5)];
    const int freeboard = WATERLINE - c.top; // rail to water
    const int depth = freeboard + 6;         // the hull goes on under the water
    const bool flat = id == 3;               // the punt has square ends
    for (int y = oy; y < oy + depth && y < p.height; y++) {
        int d = y - oy;
        float t = d / (float)depth;
        int stern = flat ? 1 + d / 4 : 2 + (int)(t * t * 14.0f);     // the left end rounds under
        int bow = flat ? 1 + d / 4 : 1 + (int)(t * t * t * 26.0f);   // the bow curves up to the right
        int left = ox + stern - 3, right = ox + w - bow + 3;
        for (int x = left; x < right; x++) {
            Color col;
            if (d < 2) {
                col = (d == 0) ? paint.rail : mixColor(paint.rail, DARK, 0.25f); // the rail
            } else if (d >= freeboard - 2 && d < freeboard + 1) {
                col = mixColor(paint.hull, DARK, 0.65f); // the dark stripe at the water
            } else {
                float lit = 1.0f - t * 1.4f;
                col = mixColor(paint.hull, paint.hullLight, std::clamp(lit, 0.0f, 1.0f) * 0.7f);
                if (d % 3 == 2) col = mixColor(col, DARK, 0.18f); // planks
            }
            if (x == left || x == right - 1) col = mixColor(col, DARK, 0.35f);
            p.set(x, y, col);
        }
    }
    if (!flat) { // the sheer: the rail sweeps up to the bow, and a little to the stern
        const int left = ox - 1, right = ox + w + 2; // the hull's ends at the rail
        for (int x = left; x < right; x++) {
            float toBow = std::clamp((x - (right - 18)) / 18.0f, 0.0f, 1.0f);
            float toStern = std::clamp((left + 7 - x) / 7.0f, 0.0f, 1.0f);
            int rise = (int)std::lround(std::fmax(4.0f * toBow * toBow, 2.0f * toStern));
            for (int k = 1; k <= rise; k++) {
                Color col = (k == rise) ? paint.rail : mixColor(paint.hull, paint.hullLight, 0.7f);
                if (x == left || x == right - 1) col = mixColor(col, DARK, 0.35f);
                p.set(x, oy - k, col);
            }
        }
    }
    for (int k = 0; k < 3; k++) p.set(ox + 8 + k * (w - 16) / 2, oy + 2, mixColor(paint.rail, DARK, 0.5f)); // rowlocks
    for (int k = 0; k < 16; k++) { // an oar shipped over the side, its blade in the water
        int x = ox + w / 2 - 4 + k;
        int y = oy - 1 + (int)(k * 0.7f);
        if (y < oy + 2) continue;
        p.set(x, y, WOOD[3]);
    }
    p.rect(ox + w / 2 + 12, oy + freeboard - 3, 5, 2, WOOD[2]);
    for (int k = 0; k < 6; k++) { // a coil of rope by the stern
        float a = k * 1.05f;
        p.set(ox + 10 + (int)std::lround(std::cos(a) * 2.5f), oy - 1 + (int)std::lround(std::sin(a) * 1.0f), ROPE);
    }
    if (paint.mast) { // a mast with its sail furled along the boom
        const int mx = ox + w / 2 + 6;
        const int height = (id == 6) ? 44 : 38;
        for (int y = oy - height; y < oy; y++) {
            p.set(mx, y, WOOD[1]);
            p.set(mx + 1, y, WOOD[3]);
        }
        const int boom = oy - ((id == 6) ? 36 : 30); // the lugger's yard high, the yawl's boom: both clear of the traveler
        for (int x = mx - 22; x <= mx + 3; x++) p.set(x, boom, WOOD[1]); // the boom
        Color sail = (id == 6) ? Color{176, 84, 62, 255} : SAIL;
        for (int x = mx - 21; x <= mx + 1; x++) { // the furled sail, bundled on the boom
            int bulge = (int)std::lround(2.0f + std::sin((x - mx) * 0.5f) * 0.8f);
            for (int y = boom - bulge; y < boom; y++) p.set(x, y, (y == boom - bulge) ? mixColor(sail, WHITE, 0.3f) : sail);
        }
        for (int k = 0; k < height - 4; k += 2) p.set(mx + 2 + k / 3, oy - height + 2 + k, mixColor(ROPE, DARK, 0.2f)); // a stay
        p.rect(mx - 1, oy - height - 3, 4, 3, (id == 6) ? Color{176, 84, 62, 255} : Color{60, 104, 150, 255}); // a pennant
    }
    if (id == 2 || id == 4) { // a stern lantern on a pole
        p.rect(ox + 3, oy - 10, 1, 10, WOOD[1]);
        p.rect(ox + 1, oy - 14, 4, 4, WOOD[0]);
        p.rect(ox + 2, oy - 13, 2, 2, LAMP_WARM);
    }
}

// The ship that waits at the end of the line: a dark hull with a cream wale, a cabin with its door
// at the right end of the deck, two masts with their sails half furled, and the rigging.
void paintShip(LedgeCanvas& c) {
    Picture& p = c.picture;
    const int ox = c.originX, oy = c.originY, w = c.width;
    const int freeboard = WATERLINE - c.top;
    const int depth = freeboard + 10;
    const int length = w + 140; // the bow runs on past the deck you can stand on
    for (int y = oy; y < oy + depth && y < p.height; y++) {
        int d = y - oy;
        float t = d / (float)depth;
        int stern = 4 + (int)(t * t * 18.0f);
        int bow = (int)(t * t * 60.0f);
        int left = ox - 14 + stern, right = ox + length - bow;
        for (int x = left; x < right; x++) {
            Color col = pickRamp(WOOD, 5, 0.42f - t * 0.35f + ((x - left) % 9 == 0 ? -0.08f : 0.0f), x, y);
            if (d < 2) col = WOOD[4];                                                  // the rail cap
            else if (d >= 9 && d < 12) col = mixColor(WALL[2], WOOD[2], 0.15f);        // the wale
            else if (d >= 15 && d < 17) col = Color{30, 112, 108, 255};                // a teal stripe
            else if (d >= freeboard - 3 && d < freeboard + 1) col = mixColor(WOOD[0], DARK, 0.5f);
            if (d >= 9 && d < 12 && (x - left) % 14 == 6) col = DARK;                    // little windows in the wale
            p.set(x, y, col);
        }
    }
    // Two masts with yards and half-furled sails, and the rigging down to the rails.
    const int mastX[2] = {ox + 18, ox + 74};
    const int mastTop[2] = {oy - 186, oy - 160};
    for (int m = 0; m < 2; m++) {
        for (int y = mastTop[m]; y < oy; y++) {
            p.set(mastX[m], y, WOOD[1]);
            p.set(mastX[m] + 1, y, WOOD[3]);
        }
        for (int k = 0; k < 2; k++) { // two yards on each mast
            int yy = mastTop[m] + 30 + k * 52;
            int half = 30 - k * 4;
            for (int x = mastX[m] - half; x <= mastX[m] + half; x++) p.set(x, yy, WOOD[1]);
            int drop = (k == 0) ? 38 : 10; // the upper sail is set, the lower one furled
            for (int x = mastX[m] - half + 2; x <= mastX[m] + half - 2; x++) {
                float u = (x - mastX[m]) / (float)half;
                int belly = (int)std::lround(drop * (0.92f + 0.08f * std::cos(u * 1.6f)));
                for (int y = yy + 1; y <= yy + belly; y++) {
                    float shade = 0.5f + 0.5f * u;
                    Color col = mixColor(SAIL_SHADE, SAIL, shade);
                    if ((x - mastX[m]) % 10 == 0) col = mixColor(col, SAIL_SHADE, 0.5f); // seams
                    p.set(x, y, col);
                }
            }
        }
        for (int k = 0; k < 40; k++) { // shrouds from the masthead to the rail
            float t = k / 40.0f;
            p.set(mastX[m] - (int)(t * 26.0f), mastTop[m] + 4 + (int)(t * (oy - mastTop[m] - 4)), mixColor(ROPE, DARK, 0.3f));
            p.set(mastX[m] + (int)(t * 26.0f), mastTop[m] + 4 + (int)(t * (oy - mastTop[m] - 4)), mixColor(ROPE, DARK, 0.3f));
        }
    }
    // The cabin on the deck, at its right end (its door is drawn live; it is the way out).
    const int cabinLeft = ox + w - 38, cabinRight = ox + w + 2;
    for (int y = oy - 34; y < oy; y++) {
        for (int x = cabinLeft; x < cabinRight; x++) {
            Color col = pickRamp(WALL, 4, 0.55f + 0.35f * (x - cabinLeft) / 40.0f, x, y);
            if ((x - cabinLeft) % 8 == 0) col = WOOD[2];
            if (y < oy - 30) col = (y == oy - 34) ? WOOD[4] : WOOD[2]; // the roof
            p.set(x, y, col);
        }
    }
    p.rect(cabinLeft + 4, oy - 26, 5, 5, Color{80, 120, 150, 255}); // a window
    p.rect(cabinLeft + 5, oy - 25, 3, 3, Color{170, 210, 226, 255});
    for (int k = 1; k <= 10; k++) p.set(mastX[0], mastTop[0] - k, WOOD[1]); // the main topmast
    for (int k = 0; k < 8; k++) { // and a pennant flying from it, out with the breeze
        for (int y = 0; y < 3 - k / 3; y++) p.set(mastX[0] + 1 + k, mastTop[0] - 10 + y + k / 4, Color{30, 112, 108, 255});
    }
    for (int x = ox - 12; x < ox + w + 120; x += 4) p.set(x, oy - 6, mixColor(ROPE, DARK, 0.2f)); // a lifeline along the deck
    for (int x = ox - 12; x < ox + w + 120; x += 12) {
        for (int y = oy - 6; y < oy; y++) p.set(x, y, WOOD[2]); // its stanchions
    }
    // A swallow painted on the stern.
    const int sx = ox - 4, sy = oy + 5;
    p.rect(sx, sy, 4, 1, Color{30, 112, 108, 255});
    p.rect(sx - 2, sy - 1, 2, 1, Color{30, 112, 108, 255});
    p.rect(sx + 4, sy - 1, 2, 1, Color{30, 112, 108, 255});
}

// The far side: the open sea, a headland on the left with the lighthouse the traveler came from,
// and islands far out on the right.
void paintFar(Picture& far) {
    for (int y = HORIZON; y < CANVAS_HEIGHT; y++) {
        float t = (y - HORIZON) / (float)(WATERLINE - HORIZON);
        Color row = mixColor(SEA_FAR, SEA_NEAR, std::clamp(t, 0.0f, 1.0f));
        for (int x = 0; x < far.width; x++) {
            Color c = row;
            float n = noise2(x * 0.06f, y * 0.8f, 41);
            if (n > 0.7f) c = mixColor(c, FOAM, 0.12f + 0.12f * (1.0f - t));
            far.set(x, y, c);
        }
    }
    for (int x = 0; x < far.width; x++) far.set(x, HORIZON, mixColor(SEA_FAR, SKY_LOW, 0.6f));
    for (int x = 0; x < 150; x++) { // the headland and the lighthouse on it
        float h = 34.0f * smooth01((150.0f - x) / 120.0f) + (noise2(x * 0.1f, 0.0f, 5) - 0.5f) * 5.0f;
        int top = HORIZON - (int)h;
        for (int y = top; y < HORIZON; y++) far.set(x, y, mixColor(HILL[(y - top < 2) ? 3 : 1], HAZE, 0.55f));
    }
    const int lx = 20, ly = HORIZON - 34; // the tower, small and far, where the town hill is low
    for (int y = ly - 18; y < ly; y++) {
        int half = 2 + (y - (ly - 18)) / 9;
        for (int x = lx - half; x <= lx + half; x++) {
            bool band = (y - (ly - 18)) % 7 < 2;
            far.set(x, y, mixColor(band ? Color{180, 78, 64, 255} : WALL[3], HAZE, 0.3f));
        }
    }
    far.rect(lx - 1, ly - 21, 3, 3, mixColor(Color{60, 60, 66, 255}, HAZE, 0.3f));
    for (int i = 0; i < 3; i++) { // islands far out
        int cx = 380 + i * 110 + (int)(hash01(i * 3) * 30.0f);
        int half = 18 + (int)(hash01(i * 5 + 1) * 20.0f);
        for (int x = cx - half; x <= cx + half; x++) {
            float u = (x - cx) / (float)half;
            int h = (int)(std::sqrt(std::fmax(0.0f, 1.0f - u * u)) * (6.0f + 4.0f * hash01(i * 7)));
            for (int y = HORIZON - h; y < HORIZON; y++) far.set(x, y, mixColor(HILL[1], HAZE, 0.7f));
        }
    }
    fogRows(far, HAZE, HORIZON - 40, HORIZON + 30, 0.25f, 0.05f);
}

// The harbor town on its hill, a breakwater across the middle distance with a light at its end,
// and boats moored behind it.
void paintMid(Picture& mid) {
    for (int x = 0; x < mid.width; x++) { // the hill behind the town, sloping down to the harbor
        float top = hillTopAt((float)x) + (noise2(x * 0.03f, 0.0f, 13) - 0.5f) * 14.0f;
        if (top > 234.0f) continue;
        for (int y = (int)top; y < 237; y++) {
            float t = (y - top) / 30.0f;
            mid.set(x, y, pickRamp(HILL, 4, 0.75f - std::fmin(0.4f, t * 0.3f), x, y));
        }
        for (int y = 237; y < 240; y++) mid.set(x, y, pickRamp(STONE, 5, 0.7f - (y - 237) * 0.14f, x, y)); // the quay along the town
        mid.set(x, 240, mixColor(FOAM, SEA_NEAR, 0.5f)); // the wash at its foot
    }
    for (int i = 0; i < 26; i++) { // white houses with red roofs, stepping down the hill
        int x = 6 + i * 11 + (int)(hash01(i * 7 + 2) * 6.0f);
        if (x > 300) break;
        float hillTop = hillTopAt((float)x);
        int base = std::min(237, (int)hillTop + 18 + (int)(hash01(i * 5 + 1) * 34.0f));
        int width = 8 + (int)(hash01(i * 11 + 3) * 6.0f);
        int height = 7 + (int)(hash01(i * 13 + 4) * 6.0f);
        for (int y = base - height; y < base; y++) {
            for (int x2 = x; x2 < x + width; x2++) {
                mid.set(x2, y, pickRamp(WALL, 4, 0.5f + 0.4f * (x2 - x) / (float)width, x2, y));
            }
        }
        for (int k = 0; k < width / 2 + 2; k++) { // the roof
            for (int x2 = x - 1 + k; x2 < x + width + 1 - k; x2++) mid.set(x2, base - height - 1 - k / 2, ROOF[(k % 2 == 0) ? 1 : 2]);
        }
        if (width > 9) mid.rect(x + 2, base - height + 2, 2, 2, Color{70, 96, 128, 255}); // windows
        mid.rect(x + width - 4, base - 4, 2, 3, Color{90, 70, 56, 255});
    }
    for (int i = 0; i < 9; i++) { // trees among the houses
        int x = 20 + i * 34 + (int)(hash01(i * 3 + 9) * 12.0f);
        float hillTop = hillTopAt((float)x);
        if (hillTop > 218.0f) continue; // past here the hill runs into the harbor: no ground for a tree
        paintFoliage(mid, (float)x, hillTop + 12.0f + hash01(i) * 20.0f, 5.0f + hash01(i * 5) * 3.0f, TREE, 4, TO_SUN, 300 + i, 0.3f);
    }
    for (int y = 172; y < 214; y++) { // a bell tower in the town, the twin of the lighthouse's bell
        for (int x = 140; x < 150; x++) {
            Color c = pickRamp(WALL, 4, 0.3f + 0.07f * (x - 140), x, y); // in shade on the side away from the sun
            if ((y - 172) % 9 == 8) c = WALL[0];                           // a string course
            mid.set(x, y, c);
        }
    }
    for (int k = 0; k < 7; k++) mid.rect(139 + k / 2, 172 - k, 12 - k, 1, ROOF[(k < 2) ? 0 : 1]);
    mid.rect(143, 177, 4, 6, Color{60, 50, 44, 255}); // the belfry, and its bell
    mid.rect(144, 179, 2, 2, Color{196, 160, 84, 255});
    mid.rect(144, 194, 2, 4, Color{70, 96, 128, 255}); // a window lower down
    // The breakwater: a long stone wall across the harbor, from a heap of rubble at its left end to
    // its light, where the harbor opens to the sea and the ship waits.
    const int wallLeft = 214;
    for (int x = wallLeft; x < BREAKWATER_END; x++) {
        int top = 252 + (int)(std::sin(x * 0.02f) * 1.5f);
        const bool rubble = x < wallLeft + 16;
        if (rubble) top = 266 - (int)std::lround((x - wallLeft + 1) / 16.0f * (266 - top)); // sloping into the water
        for (int y = top; y < 266; y++) {
            Color c = pickRamp(STONE, 5, (y == top) ? 0.92f : 0.5f - (y - top) * 0.02f, x, y);
            if (rubble ? hash01(x * 31 + y * 17) > 0.72f : ((x + (y / 4) * 6) % 13 == 0 && y > top)) c = STONE[0];
            mid.set(x, y, c);
        }
    }
    for (int y = 226; y < 252; y++) { // the harbor light
        for (int x = BREAKWATER_END - 8; x < BREAKWATER_END; x++) mid.set(x, y, ((y / 5) % 2 == 0) ? WALL[3] : ROOF[1]);
    }
    mid.rect(BREAKWATER_END - 9, 222, 10, 4, Color{60, 60, 66, 255});
    for (int i = 0; i < 5; i++) { // masts of boats moored behind the breakwater
        int x = 290 + i * 58 + (int)(hash01(i * 7) * 24.0f);
        mid.rect(x, 222 - (i % 3) * 6, 1, 32 + (i % 3) * 6, WOOD[1]);
        mid.rect(x - 6, 240 - (i % 2) * 3, 13, 2, (i % 2 == 0) ? WALL[2] : BOATS[i % 6].hull);
    }
    fogRows(mid, HAZE, 140, 270, 0.32f, 0.12f);
}

// The route layer: the old jetty at the left, and a mooring post for every place in the line.
// Each post's lamp is recorded in `spots` (by boat ID), so the next one can be lit live.
void paintRoute(Picture& route, const World& world, float regionLeft, std::vector<Vector2>& spots) {
    spots.assign(World::BOAT_SLOTS + 1, Vector2{0.0f, 0.0f});
    for (int x = 0; x < 52; x++) { // the jetty's deck and its piles
        for (int y = 296; y < 300; y++) route.set(x, y, pickRamp(WOOD, 5, (y == 296) ? 0.85f : 0.5f, x, y));
        if (x % 5 == 0) route.set(x, 297, WOOD[0]);
    }
    for (int k = 0; k < 4; k++) {
        int x = 4 + k * 14;
        for (int y = 300; y < WATERLINE + 10; y++) {
            route.set(x, y, WOOD[1]);
            route.set(x + 1, y, WOOD[2]);
            route.set(x + 2, y, WOOD[0]);
        }
    }
    for (int id = 1; id <= World::BOAT_SLOTS; id++) { // a post where each boat ties up
        Rectangle slot = world.getBoatSlot(id);
        int x = localX(slot.x, regionLeft) - 6;
        int top = toCanvasY(slot.y) - 8;
        if (id == SHIP_ID) {
            x = localX(slot.x, regionLeft) - 24;
            top = toCanvasY(slot.y) + 16;
        }
        for (int y = top; y < WATERLINE + 10; y++) {
            route.set(x, y, WOOD[2]);
            route.set(x + 1, y, WOOD[3]);
            route.set(x + 2, y, WOOD[1]);
            if ((y - top) % 9 == 4) route.set(x + 1, y, WOOD[0]);
        }
        route.rect(x - 1, top - 1, 5, 2, WOOD[4]);
        route.rect(x, top - 5, 3, 4, Color{58, 60, 66, 255}); // a lamp on the post (lit for the next boat)
        route.rect(x + 1, top - 4, 1, 2, Color{120, 112, 96, 255});
        spots[id] = Vector2{(float)x + 1.0f, (float)top - 4.0f};
        for (int k = 0; k < 4; k++) route.set(x - 1 + k, top + 6, ROPE); // a rope ring
    }
}

// A puffy cloud: a few lumps, lit from above.
Picture paintCloud(float r, int seed) {
    int size = (int)(r * 3.8f);
    Picture p(size, (int)(size * 0.6f));
    float cx = size * 0.5f, cy = p.height * 0.62f;
    paintFoliage(p, cx - r * 0.85f, cy + r * 0.12f, r * 0.62f, CLOUD, 4, Vector2{0.2f, -0.98f}, seed + 1, 0.2f);
    paintFoliage(p, cx + r * 0.9f, cy + r * 0.18f, r * 0.55f, CLOUD, 4, Vector2{0.2f, -0.98f}, seed + 2, 0.2f);
    paintFoliage(p, cx, cy - r * 0.1f, r, CLOUD, 4, Vector2{0.2f, -0.98f}, seed, 0.2f);
    for (int y = 0; y < p.height; y++) { // a flat, shaded underside
        for (int x = 0; x < p.width; x++) {
            if (p.painted(x, y) && y > cy + r * 0.35f) p.set(x, y, mixColor(p.get(x, y), CLOUD[0], 0.5f));
        }
    }
    return p;
}

void bakeHarbor(RegionArt& art, const World& world) {
    const RegionInfo& info = world.getRegion(5);
    const int range = scrollRangeOf(info);
    art.waterline = WATERLINE;

    Picture sky(CANVAS_WIDTH, CANVAS_HEIGHT);
    ditherGradient(sky, 0, 120, SKY_TOP, SKY_MID);
    ditherGradient(sky, 120, HORIZON + 4, SKY_MID, SKY_LOW);
    ditherGradient(sky, HORIZON + 4, CANVAS_HEIGHT, SKY_LOW, SKY_LOW);
    storeLayer(art, sky, PARALLAX[SKY_LAYER], 0);

    Picture far(layerWidth(range, PARALLAX[FAR_LAYER]), CANVAS_HEIGHT);
    paintFar(far);
    storeLayer(art, far, PARALLAX[FAR_LAYER], 0);

    Picture mid(layerWidth(range, PARALLAX[MID_LAYER]), CANVAS_HEIGHT);
    paintMid(mid);
    storeLayer(art, mid, PARALLAX[MID_LAYER], 0);

    Picture route(layerWidth(range, PARALLAX[ROUTE_LAYER]), CANVAS_HEIGHT);
    paintRoute(route, world, info.left, art.spots);
    storeLayer(art, route, PARALLAX[ROUTE_LAYER], 0);

    for (int i = 0; i < 4; i++) { // clouds, drawn drifting (see drawHarborBackground)
        art.extra.push_back(paintCloud(14.0f + hash01(i * 7 + 1) * 10.0f, 500 + i * 11).upload());
    }

    // Every boat the line will hold, and the ship, by ID. The ones in the queue now are painted
    // from their real nodes; the rest ahead from the layout enqueue() will give them.
    const Node* front = world.getHead(5);
    for (int id = 1; id <= World::BOAT_SLOTS; id++) {
        Rectangle slot = world.getBoatSlot(id);
        for (const Node* node = front; node != nullptr; node = node->next) {
            if (node->id == id) slot = getPlatformRect(*node);
        }
        LedgeCanvas c;
        c.width = (int)(slot.width / CANVAS_SCALE);
        c.top = toCanvasY(slot.y);
        const bool ship = id == SHIP_ID;
        const int above = ship ? 204 : (BOATS[id - 1].mast ? 50 : 18);
        c.originX = ship ? 30 : 12;
        c.originY = above;
        const int width = ship ? c.width + 30 + 150 : c.width + 24;
        c.picture = Picture(width, above + (WATERLINE - c.top) + 14);
        if (ship) paintShip(c);
        else paintBoat(c, id);
        storeLedgeById(art, id, c);
    }

    // Foreground: the near piles of the old jetty.
    Picture front2(layerWidth(range, FRONT_PARALLAX), CANVAS_HEIGHT);
    for (int k = 0; k < 2; k++) {
        int x = 6 + k * 22;
        int top = 280 + k * 12;
        for (int y = top; y < CANVAS_HEIGHT; y++) {
            for (int dx = 0; dx < 6; dx++) {
                Color c = (dx == 4) ? mixColor(WOOD[1], DARK, 0.25f) : mixColor(WOOD[0], DARK, 0.4f); // lit on the sun's side
                if ((y - top) % 23 == 9 && dx > 0 && dx < 4) c = mixColor(WOOD[0], DARK, 0.7f); // a split in the wood
                if (dx == 0 || dx == 5) c = DARK;
                front2.set(x + dx, y, c);
            }
        }
        for (int dx = 1; dx < 5; dx++) front2.set(x + dx, top, mixColor(WOOD[2], DARK, 0.3f)); // the sawn top
    }
    storeLayer(art, front2, FRONT_PARALLAX, 0, true);
}

// ---------------------------------------------------------------------------
// Live pieces: boats coming and going, the ship's door, the water
// ---------------------------------------------------------------------------

// Where a boat is drawn while it rows in (t 0 -> 1) or away (`leaving`, t 0 -> 1).
Vector2 boatOffset(int id, float t, bool leaving, float time) {
    if (leaving) {
        return Vector2{-easeIn(t) * 96.0f, std::sin(time * 2.6f + id) * 0.6f + t * 2.0f};
    }
    float distance = (id == SHIP_ID) ? 260.0f : 150.0f;
    return Vector2{(1.0f - easeOut(t)) * distance, std::sin(t * 3.14159f * 2.0f) * (1.0f - t)};
}

void drawWake(int left, int width, float strength, float time, int seed) {
    if (strength <= 0.02f) return;
    for (int k = 0; k < 14; k++) {
        int x = left + width + 2 + k * 3 + (int)(std::sin(time * 3.0f + k + seed) * 1.0f);
        DrawRectangle(x, WATERLINE - 1 + (k % 2), 2, 1, withAlpha(FOAM, strength * (1.0f - k / 14.0f)));
    }
}

void drawBoatPicture(const LedgeArt& art, int x, int y, float alpha, float mist) {
    DrawTexture(art.picture, x + art.dx, y + art.dy, withAlpha(WHITE, alpha));
    if (mist > 0.01f) DrawTexture(art.mask, x + art.dx, y + art.dy, withAlpha(VEIL, 0.5f * mist * alpha));
}

// The ship's cabin door: closed until the traveler is aboard, then it opens on warm light.
void drawShipDoor(Rectangle gate, int cameraX, float open, float time, float shipIn) {
    if (shipIn < 1.0f) return; // the door comes with the ship; it is part of it once it has moored
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    float light = smooth01(open);
    DrawRectangle(gx, gy + 6, gw, gh - 6, Color{40, 30, 24, 255});
    if (light > 0.0f) {
        DrawRectangleGradientV(gx + 1, gy + 7, gw - 2, gh - 7, withAlpha(Color{255, 236, 190, 255}, 0.95f * light),
                               withAlpha(LAMP_WARM, 0.75f * light));
        for (int i = 0; i < 5; i++) {
            float t = std::fmod(time * (0.3f + 0.1f * hash01(i)) + i * 0.2f, 1.0f);
            DrawRectangle(gx + 2 + (int)(hash01(i * 7 + 1) * (gw - 4)), gy + gh - 2 - (int)(t * (gh - 8)), 1, 1,
                          withAlpha(WHITE, light * loopEnvelope(t)));
        }
    }
    int panel = (int)std::lround((gw - 2) * (1.0f - 0.8f * light)); // the door swings in
    DrawRectangle(gx + 1, gy + 7, panel, gh - 7, WOOD[2]);
    DrawRectangle(gx + 1, gy + 7, panel, 1, WOOD[4]);
    if (panel > 4) DrawRectangle(gx + panel - 2, gy + gh / 2 + 2, 1, 2, Color{210, 180, 90, 255}); // the handle
    DrawRectangle(gx - 1, gy + 5, gw + 2, 1, WOOD[4]);
}

// ---------------------------------------------------------------------------
// Region 6 drawing
// ---------------------------------------------------------------------------

void drawHarborBackground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    drawBackLayers(view, art, SKY_LAYER, FAR_LAYER, 0.6f);
    for (size_t i = 0; i < art.extra.size(); i++) { // clouds drifting slowly over the sea
        const Texture2D& cloud = art.extra[i];
        float x = std::fmod(i * 210.0f + time * (1.6f + 0.5f * i) - view.scroll * 0.04f + 4000.0f, 900.0f) - 160.0f;
        int y = 22 + (int)(hash01((int)i * 9 + 2) * 80.0f);
        DrawTexture(cloud, (int)x, y, withAlpha(WHITE, 0.92f));
    }
    drawBackLayers(view, art, FAR_LAYER, MID_LAYER, 0.6f);
    for (int i = 0; i < 4; i++) { // sails far out at sea
        float x = std::fmod(300.0f + i * 160.0f + time * (1.2f + 0.4f * i) - view.scroll * PARALLAX[FAR_LAYER] + 4000.0f, 800.0f) - 80.0f;
        int y = HORIZON - 1 - (i % 2);
        DrawRectangle((int)x - 2, y, 5, 1, withAlpha(Color{70, 84, 100, 255}, 0.6f));
        drawTriangleSafe(Vector2{x, (float)y - 6.0f}, Vector2{x, (float)y}, Vector2{x + 3.0f, (float)y}, withAlpha(WHITE, 0.75f));
    }
    for (int i = 0; i < 4; i++) { // gulls riding the wind over the town
        float a = time * (0.3f + 0.06f * i) + i * 1.9f;
        float x = layerToScreen(120.0f + i * 60.0f, view.scroll, 0.2f) + std::cos(a) * 40.0f;
        float y = 120.0f + std::sin(a * 1.3f) * 12.0f + i * 8.0f;
        drawGull(x, y, 2.0f, std::sin(time * 5.0f + i), withAlpha(Color{90, 100, 112, 255}, 0.7f));
    }
    drawBackLayers(view, art, MID_LAYER, ROUTE_LAYER, 0.6f);
    { // the harbor light on the breakwater blinks
        float x = layerToScreen(BREAKWATER_END - 4.5f, view.scroll, PARALLAX[MID_LAYER]);
        float on = std::fmod(time, 3.0f) < 0.5f ? 1.0f : 0.0f;
        DrawRectangle((int)x - 1, 223, 3, 2, withAlpha(Color{255, 120, 90, 255}, 0.3f + 0.7f * on));
    }
    for (int i = 0; i < 3; i++) { // chimney smoke in the town, drifting with the breeze
        float baseX = layerToScreen(40.0f + i * 90.0f, view.scroll, PARALLAX[MID_LAYER]);
        for (int k = 0; k < 6; k++) {
            float t = std::fmod(time * 0.12f + k / 6.0f + i * 0.3f, 1.0f);
            DrawCircle((int)(baseX + t * 18.0f + std::sin(time + k) * 2.0f), (int)(186.0f - t * 30.0f + i * 10.0f), 2.0f + t * 3.0f,
                       withAlpha(Color{236, 238, 240, 255}, 0.35f * loopEnvelope(t)));
        }
    }
    drawBackLayers(view, art, ROUTE_LAYER, LAYER_COUNT, 0.6f);
}

void drawHarborPlatforms(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    // Boats that just left the line, rowing away toward the town.
    for (int id = 1; view.removedById != nullptr && id <= World::BOAT_SLOTS; id++) {
        const RemovedPiece& piece = view.removedById[id];
        float t = (time - piece.at) / BOAT_LEAVE_SECONDS;
        if (!piece.active || t >= 1.0f) continue;
        const LedgeArt* ledge = findLedgeArt(art, id);
        if (ledge == nullptr) continue;
        Vector2 off = boatOffset(id, t, true, time);
        int x = toCanvasX(piece.rect.x, view.cameraX) + (int)off.x, y = toCanvasY(piece.rect.y) + (int)off.y;
        drawBoatPicture(*ledge, x, y, 1.0f - easeIn(t), 0.0f);
    }

    // The boats in the line, walked from the front; each one rows in to its place when it is called.
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr; // checked before use
    const Node* rear = nullptr;
    const Node* node = view.head;
    float shipIn = 0.0f;
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        rear = node;
        const LedgeArt* ledge = findLedgeArt(art, node->id);
        Ledge l = ledgeFor(*node, view.cameraX);
        float t = arrival(view, node->id);
        if (node->id == SHIP_ID) shipIn = t;
        if (ledge != nullptr && l.left + 200 > -80 && l.left - 200 < CANVAS_WIDTH + 80) {
            Vector2 off = boatOffset(node->id, t, false, time);
            drawBoatPicture(*ledge, l.left + (int)off.x, l.top + (int)off.y, std::fmin(1.0f, t * 4.0f), ledgeMist(view, *node));
            drawWake(l.left + (int)off.x, (node->id == SHIP_ID) ? l.width + 140 : l.width, (1.0f - t) * 0.8f, time, node->id);
        }
        if (node == next) {
            drawLinkLip(l, WARM_LIGHT, arrivalEase(view) * smooth01((t - 0.6f) / 0.4f), time, true);
        }
        node = node->next;
    }

    // Past the rear, the lamp on the next empty post is lit: that is where the next boat will moor.
    if (view.moreToCome && rear != nullptr && rear->id + 1 < (int)art.spots.size()) {
        Vector2 lamp = art.spots[rear->id + 1];
        float pulse = 0.7f + 0.3f * std::sin(time * 3.0f);
        int x = (int)layerToScreen(lamp.x, view.scroll, PARALLAX[ROUTE_LAYER]);
        DrawRectangle(x, (int)lamp.y, 1, 2, withAlpha(LAMP_WARM, pulse));
        DrawRectangle(x, (int)lamp.y, 1, 1, withAlpha(WHITE, pulse));
    }
    if (view.hasGate) drawShipDoor(view.gateRect, view.cameraX, view.gateProgress, time, shipIn);
}

void drawHarborWater(const SceneView& view, const RegionArt&, const Texture2D& reflection) {
    const float time = view.time;
    const int depth = CANVAS_HEIGHT - WATERLINE;
    DrawRectangleGradientV(0, WATERLINE, CANVAS_WIDTH, depth, WATER_TOP, WATER_DEEP);
    for (int y = WATERLINE; y < CANVAS_HEIGHT; y++) {
        int k = y - WATERLINE;
        int row = WATERLINE - 1 - k; // the row of the scene mirrored at this depth
        if (row < 0) break;
        float wave = std::sin(time * 2.1f + y * 0.6f) + 0.5f * std::sin(time * 3.3f - y * 1.1f);
        int wobble = (int)std::lround(wave * (0.8f + k * 0.08f));
        float fade = 1.0f - (float)k / depth;
        Color tint = {176, 226, 232, (unsigned char)(255.0f * (0.5f * fade + 0.1f))};
        DrawTextureRec(reflection, Rectangle{(float)wobble, (float)(CANVAS_HEIGHT - 1 - row), (float)CANVAS_WIDTH, 1.0f},
                       Vector2{0.0f, (float)y}, tint);
    }
    DrawRectangle(0, WATERLINE, CANVAS_WIDTH, 1, withAlpha(FOAM, 0.35f));
    for (int i = 0; i < 46; i++) { // ripples, and the sun glinting on them
        float x = std::fmod(hash01(i * 7 + 2) * 760.0f + time * (4.0f + (i % 3) * 2.0f) - view.scroll + 3800.0f, 760.0f) - 60.0f;
        int y = WATERLINE + 2 + (int)(hash01(i * 11 + 5) * (depth - 3));
        float shimmer = 0.5f + 0.5f * std::sin(time * (1.6f + 0.7f * hash01(i * 3)) + i * 2.3f);
        int width = 3 + (int)(hash01(i * 13 + 1) * 10.0f);
        DrawRectangle((int)x, y, width, 1, withAlpha(FOAM, 0.08f + 0.16f * shimmer));
        float sun = std::fmax(0.0f, 1.0f - std::fabs(x - SUN_X) / 140.0f);
        if (shimmer > 0.85f && sun > 0.0f) DrawRectangle((int)x + width / 2, y, 1, 1, withAlpha(WHITE, sun));
    }
    const Node* node = view.head; // a line of foam where each moored hull meets the water
    for (int steps = 0; node != nullptr && steps < MAX_NODES_PER_REGION; steps++) {
        Ledge l = ledgeFor(*node, view.cameraX);
        float t = arrival(view, node->id);
        int left = l.left + (int)boatOffset(node->id, t, false, time).x;
        int width = (node->id == SHIP_ID) ? l.width + 120 : l.width;
        for (int x = left - 2; x < left + width + 2; x++) {
            if (noise2(x * 0.3f, time * 0.8f, node->id) > 0.4f) DrawRectangle(x, WATERLINE, 1, 1, withAlpha(FOAM, 0.75f));
        }
        node = node->next;
    }
}

void drawHarborForeground(const SceneView& view, const RegionArt& art) {
    const float time = view.time;
    for (int i = 0; i < 3; i++) { // gulls gliding past close by
        float t = std::fmod(time * 0.05f + i * 0.33f, 1.0f);
        float x = -40.0f + t * 720.0f;
        float y = 70.0f + i * 28.0f + std::sin(time * 0.8f + i) * 8.0f;
        drawGull(x, y, 4.0f, std::sin(time * 4.0f + i * 1.7f) * 0.6f, withAlpha(Color{250, 250, 248, 255}, loopEnvelope(t)));
    }
    drawFrontLayers(view, art, 0.6f);
}

void drawHarborAdditive(const SceneView& view, const RegionArt&, const SceneRenderer& renderer) {
    renderer.drawSoftLight(Vector2{SUN_X, SUN_Y}, 220.0f, SUN, 0.18f);
    renderer.drawSoftLight(Vector2{SUN_X, SUN_Y}, 36.0f, SUN, 0.5f);
    for (int i = 0; i < 12; i++) { // the glitter path under the sun
        float t = std::fmod(view.time * 0.5f + i * 0.21f, 1.0f);
        float x = SUN_X - 60.0f + hash01(i * 7) * 120.0f;
        float y = WATERLINE + 3.0f + hash01(i * 11) * 40.0f;
        renderer.drawSoftLight(Vector2{x, y}, 4.0f, WHITE, 0.4f * std::sin(t * 3.14159f));
    }
    if (view.hasGate && view.gateProgress > 0.0f) {
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height * 0.6f);
        renderer.drawSoftLight(Vector2{x, y}, 46.0f, LAMP_WARM, 0.35f * smooth01(view.gateProgress));
    }
}

void addHarborLights(const SceneView& view, const RegionArt& art, std::vector<Light>& lights) {
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;
    if (next != nullptr) addLinkLight(lights, ledgeFor(*next, view.cameraX), WARM_LIGHT, 0.6f * arrivalEase(view) * arrival(view, next->id));
    const Node* rear = view.head;
    while (rear != nullptr && rear->next != nullptr) rear = rear->next;
    if (view.moreToCome && rear != nullptr && rear->id + 1 < (int)art.spots.size()) { // the lit post for the next boat
        Vector2 lamp = art.spots[rear->id + 1];
        float x = layerToScreen(lamp.x, view.scroll, PARALLAX[ROUTE_LAYER]);
        lights.push_back(Light{Vector2{x, lamp.y + 1.0f}, 26.0f, LAMP_WARM, 0.4f, 6.0f, 0.55f + 0.2f * std::sin(view.time * 3.0f)});
    }
    if (view.hasGate && view.gateProgress > 0.0f) {
        float light = smooth01(view.gateProgress);
        float x = (float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX);
        float y = (float)toCanvasY(view.gateRect.y + view.gateRect.height / 2.0f);
        lights.push_back(Light{Vector2{x, y}, 70.0f * light, LAMP_WARM, 0.5f * light, 20.0f * light, 0.4f * light});
    }
}

Mood harborMood(const SceneView&) {
    Mood mood;
    mood.skyLight = WHITE;
    mood.groundLight = Color{238, 238, 234, 255};
    mood.lightFrom = 40;
    mood.lightTo = 300;
    mood.shadowTint = Vector3{0.96f, 1.0f, 1.05f};
    mood.highlightTint = Vector3{1.03f, 1.02f, 0.98f};
    mood.saturation = 1.05f;
    mood.bloom = 0.42f;
    mood.vignette = 0.24f;
    return mood;
}

} // namespace

const RegionPainter HARBOR_PAINTER = {bakeHarbor, drawHarborBackground, drawHarborPlatforms, drawHarborWater,
                                      drawHarborForeground, drawHarborAdditive, addHarborLights, harborMood};
