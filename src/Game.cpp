#include "Game.h"

#include <cassert>
#include <cmath>
#include <cstdio>

#include "Theme.h"

static constexpr float RECOVERY_FADE_SECONDS = 0.6f;
static constexpr int HUD_CARD_BOTTOM = 60; // the objective card ends here; the code chip goes below it

// ---------------------------------------------------------------------------
// Setup and teardown
// ---------------------------------------------------------------------------

Game::Game() {
    camera.zoom = 1.0f;
    player.size = Vector2{32.0f, 48.0f};
    player.facing = 1.0f;

    // Region 1 is drawn into this small canvas and scaled up 2x (pixel art).
    forestCanvas = LoadRenderTexture(CANVAS_WIDTH, CANVAS_HEIGHT);

    world.build();
    enterRegion(0);
}

Game::~Game() {
    // Drop every non-owning pointer before `world` (the owner) deletes the nodes.
    player.currentNode = nullptr;
    checkpointNode = nullptr;
    loopStartNode = nullptr;

    UnloadRenderTexture(forestCanvas);
}

bool Game::shouldQuit() const {
    return quitRequested;
}

void Game::resetStage() {
    // Non-owning pointers first, then the owners free and rebuild their nodes.
    player.currentNode = nullptr;
    checkpointNode = nullptr;
    loopStartNode = nullptr;

    world.clearAll();
    world.build();

    inspectorOpen = false;
    enterRegion(0);
}

void Game::enterRegion(int index) {
    regionIndex = index;
    stage = 0;
    gateOpen = false;
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
    updateLedgeMist(0.0f); // start with the right ledges veiled, no fade-in

    switch (index) {
        case 0:
            showMessage({
                "Welcome, wanderer. Every ledge in this forest is a real Node,\n"
                "created with new and owned by a singly linked list.",
                "Each node stores one pointer: next. Only the ledge it points to is solid,\n"
                "the rest fade into mist. Follow next from the head N1 to the tail N5.",
            });
            break;
        case 1:
            showMessage({
                "Region 2: Doubly Linked List\n"
                "Each node now has two pointers: next and previous.",
                "You can move both ways. Orange arrows are previous links.\n"
                "head->previous and tail->next are NULL.",
                "Goal: go forward to N4 (Key), go back to N2 using previous,\n"
                "then continue to the tail N5.",
            });
            break;
        case 2:
            showMessage({
                "Region 3: Circular Singly Linked List\n"
                "The tail's next points back to the head, so no next is NULL.",
                "The portal on the tail is the link tail->next. The arc above shows\n"
                "where it leads: back to the head. Walk into the portal to follow it.",
                "A loop over a circular list needs a stop condition, or it never ends.\n"
                "Goal: follow next all the way around until current == start (N1).",
            });
            break;
        case 3:
            showMessage({
                "Region 4: NULL Pointer Safety\n"
                "nullptr means \"no node\". Following it is a bug: in C++ it is undefined behavior.",
                "Always check before you follow:\n"
                "    if (current->next != nullptr) { current = current->next; }",
                "Goal: reach N3 (Edge), inspect its next link, and fix the gap safely.\n"
                "Stay out of the amber gap.",
            });
            break;
        default:
            break;
    }
}

void Game::completeRegionObjective(std::vector<std::string> checkpointPages) {
    gateOpen = true;
    setCheckpoint(player.currentNode);
    showMessage(std::move(checkpointPages));
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::update(float dt) {
    elapsed += dt;
    if (toastTimer > 0.0f) toastTimer -= dt;
    if (fadeTimer > 0.0f) fadeTimer -= dt;
    if (arrivalPulse.timer > 0.0f) arrivalPulse.timer -= dt;
    if (checkpointPulse.timer > 0.0f) checkpointPulse.timer -= dt;
    updateLedgeMist(dt);

    if (state != GameState::Complete && IsKeyPressed(KEY_TAB)) {
        inspectorOpen = !inspectorOpen;
    }

    bool continuePressed = IsKeyPressed(KEY_E) || IsKeyPressed(KEY_ENTER);
    switch (state) {
        case GameState::Paused:
            if (IsKeyPressed(KEY_ESCAPE)) state = stateBeforePause;
            if (IsKeyPressed(KEY_Q)) quitRequested = true;
            break;

        case GameState::Complete:
            if (IsKeyPressed(KEY_R)) resetStage();
            if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_Q)) quitRequested = true;
            break;

        case GameState::Message:
            if (IsKeyPressed(KEY_ESCAPE)) {
                stateBeforePause = GameState::Message;
                state = GameState::Paused;
            } else if (continuePressed) {
                pageIndex++;
                if (pageIndex >= pages.size()) {
                    pages.clear();
                    pageIndex = 0;
                    state = GameState::Playing;
                }
            }
            break;

        case GameState::Playing:
            if (IsKeyPressed(KEY_ESCAPE)) {
                stateBeforePause = GameState::Playing;
                state = GameState::Paused;
            } else {
                if (continuePressed) tryBuildBridge();
                if (state == GameState::Playing) updatePlaying(dt); // the bridge may have opened a message
            }
            break;
    }
}

