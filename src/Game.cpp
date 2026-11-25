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

    // The whole world is drawn into a small canvas and scaled up 2x (pixel art), into a 1280x720
    // frame that is shown in the window or full screen.
    display.load();
    renderer.load();

    world.build();
    checkRouteJumps();
    audio.load(); // after InitAudioDevice() in main; silent if there is no device or no files
    startJourney();
}

// A new journey (the first start or a replay): the first place is entered behind a dark beat
// that names it, then the scene fades in.
void Game::startJourney() {
    audio.cancelPending(); // a replay: nothing from the last journey is still to come
    for (bool& seen : calloutSeen) {
        seen = false;
    }
    calloutTimer = 0.0f;
    journeyStarted = false;
    journeyTimer = 0.0f;
    enterRegion(0);
    transitionTarget = 0;
    transitionTimer = TRANSITION_OUT_SECONDS; // already dark: hold, then fade in
    transitionSwitched = true;
    titleCardTimer = TITLE_CARD_SECONDS;
}

// Every jump the route needs must be possible with the real movement code. A failure is reported
// like a failed link check: in the console, and as an assert in debug builds.
void Game::checkRouteJumps() const {
    auto report = [](int region, int fromId, int toId, bool ok) {
        TraceLog(ok ? LOG_INFO : LOG_WARNING, "Jump check %s: Region %d N%d -> N%d", ok ? "OK" : "FAILED", region + 1,
                 fromId, toId);
        assert(ok && "A jump on the route cannot be made");
    };
    for (int region = 0; region < REGION_COUNT; region++) {
        const ListKind kind = world.getRegion(region).kind;
        // Regions 5 and 6 create their nodes while you play, so their jumps are checked on the
        // layout those nodes will have: each stone up to the next, each boat on to the next.
        if (kind == ListKind::Stack) {
            for (int id = 1; id < World::STONE_SLOTS; id++) {
                report(region, id, id + 1, canJumpBetweenRects(world.getStoneSlot(id), world.getStoneSlot(id + 1), physics, player.size));
            }
            continue;
        }
        if (kind == ListKind::Queue) {
            for (int id = 1; id < World::BOAT_SLOTS; id++) {
                report(region, id, id + 1, canJumpBetweenRects(world.getBoatSlot(id), world.getBoatSlot(id + 1), physics, player.size));
            }
            continue;
        }
        const bool doubly = kind == ListKind::Doubly;
        Node* head = world.getHead(region);
        Node* node = head;
        int steps = 0;
        while (node != nullptr && steps < MAX_NODES_PER_REGION) {
            Node* next = node->next;
            if (next != nullptr && next != head) { // Region 3's tail -> head is the current, not a jump
                report(region, node->id, next->id, canJumpBetween(*node, *next, physics, player.size));
            }
            if (doubly && node->previous != nullptr) { // Region 2 also needs the way back
                report(region, node->id, node->previous->id, canJumpBetween(*node, *node->previous, physics, player.size));
            }
            node = next;
            steps++;
            if (node == head) break;
        }
    }
}

Game::~Game() {
    // Drop every non-owning pointer before `world` (the owner) deletes the nodes.
    player.currentNode = nullptr;
    checkpointNode = nullptr;
    loopStartNode = nullptr;
    passageDestination = nullptr;

    regionArt.clear();
    renderer.unload();
    display.unload(); // before main closes the window
    audio.unload(); // every sound and stream, before main closes the audio device
}

bool Game::shouldQuit() const {
    return quitRequested;
}

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------

// F11 or Alt+Enter anywhere, or the pause screen's Screen row: exclusive fullscreen at the
// resolution of the monitor the window is on, or back to the 1280x720 window.
void Game::toggleFullscreen() {
    display.setFullscreen(!display.isFullscreen());
    audio.play(Sfx::Toggle);
    if (state != GameState::Paused) {
        showToast(display.isFullscreen() ? TextFormat("Fullscreen at %d x %d. F11 returns to the window.", display.width(), display.height())
                                         : "Back in the window. F11 for fullscreen.",
                  3.0f);
    }
}

