#pragma once

#include <string>
#include <vector>

#include "raylib.h"

// Sound for the journey: a looping score that changes with each place, a bed of ambience for each
// place, and a small library of effects. Everything here is optional: without an audio device, or
// without the files in assets/audio, every call does nothing and the game plays on in silence.
// Gameplay calls this from its event handlers; drawing code only reads settings().

enum class Track { Forest, Ruins, Garden, Edge, Dawn, Finale, Count };        // music loops
enum class Bed { Forest, Ruins, Garden, EdgeWind, EdgeGap, EdgeDawn, Count }; // ambience loops
enum class Surface { Grass, Earth, Stone, WetStone, Wood, Soft, Count };      // what a footstep sounds like

enum class Sfx {
    // The traveler and the world
    Jump, Land, Splash, Fall, Recover,
    Passage, Key, Brazier, Checkpoint, Exit, Complete,
    Gate1, Gate2, Gate3, Gate4, Arrive1, Arrive2, Arrive3, Arrive4,
    EdgeGust, Hazard, Repair, Plank, Pier, Rope, Lantern1, Lantern2, Lantern3,
    // The field journal and the interface (these keep playing while the world is paused)
    JournalOpen, JournalClose, Page, NoteOpen, NoteDismiss, Callout, CoverOpen, CoverClose, Tick, Toggle,
    // Random sounds of a place (see setAccents)
    Drip, Chime, Creak, Pebbles,
    Count
};

struct AudioSettings {
    float music = 0.75f; // 0..1
    float sounds = 0.8f; // effects and ambience, 0..1
    bool muted = false;
};

class GameAudio {
public:
    GameAudio() = default;
    ~GameAudio();
    GameAudio(const GameAudio&) = delete;
    GameAudio& operator=(const GameAudio&) = delete;

    void load();   // after InitAudioDevice(): finds assets/audio and loads every file it can
    void unload(); // before CloseAudioDevice(): fades to silence, then unloads everything; safe to call twice
    void update(float dt, bool worldRunning); // every frame, also while paused

    // Music: the playing track fades out while `track` fades in, starting after `delay` seconds.
    void playMusic(Track track, float fadeOut, float fadeIn, float delay = 0.0f);
    void stopMusic(float fadeOut);
    // Ambience: each bed moves to its own level (0..1) over `fade` seconds, after `delay`.
    void setBed(Bed bed, float level, float fade, float delay = 0.0f);
    void stopBeds(float fadeOut);
    // A temporary dip under notes, the journal, and the pause screen (1 = none).
    void setDuck(float music, float beds);

    // Effects. `side` places a sound from -1 (left) through 0 (center) to 1 (right). A `delay`
    // schedules it; delays only count down while the world is running.
    void play(Sfx sfx, float gain = 1.0f, float pitch = 1.0f, float side = 0.0f, float delay = 0.0f);
    void playStep(Surface surface, float gain, float side);
    void playNote(int region, int nodeId, bool previous, float side); // the note of the link just followed
    void setAccents(int region, bool calm = false); // the place's random sounds; region -1 stops them
    void pauseWorld();    // the world's sounds glide down and wait; interface sounds play on
    void resumeWorld();
    void cancelPending(); // forget scheduled effects (a replay)

    const AudioSettings& settings() const { return config; }
    void setMusicVolume(float volume);
    void setSoundVolume(float volume);
    void setMuted(bool muted); // the whole mix glides to silence and back, so it never clicks
    bool isReady() const { return ready; }

private:
    struct Stream {
        Music music = {};
        const char* name = "";
        bool valid = false;
        bool playing = false;
        float gain = 0.0f;   // 0..1, eased into the volume
        float target = 0.0f;
        float rate = 1.0f;   // gain per second
        float delay = 0.0f;  // seconds before a fade in begins
    };
    struct Effect {
        std::vector<Sound> sources;             // one per file: owns the samples
        std::vector<std::vector<Sound>> copies; // per source: the source, then aliases that share its samples
        int last = -1;
        float lastPlayed = -100.0f;
    };
    struct Voice {           // a sound that is playing; its level is eased every frame, never cut
        Sound sound;
        float gain;          // its own gain, before the volume setting
        float started;       // audio clock
        bool ui;             // interface sounds are not dipped or paused with the world
        bool paused;
        float applied;       // the volume last given to raylib
    };
    struct Pending {
        Sfx sfx;
        float gain, pitch, side, wait;
    };
    struct Accent {
        Sfx sfx;
        float minGap, maxGap, gainLow, gainHigh, pitchLow, pitchHigh;
        float next;
    };

    void loadStream(Stream& stream, const char* file);
    void loadEffect(Effect& effect, const std::string& base, int aliases);
    void unloadEffect(Effect& effect);
    bool start(Effect& effect, float gain, float pitch, float side, bool ui, const char* name);
    void startVoice(Sound sound, float gain, float pitch, float side, bool ui);
    bool inUse(const Sound& sound) const;
    float voiceVolume(const Voice& voice) const;
    void updateStream(Stream& stream, float dt, float volume);
    float random01();
    float randomRange(float low, float high);

    bool ready = false;   // the device is open and the folder was found
    bool attached = false;
    std::string root;     // the assets/audio folder, with a trailing slash
    AudioSettings config;
    Stream tracks[(int)Track::Count];
    Stream beds[(int)Bed::Count];
    Effect effects[(int)Sfx::Count];
    Effect steps[(int)Surface::Count];
    Effect notes[4][6];   // [region][node id]: arriving through next
    Effect backNotes[6];  // Region 2: arriving through previous
    std::vector<Voice> voices;
    std::vector<Pending> pending;
    std::vector<Accent> accents;
    float clock = 0.0f;
    float duckMusic = 1.0f, duckMusicTarget = 1.0f;
    float duckBeds = 1.0f, duckBedsTarget = 1.0f;
    float worldGain = 1.0f, worldGainTarget = 1.0f;
    unsigned int seed = 0x9E3779B9u;
};
