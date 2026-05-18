#pragma once
#include <vector>
#include <string>
#include <random>
#include <cstdint>

namespace terry {

enum class ParticleType { Sparkle, Rune, Comet, Ook };

struct ParticleTypeDef {
    std::vector<std::string> glyphs;
    uint8_t r, g, b;
    float speed_min, speed_max;   // horizontal speed
    float vy_min,    vy_max;      // vertical speed (negative = up)
    float life_min,  life_max;
    bool  has_trail;
};

struct Particle {
    float        x,  y;
    float        vx, vy;
    std::string  glyph;
    ParticleType type;
    float        lifetime;
    float        prev_x, prev_y;  // for comet trail
};

const ParticleTypeDef& particle_type_def(ParticleType t);

struct ParticleSystem {
    std::vector<Particle> particles;
    std::mt19937 rng{42};

    int  base_count   = 10;
    int  target_count = 10;  // multiplied by events
    int  max_count    = 300;
    int  stored_cols  = 80;
    int  stored_rows  = 22;

    void Init(int cols, int rows, int count = 10);
    void Update(float delta, int cols, int rows);
    void SpawnOne(int cols, int rows, ParticleType type, int force_col = -1);
    void SpawnRune(int col);  // for NarrativiumSurge
};

} // namespace terry
