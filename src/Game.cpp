#include "Game.h"

#include <cassert>
#include <cmath>

#include "ArtCommon.h" // shared light colors for effects
#include "Theme.h"

// Game rules and flow. Drawing lives in GameScene.cpp (world) and GameUi.cpp (interface).

// ---------------------------------------------------------------------------
// Setup and teardown
// ---------------------------------------------------------------------------

Game::Game() {
    camera.zoom = 1.0f;
    player.size = Vector2{32.0f, 48.0f};
    player.facing = 1.0f;

    // The whole world is drawn into this small canvas and scaled up 2x (pixel art).
    sceneCanvas = LoadRenderTexture(CANVAS_WIDTH, CANVAS_HEIGHT);

    world.build();
    enterRegion(0);
    fadeTimer = RECOVERY_FADE_SECONDS;
    titleCardTimer = TITLE_CARD_SECONDS;
}

Game::~Game() {
    // Drop every non-owning pointer before `world` (the owner) deletes the nodes.
    player.currentNode = nullptr;
    checkpointNode = nullptr;
    loopStartNode = nullptr;
    passageDestination = nullptr;

    UnloadRenderTexture(sceneCanvas);
}

bool Game::shouldQuit() const {
    return quitRequested;
}

void Game::resetStage() {
    // Non-owning pointers first, then the owners free and rebuild their nodes.
    player.currentNode = nullptr;
    checkpointNode = nullptr;
    loopStartNode = nullptr;
    passageDestination = nullptr;
    passageTimer = -1.0f;
    bridgeTimer = 0.0f;
    clearParticles(particles);

    // Leftovers from the summary screen: a fade still running, an old toast, a lantern flash.
    transitionTarget = -1;
    transitionTimer = 0.0f;
    transitionSwitched = false;
    toastText.clear();
    toastTimer = 0.0f;
    checkpointPulse = {};

    world.clearAll();
    world.build();

    inspectorOpen = false;
    enterRegion(0);
    fadeTimer = RECOVERY_FADE_SECONDS;
    titleCardTimer = TITLE_CARD_SECONDS;
}

void Game::enterRegion(int index) {
    regionIndex = index;
    stage = 0;
    stageTimer = 0.0f;
    gateOpen = false;
    gateTimer = 0.0f;
    nodeTimer = 0.0f;
    loopSteps = 0;
    for (bool& visited : visitedIds) {
        visited = false;
    }

    // Regions are separate lists. The player starts at the new list's head.
    Node* head = world.getHead(index);
    assert(head != nullptr && "Every region must have a head node");
    player.currentNode = head;
    checkpointNode = head;
    loopStartNode = (index == 2) ? head : nullptr; // Region 3 stop condition target
    markVisited(head);

    placePlayerOnPlatform(player, *head);
    player.facing = 1.0f;
    player.landTimer = 0.0f;
    updateCamera(0.0f);
    updateLedgeMist(0.0f); // start with the right platforms veiled, no fade-in

    switch (index) {
        case 0:
            showMessage({
                "Welcome, wanderer. Every ledge in this forest is a real Node,\n"
                "created with new and owned by a singly linked list.",
                "Each node stores one pointer: next. Only the ledge it points to is solid,\n"
                "the rest fade into mist. Follow next from the head N1 to the tail N5.",
            }, NoteKind::Intro);
            break;
        case 1:
            showMessage({
                "The Forgotten Return. These ruins form a doubly linked list:\n"
                "each node stores next and previous.",
                "Warm light rises where next points. Cool light falls where previous points.\n"
                "head->previous and tail->next are both NULL.",
                "Goal: take the key from N4 (Key Shrine), carry its light back to\n"
                "N2 (Old Hall) through previous, then go on to the tail N5.",
            }, NoteKind::Intro);
            break;
        case 2:
            showMessage({
                "The Endless Garden. This list is circular:\n"
                "the tail's next points back to the head, so no next is NULL.",
                "The hollow in the tail's root is the link tail->next.\n"
                "Walk into it, and the great root carries you back to the head.",
                "A loop over a circular list needs a stop condition, or it never ends.\n"
                "Goal: follow next all the way around until current == start (N1).",
            }, NoteKind::Intro);
            break;
        case 3:
            showMessage({
                "The Edge of Nothing. nullptr means \"no node\".\n"
                "Following it is a bug: in C++ it is undefined behavior.",
                "Always check before you follow:\n"
                "    if (current->next != nullptr) { current = current->next; }",
                "Goal: reach N3 (Edge), inspect its next link, and repair it safely.\n"
                "Stay out of the amber gap.",
            }, NoteKind::Intro);
            break;
        default:
            break;
    }
}

