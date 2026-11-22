#include "Game.h"

#include <cmath>

#include "ArtCommon.h"

// Composes one frame of the world. Every region goes through the same steps:
//   light map  ->  canvas: background, platforms, particles, traveler
//              ->  water (from a copy of the canvas, so it reflects everything above it)
//              ->  guiding light, foreground, light map, glows  ->  2x scale with bloom and grade.
// The region art reads the same real nodes that gameplay uses; nothing here creates or links nodes.

// Where the guiding light leads from the current node: next, or previous while the key's light is
// carried back in Region 2. On Region 3's tail the way on is the hollow, which shows itself.
const Node* Game::guideNode() const {
    const Node* current = player.currentNode;
    if (current == nullptr) {
        return nullptr;
    }
    if (regionIndex == 1 && stage == 1 && !gateOpen && current->previous != nullptr) {
        return current->previous;
    }
    const Node* next = current->next; // may be NULL: then there is no way on
    if (regionIndex == 2 && next == world.getHead(regionIndex)) {
        return nullptr;
    }
    return next;
}

SceneView Game::makeSceneView() const {
    const RegionInfo& region = world.getRegion(regionIndex);
    SceneView view = {};
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
    view.gateProgress = gateOpen ? std::fmin(1.0f, gateTimer / GATE_OPEN_SECONDS) : 0.0f;
    view.checkpointFlash = (checkpointPulse.timer > 0.0f) ? checkpointPulse.timer / checkpointPulse.duration : 0.0f;
    view.time = elapsed;
    view.stage = stage;
    view.stageTime = stageTimer;
    view.nodeTime = nodeTimer;
    view.emphasizePrevious = regionIndex == 1 && stage == 1 && !gateOpen;
    view.portalRect = world.getPortalRect();
    view.passage = (passageTimer >= 0.0f) ? std::fmin(1.0f, passageTimer / PASSAGE_SECONDS) : -1.0f;
    view.bridgeBuilt = world.isBridgeBuilt();
    view.bridgeProgress = view.bridgeBuilt ? std::fmin(1.0f, bridgeTimer / BRIDGE_BUILD_SECONDS) : 0.0f;
    view.bridgeTime = view.bridgeBuilt ? bridgeTimer : 0.0f;
    view.hazardZone = world.getNullHazardZone();
    view.playerFeet = Vector2{(float)toCanvasX(player.position.x + player.size.x / 2.0f, view.cameraX),
                              (float)toCanvasY(player.position.y + player.size.y)};
    view.playerSpeed = player.velocity.x;
    view.playerAirborne = !player.grounded;
    bool guiding = state == GameState::Playing && passageTimer < 0.0f && transitionTarget < 0;
    view.guide = guiding ? guideNode() : nullptr;
    view.guideToGate = guiding && view.guide == nullptr && gateOpen;
    bool back = view.guide != nullptr && player.currentNode != nullptr && view.guide == player.currentNode->previous;
    view.guideColor = back ? COOL_LIGHT : WARM_LIGHT;
    return view;
}

// The traveler's lantern lights whatever is near them.
void Game::addTravelerLights(const SceneView& view, std::vector<Light>& lights) const {
    if (passageTimer >= 0.0f) {
        return; // inside the current, the traveler is the moving light the region draws
    }
    float flicker = 0.9f + 0.1f * std::sin(elapsed * 13.0f) * std::sin(elapsed * 7.0f);
    Vector2 lantern = {view.playerFeet.x + player.facing * 5.5f + lanternSwing(player), view.playerFeet.y - 6.0f};
    lights.push_back(Light{lantern, 66.0f, Color{255, 214, 150, 255}, 0.5f * flicker, 7.0f, 0.34f * flicker});
}

void Game::drawScene() const {
    SceneView view = makeSceneView();
    Mood mood = regionMood(view);
    std::vector<Light> lights;
    lights.reserve(48);
    addRegionLights(view, regionArt, lights);
    addTravelerLights(view, lights);
    renderer.renderLightMap(lights, mood);

    const bool travelerVisible = passageTimer < 0.0f;
    BeginTextureMode(renderer.canvas);
    ClearBackground(BLACK);
    drawRegionBackground(view, regionArt);
    drawRegionPlatforms(view, regionArt);
    drawParticles(particles, view.cameraX);
    if (travelerVisible) drawTraveler(player, elapsed, view.playerFeet, 1.0f);
    EndTextureMode();

    if (regionArt.waterline >= 0) { // the water reflects everything drawn so far
        renderer.copyCanvasToReflection();
        BeginTextureMode(renderer.canvas);
        drawRegionWater(view, regionArt, renderer.reflection.texture);
        EndTextureMode();
    }

    BeginTextureMode(renderer.canvas);
    if (travelerVisible) drawGuideStream(view);
    drawRegionForeground(view, regionArt);
    renderer.applyLightMap();
    BeginBlendMode(BLEND_ADDITIVE);
    drawRegionAdditive(view, regionArt, renderer);
    drawParticleGlows(particles, view.cameraX, renderer);
    EndBlendMode();
    renderer.drawGlows(lights);
    EndTextureMode();

    renderer.present(mood, SCREEN_WIDTH, SCREEN_HEIGHT);
}
