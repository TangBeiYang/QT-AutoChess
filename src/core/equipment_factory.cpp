#include "equipment_factory.h"

#include <algorithm>
#include <unordered_map>

namespace {

vector<EquipmentTemplate> BuildAllTemplates() {
    vector<EquipmentTemplate> templates;

    // ===== 基础装备 (2费, 1-2级商店) =====
    templates.push_back({"长剑", 2, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"布甲", 2, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"斗篷", 2, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"红宝石", 2, 0, 250, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"蓝宝石", 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, false, 0, 0, false, true, "", ""});

    // ===== 高级装备 (3费, 3-4级商店) =====
    templates.push_back({"吸血面具", 3, 0, 0, 0, 0, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"狂战斧", 3, 0, 0, 0, 0, 0, 0, 1.0, 0.30, 0, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"M3茧甲", 3, 0, 0, 0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"破甲剑", 3, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});
    templates.push_back({"烈焰披风", 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30, 0, 0, 0, 0, false, 0, 0, false, true, "", ""});

    // ===== 特殊装备 (3费, 4级商店, 不可升星) =====
    templates.push_back({"血怒徽记", 3, 25, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, false, "", ""});
    templates.push_back({"荆棘核心", 3, 0, 0, 35, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, false, 0, 0, false, false, "", ""});
    templates.push_back({"奥术洪流", 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 50, 0, 0, false, 0, 0, false, false, "", ""});
    templates.push_back({"不朽图腾", 3, 0, 500, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, false, 0, 0, false, false, "", ""});
    templates.push_back({"幽影之刃", 3, 25, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0, false, false, "", ""});
    templates.push_back({"风暴连弩", 3, 10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, true, 0, 0, false, false, "", ""});

    // ===== 终极合成装备 (不可升星, 有recipe) =====
    templates.push_back({"魔神杀刃", 0, 70, 0, 0, 0, 35, 30, 0, 0, 0, 0, 0, 0, 0, 0, false, 0.02, 0, false, false, "血怒徽记", "幽影之刃"});
    templates.push_back({"永恒壁垒", 0, 0, 0, 80, 20, 0, 0, 0, 0, 0, 0, 0, 0, 3, 30, false, 0, 0, false, false, "荆棘核心", "不朽图腾"});
    templates.push_back({"虚空脉冲炮", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 30, 0, 0, 0, false, 0, 20, true, false, "奥术洪流", "风暴连弩"});

    return templates;
}

const vector<EquipmentTemplate>& GetTemplates() {
    static const vector<EquipmentTemplate> templates = BuildAllTemplates();
    return templates;
}

const unordered_map<string, const EquipmentTemplate*>& GetNameMap() {
    static const unordered_map<string, const EquipmentTemplate*> map = []() {
        unordered_map<string, const EquipmentTemplate*> m;
        for (const EquipmentTemplate& t : GetTemplates()) {
            m[t.name] = &t;
        }
        return m;
    }();
    return map;
}

} // anonymous namespace

const vector<EquipmentTemplate>& GetAllEquipmentTemplates() {
    return GetTemplates();
}

Equipment CreateEquipmentByName(const string& name) {
    const EquipmentTemplate* tpl = FindEquipmentTemplate(name);
    if (tpl == nullptr) {
        return Equipment();
    }
    return Equipment(tpl->name, tpl->price, tpl->atkBonus, tpl->hpBonus,
                     tpl->defenseBonus, tpl->magicResistBonus, 1,
                     tpl->lifesteal, tpl->armorPenetration, tpl->cleaveRange,
                     tpl->cleaveDamage, tpl->reviveHpPercent, tpl->auraDamage,
                     tpl->skillDamageBonus, tpl->initialManaBonus,
                     tpl->hpRegenPercent, tpl->damageReflectPercent,
                     tpl->doubleHit, tpl->percentHpDamage, tpl->extraManaOnHit,
                     tpl->splashAttack, tpl->canFuse, tpl->recipe1, tpl->recipe2);
}

vector<EquipmentTemplate> GetEquipmentPoolForShopLevel(int shopLevel) {
    vector<EquipmentTemplate> pool;
    for (const EquipmentTemplate& t : GetTemplates()) {
        if (t.price == 0) continue; // skip ultimate合成装备 (price=0 means not sold in shop)
        if (t.price == 2 && shopLevel >= 1) {
            pool.push_back(t);
        } else if (t.price == 3 && shopLevel >= 3) {
            pool.push_back(t);
        }
    }
    return pool;
}

const EquipmentTemplate* FindRecipeResult(const string& name1, const string& name2) {
    for (const EquipmentTemplate& t : GetTemplates()) {
        if (t.recipe1.empty() && t.recipe2.empty()) continue;
        if ((t.recipe1 == name1 && t.recipe2 == name2) ||
            (t.recipe1 == name2 && t.recipe2 == name1)) {
            return &t;
        }
    }
    return nullptr;
}

const EquipmentTemplate* FindEquipmentTemplate(const string& name) {
    const auto& map = GetNameMap();
    auto it = map.find(name);
    return it == map.end() ? nullptr : it->second;
}