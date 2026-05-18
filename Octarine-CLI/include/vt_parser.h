#pragma once
#include "cell_buffer.h"
#include <string>
#include <vector>

namespace terry {

// Minimal VT100/ANSI sequence parser.
// Parses output from a PTY and updates a CellBuffer accordingly.
// Supports: cursor movement, SGR colors (8/256/truecolor), erase, scroll,
// alternate screen (?1049h/l), cursor visibility (?25h/l).
class VtParser {
public:
    explicit VtParser(CellBuffer& buf);

    void Feed(const std::string& data);

private:
    CellBuffer& buf_;

    enum class State { Normal, Escape, CSI, OSC, DCS };
    State state_ = State::Normal;

    std::string csi_buf_;  // accumulates CSI parameter bytes + intermediate bytes
    char csi_final_ = 0;

    std::string osc_buf_;

    void ProcessNormal(unsigned char c);
    void ProcessEscape(unsigned char c);
    void ProcessCSI(unsigned char c);
    void ProcessOSC(unsigned char c);

    void DispatchCSI();
    void HandleSGR(const std::vector<int>& params);
    void HandleCursorPos(const std::vector<int>& p);
    void HandleErase(char cmd, const std::vector<int>& p);
    void HandleDECPrivate(bool set, const std::vector<int>& p);

    std::vector<int> ParseParams(const std::string& s, int def = 0);

    static ftxui::Color StandardColor(int n);
    static ftxui::Color Color256(int n);

    void CarriageReturn();
    void LineFeed();
    void Backspace();
    void Tab();

    // UTF-8 multi-byte decoding state
    char32_t utf8_accum_ = 0;
    int      utf8_left_  = 0;

    // Saved cursor for ESC 7 / ESC 8
    int saved_col_ = 0, saved_row_ = 0;
};

} // namespace terry
