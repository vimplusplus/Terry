#include "vt_parser.h"
#include <sstream>
#include <algorithm>

namespace terry {

VtParser::VtParser(CellBuffer& buf) : buf_(buf) {}

void VtParser::Feed(const std::string& data) {
    for (auto byte : data) {
        unsigned char c = static_cast<unsigned char>(byte);
        switch (state_) {
            case State::Normal:  ProcessNormal(c);  break;
            case State::Escape:  ProcessEscape(c);  break;
            case State::CSI:     ProcessCSI(c);     break;
            case State::OSC:     ProcessOSC(c);     break;
            case State::DCS:
                if (c == 0x1B) state_ = State::Escape;
                break;
        }
    }
}

void VtParser::CarriageReturn() { buf_.SetCursor(0, buf_.CursorRow()); }

void VtParser::LineFeed() {
    int r = buf_.CursorRow() + 1;
    if (r >= buf_.Rows()) {
        buf_.ScrollUp(1);
        r = buf_.Rows() - 1;
    }
    buf_.SetCursor(buf_.CursorCol(), r);
}

void VtParser::Backspace() {
    int c = buf_.CursorCol() - 1;
    if (c < 0) c = 0;
    buf_.SetCursor(c, buf_.CursorRow());
}

void VtParser::Tab() {
    int c = ((buf_.CursorCol() / 8) + 1) * 8;
    if (c >= buf_.Cols()) c = buf_.Cols() - 1;
    buf_.SetCursor(c, buf_.CursorRow());
}

void VtParser::ProcessNormal(unsigned char c) {
    if (c == 0x1B) { state_ = State::Escape; utf8_left_ = 0; return; }
    switch (c) {
        case '\r': CarriageReturn(); return;
        case '\n': case 0x0C: case 0x0B: LineFeed(); return;
        case '\b': Backspace(); return;
        case '\t': Tab(); return;
        case 0x07: return; // BEL
        case 0x0E: case 0x0F: return; // SO/SI charset switch — ignore
    }
    if (c < 0x20 || c == 0x7F) return;

    // UTF-8 decoding
    if (c < 0x80) {
        utf8_left_ = 0;
        buf_.PutChar(static_cast<char32_t>(c));
    } else if ((c & 0xC0) == 0x80) {
        // continuation
        if (utf8_left_ > 0) {
            utf8_accum_ = (utf8_accum_ << 6) | (c & 0x3F);
            if (--utf8_left_ == 0) buf_.PutChar(utf8_accum_);
        }
    } else if ((c & 0xE0) == 0xC0) {
        utf8_accum_ = c & 0x1F; utf8_left_ = 1;
    } else if ((c & 0xF0) == 0xE0) {
        utf8_accum_ = c & 0x0F; utf8_left_ = 2;
    } else if ((c & 0xF8) == 0xF0) {
        utf8_accum_ = c & 0x07; utf8_left_ = 3;
    }
}

void VtParser::ProcessEscape(unsigned char c) {
    state_ = State::Normal;
    switch (c) {
        case '[': state_ = State::CSI; csi_buf_.clear(); break;
        case ']': state_ = State::OSC; osc_buf_.clear(); break;
        case 'P': state_ = State::DCS; break;
        case 'M': // Reverse index
        {
            int r = buf_.CursorRow() - 1;
            if (r < 0) { buf_.ScrollDown(1); r = 0; }
            buf_.SetCursor(buf_.CursorCol(), r);
            break;
        }
        case '7': saved_col_ = buf_.CursorCol(); saved_row_ = buf_.CursorRow(); break;
        case '8': buf_.SetCursor(saved_col_, saved_row_); break;
        case 'c': buf_.Clear(); buf_.ResetSGR(); break;
        default: break; // ignore others
    }
}

void VtParser::ProcessCSI(unsigned char c) {
    if (c >= 0x20 && c < 0x40) {
        // parameter / intermediate byte
        csi_buf_ += static_cast<char>(c);
    } else if (c >= 0x40 && c <= 0x7E) {
        // final byte
        csi_final_ = static_cast<char>(c);
        DispatchCSI();
        state_ = State::Normal;
    } else if (c == 0x1B) {
        state_ = State::Escape;
    }
}

void VtParser::ProcessOSC(unsigned char c) {
    if (c == 0x07) { state_ = State::Normal; osc_buf_.clear(); } // BEL ends OSC
    else if (c == 0x1B) { state_ = State::Escape; } // ESC may start ESC\ terminator
    // just accumulate / ignore content
}

std::vector<int> VtParser::ParseParams(const std::string& s, int def) {
    std::vector<int> result;
    std::istringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ';')) {
        if (tok.empty()) result.push_back(def);
        else {
            try { result.push_back(std::stoi(tok)); }
            catch (...) { result.push_back(def); }
        }
    }
    if (result.empty()) result.push_back(def);
    return result;
}

