#pragma once

#include <memory>
#include <vector>

using namespace std;

class Unit;

class Player
{
public:
    Player(int hp = 20, int gold = 0, int level = 1, int populationLimit = 1);

    int GetHp() const;
    int GetGold() const;
    int GetLevel() const;
    int GetPopulationLimit() const;
    const vector<shared_ptr<Unit>> &GetUnits() const;

    void SetHp(int hp);
    void SetGold(int gold);
    void SetLevel(int level);
    void SetPopulationLimit(int populationLimit);
    void AddGold(int amount);
    bool SpendGold(int amount);

    bool AddUnit(const shared_ptr<Unit> &unit);
    bool RemoveUnit(const shared_ptr<Unit> &unit);
    void ClearUnits();

private:
    int hp_;
    int gold_;
    int level_;
    vector<shared_ptr<Unit>> units_;
    int populationLimit_;
};
