#include <cstdio>

#include "raylib.h"
#include "Game.h"
#include "Input.h"

int main() {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT);
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "The Node Wanderer - Tutorial Stage");
    SetExitKey(KEY_NULL); // Escape pauses the game instead of closing the window
    SetTargetFPS(60);
    InitAudioDevice(); // after the window, before the game loads any sound
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "AUDIO: the audio device could not be opened; the game will be silent");
    }

    {
        Game game; // builds the world (every platform node is allocated here) and loads the sound

        while (!WindowShouldClose() && !game.shouldQuit()) {
            float dt = GetFrameTime();
            if (dt > 1.0f / 30.0f) {
                dt = 1.0f / 30.0f; // avoid falling through platforms after a stall
            }

            game.update(readKeyboard(), dt);

            BeginDrawing();
            game.draw();
            EndDrawing();
        }
    } // Game is destroyed here: each list deletes its own nodes, and every sound is unloaded

    std::printf("Nodes still allocated at shutdown: %d (expected 0)\n", g_liveNodeCount);
    if (IsAudioDeviceReady()) CloseAudioDevice(); // after every sound and stream is unloaded
    CloseWindow();
    return g_liveNodeCount == 0 ? 0 : 1;
}
