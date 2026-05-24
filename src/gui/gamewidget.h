#pragma once

#include "bench.h"
#include "board.h"
#include "equipment_inventory.h"
#include "player.h"
#include "shop.h"
#include "unit.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <QPoint>
#include <QPointF>
#include <QJsonObject>
#include <QRect>
#include <QString>
#include <QWidget>
#include <QPixmap>

class QMouseEvent;
class QTimer;

class GameWidget : public QWidget {
public:
    explicit GameWidget(QWidget* parent = nullptr);
    bool SaveGame() const;
    bool LoadGame();

    enum class Difficulty { Easy, Normal, Hard };

    struct DamagePhase {
        int roundStart;
        int roundEnd;
        int baseDamage;
        int star1Damage;
        int star2Damage;
        int star3Damage;
    };

    struct DifficultyConfig {
        int initialHp;
        int initialGold;
        int totalRounds;
        vector<DamagePhase> damagePhases;
    };

    static DifficultyConfig GetDifficultyConfig(Difficulty diff);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    enum class CombatState {
        Idle,
        Moving,
        Attacking,
        Casting,
        Dead
    };

    enum class ScreenState {
        MainMenu,
        LoadSelect,
        NewSelect,
        DifficultySelect,
        Playing
    };

    struct VisualEffect {
        enum class Type {
            Shockwave,
            HealCross,
            DaggerStrike
        };
        Type type;
        QPointF position;
        double timer;
        double duration;
        QPointF targetPosition;
    };

    struct CombatUnit {
        shared_ptr<Unit> unit;
        QPointF position;
        bool isEnemy;
        int boardRow;
        int boardCol;
        double attackRange;
        double moveSpeed;
        double attackSpeed;
        double attackCooldown;
        double attackTimer;
        CombatState state;
        double buffTimer;
        int bonusAtk;
        int synergyBonusAtk;
        double damageTakenMultiplier;
        double baseDamageTakenMultiplier;

        // New systems
        int barrier = 0;

        // Crit
        int critRate = 0;
        double critDamage = 1.5;

        // Multi-hit
        int multiHitCount = 0;
        double multiHitRatio = 0;

        // Lifesteal
        double lifestealRatio = 0;

        // Damage reduction (temporary)
        bool hasDmgReduction = false;
        double dmgReductionTimer = 0;

        // Reflect
        double reflectDamage = 0;
        double reflectAoeRange = 0;

        // Barrier aura
        double barrierSelfHealRatio = 0;
        double teamDmgReductionRatio = 0;
        double barrierAuraTimer = 0;

        // Debuff
        double atkDebuffRatio = 0;
        int defenseDebuff = 0;
        double moveSpeedDebuff = 0;
        double debuffTimer = 0;

        // Defense buff (from teammate skills)
        int buffDefense = 0;

        // Next attack buff
        double nextAttackRatio = 0;
        bool nextAttackGuaranteedCrit = false;

        // Untargetable (teleport)
        double untargetableTimer = 0;

        // Persist aura (doesn't use attack action)
        bool hasPersistAura = false;
        double persistAuraTimer = 0;
        double persistAtkSpeedBuff = 0;
        double persistManaRegenBuff = 0;

        // Equipment combat effects
        int equipLifesteal = 0;
        int equipArmorPenetration = 0;
        double equipCleaveRange = 0;
        double equipCleaveDamage = 0;
        int equipReviveHpPercent = 0;
        int equipAuraDamage = 0;
        int equipSkillDamageBonus = 0;
        int equipHpRegenPercent = 0;
        int equipDamageReflectPercent = 0;
        bool equipDoubleHit = false;
        double equipPercentHpDamage = 0;
        int equipExtraManaOnHit = 0;
        bool equipSplashAttack = false;
        bool hasRevived = false;
    };

    struct SynergyStatus {
        QString id;
        QString label;
        QString description;
        int count;
        int required;
    };

