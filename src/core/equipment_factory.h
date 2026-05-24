#pragma once

#include "equipment.h"

#include <memory>
#include <string>
#include <vector>

using namespace std;

struct EquipmentTemplate {
    string name;
    int price;
    int atkBonus;
    int hpBonus;
    int defenseBonus;
    int magicResistBonus;
    int lifesteal;
    int armorPenetration;
    double cleaveRange;
    double cleaveDamage;
    int reviveHpPercent;
    int auraDamage;
    int skillDamageBonus;
    int initialManaBonus;
    int hpRegenPercent;
    int damageReflectPercent;
    bool doubleHit;
    double percentHpDamage;
    int extraManaOnHit;
    bool splashAttack;
    bool canFuse;
    string recipe1;
    string recipe2;
};

// Returns all equipment templates
const vector<EquipmentTemplate>& GetAllEquipmentTemplates();

// Create an Equipment object by name (returns empty Equipment if not found)
Equipment CreateEquipmentByName(const string& name);

// Get equipment templates available at a given shop level
vector<EquipmentTemplate> GetEquipmentPoolForShopLevel(int shopLevel);

// Check if two equipment names form a valid recipe
const EquipmentTemplate* FindRecipeResult(const string& name1, const string& name2);

// Find a template by name
const EquipmentTemplate* FindEquipmentTemplate(const string& name);