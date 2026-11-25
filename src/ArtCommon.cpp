#include "ArtCommon.h"

#include <algorithm>

#include "rlgl.h"

// ---------------------------------------------------------------------------
// Which region file draws the active region.
// ---------------------------------------------------------------------------

namespace {

const RegionPainter& painterFor(int region) {
    switch (region) {
        case 0: return FOREST_PAINTER;
        case 1: return RUINS_PAINTER;
        case 2: return GARDEN_PAINTER;
        case 3: return EDGE_PAINTER;
        case 4: return LIGHTHOUSE_PAINTER;
        default: return HARBOR_PAINTER;
    }
}

} // namespace

void RegionArt::clear() {
    for (ArtLayer& layer : back) UnloadTexture(layer.texture);
    for (ArtLayer& layer : front) UnloadTexture(layer.texture);
    for (SwayPiece& piece : sway) UnloadTexture(piece.texture);
    for (LedgeArt& ledge : ledges) {
        UnloadTexture(ledge.picture);
        UnloadTexture(ledge.mask);
    }
    for (Texture2D& texture : extra) UnloadTexture(texture);
    back.clear();
    front.clear();
    sway.clear();
    ledges.clear();
    extra.clear();
    spots.clear();
    waterline = -1;
    region = -1;
}

void bakeRegionArt(RegionArt& art, int region, const World& world) {
    art.clear();
    art.region = region;
    painterFor(region).bake(art, world);
}

void drawRegionBackground(const SceneView& view, const RegionArt& art) { painterFor(view.region).background(view, art); }
void drawRegionPlatforms(const SceneView& view, const RegionArt& art) { painterFor(view.region).platforms(view, art); }
void drawRegionForeground(const SceneView& view, const RegionArt& art) { painterFor(view.region).foreground(view, art); }
Mood regionMood(const SceneView& view) { return painterFor(view.region).mood(view); }

void drawRegionWater(const SceneView& view, const RegionArt& art, const Texture2D& reflection) {
    const RegionPainter& painter = painterFor(view.region);
    if (painter.water != nullptr) painter.water(view, art, reflection);
}

void drawRegionAdditive(const SceneView& view, const RegionArt& art, const SceneRenderer& renderer) {
    painterFor(view.region).additive(view, art, renderer);
}

void addRegionLights(const SceneView& view, const RegionArt& art, std::vector<Light>& lights) {
    painterFor(view.region).lights(view, art, lights);
}

// ---------------------------------------------------------------------------
// Baking
// ---------------------------------------------------------------------------

int scrollRangeOf(const RegionInfo& region) {
    float range = (region.right - region.left - CANVAS_WIDTH * CANVAS_SCALE) / CANVAS_SCALE;
    return (region.kind == ListKind::Circular || range < 0.0f) ? 0 : (int)std::ceil(range);
}

LedgeCanvas makeLedgeCanvas(const Node& node, int margin, int above) {
    LedgeCanvas c;
    c.width = (int)(node.platformWidth / CANVAS_SCALE);
    c.top = toCanvasY(node.platformPosition.y);
    c.originX = margin;
    c.originY = above;
    c.picture = Picture(c.width + margin * 2, CANVAS_HEIGHT - c.top + above);
    return c;
}

void storeLedge(RegionArt& art, const Node& node, const LedgeCanvas& c) {
    LedgeArt ledge;
    ledge.nodeId = node.id;
    ledge.picture = c.picture.upload();
    ledge.mask = c.picture.uploadMask();
    ledge.dx = -c.originX;
    ledge.dy = -c.originY;
    art.ledges.push_back(ledge);
}

void storeLedgeById(RegionArt& art, int nodeId, const LedgeCanvas& c) {
    LedgeArt ledge;
    ledge.nodeId = nodeId;
    ledge.picture = c.picture.upload();
    ledge.mask = c.picture.uploadMask();
    ledge.dx = -c.originX;
    ledge.dy = -c.originY;
    art.ledges.push_back(ledge);
}

const LedgeArt* findLedgeArt(const RegionArt& art, int nodeId) {
    for (const LedgeArt& ledge : art.ledges) {
        if (ledge.nodeId == nodeId) return &ledge;
    }
    return nullptr;
}