    QRect GetBoardCellRect(int row, int col) const;
    QRect GetBenchCellRect(int col) const;
    QRect GetShopCellRect(int col) const;
    QRect GetRefreshButtonRect() const;
    QRect GetFreezeButtonRect() const;
    QRect GetShopUpgradeButtonRect() const;
    QRect GetStartBattleButtonRect() const;
    QRect GetSaveButtonRect() const;
    QRect GetGameBackButtonRect() const;
    QRect GetDeployPanelRect() const;
    QRect GetDeployUpgradeButtonRect() const;
    QRect GetEquipmentInventoryRect() const;
    QRect GetEquipmentInventoryCellRect(int index) const;
    QRect GetEquipmentOverflowCellRect(int index) const;
    QRect GetMenuTitleRect() const;
    QRect GetMainMenuButtonRect(int index) const;
    QRect GetSaveSlotButtonRect(int slot) const;
    QRect GetMenuBackButtonRect() const;
    QRect GetCombatUnitRect(const QPointF& position) const;
    QRect GetSynergyButtonRect(int index) const;
    QRect GetSynthesisRecipeButtonRect() const;
    bool GetBoardPositionAt(const QPoint& position, int& row, int& col) const;
    int GetBenchSlotAt(const QPoint& position) const;
    int GetShopUnitSlotAt(const QPoint& position) const;
    int GetEquipmentShopSlotAt(const QPoint& position) const;
    int GetEquipmentInventorySlotAt(const QPoint& position) const;
    int GetEquipmentOverflowSlotAt(const QPoint& position) const;
    int GetRightmostEmptyBenchSlot() const;
    int GetPlayerBoardUnitCount() const;
    vector<SynergyStatus> GetVisibleSynergies() const;
    bool IsPlayerBoardUnit(const shared_ptr<Unit>& unit) const;
    bool IsSynergyActiveForProfession(const string& profession) const;
    int GetSynergyAdjustedAtk(const shared_ptr<Unit>& unit) const;
    QString GetUnitSynergyText(const shared_ptr<Unit>& unit) const;
    QString GetEquipmentSummary(const shared_ptr<Unit>& unit) const;
    QString GetEquipmentDescription(const Equipment& equipment) const;
    QString GetSkillDescription(const shared_ptr<Unit>& unit) const;
    int GetEquipmentSellPrice(const Equipment& equipment) const;
    void DrawUnitIcon(QPainter& painter, const QRect& cell, const shared_ptr<Unit>& unit, bool isSelected, int cellSize, bool isEnemy = false);
    void DrawEquipmentIcon(QPainter& painter, const QRect& cell, const Equipment& equipment, bool isSelected, int cellSize) const;
    void DrawHealthBar(QPainter& painter, const QRect& cell, const shared_ptr<Unit>& unit, bool isEnemy);
    void DrawSynergies(QPainter& painter);
    void DrawVisualEffects(QPainter& painter, int cellSize);
    void DrawBuffIndicator(QPainter& painter, const QRect& cell, const CombatUnit& combatUnit);
    void DrawMainMenu(QPainter& painter);
    void DrawSaveSelectMenu(QPainter& painter);
    void DrawDifficultySelectMenu(QPainter& painter);
    void DrawMenuButton(QPainter& painter, const QRect& buttonRect, const QString& text, int pointSize);
    void DrawMenuStar(QPainter& painter, const QPoint& center, int size);
    void StartNewGame(int saveSlot, bool canSave);
    void MarkGameChanged();
    QString GetSaveFilePath(int saveSlot) const;
    QString GetSaveMetaFilePath() const;
    QString GetDefaultSaveSlotName(int saveSlot) const;
    QString GetSaveSlotName(int saveSlot) const;
    QString GetSaveSlotButtonText(int saveSlot) const;
    QString GetCurrentSaveSlotName() const;
    void LoadSaveSlotNames();
    bool SaveSaveSlotNames() const;
    bool ResetSaveSlot(int saveSlot);
    void ShowSaveSlotContextMenu(int saveSlot);
    void RenameSaveSlot(int saveSlot);
    bool SaveGameToSlot(int saveSlot) const;
    bool LoadGameFromSlot(int saveSlot);
    void SpawnTestEnemies();
    void ClearEnemyUnits();
    void PlaceEnemyWave(const vector<pair<string, int>>& enemies);
    using WaveVariant = vector<pair<string, int>>;
    using WaveData = vector<WaveVariant>;
    static vector<WaveData> GetEnemyWaveData(Difficulty diff);
    void RestoreBoardUnitsAfterBattle(bool restoreEnemies);
    void RefreshShopAfterBattle();
    QJsonObject UnitToJson(const shared_ptr<Unit>& unit) const;
    shared_ptr<Unit> UnitFromJson(const QJsonObject& object) const;
    QJsonObject EquipmentToJson(const Equipment& equipment) const;
    Equipment EquipmentFromJson(const QJsonObject& object) const;
    void RefreshEquipmentShop(bool onlyEmptySlots = false);
    void ReturnUnitEquipmentsToInventory(const shared_ptr<Unit>& unit);
    bool TryFuseEquipment();
    bool TrySynthesizeEquipment();
    void StartBattle();
    void UpdateBattle();
    void FinishBattle(bool playerWon);
    int FindNearestEnemyIndex(int sourceIndex) const;
    int FindLowestMaxHpEnemyIndex(int sourceIndex) const;
    int GetCombatAttack(const CombatUnit& combatUnit) const;
        void DealDamage(CombatUnit& target, int damage, Unit::DamageType damageType);
    void ApplySynergyBonuses();
    void CastSkill(int sourceIndex);
    void SpawnVisualEffect(VisualEffect::Type type, const QPointF& position, double duration, const QPointF& targetPosition = QPointF());
    bool TryFusion(shared_ptr<Unit>& fusedUnit);
    shared_ptr<Unit> CreateFusionRewardUnit() const;
    void ResolveCombatSeparation();
    void BeginUnitInteraction(const shared_ptr<Unit>& unit, const QPoint& position, bool sourceIsBench, int benchSlot, int boardRow, int boardCol);
    void ResetUnitInteraction();
    void HandleDraggedUnitRelease();
    void BeginEquipmentInteraction(const shared_ptr<Equipment>& equipment, const QPoint& position, int shopSlot);
    void ResetEquipmentInteraction();
    bool HandleDraggedEquipmentRelease();
    void MoveOrSwapBenchUnitToBoard();
    void MoveOrSwapBoardUnitToBoard();
    void MoveOrSwapBenchUnitToBench();
    void MoveOrSwapBoardUnitToBench();
    void SwapBoardAndBenchUnits(int boardRow, int boardCol, int benchSlot);
    void HandleRefreshButton();
    void HandleFreezeButton();
    void HandleUpgradeButton();
    void HandleDeployUpgradeButton();
    void HandleSaveButton();
    void HandleGameBackButton();
    void SelectAndShowUnitDetail(const shared_ptr<Unit>& unit, int& selectedSlot, int slot, bool canBuy, int shopSlot = -1);
    void ShowSynergyDetailDialog(const SynergyStatus& synergy);
    void ShowSynthesisRecipeDialog();
    void ShowEquipmentDetailDialog(int shopSlot);
    void ShowEquipmentDetailDialog(const shared_ptr<Equipment>& equipment, bool canBuy, int shopSlot = -1, int inventorySlot = -1, int overflowIndex = -1);
    void ShowUnitDetailDialog(int shopSlot);
    void ShowUnitDetailDialog(const shared_ptr<Unit>& unit, bool canBuy, int shopSlot = -1, int benchSlot = -1);
    void ShowUnitSkillDialog(const shared_ptr<Unit>& unit);

