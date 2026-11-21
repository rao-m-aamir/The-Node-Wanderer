#pragma once

#include "raylib.h"
#include "Node.h"

// Tunable movement values (pixels and seconds).
struct PhysicsSettings {
    float moveSpeed = 260.0f;
    float jumpImpulse = 720.0f;
    float gravity = 1800.0f;
    float maxFallSpeed = 1100.0f;
};

struct Player {
    Vector2 position; // top-left corner of the collision box, world coordinates
    Vector2 velocity;
    Vector2 size;
    bool grounded;
    float facing;      // +1 facing right, -1 facing left (drawing only)
    Node* currentNode; // NON-OWNING: points into the active region's list. Never delete it.

    // Animation only. Physics and the list never read these.
    float walkPhase;
    float landTimer; // > 0 for a moment after landing (squash pose)
};

constexpr float LAND_POSE_SECONDS = 0.14f;

Rectangle getPlayerRect(const Player& player);

// Puts the player on top of a node's platform (does not change currentNode).
void placePlayerOnPlatform(Player& player, const Node& node);

// Reads input, applies gravity, and moves the player. Collisions happen in Game.
void updatePlayerMovement(Player& player, const PhysicsSettings& settings, float dt);

// Draws the traveler pixel sprite (idle, walk, jump, fall, and landing poses).
// `feet` is the point between the feet in the target's coordinates, and `pixel`
// is the size of one sprite pixel there (1 in the Region 1 canvas, 2 in world space).
void drawTraveler(const Player& player, float time, Vector2 feet, float pixel);
