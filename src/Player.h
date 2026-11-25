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

constexpr int SCARF_POINTS = 6;

// Secondary motion, for drawing only: the scarf's tail (a short chain of points that trails,
// falls, and flutters) and the lantern swinging on its handle. Physics never reads it.
struct TravelerMotion {
    bool ready = false;
    Vector2 scarf[SCARF_POINTS] = {};    // canvas pixels without the camera; scarf[0] is tied at the neck
    Vector2 scarfOld[SCARF_POINTS] = {};
    float flutter = 0.0f;                // how far the cloth ripples, in canvas pixels (speed and wind)
    float lanternAngle = 0.0f;           // radians from hanging straight down
    float lanternSpin = 0.0f;
    float lastSpeed = 0.0f;
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
    TravelerMotion motion;
};

constexpr float LAND_POSE_SECONDS = 0.14f;

Rectangle getPlayerRect(const Player& player);

// Puts the player on top of a node's platform (does not change currentNode).
void placePlayerOnPlatform(Player& player, const Node& node);

// Applies one frame of input (move: -1..1, jump pressed), gravity, and motion.
// Collisions happen in Game.
void updatePlayerMovement(Player& player, const PhysicsSettings& settings, float move, bool jumpPressed, float dt);

// Draws the traveler pixel sprite (idle, walk, jump, fall, and landing poses).
// `feet` is the point between the feet in the target's coordinates, and `pixel`
// is the size of one sprite pixel there (1 in the Region 1 canvas, 2 in world space).
void drawTraveler(const Player& player, float time, Vector2 feet, float pixel);

// Moves the scarf and the lantern one frame on. `wind` is the wind where the traveler stands
// (windAt); gusts make the scarf stream and ripple. Animation only.
void updateTravelerMotion(Player& player, float dt, float wind);
void resetTravelerMotion(Player& player); // after a jump in position (region change, recovery)
float lanternSwing(const Player& player); // how far the lantern hangs to the side, in canvas pixels

// Layout check: runs the real movement code to see whether the traveler can get from the top of
// `from` onto the top of `to` (run to the nearer edge, jump, steer, and land with their middle
// over `to`; a platform right overhead is jumped to from under its middle). Used at startup so
// every jump the route needs is known to be possible.
bool canJumpBetween(const Node& from, const Node& to, const PhysicsSettings& settings, Vector2 size);
// The same check for platforms given by their layout (nodes that push() or enqueue() create later).
bool canJumpBetweenRects(Rectangle from, Rectangle to, const PhysicsSettings& settings, Vector2 size);
