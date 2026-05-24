#include "unit_factory.h"

#include <algorithm>
#include <unordered_map>

namespace {

// Helper: create per-unit SkillParams from a function
using SkillParamsMaker = Unit::SkillParams(*)(const string& name);

// Store profession-level default values
struct ProfData {
    double attackRange;
    double moveSpeed;
    int manaPerAttack;
    Unit::DamageType damageType = Unit::DamageType::Physical;
};

const ProfData& GetProfData(const string& prof) {
    static const unordered_map<string, ProfData> map = {
        {"战士", {0.8, 1.5, 10, Unit::DamageType::Physical}},
        {"坦克", {0.8, 1.35, 8, Unit::DamageType::Physical}},
        {"弓箭手", {3.0, 1.55, 12, Unit::DamageType::Physical}},
        {"法师", {3.0, 1.5, 10, Unit::DamageType::Magical}},
        {"刺客", {0.72, 1.9, 14, Unit::DamageType::Physical}},
        {"辅助", {3.0, 1.4, 10, Unit::DamageType::Physical}},
    };
    auto it = map.find(prof);
    return it == map.end() ? map.at("战士") : it->second;
}

// Generic unit subclass: stores overridable values per instance
class GenericUnit : public Unit {
public:
    GenericUnit(const string& name, int star, int maxHp, int atk, int defense, int magicResist,
                int price, const string& profession, double attackSpeed,
                int maxMana, double manaOnHitMult, int manaRegen,
                int critRate, double critDamage,
                const Unit::SkillParams& skillParams,
                Unit::DamageType damageType = Unit::DamageType::Physical)
        : Unit(name, star, maxHp, atk, defense, magicResist, nullptr, price, profession,
               attackSpeed, GetProfData(profession).attackRange, GetProfData(profession).moveSpeed,
               GetProfData(profession).manaPerAttack, maxMana,
               manaOnHitMult, manaRegen, critRate, critDamage),
          skillParams_(skillParams), damageType_(damageType) {}