void storeLayer(RegionArt& art, const Picture& picture, float parallax, int y, bool front) {
    ArtLayer layer;
    layer.texture = picture.upload();
    layer.parallax = parallax;
    layer.y = y;
    (front ? art.front : art.back).push_back(layer);
}

void storeEmptyLayer(RegionArt& art, float parallax, bool front) {
    ArtLayer layer;
    layer.parallax = parallax;
    (front ? art.front : art.back).push_back(layer);
}

void storeSway(RegionArt& art, const Picture& piece, int layer, bool front, float x, float y, float sway, bool hangs,
               int bands, float lag) {
    SwayPiece s;
    s.texture = piece.upload();
    s.layer = layer;
    s.front = front;
    s.x = x;
    s.y = y;
    s.sway = sway;
    s.hangs = hangs;
    s.bands = std::max(1, bands);
    s.lag = lag;
    art.sway.push_back(s);
}

void paintFoliage(Picture& p, float cx, float cy, float r, const Color* ramp, int steps, Vector2 toLight, int seed,
                  float roughness) {
    int size = (int)std::ceil(r * (1.0f + roughness) * 2.0f) + 6;
    int ox = (int)std::floor(cx) - size / 2;
    int oy = (int)std::floor(cy) - size / 2;
    Mask mask(size, size);
    float local = size / 2.0f + (cx - std::floor(cx));
    mask.blob(local, size / 2.0f + (cy - std::floor(cy)), r, seed, roughness);
    for (int i = 0; i < 5; i++) { // a few smaller clumps around the edge make it read as leaves
        float angle = hash01(seed * 7 + i) * 6.283f;
        float d = r * (0.55f + 0.3f * hash01(seed * 11 + i));
        mask.blob(local + std::cos(angle) * d, size / 2.0f + std::sin(angle) * d * 0.8f, r * (0.3f + 0.2f * hash01(seed + i)),
                  seed + i * 13, 0.3f);
    }
    mask.measure(); // the box has room around the blob, so its whole edge gets the rim light
    Shading s;
    s.ramp = ramp;
    s.steps = steps;
    s.toLight = toLight;
    s.base = 0.42f;
    s.rim = 0.42f;
    s.bevel = std::fmax(3.0f, r * 0.45f);
    s.grain = 0.08f;
    s.grainScale = 0.3f;
    s.seed = seed;
    shade(p, mask, s, ox, oy);
    for (int i = 0; i < (int)(r * r * 0.12f); i++) { // leaf marks: small strokes, lighter toward the light
        float angle = hash01(seed * 31 + i) * 6.283f;
        float d = std::sqrt(hash01(seed * 37 + i)) * r * 0.95f;
        int x = (int)(cx + std::cos(angle) * d);
        int y = (int)(cy + std::sin(angle) * d * 0.9f);
        if (!p.painted(x, y) || !p.painted(x + 1, y)) continue;
        float facing = std::cos(angle) * toLight.x + std::sin(angle) * toLight.y;
        int step = std::clamp((int)std::lround((0.5f + 0.45f * facing) * (steps - 1)), 0, steps - 1);
        p.set(x, y, ramp[step]);
        p.set(x + 1, y, ramp[std::max(0, step - 1)]);
    }
}

