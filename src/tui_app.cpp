// Windows headers must come before FTXUI to allow color.hpp to #undef RGB
// NOMINMAX prevents min/max macro definitions that break std::min/max
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef RGB   // wingdi.h RGB(r,g,b) macro conflicts with ftxui::Color::RGB
#undef min   // winnt.h min/max macros conflict with std::min/max
#undef max
#include <direct.h>
#define getcwd _getcwd
#endif

#include "tui_app.h"
#include "vscode_launcher.h"
#include "skill_registry.h"
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <string>
#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#endif

namespace terry {

// Keep ftxui names accessible within our namespace
using namespace ftxui;

// Status labels at file scope (avoids MSVC static constexpr array issues)
static constexpr const char* kLabels[] = {
    "DEATH AWAITS ALL",
    "THE DISC TURNS",
    "MAGIC IS AFOOT",
    "OOK",
    "WHERE'S MY LUGGAGE?",
    "SPACE: THE UNIMAGINATIVE FRONTIER",
    "MILLION-TO-ONE CHANCES CROP UP NINE TIMES OUT OF TEN",
    "THE TURTLE MOVES",
    "NARRATIVIUM DETECTED",
    "NUNC TEMPORIS FUGIT",
    "TURTLES ALL THE WAY DOWN",
    "BEWARE THE WIZZARD",
    "I ATE'NT DEAD",
    "THERE'S NO PLACE LIKE ANKH-MORPORK",
    "THE WIZZARD IS IN",
    "WORDS IN THE HEART CANNOT BE TAKEN",
    "KNEEL BEFORE THE NARRATIVE",
    "RUNNING ON NARRATIVIUM",
};
static constexpr int kLabelCount = 18;

// ── Magical event config ──────────────────────────────────────────────────────

struct EventConfig {
    const char* label;
    float cd_min, cd_max;  // cooldown range (seconds)
    float duration;         // active duration (seconds)
};

static const EventConfig kEventConfig[kEventCount] = {
    {"OCTARINE STORM",            15.0f,  60.0f, 4.0f},
    {"NARRATIVIUM SURGE",         20.0f,  65.0f, 2.0f},
    {"THE LUGGAGE IS DISPLEASED", 12.0f,  50.0f, 6.0f},
};

// ── Discworld quote ticker ────────────────────────────────────────────────────

static constexpr float kQuoteSpeed = 22.0f;  // columns per second

static constexpr const char* kDiscworldQuotes[] = {
    "\"Real stupidity beats artificial intelligence every time.\"",
    "\"Million-to-one chances crop up nine times out of ten.\"",
    "\"The truth may be out there, but the lies are inside your head.\"",
    "\"Five exclamation marks, the sure sign of an insane mind.\"",
    "\"Fantasy is an exercise bicycle for the mind.\"",
    "\"I'd rather be a rising ape than a falling angel.\"",
    "\"It's not worth doing something unless someone, somewhere, would much rather you weren't doing it.\"",
    "\"Evil begins when you begin to treat people as things.\"",
    "\"Stories of imagination tend to upset those without one.\"",
    "\"Coming back to where you started is not the same as never leaving.\"",
    "\"The Turtle Moves.\"",
    "\"Wisdom comes from experience. Experience is often a result of lack of wisdom.\"",
    "\"The pen is mightier than the sword if the sword is very short and the pen is very sharp.\"",
    "\"Nanny Ogg knew how to start spelling 'banana', but didn't know how you stopped.\"",
    "\"Personal isn't the same as important.\"",
};
static constexpr int kQuoteCount = 15;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string current_time_str() {
    std::time_t t = std::time(nullptr);
    std::tm local_tm = {};
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&local_tm, "%H:%M");
    return ss.str();
}

static std::string get_cwd() {
    char buf[1024] = {};
    if (::getcwd(buf, sizeof(buf) - 1)) return buf;
    return "?";
}

static std::string str_truncate(const std::string& s, int max_len) {
    if (static_cast<int>(s.size()) <= max_len) return s;
    return "..." + s.substr(s.size() - static_cast<size_t>(max_len - 3));
}

// ── TerryNode — custom FTXUI node rendering our terminal cell buffer ──────────

class TerryNode : public ftxui::Node {
public:
    TerryNode(CellBuffer& buf, ParticleSystem& ps,
              const ShootingStar*  star,
              BackgroundSlideshow* slideshow,
              const LuggageRun*    luggage_run,
              int cursor_col, int cursor_row, bool cursor_vis,
              float anim_time, float crt_flicker,
              const std::vector<float>*    burn,
              const std::vector<char32_t>* burn_ch)
        : buf_(buf), ps_(ps), star_(star),
          slideshow_(slideshow), luggage_run_(luggage_run),
          cursor_col_(cursor_col), cursor_row_(cursor_row),
          cursor_vis_(cursor_vis),
          anim_time_(anim_time), crt_flicker_(crt_flicker),
          burn_(burn), burn_ch_(burn_ch) {}

    void ComputeRequirement() override {
        requirement_.min_x = buf_.Cols();
        requirement_.min_y = buf_.Rows();
    }

