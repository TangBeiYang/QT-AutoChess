#include "equipment.h"

#include <algorithm>
#include <utility>

using namespace std;

Equipment::Equipment(string name, int price, int atkBonus, int hpBonus, int defenseBonus, int magicResistBonus,
                     int star, int lifesteal, int armorPenetration, double cleaveRange, double cleaveDamage,
                     int reviveHpPercent, int auraDamage, int skillDamageBonus, int initialManaBonus,
                     int hpRegenPercent, int damageReflectPercent, bool doubleHit, double percentHpDamage,
                     int extraManaOnHit, bool splashAttack, bool canFuse, string recipe1, string recipe2)
    : name_(move(name)),
      star_(max(1, min(star, 2))),
      price_(max(0, price)),
      atkBonus_(max(0, atkBonus)),
      hpBonus_(max(0, hpBonus)),
      defenseBonus_(max(0, defenseBonus)),
      magicResistBonus_(max(0, min(magicResistBonus, 99))),
      lifesteal_(max(0, lifesteal)),
      armorPenetration_(max(0, min(armorPenetration, 100))),
      cleaveRange_(max(0.0, cleaveRange)),
      cleaveDamage_(max(0.0, cleaveDamage)),
      reviveHpPercent_(max(0, min(reviveHpPercent, 100))),
      auraDamage_(max(0, auraDamage)),
      skillDamageBonus_(max(0, skillDamageBonus)),
      initialManaBonus_(max(0, initialManaBonus)),
      hpRegenPercent_(max(0, min(hpRegenPercent, 100))),
      damageReflectPercent_(max(0, min(damageReflectPercent, 100))),
      doubleHit_(doubleHit),
      percentHpDamage_(max(0.0, percentHpDamage)),
      extraManaOnHit_(max(0, extraManaOnHit)),
      splashAttack_(splashAttack),
      canFuse_(canFuse),
      recipe1_(move(recipe1)),
      recipe2_(move(recipe2)) {}

const string& Equipment::GetName() const { return name_; }
int Equipment::GetStar() const { return star_; }
int Equipment::GetPrice() const { return price_; }
int Equipment::GetAtkBonus() const { return atkBonus_; }
int Equipment::GetHpBonus() const { return hpBonus_; }
int Equipment::GetDefenseBonus() const { return defenseBonus_; }
int Equipment::GetMagicResistBonus() const { return magicResistBonus_; }

int Equipment::GetLifesteal() const { return lifesteal_; }
int Equipment::GetArmorPenetration() const { return armorPenetration_; }
double Equipment::GetCleaveRange() const { return cleaveRange_; }
double Equipment::GetCleaveDamage() const { return cleaveDamage_; }
int Equipment::GetReviveHpPercent() const { return reviveHpPercent_; }
int Equipment::GetAuraDamage() const { return auraDamage_; }
int Equipment::GetSkillDamageBonus() const { return skillDamageBonus_; }
int Equipment::GetInitialManaBonus() const { return initialManaBonus_; }
int Equipment::GetHpRegenPercent() const { return hpRegenPercent_; }
int Equipment::GetDamageReflectPercent() const { return damageReflectPercent_; }
bool Equipment::GetDoubleHit() const { return doubleHit_; }
double Equipment::GetPercentHpDamage() const { return percentHpDamage_; }
int Equipment::GetExtraManaOnHit() const { return extraManaOnHit_; }
bool Equipment::GetSplashAttack() const { return splashAttack_; }
bool Equipment::CanFuse() const { return canFuse_; }
const string& Equipment::GetRecipe1() const { return recipe1_; }
const string& Equipment::GetRecipe2() const { return recipe2_; }

void Equipment::SetName(const string& name) { name_ = name; }
void Equipment::SetStar(int star) { star_ = max(1, min(star, 2)); }
void Equipment::SetPrice(int price) { price_ = max(0, price); }
void Equipment::SetAtkBonus(int atkBonus) { atkBonus_ = max(0, atkBonus); }
void Equipment::SetHpBonus(int hpBonus) { hpBonus_ = max(0, hpBonus); }
void Equipment::SetDefenseBonus(int defenseBonus) { defenseBonus_ = max(0, defenseBonus); }
void Equipment::SetMagicResistBonus(int magicResistBonus) { magicResistBonus_ = max(0, min(magicResistBonus, 99)); }

void Equipment::SetLifesteal(int lifesteal) { lifesteal_ = max(0, lifesteal); }
void Equipment::SetArmorPenetration(int armorPenetration) { armorPenetration_ = max(0, min(armorPenetration, 100)); }
void Equipment::SetCleaveRange(double range) { cleaveRange_ = max(0.0, range); }
void Equipment::SetCleaveDamage(double damage) { cleaveDamage_ = max(0.0, damage); }
void Equipment::SetReviveHpPercent(int percent) { reviveHpPercent_ = max(0, min(percent, 100)); }
void Equipment::SetAuraDamage(int damage) { auraDamage_ = max(0, damage); }
void Equipment::SetSkillDamageBonus(int bonus) { skillDamageBonus_ = max(0, bonus); }
void Equipment::SetInitialManaBonus(int bonus) { initialManaBonus_ = max(0, bonus); }
void Equipment::SetHpRegenPercent(int percent) { hpRegenPercent_ = max(0, min(percent, 100)); }
void Equipment::SetDamageReflectPercent(int percent) { damageReflectPercent_ = max(0, min(percent, 100)); }
void Equipment::SetDoubleHit(bool doubleHit) { doubleHit_ = doubleHit; }
void Equipment::SetPercentHpDamage(double percent) { percentHpDamage_ = max(0.0, percent); }
void Equipment::SetExtraManaOnHit(int mana) { extraManaOnHit_ = max(0, mana); }
void Equipment::SetSplashAttack(bool splash) { splashAttack_ = splash; }
void Equipment::SetCanFuse(bool canFuse) { canFuse_ = canFuse; }
void Equipment::SetRecipe(const string& recipe1, const string& recipe2) {
    recipe1_ = recipe1;
    recipe2_ = recipe2;
}

void Equipment::UpgradeStar() {
    if (star_ >= 2 || !canFuse_) {
        return;
    }

    ++star_;
    price_ *= 2;
    atkBonus_ *= 2;
    hpBonus_ *= 2;
    defenseBonus_ *= 2;
    magicResistBonus_ = min(99, magicResistBonus_ * 2);
    lifesteal_ *= 2;
    armorPenetration_ = min(100, armorPenetration_ * 2);
    auraDamage_ *= 2;
    skillDamageBonus_ *= 2;
    initialManaBonus_ *= 2;
    hpRegenPercent_ = min(100, hpRegenPercent_ * 2);
    damageReflectPercent_ = min(100, damageReflectPercent_ * 2);
}