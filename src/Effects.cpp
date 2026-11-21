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
        float fade = p.life / p.maxLife;
        int x = (int)std::floor(p.position.x / 2.0f) - cameraX;
        int y = (int)std::floor(p.position.y / 2.0f);
        int size = (int)std::ceil(p.size * (0.5f + 0.5f * fade));
        DrawRectangle(x, y, size, size, withAlpha(p.color, fade));
    }
}

void spawnDust(ParticlePool& pool, Vector2 feet, int count, Color color) {
    for (int i = 0; i < count; i++) {
        float side = (i % 2 == 0) ? -1.0f : 1.0f;
        float spread = 0.4f + hash01(i * 13 + (int)feet.x) * 0.8f;
        Particle p;
        p.position = Vector2{feet.x + side * (4.0f + i * 2.0f), feet.y - 2.0f};
        p.velocity = Vector2{side * 70.0f * spread, -30.0f - 40.0f * hash01(i * 7 + (int)feet.y)};
        p.gravity = 120.0f;
        p.life = p.maxLife = 0.35f + 0.2f * spread;
        p.size = 2.0f;
        p.color = color;
        spawnParticle(pool, p);
    }
}

void spawnBurst(ParticlePool& pool, Vector2 at, int count, Color first, Color second, float speed, float gravity,
                float life) {
    for (int i = 0; i < count; i++) {
        float angle = -3.14159f * (0.1f + 0.8f * hash01(i * 31 + (int)at.x)); // mostly upward
        float strength = speed * (0.5f + 0.7f * hash01(i * 17 + (int)at.y));
        Particle p;
        p.position = at;
        p.velocity = Vector2{std::cos(angle) * strength, std::sin(angle) * strength};
        p.gravity = gravity;
        p.life = p.maxLife = life * (0.7f + 0.6f * hash01(i * 5 + 3));
        p.size = (i % 3 == 0) ? 2.0f : 1.0f;
        p.color = (i % 2 == 0) ? first : second;
        spawnParticle(pool, p);
    }
}

void spawnGather(ParticlePool& pool, Vector2 at, int count, Color color) {
    for (int i = 0; i < count; i++) {
        float angle = 6.2832f * i / count;
        float distance = 46.0f + 20.0f * hash01(i * 11 + 7);
        float life = 0.55f;
        Particle p;
        p.position = Vector2{at.x + std::cos(angle) * distance, at.y + std::sin(angle) * distance};
        p.velocity = Vector2{-std::cos(angle) * distance / life, -std::sin(angle) * distance / life};
        p.gravity = 0.0f;
        p.life = p.maxLife = life;
        p.size = 2.0f;
        p.color = color;
        spawnParticle(pool, p);
    }
}
