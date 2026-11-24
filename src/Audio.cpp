#include "Audio.h"

#include <atomic>
#include <cmath>

// The files are made by tools/audio/build_audio.py; their sources and licenses are listed in
// assets/audio/CREDITS.md. An effect may have variants: base.ogg, or base_1.ogg, base_2.ogg, ...

namespace {

const char* const TRACK_FILES[(int)Track::Count] = {
    "music/forest.ogg", "music/ruins.ogg", "music/garden.ogg", "music/edge.ogg", "music/dawn.ogg", "music/finale.ogg",
};

const char* const BED_FILES[(int)Bed::Count] = {
    "ambience/forest.ogg", "ambience/ruins.ogg", "ambience/garden.ogg",
    "ambience/edge_wind.ogg", "ambience/edge_gap.ogg", "ambience/edge_dawn.ogg",
};
const float BED_GAIN[(int)Bed::Count] = {1.26f, 1.33f, 1.2f, 1.12f, 1.4f, 1.2f}; // about 8 dB under the music

struct EffectInfo {
    const char* name;  // file base under assets/audio, and the name in debug logs
    float gain;        // its place in the mix
    float cooldown;    // seconds before it may start again (stops double triggers)
    bool ui;           // keeps playing while the world is paused
    int priority;      // 0 = dropped first when many sounds play at once
};

// Gains were set from measurements of the running game (each cue alone, as momentary loudness)
// against the bed of music and ambience at about -23.5 LUFS: the rewards of a place (key, brazier,
// gates, the repair) reach the bed's level, link notes sit a little under it, interface sounds
// lower, and footsteps about 9 dB under it.
const EffectInfo EFFECTS[(int)Sfx::Count] = {
    {"sfx/move/jump", 0.95f, 0.08f, false, 1},
    {"sfx/move/land", 0.9f, 0.06f, false, 1},
    {"sfx/move/splash", 1.05f, 0.25f, false, 2},
    {"sfx/move/fall", 1.05f, 0.6f, false, 2},
    {"sfx/move/recover", 1.2f, 0.6f, false, 3},
    {"sfx/world/passage", 1.3f, 0.6f, false, 3},
    {"sfx/world/key", 1.25f, 0.6f, false, 3},
    {"sfx/world/brazier", 1.2f, 0.6f, false, 3},
    {"sfx/world/checkpoint", 0.9f, 0.3f, false, 2},
    {"sfx/world/exit", 1.1f, 0.6f, false, 3},
    {"sfx/world/complete", 1.25f, 1.0f, false, 3},
    {"sfx/world/gate_1", 1.25f, 0.6f, false, 3},
    {"sfx/world/gate_2", 1.25f, 0.6f, false, 3},
    {"sfx/world/gate_3", 1.25f, 0.6f, false, 3},
    {"sfx/world/gate_4", 1.25f, 0.6f, false, 3},
    {"sfx/world/arrive_1", 0.9f, 0.6f, false, 3},
    {"sfx/world/arrive_2", 0.9f, 0.6f, false, 3},
    {"sfx/world/arrive_3", 0.9f, 0.6f, false, 3},
    {"sfx/world/arrive_4", 0.9f, 0.6f, false, 3},
    {"sfx/world/edge_gust", 1.0f, 0.6f, false, 3},
    {"sfx/world/hazard", 1.15f, 0.6f, false, 3},
    {"sfx/world/repair", 1.4f, 0.6f, false, 3},
    {"sfx/world/plank", 0.9f, 0.03f, false, 2},
    {"sfx/world/pier", 1.05f, 0.3f, false, 2},
    {"sfx/world/rope", 0.9f, 0.3f, false, 2},
    {"sfx/world/lantern_1", 1.1f, 0.2f, false, 3},
    {"sfx/world/lantern_2", 1.1f, 0.2f, false, 3},
    {"sfx/world/lantern_3", 1.1f, 0.2f, false, 3},
    {"sfx/journal/open", 1.05f, 0.05f, true, 3},
    {"sfx/journal/close", 1.05f, 0.05f, true, 3},
    {"sfx/journal/page", 1.05f, 0.05f, true, 3},
    {"sfx/journal/note", 1.05f, 0.2f, true, 2},
    {"sfx/journal/dismiss", 1.2f, 0.05f, true, 3},
    {"sfx/journal/callout", 1.4f, 0.3f, true, 2},
    {"sfx/journal/cover_open", 1.1f, 0.05f, true, 3},
    {"sfx/journal/cover_close", 1.0f, 0.05f, true, 3},
    {"sfx/journal/tick", 1.0f, 0.03f, true, 3},
    {"sfx/journal/toggle", 1.0f, 0.05f, true, 3},
    {"sfx/accent/drip", 1.25f, 0.5f, false, 0},
    {"sfx/accent/chime", 1.25f, 1.0f, false, 0},
    {"sfx/accent/creak", 1.0f, 1.0f, false, 0},
    {"sfx/accent/pebbles", 1.25f, 1.0f, false, 0},
};
const char* const STEP_FILES[(int)Surface::Count] = {
    "sfx/step/grass", "sfx/step/earth", "sfx/step/stone", "sfx/step/wet", "sfx/step/wood", "sfx/step/soft",
};
const float STEP_GAIN = 1.0f;
const float SURFACE_GAIN[(int)Surface::Count] = {0.99f, 1.11f, 1.05f, 0.99f, 1.0f, 1.2f}; // grass .. soft: about -33 LUFS
const float NOTE_GAIN = 1.35f;
const int MAX_VOICES = 16;
// raylib pans with an equal-power law, so a centered sound plays at 0.69 in each speaker. This
// brings the center back to unity; the mix is balanced around it.
const float PAN_LAW = 1.0f / 0.6875f;

// The last stage of the mix, on the audio thread: a master gain that glides (so muting and
// closing never click), then a soft limiter (peaks above the knee are rounded off, not clipped).
std::atomic<float> g_masterTarget{1.0f};
float g_masterGain = 1.0f; // touched only by the audio thread while attached

void finalStage(void* buffer, unsigned int frames) {
    float* samples = (float*)buffer; // the mix is always stereo float
    const float target = g_masterTarget.load(std::memory_order_relaxed);
    const float step = 1.0f / 3000.0f; // a full swing in about 60 ms
    const float knee = 0.7f;
    float gain = g_masterGain;
    for (unsigned int i = 0; i < frames; i++) {
        if (gain != target) gain = (gain < target) ? std::fmin(target, gain + step) : std::fmax(target, gain - step);
        for (unsigned int c = 0; c < 2; c++) {
            float x = samples[2 * i + c] * gain;
            float magnitude = std::fabs(x);
            if (magnitude > knee) x = std::copysign(knee + (1.0f - knee) * std::tanh((magnitude - knee) / (1.0f - knee)), x);
            samples[2 * i + c] = x;
        }
    }
    g_masterGain = gain;
}

std::string findAudioFolder() {
    const std::string app = GetApplicationDirectory(); // ends with a separator
    const std::string candidates[] = {
        app + "assets/audio/",                                 // a packaged folder, or the build folder (copied there)
        std::string(GetWorkingDirectory()) + "/assets/audio/", // run from the repository
        app + "../assets/audio/",                              // the build folder, before the copy
    };
    for (const std::string& path : candidates) {
        if (DirectoryExists(path.c_str())) return path;
    }
    return "";
}

float smoothFade(float t) {
    t = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

// raylib's pan is 0.5 at the center and 1.0 at the far left.
float panFor(float side) {
    side = (side < -1.0f) ? -1.0f : ((side > 1.0f) ? 1.0f : side);
    return 0.5f - 0.5f * side;
}

int loadedFiles = 0;
int missingFiles = 0;

void reportMissing(const std::string& path) {
    missingFiles++;
    if (missingFiles <= 8) TraceLog(LOG_WARNING, "AUDIO: could not load %s", path.c_str());
}

} // namespace

GameAudio::~GameAudio() {
    unload();
}

void GameAudio::load() {
    if (ready) return;
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "AUDIO: no audio device, so the game plays without sound");
        return;
    }
    root = findAudioFolder();
    if (root.empty()) {
        TraceLog(LOG_WARNING, "AUDIO: no assets/audio folder next to the game or in the working folder, "
                              "so the game plays without sound");
        return;
    }

    const double startedAt = GetTime();
    loadedFiles = 0;
    missingFiles = 0;
    SetTraceLogLevel(LOG_WARNING); // raylib reports every file it loads; keep the console readable
    // Large stream buffers (0.34 s each) ride out the moment a place is painted when it is entered.
    SetAudioStreamBufferSizeDefault(16384);
    for (int i = 0; i < (int)Track::Count; i++) loadStream(tracks[i], TRACK_FILES[i]);
    for (int i = 0; i < (int)Bed::Count; i++) loadStream(beds[i], BED_FILES[i]);
    SetAudioStreamBufferSizeDefault(0);
    // Each effect gets a second voice (an alias sharing its samples), so a sound that is still
    // ringing is not cut off when it plays again; notes ring longest and get two.
    for (int i = 0; i < (int)Sfx::Count; i++) loadEffect(effects[i], EFFECTS[i].name, 1);
    for (int i = 0; i < (int)Surface::Count; i++) loadEffect(steps[i], STEP_FILES[i], 1);
    for (int region = 1; region <= 4; region++) {
        for (int id = 1; id <= 5; id++) {
            std::string base = TextFormat("sfx/notes/r%d_n%d", region, id);
            if (FileExists((root + base + ".ogg").c_str())) loadEffect(notes[region - 1][id], base, 2);
        }
    }
    for (int id = 1; id <= 5; id++) {
        std::string base = TextFormat("sfx/notes/r2_back_n%d", id);
        if (FileExists((root + base + ".ogg").c_str())) loadEffect(backNotes[id], base, 2);
    }
    SetTraceLogLevel(LOG_INFO);
    if (missingFiles > 8) TraceLog(LOG_WARNING, "AUDIO: ... and %d more files could not be loaded", missingFiles - 8);
    TraceLog(LOG_INFO, "AUDIO: loaded %d files from %s in %.2f s", loadedFiles, root.c_str(), GetTime() - startedAt);

    g_masterTarget.store(config.muted ? 0.0f : 1.0f);
    g_masterGain = config.muted ? 0.0f : 1.0f; // not attached yet: safe to set from here
    AttachAudioMixedProcessor(finalStage);
    attached = true;
    ready = true;
}