void paintTrunk(Picture& p, float x, float yTop, float yBottom, float halfTop, float halfBottom, const Color* ramp,
                int steps, Vector2 toLight, int seed) {
    for (int y = (int)yTop; y < (int)yBottom; y++) {
        float t = (y - yTop) / std::fmax(1.0f, yBottom - yTop);
        float half = halfTop + (halfBottom - halfTop) * t * t; // the trunk flares toward its roots
        float wobble = (noise2(y * 0.05f, 0.0f, seed) - 0.5f) * 2.0f;
        int left = (int)std::floor(x - half + wobble);
        int right = (int)std::ceil(x + half + wobble);
        for (int xx = left; xx < right; xx++) {
            float u = (xx + 0.5f - (x + wobble)) / std::fmax(1.0f, half); // -1 left edge .. 1 right edge
            float round = std::sqrt(std::fmax(0.0f, 1.0f - u * u));
            float lit = u * toLight.x; // the side facing the light is brighter
            // Long vertical streaks of bark, and the side facing the light a step brighter.
            float bark = (noise2(xx * 0.7f, y * 0.035f, seed) - 0.5f) * 0.22f + (noise2(xx * 0.2f, y * 0.012f, seed + 9) - 0.5f) * 0.14f;
            float value = 0.25f + 0.25f * round + 0.3f * lit + bark;
            int step = std::clamp((int)std::lround(std::clamp(value, 0.0f, 1.0f) * (steps - 1)), 0, steps - 1);
            Color c = ramp[step];
            if (xx == left || xx == right - 1) c = ramp[0];
            p.set(xx, y, c);
        }
    }
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

namespace {

void drawSwayPieces(const SceneView& view, const RegionArt& art, int layer, bool front, float wind) {
    const std::vector<ArtLayer>& layers = front ? art.front : art.back;
    if (layer < 0 || layer >= (int)layers.size()) return;
    const ArtLayer& host = layers[layer];
    float shift = std::floor(view.scroll * host.parallax);
    for (const SwayPiece& piece : art.sway) {
        if (piece.layer != layer || piece.front != front) continue;
        const Texture2D& tex = piece.texture;
        float x = piece.x - shift;
        if (x + tex.width < -8 || x > CANVAS_WIDTH + 8) continue;
        float lean = windAt(piece.x + tex.width * 0.5f, view.time - piece.lag, wind) * piece.sway;
        int bandHeight = (tex.height + piece.bands - 1) / piece.bands;
        for (int b = 0; b < piece.bands; b++) {
            int y0 = b * bandHeight;
            int h = std::min(bandHeight, tex.height - y0);
            if (h <= 0) break;
            float along = (b + 0.5f) / piece.bands; // 0 at the top of the picture, 1 at the bottom
            float free = piece.hangs ? along : 1.0f - along;
            int offset = (int)std::lround(lean * free * free);
            DrawTextureRec(tex, Rectangle{0, (float)y0, (float)tex.width, (float)h},
                           Vector2{std::floor(x) + offset, host.y + piece.y + y0}, WHITE);
        }
    }
}

} // namespace

void drawBackLayers(const SceneView& view, const RegionArt& art, int from, int to, float wind) {
    for (int i = from; i < to && i < (int)art.back.size(); i++) {
        const ArtLayer& layer = art.back[i];
        if (layer.texture.id != 0) DrawTexture(layer.texture, (int)-std::floor(view.scroll * layer.parallax), layer.y, WHITE);
        drawSwayPieces(view, art, i, false, wind);
    }
}

void drawFrontLayers(const SceneView& view, const RegionArt& art, float wind) {
    for (int i = 0; i < (int)art.front.size(); i++) {
        const ArtLayer& layer = art.front[i];
        if (layer.texture.id != 0) DrawTexture(layer.texture, (int)-std::floor(view.scroll * layer.parallax), layer.y, WHITE);
        drawSwayPieces(view, art, i, true, wind);
    }
}

void drawLedges(const SceneView& view, const RegionArt& art, Color veil) {
    const Node* node = view.head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        for (const LedgeArt& ledge : art.ledges) {
            if (ledge.nodeId != node->id) continue;
            int x = toCanvasX(node->platformPosition.x, view.cameraX) + ledge.dx;
            int y = toCanvasY(node->platformPosition.y) + ledge.dy;
            if (x + ledge.picture.width < 0 || x > CANVAS_WIDTH) break;
            DrawTexture(ledge.picture, x, y, WHITE);
            float mist = 0.0f;
            if (view.mistById != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION) mist = view.mistById[node->id];
            if (mist > 0.01f) DrawTexture(ledge.mask, x, y, withAlpha(veil, 0.66f * mist));
            break;
        }
        node = node->next;
        steps++;
        if (node == view.head) break; // a circular list: back at the start
    }
}

