#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "Art.h"
#include "Effects.h"
#include "Input.h"
#include "Player.h"
#include "World.h"

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

// Region 3 traversal stops when current == start, or after this many steps.
constexpr int MAX_LOOP_STEPS = 12;

// Timings in seconds (shared by the rules, the scene, and the interface).
constexpr float RECOVERY_FADE_SECONDS = 0.6f;
constexpr float TRANSITION_OUT_SECONDS = 0.6f;
constexpr float TRANSITION_IN_SECONDS = 0.9f;
constexpr float TITLE_CARD_SECONDS = 2.6f;
constexpr float PASSAGE_SECONDS = 1.1f;      // Region 3: travelling through the root
constexpr float BRIDGE_BUILD_SECONDS = 1.6f; // Region 4: the new bridge assembling

enum class GameState { Playing, Message, Paused, Complete };

// The mark drawn on a journal note, so each kind of message is recognizable by shape, not only color.
enum class NoteKind { Intro, Question, Answer, Recovery, Info };

// A short timer for a visual flash (checkpoint lantern). Visual only.
struct Pulse {
    Vector2 position;
    float timer;
    float duration;
};

class Game {
public:
    Game();
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    void update(const InputFrame& input, float dt);
    void draw() const;
    bool shouldQuit() const;

private:
    // Stage flow
    void resetStage();
    void enterRegion(int index);
    void startTransition(int target); // fade out, switch to `target` (REGION_COUNT = completion), fade in
    void completeRegionObjective(const std::string& question, const std::string& answer);

    // Gameplay
    void updatePlaying(const InputFrame& input, float dt);
    void resolveLandings(float previousBottom);
    void followLink(Node* destination);
    void startRootPassage(Node* destination);
    void finishRootPassage();
    void checkObjectives();
    void tryBuildBridge();
    void recoverFromFall(bool movedBackward);
    void recoverFromNullHazard();
    void restoreToCheckpoint();
    void setCheckpoint(Node* node);
    void markVisited(const Node* node);
    void updateLedgeMist(float dt); // dt <= 0 snaps instantly
    void updateCamera(float dt);    // dt <= 0 snaps the camera

    // Effects (visual only)
    Vector2 playerFeet() const;
    void spawnArrivalEffects(bool viaPrevious);
    Color dustColor() const;

    // Scene (GameScene.cpp)
    SceneView makeSceneView() const;
    void drawScene() const;
    void drawWispHint(const SceneView& view) const;

    // Interface (GameUi.cpp)
    void showMessage(std::vector<std::string> newPages, NoteKind kind);
    void showToast(const std::string& text, float seconds = 2.5f);
    const char* currentObjective() const;
    void drawHud() const;
    void drawProgressPips() const;
    void drawControlsStrip() const;
    void drawToast() const;
    void drawInteractionPrompt() const;
    void drawInspector() const;
    void drawMessageBox() const;
    void drawTitleCard() const;
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

    Node* checkpointNode = nullptr;     // NON-OWNING: where recovery puts the player
    Node* loopStartNode = nullptr;      // NON-OWNING: Region 3 stop condition target
    Node* passageDestination = nullptr; // NON-OWNING: tail->next while travelling through the root
    int loopSteps = 0;

    // Progress and visual state by node ID for the active region (no links, no positions).
    bool visitedIds[MAX_NODES_PER_REGION + 1] = {};
    float ledgeMist[MAX_NODES_PER_REGION + 1] = {}; // 1 = platform veiled because it is not solid now

    RenderTexture2D sceneCanvas = {}; // the pixel canvas (owned; unloaded in ~Game)
    ParticlePool particles;

    std::vector<std::string> pages; // current journal note pages
    std::vector<NoteKind> pageKinds;
    size_t pageIndex = 0;
    std::string toastText;
    float toastTimer = 0.0f;
    float fadeTimer = 0.0f;        // > 0 while fading in after a recovery or at the start
    float passageTimer = -1.0f;    // >= 0 while the Region 3 root passage plays
    float bridgeTimer = 0.0f;      // seconds since the Region 4 bridge node was appended
    int transitionTarget = -1;     // >= 0 while fading to another region
    float transitionTimer = 0.0f;
    bool transitionSwitched = false;
    float titleCardTimer = 0.0f;   // > 0 while the region's name is shown
    Pulse checkpointPulse = {};
    float elapsed = 0.0f;
};