void Game::updatePlaying(float dt) {
    float previousBottom = player.position.y + player.size.y;
    bool wasGrounded = player.grounded;

    updatePlayerMovement(player, physics, dt);

    // Keep the player inside the world horizontally.
    if (player.position.x < 0.0f) player.position.x = 0.0f;
    if (player.position.x > WORLD_WIDTH - player.size.x) player.position.x = WORLD_WIDTH - player.size.x;

    resolveLandings(previousBottom);
    if (!wasGrounded && player.grounded) {
        player.landTimer = LAND_POSE_SECONDS;
    }
    if (state != GameState::Playing) {
        updateCamera(dt);
        return; // a landing finished an objective and opened a message
    }

    Rectangle playerRect = getPlayerRect(player);

    // Region 3: the portal on the tail is the link tail->next.
    if (regionIndex == 2) {
        Node* tail = world.getLoopTail();
        if (tail != nullptr && player.currentNode == tail && CheckCollisionRecs(playerRect, world.getPortalRect())) {
            Node* destination = tail->next; // read the real link (it is the head)
            if (destination != nullptr) {
                followLink(destination);
                placePlayerOnPlatform(player, *destination);
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

    // Exit gate: a hand-off to the next region's list, not a pointer.
    Rectangle gate;
    if (state == GameState::Playing && gateOpen && world.getGateRect(regionIndex, gate) &&
        CheckCollisionRecs(playerRect, gate)) {
        if (regionIndex < REGION_COUNT - 1) {
            enterRegion(regionIndex + 1);
            showToast(TextFormat("Gate: Region %d is a separate list.  current = nextList.getHead();",
                                 regionIndex + 1), 4.0f);
        } else {
            state = GameState::Complete;
        }
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
    if (from->next == destination) {
        code = "current = current->next;";
    } else if (world.getRegion(regionIndex).kind == ListKind::Doubly && from->previous == destination) {
        code = "current = current->previous;";
    } else {
        return; // not reachable through a permitted link: do not move
    }

    player.currentNode = destination;
    markVisited(destination);

    // Arrival effect at the player's feet: marks a successful traversal. It does not touch any link.
    Rectangle platform = getPlatformRect(*destination);
    arrivalPulse = Pulse{Vector2{player.position.x + player.size.x / 2.0f, platform.y}, 0.5f, 0.5f,
                         REGION_THEMES[regionIndex].accent};

    if (regionIndex == 2 && destination == loopStartNode) {
        showToast(TextFormat("%s   N%d -> N%d  (tail->next is the head)", code, from->id, destination->id));
    } else {
        showToast(TextFormat("%s   N%d -> N%d", code, from->id, destination->id));
    }

    if (regionIndex == 2) {
        loopSteps++;
    }
    checkObjectives();
}

void Game::checkObjectives() {
    Node* current = player.currentNode;
    if (current == nullptr || gateOpen) {
        return;
    }

    switch (regionIndex) {
        case 0:
            if (current->next == nullptr) { // reached the tail
                completeRegionObjective({
                    "Checkpoint question: Which link tells you where to go next?",
                    "Answer: next. Each node stores the address of the following node.\n"
                    "The tail's next is NULL, so the list ends at N5. The gate is now open.",
                });
            }
            break;

        case 1:
            if (stage == 0 && current->id == 4) {
                stage = 1;
                setCheckpoint(current);
                showToast("N4 (Key) reached. Now follow previous links back to N2.", 4.0f);
            } else if (stage == 1 && current->id == 2) {
                stage = 2;
                setCheckpoint(current);
                showToast("Back at N2 through previous. Now continue forward to the tail.", 4.0f);
            } else if (stage == 2 && current->next == nullptr) {
                completeRegionObjective({
                    "Checkpoint question: What does a doubly linked list store that a\n"
                    "singly linked list does not?",
                    "Answer: a previous link. That is how you walked back from N4 to N2.\n"
                    "head->previous and tail->next are both NULL. The gate is now open.",
                });
            }
            break;

        case 2:
            // Stop condition: we are back at the node we started from.
            if (current == loopStartNode && loopSteps > 0) {
                completeRegionObjective({
                    "Checkpoint question: In a circular list no next is NULL.\n"
                    "So when does a traversal stop?",
                    TextFormat("Answer: when current == start, or after a maximum number of steps.\n"
                               "Without a stop condition, the loop never ends. You stopped after %d steps.\n"
                               "The gate on N1 is now open.", loopSteps),
                });
            } else if (loopSteps >= MAX_LOOP_STEPS) {
                // Safety stop: a bounded step count also ends the traversal.
                loopSteps = 0;
                checkpointNode = loopStartNode;
                restoreToCheckpoint();
                showMessage({TextFormat("Safety limit: %d steps without reaching the start.\n"
                                        "The traversal was stopped and restarted at N1.", MAX_LOOP_STEPS)});
            }
            break;

        case 3:
            if (stage == 0 && current->id == 3) {
                stage = 1;
                setCheckpoint(current);
                showToast("You are on N3 (Edge). Open the inspector (Tab) and read N3->next.", 4.0f);
            } else if (stage == 2 && current->id == 4) {
                completeRegionObjective({
                    "Checkpoint question: What must you do before following a pointer?",
                    "Answer: check it.   if (current->next != nullptr) { current = current->next; }\n"
                    "nullptr means there is no node: nothing to read, nothing to stand on.\n"
                    "The exit gate is open.",
                });
            }
            break;

        default:
            break;
    }
}

// Region 4 interaction: the safe check, then append() fixes the missing link.
void Game::tryBuildBridge() {
    if (regionIndex != 3 || stage != 1) {
        return;
    }
    Node* current = player.currentNode;
    if (current == nullptr || current->id != 3) {
        showToast("Stand on N3 (Edge) to fix its link.");
        return;
    }
    if (!inspectorOpen) {
        showToast("Inspect first: open the inspector (Tab) and read N3->next.");
        return;
    }

    // Read the pointer before following it.
    if (current->next == nullptr) {
        Node* bridge = world.appendBridgeNode(); // append() allocates N4 and sets N3->next = N4
        stage = 2;
        if (bridge != nullptr) {
            Rectangle platform = getPlatformRect(*bridge);
            arrivalPulse = Pulse{Vector2{platform.x + platform.width / 2.0f, platform.y}, 0.7f, 0.7f, NULL_AMBER};
        }
        showMessage({
            "Check: N3->next == nullptr, so there was nothing to follow.",
            "You called append(). It created a new Node with new and set N3->next to it.\n"
            "The link is valid now, so the new node N4 is solid. Jump onto it and take the exit.",
        });
    }
}

void Game::recoverFromFall(bool movedBackward) {
    int fromId = (player.currentNode != nullptr) ? player.currentNode->id : 0;

    if (regionIndex == 2) {
        loopSteps = 0; // the loop must start again from the head
        checkpointNode = loopStartNode;
    }
    restoreToCheckpoint();
    int checkpointId = (player.currentNode != nullptr) ? player.currentNode->id : 0;

    std::vector<std::string> recoveryPages;
    if (regionIndex == 0 && movedBackward) {
        recoveryPages.push_back(TextFormat(
            "No previous link! N%d only stores next.\n"
            "A singly linked list cannot step backward, so the platform behind you was not solid.", fromId));
    } else if (regionIndex == 2 && movedBackward) {
        recoveryPages.push_back(
            "A circular singly linked list also stores only next.\n"
            "To reach an earlier node, keep going forward around the loop.");
    } else {
        recoveryPages.push_back(
            "You fell between nodes.\n"
            "Only your current node and the nodes its links point to are solid.");
    }

    if (regionIndex == 2) {
        recoveryPages.push_back(TextFormat("Recovery: back at the start N%d. Loop steps reset to 0.", checkpointId));
    } else {
        recoveryPages.push_back(TextFormat("Recovery: you are back on checkpoint N%d.", checkpointId));
    }
    showMessage(recoveryPages);
}

void Game::recoverFromNullHazard() {
    restoreToCheckpoint();
    showMessage({
        "NULL hazard! You stepped past N3, but N3->next == nullptr.\n"
        "There is no node there: nothing to stand on, nothing to read.",
        "In C++, dereferencing nullptr is undefined behavior (usually a crash).\n"
        "The game checks first:   if (current->next == nullptr) { /* do not follow */ }",
        "Recovery: you are back on N3.\n"
        "Open the inspector (Tab), then press E to fix the missing link.",
    });
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
}

void Game::setCheckpoint(Node* node) {
    if (node == nullptr || node == checkpointNode) {
        return;
    }
    checkpointNode = node;
    Rectangle platform = getPlatformRect(*node);
    checkpointPulse = Pulse{Vector2{platform.x + platform.width / 2.0f, platform.y}, 0.8f, 0.8f, CHECKPOINT_CREAM};
}

void Game::markVisited(const Node* node) {
    if (node != nullptr && node->id >= 0 && node->id <= MAX_NODES_PER_REGION) {
        visitedIds[node->id] = true;
    }
}

// Visual only: ledges that are not solid right now (Traversal Rule A) fade into mist.
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

    // The loop region fits on one screen: frame all of it so tail->next is visible.
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

void Game::showMessage(std::vector<std::string> newPages) {
    pages = std::move(newPages);
    pageIndex = 0;
    state = pages.empty() ? GameState::Playing : GameState::Message;
}

void Game::showToast(const std::string& text, float seconds) {
    toastText = text;
    toastTimer = seconds;
}

const char* Game::currentObjective() const {
    switch (regionIndex) {
        case 0:
            return gateOpen ? "Done! Walk into the gate on N5." : "Follow next from the head N1 to the tail N5.";
        case 1:
            if (gateOpen) return "Done! Walk into the gate on N5.";
            if (stage == 0) return "Go forward to N4 (Key).";
            if (stage == 1) return "Follow previous back to N2 (Hall).";
            return "Continue forward to the tail N5.";
        case 2:
            return gateOpen ? "Back at the start! Walk into the gate on N1."
                            : "Follow next around the loop back to the start N1.";
        case 3:
            if (gateOpen) return "Stage clear! Walk into the EXIT gate.";
            if (stage == 0) return "Reach N3 (Edge) and inspect its next link.";
            if (stage == 1) return "N3->next is NULL. Open the inspector (Tab), press E.";
            return "Jump onto the new node N4 (Bridge).";
        default:
            return "";
    }
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

namespace {

// Key cap followed by an action, e.g. [TAB] inspector. Returns the width used.
float drawKeyHint(const char* key, const char* action, float x, float y, Color actionColor) {
    int keyWidth = MeasureText(key, 10) + 10;
    DrawRectangleRounded(Rectangle{x, y, (float)keyWidth, 14.0f}, 0.3f, 4, UI_TEXT);
    DrawText(key, (int)(x + 5.0f), (int)(y + 2.0f), 10, OUTLINE_DARK);
    DrawText(action, (int)(x + keyWidth + 6.0f), (int)(y + 2.0f), 10, actionColor);
    return keyWidth + 6.0f + MeasureText(action, 10);
}

float measureKeyHint(const char* key, const char* action) {
    return MeasureText(key, 10) + 10.0f + 6.0f + MeasureText(action, 10);
}

// The inspector's "walk from head" line is made of tokens such as N1, ->, NULL.
struct Token {
    char text[24];
    Color color;
};

int addToken(Token* tokens, int count, int capacity, const char* text, Color color) {
    if (count >= capacity) return count;
    std::snprintf(tokens[count].text, sizeof(tokens[count].text), "%s", text);
    tokens[count].color = color;
    return count + 1;
}

// Lays tokens out left to right, wrapping at maxX. Draws them only if `draw` is true.
int layoutTokens(const Token* tokens, int count, float x, float y, float maxX, bool draw) {
    float cursor = x;
    int lines = 1;
    for (int i = 0; i < count; i++) {
        int width = MeasureText(tokens[i].text, 20);
        if (cursor + width > maxX && cursor > x) {
            cursor = x;
            y += 26.0f;
            lines++;
        }
        if (draw) DrawText(tokens[i].text, (int)cursor, (int)y, 20, tokens[i].color);
        cursor += width + 8.0f;
    }
    return lines;
}

// "N3 - Beta" for a node, "NULL" for nullptr.
void describeNode(char* out, size_t size, const Node* node) {
    if (node == nullptr) {
        std::snprintf(out, size, "NULL");
    } else {
        std::snprintf(out, size, "N%d - %s", node->id, node->label);
    }
}

// Small "follow" chip shown next to a link the player can actually take.
void drawFollowChip(float rightX, float y, Color color, bool filled) {
    int width = MeasureText("FOLLOW", 10) + 10;
    Rectangle chip = {rightX - width, y + 3.0f, (float)width, 14.0f};
    if (filled) {
        DrawRectangleRounded(chip, 0.3f, 4, color);
        DrawText("FOLLOW", (int)(chip.x + 5.0f), (int)(chip.y + 2.0f), 10, OUTLINE_DARK);
    } else {
        DrawRectangleRoundedLinesEx(chip, 0.3f, 4, 1.0f, color);
        DrawText("FOLLOW", (int)(chip.x + 5.0f), (int)(chip.y + 2.0f), 10, color);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void Game::draw() const {
    ClearBackground(UI_NAVY);

    // Region 1 has the natural forest art; the other regions still use the classic look.
    if (regionIndex == 0) {
        drawForestScene();
    } else {
        drawClassicScene();
    }

    if (fadeTimer > 0.0f) {
        DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(OUTLINE_DARK, fadeTimer / RECOVERY_FADE_SECONDS));
    }

    drawHud();
    if (inspectorOpen && state != GameState::Complete) drawInspector();
    drawToast();

    if (state == GameState::Playing) drawControlsStrip();
    if (state == GameState::Message) drawMessageBox();
    if (state == GameState::Paused) drawPauseScreen();
    if (state == GameState::Complete) drawCompleteScreen();
}

void Game::drawPulse(const Pulse& pulse) const {
    if (pulse.timer <= 0.0f || pulse.duration <= 0.0f) {
        return;
    }
    float t = 1.0f - pulse.timer / pulse.duration; // 0 -> 1
    float radius = 10.0f + t * 60.0f;
    DrawRing(pulse.position, radius - 3.0f, radius, 0.0f, 360.0f, 36, Fade(pulse.color, (1.0f - t) * 0.9f));
    for (int k = 0; k < 6; k++) { // a few sparkles flying outward
        float angle = k * 60.0f * DEG2RAD + 0.5f;
        float distance = 12.0f + t * 44.0f;
        float x = pulse.position.x + std::cos(angle) * distance;
        float y = pulse.position.y - 6.0f + std::sin(angle) * distance * 0.5f;
        DrawRectangle((int)x, (int)y, 3, 3, Fade(pulse.color, 1.0f - t));
    }
}

// Region 4: a key hint above the player while standing on N3 with the NULL link.
void Game::drawInteractionPrompt() const {
    const Node* current = player.currentNode;
    if (state != GameState::Playing || regionIndex != 3 || stage != 1 || current == nullptr || current->id != 3) {
        return;
    }
    const char* key = inspectorOpen ? "E" : "TAB";
    const char* action = inspectorOpen ? "check & fix N3->next" : "inspect N3->next";
    float width = measureKeyHint(key, action) + 12.0f;
    float x = player.position.x + player.size.x / 2.0f - width / 2.0f;
    float y = player.position.y - 30.0f;
    DrawRectangleRounded(Rectangle{x, y, width, 20.0f}, 0.4f, 4, Fade(UI_NAVY, 0.92f));
    DrawRectangleRoundedLinesEx(Rectangle{x, y, width, 20.0f}, 0.4f, 4, 1.5f, NULL_AMBER);
    drawKeyHint(key, action, x + 6.0f, y + 3.0f, NULL_AMBER);
}

// Compact HUD: a small card with the region and the objective (top left), and the
// list progress read from the real nodes (top right, hidden while the inspector is open).
void Game::drawHud() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionTheme& theme = REGION_THEMES[regionIndex];

    const char* regionLine = TextFormat("REGION %d  -  %s", regionIndex + 1, TextToUpper(region.title));
    const char* objective = currentObjective();
    int textWidth = MeasureText(regionLine, 10);
    int objectiveWidth = MeasureText(objective, 20);
    if (objectiveWidth > textWidth) textWidth = objectiveWidth;

    Rectangle card = {12.0f, 10.0f, textWidth + 40.0f, 50.0f};
    DrawRectangleRounded(card, 0.25f, 6, Fade(UI_NAVY, 0.78f));
    DrawRectangleRounded(Rectangle{card.x + 10.0f, card.y + 10.0f, 3.0f, 30.0f}, 1.0f, 4, theme.accent);
    DrawText(regionLine, (int)card.x + 22, (int)card.y + 9, 10, theme.accent);
    DrawText(objective, (int)card.x + 22, (int)card.y + 22, 20, OBJECTIVE_YELLOW);

    if (!inspectorOpen) {
        drawProgressPill();
    }
}

void Game::drawControlsStrip() const {
    struct Hint { const char* key; const char* action; };
    const Hint hints[] = {
        {"A/D or ARROWS", "move"}, {"SPACE", "jump"}, {"E/ENTER", "interact"}, {"TAB", "inspector"}, {"ESC", "pause"},
    };
    float total = 0.0f;
    for (const Hint& hint : hints) total += measureKeyHint(hint.key, hint.action) + 14.0f;
    total -= 14.0f;

    // Small and quiet in the bottom-left corner, away from the route.
    float x = 22.0f;
    float y = SCREEN_HEIGHT - 26.0f;
    DrawRectangleRounded(Rectangle{x - 10.0f, y - 4.0f, total + 20.0f, 22.0f}, 0.5f, 6, Fade(UI_NAVY, 0.6f));
    for (const Hint& hint : hints) {
        x += drawKeyHint(hint.key, hint.action, x, y, UI_DIM_TEXT) + 14.0f;
    }
}

// Code chip under the top band, e.g. "current = current->next;   N2 -> N3".
void Game::drawToast() const {
    if (toastTimer <= 0.0f || toastText.empty()) {
        return;
    }
    float alpha = toastTimer < 0.4f ? toastTimer / 0.4f : 1.0f;
    int width = MeasureText(toastText.c_str(), 20);
    float y = HUD_CARD_BOTTOM + 8.0f;
    DrawRectangleRounded(Rectangle{10.0f, y, width + 28.0f, 30.0f}, 0.3f, 4, Fade(UI_NAVY, 0.88f * alpha));
    DrawRectangle(10, (int)y + 6, 3, 18, Fade(REGION_THEMES[regionIndex].accent, alpha));
    DrawText(toastText.c_str(), 22, (int)y + 5, 20, Fade(Color{150, 230, 255, 255}, alpha));
}

// Compact code-learning panel: the current node's real links, then a walk from head.
void Game::drawInspector() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    const RegionTheme& theme = REGION_THEMES[regionIndex];
    Node* head = world.getHead(regionIndex);
    const Node* current = player.currentNode;
    bool isDoubly = region.kind == ListKind::Doubly;
    bool emphasizePrevious = regionIndex == 1 && stage == 1 && !gateOpen;

    // Build the "walk from head" line by reading the actual links.
    const int capacity = 40;
    Token tokens[capacity];
    int count = 0;
    const char* stopNote = "stopped: safety step limit reached";
    if (isDoubly && head != nullptr && head->previous == nullptr) {
        count = addToken(tokens, count, capacity, "NULL", NULL_AMBER);
        count = addToken(tokens, count, capacity, "<-", UI_DIM_TEXT);
    }
    Node* node = head;
    int steps = 0;
    char id[16];
    while (node != nullptr && steps < MAX_NODES_PER_REGION) {
        std::snprintf(id, sizeof(id), "N%d", node->id);
        count = addToken(tokens, count, capacity, id, node == current ? CURRENT_GOLD : UI_TEXT);
        Node* next = node->next;
        steps++;
        if (next == nullptr) {
            count = addToken(tokens, count, capacity, "->", UI_DIM_TEXT);
            count = addToken(tokens, count, capacity, "NULL", NULL_AMBER);
            stopNote = "stopped: next == NULL (end of the list)";
            break;
        }
        if (next == head) { // circular list: the stop condition fires
            std::snprintf(id, sizeof(id), "N%d", head->id);
            count = addToken(tokens, count, capacity, "->", UI_DIM_TEXT);
            count = addToken(tokens, count, capacity, id, theme.accent);
            stopNote = "stopped: back at the head (node == head)";
            break;
        }
        bool twoWay = isDoubly && next->previous == node;
        count = addToken(tokens, count, capacity, twoWay ? "<->" : "->", UI_DIM_TEXT);
        node = next;
    }

    const char* note = nullptr;
    if (current != nullptr) {
        if (regionIndex == 3 && stage == 1 && current->next == nullptr) {
            note = "next is NULL: no node to follow. Press E to append one.";
        } else if (isDoubly && current->previous == nullptr) {
            note = "previous is NULL: this is the head.";
        } else if (current->next == nullptr) {
            note = "next is NULL: this is the tail.";
        } else if (region.kind == ListKind::Circular && current->next == head) {
            note = "next is the head: this is the tail of the loop.";
        }
    }

    // Panel size depends on how many lines the walk needs.
    const float x = SCREEN_WIDTH - 414.0f;
    const float width = 404.0f;
    const float pad = 14.0f;
    const float valueX = x + pad + 130.0f;
    const float rightX = x + width - pad;
    int walkLines = layoutTokens(tokens, count, x + pad, 0.0f, rightX, false);
    float height = 12.0f + 24.0f + 18.0f + 28.0f + 26.0f + 28.0f + 10.0f + 16.0f + walkLines * 26.0f + 18.0f +
                   (note != nullptr ? 18.0f : 0.0f) + 6.0f;

    float y = 10.0f; // top right; the progress pill is hidden while the inspector is open
    Rectangle panel = {x, y, width, height};
    DrawRectangleRounded(panel, 0.06f, 6, Fade(UI_NAVY, 0.93f));
    DrawRectangleRoundedLinesEx(panel, 0.06f, 6, 1.5f, Fade(theme.accent, 0.8f));
    y += 12.0f;

    DrawText("INSPECTOR", (int)(x + pad), (int)y, 20, UI_TEXT);
    drawKeyHint("TAB", "hide", rightX - measureKeyHint("TAB", "hide"), y + 3.0f, UI_DIM_TEXT);
    y += 24.0f;
    const char* kindName = region.kind == ListKind::Singly   ? "SINGLY LINKED LIST"
                         : region.kind == ListKind::Doubly   ? "DOUBLY LINKED LIST"
                                                             : "CIRCULAR SINGLY LINKED LIST";
    DrawText(kindName, (int)(x + pad), (int)y, 10, theme.accent);
    y += 18.0f;

    // The node the player stands on, with its real pointer values.
    char text[48];
    if (current == nullptr) {
        DrawText("current = NULL (no node!)", (int)(x + pad), (int)y, 20, NULL_AMBER);
        y += 82.0f;
    } else {
        describeNode(text, sizeof(text), current);
        DrawText(TextFormat("Node %s", text), (int)(x + pad), (int)y, 20, CURRENT_GOLD);
        y += 28.0f;

        // next: every list kind stores it.
        Color nextColor = (current->next != nullptr) ? theme.accent : NULL_AMBER;
        describeNode(text, sizeof(text), current->next);
        DrawText("next", (int)(x + pad), (int)y, 20, nextColor);
        DrawText("->", (int)(valueX - 30.0f), (int)y, 20, UI_DIM_TEXT);
        DrawText(text, (int)valueX, (int)y, 20, current->next != nullptr ? UI_TEXT : NULL_AMBER);
        if (current->next != nullptr) drawFollowChip(rightX, y, theme.accent, !emphasizePrevious);
        y += 26.0f;

        // previous: only the doubly linked list stores it.
        if (isDoubly) {
            Color prevColor = (current->previous != nullptr) ? PREVIOUS_ORANGE : NULL_AMBER;
            describeNode(text, sizeof(text), current->previous);
            DrawText("previous", (int)(x + pad), (int)y, 20, prevColor);
            DrawText("->", (int)(valueX - 30.0f), (int)y, 20, UI_DIM_TEXT);
            DrawText(text, (int)valueX, (int)y, 20, current->previous != nullptr ? UI_TEXT : NULL_AMBER);
            if (current->previous != nullptr) drawFollowChip(rightX, y, PREVIOUS_ORANGE, emphasizePrevious);
        } else {
            DrawText("previous", (int)(x + pad), (int)y, 20, Fade(UI_DIM_TEXT, 0.6f));
            DrawText("not stored in this list", (int)valueX, (int)(y + 5.0f), 10, UI_DIM_TEXT);
        }
        y += 28.0f;
    }

    DrawRectangle((int)(x + pad), (int)y, (int)(width - 2.0f * pad), 1, Fade(UI_DIM_TEXT, 0.4f));
    y += 10.0f;
    DrawText("WALK FROM HEAD", (int)(x + pad), (int)y, 10, UI_DIM_TEXT);
    y += 16.0f;
    layoutTokens(tokens, count, x + pad, y, rightX, true);
    y += walkLines * 26.0f;
    DrawText(stopNote, (int)(x + pad), (int)y, 10, UI_DIM_TEXT);
    y += 18.0f;
    if (note != nullptr) {
        DrawText(note, (int)(x + pad), (int)y, 10, (regionIndex == 3) ? NULL_AMBER : CHECKPOINT_CREAM);
    }
}

// Slim message card in the lower part of the screen; it closes after the last page.
void Game::drawMessageBox() const {
    if (pageIndex >= pages.size()) return;
    const std::string& text = pages[pageIndex];
    const RegionTheme& theme = REGION_THEMES[regionIndex];

    int lines = 1;
    for (char c : text) {
        if (c == '\n') lines++;
    }
    float width = MeasureText(text.c_str(), 20) + 56.0f;
    if (width < 420.0f) width = 420.0f;
    if (width > SCREEN_WIDTH - 40.0f) width = SCREEN_WIDTH - 40.0f;
    float height = lines * 22.0f + 50.0f;
    Rectangle card = {SCREEN_WIDTH / 2.0f - width / 2.0f, SCREEN_HEIGHT - height - 12.0f, width, height};

    DrawRectangleRounded(card, 0.12f, 6, Fade(UI_NAVY, 0.95f));
    DrawRectangleRoundedLinesEx(card, 0.12f, 6, 1.5f, Fade(theme.accent, 0.7f));
    DrawRectangle((int)card.x + 10, (int)card.y + 12, 3, (int)card.height - 24, theme.accent);
    DrawText(text.c_str(), (int)card.x + 28, (int)card.y + 14, 20, UI_TEXT);

    // Footer: page dots on the left, continue hint on the right.
    float footerY = card.y + card.height - 22.0f;
    for (size_t i = 0; i < pages.size(); i++) {
        Color dot = (i == pageIndex) ? theme.accent : Fade(UI_DIM_TEXT, 0.5f);
        DrawCircleV(Vector2{card.x + 32.0f + i * 12.0f, footerY + 7.0f}, 3.0f, dot);
    }
    const char* action = (pageIndex + 1 < pages.size()) ? "next" : "close";
    float hintWidth = measureKeyHint("E / ENTER", action);
    drawKeyHint("E / ENTER", action, card.x + card.width - hintWidth - 18.0f, footerY, UI_DIM_TEXT);
}

void Game::drawPauseScreen() const {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(OUTLINE_DARK, 0.6f));
    Rectangle card = {SCREEN_WIDTH / 2.0f - 220.0f, 250.0f, 440.0f, 170.0f};
    DrawRectangleRounded(card, 0.12f, 6, Fade(UI_NAVY, 0.96f));
    DrawRectangleRoundedLinesEx(card, 0.12f, 6, 1.5f, REGION_THEMES[regionIndex].accent);

    const char* title = "PAUSED";
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, 40) / 2, (int)card.y + 26, 40, UI_TEXT);

    float total = measureKeyHint("ESC", "resume") + 30.0f + measureKeyHint("Q", "quit");
    float x = SCREEN_WIDTH / 2.0f - total / 2.0f;
    x += drawKeyHint("ESC", "resume", x, card.y + 96.0f, UI_TEXT) + 30.0f;
    drawKeyHint("Q", "quit", x, card.y + 96.0f, UI_TEXT);
    const char* tip = "Tab opens the inspector at any time.";
    DrawText(tip, SCREEN_WIDTH / 2 - MeasureText(tip, 10) / 2, (int)card.y + 136, 10, UI_DIM_TEXT);
}

void Game::drawCompleteScreen() const {
    DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(OUTLINE_DARK, 0.85f));
    Rectangle card = {150.0f, 90.0f, 980.0f, 540.0f};
    DrawRectangleRounded(card, 0.06f, 6, Fade(UI_NAVY, 0.97f));
    DrawRectangleRoundedLinesEx(card, 0.06f, 6, 2.0f, CURRENT_GOLD);

    const char* title = "Tutorial Stage Complete!";
    DrawText(title, SCREEN_WIDTH / 2 - MeasureText(title, 40) / 2, (int)card.y + 30, 40, CURRENT_GOLD);

    struct Summary { const char* name; const char* line1; const char* line2; };
    const Summary rows[REGION_COUNT] = {
        {"SINGLY", "Follow next in one direction.", "The tail's next is NULL."},
        {"DOUBLY", "next and previous let you move both ways.", "head->previous and tail->next are NULL."},
        {"CIRCULAR", "tail->next is the head, so there is no NULL end.",
         "Stop at the start (current == start) or after a maximum step count."},
        {"NULL CHECK", "Test a pointer before you follow it.", "nullptr means there is no node."},
    };

    float y = card.y + 104.0f;
    for (int i = 0; i < REGION_COUNT; i++) {
        const RegionTheme& theme = REGION_THEMES[i];
        Rectangle chip = {card.x + 40.0f, y + 2.0f, 140.0f, 26.0f};
        DrawRectangleRounded(chip, 0.3f, 4, theme.accent);
        DrawText(rows[i].name, (int)(chip.x + chip.width / 2.0f - MeasureText(rows[i].name, 20) / 2.0f),
                 (int)(chip.y + 3.0f), 20, OUTLINE_DARK);
        DrawText(rows[i].line1, (int)(card.x + 200.0f), (int)y, 20, UI_TEXT);
        DrawText(rows[i].line2, (int)(card.x + 200.0f), (int)(y + 24.0f), 20, UI_DIM_TEXT);
        y += 72.0f;
    }

    DrawText(TextFormat("Nodes still allocated: %d. Each list frees its own nodes, once, on exit.", g_liveNodeCount),
             (int)(card.x + 40.0f), (int)(y + 8.0f), 20, Color{150, 230, 255, 255});

    float total = measureKeyHint("R", "replay") + 40.0f + measureKeyHint("Q / ESC", "exit");
    float x = SCREEN_WIDTH / 2.0f - total / 2.0f;
    x += drawKeyHint("R", "replay", x, card.y + card.height - 44.0f, UI_TEXT) + 40.0f;
    drawKeyHint("Q / ESC", "exit", x, card.y + card.height - 44.0f, UI_TEXT);
}