void GameAudio::loadStream(Stream& s, const char* file) {
    s.name = file;
    std::string path = root + file;
    if (FileExists(path.c_str())) s.music = LoadMusicStream(path.c_str());
    s.valid = IsMusicValid(s.music);
    if (!s.valid) {
        reportMissing(path);
        return;
    }
    s.music.looping = true;
    loadedFiles++;
}

void GameAudio::loadEffect(Effect& effect, const std::string& base, int aliases) {
    std::vector<std::string> files;
    const std::string single = root + base + ".ogg";
    if (FileExists(single.c_str())) {
        files.push_back(single);
    } else {
        for (int i = 1; i <= 8; i++) {
            std::string path = root + base + TextFormat("_%d.ogg", i);
            if (!FileExists(path.c_str())) break;
            files.push_back(path);
        }
    }
    if (files.empty()) {
        reportMissing(single);
        return;
    }
    for (const std::string& path : files) {
        Sound sound = LoadSound(path.c_str());
        if (!IsSoundValid(sound)) {
            reportMissing(path);
            continue;
        }
        loadedFiles++;
        effect.sources.push_back(sound);
        std::vector<Sound> copies = {sound};
        for (int k = 0; k < aliases; k++) copies.push_back(LoadSoundAlias(sound));
        effect.copies.push_back(copies);
    }
}

