#include "cell_buffer.h"
#include <algorithm>

namespace terry {

static Cell blank_cell() {
    return Cell{U' ', ftxui::Color{}, ftxui::Color{}, false};
}

CellBuffer::CellBuffer(int cols, int rows)
    : cols_(cols), rows_(rows), cells_(cols * rows, blank_cell()) {}

void CellBuffer::Resize(int nc, int nr) {
    std::vector<Cell> nc_cells(nc * nr, blank_cell());
    int cr = std::min(rows_, nr);
    int cc = std::min(cols_, nc);
    for (int r = 0; r < cr; ++r)
        for (int c = 0; c < cc; ++c)
            nc_cells[r * nc + c] = cells_[r * cols_ + c];
    cols_ = nc; rows_ = nr;
    cells_ = std::move(nc_cells);
    ClampCursor();
}

Cell& CellBuffer::At(int col, int row) {
    return cells_[row * cols_ + col];
}
const Cell& CellBuffer::At(int col, int row) const {
    return cells_[row * cols_ + col];
}

void CellBuffer::SetCursor(int col, int row) {
    cursor_col_ = col;
    cursor_row_ = row;
    ClampCursor();
}

void CellBuffer::ClampCursor() {
    cursor_col_ = std::max(0, std::min(cursor_col_, cols_ - 1));
    cursor_row_ = std::max(0, std::min(cursor_row_, rows_ - 1));
}

void CellBuffer::ResetSGR() {
    current_fg_   = ftxui::Color{};
    current_bg_   = ftxui::Color{};
    current_bold_ = false;
}

Cell CellBuffer::blank() const {
    return Cell{U' ', ftxui::Color{}, ftxui::Color{}, false};
}

void CellBuffer::PutChar(char32_t ch) {
    if (cursor_col_ >= cols_) {
        cursor_col_ = 0;
        ++cursor_row_;
        if (cursor_row_ >= rows_) {
            ScrollUp(1);
            cursor_row_ = rows_ - 1;
        }
    }
    if (cursor_row_ < 0 || cursor_row_ >= rows_ ||
        cursor_col_ < 0 || cursor_col_ >= cols_) return;

    At(cursor_col_, cursor_row_) = {ch, current_fg_, current_bg_, current_bold_};
    ++cursor_col_;
}

void CellBuffer::ClearLine(int row, int from_col, int to_col) {
    if (row < 0 || row >= rows_) return;
    if (to_col < 0) to_col = cols_ - 1;
    from_col = std::max(0, std::min(from_col, cols_ - 1));
    to_col   = std::max(0, std::min(to_col,   cols_ - 1));
    for (int c = from_col; c <= to_col; ++c)
        At(c, row) = blank_cell();
}

void CellBuffer::Clear() {
    std::fill(cells_.begin(), cells_.end(), blank_cell());
    cursor_col_ = cursor_row_ = 0;
}

void CellBuffer::EraseDisplay(int mode) {
    if (mode == 0) {
        ClearLine(cursor_row_, cursor_col_, -1);
        for (int r = cursor_row_ + 1; r < rows_; ++r) ClearLine(r);
    } else if (mode == 1) {
        for (int r = 0; r < cursor_row_; ++r) ClearLine(r);
        ClearLine(cursor_row_, 0, cursor_col_);
    } else {
        Clear();
    }
}

void CellBuffer::EraseLine(int mode) {
    if (mode == 0)      ClearLine(cursor_row_, cursor_col_, -1);
    else if (mode == 1) ClearLine(cursor_row_, 0, cursor_col_);
    else                ClearLine(cursor_row_);
}

void CellBuffer::ScrollUp(int n) {
    for (int i = 0; i < n; ++i) {
        for (int r = 0; r < rows_ - 1; ++r)
            for (int c = 0; c < cols_; ++c)
                At(c, r) = At(c, r + 1);
        ClearLine(rows_ - 1);
    }
}

void CellBuffer::ScrollDown(int n) {
    for (int i = 0; i < n; ++i) {
        for (int r = rows_ - 1; r > 0; --r)
            for (int c = 0; c < cols_; ++c)
                At(c, r) = At(c, r - 1);
        ClearLine(0);
    }
}

void CellBuffer::SaveState() {
    saved_ = std::make_unique<SavedState>();
    saved_->cols = cols_; saved_->rows = rows_;
    saved_->cursor_col = cursor_col_; saved_->cursor_row = cursor_row_;
    saved_->fg = current_fg_; saved_->bg = current_bg_; saved_->bold = current_bold_;
    saved_->cells = cells_;
}

void CellBuffer::RestoreState() {
    if (!saved_) { Clear(); return; }
    cols_ = saved_->cols; rows_ = saved_->rows;
    cursor_col_ = saved_->cursor_col; cursor_row_ = saved_->cursor_row;
    current_fg_ = saved_->fg; current_bg_ = saved_->bg; current_bold_ = saved_->bold;
    cells_ = std::move(saved_->cells);
    saved_.reset();
}

} // namespace terry
