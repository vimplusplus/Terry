#pragma once
// Ensure windows.h is included with NOMINMAX before FTXUI color headers
// to prevent RGB/min/max macro conflicts.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef RGB
#undef min
#undef max
#endif

#include "config.h"
#include "theme.h"
#include "cell_buffer.h"
#include "vt_parser.h"
#include "shell_host.h"
#include "particle_system.h"
#include "animations.h"
#include "boot_sequence.h"

#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <mutex>
#include <memory>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional>

namespace terry {

// ── Event system ──────────────────────────────────────────────────────────────

struct EventState {
    float cooldown  = 0.0f;  // seconds until next activation
    float remaining = 0.0f;  // seconds of active time left
    bool  active    = false;
};

} // namespace terry (re-opened below)

namespace terry {

enum class AppState { Boot, Live, Exit };

static constexpr int kEventCount = 3;
enum EventId { EVT_OCTARINE_STORM = 0, EVT_NARRATIVIUM_SURGE = 1, EVT_LUGGAGE_RAMPAGE = 2 };

class TuiApp {
public:
    explicit TuiApp(Config cfg);
    int Run();  // Blocks until shell exits; returns shell exit code.

private:
    // Terminal dimensions first — used to construct buf_ and parser_
    int term_cols_ = 80;
    int term_rows_ = 22;

    // Configuration / subsystems
    Config       cfg_;
    TerryTheme   theme_;
    CellBuffer   buf_;
    VtParser     parser_;
    ParticleSystem particles_;
    LuggageState   luggage_;
    RincewindState rincewind_;
    DeathState     death_;
    std::vector<SpriteOverlay> frame_overlays_;
    std::unique_ptr<ShellHost> shell_;

    // State
    AppState state_ = AppState::Boot;
    std::atomic<bool> quit_{false};

    // Shell output queue (written from read thread, drained on UI thread)
    std::string  pending_data_;
    std::mutex   data_mutex_;

    // Animation timing
    using Clock = std::chrono::steady_clock;
    Clock::time_point last_tick_;
    float anim_time_ = 0.0f;

    // Boot sequence
    size_t boot_line_index_ = 0;
    float  boot_line_timer_ = 0.0f;
    std::vector<std::string> boot_displayed_; // lines shown so far

    // Status bar rotation
    int   label_index_ = 0;
    float label_timer_ = 0.0f;

    // Farewell
    bool  farewell_showing_ = false;
    float farewell_timer_   = 0.0f;
    int   exit_code_        = 0;

    // Current working directory (polled)
    std::string cwd_;
    float cwd_poll_timer_ = 0.0f;

    // Status bar flash message
    std::string flash_message_;
    float       flash_timer_ = 0.0f;

    // Magical event system
    EventState events_[kEventCount];

    // Internal methods
    void OnTick(float delta, ftxui::ScreenInteractive& screen);
    void UpdateBoot(float delta);
    void UpdateLive(float delta);
    void PollCwd();

    ftxui::Element RenderFrame(int total_cols, int total_rows);
    ftxui::Element RenderBoot(int cols, int rows);
    ftxui::Element RenderTerminalArea(int cols, int rows);
    ftxui::Element RenderStatusBar(int cols);
    ftxui::Element RenderBorder(ftxui::Element inner, int cols, int rows);
};

} // namespace terry
