#include "particle_system.h"
#include <algorithm>
#include <cmath>

namespace terry {

// ── Type config table ─────────────────────────────────────────────────────────
// Weights: Sparkle 40%, Rune 30%, Comet 20%, Ook 10%

static const ParticleTypeDef kTypeDefs[] = {
    // Sparkle  — pale gold, drifts gently
    {{"*", ".", "+", "~", "*", "."}, 220, 210, 140,
      -2.0f, 2.0f,   -1.5f, -0.2f,   7.0f, 16.0f,  false},
    // Rune     — octarine purple, floats slowly
    {{"*", "+", "~", "#", "%", "@"}, 130, 60, 210,
      -1.2f, 1.2f,   -1.2f,  0.4f,   5.0f, 12.0f,  false},
    // Comet    — blue-white, moves fast rightward with trail
    {{"-", "=", "~"}, 180, 170, 255,
       3.0f, 9.0f,   -0.4f,  0.4f,   3.0f,  7.0f,  true},
    // Ook      — amber, drifts upward
    {{"O", "K", "!"}, 210, 160, 50,
      -0.5f, 0.5f,   -3.0f, -1.2f,   2.5f,  5.0f,  false},
};

const ParticleTypeDef& particle_type_def(ParticleType t) {
    return kTypeDefs[static_cast<int>(t)];
}

// ── Weighted type selection ───────────────────────────────────────────────────

static ParticleType random_type(std::mt19937& rng) {
    std::uniform_int_distribution<int> d(0, 99);
    int r = d(rng);
    if (r < 40) return ParticleType::Sparkle;
    if (r < 70) return ParticleType::Rune;
    if (r < 90) return ParticleType::Comet;
    return ParticleType::Ook;
}

// ── ParticleSystem ────────────────────────────────────────────────────────────

void ParticleSystem::SpawnOne(int cols, int rows, ParticleType type, int force_col) {
    if ((int)particles.size() >= max_count) return;

    const ParticleTypeDef& def = particle_type_def(type);
    std::uniform_real_distribution<float> xd(0.0f, static_cast<float>(cols));
    std::uniform_real_distribution<float> yd(0.0f, static_cast<float>(rows));
    std::uniform_real_distribution<float> spd(def.speed_min, def.speed_max);
    std::uniform_real_distribution<float> vyd(def.vy_min, def.vy_max);
    std::uniform_real_distribution<float> ltd(def.life_min, def.life_max);
    std::uniform_int_distribution<int>    gd(0, (int)def.glyphs.size() - 1);

    Particle p;
    p.x       = (force_col >= 0) ? static_cast<float>(force_col) : xd(rng);
    p.y       = yd(rng);
    p.vx      = spd(rng);
    p.vy      = vyd(rng);
    p.glyph   = def.glyphs[gd(rng)];
    p.type    = type;
    p.lifetime = ltd(rng);
    p.prev_x  = p.x;
    p.prev_y  = p.y;

    if (type == ParticleType::Comet && p.vx < 0) p.vx = -p.vx;

    particles.push_back(std::move(p));
}

void ParticleSystem::SpawnRune(int col) {
    SpawnOne(stored_cols, stored_rows, ParticleType::Rune, col);
}

void ParticleSystem::Init(int cols, int rows, int count) {
    base_count    = count;
    target_count  = count;
    stored_cols   = cols;
    stored_rows   = rows;
    particles.clear();
    for (int i = 0; i < count; ++i)
        SpawnOne(cols, rows, random_type(rng));
}

void ParticleSystem::Update(float delta, int cols, int rows) {
    stored_cols = cols;
    stored_rows = rows;
    float fc = static_cast<float>(cols);
    float fr = static_cast<float>(rows);

    for (auto& p : particles) {
        p.prev_x = p.x;
        p.prev_y = p.y;
        p.x += p.vx * delta;
        p.y += p.vy * delta;
        p.lifetime -= delta;
        while (p.x < 0.0f) p.x += fc;
        while (p.x >= fc)  p.x -= fc;
        while (p.y < 0.0f) p.y += fr;
        while (p.y >= fr)  p.y -= fr;
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p) { return p.lifetime <= 0.0f; }),
        particles.end());

    // Respawn up to target_count
    int to_spawn = std::min(
        target_count - (int)particles.size(),
        max_count    - (int)particles.size());
    for (int i = 0; i < to_spawn; ++i)
        SpawnOne(cols, rows, random_type(rng));
}

} // namespace terry
