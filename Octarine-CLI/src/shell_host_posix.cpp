// POSIX PTY shell host (Linux/macOS) — tasks 2.3, 2.4, 2.5
#ifndef _WIN32
#include "shell_host.h"
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

namespace terry {

class PosixShellHost : public ShellHost {
public:
    ~PosixShellHost() override {
        running_ = false;
        if (master_fd_ >= 0) ::close(master_fd_);
        if (child_pid_ > 0)  ::kill(child_pid_, SIGTERM);
        if (read_thread_.joinable())  read_thread_.join();
        if (exit_thread_.joinable())  exit_thread_.join();
    }

    bool Start(const std::string& shell_path, int cols, int rows) override {
        struct winsize ws = {};
        ws.ws_col = static_cast<unsigned short>(cols);
        ws.ws_row = static_cast<unsigned short>(rows);

        child_pid_ = forkpty(&master_fd_, nullptr, nullptr, &ws);
        if (child_pid_ < 0) return false;

        if (child_pid_ == 0) {
            // Child — exec the shell
            const char* args[] = {shell_path.c_str(), nullptr};
            execvp(shell_path.c_str(), const_cast<char**>(args));
            _exit(1);
        }

        // Parent
        running_    = true;
        exit_code_  = -1;

        read_thread_ = std::thread([this] {
            const int BUF = 4096;
            std::vector<char> buf(BUF);
            while (running_) {
                ssize_t n = ::read(master_fd_, buf.data(), BUF);
                if (n <= 0) break;
                if (output_cb_)
                    output_cb_(std::string(buf.data(), static_cast<size_t>(n)));
            }
        });

        exit_thread_ = std::thread([this] {
            int status = 0;
            waitpid(child_pid_, &status, 0);
            exit_code_ = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            running_ = false;
            if (exit_cb_) exit_cb_(exit_code_);
        });

        return true;
    }

    void Write(const std::string& data) override {
        if (master_fd_ < 0) return;
        ::write(master_fd_, data.data(), data.size());
    }

    void Resize(int cols, int rows) override {
        if (master_fd_ < 0) return;
        struct winsize ws = {};
        ws.ws_col = static_cast<unsigned short>(cols);
        ws.ws_row = static_cast<unsigned short>(rows);
        ::ioctl(master_fd_, TIOCSWINSZ, &ws);
    }

    bool IsRunning() const override { return running_; }
    int  ExitCode()  const override { return exit_code_; }

private:
    int   master_fd_ = -1;
    pid_t child_pid_ = -1;

    std::atomic<bool> running_{false};
    std::atomic<int>  exit_code_{-1};
    std::thread read_thread_;
    std::thread exit_thread_;
};

std::unique_ptr<ShellHost> ShellHost::Create() {
    return std::make_unique<PosixShellHost>();
}

} // namespace terry
#endif // !_WIN32
