// Windows ConPTY shell host — tasks 2.2, 2.4, 2.5
#ifdef _WIN32
#include "shell_host.h"
#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <vector>

namespace terry {

class WindowsShellHost : public ShellHost {
public:
    ~WindowsShellHost() override {
        running_ = false;
        if (hPC_)       ClosePseudoConsole(hPC_);
        if (hInput_)    CloseHandle(hInput_);
        if (hOutput_)   CloseHandle(hOutput_);
        if (hProcess_)  CloseHandle(hProcess_);
        if (read_thread_.joinable())  read_thread_.join();
        if (exit_thread_.joinable())  exit_thread_.join();
        if (pAttrList_) {
            DeleteProcThreadAttributeList(pAttrList_);
            HeapFree(GetProcessHeap(), 0, pAttrList_);
        }
    }

    bool Start(const std::string& shell_path, int cols, int rows) override {
        // ── Pipes ────────────────────────────────────────────────────────────
        HANDLE hPipeInRead, hPipeInWrite;
        HANDLE hPipeOutRead, hPipeOutWrite;
        if (!CreatePipe(&hPipeInRead,  &hPipeInWrite,  nullptr, 0)) return false;
        if (!CreatePipe(&hPipeOutRead, &hPipeOutWrite, nullptr, 0)) {
            CloseHandle(hPipeInRead); CloseHandle(hPipeInWrite);
            return false;
        }

        // ── ConPTY ───────────────────────────────────────────────────────────
        COORD size = {static_cast<SHORT>(cols), static_cast<SHORT>(rows)};
        HRESULT hr = CreatePseudoConsole(size, hPipeInRead, hPipeOutWrite, 0, &hPC_);
        CloseHandle(hPipeInRead);
        CloseHandle(hPipeOutWrite);
        if (FAILED(hr)) {
            CloseHandle(hPipeInWrite);
            CloseHandle(hPipeOutRead);
            return false;
        }
        hInput_  = hPipeInWrite;
        hOutput_ = hPipeOutRead;

        // ── Process attribute list ────────────────────────────────────────────
        SIZE_T attrSize = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
        pAttrList_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
            HeapAlloc(GetProcessHeap(), 0, attrSize));
        if (!pAttrList_) return false;
        if (!InitializeProcThreadAttributeList(pAttrList_, 1, 0, &attrSize)) return false;
        if (!UpdateProcThreadAttribute(pAttrList_, 0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                hPC_, sizeof(HPCON), nullptr, nullptr)) return false;

        // ── Create process ────────────────────────────────────────────────────
        STARTUPINFOEXW si = {};
        si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        si.lpAttributeList = pAttrList_;

        std::wstring wcmd(shell_path.begin(), shell_path.end());
        PROCESS_INFORMATION pi = {};
        BOOL ok = CreateProcessW(
            nullptr, wcmd.data(),
            nullptr, nullptr, FALSE,
            EXTENDED_STARTUPINFO_PRESENT,
            nullptr, nullptr,
            reinterpret_cast<LPSTARTUPINFOW>(&si), &pi);
        if (!ok) return false;

        hProcess_ = pi.hProcess;
        CloseHandle(pi.hThread);

        running_ = true;
        exit_code_ = -1;

        // ── Background threads ───────────────────────────────────────────────
        read_thread_ = std::thread([this] {
            const DWORD BUF = 4096;
            std::vector<char> buf(BUF);
            DWORD nRead = 0;
            while (running_) {
                if (!ReadFile(hOutput_, buf.data(), BUF, &nRead, nullptr) || nRead == 0)
                    break;
                if (output_cb_)
                    output_cb_(std::string(buf.data(), nRead));
            }
        });

        exit_thread_ = std::thread([this, proc = pi.hProcess] {
            WaitForSingleObject(proc, INFINITE);
            DWORD code = 0;
            GetExitCodeProcess(proc, &code);
            exit_code_ = static_cast<int>(code);
            running_ = false;
            if (exit_cb_) exit_cb_(exit_code_);
        });

        return true;
    }

    void Write(const std::string& data) override {
        if (!hInput_) return;
        DWORD written = 0;
        WriteFile(hInput_, data.data(), static_cast<DWORD>(data.size()), &written, nullptr);
    }

    void Resize(int cols, int rows) override {
        if (!hPC_) return;
        COORD size = {static_cast<SHORT>(cols), static_cast<SHORT>(rows)};
        ResizePseudoConsole(hPC_, size);
    }

    bool IsRunning() const override { return running_; }
    int  ExitCode()  const override { return exit_code_; }

private:
    HPCON  hPC_      = nullptr;
    HANDLE hInput_   = nullptr;
    HANDLE hOutput_  = nullptr;
    HANDLE hProcess_ = nullptr;
    LPPROC_THREAD_ATTRIBUTE_LIST pAttrList_ = nullptr;

    std::atomic<bool> running_{false};
    std::atomic<int>  exit_code_{-1};
    std::thread read_thread_;
    std::thread exit_thread_;
};

std::unique_ptr<ShellHost> ShellHost::Create() {
    return std::make_unique<WindowsShellHost>();
}

} // namespace terry
#endif // _WIN32
