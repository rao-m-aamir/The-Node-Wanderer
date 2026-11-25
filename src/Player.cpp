#include "Player.h"

#include <algorithm>
#include <cmath>

#include "World.h" // WORLD_KILL_Y for the jump check

Rectangle getPlayerRect(const Player& player) {
    return Rectangle{player.position.x, player.position.y, player.size.x, player.size.y};
}

void placePlayerOnPlatform(Player& player, const Node& node) {
    Rectangle platform = getPlatformRect(node);
    player.position.x = platform.x + platform.width * 0.5f - player.size.x * 0.5f;
    player.position.y = platform.y - player.size.y;
    player.velocity = Vector2{0.0f, 0.0f};
    player.grounded = true;
}

void updatePlayerMovement(Player& player, const PhysicsSettings& settings, float move, bool jumpPressed, float dt) {
    float direction = move;

    player.velocity.x = direction * settings.moveSpeed;
    if (direction != 0.0f) {
        player.facing = direction;
    }

    // Jump only from the ground: no second jump while airborne.
    if (player.grounded && jumpPressed) {
        player.velocity.y = -settings.jumpImpulse;
        player.grounded = false;
    }

    // Gravity is constant, so the vertical motion is integrated exactly (with the fall-speed cap).
    // The jump arc is then the same at 30, 60, or 144 FPS.
    float v0 = player.velocity.y;
    float g = settings.gravity;
    float vMax = settings.maxFallSpeed;
    float dy;
    if (v0 >= vMax) {
        player.velocity.y = vMax;
        dy = vMax * dt;
    } else {
        float v1 = v0 + g * dt;
        if (v1 <= vMax) {
            dy = v0 * dt + 0.5f * g * dt * dt;
            player.velocity.y = v1;
        } else { // reaches the cap during this frame
            float tCap = (vMax - v0) / g;
            dy = v0 * tCap + 0.5f * g * tCap * tCap + vMax * (dt - tCap);
            player.velocity.y = vMax;
        }
    }

    player.position.x += player.velocity.x * dt;
    player.position.y += dy;

    // Animation timers.
    if (player.grounded && direction != 0.0f) {
        player.walkPhase += dt * 14.0f;
    }
    if (player.landTimer > 0.0f) {
        player.landTimer -= dt;
    }
}

bool canJumpBetween(const Node& from, const Node& to, const PhysicsSettings& settings, Vector2 size) {
    return canJumpBetweenRects(getPlatformRect(from), getPlatformRect(to), settings, size);
}

bool canJumpBetweenRects(Rectangle a, Rectangle b, const PhysicsSettings& settings, Vector2 size) {
    const float direction = (b.x + b.width / 2.0f >= a.x + a.width / 2.0f) ? 1.0f : -1.0f;
    const float dt = 1.0f / 60.0f;

    Player p = {};
    p.size = size;
    p.grounded = true;
    p.position = Vector2{(direction > 0.0f) ? a.x + a.width - size.x : a.x, a.y - size.y}; // standing at the edge
    const float underTarget = b.x + b.width / 2.0f - size.x / 2.0f;
    if (b.y < a.y && underTarget >= a.x && underTarget + size.x <= a.x + a.width) {
        p.position.x = underTarget; // the target is right overhead: jump straight up through it (one-way platforms)
    }
    p.velocity = Vector2{0.0f, 0.0f};

    for (int frame = 0; frame < 240; frame++) {
        float centerX = p.position.x + size.x / 2.0f;
        bool overTarget = centerX > b.x + 12.0f && centerX < b.x + b.width - 12.0f;
        float move = overTarget ? 0.0f : direction; // steer until over the target, then drop onto it
        float previousBottom = p.position.y + size.y;
        updatePlayerMovement(p, settings, move, frame == 0, dt);
        p.grounded = false;

        float bottom = p.position.y + size.y;
        bool overlapsX = p.position.x + size.x > b.x && p.position.x < b.x + b.width;
        if (p.velocity.y >= 0.0f && overlapsX && previousBottom <= b.y + 1.0f && bottom >= b.y) {
            centerX = p.position.x + size.x / 2.0f;
            return centerX >= b.x && centerX <= b.x + b.width; // landed with the middle over it
        }
        if (p.position.y > WORLD_KILL_Y) return false;
    }
    return false;
}

