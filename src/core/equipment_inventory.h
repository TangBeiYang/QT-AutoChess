#pragma once

#include "equipment.h"

#include <memory>
#include <vector>

using namespace std;

class EquipmentInventory {
public:
    static const int kSlotCount = 4;

    EquipmentInventory();

    bool IsValidSlot(int slot) const;
    bool IsEmpty(int slot) const;
    bool IsFull() const;
    int GetFirstEmptySlot() const;
    int GetOverflowCount() const;

    shared_ptr<Equipment> GetEquipmentAt(int slot) const;
    shared_ptr<Equipment> GetOverflowEquipmentAt(int index) const;

    bool AddPurchasedEquipment(const Equipment& equipment);
    bool AddPurchasedEquipment(const shared_ptr<Equipment>& equipment);
    void AddRecoveredEquipment(const Equipment& equipment);
    void AddRecoveredEquipment(const shared_ptr<Equipment>& equipment);

    shared_ptr<Equipment> RemoveEquipmentAt(int slot);
    shared_ptr<Equipment> RemoveOverflowEquipmentAt(int index);

    void SetEquipmentAt(int slot, const shared_ptr<Equipment>& equipment);
    void AddOverflowEquipment(const shared_ptr<Equipment>& equipment);
    void CompactOverflow();
    void Clear();

private:
    vector<shared_ptr<Equipment>> slots_;
    vector<shared_ptr<Equipment>> overflow_;
};
