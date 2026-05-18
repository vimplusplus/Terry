#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace terry {

// ── ArtPiece — metadata for one background art piece ─────────────────────────

struct ArtPiece {
    const std::vector<std::string>& rows;
    uint8_t dim_r,    dim_g,    dim_b;     // 40% brightness colour
    uint8_t bright_r, bright_g, bright_b;  // 100% brightness colour (event flash)
    const char* name;
};

// ── Getters (defined in background_art.cpp) ───────────────────────────────────

namespace background_art {
    const std::vector<std::string>& death_rows();
    const std::vector<std::string>& luggage_rows();
    const std::vector<std::string>& rincewind_rows();

    ArtPiece death_piece();
    ArtPiece luggage_piece();
    ArtPiece rincewind_piece();

    // idx: 0 = Death, 1 = Luggage, 2 = Rincewind
    ArtPiece piece(int idx);
    int      piece_count();
} // namespace background_art

// ── BackgroundSlideshow ───────────────────────────────────────────────────────
// Manages a timed slideshow of full-screen ASCII art pieces, rendered as a
// background layer (layer 0) behind the PTY cell buffer.
//
// Usage:
//   1. Call set_terminal_size(cols, rows) on resize.
//   2. Call tick(delta) every frame in UpdateLive().
//   3. Call flash(piece_idx) when an event fires.
//   4. In TerryNode::Render(), call char_at(col, row, brightness) for each cell
//      before rendering the PTY buffer.

class BackgroundSlideshow {
public:
    BackgroundSlideshow();

    void set_terminal_size(int cols, int rows);
    void tick(float delta);

    // Flash piece_idx to full brightness for kFlashDuration seconds.
    // Immediately switches the displayed slide to that piece.
    void flash(int piece_idx);

    // Returns the character to render at terminal cell (col, row).
    // Sets out_brightness in [0.0, 1.0].
    // Optionally sets *out_piece_idx to the art piece (0=Death,1=Luggage,2=Rincewind),
    // or -1 when returning space. Pass nullptr to ignore.
    // Returns ' ' (space) when: no art at that position, or terminal < kMinTermWidth.
    char char_at(int col, int row, float& out_brightness,
                 int* out_piece_idx = nullptr) const;

    // True when the terminal is wide enough to show background art.
    bool active() const { return term_cols_ >= kMinTermWidth; }

private:
    void start_transition();
    void recompute_offsets();

    static constexpr int   kPieceCount          = 3;
    static constexpr float kSlideDuration        = 30.0f;
    static constexpr float kTransitionDuration   = 1.5f;
    static constexpr float kNormalBrightness     = 0.4f;
    static constexpr float kFlashDuration        = 0.5f;
    static constexpr float kFadeDuration         = 0.3f;
    static constexpr int   kMinTermWidth         = 60;

    int term_cols_ = 0;
    int term_rows_ = 0;

    // Slideshow
    int   slide_idx_          = 0;
    float slide_timer_        = 0.0f;
    bool  transition_active_  = false;
    float transition_elapsed_ = 0.0f;
    int   next_slide_idx_     = 1;

    // Shuffled list of cell indices (row * term_cols_ + col), built per transition.
    // The first half of the transition erases these cells; the second half fills them.
    std::vector<int> transition_order_;

    // Reverse map: cell_index → position in transition_order_ (for O(1) lookup in char_at).
    std::vector<int> cell_order_pos_;

    // Flash
    float flash_timer_     = 0.0f;
    int   flash_piece_idx_ = -1;
    float fade_timer_      = 0.0f;   // for soft fade-out after flash

    // Per-piece centering offsets into the terminal grid
    int art_col_offset_[kPieceCount] = {0, 0, 0};
    int art_row_offset_[kPieceCount] = {0, 0, 0};
};

} // namespace terry
