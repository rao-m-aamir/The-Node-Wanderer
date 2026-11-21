#pragma once

#include "raylib.h"
#include "Node.h"
#include "SinglyLinkedList.h"
#include "DoublyLinkedList.h"
#include "CircularSinglyLinkedList.h"

// Which kind of list a region is built from.
enum class ListKind { Singly, Doubly, Circular };

// Safety bound for every loop that walks a region's list.
// No region has this many nodes, so reaching it means a link is broken.
constexpr int MAX_NODES_PER_REGION = 16;

constexpr int REGION_COUNT = 4;
constexpr float WORLD_WIDTH = 6800.0f;
constexpr float WORLD_KILL_Y = 760.0f; // falling below this line triggers recovery

// Fixed description of one region. This is UI and layout data only:
// the platforms and their links live in the region's owning list.
struct RegionInfo {
    const char* title;
    ListKind kind;
    float left;
    float right;
    Node* gateNode;  // NON-OWNING: the node the exit gate stands on (nullptr = no gate yet)
    bool gateOnLeft; // gate on the left or right end of that platform
};

// What the renderer needs from gameplay. Read-only, non-owning pointers only.
struct WorldView {
    int activeRegion;
    const Node* currentNode;
    const Node* checkpointNode;
    const bool* visitedIds;  // visitedIds[node id] for the active region (progress, not links)
    bool gateOpen;
    bool showAllLinks;       // inspector open: show every stored link more clearly
    bool emphasizePrevious;  // Region 2 puzzle step that uses previous
    float time;
};

// TRAVERSAL RULE A (link-gated solidity):
// a platform is solid only if it is the current node, or the node that one of
// the current node's permitted links points to. Collision and drawing both
// use this one function, so what you see is what the links allow.
bool isPlatformSolid(const Node* current, const Node* candidate, ListKind kind);

// Owns the four region lists for the whole stage.
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

    // Region 3: the tail of the circular list and the portal that shows tail->next.
    Node* getLoopTail() const;
    Rectangle getPortalRect() const;

    // Region 4: the gap where the tail's next is NULL, and the fix.
    Rectangle getNullHazardZone() const;
    bool isBridgeBuilt() const;
    Node* appendBridgeNode(); // calls append() on Region 4's list

    // Returns false if the region has no gate yet.
    bool getGateRect(int regionIndex, Rectangle& outRect) const;

    // Draws platforms, links, gates, portal, and hazard (world coordinates).
    void draw(const WorldView& view) const;

private:
    void verifyLinks() const;
    void drawRegionLinks(int regionIndex, const WorldView& view) const;
    void drawRegionPlatforms(int regionIndex, const WorldView& view) const;
    void drawNullHazard(const WorldView& view) const;
    void drawLoopPortal(const WorldView& view) const;
    void drawGates(const WorldView& view) const;
    void drawCheckpointAndCurrentMarkers(const WorldView& view) const;

    // The four owning lists. Every platform in the game is a node in one of these.
    SinglyLinkedList forwardList;       // Region 1
    DoublyLinkedList twoWayList;        // Region 2
    CircularSinglyLinkedList loopList;  // Region 3
    SinglyLinkedList nullList;          // Region 4

    RegionInfo regions[REGION_COUNT] = {};
    bool bridgeBuilt = false;
};