    void Render(ftxui::Screen& screen) override {
        int ox = box_.x_min;
        int oy = box_.y_min;
        int w  = box_.x_max - box_.x_min + 1;
        int h  = box_.y_max - box_.y_min + 1;

        // ── Discworld rune set for digital rain background ────────────────────
        static constexpr const char* kRunes[] = {
            "\xe2\x80\xa0",  // †
            "\xce\xa8",      // Ψ
            "\xce\xa9",      // Ω
            "\xce\x94",      // Δ
            "\xce\x98",      // Θ
            "\xce\xa6",      // Φ
            "\xce\x9e",      // Ξ
            "\xc2\xa7",      // §
            "\xc2\xb6",      // ¶
            "\xe2\x88\x9e",  // ∞
            "\xe2\x89\x88",  // ≈
            "\xe2\x96\x93",  // ▓
            "\xe2\x96\x91",  // ░
            "\xe2\x94\x82",  // │
            "\xe2\x94\xbc",  // ┼
            ":", "|", "+", "/", "=", "\\", "0", "1",
        };
        static constexpr int kRuneCount = 23;

        // UTF-8 encoder (reused throughout render)
        auto encode_utf8 = [](char32_t ch) -> std::string {
            std::string s;
            if      (ch < 0x80u)    { s += static_cast<char>(ch); }
            else if (ch < 0x800u)   { s += static_cast<char>(0xC0u|(ch>>6u)); s += static_cast<char>(0x80u|(ch&0x3Fu)); }
            else if (ch < 0x10000u) { s += static_cast<char>(0xE0u|(ch>>12u)); s += static_cast<char>(0x80u|((ch>>6u)&0x3Fu)); s += static_cast<char>(0x80u|(ch&0x3Fu)); }
            else                    { s += static_cast<char>(0xF0u|(ch>>18u)); s += static_cast<char>(0x80u|((ch>>12u)&0x3Fu)); s += static_cast<char>(0x80u|((ch>>6u)&0x3Fu)); s += static_cast<char>(0x80u|(ch&0x3Fu)); }
            return s;
        };

        // Stable column hash for rain (no per-frame allocation)
        auto hf = [](unsigned x) -> float {
            x = ((x >> 16u) ^ x) * 0x45d9f3bu;
            x = ((x >> 16u) ^ x) * 0x45d9f3bu;
            return static_cast<float>(x % 10000u) / 10000.0f;
        };

        // ── Unified CRT render pass ───────────────────────────────────────────
        for (int row = 0; row < h; ++row) {
            // Scanlines: alternate rows at 58% brightness — visible CRT bands
            float scan = (row % 2 == 0) ? 0.58f : 1.0f;
            float vy   = static_cast<float>(row * 2 - h) / static_cast<float>(h > 1 ? h : 1);

            for (int col = 0; col < w; ++col) {
                // Vignette: quadratic darkening toward edges/corners
                float vx  = static_cast<float>(col * 2 - w) / static_cast<float>(w > 1 ? w : 1);
                float vig = 1.0f - 0.42f * (vx * vx + vy * vy);
                if (vig < 0.22f) vig = 0.22f;
                float crt = scan * vig * crt_flicker_;  // combined CRT multiplier

                ftxui::Pixel& px = screen.PixelAt(ox + col, oy + row);
                int bc = (col < buf_.Cols()) ? col : buf_.Cols() - 1;
                int br = (row < buf_.Rows()) ? row : buf_.Rows() - 1;

                // ── PTY content ──────────────────────────────────────────────
                if (bc >= 0 && br >= 0 && buf_.At(bc, br).ch != U' ') {
                    const Cell& cell = buf_.At(bc, br);
                    std::string s = encode_utf8(cell.ch);
                    px.character = s.empty() ? " " : s;
                    px.bold      = cell.bold;
                    if (cell.fg != ftxui::Color{}) px.foreground_color = cell.fg;
                    if (cell.bg != ftxui::Color{}) px.background_color = cell.bg;
                    // CRT scanline band: faint purple tint on even rows
                    if (row % 2 == 0)
                        px.background_color = ftxui::Color::RGB(4, 0, 7);
                    continue;
                }

                // ── Background slideshow art ──────────────────────────────────
                if (slideshow_) {
                    float art_bright = 0.0f;
                    int   art_pidx   = -1;
                    char  art_ch = slideshow_->char_at(col, row, art_bright, &art_pidx);
                    if (art_ch != ' ' && art_pidx >= 0) {
                        const ArtPiece ap = background_art::piece(art_pidx);
                        float b = art_bright * crt;
                        float lerp = art_bright;  // 0=dim colour, 1=bright
                        auto lc = [&](uint8_t d, uint8_t brt) -> uint8_t {
                            return static_cast<uint8_t>(std::min(255.0f,
                                (d + static_cast<float>(brt - d) * lerp) * b * 2.8f));
                        };
                        px.character        = std::string(1, art_ch);
                        px.foreground_color = ftxui::Color::RGB(
                            lc(ap.dim_r, ap.bright_r),
                            lc(ap.dim_g, ap.bright_g),
                            lc(ap.dim_b, ap.bright_b));
                        continue;
                    }
                }

                // ── Phosphor burn-in: recently-vacated cells linger ───────────
                if (burn_ && burn_ch_) {
                    int bidx = br * buf_.Cols() + bc;
                    if (bidx >= 0 && bidx < static_cast<int>(burn_->size()) &&
                        (*burn_)[static_cast<size_t>(bidx)] > 0.03f &&
                        (*burn_ch_)[static_cast<size_t>(bidx)] != U' ') {
                        std::string s = encode_utf8((*burn_ch_)[static_cast<size_t>(bidx)]);
                        if (!s.empty()) {
                            float bv = (*burn_)[static_cast<size_t>(bidx)] * crt * 0.55f;
                            // Octarine purple tint (RGB 204,136,255) at burn brightness
                            px.character        = s;
                            px.foreground_color = ftxui::Color::RGB(
                                static_cast<uint8_t>(std::min(255.0f, 204.0f * bv)),
                                static_cast<uint8_t>(std::min(255.0f, 136.0f * bv)),
                                static_cast<uint8_t>(std::min(255.0f, 255.0f * bv)));
                            continue;
                        }
                    }
                }

                // ── Digital rain: Discworld rune columns falling ──────────────
                {
                    float col_speed = 3.0f + hf(static_cast<unsigned>(col)) * 9.0f;
                    float col_phase = hf(static_cast<unsigned>(col) * 31u + 7u)
                                      * static_cast<float>(h > 0 ? h : 1);
                    float head_y    = std::fmod(anim_time_ * col_speed + col_phase,
                                               static_cast<float>(h > 0 ? h : 1));
                    float dist      = head_y - static_cast<float>(row);
                    if (dist < 0.0f) dist += static_cast<float>(h);

                    int ri = static_cast<int>(anim_time_ * 2.5f +
                                             static_cast<float>(col * 7 + row * 13));
                    ri = ((ri % kRuneCount) + kRuneCount) % kRuneCount;

                    uint8_t rr = 0, rg = 0, rb = 0;
                    bool visible = true;

                    if (dist < 1.5f) {
                        // Head — bright octarine-green (matches border neon)
                        float b = 0.88f * crt;
                        rr = static_cast<uint8_t>(std::min(255.0f, 200.0f * b));
                        rg = static_cast<uint8_t>(std::min(255.0f, 255.0f * b));
                        rb = static_cast<uint8_t>(std::min(255.0f, 200.0f * b));
                        px.bold = true;
                    } else if (dist < 15.0f) {
                        // Trail — octarine purple fading (RGB 204,136,255)
                        float fade = 1.0f - (dist - 1.5f) / 13.5f;
                        float b    = fade * 0.72f * crt;
                        rr = static_cast<uint8_t>(std::min(255.0f, 204.0f * b));
                        rg = static_cast<uint8_t>(std::min(255.0f, 136.0f * b));
                        rb = static_cast<uint8_t>(std::min(255.0f, 255.0f * b));
                    } else {
                        // Far static — sparse dim texture (not every cell)
                        unsigned noise = (static_cast<unsigned>(col) * 1301u +
                                         static_cast<unsigned>(row) * 4999u +
                                         static_cast<unsigned>(anim_time_ * 0.7f) * 97u)
                                        * 2654435761u;
                        float nf = static_cast<float>(noise % 1000u) / 1000.0f;
                        if (nf > 0.72f) {
                            float b = (0.05f + nf * 0.06f) * crt;
                            rr = static_cast<uint8_t>(std::min(255.0f, 74.0f * b * 3.8f));
                            rg = static_cast<uint8_t>(std::min(255.0f, 48.0f * b * 3.8f));
                            rb = static_cast<uint8_t>(std::min(255.0f, 96.0f * b * 3.8f));
                        } else {
                            visible = false;
                        }
                    }

                    if (visible) {
                        px.character        = kRunes[ri];
                        px.foreground_color = ftxui::Color::RGB(rr, rg, rb);
                    }
                }
            }
        }

        // ── Layer 2: particle overlay ─────────────────────────────────────────
        for (const auto& p : ps_.particles) {
            // Comet trail
            if (p.type == ParticleType::Comet) {
                int tx = static_cast<int>(p.prev_x);
                int ty = static_cast<int>(p.prev_y);
                if (tx >= 0 && tx < w && ty >= 0 && ty < h) {
                    int bc = std::min(tx, buf_.Cols() - 1);
                    int br = std::min(ty, buf_.Rows() - 1);
                    if (buf_.At(bc, br).ch == U' ') {
                        ftxui::Pixel& px = screen.PixelAt(ox + tx, oy + ty);
                        px.character = ".";
                        px.foreground_color = ftxui::Color::RGB(100, 90, 180);
                    }
                }
            }
            int col = static_cast<int>(p.x);
            int row = static_cast<int>(p.y);
            if (col < 0 || col >= w || row < 0 || row >= h) continue;
            int bc = std::min(col, buf_.Cols() - 1);
            int br = std::min(row, buf_.Rows() - 1);
            if (buf_.At(bc, br).ch == U' ') {
                const auto& def = particle_type_def(p.type);
                ftxui::Pixel& px = screen.PixelAt(ox + col, oy + row);
                px.character = p.glyph;
                px.foreground_color = ftxui::Color::RGB(def.r, def.g, def.b);
            }
        }

        // Block cursor
        if (cursor_vis_ && cursor_col_ < w && cursor_row_ < h) {
            screen.PixelAt(ox + cursor_col_, oy + cursor_row_).inverted = true;
        }

        // ── Shooting star ───────────────────────────────────────────────
        if (star_ && star_->active) {
            int  srow       = star_->row;
            int  scol       = static_cast<int>(star_->x);
            bool going_right = star_->speed > 0.0f;
            // Trail (6 chars behind the head)
            static constexpr const char* kTrail[] = { "-", "-", ".", ".", ".", "`" };
            for (int i = 1; i <= 6; ++i) {
                int tc = going_right ? scol - i : scol + i;
                if (tc < 0 || tc >= w || srow < 0 || srow >= h) continue;
                ftxui::Pixel& px = screen.PixelAt(ox + tc, oy + srow);
                float fade = 1.0f - (i / 7.0f);
                uint8_t rv = static_cast<uint8_t>(220 * fade);
                uint8_t gv = static_cast<uint8_t>(200 * fade);
                uint8_t bv = static_cast<uint8_t>(255 * fade);
                px.character        = kTrail[i - 1];
                px.foreground_color = ftxui::Color::RGB(rv, gv, bv);
            }
            // Star head
            if (scol >= 0 && scol < w && srow >= 0 && srow < h) {
                ftxui::Pixel& px    = screen.PixelAt(ox + scol, oy + srow);
                px.character        = "*";
                px.foreground_color = ftxui::Color::RGB(255, 255, 255);
                px.bold             = true;
            }
        }

        // ── Luggage run — scurries left-to-right across the top ─────────────────
        if (luggage_run_ && luggage_run_->active) {
            int  lx   = static_cast<int>(luggage_run_->x);
            bool legA = (static_cast<int>(luggage_run_->leg_anim * 6.0f) % 2) == 0;
            // 5-char wide, 3-row sprite:  .===.  |###|  /| |\  or  _| |_
            static constexpr char kLid []  = ".===";
            static constexpr char kBody[]  = "|###";
            static constexpr char kFeetA[] = "/| |";
            static constexpr char kFeetB[] = "_| |";
            const char* feet = legA ? kFeetA : kFeetB;
            constexpr int sw = 4;
            auto dpx = [&](int dr, int dc, char ch) {
                int sc = lx + dc, sr = dr;
                if (sc < 0 || sc >= w || sr < 0 || sr >= h || ch == ' ') return;
                ftxui::Pixel& px = screen.PixelAt(ox + sc, oy + sr);
                px.character        = std::string(1, ch);
                px.foreground_color = ftxui::Color::RGB(210, 160, 50);
                px.bold             = true;
            };
            for (int i = 0; i < sw; ++i) {
                dpx(0, i, kLid[i]);
                dpx(1, i, kBody[i]);
                dpx(2, i, feet[i]);
            }
            // Right wall of sprite
            dpx(0, sw, '.');
            dpx(1, sw, '|');
            dpx(2, sw, legA ? '\\' : '_');
        }
    }

private:
    CellBuffer&           buf_;
    ParticleSystem&       ps_;
    const ShootingStar*   star_;
    BackgroundSlideshow*  slideshow_;
    const LuggageRun*     luggage_run_;
    int  cursor_col_, cursor_row_;
    bool cursor_vis_;
    float anim_time_;
    float crt_flicker_;
    const std::vector<float>*    burn_;
    const std::vector<char32_t>* burn_ch_;
};