void GameAudio::unloadEffect(Effect& effect) {
    for (std::vector<Sound>& copies : effect.copies) {
        for (size_t k = 1; k < copies.size(); k++) UnloadSoundAlias(copies[k]); // aliases first: they borrow the samples
    }
    for (Sound& sound : effect.sources) UnloadSound(sound);
    effect = Effect{};
}

void GameAudio::unload() {
    if (attached) {
        g_masterTarget.store(0.0f);
        WaitTime(0.08); // the final stage glides to silence first, so stopping cannot click
    }
    for (Stream& s : tracks) {
        if (s.valid) {
            StopMusicStream(s.music);
            UnloadMusicStream(s.music);
        }
        s = Stream{};
    }
    for (Stream& s : beds) {
        if (s.valid) {
            StopMusicStream(s.music);
            UnloadMusicStream(s.music);
        }
        s = Stream{};
    }
    for (Effect& e : effects) unloadEffect(e);
    for (Effect& e : steps) unloadEffect(e);
    for (auto& region : notes) {
        for (Effect& e : region) unloadEffect(e);
    }
    for (Effect& e : backNotes) unloadEffect(e);
    if (attached) {
        DetachAudioMixedProcessor(finalStage);
        attached = false;
    }
    g_masterTarget.store(1.0f);
    g_masterGain = 1.0f; // detached: the audio thread no longer touches it
    voices.clear();
    pending.clear();
    accents.clear();
    ready = false;
}

