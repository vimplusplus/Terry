#pragma once
#include <vector>
#include <random>

namespace terry {

struct Particle {
    float  x, y;   // position in cell coordinates (fractional)
    float  vx, vy; // velocity in cells/second
    char   glyph;  // one of: *, ., +, ~
};

struct ParticleSystem {
    std::vector<Particle> particles;
    std::mt19937 rng{42};

    void Init(int cols, int rows, int count = 10);
    void Update(float delta, int cols, int rows);
};

} // namespace terry
