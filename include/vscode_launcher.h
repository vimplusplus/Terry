#pragma once
// vscode_launcher.h — VS Code detection, installation, and launch helpers.
// Used by the Rincewind command in TuiApp.

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <functional>
#include <string>

namespace terry {

// Callback used to post progress lines to the Rincewind log.
using PostLogFn = std::function<void(const std::string&)>;

// Returns the full path to the 'code' executable, or empty if not found.
std::string DetectVsCode();

// Downloads a file from |url| to |dest_path|.
// Posts "Downloading... N%" lines via |post|.
// Returns true on success.
bool DownloadFile(const std::string& url, const std::string& dest_path, PostLogFn post);

// Downloads and silently installs VS Code (Windows only).
// On macOS/Linux logs an error and returns false.
// Returns true on success.
bool InstallVsCode(PostLogFn post);

// Runs `code --install-extension <ext_id>` using |code_path|.
// Non-fatal on failure — logs the error and returns false.
bool InstallExtension(const std::string& code_path,
                      const std::string& ext_id,
                      PostLogFn post);

// Opens VS Code at |cwd| using |code_path|.
void OpenVsCode(const std::string& code_path,
                const std::string& cwd,
                PostLogFn post);

} // namespace terry
