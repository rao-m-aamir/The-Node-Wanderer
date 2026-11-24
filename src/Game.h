#pragma once

#include <string>
#include <vector>

#include "raylib.h"
#include "Art.h"
#include "Audio.h"
#include "Effects.h"
#include "Input.h"
#include "Player.h"
#include "World.h"

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

// Region 3 traversal stops when current == start. This many steps is only a safety fallback,
// in case the start is never reached.
constexpr int MAX_LOOP_STEPS = 12;

// Timings in seconds (shared by the rules, the scene, and the interface).
constexpr float RECOVERY_FADE_SECONDS = 0.6f;
constexpr float TRANSITION_OUT_SECONDS = 0.6f;
constexpr float TRANSITION_HOLD_SECONDS = 1.3f; // a dark beat that names the place ahead
constexpr float TRANSITION_IN_SECONDS = 0.9f;
constexpr float TITLE_CARD_SECONDS = TRANSITION_HOLD_SECONDS + 1.8f;
constexpr float CALLOUT_SECONDS = 7.0f;
constexpr float PASSAGE_SECONDS = 1.8f;      // Region 3: carried back along the current
constexpr float BRIDGE_BUILD_SECONDS = 1.6f; // Region 4: the new bridge assembling
constexpr float GATE_OPEN_SECONDS = 1.2f;    // an exit opening once its objective is done

enum class GameState { Playing, Message, Paused, Complete };

// The mark drawn on a journal note, so each kind of message is recognizable by shape, not only color.
enum class NoteKind { Intro, Question, Answer, Recovery, Info };

// The first time each kind of link is used, a short journal callout names it (with one code line).
enum class Callout { FollowNext, FollowPrevious, LoopBack, NullLink, Append, Count };

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
    void checkRouteJumps() const;
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
    void setCheckpoint(Node* node, bool flashIfSame = false);
    void setStage(int newStage);
    void stopPlayerPose(); // a note is opening: settle the traveler into the idle pose
    void markVisited(const Node* node);
    void updateLedgeMist(float dt); // dt <= 0 snaps instantly
    void updateCamera(float dt);    // dt <= 0 snaps the camera

    // Effects (visual only)
    Vector2 playerFeet() const;
    void spawnArrivalEffects(bool viaPrevious);
    Color dustColor() const;
    float regionWind() const;

    // Sound (GameSound.cpp): which effect fits an event, and the mix for the current state.
    void startRegionAudio(int index);
    void updateAudioMix(float dt);
    void updatePauseAudioPanel(const InputFrame& input);
    void toggleMute();
    void playBridgeAssembly();
    Surface surfaceUnderfoot() const;
    float sideOf(float worldX) const; // -1 left of the screen .. 1 right, for panning

    // Scene (GameScene.cpp)
    SceneView makeSceneView() const;
    void drawScene() const;
    const Node* guideNode() const; // where the guiding light leads from the current node (may be null)
    void addTravelerLights(const SceneView& view, std::vector<Light>& lights) const;

    // Interface (GameUi.cpp)
    void showMessage(std::vector<std::string> newPages, NoteKind kind);
    void showToast(const std::string& text, float seconds = 4.0f);
    void showCallout(Callout which); // only the first time `which` happens on this journey
    void startJourney();             // a new game or a replay: hold on the first place's name, then fade in
    const char* currentObjective() const;
    void drawHud() const;
    void drawJournalHint() const;
    void drawControlsStrip() const;
    void drawToast() const;
    void drawCallout() const;
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

    SceneRenderer renderer; // the pixel canvas, light map, and final pass (owned; unloaded in ~Game)
    RegionArt regionArt;    // the active region's painted scenery and platforms (owned)
    ParticlePool particles;
    GameAudio audio;        // music, ambience, and effects (owned; unloaded in ~Game, before the device closes)

    std::vector<std::string> pages; // current journal note pages
    std::vector<NoteKind> pageKinds;
    size_t pageIndex = 0;
    std::string toastText;
    float toastTimer = 0.0f;
    float fadeTimer = 0.0f;        // > 0 while fading in after a recovery or at the start
    float passageTimer = -1.0f;    // >= 0 while the Region 3 root passage plays
    float bridgeTimer = 0.0f;      // seconds since the Region 4 bridge node was appended
    float gateTimer = 0.0f;        // seconds since the region's exit opened (animation only)
    float stageTimer = 0.0f;       // seconds since the objective step last changed (animation only)
    float nodeTimer = 0.0f;        // seconds since the player arrived on the current node (animation only)
    int transitionTarget = -1;     // >= 0 while fading to another region
    float transitionTimer = 0.0f;
    bool transitionSwitched = false;
    float titleCardTimer = 0.0f;   // > 0 while the region's name is shown
    bool calloutSeen[(int)Callout::Count] = {};
    Callout calloutShown = Callout::FollowNext;
    float calloutTimer = 0.0f;     // > 0 while a callout is shown
    bool journeyStarted = false;   // the first link has been followed: the controls strip fades out
    float journeyTimer = 0.0f;     // seconds since then
    Pulse checkpointPulse = {};
    float elapsed = 0.0f;

    // Sound state (no gameplay reads these)
    bool fallSoundPlayed = false;  // the rush of air plays once per long fall
    bool notePending = false;      // a note opened; its paper sound waits until it can be seen
    int audioRow = 0;              // the pause screen's sound row: 0 music, 1 sounds, 2 mute
    float audioRowFlash = 0.0f;    // > 0 just after a value there changed
};
