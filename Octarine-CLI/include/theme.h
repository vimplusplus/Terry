#pragma once
#include <ftxui/screen/color.hpp>
#include <cmath>

namespace terry {

// TerryTheme — the octarine magic colour palette.
// Octarine: the eighth colour of the rainbow, the colour of magic.
// Only wizards (and cats) can see it. It's a sort of greenish-purple.
struct TerryTheme {
    ftxui::Color background = ftxui::Color::RGB(10, 0, 16);
    ftxui::Color text       = ftxui::Color::RGB(200, 255, 200);
    ftxui::Color highlight  = ftxui::Color::RGB(204, 136, 255);
    ftxui::Color dim        = ftxui::Color::RGB(74, 48, 96);
    ftxui::Color error_col  = ftxui::Color::RGB(204, 51, 102);
    ftxui::Color death      = ftxui::Color::RGB(255, 255, 255);

    // Animated border: deep purple → electric green → mid purple, 4-second cycle
    ftxui::Color BorderColor(float t) const {
        struct RGB { uint8_t r, g, b; };
        static constexpr RGB stops[3] = {
            {75,  0,   130},  // deep indigo-purple
            {57,  255, 20},   // electric neon green
            {155, 77,  202},  // mid octarine-purple
        };
        float phase = std::fmod(t, 4.0f) / 4.0f * 3.0f;
        if (phase < 0.0f) phase += 3.0f;
        int i = static_cast<int>(phase) % 3;
        int j = (i + 1) % 3;
        float f = phase - std::floor(phase);
        auto lerp = [](int a, int b, float x) -> uint8_t {
            return static_cast<uint8_t>(a + static_cast<int>((b - a) * x));
        };
        return ftxui::Color::RGB(
            lerp(stops[i].r, stops[j].r, f),
            lerp(stops[i].g, stops[j].g, f),
            lerp(stops[i].b, stops[j].b, f)
        );
    }
};

} // namespace terry
