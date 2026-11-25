#pragma once

#include "raylib.h"
#include "Node.h"
#include "SinglyLinkedList.h"
#include "DoublyLinkedList.h"
#include "CircularSinglyLinkedList.h"
#include "LinkedQueue.h"
#include "LinkedStack.h"

// Which kind of list a region is built from.
enum class ListKind { Singly, Doubly, Circular, Stack, Queue };

// Safety bound for every loop that walks a region's list.
// No region has this many nodes, so reaching it means a link is broken.
constexpr int MAX_NODES_PER_REGION = 16;

constexpr int REGION_COUNT = 6;
constexpr float WORLD_WIDTH = 10000.0f;
constexpr float WORLD_KILL_Y = 760.0f; // falling below this line triggers recovery

// Fixed description of one region. This is UI and layout data only:
// the platforms and their links live in the region's owning list.
struct RegionInfo {
    const char* placeName; // e.g. "The First Connection"
    const char* title;     // the data structure it teaches
    ListKind kind;
    float left;
    float right;
    Node* gateNode;  // NON-OWNING: the node the exit gate stands on (nullptr = no gate yet)
    bool gateOnLeft; // gate on the left or right end of that platform
    float waterline; // world y of water the traveler can fall into (splash), or 0 for none
};

// TRAVERSAL RULE A (link-gated solidity):
// a platform is solid only if it is the current node, or the node that one of
// the current node's permitted links points to. A stack only lets you reach its
// top, so there the top (the list's head) is the one other solid platform.
// Collision and drawing both use this one function, so what you see is what the links allow.
bool isPlatformSolid(const Node* current, const Node* candidate, ListKind kind, const Node* head);

// Owns the six region lists for the whole stage.
class World {
public:
    World() = default;
    ~World() = default; // each list's destructor deletes its own nodes

    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Allocates every region's nodes. The lists must be empty.
    void build();

    // Deletes every node. Callers must first drop all non-owning Node* they hold.
    void clearAll();

    Node* getHead(int regionIndex) const;
    const RegionInfo& getRegion(int regionIndex) const;

    // Region 3: the tail of the circular list and the root hollow that follows tail->next.
    Node* getLoopTail() const;
    Rectangle getPortalRect() const;

    // Region 4: the gap where the tail's next is NULL, and the fix.
    Rectangle getNullHazardZone() const;
    bool isBridgeBuilt() const;
    Node* appendBridgeNode(); // calls append() on Region 4's list
    Rectangle getBridgeRect() const; // layout only: where append() puts the bridge (the art paints its far landing)

    // Region 5: the stack of stair stones on the lighthouse. The floor stone at the tower's foot
    // is the bottom; push() sets the next stone on top, pop() takes the top one off.
    static constexpr int STONE_SLOTS = 5;      // the floor stone and four stair stones
    Node* pushStone();                         // nullptr when the stair already reaches the bell
    bool popStone();                           // false at the floor stone: it is the bottom (its next is NULL)
    Rectangle getStoneSlot(int id) const;      // layout only: where push() puts the stone with this ID

    // Region 6: the queue of boats in the harbor. Each new boat joins the rear; the front one leaves.
    static constexpr int BOAT_SLOTS = 7;       // six boats, then the ship
    Node* enqueueBoat();                       // nullptr when the ship has already come
    bool dequeueBoat();
    bool boatsRemain() const;
    Node* getQueueRear() const;
    Rectangle getBoatSlot(int id) const;       // layout only: where the boat with this ID moors

    // Returns false if the region has no gate yet.
    bool getGateRect(int regionIndex, Rectangle& outRect) const;

private:
    void verifyLinks() const;

    // The six owning lists. Every platform in the game is a node in one of these.
    SinglyLinkedList forwardList;       // Region 1
    DoublyLinkedList twoWayList;        // Region 2
    CircularSinglyLinkedList loopList;  // Region 3
    SinglyLinkedList nullList;          // Region 4
    LinkedStack stoneStack;             // Region 5
    LinkedQueue boatQueue;              // Region 6

    RegionInfo regions[REGION_COUNT] = {};
    bool bridgeBuilt = false;
    Rectangle nullGap = {}; // Region 4: the gap after N3, measured once when the list is built
    int nextBoatId = 1;     // Region 6: the next boat to call (BOAT_SLOTS + 1 once the ship has come)
};