// ---------------------------------------------------------------------------
// The traveler sprite: hat, red scarf, ochre coat, backpack, and a lantern.
// Each part is a small grid of characters (one character = one pixel), drawn
// facing right inside a 14-pixel-wide box and mirrored when facing left.
// ---------------------------------------------------------------------------

namespace {

const int BOX_WIDTH = 14;

const char* const HEAD[10] = {
    "...oooooo...",
    "..ohhhhhho..",
    "..oHHHHHHo..",
    "oohhhhhhhhoo",
    "ohhhhhhhhhho",
    ".okkSSSSSSo.",
    ".okssssssso.",
    ".okssssseso.",
    ".oSssssssSo.",
    "..oSssssSo..",
};

const char* const BODY[8] = {
    "...orrrrrro.",
    "oBBoccccRco.",
    "obboccccRco.",
    "obboccccccco",
    "obboCcccccco",
    "obbowwwwwwso",
    "obbocccccco.",
    ".oooCCCCCCo.",
};

const char* const LEGS_STAND[6] = {
    "...oppppppo.",
    "...oppooppo.",
    "...oppooppo.",
    "...oppooppo.",
    "...offoofffo",
    "...ooooooooo",
};

const char* const LEGS_STRIDE[6] = {
    "...oppppppo.",
    "..oppo..oppo",
    ".oppo...oppo",
    "oppo....oppo",
    "offo....offo",
    "oooo....oooo",
};

const char* const LEGS_STRIDE_LIFT[6] = { // the other step: the front boot lifts off the ground
    "...oppppppo.",
    "..oppo.oppo.",
    ".oppo..oppo.",
    "oppo...offo.",
    "offo...oooo.",
    "oooo........",
};

const char* const LEGS_JUMP[4] = { // legs tucked while rising
    "...oppppppo.",
    "..oppppppppo",
    "..offo..offo",
    "..oooo..oooo",
};

const char* const LEGS_FALL[5] = { // legs apart while falling
    "...oppppppo.",
    "..oppo..oppo",
    ".oppo...oppo",
    ".offo...offo",
    ".oooo...oooo",
};

const char* const LEGS_LAND[3] = { // knees bent for a moment after landing
    "..oppppppppo",
    ".offo...offo",
    ".oooo...oooo",
};

const Color OUTLINE = {22, 18, 30, 255};
const float SCARF_LINK = 1.35f; // canvas pixels between the scarf's points
const Color SCARF = {232, 72, 60, 255};
const Color SCARF_SHADE = {172, 44, 48, 255};
const Color LANTERN_FRAME = {96, 64, 44, 255};
const Color LANTERN_LIGHT = {255, 224, 140, 255};

Color spriteColor(char code) {
    switch (code) {
        case 'o': return OUTLINE;
        case 'h': return Color{58, 122, 106, 255};  // hat
        case 'H': return Color{150, 96, 60, 255};   // hat band
        case 'k': return Color{92, 58, 42, 255};    // hair
        case 's': return Color{252, 218, 184, 255}; // skin
        case 'S': return Color{214, 162, 128, 255}; // skin in shadow
        case 'e': return Color{24, 20, 34, 255};    // eye
        case 'r': return SCARF;
        case 'R': return SCARF_SHADE;
        case 'c': return Color{226, 160, 78, 255};  // coat
        case 'C': return Color{178, 110, 54, 255};  // coat in shadow
        case 'w': return Color{98, 64, 44, 255};    // belt
        case 'b': return Color{132, 90, 58, 255};   // backpack
        case 'B': return Color{216, 192, 142, 255}; // bedroll
        case 'p': return Color{72, 64, 102, 255};   // trousers
        case 'f': return Color{54, 38, 42, 255};    // boots
        default: return BLANK;
    }
}

// Where the sprite box sits, and how big one sprite pixel is.
struct SpriteBox {
    float left;
    float top;
    float pixel;
    bool facingRight;
};

// Draws one sprite pixel at (col, row) of the box, mirrored when facing left.
void plot(const SpriteBox& box, int col, int row, Color color) {
    int x = box.facingRight ? col : (BOX_WIDTH - 1 - col);
    DrawRectangle((int)(box.left + x * box.pixel), (int)(box.top + row * box.pixel), (int)box.pixel,
                  (int)box.pixel, color);
}

// Draws a part whose top-left corner is at (col, row) of the box.
void drawPart(const char* const* rows, int rowCount, int col, int row, const SpriteBox& box, bool blink) {
    for (int r = 0; r < rowCount; r++) {
        for (int c = 0; rows[r][c] != '\0'; c++) {
            char code = rows[r][c];
            if (code == '.') continue;
            if (blink && code == 'e') code = 's'; // eye closed
            plot(box, col + c, row + r, spriteColor(code));
        }
    }
}

// The static scarf tail, for drawings without motion (the portrait on the summary page).
void drawScarfTail(const SpriteBox& box, int neckRow, float droop) {
    int rows[5] = {0, 0, 0, 0, 0};
    for (int i = 1; i <= 4; i++) rows[i] = neckRow + (int)std::round(droop * i);
    for (int i = 1; i <= 4; i++) { // outline first, so it never covers the cloth
        int thickness = (i < 3) ? 2 : 1;
        plot(box, 3 - i, rows[i] - 1, OUTLINE);
        plot(box, 3 - i, rows[i] + thickness, OUTLINE);
    }
    for (int i = 1; i <= 4; i++) {
        plot(box, 3 - i, rows[i], SCARF);
        if (i < 3) plot(box, 3 - i, rows[i] + 1, SCARF_SHADE);
    }
}

// Where the scarf's tail wants to lie, relative to the knot at the neck: point i sits i links
// behind the traveler, `droop` lower per pixel back (negative lifts it), curling down at the tip.
Vector2 scarfRest(int i, float side, float droop) {
    float scale = SCARF_LINK / std::sqrt(1.0f + droop * droop);
    return Vector2{side * i * scale, droop * i * scale + i * i * 0.05f};
}

// The moving scarf tail: its simulated points plus a ripple that runs toward the tip, drawn as
// pixels from the neck outward, two pixels thick near the neck and one at the tip. The outline
// pass is drawn before the body and the cloth after it, so the tail lies over the backpack and
// is outlined only where it crosses the background.
void drawMovingScarf(const Player& player, Vector2 neck, float time, bool outline) {
    const TravelerMotion& m = player.motion;
    Vector2 points[SCARF_POINTS];
    for (int i = 0; i < SCARF_POINTS; i++) {
        float along = i / (float)(SCARF_POINTS - 1);
        float ripple = std::sin(time * 11.0f - i * 1.3f) * m.flutter * along;
        points[i] = Vector2{neck.x + m.scarf[i].x - m.scarf[0].x, neck.y + m.scarf[i].y - m.scarf[0].y + ripple};
    }
    for (int i = 1; i < SCARF_POINTS; i++) {
        Vector2 a = points[i - 1], b = points[i];
        int steps = 1 + (int)(std::fmax(std::fabs(b.x - a.x), std::fabs(b.y - a.y)) * 2.0f);
        bool thick = i < 3;
        for (int s = 0; s <= steps; s++) {
            float t = s / (float)steps;
            int x = (int)std::floor(a.x + (b.x - a.x) * t);
            int y = (int)std::floor(a.y + (b.y - a.y) * t);
            if (outline) {
                DrawRectangle(x - 1, y - 1, 3, thick ? 4 : 3, OUTLINE);
            } else {
                DrawRectangle(x, y, 1, 1, SCARF);
                if (thick) DrawRectangle(x, y + 1, 1, 1, SCARF_SHADE);
            }
        }
    }
}
} // namespace

