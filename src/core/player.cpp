#include "player.h"

#include "unit.h"

#include <algorithm>

using namespace std;

Player::Player(int hp, int gold, int level, int populationLimit)
    : hp_(max(0, hp)),
      gold_(max(0, gold)),
      level_(max(1, level)),
      populationLimit_(max(0, populationLimit)) {}

int Player::GetHp() const {
    return hp_;
}

int Player::GetGold() const {
    return gold_;
}

int Player::GetLevel() const {
    return level_;
}

int Player::GetPopulationLimit() const {
    return populationLimit_;
}

const vector<shared_ptr<Unit>>& Player::GetUnits() const {
    return units_;
}

void Player::SetHp(int hp) {
    hp_ = max(0, hp);
}

void Player::SetGold(int gold) {
    gold_ = max(0, gold);
}

void Player::SetLevel(int level) {
    level_ = max(1, level);
}

void Player::SetPopulationLimit(int populationLimit) {
    populationLimit_ = max(0, populationLimit);
    if (static_cast<int>(units_.size()) > populationLimit_) {
        for (int i = populationLimit_; i < static_cast<int>(units_.size()); ++i) {
            if (units_[i]) {
                units_[i]->SetOwner(nullptr);
            }
        }
        units_.resize(populationLimit_);
    }
}

void Player::AddGold(int amount) {
    if (amount <= 0) {
        return;
    }

    gold_ += amount;
}

bool Player::SpendGold(int amount) {
    if (amount < 0 || gold_ < amount) {
        return false;
    }

    gold_ -= amount;
    return true;
}

bool Player::AddUnit(const shared_ptr<Unit>& unit) {
    if (!unit) {
        return false;
    }

    if (static_cast<int>(units_.size()) >= populationLimit_) {
        return false;
    }

    units_.push_back(unit);
    unit->SetOwner(this);
    return true;
}

bool Player::RemoveUnit(const shared_ptr<Unit>& unit) {
    auto it = find(units_.begin(), units_.end(), unit);
    if (it == units_.end()) {
        return false;
    }

    if (*it) {
        (*it)->SetOwner(nullptr);
    }
    units_.erase(it);
    return true;
}

void Player::ClearUnits() {
    for (const auto& unit : units_) {
        if (unit) {
            unit->SetOwner(nullptr);
        }
    }
    units_.clear();
}