// Fade to black, switch region (or show the summary), then fade back in with the region's name.
void Game::startTransition(int target) {
    transitionTarget = target;
    transitionTimer = 0.0f;
    transitionSwitched = false;
    player.velocity = Vector2{0.0f, 0.0f};
}

void Game::completeRegionObjective(const std::string& question, const std::string& answer) {
    gateOpen = true;
    gateTimer = 0.0f;
    setCheckpoint(player.currentNode, true); // the lantern flashes even when it is already here (Region 3: N1)
    stopPlayerPose();
    pages = {question, answer};
    pageKinds = {NoteKind::Question, NoteKind::Answer};
    pageIndex = 0;
    state = GameState::Message;
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::update(const InputFrame& input, float dt) {
    // Paused: the world and all of its clocks stand still until Esc resumes.
    if (state == GameState::Paused) {
        if (input.pause) state = stateBeforePause;
        if (input.quit) quitRequested = true;
        return;
    }

    elapsed += dt;
    if (toastTimer > 0.0f) toastTimer -= dt;
    if (fadeTimer > 0.0f) fadeTimer -= dt;
    if (titleCardTimer > 0.0f) titleCardTimer -= dt;
    if (checkpointPulse.timer > 0.0f) checkpointPulse.timer -= dt;
    if (world.isBridgeBuilt()) bridgeTimer += dt;
    if (gateOpen) gateTimer += dt;
    stageTimer += dt;
    nodeTimer += dt;
    updateParticles(particles, dt);
    updateLedgeMist(dt);

    // Moving between regions: input waits until the new region has been entered.
    if (transitionTarget >= 0) {
        transitionTimer += dt;
        if (!transitionSwitched && transitionTimer >= TRANSITION_OUT_SECONDS) {
            transitionSwitched = true;
            clearParticles(particles);
            if (transitionTarget < REGION_COUNT) {
                enterRegion(transitionTarget);
                titleCardTimer = TITLE_CARD_SECONDS;
                showToast(TextFormat("Region %d is a separate list.  current = nextList.getHead();", regionIndex + 1), 4.0f);
            } else {
                state = GameState::Complete;
            }
            return; // a key pressed on this frame belonged to the old screen, not the new note or summary
        }
        if (transitionTimer >= TRANSITION_OUT_SECONDS + TRANSITION_IN_SECONDS) {
            transitionTarget = -1;
        }
        if (!transitionSwitched) return;
    }

    if (state != GameState::Complete && input.toggleInspector) {
        inspectorOpen = !inspectorOpen;
    }

    switch (state) {
        case GameState::Paused:
            break; // handled at the top

        case GameState::Complete:
            if (input.replay) resetStage();
            if (input.pause || input.quit) quitRequested = true;
            break;

        case GameState::Message:
            if (input.pause) {
                stateBeforePause = GameState::Message;
                state = GameState::Paused;
            } else if (input.interact) {
                pageIndex++;
                if (pageIndex >= pages.size()) {
                    pages.clear();
                    pageKinds.clear();
                    pageIndex = 0;
                    state = GameState::Playing;
                }
            }
            break;

        case GameState::Playing:
            if (input.pause) {
                stateBeforePause = GameState::Playing;
                state = GameState::Paused;
            } else {
                if (input.interact) tryBuildBridge();
                if (state == GameState::Playing) updatePlaying(input, dt); // the bridge may have opened a note
            }
            break;
    }
}

void Game::updatePlaying(const InputFrame& input, float dt) {
    // Region 3: while the root carries the traveler back to the head, nothing else moves.
    if (passageTimer >= 0.0f) {
        passageTimer += dt;
        if (passageTimer >= PASSAGE_SECONDS) finishRootPassage();
        return;
    }
    if (transitionTarget >= 0) {
        return; // walking through an exit: wait for the fade
    }

    float previousBottom = player.position.y + player.size.y;
    bool wasGrounded = player.grounded;
    bool jumping = player.grounded && input.jump;

    updatePlayerMovement(player, physics, input.move, input.jump, dt);
    if (jumping) spawnDust(particles, playerFeet(), 4, dustColor());

    // Keep the player inside the world horizontally.
    if (player.position.x < 0.0f) player.position.x = 0.0f;
    if (player.position.x > WORLD_WIDTH - player.size.x) player.position.x = WORLD_WIDTH - player.size.x;

    resolveLandings(previousBottom);
    if (!wasGrounded && player.grounded) {
        player.landTimer = LAND_POSE_SECONDS;
        spawnDust(particles, playerFeet(), 7, dustColor());
    }
    if (state != GameState::Playing) {
        updateCamera(dt);
        return; // a landing finished an objective and opened a note
    }

    Rectangle playerRect = getPlayerRect(player);

    // Region 3: the hollow on the tail is the link tail->next.
    if (regionIndex == 2) {
        Node* tail = world.getLoopTail();
        if (tail != nullptr && player.currentNode == tail && CheckCollisionRecs(playerRect, world.getPortalRect())) {
            Node* destination = tail->next; // read the real link (it is the head)
            if (destination != nullptr) {
                startRootPassage(destination);
                return;
            }
        }
    }

    // Region 4: stepping into the gap where N3->next is NULL.
    if (regionIndex == 3 && !world.isBridgeBuilt() && CheckCollisionRecs(playerRect, world.getNullHazardZone())) {
        recoverFromNullHazard();
        return;
    }

    // Ordinary fall between nodes.
    if (player.position.y > WORLD_KILL_Y) {
        bool movedBackward = player.currentNode != nullptr &&
                             player.position.x + player.size.x < player.currentNode->platformPosition.x;
        recoverFromFall(movedBackward);
        return;
    }

    // Exit: a hand-off to the next region's list (or the summary), not a pointer.
    Rectangle gate;
    if (gateOpen && world.getGateRect(regionIndex, gate) && CheckCollisionRecs(playerRect, gate)) {
        startTransition(regionIndex + 1); // REGION_COUNT means the stage is complete
        spawnBurst(particles, playerFeet(), 14, WARM_LIGHT, WHITE, 70.0f, 20.0f, 0.9f);
        return;
    }

    updateCamera(dt);
}

// One-way platforms: the player lands only when falling onto a platform's top.
// Only platforms allowed by Traversal Rule A take part in collision.
void Game::resolveLandings(float previousBottom) {
    player.grounded = false;
    if (player.velocity.y < 0.0f) {
        return; // moving up: pass through platforms from below
    }

    ListKind kind = world.getRegion(regionIndex).kind;
    Rectangle playerRect = getPlayerRect(player);
    float bottom = playerRect.y + playerRect.height;

    Node* head = world.getHead(regionIndex);
    Node* landedOn = nullptr;
    Node* node = head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        if (isPlatformSolid(player.currentNode, node, kind)) {
            Rectangle platform = getPlatformRect(*node);
            bool overlapsX = playerRect.x + playerRect.width > platform.x && playerRect.x < platform.x + platform.width;
            bool crossedTop = previousBottom <= platform.y + 1.0f && bottom >= platform.y;
            if (overlapsX && crossedTop) {
                // Standing across two platforms: stay on the current one.
                if (landedOn == nullptr || node == player.currentNode) {
                    landedOn = node;
                }
            }
        }
        node = node->next;
        steps++;
        if (node == head) break; // circular list: back at the start
    }

    if (landedOn != nullptr) {
        player.position.y = landedOn->platformPosition.y - player.size.y;
        player.velocity.y = 0.0f;
        player.grounded = true;
        if (landedOn != player.currentNode) {
            followLink(landedOn);
        }
    }
}