// ── TuiApp implementation ─────────────────────────────────────────────────────

TuiApp::TuiApp(Config cfg)
    : cfg_(std::move(cfg)),
      buf_(term_cols_, term_rows_),
      parser_(buf_) {
    cwd_ = get_cwd();
    particles_.Init(term_cols_, term_rows_, 15);
    slideshow_.set_terminal_size(term_cols_, term_rows_);
    quote_x_ = static_cast<float>(term_cols_);
    crt_burn_.assign(static_cast<size_t>(term_cols_ * term_rows_), 0.0f);
    crt_burn_ch_.assign(static_cast<size_t>(term_cols_ * term_rows_), U' ');
    crt_flicker_timer_ = 8.0f;  // first flicker check after 8 s
    last_tick_ = Clock::now();
    // Stagger initial event cooldowns so they don’t all fire at once
    static std::mt19937 erng{7777};
    for (int i = 0; i < kEventCount; ++i) {
        std::uniform_real_distribution<float> cd(
            kEventConfig[i].cd_min, kEventConfig[i].cd_max);
        events_[i].cooldown = cd(erng);
    }
}

void TuiApp::PollCwd() {
    cwd_ = get_cwd();
}

void TuiApp::SpawnShootingStar() {
    static std::mt19937 rng{12345};
    std::uniform_int_distribution<int> dir(0, 1);
    bool go_right = dir(rng) == 1;
    float speed   = go_right ? 180.0f : -180.0f;
    shoot_star_.x      = go_right ? -2.0f : static_cast<float>(term_cols_ + 2);
    shoot_star_.speed  = speed;
    shoot_star_.row    = std::max(0, buf_.CursorRow());
    shoot_star_.life   = (term_cols_ + 10) / 180.0f;
    shoot_star_.active = true;
}

void TuiApp::UpdateBoot(float delta) {
    const auto& script = boot_script();

    // All lines shown — wait out the end-of-boot pause before going Live.
    if (boot_line_index_ >= script.size()) {
        boot_line_timer_ -= delta;
        if (boot_line_timer_ > 0.0f) return;

        state_ = AppState::Live;
        std::string sh = cfg_.shell_path.empty() ? default_shell() : cfg_.shell_path;
        shell_->OnOutput([this](const std::string& data) {
            std::lock_guard<std::mutex> lk(data_mutex_);
            pending_data_ += data;
        });
        shell_->OnExit([this](int code) {
            exit_code_        = code;
            farewell_showing_ = true;
            farewell_timer_   = 2.5f;
        });
        shell_->Start(sh, term_cols_, term_rows_);
        return;
    }

    boot_line_timer_ -= delta;
    if (boot_line_timer_ <= 0.0f) {
        const BootLine& bl = script[boot_line_index_];
        boot_displayed_.push_back(bl.text);
        ++boot_line_index_;
        if (boot_line_index_ < script.size()) {
            boot_line_timer_ = script[boot_line_index_].delay_ms / 1000.0f;
        } else {
            // Linger 3 s after the last Death line so it can be read.
            boot_line_timer_ = 3.0f;
        }
    }
}

