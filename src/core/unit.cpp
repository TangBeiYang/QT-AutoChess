#include "unit.h"

#include <algorithm>
#include <utility>

using namespace std;

Unit::Unit(string name, int star, int maxHp, int atk, int defense, int magicResist, Player* owner, int price, string profession,
           double attackSpeed, double attackRange, double moveSpeed, int manaPerAttack, int maxMana,
           double manaOnHitMultiplier, int manaRegenPerSecond, int critRate, double critDamage)
    : name_(move(name)),
      profession_(profession.empty() ? "战士" : std::move(profession)),
      star_(max(1, star)),
      hp_(max(0, maxHp)),
      maxHp_(max(0, maxHp)),
      atk_(max(0, atk)),
      defense_(max(0, defense)),
      magicResist_(max(0, min(magicResist, 99))),
      mana_(0),
      maxMana_(max(1, maxMana)),
      price_(max(0, price)),
      sellPrice_(max(1, price - 1)),
      owner_(owner),
      maxEquipmentCount_(2),
      currentEquipmentCount_(0),
      baseAttackSpeed_(max(0.1, attackSpeed)),
      baseMoveSpeed_(max(0.1, moveSpeed)),
      baseAttackRange_(max(0.1, attackRange)),
      baseManaPerAttack_(max(0, manaPerAttack)),
      manaRegenPerSecond_(max(0, manaRegenPerSecond)),
      manaOnHitMultiplier_(max(0.0, manaOnHitMultiplier)),
      critRate_(max(0, min(critRate, 100))),
      critDamage_(max(1.0, critDamage)) {}

const string& Unit::GetName() const {
    return name_;
}

const string& Unit::GetProfession() const {
    return profession_;
}

int Unit::GetStar() const {
    return star_;
}

int Unit::GetHp() const {
    return hp_;
}

int Unit::GetMaxHp() const {
    int bonus = 0;
    for (const Equipment& equipment : equipments_) {
        bonus += equipment.GetHpBonus();
    }
    return maxHp_ + bonus;
}

int Unit::GetBaseMaxHp() const {
    return maxHp_;
}

int Unit::GetAtk() const {
    int bonus = 0;
    for (const Equipment& equipment : equipments_) {
        bonus += equipment.GetAtkBonus();
    }
    return atk_ + bonus;
}

int Unit::GetBaseAtk() const {
    return atk_;
}

int Unit::GetDefense() const {
    int bonus = 0;
    for (const Equipment& equipment : equipments_) {
        bonus += equipment.GetDefenseBonus();
    }
    return defense_ + bonus;
}

int Unit::GetBaseDefense() const {
    return defense_;
}

int Unit::GetMagicResist() const {
    int bonus = 0;
    for (const Equipment& equipment : equipments_) {
        bonus += equipment.GetMagicResistBonus();
    }
    return min(99, magicResist_ + bonus);
}

int Unit::GetBaseMagicResist() const {
    return magicResist_;
}

int Unit::GetMana() const {
    return mana_;
}

void Unit::SetMana(int mana) {
    mana_ = max(0, min(mana, maxMana_));
}

int Unit::GetMaxMana() const {
    return maxMana_;
}

void Unit::SetMaxMana(int maxMana) {
    maxMana_ = max(1, maxMana);
    mana_ = min(mana_, maxMana_);
}

void Unit::AddMana(int amount) {
    if (amount <= 0 || mana_ >= maxMana_) {
        return;
    }
    mana_ = min(maxMana_, mana_ + amount);
}

bool Unit::IsManaFull() const {
    return mana_ >= maxMana_;
}

void Unit::ResetMana() {
    mana_ = 0;
}

int Unit::GetPrice() const {
    return price_;
}

int Unit::GetSellPrice() const {
    return sellPrice_;
}

Player* Unit::GetOwner() const {
    return owner_;
}

const vector<Equipment>& Unit::GetEquipments() const {
    return equipments_;
}

int Unit::GetMaxEquipmentCount() const {
    return maxEquipmentCount_;
}

int Unit::GetCurrentEquipmentCount() const {
    return currentEquipmentCount_;
}

void Unit::SetName(const string& name) {
    name_ = name;
}

void Unit::SetProfession(const string& profession) {
    profession_ = profession.empty() ? "战士" : profession;
    mana_ = min(mana_, maxMana_);
}

void Unit::SetStar(int star) {
    star_ = max(1, star);
}

void Unit::SetHp(int hp) {
    hp_ = max(0, min(hp, GetMaxHp()));
}

void Unit::SetMaxHp(int maxHp) {
    maxHp_ = max(0, maxHp);
    hp_ = max(0, min(hp_, GetMaxHp()));
}

void Unit::SetAtk(int atk) {
    atk_ = max(0, atk);
}

void Unit::SetDefense(int defense) {
    defense_ = max(0, defense);
}

void Unit::SetMagicResist(int magicResist) {
    magicResist_ = max(0, min(magicResist, 99));
}

void Unit::SetPrice(int price) {
    price_ = max(0, price);
}

void Unit::SetSellPrice(int sellPrice) {
    sellPrice_ = max(0, sellPrice);
}

void Unit::SetOwner(Player* owner) {
    owner_ = owner;
}

