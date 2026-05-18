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
};
static constexpr int kLabelCount = 6;

// ── Magical event config ──────────────────────────────────────────────────────

struct EventConfig {
    const char* label;
    float cd_min, cd_max;  // cooldown range (seconds)
    float duration;         // active duration (seconds)
};

static const EventConfig kEventConfig[kEventCount] = {
    {"OCTARINE STORM",           120.0f, 480.0f, 4.0f},
    {"NARRATIVIUM SURGE",        180.0f, 480.0f, 2.0f},
    {"THE LUGGAGE IS DISPLEASED", 90.0f, 480.0f, 6.0f},
};

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

// Split a UTF-8 string into one std::string per Unicode codepoint
static std::vector<std::string> split_utf8(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        auto c = static_cast<unsigned char>(s[i]);
        int len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        out.push_back(s.substr(i, static_cast<size_t>(len)));
        i += static_cast<size_t>(len);
    }
    return out;
}

// ── TerryNode — custom FTXUI node rendering our terminal cell buffer ──────────

class TerryNode : public ftxui::Node {
public:
    TerryNode(CellBuffer& buf, ParticleSystem& ps,
              const std::vector<SpriteOverlay>& overlays,
              int cursor_col, int cursor_row, bool cursor_vis)
        : buf_(buf), ps_(ps), overlays_(overlays),
          cursor_col_(cursor_col), cursor_row_(cursor_row),
          cursor_vis_(cursor_vis) {}

    void ComputeRequirement() override {
        requirement_.min_x = buf_.Cols();
        requirement_.min_y = buf_.Rows();
    }

    void Render(ftxui::Screen& screen) override {
        int ox = box_.x_min;
        int oy = box_.y_min;
        int w  = box_.x_max - box_.x_min + 1;
        int h  = box_.y_max - box_.y_min + 1;

        for (int row = 0; row < std::min(buf_.Rows(), h); ++row) {
            for (int col = 0; col < std::min(buf_.Cols(), w); ++col) {
                const Cell& cell = buf_.At(col, row);
                ftxui::Pixel& px = screen.PixelAt(ox + col, oy + row);

                // Encode char32_t to UTF-8 for FTXUI
                char32_t ch = cell.ch;
                std::string s;
                if (ch < 0x80u) {
                    s += static_cast<char>(ch);
                } else if (ch < 0x800u) {
                    s += static_cast<char>(0xC0u | (ch >> 6u));
                    s += static_cast<char>(0x80u | (ch & 0x3Fu));
                } else if (ch < 0x10000u) {
                    s += static_cast<char>(0xE0u | (ch >> 12u));
                    s += static_cast<char>(0x80u | ((ch >> 6u) & 0x3Fu));
                    s += static_cast<char>(0x80u | (ch & 0x3Fu));
                } else {
                    s += static_cast<char>(0xF0u | (ch >> 18u));
                    s += static_cast<char>(0x80u | ((ch >> 12u) & 0x3Fu));
                    s += static_cast<char>(0x80u | ((ch >> 6u) & 0x3Fu));
                    s += static_cast<char>(0x80u | (ch & 0x3Fu));
                }
                px.character = s.empty() ? " " : s;
                px.bold      = cell.bold;
                if (cell.fg != ftxui::Color{}) px.foreground_color = cell.fg;
                if (cell.bg != ftxui::Color{}) px.background_color = cell.bg;
            }
        }

        // Particle overlay — typed particles with comet trails
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

        // Sprite overlay pass — sorted by z-order ascending
        // (lower z drawn first, higher z draws on top)
        auto sorted = overlays_;
        std::sort(sorted.begin(), sorted.end(),
            [](const SpriteOverlay& a, const SpriteOverlay& b) { return a.z < b.z; });

        for (const auto& ov : sorted) {
            int sx0 = static_cast<int>(ov.x);
            int sy0 = static_cast<int>(ov.y);
            for (int ri = 0; ri < (int)ov.rows.size(); ++ri) {
                const auto& row = ov.rows[ri];
                auto chars = split_utf8(row.text);
                for (int ci = 0; ci < (int)chars.size(); ++ci) {
                    int sx = sx0 + ci;
                    int sy = sy0 + ri;
                    if (sx < 0 || sx >= w || sy < 0 || sy >= h) continue;
                    if (ov.skip_nonempty) {
                        int bc = std::min(sx, buf_.Cols() - 1);
                        int br = std::min(sy, buf_.Rows() - 1);
                        if (buf_.At(bc, br).ch != U' ') continue;
                    }
                    ftxui::Pixel& px = screen.PixelAt(ox + sx, oy + sy);
                    px.character = chars[ci];
                    px.foreground_color = ftxui::Color::RGB(
                        row.color.r, row.color.g, row.color.b);
                }
            }
        }

        // Block cursor
        if (cursor_vis_ && cursor_col_ < w && cursor_row_ < h) {
            screen.PixelAt(ox + cursor_col_, oy + cursor_row_).inverted = true;
        }
    }

private:
    CellBuffer&     buf_;
    ParticleSystem& ps_;
    const std::vector<SpriteOverlay>& overlays_;
    int  cursor_col_, cursor_row_;
    bool cursor_vis_;
};

// ── TuiApp implementation ─────────────────────────────────────────────────────