// Moves player.currentNode along a link. The destination must be exactly
// current->next, or current->previous in a doubly linked list.
void Game::followLink(Node* destination) {
    Node* from = player.currentNode;
    if (from == nullptr || destination == nullptr) {
        return; // never follow NULL, never move from "no node"
    }

    const char* code = nullptr;
    bool viaPrevious = false;
    if (from->next == destination) {
        code = "current = current->next;";
    } else if (world.getRegion(regionIndex).kind == ListKind::Doubly && from->previous == destination) {
        code = "current = current->previous;";
        viaPrevious = true;
    } else {
        return; // not reachable through a permitted link: do not move
    }

    player.currentNode = destination;
    nodeTimer = 0.0f;
    markVisited(destination);
    spawnArrivalEffects(viaPrevious);

    if (regionIndex == 2 && destination == loopStartNode) {
        showToast(TextFormat("%s   N%d -> N%d  (tail->next is the head)", code, from->id, destination->id));
    } else {
        showToast(TextFormat("%s   N%d -> N%d", code, from->id, destination->id));
    }

    if (regionIndex == 2 && !gateOpen) {
        loopSteps++; // only the first trip around counts; the stop condition already fired after that
    }
    checkObjectives();
}

// Region 3: entering the tail's hollow. The destination was read from tail->next by the caller.
void Game::startRootPassage(Node* destination) {
    passageDestination = destination;
    passageTimer = 0.0f;
    player.velocity = Vector2{0.0f, 0.0f};
    spawnBurst(particles, Vector2{player.position.x + player.size.x / 2.0f, player.position.y + 20.0f}, 14,
               Color{255, 214, 236, 255}, Color{120, 255, 230, 255}, 80.0f, 40.0f, 0.8f);
}

