#include <cstdio>

#include "raylib.h"
#include "Game.h"

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "The Node Wanderer - Tutorial Stage");
    SetExitKey(KEY_NULL); // Escape pauses the game instead of closing the window
    SetTargetFPS(60);

    {
        Game game; // builds the world: every platform node is allocated here

        while (!WindowShouldClose() && !game.shouldQuit()) {
            float dt = GetFrameTime();
            if (dt > 1.0f / 30.0f) {
                dt = 1.0f / 30.0f; // avoid falling through platforms after a stall
            }

            game.update(dt);

            BeginDrawing();
            game.draw();
            EndDrawing();
        }
    } // Game is destroyed here: each list deletes its own nodes

    std::printf("Nodes still allocated at shutdown: %d (expected 0)\n", g_liveNodeCount);
    CloseWindow();
    return g_liveNodeCount == 0 ? 0 : 1;
}
