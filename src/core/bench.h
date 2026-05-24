#pragma once

#include <memory>
#include <vector>

using namespace std;

class Unit;

class Bench {
public:
    static const int kSize = 8;

    Bench();

    bool IsValidSlot(int slot) const;
    bool IsEmpty(int slot) const;

    bool PlaceUnit(int slot, const shared_ptr<Unit>& unit);
    bool AddUnitToFirstEmptyFromRight(const shared_ptr<Unit>& unit);
    bool MoveUnit(int fromSlot, int toSlot);
    bool SwapUnits(int firstSlot, int secondSlot);

    shared_ptr<Unit> RemoveUnit(int slot);
    shared_ptr<Unit> RemoveUnitAndShiftRight(int slot);
    shared_ptr<Unit> SellUnitAndShiftRight(int slot);
    shared_ptr<Unit> GetUnitAt(int slot) const;

    int GetCurrentCount() const;
    void Clear();

private:
    vector<shared_ptr<Unit>> slots_;
};
