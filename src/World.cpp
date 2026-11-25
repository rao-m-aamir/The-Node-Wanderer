#include "World.h"

#include <cassert>

// World owns the six lists and answers questions about them.
// It does no drawing: the art files read the same nodes to draw each region.

// ---------------------------------------------------------------------------
// Traversal rule
// ---------------------------------------------------------------------------

bool isPlatformSolid(const Node* current, const Node* candidate, ListKind kind, const Node* head) {
    if (current == nullptr || candidate == nullptr) {
        return false;
    }
    if (candidate == current) {
        return true; // you can always stand on the node you are at
    }
    if (kind == ListKind::Stack) {
        return candidate == head; // a stack only lets you reach its top (the stones under it wait their turn)
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
// It starts right at the edge of N3 and reaches the far cliff, where the dawn gate stands.
static const Vector2 BRIDGE_POSITION = {6000.0f, 470.0f};
static const float BRIDGE_WIDTH = 420.0f;

// Layout of the nodes that Regions 5 and 6 create while you play. push() and enqueue() allocate
// each node with these values, and the art paints them from the same table.
namespace {
struct Slot {
    int id;
    const char* label;
    Vector2 position;
    float width;
};
} // namespace

// Region 5: the floor stone at the lighthouse's foot, then four stair stones, one above the
// other along the tower wall (it narrows as it rises, so each stone sits a little further left).
static const Slot STONE_LAYOUT[World::STONE_SLOTS] = {
    {1, "Tower Foot", {7300.0f, 600.0f}, 460.0f},
    {2, "Wave Stone", {7430.0f, 504.0f}, 150.0f},
    {3, "Shell Stone", {7424.0f, 408.0f}, 150.0f},
    {4, "Gull Stone", {7418.0f, 312.0f}, 150.0f},
    {5, "Bell Stone", {7412.0f, 216.0f}, 150.0f},
};

// Region 6: boats moored in a line across the harbor, and the ship at the far end.
static const Slot BOAT_LAYOUT[World::BOAT_SLOTS] = {
    {1, "Red Skiff", {8190.0f, 596.0f}, 170.0f},
    {2, "Blue Dory", {8456.0f, 586.0f}, 150.0f},
    {3, "Green Punt", {8710.0f, 598.0f}, 180.0f},
    {4, "Yellow Gig", {8982.0f, 580.0f}, 150.0f},
    {5, "White Yawl", {9240.0f, 592.0f}, 170.0f},
    {6, "Old Lugger", {9510.0f, 574.0f}, 170.0f},
    {7, "The Swallow", {9780.0f, 494.0f}, 200.0f},
};
static const int BOATS_AT_START = 3; // the first boats are already waiting in line

void World::build() {
    assert(forwardList.getHead() == nullptr && "build() needs empty lists");

    // The positions describe each place's route: it rises, dips, and turns with the scenery.
    // List order alone decides which platform is reachable; the jumps are checked in Game.

    // Region 1: singly linked list. Each append() sets old tail->next = new node.
    // The path climbs from the forest floor onto a log high between the trunks, then steps down.
    forwardList.append(1, "Grove Edge", Vector2{70.0f, 560.0f}, 300.0f);
    forwardList.append(2, "Mossy Rock", Vector2{470.0f, 470.0f}, 170.0f);
    forwardList.append(3, "Fallen Log", Vector2{760.0f, 380.0f}, 240.0f);
    forwardList.append(4, "Old Ruin",   Vector2{1100.0f, 420.0f}, 190.0f);
    Node* forwardTail = forwardList.append(5, "Arch Cliff", Vector2{1400.0f, 500.0f}, 320.0f);

    // Region 2: doubly linked list. append() sets both tail->next and new->previous.
    // The path dips to a hall by the water, then climbs a ruined tower to the shrine.
    twoWayList.append(1, "Gatehouse",  Vector2{1880.0f, 470.0f}, 250.0f);
    twoWayList.append(2, "Old Hall",   Vector2{2230.0f, 550.0f}, 230.0f);
    twoWayList.append(3, "Colonnade",  Vector2{2560.0f, 460.0f}, 200.0f);
    twoWayList.append(4, "Key Shrine", Vector2{2860.0f, 390.0f}, 220.0f);
    twoWayList.append(5, "Far Steps",  Vector2{3180.0f, 480.0f}, 300.0f);

    // Region 3: circular list. append() keeps tail->next == head after every call.
    // The path climbs over the front of the ancient tree; the way back runs under its roots.
    Node* loopHead = loopList.append(1, "Garden Gate", Vector2{3680.0f, 560.0f}, 220.0f);
    loopList.append(2, "Moon Cap",    Vector2{3990.0f, 470.0f}, 170.0f);
    loopList.append(3, "Flower Bed",  Vector2{4250.0f, 380.0f}, 210.0f);
    loopList.append(4, "Root Hollow", Vector2{4560.0f, 470.0f}, 230.0f);

    // Region 4: singly linked list whose tail (N3 "Edge") has next == nullptr.
    // The gap after it is the NULL hazard. The player fixes it later with append().
    nullList.append(1, "Lookout",   Vector2{5080.0f, 500.0f}, 260.0f);
    nullList.append(2, "Rope Post", Vector2{5440.0f, 420.0f}, 200.0f);
    Node* edge = nullList.append(3, "Edge", Vector2{5740.0f, 470.0f}, 260.0f);
    bridgeBuilt = false;
    // The gap is measured now, from the tail whose next is NULL. It keeps its place after the
    // repair, so the art can fade it out while the bridge assembles over it.
    float gapStart = edge->platformPosition.x + edge->platformWidth;
    nullGap = Rectangle{gapStart + 10.0f, edge->platformPosition.y + 10.0f,
                        BRIDGE_POSITION.x + BRIDGE_WIDTH - gapStart - 10.0f, WORLD_KILL_Y - edge->platformPosition.y};

    // Region 5: a stack holding only its bottom, the floor stone (its next is nullptr).
    // The player pushes the stair stones onto it, then pops them off again.
    const Slot& floor = STONE_LAYOUT[0];
    Node* floorStone = stoneStack.push(floor.id, floor.label, floor.position, floor.width);

    // Region 6: a queue with the first boats already in line. enqueue() keeps rear->next == nullptr.
    nextBoatId = 1;
    for (int i = 0; i < BOATS_AT_START; i++) {
        enqueueBoat();
    }

    regions[0] = RegionInfo{"The First Connection", "Singly Linked List", ListKind::Singly, 0.0f, 1800.0f,
                            forwardTail, false, 0.0f};
    regions[1] = RegionInfo{"The Forgotten Return", "Doubly Linked List", ListKind::Doubly, 1800.0f, 3600.0f,
                            twoWayList.getTail(), false, 624.0f}; // the water's surface (canvas row 312 in RuinsArt.cpp)
    regions[2] = RegionInfo{"The Endless Garden", "Circular Singly Linked List", ListKind::Circular, 3600.0f,
                            5000.0f, loopHead, true, 0.0f};
    regions[3] = RegionInfo{"The Edge of Nothing", "NULL Pointer Safety", ListKind::Singly, 5000.0f, 6800.0f,
                            nullptr, false, 0.0f}; // the exit gate appears with the bridge
    regions[4] = RegionInfo{"The Lighthouse Stair", "Stack (Linked List)", ListKind::Stack, 6800.0f, 8080.0f,
                            floorStone, true, 672.0f}; // the sea door in the tower's foot; the sea below
    regions[5] = RegionInfo{"The Patient Harbor", "Queue (Linked List)", ListKind::Queue, 8080.0f, WORLD_WIDTH,
                            nullptr, false, 616.0f}; // the ship's door appears with the ship; harbor water

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
    stoneStack.clear();
    boatQueue.clear();
    bridgeBuilt = false;
    nextBoatId = 1;
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

    // Region 5: walking next from the top ends at the bottom stone, whose next is null.
    count = 0;
    node = stoneStack.getTop();
    while (node != nullptr && node->next != nullptr && count < MAX_NODES_PER_REGION) {
        node = node->next;
        count++;
    }
    checkLink(node != nullptr && node->next == nullptr, "Region 5 bottom->next == nullptr");
    checkLink(node != nullptr && node->id == 1, "Region 5 the floor stone is the bottom of the stack");

    // Region 6: walking next from the front reaches the rear, and the rear's next is null.
    count = 0;
    node = boatQueue.getFront();
    while (node != nullptr && node != boatQueue.getRear() && count < MAX_NODES_PER_REGION) {
        node = node->next;
        count++;
    }
    checkLink(node != nullptr && node == boatQueue.getRear() && count == BOATS_AT_START - 1,
              "Region 6 front reaches rear through next");
    checkLink(boatQueue.getRear() != nullptr && boatQueue.getRear()->next == nullptr, "Region 6 rear->next == nullptr");
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
        case 4: return stoneStack.getTop();   // a stack's head is its top
        case 5: return boatQueue.getFront();  // a queue's head is its front
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
    return nullGap; // collision with it is only checked while the bridge is not built (Game.cpp)
}

Rectangle World::getBridgeRect() const {
    return Rectangle{BRIDGE_POSITION.x, BRIDGE_POSITION.y, BRIDGE_WIDTH, PLATFORM_HEIGHT};
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

Node* World::pushStone() {
    Node* top = stoneStack.getTop();
    int id = (top != nullptr) ? top->id + 1 : 1;
    if (id > STONE_SLOTS) {
        return nullptr; // the stair already reaches the bell
    }
    const Slot& slot = STONE_LAYOUT[id - 1];
    return stoneStack.push(slot.id, slot.label, slot.position, slot.width); // stone->next = top; top = stone;
}

bool World::popStone() {
    Node* top = stoneStack.getTop();
    if (top == nullptr || top->next == nullptr) {
        return false; // the floor stone is the bottom: there is nothing under it to stand on
    }
    return stoneStack.pop(); // top = top->next; the old top is deleted
}

Rectangle World::getStoneSlot(int id) const {
    if (id < 1 || id > STONE_SLOTS) return Rectangle{0, 0, 0, 0};
    const Slot& slot = STONE_LAYOUT[id - 1];
    return Rectangle{slot.position.x, slot.position.y, slot.width, PLATFORM_HEIGHT};
}

Node* World::enqueueBoat() {
    if (nextBoatId > BOAT_SLOTS) {
        return nullptr; // the ship has come: no boat is left to call
    }
    const Slot& slot = BOAT_LAYOUT[nextBoatId - 1];
    nextBoatId++;
    Node* boat = boatQueue.enqueue(slot.id, slot.label, slot.position, slot.width); // rear->next = boat; rear = boat;
    if (boat->id == BOAT_SLOTS) {
        regions[5].gateNode = boat; // the way out is the ship's door
    }
    return boat;
}

bool World::dequeueBoat() {
    Node* front = boatQueue.getFront();
    if (front == nullptr) {
        return false;
    }
    if (regions[5].gateNode == front) {
        regions[5].gateNode = nullptr; // never keep a pointer to a node that is about to be deleted
    }
    return boatQueue.dequeue(); // front = front->next; the old front is deleted
}

bool World::boatsRemain() const {
    return nextBoatId <= BOAT_SLOTS;
}

Node* World::getQueueRear() const {
    return boatQueue.getRear();
}

Rectangle World::getBoatSlot(int id) const {
    if (id < 1 || id > BOAT_SLOTS) return Rectangle{0, 0, 0, 0};
    const Slot& slot = BOAT_LAYOUT[id - 1];
    return Rectangle{slot.position.x, slot.position.y, slot.width, PLATFORM_HEIGHT};
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