void drawLinkLip(const Ledge& ledge, Color color, float strength, float time, bool rising) {
    if (strength <= 0.01f) return;
    float pulse = strength * (0.7f + 0.3f * std::sin(time * 2.6f));
    DrawRectangle(ledge.left, ledge.top, ledge.width, 1, withAlpha(color, 0.75f * pulse));
    DrawRectangleGradientV(ledge.left, ledge.top - 10, ledge.width, 10, withAlpha(color, 0.0f), withAlpha(color, 0.18f * pulse));
    int count = 4 + ledge.width / 16;
    for (int i = 0; i < count; i++) { // motes rising from it (next) or settling onto it (previous)
        float cycle = time * (0.32f + 0.12f * hash01(ledge.seed + i * 5)) + hash01(ledge.seed * 3 + i);
        float t = cycle - std::floor(cycle);
        int pass = (int)std::floor(cycle);
        int x = ledge.left + 4 + (int)(hash01(ledge.seed * 7 + i * 31 + pass * 17) * (ledge.width - 8));
        float rise = rising ? t : 1.0f - t;
        int y = ledge.top - 2 - (int)(rise * 22.0f);
        x += (int)std::lround(std::sin(time * 1.7f + i) * 1.5f * rise);
        DrawRectangle(x, y, 1, 1, withAlpha(lerpColor(color, WHITE, 0.4f), pulse * loopEnvelope(t)));
    }
}

void addLinkLight(std::vector<Light>& lights, const Ledge& ledge, Color color, float strength) {
    if (strength <= 0.01f) return;
    Light light;
    light.position = Vector2{ledge.left + ledge.width * 0.5f, ledge.top - 4.0f};
    light.radius = ledge.width * 0.55f + 34.0f;
    light.color = color;
    light.strength = 0.42f * strength;
    light.glowRadius = ledge.width * 0.42f + 10.0f;
    light.glow = 0.16f * strength;
    lights.push_back(light);
}

void drawGrass(const Ledge& ledge, const SceneView& view, Color tip, Color stem, float wind, float density) {
    for (int dx = 1; dx < ledge.width - 1; dx++) {
        float h = hash01(ledge.seed * 131 + dx * 7);
        if (h > density) continue;
        int x = ledge.left + dx;
        int height = 2 + (int)(hash01(ledge.seed * 17 + dx) * 4.0f);
        float lean = windAt((float)(x + view.cameraX), view.time, wind) * 1.6f - 0.4f;
        float away = (float)x - view.playerFeet.x; // the traveler pushes the grass aside
        if (std::fabs(away) < 9.0f && std::fabs(view.playerFeet.y - ledge.top) < 3.0f) {
            lean += (away >= 0.0f ? 1.0f : -1.0f) * (9.0f - std::fabs(away)) * 0.45f;
        }
        for (int k = 0; k < height; k++) {
            float along = (k + 1.0f) / height;
            int offset = (int)std::lround(lean * along * along);
            DrawRectangle(x + offset, ledge.top - 1 - k, 1, 1, (k >= height - 1) ? tip : stem);
        }
    }
}

void drawStrand(int x, int y, int length, Color color, Color leaf, float lean, int seed) {
    for (int k = 0; k < length; k++) {
        float along = k / (float)std::max(1, length);
        int offset = (int)std::lround(lean * along * along + std::sin(seed * 1.7f + k * 0.35f) * 0.6f);
        DrawRectangle(x + offset, y + k, 1, 1, color);
        if (k % 4 == 2) DrawRectangle(x + offset + ((k / 4) % 2 == 0 ? 1 : -1), y + k, 1, 1, leaf);
    }
}

void drawLanternPost(int x, int baseY, float flash, float time) {
    const Color wood = {64, 44, 38, 255};
    const Color woodLit = {110, 78, 54, 255};
    DrawRectangle(x, baseY - 18, 2, 18, wood); // post
    DrawRectangle(x + 1, baseY - 18, 1, 18, woodLit);
    DrawRectangle(x, baseY - 18, 8, 1, wood);  // arm
    float flicker = 0.85f + 0.15f * std::sin(time * 11.0f) * std::sin(time * 6.3f);
    int swing = (int)std::lround(std::sin(time * 1.6f + x * 0.37f) * 0.6f); // it rocks a little on its hook
    int lx = x + 5 + swing;
    int ly = baseY - 15;
    DrawRectangle(x + 7, baseY - 17, 1, 2, wood); // hook
    DrawRectangle(lx + 1, ly - 1, 3, 1, wood);    // cap
    DrawRectangle(lx, ly, 5, 6, wood);            // frame
    DrawRectangle(lx + 1, ly + 1, 3, 4, (flicker > 0.9f) ? WARM_LIGHT : Color{255, 196, 100, 255});
    DrawRectangle(lx + 2, ly + 2, 1, 2, WHITE);   // flame
    for (int i = 0; flash > 0.0f && i < 10; i++) { // sparks when the lantern lights up
        float angle = i * 0.628f + 0.3f;
        float distance = (1.0f - flash) * 16.0f;
        DrawRectangle(lx + 2 + (int)(std::cos(angle) * distance), ly + 3 + (int)(std::sin(angle) * distance - (1.0f - flash) * 6.0f),
                      1, 1, withAlpha(WARM_LIGHT, flash));
    }
}