// The root sets the traveler down on the node tail->next points to; that is the loop step.
void Game::finishRootPassage() {
    Node* destination = passageDestination;
    passageDestination = nullptr;
    passageTimer = -1.0f;
    if (destination == nullptr) {
        return;
    }
    placePlayerOnPlatform(player, *destination);
    followLink(destination); // current = current->next; (tail->next is the head)
}

void Game::checkObjectives() {
    Node* current = player.currentNode;
    if (current == nullptr || gateOpen) {
        return;
    }

    switch (regionIndex) {
        case 0:
            if (current->next == nullptr) { // reached the tail
                completeRegionObjective(
                    "Checkpoint question: Which link tells you where to go next?",
                    "Answer: next. Each node stores the address of the following node.\n"
                    "The tail's next is NULL, so the list ends at N5. The exit is open.");
            }
            break;

        case 1:
            if (stage == 0 && current->id == 4) {
                setStage(1); // the key's light now travels with the player
                setCheckpoint(current);
                spawnBurst(particles, Vector2{current->platformPosition.x + current->platformWidth / 2.0f,
                                              current->platformPosition.y - 44.0f},
                           16, Color{214, 176, 96, 255}, COOL_LIGHT, 90.0f, 30.0f, 0.9f);
                showToast("You took the key's light. Carry it back to N2 through previous.", 4.0f);
            } else if (stage == 1 && current->id == 2) {
                setStage(2); // the light kindles the Old Hall's brazier
                setCheckpoint(current);
                spawnBurst(particles, Vector2{current->platformPosition.x + current->platformWidth - 20.0f,
                                              current->platformPosition.y - 18.0f},
                           18, Color{255, 168, 76, 255}, Color{255, 232, 150, 255}, 80.0f, -20.0f, 1.0f);
                showToast("The brazier is lit. Now continue forward through next to the tail.", 4.0f);
            } else if (stage == 2 && current->next == nullptr) {
                completeRegionObjective(
                    "Checkpoint question: What does a doubly linked list store that a\n"
                    "singly linked list does not?",
                    "Answer: a previous link. That is how you carried the light back to N2.\n"
                    "head->previous and tail->next are both NULL. The exit is open.");
            }
            break;

        case 2:
            // Stop condition: we are back at the node we started from.
            if (current == loopStartNode && loopSteps > 0) {
                completeRegionObjective(
                    "Checkpoint question: In a circular list no next is NULL.\n"
                    "So when does a traversal stop?",
                    TextFormat("Answer: when current == start, or after a maximum number of steps.\n"
                               "Without a stop condition, the loop never ends. You stopped after %d steps.\n"
                               "The moon gate on N1 is open.", loopSteps));
            } else if (loopSteps >= MAX_LOOP_STEPS) {
                // Safety stop: a bounded step count also ends the traversal.
                loopSteps = 0;
                checkpointNode = loopStartNode;
                restoreToCheckpoint();
                showMessage({TextFormat("Safety limit: %d steps without reaching the start.\n"
                                        "The traversal was stopped and restarted at N1.", MAX_LOOP_STEPS)},
                            NoteKind::Recovery);
            }
            break;

        case 3:
            if (stage == 0 && current->id == 3) {
                setStage(1);
                setCheckpoint(current);
                showToast("You are on N3 (Edge). Open the journal (Tab) and read N3->next.", 4.0f);
            } else if (stage == 2 && current->id == 4) {
                completeRegionObjective(
                    "Checkpoint question: What must you do before following a pointer?",
                    "Answer: check it.   if (current->next != nullptr) { current = current->next; }\n"
                    "nullptr means there is no node: nothing to read, nothing to stand on.\n"
                    "The dawn gate is open.");
            }
            break;

        default:
            break;
    }
}

