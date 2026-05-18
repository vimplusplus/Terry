#pragma once
#include <string>

namespace terry {

struct Config {
    std::string shell_path; // empty = use platform default
};

Config load_config();
std::string default_shell();

} // namespace terry
