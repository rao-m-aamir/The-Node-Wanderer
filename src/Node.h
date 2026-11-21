#pragma once

#include "raylib.h"

// Height of every platform's collision surface in pixels. The width comes from each node.
constexpr float PLATFORM_HEIGHT = 28.0f;

// One Node = one platform in the world.
// The game draws, collides with, and inspects these exact objects.
// There is no second "platform" structure anywhere in the program.
struct Node {
    int id;                   // stable ID shown in the game (N1, N2, ...)
    const char* label;        // short readable name
    Vector2 platformPosition; // world coordinates of the platform's top-left corner
    float platformWidth;
    Node* next;               // the following node, or nullptr
    Node* previous;           // used only by the doubly linked list; nullptr otherwise
};

// Number of nodes created with new and not yet deleted.
// Each list increases it in append() and decreases it in clear().
// It must be 0 when the program exits (main.cpp prints it).
inline int g_liveNodeCount = 0;

// Collision rectangle, derived from the node's own fields.
inline Rectangle getPlatformRect(const Node& node) {
    return Rectangle{node.platformPosition.x, node.platformPosition.y, node.platformWidth, PLATFORM_HEIGHT};
}
