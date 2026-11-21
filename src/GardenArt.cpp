#include "ArtCommon.h"

// Region 3, "The Endless Garden": a moonlit garden around an ancient tree and a
// glowing spring. A great root arches from the tail's hollow back to the head:
// it is drawn between the real tail node and the node its next points to.

namespace {

const Color SKY_TOP = {16, 10, 36, 255};
const Color SKY_MID = {40, 28, 78, 255};
const Color SKY_LOW = {92, 70, 132, 255};
const Color MOON = {236, 228, 255, 255};
const Color FAR_HILLS = {56, 42, 96, 255};
const Color MID_GARDEN = {36, 26, 68, 255};
const Color MIST = {170, 150, 220, 255};
const Color VEIL = {110, 96, 150, 255};
const Color DEPTH = {20, 14, 40, 255};
const Color DARK = {8, 6, 18, 255};

const Color STONE_LIGHT = {200, 194, 218, 255};
const Color STONE = {132, 124, 160, 255};
const Color STONE_DARK = {84, 76, 114, 255};
const Color STONE_SHADE = {56, 48, 84, 255};
const Color MOSS_LIGHT = {150, 230, 190, 255};
const Color MOSS = {80, 170, 140, 255};
const Color MOSS_DARK = {40, 100, 96, 255};
const Color CAP_TOP = {176, 240, 232, 255};
const Color CAP = {92, 172, 196, 255};
const Color CAP_DARK = {50, 98, 140, 255};
const Color STALK = {204, 198, 224, 255};
const Color BARK_LIGHT = {156, 124, 136, 255};
const Color BARK = {102, 78, 98, 255};
const Color BARK_DARK = {62, 44, 68, 255};
const Color SPRING = {120, 255, 230, 255};
const Color BLOSSOM = {255, 150, 210, 255};
const Color PETAL = {255, 214, 236, 255};
const Color WISTERIA = {176, 130, 240, 255};

const LedgeMaterial GARDEN_STONE = {STONE_LIGHT, STONE, STONE_DARK, 4, STONE, STONE_SHADE, STONE_LIGHT, DEPTH};
const LedgeMaterial ROOT_BARK = {MOSS_LIGHT, MOSS, MOSS_DARK, 3, BARK, BARK_DARK, BARK_LIGHT, DEPTH};

const int SPRING_Y = 300; // canvas y of the spring's surface

// ---------------------------------------------------------------------------
// Platform shapes, one per node ID.
// ---------------------------------------------------------------------------

enum class GardenShape { Terrace, MoonCap, Planter, RootKnot };

GardenShape shapeFor(const Node& node) {
    switch (node.id) {
        case 2: return GardenShape::MoonCap;
        case 3: return GardenShape::Planter;
        case 4: return GardenShape::RootKnot;
        default: return GardenShape::Terrace;
    }
}

Silhouette buildGardenSilhouette(GardenShape shape, const Ledge& l) {
    Silhouette s;
    s.top = l.top;
    const int x0 = l.left;
    const int x1 = l.left + l.width;
    const int w = l.width;
    const int cx = x0 + w / 2;
    const float seed = (float)l.seed;
    for (int dy = 0; l.top + dy < CANVAS_HEIGHT; dy++) {
        switch (shape) {
            case GardenShape::Terrace: { // a stone retaining wall that steps out at its foot
                int inset = (dy < 4) ? 0 : (dy < 34 ? 1 : 1 - (dy - 34) / 8);
                if (inset < -5) inset = -5;
                s.add(dy, x0 + inset, x1 - inset);
                break;
            }
            case GardenShape::MoonCap: { // a flat-topped glowing mushroom cap on a pale stalk
                if (dy < 2) s.add(dy, x0, x1);
                else if (dy < 7) s.add(dy, x0 - 2, x1 + 2); // the rim of the cap
                else if (dy < 11) s.add(dy, x0 + (dy - 6) * 3, x1 - (dy - 6) * 3); // gills underneath
                else {
                    int half = w * 11 / 100 + (dy > 60 ? (dy - 60) / 12 : 0);
                    int wobble = (int)std::round(std::sin(dy * 0.08f + seed) * 2.0f);
                    s.add(dy, cx - half + wobble, cx + half + wobble);
                }
                break;
            }
            case GardenShape::Planter: { // a stone urn on a slim pedestal
                if (dy < 3) s.add(dy, x0, x1);
                else if (dy < 20) {
                    float t = (dy - 3) / 17.0f;
                    int inset = (int)(w * 0.32f * t * t);
                    s.add(dy, x0 + 2 + inset, x1 - 2 - inset);
                } else if (dy < 24) s.add(dy, cx - w / 5, cx + w / 5); // a carved collar
                else s.add(dy, cx - w / 8, cx + w / 8);
                break;
            }
            case GardenShape::RootKnot: { // a gnarled root knot over a thick root that dives into the spring
                if (dy < 3) s.add(dy, x0, x1);
                else if (dy < 22) {
                    int bulge = (int)std::round(3.0f * std::sin((dy - 3) * 0.16f)) + (int)std::round(std::sin(dy * 0.7f + seed));
                    s.add(dy, x0 - bulge, x1 + bulge);
                } else if (dy < 34) {
                    float t = (dy - 22) / 12.0f; // the knot narrows into the main root
                    int inset = (int)(w * 0.3f * t);
                    s.add(dy, x0 + inset, x1 - inset);
                } else {
                    int half = w * 20 / 100 + (int)std::round(std::sin(dy * 0.09f + seed) * 2.0f) + (dy > 90 ? (dy - 90) / 10 : 0);
                    int sway = (int)std::round(std::sin(dy * 0.05f) * 3.0f);
                    s.add(dy, cx - half + sway, cx + half + sway);
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

// Small glowing blossoms along a walkable edge (they sit on top, like grass).
void drawBlossoms(const Ledge& l, int count, float time) {
    for (int i = 0; i < count; i++) {
        int x = l.left + 2 + (int)(hash01(l.seed * 31 + i) * (l.width - 5));
        Color color = (i % 3 == 0) ? BLOSSOM : ((i % 3 == 1) ? SPRING : PETAL);
        float glow = 0.6f + 0.4f * std::sin(time * 2.0f + i * 1.7f);
        DrawRectangle(x, l.top - 2, 1, 2, MOSS_DARK);
        DrawRectangle(x - 1, l.top - 3, 3, 1, withAlpha(color, glow));
    }
}

void drawGardenDetails(GardenShape shape, const Ledge& l, const Silhouette& s, float time) {
    const int x0 = l.left;
    const int x1 = l.left + l.width;
    switch (shape) {
        case GardenShape::Terrace:
            for (int dy = 9; dy < 60; dy += 8) { // stone courses
                paintRows(s, dy, dy + 1, depthShade(STONE_SHADE, dy, DEPTH), 1);
                for (int x = x0 + 3 + ((dy / 8) % 2) * 8; x < x1 - 2; x += 16) {
                    for (int k = 1; k < 8; k++) if (insideSilhouette(s, x, dy + k)) DrawRectangle(x, l.top + dy + k, 1, 1, depthShade(STONE_SHADE, dy + k, DEPTH));
                }
            }
            paintRows(s, 1, 3, MOSS, 0);
            for (int i = 0; i < 5; i++) { // ivy over the wall
                int x = x0 + 6 + (int)(hash01(l.seed * 7 + i) * (l.width - 12));
                for (int k = 0; k < 10 + i * 3; k++) DrawRectangle(x + (k / 6) % 2, l.top + 3 + k, 1, 1, MOSS_DARK);
            }
            drawBlossoms(l, 7, time);
            break;
        case GardenShape::MoonCap: {
            paintRows(s, 0, 1, CAP_TOP, 0);
            paintRows(s, 1, 7, CAP, 0);
            paintRows(s, 6, 7, CAP_DARK, 0);
            paintRows(s, 7, 11, CAP_DARK, 0);
            for (int dy = 7; dy < 11; dy++) { // gills
                for (int x = x0; x < x1; x += 3) if (insideSilhouette(s, x, dy)) DrawRectangle(x, l.top + dy, 1, 1, DEPTH);
            }
            for (int i = 0; i < 7; i++) { // glowing spots on the cap
                int x = x0 + 4 + (int)(hash01(l.seed * 13 + i) * (l.width - 8));
                int y = l.top + 2 + (i % 3);
                float pulse = 0.5f + 0.5f * std::sin(time * 1.8f + i * 1.3f);
                DrawCircleGradient(x, y, 5.0f, withAlpha(SPRING, 0.25f * pulse), withAlpha(SPRING, 0.0f));
                DrawRectangle(x, y, 2, 1, lerpColor(CAP_TOP, SPRING, pulse));
            }
            for (int dy = 11; dy < s.rows; dy++) { // the pale stalk, with a ring
                if (s.count[dy] == 0) continue;
                const Span& span = s.spans[dy][0];
                Color stalk = depthShade((dy == 18 || dy == 19) ? STONE_LIGHT : STALK, dy, DEPTH);
                DrawRectangle(span.left, l.top + dy, span.right - span.left, 1, stalk);
                DrawRectangle(span.left, l.top + dy, 2, 1, depthShade(STONE_DARK, dy, DEPTH));
            }
            break;
        }
        case GardenShape::Planter:
            paintRows(s, 0, 1, STONE_LIGHT, 0);
            paintRows(s, 8, 9, STONE_SHADE, 3);
            for (int x = x0 + 8; x < x1 - 8; x += 6) DrawRectangle(x, l.top + 11, 3, 2, STONE_LIGHT); // carved band
            paintRows(s, 20, 24, STONE_LIGHT, 0);
            for (int i = 0; i < 4; i++) { // trailing vines over the rim
                int x = x0 + 4 + i * (l.width - 8) / 3;
                for (int k = 0; k < 8 + i * 4; k++) DrawRectangle(x + (int)std::round(std::sin(time + k * 0.3f + i)), l.top + 2 + k, 1, 1, MOSS_DARK);
            }
            drawBlossoms(l, 10, time);
            break;
        case GardenShape::RootKnot:
            for (int dy = 4; dy < s.rows; dy += 3) { // bark grooves
                for (int i = 0; i < s.count[dy]; i++) {
                    const Span& span = s.spans[dy][i];
                    for (int x = span.left + 2; x < span.right - 2; x += 5) {
                        if (hash01(l.seed + dy * 31 + x) < 0.6f) DrawRectangle(x, l.top + dy, 1, 2, depthShade(BARK_DARK, dy, DEPTH));
                    }
                }
            }
            for (int i = 0; i < 4; i++) { // glowing runes in the bark
                float pulse = 0.5f + 0.5f * std::sin(time * 2.0f + i);
                DrawRectangle(x0 + 12 + i * (l.width - 24) / 3, l.top + 9 + (i % 2) * 4, 2, 2, withAlpha(SPRING, 0.4f + 0.5f * pulse));
            }
            for (int side = 0; side < 2; side++) { // two side roots curling down and out from the knot
                float startX = (side == 0) ? x0 + 10.0f : x1 - 10.0f;
                float direction = (side == 0) ? -1.0f : 1.0f;
                Vector2 previous = {startX, l.top + 20.0f};
                for (int k = 1; k <= 16; k++) {
                    float t = k / 16.0f;
                    Vector2 point = {startX + direction * (26.0f * t * t + 4.0f * t), l.top + 20.0f + 56.0f * t};
                    float thickness = 6.0f * (1.0f - t) + 1.5f;
                    DrawLineEx(previous, point, thickness + 2.0f, DARK);
                    DrawLineEx(previous, point, thickness, depthShade(BARK, 20 + (int)(56.0f * t), DEPTH));
                    previous = point;
                }
            }
            drawBlossoms(l, 4, time);
            break;
    }
}

// ---------------------------------------------------------------------------
// The Great Root: the visible form of tail->next.
// ---------------------------------------------------------------------------

struct RootPath {
    Vector2 start;   // at the tail's hollow
    Vector2 control; // pulls the curve up into an arch
    Vector2 end;     // above the node that tail->next points to
};

Vector2 pointOnRoot(const RootPath& p, float t) {
    float u = 1.0f - t;
    return Vector2{u * u * p.start.x + 2.0f * u * t * p.control.x + t * t * p.end.x,
                   u * u * p.start.y + 2.0f * u * t * p.control.y + t * t * p.end.y};
}

// Finds the tail (the node whose next is the head) and builds the arch from it to tail->next.
bool findRootPath(const SceneView& view, RootPath& path) {
    const Node* head = view.head;
    const Node* tail = head;
    int steps = 0;
    while (tail != nullptr && tail->next != nullptr && tail->next != head && steps < MAX_NODES_PER_REGION) {
        tail = tail->next;
        steps++;
    }
    if (tail == nullptr || tail->next == nullptr) return false; // not a circle: nothing to draw
    const Node* target = tail->next;                            // the real link
    Ledge from = ledgeFor(*tail, view.cameraX);
    Ledge to = ledgeFor(*target, view.cameraX);
    path.start = Vector2{(float)(from.left + from.width - 9), (float)(from.top - 2)};
    path.end = Vector2{(float)(to.left + to.width / 2), (float)(to.top - 2)};
    path.control = Vector2{(path.start.x + path.end.x) / 2.0f, -110.0f};
    return true;
}

void drawGreatRoot(const RootPath& path, const SceneView& view) {
    const int segments = 160;
    const Color bark = lerpColor(BARK, MID_GARDEN, 0.25f);
    // Half thickness along the root: slender at the top of the arch, thick where it meets the ground.
    auto thicknessAt = [](float t) { float edge = std::fabs(t - 0.5f) * 2.0f; return 3.0f + 3.5f * edge * edge; };

    for (int pass = 0; pass < 2; pass++) { // a dark outline, then the bark
        Vector2 previous = pointOnRoot(path, 0.0f);
        for (int i = 1; i <= segments; i++) {
            float t = i / (float)segments;
            Vector2 point = pointOnRoot(path, t);
            float half = thicknessAt(t);
            DrawLineEx(previous, point, pass == 0 ? half * 2.0f + 2.0f : half * 2.0f, pass == 0 ? DARK : bark);
            previous = point;
        }
    }
    Vector2 previous = pointOnRoot(path, 0.0f);
    for (int i = 1; i <= segments; i++) { // the lit upper edge and a darker groove along the root
        float t = i / (float)segments;
        Vector2 point = pointOnRoot(path, t);
        float dx = point.x - previous.x;
        float dy = point.y - previous.y;
        float length = std::sqrt(dx * dx + dy * dy);
        if (length > 0.001f) {
            Vector2 up = {dy / length, -dx / length};
            if (up.y > 0.0f) up = Vector2{-up.x, -up.y};
            float half = thicknessAt(t);
            DrawRectangle((int)(point.x + up.x * (half - 1.0f)), (int)(point.y + up.y * (half - 1.0f)), 1, 1, withAlpha(BARK_LIGHT, 0.8f));
            if (i % 3 == 0) DrawRectangle((int)(point.x - up.x * 1.0f), (int)(point.y - up.y * 1.0f), 1, 1, BARK_DARK);
        }
        previous = point;
    }
    for (int i = 1; i < 18; i++) { // moss, hanging leaves, blossoms, and runes that glow along the root
        float t = i / 18.0f;
        Vector2 point = pointOnRoot(path, t);
        int up = (int)thicknessAt(t);
        DrawRectangle((int)point.x - 2, (int)point.y - up, 4, 1, MOSS);
        if (i % 2 == 0) {
            for (int k = 0; k < 3 + i % 4; k++) DrawRectangle((int)point.x + (i % 3) - 1, (int)point.y + up + k, 1, 1, MOSS_DARK);
        }
        if (i % 5 == 0) DrawRectangle((int)point.x + 1, (int)point.y - up - 1, 2, 1, BLOSSOM);
        float glow = 0.35f + 0.3f * std::sin(view.time * 2.0f - t * 6.0f);
        if (view.passage >= 0.0f) glow += 0.8f * std::fmax(0.0f, 1.0f - std::fabs(view.passage - t) * 6.0f);
        if (i % 2 == 1) DrawRectangle((int)point.x, (int)point.y - 1, 2, 2, withAlpha(SPRING, std::fmin(1.0f, glow)));
    }
}

// The hollow at the tail end of the root. It glows when the player stands on the tail.
void drawRootHollow(Rectangle portal, const SceneView& view, bool ready) {
    int px = toCanvasX(portal.x, view.cameraX);
    int py = toCanvasY(portal.y);
    int pw = (int)(portal.width / CANVAS_SCALE);
    int ph = (int)(portal.height / CANVAS_SCALE);
    Vector2 center = {(float)(px + pw / 2), (float)(py + ph / 2 + 2)};
    float pulse = 0.5f + 0.5f * std::sin(view.time * 3.0f);
    DrawCircleGradient((int)center.x, (int)center.y, ready ? 34.0f : 20.0f, withAlpha(SPRING, ready ? 0.35f : 0.12f),
                       withAlpha(SPRING, 0.0f));
    DrawEllipse((int)center.x, (int)center.y, 7.0f, 16.0f, DARK);
    DrawEllipse((int)center.x, (int)center.y + 2, 5.0f, 12.0f, withAlpha(SPRING, ready ? 0.45f + 0.3f * pulse : 0.15f));
    DrawRing(Vector2{center.x, center.y - 4.0f}, 7.0f, 11.0f, 180.0f, 360.0f, 16, BARK);
    DrawRing(Vector2{center.x, center.y - 4.0f}, 10.0f, 11.0f, 270.0f, 360.0f, 8, BARK_LIGHT);
    for (int i = 0; ready && i < 4; i++) { // petals drifting into the hollow
        float t = std::fmod(view.time * 0.5f + i * 0.25f, 1.0f);
        DrawRectangle((int)(center.x - 22.0f + t * 20.0f), (int)(center.y - 14.0f + std::sin(t * 6.0f + i) * 4.0f), 2, 1,
                      withAlpha(PETAL, 1.0f - t));
    }
}

// The traveler, as a mote of light, carried through the root from the tail to tail->next.
void drawPassageMote(const RootPath& path, float t, float time) {
    Vector2 point = pointOnRoot(path, t);
    DrawCircleGradient((int)point.x, (int)point.y, 16.0f, withAlpha(SPRING, 0.55f), withAlpha(SPRING, 0.0f));
    DrawRectangle((int)point.x - 1, (int)point.y - 1, 3, 3, WHITE);
    for (int k = 1; k <= 7; k++) { // a trail of petals
        float behind = t - k * 0.025f;
        if (behind < 0.0f) break;
        Vector2 p = pointOnRoot(path, behind);
        DrawRectangle((int)p.x + (int)(std::sin(time * 8.0f + k) * 2.0f), (int)p.y + k, 2, 1, withAlpha(PETAL, 1.0f - k / 8.0f));
    }
}

// A round moon gate at the exit. A curtain of wisteria hangs in it until the loop is complete.
void drawMoonGate(Rectangle gate, int cameraX, bool open, float time) {
    const int gx = toCanvasX(gate.x, cameraX);
    const int gy = toCanvasY(gate.y);
    const int gw = (int)(gate.width / CANVAS_SCALE);
    const int gh = (int)(gate.height / CANVAS_SCALE);
    const float radius = 17.0f;
    const Vector2 center = {(float)(gx + gw / 2), (float)(gy + gh) - radius - 2.0f};

    if (open) {
        DrawCircleGradient((int)center.x, (int)center.y, 40.0f, withAlpha(SPRING, 0.35f), withAlpha(SPRING, 0.0f));
        DrawCircleV(center, radius - 3.0f, withAlpha(SPRING, 0.75f));
        DrawCircleV(center, radius - 7.0f, withAlpha(WHITE, 0.5f));
        for (int i = 0; i < 6; i++) {
            float t = std::fmod(time * 0.4f + i * 0.17f, 1.0f);
            DrawRectangle((int)center.x - 8 + (int)(hash01(i * 5 + 1) * 16.0f), (int)(center.y + 10.0f - t * 24.0f), 1, 1,
                          withAlpha(WHITE, 1.0f - t));
        }
    } else {
        DrawCircleV(center, radius - 3.0f, Color{24, 16, 46, 255});
        for (int strand = 0; strand < 7; strand++) { // wisteria curtain
            int x = (int)center.x - 10 + strand * 3;
            int length = 14 + (strand % 3) * 5;
            int sway = (int)std::round(std::sin(time * 1.5f + strand) * 1.0f);
            for (int k = 0; k < length; k++) {
                DrawRectangle(x + sway, (int)center.y - 12 + k, 1, 1, (k % 3 == 0) ? WISTERIA : lerpColor(WISTERIA, DARK, 0.4f));
            }
        }
    }
    DrawRing(center, radius - 3.0f, radius + 2.0f, 0.0f, 360.0f, 40, STONE);
    DrawRing(center, radius + 1.0f, radius + 2.0f, 270.0f, 360.0f, 12, STONE_LIGHT);
    for (int i = 0; i < 6; i++) { // vines and blossoms on the ring
        float angle = 3.4f + i * 0.45f;
        int x = (int)(center.x + std::cos(angle) * radius);
        int y = (int)(center.y + std::sin(angle) * radius);
        DrawRectangle(x, y, 2, 2, MOSS_DARK);
        if (i % 2 == 0) DrawRectangle(x, y - 1, 1, 1, BLOSSOM);
    }
}

// ---------------------------------------------------------------------------
// Background: night sky, distant hills, the ancient tree, the spring.
// (This region fits on one screen, so the camera does not scroll here.)
// ---------------------------------------------------------------------------

void drawGardenSky(float time) {
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 180, SKY_TOP, SKY_MID);
    DrawRectangleGradientV(0, 180, CANVAS_WIDTH, CANVAS_HEIGHT - 180, SKY_MID, SKY_LOW);
    for (int i = 0; i < 70; i++) { // stars
        int x = (int)(hash01(i * 7 + 3) * CANVAS_WIDTH);
        int y = (int)(hash01(i * 11 + 5) * 170.0f);
        float twinkle = 0.5f + 0.5f * std::sin(time * 1.5f + i * 1.9f);
        DrawRectangle(x, y, 1, 1, withAlpha(MOON, (0.3f + 0.7f * hash01(i * 3)) * (0.4f + 0.6f * twinkle)));
    }
    for (int x = 0; x < CANVAS_WIDTH; x += 2) { // a faint ribbon of night light
        float wave = std::sin(x * 0.012f + time * 0.15f) * 14.0f + std::sin(x * 0.031f) * 5.0f;
        int y = 54 + (int)wave;
        DrawRectangleGradientV(x, y, 2, 34, withAlpha(SPRING, 0.0f), withAlpha(SPRING, 0.05f));
        DrawRectangleGradientV(x, y + 34, 2, 16, withAlpha(SPRING, 0.05f), withAlpha(WISTERIA, 0.0f));
    }
    DrawCircleGradient(116, 64, 110.0f, withAlpha(MOON, 0.14f), withAlpha(MOON, 0.0f));
    DrawCircle(116, 64, 19.0f, MOON);
    DrawCircle(110, 60, 4.0f, lerpColor(MOON, SKY_LOW, 0.25f));
    DrawCircle(123, 70, 3.0f, lerpColor(MOON, SKY_LOW, 0.25f));
}

void drawGardenHills() {
    for (int x = 0; x < CANVAS_WIDTH; x++) { // rolling hills, stepped like pixel art
        float y = 214.0f + std::sin(x * 0.011f + 1.3f) * 14.0f + std::sin(x * 0.027f) * 6.0f;
        int top = (int)(std::floor(y / 2.0f) * 2.0f);
        DrawRectangle(x, top, 1, CANVAS_HEIGHT - top, FAR_HILLS);
    }
    for (int i = 0; i < 14; i++) { // cypress trees
        int x = 20 + i * 46 + (int)(hash01(i * 13 + 2) * 20.0f);
        int height = 30 + (int)(hash01(i * 17 + 4) * 26.0f);
        int base = 214 + (int)(std::sin(x * 0.011f + 1.3f) * 14.0f);
        drawTriangleSafe(Vector2{(float)x - 5.0f, (float)base}, Vector2{(float)x + 5.0f, (float)base},
                         Vector2{(float)x, (float)(base - height)}, FAR_HILLS);
    }
    // An overgrown observatory dome on the far right hill.
    DrawCircleSector(Vector2{566.0f, 206.0f}, 18.0f, 180.0f, 360.0f, 20, FAR_HILLS);
    DrawRectangle(548, 206, 36, 10, FAR_HILLS);
    DrawRectangle(564, 190, 3, 16, lerpColor(FAR_HILLS, SKY_LOW, 0.5f)); // its open slit
    DrawRectangle(556, 209, 2, 3, withAlpha(BLOSSOM, 0.6f));
    DrawRectangleGradientV(0, 180, CANVAS_WIDTH, 90, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.3f));
}

// The landmark: an ancient tree in the middle of the garden, hung with glowing fruit.
void drawAncientTree(float time) {
    const float cx = 315.0f;
    Color shadow = lerpColor(BARK, DARK, 0.5f);
    for (int y = 60; y < CANVAS_HEIGHT; y++) {
        float half = 16.0f + std::sin(y * 0.05f) * 2.0f;
        if (y > 250) half += (y - 250) * 0.5f; // roots spread into the spring
        int left = (int)(cx - half);
        int right = (int)(cx + half);
        DrawRectangle(left, y, right - left, 1, lerpColor(BARK, MID_GARDEN, 0.45f));
        DrawRectangle(left, y, 3, 1, shadow);
        DrawRectangle(right - 2, y, 2, 1, lerpColor(BARK_LIGHT, MID_GARDEN, 0.3f));
    }
    for (int g = 0; g < 3; g++) { // bark grooves
        for (int y = 70; y < CANVAS_HEIGHT; y++) {
            int wiggle = (int)std::round(std::sin(y * 0.07f + g * 2.0f) * 1.5f);
            if (hash01(g * 991 + y) < 0.75f) DrawRectangle((int)cx - 8 + g * 8 + wiggle, y, 1, 1, shadow);
        }
    }
    Color leaves = {44, 28, 84, 255};
    Color leavesLit = {78, 56, 128, 255};
    for (int i = 0; i < 16; i++) { // the canopy: clustered crowns, lit from the moon on the left
        float x = 60.0f + i * 34.0f + hash01(i * 7 + 1) * 18.0f;
        float y = 6.0f + hash01(i * 11 + 2) * 50.0f + std::fabs(x - cx) * 0.12f;
        float r = 26.0f + hash01(i * 13 + 3) * 16.0f;
        DrawCircleV(Vector2{x - 2.0f, y - 2.0f}, r, leavesLit);
        DrawCircleV(Vector2{x, y}, r, leaves);
    }
    for (int i = 0; i < 18; i++) { // glowing fruit lanterns hanging from the canopy
        float x = 70.0f + hash01(i * 17 + 5) * 500.0f;
        float y = 40.0f + hash01(i * 19 + 7) * 50.0f;
        float swing = std::sin(time * 1.2f + i) * 1.5f;
        Color glow = (i % 3 == 0) ? BLOSSOM : SPRING;
        DrawLineEx(Vector2{x, y - 8.0f}, Vector2{x + swing, y}, 1.0f, leaves);
        DrawCircleGradient((int)(x + swing), (int)y + 2, 7.0f, withAlpha(glow, 0.35f), withAlpha(glow, 0.0f));
        DrawRectangle((int)(x + swing) - 1, (int)y, 2, 3, glow);
    }
}

// The luminous spring at the tree's feet, below the route.
void drawSpring(float time) {
    DrawCircleGradient(315, SPRING_Y + 10, 170.0f, withAlpha(SPRING, 0.16f), withAlpha(SPRING, 0.0f));
    DrawRectangleGradientV(150, SPRING_Y, 330, CANVAS_HEIGHT - SPRING_Y, withAlpha(SPRING, 0.42f), Color{20, 40, 70, 255});
    DrawEllipse(150, SPRING_Y + 30, 22.0f, 30.0f, withAlpha(SPRING, 0.35f));
    DrawEllipse(480, SPRING_Y + 30, 22.0f, 30.0f, withAlpha(SPRING, 0.35f));
    DrawRectangle(150, SPRING_Y, 330, 1, withAlpha(WHITE, 0.6f));
    for (int i = 0; i < 26; i++) { // shimmer on the surface
        float x = 160.0f + std::fmod(hash01(i * 5 + 3) * 310.0f + time * (6.0f + i % 4), 310.0f);
        int y = SPRING_Y + 3 + (int)(hash01(i * 7 + 1) * 40.0f);
        DrawRectangle((int)x, y, 3 + (int)(hash01(i * 11) * 8.0f), 1, withAlpha(WHITE, 0.25f));
    }
    for (int i = 0; i < 6; i++) { // small lotus flowers floating
        int x = 175 + i * 52 + (int)(hash01(i * 3 + 9) * 20.0f);
        int y = SPRING_Y + 4 + (i % 2) * 6 + (int)(std::sin(time + i) * 1.0f);
        DrawRectangle(x - 2, y, 5, 1, MOSS);
        DrawRectangle(x - 1, y - 1, 3, 1, BLOSSOM);
        DrawRectangle(x, y - 2, 1, 1, PETAL);
    }
    for (int i = 0; i < 10; i++) { // light rising from the water
        float t = std::fmod(time * 0.3f + i * 0.1f, 1.0f);
        int x = 170 + (int)(hash01(i * 13 + 4) * 290.0f);
        DrawRectangle(x, SPRING_Y - (int)(t * 60.0f), 1, 1, withAlpha(SPRING, 0.7f * (1.0f - t)));
    }
}

// Hedges, glowing flower bushes, and two stone lanterns around the spring.
void drawGardenMiddle(float time) {
    for (int i = 0; i < 9; i++) { // hedges and flower bushes along the back of the garden
        int x = 10 + i * 74 + (int)(hash01(i * 7 + 3) * 20.0f);
        int y = 288 + (int)(hash01(i * 11 + 1) * 10.0f);
        float r = 16.0f + hash01(i * 13 + 5) * 10.0f;
        DrawCircleV(Vector2{(float)x, (float)y}, r, MID_GARDEN);
        DrawCircleV(Vector2{(float)x + r * 0.7f, (float)y + 4.0f}, r * 0.7f, MID_GARDEN);
        for (int b = 0; b < 5; b++) {
            float glow = 0.5f + 0.5f * std::sin(time * 1.6f + i + b);
            Color color = ((i + b) % 2 == 0) ? BLOSSOM : SPRING;
            DrawRectangle(x - (int)r / 2 + b * 6, y - (int)(r * 0.5f) + (b % 2) * 4, 2, 2, withAlpha(color, 0.35f + 0.4f * glow));
        }
    }
    const int lanternX[2] = {168, 466};
    for (int i = 0; i < 2; i++) { // stone garden lanterns
        int x = lanternX[i];
        int base = 298;
        Color stone = lerpColor(STONE_DARK, MID_GARDEN, 0.3f);
        DrawRectangle(x - 6, base - 4, 12, 4, stone);
        DrawRectangle(x - 2, base - 16, 4, 12, stone);
        DrawRectangle(x - 5, base - 24, 10, 8, stone);
        float flicker = 0.85f + 0.15f * std::sin(time * 9.0f + i * 2.0f);
        DrawCircleGradient(x, base - 20, 18.0f, withAlpha(WARM_LIGHT, 0.3f * flicker), withAlpha(WARM_LIGHT, 0.0f));
        DrawRectangle(x - 3, base - 22, 6, 4, withAlpha(WARM_LIGHT, flicker));
        drawTriangleSafe(Vector2{(float)x - 9.0f, (float)(base - 24)}, Vector2{(float)x + 9.0f, (float)(base - 24)},
                         Vector2{(float)x, (float)(base - 31)}, stone);
    }
    DrawRectangleGradientV(0, 230, CANVAS_WIDTH, 70, withAlpha(MIST, 0.0f), withAlpha(MIST, 0.16f));
}

} // namespace

// ---------------------------------------------------------------------------
// Region 3 drawing
// ---------------------------------------------------------------------------

void drawGardenBackground(const SceneView& view) {
    drawGardenSky(view.time);
    drawGardenHills();
    drawAncientTree(view.time);
    drawSpring(view.time);
    drawGardenMiddle(view.time);
    RootPath path;
    if (findRootPath(view, path)) drawGreatRoot(path, view); // drawn behind the platforms
}

void drawGardenPlatforms(const SceneView& view) {
    // The platform the current node's next points to. In a circular list next is never NULL,
    // but it is still checked before use.
    const Node* next = (view.current != nullptr) ? view.current->next : nullptr;

    // Walk the real list from its head. In a circle the walk stops when it is back at the head.
    const Node* node = view.head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        Ledge ledge = ledgeFor(*node, view.cameraX);
        if (ledgeOnScreen(ledge)) {
            float mist = 0.0f;
            if (view.mistById != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION) {
                mist = view.mistById[node->id];
            }
            GardenShape shape = shapeFor(*node);
            Silhouette outline = buildGardenSilhouette(shape, ledge);
            fillSilhouette(outline, (shape == GardenShape::RootKnot) ? ROOT_BARK : GARDEN_STONE, false);
            drawGardenDetails(shape, ledge, outline, view.time);
            drawMistVeil(outline, mist, VEIL);
            // The tail's next is the head; that link is shown by the root, so no glow at the head.
            if (node == next && node != view.head) drawNextGlow(ledge, view.time);
        }
        node = node->next;
        steps++;
        if (node == view.head) break; // stop condition: back at the start
    }

    if (view.checkpoint != nullptr) {
        Ledge ledge = ledgeFor(*view.checkpoint, view.cameraX);
        drawLanternPost(ledge.left + ledge.width - 14, ledge.top, view.checkpointFlash, view.time);
    }
    bool onTail = view.current != nullptr && view.current->next == view.head;
    drawRootHollow(view.portalRect, view, onTail);
    if (view.hasGate) drawMoonGate(view.gateRect, view.cameraX, view.gateOpen, view.time);

    RootPath path;
    if (view.passage >= 0.0f && findRootPath(view, path)) drawPassageMote(path, view.passage, view.time);
}

void drawGardenForeground(const SceneView& view) {
    const float time = view.time;
    for (int i = 0; i < 26; i++) { // petals drifting down through the garden
        float t = std::fmod(time * 0.06f + hash01(i * 7 + 1), 1.0f);
        float x = std::fmod(hash01(i * 13 + 2) * 700.0f + t * 120.0f, 700.0f) - 30.0f + std::sin(time + i) * 8.0f;
        float y = t * 380.0f - 10.0f;
        DrawRectangle((int)x, (int)y, 2, 1, withAlpha((i % 3 == 0) ? WISTERIA : PETAL, 0.75f));
    }
    for (int i = 0; i < 16; i++) { // fireflies
        float x = hash01(i * 5 + 9) * CANVAS_WIDTH + std::sin(time * 0.6f + i * 1.4f) * 12.0f;
        float y = 120.0f + hash01(i * 3 + 4) * 160.0f + std::sin(time * 0.8f + i) * 8.0f;
        float twinkle = 0.5f + 0.5f * std::sin(time * 2.4f + i * 2.0f);
        DrawCircleGradient((int)x, (int)y, 5.0f, withAlpha(SPRING, 0.3f * twinkle), withAlpha(SPRING, 0.0f));
        DrawRectangle((int)x, (int)y, 1, 1, withAlpha(SPRING, 0.4f + 0.6f * twinkle));
    }
    for (int i = 0; i < 9; i++) { // wisteria hanging along the top edge
        int x = 8 + i * 76 + (int)(hash01(i * 3 + 1) * 20.0f);
        int length = 18 + (int)(hash01(i * 9 + 2) * 26.0f);
        for (int k = 0; k < length; k++) {
            int sway = (int)std::round(std::sin(time * 1.1f + i + k * 0.1f) * (k / (float)length) * 3.0f);
            int width = 5 - k * 4 / length;
            DrawRectangle(x - width / 2 + sway, k, width, 1, (k % 4 == 0) ? WISTERIA : lerpColor(WISTERIA, DARK, 0.45f));
        }
    }
    for (int i = 0; i < 18; i++) { // dark ferns along the bottom
        float x = i * 38.0f + hash01(i * 5 + 1) * 14.0f;
        for (int b = 0; b < 4; b++) {
            float height = 12.0f + hash01(i * 11 + b) * 22.0f;
            float lean = (b - 1.5f) * 5.0f + std::sin(time * 1.3f + i + b) * 1.2f;
            drawTriangleSafe(Vector2{x + b * 4.0f - 2.0f, CANVAS_HEIGHT + 2.0f}, Vector2{x + b * 4.0f + 2.0f, CANVAS_HEIGHT + 2.0f},
                             Vector2{x + b * 4.0f + lean, CANVAS_HEIGHT - height}, DARK);
        }
    }
    DrawRectangleGradientH(0, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.5f), withAlpha(DARK, 0.0f));
    DrawRectangleGradientH(CANVAS_WIDTH - 70, 0, 70, CANVAS_HEIGHT, withAlpha(DARK, 0.0f), withAlpha(DARK, 0.5f));
    DrawRectangleGradientV(0, 0, CANVAS_WIDTH, 40, withAlpha(DARK, 0.5f), withAlpha(DARK, 0.0f));
}
