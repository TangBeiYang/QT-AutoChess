#include "bench.h"

#include "unit.h"

#include <utility>

Bench::Bench() : slots_(kSize, nullptr) {}

bool Bench::IsValidSlot(int slot) const {
    return slot >= 0 && slot < kSize;
}

bool Bench::IsEmpty(int slot) const {
    if (!IsValidSlot(slot)) {
        return false;
    }
    return slots_[slot] == nullptr;
}

bool Bench::PlaceUnit(int slot, const shared_ptr<Unit>& unit) {
    if (!IsValidSlot(slot) || unit == nullptr) {
        return false;
    }

    if (slots_[slot] != nullptr) {
        return false;
    }

    slots_[slot] = unit;
    return true;
}

bool Bench::AddUnitToFirstEmptyFromRight(const shared_ptr<Unit>& unit) {
    if (unit == nullptr) {
        return false;
    }

    for (int i = kSize - 1; i >= 0; --i) {
        if (slots_[i] == nullptr) {
            slots_[i] = unit;
            return true;
        }
    }

    return false;
}

bool Bench::MoveUnit(int fromSlot, int toSlot) {
    if (!IsValidSlot(fromSlot) || !IsValidSlot(toSlot)) {
        return false;
    }

    if (slots_[fromSlot] == nullptr || slots_[toSlot] != nullptr) {
        return false;
    }

    slots_[toSlot] = slots_[fromSlot];
    slots_[fromSlot] = nullptr;
    return true;
}

bool Bench::SwapUnits(int firstSlot, int secondSlot) {
    if (!IsValidSlot(firstSlot) || !IsValidSlot(secondSlot)) {
        return false;
    }

    if (slots_[firstSlot] == nullptr && slots_[secondSlot] == nullptr) {
        return false;
    }

    swap(slots_[firstSlot], slots_[secondSlot]);
    return true;
}

shared_ptr<Unit> Bench::RemoveUnit(int slot) {
    if (!IsValidSlot(slot)) {
        return nullptr;
    }

    shared_ptr<Unit> removed = slots_[slot];
    slots_[slot] = nullptr;
    return removed;
}

shared_ptr<Unit> Bench::RemoveUnitAndShiftRight(int slot) {
    if (!IsValidSlot(slot) || slots_[slot] == nullptr) {
        return nullptr;
    }

    shared_ptr<Unit> removedUnit = slots_[slot];
    for (int i = slot; i > 0; --i) {
        slots_[i] = slots_[i - 1];
    }
    slots_[0] = nullptr;
    return removedUnit;
}

shared_ptr<Unit> Bench::SellUnitAndShiftRight(int slot) {
    return RemoveUnitAndShiftRight(slot);
}

shared_ptr<Unit> Bench::GetUnitAt(int slot) const {
    if (!IsValidSlot(slot)) {
        return nullptr;
    }
    return slots_[slot];
}

int Bench::GetCurrentCount() const {
    int count = 0;
    for (int i = 0; i < kSize; ++i) {
        if (slots_[i] != nullptr) {
            ++count;
        }
    }
    return count;
}

void Bench::Clear() {
    for (int i = 0; i < kSize; ++i) {
        slots_[i] = nullptr;
    }
}
