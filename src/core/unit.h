#pragma once

#include "equipment.h"

#include <string>
#include <vector>

using namespace std;

class Player;

class Unit {
public:
    enum class DamageType {
        Physical,
        Magical
    };

    struct SkillParams {
        enum class Type {
            None,
            AoE,                    // 范围伤害
            HealSelf,               // 自愈
            AtkBuff,                // 自身ATK buff
            Nuke,                   // 单体高伤（最近敌人）
            MultiHit,               // 多段射击
            DamageReduction,        // 自身百分比减伤
            Barrier,                // 护盾/屏障
            BarrierAura,            // 护盾+受击回血+友军减伤光环
            TeamBuff,               // 全队增益
            MultiTargetNuke,        // 对特定目标高伤（无视防御等）
            DebuffEnemies,          // 敌方全体弱化
            Bounce,                 // 弹射
            HealAlly,               // 治疗友军
            TeleportNuke,           // 瞬移+刺杀
            Cleave,                 // 攻击周围所有阻挡敌人
            NextAttackBuff,         // 强化下次攻击
            PersistAura             // 持续光环（不占攻击动作）
        };

        enum class Effect { None, Shockwave, HealCross, DaggerStrike } effect = Effect::None;
        double effectDuration = 0;

        Type type = Type::None;

        // === Damage ===
        double ratio = 0;               // 伤害倍率
        DamageType damageType = DamageType::Physical;
        double aoeRange = 0;            // AOE 半径

        // === Duration ===
        double duration = 0;            // buff/debuff 持续时间

        // === MultiHit ===
        int hitCount = 0;               // 射击次数

        // === Barrier ===
        int barrierFlat = 0;            // 固定值屏障
        double barrierMaxHpRatio = 0;   // 最大HP比率屏障

        // === Healing ===
        double healRatio = 0;           // 治疗比率
        double selfHealOnTickRatio = 0; // 每秒自动回血比率

        // === Buff values ===
        double atkBuffRatio = 0;
        double atkSpeedBuffRatio = 0;
        int defenseBuffFlat = 0;
        double manaRegenBuffRatio = 0;
        double teamDmgReductionRatio = 0; // 友军减伤光环

        // === Debuff values ===
        double atkDebuffRatio = 0;
        int defenseDebuffFlat = 0;
        double moveSpeedDebuffRatio = 0;

        // === Damage reduction ===
        double dmgReductionPct = 0;      // 百分比减伤（仅物理/全类型）

        // === Next attack buff ===
        double nextAtkRatio = 0;
        bool nextAtkGuaranteedCrit = false;

        // === Targeting ===
        bool targetLowestHp = false;     // 目标最低HP
        bool targetHighestAtk = false;   // 目标最高ATK
        bool preferMageSupport = false;  // 优先法师/辅助
        double ignoreDefenseRatio = 0;   // 无视防御比率

        // === Reflect / proc on hit ===
        double reflectDamage = 0;
        DamageType reflectDamageType = DamageType::Magical;
        double reflectAoeRange = 0;

        // === Lifesteal on attack ===
        double lifestealOnAttackRatio = 0;

        // === Whether skill uses attack action ===
        bool usesAttackAction = true;

        // === Teleport ===
        double untargetableDuration = 0; // 不可选中时间（秒）
    };

    Unit(string name,
         int star,
         int maxHp,
         int atk,
         int defense,
         int magicResist,
         Player* owner = nullptr,
         int price = 0,
         string profession = "战士",
         double attackSpeed = 1.0,
         double attackRange = 0.78,
         double moveSpeed = 1.55,
         int manaPerAttack = 12,
         int maxMana = 100,
         double manaOnHitMultiplier = 1.0,
         int manaRegenPerSecond = 3,
         int critRate = 0,
         double critDamage = 1.5);
    virtual ~Unit() = default;

    const string& GetName() const;
    const string& GetProfession() const;
    int GetStar() const;
    int GetHp() const;
    int GetMaxHp() const;
    int GetBaseMaxHp() const;
    int GetAtk() const;
    int GetBaseAtk() const;
    int GetDefense() const;
    int GetBaseDefense() const;
    int GetMagicResist() const;
    int GetBaseMagicResist() const;
    int GetPrice() const;
    int GetSellPrice() const;
    Player* GetOwner() const;
    const vector<Equipment>& GetEquipments() const;
    int GetMaxEquipmentCount() const;
    int GetCurrentEquipmentCount() const;

    int GetMana() const;
    void SetMana(int mana);
    int GetMaxMana() const;
    void SetMaxMana(int maxMana);
    void AddMana(int amount);
    bool IsManaFull() const;
    void ResetMana();

    void SetName(const string& name);
    void SetProfession(const string& profession);
    void SetStar(int star);
    void SetHp(int hp);
    void SetMaxHp(int maxHp);
    void SetAtk(int atk);
    void SetDefense(int defense);
    void SetMagicResist(int magicResist);
    void SetPrice(int price);
    void SetSellPrice(int sellPrice);
    void SetOwner(Player* owner);
    void SetMaxEquipmentCount(int maxEquipmentCount);

    static double GetStarMultiplier(int star);

    // Virtual methods for profession-specific behavior
    virtual double GetAttackRange() const;
    virtual double GetMoveSpeed() const;
    virtual int GetManaPerAttack() const;
    virtual SkillParams GetSkillParams() const;
    virtual DamageType GetDamageType() const;

    // New getters
    double GetBaseAttackSpeed() const;
    double GetBaseMoveSpeed() const;
    double GetBaseAttackRange() const;
    int GetBaseManaPerAttack() const;
    int GetBaseMaxMana() const;
    int GetManaRegenPerSecond() const;
    double GetManaOnHitMultiplier() const;
    int GetCritRate() const;
    double GetCritDamage() const;

    void SetBaseAttackSpeed(double speed);
    void SetBaseMoveSpeed(double speed);
    void SetBaseAttackRange(double range);
    void SetManaRegenPerSecond(int regen);
    void SetManaOnHitMultiplier(double mult);
    void SetCritRate(int rate);
    void SetCritDamage(double dmg);

    void SetMaxManaBase(int maxMana);

    void TakeDamage(int damage, DamageType damageType = DamageType::Physical);
    void Heal(int amount);
    bool IsAlive() const;
    void UpgradeStar();

    bool AddEquipment(const Equipment& equipment);
    bool RemoveEquipment(int index);
    Equipment TakeEquipment(int index);
    vector<Equipment> TakeAllEquipments();
    void ClearEquipments();

private:
    string name_;
    string profession_;
    int star_;
    int hp_;
    int maxHp_;
    int atk_;
    int defense_;
    int magicResist_;
    int mana_;
    int maxMana_;
    int price_;
    int sellPrice_;
    Player* owner_;
    vector<Equipment> equipments_;
    int maxEquipmentCount_;
    int currentEquipmentCount_;

    // New properties
    double baseAttackSpeed_;
    double baseMoveSpeed_;
    double baseAttackRange_;
    int baseManaPerAttack_;
    int manaRegenPerSecond_;
    double manaOnHitMultiplier_;
    int critRate_;
    double critDamage_;
};
