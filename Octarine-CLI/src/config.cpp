#include "config.h"
#include <fstream>
#include <string>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace terry {

std::string default_shell() {
#ifdef _WIN32
    return "cmd.exe";
#else
    const char* s = std::getenv("SHELL");
    return (s && *s) ? s : "/bin/bash";
#endif
}

static std::string config_path() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (!appdata) return "";
    return std::string(appdata) + "\\terry\\config";
#else
    const char* home = std::getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    return home ? (std::string(home) + "/.terry") : "";
#endif
}

static std::string trim(const std::string& s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

Config load_config() {
    Config cfg;
    std::string path = config_path();
    if (path.empty()) return cfg;

    std::ifstream f(path);
    if (!f.is_open()) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        if (key == "shell") cfg.shell_path = val;
    }
    return cfg;
}

} // namespace terry