void TuiApp::UpdateLive(float delta) {
    std::string chunk;
    {
        std::lock_guard<std::mutex> lk(data_mutex_);
        chunk = std::move(pending_data_);
        pending_data_.clear();
    }
    if (!chunk.empty()) parser_.Feed(chunk);

    if (farewell_showing_) {
        farewell_timer_ -= delta;
        if (farewell_timer_ <= 0.0f) quit_ = true;
    }

    cwd_poll_timer_ -= delta;
    if (cwd_poll_timer_ <= 0.0f) {
        PollCwd();
        cwd_poll_timer_ = 2.0f;
    }

    label_timer_ -= delta;
    if (label_timer_ <= 0.0f) {
        label_index_ = (label_index_ + 1) % kLabelCount;
        label_timer_ = 8.0f;
    }

    // Flash timer
    if (flash_timer_ > 0.0f) flash_timer_ -= delta;

    // ── Background slideshow + Luggage run ───────────────────────────────────
    slideshow_.tick(delta);

    // ── CRT flicker ───────────────────────────────────────────────────────────
    {
        static std::mt19937 frng{9876};
        crt_flicker_timer_ -= delta;
        if (crt_flicker_timer_ <= 0.0f) {
            if (crt_flicker_ < 1.0f) {
                // End dim period — restore, schedule next check
                crt_flicker_ = 1.0f;
                std::uniform_real_distribution<float> wait(5.0f, 18.0f);
                crt_flicker_timer_ = wait(frng);
            } else {
                // Start a flicker: brief screen dim
                std::uniform_real_distribution<float> dim(0.62f, 0.87f);
                std::uniform_real_distribution<float> dur(0.04f, 0.14f);
                crt_flicker_      = dim(frng);
                crt_flicker_timer_ = dur(frng);
            }
        }
    }

    // ── Phosphor burn-in decay ────────────────────────────────────────────────
    {
        int ncells = buf_.Cols() * buf_.Rows();
        if (static_cast<int>(crt_burn_.size()) == ncells) {
            static constexpr float kBurnDecay = 0.55f;  // full fade in ~0.55 s
            for (int r = 0; r < buf_.Rows(); ++r) {
                for (int c = 0; c < buf_.Cols(); ++c) {
                    int idx = r * buf_.Cols() + c;
                    const Cell& cell = buf_.At(c, r);
                    if (cell.ch != U' ') {
                        crt_burn_[static_cast<size_t>(idx)] = 1.0f;
                        crt_burn_ch_[static_cast<size_t>(idx)] = cell.ch;
                    } else {
                        crt_burn_[static_cast<size_t>(idx)] -= delta / kBurnDecay;
                        if (crt_burn_[static_cast<size_t>(idx)] < 0.0f)
                            crt_burn_[static_cast<size_t>(idx)] = 0.0f;
                    }
                }
            }
        }
    }
    if (luggage_run_.active) {
        luggage_run_.x        += luggage_run_.speed * delta;
        luggage_run_.leg_anim += delta;
        if (luggage_run_.x > static_cast<float>(term_cols_ + 10))
            luggage_run_.active = false;
    }

    // ── Magical event system ─────────────────────────────────────────────────
    static std::mt19937 erng{1234};
    for (int i = 0; i < kEventCount; ++i) {
        EventState& ev  = events_[i];
        const auto& cfg = kEventConfig[i];

        if (ev.active) {
            ev.remaining -= delta;
            if (ev.remaining <= 0.0f) {
                ev.active = false;
                // Deactivate effects
                if (i == EVT_OCTARINE_STORM)    particles_.target_count = particles_.base_count;
                // Start next cooldown
                std::uniform_real_distribution<float> cd(cfg.cd_min, cfg.cd_max);
                ev.cooldown = cd(erng);
            }
        } else {
            ev.cooldown -= delta;
            if (ev.cooldown <= 0.0f) {
                ev.active      = true;
                ev.remaining   = cfg.duration;
                flash_message_ = cfg.label;
                flash_timer_   = 1.5f;
                // Activate effects
                if (i == EVT_OCTARINE_STORM) {
                    particles_.target_count = particles_.base_count * 8;
                    slideshow_.flash(0);  // Death art
                }
                if (i == EVT_NARRATIVIUM_SURGE) slideshow_.flash(2);  // Rincewind art
                if (i == EVT_LUGGAGE_RAMPAGE) {
                    slideshow_.flash(1);  // Luggage art
                    luggage_run_.x        = static_cast<float>(-5);
                    luggage_run_.speed    = 45.0f;
                    luggage_run_.leg_anim = 0.0f;
                    luggage_run_.active   = true;
                }
            }
        }
    }

    // NarrativiumSurge: spawn one rune per column per frame while active
    if (events_[EVT_NARRATIVIUM_SURGE].active) {
        for (int col = 0; col < term_cols_; ++col)
            particles_.SpawnRune(col);
    }
}


void TuiApp::OnTick(float delta, ftxui::ScreenInteractive& screen) {
    anim_time_ += delta;

    // Advance quote ticker (always, regardless of state)
    quote_x_ -= kQuoteSpeed * delta;
    if (quote_x_ + static_cast<float>(std::strlen(kDiscworldQuotes[quote_index_])) < 0.0f) {
        quote_index_ = (quote_index_ + 1) % kQuoteCount;
        quote_x_ = static_cast<float>(term_cols_);
    }

    if (shoot_star_.active) {
        shoot_star_.x    += shoot_star_.speed * delta;
        shoot_star_.life -= delta;
        if (shoot_star_.life <= 0.0f) shoot_star_.active = false;
    }
    particles_.Update(delta, term_cols_, term_rows_);

    if (state_ == AppState::Boot)           UpdateBoot(delta);
    else if (state_ == AppState::Live)      UpdateLive(delta);

    if (quit_) screen.ExitLoopClosure()();
}

// ── Rendering ─────────────────────────────────────────────────────────────────

ftxui::Element TuiApp::RenderTerminalArea(int /*cols*/, int /*rows*/) {
    return std::make_shared<TerryNode>(
        buf_, particles_, &shoot_star_,
        &slideshow_, &luggage_run_,
        buf_.CursorCol(), buf_.CursorRow(), buf_.CursorVisible(),
        anim_time_, crt_flicker_,
        &crt_burn_, &crt_burn_ch_);
}

ftxui::Element TuiApp::RenderBoot(int /*cols*/, int /*rows*/) {
    const auto& script = boot_script();
    std::vector<ftxui::Element> lines;
    lines.push_back(text(""));
    for (size_t i = 0; i < boot_displayed_.size(); ++i) {
        const std::string& l = boot_displayed_[i];
        if (i < script.size() && script[i].is_death) {
            lines.push_back(text(l) | color(theme_.death) | bold);
        } else {
            lines.push_back(text(l) | color(theme_.text));
        }
    }
    lines.push_back(text("_") | color(theme_.highlight) | blink);
    return vbox(std::move(lines)) | center;
}

ftxui::Element TuiApp::RenderStatusBar(int /*cols*/) {
    std::string sh = cfg_.shell_path.empty() ? default_shell() : cfg_.shell_path;
    auto slash = sh.find_last_of("/\\");
    if (slash != std::string::npos) sh = sh.substr(slash + 1);

    std::string cwd_str = str_truncate(cwd_, 30);
    std::string label   = (flash_timer_ > 0.0f) ? flash_message_ : kLabels[label_index_];
    std::string timestr = current_time_str();

    return hbox({
        text(" " + cwd_str + " ") | color(theme_.text),
        separator(),
        text(" " + sh + " ")      | color(theme_.dim),
        separator(),
        text(" " + label + " ")   | color(theme_.highlight),
        separator(),
        text(" " + timestr + " ") | color(theme_.text),
    });
}

ftxui::Element TuiApp::RenderBorder(ftxui::Element inner, int /*cols*/, int /*rows*/) {
    return inner | border | color(theme_.BorderColor(anim_time_));
}