// The pause screen's rows: up and down choose one (music, sounds, mute, screen), left and right
// (or E on the switches) change it.
void Game::updatePauseSettings(const InputFrame& input) {
    constexpr int ROWS = 4;
    constexpr int SCREEN_ROW = 3;
    if (input.navY != 0) {
        settingsRow = (settingsRow + input.navY + ROWS) % ROWS;
        audio.play(Sfx::Tick, 0.8f);
    }
    if (settingsRow == SCREEN_ROW) {
        if (input.navX != 0 || input.interact) toggleFullscreen();
        return;
    }
    updatePauseAudioPanel(input); // the sound rows (GameSound.cpp)
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
    startJourney();
}

void Game::enterRegion(int index) {
    regionIndex = index;
    bakeRegionArt(regionArt, index, world); // paint this place from its list's nodes
    stage = 0;
    stageTimer = 0.0f;
    gateOpen = false;
    gateTimer = 0.0f;
    nodeTimer = 0.0f;
    loopSteps = 0;
    toastTimer = 0.0f;   // lines and callouts belong to the place where they happened
    calloutTimer = 0.0f;
    for (bool& visited : visitedIds) {
        visited = false;
    }
    for (float& at : createdAt) {
        at = -100.0f; // the nodes that exist now were there all along: no arrival to show
    }
    for (RemovedPiece& piece : removedIds) {
        piece = RemovedPiece{};
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

    // A short welcome in the exploring voice. The journal (Tab) explains the list itself.
    switch (index) {
        case 0:
            showMessage({
                "Welcome, wanderer. This old forest path has crumbled,\n"
                "and only the ledge ahead of you will hold your weight.",
                "Follow the warm light from ledge to ledge, up through\n"
                "the trees and down to the sunlit arch.",
            }, NoteKind::Intro);
            break;
        case 1:
            showMessage({
                "The Forgotten Return. These drowned halls remember both ways:\n"
                "from any stone you can reach the next one, or go back.",
                "Climb to the Key Shrine and take the key.\n"
                "Its light belongs to the brazier in the Old Hall below.",
            }, NoteKind::Intro);
            break;
        case 2:
            showMessage({
                "The Endless Garden. Here the last terrace flows back to the first,\n"
                "so the path never ends by itself.",
                "Walk the whole circle once, and stop when you are back at the gate.\n"
                "The hollow under the great tree will carry you home.",
            }, NoteKind::Intro);
            break;
        case 3:
            showMessage({
                "The Edge of Nothing. The old bridge has fallen,\n"
                "and past the last cliff there is nothing to stand on.",
                "Reach the edge, look before you step,\n"
                "and find a way across to the far side.",
            }, NoteKind::Intro);
            break;
        case 4:
            showMessage({
                "The Lighthouse Stair. Its old stair has fallen,\n"
                "and the fog bell hangs at the top of the tower, out of reach.",
                "Set stair stones one on top of another (E) and climb to the bell.\n"
                "Then take them down again (S), top stone first, to the sea door.",
            }, NoteKind::Intro);
            break;
        case 5:
            showMessage({
                "The Patient Harbor. The bell has called the boats,\n"
                "and they come into the harbor one at a time.",
                "Call a boat to the end of the line (E) and cross from boat to boat to the ship.\n"
                "Each boat leaves once you step off it, in the order they came.",
            }, NoteKind::Intro);
            break;
        default:
            break;
    }
    startRegionAudio(index);
}

// Fade to black, switch region (or show the summary), then fade back in with the region's name.
void Game::startTransition(int target) {
    transitionTarget = target;
    transitionTimer = 0.0f;
    transitionSwitched = false;
    player.velocity = Vector2{0.0f, 0.0f};
    audio.play(Sfx::Exit); // a breath of air into the light
    audio.stopMusic(1.1f);  // fading with the screen; the next place's music begins in the dark
    audio.stopBeds(0.9f);
    audio.setAccents(-1);
}

void Game::completeRegionObjective(const std::string& question, const std::string& answer) {
    gateOpen = true;
    gateTimer = 0.0f;
    setCheckpoint(player.currentNode, true); // the lantern flashes even when it is already here (Region 3: N1)
    stopPlayerPose();
    pages = {question, answer};
    pageKinds = {NoteKind::Question, NoteKind::Answer};
    pageIndex = 0;
    state = GameState::Message; // the exit's chord announces this note
    const Sfx gates[REGION_COUNT] = {Sfx::Gate1, Sfx::Gate2, Sfx::Gate3, Sfx::Gate4, Sfx::Gate5, Sfx::Gate6};
    Rectangle gate;
    float side = world.getGateRect(regionIndex, gate) ? sideOf(gate.x + gate.width * 0.5f) : 0.0f;
    audio.play(gates[regionIndex], 1.0f, 1.0f, side, 0.3f);
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------

void Game::update(const InputFrame& input, float dt) {
    if (input.mute) toggleMute(); // works everywhere, also on the pause screen
    if (input.fullscreen) toggleFullscreen(); // so does this (before drawing: the screen may change size)
    updateAudioMix(dt);
    audio.update(dt, state != GameState::Paused); // streams keep playing (ducked) while paused

    // Paused: the world and all of its clocks stand still until Esc resumes.
    if (state == GameState::Paused) {
        updatePauseSettings(input);
        if (input.pause) {
            state = stateBeforePause;
            audio.resumeWorld();
            audio.play(Sfx::CoverOpen);
        }
        if (input.quit) quitRequested = true;
        return;
    }

    elapsed += dt;
    if (toastTimer > 0.0f) toastTimer -= dt;
    if (calloutTimer > 0.0f) calloutTimer -= dt;
    if (journeyStarted) journeyTimer += dt;
    if (fadeTimer > 0.0f) fadeTimer -= dt;
    if (titleCardTimer > 0.0f) titleCardTimer -= dt;
    if (checkpointPulse.timer > 0.0f) checkpointPulse.timer -= dt;
    if (world.isBridgeBuilt()) bridgeTimer += dt;
    if (gateOpen) gateTimer += dt;
    stageTimer += dt;
    nodeTimer += dt;
    updateParticles(particles, dt);
    updateLedgeMist(dt);
    updateTravelerMotion(player, dt, windAt(playerFeet().x / CANVAS_SCALE, elapsed, regionWind()));

    // Moving between places: fade out, a dark beat that names the place ahead, then fade in.
    // Input waits until the new place starts to appear.
    if (transitionTarget >= 0) {
        transitionTimer += dt;
        if (!transitionSwitched && transitionTimer >= TRANSITION_OUT_SECONDS) {
            transitionSwitched = true;
            clearParticles(particles);
            if (transitionTarget < REGION_COUNT) {
                enterRegion(transitionTarget); // each place is its own list: start at its head
                titleCardTimer = TITLE_CARD_SECONDS;
            } else {
                state = GameState::Complete;
                audio.playMusic(Track::Finale, 1.0f, 5.0f, 0.3f); // the dawn piece, further on
                audio.stopBeds(2.5f);
                audio.play(Sfx::Complete, 1.0f, 1.0f, 0.0f, 0.4f);
            }
            return; // a key pressed on this frame belonged to the old screen, not the new note or summary
        }
        if (transitionTimer >= TRANSITION_OUT_SECONDS + TRANSITION_HOLD_SECONDS + TRANSITION_IN_SECONDS) {
            transitionTarget = -1;
        }
        if (!transitionSwitched || transitionTimer < TRANSITION_OUT_SECONDS + TRANSITION_HOLD_SECONDS) return;
    }

    if (state != GameState::Complete && input.toggleInspector) {
        inspectorOpen = !inspectorOpen;
        audio.play(inspectorOpen ? Sfx::JournalOpen : Sfx::JournalClose);
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
                audio.pauseWorld();
                audio.play(Sfx::CoverClose);
            } else if (input.interact) {
                pageIndex++;
                audio.play(pageIndex >= pages.size() ? Sfx::NoteDismiss : Sfx::Page);
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
                audio.pauseWorld();
                audio.play(Sfx::CoverClose);
            } else {
                if (input.interact) interact();
                if (input.lower) tryPopStone();
                if (state == GameState::Playing) updatePlaying(input, dt); // an interaction may have opened a note
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
    if (transitionTarget >= 0 && !transitionSwitched) {
        return; // walking through an exit: wait for the fade
    }

    float previousBottom = player.position.y + player.size.y;
    bool wasGrounded = player.grounded;
    bool jumping = player.grounded && input.jump;

    int stepBefore = (int)(player.walkPhase / 3.14159f);
    updatePlayerMovement(player, physics, input.move, input.jump, dt);
    const float side = sideOf(player.position.x + player.size.x / 2.0f);
    if (jumping) {
        spawnDust(particles, playerFeet(), 4, dustColor());
        audio.play(Sfx::Jump, 1.0f, 1.0f, side);
    }
    if (player.grounded && (int)(player.walkPhase / 3.14159f) != stepBefore) {
        spawnDust(particles, playerFeet(), 2, dustColor()); // a little dust at each footstep
        audio.playStep(surfaceUnderfoot(), 1.0f, side);     // and its sound, in time with the walk
    }
    const float fallSpeed = player.velocity.y; // before a landing stops it
    if (!player.grounded) {
        // Falling well below the ledge it left (no jump on the route drops more than 90 px): the rush of air.
        float below = player.position.y + player.size.y - ((player.currentNode != nullptr) ? player.currentNode->platformPosition.y : 0.0f);
        if (below > 110.0f && fallSpeed > 0.0f && !fallSoundPlayed && player.currentNode != nullptr) {
            audio.play(Sfx::Fall, 1.0f, 1.0f, side);
            fallSoundPlayed = true;
        }
    }
    const RegionInfo& place = world.getRegion(regionIndex);
    float bottom = player.position.y + player.size.y;
    if (place.waterline > 0.0f && previousBottom < place.waterline && bottom >= place.waterline) { // into the water
        Vector2 splash = {player.position.x + player.size.x / 2.0f, place.waterline};
        spawnBurst(particles, splash, 16, Color{190, 214, 240, 255}, Color{120, 150, 200, 255}, 150.0f, 520.0f, 0.7f);
        audio.play(Sfx::Splash, 1.0f, 1.0f, side);
        fallSoundPlayed = true; // under the water there is no rush of air
    }

    // Keep the player inside the world horizontally.
    if (player.position.x < 0.0f) player.position.x = 0.0f;
    if (player.position.x > WORLD_WIDTH - player.size.x) player.position.x = WORLD_WIDTH - player.size.x;

    resolveLandings(previousBottom);
    if (!wasGrounded && player.grounded) {
        player.landTimer = LAND_POSE_SECONDS;
        spawnDust(particles, playerFeet(), 7, dustColor());
        float impact = std::fmax(0.0f, std::fmin(1.0f, (fallSpeed - 200.0f) / 800.0f));
        audio.playStep(surfaceUnderfoot(), 0.8f + 0.5f * impact, side); // the surface it landed on
        if (impact > 0.3f) audio.play(Sfx::Land, 0.3f + 0.7f * impact, 1.0f, side); // only a real drop thuds
        if (regionIndex == 4) checkObjectives(); // Region 5: set down on the stone under the one just popped
    }
    if (player.grounded) fallSoundPlayed = false;
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
        if (isPlatformSolid(player.currentNode, node, kind, head)) {
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

// Moves player.currentNode along a link. The destination must be exactly current->next, or
// current->previous in a doubly linked list. In a stack the only way up is onto the top: the
// stone just pushed, whose next is the stone the traveler stands on.
void Game::followLink(Node* destination) {
    Node* from = player.currentNode;
    if (from == nullptr || destination == nullptr) {
        return; // never follow NULL, never move from "no node"
    }

    // The destination must be one of the current node's real links.
    const ListKind kind = world.getRegion(regionIndex).kind;
    bool viaNext = kind != ListKind::Stack && from->next == destination; // current = current->next;
    bool viaPrevious = !viaNext && kind == ListKind::Doubly && from->previous == destination; // current = current->previous;
    bool ontoTop = kind == ListKind::Stack && destination == world.getHead(regionIndex) && destination->next == from; // current = top;
    if (!viaNext && !viaPrevious && !ontoTop) {
        return; // not reachable through a permitted link: do not move
    }

    player.currentNode = destination;
    nodeTimer = 0.0f;
    markVisited(destination);
    spawnArrivalEffects(viaPrevious);
    if (ontoTop) {
        checkpointNode = destination; // a fall returns the traveler to the top, the only stone that holds
    } else {
        // Each link has a note in the place's key: rising along next, falling back along previous.
        // (On the stair the notes belong to push and pop instead.)
        audio.playNote(regionIndex, destination->id, viaPrevious, sideOf(destination->platformPosition.x + destination->platformWidth / 2.0f));
    }
    if (!journeyStarted) {
        journeyStarted = true; // the first step: the controls have done their job and fade out
        journeyTimer = 0.0f;
    }

    // Ordinary steps show no code. The first time each kind of link is used, the journal names it.
    if (viaPrevious) {
        showCallout(Callout::FollowPrevious);
    } else if (regionIndex == 2 && destination == loopStartNode) {
        showCallout(Callout::LoopBack); // tail->next is the head
    } else if (!ontoTop) {
        showCallout(Callout::FollowNext);
    }

    if (regionIndex == 2 && !gateOpen) {
        loopSteps++; // only the first trip around counts; the stop condition already fired after that
    }
    if (kind == ListKind::Queue) {
        leaveFrontBoat(from); // the boat stepped off was the front: it is the first to leave (`from` is deleted)
    }
    checkObjectives();
}

// Region 3: entering the tail's hollow. The destination was read from tail->next by the caller.
void Game::startRootPassage(Node* destination) {
    passageDestination = destination;
    passageTimer = 0.0f;
    player.velocity = Vector2{0.0f, 0.0f};
    audio.play(Sfx::Passage, 1.0f, 1.0f, sideOf(player.position.x));
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
                    "Journal question: which link tells you where to go next?",
                    "Answer: next. Each node stores the address of the node after it,\n"
                    "and nothing points back. The tail's next is NULL, so the list ends here.\n"
                    "The arch is open.");
            }
            break;

        case 1:
            if (stage == 0 && current->id == 4) {
                setStage(1); // the key's light now travels with the player
                setCheckpoint(current);
                spawnBurst(particles, Vector2{current->platformPosition.x + current->platformWidth / 2.0f,
                                              current->platformPosition.y - 44.0f},
                           16, Color{214, 176, 96, 255}, COOL_LIGHT, 90.0f, 30.0f, 0.9f);
                showToast("You took the key. Its light stirs in your hands.", 5.0f);
                audio.play(Sfx::Key, 1.0f, 1.0f, sideOf(current->platformPosition.x + current->platformWidth / 2.0f), 0.15f);
            } else if (stage == 1 && current->id == 2) {
                setStage(2); // the light kindles the Old Hall's brazier
                setCheckpoint(current);
                spawnBurst(particles, Vector2{current->platformPosition.x + current->platformWidth - 20.0f,
                                              current->platformPosition.y - 18.0f},
                           18, Color{255, 168, 76, 255}, Color{255, 232, 150, 255}, 80.0f, -20.0f, 1.0f);
                showToast("The brazier blazes up, and the old hall wakes.", 5.0f);
                audio.play(Sfx::Brazier, 1.0f, 1.0f, sideOf(current->platformPosition.x + current->platformWidth - 20.0f), 0.1f);
            } else if (stage == 2 && current->next == nullptr) {
                completeRegionObjective(
                    "Journal question: what does a doubly linked list store\n"
                    "that a singly linked list does not?",
                    "Answer: previous, a link to the node before. That is how you carried\n"
                    "the light back to the Old Hall. At the two ends, head->previous and\n"
                    "tail->next are NULL. The doorway is open.");
            }
            break;

        case 2:
            // The stop condition: we are back at the node we started from.
            if (current == loopStartNode && loopSteps > 0) {
                completeRegionObjective(
                    "Journal question: in a circular list no next is NULL.\n"
                    "So when does a walk around it stop?",
                    TextFormat("Answer: when it is back where it started, current == start.\n"
                               "You stopped after %d steps. A step limit is only a safety net\n"
                               "in case the start is never reached. The moon gate is open.", loopSteps));
            } else if (loopSteps >= MAX_LOOP_STEPS) {
                // Safety fallback: only if the start was never reached.
                loopSteps = 0;
                checkpointNode = loopStartNode;
                restoreToCheckpoint();
                showMessage({TextFormat("The walk went on for %d steps without coming home,\n"
                                        "so the safety limit stopped it. Begin again at the gate.", MAX_LOOP_STEPS)},
                            NoteKind::Recovery);
            }
            break;

        case 3:
            if (stage == 0 && current->id == 3) {
                setStage(1);
                setCheckpoint(current);
                showToast("The bridge is gone. Only wind lies past the edge.", 5.0f);
                audio.play(Sfx::EdgeGust, 1.0f, 1.0f, 0.4f);
            } else if (stage == 2 && current->id == 4) {
                completeRegionObjective(
                    "Journal question: what must you do before following a pointer?",
                    "Answer: check it.   if (current->next != nullptr) { current = current->next; }\n"
                    "nullptr means there is no node: nothing to read, nothing to stand on.\n"
                    "The dawn gate is open.");
            }
            break;

        case 4:
            // Back on the floor stone after the bell: every stone has come off, the last one first.
            if (stage == 1 && current->id == 1 && player.grounded) {
                completeRegionObjective(
                    "Journal question: you set the stones Wave, Shell, Gull, Bell.\n"
                    "In what order did they come off?",
                    "Answer: Bell, Gull, Shell, Wave. The last one set came off first.\n"
                    "A stack adds and removes at one end, the top: last in, first out.\n"
                    "The floor stone is the bottom, and its next is NULL. The sea door is open.");
            }
            break;

        case 5:
            if (current->id == World::BOAT_SLOTS) { // aboard the ship at the end of the line
                completeRegionObjective(
                    "Journal question: which boat was the first to leave the line?",
                    "Answer: the Red Skiff, the first one that came. A queue adds at the rear\n"
                    "and takes from the front: first in, first out. It keeps a pointer to each\n"
                    "end, so neither one has to walk the line. The ship is ready to sail.");
            }
            break;

        default:
            break;
    }
}

// E: what it does depends on the place.
void Game::interact() {
    switch (regionIndex) {
        case 3: tryBuildBridge(); break;
        case 4: tryPushStone(); break;
        case 5: tryCallBoat(); break;
        default: break;
    }
}

// Region 5: E sets the next stair stone on top of the stack with push(). A new stone can only go
// on the top, so the traveler must be standing on it. On the highest stone, E rings the bell.
void Game::tryPushStone() {
    Node* current = player.currentNode;
    Node* top = world.getHead(regionIndex);
    if (current == nullptr || top == nullptr || !player.grounded) {
        return; // not while jumping or dropping
    }
    if (stage == 0 && current == top && current->id == World::STONE_SLOTS) {
        ringBell();
        return;
    }
    if (stage != 0) {
        showToast("The bell has rung. Take the stair down: press S.");
        audio.play(Sfx::Tick);
        return;
    }
    if (current != top) {
        showToast("Climb onto the new stone first. Only the top takes a new one.");
        audio.play(Sfx::Tick);
        return;
    }

    Node* stone = world.pushStone(); // push(): stone->next = top; top = stone;
    if (stone == nullptr) {
        return;
    }
    noteCreated(*stone);
    const float side = sideOf(stone->platformPosition.x + stone->platformWidth / 2.0f);
    audio.play(Sfx::StonePush, 1.0f, 1.0f, side);
    audio.playNote(regionIndex, stone->id, false, side); // each stone has its note, rising with the stair
    spawnBurst(particles, Vector2{stone->platformPosition.x + 6.0f, stone->platformPosition.y + 10.0f}, 10, dustColor(),
               Color{255, 236, 200, 255}, 60.0f, 80.0f, 0.7f); // grit shaken loose from the wall
    showCallout(Callout::Push);
}

// Region 5: the highest stone stands under the fog bell.
void Game::ringBell() {
    setStage(1);
    const Node* current = player.currentNode;
    const float x = current->platformPosition.x + current->platformWidth * 0.72f;
    audio.play(Sfx::Bell, 1.0f, 1.0f, sideOf(x));
    spawnBurst(particles, Vector2{x, current->platformPosition.y - 62.0f}, 18, WARM_LIGHT, WHITE, 90.0f, 10.0f, 1.0f);
    showToast("The bell rings out over the fog. Now take the stair down.", 5.0f);
}

// Region 5: S takes the top stone off the stack with pop(). The traveler is standing on it, so
// they drop onto the stone under it, which is the new top. The floor stone is the bottom: its
// next is NULL, and popping it would leave nothing to stand on.
void Game::tryPopStone() {
    if (regionIndex != 4) {
        return;
    }
    Node* top = world.getHead(regionIndex);
    if (top == nullptr || player.currentNode == nullptr || !player.grounded) {
        return;
    }
    if (top->next == nullptr) {
        showToast("This is the bottom stone. Under it there is only the sea.");
        audio.play(Sfx::StoneStuck, 1.0f, 1.0f, sideOf(top->platformPosition.x + top->platformWidth / 2.0f));
        showCallout(Callout::StackBottom);
        return;
    }

    Node* below = top->next; // read the link before the top is deleted
    const int id = top->id;
    const Rectangle rect = getPlatformRect(*top);
    const bool standingOnTop = player.currentNode == top;
    // Every non-owning pointer leaves the top stone before pop() deletes it.
    if (standingOnTop) player.currentNode = below;
    if (checkpointNode == top) checkpointNode = below;
    world.popStone(); // pop(): Node* old = top; top = top->next; delete old;
    noteRemoved(id, rect);

    const float side = sideOf(rect.x + rect.width / 2.0f);
    audio.play(Sfx::StonePop, 1.0f, 1.0f, side);
    audio.playNote(regionIndex, id, true, side); // the note it rose with, now on the way down
    if (standingOnTop) {
        player.grounded = false; // the stone has slid away: the traveler drops onto the new top
        nodeTimer = 0.0f;
        markVisited(below);
        if (!journeyStarted) {
            journeyStarted = true;
            journeyTimer = 0.0f;
        }
    }
    showCallout(Callout::Pop);
}

// Region 6: E calls the next boat to the end of the line with enqueue(). The queue keeps a pointer
// to its rear, so this works from wherever the traveler stands.
void Game::tryCallBoat() {
    if (!world.boatsRemain()) {
        showToast("The ship is the last in line. No more boats are coming.");
        audio.play(Sfx::Tick);
        return;
    }
    Node* boat = world.enqueueBoat(); // enqueue(): rear->next = boat; rear = boat;
    if (boat == nullptr) {
        return;
    }
    noteCreated(*boat);
    const float side = sideOf(boat->platformPosition.x + boat->platformWidth / 2.0f);
    audio.play(Sfx::BoatArrive, 1.0f, 1.0f, side);
    audio.playNote(regionIndex, boat->id, false, side, BOAT_ARRIVE_SECONDS * 0.7f); // its note as it moors
    showCallout(Callout::Enqueue);
    if (boat->id == World::BOAT_SLOTS) {
        showToast("The ship comes in at the end of the line.", 4.0f);
    }
}

// Region 6: the traveler has stepped off the front boat, so it leaves the line with dequeue().
void Game::leaveFrontBoat(Node* front) {
    if (front == nullptr || front != world.getHead(regionIndex) || front == player.currentNode) {
        return;
    }
    const int id = front->id;
    const Rectangle rect = getPlatformRect(*front);
    if (checkpointNode == front) checkpointNode = player.currentNode; // never keep a pointer to a node about to be deleted
    world.dequeueBoat(); // dequeue(): Node* old = front; front = front->next; delete old;
    noteRemoved(id, rect);

    const float side = sideOf(rect.x + rect.width / 2.0f);
    audio.play(Sfx::BoatLeave, 1.0f, 1.0f, side, 0.25f);
    audio.playNote(regionIndex, id, true, side, 0.55f); // the note it came with, so the tune leaves in the order it came
    showCallout(Callout::Dequeue);
}

// Presentation only: when nodes were created and which were just deleted, by ID.
void Game::noteCreated(const Node& node) {
    if (node.id < 0 || node.id > MAX_NODES_PER_REGION) return;
    createdAt[node.id] = elapsed;
    removedIds[node.id] = RemovedPiece{};
    // A new node comes in already veiled or clear, so it never fades into mist after it arrives.
    const ListKind kind = world.getRegion(regionIndex).kind;
    ledgeMist[node.id] = isPlatformSolid(player.currentNode, &node, kind, world.getHead(regionIndex)) ? 0.0f : 1.0f;
}

void Game::noteRemoved(int id, Rectangle rect) {
    if (id < 0 || id > MAX_NODES_PER_REGION) return;
    removedIds[id] = RemovedPiece{true, rect, elapsed};
}

// Region 4 interaction: the safe check, then append() repairs the missing link.
void Game::tryBuildBridge() {
    if (regionIndex != 3 || stage != 1) {
        return;
    }
    Node* current = player.currentNode;
    if (current == nullptr || current->id != 3 || !player.grounded) {
        showToast("Stand at the cliff's edge to mend the bridge."); // not while jumping or falling past the edge
        audio.play(Sfx::Tick);
        return;
    }
    if (!inspectorOpen) {
        showToast("Look before you build: open your journal (Tab).");
        audio.play(Sfx::Tick);
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
        showCallout(Callout::Append); // no note: the bridge assembling is the answer
        playBridgeAssembly();
        toastTimer = 0.0f;            // "the bridge is gone" no longer holds
    }
}

void Game::recoverFromFall(bool movedBackward) {
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
    const char* place = (player.currentNode != nullptr) ? player.currentNode->label : "path";
    markVisited(player.currentNode);

    std::vector<std::string> recoveryPages;
    if (regionIndex == 0 && movedBackward) {
        recoveryPages.push_back("There is no way back here: each ledge only knows the one after it.");
    } else if (regionIndex == 2 && movedBackward) {
        recoveryPages.push_back("The garden flows one way. To reach an earlier terrace,\n"
                                "keep going around the circle.");
    } else if (regionIndex == 4) {
        recoveryPages.push_back("You slipped off the stair. Only the top stone will hold you;\n"
                                "the stones under it wait their turn.");
    } else if (regionIndex == 5 && movedBackward) {
        recoveryPages.push_back("The boat behind you has gone. In this line the boat at the front\n"
                                "is always the first to leave.");
    } else if (regionIndex == 5) {
        recoveryPages.push_back("You slipped into the harbor. Only the boat ahead of you will hold.");
    } else {
        recoveryPages.push_back("You slipped into the mist. Only the ledges lit for you will hold.");
    }

    if (restartLoop) {
        recoveryPages.push_back("Your lantern brought you back to the gate. The walk begins again.");
    } else {
        recoveryPages.push_back(TextFormat("Your lantern brought you back to the %s.", place));
    }
    showMessage(recoveryPages, NoteKind::Recovery);
}

void Game::recoverFromNullHazard() {
    audio.play(Sfx::Hazard, 1.0f, 1.0f, sideOf(player.position.x)); // a low hush from the depth, never a jolt
    restoreToCheckpoint();
    const char* place = (player.currentNode != nullptr) ? player.currentNode->label : "path";
    showCallout(Callout::NullLink); // the journal's side of it: N3->next is NULL
    showMessage({
        "Nothing holds past the edge. The bridge is gone,\n"
        "and there is no ledge there to land on.",
        TextFormat("Your lantern brought you back to the %s.\n"
                   "Look before you step: open your journal (Tab), then mend the way (E).", place),
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
    fallSoundPlayed = false;
    audio.play(Sfx::Recover, 1.0f, 1.0f, 0.0f, 0.15f); // the light gathers back into the traveler
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
    audio.play(Sfx::Checkpoint, 1.0f, 1.0f, sideOf(node->platformPosition.x + 22.0f), 0.2f); // the lantern catches
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

// How hard the wind blows in this region (for the traveler's scarf; the art has its own).
float Game::regionWind() const {
    if (regionIndex == 3) return world.isBridgeBuilt() ? 0.7f : 1.6f;
    if (regionIndex == 4) return 1.0f; // a sea breeze around the tower
    if (regionIndex == 5) return 0.6f;
    return (regionIndex == 2) ? 0.5f : 0.8f;
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
            float target = isPlatformSolid(player.currentNode, node, kind, head) ? 0.0f : 1.0f;
            ledgeMist[node->id] += (target - ledgeMist[node->id]) * follow;
        }
        node = node->next;
        steps++;
        if (node == head) break; // circular list: back at the start
    }
}

void Game::updateCamera(float dt) {
    // Center on the traveler, leaning toward where the guiding light leads, so the way on comes into view.
    float center = player.position.x + player.size.x / 2.0f;
    float lean = 0.0f;
    const Node* toward = guideNode();
    Rectangle gate;
    if (toward != nullptr) {
        lean = (toward->platformPosition.x + toward->platformWidth / 2.0f - center) * 0.35f;
    } else if (gateOpen && world.getGateRect(regionIndex, gate)) {
        lean = (gate.x + gate.width / 2.0f - center) * 0.35f;
    } else if (regionIndex == 5 && world.boatsRemain() && world.getQueueRear() != nullptr) {
        Rectangle slot = world.getBoatSlot(world.getQueueRear()->id + 1); // at the rear: look out to where the next boat comes in
        lean = (slot.x + slot.width / 2.0f - center) * 0.35f;
    }
    lean = std::fmax(-220.0f, std::fmin(220.0f, lean));
    float desired = center + lean - SCREEN_WIDTH / 2.0f;

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
        float follow = 1.0f - std::exp(-dt * 4.0f); // eases in the same way at any frame rate
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
        case 4: return Color{222, 210, 186, 255}; // sandstone grit
        case 5: return Color{226, 240, 246, 255}; // spray
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
        case 4: first = Color{246, 232, 200, 255}; second = WARM_LIGHT; break;
        case 5: first = Color{236, 248, 255, 255}; second = Color{150, 222, 230, 255}; break;
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
