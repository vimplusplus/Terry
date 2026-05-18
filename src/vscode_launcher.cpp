// vscode_launcher.cpp — VS Code detection, download, install, and launch.
// Windows primary. macOS/Linux stubs for detection and open.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#undef min
#undef max
#endif

#include "vscode_launcher.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace terry {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

#ifdef _WIN32
// Expand a single environment variable token like %LOCALAPPDATA%.
static std::string expand_env(const std::string& path) {
    char buf[MAX_PATH] = {};
    ExpandEnvironmentStringsA(path.c_str(), buf, static_cast<DWORD>(sizeof(buf) - 1));
    return buf;
}

static bool file_exists(const std::string& p) {
    DWORD attr = GetFileAttributesA(p.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

// Run a command via _popen and return trimmed output (first line).
static std::string popen_first_line(const std::string& cmd) {
    FILE* f = _popen((cmd + " 2>nul").c_str(), "r");
    if (!f) return {};
    char buf[MAX_PATH] = {};
    if (fgets(buf, sizeof(buf) - 1, f)) {
        // Strip trailing newline/CR
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
    }
    _pclose(f);
    return buf;
}
#else
static std::string popen_first_line(const std::string& cmd) {
    FILE* f = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!f) return {};
    char buf[1024] = {};
    if (fgets(buf, sizeof(buf) - 1, f)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
    }
    pclose(f);
    return buf;
}
#endif

// ─────────────────────────────────────────────────────────────────────────────
// DetectVsCode
// ─────────────────────────────────────────────────────────────────────────────

std::string DetectVsCode() {
#ifdef _WIN32
    // Well-known install locations
    static const char* kCandidates[] = {
        "%LOCALAPPDATA%\\Programs\\Microsoft VS Code\\bin\\code.cmd",
        "%ProgramFiles%\\Microsoft VS Code\\bin\\code.cmd",
    };
    for (const char* cand : kCandidates) {
        std::string path = expand_env(cand);
        if (file_exists(path)) return path;
    }
    // Fallback: PATH
    std::string found = popen_first_line("where code");
    if (!found.empty()) return found;
    return {};

#elif defined(__APPLE__)
    const std::string app_bin =
        "/Applications/Visual Studio Code.app/Contents/Resources/app/bin/code";
    if (access(app_bin.c_str(), X_OK) == 0) return app_bin;
    std::string found = popen_first_line("which code");
    if (!found.empty()) return found;
    return {};

#else
    std::string found = popen_first_line("which code");
    if (!found.empty()) return found;
    found = popen_first_line("which code-oss");
    return found;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// DownloadFile (Windows WinINet — stub on other platforms)
// ─────────────────────────────────────────────────────────────────────────────

bool DownloadFile(const std::string& url, const std::string& dest_path, PostLogFn post) {
#ifdef _WIN32
    HINTERNET hInet = InternetOpenA(
        "Terry/1.0",
        INTERNET_OPEN_TYPE_PRECONFIG,
        nullptr, nullptr, 0);
    if (!hInet) {
        post("x Failed to initialise WinINet.");
        return false;
    }

    HINTERNET hUrl = InternetOpenUrlA(
        hInet, url.c_str(),
        nullptr, 0,
        INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_SECURE,
        0);
    if (!hUrl) {
        InternetCloseHandle(hInet);
        post("x Could not connect to VS Code CDN. Check internet connection.");
        return false;
    }

    // Query content length for progress
    DWORD total_bytes = 0;
    DWORD len = sizeof(DWORD);
    HttpQueryInfoA(hUrl, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
                   &total_bytes, &len, nullptr);

    HANDLE hFile = CreateFileA(dest_path.c_str(),
        GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInet);
        post("x Could not create temporary installer file.");
        return false;
    }

    static const DWORD kBuf = 64 * 1024;
    std::vector<char> buf(kBuf);
    DWORD downloaded = 0;
    DWORD last_pct   = 0;
    bool  ok         = true;

    while (true) {
        DWORD read = 0;
        if (!InternetReadFile(hUrl, buf.data(), kBuf, &read)) { ok = false; break; }
        if (read == 0) break;
        DWORD written = 0;
        WriteFile(hFile, buf.data(), read, &written, nullptr);
        downloaded += read;
        if (total_bytes > 0) {
            DWORD pct = downloaded * 100 / total_bytes;
            if (pct >= last_pct + 10) {
                last_pct = pct - (pct % 10);
                std::string msg = "  Downloading VS Code installer... " +
                                  std::to_string(last_pct) + "%";
                post(msg);
            }
        }
    }

    CloseHandle(hFile);
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInet);

    if (!ok) {
        post("x Download failed.");
        DeleteFileA(dest_path.c_str());
    }
    return ok;

#else
    (void)url; (void)dest_path; (void)post;
    return false;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// InstallVsCode
// ─────────────────────────────────────────────────────────────────────────────

bool InstallVsCode(PostLogFn post) {
#ifdef _WIN32
    // Destination: %TEMP%\terry-vscode-setup.exe
    std::string temp_dir = expand_env("%TEMP%");
    std::string installer = temp_dir + "\\terry-vscode-setup.exe";

    const std::string url =
        "https://update.code.visualstudio.com/latest/win32-x64/stable";

    post("  Downloading VS Code installer...");
    if (!DownloadFile(url, installer, post)) return false;
    post("+ Download complete.");

    // Run silently
    post("  Installing VS Code (this may take a moment)...");
    std::string args = "\"" + installer + "\" /VERYSILENT /MERGETASKS=!runcode /NORESTART";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessA(nullptr, &args[0],
                        nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        post("x Failed to launch VS Code installer.");
        DeleteFileA(installer.c_str());
        return false;
    }

    // Wait up to 5 minutes
    DWORD wait = WaitForSingleObject(pi.hProcess, 5 * 60 * 1000);
    DWORD exit_code = 1;
    if (wait == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    DeleteFileA(installer.c_str());

    if (exit_code != 0) {
        post("x VS Code installer exited with code " + std::to_string(exit_code) + ".");
        return false;
    }

    // Confirm
    if (DetectVsCode().empty()) {
        post("x VS Code installed but could not be located. Try reopening Terry.");
        return false;
    }

    post("+ VS Code installed.");
    return true;

#else
    post("x VS Code not found. Download from https://code.visualstudio.com");
    return false;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// InstallExtension
// ─────────────────────────────────────────────────────────────────────────────

bool InstallExtension(const std::string& code_path,
                      const std::string& ext_id,
                      PostLogFn post) {
    post("  Installing extension " + ext_id + "...");

#ifdef _WIN32
    std::string cmd = "\"" + code_path + "\" --install-extension " + ext_id + " --force";
    // Run via cmd.exe so .cmd scripts work
    std::string full_cmd = "cmd.exe /C " + cmd;

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(nullptr, &full_cmd[0],
                        nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        post("! Could not run extension installer for " + ext_id + ".");
        return false;
    }
    WaitForSingleObject(pi.hProcess, 60 * 1000);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (exit_code != 0) {
        post("! Could not install " + ext_id +
             ". Install manually from VS Code Marketplace.");
        return false;
    }
    post("+ Installed " + ext_id + ".");
    return true;

#else
    // Fork + execvp
    pid_t pid = fork();
    if (pid < 0) {
        post("! fork() failed for extension " + ext_id + ".");
        return false;
    }
    if (pid == 0) {
        // Child
        const char* argv[] = {
            code_path.c_str(),
            "--install-extension",
            ext_id.c_str(),
            "--force",
            nullptr
        };
        execvp(code_path.c_str(), const_cast<char* const*>(argv));
        _exit(1);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        post("! Could not install " + ext_id +
             ". Install manually from VS Code Marketplace.");
        return false;
    }
    post("+ Installed " + ext_id + ".");
    return true;
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// OpenVsCode
// ─────────────────────────────────────────────────────────────────────────────

void OpenVsCode(const std::string& code_path,
                const std::string& cwd,
                PostLogFn post) {
    post("  Opening VS Code at " + cwd + "...");

#ifdef _WIN32
    // ShellExecuteW: fire-and-forget, opens as a normal foreground window.
    // code.cmd can't be launched directly via ShellExecute — use cmd.exe.
    std::string cmd = "\"" + code_path + "\" \"" + cwd + "\"";
    std::string full_cmd = "/C " + cmd;
    ShellExecuteA(nullptr, "open", "cmd.exe", full_cmd.c_str(), cwd.c_str(), SW_HIDE);
    post("+ VS Code opened.");

#elif defined(__APPLE__)
    pid_t pid = fork();
    if (pid == 0) {
        const char* argv[] = { "open", "-a", "Visual Studio Code", cwd.c_str(), nullptr };
        execvp("open", const_cast<char* const*>(argv));
        _exit(1);
    }
    if (pid > 0) waitpid(pid, nullptr, 0);
    post("+ VS Code opened.");

#else
    pid_t pid = fork();
    if (pid == 0) {
        const char* argv[] = { code_path.c_str(), cwd.c_str(), nullptr };
        execvp(code_path.c_str(), const_cast<char* const*>(argv));
        _exit(1);
    }
    // Detach — don't wait
    (void)pid;
    post("+ VS Code opened.");
#endif
}

} // namespace terry
