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

// ── TerryNode — custom FTXUI node rendering our terminal cell buffer ──────────

class TerryNode : public ftxui::Node {
public:
    TerryNode(CellBuffer& buf, ParticleSystem& ps,
              const ShootingStar* star,
              int cursor_col, int cursor_row, bool cursor_vis)
        : buf_(buf), ps_(ps), star_(star),
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

        // ── PTY cell buffer ───────────────────────────────────────────────────
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
    }

private:
    CellBuffer&           buf_;
    ParticleSystem&       ps_;
    const ShootingStar*   star_;
    int  cursor_col_, cursor_row_;
    bool cursor_vis_;
};


// ── TuiApp implementation ─────────────────────────────────────────────────────

TuiApp::TuiApp(Config cfg)
    : cfg_(std::move(cfg)),
      buf_(term_cols_, term_rows_),
      parser_(buf_) {
    cwd_ = get_cwd();
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
                ev.active    = true;
                ev.remaining = cfg.duration;
                flash_message_ = cfg.label;
                flash_timer_   = 1.5f;
                // Activate effects
                if (i == EVT_OCTARINE_STORM)    particles_.target_count = particles_.base_count * 8;
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
        buf_.CursorCol(), buf_.CursorRow(), buf_.CursorVisible());
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
