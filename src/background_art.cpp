#include "background_art.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>

namespace terry {

// ═════════════════════════════════════════════════════════════════════════════
// ASCII Art Pieces
// ─────────────────────────────────────────────────────────────────────────────
// Characters are handcrafted. Each piece is stored as one string per row.
// Short rows are fine — the renderer pads with spaces.
// '#' = dark robe/body fill   '.' = mid-tone surface detail
// ' ' = highlight / empty     box-drawing / punctuation = structural lines
// ─────────────────────────────────────────────────────────────────────────────

// ── DEATH ─────────────────────────────────────────────────────────────────────
// Hooded skeleton in dark robes. ~52 wide, ~32 rows.
// dim: pale silver   bright: cold white
static const std::vector<std::string> kDeathArt = {
    "                    .--------.                  ",
    "                  .'..........'.                ",
    "                 /..  .-----.  .\\               ",
    "                |..  /       \\  ..|              ",
    "                |. |  ( ) ( )  |..|              ",
    "                |. |   -----   |..|              ",
    "                |. |   '---'   |..|              ",
    "                |..  \\ . . . /  ..|              ",
    "                 \\..  '-----'  ../               ",
    "                  '..  . . .  ..'               ",
    "              .----'...........'----.            ",
    "             / ##################### \\          ",
    "            / ######################### \\        ",
    "           | ###.---.###########.---.### |       ",
    "           | ###|   |###########|   |### |       ",
    "           | ###'---'###########'---'### |       ",
    "           | ########################### |       ",
    "           | ###  .---------------.  ### |       ",
    "           | ##  /                 \\  ## |       ",
    "           | ##  |    .-------.    |  ## |       ",
    "           | ##  |   |         |   |  ## |       ",
    "           | ##  |   |  .---.  |   |  ## |       ",
    "           | ##  |   |  |{+}|  |   |  ## |       ",
    "           | ##  |   |  '---'  |   |  ## |       ",
    "           | ##  |    '-------'    |  ## |       ",
    "           | ##  \\                 /  ## |       ",
    "           | ###  '---------------'  ### |       ",
    "            \\ ######################### /        ",
    "             \\ ##################### /          ",
    "              '---.           .---'             ",
    "                  '-----------'                 ",
    "                 /             \\                ",
    "                '---------------'               ",
};

// ── LUGGAGE ───────────────────────────────────────────────────────────────────
// Sapient pearwood chest — jgs art from ascii.co.uk/art/treasure
// dim: warm brown   bright: amber/gold
static const std::vector<std::string> kLuggageArt = {
    "                            _.--.          ",
    "                        _.-'_:-'||          ",
    "                    _.-'_.-::::'||          ",
    "               _.-:'_.-::::::'  ||          ",
    "             .'`-.-:::::::'     ||          ",
    "            /.'`;|:::::::'      ||_         ",
    "           ||   ||::::::'     _.;._'-._     ",
    "           ||   ||:::::'  _.-!oo @.!-._'-.  ",
    "           \\'.  ||:::::.-!()oo @!()@.-'_.|  ",
    "            '.'-;|:.-'.&$@.& ()$%-'o.'\\U||  ",
    "              `>'-.!@%()@'@_%-'_.-o _.|'||  ",
    "               ||-._'-.@.-'_.-' _.-o  |'||  ",
    "               ||=[ '-._.-\\U/.-'    o |'||  ",
    "               || '-.]=|| |'|      o  |'||  ",
    "               ||      || |'|        _| ';  ",
    "               ||      || |'|    _.-'_.-'   ",
    "               |'-._   || |'|_.-'_.-'       ",
    "            jgs '-._'-.|| |' `_.-'           ",
    "                    '-.||_/.-'               ",
};

// ── RINCEWIND ─────────────────────────────────────────────────────────────────
// Running wizard in hat and robes, mid-flee. ~44 wide, ~28 rows.
// dim: muted purple   bright: vivid purple
static const std::vector<std::string> kRincewindArt = {
    "               .                               ",
    "              /|\\                              ",
    "             / | \\                             ",
    "            /  |  \\                            ",
    "           /   .   \\                           ",
    "           |  / \\  |                           ",
    "           \\ | . | /                           ",
    "            \\|   |/                            ",
    "          .--'   '--.                          ",
    "         /   .---.   \\                         ",
    "        |   / . . \\   |                        ",
    "        |  |  ( )  |  |                        ",
    "        |  |  ---  |  |                        ",
    "        |  |  '-'  |  |                        ",
    "        |   \\     /   |                        ",
    "         \\   '---'   /                         ",
    "    .-----'         '-----.                    ",
    "   / ####################### \\                 ",
    "  / ##########################  \\               ",
    " | ##  .----.   .   .----.  ##  |              ",
    " | ## /      \\ / \\ /      \\ ## |              ",
    " | ##|  .--. | |X| | .--.  |## |              ",
    " | ## \\  '--' \\ / \\ '--'  / ## |              ",
    " | ##  '-----' ' '------'  ##  |              ",
    "  \\ ##########################  /               ",
    "   '\\  ####################  /'                ",
    "     '----..        ..----'                    ",
    "           '--------'                          ",
};

// ═════════════════════════════════════════════════════════════════════════════
// background_art namespace — getters
// ═════════════════════════════════════════════════════════════════════════════

namespace background_art {

const std::vector<std::string>& death_rows()    { return kDeathArt; }
const std::vector<std::string>& luggage_rows()  { return kLuggageArt; }
const std::vector<std::string>& rincewind_rows(){ return kRincewindArt; }

ArtPiece death_piece() {
    return { kDeathArt, 72, 72, 78, 230, 230, 240, "DEATH" };
}
ArtPiece luggage_piece() {
    return { kLuggageArt, 100, 65, 25, 200, 140, 55, "LUGGAGE" };
}
ArtPiece rincewind_piece() {
    return { kRincewindArt, 80, 60, 120, 155, 115, 220, "RINCEWIND" };
}

ArtPiece piece(int idx) {
    switch (idx % 3) {
        case 0: return death_piece();
        case 1: return luggage_piece();
        default: return rincewind_piece();
    }
}
int piece_count() { return 3; }

} // namespace background_art

// ═════════════════════════════════════════════════════════════════════════════
// BackgroundSlideshow implementation
// ═════════════════════════════════════════════════════════════════════════════

BackgroundSlideshow::BackgroundSlideshow() {}

void BackgroundSlideshow::set_terminal_size(int cols, int rows) {
    if (cols == term_cols_ && rows == term_rows_) return;
    term_cols_ = cols;
    term_rows_ = rows;
    recompute_offsets();
    // Rebuild shuffle for any active transition at new size
    if (transition_active_) {
        transition_active_  = false;
        transition_elapsed_ = 0.0f;
    }
}

void BackgroundSlideshow::recompute_offsets() {
    for (int i = 0; i < kPieceCount; ++i) {
        const auto& ap = background_art::piece(i);
        int art_rows = static_cast<int>(ap.rows.size());
        // Find max row width
        int art_cols = 0;
        for (const auto& r : ap.rows)
            art_cols = std::max(art_cols, static_cast<int>(r.size()));

        art_col_offset_[i] = std::max(0, (term_cols_ - art_cols) / 2);
        art_row_offset_[i] = std::max(0, (term_rows_ - art_rows) / 2);
    }
}

void BackgroundSlideshow::tick(float delta) {
    if (!active()) return;

    // Flash
    if (flash_timer_ > 0.0f) {
        flash_timer_ -= delta;
        if (flash_timer_ <= 0.0f) {
            flash_timer_     = 0.0f;
            fade_timer_      = kFadeDuration;
            flash_piece_idx_ = -1;
        }
    } else if (fade_timer_ > 0.0f) {
        fade_timer_ -= delta;
        if (fade_timer_ < 0.0f) fade_timer_ = 0.0f;
    }

    // Transition
    if (transition_active_) {
        transition_elapsed_ += delta;
        if (transition_elapsed_ >= kTransitionDuration) {
            transition_active_  = false;
            transition_elapsed_ = 0.0f;
            slide_idx_          = next_slide_idx_;
            slide_timer_        = 0.0f;
        }
        return;  // don't advance slide_timer_ during transition
    }

    // Normal slide advance
    slide_timer_ += delta;
    if (slide_timer_ >= kSlideDuration) {
        next_slide_idx_ = (slide_idx_ + 1) % kPieceCount;
        start_transition();
    }
}

void BackgroundSlideshow::start_transition() {
    if (term_cols_ <= 0 || term_rows_ <= 0) return;
    int total = term_cols_ * term_rows_;
    transition_order_.resize(static_cast<size_t>(total));
    std::iota(transition_order_.begin(), transition_order_.end(), 0);

    // Seed from current slide index so same slide always has same transition pattern
    std::mt19937 rng(static_cast<unsigned>(slide_idx_ * 7919));
    std::shuffle(transition_order_.begin(), transition_order_.end(), rng);

    // Build reverse map
    cell_order_pos_.assign(static_cast<size_t>(total), 0);
    for (int i = 0; i < total; ++i)
        cell_order_pos_[static_cast<size_t>(transition_order_[i])] = i;

    transition_active_  = true;
    transition_elapsed_ = 0.0f;
}

void BackgroundSlideshow::flash(int piece_idx) {
    flash_timer_     = kFlashDuration;
    fade_timer_      = 0.0f;
    flash_piece_idx_ = piece_idx;
    // If flashing a different piece, snap to it
    if (piece_idx != slide_idx_) {
        // Don't change slide_idx_ — just brighten the requested piece this frame.
        // The slideshow continues from where it was.
    }
}

char BackgroundSlideshow::char_at(int col, int row, float& out_brightness,
                                  int* out_piece_idx) const {
    auto ret_space = [&]() -> char {
        out_brightness = 0.0f;
        if (out_piece_idx) *out_piece_idx = -1;
        return ' ';
    };

    if (!active() || term_cols_ <= 0 || term_rows_ <= 0)
        return ret_space();

    // Determine which piece we're looking at and what brightness to use.
    int   display_idx = (flash_piece_idx_ >= 0) ? flash_piece_idx_ : slide_idx_;
    float brightness  = kNormalBrightness;

    if (flash_piece_idx_ >= 0 && flash_timer_ > 0.0f) {
        brightness = 1.0f;
    } else if (fade_timer_ > 0.0f) {
        float t = fade_timer_ / kFadeDuration;
        brightness = kNormalBrightness + t * (1.0f - kNormalBrightness);
    }

    // During transition (no flash override), blend outgoing→incoming
    if (transition_active_ && flash_piece_idx_ < 0) {
        float progress = transition_elapsed_ / kTransitionDuration;
        int cell_idx = row * term_cols_ + col;
        if (cell_idx < 0 || cell_idx >= static_cast<int>(cell_order_pos_.size()))
            return ret_space();

        int pos   = cell_order_pos_[static_cast<size_t>(cell_idx)];
        int total = term_cols_ * term_rows_;
        float threshold = static_cast<float>(pos) / static_cast<float>(total);

        if (progress < 0.5f) {
            float erase_progress = progress / 0.5f;
            if (threshold < erase_progress)
                return ret_space();          // erased
            display_idx = slide_idx_;        // still outgoing
        } else {
            float draw_progress = (progress - 0.5f) / 0.5f;
            if (threshold < draw_progress)
                display_idx = next_slide_idx_;  // incoming
            else
                return ret_space();             // not yet filled
        }
    }

    // Look up art character
    const ArtPiece ap = background_art::piece(display_idx);
    int art_row = row - art_row_offset_[display_idx];
    int art_col = col - art_col_offset_[display_idx];

    if (art_row < 0 || art_row >= static_cast<int>(ap.rows.size()))
        return ret_space();

    const std::string& art_line = ap.rows[static_cast<size_t>(art_row)];
    if (art_col < 0 || art_col >= static_cast<int>(art_line.size()))
        return ret_space();

    char ch = art_line[static_cast<size_t>(art_col)];
    if (ch == ' ')
        return ret_space();

    out_brightness = brightness;
    if (out_piece_idx) *out_piece_idx = display_idx;
    return ch;
}

} // namespace terry