void Unit::SetMaxEquipmentCount(int maxEquipmentCount) {
    maxEquipmentCount_ = max(0, maxEquipmentCount);
    if (static_cast<int>(equipments_.size()) > maxEquipmentCount_) {
        equipments_.resize(maxEquipmentCount_);
    }
    currentEquipmentCount_ = static_cast<int>(equipments_.size());
}

void Unit::TakeDamage(int damage, DamageType damageType) {
    if (damage <= 0) {
        return;
    }

    int finalDamage = damage;
    if (damageType == DamageType::Physical) {
        constexpr int kMinimumDamageDivisor = 100;
        const int minimumDamage = max(1, damage / kMinimumDamageDivisor);
        finalDamage = max(minimumDamage, damage - GetDefense());
    } else {
        finalDamage = max(1, damage * (100 - GetMagicResist()) / 100);
    }

    hp_ = max(0, hp_ - finalDamage);
}

void Unit::Heal(int amount) {
    if (amount <= 0) {
        return;
    }
    hp_ = min(GetMaxHp(), hp_ + amount);
}

bool Unit::IsAlive() const {
    return hp_ > 0;
}

double Unit::GetAttackRange() const {
    return baseAttackRange_;
}

double Unit::GetMoveSpeed() const {
    return baseMoveSpeed_;
}

int Unit::GetManaPerAttack() const {
    return baseManaPerAttack_;
}

Unit::SkillParams Unit::GetSkillParams() const {
    return {};
}

Unit::DamageType Unit::GetDamageType() const {
    return DamageType::Physical;
}

double Unit::GetBaseAttackSpeed() const { return baseAttackSpeed_; }
double Unit::GetBaseMoveSpeed() const { return baseMoveSpeed_; }
double Unit::GetBaseAttackRange() const { return baseAttackRange_; }
int Unit::GetBaseManaPerAttack() const { return baseManaPerAttack_; }
int Unit::GetBaseMaxMana() const { return maxMana_; }
int Unit::GetManaRegenPerSecond() const { return manaRegenPerSecond_; }
double Unit::GetManaOnHitMultiplier() const { return manaOnHitMultiplier_; }
int Unit::GetCritRate() const { return critRate_; }
double Unit::GetCritDamage() const { return critDamage_; }

void Unit::SetBaseAttackSpeed(double speed) { baseAttackSpeed_ = max(0.1, speed); }
void Unit::SetBaseMoveSpeed(double speed) { baseMoveSpeed_ = max(0.1, speed); }
void Unit::SetBaseAttackRange(double range) { baseAttackRange_ = max(0.1, range); }
void Unit::SetManaRegenPerSecond(int regen) { manaRegenPerSecond_ = max(0, regen); }
void Unit::SetManaOnHitMultiplier(double mult) { manaOnHitMultiplier_ = max(0.0, mult); }
void Unit::SetCritRate(int rate) { critRate_ = max(0, min(rate, 100)); }
void Unit::SetCritDamage(double dmg) { critDamage_ = max(1.0, dmg); }

void Unit::SetMaxManaBase(int maxMana) { maxMana_ = max(1, maxMana); mana_ = min(mana_, maxMana_); }

double Unit::GetStarMultiplier(int star) {
    switch (star) {
    case 1: return 1.0;
    case 2: return 1.8;
    case 3: return 3.2;
    default: return 1.0;
    }
}

void Unit::UpgradeStar() {
    if (star_ >= 3) {
        return;
    }

    const double oldMult = GetStarMultiplier(star_);
    ++star_;
    const double newMult = GetStarMultiplier(star_);
    const double ratio = newMult / oldMult;

    maxHp_ = static_cast<int>(maxHp_ * ratio);
    hp_ = maxHp_;
    atk_ = static_cast<int>(atk_ * ratio);
    defense_ = static_cast<int>(defense_ * ratio);
    magicResist_ = static_cast<int>(magicResist_ * ratio);
    sellPrice_ = static_cast<int>(sellPrice_ * ratio);
    mana_ = 0;
}

bool Unit::AddEquipment(const Equipment& equipment) {
    if (currentEquipmentCount_ >= maxEquipmentCount_) {
        return false;
    }

    const int hpBefore = GetMaxHp();
    equipments_.push_back(equipment);
    currentEquipmentCount_ = static_cast<int>(equipments_.size());
    const int hpAfter = GetMaxHp();
    hp_ = min(hpAfter, hp_ + max(0, hpAfter - hpBefore));
    return true;
}

bool Unit::RemoveEquipment(int index) {
    if (index < 0 || index >= currentEquipmentCount_) {
        return false;
    }

    equipments_.erase(equipments_.begin() + index);
    currentEquipmentCount_ = static_cast<int>(equipments_.size());
    hp_ = min(hp_, GetMaxHp());
    return true;
}

Equipment Unit::TakeEquipment(int index) {
    if (index < 0 || index >= currentEquipmentCount_) {
        return Equipment();
    }

    Equipment removed = equipments_[index];
    equipments_.erase(equipments_.begin() + index);
    currentEquipmentCount_ = static_cast<int>(equipments_.size());
    hp_ = min(hp_, GetMaxHp());
    return removed;
}

vector<Equipment> Unit::TakeAllEquipments() {
    vector<Equipment> removed = equipments_;
    equipments_.clear();
    currentEquipmentCount_ = 0;
    hp_ = min(hp_, GetMaxHp());
    return removed;
}

void Unit::ClearEquipments() {
    equipments_.clear();
    currentEquipmentCount_ = 0;
}
