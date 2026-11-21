#include "Game.h"

#include <cmath>

#include "ArtCommon.h"
#include "rlgl.h"

// Draws the world: every region goes through the same pixel canvas in the same order
// (background, platforms, effects, traveler, foreground). The region art reads the
// same real nodes that gameplay uses; nothing here creates or links nodes.

SceneView Game::makeSceneView() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    SceneView view;
    view.region = regionIndex;
    view.cameraX = (int)std::floor(camera.target.x / CANVAS_SCALE);
    view.scroll = view.cameraX - (int)std::floor(region.left / CANVAS_SCALE);
    view.head = world.getHead(regionIndex);
    view.current = player.currentNode;
    view.checkpoint = checkpointNode;
    view.mistById = ledgeMist;
    view.kind = region.kind;
    view.hasGate = world.getGateRect(regionIndex, view.gateRect);
    view.gateOpen = gateOpen;
    view.checkpointFlash = (checkpointPulse.timer > 0.0f) ? checkpointPulse.timer / checkpointPulse.duration : 0.0f;
    view.time = elapsed;
    view.stage = stage;
    view.emphasizePrevious = regionIndex == 1 && stage == 1 && !gateOpen;
    view.portalRect = world.getPortalRect();
    view.passage = (passageTimer >= 0.0f) ? std::fmin(1.0f, passageTimer / PASSAGE_SECONDS) : -1.0f;
    view.bridgeBuilt = world.isBridgeBuilt();
    view.bridgeProgress = view.bridgeBuilt ? std::fmin(1.0f, bridgeTimer / BRIDGE_BUILD_SECONDS) : 0.0f;
    view.hazardZone = world.getNullHazardZone();
    view.playerFeet = Vector2{(float)toCanvasX(player.position.x + player.size.x / 2.0f, view.cameraX),
                              (float)toCanvasY(player.position.y + player.size.y)};
    view.gateProgress = gateOpen ? std::fmin(1.0f, gateTimer / GATE_OPEN_SECONDS) : 0.0f;
    view.stageTime = stageTimer;
    view.nodeTime = nodeTimer;
    return view;
}

void Game::drawScene() const {
    SceneView view = makeSceneView();

    BeginTextureMode(sceneCanvas);
    ClearBackground(BLACK);
    drawRegionBackground(view);
    drawRegionPlatforms(view);
    drawParticles(particles, view.cameraX);
    if (passageTimer < 0.0f) { // inside the root, the traveler is the moving light instead
        drawTraveler(player, elapsed, view.playerFeet, 1.0f);
        drawWispHint(view);
    }
    drawRegionForeground(view);
    EndTextureMode();

    // Scale the canvas up 2x. Blending is off for this copy: the canvas is a finished
    // opaque picture, and its alpha channel is not meant to let the background through.
    rlDrawRenderBatchActive();
    rlDisableColorBlend();
    Rectangle source = {0.0f, 0.0f, (float)CANVAS_WIDTH, -(float)CANVAS_HEIGHT}; // render textures are stored upside down
    Rectangle dest = {0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT};
    DrawTexturePro(sceneCanvas.texture, source, dest, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
    rlDrawRenderBatchActive();
    rlEnableColorBlend();
}

// A small light near the traveler drifts toward the platform a link points to, then back.
// Warm for next. In Region 2, while carrying the light back, it is cool and points to previous.
// At a tail (next == NULL) it points to the open exit instead, if there is one.
void Game::drawWispHint(const SceneView& view) const {
    if (state != GameState::Playing) {
        return;
    }
    const Node* current = player.currentNode;
    if (current == nullptr) {
        return;
    }
    Vector2 target;
    Color color = WARM_LIGHT;
    const Node* toward = current->next; // checked for NULL below
    if (view.emphasizePrevious && current->previous != nullptr) {
        toward = current->previous;
        color = COOL_LIGHT;
    }
    if (regionIndex == 2 && toward == view.head && toward != current) {
        return; // the tail's way back is the root hollow, which already glows
    }
    if (toward != nullptr) {
        target = Vector2{(float)toCanvasX(toward->platformPosition.x + toward->platformWidth / 2.0f, view.cameraX),
                         (float)toCanvasY(toward->platformPosition.y) - 8.0f};
    } else if (gateOpen && view.hasGate) {
        target = Vector2{(float)toCanvasX(view.gateRect.x + view.gateRect.width / 2.0f, view.cameraX),
                         (float)toCanvasY(view.gateRect.y + view.gateRect.height / 2.0f)};
    } else {
        return; // nowhere to point
    }

    Vector2 start = {view.playerFeet.x + player.facing * 7.0f, view.playerFeet.y - 30.0f};
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
        drawWisp(point, (i == 0) ? 1.0f : 0.35f / i, color, elapsed);
    }
}
