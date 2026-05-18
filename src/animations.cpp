#include "animations.h"
#include <cmath>
#include <random>
#include <algorithm>

namespace terry {

// ── Luggage ──────────────────────────────────────────────────────────────────

std::string luggage_frame(int direction) {
    // The Luggage: sapient pearwood chest, many legs, going somewhere important
    // [▓] = chest body, ╾╾ / ╼╼ = scurrying legs
    return direction >= 0 ? "[▓]╾╾" : "╼╼[▓]";
}

void update_luggage(LuggageState& s, float delta, int bar_width) {
    if (s.paused) {
        s.pause_t -= delta;
        if (s.pause_t <= 0.0f) {
            s.paused = false;
            // Reverse direction half the time
            static std::mt19937 rng{1234};
            std::uniform_int_distribution<int> coin(0, 1);
            if (coin(rng)) s.vel = -s.vel;
        }
        return;
    }

    s.pos += s.vel * delta;

    // Bounce at edges
    float max_pos = static_cast<float>(std::max(1, bar_width - 5));
    if (s.pos < 0.0f) {
        s.pos = 0.0f;
        s.vel = std::abs(s.vel);
    }
    if (s.pos > max_pos) {
        s.pos = max_pos;
        s.vel = -std::abs(s.vel);
    }

    // Random pause countdown
    s.pause_cd -= delta;
    if (s.pause_cd <= 0.0f) {
        static std::mt19937 rng{5678};
        std::uniform_real_distribution<float> pause_d(0.5f, 2.5f);
        std::uniform_real_distribution<float> cd_d(4.0f, 12.0f);
        s.paused  = true;
        s.pause_t = pause_d(rng);
        s.pause_cd = cd_d(rng);
    }
}

// ── Rincewind ─────────────────────────────────────────────────────────────────

const char* rincewind_sprite() {
    // Rincewind running: pointy hat (∧), body (o), fleeing (≫)
    // He is always running away from something. This is his natural state.
    return "∧o≫";
}

void update_rincewind(RincewindState& s, float delta, int width) {
    if (!s.active) {
        s.cameo_cd -= delta;
        if (s.cameo_cd <= 0.0f) {
            s.active = true;
            s.x = -3.0f; // start just off left edge
            // Next cameo in 3-5 minutes (180-300 seconds)
            static std::mt19937 rng{9999};
            std::uniform_real_distribution<float> d(180.0f, 300.0f);
            s.cameo_cd = d(rng);
        }
        return;
    }

    s.x += s.speed * delta;
    if (s.x > static_cast<float>(width + 3)) {
        s.active = false;
    }
}

} // namespace terry
