#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "ForestArt.h"
#include "Player.h"
#include "World.h"

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

// Region 3 traversal stops when current == start, or after this many steps.
constexpr int MAX_LOOP_STEPS = 12;

enum class GameState { Playing, Message, Paused, Complete };

// A short expanding ring: node arrival or checkpoint activation. Visual only.
struct Pulse {
    Vector2 position;
    float timer;
    float duration;
    Color color;
};

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void update(float dt);
    void draw() const;
    bool shouldQuit() const;

private:
    // Stage flow
    void resetStage();
    void enterRegion(int index);
    void completeRegionObjective(std::vector<std::string> checkpointPages);

    // Gameplay
    void updatePlaying(float dt);
    void resolveLandings(float previousBottom);
    void followLink(Node* destination);
    void checkObjectives();
    void tryBuildBridge();
    void recoverFromFall(bool movedBackward);
    void recoverFromNullHazard();
    void restoreToCheckpoint();
    void setCheckpoint(Node* node);
    void markVisited(const Node* node);
    void updateLedgeMist(float dt); // dt <= 0 snaps instantly
    void updateCamera(float dt);    // dt <= 0 snaps the camera

    // Scenes (GameScene.cpp): which renderer draws the active region.
    void drawForestScene() const;  // Region 1: natural forest art in a 2x pixel canvas
    void drawClassicScene() const; // Regions 2-4: the earlier diagram-style look
    void drawWispHint(int cameraX, Vector2 feet) const;
    void drawProgressPill() const;

    // UI
    void showMessage(std::vector<std::string> newPages);
    void showToast(const std::string& text, float seconds = 2.5f);
    const char* currentObjective() const;
    void drawPulse(const Pulse& pulse) const;
    void drawInteractionPrompt() const;
    void drawHud() const;
    void drawControlsStrip() const;
    void drawToast() const;
    void drawInspector() const;
    void drawMessageBox() const;
    void drawPauseScreen() const;
    void drawCompleteScreen() const;

    // `world` is declared first so it is destroyed last,
    // after everything that holds non-owning Node pointers.
    World world;
    Player player = {};
    PhysicsSettings physics;
    Camera2D camera = {};

    GameState state = GameState::Message;
    GameState stateBeforePause = GameState::Playing;

    int regionIndex = 0;
    int stage = 0;           // objective step inside the current region
    bool gateOpen = false;
    bool inspectorOpen = false;
    bool quitRequested = false;

    Node* checkpointNode = nullptr; // NON-OWNING: where recovery puts the player
    Node* loopStartNode = nullptr;  // NON-OWNING: Region 3 stop condition target
    int loopSteps = 0;

    // Progress state by node ID for the active region (not a second graph: no links, no positions).
    bool visitedIds[MAX_NODES_PER_REGION + 1] = {};
    float ledgeMist[MAX_NODES_PER_REGION + 1] = {}; // visual only: 1 = ledge veiled because it is not solid

    RenderTexture2D forestCanvas = {}; // Region 1 pixel canvas (owned; unloaded in ~Game)

    std::vector<std::string> pages; // current message card pages
    size_t pageIndex = 0;
    std::string toastText;
    float toastTimer = 0.0f;
    float fadeTimer = 0.0f;  // > 0 while fading in after a recovery
    Pulse arrivalPulse = {};
    Pulse checkpointPulse = {};
    float elapsed = 0.0f;
};
