#include "Player.h"

#include <cmath>

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

void updatePlayerMovement(Player& player, const PhysicsSettings& settings, float dt) {
    float direction = 0.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) direction -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) direction += 1.0f;

    player.velocity.x = direction * settings.moveSpeed;
    if (direction != 0.0f) {
        player.facing = direction;
    }

    // Jump only from the ground: no second jump while airborne.
    if (player.grounded && IsKeyPressed(KEY_SPACE)) {
        player.velocity.y = -settings.jumpImpulse;
        player.grounded = false;
    }

    player.velocity.y += settings.gravity * dt;
    if (player.velocity.y > settings.maxFallSpeed) {
        player.velocity.y = settings.maxFallSpeed;
    }

    player.position.x += player.velocity.x * dt;
    player.position.y += player.velocity.y * dt;

    // Animation timers.
    if (player.grounded && direction != 0.0f) {
        player.walkPhase += dt * 14.0f;
    }
    if (player.landTimer > 0.0f) {
        player.landTimer -= dt;
    }
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

// Scarf tail streaming from the back of the neck. droop > 0 hangs down, < 0 lifts up.
void drawScarfTail(const SpriteBox& box, int neckRow, float droop, float time) {
    int rows[5] = {0, 0, 0, 0, 0};
    for (int i = 1; i <= 4; i++) {
        float wave = std::sin(time * 9.0f + i * 1.3f) * 0.6f * (i / 4.0f);
        rows[i] = neckRow + (int)std::round(droop * i + wave);
    }
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

} // namespace

void drawTraveler(const Player& player, float time, Vector2 feet, float pixel) {
    // Pick the pose from the physics state.
    const char* const* legs = LEGS_STAND;
    int legRows = 6;
    int bob = 0;          // moves the head and body up (-) or down (+)
    int lanternLift = 0;  // moves the lantern up (-) or down (+)
    int lanternSwing = 0; // moves the lantern forward
    float scarfDroop = 0.45f;

    if (!player.grounded) {
        bool rising = player.velocity.y < 0.0f;
        legs = rising ? LEGS_JUMP : LEGS_FALL;
        legRows = rising ? 4 : 5;
        lanternLift = rising ? -1 : -2;
        scarfDroop = rising ? 0.7f : -0.5f;
    } else if (player.landTimer > 0.0f) {
        legs = LEGS_LAND;
        legRows = 3;
        lanternLift = 1;
    } else if (player.velocity.x != 0.0f) {
        int frame = (int)(player.walkPhase / 1.5708f) % 4;
        if (frame == 0 || frame == 2) {
            legs = LEGS_STRIDE;
        } else {
            bob = -1; // passing pose: body one pixel higher
        }
        lanternSwing = (frame == 0) ? 1 : 0;
        scarfDroop = 0.0f; // streams straight back while walking
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

    // Warm lantern light behind the character.
    int glassCol = 12 + lanternSwing;
    int glassRow = bodyTop + 8 + lanternLift;
    float glowX = box.left + ((box.facingRight ? glassCol : BOX_WIDTH - 1 - glassCol) + 0.5f) * pixel;
    float glowY = box.top + (glassRow + 0.5f) * pixel;
    float flicker = 0.85f + 0.15f * std::sin(time * 13.0f) * std::sin(time * 7.0f);
    DrawCircleGradient((int)glowX, (int)glowY, 15.0f * pixel, Fade(LANTERN_LIGHT, 0.24f * flicker),
                       Fade(LANTERN_LIGHT, 0.0f));

    drawScarfTail(box, bodyTop, scarfDroop, time);
    drawPart(legs, legRows, 0, legsTop, box, false);
    drawPart(BODY, 8, 0, bodyTop, box, false);
    bool blink = std::fmod(time + 0.37f, 3.4f) < 0.12f;
    drawPart(HEAD, 10, 1, headTop, box, blink);

    // Lantern hanging from the front hand: a ring, a frame, and a flickering light.
    int top = bodyTop + 6 + lanternLift;
    int left = 11 + lanternSwing;
    Color light = (flicker > 0.9f) ? LANTERN_LIGHT : Color{255, 196, 100, 255};
    plot(box, left + 1, top, LANTERN_FRAME);
    for (int row = 1; row <= 3; row++) {
        plot(box, left, top + row, LANTERN_FRAME);
        plot(box, left + 1, top + row, (row < 3) ? light : LANTERN_FRAME);
        plot(box, left + 2, top + row, LANTERN_FRAME);
    }
}
