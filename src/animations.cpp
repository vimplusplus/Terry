#include "animations.h"
#include <cmath>
#include <random>
#include <algorithm>

namespace terry {

// ── Helpers ───────────────────────────────────────────────────────────────────

static constexpr SpriteColor kLuggageColor   = {180, 120,  60};
static constexpr SpriteColor kLuggageLegColor= {140,  90,  40};
static constexpr SpriteColor kRincewindColor = {130, 100, 200};
static constexpr SpriteColor kDeathColor     = {200, 200, 210};

// ── Luggage ───────────────────────────────────────────────────────────────────
// The Luggage: sapient pearwood chest, many legs. Going somewhere important.
//
// Right-moving (vel > 0):
//   ╔═════╗
//   ║[▓▓▓]║
//   \/\/\/\
//
// Left-moving (vel < 0):
//   ╔═════╗
//   ║[▓▓▓]║
//   /\/\/\/

void update_luggage(LuggageState& s, float delta, int width,
                    float rincewind_x, bool rincewind_active) {
    // Choreography: accelerate toward Rincewind when close
    if (rincewind_active && std::abs(rincewind_x - s.pos) < 20.0f) {
        s.chase_mult = 2.5f;
    } else {
        s.chase_mult = 1.0f;
    }

    if (s.paused) {
        s.pause_t -= delta;
        if (s.pause_t <= 0.0f) {
            s.paused = false;
            static std::mt19937 rng{1234};
            std::uniform_int_distribution<int> coin(0, 1);
            if (coin(rng)) s.vel = -s.vel;
        }
        return;
    }

    float effective_vel = s.vel * s.chase_mult * s.event_mult;
    s.pos += effective_vel * delta;

    float max_pos = static_cast<float>(std::max(1, width - LUGGAGE_SPRITE_WIDTH));
    if (s.pos < 0.0f) {
        s.pos = 0.0f;
        s.vel = std::abs(s.vel);
    }
    if (s.pos > max_pos) {
        s.pos = max_pos;
        s.vel = -std::abs(s.vel);
    }

    s.pause_cd -= delta;
    if (s.pause_cd <= 0.0f) {
        static std::mt19937 rng{5678};
        std::uniform_real_distribution<float> pause_d(0.5f, 2.5f);
        std::uniform_real_distribution<float> cd_d(4.0f, 12.0f);
        s.paused   = true;
        s.pause_t  = pause_d(rng);
        s.pause_cd = cd_d(rng);
    }
}

SpriteOverlay luggage_overlay(const LuggageState& s, int inner_rows) {
    bool going_right = (s.vel >= 0.0f);
    SpriteOverlay ov;
    ov.x = s.pos;
    ov.y = static_cast<float>(inner_rows - 4);  // 3-row sprite, 1 row from bottom
    ov.z = 2;
    ov.skip_nonempty = true;

    ov.rows.push_back({"╔═════╗", kLuggageColor});
    ov.rows.push_back({"║[▓▓▓]║", kLuggageColor});
    ov.rows.push_back({going_right ? "\\/\\/\\/" : "/\\/\\/\\", kLuggageLegColor});
    return ov;
}

// ── Rincewind ─────────────────────────────────────────────────────────────────
// He is always running away from something. This is his natural state.
//
// 5-wide, 4-row sprite:
//   "  ^  "   hat tip
//   " /|\ "   hat brim + shoulders
//   "(>.<)"   terrified face
//   " }:> "   fleeing robes/legs

void update_rincewind(RincewindState& s, float delta, int width,
                      float luggage_x) {
    if (!s.active) {
        s.cameo_cd -= delta;
        if (s.cameo_cd <= 0.0f) {
            s.active = true;
            s.x      = static_cast<float>(-RINCEWIND_SPRITE_WIDTH);
            static std::mt19937 rng{9999};
            std::uniform_real_distribution<float> d(30.0f, 60.0f);
            s.cameo_cd = d(rng);
        }
        return;
    }

    // Choreography: flee faster when Luggage is close
    if (std::abs(luggage_x - s.x) < 20.0f) {
        s.speed_mult = 1.8f;
    } else {
        s.speed_mult = 1.0f;
    }

    s.x += s.speed * s.speed_mult * delta;
    if (s.x > static_cast<float>(width + RINCEWIND_SPRITE_WIDTH)) {
        s.active     = false;
        s.speed_mult = 1.0f;
    }
}

SpriteOverlay rincewind_overlay(const RincewindState& s) {
    SpriteOverlay ov;
    ov.x = s.x;
    ov.y = 2.0f;   // runs across upper area of terminal content
    ov.z = 3;
    ov.skip_nonempty = true;

    ov.rows.push_back({"  ^  ", kRincewindColor});
    ov.rows.push_back({" /|\\ ", kRincewindColor});
    ov.rows.push_back({"(>.<)", kRincewindColor});
    ov.rows.push_back({" }:> ", kRincewindColor});
    return ov;
}

// ── Death ─────────────────────────────────────────────────────────────────────
// He does not move. He simply IS there.
//
// 9-wide, 6-row sprite (scythe on row 4 alternates glyph):
//   "   .-.   "   skull top
//   "  (X X)  "   hollow eye sockets
//   "   `-'   "   jaw
//   "   |||   "   upper body / robes
//   "  /|||<  "   lower body + scythe blade (variant A)
//   "  /|||>  "   lower body + scythe blade (variant B)
//   " /|___|\ "   robe base

SpriteOverlay death_overlay(float anim_time, int inner_rows) {
    bool scythe_a = (std::sin(anim_time * 3.14159f / 0.6f) >= 0.0f);

    SpriteOverlay ov;
    ov.x = 0.0f;
    ov.y = static_cast<float>(inner_rows - 7);  // 7-row sprite flush to bottom
    ov.z = 1;
    ov.skip_nonempty = true;

    ov.rows.push_back({"   .-.   ", kDeathColor});
    ov.rows.push_back({"  (X X)  ", kDeathColor});
    ov.rows.push_back({"   `-'   ", kDeathColor});
    ov.rows.push_back({"   |||   ", kDeathColor});
    ov.rows.push_back({scythe_a ? "  /|||<  " : "  /|||>  ", kDeathColor});
    ov.rows.push_back({" /|___|/ ", kDeathColor});
    ov.rows.push_back({"/_______\\", kDeathColor});
    return ov;
}

const char* death_farewell() {
    return "AND SO IT ENDS. I TRUST YOU USED THE TIME WISELY.";
}

} // namespace terry