// ---------------------------------------------------------------------------
// Every frame
// ---------------------------------------------------------------------------

void GameAudio::update(float dt, bool worldRunning) {
    if (!ready) return;
    clock += dt;
    const float follow = 1.0f - std::exp(-dt / 0.25f);
    duckMusic += (duckMusicTarget - duckMusic) * follow;
    duckBeds += (duckBedsTarget - duckBeds) * follow;
    for (Stream& s : tracks) updateStream(s, dt, config.music * duckMusic);
    for (int i = 0; i < (int)Bed::Count; i++) updateStream(beds[i], dt, BED_GAIN[i] * config.sounds * duckBeds);

    // The world's sounds glide down before they pause, and back up when they resume (0.12 s).
    const float glide = dt * 8.0f;
    worldGain = (worldGain < worldGainTarget) ? std::fmin(worldGainTarget, worldGain + glide)
                                              : std::fmax(worldGainTarget, worldGain - glide);

    if (worldRunning) {
        for (size_t i = 0; i < pending.size();) { // scheduled effects (bridge planks, gate chords...)
            pending[i].wait -= dt;
            if (pending[i].wait <= 0.0f) {
                Pending p = pending[i];
                pending[i] = pending.back();
                pending.pop_back();
                play(p.sfx, p.gain, p.pitch, p.side);
            } else {
                i++;
            }
        }
        for (Accent& a : accents) { // the sounds of the place, at random
            a.next -= dt;
            if (a.next <= 0.0f) {
                play(a.sfx, randomRange(a.gainLow, a.gainHigh), randomRange(a.pitchLow, a.pitchHigh), randomRange(-0.8f, 0.8f));
                a.next = randomRange(a.minGap, a.maxGap);
            }
        }
    }

    for (size_t i = 0; i < voices.size();) {
        Voice& v = voices[i];
        bool playing = IsSoundPlaying(v.sound);
        if (!playing && !v.paused) { // finished
            voices[i] = voices.back();
            voices.pop_back();
            continue;
        }
        if (playing && !v.ui && worldGainTarget <= 0.0f && worldGain <= 0.0f) { // dipped all the way: wait
            PauseSound(v.sound);
            v.paused = true;
        }
        float volume = voiceVolume(v);
        if (std::fabs(volume - v.applied) > 0.0005f) {
            SetSoundVolume(v.sound, volume);
            v.applied = volume;
        }
        i++;
    }
}

void GameAudio::updateStream(Stream& s, float dt, float volume) {
    if (!s.valid) return;
    if (s.delay > 0.0f) {
        s.delay -= dt;
    } else if (s.gain != s.target) {
        float step = s.rate * dt;
        s.gain = (s.gain < s.target) ? std::fmin(s.target, s.gain + step) : std::fmax(s.target, s.gain - step);
    }
    if (s.gain > 0.0f && !s.playing) {
        SetMusicVolume(s.music, 0.0f);
        PlayMusicStream(s.music); // from the start of its loop, from silence
        s.playing = true;
        TraceLog(LOG_DEBUG, "AUDIO stream start %s", s.name);
    }
    if (!s.playing) return;
    SetMusicVolume(s.music, smoothFade(s.gain) * volume * PAN_LAW);
    UpdateMusicStream(s.music);
    if (s.gain <= 0.0f && s.target <= 0.0f && s.delay <= 0.0f) { // faded all the way out: stop and rewind
        StopMusicStream(s.music);
        s.playing = false;
        TraceLog(LOG_DEBUG, "AUDIO stream stop %s", s.name);
    }
}

// ---------------------------------------------------------------------------
// Streams
// ---------------------------------------------------------------------------

