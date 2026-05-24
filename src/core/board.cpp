#include "board.h"

#include "unit.h"

#include <utility>

Board::Board() : grid_(kRows, vector<shared_ptr<Unit>>(kCols, nullptr)) {}

bool Board::IsValidPosition(int row, int col) const {
    return row >= 0 && row < kRows && col >= 0 && col < kCols;
}

bool Board::IsOpponentHalf(int row) const {
    return row >= 0 && row < (kRows / 2);
}

bool Board::IsPlayerHalf(int row) const {
    return row >= (kRows / 2) && row < kRows;
}

bool Board::PlaceOpponentUnit(int row, int col, const shared_ptr<Unit>& unit) {
    if (!IsOpponentHalf(row)) {
        return false;
    }
    return PlaceUnit(row, col, unit);
}

bool Board::PlaceUnitAt(int row, int col, const shared_ptr<Unit>& unit) {
    return PlaceUnit(row, col, unit);
}

bool Board::MoveUnit(int fromRow, int fromCol, int toRow, int toCol) {
    if (!IsValidPosition(fromRow, fromCol) || !IsValidPosition(toRow, toCol)) {
        return false;
    }

    if (grid_[fromRow][fromCol] == nullptr || grid_[toRow][toCol] != nullptr) {
        return false;
    }

    grid_[toRow][toCol] = grid_[fromRow][fromCol];
    grid_[fromRow][fromCol] = nullptr;
    return true;
}

bool Board::SwapUnits(int firstRow, int firstCol, int secondRow, int secondCol) {
    if (!IsValidPosition(firstRow, firstCol) || !IsValidPosition(secondRow, secondCol)) {
        return false;
    }

    if (grid_[firstRow][firstCol] == nullptr && grid_[secondRow][secondCol] == nullptr) {
        return false;
    }

    swap(grid_[firstRow][firstCol], grid_[secondRow][secondCol]);
    return true;
}

shared_ptr<Unit> Board::RemoveUnit(int row, int col) {
    if (!IsValidPosition(row, col)) {
        return nullptr;
    }

    shared_ptr<Unit> removed = grid_[row][col];
    grid_[row][col] = nullptr;
    return removed;
}

shared_ptr<Unit> Board::GetUnitAt(int row, int col) const {
    if (!IsValidPosition(row, col)) {
        return nullptr;
    }
    return grid_[row][col];
}

bool Board::IsEmpty(int row, int col) const {
    if (!IsValidPosition(row, col)) {
        return false;
    }
    return grid_[row][col] == nullptr;
}

void Board::Clear() {
    for (int row = 0; row < kRows; ++row) {
        for (int col = 0; col < kCols; ++col) {
            grid_[row][col] = nullptr;
        }
    }
}

bool Board::PlaceUnit(int row, int col, const shared_ptr<Unit>& unit) {
    if (!IsValidPosition(row, col) || unit == nullptr) {
        return false;
    }

    if (grid_[row][col] != nullptr) {
        return false;
    }

    grid_[row][col] = unit;
    return true;
}
