#include "World.h"

#include <cassert>
#include <cmath>
#include <cstdio>

#include "DrawHelpers.h"
#include "Theme.h"

// ---------------------------------------------------------------------------
// Traversal rule
// ---------------------------------------------------------------------------

bool isPlatformSolid(const Node* current, const Node* candidate, ListKind kind) {
    if (current == nullptr || candidate == nullptr) {
        return false;
    }
    if (candidate == current) {
        return true; // you can always stand on the node you are at
    }
    if (current->next != nullptr && candidate == current->next) {
        return true; // every list kind allows following next
    }
    if (kind == ListKind::Doubly && current->previous != nullptr && candidate == current->previous) {
        return true; // only a doubly linked list allows following previous
    }
    return false;
}

// ---------------------------------------------------------------------------
// Construction and cleanup
// ---------------------------------------------------------------------------

// Layout of Region 4's bridge node. It is created by append() when the player fixes the link.
static const Vector2 BRIDGE_POSITION = {6110.0f, 450.0f};
static const float BRIDGE_WIDTH = 280.0f;

void World::build() {
    assert(forwardList.getHead() == nullptr && "build() needs empty lists");

    // Region 1: singly linked list. Each append() sets old tail->next = new node.
    // The labels name what each ledge looks like in the forest.
    forwardList.append(1, "Grove Edge", Vector2{90.0f, 470.0f}, 260.0f);
    forwardList.append(2, "Mossy Rock", Vector2{460.0f, 430.0f}, 230.0f);
    forwardList.append(3, "Fallen Log", Vector2{800.0f, 410.0f}, 230.0f);
    forwardList.append(4, "Old Ruin",   Vector2{1140.0f, 440.0f}, 230.0f);
    Node* forwardTail = forwardList.append(5, "Arch Cliff", Vector2{1480.0f, 470.0f}, 260.0f);

    // Region 2: doubly linked list. append() sets both tail->next and new->previous.
    twoWayList.append(1, "Head", Vector2{1890.0f, 480.0f}, 260.0f);
    twoWayList.append(2, "Hall", Vector2{2260.0f, 450.0f}, 230.0f);
    twoWayList.append(3, "Hub",  Vector2{2600.0f, 420.0f}, 230.0f);
    twoWayList.append(4, "Key",  Vector2{2940.0f, 450.0f}, 230.0f);
    twoWayList.append(5, "Tail", Vector2{3280.0f, 480.0f}, 260.0f);

    // Region 3: circular list. append() keeps tail->next == head after every call.
    // The region is narrow enough to fit on one screen, so the return link is visible.
    Node* loopHead = loopList.append(1, "Start", Vector2{3700.0f, 480.0f}, 200.0f);
    loopList.append(2, "Orbit", Vector2{4000.0f, 420.0f}, 180.0f);
    loopList.append(3, "Ring",  Vector2{4280.0f, 420.0f}, 180.0f);
    loopList.append(4, "Tail",  Vector2{4560.0f, 480.0f}, 220.0f);

    // Region 4: singly linked list whose tail (N3 "Edge") has next == nullptr.
    // The gap after it is the NULL hazard. The player fixes it later with append().
    nullList.append(1, "Head", Vector2{5090.0f, 480.0f}, 260.0f);
    nullList.append(2, "Path", Vector2{5460.0f, 450.0f}, 230.0f);
    nullList.append(3, "Edge", Vector2{5800.0f, 450.0f}, 240.0f);
    bridgeBuilt = false;

    regions[0] = RegionInfo{"Singly Linked List", ListKind::Singly, 0.0f, 1800.0f, forwardTail, false};
    regions[1] = RegionInfo{"Doubly Linked List", ListKind::Doubly, 1800.0f, 3600.0f, twoWayList.getTail(), false};
    regions[2] = RegionInfo{"Circular Singly Linked List", ListKind::Circular, 3600.0f, 5000.0f, loopHead, true};
    regions[3] = RegionInfo{"NULL Pointer Safety", ListKind::Singly, 5000.0f, WORLD_WIDTH,
                            nullptr, false}; // the exit gate appears with the bridge

    verifyLinks();
}

void World::clearAll() {
    // Gate pointers point into the lists, so drop them before the nodes go away.
    for (int i = 0; i < REGION_COUNT; i++) {
        regions[i].gateNode = nullptr;
    }
    forwardList.clear();
    twoWayList.clear();
    loopList.clear();
    nullList.clear();
    bridgeBuilt = false;
}