void GameAudio::playMusic(Track track, float fadeOut, float fadeIn, float delay) {
    if (!ready) return;
    for (int i = 0; i < (int)Track::Count; i++) {
        Stream& s = tracks[i];
        if (i == (int)track) {
            if (s.target >= 1.0f && (s.playing || s.delay > 0.0f)) continue; // already on its way in
            s.target = 1.0f;
            s.rate = 1.0f / std::fmax(0.05f, fadeIn);
            s.delay = s.playing ? 0.0f : delay;
        } else if (s.target > 0.0f || s.playing) {
            s.target = 0.0f;
            s.rate = 1.0f / std::fmax(0.05f, fadeOut);
            s.delay = 0.0f;
        }
    }
}

void GameAudio::stopMusic(float fadeOut) {
    for (Stream& s : tracks) {
        s.target = 0.0f;
        s.rate = 1.0f / std::fmax(0.05f, fadeOut);
        s.delay = 0.0f;
    }
}

void GameAudio::setBed(Bed bed, float level, float fade, float delay) {
    Stream& s = beds[(int)bed];
    if (s.target == level && s.delay <= 0.0f) return;
    s.target = level;
    s.rate = std::fabs(level - s.gain) / std::fmax(0.05f, fade);
    if (s.rate <= 0.0f) s.rate = 1.0f;
    s.delay = (level > 0.0f && !s.playing) ? delay : 0.0f;
}

void GameAudio::stopBeds(float fadeOut) {
    for (Stream& s : beds) {
        s.target = 0.0f;
        s.rate = 1.0f / std::fmax(0.05f, fadeOut);
        s.delay = 0.0f;
    }
}

void GameAudio::setDuck(float music, float bedLevel) {
    duckMusicTarget = music;
    duckBedsTarget = bedLevel;
}

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

float GameAudio::voiceVolume(const Voice& v) const {
    return v.gain * config.sounds * (v.ui ? 1.0f : worldGain) * PAN_LAW;
}

bool GameAudio::inUse(const Sound& sound) const {
    if (IsSoundPlaying(sound)) return true;
    for (const Voice& v : voices) {
        if (v.paused && v.sound.stream.buffer == sound.stream.buffer) return true;
    }
    return false;
}

void GameAudio::startVoice(Sound sound, float gain, float pitch, float side, bool ui) {
    for (size_t i = 0; i < voices.size(); i++) { // one entry per playing copy
        if (voices[i].sound.stream.buffer == sound.stream.buffer) {
            voices[i] = voices.back();
            voices.pop_back();
            break;
        }
    }
    Voice v = {sound, gain, clock, ui, false, 0.0f};
    v.applied = voiceVolume(v);
    SetSoundVolume(sound, v.applied);
    SetSoundPitch(sound, pitch);
    SetSoundPan(sound, panFor(side));
    PlaySound(sound);
    voices.push_back(v);
}

bool GameAudio::start(Effect& effect, float gain, float pitch, float side, bool ui, const char* name) {
    const int count = (int)effect.sources.size();
    if (count == 0) return false;
    int choice = 0;
    if (count > 1) { // a different variant from last time
        if (effect.last < 0) {
            choice = (int)(random01() * (float)count);
        } else {
            choice = (int)(random01() * (float)(count - 1));
            if (choice >= effect.last) choice++;
        }
        if (choice >= count) choice = count - 1;
    }
    effect.last = choice;
    const std::vector<Sound>& copies = effect.copies[(size_t)choice];
    const Sound* pick = nullptr;
    for (const Sound& copy : copies) {
        if (!inUse(copy)) {
            pick = &copy;
            break;
        }
    }
    if (pick == nullptr) { // every copy still rings: restart the one that began first (rare)
        float oldest = 1e30f;
        for (const Sound& copy : copies) {
            for (const Voice& v : voices) {
                if (v.sound.stream.buffer == copy.stream.buffer && v.started < oldest) {
                    oldest = v.started;
                    pick = &copy;
                }
            }
        }
        if (pick == nullptr) pick = &copies[0];
    }
    startVoice(*pick, gain, pitch, side, ui);
    effect.lastPlayed = clock;
    TraceLog(LOG_DEBUG, "AUDIO play %s gain %.2f pitch %.2f side %.2f", name, gain, pitch, side);
    return true;
}