void resetTravelerMotion(Player& player) {
    player.motion.ready = false;
}

void updateTravelerMotion(Player& player, float dt, float wind) {
    TravelerMotion& m = player.motion;
    const float side = (player.facing >= 0.0f) ? -1.0f : 1.0f; // the scarf is tied at the back of the neck
    // The neck in canvas pixels (world / 2), without the camera: only differences between points are drawn.
    Vector2 anchor = {(player.position.x + player.size.x * 0.5f) * 0.5f + side * 4.5f, (player.position.y + player.size.y) * 0.5f - 13.5f};

    // How the tail wants to lie: drooping when still, streaming out when walking or in a gust,
    // pressed down while rising and lifted while falling.
    float run = std::fmin(1.0f, std::fabs(player.velocity.x) / 260.0f);
    float gust = std::fmin(1.0f, std::fabs(wind) / 1.4f);
    float droop = 0.8f - 0.6f * run - 0.3f * gust;
    if (!player.grounded) droop = (player.velocity.y < 0.0f) ? 1.0f : -0.55f;

    float jump = std::fabs(anchor.x - m.scarf[0].x) + std::fabs(anchor.y - m.scarf[0].y);
    if (!m.ready || jump > 24.0f) { // the first frame, or the traveler was moved (recovery): settle at rest
        for (int i = 0; i < SCARF_POINTS; i++) {
            Vector2 rest = scarfRest(i, side, droop);
            m.scarf[i] = Vector2{anchor.x + rest.x, anchor.y + rest.y};
            m.scarfOld[i] = m.scarf[i];
        }
        m.flutter = 0.0f;
        m.lanternAngle = 0.0f;
        m.lanternSpin = 0.0f;
        m.lastSpeed = player.velocity.x;
        m.ready = true;
    }
    float flutterTarget = 0.2f + 0.6f * run + 0.7f * gust + (player.grounded ? 0.0f : 0.4f);
    m.flutter += (flutterTarget - m.flutter) * std::fmin(1.0f, dt * 4.0f);

    // Verlet steps: each point keeps its momentum (slowed by the air), springs toward its rest place,
    // and the links keep their length, so the tail lags behind a start, a stop, or a turn, then settles.
    const float step = 1.0f / 120.0f;
    int steps = std::max(1, std::min(8, (int)std::ceil(dt / step)));
    float h = dt / steps;
    float keep = std::exp(-6.0f * h);
    Vector2 from = m.scarf[0];
    for (int s = 0; s < steps; s++) {
        float k = (s + 1.0f) / steps;
        m.scarf[0] = Vector2{from.x + (anchor.x - from.x) * k, from.y + (anchor.y - from.y) * k};
        for (int i = 1; i < SCARF_POINTS; i++) {
            Vector2 rest = scarfRest(i, side, droop);
            float stiffness = 170.0f - i * 20.0f; // the tip is floppier than the knot
            Vector2 velocity = {(m.scarf[i].x - m.scarfOld[i].x) * keep, (m.scarf[i].y - m.scarfOld[i].y) * keep};
            m.scarfOld[i] = m.scarf[i];
            m.scarf[i].x += velocity.x + (m.scarf[0].x + rest.x - m.scarf[i].x) * stiffness * h * h;
            m.scarf[i].y += velocity.y + ((m.scarf[0].y + rest.y - m.scarf[i].y) * stiffness + 20.0f) * h * h;
        }
        for (int pass = 0; pass < 3; pass++) {
            for (int i = 1; i < SCARF_POINTS; i++) {
                float dx = m.scarf[i].x - m.scarf[i - 1].x, dy = m.scarf[i].y - m.scarf[i - 1].y;
                float length = std::sqrt(dx * dx + dy * dy);
                if (length < 0.0001f) continue;
                float push = (length - SCARF_LINK) / length;
                if (i == 1) { // the first link is tied to the neck
                    m.scarf[i].x -= dx * push;
                    m.scarf[i].y -= dy * push;
                } else {
                    m.scarf[i].x -= dx * push * 0.5f;
                    m.scarf[i].y -= dy * push * 0.5f;
                    m.scarf[i - 1].x += dx * push * 0.5f;
                    m.scarf[i - 1].y += dy * push * 0.5f;
                }
            }
        }
    }
    // The lantern is a small pendulum: starting and stopping make it swing, then it settles.
    float acceleration = (dt > 0.0f) ? (player.velocity.x - m.lastSpeed) / dt : 0.0f;
    m.lastSpeed = player.velocity.x;
    float push = -acceleration * 0.0009f + wind * 0.25f + (player.grounded ? 0.0f : -player.velocity.y * 0.0006f);
    m.lanternSpin += (-42.0f * m.lanternAngle - 4.5f * m.lanternSpin + push * 42.0f) * dt;
    m.lanternAngle = std::fmax(-0.8f, std::fmin(0.8f, m.lanternAngle + m.lanternSpin * dt));
}

