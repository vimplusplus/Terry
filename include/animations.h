#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace terry {

// ── Sprite overlay ────────────────────────────────────────────────────────────

struct SpriteColor { uint8_t r, g, b; };

struct SpriteRow {
    std::string text;
    SpriteColor color;
};

struct SpriteOverlay {
    std::vector<SpriteRow> rows;
    float x             = 0.0f;  // top-left column (float for sub-cell motion)
    float y             = 0.0f;  // top-left row
    int   z             = 0;     // draw order: higher draws on top
    bool  skip_nonempty = true;  // skip cells with non-space terminal content
};

// ── Luggage ───────────────────────────────────────────────────────────────────
// The Luggage. Sapient pearwood. Many legs. Going somewhere important.

struct LuggageState {
    float pos         = 0.0f;  // fractional column of left edge
    float vel         = 4.0f;  // cells/s, positive = right
    float pause_t     = 0.0f;
    bool  paused      = false;
    float pause_cd    = 0.0f;
    float chase_mult  = 1.0f;  // choreography speed multiplier
    float event_mult  = 1.0f;  // event system speed multiplier
};

void update_luggage(LuggageState& s, float delta, int width,
                    float rincewind_x, bool rincewind_active);

SpriteOverlay luggage_overlay(const LuggageState& s, int inner_rows);

static constexpr int LUGGAGE_SPRITE_WIDTH = 7;

// ── Rincewind ─────────────────────────────────────────────────────────────────
// He is always running away. This is his natural state.

struct RincewindState {
    bool  active      = false;
    float x           = 0.0f;   // column of left edge of sprite
    float speed       = 30.0f;
    float cameo_cd    = 10.0f;  // quick first cameo
    float speed_mult  = 1.0f;   // choreography speed multiplier
};

void update_rincewind(RincewindState& s, float delta, int width,
                      float luggage_x);

SpriteOverlay rincewind_overlay(const RincewindState& s);

static constexpr int RINCEWIND_SPRITE_WIDTH = 5;

// ── Death ─────────────────────────────────────────────────────────────────────

struct DeathState {};  // stateless — position derived from inner_rows, scythe from anim_time

SpriteOverlay death_overlay(float anim_time, int inner_rows);

// Returns Death's farewell line (ALL-CAPS)
const char* death_farewell();

} // namespace terry