ftxui::Element TuiApp::RenderFrame(int total_cols, int total_rows) {
    int inner_rows = std::max(1, total_rows - 6);
    int inner_cols = std::max(1, total_cols - 2);

    if (inner_cols != term_cols_ || inner_rows != term_rows_) {
        term_cols_ = inner_cols;
        term_rows_ = inner_rows;
        buf_.Resize(term_cols_, term_rows_);
        particles_.Init(term_cols_, term_rows_, particles_.base_count);
        slideshow_.set_terminal_size(term_cols_, term_rows_);
        crt_burn_.assign(static_cast<size_t>(term_cols_ * term_rows_), 0.0f);
        crt_burn_ch_.assign(static_cast<size_t>(term_cols_ * term_rows_), U' ');
        if (shell_ && shell_->IsRunning())
            shell_->Resize(term_cols_, term_rows_);
    }

    ftxui::Color bc = theme_.BorderColor(anim_time_);

    ftxui::Element content;
    if (state_ == AppState::Boot) {
        content = RenderBoot(inner_cols, inner_rows);
    } else if (state_ == AppState::LuggageBrowser) {
        content = RenderLuggageBrowser(inner_cols, inner_rows);
    } else if (state_ == AppState::LuggageInstall) {
        content = RenderLuggageInstall(inner_cols, inner_rows);
    } else if (state_ == AppState::Rincewind) {
        content = RenderRincewind(inner_cols, inner_rows);
    } else if (farewell_showing_) {
        content = vbox({
            filler(),
            text("  DEATH: AND SO IT ENDS. I TRUST YOU USED THE TIME WISELY.") | color(theme_.death) | bold,
            filler(),
        });
    } else {
        content = RenderTerminalArea(inner_cols, inner_rows);
    }
    content = content | size(WIDTH, EQUAL, inner_cols)
                      | size(HEIGHT, EQUAL, inner_rows);

    // Top border title line (Unicode box-drawing characters)
    std::string title_str = "\u255e TERRY \u2561";  // ╞ TERRY ╡
    int pad = std::max(0, (inner_cols - static_cast<int>(title_str.size())) / 2);
    std::string title_padded(static_cast<size_t>(pad), ' ');
    title_padded += title_str;
    // (Rincewind now rendered as sprite overlay on terminal content, not in title)

    auto status = RenderStatusBar(inner_cols);

    // Build scrolling quote line (ASCII viewport, inner_cols wide)
    std::string quote_line(static_cast<size_t>(inner_cols), ' ');
    {
        const char* q  = kDiscworldQuotes[quote_index_];
        int qlen = static_cast<int>(std::strlen(q));
        int qx   = static_cast<int>(quote_x_);
        for (int c = 0; c < inner_cols; ++c) {
            int qi = c - qx;
            if (qi >= 0 && qi < qlen) quote_line[static_cast<size_t>(c)] = q[qi];
        }
    }

    return vbox({
        hbox({
            text("\u2554") | color(bc),   // ╔
            text(title_padded) | color(bc),
            filler(),
            text("\u2557") | color(bc),   // ╗
        }),
        hbox({
            text("\u2551") | color(bc),   // ║
            content,
            text("\u2551") | color(bc),   // ║
        }),
        hbox({
            text("\u2560") | color(bc),   // ╠
            separatorDouble() | color(bc),
            text("\u2563") | color(bc),   // ╣
        }),
        hbox({
            text("\u2551") | color(bc),   // ║
            status | flex,
            text("\u2551") | color(bc),   // ║
        }),
        hbox({
            text("\u2560") | color(bc),   // ╠
            separatorDouble() | color(bc),
            text("\u2563") | color(bc),   // ╣
        }),
        hbox({
            text("\u2551") | color(bc),                                              // ║
            text(quote_line) | color(ftxui::Color::RGB(140, 90, 200)),             // quote
            text("\u2551") | color(bc),                                              // ║
        }),
        hbox({
            text("\u255a") | color(bc),   // ╚
            separator() | color(bc),
            text("\u255d") | color(bc),   // ╝
        }),
    });
}

// ── Luggage ───────────────────────────────────────────────────────────────────

void TuiApp::ResetLuggageState() {
    if (luggage_thread_.joinable()) luggage_thread_.detach();
    {
        std::lock_guard<std::mutex> lk(luggage_log_mutex_);
        luggage_log_.clear();
    }
    luggage_selected_.clear();
    luggage_input_buf_.clear();
    luggage_install_path_.clear();
    luggage_installing_ = false;
    luggage_done_       = false;
    state_ = AppState::Live;
}

ftxui::Element TuiApp::RenderLuggageBrowser(int cols, int rows) {
    using namespace ftxui;
    Color title_col  = Color::RGB(255, 200, 50);  // Luggage gold
    Color check_col  = Color::RGB(100, 220, 100);
    Color cursor_col = Color::RGB(255, 200, 50);
    Color dim_col    = theme_.dim;

    // Count selected
    int sel_count = 0;
    for (bool s : luggage_selected_) if (s) ++sel_count;

    // Viewport: rows minus header (3) and footer (2)
    int visible = std::max(1, rows - 5);

    // Clamp scroll so cursor is always in viewport
    if (luggage_cursor_ < luggage_scroll_)
        luggage_scroll_ = luggage_cursor_;
    if (luggage_cursor_ >= luggage_scroll_ + visible)
        luggage_scroll_ = luggage_cursor_ - visible + 1;
    luggage_scroll_ = std::max(0, std::min(luggage_scroll_, kSkillRegistrySize - visible));

    // Header
    std::string title_str = " THE LUGGAGE ";
    int pad = std::max(0, (cols - 2 - static_cast<int>(title_str.size())) / 2);
    std::string title_padded(static_cast<size_t>(pad), '=');
    title_padded += title_str;

    std::vector<Element> rows_elems;
    rows_elems.push_back(
        text(title_padded) | bold | color(title_col));
    rows_elems.push_back(
        text("  \u2191\u2193/jk navigate   SPACE select   ENTER confirm   ESC cancel")
        | color(dim_col));
    rows_elems.push_back(separator() | color(title_col));

    // Skill rows
    for (int i = luggage_scroll_; i < std::min(kSkillRegistrySize, luggage_scroll_ + visible); ++i) {
        const SkillEntry& sk = kSkillRegistry[i];
        bool selected  = luggage_selected_[i];
        bool is_cursor = (i == luggage_cursor_);

        std::string checkbox = selected ? "[x] " : "[ ] ";
        std::string label_s  = sk.label;
        // Pad label to 20 chars
        while (static_cast<int>(label_s.size()) < 20) label_s += ' ';
        std::string desc_s = sk.description;

        Element chk_elem = text(checkbox) | color(selected ? check_col : dim_col);
        Element lbl_elem = text(label_s)  | color(is_cursor ? cursor_col : theme_.text);
        Element dsc_elem = text(desc_s)   | color(dim_col) | flex;
        Element row_elem = hbox({ chk_elem, lbl_elem, text("  "), dsc_elem });

        if (is_cursor) row_elem = row_elem | inverted;
        rows_elems.push_back(std::move(row_elem));
    }

    // Filler to push footer to bottom
    rows_elems.push_back(filler());

    // Footer
    rows_elems.push_back(separator() | color(title_col));
    rows_elems.push_back(
        text("  " + std::to_string(sel_count) + " skill(s) selected"
             + (sel_count > 0 ? "  \u2014 press ENTER to install" : ""))
        | color(sel_count > 0 ? check_col : dim_col));

    return vbox(std::move(rows_elems))
           | size(WIDTH, EQUAL, cols)
           | size(HEIGHT, EQUAL, rows);
}

