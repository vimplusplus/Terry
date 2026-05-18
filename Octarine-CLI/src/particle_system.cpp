#include "particle_system.h"
#include <algorithm>
#include <cmath>

namespace terry {

static const char GLYPHS[] = {'*', '.', '+', '~', '*', '.', '*'};

void ParticleSystem::Init(int cols, int rows, int count) {
    particles.clear();
    std::uniform_real_distribution<float> xd(0.0f, static_cast<float>(cols));
    std::uniform_real_distribution<float> yd(0.0f, static_cast<float>(rows));
    std::uniform_real_distribution<float> vd(-2.5f, 2.5f);
    std::uniform_int_distribution<int>    gd(0, static_cast<int>(sizeof(GLYPHS)) - 1);

    for (int i = 0; i < count; ++i) {
        Particle p;
        p.x = xd(rng); p.y = yd(rng);
        do { p.vx = vd(rng); } while (std::abs(p.vx) < 0.3f);
        do { p.vy = vd(rng); } while (std::abs(p.vy) < 0.2f);
        p.glyph = GLYPHS[gd(rng)];
        particles.push_back(p);
    }
}

void ParticleSystem::Update(float delta, int cols, int rows) {
    float fc = static_cast<float>(cols);
    float fr = static_cast<float>(rows);
    for (auto& p : particles) {
        p.x += p.vx * delta;
        p.y += p.vy * delta;
        // Wrap
        while (p.x < 0.0f) p.x += fc;
        while (p.x >= fc)  p.x -= fc;
        while (p.y < 0.0f) p.y += fr;
        while (p.y >= fr)  p.y -= fr;
    }
}

} // namespace terry