void GameAudio::play(Sfx sfx, float gain, float pitch, float side, float delay) {
    if (!ready || sfx == Sfx::Count) return;
    if (delay > 0.0f) {
        pending.push_back(Pending{sfx, gain, pitch, side, delay});
        return;
    }
    const EffectInfo& info = EFFECTS[(int)sfx];
    Effect& effect = effects[(int)sfx];
    if (clock - effect.lastPlayed < info.cooldown) return; // the same event fired twice
    const int busy = (int)voices.size();
    if (info.priority == 0 && busy >= MAX_VOICES / 2) return;
    if (info.priority <= 1 && busy >= MAX_VOICES) return;
    start(effect, gain * info.gain, pitch, side, info.ui, info.name);
}

void GameAudio::playStep(Surface surface, float gain, float side) {
    if (!ready || surface == Surface::Count) return;
    Effect& effect = steps[(int)surface];
    if (clock - effect.lastPlayed < 0.07f || (int)voices.size() >= MAX_VOICES) return;
    // Each step a little different: a touch higher or lower, a touch softer or firmer.
    start(effect, gain * STEP_GAIN * SURFACE_GAIN[(int)surface] * randomRange(0.82f, 1.0f), randomRange(0.93f, 1.07f), side,
          false, STEP_FILES[(int)surface]);
}

void GameAudio::playNote(int region, int nodeId, bool previous, float side) {
    if (!ready || region < 0 || region > 3 || nodeId < 1 || nodeId > 5) return;
    Effect& effect = previous ? backNotes[nodeId] : notes[region][nodeId];
    // Notes are in the key of the place's music, so they are never detuned.
    start(effect, NOTE_GAIN, 1.0f, side, false, TextFormat("note r%d n%d%s", region + 1, nodeId, previous ? " (previous)" : ""));
}

void GameAudio::setAccents(int region, bool calm) {
    accents.clear();
    if (!ready) return;
    switch (region) {
        case 1: // drips falling into the flooded halls
            accents.push_back(Accent{Sfx::Drip, 3.5f, 9.0f, 0.45f, 0.85f, 0.92f, 1.08f, 0.0f});
            break;
        case 2: // single chimes in the night garden (in its key, so never detuned)
            accents.push_back(Accent{Sfx::Chime, 8.0f, 18.0f, 0.5f, 0.9f, 1.0f, 1.0f, 0.0f});
            break;
        case 3: // ropes and loose stones on the cliffs; fewer once the bridge holds
            if (calm) {
                accents.push_back(Accent{Sfx::Creak, 16.0f, 32.0f, 0.3f, 0.55f, 0.9f, 1.05f, 0.0f});
            } else {
                accents.push_back(Accent{Sfx::Creak, 8.0f, 19.0f, 0.5f, 0.9f, 0.9f, 1.1f, 0.0f});
                accents.push_back(Accent{Sfx::Pebbles, 13.0f, 28.0f, 0.45f, 0.8f, 0.85f, 1.1f, 0.0f});
            }
            break;
        default:
            break;
    }
    for (Accent& a : accents) a.next = randomRange(a.minGap * 0.5f, a.maxGap);
}

void GameAudio::pauseWorld() {
    worldGainTarget = 0.0f; // update() pauses each world sound once it has glided to silence
}

void GameAudio::resumeWorld() {
    worldGainTarget = 1.0f;
    for (Voice& v : voices) {
        if (v.paused) {
            ResumeSound(v.sound); // at silence; it glides back up
            v.paused = false;
        }
    }
}

void GameAudio::cancelPending() {
    pending.clear();
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

void GameAudio::setMusicVolume(float volume) {
    config.music = std::round(std::fmax(0.0f, std::fmin(1.0f, volume)) * 20.0f) / 20.0f;
}

void GameAudio::setSoundVolume(float volume) {
    config.sounds = std::round(std::fmax(0.0f, std::fmin(1.0f, volume)) * 20.0f) / 20.0f;
}

void GameAudio::setMuted(bool muted) {
    config.muted = muted;
    g_masterTarget.store(muted ? 0.0f : 1.0f);
}

float GameAudio::random01() {
    seed ^= seed << 13; // xorshift32: variety for steps and accents, no need for more
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return (float)(seed & 0xFFFFFFu) / 16777216.0f;
}

float GameAudio::randomRange(float low, float high) {
    return low + (high - low) * random01();
}