ftxui::Element TuiApp::RenderLuggageInstall(int cols, int rows) {
    using namespace ftxui;
    Color title_col = Color::RGB(255, 200, 50);
    Color ok_col    = Color::RGB(100, 220, 100);
    Color err_col   = Color::RGB(255, 80, 80);

    std::string title_str = " THE LUGGAGE — INSTALL ";
    int pad = std::max(0, (cols - 2 - static_cast<int>(title_str.size())) / 2);
    std::string title_padded(static_cast<size_t>(pad), '=');
    title_padded += title_str;

    std::vector<Element> elems;
    elems.push_back(text(title_padded) | bold | color(title_col));
    elems.push_back(separator() | color(title_col));

    if (!luggage_installing_) {
        // Input phase
        elems.push_back(text("") );
        elems.push_back(text("  Where should the skills workspace be created?") | color(theme_.text));
        elems.push_back(text(""));
        elems.push_back(
            hbox({
                text("  Install to: ") | color(theme_.dim),
                text(luggage_input_buf_) | color(theme_.highlight),
                text("_") | color(title_col) | blink,
            }));
        elems.push_back(text(""));
        elems.push_back(text("  Example: C:\\Users\\Rhys\\Documents\\my-workspace") | color(theme_.dim));
        elems.push_back(text("  Press ENTER to install, ESC to go back.") | color(theme_.dim));
    } else {
        // Progress / done phase
        std::vector<std::string> log_snap;
        {
            std::lock_guard<std::mutex> lk(luggage_log_mutex_);
            log_snap = luggage_log_;
        }
        for (const auto& line : log_snap) {
            Color c = theme_.text;
            if (!line.empty()) {
                if (line[0] == '+') c = ok_col;
                if (line[0] == 'x' || line[0] == '!') c = err_col;
            }
            elems.push_back(text("  " + line) | color(c));
        }
    }

    elems.push_back(filler());

    return vbox(std::move(elems))
           | size(WIDTH, EQUAL, cols)
           | size(HEIGHT, EQUAL, rows);
}

void TuiApp::DoLuggageInstall() {
    namespace fs = std::filesystem;

    // Snapshot selection and path (UI thread may change state)
    std::vector<int> selected;
    for (int i = 0; i < (int)luggage_selected_.size(); ++i)
        if (luggage_selected_[i]) selected.push_back(i);
    const std::string install_path = luggage_install_path_;

    auto post = [this](const std::string& line) {
        std::lock_guard<std::mutex> lk(luggage_log_mutex_);
        luggage_log_.push_back(line);
    };

    // Validate path
    if (install_path.empty() ||
        install_path.find("..") != std::string::npos ||
        install_path.size() > 500) {
        post("x Invalid install path.");
        luggage_done_ = true;
        return;
    }

    // Create root directory
    std::error_code ec;
    fs::create_directories(install_path, ec);
    if (ec) {
        post("x Could not create directory: " + ec.message());
        luggage_done_ = true;
        return;
    }

    post("+ Created " + install_path);

    // Create workspace folders: input/, output/, theme/raw/
    auto make_folder = [&](const std::string& rel) {
        std::string dir = install_path + "/" + rel;
        fs::create_directories(dir, ec);
        if (ec) {
            post("! Could not create " + rel + ": " + ec.message());
        } else {
            std::ofstream gk(dir + "/.gitkeep"); // keep empty dir in git
            post("+ Created " + rel + "/");
        }
    };
    make_folder("input");
    make_folder("output");
    make_folder("theme/raw");

    // Write each selected skill's SKILL.md
    int written = 0;
    std::vector<const SkillEntry*> installed_skills;

    for (int idx : selected) {
        if (idx < 0 || idx >= kSkillRegistrySize) continue;
        const SkillEntry& sk = kSkillRegistry[idx];

        std::string skill_dir = install_path + "/.github/skills/" + sk.id;
        fs::create_directories(skill_dir, ec);
        if (ec) {
            post("! Could not create dir for " + std::string(sk.id));
            continue;
        }

        std::ofstream skill_f(skill_dir + "/SKILL.md", std::ios::binary);
        if (!skill_f) {
            post("! Could not write " + std::string(sk.id) + "/SKILL.md");
            continue;
        }
        skill_f << sk.content;
        skill_f.close();
        post("+ " + std::string(sk.label));
        installed_skills.push_back(&sk);
        ++written;
    }

    // Always install theme-extract skill (workspace scaffold — not user-selectable)
    {
        std::string te_dir = install_path + "/.github/skills/theme-extract";
        fs::create_directories(te_dir, ec);
        if (!ec) {
            std::ofstream te(te_dir + "/SKILL.md");
            if (te) {
                te <<
"---\n"
"name: theme-extract\n"
"description: Extract brand/theme from documents in theme/raw/ and write theme/brand-guide.md. Triggers on generate my theme, extract theme, analyze brand docs, create brand guide.\n"
"---\n"
"\n"
"Extract a persistent brand guide from reference documents in theme/raw/.\n"
"\n"
"**Triggers:** \"generate my theme\", \"extract theme\", \"analyze my brand docs\",\n"
"\"create brand guide\", or any request to process files in theme/raw/.\n"
"\n"
"**Steps**\n"
"\n"
"1. **Check theme/raw/** - list all files. If empty, ask the user to drop\n"
"   brand documents in first (PDFs, PowerPoints, style guides, Word docs).\n"
"\n"
"2. **Read every file** Copilot can access. Extract:\n"
"   - Colour palette: primary, secondary, accent, background, text\n"
"     (extract exact hex codes where present; note estimated values)\n"
"   - Typography: heading font, body font, weights, size ranges\n"
"   - Logo: name, usage rules, approved background combinations\n"
"   - Tone of voice: formal/casual, key descriptors, things to avoid\n"
"   - Layout / presentation rules: header/footer patterns, bullet styles,\n"
"     slide structure, clear space rules\n"
"   - Brand name: exact capitalisation / formatting rules\n"
"\n"
"3. **Write theme/brand-guide.md** using this structure:\n"
"\n"
"   # Brand Guide - [Brand Name]\n"
"   > Generated from theme/raw/ - edit to update. All AI output uses these values.\n"
"\n"
"   ## Brand\n"
"   ## Colours        (table: name, hex, RGB, pantone if known)\n"
"   ## Typography     (table: weight, use, size range)\n"
"   ## Presentation Layout Rules\n"
"   ## Logo Usage Rules\n"
"   ## Logo Backgrounds\n"
"   ## Tone of Voice\n"
"   ## Assets\n"
"   ## Rules\n"
"   ## Source Documents\n"
"\n"
"4. **Confirm** - tell the user brand-guide.md was written and is now active.\n"
"   All future document/presentation/design output will use this theme.\n"
"\n"
"**Important**\n"
"- Never overwrite an existing theme/brand-guide.md without asking first.\n"
"- Note any values that were estimated rather than explicitly stated.\n"
"- The .potx / .pptx files in theme/raw/ remain there - reference them\n"
"  directly via the pptx skill when creating presentations.\n";
                post("+ theme-extract (workspace scaffold)");
            }
        }
    }

    // Write copilot-instructions.md
    {
        std::ofstream ci(install_path + "/copilot-instructions.md");
        if (ci) {
            ci << "<!-- Generated by Terry Luggage Skill Browser -->\n"
               << "<!-- Open this folder in VS Code to activate the installed skills. -->\n\n"
               << "## Workspace Conventions\n\n"
               << "### Input\n"
               << "Files in `input/` are raw material. Read them when the user asks you to work with\n"
               << "documents. Never modify input files.\n\n"
               << "### Output\n"
               << "All generated files go in `output/`. Use descriptive filenames. Never overwrite\n"
               << "without asking.\n\n"
               << "### Theme\n"
               << "`theme/brand-guide.md` is the persistent brand guide for this workspace. Before\n"
               << "creating ANY document, presentation, or visual output:\n"
               << "1. Check if `theme/brand-guide.md` exists. If yes, read it and apply the brand.\n"
               << "2. If it does not exist yet, tell the user to run \"generate my theme\" first.\n\n"
               << "`theme/raw/` contains the source brand documents used to generate the guide.\n\n"
               << "<skills>\n";
            for (const SkillEntry* sk : installed_skills) {
                ci << "<skill>\n"
                   << "<name>" << sk->id << "</name>\n"
                   << "<description>" << sk->description << "</description>\n"
                   << "<file>.github/skills/" << sk->id << "/SKILL.md</file>\n"
                   << "</skill>\n";
            }
            ci << "<skill>\n"
               << "<name>theme-extract</name>\n"
               << "<description>Extract brand/theme from documents in theme/raw/ and write theme/brand-guide.md</description>\n"
               << "<file>.github/skills/theme-extract/SKILL.md</file>\n"
               << "</skill>\n"
               << "</skills>\n";
        }
    }

    // Write skills-lock.json
    {
        // Get current UTC time as ISO 8601
        std::time_t now = std::time(nullptr);
        std::tm utc = {};
#ifdef _WIN32
        gmtime_s(&utc, &now);
#else
        gmtime_r(&now, &utc);
#endif
        char ts[32] = {};
        std::strftime(ts, sizeof(ts) - 1, "%Y-%m-%dT%H:%M:%SZ", &utc);

        std::ofstream lf(install_path + "/skills-lock.json");
        if (lf) {
            lf << "{\n  \"version\": 1,\n  \"generatedBy\": \"terry-luggage\",\n  \"skills\": {\n";
            bool first = true;
            for (const SkillEntry* sk : installed_skills) {
                if (!first) lf << ",\n";
                lf << "    \"" << sk->id << "\": {\n"
                   << "      \"source\": \"" << sk->source_repo << "\",\n"
                   << "      \"skillPath\": \"" << sk->skill_path << "\",\n"
                   << "      \"installedAt\": \"" << ts << "\"\n"
                   << "    }";
                first = false;
            }
            lf << "\n  }\n}\n";
        }
    }

    // Write SKILLS.md
    {
        std::ofstream sm(install_path + "/SKILLS.md");
        if (sm) {
            sm << "# Your AI Workspace\n\n"
               << "Set up by Terry Luggage. Open this folder in VS Code with GitHub Copilot "
               << "installed.\nEvery skill listed below is live immediately \xe2\x80\x94 no setup required.\n\n"
               << "## Your Workspace\n\n"
               << "| Folder | Purpose |\n"
               << "|--------|---------|\n"
               << "| `input/` | Drop source files here (Word, PDF, PowerPoint, text) |\n"
               << "| `output/` | All AI-generated files land here |\n"
               << "| `theme/raw/` | Drop brand documents here (style guides, decks, PDFs) |\n"
               << "| `theme/brand-guide.md` | Your persisted brand \xe2\x80\x94 generated once, used forever |\n\n"
               << "**First-time setup:** Drop your brand documents into `theme/raw/`, then tell your\n"
               << "AI: *\"generate my theme\"*. Takes 30 seconds. Never needs repeating unless your brand changes.\n\n"
               << "**Day-to-day:** Drop files into `input/`, describe what you want, find results in `output/`.\n\n";

            // Recommended skills
            sm << "## Always active\n\n"
               << "These are installed in every workspace. They activate automatically.\n\n"
               << "| Skill | What it does | How to use |\n"
               << "|-------|-------------|------------|\n";
            for (const SkillEntry* sk : installed_skills) {
                if (sk->recommended) {
                    sm << "| " << sk->label << " | " << sk->description
                       << " | Active automatically. |\n";
                }
            }

            // Optional skills
            bool has_optional = false;
            for (const SkillEntry* sk : installed_skills)
                if (!sk->recommended) { has_optional = true; break; }

            if (has_optional) {
                sm << "\n## Also installed\n\n"
                   << "| Skill | What it does | How to use |\n"
                   << "|-------|-------------|------------|\n";
                for (const SkillEntry* sk : installed_skills) {
                    if (!sk->recommended) {
                        sm << "| " << sk->label << " | " << sk->description
                           << " | Just ask your AI naturally. |\n";
                    }
                }
            }

            sm << "\n## Tips\n\n"
               << "- Just talk to your AI. Skills activate based on what you ask for.\n"
               << "- If responses feel too long, type `/caveman`.\n"
               << "- To start a new project: `/opsx:propose <idea>`\n"
               << "- To see what's installed: open `.github/skills/` in VS Code Explorer\n";
        }
    }

    post("");
    post("+ " + std::to_string(written) + " skill(s) installed to " + install_path);
    post("  Drop brand docs into theme/raw/ then say \"generate my theme\".");
    post("  Open in VS Code with Copilot to activate. Type rincewind to open now.");
    post("  Press any key to return to the shell.");
    luggage_done_ = true;
}

