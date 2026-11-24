#pragma once

#include "raylib.h"

// One frame of player input. The keyboard is read in one place (readKeyboard),
// so gameplay code works with plain values instead of calling Raylib directly.
struct InputFrame {
    float move = 0.0f;            // -1 left, +1 right, 0 none
    bool jump = false;            // pressed this frame
    bool interact = false;        // E or Enter pressed this frame
    bool toggleInspector = false; // Tab pressed this frame
    bool pause = false;           // Escape pressed this frame
    bool quit = false;            // Q pressed this frame
    bool replay = false;          // R pressed this frame
    bool mute = false;            // M pressed this frame: sound off or on
    int navX = 0;                 // pause menu: -1 left (A, Left), +1 right (D, Right), pressed or repeating
    int navY = 0;                 // pause menu: -1 up (W, Up), +1 down (S, Down)
};

inline InputFrame readKeyboard() {
    InputFrame input;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) input.move -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.move += 1.0f;
    input.jump = IsKeyPressed(KEY_SPACE);
    input.interact = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER);
    input.toggleInspector = IsKeyPressed(KEY_TAB);
    input.pause = IsKeyPressed(KEY_ESCAPE);
    input.quit = IsKeyPressed(KEY_Q);
    input.replay = IsKeyPressed(KEY_R);
    input.mute = IsKeyPressed(KEY_M);
    auto tapped = [](int key) { return IsKeyPressed(key) || IsKeyPressedRepeat(key); };
    if (tapped(KEY_A) || tapped(KEY_LEFT)) input.navX -= 1;
    if (tapped(KEY_D) || tapped(KEY_RIGHT)) input.navX += 1;
    if (tapped(KEY_W) || tapped(KEY_UP)) input.navY -= 1;
    if (tapped(KEY_S) || tapped(KEY_DOWN)) input.navY += 1;
    return input;
}
