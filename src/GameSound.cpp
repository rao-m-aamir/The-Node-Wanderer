#include "Game.h"

#include <cmath>

// The game's side of the sound: which music, ambience, and effects fit each place and state.
// Effects are triggered from the event handlers in Game.cpp, once per event; this file never
// reads the list to decide what is reachable. Surfaces are presentation data keyed by node ID,
// like the platform art.

namespace {

const Track REGION_TRACKS[REGION_COUNT] = {Track::Forest, Track::Ruins, Track::Garden, Track::Edge};
const Bed REGION_BEDS[REGION_COUNT] = {Bed::Forest, Bed::Ruins, Bed::Garden, Bed::EdgeWind};
const Sfx ARRIVALS[REGION_COUNT] = {Sfx::Arrive1, Sfx::Arrive2, Sfx::Arrive3, Sfx::Arrive4};

} // namespace

// A new place: its music and ambience fade in under the dark beat that names it, with a short
// motif in its key as the name appears.
void Game::startRegionAudio(int index) {
    if (index < 0 || index >= REGION_COUNT) return;
    audio.playMusic(REGION_TRACKS[index], 0.8f, 4.0f, 0.5f);
    audio.stopBeds(0.6f);
    audio.setBed(REGION_BEDS[index], 1.0f, 2.5f, 0.15f);
    audio.setAccents(index);
    audio.play(ARRIVALS[index], 1.0f, 1.0f, 0.0f, 0.45f);
}

// Every frame: the music and ambience dip under notes, the journal, and the pause screen, and in
// the Edge of Nothing the deep draft of the ravine grows as the traveler nears the broken bridge.
void Game::updateAudioMix(float dt) {
    float music = 1.0f, beds = 1.0f;
    if (state == GameState::Paused) {
        music = 0.35f;
        beds = 0.3f;
        if (audioRowFlash > 0.0f && audioRow == 0) music = 1.0f; // while a level changes, play it at that level
        if (audioRowFlash > 0.0f && audioRow == 1) beds = 1.0f;
    } else if (state == GameState::Message) {
        music = 0.72f;
        beds = 0.85f;
    } else if (inspectorOpen && state == GameState::Playing) {
        music = 0.8f;
        beds = 0.9f;
    }
    audio.setDuck(music, beds);

    if (state != GameState::Paused && regionIndex == 3 && transitionTarget < 0) {
        float level = 0.0f;
        if (!world.isBridgeBuilt()) {
            Rectangle gap = world.getNullHazardZone();
            float center = player.position.x + player.size.x * 0.5f;
            float distance = std::fabs(center - gap.x);
            level = 1.0f - smooth01((distance - 80.0f) / 700.0f);
        }
        audio.setBed(Bed::EdgeGap, level, 0.8f);
    }

    // The note's paper sound plays when the note can be seen (not under the dark beat).
    bool hidden = transitionTarget >= 0 && (!transitionSwitched || transitionTimer < TRANSITION_OUT_SECONDS + TRANSITION_HOLD_SECONDS);
    if (notePending && state == GameState::Message && !hidden) {
        audio.play(Sfx::NoteOpen);
        notePending = false;
    } else if (state != GameState::Message) {
        notePending = false;
    }
    if (audioRowFlash > 0.0f) audioRowFlash -= dt;
}

// The pause screen's sound panel: up and down choose a row, left and right change it.
void Game::updatePauseAudioPanel(const InputFrame& input) {
    if (input.navY != 0) {
        audioRow = (audioRow + input.navY + 3) % 3;
        audio.play(Sfx::Tick, 0.8f);
    }
    const AudioSettings& s = audio.settings();
    if (audioRow == 2) {
        if (input.navX != 0 || input.interact) toggleMute();
        return;
    }
    if (input.navX == 0) return;
    float step = 0.1f * (float)input.navX;
    if (audioRow == 0) {
        audio.setMusicVolume(s.music + step);
    } else {
        audio.setSoundVolume(s.sounds + step);
    }
    if (s.muted) audio.setMuted(false); // turning a level up or down means you want to hear it
    audio.play(Sfx::Tick); // at the new level, so the change can be heard
    audioRowFlash = 1.5f;
}

void Game::toggleMute() {
    bool muted = !audio.settings().muted;
    audio.setMuted(muted);
    if (!muted) audio.play(Sfx::Toggle);
    audioRowFlash = 1.5f;
    if (state != GameState::Paused) showToast(muted ? "Sound off. Press M to turn it back on." : "Sound on.", 3.0f);
}

// Region 4: the bridge is laid from N3's edge in about a second and a half (see drawBridge in
// EdgeArt.cpp), then lanterns light along it, the wind drops, and the morning comes.
void Game::playBridgeAssembly() {
    Rectangle bridge = world.getBridgeRect();
    audio.play(Sfx::Repair, 1.0f, 1.0f, sideOf(bridge.x));
    for (int i = 0; i < 9; i++) { // planks landing one after another, a little brighter as they go
        float x = bridge.x + bridge.width * 0.7f * (float)i / 8.0f;
        audio.play(Sfx::Plank, 0.75f + 0.03f * (float)i, 0.95f + 0.02f * (float)i, sideOf(x), 0.08f + 0.12f * (float)i);
    }
    audio.play(Sfx::Pier, 1.0f, 1.0f, sideOf(bridge.x + bridge.width * 0.5f), 0.35f);
    audio.play(Sfx::Rope, 1.0f, 1.0f, sideOf(bridge.x + bridge.width * 0.6f), 1.15f);
    const float posts[3] = {bridge.x + 4.0f, bridge.x + bridge.width * 0.5f, bridge.x + bridge.width - 52.0f};
    const Sfx lanterns[3] = {Sfx::Lantern1, Sfx::Lantern2, Sfx::Lantern3};
    for (int p = 0; p < 3; p++) audio.play(lanterns[p], 1.0f, 1.0f, sideOf(posts[p]), 1.75f + 0.35f * (float)p);

    audio.playMusic(Track::Dawn, 2.5f, 6.0f, 1.2f);
    audio.setBed(Bed::EdgeWind, 0.5f, 5.0f);
    audio.setBed(Bed::EdgeGap, 0.0f, 2.0f);
    audio.setBed(Bed::EdgeDawn, 1.0f, 6.0f, 2.4f);
    audio.setAccents(3, true);
}

// What the current platform sounds like underfoot, by place and node ID (like the platform art).
Surface Game::surfaceUnderfoot() const {
    const int id = (player.currentNode != nullptr) ? player.currentNode->id : 0;
    switch (regionIndex) {
        case 0: // grove bank, mossy rock, fallen log, old ruin, cliff
            if (id == 3) return Surface::Wood;
            if (id == 4) return Surface::Stone;
            return Surface::Grass;
        case 1: // halls and steps; the Old Hall stands in the water
            return (id == 2) ? Surface::WetStone : Surface::Stone;
        case 2: // the gate terrace, the moon cap, the flower bed, the roots
            if (id == 2) return Surface::Soft;
            if (id == 3) return Surface::Earth;
            if (id == 4) return Surface::Wood;
            return Surface::Stone;
        default: // the lookout, the rope post's rock, the cliff's edge, the new bridge
            if (id == 2) return Surface::Stone;
            if (id == 3) return Surface::Earth;
            if (id == 4) return Surface::Wood;
            return Surface::Grass;
    }
}

float Game::sideOf(float worldX) const {
    float side = (worldX - (camera.target.x + SCREEN_WIDTH * 0.5f)) / (SCREEN_WIDTH * 0.5f) * 0.6f;
    return std::fmax(-0.8f, std::fmin(0.8f, side));
}
