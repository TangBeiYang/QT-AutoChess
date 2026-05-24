#include "equipment_inventory.h"

EquipmentInventory::EquipmentInventory()
    : slots_(kSlotCount, nullptr) {}

bool EquipmentInventory::IsValidSlot(int slot) const {
    return slot >= 0 && slot < kSlotCount;
}

bool EquipmentInventory::IsEmpty(int slot) const {
    return IsValidSlot(slot) && slots_[slot] == nullptr;
}

bool EquipmentInventory::IsFull() const {
    if (!overflow_.empty()) {
        return true;
    }
    return GetFirstEmptySlot() == -1;
}

int EquipmentInventory::GetFirstEmptySlot() const {
    for (int i = 0; i < kSlotCount; ++i) {
        if (slots_[i] == nullptr) {
            return i;
        }
    }
    return -1;
}

int EquipmentInventory::GetOverflowCount() const {
    return static_cast<int>(overflow_.size());
}

shared_ptr<Equipment> EquipmentInventory::GetEquipmentAt(int slot) const {
    if (!IsValidSlot(slot)) {
        return nullptr;
    }
    return slots_[slot];
}

shared_ptr<Equipment> EquipmentInventory::GetOverflowEquipmentAt(int index) const {
    if (index < 0 || index >= static_cast<int>(overflow_.size())) {
        return nullptr;
    }
    return overflow_[index];
}

bool EquipmentInventory::AddPurchasedEquipment(const Equipment& equipment) {
    return AddPurchasedEquipment(make_shared<Equipment>(equipment));
}

bool EquipmentInventory::AddPurchasedEquipment(const shared_ptr<Equipment>& equipment) {
    if (equipment == nullptr || IsFull()) {
        return false;
    }

    const int emptySlot = GetFirstEmptySlot();
    if (emptySlot == -1) {
        return false;
    }

    slots_[emptySlot] = make_shared<Equipment>(*equipment);
    return true;
}

void EquipmentInventory::AddRecoveredEquipment(const Equipment& equipment) {
    AddRecoveredEquipment(make_shared<Equipment>(equipment));
}

void EquipmentInventory::AddRecoveredEquipment(const shared_ptr<Equipment>& equipment) {
    if (equipment == nullptr) {
        return;
    }

    if (overflow_.empty()) {
        const int emptySlot = GetFirstEmptySlot();
        if (emptySlot != -1) {
            slots_[emptySlot] = make_shared<Equipment>(*equipment);
            return;
        }
    }

    overflow_.push_back(make_shared<Equipment>(*equipment));
}

shared_ptr<Equipment> EquipmentInventory::RemoveEquipmentAt(int slot) {
    if (!IsValidSlot(slot)) {
        return nullptr;
    }

    shared_ptr<Equipment> removed = slots_[slot];
    slots_[slot] = nullptr;
    CompactOverflow();
    return removed;
}

shared_ptr<Equipment> EquipmentInventory::RemoveOverflowEquipmentAt(int index) {
    if (index < 0 || index >= static_cast<int>(overflow_.size())) {
        return nullptr;
    }

    shared_ptr<Equipment> removed = overflow_[index];
    overflow_.erase(overflow_.begin() + index);
    CompactOverflow();
    return removed;
}

void EquipmentInventory::SetEquipmentAt(int slot, const shared_ptr<Equipment>& equipment) {
    if (!IsValidSlot(slot)) {
        return;
    }

    if (equipment == nullptr) {
        slots_[slot] = nullptr;
        CompactOverflow();
        return;
    }

    slots_[slot] = make_shared<Equipment>(*equipment);
}

void EquipmentInventory::AddOverflowEquipment(const shared_ptr<Equipment>& equipment) {
    if (equipment == nullptr) {
        return;
    }
    overflow_.push_back(make_shared<Equipment>(*equipment));
}

void EquipmentInventory::CompactOverflow() {
    if (overflow_.empty()) {
        return;
    }

    int emptySlot = GetFirstEmptySlot();
    while (emptySlot != -1 && !overflow_.empty()) {
        slots_[emptySlot] = overflow_.front();
        overflow_.erase(overflow_.begin());
        emptySlot = GetFirstEmptySlot();
    }
}

void EquipmentInventory::Clear() {
    for (shared_ptr<Equipment>& slot : slots_) {
        slot = nullptr;
    }
    overflow_.clear();
}
