#pragma once

#include <memory>
#include <vector>

using namespace std;

class Unit;

class Board {
public:
    static const int kRows = 8;
    static const int kCols = 8;

    Board();

    bool IsValidPosition(int row, int col) const;
    bool IsOpponentHalf(int row) const;
    bool IsPlayerHalf(int row) const;

    bool PlaceOpponentUnit(int row, int col, const shared_ptr<Unit>& unit);
    bool PlaceUnitAt(int row, int col, const shared_ptr<Unit>& unit);
    bool MoveUnit(int fromRow, int fromCol, int toRow, int toCol);
    bool SwapUnits(int firstRow, int firstCol, int secondRow, int secondCol);

    shared_ptr<Unit> RemoveUnit(int row, int col);
    shared_ptr<Unit> GetUnitAt(int row, int col) const;

    bool IsEmpty(int row, int col) const;
    void Clear();

private:
    bool PlaceUnit(int row, int col, const shared_ptr<Unit>& unit);

    vector<vector<shared_ptr<Unit>>> grid_;
};