void addLanternLight(std::vector<Light>& lights, int x, int baseY, float flash, float time) {
    float flicker = 0.9f + 0.1f * std::sin(time * 11.0f) * std::sin(time * 6.3f);
    Light light;
    light.position = Vector2{x + 7.5f, baseY - 12.0f};
    light.radius = 46.0f + 30.0f * flash;
    light.color = WARM_LIGHT;
    light.strength = (0.5f + 0.4f * flash) * flicker;
    light.glowRadius = 9.0f + 10.0f * flash;
    light.glow = 0.4f + 0.5f * flash;
    lights.push_back(light);
}

bool guideTarget(const SceneView& view, Vector2& target) {
    if (view.guide != nullptr) {
        target = Vector2{(float)toCanvasX(view.guide->platformPosition.x + view.guide->platformWidth / 2.0f, view.cameraX),
                         (float)toCanvasY(view.guide->platformPosition.y) - 6.0f};
        return true;
    }
    if (view.guideToGate && view.hasGate) {
        target = Vector2{(float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX),
                         (float)toCanvasY(view.gateRect.y + view.gateRect.height * 0.55f)};
        return true;
    }
    return false;
}

void drawGuideStream(const SceneView& view) {
    Vector2 target;
    if (!guideTarget(view, target)) return;
    Vector2 start = {view.playerFeet.x, view.playerFeet.y - 16.0f};
    float dx = target.x - start.x;
    float dy = target.y - start.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance < 6.0f) return;
    // The stream reaches part of the way and fades: a hint of direction, not a line on the map.
    float reach = std::fmin(1.0f, 150.0f / distance);
    Vector2 control = {start.x + dx * 0.5f, std::fmin(start.y, target.y) - 18.0f - distance * 0.12f};
    float ease = arrivalEase(view);
    for (int i = 0; i < 7; i++) {
        float t = std::fmod(view.time * 0.42f + i / 7.0f, 1.0f);
        float u = t * reach;
        float a = (1 - u) * (1 - u), b = 2 * (1 - u) * u, c = u * u;
        Vector2 p = {a * start.x + b * control.x + c * target.x, a * start.y + b * control.y + c * target.y};
        p.y += std::sin(view.time * 3.0f + i * 1.9f) * 1.2f;
        float alpha = loopEnvelope(t) * ease * (0.55f + 0.45f * (1.0f - t));
        Color core = lerpColor(view.guideColor, WHITE, 0.55f);
        DrawRectangle((int)p.x, (int)p.y, 1, 1, withAlpha(core, alpha));
        if (i % 2 == 0) DrawRectangle((int)p.x - 1, (int)p.y, 1, 1, withAlpha(view.guideColor, alpha * 0.45f));
    }
}

void drawGradientQuad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color ca, Color cb, Color cc, Color cd) {
    auto triangle = [](Vector2 p, Vector2 q, Vector2 r, Color cp, Color cq, Color cr) {
        float cross = (q.x - p.x) * (r.y - p.y) - (q.y - p.y) * (r.x - p.x);
        if (cross > 0.0f) { // raylib wants counter-clockwise order on screen
            Vector2 sv = q; q = r; r = sv;
            Color sc = cq; cq = cr; cr = sc;
        }
        rlBegin(RL_TRIANGLES);
        rlColor4ub(cp.r, cp.g, cp.b, cp.a); rlVertex2f(p.x, p.y);
        rlColor4ub(cq.r, cq.g, cq.b, cq.a); rlVertex2f(q.x, q.y);
        rlColor4ub(cr.r, cr.g, cr.b, cr.a); rlVertex2f(r.x, r.y);
        rlEnd();
    };
    triangle(a, b, c, ca, cb, cc);
    triangle(a, c, d, ca, cc, cd);
}