// ── Rincewind ─────────────────────────────────────────────────────────────────

void TuiApp::ResetRincewindState() {
    // Detach thread if it's still running so we don't block
    if (rincewind_thread_.joinable()) rincewind_thread_.detach();
    {
        std::lock_guard<std::mutex> lk(rincewind_log_mutex_);
        rincewind_log_.clear();
    }
    rincewind_done_  = false;
    rincewind_error_ = false;
    rincewind_cwd_.clear();
    state_ = AppState::Live;
}

void TuiApp::StartRincewindWorker() {
    if (rincewind_thread_.joinable()) rincewind_thread_.detach();

    rincewind_thread_ = std::thread([this] {
        // Helper: append a log line and trigger a TUI redraw.
        // Captured by pointer — the lambda lifetime is scoped to this thread.
        // Note: screen is not accessible here, so we use the timer loop's
        // PostEvent path by writing to the log; the render loop picks it up
        // on the next frame.
        auto post = [this](const std::string& line) {
            std::lock_guard<std::mutex> lk(rincewind_log_mutex_);
            rincewind_log_.push_back(line);
        };

        post("Checking for VS Code...");
        std::string code_path = DetectVsCode();

        if (code_path.empty()) {
            post("VS Code not found. Installing...");
            if (!InstallVsCode(post)) {
                rincewind_error_ = true;
                rincewind_done_  = true;
                return;
            }
            code_path = DetectVsCode();
            if (code_path.empty()) {
                post("x VS Code installed but could not be located.");
                rincewind_error_ = true;
                rincewind_done_  = true;
                return;
            }
        } else {
            post("+ VS Code found: " + code_path);
        }

        // Install extensions (best-effort)
        InstallExtension(code_path, "github.copilot",      post);
        InstallExtension(code_path, "github.copilot-chat", post);

        // Open VS Code at the captured CWD
        OpenVsCode(code_path, rincewind_cwd_, post);

        post("");
        if (!rincewind_error_) {
            post("  VS Code is open. Press any key to return to the shell.");
        } else {
            post("  Completed with errors. Press any key to return to the shell.");
        }
        rincewind_done_ = true;
    });
}