void VtParser::DispatchCSI() {
    // Check for DEC private mode prefix '?'
    bool dec = !csi_buf_.empty() && csi_buf_[0] == '?';
    std::string param_str = dec ? csi_buf_.substr(1) : csi_buf_;

    auto params = ParseParams(param_str, 0);
    int p1 = params.empty() ? 0 : params[0];

    switch (csi_final_) {
        case 'm': HandleSGR(ParseParams(param_str, 0)); break;
        case 'H': case 'f': HandleCursorPos(params); break;
        case 'A': buf_.SetCursor(buf_.CursorCol(), std::max(0, buf_.CursorRow() - std::max(1, p1))); break;
        case 'B': buf_.SetCursor(buf_.CursorCol(), std::min(buf_.Rows()-1, buf_.CursorRow() + std::max(1, p1))); break;
        case 'C': buf_.SetCursor(std::min(buf_.Cols()-1, buf_.CursorCol() + std::max(1, p1)), buf_.CursorRow()); break;
        case 'D': buf_.SetCursor(std::max(0, buf_.CursorCol() - std::max(1, p1)), buf_.CursorRow()); break;
        case 'G': buf_.SetCursor(std::max(1, p1) - 1, buf_.CursorRow()); break;
        case 'd': buf_.SetCursor(buf_.CursorCol(), std::max(1, p1) - 1); break;
        case 'J': buf_.EraseDisplay(p1); break;
        case 'K': buf_.EraseLine(p1); break;
        case 'S': buf_.ScrollUp(std::max(1, p1)); break;
        case 'T': buf_.ScrollDown(std::max(1, p1)); break;
        case 'L': buf_.ScrollDown(std::max(1, p1)); break;
        case 'M': buf_.ScrollUp(std::max(1, p1)); break;
        case 'P': // delete chars — shift left
        {
            int n = std::max(1, p1);
            int row = buf_.CursorRow();
            int col = buf_.CursorCol();
            for (int c = col; c < buf_.Cols() - n; ++c)
                buf_.At(c, row) = buf_.At(c + n, row);
            buf_.ClearLine(row, buf_.Cols() - n, -1);
            break;
        }
        case 'h': if (dec) HandleDECPrivate(true,  params); break;
        case 'l': if (dec) HandleDECPrivate(false, params); break;
        case 'r': break; // set scroll region — ignore in v1
        default: break;
    }
}

void VtParser::HandleCursorPos(const std::vector<int>& p) {
    int row = (p.size() > 0 && p[0] > 0) ? p[0] - 1 : 0;
    int col = (p.size() > 1 && p[1] > 0) ? p[1] - 1 : 0;
    buf_.SetCursor(col, row);
}

void VtParser::HandleDECPrivate(bool set, const std::vector<int>& params) {
    for (int p : params) {
        switch (p) {
            case 25: buf_.SetCursorVisible(set); break;
            case 1049:
                if (set)  buf_.SaveState();
                else      buf_.RestoreState();
                break;
            case 47: case 1047:
                if (set)  { buf_.SaveState(); buf_.Clear(); }
                else      buf_.RestoreState();
                break;
            default: break;
        }
    }
}

ftxui::Color VtParser::StandardColor(int n) {
    static const uint8_t rgb[16][3] = {
        {0,0,0}, {170,0,0}, {0,170,0}, {170,170,0},
        {0,0,170}, {170,0,170}, {0,170,170}, {170,170,170},
        {85,85,85}, {255,85,85}, {85,255,85}, {255,255,85},
        {85,85,255}, {255,85,255}, {85,255,255}, {255,255,255},
    };
    if (n < 0 || n > 15) return ftxui::Color{};
    return ftxui::Color::RGB(rgb[n][0], rgb[n][1], rgb[n][2]);
}

ftxui::Color VtParser::Color256(int n) {
    if (n < 0 || n > 255) return ftxui::Color{};
    if (n < 16) return StandardColor(n);
    if (n >= 232) {
        uint8_t v = static_cast<uint8_t>(8 + (n - 232) * 10);
        return ftxui::Color::RGB(v, v, v);
    }
    n -= 16;
    int b = n % 6; n /= 6;
    int g = n % 6; n /= 6;
    int r = n;
    auto cv = [](int x) -> uint8_t { return x == 0 ? 0 : static_cast<uint8_t>(55 + x * 40); };
    return ftxui::Color::RGB(cv(r), cv(g), cv(b));
}

void VtParser::HandleSGR(const std::vector<int>& params) {
    if (params.empty() || (params.size() == 1 && params[0] == 0)) {
        buf_.ResetSGR(); return;
    }
    for (size_t i = 0; i < params.size(); ++i) {
        int p = params[i];
        if (p == 0) {
            buf_.ResetSGR();
        } else if (p == 1) {
            buf_.SetCurrentBold(true);
        } else if (p == 22) {
            buf_.SetCurrentBold(false);
        } else if (p >= 30 && p <= 37) {
            buf_.SetCurrentFg(StandardColor(p - 30));
        } else if (p == 38) {
            if (i + 2 < params.size() && params[i+1] == 5) {
                buf_.SetCurrentFg(Color256(params[i+2])); i += 2;
            } else if (i + 4 < params.size() && params[i+1] == 2) {
                buf_.SetCurrentFg(ftxui::Color::RGB(
                    (uint8_t)params[i+2], (uint8_t)params[i+3], (uint8_t)params[i+4]));
                i += 4;
            }
        } else if (p == 39) {
            buf_.SetCurrentFg(ftxui::Color{});
        } else if (p >= 40 && p <= 47) {
            buf_.SetCurrentBg(StandardColor(p - 40));
        } else if (p == 48) {
            if (i + 2 < params.size() && params[i+1] == 5) {
                buf_.SetCurrentBg(Color256(params[i+2])); i += 2;
            } else if (i + 4 < params.size() && params[i+1] == 2) {
                buf_.SetCurrentBg(ftxui::Color::RGB(
                    (uint8_t)params[i+2], (uint8_t)params[i+3], (uint8_t)params[i+4]));
                i += 4;
            }
        } else if (p == 49) {
            buf_.SetCurrentBg(ftxui::Color{});
        } else if (p >= 90 && p <= 97) {
            buf_.SetCurrentFg(StandardColor(p - 90 + 8));
        } else if (p >= 100 && p <= 107) {
            buf_.SetCurrentBg(StandardColor(p - 100 + 8));
        }
    }
}

} // namespace terry