    static const unordered_map<string, string>& GetUnitSkillNameMap();
    static const unordered_map<string, string>& GetUnitSkillDescriptionMap();

    Board board_;
    Bench bench_;
    Shop shop_;
    Player testPlayer_;
    QTimer* battleTimer_;
    vector<CombatUnit> combatUnits_;
    vector<VisualEffect> visualEffects_;
    ScreenState screenState_;
    int currentSaveSlot_;
    bool canSaveCurrentGame_;
    bool hasUnsavedChanges_;
    vector<QString> saveSlotNames_;
    int selectedShopSlot_;
    int selectedBenchSlot_;
    int selectedBoardRow_;
    int selectedBoardCol_;
    int selectedEquipmentShopSlot_;
    int selectedEquipmentInventorySlot_;
    int selectedEquipmentOverflowIndex_;
    int currentEnemyRound_;
    int deployUpgradeCost_;
    int deployUpgradeDiscount_;
    int winStreak_;
    Difficulty difficulty_;
    bool hasPendingUnitClick_;
    bool hasPendingEquipmentClick_;
    bool isDraggingUnit_;
    bool isDraggingEquipment_;
    bool isBattleActive_;
    double battleCountdownTimer_;
    bool battleTimeExpired_;
    bool isGameWon_;
    bool isGameOver_;
    bool isShopFrozen_;
    bool dragSourceIsBench_;
    int pressedBenchSlot_;
    int pressedBoardRow_;
    int pressedBoardCol_;
    int pressedEquipmentShopSlot_;
    int pressedEquipmentInventorySlot_;
    int pressedEquipmentOverflowIndex_;
    int dragHoverRow_;
    int dragHoverCol_;
    int dragHoverBenchSlot_;
    QPoint dragStartPos_;
    QPoint dragPosition_;
    shared_ptr<Unit> draggingUnit_;
    vector<shared_ptr<Equipment>> equipmentShopSlots_;
    EquipmentInventory equipmentInventory_;
    shared_ptr<Equipment> draggingEquipment_;
    unordered_map<string, QPixmap> professionIconCache_;
};