float lanternSwing(const Player& player) {
    return player.motion.ready ? std::sin(player.motion.lanternAngle) * 2.4f : 0.0f;
}

void drawTraveler(const Player& player, float time, Vector2 feet, float pixel) {
    // Pick the pose from the physics state.
    const char* const* legs = LEGS_STAND;
    int legRows = 6;
    int bob = 0;          // moves the head and body up (-) or down (+)
    int lanternLift = 0;  // moves the lantern up (-) or down (+)
    float scarfDroop = 0.45f;

    if (!player.grounded) {
        bool rising = player.velocity.y < 0.0f;
        legs = rising ? LEGS_JUMP : LEGS_FALL;
        legRows = rising ? 4 : 5;
        lanternLift = rising ? -1 : -2;
        scarfDroop = rising ? 0.7f : -0.5f;
        if (player.velocity.y < -450.0f) bob = -1; // a little stretch right after take-off
    } else if (player.landTimer > 0.0f) {
        legs = LEGS_LAND;
        legRows = 3;
        lanternLift = 1;
    } else if (player.velocity.x != 0.0f) {
        int frame = (int)(player.walkPhase / 1.5708f) % 4;
        if (frame == 0) {
            legs = LEGS_STRIDE;
        } else if (frame == 2) {
            legs = LEGS_STRIDE_LIFT;
        } else {
            bob = -1; // passing pose: body one pixel higher
        }
        scarfDroop = -0.12f;
    } else if (std::fmod(time, 1.6f) >= 0.8f) {
        bob = 1; // idle breathing
    }

    const int height = 10 + 8 + legRows;
    SpriteBox box;
    box.pixel = pixel;
    box.facingRight = player.facing >= 0.0f;
    box.left = feet.x - 7.0f * pixel;
    box.top = feet.y - height * pixel;
    const int headTop = bob;
    const int bodyTop = 10 + bob;
    const int legsTop = 18;
    float flicker = 0.85f + 0.15f * std::sin(time * 13.0f) * std::sin(time * 7.0f);

    // The lantern swings on its handle (in the world it is a pendulum; see updateTravelerMotion).
    float swing = lanternSwing(player);
    int swingCols = (int)std::lround(box.facingRight ? swing : -swing);

    if (pixel > 1.0f) { // away from the scene's lighting (the summary portrait), the lantern glows on its own
        float glowX = box.left + ((box.facingRight ? 12 : 1) + 0.5f) * pixel;
        float glowY = box.top + (bodyTop + 8 + lanternLift + 0.5f) * pixel;
        DrawCircleGradient((int)glowX, (int)glowY, 15.0f * pixel, Fade(LANTERN_LIGHT, 0.24f * flicker), Fade(LANTERN_LIGHT, 0.0f));
    }
    if (player.grounded) { // a soft contact shadow on the ground under the feet
        DrawRectangle((int)(feet.x - 6.0f * pixel), (int)feet.y, (int)(12.0f * pixel), (int)pixel, Fade(BLACK, 0.35f));
    }

    const bool movingScarf = pixel == 1.0f && player.motion.ready;
    const Vector2 neck = {box.left + (box.facingRight ? 2 : BOX_WIDTH - 3) + 0.5f, box.top + bodyTop + 0.5f};
    if (movingScarf) {
        drawMovingScarf(player, neck, time, true);
    } else {
        drawScarfTail(box, bodyTop, scarfDroop);
    }
    drawPart(legs, legRows, 0, legsTop, box, false);
    drawPart(BODY, 8, 0, bodyTop, box, false);
    if (bob < 0) drawPart(BODY + 7, 1, 0, bodyTop + 8, box, false); // the coat's hem closes the gap above the legs
    if (movingScarf) drawMovingScarf(player, neck, time, false);
    bool blink = std::fmod(time + 0.37f, 3.4f) < 0.12f;
    drawPart(HEAD, 10, 1, headTop, box, blink);

    // Lantern hanging from the front hand: a ring, a frame, and a flickering light.
    int top = bodyTop + 6 + lanternLift;
    int left = 11 + swingCols;
    Color light = (flicker > 0.9f) ? LANTERN_LIGHT : Color{255, 196, 100, 255};
    plot(box, 12 + swingCols / 2, top, LANTERN_FRAME); // the handle stays near the hand
    for (int row = 1; row <= 3; row++) {
        plot(box, left, top + row, LANTERN_FRAME);
        plot(box, left + 1, top + row, (row < 3) ? light : LANTERN_FRAME);
        plot(box, left + 2, top + row, LANTERN_FRAME);
    }
}