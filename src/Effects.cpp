#include "Effects.h"

#include <cmath>

#include "DrawHelpers.h"

void spawnParticle(ParticlePool& pool, const Particle& particle) {
    pool.items[pool.next] = particle;
    pool.next = (pool.next + 1) % MAX_PARTICLES;
}

void updateParticles(ParticlePool& pool, float dt) {
    for (Particle& p : pool.items) {
        if (p.life <= 0.0f) continue;
        p.life -= dt;
        if (p.drag > 0.0f) p.velocity.x *= std::fmax(0.0f, 1.0f - p.drag * dt);
        p.velocity.y += p.gravity * dt;
        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
    }
}

void clearParticles(ParticlePool& pool) {
    for (Particle& p : pool.items) {
        p.life = 0.0f;
    }
}

void drawParticles(const ParticlePool& pool, int cameraX) {
    for (const Particle& p : pool.items) {
        if (p.life <= 0.0f || p.maxLife <= 0.0f) continue;
        float fade = p.life / p.maxLife; // 1 -> 0 over the particle's life
        float age = 1.0f - fade;
        float alpha = p.gathering ? 0.25f + 0.75f * age   // motes brighten as they reach the traveler
                                  : std::fmin(1.0f, fade / 0.4f); // full, then fading over the last 40%
        alpha *= std::fmin(1.0f, age / 0.06f);              // a quick fade-in: nothing pops into view
        int x = (int)std::floor(p.position.x / 2.0f) - cameraX;
        int y = (int)std::floor(p.position.y / 2.0f);
        int size = (int)p.size;
        if (size > 1 && fade < 0.35f && !p.gathering) size -= 1; // large flakes shrink as they fade
        DrawRectangle(x, y, size, size, withAlpha(p.color, alpha));
    }
}

void drawParticleGlows(const ParticlePool& pool, int cameraX, const SceneRenderer& renderer) {
    for (const Particle& p : pool.items) {
        if (p.life <= 0.0f || p.maxLife <= 0.0f || !p.glows) continue;
        float fade = p.life / p.maxLife;
        float alpha = p.gathering ? 0.25f + 0.75f * (1.0f - fade) : std::fmin(1.0f, fade / 0.4f);
        Vector2 at = {std::floor(p.position.x / 2.0f) - cameraX + 0.5f, std::floor(p.position.y / 2.0f) + 0.5f};
        renderer.drawSoftLight(at, 4.0f + p.size * 2.0f, p.color, 0.4f * alpha);
    }
}

void spawnDust(ParticlePool& pool, Vector2 feet, int count, Color color) {
    for (int i = 0; i < count; i++) {
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        float spread = 0.4f + hash01(i * 13 + (int)feet.x) * 0.8f;
        Particle p;
        p.position = Vector2{feet.x + side * (4.0f + i * 2.0f), feet.y - 2.0f};
        p.velocity = Vector2{side * 90.0f * spread, -30.0f - 40.0f * hash01(i * 7 + (int)feet.y)};
        p.gravity = 120.0f;
        p.drag = 4.0f; // puffs out, then hangs in the air
        p.life = p.maxLife = 0.4f + 0.25f * spread;
        p.size = 2.0f;
        p.color = color;
        spawnParticle(pool, p);
    }
}

void spawnBurst(ParticlePool& pool, Vector2 at, int count, Color first, Color second, float speed, float gravity,
                float life, bool glows) {
    for (int i = 0; i < count; i++) {
        float angle = -3.14159f * (0.1f + 0.8f * hash01(i * 31 + (int)at.x)); // mostly upward
        float strength = speed * (0.5f + 0.7f * hash01(i * 17 + (int)at.y));
        Particle p;
        p.position = at;
        p.velocity = Vector2{std::cos(angle) * strength, std::sin(angle) * strength};
        p.gravity = gravity;
        p.drag = 1.2f;
        p.life = p.maxLife = life * (0.7f + 0.6f * hash01(i * 5 + 3));
        p.size = (i % 3 == 0) ? 2.0f : 1.0f;
        p.color = (i % 2 == 0) ? first : second;
        p.glows = glows;
        spawnParticle(pool, p);
    }
}

void spawnGather(ParticlePool& pool, Vector2 at, int count, Color color) {
    for (int i = 0; i < count; i++) {
        float angle = 6.2832f * i / count;
        float distance = 46.0f + 20.0f * hash01(i * 11 + 7);
        float life = 0.5f + 0.12f * hash01(i * 3 + 1);
        Particle p;
        p.position = Vector2{at.x + std::cos(angle) * distance, at.y + std::sin(angle) * distance};
        p.velocity = Vector2{-std::cos(angle) * distance / life, -std::sin(angle) * distance / life};
        p.gravity = 0.0f;
        p.life = p.maxLife = life;
        p.size = 2.0f;
        p.color = color;
        p.gathering = true;
        p.glows = true;
        spawnParticle(pool, p);
    }
}