// Region 4 interaction: the safe check, then append() repairs the missing link.
void Game::tryBuildBridge() {
    if (regionIndex != 3 || stage != 1) {
        return;
    }
    Node* current = player.currentNode;
    if (current == nullptr || current->id != 3 || !player.grounded) {
        showToast("Stand on N3 (Edge) to repair its link."); // not while jumping or falling past the edge
        return;
    }
    if (!inspectorOpen) {
        showToast("Check first: open the journal (Tab) and read N3->next.");
        return;
    }

    // Read the pointer before following it.
    if (current->next == nullptr) {
        Node* bridge = world.appendBridgeNode(); // append() allocates N4 and sets N3->next = N4
        setStage(2);
        bridgeTimer = 0.0f;
        if (bridge != nullptr) {
            spawnBurst(particles, Vector2{bridge->platformPosition.x + 20.0f, bridge->platformPosition.y}, 16,
                       WARM_LIGHT, WHITE, 80.0f, 60.0f, 1.0f);
        }
        showMessage({
            "Check: N3->next == nullptr, so there was nothing to follow.",
            "You called append(). It created a new Node with new and set N3->next to it.\n"
            "The link is valid now, so the bridge N4 is solid. Cross it to the dawn gate.",
        }, NoteKind::Info);
    }
}

