#include "Game.h"

#include <cmath>

#include "rlgl.h"
#include "Scenery.h"
#include "Theme.h"

// Which renderer draws the active region. Region 1 uses the natural forest art,
// drawn into a small pixel canvas; Regions 2-4 still use the classic look.
// Both read the same real nodes: nothing here creates or links nodes.

void Game::drawForestScene() const {
    const int cameraX = (int)std::floor(camera.target.x / CANVAS_SCALE);

    BeginTextureMode(forestCanvas);
    ClearBackground(BLACK);
    drawForestBackground(cameraX, elapsed);

    ForestView view;
    view.cameraX = cameraX;
    view.head = world.getHead(0);
    view.current = player.currentNode;
    view.checkpoint = checkpointNode;
    view.mistById = ledgeMist;
    view.hasGate = world.getGateRect(0, view.gateRect);
    view.gateOpen = gateOpen;
    view.checkpointFlash = (checkpointPulse.timer > 0.0f) ? checkpointPulse.timer / checkpointPulse.duration : 0.0f;
    view.time = elapsed;
    drawForestLedges(view);

    // Leaves and light where the player just arrived.
    if (arrivalPulse.timer > 0.0f && arrivalPulse.duration > 0.0f) {
        Vector2 at = {(float)toCanvasX(arrivalPulse.position.x, cameraX), (float)toCanvasY(arrivalPulse.position.y)};
        drawArrivalBurst(at, 1.0f - arrivalPulse.timer / arrivalPulse.duration);
    }

    Vector2 feet = {(float)toCanvasX(player.position.x + player.size.x / 2.0f, cameraX),
                    (float)toCanvasY(player.position.y + player.size.y)};
    drawTraveler(player, elapsed, feet, 1.0f);
    drawWispHint(cameraX, feet);
    drawForestForeground(cameraX, elapsed);
    EndTextureMode();

    // Scale the canvas up 2x. Blending is off for this copy: the canvas is a finished
    // opaque picture, and its alpha channel is not meant to let the background through.
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    Rectangle source = {0.0f, 0.0f, (float)CANVAS_WIDTH, -(float)CANVAS_HEIGHT}; // render textures are stored upside down
    Rectangle dest = {0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};
    DrawTexturePro(forestCanvas.texture, source, dest, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
}

void Game::drawClassicScene() const {
    drawBackdrop(world, camera.target.x, elapsed); // decoration, screen space

    BeginMode2D(camera);
    WorldView view;
    view.activeRegion = regionIndex;
    view.currentNode = player.currentNode;
    view.checkpointNode = checkpointNode;
    view.visitedIds = visitedIds;
    view.gateOpen = gateOpen;
    view.showAllLinks = inspectorOpen;
    view.emphasizePrevious = regionIndex == 1 && stage == 1 && !gateOpen;
    view.time = elapsed;
    world.draw(view); // platforms and links, read from the real lists

    drawPulse(checkpointPulse);
    drawPulse(arrivalPulse);
    // The same traveler sprite as Region 1, with each sprite pixel 2 world pixels wide.
    Vector2 feet = {std::floor((player.position.x + player.size.x / 2.0f) / 2.0f) * 2.0f,
                    std::floor((player.position.y + player.size.y) / 2.0f) * 2.0f};
    drawTraveler(player, elapsed, feet, 2.0f);
    drawInteractionPrompt();
    EndMode2D();

    drawForeground(world, camera.target.x, elapsed);
}

// A firefly near the traveler drifts toward the ledge current->next points to, then back.
// When the tail is reached (next == NULL) it points at the open exit instead.
void Game::drawWispHint(int cameraX, Vector2 feet) const {
    if (state != GameState::Playing) {
        return;
    }
    const Node* current = player.currentNode;
    Vector2 target;
    if (current != nullptr && current->next != nullptr) { // null check before following the link
        const Node* next = current->next;
        target = Vector2{(float)toCanvasX(next->platformPosition.x + next->platformWidth / 2.0f, cameraX),
                         (float)toCanvasY(next->platformPosition.y) - 8.0f};
    } else if (gateOpen) {
        Rectangle gate;
        if (!world.getGateRect(regionIndex, gate)) return;
        target = Vector2{(float)toCanvasX(gate.x + gate.width / 2.0f, cameraX), (float)toCanvasY(gate.y + gate.height / 2.0f)};
    } else {
        return; // nowhere to point
    }

    Vector2 start = {feet.x + player.facing * 7.0f, feet.y - 30.0f};
    float dx = target.x - start.x;
    float dy = target.y - start.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance < 1.0f) return;
    float reach = std::fmin(distance * 0.8f, 64.0f); // stays a hint near the player
    for (int i = 2; i >= 0; i--) {                    // two faint trail points, then the wisp
        float t = elapsed - i * 0.08f;
        float travel = 0.5f - 0.5f * std::cos(t * 1.6f);
        Vector2 point = {start.x + dx / distance * reach * travel,
                         start.y + dy / distance * reach * travel + std::sin(t * 4.0f) * 2.0f};
        drawForestWisp(point, (i == 0) ? 1.0f : 0.35f / i, elapsed);
    }
}