TuiApp::TuiApp(Config cfg)
    : cfg_(std::move(cfg)),
      buf_(term_cols_, term_rows_),
      parser_(buf_) {
    cwd_ = get_cwd();
    luggage_.pos      = 0.0f;
    luggage_.vel      = 4.0f;
    luggage_.pause_cd = 8.0f;
    rincewind_.active   = false;
    rincewind_.cameo_cd = 10.0f;  // first cameo quickly
    rincewind_.x        = 0.0f;
    rincewind_.speed    = 30.0f;
    particles_.Init(term_cols_, term_rows_, 15);
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

void TuiApp::UpdateBoot(float delta) {
    const auto& script = boot_script();
    if (boot_line_index_ >= script.size()) {
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
            boot_line_timer_ = 1.0f;
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
                if (i == EVT_LUGGAGE_RAMPAGE)   luggage_.event_mult = 1.0f;
                // Start next cooldown
                std::uniform_real_distribution<float> cd(cfg.cd_min, cfg.cd_max);
                ev.cooldown = cd(erng);
            }
        } else {
            ev.cooldown -= delta;
            if (ev.cooldown <= 0.0f) {
                ev.active    = true;
                ev.remaining = cfg.duration;
                flash_message_ = cfg.label;
                flash_timer_   = 1.5f;
                // Activate effects
                if (i == EVT_OCTARINE_STORM)  particles_.target_count = particles_.base_count * 8;
                if (i == EVT_LUGGAGE_RAMPAGE) luggage_.event_mult = 3.0f;
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
    update_luggage(luggage_, delta, term_cols_, rincewind_.x, rincewind_.active);
    update_rincewind(rincewind_, delta, term_cols_, luggage_.pos);
    particles_.Update(delta, term_cols_, term_rows_);

    if (state_ == AppState::Boot)      UpdateBoot(delta);
    else if (state_ == AppState::Live) UpdateLive(delta);

    if (quit_) screen.ExitLoopClosure()();
}

// ── Rendering ─────────────────────────────────────────────────────────────────

ftxui::Element TuiApp::RenderTerminalArea(int /*cols*/, int /*rows*/) {
    return std::make_shared<TerryNode>(
        buf_, particles_, frame_overlays_,
        buf_.CursorCol(), buf_.CursorRow(), buf_.CursorVisible());
}

ftxui::Element TuiApp::RenderBoot(int /*cols*/, int /*rows*/) {
    const auto& script = boot_script();
    std::vector<ftxui::Element> lines;
    for (const char* art_line : {
        "                     .--.     ",
        "              _,-'`      `'-._",
        "           ,-'    A'TUIN      '-.",
        "         ,'  .--.         .--.  '.",
        "        /  /      \\     /      \\  \\",
        "       |  |  ~O~  | | |  ~O~  |  |",
        "        \\  \\  ---  /|\\  ---  /  /",
        "         '.  '~~'  | '~~~~'  .'",
        "           '-.___,-'--.___,-'",
    }) {
        lines.push_back(text(art_line) | color(theme_.dim));
    }
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
    return vbox(std::move(lines));
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
    int inner_rows = std::max(1, total_rows - 4);
    int inner_cols = std::max(1, total_cols - 2);

    if (inner_cols != term_cols_ || inner_rows != term_rows_) {
        term_cols_ = inner_cols;
        term_rows_ = inner_rows;
        buf_.Resize(term_cols_, term_rows_);
        particles_.Init(term_cols_, term_rows_, particles_.base_count);
        if (shell_ && shell_->IsRunning())
            shell_->Resize(term_cols_, term_rows_);
    }

    ftxui::Color bc = theme_.BorderColor(anim_time_);

    ftxui::Element content;
    if (state_ == AppState::Boot) {
        content = RenderBoot(inner_cols, inner_rows);
    } else if (farewell_showing_) {
        content = vbox({
            filler(),
            text("  DEATH: " + std::string(death_farewell())) | color(theme_.death) | bold,
            filler(),
        });
    } else {
        // Build sprite overlay list for this frame
        frame_overlays_.clear();
        frame_overlays_.push_back(death_overlay(anim_time_, inner_rows));
        frame_overlays_.push_back(luggage_overlay(luggage_, inner_rows));
        if (rincewind_.active)
            frame_overlays_.push_back(rincewind_overlay(rincewind_));
        // sort ascending z (lower z drawn first)
        std::sort(frame_overlays_.begin(), frame_overlays_.end(),
            [](const SpriteOverlay& a, const SpriteOverlay& b) { return a.z < b.z; });
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
            text("\u255a") | color(bc),   // ╚
            separator() | color(bc),
            text("\u255d") | color(bc),   // ╝
        }),
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
            if (event.is_character()) { shell_->Write(event.character());  return true; }
            if (event == Event::Return)     { shell_->Write("\r");      return true; }
            if (event == Event::Backspace)  { shell_->Write("\x7f");    return true; }
            if (event == Event::Tab)        { shell_->Write("\t");      return true; }
            if (event == Event::Escape)     { shell_->Write("\x1b");    return true; }
            if (event == Event::ArrowUp)    { shell_->Write("\x1b[A");  return true; }
            if (event == Event::ArrowDown)  { shell_->Write("\x1b[B");  return true; }
            if (event == Event::ArrowRight) { shell_->Write("\x1b[C");  return true; }
            if (event == Event::ArrowLeft)  { shell_->Write("\x1b[D");  return true; }
            if (event == Event::Home)       { shell_->Write("\x1b[H");  return true; }
            if (event == Event::End)        { shell_->Write("\x1b[F");  return true; }
            if (event == Event::Delete)     { shell_->Write("\x1b[3~"); return true; }
            if (event == Event::PageUp)     { shell_->Write("\x1b[5~"); return true; }
            if (event == Event::PageDown)   { shell_->Write("\x1b[6~"); return true; }
            if (!event.input().empty()) { shell_->Write(event.input()); return true; }
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