    Unit::SkillParams GetSkillParams() const override { return skillParams_; }
    Unit::DamageType GetDamageType() const override { return damageType_; }

private:
    Unit::SkillParams skillParams_;
    Unit::DamageType damageType_;
};

// Helper factory function
shared_ptr<Unit> MakeUnit(const string& name, int star, int maxHp, int atk, int defense, int magicResist,
                          int price, const string& profession, double attackSpeed,
                          int maxMana, double manaOnHitMult = 1.0, int manaRegen = 3,
                          int critRate = 0, double critDamage = 1.5,
                          const Unit::SkillParams& skillParams = {},
                          Unit::DamageType damageType = Unit::DamageType::Physical) {
    return make_shared<GenericUnit>(name, star, maxHp, atk, defense, magicResist, price, profession,
                                     attackSpeed, maxMana, manaOnHitMult, manaRegen,
                                     critRate, critDamage, skillParams, damageType);
}

using SkillParams = Unit::SkillParams;

SkillParams MakeWarriorSkill(const string& name) {
    // 赫德雷 - 重剑横扫
    SkillParams sp;
    sp.type = SkillParams::Type::Cleave;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.ratio = 1.6;
    sp.damageType = Unit::DamageType::Physical;
    return sp;
}

SkillParams MakeMountainSkill(const string& name) {
    // 山 - 横扫架势
    SkillParams sp;
    sp.type = SkillParams::Type::AtkBuff;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.4;
    sp.duration = 5.0;
    sp.atkSpeedBuffRatio = 0.6;
    sp.lifestealOnAttackRatio = 0.03;
    return sp;
}

SkillParams MakeMłynarSkill(const string& name) {
    // 玛恩纳 - 未照耀的荣光
    SkillParams sp;
    sp.type = SkillParams::Type::TeamBuff;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.duration = 6.0;
    sp.defenseBuffFlat = 15;
    sp.atkBuffRatio = 0.15;
    return sp;
}

SkillParams MakeNianSkill(const string& name) {
    // 年 - 战术装甲
    SkillParams sp;
    sp.type = SkillParams::Type::DamageReduction;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.4;
    sp.duration = 4.0;
    sp.dmgReductionPct = 0.4;
    return sp;
}

SkillParams MakeHoshigumaSkill(const string& name) {
    // 斩业星熊 - 荆棘反震
    SkillParams sp;
    sp.type = SkillParams::Type::Barrier;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.duration = 5.0;
    sp.barrierFlat = 300;
    sp.reflectDamage = 80;
    sp.reflectAoeRange = 0.8;
    return sp;
}

SkillParams MakeMudrockSkill(const string& name) {
    // 泥岩 - 不屈领域
    SkillParams sp;
    sp.type = SkillParams::Type::BarrierAura;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.6;
    sp.duration = 6.0;
    sp.barrierMaxHpRatio = 0.3;
    sp.selfHealOnTickRatio = 0.02;
    sp.teamDmgReductionRatio = 0.15;
    return sp;
}

SkillParams MakeExusiaiSkill(const string& name) {
    // 能天使 - 过载模式
    SkillParams sp;
    sp.type = SkillParams::Type::MultiHit;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.4;
    sp.hitCount = 4;
    sp.ratio = 0.6;
    return sp;
}

SkillParams MakeWisdelSkill(const string& name) {
    // 维什戴尔 - 爆裂黎明
    SkillParams sp;
    sp.type = SkillParams::Type::AoE;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.ratio = 1.4;
    sp.aoeRange = 1.0;
    return sp;
}

SkillParams MakeRaySkill(const string& name) {
    // 莱伊 - 战术狙杀
    SkillParams sp;
    sp.type = SkillParams::Type::MultiTargetNuke;
    sp.effect = SkillParams::Effect::DaggerStrike;
    sp.effectDuration = 0.35;
    sp.ratio = 3.5;
    sp.ignoreDefenseRatio = 0.5;
    sp.targetHighestAtk = true;
    sp.preferMageSupport = true;
    return sp;
}

SkillParams MakeEyjafjallaSkill(const string& name) {
    // 艾雅法拉 - 火山喷发
    SkillParams sp;
    sp.type = SkillParams::Type::AoE;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.ratio = 1.5;
    sp.aoeRange = 0.8;
    sp.damageType = Unit::DamageType::Magical;
    sp.moveSpeedDebuffRatio = 0.3;
    sp.duration = 2.0;
    return sp;
}

SkillParams MakeLogosSkill(const string& name) {
    // 逻各斯 - 法术湮灭
    SkillParams sp;
    sp.type = SkillParams::Type::AoE;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.ratio = 2.3;
    sp.aoeRange = 1.0;
    sp.damageType = Unit::DamageType::Magical;
    return sp;
}

SkillParams MakePassengerSkill(const string& name) {
    // 异客 - 聚焦指令（弹射）
    SkillParams sp;
    sp.type = SkillParams::Type::Bounce;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.ratio = 1.4;
    sp.hitCount = 6;
    sp.damageType = Unit::DamageType::Magical;
    return sp;
}

SkillParams MakePhantomSkill(const string& name) {
    // 傀影 - 夜幕突袭
    SkillParams sp;
    sp.type = SkillParams::Type::NextAttackBuff;
    sp.effect = SkillParams::Effect::DaggerStrike;
    sp.effectDuration = 0.35;
    sp.nextAtkRatio = 2.8;
    sp.nextAtkGuaranteedCrit = true;
    return sp;
}

SkillParams MakeTexasSkill(const string& name) {
    // 缄默德克萨斯 - 剑影瞬闪
    SkillParams sp;
    sp.type = SkillParams::Type::TeleportNuke;
    sp.effect = SkillParams::Effect::DaggerStrike;
    sp.effectDuration = 0.35;
    sp.ratio = 2.2;
    sp.targetLowestHp = true;
    sp.untargetableDuration = 2.0;
    return sp;
}

SkillParams MakeWarfarinSkill(const string& name) {
    // 华法琳 - 紧急包扎
    SkillParams sp;
    sp.type = SkillParams::Type::HealAlly;
    sp.effect = SkillParams::Effect::HealCross;
    sp.effectDuration = 0.5;
    sp.healRatio = 0.18;
    return sp;
}

SkillParams MakeSkadiSkill(const string& name) {
    // 浊心斯卡蒂 - 海嗣赞歌
    SkillParams sp;
    sp.type = SkillParams::Type::PersistAura;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.4;
    sp.duration = 5.0;
    sp.atkSpeedBuffRatio = 0.3;
    sp.manaRegenBuffRatio = 0.2;
    return sp;
}

SkillParams MakeSuzuranSkill(const string& name) {
    // 铃兰 - 狐火领域
    SkillParams sp;
    sp.type = SkillParams::Type::DebuffEnemies;
    sp.effect = SkillParams::Effect::Shockwave;
    sp.effectDuration = 0.5;
    sp.duration = 5.0;
    sp.atkDebuffRatio = 0.25;
    sp.defenseDebuffFlat = 20;
    sp.moveSpeedDebuffRatio = 0.4;
    return sp;
}

// Map unit name to its SkillParams maker
const unordered_map<string, SkillParams(*)(const string&)>& GetSkillMakers() {
    static const unordered_map<string, SkillParams(*)(const string&)> makers = {
        {"赫德雷", MakeWarriorSkill},
        {"山", MakeMountainSkill},
        {"玛恩纳", MakeMłynarSkill},
        {"年", MakeNianSkill},
        {"斩业星熊", MakeHoshigumaSkill},
        {"泥岩", MakeMudrockSkill},
        {"能天使", MakeExusiaiSkill},
        {"维什戴尔", MakeWisdelSkill},
        {"莱伊", MakeRaySkill},
        {"艾雅法拉", MakeEyjafjallaSkill},
        {"逻各斯", MakeLogosSkill},
        {"异客", MakePassengerSkill},
        {"傀影", MakePhantomSkill},
        {"缄默德克萨斯", MakeTexasSkill},
        {"华法琳", MakeWarfarinSkill},
        {"浊心斯卡蒂", MakeSkadiSkill},
        {"铃兰", MakeSuzuranSkill},
    };
    return makers;
}

// Get crit rate for assassin units
int GetBaseCritRate(const string& prof) {
    return prof == "刺客" ? 0 : 0;  // Base is 0, assassin synergy adds crit
}

} // namespace