// Top-right HUD: the active list as small pips, walked from head through the real next links.
// Current node = gold, visited = filled, not visited yet = hollow. The end shows NULL or the loop.
void Game::drawProgressPill() const {
    const RegionTheme& theme = REGION_THEMES[regionIndex];
    Node* head = world.getHead(regionIndex);

    int count = 0;
    bool loops = false;
    Node* node = head;
    while (node != nullptr && count < MAX_NODES_PER_REGION) {
        count++;
        node = node->next;
        if (node == head) {
            loops = true; // circular list: the walk stops when it is back at the head
            break;
        }
    }

    const float pipGap = 18.0f;
    const char* endLabel = loops ? "BACK TO HEAD" : "NULL";
    const char* steps = TextFormat("STEPS %d / MAX %d   STOP WHEN current == N1", loopSteps, MAX_LOOP_STEPS);
    float width = 28.0f + MeasureText("HEAD", 10) + 10.0f + count * pipGap + 4.0f + MeasureText(endLabel, 10);
    if (regionIndex == 2 && width < MeasureText(steps, 10) + 28.0f) width = MeasureText(steps, 10) + 28.0f;
    float height = (regionIndex == 2) ? 46.0f : 30.0f;
    Rectangle card = {SCREEN_WIDTH - 12.0f - width, 10.0f, width, height};
    DrawRectangleRounded(card, 0.3f, 6, Fade(UI_NAVY, 0.78f));

    float x = card.x + 14.0f;
    float y = card.y + 15.0f;
    DrawText("HEAD", (int)x, (int)y - 4, 10, UI_DIM_TEXT);
    x += MeasureText("HEAD", 10) + 10.0f;

    node = head;
    for (int i = 0; i < count && node != nullptr; i++) {
        Vector2 pip = {x + i * pipGap + 4.0f, y};
        if (i + 1 < count) { // the link to the next node
            DrawLineEx(Vector2{pip.x + 6.0f, y}, Vector2{pip.x + pipGap - 6.0f, y}, 1.0f, Fade(UI_DIM_TEXT, 0.6f));
        }
        bool visited = node->id >= 0 && node->id <= MAX_NODES_PER_REGION && visitedIds[node->id];
        if (node == player.currentNode) {
            DrawCircleV(pip, 5.0f, CURRENT_GOLD);
            DrawRing(pip, 6.5f, 7.5f, 0.0f, 360.0f, 20, Fade(CURRENT_GOLD, 0.6f));
        } else if (visited) {
            DrawCircleV(pip, 3.5f, theme.accent);
        } else {
            DrawRing(pip, 2.5f, 4.0f, 0.0f, 360.0f, 16, Fade(UI_DIM_TEXT, 0.9f));
        }
        node = node->next;
    }
    x += count * pipGap;
    DrawText(endLabel, (int)x, (int)y - 4, 10, loops ? theme.accent : NULL_AMBER);

    if (regionIndex == 2) {
        DrawText(steps, (int)card.x + 14, (int)card.y + 30, 10, UI_DIM_TEXT);
    }
}
