#pragma once
#include <string>
#include <functional>
#include <memory>

namespace terry {

// Abstract PTY shell host. Platform implementations in shell_host_windows.cpp
// and shell_host_posix.cpp.
class ShellHost {
public:
    virtual ~ShellHost() = default;

    virtual bool Start(const std::string& shell_path, int cols, int rows) = 0;
    virtual void Write(const std::string& data) = 0;
    virtual void Resize(int cols, int rows) = 0;
    virtual bool IsRunning() const = 0;
    virtual int  ExitCode() const = 0;

    void OnOutput(std::function<void(const std::string&)> cb) {
        output_cb_ = std::move(cb);
    }
    void OnExit(std::function<void(int)> cb) {
        exit_cb_ = std::move(cb);
    }

    // Factory: returns a platform-appropriate implementation
    static std::unique_ptr<ShellHost> Create();

protected:
    std::function<void(const std::string&)> output_cb_;
    std::function<void(int)> exit_cb_;
};

} // namespace terry