const vector<UnitTemplate>& GetUnitTemplates() {
    static const vector<UnitTemplate> templates = {
        // 战士 (主职业)
        {"赫德雷", "战士", 1550, 160, 35, 15, 2},
        {"山", "战士", 1450, 205, 25, 10, 3},
        {"玛恩纳", "战士", 1950, 180, 45, 20, 4},

        // 坦克 (主职业)
        {"年", "坦克", 2200, 80, 65, 10, 2},
        {"斩业星熊", "坦克", 1850, 130, 30, 25, 3},
        {"泥岩", "坦克", 3500, 150, 70, 20, 5},

        // 弓箭手 (主职业)
        {"能天使", "弓箭手", 950, 175, 15, 10, 2},
        {"维什戴尔", "弓箭手", 1000, 195, 12, 10, 3},
        {"莱伊", "弓箭手", 950, 220, 15, 10, 4},

        // 法师 (主职业)
        {"艾雅法拉", "法师", 900, 200, 12, 15, 2},
        {"逻各斯", "法师", 920, 215, 10, 15, 3},
        {"异客", "法师", 950, 205, 10, 15, 5},

        // 刺客 (插件职业)
        {"傀影", "刺客", 850, 250, 20, 10, 3},
        {"缄默德克萨斯", "刺客", 880, 240, 18, 10, 4},

        // 辅助 (插件职业)
        {"华法琳", "辅助", 1000, 80, 20, 25, 2},
        {"浊心斯卡蒂", "辅助", 1050, 90, 22, 28, 3},
        {"铃兰", "辅助", 1080, 100, 25, 30, 4},
    };
    return templates;
}

const UnitTemplate* FindUnitTemplate(const string& name) {
    const vector<UnitTemplate>& templates = GetUnitTemplates();
    auto it = find_if(templates.begin(), templates.end(), [&](const UnitTemplate& t) {
        return t.name == name;
    });
    return it == templates.end() ? nullptr : &(*it);
}

shared_ptr<Unit> CreateUnitByName(const string& name, int star) {
    const UnitTemplate* t = FindUnitTemplate(name);
    if (t == nullptr) return nullptr;

    const int normalizedStar = max(1, star);
    const ProfData& prof = GetProfData(t->profession);

    // Build skill params
    const auto& makers = GetSkillMakers();
    SkillParams sp;
    auto it = makers.find(t->name);
    if (it != makers.end()) {
        sp = it->second(t->name);
    }

    // Per-unit overrides
    double attackSpeed = 1.0;
    int maxMana = 100;
    double manaOnHitMult = 1.0;
    int manaRegen = 3;
    int critRate = 0;
    double critDamage = 1.5;
    Unit::DamageType damageType = prof.damageType;

    // Unit-specific overrides
    if (t->name == "年") { maxMana = 90; manaOnHitMult = 1.5; }
    else if (t->name == "斩业星熊") { maxMana = 80; manaOnHitMult = 1.7; damageType = Unit::DamageType::Magical; }
    else if (t->name == "泥岩") { maxMana = 120; }
    else if (t->name == "山") { attackSpeed = 1.15; maxMana = 90; }
    else if (t->name == "能天使") { attackSpeed = 1.2; maxMana = 80; }
    else if (t->name == "维什戴尔") { attackSpeed = 1.1; maxMana = 100; }
    else if (t->name == "莱伊") { maxMana = 120; }
    else if (t->name == "艾雅法拉") { attackSpeed = 1.1; maxMana = 80; }
    else if (t->name == "逻各斯") { maxMana = 100; }
    else if (t->name == "异客") { maxMana = 120; }
    else if (t->name == "傀影") { attackSpeed = 1.35; maxMana = 70; }
    else if (t->name == "缄默德克萨斯") { attackSpeed = 1.4; maxMana = 90; }
    else if (t->name == "赫德雷") { maxMana = 100; }
    else if (t->name == "玛恩纳") { maxMana = 110; }
    else if (t->name == "华法琳") { maxMana = 80; }
    else if (t->name == "浊心斯卡蒂") { maxMana = 100; }
    else if (t->name == "铃兰") { maxMana = 100; }

    // Assassin-specific: base crit rate from synergy only
    if (t->profession == "刺客") { critRate = 0; critDamage = 1.5; }

    shared_ptr<Unit> unit = MakeUnit(
        t->name, 1, t->maxHp, t->atk, t->defense, t->magicResist, t->price,
        t->profession, attackSpeed, maxMana, manaOnHitMult, manaRegen,
        critRate, critDamage, sp, damageType
    );

    for (int curStar = 1; curStar < normalizedStar; ++curStar) {
        unit->UpgradeStar();
    }
    return unit;
}