ftxui::Element TuiApp::RenderRincewind(int cols, int /*rows*/) {
    using namespace ftxui;
    ftxui::Color title_col = Color::RGB(180, 100, 255);  // octarine purple

    // Snapshot log under lock
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lk(rincewind_log_mutex_);
        lines = rincewind_log_;
    }

    std::vector<Element> log_elems;
    for (const auto& l : lines) {
        Color c = theme_.text;
        if (!l.empty()) {
            if (l[0] == '+') c = Color::RGB(0, 220, 100);   // green = success
            if (l[0] == 'x') c = Color::RGB(255, 80, 80);   // red = error
            if (l[0] == '!') c = Color::RGB(255, 180, 0);   // amber = warning
        }
        log_elems.push_back(text("  " + l) | color(c));
    }
    if (log_elems.empty()) {
        log_elems.push_back(text("") | color(theme_.dim));
    }

    Element log_box = vbox(std::move(log_elems))
                      | size(WIDTH, EQUAL, cols - 2);

    std::string title_text = " RINCEWIND ";
    int pad = std::max(0, (cols - 2 - static_cast<int>(title_text.size())) / 2);
    std::string title_padded(static_cast<size_t>(pad), '=');
    title_padded += title_text;

    return vbox({
        text(title_padded) | bold | color(title_col),
        separator()        | color(title_col),
        log_box,
        filler(),
    });
}

// ── Run() ─────────────────────────────────────────────────────────────────────

int TuiApp::Run() {
    shell_ = ShellHost::Create();

    auto screen = ftxui::ScreenInteractive::Fullscreen();

    auto renderer = Renderer([&] {
        ftxui::Dimensions dims = ftxui::Terminal::Size();
        return RenderFrame(dims.dimx, dims.dimy);
    });

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        if (event == Event::Custom) {
            auto now = Clock::now();
            float delta = std::chrono::duration<float>(now - last_tick_).count();
            last_tick_  = now;
            if (delta > 0.1f) delta = 0.1f;
            OnTick(delta, screen);
            return false;
        }

        if (state_ == AppState::Boot) {
            if (event.is_character() || event == Event::Return || event == Event::Escape) {
                boot_line_index_ = boot_script().size();
                boot_line_timer_ = 0.0f;
            }
            return true;
        }

        if (state_ == AppState::Live && shell_ && shell_->IsRunning()) {
            if (event.is_character()) {
                typed_line_ += event.character();
                shell_->Write(event.character());
                return true;
            }
            if (event == Event::Return) {
                if (typed_line_ == "luggage") {
                    typed_line_.clear();
                    state_ = AppState::LuggageBrowser;
                    luggage_selected_.assign(kSkillRegistrySize, false);
                    // Pre-select recommended skills
                    for (int i = 0; i < kSkillRegistrySize; ++i)
                        luggage_selected_[i] = kSkillRegistry[i].recommended;
                    luggage_cursor_ = 0;
                    luggage_scroll_ = 0;
                    return true;
                }
                if (typed_line_ == "rincewind") {
                    typed_line_.clear();
                    rincewind_cwd_ = get_cwd();
                    state_ = AppState::Rincewind;
                    StartRincewindWorker();
                    return true;
                }
                typed_line_.clear();
                SpawnShootingStar();
                shell_->Write("\r");
                return true;
            }
            if (event == Event::Backspace) {
                if (!typed_line_.empty()) typed_line_.pop_back();
                shell_->Write("\x7f");
                return true;
            }
            if (event == Event::Tab)        { shell_->Write("\t");      return true; }
            if (event == Event::Escape)     { typed_line_.clear(); shell_->Write("\x1b");    return true; }
            if (event == Event::ArrowUp)    { typed_line_.clear(); shell_->Write("\x1b[A");  return true; }
            if (event == Event::ArrowDown)  { typed_line_.clear(); shell_->Write("\x1b[B");  return true; }
            if (event == Event::ArrowRight) { shell_->Write("\x1b[C");  return true; }
            if (event == Event::ArrowLeft)  { shell_->Write("\x1b[D");  return true; }
            if (event == Event::Home)       { typed_line_.clear(); shell_->Write("\x1b[H");  return true; }
            if (event == Event::End)        { shell_->Write("\x1b[F");  return true; }
            if (event == Event::Delete)     { shell_->Write("\x1b[3~"); return true; }
            if (event == Event::PageUp)     { shell_->Write("\x1b[5~"); return true; }
            if (event == Event::PageDown)   { shell_->Write("\x1b[6~"); return true; }
            if (!event.input().empty()) { shell_->Write(event.input()); return true; }
        }

        if (state_ == AppState::Rincewind) {
            if (event == Event::Escape) {
                ResetRincewindState();
                return true;
            }
            if (rincewind_done_) {
                ResetRincewindState();
                return true;
            }
            return true;
        }

        if (state_ == AppState::LuggageBrowser) {
            int list_size = kSkillRegistrySize;
            if (event == Event::ArrowUp ||
                (event.is_character() && event.character() == "k")) {
                if (luggage_cursor_ > 0) --luggage_cursor_;
            } else if (event == Event::ArrowDown ||
                       (event.is_character() && event.character() == "j")) {
                if (luggage_cursor_ < list_size - 1) ++luggage_cursor_;
            } else if (event.is_character() && event.character() == " ") {
                luggage_selected_[luggage_cursor_] = !luggage_selected_[luggage_cursor_];
            } else if (event == Event::Return) {
                bool any = false;
                for (bool s : luggage_selected_) if (s) { any = true; break; }
                if (any) {
                    state_ = AppState::LuggageInstall;
                    luggage_input_buf_.clear();
                    luggage_install_path_.clear();
                    luggage_installing_ = false;
                    luggage_done_       = false;
                    {
                        std::lock_guard<std::mutex> lk(luggage_log_mutex_);
                        luggage_log_.clear();
                    }
                }
            } else if (event == Event::Escape) {
                state_ = AppState::Live;
                luggage_selected_.clear();
            } else if (event.is_mouse()) {
                auto& m = event.mouse();
                if (m.button == ftxui::Mouse::WheelUp && luggage_scroll_ > 0)
                    --luggage_scroll_;
                if (m.button == ftxui::Mouse::WheelDown &&
                    luggage_scroll_ < list_size - 1)
                    ++luggage_scroll_;
            }
            return true;
        }

        if (state_ == AppState::LuggageInstall) {
            if (!luggage_installing_) {
                // Input phase
                if (event.is_character()) {
                    luggage_input_buf_ += event.character();
                } else if (event == Event::Backspace) {
                    if (!luggage_input_buf_.empty()) luggage_input_buf_.pop_back();
                } else if (event == Event::Return) {
                    if (!luggage_input_buf_.empty()) {
                        luggage_install_path_ = luggage_input_buf_;
                        luggage_installing_   = true;
                        if (luggage_thread_.joinable()) luggage_thread_.detach();
                        luggage_thread_ = std::thread([this] { DoLuggageInstall(); });
                    }
                } else if (event == Event::Escape) {
                    state_ = AppState::LuggageBrowser;
                }
            } else if (luggage_done_) {
                // Done phase — any key returns to shell
                ResetLuggageState();
            }
            return true;
        }

        return false;
    });

    // 30fps timer thread
    std::thread timer_thread([&] {
        while (!quit_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            screen.PostEvent(Event::Custom);
        }
    });

    const auto& script = boot_script();
    if (!script.empty())
        boot_line_timer_ = script[0].delay_ms / 1000.0f;

    screen.Loop(component);

    quit_ = true;
    if (timer_thread.joinable()) timer_thread.join();

    return exit_code_;
}

} // namespace terry
