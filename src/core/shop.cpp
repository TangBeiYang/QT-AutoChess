#include "shop.h"

#include "unit.h"
#include "unit_factory.h"

#include <algorithm>

Shop::Shop(int level, int maxLevel, int sellableUnitCount)
    : level_(max(1, level)),
      maxLevel_(max(1, maxLevel)),
      sellableUnitCount_(max(0, sellableUnitCount)),
      refreshCost_(kDefaultRefreshCost),
      upgradeCost_(kDefaultUpgradeCost),
      discount_(0),
      currentUnitData_(sellableUnitCount_),
      currentUnits_(sellableUnitCount_, nullptr),
      randomEngine_(random_device{}()) {
    if (level_ > maxLevel_) {
        level_ = maxLevel_;
    }
}

int Shop::GetLevel() const {
    return level_;
}

int Shop::GetMaxLevel() const {
    return maxLevel_;
}

int Shop::GetSellableUnitCount() const {
    return sellableUnitCount_;
}

int Shop::GetRefreshCost() const {
    return refreshCost_;
}

int Shop::GetUpgradeCost() const {
    return upgradeCost_;
}

void Shop::SetLevel(int level) {
    level_ = max(1, min(level, maxLevel_));
}

void Shop::SetMaxLevel(int maxLevel) {
    maxLevel_ = max(1, maxLevel);
    if (level_ > maxLevel_) {
        level_ = maxLevel_;
    }
}

void Shop::SetSellableUnitCount(int sellableUnitCount) {
    sellableUnitCount_ = max(0, sellableUnitCount);
    currentUnitData_.resize(sellableUnitCount_);
    currentUnits_.resize(sellableUnitCount_, nullptr);
}

void Shop::SetRefreshCost(int refreshCost) {
    refreshCost_ = max(0, refreshCost);
}

void Shop::SetUpgradeCost(int upgradeCost) {
    upgradeCost_ = max(0, upgradeCost);
}

int Shop::GetDiscount() const {
    return discount_;
}

void Shop::ApplyDiscount() {
    // If not upgraded this round, next round costs 1 less
    // Minimum 50% of base cost, rounded up
    const int baseCost = GetUpgradeBaseCost();
    if (discount_ < baseCost - (baseCost + 1) / 2) {
        ++discount_;
    }
}

void Shop::ResetDiscount() {
    discount_ = 0;
}

int Shop::GetUpgradeBaseCost() const {
    // Tiers: level 1->2 costs 5, 2->3 costs 7, 3->4 costs 10
    if (level_ >= 3) return 10;
    if (level_ == 2) return 7;
    return 5;
}

int Shop::GetCurrentUpgradeCost() const {
    return max((GetUpgradeBaseCost() + 1) / 2, GetUpgradeBaseCost() - discount_);
}

bool Shop::AddUnitToPool(const ShopUnitData& unitData) {
    if (unitData.name.empty() || FindUnitTemplate(unitData.name) == nullptr) {
        return false;
    }

    ShopUnitData normalized = unitData;
    normalized.star = max(1, normalized.star);
    unitPool_.push_back(normalized);
    return true;
}

void Shop::ClearUnitPool() {
    unitPool_.clear();
    ClearCurrentUnits();
}

void Shop::RefreshShop() {
    if (unitPool_.empty()) {
        ClearCurrentUnits();
        return;
    }

    for (int i = 0; i < sellableUnitCount_; ++i) {
        currentUnitData_[i] = PickRandomUnitData();
        currentUnits_[i] = CreateUnit(currentUnitData_[i]);
    }
}

void Shop::RefreshEmptySlots() {
    if (unitPool_.empty()) {
        return;
    }

    for (int i = 0; i < sellableUnitCount_; ++i) {
        if (currentUnits_[i] == nullptr) {
            currentUnitData_[i] = PickRandomUnitData();
            currentUnits_[i] = CreateUnit(currentUnitData_[i]);
        }
    }
}

bool Shop::IsValidSlot(int slot) const {
    return slot >= 0 && slot < sellableUnitCount_;
}

shared_ptr<Unit> Shop::GetUnitAt(int slot) const {
    if (!IsValidSlot(slot)) {
        return nullptr;
    }
    return currentUnits_[slot];
}

bool Shop::SetCurrentUnitAt(int slot, const shared_ptr<Unit>& unit) {
    if (!IsValidSlot(slot)) {
        return false;
    }

    currentUnits_[slot] = unit;
    if (unit == nullptr) {
        currentUnitData_[slot] = ShopUnitData();
        return true;
    }

    currentUnitData_[slot] = {unit->GetName(), unit->GetStar()};
    return true;
}

shared_ptr<Unit> Shop::BuyUnit(int slot) {
    if (!IsValidSlot(slot)) {
        return nullptr;
    }

    shared_ptr<Unit> boughtUnit = currentUnits_[slot];
    currentUnits_[slot] = nullptr;
    return boughtUnit;
}

bool Shop::UpgradeLevel() {
    if (level_ >= maxLevel_) {
        return false;
    }

    ++level_;
    return true;
}

void Shop::ClearCurrentUnits() {
    for (int i = 0; i < sellableUnitCount_; ++i) {
        currentUnits_[i] = nullptr;
        currentUnitData_[i] = ShopUnitData();
    }
}

ShopUnitData Shop::PickRandomUnitData() {
    uniform_int_distribution<int> distribution(0, static_cast<int>(unitPool_.size()) - 1);
    return unitPool_[distribution(randomEngine_)];
}

shared_ptr<Unit> Shop::CreateUnit(const ShopUnitData& unitData) const {
    return CreateUnitByName(unitData.name, unitData.star);
}
