#pragma once
#include <vector>
#include <memory>
#include <ftxui/screen/color.hpp>

namespace terry {

struct Cell {
    char32_t ch  = U' ';
    ftxui::Color fg{};
    ftxui::Color bg{};
    bool bold    = false;
};

class CellBuffer {
public:
    CellBuffer(int cols = 80, int rows = 24);

    void Resize(int cols, int rows);

    Cell&       At(int col, int row);
    const Cell& At(int col, int row) const;

    int Cols() const { return cols_; }
    int Rows() const { return rows_; }

    int  CursorCol()     const { return cursor_col_; }
    int  CursorRow()     const { return cursor_row_; }
    bool CursorVisible() const { return cursor_visible_; }

    void SetCursor(int col, int row);
    void ClampCursor();
    void SetCursorVisible(bool v) { cursor_visible_ = v; }

    // SGR current state
    ftxui::Color CurrentFg()   const { return current_fg_; }
    ftxui::Color CurrentBg()   const { return current_bg_; }
    bool         CurrentBold() const { return current_bold_; }
    void SetCurrentFg(ftxui::Color c)   { current_fg_   = c; }
    void SetCurrentBg(ftxui::Color c)   { current_bg_   = c; }
    void SetCurrentBold(bool b)         { current_bold_ = b; }
    void ResetSGR();

    // Write at cursor, advance
    void PutChar(char32_t ch);

    // Erase
    void EraseDisplay(int mode); // 0=below, 1=above, 2=all
    void EraseLine(int mode);    // 0=right, 1=left, 2=all
    void ClearLine(int row, int from_col = 0, int to_col = -1);
    void Clear();

    // Scroll
    void ScrollUp(int n = 1);
    void ScrollDown(int n = 1);

    // Alternate screen
    void SaveState();
    void RestoreState();

private:
    int cols_, rows_;
    int cursor_col_ = 0, cursor_row_ = 0;
    bool cursor_visible_ = true;

    ftxui::Color current_fg_{};
    ftxui::Color current_bg_{};
    bool current_bold_ = false;

    std::vector<Cell> cells_;

    struct SavedState {
        int cols, rows;
        int cursor_col, cursor_row;
        ftxui::Color fg, bg;
        bool bold;
        std::vector<Cell> cells;
    };
    std::unique_ptr<SavedState> saved_;

    Cell blank() const;
};

} // namespace terry