void Game::recoverFromFall(bool movedBackward) {
    int fromId = (player.currentNode != nullptr) ? player.currentNode->id : 0;

    // Region 3: a fall during the first trip around restarts the traversal from the head.
    // Once the stop condition has fired, the loop is done and a fall is an ordinary one.
    bool restartLoop = regionIndex == 2 && !gateOpen;
    if (restartLoop) {
        loopSteps = 0;
        checkpointNode = loopStartNode;
        for (bool& visited : visitedIds) {
            visited = false; // the walk starts again, so its marks do too
        }
    }
    restoreToCheckpoint();
    int checkpointId = (player.currentNode != nullptr) ? player.currentNode->id : 0;
    markVisited(player.currentNode);

    std::vector<std::string> recoveryPages;
    if (regionIndex == 0 && movedBackward) {
        recoveryPages.push_back(TextFormat(
            "No previous link! N%d only stores next.\n"
            "A singly linked list cannot step backward, so the ledge behind you was not solid.", fromId));
    } else if (regionIndex == 2 && movedBackward) {
        recoveryPages.push_back(
            "A circular singly linked list also stores only next.\n"
            "To reach an earlier node, keep going forward around the loop.");
    } else {
        recoveryPages.push_back(
            "You fell between nodes.\n"
            "Only your current node and the nodes its links point to are solid.");
    }

    if (restartLoop) {
        recoveryPages.push_back(TextFormat("Recovery: back at the start N%d. Loop steps reset to 0.", checkpointId));
    } else {
        recoveryPages.push_back(TextFormat("Recovery: you are back at the lantern on N%d.", checkpointId));
    }
    showMessage(recoveryPages, NoteKind::Recovery);
}

void Game::recoverFromNullHazard() {
    restoreToCheckpoint();
    showMessage({
        "NULL hazard! You stepped past N3, but N3->next == nullptr.\n"
        "There is no node there: nothing to stand on, nothing to read.",
        "In C++, dereferencing nullptr is undefined behavior (usually a crash).\n"
        "The game checks first:   if (current->next == nullptr) { /* do not follow */ }",
        "Recovery: you are back on N3.\n"
        "Open the journal (Tab), then press E to repair the missing link.",
    }, NoteKind::Recovery);
}

void Game::restoreToCheckpoint() {
    // Recovery must always resume on a real node, never on nullptr.
    if (checkpointNode == nullptr) {
        checkpointNode = world.getHead(regionIndex);
    }
    player.currentNode = checkpointNode;
    if (player.currentNode != nullptr) {
        placePlayerOnPlatform(player, *player.currentNode);
    }
    player.landTimer = 0.0f;
    fadeTimer = RECOVERY_FADE_SECONDS; // brief fade back in at the checkpoint
    updateCamera(0.0f);
    clearParticles(particles);
    spawnGather(particles, Vector2{player.position.x + player.size.x / 2.0f, player.position.y + 24.0f}, 16, WARM_LIGHT);
}

void Game::setCheckpoint(Node* node, bool flashIfSame) {
    if (node == nullptr || (node == checkpointNode && !flashIfSame)) {
        return;
    }
    checkpointNode = node;
    checkpointPulse = Pulse{Vector2{node->platformPosition.x, node->platformPosition.y}, 0.8f, 0.8f};
    spawnBurst(particles, Vector2{node->platformPosition.x + 22.0f, node->platformPosition.y - 26.0f}, 12, WARM_LIGHT,
               CHECKPOINT_CREAM, 60.0f, -10.0f, 0.9f);
}

void Game::setStage(int newStage) {
    stage = newStage;
    stageTimer = 0.0f; // lets the art ease in what the new step changes (brazier, relic, repair)
}

void Game::stopPlayerPose() {
    // Movement only updates in play. Without this the walk or landing pose would stay frozen under the note.
    player.velocity.x = 0.0f;
    player.landTimer = 0.0f;
}

void Game::markVisited(const Node* node) {
    if (node != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION) {
        visitedIds[node->id] = true;
    }
}

