#pragma once

#include "raylib.h"
#include "Lighting.h"

// Small visual particles: dust, leaves, petals, sparks. Purely decorative;
// they never touch nodes, links, or collision.
struct Particle {
    Vector2 position; // world coordinates
    Vector2 velocity; // world pixels per second
    float gravity;    // added to velocity.y every second
    float life;       // seconds left (0 = unused)
    float maxLife;
    float size;       // in canvas pixels
    Color color;
    float drag = 0.0f;      // horizontal slow-down per second (dust and chips settle)
    bool gathering = false; // brightens as it arrives instead of fading out (recovery motes)
    bool glows = false;     // a spark or mote of light: it also gets a soft halo
};

constexpr int MAX_PARTICLES = 192;

// A fixed ring of particles. New particles replace the oldest ones.
struct ParticlePool {
    Particle items[MAX_PARTICLES] = {};
    int next = 0;
};

void spawnParticle(ParticlePool& pool, const Particle& particle);
void updateParticles(ParticlePool& pool, float dt);
void clearParticles(ParticlePool& pool);

// Draws into the pixel canvas (cameraX is the camera's left edge in canvas pixels).
void drawParticles(const ParticlePool& pool, int cameraX);

// The halos of glowing particles (call while additive blending is on).
void drawParticleGlows(const ParticlePool& pool, int cameraX, const SceneRenderer& renderer);

// A puff at the traveler's feet (jumping and landing).
void spawnDust(ParticlePool& pool, Vector2 feet, int count, Color color);

// Pieces flying out from a point: leaves, petals, chips, sparks.
void spawnBurst(ParticlePool& pool, Vector2 at, int count, Color first, Color second, float speed, float gravity,
                float life, bool glows = false);

// Motes drifting in toward a point from a ring around it (the traveler re-forming after a fall).
void spawnGather(ParticlePool& pool, Vector2 at, int count, Color color);
