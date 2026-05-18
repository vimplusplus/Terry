#pragma once
#include <string>

namespace terry {

// ── Luggage ──────────────────────────────────────────────────────────────────
// The Luggage walks the status bar. Silent. Purposeful. Going somewhere important.

struct LuggageState {
    float pos       = 0.0f;  // fractional position along status bar
    float vel       = 4.0f;  // cells per second (positive = right)
    float pause_t   = 0.0f;  // countdown timer for pausing
    bool  paused    = false;
    float pause_cd  = 0.0f;  // countdown to next random pause/reversal
};

void update_luggage(LuggageState& s, float delta, int bar_width);

// Returns a 3-char string representation of the Luggage at the given step
// direction: +1 = right, -1 = left
std::string luggage_frame(int direction);

// ── Rincewind ─────────────────────────────────────────────────────────────────
// Rincewind appears briefly, running across the top of the screen.
// He does not stop. He never stops.

struct RincewindState {
    bool  active     = false;
    float x          = 0.0f;   // current x position (cell column)
    float speed      = 40.0f;  // cells per second
    float cameo_cd   = 60.0f;  // seconds until next cameo (starts soon for first appearance)
};

// Returns the column of Rincewind's head if active, or -1
void update_rincewind(RincewindState& s, float delta, int width);

// The 3-char string to render at Rincewind's position
const char* rincewind_sprite();

} // namespace terry