static void checkLink(bool ok, const char* description) {
    if (ok) {
        TraceLog(LOG_INFO, "Link check OK: %s", description);
    } else {
        TraceLog(LOG_WARNING, "Link check FAILED: %s", description);
    }
    assert(ok && "Region link check failed");
}

// Debug checks for the boundary links each region is supposed to have.
void World::verifyLinks() const {
    // Region 1: walking next must end at nullptr, and no node has a previous link.
    int count = 0;
    bool noPrevious = true;
    Node* node = forwardList.getHead();
    while (node != nullptr && count < MAX_NODES_PER_REGION) {
        if (node->previous != nullptr) noPrevious = false;
        node = node->next;
        count++;
    }
    checkLink(node == nullptr && count >= 4, "Region 1 tail->next == nullptr");
    checkLink(noPrevious, "Region 1 stores no previous links");

    // Region 2: head->previous and tail->next are null; links agree in both directions.
    Node* head = twoWayList.getHead();
    Node* tail = twoWayList.getTail();
    checkLink(head != nullptr && head->previous == nullptr, "Region 2 head->previous == nullptr");
    checkLink(tail != nullptr && tail->next == nullptr, "Region 2 tail->next == nullptr");
    bool consistent = true;
    count = 0;
    node = head;
    while (node != nullptr && node->next != nullptr && count < MAX_NODES_PER_REGION) {
        if (node->next->previous != node) consistent = false;
        node = node->next;
        count++;
    }
    checkLink(consistent, "Region 2 node->next->previous == node");

    // Region 3: tail->next is head, and walking next returns to head.
    head = loopList.getHead();
    tail = loopList.getTail();
    checkLink(tail != nullptr && tail->next == head, "Region 3 tail->next == head");
    count = 0;
    node = head;
    do {
        node = (node != nullptr) ? node->next : nullptr;
        count++;
    } while (node != nullptr && node != head && count < MAX_NODES_PER_REGION);
    checkLink(node == head && count >= 4, "Region 3 traversal returns to head");

    // Region 4: the tail's next is null (that's the hazard).
    count = 0;
    node = nullList.getHead();
    while (node != nullptr && node->next != nullptr && count < MAX_NODES_PER_REGION) {
        node = node->next;
        count++;
    }
    checkLink(node != nullptr && node->next == nullptr, "Region 4 tail->next == nullptr");
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

Node* World::getHead(int regionIndex) const {
    switch (regionIndex) {
        case 0: return forwardList.getHead();
        case 1: return twoWayList.getHead();
        case 2: return loopList.getHead();
        case 3: return nullList.getHead();
        default: return nullptr;
    }
}

const RegionInfo& World::getRegion(int regionIndex) const {
    assert(regionIndex >= 0 && regionIndex < REGION_COUNT);
    return regions[regionIndex];
}

Node* World::getLoopTail() const {
    return loopList.getTail();
}

Rectangle World::getPortalRect() const {
    Node* tail = loopList.getTail();
    if (tail == nullptr) {
        return Rectangle{0, 0, 0, 0};
    }
    Rectangle platform = getPlatformRect(*tail);
    return Rectangle{platform.x + platform.width - 36.0f, platform.y - 80.0f, 36.0f, 80.0f};
}

// The NULL gap starts at the right edge of Region 4's tail (the node whose next is nullptr)
// and ends where the bridge node will be placed.
Rectangle World::getNullHazardZone() const {
    Node* tail = nullList.getHead();
    int steps = 0;
    while (tail != nullptr && tail->next != nullptr && steps < MAX_NODES_PER_REGION) {
        tail = tail->next;
        steps++;
    }
    if (tail == nullptr) {
        return Rectangle{0, 0, 0, 0};
    }
    float gapStart = tail->platformPosition.x + tail->platformWidth;
    return Rectangle{gapStart + 10.0f, tail->platformPosition.y + 10.0f,
                     BRIDGE_POSITION.x + BRIDGE_WIDTH - gapStart - 10.0f, WORLD_KILL_Y - tail->platformPosition.y};
}

bool World::isBridgeBuilt() const {
    return bridgeBuilt;
}

Node* World::appendBridgeNode() {
    if (bridgeBuilt) {
        return nullptr;
    }
    // append() walks to the tail (N3) and sets N3->next = the new node.
    Node* bridge = nullList.append(4, "Bridge", BRIDGE_POSITION, BRIDGE_WIDTH);
    bridgeBuilt = true;
    regions[3].gateNode = bridge; // the exit gate stands on the new node
    return bridge;
}

bool World::getGateRect(int regionIndex, Rectangle& outRect) const {
    const RegionInfo& region = getRegion(regionIndex);
    if (region.gateNode == nullptr) {
        return false;
    }
    Rectangle platform = getPlatformRect(*region.gateNode);
    float x = region.gateOnLeft ? platform.x + 10.0f : platform.x + platform.width - 50.0f;
    outRect = Rectangle{x, platform.y - 70.0f, 40.0f, 70.0f};
    return true;
}

// ---------------------------------------------------------------------------
// Drawing helpers. They receive real nodes or points derived from them;
// none of them decides which nodes are connected.
// ---------------------------------------------------------------------------

namespace {

enum class PlatformState { Current, Reachable, Ghost, Completed, Future };
enum class LinkStyle { Dim, Medium, Bright };

Vector2 unitVector(Vector2 from, Vector2 to) {
    float dx = to.x - from.x;
    float dy = to.y - from.y;
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 0.001f) return Vector2{1.0f, 0.0f};
    return Vector2{dx / length, dy / length};
}

void drawArrowHead(Vector2 tip, Vector2 dir, float size, Color color) {
    Vector2 base = {tip.x - dir.x * size, tip.y - dir.y * size};
    Vector2 left = {base.x - dir.y * size * 0.6f, base.y + dir.x * size * 0.6f};
    Vector2 right = {base.x + dir.y * size * 0.6f, base.y - dir.x * size * 0.6f};
    drawTriangleSafe(tip, left, right, color);
}

void drawCenteredText(const char* text, float centerX, float y, int size, Color color) {
    int width = MeasureText(text, size);
    DrawText(text, (int)(centerX - width / 2.0f), (int)y, size, color);
}

// Small dark label chip, used for link names and markers.
void drawChip(const char* text, float centerX, float y, int size, Color textColor, float alpha) {
    int width = MeasureText(text, size);
    Rectangle chip = {centerX - width / 2.0f - 5.0f, y - 2.0f, width + 10.0f, size + 4.0f};
    DrawRectangleRounded(chip, 0.4f, 4, withAlpha(UI_NAVY, 0.85f * alpha));
    DrawText(text, (int)(centerX - width / 2.0f), (int)y, size, withAlpha(textColor, alpha));
}

// One connector between two platform edges. The caller passes points taken
// from a real link (node -> node->next or node -> node->previous).
void drawConnector(Vector2 from, Vector2 to, Color color, LinkStyle style, const char* label, bool labelBelow,
                   float time, float dimAlpha) {
    Vector2 dir = unitVector(from, to);
    Vector2 middle = {(from.x + to.x) / 2.0f, (from.y + to.y) / 2.0f};

    if (style == LinkStyle::Dim) {
        Color dim = withAlpha(color, dimAlpha);
        DrawLineEx(from, to, 2.0f, dim);
        drawArrowHead(to, dir, 8.0f, dim);
        if (dimAlpha >= 0.4f) { // inspector open: name every stored link
            drawChip(label, middle.x, labelBelow ? middle.y + 8.0f : middle.y - 20.0f, 10, color, 0.8f);
        }
        return;
    }

    bool bright = style == LinkStyle::Bright;
    float thick = bright ? 4.0f : 3.0f;
    Color main = withAlpha(color, bright ? 1.0f : 0.65f);
    DrawLineEx(from, to, thick + 8.0f, withAlpha(color, bright ? 0.22f : 0.12f)); // soft glow
    DrawLineEx(from, to, thick, main);
    drawArrowHead(to, dir, 13.0f, main);

    if (bright) {
        // A light travelling along the link shows its direction.
        float t = std::fmod(time * 0.9f, 1.0f);
        DrawCircleV(Vector2{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t}, 3.5f, WHITE);
        drawChip(label, middle.x, labelBelow ? middle.y + 10.0f : middle.y - 30.0f, 20, color, 1.0f);
    } else {
        drawChip(label, middle.x, labelBelow ? middle.y + 8.0f : middle.y - 20.0f, 10, color, 0.85f);
    }
}

// A null link has no connector: a short stub ends in a stop sign marked NULL.
void drawNullMarker(Vector2 edge, float direction, float alpha) {
    Color amber = withAlpha(NULL_AMBER, alpha);
    Vector2 stubEnd = {edge.x + direction * 14.0f, edge.y};
    DrawLineEx(edge, stubEnd, 3.0f, amber);
    Vector2 sign = {stubEnd.x + direction * 10.0f, edge.y};
    DrawPoly(sign, 8, 10.0f, 22.5f, amber);
    DrawRectangle((int)(sign.x - 5.0f), (int)(sign.y - 1.5f), 10, 3, withAlpha(OUTLINE_DARK, alpha));
    drawChip("NULL", sign.x, sign.y + 14.0f, 10, NULL_AMBER, alpha);
}

// Platform shape: a broad top surface, a solid body, and a stepped underside.
// Everything is derived from the node's own position and width.
void drawPlatformBody(const Node& node, int regionIndex, PlatformState state, float time) {
    const RegionTheme& theme = REGION_THEMES[regionIndex];
    Rectangle r = getPlatformRect(node);

    float alpha = 1.0f;
    if (state == PlatformState::Ghost) alpha = 0.3f;
    if (state == PlatformState::Completed) alpha = 0.7f;
    if (state == PlatformState::Future) alpha = 0.55f;

    Color top = theme.platformTop;
    Color body = theme.platformBody;
    Color dark = theme.platformDark;
    if (state == PlatformState::Completed) { // muted: this part is done
        top = lerpColor(top, Color{110, 112, 124, 255}, 0.55f);
        body = lerpColor(body, Color{90, 92, 104, 255}, 0.5f);
        dark = lerpColor(dark, Color{60, 62, 72, 255}, 0.5f);
    }

    if (state == PlatformState::Reachable) {
        // Light as a route cue: the platform you may go to next glows.
        float pulse = 0.24f + 0.08f * std::sin(time * 4.0f);
        DrawCircleGradient((int)(r.x + r.width / 2.0f), (int)(r.y + 8.0f), r.width * 0.6f,
                           withAlpha(theme.accent, pulse), withAlpha(theme.accent, 0.0f));
    }

    // Stepped underside (a small floating island).
    DrawRectangleRec(Rectangle{r.x + 8.0f, r.y + r.height, r.width - 16.0f, 10.0f}, withAlpha(dark, alpha));
    DrawRectangleRec(Rectangle{r.x + r.width * 0.2f, r.y + r.height + 10.0f, r.width * 0.6f, 8.0f},
                     withAlpha(dark, alpha * 0.85f));
    DrawRectangleRec(Rectangle{r.x + r.width * 0.36f, r.y + r.height + 18.0f, r.width * 0.28f, 6.0f},
                     withAlpha(dark, alpha * 0.7f));

    DrawRectangleRec(r, withAlpha(body, alpha));
    DrawRectangleRec(Rectangle{r.x, r.y, r.width, 7.0f}, withAlpha(top, alpha));
    DrawRectangleRec(Rectangle{r.x, r.y + 7.0f, r.width, 2.0f}, withAlpha(dark, alpha * 0.6f));

    // Region material details.
    int detailCount = (int)(r.width / 22.0f);
    for (int i = 0; i < detailCount; i++) {
        float x = r.x + 8.0f + i * 22.0f;
        float h = hash01(node.id * 97 + regionIndex * 13 + i);
        switch (regionIndex) {
            case 0: // grass tufts
                DrawRectangleRec(Rectangle{x, r.y - 2.0f - h * 4.0f, 4.0f, 2.0f + h * 4.0f}, withAlpha(top, alpha));
                break;
            case 1: // stone joints
                if (i % 2 == 0) DrawRectangleRec(Rectangle{x, r.y + 10.0f, 2.0f, r.height - 10.0f}, withAlpha(dark, alpha));
                break;
            case 2: // glowing crystals under the platform
                if (h > 0.55f) {
                    float cy = r.y + r.height + 10.0f;
                    drawTriangleSafe(Vector2{x, cy}, Vector2{x + 6.0f, cy}, Vector2{x + 3.0f, cy + 8.0f + h * 6.0f},
                                     withAlpha(theme.light, alpha * 0.8f));
                }
                break;
            case 3: // rivets
                if (i % 2 == 1) DrawCircleV(Vector2{x, r.y + 18.0f}, 2.0f, withAlpha(top, alpha * 0.8f));
                break;
            default:
                break;
        }
    }

    // State outline: gold for the current node, accent for a reachable one.
    if (state == PlatformState::Current) {
        DrawRectangleLinesEx(Rectangle{r.x - 3.0f, r.y - 3.0f, r.width + 6.0f, r.height + 6.0f}, 3.0f, CURRENT_GOLD);
    } else if (state == PlatformState::Reachable) {
        DrawRectangleLinesEx(Rectangle{r.x - 2.0f, r.y - 2.0f, r.width + 4.0f, r.height + 4.0f}, 2.0f, theme.accent);
    } else {
        DrawRectangleLinesEx(r, 1.0f, withAlpha(OUTLINE_DARK, alpha));
    }
}

// Node badge: a small sign hanging under the platform, e.g. [HEAD] N1 Head.
void drawNodeBadge(const Node& node, int regionIndex, PlatformState state, bool isHead, bool isTail, bool done) {
    const RegionTheme& theme = REGION_THEMES[regionIndex];
    Rectangle r = getPlatformRect(node);

    float alpha = 1.0f;
    if (state == PlatformState::Ghost) alpha = 0.6f;
    if (state == PlatformState::Future) alpha = 0.5f;
    if (state == PlatformState::Completed) alpha = 0.7f;

    const char* tag = isHead ? "HEAD" : (isTail ? "TAIL" : nullptr);
    char idText[8];
    std::snprintf(idText, sizeof(idText), "N%d", node.id);
    int idWidth = MeasureText(idText, 20);
    int labelWidth = MeasureText(node.label, 20);
    int tagWidth = (tag != nullptr) ? MeasureText(tag, 10) + 8 : 0;
    int width = 16 + idWidth + 8 + labelWidth + (tag != nullptr ? tagWidth + 8 : 0) + (done ? 18 : 0);

    float x = r.x + r.width / 2.0f - width / 2.0f;
    float y = r.y + r.height + 6.0f;

    // Two short chains attach the sign to the platform.
    DrawRectangle((int)(x + 10.0f), (int)(r.y + r.height), 2, 6, withAlpha(OUTLINE_DARK, alpha));
    DrawRectangle((int)(x + width - 12.0f), (int)(r.y + r.height), 2, 6, withAlpha(OUTLINE_DARK, alpha));

    Rectangle sign = {x, y, (float)width, 26.0f};
    DrawRectangleRounded(sign, 0.3f, 4, withAlpha(UI_NAVY, 0.9f * alpha));
    DrawRectangleRoundedLinesEx(sign, 0.3f, 4, 1.5f, withAlpha(theme.accent, 0.55f * alpha));

    float cursor = x + 8.0f;
    if (tag != nullptr) {
        DrawRectangleRounded(Rectangle{cursor, y + 7.0f, (float)tagWidth, 13.0f}, 0.3f, 4, withAlpha(theme.accent, alpha));
        DrawText(tag, (int)(cursor + 4.0f), (int)(y + 8.0f), 10, withAlpha(OUTLINE_DARK, alpha));
        cursor += tagWidth + 8.0f;
    }
    DrawText(idText, (int)cursor, (int)(y + 3.0f), 20, withAlpha(theme.accent, alpha));
    cursor += idWidth + 8.0f;
    DrawText(node.label, (int)cursor, (int)(y + 3.0f), 20, withAlpha(UI_TEXT, alpha));
    cursor += labelWidth + 4.0f;

    if (done) { // check mark: this node has been visited
        Color check = withAlpha(Color{140, 230, 140, 255}, alpha);
        DrawLineEx(Vector2{cursor + 1.0f, y + 13.0f}, Vector2{cursor + 5.0f, y + 18.0f}, 2.5f, check);
        DrawLineEx(Vector2{cursor + 5.0f, y + 18.0f}, Vector2{cursor + 12.0f, y + 7.0f}, 2.5f, check);
    }
}

// Point on a quadratic Bezier curve.
Vector2 bezierPoint(Vector2 p0, Vector2 control, Vector2 p2, float t) {
    float u = 1.0f - t;
    return Vector2{u * u * p0.x + 2.0f * u * t * control.x + t * t * p2.x,
                   u * u * p0.y + 2.0f * u * t * control.y + t * t * p2.y};
}

// Circular return link: an elevated arc from the tail's portal back down to the head.
void drawLoopReturnArc(const Node& tail, const Node& head, LinkStyle style, float dimAlpha, bool showLabel, float time) {
    Color accent = REGION_THEMES[2].accent;
    Rectangle tailRect = getPlatformRect(tail);
    Rectangle headRect = getPlatformRect(head);
    Vector2 start = {tailRect.x + tailRect.width - 18.0f, tailRect.y - 84.0f};
    Vector2 end = {headRect.x + headRect.width / 2.0f, headRect.y - 60.0f};
    Vector2 control = {(start.x + end.x) / 2.0f, 190.0f};

    bool bright = style == LinkStyle::Bright;
    float alpha = bright ? 1.0f : dimAlpha;
    const int segments = 40;
    Vector2 previous = start;
    for (int i = 1; i <= segments; i++) {
        Vector2 point = bezierPoint(start, control, end, (float)i / segments);
        if (bright) {
            DrawLineEx(previous, point, 12.0f, withAlpha(accent, 0.18f));
            DrawLineEx(previous, point, 4.0f, accent);
        } else if (i % 2 == 0) { // dashed when it is not the link you can follow now
            DrawLineEx(previous, point, 2.0f, withAlpha(accent, alpha));
        }
        previous = point;
    }
    Vector2 nearEnd = bezierPoint(start, control, end, 0.97f);
    drawArrowHead(end, unitVector(nearEnd, end), bright ? 14.0f : 9.0f, withAlpha(accent, bright ? 1.0f : alpha));

    if (bright) {
        float t = std::fmod(time * 0.5f, 1.0f);
        DrawCircleV(bezierPoint(start, control, end, t), 4.0f, WHITE);
    }
    if (bright || showLabel) {
        Vector2 peak = bezierPoint(start, control, end, 0.5f);
        drawChip("tail->next = head", peak.x, peak.y - 30.0f, bright ? 20 : 10, accent, bright ? 1.0f : 0.8f);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// World drawing (reads the real nodes; never creates any)
// ---------------------------------------------------------------------------

void World::drawRegionLinks(int regionIndex, const WorldView& view) const {
    const RegionInfo& region = regions[regionIndex];
    const RegionTheme& theme = REGION_THEMES[regionIndex];
    bool active = regionIndex == view.activeRegion;
    float dimAlpha = active ? (view.showAllLinks ? 0.5f : 0.22f) : 0.1f;

    // Walk the real list. The `node == head` check stops the circular list.
    Node* head = getHead(regionIndex);
    Node* node = head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        Rectangle r = getPlatformRect(*node);
        bool isCurrent = active && node == view.currentNode;

        // next link
        if (node->next == nullptr) {
            // No connector for NULL: just an end marker (always shown in the NULL region).
            if (active && (isCurrent || view.showAllLinks || regionIndex == 3)) {
                drawNullMarker(Vector2{r.x + r.width + 2.0f, r.y + 9.0f}, 1.0f, 1.0f);
            }
        } else if (node->next->platformPosition.x > node->platformPosition.x) {
            Rectangle target = getPlatformRect(*node->next);
            LinkStyle style = LinkStyle::Dim;
            if (isCurrent) {
                style = (region.kind == ListKind::Doubly && view.emphasizePrevious) ? LinkStyle::Medium : LinkStyle::Bright;
            }
            drawConnector(Vector2{r.x + r.width + 2.0f, r.y + 9.0f}, Vector2{target.x - 2.0f, target.y + 9.0f},
                          theme.accent, style, "next", false, view.time, dimAlpha);
        } else {
            // A next link that points back to the left is the circular return: tail->next == head.
            LinkStyle style = isCurrent ? LinkStyle::Bright : LinkStyle::Dim;
            drawLoopReturnArc(*node, *node->next, style, active ? 0.4f : 0.1f, active && view.showAllLinks, view.time);
        }

        // previous link: only the doubly linked list stores it.
        if (region.kind == ListKind::Doubly) {
            if (node->previous == nullptr) {
                if (active && (isCurrent || view.showAllLinks)) {
                    drawNullMarker(Vector2{r.x - 2.0f, r.y + 21.0f}, -1.0f, 1.0f);
                }
            } else {
                Rectangle target = getPlatformRect(*node->previous);
                LinkStyle style = LinkStyle::Dim;
                if (isCurrent) {
                    style = view.emphasizePrevious ? LinkStyle::Bright : LinkStyle::Medium;
                }
                drawConnector(Vector2{r.x - 2.0f, r.y + 21.0f},
                              Vector2{target.x + target.width + 2.0f, target.y + 21.0f}, PREVIOUS_ORANGE, style,
                              "previous", true, view.time, dimAlpha);
            }
        }

        node = node->next;
        steps++;
        if (node == head) {
            break; // circular list: back at the start, stop drawing
        }
    }
}

void World::drawRegionPlatforms(int regionIndex, const WorldView& view) const {
    const RegionInfo& region = regions[regionIndex];
    bool active = regionIndex == view.activeRegion;
    bool completedRegion = regionIndex < view.activeRegion;

    Node* head = getHead(regionIndex);
    Node* node = head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        PlatformState state;
        if (completedRegion) state = PlatformState::Completed;
        else if (!active) state = PlatformState::Future;
        else if (node == view.currentNode) state = PlatformState::Current;
        else if (isPlatformSolid(view.currentNode, node, region.kind)) state = PlatformState::Reachable;
        else state = PlatformState::Ghost;

        // Head and tail are read from the links, not stored separately.
        bool isHead = node == head;
        bool isTail = (region.kind == ListKind::Circular) ? node->next == head : node->next == nullptr;
        bool visited = active && view.visitedIds != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION &&
                       view.visitedIds[node->id] && node != view.currentNode;

        drawPlatformBody(*node, regionIndex, state, view.time);
        drawNodeBadge(*node, regionIndex, state, isHead, isTail, completedRegion || visited);

        node = node->next;
        steps++;
        if (node == head) break; // circular list: back at the start
    }
}

void World::drawNullHazard(const WorldView& view) const {
    if (bridgeBuilt) {
        return;
    }
    // A missing connection, not a missing program: an empty gap with a broken bridge.
    Rectangle zone = getNullHazardZone();
    bool active = view.activeRegion == 3;
    float pulse = active ? 0.6f + 0.25f * std::sin(view.time * 4.0f) : 0.25f;

    DrawRectangleGradientV((int)zone.x, (int)(zone.y + 80.0f), (int)zone.width, (int)(zone.height - 80.0f),
                           withAlpha(NULL_AMBER, 0.0f), withAlpha(NULL_AMBER, 0.4f * pulse));

    float edgeX = zone.x - 10.0f; // the tail's right edge
    float y = zone.y - 10.0f;     // the tail's top
    Color plank = {130, 96, 64, 255};
    Color rope = {196, 170, 120, 255};
    DrawRectangleRec(Rectangle{edgeX, y + 4.0f, 22.0f, 7.0f}, plank);
    DrawRectanglePro(Rectangle{edgeX + 20.0f, y + 8.0f, 26.0f, 7.0f}, Vector2{0.0f, 3.5f},
                     40.0f + std::sin(view.time * 2.0f) * 4.0f, plank);
    DrawLineEx(Vector2{edgeX + 3.0f, y + 10.0f}, Vector2{edgeX + 8.0f + std::sin(view.time * 1.7f) * 3.0f, y + 70.0f},
               2.0f, rope);
    DrawLineEx(Vector2{edgeX + 13.0f, y + 10.0f}, Vector2{edgeX + 20.0f + std::sin(view.time * 1.3f) * 3.0f, y + 56.0f},
               2.0f, rope);

    drawCenteredText("no node here", zone.x + zone.width / 2.0f, y - 34.0f, 20, withAlpha(NULL_AMBER, 0.45f + 0.4f * pulse));
}

void World::drawLoopPortal(const WorldView& view) const {
    Rectangle portal = getPortalRect();
    if (portal.width <= 0.0f) {
        return;
    }
    const RegionTheme& theme = REGION_THEMES[2];
    bool active = view.activeRegion == 2;
    bool ready = active && view.currentNode == loopList.getTail();
    float alpha = active ? (ready ? 1.0f : 0.6f) : 0.25f;
    Vector2 center = {portal.x + portal.width / 2.0f, portal.y + portal.height / 2.0f};

    DrawCircleGradient((int)center.x, (int)center.y, 64.0f, withAlpha(theme.accent, 0.3f * alpha),
                       withAlpha(theme.accent, 0.0f));
    DrawEllipse((int)center.x, (int)center.y, 20.0f, 40.0f, withAlpha(OUTLINE_DARK, 0.85f * alpha));
    for (int i = 0; i < 3; i++) {
        float spin = view.time * (120.0f + i * 50.0f) + i * 120.0f;
        float radius = 18.0f - i * 5.0f;
        Color ring = (i % 2 == 0) ? theme.accent : theme.light;
        DrawRing(center, radius - 3.0f, radius, spin, spin + 230.0f, 24, withAlpha(ring, alpha));
    }
    DrawEllipseLines((int)center.x, (int)center.y, 20.0f, 40.0f, withAlpha(theme.accent, alpha));
}

void World::drawGates(const WorldView& view) const {
    for (int i = 0; i < REGION_COUNT; i++) {
        Rectangle gate;
        if (!getGateRect(i, gate)) continue;

        bool open = (i < view.activeRegion) || (i == view.activeRegion && view.gateOpen);
        bool isExit = i == REGION_COUNT - 1;
        Color frame = open ? (isExit ? CURRENT_GOLD : REGION_THEMES[i].accent) : Color{96, 100, 116, 255};
        float centerX = gate.x + gate.width / 2.0f;

        if (open) {
            DrawCircleGradient((int)centerX, (int)(gate.y + 35.0f), 64.0f, withAlpha(frame, 0.3f), withAlpha(frame, 0.0f));
            DrawRectangleRec(gate, withAlpha(frame, 0.35f + 0.1f * std::sin(view.time * 3.0f)));
        } else {
            DrawRectangleRec(gate, Color{24, 26, 38, 235});
            for (int bar = 1; bar <= 3; bar++) {
                DrawRectangle((int)(gate.x + bar * 10.0f - 1.0f), (int)gate.y, 2, (int)gate.height, withAlpha(frame, 0.7f));
            }
            DrawRectangle((int)(centerX - 6.0f), (int)(gate.y + 32.0f), 12, 10, frame);
            DrawRing(Vector2{centerX, gate.y + 32.0f}, 3.0f, 5.0f, 180.0f, 360.0f, 8, frame);
        }

        // Posts and lintel.
        DrawRectangle((int)(gate.x - 6.0f), (int)gate.y, 6, (int)gate.height, frame);
        DrawRectangle((int)(gate.x + gate.width), (int)gate.y, 6, (int)gate.height, frame);
        DrawRectangle((int)(gate.x - 8.0f), (int)(gate.y - 8.0f), (int)(gate.width + 16.0f), 8, frame);

        const char* label = open ? (isExit ? "EXIT" : "GATE") : "LOCKED";
        drawChip(label, centerX, gate.y - 24.0f, 10, frame, (i == view.activeRegion) ? 1.0f : 0.5f);
    }
}

void World::drawCheckpointAndCurrentMarkers(const WorldView& view) const {
    // Checkpoint flag on the node recovery returns you to.
    const Node* checkpoint = view.checkpointNode;
    if (checkpoint != nullptr) {
        Rectangle r = getPlatformRect(*checkpoint);
        const RegionInfo& region = regions[view.activeRegion];
        bool gateOnLeftHere = region.gateNode == checkpoint && region.gateOnLeft;
        float poleX = gateOnLeftHere ? r.x + r.width - 26.0f : r.x + 16.0f;
        float wave = std::sin(view.time * 4.0f) * 2.0f;

        DrawCircleGradient((int)poleX, (int)(r.y - 30.0f), 26.0f, withAlpha(CHECKPOINT_CREAM, 0.25f),
                           withAlpha(CHECKPOINT_CREAM, 0.0f));
        DrawRectangle((int)(poleX - 1.0f), (int)(r.y - 42.0f), 3, 42, Color{206, 196, 176, 255});
        drawTriangleSafe(Vector2{poleX + 2.0f, r.y - 42.0f}, Vector2{poleX + 2.0f, r.y - 28.0f},
                         Vector2{poleX + 20.0f, r.y - 35.0f + wave}, CHECKPOINT_CREAM);
    }

    // "current" pointer tag above the node the player is on.
    const Node* current = view.currentNode;
    if (current != nullptr) {
        Rectangle r = getPlatformRect(*current);
        float bob = std::sin(view.time * 3.0f) * 3.0f;
        float centerX = r.x + r.width / 2.0f;
        float y = r.y - 104.0f + bob;
        int width = MeasureText("current", 10);
        DrawRectangleRounded(Rectangle{centerX - width / 2.0f - 6.0f, y, width + 12.0f, 16.0f}, 0.4f, 4, CURRENT_GOLD);
        DrawText("current", (int)(centerX - width / 2.0f), (int)(y + 3.0f), 10, OUTLINE_DARK);
        drawTriangleSafe(Vector2{centerX - 5.0f, y + 16.0f}, Vector2{centerX + 5.0f, y + 16.0f},
                         Vector2{centerX, y + 23.0f}, CURRENT_GOLD);
    }
}

void World::draw(const WorldView& view) const {
    drawNullHazard(view);

    // Links first, so platform edges sit on top of connector ends.
    for (int i = 0; i < REGION_COUNT; i++) {
        drawRegionLinks(i, view);
    }
    for (int i = 0; i < REGION_COUNT; i++) {
        drawRegionPlatforms(i, view);
    }

    drawLoopPortal(view);
    drawGates(view);
    drawCheckpointAndCurrentMarkers(view);
}
