#pragma once
#include <vector>
#include <string>
#include <array>

namespace terry {

struct BootLine {
    int         delay_ms;
    std::string text;
    bool        is_death = false; // render white bold
};

// Multi-line ASCII art of Great A'Tuin
const std::string& atuin_art();

// Timed boot script
const std::vector<BootLine>& boot_script();

} // namespace terry