// Visual only: platforms that are not solid right now (Traversal Rule A) fade into mist.
// The target is read from the real links of the active list; dt <= 0 snaps instantly.
void Game::updateLedgeMist(float dt) {
    ListKind kind = world.getRegion(regionIndex).kind;
    float follow = (dt <= 0.0f) ? 1.0f : std::fmin(1.0f, dt * 3.0f);

    Node* head = world.getHead(regionIndex);
    Node* node = head;
    int steps = 0;
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        if (node->id >= 0 && node->id <= MAX_NODES_PER_REGION) {
            float target = isPlatformSolid(player.currentNode, node, kind) ? 0.0f : 1.0f;
            ledgeMist[node->id] += (target - ledgeMist[node->id]) * follow;
        }
        node = node->next;
        steps++;
        if (node == head) break; // circular list: back at the start
    }
}

void Game::updateCamera(float dt) {
    float desired = player.position.x + player.size.x / 2.0f - SCREEN_WIDTH / 2.0f;

    // The camera stays inside the active region: each region is its own list and its own scene.
    const RegionInfo& region = world.getRegion(regionIndex);
    float minX = region.left;
    float maxX = region.right - SCREEN_WIDTH;
    if (maxX < minX) maxX = minX;

    // The loop region fits on one screen: frame all of it so the way back to the head is visible.
    if (region.kind == ListKind::Circular) {
        desired = region.left;
    }

    if (desired < minX) desired = minX;
    if (desired > maxX) desired = maxX;
    if (desired < 0.0f) desired = 0.0f;
    if (desired > WORLD_WIDTH - SCREEN_WIDTH) desired = WORLD_WIDTH - SCREEN_WIDTH;

    if (dt <= 0.0f) {
        camera.target.x = desired; // snap (region change, recovery)
    } else {
        float follow = dt * 8.0f;
        if (follow > 1.0f) follow = 1.0f;
        camera.target.x += (desired - camera.target.x) * follow;
    }
    camera.target.y = 0.0f;
    camera.offset = Vector2{0.0f, 0.0f};
}

// ---------------------------------------------------------------------------
// Effects (visual only)
// ---------------------------------------------------------------------------

Vector2 Game::playerFeet() const {
    return Vector2{player.position.x + player.size.x / 2.0f, player.position.y + player.size.y};
}

Color Game::dustColor() const {
    switch (regionIndex) {
        case 0: return Color{150, 124, 92, 255};  // forest earth
        case 1: return Color{150, 160, 182, 255}; // stone dust
        case 2: return Color{204, 186, 232, 255}; // pollen
        default: return Color{150, 150, 162, 255}; // grit
    }
}

// Leaves, chips, or petals kicked up when the player arrives on a new node.
// Arriving through previous (Region 2) uses the cool light of the returning path.
void Game::spawnArrivalEffects(bool viaPrevious) {
    Color first = WARM_LIGHT;
    Color second = WHITE;
    switch (regionIndex) {
        case 0: first = Color{184, 226, 112, 255}; second = WARM_LIGHT; break;
        case 1: first = viaPrevious ? COOL_LIGHT : WARM_LIGHT; second = viaPrevious ? WHITE : Color{184, 192, 204, 255}; break;
        case 2: first = Color{255, 150, 210, 255}; second = Color{120, 255, 230, 255}; break;
        default: first = WARM_LIGHT; second = Color{164, 164, 172, 255}; break;
    }
    spawnBurst(particles, playerFeet(), 10, first, second, 90.0f, 160.0f, 0.7f);
    if (viaPrevious) { // a streak of returning light behind the traveler
        for (int i = 0; i < 8; i++) {
            Particle p;
            p.position = Vector2{playerFeet().x + 10.0f + i * 7.0f, playerFeet().y - 20.0f - (i % 3) * 6.0f};
            p.velocity = Vector2{-60.0f - i * 6.0f, 0.0f};
            p.gravity = 0.0f;
            p.life = p.maxLife = 0.5f + i * 0.03f;
            p.size = 2.0f;
            p.color = COOL_LIGHT;
            spawnParticle(particles, p);
        }
    }
}
