#include "World.h"

#include <cassert>

// World owns the four lists and answers questions about them.
// It does no drawing: the art files read the same nodes to draw each region.

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

// Layout of Region 4's bridge node. append() creates it when the player fixes the link.
// It starts right at the edge of N3, so the repaired route continues without a gap.
static const Vector2 BRIDGE_POSITION = {6040.0f, 450.0f};
static const float BRIDGE_WIDTH = 320.0f;

void World::build() {
    assert(forwardList.getHead() == nullptr && "build() needs empty lists");

    // Region 1: singly linked list. Each append() sets old tail->next = new node.
    // Labels name what each platform looks like in its region.
    forwardList.append(1, "Grove Edge", Vector2{90.0f, 470.0f}, 260.0f);
    forwardList.append(2, "Mossy Rock", Vector2{460.0f, 430.0f}, 230.0f);
    forwardList.append(3, "Fallen Log", Vector2{800.0f, 410.0f}, 230.0f);
    forwardList.append(4, "Old Ruin",   Vector2{1140.0f, 440.0f}, 230.0f);
    Node* forwardTail = forwardList.append(5, "Arch Cliff", Vector2{1480.0f, 470.0f}, 260.0f);

    // Region 2: doubly linked list. append() sets both tail->next and new->previous.
    twoWayList.append(1, "Gatehouse",  Vector2{1890.0f, 480.0f}, 260.0f);
    twoWayList.append(2, "Old Hall",   Vector2{2260.0f, 450.0f}, 230.0f);
    twoWayList.append(3, "Colonnade",  Vector2{2600.0f, 420.0f}, 230.0f);
    twoWayList.append(4, "Key Shrine", Vector2{2940.0f, 450.0f}, 230.0f);
    twoWayList.append(5, "Far Steps",  Vector2{3280.0f, 480.0f}, 260.0f);

    // Region 3: circular list. append() keeps tail->next == head after every call.
    // The region fits on one screen, so the way back to the head stays visible.
    Node* loopHead = loopList.append(1, "Garden Gate", Vector2{3700.0f, 480.0f}, 200.0f);
    loopList.append(2, "Moon Cap",    Vector2{4000.0f, 420.0f}, 180.0f);
    loopList.append(3, "Flower Bed",  Vector2{4280.0f, 420.0f}, 180.0f);
    loopList.append(4, "Root Hollow", Vector2{4560.0f, 480.0f}, 220.0f);

    // Region 4: singly linked list whose tail (N3 "Edge") has next == nullptr.
    // The gap after it is the NULL hazard. The player fixes it later with append().
    nullList.append(1, "Lookout",   Vector2{5090.0f, 480.0f}, 260.0f);
    nullList.append(2, "Rope Post", Vector2{5460.0f, 450.0f}, 230.0f);
    nullList.append(3, "Edge",      Vector2{5800.0f, 450.0f}, 240.0f);
    bridgeBuilt = false;

    regions[0] = RegionInfo{"The First Connection", "Singly Linked List", ListKind::Singly, 0.0f, 1800.0f,
                            forwardTail, false};
    regions[1] = RegionInfo{"The Forgotten Return", "Doubly Linked List", ListKind::Doubly, 1800.0f, 3600.0f,
                            twoWayList.getTail(), false};
    regions[2] = RegionInfo{"The Endless Garden", "Circular Singly Linked List", ListKind::Circular, 3600.0f,
                            5000.0f, loopHead, true};
    regions[3] = RegionInfo{"The Edge of Nothing", "NULL Pointer Safety", ListKind::Singly, 5000.0f, WORLD_WIDTH,
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

// The root hollow at the right end of the loop's tail. Walking into it follows tail->next.
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
