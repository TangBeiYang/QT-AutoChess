#pragma once

#include <string>
#include <vector>

using namespace std;

class Equipment {
public:
    Equipment(string name = "",
              int price = 0,
              int atkBonus = 0,
              int hpBonus = 0,
              int defenseBonus = 0,
              int magicResistBonus = 0,
              int star = 1,
              int lifesteal = 0,
              int armorPenetration = 0,
              double cleaveRange = 0,
              double cleaveDamage = 0,
              int reviveHpPercent = 0,
              int auraDamage = 0,
              int skillDamageBonus = 0,
              int initialManaBonus = 0,
              int hpRegenPercent = 0,
              int damageReflectPercent = 0,
              bool doubleHit = false,
              double percentHpDamage = 0,
              int extraManaOnHit = 0,
              bool splashAttack = false,
              bool canFuse = true,
              string recipe1 = "",
              string recipe2 = "");

    // Basic stat getters
    const string& GetName() const;
    int GetStar() const;
    int GetPrice() const;
    int GetAtkBonus() const;
    int GetHpBonus() const;
    int GetDefenseBonus() const;
    int GetMagicResistBonus() const;

    // Special effect getters
    int GetLifesteal() const;
    int GetArmorPenetration() const;
    double GetCleaveRange() const;
    double GetCleaveDamage() const;
    int GetReviveHpPercent() const;
    int GetAuraDamage() const;
    int GetSkillDamageBonus() const;
    int GetInitialManaBonus() const;
    int GetHpRegenPercent() const;
    int GetDamageReflectPercent() const;
    bool GetDoubleHit() const;
    double GetPercentHpDamage() const;
    int GetExtraManaOnHit() const;
    bool GetSplashAttack() const;
    bool CanFuse() const;
    const string& GetRecipe1() const;
    const string& GetRecipe2() const;

    // Basic stat setters
    void SetName(const string& name);
    void SetStar(int star);
    void SetPrice(int price);
    void SetAtkBonus(int atkBonus);
    void SetHpBonus(int hpBonus);
    void SetDefenseBonus(int defenseBonus);
    void SetMagicResistBonus(int magicResistBonus);

    // Special effect setters
    void SetLifesteal(int lifesteal);
    void SetArmorPenetration(int armorPenetration);
    void SetCleaveRange(double range);
    void SetCleaveDamage(double damage);
    void SetReviveHpPercent(int percent);
    void SetAuraDamage(int damage);
    void SetSkillDamageBonus(int bonus);
    void SetInitialManaBonus(int bonus);
    void SetHpRegenPercent(int percent);
    void SetDamageReflectPercent(int percent);
    void SetDoubleHit(bool doubleHit);
    void SetPercentHpDamage(double percent);
    void SetExtraManaOnHit(int mana);
    void SetSplashAttack(bool splash);
    void SetCanFuse(bool canFuse);
    void SetRecipe(const string& recipe1, const string& recipe2);

    void UpgradeStar();

private:
    string name_;
    int star_;
    int price_;
    int atkBonus_;
    int hpBonus_;
    int defenseBonus_;
    int magicResistBonus_;

    // Special effects
    int lifesteal_;
    int armorPenetration_;
    double cleaveRange_;
    double cleaveDamage_;
    int reviveHpPercent_;
    int auraDamage_;
    int skillDamageBonus_;
    int initialManaBonus_;
    int hpRegenPercent_;
    int damageReflectPercent_;
    bool doubleHit_;
    double percentHpDamage_;
    int extraManaOnHit_;
    bool splashAttack_;
    bool canFuse_;
    string recipe1_;
    string recipe2_;
};