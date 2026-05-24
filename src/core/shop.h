#pragma once

#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace std;

class Unit;

struct ShopUnitData {
    string name;
    int star = 1;
};

class Shop {
public:
    static const int kDefaultLevel = 1;
    static const int kDefaultMaxLevel = 9;
    static const int kDefaultSellableUnitCount = 5;
    static const int kDefaultRefreshCost = 1;
    static const int kDefaultUpgradeCost = 5;

    Shop(int level = kDefaultLevel,
         int maxLevel = kDefaultMaxLevel,
         int sellableUnitCount = kDefaultSellableUnitCount);

    int GetLevel() const;
    int GetMaxLevel() const;
    int GetSellableUnitCount() const;
    int GetRefreshCost() const;
    int GetUpgradeCost() const;
    int GetDiscount() const;

    void SetLevel(int level);
    void SetMaxLevel(int maxLevel);
    void SetSellableUnitCount(int sellableUnitCount);
    void SetRefreshCost(int refreshCost);
    void SetUpgradeCost(int upgradeCost);

    // Discount mechanism
    void ApplyDiscount();
    void ResetDiscount();
    int GetUpgradeBaseCost() const;
    int GetCurrentUpgradeCost() const;

    bool AddUnitToPool(const ShopUnitData& unitData);
    void ClearUnitPool();
    void RefreshShop();
    void RefreshEmptySlots();

    bool IsValidSlot(int slot) const;
    shared_ptr<Unit> GetUnitAt(int slot) const;
    bool SetCurrentUnitAt(int slot, const shared_ptr<Unit>& unit);
    shared_ptr<Unit> BuyUnit(int slot);
    bool UpgradeLevel();
    void ClearCurrentUnits();

private:
    ShopUnitData PickRandomUnitData();
    shared_ptr<Unit> CreateUnit(const ShopUnitData& unitData) const;

    int level_;
    int maxLevel_;
    int sellableUnitCount_;
    int refreshCost_;
    int upgradeCost_;
    int discount_;
    vector<ShopUnitData> unitPool_;
    vector<ShopUnitData> currentUnitData_;
    vector<shared_ptr<Unit>> currentUnits_;
    mt19937 randomEngine_;
};
