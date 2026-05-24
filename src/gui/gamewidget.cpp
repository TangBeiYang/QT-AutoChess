#include "gamewidget.h"

#include "unit.h"
#include "unit_factory.h"
#include "equipment_factory.h"
#include <QCoreApplication>

#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QCursor>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QInputDialog>
#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QAction>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace {
constexpr double kBattleTickSeconds = 1.0 / 60.0;
constexpr int kBattleTickMs = 16;
constexpr double kBattleTimeLimit = 80.0;
constexpr double kUnitRadius = 0.28;
constexpr double kMinUnitDistance = kUnitRadius * 2.0;
constexpr double kSeparationStrength = 0.45;
constexpr int kMaxPopulationLimit = 6;
constexpr int kSaveSlotCount = 4;
constexpr int kShopUnitStartCol = 1;
constexpr int kVisibleShopUnitCount = 5;
constexpr int kWeaponShopStartCol = 6;
constexpr int kWeaponShopSlotCount = 1;
constexpr int kShopLevelCol = 7;

struct BoardLayout {
    int cellSize;
    int boardSize;
    int benchHeight;
    int shopHeight;
    int gap;
    int boardX;
    int boardY;
    int benchY;
    int shopX;
    int shopY;
};

BoardLayout GetBoardLayout(const QWidget& widget) {
    constexpr int kMargin = 32;
    const int availableWidth = widget.width() - kMargin * 2;
    const int availableHeight = widget.height() - kMargin * 2;
    const int cellSize = qMax(24, qMin(availableWidth / 10, availableHeight / 12));
    const int boardSize = cellSize * Board::kCols;
    const int benchHeight = cellSize;
    const int shopHeight = cellSize;
    const int gap = cellSize / 2;
    const int totalHeight = boardSize + gap + benchHeight + gap + shopHeight;
    const int boardX = (widget.width() - boardSize) / 2;
    const int boardY = (widget.height() - totalHeight) / 2;
    const int benchY = boardY + boardSize + gap;
    const int shopX = (widget.width() - cellSize * 8) / 2;
    const int shopY = benchY + benchHeight + gap;
    return {cellSize, boardSize, benchHeight, shopHeight, gap, boardX, boardY, benchY, shopX, shopY};
}

void AddBaseUnitsToPool(Shop& shop) {
    // 等级1：2费单位
    shop.AddUnitToPool({"赫德雷", 1});
    shop.AddUnitToPool({"年", 1});
    shop.AddUnitToPool({"能天使", 1});
    shop.AddUnitToPool({"艾雅法拉", 1});
    shop.AddUnitToPool({"华法琳", 1});
}

void AddLevel2UnitsToPool(Shop& shop) {
    // 3费单位（商店等级2可用）
    shop.AddUnitToPool({"山", 1});
    shop.AddUnitToPool({"斩业星熊", 1});
    shop.AddUnitToPool({"维什戴尔", 1});
    shop.AddUnitToPool({"逻各斯", 1});
    shop.AddUnitToPool({"傀影", 1});
    shop.AddUnitToPool({"浊心斯卡蒂", 1});
}

void AddLevel3UnitsToPool(Shop& shop) {
    // 4费单位（商店等级3可用）
    shop.AddUnitToPool({"玛恩纳", 1});
    shop.AddUnitToPool({"莱伊", 1});
    shop.AddUnitToPool({"缄默德克萨斯", 1});
    shop.AddUnitToPool({"铃兰", 1});
}

void AddLevel4UnitsToPool(Shop& shop) {
    // 5费单位（商店等级4可用）
    shop.AddUnitToPool({"泥岩", 1});
    shop.AddUnitToPool({"异客", 1});
}

void AddAdvancedUnitsToPool(Shop& shop) {
    // 根据商店等级添加相应费用的单位
    if (shop.GetLevel() >= 2) {
        AddLevel2UnitsToPool(shop);
    }
    if (shop.GetLevel() >= 3) {
        AddLevel3UnitsToPool(shop);
    }
    if (shop.GetLevel() >= 4) {
        AddLevel4UnitsToPool(shop);
    }
}

shared_ptr<Equipment> CreateRandomEquipment(int shopLevel) {
    const vector<EquipmentTemplate>& pool = GetEquipmentPoolForShopLevel(shopLevel);
    if (pool.empty()) {
        return nullptr;
    }

    const int index = QRandomGenerator::global()->bounded(static_cast<int>(pool.size()));
    return make_shared<Equipment>(CreateEquipmentByName(pool[index].name));
}

struct SynergyDefinition {
    const char* id;         // unique key
    const char* label;
    const char* description;
    const vector<const char*> professions;  // professions that count
    int required2;          // threshold 1
    int required4;          // threshold 2 (0 = no second threshold)
};

const vector<SynergyDefinition>& GetSynergyDefinitions() {
    static const vector<SynergyDefinition> definitions = {
        {"steel", "钢铁战线", "2：全队防御 +15 | 4：战士/坦克受伤-20%，每秒回2%HP",
         {"战士", "坦克"}, 2, 4},
        {"elemental", "元素火力", "2：全队ATK +15% | 4：技能伤害+30%，攻速+25%",
         {"弓箭手", "法师"}, 2, 4},
        {"assassin", "刺客", "2刺客：优先锁定最低生命值上限敌人，杀死敌人后刷新技能（回满法力值）",
         {"刺客"}, 2, 0},
        {"support", "辅助", "2辅助：全队回蓝+25%，受治疗效果+25%",
         {"辅助"}, 2, 0},
    };
    return definitions;
}

double Distance(const QPointF& first, const QPointF& second) {
    const double dx = first.x() - second.x();
    const double dy = first.y() - second.y();
    return std::sqrt(dx * dx + dy * dy);
}

QPointF Normalized(const QPointF& vector) {
    const double length = std::sqrt(vector.x() * vector.x() + vector.y() * vector.y());
    if (length <= 0.0001) {
        return QPointF(0.0, 0.0);
    }
    return QPointF(vector.x() / length, vector.y() / length);
}

bool IsProfession(const shared_ptr<Unit>& unit, const string& profession) {
    return unit != nullptr && unit->GetProfession() == profession;
}


QPolygonF BuildStarPolygon(const QRect& rect) {
    QPolygonF star;
    const QPointF center = rect.center();
    const double outerRadius = rect.width() * 0.48;
    const double innerRadius = outerRadius * 0.45;
    constexpr double kPi = 3.14159265358979323846;
    for (int i = 0; i < 10; ++i) {
        const double radius = (i % 2 == 0) ? outerRadius : innerRadius;
        const double angle = -kPi / 2.0 + i * kPi / 5.0;
        star << QPointF(center.x() + std::cos(angle) * radius, center.y() + std::sin(angle) * radius);
    }
    return star;
}
}

GameWidget::GameWidget(QWidget* parent)
    : QWidget(parent),
      shop_(1, 2, 3),
      testPlayer_(20, 10, 1, 3),
      battleTimer_(new QTimer(this)),
      screenState_(ScreenState::MainMenu),
    difficulty_(GameWidget::Difficulty::Normal),
      currentSaveSlot_(-1),
      canSaveCurrentGame_(false),
      hasUnsavedChanges_(false),
      saveSlotNames_(kSaveSlotCount),
      selectedShopSlot_(-1),
      selectedBenchSlot_(-1),
      selectedBoardRow_(-1),
      selectedBoardCol_(-1),
      selectedEquipmentShopSlot_(-1),
      selectedEquipmentInventorySlot_(-1),
      selectedEquipmentOverflowIndex_(-1),
      currentEnemyRound_(0),
      deployUpgradeCost_(4),
    deployUpgradeDiscount_(0),
      winStreak_(0),
      hasPendingUnitClick_(false),
      hasPendingEquipmentClick_(false),
      isDraggingUnit_(false),
      isDraggingEquipment_(false),
      isBattleActive_(false),
      isGameWon_(false),
      isGameOver_(false),
      isShopFrozen_(false),
      dragSourceIsBench_(false),
      pressedBenchSlot_(-1),
      pressedBoardRow_(-1),
      pressedBoardCol_(-1),
      pressedEquipmentShopSlot_(-1),
      pressedEquipmentInventorySlot_(-1),
      pressedEquipmentOverflowIndex_(-1),
      dragHoverRow_(-1),
      dragHoverCol_(-1),
      dragHoverBenchSlot_(-1),
      equipmentShopSlots_(kWeaponShopSlotCount) {
    setMinimumSize(900, 860);
    setMouseTracking(true);
    LoadSaveSlotNames();
    connect(battleTimer_, &QTimer::timeout, this, [this]() {
        UpdateBattle();
    });
}

void GameWidget::StartNewGame(int saveSlot, bool canSave) {
    if (isBattleActive_) {
        battleTimer_->stop();
        combatUnits_.clear();
        visualEffects_.clear();
        isBattleActive_ = false;
    }

    const DifficultyConfig cfg = GetDifficultyConfig(difficulty_);

    board_.Clear();
    bench_.Clear();
    testPlayer_.ClearUnits();
    testPlayer_.SetHp(cfg.initialHp);
    testPlayer_.SetGold(cfg.initialGold);
    testPlayer_.SetLevel(1);
    testPlayer_.SetPopulationLimit(3);

    shop_.ClearUnitPool();
    shop_.SetMaxLevel(4);
    shop_.SetLevel(1);
    shop_.SetSellableUnitCount(5);
    shop_.SetRefreshCost(1);
    shop_.SetUpgradeCost(4);
    shop_.ResetDiscount();
    AddBaseUnitsToPool(shop_);
    shop_.ClearCurrentUnits();
    shop_.RefreshShop();
    RefreshEquipmentShop();
    equipmentInventory_.Clear();

    currentEnemyRound_ = 0;
    deployUpgradeCost_ = 4;
    deployUpgradeDiscount_ = 0;
    winStreak_ = 0;
    selectedShopSlot_ = -1;
    selectedBenchSlot_ = -1;
    selectedBoardRow_ = -1;
    selectedBoardCol_ = -1;
    selectedEquipmentShopSlot_ = -1;
    selectedEquipmentInventorySlot_ = -1;
    selectedEquipmentOverflowIndex_ = -1;
    isGameWon_ = false;
    isGameOver_ = false;
    isShopFrozen_ = false;
    ResetUnitInteraction();
    ResetEquipmentInteraction();
    currentSaveSlot_ = saveSlot;
    canSaveCurrentGame_ = canSave;
    hasUnsavedChanges_ = false;
    screenState_ = ScreenState::Playing;
    ResetUnitInteraction();
    SpawnTestEnemies();
    update();
}

void GameWidget::MarkGameChanged() {
    if (canSaveCurrentGame_) {
        hasUnsavedChanges_ = true;
    }
}

GameWidget::DifficultyConfig GameWidget::GetDifficultyConfig(Difficulty diff) {
    switch (diff) {
    case Difficulty::Easy:
        return {40, 20, 8, {
            {1, 2, 4, 2, 3, 5},
            {3, 5, 6, 2, 3, 5},
            {6, 8, 10, 2, 3, 5}
        }};
    case Difficulty::Normal:
        return {60, 15, 10, {
            {1, 3, 3, 1, 2, 4},
            {4, 7, 5, 1, 2, 4},
            {8, 10, 8, 1, 2, 4}
        }};
    case Difficulty::Hard:
        return {80, 12, 13, {
            {1, 4, 3, 1, 2, 3},
            {5, 9, 5, 1, 2, 3},
            {10, 13, 7, 1, 2, 3}
        }};
    }
    return {40, 20, 8, {}};
}

vector<GameWidget::WaveData> GameWidget::GetEnemyWaveData(Difficulty diff) {
    using Entry = pair<string, int>;
    if (diff == Difficulty::Easy) {
        return {
            // W1: 3 variants
            {{{"赫德雷",1},{"能天使",1}},
             {{"年",1},{"艾雅法拉",1}},
             {{"华法琳",1},{"山",1}}},
            // W2
            {{{"赫德雷",2},{"维什戴尔",1}},
             {{"斩业星熊",1},{"能天使",1}},
             {{"傀影",1},{"浊心斯卡蒂",1}}},
            // W3
            {{{"玛恩纳",1},{"莱伊",1}},
             {{"山",2},{"逻各斯",1}},
             {{"缄默德克萨斯",1},{"铃兰",1}}},
            // W4
            {{{"泥岩",1},{"异客",1}},
             {{"年",2},{"艾雅法拉",2}},
             {{"傀影",2},{"维什戴尔",1}}},
            // W5
            {{{"玛恩纳",2},{"莱伊",2}},
             {{"泥岩",1},{"逻各斯",2},{"华法琳",1}},
             {{"缄默德克萨斯",2},{"能天使",2}}},
            // W6
            {{{"泥岩",2},{"异客",2},{"铃兰",1}},
             {{"山",2},{"斩业星熊",2},{"浊心斯卡蒂",1}},
             {{"赫德雷",3},{"维什戴尔",2},{"莱伊",1}}},
            // W7
            {{{"泥岩",2},{"玛恩纳",2},{"缄默德克萨斯",2}},
             {{"年",3},{"逻各斯",2},{"浊心斯卡蒂",2}},
             {{"异客",2},{"莱伊",2},{"铃兰",2}}},
            // W8: BOSS fixed
            {{{"泥岩",3},{"异客",3},{"缄默德克萨斯",2},{"浊心斯卡蒂",2}}}
        };
    }
    if (diff == Difficulty::Normal) {
        return {
            // W1
            {{{"赫德雷",1}},
             {{"能天使",1}},
             {{"年",1}}},
            // W2
            {{{"山",1},{"艾雅法拉",1}},
             {{"傀影",1},{"华法琳",1}},
             {{"赫德雷",2}}},
            // W3
            {{{"斩业星熊",1},{"维什戴尔",1}},
             {{"能天使",2},{"逻各斯",1}},
             {{"缄默德克萨斯",1},{"铃兰",1}}},
            // W4
            {{{"玛恩纳",1},{"莱伊",1}},
             {{"山",2},{"年",2}},
             {{"浊心斯卡蒂",1},{"异客",1}}},
            // W5
            {{{"傀影",2},{"维什戴尔",2}},
             {{"泥岩",1},{"艾雅法拉",2}},
             {{"斩业星熊",2},{"莱伊",1}}},
            // W6
            {{{"玛恩纳",2},{"逻各斯",2},{"华法琳",1}},
             {{"缄默德克萨斯",2},{"能天使",2},{"铃兰",1}},
             {{"泥岩",1},{"山",2},{"浊心斯卡蒂",2}}},
            // W7
            {{{"泥岩",2},{"异客",2},{"莱伊",2}},
             {{"赫德雷",3},{"缄默德克萨斯",2},{"华法琳",2}},
             {{"玛恩纳",2},{"铃兰",2},{"傀影",2}}},
            // W8
            {{{"泥岩",2},{"逻各斯",2},{"浊心斯卡蒂",2},{"维什戴尔",1}},
             {{"年",3},{"异客",2},{"莱伊",2},{"玛恩纳",1}},
             {{"缄默德克萨斯",2},{"山",2},{"斩业星熊",2},{"能天使",2}}},
            // W9
            {{{"傀影",3},{"泥岩",2},{"铃兰",2},{"异客",2}},
             {{"缄默德克萨斯",2},{"浊心斯卡蒂",2},{"莱伊",2},{"玛恩纳",2}},
             {{"能天使",3},{"逻各斯",2},{"华法琳",2},{"年",2}}},
            // W10: BOSS fixed
            {{{"泥岩",3},{"缄默德克萨斯",3},{"异客",3},{"浊心斯卡蒂",2},{"铃兰",2}}}
        };
    }
    // Hard
    return {
        // W1
        {{{"赫德雷",1}},
         {{"能天使",1}},
         {{"华法琳",1}}},
        // W2
        {{{"山",1}},
         {{"年",1}},
         {{"艾雅法拉",1}}},
        // W3
        {{{"傀影",1},{"维什戴尔",1}},
         {{"赫德雷",2},{"浊心斯卡蒂",1}},
         {{"斩业星熊",1},{"逻各斯",1}}},
        // W4
        {{{"缄默德克萨斯",1},{"莱伊",1}},
         {{"能天使",2},{"铃兰",1}},
         {{"玛恩纳",1},{"山",1}}},
        // W5
        {{{"玛恩纳",1},{"异客",1}},
         {{"傀影",2},{"艾雅法拉",2}},
         {{"泥岩",1},{"华法琳",1}}},
        // W6
        {{{"山",2},{"维什戴尔",2},{"斩业星熊",1}},
         {{"缄默德克萨斯",2},{"逻各斯",2},{"莱伊",1}},
         {{"泥岩",1},{"能天使",2},{"浊心斯卡蒂",2}}},
        // W7
        {{{"玛恩纳",2},{"异客",2},{"铃兰",2}},
         {{"泥岩",2},{"傀影",2},{"华法琳",2}},
         {{"赫德雷",3},{"缄默德克萨斯",2},{"年",2}}},
        // W8
        {{{"泥岩",2},{"莱伊",2},{"浊心斯卡蒂",2},{"逻各斯",1}},
         {{"能天使",3},{"玛恩纳",2},{"斩业星熊",2},{"异客",1}},
         {{"缄默德克萨斯",2},{"山",2},{"铃兰",2},{"维什戴尔",2}}},
        // W9
        {{{"泥岩",2},{"缄默德克萨斯",2},{"异客",2},{"浊心斯卡蒂",2}},
         {{"傀影",3},{"莱伊",2},{"玛恩纳",2},{"华法琳",2}},
         {{"铃兰",2},{"逻各斯",2},{"斩业星熊",2},{"能天使",2}}},
        // W10
        {{{"山",3},{"泥岩",2},{"缄默德克萨斯",2},{"异客",2},{"浊心斯卡蒂",1}},
         {{"缄默德克萨斯",2},{"年",3},{"莱伊",2},{"铃兰",2},{"傀影",2}},
         {{"泥岩",2},{"玛恩纳",2},{"浊心斯卡蒂",2},{"异客",2},{"维什戴尔",2}}},
        // W11
        {{{"泥岩",3},{"缄默德克萨斯",2},{"异客",2},{"浊心斯卡蒂",2},{"莱伊",2}},
         {{"逻各斯",3},{"缄默德克萨斯",2},{"铃兰",2},{"泥岩",2},{"玛恩纳",2}},
         {{"泥岩",2},{"傀影",3},{"异客",2},{"浊心斯卡蒂",2},{"年",2}}},
        // W12
        {{{"泥岩",3},{"缄默德克萨斯",3},{"异客",2},{"浊心斯卡蒂",2},{"铃兰",2},{"玛恩纳",1}},
         {{"异客",3},{"山",3},{"缄默德克萨斯",2},{"浊心斯卡蒂",2},{"莱伊",2},{"华法琳",1}},
         {{"浊心斯卡蒂",3},{"傀影",3},{"泥岩",2},{"异客",2},{"铃兰",2},{"维什戴尔",1}}},
        // W13: BOSS fixed
        {{{"泥岩",3},{"缄默德克萨斯",3},{"异客",3},{"浊心斯卡蒂",3},{"铃兰",2},{"莱伊",2}}}
    };
}

void GameWidget::PlaceEnemyWave(const vector<pair<string, int>>& enemies) {
    ClearEnemyUnits();
    const int count = static_cast<int>(enemies.size());
    if (count == 0) return;

    // Distribute units evenly across the upper half (rows 0-3).
    // Up to 4 units → rows 1-2, 5-6 units → rows 0-3.
    // Columns are spread with at least 1 gap between units.
    if (count <= 4) {
        const int perRow = (count + 1) / 2;
        const int step = 6 / (perRow + 1);
        for (int i = 0; i < count; ++i) {
            const int row = (i < perRow) ? 1 : 2;
            const int idx = (i < perRow) ? i : i - perRow;
            const int col = step * (idx + 1);
            board_.PlaceOpponentUnit(row, col, CreateUnitByName(enemies[i].first, enemies[i].second));
        }
    } else {
        // 5-6 units: fill rows 0-3, alternating columns to avoid stacking
        static const int colMap5[5] = {1, 3, 5, 2, 4};
        static const int colMap6[6] = {1, 3, 5, 2, 4, 6};
        static const int rowMap5[5] = {0, 1, 2, 1, 2};
        static const int rowMap6[6] = {0, 1, 2, 3, 1, 2};
        const int* cols = (count == 5) ? colMap5 : colMap6;
        const int* rows = (count == 5) ? rowMap5 : rowMap6;
        for (int i = 0; i < count; ++i) {
            board_.PlaceOpponentUnit(rows[i], cols[i], CreateUnitByName(enemies[i].first, enemies[i].second));
        }
    }
}

void GameWidget::SpawnTestEnemies() {
    const auto waveData = GetEnemyWaveData(difficulty_);
    if (currentEnemyRound_ < 0 || currentEnemyRound_ >= static_cast<int>(waveData.size())) {
        ClearEnemyUnits();
        return;
    }
    const WaveData& wave = waveData[currentEnemyRound_];
    // Boss waves have only 1 variant; others have 3, pick randomly
    const int variantCount = static_cast<int>(wave.size());
    const int variantIndex = (variantCount <= 1) ? 0 : QRandomGenerator::global()->bounded(variantCount);
    PlaceEnemyWave(wave[variantIndex]);
}

void GameWidget::ClearEnemyUnits() {
    for (int row = 0; row < Board::kRows / 2; ++row) {
        for (int col = 0; col < Board::kCols; ++col) {
            board_.RemoveUnit(row, col);
        }
    }
}

QRect GameWidget::GetMenuTitleRect() const {
    const int titleWidth = qBound(280, width() * 3 / 10, 430);
    const int titleHeight = qBound(72, height() / 12, 96);
    return QRect((width() - titleWidth) / 2, qMax(60, height() / 13), titleWidth, titleHeight);
}

QRect GameWidget::GetMainMenuButtonRect(int index) const {
    const int buttonWidth = qBound(320, width() * 7 / 20, 460);
    const int buttonHeight = qBound(78, height() / 11, 110);
    const int gap = qMax(36, height() / 18);
    const int firstY = height() / 3;
    return QRect((width() - buttonWidth) / 2, firstY + index * (buttonHeight + gap), buttonWidth, buttonHeight);
}

QRect GameWidget::GetSaveSlotButtonRect(int slot) const {
    const int buttonWidth = qBound(330, width() * 7 / 20, 470);
    const int buttonHeight = qBound(74, height() / 12, 96);
    const int gap = qMax(24, height() / 35);
    const int totalHeight = kSaveSlotCount * buttonHeight + (kSaveSlotCount - 1) * gap;
    const int firstY = (height() - totalHeight) / 2 + height() / 9;
    return QRect((width() - buttonWidth) / 2, firstY + slot * (buttonHeight + gap), buttonWidth, buttonHeight);
}

QRect GameWidget::GetMenuBackButtonRect() const {
    QRect title = GetMenuTitleRect();
    const int buttonWidth = qBound(160, width() / 5, 260);
    const int buttonHeight = qBound(58, height() / 13, 86);
    const int y = title.bottom() + qMax(36, height() / 24);
    return QRect((width() - buttonWidth) / 2, y, buttonWidth, buttonHeight);
}

void GameWidget::DrawMenuButton(QPainter& painter, const QRect& buttonRect, const QString& text, int pointSize) {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(247, 238, 182));
    painter.setPen(QPen(QColor(171, 0, 28), 5));
    painter.drawRoundedRect(buttonRect, 26, 26);

    QFont font = painter.font();
    font.setPointSize(pointSize);
    font.setBold(false);
    painter.setFont(font);
    painter.setPen(QColor(145, 0, 24));
    painter.drawText(buttonRect, Qt::AlignCenter, text);
    painter.restore();
}

void GameWidget::DrawMenuStar(QPainter& painter, const QPoint& center, int size) {
    QPolygonF star;
    star << QPointF(center.x(), center.y() - size)
         << QPointF(center.x() + size / 4.0, center.y() - size / 4.0)
         << QPointF(center.x() + size, center.y())
         << QPointF(center.x() + size / 4.0, center.y() + size / 4.0)
         << QPointF(center.x(), center.y() + size)
         << QPointF(center.x() - size / 4.0, center.y() + size / 4.0)
         << QPointF(center.x() - size, center.y())
         << QPointF(center.x() - size / 4.0, center.y() - size / 4.0);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(250, 242, 181));
    painter.setPen(QPen(QColor(255, 35, 35), qMax(4, size / 10)));
    painter.drawPolygon(star);
    painter.restore();
}

void GameWidget::DrawDifficultySelectMenu(QPainter& painter) {
    DrawMenuStar(painter, QPoint(width() / 10, height() / 10), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() * 9 / 10, height() / 10), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() / 10, height() / 2), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() * 9 / 10, height() / 2), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() / 10, height() * 9 / 10), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() * 9 / 10, height() * 9 / 10), qMax(38, width() / 26));

    DrawMenuButton(painter, GetMenuTitleRect(), QString::fromUtf8("选择难度"), qMax(24, width() / 38));
    DrawMenuButton(painter, GetMenuBackButtonRect(), QString::fromUtf8("返回"), qMax(20, width() / 44));

    const int btnCount = 3;
    const int btnWidth = qBound(320, width() * 7 / 20, 460);
    const int btnHeight = qBound(78, height() / 11, 110);
    const int gap = qMax(36, height() / 18);
    const int firstY = height() / 3;
    for (int i = 0; i < btnCount; ++i) {
        QRect btn((width() - btnWidth) / 2, firstY + i * (btnHeight + gap), btnWidth, btnHeight);
        QString text;
        if (i == 0) text = QString::fromUtf8("极速模式 (8回合)");
        else if (i == 1) text = QString::fromUtf8("标准模式 (10回合)");
        else text = QString::fromUtf8("困难模式 (13回合)");
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(i == 0 ? QColor(200, 240, 200) : (i == 1 ? QColor(247, 238, 182) : QColor(240, 200, 200)));
        painter.setPen(QPen(QColor(171, 0, 28), 5));
        painter.drawRoundedRect(btn, 26, 26);
        QFont font = painter.font();
        font.setPointSize(qMax(20, width() / 36));
        painter.setFont(font);
        painter.setPen(QColor(145, 0, 24));
        painter.drawText(btn, Qt::AlignCenter, text);
        painter.restore();
    }
}

void GameWidget::DrawMainMenu(QPainter& painter) {
    DrawMenuStar(painter, QPoint(width() / 7, height() / 10), qMax(44, width() / 25));
    DrawMenuStar(painter, QPoint(width() * 6 / 7, height() / 10), qMax(44, width() / 25));
    DrawMenuStar(painter, QPoint(width() / 7, height() / 2), qMax(44, width() / 25));
    DrawMenuStar(painter, QPoint(width() * 6 / 7, height() / 2), qMax(44, width() / 25));
    DrawMenuStar(painter, QPoint(width() / 7, height() * 9 / 10), qMax(38, width() / 25));
    DrawMenuStar(painter, QPoint(width() * 6 / 7, height() * 9 / 10), qMax(38, width() / 25));

    DrawMenuButton(painter, GetMenuTitleRect(), QString::fromUtf8("单机自走棋"), qMax(24, width() / 38));
    DrawMenuButton(painter, GetMainMenuButtonRect(0), QString::fromUtf8("读取存档"), qMax(28, width() / 33));
    DrawMenuButton(painter, GetMainMenuButtonRect(1), QString::fromUtf8("新的游戏"), qMax(28, width() / 33));
    DrawMenuButton(painter, GetMainMenuButtonRect(2), QString::fromUtf8("快速开始"), qMax(28, width() / 33));
    DrawMenuButton(painter, GetMainMenuButtonRect(3), QString::fromUtf8("退出游戏"), qMax(28, width() / 33));
}

void GameWidget::DrawSaveSelectMenu(QPainter& painter) {
    DrawMenuStar(painter, QPoint(width() / 10, height() / 10), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() * 9 / 10, height() / 10), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() / 10, height() / 2), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() * 9 / 10, height() / 2), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() / 10, height() * 9 / 10), qMax(38, width() / 26));
    DrawMenuStar(painter, QPoint(width() * 9 / 10, height() * 9 / 10), qMax(38, width() / 26));

    DrawMenuButton(painter, GetMenuTitleRect(), QString::fromUtf8("单机自走棋"), qMax(24, width() / 38));
    DrawMenuButton(painter, GetMenuBackButtonRect(), QString::fromUtf8("返回"), qMax(20, width() / 44));
    for (int slot = 0; slot < kSaveSlotCount; ++slot) {
        DrawMenuButton(painter, GetSaveSlotButtonRect(slot), GetSaveSlotButtonText(slot), qMax(24, width() / 38));
    }
}

void GameWidget::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.fillRect(rect(), QColor(48, 50, 54));

    if (screenState_ == ScreenState::MainMenu) {
        DrawMainMenu(painter);
        return;
    }
    if (screenState_ == ScreenState::LoadSelect || screenState_ == ScreenState::NewSelect) {
        DrawSaveSelectMenu(painter);
        return;
    }
    if (screenState_ == ScreenState::DifficultySelect) {
        DrawDifficultySelectMenu(painter);
        return;
    }

    const BoardLayout layout = GetBoardLayout(*this);
    const int cellSize = layout.cellSize;
    const int boardSize = layout.boardSize;
    const int boardX = layout.boardX;
    const int boardY = layout.boardY;
    const int benchY = layout.benchY;
    const int benchHeight = layout.benchHeight;
    const int gap = layout.gap;

    for (int row = 0; row < 8; ++row) {
        for (int col = 0; col < 8; ++col) {
            QRect cell(boardX + col * cellSize, boardY + row * cellSize, cellSize, cellSize);
            const bool isLight = ((row + col) % 2 == 0);
            QColor baseColor = row < 4
                                   ? (isLight ? QColor(247, 210, 210) : QColor(236, 182, 182))
                                   : (isLight ? QColor(206, 230, 250) : QColor(176, 210, 241));
            painter.fillRect(cell, baseColor);
            if (isDraggingUnit_ && dragHoverRow_ == row && dragHoverCol_ == col) {
                painter.fillRect(cell.adjusted(4, 4, -4, -4), QColor(255, 238, 120, 120));
                painter.setPen(QPen(QColor(255, 222, 70), 4));
                painter.drawRect(cell.adjusted(3, 3, -3, -3));
            }
            painter.setPen(QPen(QColor(20, 20, 20), 2));
            painter.drawRect(cell);
        }
    }

    painter.setPen(QPen(QColor(220, 80, 80), 3));
    painter.drawLine(boardX, boardY + cellSize * 4, boardX + boardSize, boardY + cellSize * 4);
    DrawSynergies(painter);

    if (isBattleActive_) {
        for (const CombatUnit& combatUnit : combatUnits_) {
            if (combatUnit.state != CombatState::Dead && combatUnit.unit != nullptr && combatUnit.unit->IsAlive()) {
                const QRect combatRect = GetCombatUnitRect(combatUnit.position);
                DrawUnitIcon(painter, combatRect, combatUnit.unit, combatUnit.state == CombatState::Attacking, cellSize, combatUnit.isEnemy);
                DrawHealthBar(painter, combatRect, combatUnit.unit, combatUnit.isEnemy);
                DrawBuffIndicator(painter, combatRect, combatUnit);
            }
        }
        DrawVisualEffects(painter, cellSize);

        // Draw battle countdown timer
        int countdownSeconds = static_cast<int>(std::ceil(battleCountdownTimer_));
        if (countdownSeconds < 0) countdownSeconds = 0;
        QString timerText = QString::fromUtf8("时间: %1s").arg(countdownSeconds);

        painter.setFont(QFont("Arial", cellSize / 2, QFont::Bold));
        painter.setPen(battleCountdownTimer_ <= 10 ? QColor(255, 50, 50) : QColor(255, 255, 255));

        QRect timerRect(boardX, boardY - cellSize, boardSize, cellSize);
        painter.drawText(timerRect, Qt::AlignCenter, timerText);
    } else {
        for (int row = 0; row < Board::kRows; ++row) {
            for (int col = 0; col < Board::kCols; ++col) {
                if (board_.GetUnitAt(row, col) != nullptr && !(isDraggingUnit_ && !dragSourceIsBench_ && pressedBoardRow_ == row && pressedBoardCol_ == col)) {
                    const bool isSelected = selectedBoardRow_ == row && selectedBoardCol_ == col;
                    const bool isEnemy = board_.IsOpponentHalf(row);
                    DrawUnitIcon(painter, GetBoardCellRect(row, col), board_.GetUnitAt(row, col), isSelected, cellSize, isEnemy);
                }
            }
        }
    }

    painter.setPen(QPen(QColor(20, 20, 20), 3));
    for (int col = 0; col < 8; ++col) {
        QRect benchCell = GetBenchCellRect(col);
        const bool isLight = (col % 2 == 0);
        painter.fillRect(benchCell, isLight ? QColor(252, 232, 173) : QColor(241, 213, 142));
        if (isDraggingUnit_ && !dragSourceIsBench_ && dragHoverBenchSlot_ == col) {
            painter.fillRect(benchCell.adjusted(4, 4, -4, -4), QColor(255, 238, 120, 120));
            painter.setPen(QPen(QColor(255, 222, 70), 4));
            painter.drawRect(benchCell.adjusted(3, 3, -3, -3));
        }
        painter.setPen(QPen(QColor(20, 20, 20), 3));
        painter.drawRect(benchCell);

        if (bench_.GetUnitAt(col) != nullptr && !(isDraggingUnit_ && dragSourceIsBench_ && pressedBenchSlot_ == col)) {
            DrawUnitIcon(painter, benchCell, bench_.GetUnitAt(col), selectedBenchSlot_ == col, cellSize);
        }
    }

    const int hudSize = qMax(46, cellSize * 3 / 5);
    const QRect deployPanel = GetDeployPanelRect();
    const int hudX = deployPanel.x() + (deployPanel.width() - hudSize) / 2;
    QRect benchOriginCell = GetBenchCellRect(0);
    QRect shopOriginCell = GetShopCellRect(0);
    QRect coinRect(hudX, benchOriginCell.center().y() - hudSize / 2, hudSize, hudSize);
    QRect heartRect(hudX, shopOriginCell.center().y() - hudSize / 2, hudSize, hudSize);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(255, 222, 35));
    painter.setPen(QPen(QColor(225, 179, 0), 3));
    painter.drawEllipse(coinRect);

    QPainterPath heartPath;
    const double x = heartRect.x();
    const double y = heartRect.y();
    const double w = heartRect.width();
    const double h = heartRect.height();
    heartPath.moveTo(x + w * 0.50, y + h * 0.88);
    heartPath.cubicTo(x + w * 0.12, y + h * 0.66, x + w * 0.02, y + h * 0.35, x + w * 0.22, y + h * 0.17);
    heartPath.cubicTo(x + w * 0.36, y + h * 0.04, x + w * 0.48, y + h * 0.18, x + w * 0.50, y + h * 0.33);
    heartPath.cubicTo(x + w * 0.52, y + h * 0.18, x + w * 0.64, y + h * 0.04, x + w * 0.78, y + h * 0.17);
    heartPath.cubicTo(x + w * 0.98, y + h * 0.35, x + w * 0.88, y + h * 0.66, x + w * 0.50, y + h * 0.88);
    painter.setBrush(QColor(185, 24, 48));
    painter.setPen(QPen(QColor(255, 239, 190), 3));
    painter.drawPath(heartPath);

    QFont hudFont = painter.font();
    hudFont.setPointSize(qMax(11, hudSize / 4));
    hudFont.setBold(true);
    painter.setFont(hudFont);
    painter.setPen(QColor(20, 20, 20));
    painter.drawText(coinRect, Qt::AlignCenter, QString::number(testPlayer_.GetGold()));
    painter.setPen(QColor(255, 245, 235));
    painter.drawText(heartRect, Qt::AlignCenter, QString::number(testPlayer_.GetHp()));
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(QPainter::Antialiasing, false);

    QRect battleButton = GetStartBattleButtonRect();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(isBattleActive_ ? QColor(130, 130, 130) : QColor(215, 96, 82));
    painter.setPen(QPen(QColor(80, 35, 30), 2));
    painter.drawRoundedRect(battleButton, 6, 6);
    painter.setPen(QColor(255, 245, 235));
    QFont battleFont = painter.font();
    battleFont.setPointSize(qMax(10, cellSize / 6));
    battleFont.setBold(true);
    painter.setFont(battleFont);
    painter.drawText(battleButton, Qt::AlignCenter, isBattleActive_ ? QString::fromUtf8("战斗中") : QString::fromUtf8("开始战斗"));
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.restore();

    // Round counter below the battle button
    {
        const DifficultyConfig& cfg = GetDifficultyConfig(difficulty_);
        QRect roundRect(battleButton.x(), battleButton.bottom() + 100, battleButton.width(), cellSize / 3);
        painter.setPen(QColor(255, 245, 235));
        QFont roundFont = painter.font();
        roundFont.setPointSize(qMax(9, cellSize / 9));
        roundFont.setBold(true);
        painter.setFont(roundFont);
        painter.drawText(roundRect, Qt::AlignCenter,
                         QString::fromUtf8("第 %1/%2 回合").arg(currentEnemyRound_ + 1).arg(cfg.totalRounds));
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setBrush(QColor(203, 195, 238));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(deployPanel, 14, 14);
    painter.setPen(QColor(30, 25, 45));
    QFont deployFont = painter.font();
    deployFont.setPointSize(qMax(10, cellSize / 7));
    deployFont.setBold(true);
    painter.setFont(deployFont);
    painter.drawText(deployPanel.adjusted(6, deployPanel.height() / 8, -6, -deployPanel.height() * 5 / 8), Qt::AlignCenter, QString::fromUtf8("可部署人数:"));
    painter.drawText(deployPanel.adjusted(6, deployPanel.height() / 3, -6, -deployPanel.height() / 3), Qt::AlignCenter,
                     QString("%1/%2").arg(GetPlayerBoardUnitCount()).arg(testPlayer_.GetPopulationLimit()));

    QRect deployUpgradeButton = GetDeployUpgradeButtonRect();
    const bool deployMaxed = testPlayer_.GetPopulationLimit() >= kMaxPopulationLimit;
    painter.setBrush(deployMaxed ? QColor(170, 170, 170) : QColor(247, 235, 150));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(deployUpgradeButton, 8, 8);
    painter.setPen(QColor(30, 25, 45));
    if (deployMaxed) {
        painter.drawText(deployUpgradeButton, Qt::AlignCenter, QString::fromUtf8("最大等级"));
    } else {
        const int pop = testPlayer_.GetPopulationLimit();
        int baseCost = (pop >= 3) ? 8 : ((pop >= 2) ? 6 : 4);
        const int actualCost = max((baseCost + 1) / 2, baseCost - deployUpgradeDiscount_);
        painter.drawText(deployUpgradeButton.adjusted(3, 2, 0, 0), Qt::AlignTop | Qt::AlignLeft, QString::number(actualCost));
        painter.drawText(deployUpgradeButton, Qt::AlignCenter, QString::fromUtf8("升级"));
    }
    painter.restore();

    // 合成配方按钮
    {
        QRect recipeBtn = GetSynthesisRecipeButtonRect();
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QColor(180, 140, 220));
        painter.setPen(QPen(QColor(120, 80, 180), 2));
        painter.drawRoundedRect(recipeBtn, 8, 8);
        QFont btnFont = painter.font();
        btnFont.setPointSize(qMax(9, cellSize / 7));
        btnFont.setBold(true);
        painter.setFont(btnFont);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(recipeBtn, Qt::AlignCenter, QString::fromUtf8("合成配方"));
        painter.restore();
    }
    
    {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        const BoardLayout layout = GetBoardLayout(*this);
        const int cellSize = layout.cellSize;
        QString saveName = GetCurrentSaveSlotName();
        QString displayText = QString::fromUtf8("当前存档：%1").arg(saveName);
        QFont nameFont = painter.font();
        nameFont.setPointSize(qMax(14, cellSize / 4));
        nameFont.setBold(true);
        painter.setFont(nameFont);
        QFontMetrics fm(nameFont);
        int textWidth = fm.horizontalAdvance(displayText);
        int textHeight = fm.height();
        int topY = qMax(10, cellSize / 3);
        int leftX = 0;
        const int padding = 12;
        QRect bgRect(leftX + textWidth / 2 + padding, 
                    topY - padding / 2, 
                    textWidth + padding * 2, 
                    textHeight + padding);
        painter.setPen(QColor(255, 245, 200));
        painter.drawText(bgRect, Qt::AlignCenter, displayText);
        painter.restore();
    }

    painter.save();
    painter.setPen(QPen(QColor(20, 20, 20), 3));
    for (int i = 0; i < 4; ++i) {
        QRect slotRect = GetEquipmentInventoryCellRect(i);
        const bool isLight = (i % 2 == 0);
        painter.fillRect(slotRect, isLight ? QColor(244, 180, 205) : QColor(235, 154, 191));
        painter.drawRect(slotRect);
        shared_ptr<Equipment> equipment = equipmentInventory_.GetEquipmentAt(i);
        if (equipment != nullptr) {
            DrawEquipmentIcon(painter, slotRect, *equipment, selectedEquipmentInventorySlot_ == i, cellSize);
        }
    }
    for (int i = 0; i < equipmentInventory_.GetOverflowCount(); ++i) {
        QRect slotRect = GetEquipmentOverflowCellRect(i);
        const bool isLight = (i % 2 == 0);
        painter.fillRect(slotRect, isLight ? QColor(244, 180, 205) : QColor(235, 154, 191));
        painter.drawRect(slotRect);
        shared_ptr<Equipment> equipment = equipmentInventory_.GetOverflowEquipmentAt(i);
        if (equipment != nullptr) {
            DrawEquipmentIcon(painter, slotRect, *equipment, selectedEquipmentOverflowIndex_ == i, cellSize);
        }
    }
    painter.restore();

    QFont labelFont = painter.font();
    labelFont.setPointSize(qMax(9, cellSize / 7));
    labelFont.setBold(true);

    QFont costFont = painter.font();
    costFont.setPointSize(qMax(10, cellSize / 6));
    costFont.setBold(true);

    for (int col = 0; col < 8; ++col) {
        QRect shopCell = GetShopCellRect(col);
        const bool isLight = (col % 2 == 0);
        const int shopUnitSlot = col - kShopUnitStartCol;
        const int equipmentShopSlot = col - kWeaponShopStartCol;
        const bool isUnitShopCell = col >= kShopUnitStartCol && col < kShopUnitStartCol + kVisibleShopUnitCount;
        const bool isWeaponShopCell = col >= kWeaponShopStartCol && col < kWeaponShopStartCol + kWeaponShopSlotCount;
        const bool isLockedShopSlot = isUnitShopCell && shopUnitSlot >= shop_.GetSellableUnitCount();
        painter.fillRect(shopCell, isLight ? QColor(202, 236, 195) : QColor(175, 219, 169));
        if (isWeaponShopCell) {
            painter.fillRect(shopCell, QColor(172, 178, 174));
        }
        if (isShopFrozen_ && isUnitShopCell && shop_.GetUnitAt(shopUnitSlot) != nullptr) {
            painter.fillRect(shopCell.adjusted(4, 4, -4, -4), QColor(135, 205, 245, 150));
            painter.setPen(QPen(QColor(95, 175, 230), 3));
            painter.drawRect(shopCell.adjusted(5, 5, -5, -5));
        }
        if (isShopFrozen_ && isWeaponShopCell && equipmentShopSlot >= 0 && equipmentShopSlot < static_cast<int>(equipmentShopSlots_.size()) &&
            equipmentShopSlots_[equipmentShopSlot] != nullptr) {
            painter.fillRect(shopCell.adjusted(4, 4, -4, -4), QColor(135, 205, 245, 150));
            painter.setPen(QPen(QColor(95, 175, 230), 3));
            painter.drawRect(shopCell.adjusted(5, 5, -5, -5));
        }
        painter.setPen(QPen(QColor(20, 20, 20), 3));
        painter.drawRect(shopCell);

        if (col == 0) {
            QRect refreshRect = GetRefreshButtonRect();
            QRect freezeRect = GetFreezeButtonRect();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(QColor(190, 229, 181));
            painter.setPen(QPen(QColor(110, 170, 205), 2));
            painter.drawRoundedRect(refreshRect, 6, 6);
            painter.setBrush(isShopFrozen_ ? QColor(166, 219, 248) : QColor(190, 229, 181));
            painter.drawRoundedRect(freezeRect, 6, 6);
            painter.setBrush(Qt::NoBrush);
            painter.setRenderHint(QPainter::Antialiasing, false);
        } else if (col == kShopLevelCol) {
            QRect buttonRect = GetShopUpgradeButtonRect();
            const bool shopMaxed = shop_.GetLevel() >= shop_.GetMaxLevel();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(shopMaxed ? QColor(175, 175, 175) : QColor(203, 236, 188));
            painter.setPen(QPen(QColor(95, 175, 230), 2));
            painter.drawRoundedRect(buttonRect, 6, 6);
            painter.setBrush(Qt::NoBrush);
            painter.setRenderHint(QPainter::Antialiasing, false);
        }

        painter.setFont(costFont);
        painter.setPen(QColor(16, 28, 22));
        if (col == 0) {
            painter.drawText(shopCell.adjusted(1, 1, 0, 0), Qt::AlignTop | Qt::AlignLeft, QString::number(shop_.GetRefreshCost()));
        } else if (isUnitShopCell && shop_.GetUnitAt(shopUnitSlot) != nullptr) {
            painter.drawText(shopCell.adjusted(1, 1, 0, 0), Qt::AlignTop | Qt::AlignLeft, QString::number(shop_.GetUnitAt(shopUnitSlot)->GetPrice()));
        } else if (isWeaponShopCell && equipmentShopSlot >= 0 && equipmentShopSlot < static_cast<int>(equipmentShopSlots_.size()) &&
                   equipmentShopSlots_[equipmentShopSlot] != nullptr) {
            painter.drawText(shopCell.adjusted(1, 1, 0, 0), Qt::AlignTop | Qt::AlignLeft,
                             QString::number(equipmentShopSlots_[equipmentShopSlot]->GetPrice()));
        } else if (col == kShopLevelCol && shop_.GetLevel() < shop_.GetMaxLevel()) {
            painter.drawText(shopCell.adjusted(1, 1, 0, 0), Qt::AlignTop | Qt::AlignLeft, QString::number(shop_.GetCurrentUpgradeCost()));
        }

        if (isUnitShopCell && shop_.GetUnitAt(shopUnitSlot) != nullptr) {
            DrawUnitIcon(painter, shopCell, shop_.GetUnitAt(shopUnitSlot), selectedShopSlot_ == shopUnitSlot, cellSize);
        } else if (isWeaponShopCell && equipmentShopSlot >= 0 && equipmentShopSlot < static_cast<int>(equipmentShopSlots_.size()) &&
                   equipmentShopSlots_[equipmentShopSlot] != nullptr) {
            DrawEquipmentIcon(painter, shopCell, *equipmentShopSlots_[equipmentShopSlot], selectedEquipmentShopSlot_ == equipmentShopSlot, cellSize);
        }

        painter.setFont(labelFont);
        painter.setPen(QColor(10, 18, 14));
        if (col == 0) {
            painter.drawText(GetRefreshButtonRect(), Qt::AlignCenter, QString::fromUtf8("刷新"));
            painter.drawText(GetFreezeButtonRect(), Qt::AlignCenter, QString::fromUtf8("冻结"));
        } else if (col == kShopLevelCol) {
            painter.drawText(GetShopUpgradeButtonRect(), Qt::AlignCenter, shop_.GetLevel() >= shop_.GetMaxLevel() ? QString::fromUtf8("最大") : QString::fromUtf8("升级"));
            QRect levelTitle = shopCell.adjusted(0, cellSize / 2, 0, -cellSize / 4);
            QRect levelNumber = shopCell.adjusted(0, cellSize * 2 / 3, 0, -cellSize / 14);
            painter.drawText(levelTitle, Qt::AlignCenter, "Level");
            painter.drawText(levelNumber, Qt::AlignCenter, QString::number(shop_.GetLevel()));
        }
    }

    if (isDraggingUnit_ && draggingUnit_ != nullptr) {
        QRect dragCell(dragPosition_.x() - cellSize / 2, dragPosition_.y() - cellSize / 2, cellSize, cellSize);
        DrawUnitIcon(painter, dragCell, draggingUnit_, true, cellSize);
    } else if (isDraggingEquipment_ && draggingEquipment_ != nullptr) {
        QRect dragCell(dragPosition_.x() - cellSize / 2, dragPosition_.y() - cellSize / 2, cellSize, cellSize);
        DrawEquipmentIcon(painter, dragCell, *draggingEquipment_, true, cellSize);
    }

    if (!isBattleActive_) {
        QRect backButton = GetGameBackButtonRect();
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(QColor(215, 96, 82));
        painter.setPen(QPen(QColor(120, 40, 32), 3));
        painter.drawRoundedRect(backButton, 10, 10);
        painter.setPen(QColor(255, 245, 235));
        QFont backFont = painter.font();
        backFont.setPointSize(qMax(11, cellSize / 5));
        backFont.setBold(true);
        painter.setFont(backFont);
        painter.drawText(backButton, Qt::AlignCenter, QString::fromUtf8("返回"));
        painter.restore();

        QRect saveButton = GetSaveButtonRect();
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setBrush(canSaveCurrentGame_ ? QColor(215, 96, 82) : QColor(145, 145, 145));
        painter.setPen(QPen(canSaveCurrentGame_ ? QColor(120, 40, 32) : QColor(95, 95, 95), 3));
        painter.drawRoundedRect(saveButton, 10, 10);
        painter.setPen(QColor(255, 245, 235));
        QFont saveFont = painter.font();
        saveFont.setPointSize(qMax(11, cellSize / 5));
        saveFont.setBold(true);
        painter.setFont(saveFont);
        painter.drawText(saveButton, Qt::AlignCenter, QString::fromUtf8("存档"));
        painter.restore();
    }
}

const unordered_map<string, string>& GameWidget::GetUnitSkillNameMap() {
    static const unordered_map<string, string> map = {
        {"赫德雷", "重剑横扫"},
        {"山", "横扫架势"},
        {"玛恩纳", "未照耀的荣光"},
        {"年", "战术装甲"},
        {"斩业星熊", "荆棘反震"},
        {"泥岩", "不屈领域"},
        {"能天使", "过载模式"},
        {"维什戴尔", "爆裂黎明"},
        {"莱伊", "战术狙杀"},
        {"艾雅法拉", "火山喷发"},
        {"逻各斯", "法术湮灭"},
        {"异客", "聚焦指令"},
        {"傀影", "夜幕突袭"},
        {"缄默德克萨斯", "剑影瞬闪"},
        {"华法琳", "紧急包扎"},
        {"浊心斯卡蒂", "海嗣赞歌"},
        {"铃兰", "狐火领域"},
    };
    return map;
}

const unordered_map<string, string>& GameWidget::GetUnitSkillDescriptionMap() {
    static const unordered_map<string, string> map = {
        {"赫德雷", "同时攻击周围所有阻挡的敌人，造成 160% ATK 物理伤害"},
        {"山", "5秒内攻速 +60%，每次攻击回复 3% 最大生命值"},
        {"玛恩纳", "全队防御 +15，ATK +15%，持续 6 秒。若场上存在坦克单位，效果翻倍"},
        {"年", "4秒内获得 40% 物理伤害减伤"},
        {"斩业星熊", "持续 5 秒，获得 300 点屏障，每次被攻击时对周围敌人造成 80 法术伤害"},
        {"泥岩", "获得 30% 最大生命护盾，护盾持续期间每秒回复 2% 最大生命。周围友军获得 15% 减伤，持续 6 秒"},
        {"能天使", "快速射击 4 箭，每箭造成 60% ATK 物理伤害"},
        {"维什戴尔", "对目标及其周围 1 格范围内的敌人造成 140% 范围物理伤害"},
        {"莱伊", "攻击攻击力最高的敌人，造成 350% 物理伤害，优先攻击法师/辅助单位。该次攻击无视目标 50% 防御"},
        {"艾雅法拉", "对目标及其周围敌人造成 150% 法伤，并使其移动速度 -30%，持续 2 秒"},
        {"逻各斯", "对目标及其周围 1 格范围内的敌人造成 230% 范围法伤"},
        {"异客", "释放一道弹射闪电，在敌人间跳跃 6 次，每次造成 140% 法伤"},
        {"傀影", "下一次攻击造成 280% 伤害，必定暴击"},
        {"缄默德克萨斯", "闪烁到当前生命值最低的敌人身后，造成 220% 伤害，随后获得 2 秒不可选中状态"},
        {"华法琳", "治疗生命值最低的友军，恢复 18% 最大生命值"},
        {"浊心斯卡蒂", "全队攻速 +30%，回蓝 +20%，持续 5 秒。该技能不占用攻击动作，可正常普攻"},
        {"铃兰", "敌方全体 ATK -25%，防御 -20，移动速度 -40%，持续 5 秒"},
    };
    return map;
}

void GameWidget::ShowUnitSkillDialog(const shared_ptr<Unit>& unit) {
    if (unit == nullptr) return;

    const auto& params = unit->GetSkillParams();

    // Build skill description from SkillParams
    auto desc = [&]() -> QString {
        const QString typeStr = [&]() {
            switch (params.type) {
                case Unit::SkillParams::Type::Cleave: return QString::fromUtf8("重剑横扫");
                case Unit::SkillParams::Type::AtkBuff: return QString::fromUtf8("自身强化");
                case Unit::SkillParams::Type::TeamBuff: return QString::fromUtf8("全队增益");
                case Unit::SkillParams::Type::DamageReduction: return QString::fromUtf8("减伤");
                case Unit::SkillParams::Type::Barrier: return QString::fromUtf8("屏障");
                case Unit::SkillParams::Type::BarrierAura: return QString::fromUtf8("屏障光环");
                case Unit::SkillParams::Type::MultiHit: return QString::fromUtf8("多段射击");
                case Unit::SkillParams::Type::AoE: return QString::fromUtf8("范围伤害");
                case Unit::SkillParams::Type::MultiTargetNuke: return QString::fromUtf8("狙杀");
                case Unit::SkillParams::Type::Bounce: return QString::fromUtf8("弹射");
                case Unit::SkillParams::Type::HealAlly: return QString::fromUtf8("治疗友军");
                case Unit::SkillParams::Type::NextAttackBuff: return QString::fromUtf8("强化攻击");
                case Unit::SkillParams::Type::TeleportNuke: return QString::fromUtf8("瞬移刺杀");
                case Unit::SkillParams::Type::DebuffEnemies: return QString::fromUtf8("敌方弱化");
                case Unit::SkillParams::Type::PersistAura: return QString::fromUtf8("持续光环");
                default: return QString::fromUtf8("普通攻击");
            }
        }();

        // Read unit_blueprint.txt style descriptions
        const auto& skillDescMap = GetUnitSkillDescriptionMap();
        auto it = skillDescMap.find(unit->GetName());
        if (it != skillDescMap.end()) {
            return QString::fromStdString(it->second);
        }

        // Fallback: auto-generate
        return typeStr;
    }();

    // Skill name lookup from blueprint
    const auto& skillNameMap = GetUnitSkillNameMap();
    QString skillName;
    auto it = skillNameMap.find(unit->GetName());
    if (it != skillNameMap.end()) {
        skillName = QString::fromStdString(it->second);
    } else {
        skillName = QString::fromUtf8("技能");
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("技能详情"));
    dialog.setMinimumSize(380, 220);
    dialog.setStyleSheet(
        "QDialog { background-color: #2a1f2e; border-radius: 12px; }"
        "QLabel#Title { color: #e8d5f5; font-size: 20px; font-weight: 700; }"
        "QLabel#Subtitle { color: #c9a0dc; font-size: 15px; font-weight: 600; }"
        "QLabel#Desc { color: #ffffff; font-size: 14px; }"
        "QLabel#ManaLabel { color: #8ac4ff; font-size: 13px; }"
        "QPushButton { background-color: #7c5c9e; color: white; border: none; border-radius: 6px; padding: 8px 18px; font-weight: 700; }"
        "QPushButton:hover { background-color: #906db5; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(12);

    QLabel* title = new QLabel(QString::fromUtf8("技能详情"), &dialog);
    title->setObjectName("Title");
    layout->addWidget(title);

    // Unit name + skill name
    QString unitSkillLabel = QString::fromStdString(unit->GetName())
                             + QString::fromUtf8(" — ") + skillName;
    QLabel* subtitle = new QLabel(unitSkillLabel, &dialog);
    subtitle->setObjectName("Subtitle");
    layout->addWidget(subtitle);

    // Mana cost
    QLabel* manaLabel = new QLabel(
        QString::fromUtf8("法力消耗: %1 / %2").arg(unit->GetMaxMana()).arg(unit->GetMaxMana()),
        &dialog);
    manaLabel->setObjectName("ManaLabel");
    layout->addWidget(manaLabel);

    // Description
    QLabel* descLabel = new QLabel(desc, &dialog);
    descLabel->setObjectName("Desc");
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    layout->addStretch();

    QPushButton* closeButton = new QPushButton(QString::fromUtf8("关闭"), &dialog);
    layout->addWidget(closeButton, 0, Qt::AlignRight);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void GameWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton && screenState_ == ScreenState::LoadSelect) {
        for (int slot = 0; slot < kSaveSlotCount; ++slot) {
            if (GetSaveSlotButtonRect(slot).contains(event->pos())) {
                ShowSaveSlotContextMenu(slot);
                return;
            }
        }
    }

    // Right-click on bench/board/shop unit in preparation phase → show skill detail
    if (event->button() == Qt::RightButton && screenState_ == ScreenState::Playing && !isBattleActive_) {
        const int shopSlot = GetShopUnitSlotAt(event->pos());
        if (shopSlot != -1) {
            shared_ptr<Unit> unit = shop_.GetUnitAt(shopSlot);
            if (unit != nullptr) {
                ShowUnitSkillDialog(unit);
                return;
            }
        }
        const int benchSlot = GetBenchSlotAt(event->pos());
        if (benchSlot != -1) {
            shared_ptr<Unit> unit = bench_.GetUnitAt(benchSlot);
            if (unit != nullptr) {
                ShowUnitSkillDialog(unit);
                return;
            }
        }
        int boardRow = -1, boardCol = -1;
        if (GetBoardPositionAt(event->pos(), boardRow, boardCol)) {
            shared_ptr<Unit> unit = board_.GetUnitAt(boardRow, boardCol);
            if (unit != nullptr) {
                ShowUnitSkillDialog(unit);
                return;
            }
        }
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (screenState_ == ScreenState::MainMenu) {
        if (GetMainMenuButtonRect(0).contains(event->pos())) {
            screenState_ = ScreenState::LoadSelect;
            update();
            return;
        }
        if (GetMainMenuButtonRect(1).contains(event->pos())) {
            screenState_ = ScreenState::DifficultySelect;
            update();
            return;
        }
        if (GetMainMenuButtonRect(2).contains(event->pos())) {
            difficulty_ = Difficulty::Normal;
            StartNewGame(-1, false);
            return;
        }
        if (GetMainMenuButtonRect(3).contains(event->pos())) {
            QApplication::quit();
            return;
        }
        return;
    }

    if (screenState_ == ScreenState::DifficultySelect) {
        if (GetMenuBackButtonRect().contains(event->pos())) {
            screenState_ = ScreenState::MainMenu;
            update();
            return;
        }
        const int btnWidth = qBound(320, width() * 7 / 20, 460);
        const int btnHeight = qBound(78, height() / 11, 110);
        const int gap = qMax(36, height() / 18);
        const int firstY = height() / 3;
        for (int i = 0; i < 3; ++i) {
            QRect btn((width() - btnWidth) / 2, firstY + i * (btnHeight + gap), btnWidth, btnHeight);
            if (btn.contains(event->pos())) {
                difficulty_ = static_cast<Difficulty>(i);
                screenState_ = ScreenState::NewSelect;
                update();
                return;
            }
        }
        return;
    }

    if (screenState_ == ScreenState::LoadSelect || screenState_ == ScreenState::NewSelect) {
        if (GetMenuBackButtonRect().contains(event->pos())) {
            screenState_ = ScreenState::MainMenu;
            update();
            return;
        }

        for (int slot = 0; slot < kSaveSlotCount; ++slot) {
            if (!GetSaveSlotButtonRect(slot).contains(event->pos())) {
                continue;
            }

            if (screenState_ == ScreenState::LoadSelect) {
                if (!LoadGameFromSlot(slot)) {
                    StartNewGame(slot, true);
                    hasUnsavedChanges_ = true;
                }
            } else {
                ResetSaveSlot(slot);
            }
            update();
            return;
        }
        return;
    }

    if (GetStartBattleButtonRect().contains(event->pos())) {
        StartBattle();
        return;
    }

    if (isBattleActive_) {
        return;
    }

    if (GetGameBackButtonRect().contains(event->pos())) {
        HandleGameBackButton();
        return;
    }

    if (GetSaveButtonRect().contains(event->pos())) {
        HandleSaveButton();
        return;
    }

    if (isGameOver_) {
        return;
    }

    const vector<SynergyStatus> synergies = GetVisibleSynergies();
    for (int i = 0; i < static_cast<int>(synergies.size()); ++i) {
        if (GetSynergyButtonRect(i).contains(event->pos())) {
            ShowSynergyDetailDialog(synergies[i]);
            return;
        }
    }

    if (GetDeployUpgradeButtonRect().contains(event->pos())) {
        HandleDeployUpgradeButton();
        return;
    }

    if (GetFreezeButtonRect().contains(event->pos())) {
        HandleFreezeButton();
        return;
    }

    if (GetRefreshButtonRect().contains(event->pos())) {
        HandleRefreshButton();
        return;
    }

    if (GetShopUpgradeButtonRect().contains(event->pos())) {
        HandleUpgradeButton();
        return;
    }

    if (screenState_ == ScreenState::Playing && !isBattleActive_ && GetSynthesisRecipeButtonRect().contains(event->pos())) {
        ShowSynthesisRecipeDialog();
        return;
    }

    const int equipmentSlot = GetEquipmentShopSlotAt(event->pos());
    if (equipmentSlot != -1) {
        if (equipmentSlot < static_cast<int>(equipmentShopSlots_.size()) && equipmentShopSlots_[equipmentSlot] != nullptr) {
            ShowEquipmentDetailDialog(equipmentSlot);
        }
        return;
    }

    const int inventorySlot = GetEquipmentInventorySlotAt(event->pos());
    if (inventorySlot != -1) {
        shared_ptr<Equipment> equipment = equipmentInventory_.GetEquipmentAt(inventorySlot);
        if (equipment != nullptr) {
            BeginEquipmentInteraction(equipment, event->pos(), -1);
            pressedEquipmentInventorySlot_ = inventorySlot;
            selectedEquipmentInventorySlot_ = inventorySlot;
        }
        return;
    }

    const int overflowSlot = GetEquipmentOverflowSlotAt(event->pos());
    if (overflowSlot != -1) {
        shared_ptr<Equipment> equipment = equipmentInventory_.GetOverflowEquipmentAt(overflowSlot);
        if (equipment != nullptr) {
            BeginEquipmentInteraction(equipment, event->pos(), -1);
            pressedEquipmentOverflowIndex_ = overflowSlot;
            selectedEquipmentOverflowIndex_ = overflowSlot;
        }
        return;
    }

    const int shopSlot = GetShopUnitSlotAt(event->pos());
    if (shopSlot != -1) {
        shared_ptr<Unit> unit = shop_.GetUnitAt(shopSlot);
        if (unit != nullptr) {
            SelectAndShowUnitDetail(unit, selectedShopSlot_, shopSlot, true, shopSlot);
        }
        return;
    }

    const int benchSlot = GetBenchSlotAt(event->pos());
    if (benchSlot != -1) {
        shared_ptr<Unit> unit = bench_.GetUnitAt(benchSlot);
        if (unit != nullptr) {
            BeginUnitInteraction(unit, event->pos(), true, benchSlot, -1, -1);
        }
        return;
    }

    int boardRow = -1;
    int boardCol = -1;
    if (GetBoardPositionAt(event->pos(), boardRow, boardCol)) {
        shared_ptr<Unit> unit = board_.GetUnitAt(boardRow, boardCol);
        if (unit != nullptr) {
            BeginUnitInteraction(unit, event->pos(), false, -1, boardRow, boardCol);
        }
        return;
    }

    QWidget::mousePressEvent(event);
}

void GameWidget::mouseMoveEvent(QMouseEvent* event) {
    if (screenState_ != ScreenState::Playing) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (isGameOver_) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (isBattleActive_) {
        // Show tooltip for combat unit under cursor
        for (const CombatUnit& cu : combatUnits_) {
            if (cu.unit == nullptr || !cu.unit->IsAlive()) continue;
            if (GetCombatUnitRect(cu.position).contains(event->pos())) {
                const auto& u = cu.unit;
                QString stateStr;
                switch (cu.state) {
                case CombatState::Idle: stateStr = QString::fromUtf8("待命"); break;
                case CombatState::Moving: stateStr = QString::fromUtf8("移动中"); break;
                case CombatState::Attacking: stateStr = QString::fromUtf8("攻击中"); break;
                case CombatState::Casting: stateStr = QString::fromUtf8("施法中"); break;
                case CombatState::Dead: stateStr = QString::fromUtf8("阵亡"); break;
                }
                QString tip = QString::fromUtf8(
                    "【%1】 %2\n"
                    "星级: %3\n"
                    "HP: %4 / %5\n"
                    "法力: %6 / %7\n"
                    "攻击力: %8\n"
                    "防御: %9  魔抗: %10\n"
                    "状态: %11")
                    .arg(QString::fromStdString(u->GetName()))
                    .arg(QString::fromStdString(u->GetProfession()))
                    .arg(u->GetStar())
                    .arg(u->GetHp()).arg(u->GetMaxHp())
                    .arg(u->GetMana()).arg(u->GetMaxMana())
                    .arg(GetCombatAttack(cu))
                    .arg(u->GetDefense()).arg(u->GetMagicResist())
                    .arg(stateStr);
                QToolTip::showText(event->globalPosition().toPoint(), tip, this);
                QWidget::mouseMoveEvent(event);
                return;
            }
        }
        QToolTip::hideText();
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (!(event->buttons() & Qt::LeftButton)) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (hasPendingEquipmentClick_ && draggingEquipment_ != nullptr) {
        dragPosition_ = event->pos();
        if (!isDraggingEquipment_ && (event->pos() - dragStartPos_).manhattanLength() >= QApplication::startDragDistance()) {
            isDraggingEquipment_ = true;
        }

        if (isDraggingEquipment_) {
            int row = -1;
            int col = -1;
            if (GetBoardPositionAt(event->pos(), row, col) && board_.IsPlayerHalf(row) && board_.GetUnitAt(row, col) != nullptr) {
                dragHoverRow_ = row;
                dragHoverCol_ = col;
            } else {
                dragHoverRow_ = -1;
                dragHoverCol_ = -1;
            }

            const int benchSlot = GetBenchSlotAt(event->pos());
            dragHoverBenchSlot_ = (benchSlot != -1 && bench_.GetUnitAt(benchSlot) != nullptr) ? benchSlot : -1;
            update();
            return;
        }
    }

    if (!hasPendingUnitClick_ || draggingUnit_ == nullptr) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    dragPosition_ = event->pos();
    if (!isDraggingUnit_ && (event->pos() - dragStartPos_).manhattanLength() >= QApplication::startDragDistance()) {
        isDraggingUnit_ = true;
    }

    if (isDraggingUnit_) {
        int row = -1;
        int col = -1;
        if (GetBoardPositionAt(event->pos(), row, col) && board_.IsPlayerHalf(row)) {
            dragHoverRow_ = row;
            dragHoverCol_ = col;
        } else {
            dragHoverRow_ = -1;
            dragHoverCol_ = -1;
        }

        dragHoverBenchSlot_ = -1;
        const int benchSlot = GetBenchSlotAt(event->pos());
        if (benchSlot != -1) {
            dragHoverBenchSlot_ = bench_.IsEmpty(benchSlot) ? GetRightmostEmptyBenchSlot() : benchSlot;
        }
        update();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void GameWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (screenState_ != ScreenState::Playing) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (isGameOver_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (isBattleActive_) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton || (!hasPendingUnitClick_ && !hasPendingEquipmentClick_)) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    if (isDraggingEquipment_) {
        if (HandleDraggedEquipmentRelease()) {
            MarkGameChanged();
        }
    } else if (hasPendingEquipmentClick_ && draggingEquipment_ != nullptr) {
        ShowEquipmentDetailDialog(draggingEquipment_, false, -1, pressedEquipmentInventorySlot_, pressedEquipmentOverflowIndex_);
    } else if (isDraggingUnit_) {
        HandleDraggedUnitRelease();
        MarkGameChanged();
    } else if (draggingUnit_ != nullptr) {
        ShowUnitDetailDialog(draggingUnit_, false, -1, dragSourceIsBench_ ? pressedBenchSlot_ : -1);
    }

    ResetUnitInteraction();
    ResetEquipmentInteraction();
    update();
}

QRect GameWidget::GetBoardCellRect(int row, int col) const {
    const BoardLayout layout = GetBoardLayout(*this);
    return QRect(layout.boardX + col * layout.cellSize,
                 layout.boardY + row * layout.cellSize,
                 layout.cellSize,
                 layout.cellSize);
}

QRect GameWidget::GetBenchCellRect(int col) const {
    const BoardLayout layout = GetBoardLayout(*this);
    return QRect(layout.boardX + col * layout.cellSize,
                 layout.benchY,
                 layout.cellSize,
                 layout.benchHeight);
}

QRect GameWidget::GetShopCellRect(int col) const {
    const BoardLayout layout = GetBoardLayout(*this);
    return QRect(layout.shopX + col * layout.cellSize,
                 layout.shopY,
                 layout.cellSize,
                 layout.shopHeight);
}

QRect GameWidget::GetRefreshButtonRect() const {
    QRect shopCell = GetShopCellRect(0);
    return QRect(shopCell.x() + shopCell.width() / 8,
                 shopCell.y() + shopCell.height() / 10,
                 shopCell.width() * 3 / 4,
                 shopCell.height() / 3);
}

QRect GameWidget::GetFreezeButtonRect() const {
    QRect shopCell = GetShopCellRect(0);
    return QRect(shopCell.x() + shopCell.width() / 8,
                 shopCell.y() + shopCell.height() * 11 / 20,
                 shopCell.width() * 3 / 4,
                 shopCell.height() / 3);
}

QRect GameWidget::GetShopUpgradeButtonRect() const {
    QRect shopCell = GetShopCellRect(kShopLevelCol);
    return QRect(shopCell.x() + shopCell.width() / 7,
                 shopCell.y() + shopCell.height() / 9,
                 shopCell.width() * 5 / 7,
                 shopCell.height() / 3);
}

QRect GameWidget::GetStartBattleButtonRect() const {
    QRect topRightCell = GetBoardCellRect(0, Board::kCols - 1);
    const int width = qMax(96, topRightCell.width() * 3 / 2);
    const int height = qMax(32, topRightCell.height() / 2);
    const int x = topRightCell.right() + topRightCell.width() / 4;
    const int y = topRightCell.y();
    return QRect(x, y, width, height);
}

QRect GameWidget::GetSaveButtonRect() const {
    QRect backButton = GetGameBackButtonRect();
    QRect boardCell = GetBoardCellRect(0, Board::kCols - 1);
    const int y = backButton.bottom() + boardCell.height() / 5;
    return QRect(backButton.x(), y, backButton.width(), backButton.height());
}

QRect GameWidget::GetGameBackButtonRect() const {
    QRect startButton = GetStartBattleButtonRect();
    QRect boardCell = GetBoardCellRect(0, Board::kCols - 1);
    const int y = startButton.bottom() + boardCell.height() / 5;
    return QRect(startButton.x(), y, startButton.width(), startButton.height());
}

QRect GameWidget::GetDeployPanelRect() const {
    QRect boardCell = GetBoardCellRect(Board::kRows - 3, 0);
    const int width = qMax(100, boardCell.width() * 7 / 5);
    const int height = qMax(180, boardCell.height() * 5 / 2);
    const int x = boardCell.left() - width - boardCell.width() / 6;
    const int y = boardCell.y();
    return QRect(x, y, width, height);
}

QRect GameWidget::GetDeployUpgradeButtonRect() const {
    QRect panel = GetDeployPanelRect();
    const int width = panel.width() * 2 / 3;
    const int height = panel.height() / 5;
    return QRect(panel.x() + (panel.width() - width) / 2,
                 panel.bottom() - height - panel.height() / 7,
                 width,
                 height);
}

QRect GameWidget::GetEquipmentInventoryRect() const {
    QRect boardCell = GetBoardCellRect(Board::kRows - 2, Board::kCols - 1);
    const int width = boardCell.width();
    const int height = boardCell.height() * 4;
    const int x = boardCell.right() + boardCell.width() / 2;
    const int y = boardCell.center().y() - height / 2;
    return QRect(x, y, width, height);
}

QRect GameWidget::GetEquipmentInventoryCellRect(int index) const {
    QRect rect = GetEquipmentInventoryRect();
    const int slotHeight = rect.width();
    return QRect(rect.x(),
                 rect.y() + index * slotHeight,
                 rect.width(),
                 slotHeight);
}

QRect GameWidget::GetEquipmentOverflowCellRect(int index) const {
    QRect inventoryCell = GetEquipmentInventoryCellRect(0);
    return QRect(inventoryCell.x(),
                 inventoryCell.y() - (index + 1) * inventoryCell.height(),
                 inventoryCell.width(),
                 inventoryCell.height());
}

QRect GameWidget::GetCombatUnitRect(const QPointF& position) const {
    QRect originCell = GetBoardCellRect(0, 0);
    const int cellSize = originCell.width();
    const QPointF center(originCell.x() + position.x() * cellSize,
                         originCell.y() + position.y() * cellSize);
    return QRect(static_cast<int>(center.x() - cellSize / 2.0),
                 static_cast<int>(center.y() - cellSize / 2.0),
                 cellSize,
                 cellSize);
}

QRect GameWidget::GetSynergyButtonRect(int index) const {
    QRect originCell = GetBoardCellRect(0, 0);
    const int cellSize = originCell.width();
    const int diameter = qMax(42, cellSize * 3 / 5);
    const int x = originCell.left() - diameter - cellSize * 3 / 5;
    const int y = originCell.top() + cellSize * 3 / 2 + index * (diameter + cellSize / 3);
    return QRect(x, y, diameter, diameter);
}

QRect GameWidget::GetSynthesisRecipeButtonRect() const {
    const BoardLayout layout = GetBoardLayout(*this);
    QRect inventoryRect = GetEquipmentInventoryRect();
    const int btnWidth = layout.cellSize;
    const int btnHeight = layout.cellSize * 3 / 5;
    return QRect(inventoryRect.x(),
                 layout.shopY,
                 btnWidth,
                 btnHeight);
}

bool GameWidget::GetBoardPositionAt(const QPoint& position, int& row, int& col) const {
    for (int r = 0; r < Board::kRows; ++r) {
        for (int c = 0; c < Board::kCols; ++c) {
            if (GetBoardCellRect(r, c).contains(position)) {
                row = r;
                col = c;
                return true;
            }
        }
    }

    row = -1;
    col = -1;
    return false;
}

int GameWidget::GetBenchSlotAt(const QPoint& position) const {
    for (int col = 0; col < Bench::kSize; ++col) {
        if (GetBenchCellRect(col).contains(position)) {
            return col;
        }
    }

    return -1;
}

int GameWidget::GetRightmostEmptyBenchSlot() const {
    for (int col = Bench::kSize - 1; col >= 0; --col) {
        if (bench_.IsEmpty(col)) {
            return col;
        }
    }

    return -1;
}

int GameWidget::GetPlayerBoardUnitCount() const {
    int count = 0;
    for (int row = Board::kRows / 2; row < Board::kRows; ++row) {
        for (int col = 0; col < Board::kCols; ++col) {
            if (board_.GetUnitAt(row, col) != nullptr) {
                ++count;
            }
        }
    }
    return count;
}

vector<GameWidget::SynergyStatus> GameWidget::GetVisibleSynergies() const {
    vector<SynergyStatus> synergies;
    for (const SynergyDefinition& definition : GetSynergyDefinitions()) {
        set<string> uniqueNames;
        for (int row = Board::kRows / 2; row < Board::kRows; ++row) {
            for (int col = 0; col < Board::kCols; ++col) {
                shared_ptr<Unit> unit = board_.GetUnitAt(row, col);
                if (unit == nullptr) continue;
                for (const char* prof : definition.professions) {
                    if (IsProfession(unit, prof)) {
                        uniqueNames.insert(unit->GetName());
                        break;
                    }
                }
            }
        }
        const int count = static_cast<int>(uniqueNames.size());
        if (count > 0) {
            const int req = (definition.required4 > 0 && count >= definition.required4) ? definition.required4 : definition.required2;
            synergies.push_back({
                QString::fromUtf8(definition.id),
                QString::fromUtf8(definition.label),
                QString::fromUtf8(definition.description),
                count,
                req
            });
        }
    }
    return synergies;
}

bool GameWidget::IsPlayerBoardUnit(const shared_ptr<Unit>& unit) const {
    if (unit == nullptr) {
        return false;
    }

    for (int row = Board::kRows / 2; row < Board::kRows; ++row) {
        for (int col = 0; col < Board::kCols; ++col) {
            if (board_.GetUnitAt(row, col) == unit) {
                return true;
            }
        }
    }
    return false;
}

bool GameWidget::IsSynergyActiveForProfession(const string& profession) const {
    for (const SynergyStatus& synergy : GetVisibleSynergies()) {
        if (synergy.count < synergy.required) continue;
        // Check if any of the synergy's professions match
        for (const SynergyDefinition& def : GetSynergyDefinitions()) {
            if (def.id == synergy.id) {
                for (const char* p : def.professions) {
                    if (p == profession) return true;
                }
                break;
            }
        }
    }
    return false;
}

int GameWidget::GetSynergyAdjustedAtk(const shared_ptr<Unit>& unit) const {
    if (unit == nullptr || !IsPlayerBoardUnit(unit)) {
        return unit == nullptr ? 0 : unit->GetAtk();
    }

    int bonus = 0;
    for (const SynergyStatus& synergy : GetVisibleSynergies()) {
        if (synergy.count < synergy.required) continue;
        if (synergy.id == "elemental") {
            bonus += static_cast<int>(unit->GetAtk() * 0.15);
        }
    }
    return unit->GetAtk() + bonus;
}

QString GameWidget::GetUnitSynergyText(const shared_ptr<Unit>& unit) const {
    if (unit == nullptr) {
        return QString::fromUtf8("无");
    }
    if (!IsPlayerBoardUnit(unit)) {
        return QString::fromUtf8("未生效（未在己方棋盘）");
    }
    if (!IsSynergyActiveForProfession(unit->GetProfession())) {
        return QString::fromUtf8("未激活");
    }

    if (IsProfession(unit, "战士")) {
        return QString::fromUtf8("战士已激活：攻击力 +25%");
    }
    if (IsProfession(unit, "弓箭手")) {
        return QString::fromUtf8("弓箭手已激活：射程 +0.75，攻击力 +10%");
    }
    if (IsProfession(unit, "坦克")) {
        return QString::fromUtf8("坦克已激活：受到伤害 -25%");
    }
    if (IsProfession(unit, "刺客")) {
        return QString::fromUtf8("刺客已激活：移动速度 +25%，攻击力 +15%");
    }
    return QString::fromUtf8("无");
}

int GameWidget::GetShopUnitSlotAt(const QPoint& position) const {
    const int visibleUnitCount = std::min(kVisibleShopUnitCount, shop_.GetSellableUnitCount());
    for (int col = kShopUnitStartCol; col < kShopUnitStartCol + visibleUnitCount; ++col) {
        if (GetShopCellRect(col).contains(position)) {
            return col - kShopUnitStartCol;
        }
    }

    return -1;
}

int GameWidget::GetEquipmentShopSlotAt(const QPoint& position) const {
    for (int col = kWeaponShopStartCol; col < kWeaponShopStartCol + kWeaponShopSlotCount; ++col) {
        if (GetShopCellRect(col).contains(position)) {
            return col - kWeaponShopStartCol;
        }
    }

    return -1;
}

int GameWidget::GetEquipmentInventorySlotAt(const QPoint& position) const {
    for (int i = 0; i < EquipmentInventory::kSlotCount; ++i) {
        if (GetEquipmentInventoryCellRect(i).contains(position)) {
            return i;
        }
    }
    return -1;
}

int GameWidget::GetEquipmentOverflowSlotAt(const QPoint& position) const {
    for (int i = 0; i < equipmentInventory_.GetOverflowCount(); ++i) {
        if (GetEquipmentOverflowCellRect(i).contains(position)) {
            return i;
        }
    }
    return -1;
}

QString GameWidget::GetEquipmentSummary(const shared_ptr<Unit>& unit) const {
    if (unit == nullptr || unit->GetEquipments().empty()) {
        return QString::fromUtf8("无");
    }

    QStringList names;
    for (const Equipment& equipment : unit->GetEquipments()) {
        names.append(QString::fromUtf8("%1 %2星")
                         .arg(QString::fromStdString(equipment.GetName()))
                         .arg(equipment.GetStar()));
    }
    return names.join(", ");
}

QString GameWidget::GetEquipmentDescription(const Equipment& equipment) const {
    QStringList parts;
    if (equipment.GetAtkBonus() > 0)
        parts.append(QString::fromUtf8("攻击力 +%1").arg(equipment.GetAtkBonus()));
    if (equipment.GetHpBonus() > 0)
        parts.append(QString::fromUtf8("生命值 +%1").arg(equipment.GetHpBonus()));
    if (equipment.GetDefenseBonus() > 0)
        parts.append(QString::fromUtf8("防御力 +%1").arg(equipment.GetDefenseBonus()));
    if (equipment.GetMagicResistBonus() > 0)
        parts.append(QString::fromUtf8("魔抗 +%1").arg(equipment.GetMagicResistBonus()));
    if (equipment.GetLifesteal() > 0)
        parts.append(QString::fromUtf8("吸血 +%1%%").arg(equipment.GetLifesteal()));
    if (equipment.GetArmorPenetration() > 0)
        parts.append(QString::fromUtf8("破甲 %1%%").arg(equipment.GetArmorPenetration()));
    if (equipment.GetCleaveRange() > 0)
        parts.append(QString::fromUtf8("溅射 %1格 %2%%伤害").arg(equipment.GetCleaveRange(), 0, 'f', 1).arg(static_cast<int>(equipment.GetCleaveDamage() * 100)));
    if (equipment.GetReviveHpPercent() > 0)
        parts.append(QString::fromUtf8("复活回复 %1%%HP").arg(equipment.GetReviveHpPercent()));
    if (equipment.GetAuraDamage() > 0)
        parts.append(QString::fromUtf8("光环伤害 %1/秒").arg(equipment.GetAuraDamage()));
    if (equipment.GetSkillDamageBonus() > 0)
        parts.append(QString::fromUtf8("技能伤害 +%1%%").arg(equipment.GetSkillDamageBonus()));
    if (equipment.GetInitialManaBonus() > 0)
        parts.append(QString::fromUtf8("初始蓝量 +%1").arg(equipment.GetInitialManaBonus()));
    if (equipment.GetHpRegenPercent() > 0)
        parts.append(QString::fromUtf8("生命回复 %1%%/秒").arg(equipment.GetHpRegenPercent()));
    if (equipment.GetDamageReflectPercent() > 0)
        parts.append(QString::fromUtf8("反弹 %1%%伤害").arg(equipment.GetDamageReflectPercent()));
    if (equipment.GetDoubleHit())
        parts.append(QString::fromUtf8("双重攻击"));
    if (equipment.GetPercentHpDamage() > 0)
        parts.append(QString::fromUtf8("额外 %1%%目标生命值伤害").arg(static_cast<int>(equipment.GetPercentHpDamage() * 100)));
    if (equipment.GetExtraManaOnHit() > 0)
        parts.append(QString::fromUtf8("普攻额外回蓝 +%1").arg(equipment.GetExtraManaOnHit()));
    if (equipment.GetSplashAttack())
        parts.append(QString::fromUtf8("范围普攻"));
    if (parts.isEmpty()) {
        return QString::fromUtf8("无加成");
    }
    return QString::fromUtf8("%1星: %2").arg(equipment.GetStar()).arg(parts.join(" | "));
}

int GameWidget::GetEquipmentSellPrice(const Equipment& equipment) const {
    return max(1, equipment.GetPrice() - 1);
}

void GameWidget::DrawUnitIcon(QPainter& painter, const QRect& cell, const shared_ptr<Unit>& unit, bool isSelected, int cellSize, bool isEnemy) {
    if (unit == nullptr) {
        return;
    }

    painter.save();

    const int iconMargin = cellSize / 4;
    QRect iconRect = cell.adjusted(iconMargin, iconMargin, -iconMargin, -iconMargin);

    // Try to load profession icon
    const string profession = unit->GetProfession();
    QPixmap professionIcon;
    bool iconLoaded = false;

    // Check cache
    auto it = professionIconCache_.find(profession);
    if (it != professionIconCache_.end()) {
        professionIcon = it->second;
        iconLoaded = !professionIcon.isNull();
    } else {
        // Build path relative to executable directory
        QString exeDir = QCoreApplication::applicationDirPath();
        QString iconPath = exeDir + "/../assets/units/" + QString::fromStdString(profession) + ".png";
        if (!professionIcon.load(iconPath)) {
            iconPath = exeDir + "/../assets/units/" + QString::fromStdString(profession) + ".jpg";
            professionIcon.load(iconPath);
        }

        if (!professionIcon.isNull()) {
            professionIconCache_[profession] = professionIcon;
            iconLoaded = true;
        } else {
            professionIconCache_[profession] = QPixmap();
        }
    }

    // Draw icon (either image or geometric fallback)
    if (iconLoaded) {
        // Draw profession image
        QPixmap scaledIcon = professionIcon.scaled(iconRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        int x = iconRect.center().x() - scaledIcon.width() / 2;
        int y = iconRect.center().y() - scaledIcon.height() / 2;
        painter.drawPixmap(x, y, scaledIcon);

        // Draw selection highlight overlay
        if (isSelected) {
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setPen(QPen(QColor(255, 235, 120), 3));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(iconRect.adjusted(-2, -2, 2, 2));
        }
    } else {
        // Fallback to geometric shapes based on profession
        const bool isTank = IsProfession(unit, "坦克");
        const bool isAssassin = IsProfession(unit, "刺客");
        const bool isArcher = IsProfession(unit, "弓箭手");
        const bool isMage = IsProfession(unit, "法师");
        const bool isSupport = IsProfession(unit, "辅助");

        painter.setRenderHint(QPainter::Antialiasing, true);
        const QColor unitColor = isEnemy ? QColor(210, 86, 82) : QColor(90, 140, 210);
        const QColor selectedColor = isEnemy ? QColor(238, 116, 110) : QColor(110, 166, 238);
        const QColor outlineColor = isEnemy ? QColor(120, 35, 35) : QColor(20, 45, 90);
        painter.setBrush(isSelected ? selectedColor : unitColor);
        painter.setPen(QPen(isSelected ? QColor(255, 235, 120) : outlineColor, isSelected ? 5 : 3));

        if (isTank) {
            painter.drawRect(iconRect);
        } else if (isAssassin) {
            QPolygonF triangle;
            triangle << QPointF(iconRect.center().x(), iconRect.bottom())
                     << QPointF(iconRect.left(), iconRect.top())
                     << QPointF(iconRect.right(), iconRect.top());
            painter.drawPolygon(triangle);
        } else if (isArcher) {
            painter.drawPolygon(BuildStarPolygon(iconRect));
        } else if (isMage) {
            const int crossThickness = qMax(4, iconRect.width() / 5);
            QPainterPath crossPath;
            crossPath.addRect(iconRect.center().x() - crossThickness / 2.0, iconRect.top(),
                              crossThickness, iconRect.height());
            crossPath.addRect(iconRect.left(), iconRect.center().y() - crossThickness / 2.0,
                              iconRect.width(), crossThickness);
            painter.drawPath(crossPath.simplified());
        } else if (isSupport) {
            QPolygonF diamond;
            diamond << QPointF(iconRect.center().x(), iconRect.top())
                    << QPointF(iconRect.right(), iconRect.center().y())
                    << QPointF(iconRect.center().x(), iconRect.bottom())
                    << QPointF(iconRect.left(), iconRect.center().y());
            painter.drawPolygon(diamond);
        } else {
            painter.drawEllipse(iconRect);
        }

        if (isSelected) {
            QRect glowRect = iconRect.adjusted(-4, -4, 4, 4);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor(255, 245, 170, 160), 2));
            if (isTank) {
                painter.drawRect(glowRect);
            } else if (isAssassin) {
                QPolygonF triangle;
                triangle << QPointF(glowRect.center().x(), glowRect.bottom())
                         << QPointF(glowRect.left(), glowRect.top())
                         << QPointF(glowRect.right(), glowRect.top());
                painter.drawPolygon(triangle);
            } else if (isArcher) {
                painter.drawPolygon(BuildStarPolygon(glowRect));
            } else if (isMage) {
                const int glowThickness = qMax(5, glowRect.width() / 6);
                QPainterPath crossGlow;
                crossGlow.addRect(glowRect.center().x() - glowThickness / 2.0, glowRect.top(),
                                  glowThickness, glowRect.height());
                crossGlow.addRect(glowRect.left(), glowRect.center().y() - glowThickness / 2.0,
                                  glowRect.width(), glowThickness);
                painter.drawPath(crossGlow.simplified());
            } else if (isSupport) {
                QPolygonF diamond;
                diamond << QPointF(glowRect.center().x(), glowRect.top())
                        << QPointF(glowRect.right(), glowRect.center().y())
                        << QPointF(glowRect.center().x(), glowRect.bottom())
                        << QPointF(glowRect.left(), glowRect.center().y());
                painter.drawPolygon(diamond);
            } else {
                painter.drawEllipse(glowRect);
            }
        }
    }

    // Draw equipment indicators
    const int equippedCount = std::min(unit->GetCurrentEquipmentCount(), unit->GetMaxEquipmentCount());
    if (equippedCount > 0) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        const int markerWidth = qMax(12, cellSize / 3);
        const int markerHeight = qMax(3, cellSize / 18);
        const int markerGap = qMax(6, cellSize / 7);
        const int totalWidth = equippedCount * markerWidth + (equippedCount - 1) * markerGap;
        const int startX = cell.center().x() - totalWidth / 2;
        const int markerY = cell.top() + qMax(3, cellSize / 20);

        painter.setBrush(QColor(145, 92, 62));
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < equippedCount; ++i) {
            QRect markerRect(startX + i * (markerWidth + markerGap), markerY, markerWidth, markerHeight);
            painter.drawRoundedRect(markerRect, markerHeight / 2.0, markerHeight / 2.0);
        }
        painter.restore();
    }

    // Draw star rating indicator
    if (unit->GetStar() >= 2) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        const int starCount = unit->GetStar();
        const int starSize = qMax(5, cellSize / 10);
        const int spacing = starSize * 2;
        const int totalWidth = starCount * spacing - spacing / 2;
        const int startX = cell.center().x() - totalWidth / 2;
        const int starY = cell.top() + starSize;

        QColor starColor = (starCount >= 3) ? QColor(255, 200, 50) : QColor(200, 215, 255);
        painter.setBrush(starColor);
        painter.setPen(QPen(QColor(255, 255, 255, 180), 1));

        for (int s = 0; s < starCount; ++s) {
            QPointF center(startX + s * spacing, starY);
            QPolygonF starPoly;
            for (int i = 0; i < 10; ++i) {
                double r = (i % 2 == 0) ? starSize : starSize * 0.45;
                double a = -3.14159 / 2.0 + i * 3.14159 / 5.0;
                starPoly << QPointF(center.x() + std::cos(a) * r, center.y() + std::sin(a) * r);
            }
            painter.drawPolygon(starPoly);
        }
        painter.restore();
    }

    painter.restore();
}

static QColor GetEquipmentRarityColor(const Equipment& equipment) {
    if (!equipment.CanFuse() && equipment.GetRecipe1().empty()) {
        // Special equipment (sold in shop, can't fuse) — purple
        return QColor(180, 120, 220);
    }
    if (!equipment.GetRecipe1().empty()) {
        // Ultimate合成装备 — red/orange
        return QColor(230, 100, 80);
    }
    if (equipment.GetPrice() >= 3) {
        // Advanced — blue
        return QColor(90, 160, 220);
    }
    // Basic — yellow/gold
    return QColor(247, 200, 72);
}

void GameWidget::DrawEquipmentIcon(QPainter& painter, const QRect& cell, const Equipment& equipment, bool isSelected, int cellSize) const {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QRect iconRect = cell.adjusted(cellSize / 5, cellSize / 5, -cellSize / 5, -cellSize / 3);
    QPolygonF diamond;
    diamond << QPointF(iconRect.center().x(), iconRect.top())
            << QPointF(iconRect.right(), iconRect.center().y())
            << QPointF(iconRect.center().x(), iconRect.bottom())
            << QPointF(iconRect.left(), iconRect.center().y());

    const QColor rarityColor = GetEquipmentRarityColor(equipment);
    painter.setBrush(isSelected ? rarityColor.lighter(140) : rarityColor);
    painter.setPen(QPen(isSelected ? rarityColor.lighter(180) : rarityColor.darker(150), isSelected ? 4 : 3));
    painter.drawPolygon(diamond);

    QFont labelFont = painter.font();
    labelFont.setPointSize(qMax(8, cellSize / 6));
    labelFont.setBold(true);
    painter.setFont(labelFont);
    painter.setPen(QColor(42, 32, 12));
    painter.drawText(cell.adjusted(2, cellSize / 2, -2, -2), Qt::AlignHCenter | Qt::AlignTop,
                     QString::fromStdString(equipment.GetName()));

    if (equipment.GetStar() >= 2) {
        QFont starFont = painter.font();
        starFont.setPointSize(qMax(7, cellSize / 7));
        starFont.setBold(true);
        painter.setFont(starFont);
        painter.setPen(QColor(80, 55, 12));
        painter.drawText(cell.adjusted(2, 2, -2, 0), Qt::AlignTop | Qt::AlignRight,
                         QString("%1*").arg(equipment.GetStar()));
    }

    painter.restore();
}

void GameWidget::DrawHealthBar(QPainter& painter, const QRect& cell, const shared_ptr<Unit>& unit, bool isEnemy) {
    if (unit == nullptr || unit->GetMaxHp() <= 0) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);

    // HP bar
    const int barHeight = qMax(4, cell.height() / 13);
    QRect barBack = cell.adjusted(cell.width() / 8, cell.height() / 10, -cell.width() / 8, -cell.height() + cell.height() / 10 + barHeight);
    const double hpRate = std::max(0.0, std::min(1.0, static_cast<double>(unit->GetHp()) / unit->GetMaxHp()));
    QRect barFill = barBack;
    barFill.setWidth(static_cast<int>(barBack.width() * hpRate));

    painter.fillRect(barBack, QColor(40, 40, 40));
    painter.fillRect(barFill, isEnemy ? QColor(220, 65, 55) : QColor(65, 135, 225));
    painter.setPen(QPen(QColor(15, 15, 15), 1));
    painter.drawRect(barBack);

    // Mana bar (below HP bar, smaller)
    if (unit->GetMaxMana() > 0) {
        const int manaBarHeight = qMax(3, barHeight * 2 / 3);
        QRect manaBack = barBack.adjusted(0, barHeight + 2, 0, barHeight + 2 - manaBarHeight);
        const double manaRate = std::max(0.0, std::min(1.0, static_cast<double>(unit->GetMana()) / unit->GetMaxMana()));
        QRect manaFill = manaBack;
        manaFill.setWidth(static_cast<int>(manaBack.width() * manaRate));

        painter.fillRect(manaBack, QColor(30, 30, 50));
        painter.fillRect(manaFill, QColor(100, 160, 255));
        painter.setPen(QPen(QColor(15, 15, 15), 1));
        painter.drawRect(manaBack);
    }

    painter.restore();
}

void GameWidget::DrawSynergies(QPainter& painter) {
    const vector<SynergyStatus> synergies = GetVisibleSynergies();
    if (synergies.empty()) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    font.setBold(true);

    for (int i = 0; i < static_cast<int>(synergies.size()); ++i) {
        const SynergyStatus& synergy = synergies[i];
        const QRect circle = GetSynergyButtonRect(i);
        const bool active = synergy.count >= synergy.required;
        const double progress = std::min(1.0, static_cast<double>(synergy.count) / synergy.required);
        const QColor fillColor = active ? QColor(178, 245, 30) : QColor(124, 220, 42);
        const QColor rimColor = active ? QColor(42, 190, 63) : QColor(24, 150, 65);

        painter.setBrush(fillColor);
        painter.setPen(QPen(rimColor, 4));
        painter.drawEllipse(circle);

        QRect ring = circle.adjusted(3, 3, -3, -3);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(QColor(238, 255, 160), qMax(4, circle.width() / 12), Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(ring, 90 * 16, static_cast<int>(progress * 360 * 16));

        font.setPointSize(qMax(8, circle.width() / 5));
        painter.setFont(font);
        painter.setPen(QColor(150, 0, 36));
        painter.drawText(circle, Qt::AlignCenter, synergy.label);
    }

    painter.restore();
}

void GameWidget::RestoreBoardUnitsAfterBattle(bool restoreEnemies) {
    for (int row = 0; row < Board::kRows; ++row) {
        for (int col = 0; col < Board::kCols; ++col) {
            shared_ptr<Unit> unit = board_.GetUnitAt(row, col);
            if (unit == nullptr) {
                continue;
            }

            if (board_.IsPlayerHalf(row) || (restoreEnemies && board_.IsOpponentHalf(row))) {
                unit->SetHp(unit->GetMaxHp());
                unit->ResetMana();
            }
        }
    }
}

void GameWidget::RefreshShopAfterBattle() {
    selectedShopSlot_ = -1;
    if (isShopFrozen_) {
        shop_.RefreshEmptySlots();
        RefreshEquipmentShop(true);
        isShopFrozen_ = false;
        return;
    }

    shop_.RefreshShop();
    RefreshEquipmentShop();
}

QJsonObject GameWidget::EquipmentToJson(const Equipment& equipment) const {
    QJsonObject object;
    object["name"] = QString::fromStdString(equipment.GetName());
    object["star"] = equipment.GetStar();
    object["price"] = equipment.GetPrice();
    object["atkBonus"] = equipment.GetAtkBonus();
    object["hpBonus"] = equipment.GetHpBonus();
    object["defenseBonus"] = equipment.GetDefenseBonus();
    object["magicResistBonus"] = equipment.GetMagicResistBonus();
    object["lifesteal"] = equipment.GetLifesteal();
    object["armorPenetration"] = equipment.GetArmorPenetration();
    object["cleaveRange"] = equipment.GetCleaveRange();
    object["cleaveDamage"] = equipment.GetCleaveDamage();
    object["reviveHpPercent"] = equipment.GetReviveHpPercent();
    object["auraDamage"] = equipment.GetAuraDamage();
    object["skillDamageBonus"] = equipment.GetSkillDamageBonus();
    object["initialManaBonus"] = equipment.GetInitialManaBonus();
    object["hpRegenPercent"] = equipment.GetHpRegenPercent();
    object["damageReflectPercent"] = equipment.GetDamageReflectPercent();
    object["doubleHit"] = equipment.GetDoubleHit();
    object["percentHpDamage"] = equipment.GetPercentHpDamage();
    object["extraManaOnHit"] = equipment.GetExtraManaOnHit();
    object["splashAttack"] = equipment.GetSplashAttack();
    object["canFuse"] = equipment.CanFuse();
    object["recipe1"] = QString::fromStdString(equipment.GetRecipe1());
    object["recipe2"] = QString::fromStdString(equipment.GetRecipe2());
    return object;
}

Equipment GameWidget::EquipmentFromJson(const QJsonObject& object) const {
    Equipment eq(
        object["name"].toString().toStdString(),
        object["price"].toInt(0),
        object["atkBonus"].toInt(0),
        object["hpBonus"].toInt(0),
        object["defenseBonus"].toInt(0),
        object["magicResistBonus"].toInt(0),
        object["star"].toInt(1));
    eq.SetLifesteal(object["lifesteal"].toInt(0));
    eq.SetArmorPenetration(object["armorPenetration"].toInt(0));
    eq.SetCleaveRange(object["cleaveRange"].toDouble(0));
    eq.SetCleaveDamage(object["cleaveDamage"].toDouble(0));
    eq.SetReviveHpPercent(object["reviveHpPercent"].toInt(0));
    eq.SetAuraDamage(object["auraDamage"].toInt(0));
    eq.SetSkillDamageBonus(object["skillDamageBonus"].toInt(0));
    eq.SetInitialManaBonus(object["initialManaBonus"].toInt(0));
    eq.SetHpRegenPercent(object["hpRegenPercent"].toInt(0));
    eq.SetDamageReflectPercent(object["damageReflectPercent"].toInt(0));
    eq.SetDoubleHit(object["doubleHit"].toBool(false));
    eq.SetPercentHpDamage(object["percentHpDamage"].toDouble(0));
    eq.SetExtraManaOnHit(object["extraManaOnHit"].toInt(0));
    eq.SetSplashAttack(object["splashAttack"].toBool(false));
    eq.SetCanFuse(object["canFuse"].toBool(true));
    eq.SetRecipe(object["recipe1"].toString().toStdString(), object["recipe2"].toString().toStdString());
    return eq;
}

void GameWidget::RefreshEquipmentShop(bool onlyEmptySlots) {
    if (equipmentShopSlots_.empty()) {
        return;
    }

    const int shopLevel = shop_.GetLevel();
    for (shared_ptr<Equipment>& equipment : equipmentShopSlots_) {
        if (onlyEmptySlots && equipment != nullptr) {
            continue;
        }
        equipment = CreateRandomEquipment(shopLevel);
    }
}

void GameWidget::ReturnUnitEquipmentsToInventory(const shared_ptr<Unit>& unit) {
    if (unit == nullptr) {
        return;
    }

    for (const Equipment& equipment : unit->TakeAllEquipments()) {
        equipmentInventory_.AddRecoveredEquipment(equipment);
    }
}

bool GameWidget::TrySynthesizeEquipment() {
    struct EquipmentRef {
        enum class Source { Inventory, Overflow, BoardUnit, BenchUnit };
        Source source;
        int first;
        int second;
        int equipmentIndex;
        Equipment equipment;
    };

    // Collect all equipment from all locations
    vector<EquipmentRef> allEquipments;

    for (int slot = 0; slot < EquipmentInventory::kSlotCount; ++slot) {
        shared_ptr<Equipment> eq = equipmentInventory_.GetEquipmentAt(slot);
        if (eq != nullptr) {
            EquipmentRef ref;
            ref.source = EquipmentRef::Source::Inventory;
            ref.first = slot;
            ref.equipment = *eq;
            allEquipments.push_back(ref);
        }
    }
    for (int index = 0; index < equipmentInventory_.GetOverflowCount(); ++index) {
        shared_ptr<Equipment> eq = equipmentInventory_.GetOverflowEquipmentAt(index);
        if (eq != nullptr) {
            EquipmentRef ref;
            ref.source = EquipmentRef::Source::Overflow;
            ref.first = index;
            ref.equipment = *eq;
            allEquipments.push_back(ref);
        }
    }
    for (int row = 0; row < Board::kRows; ++row) {
        for (int col = 0; col < Board::kCols; ++col) {
            shared_ptr<Unit> unit = board_.GetUnitAt(row, col);
            if (unit == nullptr) continue;
            const vector<Equipment>& eqs = unit->GetEquipments();
            for (int i = 0; i < static_cast<int>(eqs.size()); ++i) {
                EquipmentRef ref;
                ref.source = EquipmentRef::Source::BoardUnit;
                ref.first = row;
                ref.second = col;
                ref.equipmentIndex = i;
                ref.equipment = eqs[i];
                allEquipments.push_back(ref);
            }
        }
    }
    for (int slot = 0; slot < Bench::kSize; ++slot) {
        shared_ptr<Unit> unit = bench_.GetUnitAt(slot);
        if (unit == nullptr) continue;
        const vector<Equipment>& eqs = unit->GetEquipments();
        for (int i = 0; i < static_cast<int>(eqs.size()); ++i) {
            EquipmentRef ref;
            ref.source = EquipmentRef::Source::BenchUnit;
            ref.first = slot;
            ref.equipmentIndex = i;
            ref.equipment = eqs[i];
            allEquipments.push_back(ref);
        }
    }

    // Check each pair for recipe match
    for (size_t i = 0; i < allEquipments.size(); ++i) {
        for (size_t j = i + 1; j < allEquipments.size(); ++j) {
            const EquipmentTemplate* result = FindRecipeResult(
                allEquipments[i].equipment.GetName(),
                allEquipments[j].equipment.GetName());
            if (result == nullptr) continue;

            // Found a recipe match! Remove both source equipments
            auto removeRef = [&](const EquipmentRef& ref) {
                switch (ref.source) {
                case EquipmentRef::Source::Inventory:
                    equipmentInventory_.RemoveEquipmentAt(ref.first);
                    break;
                case EquipmentRef::Source::Overflow:
                    equipmentInventory_.RemoveOverflowEquipmentAt(ref.first);
                    break;
                case EquipmentRef::Source::BoardUnit: {
                    shared_ptr<Unit> unit = board_.GetUnitAt(ref.first, ref.second);
                    if (unit != nullptr) unit->TakeEquipment(ref.equipmentIndex);
                    break;
                }
                case EquipmentRef::Source::BenchUnit: {
                    shared_ptr<Unit> unit = bench_.GetUnitAt(ref.first);
                    if (unit != nullptr) unit->TakeEquipment(ref.equipmentIndex);
                    break;
                }
                }
            };

            removeRef(allEquipments[i]);
            removeRef(allEquipments[j]);

            // Create the result equipment and add to inventory
            Equipment synthesized = CreateEquipmentByName(result->name);
            equipmentInventory_.AddRecoveredEquipment(synthesized);

            // Try further synthesis (chain reaction)
            TrySynthesizeEquipment();
            return true;
        }
    }

    return false;
}

bool GameWidget::TryFuseEquipment() {
    struct EquipmentRef {
        enum class Source {
            Inventory,
            Overflow,
            BoardUnit,
            BenchUnit
        };

        Source source;
        int first;
        int second;
        int equipmentIndex;
        Equipment equipment;
    };

    auto collectMatches = [&](const string& name, int star) {
        vector<EquipmentRef> matches;

        for (int slot = 0; slot < EquipmentInventory::kSlotCount; ++slot) {
            shared_ptr<Equipment> equipment = equipmentInventory_.GetEquipmentAt(slot);
            if (equipment != nullptr && equipment->GetName() == name && equipment->GetStar() == star) {
                matches.push_back({EquipmentRef::Source::Inventory, slot, -1, -1, *equipment});
            }
        }

        for (int index = 0; index < equipmentInventory_.GetOverflowCount(); ++index) {
            shared_ptr<Equipment> equipment = equipmentInventory_.GetOverflowEquipmentAt(index);
            if (equipment != nullptr && equipment->GetName() == name && equipment->GetStar() == star) {
                matches.push_back({EquipmentRef::Source::Overflow, index, -1, -1, *equipment});
            }
        }

        for (int row = 0; row < Board::kRows; ++row) {
            for (int col = 0; col < Board::kCols; ++col) {
                shared_ptr<Unit> unit = board_.GetUnitAt(row, col);
                if (unit == nullptr) {
                    continue;
                }
                const vector<Equipment>& equipments = unit->GetEquipments();
                for (int index = 0; index < static_cast<int>(equipments.size()); ++index) {
                    if (equipments[index].GetName() == name && equipments[index].GetStar() == star) {
                        matches.push_back({EquipmentRef::Source::BoardUnit, row, col, index, equipments[index]});
                    }
                }
            }
        }

        for (int slot = 0; slot < Bench::kSize; ++slot) {
            shared_ptr<Unit> unit = bench_.GetUnitAt(slot);
            if (unit == nullptr) {
                continue;
            }
            const vector<Equipment>& equipments = unit->GetEquipments();
            for (int index = 0; index < static_cast<int>(equipments.size()); ++index) {
                if (equipments[index].GetName() == name && equipments[index].GetStar() == star) {
                    matches.push_back({EquipmentRef::Source::BenchUnit, slot, -1, index, equipments[index]});
                }
            }
        }

        return matches;
    };

    auto removeRef = [&](const EquipmentRef& ref) {
        switch (ref.source) {
        case EquipmentRef::Source::Inventory:
            equipmentInventory_.RemoveEquipmentAt(ref.first);
            break;
        case EquipmentRef::Source::Overflow:
            equipmentInventory_.RemoveOverflowEquipmentAt(ref.first);
            break;
        case EquipmentRef::Source::BoardUnit: {
            shared_ptr<Unit> unit = board_.GetUnitAt(ref.first, ref.second);
            if (unit != nullptr) {
                unit->TakeEquipment(ref.equipmentIndex);
            }
            break;
        }
        case EquipmentRef::Source::BenchUnit: {
            shared_ptr<Unit> unit = bench_.GetUnitAt(ref.first);
            if (unit != nullptr) {
                unit->TakeEquipment(ref.equipmentIndex);
            }
            break;
        }
        }
    };

    vector<EquipmentRef> candidates;
    for (int slot = 0; slot < EquipmentInventory::kSlotCount; ++slot) {
        shared_ptr<Equipment> equipment = equipmentInventory_.GetEquipmentAt(slot);
        if (equipment != nullptr && equipment->GetStar() < 2 && equipment->CanFuse()) {
            candidates.push_back({EquipmentRef::Source::Inventory, slot, -1, -1, *equipment});
        }
    }
    for (int index = 0; index < equipmentInventory_.GetOverflowCount(); ++index) {
        shared_ptr<Equipment> equipment = equipmentInventory_.GetOverflowEquipmentAt(index);
        if (equipment != nullptr && equipment->GetStar() < 2 && equipment->CanFuse()) {
            candidates.push_back({EquipmentRef::Source::Overflow, index, -1, -1, *equipment});
        }
    }
    for (int row = 0; row < Board::kRows; ++row) {
        for (int col = 0; col < Board::kCols; ++col) {
            shared_ptr<Unit> unit = board_.GetUnitAt(row, col);
            if (unit == nullptr) {
                continue;
            }
            const vector<Equipment>& equipments = unit->GetEquipments();
            for (int index = 0; index < static_cast<int>(equipments.size()); ++index) {
                if (equipments[index].GetStar() < 2 && equipments[index].CanFuse()) {
                    candidates.push_back({EquipmentRef::Source::BoardUnit, row, col, index, equipments[index]});
                }
            }
        }
    }
    for (int slot = 0; slot < Bench::kSize; ++slot) {
        shared_ptr<Unit> unit = bench_.GetUnitAt(slot);
        if (unit == nullptr) {
            continue;
        }
        const vector<Equipment>& equipments = unit->GetEquipments();
        for (int index = 0; index < static_cast<int>(equipments.size()); ++index) {
            if (equipments[index].GetStar() < 2 && equipments[index].CanFuse()) {
                candidates.push_back({EquipmentRef::Source::BenchUnit, slot, -1, index, equipments[index]});
            }
        }
    }

    for (const EquipmentRef& candidate : candidates) {
        vector<EquipmentRef> matches = collectMatches(candidate.equipment.GetName(), candidate.equipment.GetStar());
        if (matches.size() < 2) {
            continue;
        }

        Equipment fusedEquipment = matches[0].equipment;
        fusedEquipment.UpgradeStar();

        removeRef(matches[1]);
        removeRef(matches[0]);
        equipmentInventory_.AddRecoveredEquipment(fusedEquipment);

        TryFuseEquipment();
        return true;
    }

    return false;
}

QJsonObject GameWidget::UnitToJson(const shared_ptr<Unit>& unit) const {
    QJsonObject object;
    if (unit == nullptr) {
        object["empty"] = true;
        return object;
    }

    object["empty"] = false;
    object["name"] = QString::fromStdString(unit->GetName());
    object["profession"] = QString::fromStdString(unit->GetProfession());
    object["star"] = unit->GetStar();
    object["hp"] = unit->GetHp();
    object["maxHp"] = unit->GetBaseMaxHp();
    object["atk"] = unit->GetBaseAtk();
    object["defense"] = unit->GetBaseDefense();
    object["magicResist"] = unit->GetBaseMagicResist();
    object["mana"] = unit->GetMana();
    object["maxMana"] = unit->GetMaxMana();
    object["price"] = unit->GetPrice();
    object["sellPrice"] = unit->GetSellPrice();
    QJsonArray equipments;
    for (const Equipment& equipment : unit->GetEquipments()) {
        equipments.append(EquipmentToJson(equipment));
    }
    object["equipments"] = equipments;
    return object;
}

shared_ptr<Unit> GameWidget::UnitFromJson(const QJsonObject& object) const {
    if (object["empty"].toBool(true)) {
        return nullptr;
    }

    const string name = object["name"].toString().toStdString();
    const string profession = object["profession"].toString("战士").toStdString();
    const int star = object["star"].toInt(1);
    const int maxHp = object["maxHp"].toInt(1);
    const int atk = object["atk"].toInt(0);
    const int price = object["price"].toInt(0);
    const int hp = object["hp"].toInt(maxHp);
    const int mana = object["mana"].toInt(0);
    const int maxMana = object["maxMana"].toInt(60);
    const int sellPrice = object["sellPrice"].toInt(1);

    shared_ptr<Unit> unit = CreateUnitByName(name, star);
    if (unit == nullptr) {
        const int defense = object["defense"].toInt(0);
        const int magicResist = object["magicResist"].toInt(0);
        unit = make_shared<Unit>(name, star, maxHp, atk, defense, magicResist, nullptr, price, profession);
    }
    const int defense = object.contains("defense") ? object["defense"].toInt(unit->GetDefense()) : unit->GetDefense();
    const int magicResist = object.contains("magicResist") ? object["magicResist"].toInt(unit->GetMagicResist()) : unit->GetMagicResist();
    unit->SetHp(hp);
    unit->SetMaxHp(maxHp);
    unit->SetAtk(atk);
    unit->SetDefense(defense);
    unit->SetMagicResist(magicResist);
    unit->SetMaxMana(maxMana);
    unit->SetMana(mana);
    unit->SetSellPrice(sellPrice);
    unit->ClearEquipments();
    for (const QJsonValue& value : object["equipments"].toArray()) {
        unit->AddEquipment(EquipmentFromJson(value.toObject()));
    }
    unit->SetHp(hp);
    return unit;
}

QString GameWidget::GetSaveMetaFilePath() const {
    return QString("src/save/save_names.json");
}

QString GameWidget::GetDefaultSaveSlotName(int saveSlot) const {
    const QStringList numberText = {
        QString::fromUtf8("一"),
        QString::fromUtf8("二"),
        QString::fromUtf8("三"),
        QString::fromUtf8("四")
    };
    if (saveSlot < 0 || saveSlot >= numberText.size()) {
        return QString::fromUtf8("存档");
    }
    return QString::fromUtf8("存档") + numberText[saveSlot];
}

QString GameWidget::GetSaveSlotName(int saveSlot) const {
    if (saveSlot < 0 || saveSlot >= static_cast<int>(saveSlotNames_.size()) || saveSlotNames_[saveSlot].trimmed().isEmpty()) {
        return GetDefaultSaveSlotName(saveSlot);
    }
    return saveSlotNames_[saveSlot];
}

QString GameWidget::GetSaveSlotButtonText(int saveSlot) const {
    const QString name = GetSaveSlotName(saveSlot);
    if (screenState_ == ScreenState::NewSelect) {
        return QString::fromUtf8("重置“%1”").arg(name);
    }
    return name;
}

QString GameWidget::GetCurrentSaveSlotName() const {
    if (currentSaveSlot_ < 0 || currentSaveSlot_ >= kSaveSlotCount) {
        return QString::fromUtf8("快速游戏");
    }
    return GetSaveSlotName(currentSaveSlot_);
}

void GameWidget::LoadSaveSlotNames() {
    saveSlotNames_.assign(kSaveSlotCount, QString());

    QFile file(GetSaveMetaFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }

    const QJsonArray names = document.object()["names"].toArray();
    for (int slot = 0; slot < kSaveSlotCount && slot < names.size(); ++slot) {
        const QString name = names[slot].toString().trimmed();
        if (!name.isEmpty()) {
            saveSlotNames_[slot] = name;
        }
    }
}

bool GameWidget::SaveSaveSlotNames() const {
    QDir().mkpath("src/save");
    QFile file(GetSaveMetaFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    QJsonArray names;
    for (int slot = 0; slot < kSaveSlotCount; ++slot) {
        names.append(GetSaveSlotName(slot));
    }

    QJsonObject root;
    root["names"] = names;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool GameWidget::ResetSaveSlot(int saveSlot) {
    StartNewGame(saveSlot, true);
    if (SaveGameToSlot(saveSlot)) {
        hasUnsavedChanges_ = false;
        return true;
    }

    hasUnsavedChanges_ = true;
    QMessageBox::warning(this, QString::fromUtf8("重置失败"), QString::fromUtf8("写入初始存档失败"));
    return false;
}

void GameWidget::ShowSaveSlotContextMenu(int saveSlot) {
    QMenu menu(this);
    QAction* resetAction = menu.addAction(QString::fromUtf8("重置存档"));
    QAction* renameAction = menu.addAction(QString::fromUtf8("改名"));
    QAction* selectedAction = menu.exec(QCursor::pos());

    if (selectedAction == resetAction) {
        ResetSaveSlot(saveSlot);
        update();
    } else if (selectedAction == renameAction) {
        RenameSaveSlot(saveSlot);
    }
}

void GameWidget::RenameSaveSlot(int saveSlot) {
    constexpr int kMaxSaveNameLength = 8;
    QString currentName = GetSaveSlotName(saveSlot);

    while (true) {
        QInputDialog dialog(this);
        dialog.setWindowTitle(QString::fromUtf8("改名"));
        dialog.setLabelText(QString::fromUtf8("请输入存档名（最多 8 个字符）"));
        dialog.setInputMode(QInputDialog::TextInput);
        dialog.setTextValue(currentName);
        if (dialog.exec() != QDialog::Accepted) {
            return;
        }

        const QString newName = dialog.textValue().trimmed();
        if (newName.isEmpty()) {
            QMessageBox::warning(this, QString::fromUtf8("改名失败"), QString::fromUtf8("存档名不能为空"));
            continue;
        }
        if (newName.size() > kMaxSaveNameLength) {
            QMessageBox::warning(this, QString::fromUtf8("改名失败"), QString::fromUtf8("存档名最多 8 个字符，太长会显示不下"));
            currentName = newName.left(kMaxSaveNameLength);
            continue;
        }

        saveSlotNames_[saveSlot] = newName;
        if (!SaveSaveSlotNames()) {
            QMessageBox::warning(this, QString::fromUtf8("改名失败"), QString::fromUtf8("写入存档名称失败"));
            return;
        }
        update();
        return;
    }
}

QString GameWidget::GetSaveFilePath(int saveSlot) const {
    return QString("src/save/save%1.json").arg(saveSlot + 1);
}

bool GameWidget::SaveGame() const {
    if (!canSaveCurrentGame_ || currentSaveSlot_ < 0) {
        return false;
    }
    return SaveGameToSlot(currentSaveSlot_);
}

bool GameWidget::SaveGameToSlot(int saveSlot) const {
    try {
        if (saveSlot < 0 || saveSlot >= kSaveSlotCount) {
            return false;
        }

        QDir().mkpath("src/save");
        QFile file(GetSaveFilePath(saveSlot));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }

        QJsonObject root;
        QJsonObject player;
        player["hp"] = testPlayer_.GetHp();
        player["gold"] = testPlayer_.GetGold();
        player["level"] = testPlayer_.GetLevel();
        player["populationLimit"] = testPlayer_.GetPopulationLimit();
        player["deployUpgradeCost"] = deployUpgradeCost_;
        player["deployUpgradeDiscount"] = deployUpgradeDiscount_;
        root["player"] = player;

        QJsonObject progress;
        progress["currentEnemyRound"] = currentEnemyRound_;
        progress["isGameWon"] = isGameWon_;
        progress["isGameOver"] = isGameOver_;
        progress["difficulty"] = static_cast<int>(difficulty_);
        progress["winStreak"] = winStreak_;
        root["progress"] = progress;

        QJsonObject shop;
        shop["level"] = shop_.GetLevel();
        shop["maxLevel"] = shop_.GetMaxLevel();
        shop["sellableUnitCount"] = shop_.GetSellableUnitCount();
        shop["refreshCost"] = shop_.GetRefreshCost();
        shop["upgradeCost"] = shop_.GetUpgradeCost();
        shop["discount"] = shop_.GetDiscount();
        shop["isFrozen"] = isShopFrozen_;
        QJsonArray shopUnits;
        for (int slot = 0; slot < shop_.GetSellableUnitCount(); ++slot) {
            shopUnits.append(UnitToJson(shop_.GetUnitAt(slot)));
        }
        shop["units"] = shopUnits;
        QJsonArray equipments;
        for (const shared_ptr<Equipment>& equipment : equipmentShopSlots_) {
            if (equipment == nullptr) {
                QJsonObject emptyEquipment;
                emptyEquipment["empty"] = true;
                equipments.append(emptyEquipment);
            } else {
                QJsonObject equipmentObject = EquipmentToJson(*equipment);
                equipmentObject["empty"] = false;
                equipments.append(equipmentObject);
            }
        }
        shop["equipments"] = equipments;
        root["shop"] = shop;

        QJsonObject inventory;
        QJsonArray inventorySlots;
        for (int slot = 0; slot < EquipmentInventory::kSlotCount; ++slot) {
            shared_ptr<Equipment> equipment = equipmentInventory_.GetEquipmentAt(slot);
            if (equipment == nullptr) {
                QJsonObject emptyEquipment;
                emptyEquipment["empty"] = true;
                inventorySlots.append(emptyEquipment);
            } else {
                QJsonObject equipmentObject = EquipmentToJson(*equipment);
                equipmentObject["empty"] = false;
                inventorySlots.append(equipmentObject);
            }
        }
        QJsonArray overflowEquipments;
        for (int i = 0; i < equipmentInventory_.GetOverflowCount(); ++i) {
            shared_ptr<Equipment> equipment = equipmentInventory_.GetOverflowEquipmentAt(i);
            if (equipment != nullptr) {
                QJsonObject equipmentObject = EquipmentToJson(*equipment);
                equipmentObject["empty"] = false;
                overflowEquipments.append(equipmentObject);
            }
        }
        inventory["slots"] = inventorySlots;
        inventory["overflow"] = overflowEquipments;
        root["equipmentInventory"] = inventory;

        QJsonArray boardUnits;
        for (int row = 0; row < Board::kRows; ++row) {
            for (int col = 0; col < Board::kCols; ++col) {
                shared_ptr<Unit> unit = board_.GetUnitAt(row, col);
                if (unit == nullptr) {
                    continue;
                }
                QJsonObject entry = UnitToJson(unit);
                entry["row"] = row;
                entry["col"] = col;
                boardUnits.append(entry);
            }
        }
        root["board"] = boardUnits;

        QJsonArray benchUnits;
        for (int slot = 0; slot < Bench::kSize; ++slot) {
            shared_ptr<Unit> unit = bench_.GetUnitAt(slot);
            if (unit == nullptr) {
                continue;
            }
            QJsonObject entry = UnitToJson(unit);
            entry["slot"] = slot;
            benchUnits.append(entry);
        }
        root["bench"] = benchUnits;

        QJsonDocument document(root);
        if (file.write(document.toJson(QJsonDocument::Indented)) <= 0) {
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        qWarning() << "SaveGameToSlot exception:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "SaveGameToSlot unknown exception";
        return false;
    }
}

bool GameWidget::LoadGame() {
    if (currentSaveSlot_ < 0) {
        return false;
    }
    return LoadGameFromSlot(currentSaveSlot_);
}

bool GameWidget::LoadGameFromSlot(int saveSlot) {
    try {
        if (saveSlot < 0 || saveSlot >= kSaveSlotCount) {
            return false;
        }

        QFile file(GetSaveFilePath(saveSlot));
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            return false;
        }

        if (isBattleActive_) {
            battleTimer_->stop();
            combatUnits_.clear();
            visualEffects_.clear();
            isBattleActive_ = false;
        }
        ResetUnitInteraction();

        QJsonObject root = document.object();
        QJsonObject player = root["player"].toObject();
        testPlayer_.SetHp(player["hp"].toInt(20));
        testPlayer_.SetGold(player["gold"].toInt(10));
        testPlayer_.SetLevel(player["level"].toInt(1));
        testPlayer_.SetPopulationLimit(player["populationLimit"].toInt(3));
        deployUpgradeCost_ = player["deployUpgradeCost"].toInt(4);
        deployUpgradeDiscount_ = player["deployUpgradeDiscount"].toInt(0);

        QJsonObject progress = root["progress"].toObject();
        currentEnemyRound_ = progress["currentEnemyRound"].toInt(0);
        isGameWon_ = progress["isGameWon"].toBool(false);
        isGameOver_ = progress["isGameOver"].toBool(false);
        difficulty_ = static_cast<Difficulty>(progress["difficulty"].toInt(static_cast<int>(Difficulty::Normal)));
        winStreak_ = progress["winStreak"].toInt(0);

        QJsonObject shop = root["shop"].toObject();
        const int shopLevel = shop["level"].toInt(1);
        shop_.ClearUnitPool();
        shop_.SetMaxLevel(shop["maxLevel"].toInt(2));
        shop_.SetLevel(shopLevel);
        shop_.SetSellableUnitCount(shop["sellableUnitCount"].toInt(shopLevel >= 2 ? 5 : 3));
        shop_.SetRefreshCost(shop["refreshCost"].toInt(1));
        shop_.SetUpgradeCost(shop["upgradeCost"].toInt(4));
        shop_.ResetDiscount();
        if (shop.contains("discount")) {
            for (int d = 0; d < shop["discount"].toInt(); ++d) shop_.ApplyDiscount();
        }
        AddBaseUnitsToPool(shop_);
        if (shop_.GetLevel() >= 2) {
            AddAdvancedUnitsToPool(shop_);
        }
        shop_.ClearCurrentUnits();
        isShopFrozen_ = shop["isFrozen"].toBool(false);
        QJsonArray shopUnits = shop["units"].toArray();
        for (int slot = 0; slot < shop_.GetSellableUnitCount() && slot < shopUnits.size(); ++slot) {
            shop_.SetCurrentUnitAt(slot, UnitFromJson(shopUnits[slot].toObject()));
        }
        equipmentShopSlots_.assign(kWeaponShopSlotCount, nullptr);
        QJsonArray equipmentSlots = shop["equipments"].toArray();
        for (int slot = 0; slot < kWeaponShopSlotCount && slot < equipmentSlots.size(); ++slot) {
            QJsonObject entry = equipmentSlots[slot].toObject();
            if (!entry["empty"].toBool(false)) {
                equipmentShopSlots_[slot] = make_shared<Equipment>(EquipmentFromJson(entry));
            }
        }
        if (equipmentSlots.isEmpty()) {
            RefreshEquipmentShop();
        }

        equipmentInventory_.Clear();
        QJsonObject inventory = root["equipmentInventory"].toObject();
        QJsonArray inventorySlots = inventory["slots"].toArray();
        for (int slot = 0; slot < EquipmentInventory::kSlotCount && slot < inventorySlots.size(); ++slot) {
            QJsonObject entry = inventorySlots[slot].toObject();
            if (!entry["empty"].toBool(false)) {
                equipmentInventory_.SetEquipmentAt(slot, make_shared<Equipment>(EquipmentFromJson(entry)));
            }
        }
        QJsonArray overflowEquipments = inventory["overflow"].toArray();
        for (const QJsonValue& value : overflowEquipments) {
            QJsonObject entry = value.toObject();
            if (!entry["empty"].toBool(false)) {
                equipmentInventory_.AddOverflowEquipment(make_shared<Equipment>(EquipmentFromJson(entry)));
            }
        }

        board_.Clear();
        QJsonArray boardUnits = root["board"].toArray();
        for (const QJsonValue& value : boardUnits) {
            QJsonObject entry = value.toObject();
            const int row = entry["row"].toInt(-1);
            const int col = entry["col"].toInt(-1);
            shared_ptr<Unit> unit = UnitFromJson(entry);
            if (unit != nullptr && board_.IsValidPosition(row, col)) {
                board_.PlaceUnitAt(row, col, unit);
            }
        }

        bench_.Clear();
        QJsonArray benchUnits = root["bench"].toArray();
        for (const QJsonValue& value : benchUnits) {
            QJsonObject entry = value.toObject();
            const int slot = entry["slot"].toInt(-1);
            shared_ptr<Unit> unit = UnitFromJson(entry);
            if (unit != nullptr && bench_.IsValidSlot(slot)) {
                bench_.PlaceUnit(slot, unit);
            }
        }

        currentSaveSlot_ = saveSlot;
        canSaveCurrentGame_ = true;
        hasUnsavedChanges_ = false;
        screenState_ = ScreenState::Playing;
        update();
        return true;
    } catch (const std::exception& e) {
        qWarning() << "LoadGameFromSlot exception:" << e.what();
        return false;
    } catch (...) {
        qWarning() << "LoadGameFromSlot unknown exception";
        return false;
    }
}

void GameWidget::StartBattle() {
    if (isBattleActive_) {
        return;
    }

    if (isGameOver_) {
        return;
    }

    if (isGameWon_) {
        QMessageBox::information(this, QString::fromUtf8("游戏胜利"), QString::fromUtf8("所有预设敌人都已被击败"));
        return;
    }

    combatUnits_.clear();
    visualEffects_.clear();
    ResetUnitInteraction();

    bool hasPlayerUnit = false;
    bool hasEnemyUnit = false;
    for (int row = 0; row < Board::kRows; ++row) {
        for (int col = 0; col < Board::kCols; ++col) {
            shared_ptr<Unit> unit = board_.GetUnitAt(row, col);
            if (unit == nullptr || !unit->IsAlive()) {
                continue;
            }

            unit->ResetMana();

            const bool isEnemy = board_.IsOpponentHalf(row);
            hasEnemyUnit = hasEnemyUnit || isEnemy;
            hasPlayerUnit = hasPlayerUnit || !isEnemy;
            combatUnits_.push_back({
                .unit = unit,
                .position = QPointF(col + 0.5, row + 0.5),
                .isEnemy = isEnemy,
                .boardRow = row,
                .boardCol = col,
                .attackRange = unit->GetAttackRange(),
                .moveSpeed = unit->GetMoveSpeed(),
                .attackSpeed = unit->GetBaseAttackSpeed(),
                .attackCooldown = 1.0 / max(0.1, unit->GetBaseAttackSpeed()),
                .attackTimer = 0.15,
                .state = CombatState::Idle,
                .damageTakenMultiplier = 1.0,
                .baseDamageTakenMultiplier = 1.0,
                .critRate = unit->GetCritRate(),
                .critDamage = unit->GetCritDamage()
            });

            // Apply equipment effects to the combat unit
            CombatUnit& cu = combatUnits_.back();
            for (const Equipment& eq : unit->GetEquipments()) {
                cu.equipLifesteal += eq.GetLifesteal();
                cu.equipArmorPenetration = max(cu.equipArmorPenetration, eq.GetArmorPenetration());
                if (eq.GetCleaveRange() > 0) {
                    cu.equipCleaveRange = eq.GetCleaveRange();
                    cu.equipCleaveDamage = eq.GetCleaveDamage();
                }
                cu.equipReviveHpPercent = max(cu.equipReviveHpPercent, eq.GetReviveHpPercent());
                cu.equipAuraDamage += eq.GetAuraDamage();
                cu.equipSkillDamageBonus += eq.GetSkillDamageBonus();
                cu.equipHpRegenPercent += eq.GetHpRegenPercent();
                cu.equipDamageReflectPercent += eq.GetDamageReflectPercent();
                if (eq.GetDoubleHit()) cu.equipDoubleHit = true;
                cu.equipPercentHpDamage = max(cu.equipPercentHpDamage, eq.GetPercentHpDamage());
                cu.equipExtraManaOnHit += eq.GetExtraManaOnHit();
                if (eq.GetSplashAttack()) cu.equipSplashAttack = true;
                cu.hasRevived = false;

                // Apply initial mana bonus
                if (eq.GetInitialManaBonus() > 0) {
                    unit->AddMana(eq.GetInitialManaBonus());
                }
            }
        }
    }

    if (!hasPlayerUnit) {
        QMessageBox::information(this, QString::fromUtf8("无法开始战斗"), QString::fromUtf8("请先把至少一个单位拖到下半场"));
        combatUnits_.clear();
        return;
    }

    if (!hasEnemyUnit) {
        QMessageBox::information(this, QString::fromUtf8("无法开始战斗"), QString::fromUtf8("上半场没有敌人"));
        combatUnits_.clear();
        return;
    }

    ApplySynergyBonuses();
    isBattleActive_ = true;
    battleCountdownTimer_ = kBattleTimeLimit;
    battleTimeExpired_ = false;
    battleTimer_->start(kBattleTickMs);
    update();
}

void GameWidget::UpdateBattle() {
    if (!isBattleActive_) {
        return;
    }

    bool playerAlive = false;
    bool enemyAlive = false;
    for (auto& combatUnit : combatUnits_) {
        if (combatUnit.unit != nullptr && combatUnit.unit->IsAlive() && combatUnit.state != CombatState::Dead) {
            playerAlive = playerAlive || !combatUnit.isEnemy;
            enemyAlive = enemyAlive || combatUnit.isEnemy;
        }
    }

    if (!playerAlive || !enemyAlive) {
        FinishBattle(playerAlive && !enemyAlive);
        return;
    }

    // Update battle countdown timer
    battleCountdownTimer_ -= kBattleTickSeconds;
    if (battleCountdownTimer_ <= 0 && !battleTimeExpired_) {
        battleTimeExpired_ = true;
        // Apply damage from surviving enemies (use normal damage logic)
        const DifficultyConfig cfg = GetDifficultyConfig(difficulty_);
        const int round = currentEnemyRound_ + 1;
        const DamagePhase* phase = nullptr;
        for (const DamagePhase& p : cfg.damagePhases) {
            if (round >= p.roundStart && round <= p.roundEnd) {
                phase = &p;
                break;
            }
        }
        int totalDamage = phase ? phase->baseDamage : 2;
        if (phase) {
            for (const CombatUnit& cu : combatUnits_) {
                if (cu.isEnemy && cu.unit && cu.unit->IsAlive()) {
                    const int star = cu.unit->GetStar();
                    if (star >= 3) totalDamage += phase->star3Damage;
                    else if (star == 2) totalDamage += phase->star2Damage;
                    else totalDamage += phase->star1Damage;
                }
            }
        }
        testPlayer_.SetHp(testPlayer_.GetHp() - totalDamage);
        FinishBattle(false);
        return;
    }

    // Tick down visual effects
    for (auto it = visualEffects_.begin(); it != visualEffects_.end(); ) {
        it->timer += kBattleTickSeconds;
        if (it->timer >= it->duration) {
            it = visualEffects_.erase(it);
        } else {
            ++it;
        }
    }

    for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
        CombatUnit& cu = combatUnits_[i];
        if (cu.unit == nullptr || !cu.unit->IsAlive() || cu.state == CombatState::Dead) {
            cu.state = CombatState::Dead;
            continue;
        }

        // === Tick timers ===

        // Untargetable
        if (cu.untargetableTimer > 0) {
            cu.untargetableTimer -= kBattleTickSeconds;
        }

        // Dmg reduction
        if (cu.hasDmgReduction) {
            cu.dmgReductionTimer -= kBattleTickSeconds;
            if (cu.dmgReductionTimer <= 0) {
                cu.hasDmgReduction = false;
                cu.damageTakenMultiplier = cu.baseDamageTakenMultiplier;
            }
        }

        // Buff timer
        if (cu.buffTimer > 0.0) {
            cu.buffTimer -= kBattleTickSeconds;
            if (cu.buffTimer <= 0.0) {
                cu.bonusAtk = 0;
                cu.attackSpeed = cu.unit->GetBaseAttackSpeed();
                cu.lifestealRatio = 0;
                cu.buffTimer = 0.0;
            }
        }

        // Debuff timer
        if (cu.debuffTimer > 0) {
            cu.debuffTimer -= kBattleTickSeconds;
            if (cu.debuffTimer <= 0) {
                cu.atkDebuffRatio = 0;
                cu.defenseDebuff = 0;
                cu.moveSpeedDebuff = 0;
            }
        }

        // Barrier aura (Mudrock)
        if (cu.barrierAuraTimer > 0) {
            cu.barrierAuraTimer -= kBattleTickSeconds;
            if (cu.barrierSelfHealRatio > 0) {
                cu.unit->Heal(static_cast<int>(cu.unit->GetMaxHp() * cu.barrierSelfHealRatio * kBattleTickSeconds));
            }
            if (cu.barrierAuraTimer <= 0) {
                cu.barrierSelfHealRatio = 0;
                cu.teamDmgReductionRatio = 0;
            }
        }

        // Persist aura (Skadi — doesn't use attack action)
        if (cu.hasPersistAura) {
            cu.persistAuraTimer -= kBattleTickSeconds;
            if (cu.persistAuraTimer > 0 && cu.persistManaRegenBuff > 0) {
                cu.unit->AddMana(static_cast<int>(cu.persistManaRegenBuff * kBattleTickSeconds));
            }
            if (cu.persistAuraTimer <= 0) {
                cu.hasPersistAura = false;
            }
        }

        // Natural mana regen (3/s default)
        cu.unit->AddMana(static_cast<int>(cu.unit->GetManaRegenPerSecond() * kBattleTickSeconds));

        // Equipment: HP regen per tick
        if (cu.equipHpRegenPercent > 0) {
            cu.unit->Heal(static_cast<int>(cu.unit->GetMaxHp() * cu.equipHpRegenPercent / 100.0 * kBattleTickSeconds));
        }

        // Equipment: Aura damage per tick around the unit
        if (cu.equipAuraDamage > 0) {
            for (int j = 0; j < static_cast<int>(combatUnits_.size()); ++j) {
                if (i == j || combatUnits_[j].isEnemy == cu.isEnemy) continue;
                if (!combatUnits_[j].unit->IsAlive()) continue;
                const double dist = Distance(cu.position, combatUnits_[j].position);
                if (dist <= 1.0) {
                    DealDamage(combatUnits_[j], static_cast<int>(cu.equipAuraDamage * kBattleTickSeconds), Unit::DamageType::Magical);
                }
            }
        }

        // Skip the rest for persist aura that doesn't need to attack
        if (cu.hasPersistAura && cu.unit->GetProfession() == "辅助") {
            continue;
        }

        // If mana is full, cast skill
        if (cu.unit->IsManaFull()) {
            CastSkill(i);
            continue;
        }

        // Support units: attack to heal; others: attack to deal damage
        const bool isSupport = cu.unit->GetProfession() == "辅助";
        const bool isAssassin = cu.unit->GetProfession() == "刺客";

        // Check if assassin synergy is active (need 2+ assassins)
        int assassinCount = 0;
        if (isAssassin) {
            for (const CombatUnit& checkUnit : combatUnits_) {
                if (!checkUnit.isEnemy && checkUnit.unit != nullptr &&
                    checkUnit.unit->IsAlive() && checkUnit.unit->GetProfession() == "刺客") {
                    assassinCount++;
                }
            }
        }
        const bool assassinSynergyActive = assassinCount >= 2;

        cu.attackTimer = std::max(0.0, cu.attackTimer - kBattleTickSeconds);

        int targetIndex = -1;
        if (isSupport) {
            // Support: find friendliest unit with lowest HP ratio
            int lowestRatioIdx = -1;
            double lowestRatio = 1.0;
            for (int j = 0; j < static_cast<int>(combatUnits_.size()); ++j) {
                if (combatUnits_[j].isEnemy != cu.isEnemy) continue;
                if (!combatUnits_[j].unit->IsAlive()) continue;
                double hpRatio = static_cast<double>(combatUnits_[j].unit->GetHp()) / combatUnits_[j].unit->GetMaxHp();
                if (hpRatio < lowestRatio) {
                    lowestRatio = hpRatio;
                    lowestRatioIdx = j;
                }
            }
            targetIndex = lowestRatioIdx;
        } else if (isAssassin && assassinSynergyActive) {
            // Assassin with synergy: target lowest max HP enemy
            targetIndex = FindLowestMaxHpEnemyIndex(i);
        } else {
            targetIndex = FindNearestEnemyIndex(i);
        }

        if (targetIndex == -1) {
            cu.state = CombatState::Idle;
            continue;
        }

        CombatUnit& target = combatUnits_[targetIndex];
        const double distance = Distance(cu.position, target.position);
        if (distance <= cu.attackRange) {
            cu.state = CombatState::Attacking;
            if (cu.attackTimer <= 0.0) {
                if (isSupport) {
                    // Support attack: heal target by ATK * 10 HP
                    int healAmount = static_cast<int>(cu.unit->GetAtk() * 2);
                    target.unit->Heal(healAmount);
                } else {
                    // Normal attack: deal damage
                    int atkValue = GetCombatAttack(cu);
                    if (cu.atkDebuffRatio > 0) {
                        atkValue = static_cast<int>(atkValue * (1.0 - cu.atkDebuffRatio));
                    }

                    int hitCount = 1;
                    double hitRatio = 1.0;
                    if (cu.multiHitCount > 0) {
                        hitCount = cu.multiHitCount;
                        hitRatio = cu.multiHitRatio;
                        cu.multiHitCount = 0;
                    }

                    if (cu.nextAttackRatio > 0) {
                        hitRatio = cu.nextAttackRatio;
                        if (cu.nextAttackGuaranteedCrit) {
                            cu.critRate = 100;
                        }
                        cu.nextAttackRatio = 0;
                        cu.nextAttackGuaranteedCrit = false;
                    }

                    for (int h = 0; h < hitCount; ++h) {
                        int damage = static_cast<int>(atkValue * hitRatio);

                        // Equipment: double hit — attack twice
                        if (cu.equipDoubleHit && h == 0) {
                            hitCount++;
                        }

                        bool crit = false;
                        if (cu.critRate > 0 && QRandomGenerator::global()->bounded(100) < cu.critRate) {
                            damage = static_cast<int>(damage * cu.critDamage);
                            crit = true;
                        }

                        // Equipment: armor penetration
                        if (cu.equipArmorPenetration > 0) {
                            int originalBaseDef = target.unit->GetBaseDefense();
                            int eqDefBonus = target.unit->GetDefense() - originalBaseDef;
                            int totalDef = originalBaseDef + eqDefBonus;
                            int reducedDef = static_cast<int>(totalDef * (100 - cu.equipArmorPenetration) / 100);
                            // Set baseDefense_ so that GetDefense() (which adds equipment bonus) == reducedDef
                            int targetBaseDef = reducedDef - eqDefBonus;
                            target.unit->SetDefense(max(0, targetBaseDef));
                            DealDamage(target, damage, cu.unit->GetDamageType());
                            target.unit->SetDefense(originalBaseDef);
                        } else {
                            DealDamage(target, damage, cu.unit->GetDamageType());
                        }

                        // Equipment: percent HP damage (额外造成目标生命值百分比伤害)
                        if (cu.equipPercentHpDamage > 0 && target.unit->IsAlive()) {
                            int percentDmg = static_cast<int>(target.unit->GetMaxHp() * cu.equipPercentHpDamage);
                            DealDamage(target, percentDmg, Unit::DamageType::Magical);
                        }

                        // Equipment: extra mana on hit
                        if (cu.equipExtraManaOnHit > 0) {
                            cu.unit->AddMana(cu.equipExtraManaOnHit);
                        }

                        if (target.reflectDamage > 0 && cu.unit->IsAlive()) {
                            DealDamage(cu, static_cast<int>(target.reflectDamage), Unit::DamageType::Magical);
                        }

                        // Equipment: damage reflect
                        if (cu.equipDamageReflectPercent > 0 && target.unit->IsAlive()) {
                            int reflectDmg = damage * cu.equipDamageReflectPercent / 100;
                            DealDamage(target, reflectDmg, Unit::DamageType::Physical);
                        }

                        // Equipment: cleave (splash damage to nearby enemies)
                        if (cu.equipCleaveRange > 0 && cu.equipCleaveDamage > 0) {
                            for (int j = 0; j < static_cast<int>(combatUnits_.size()); ++j) {
                                if (j == targetIndex || combatUnits_[j].isEnemy != target.isEnemy) continue;
                                if (!combatUnits_[j].unit->IsAlive()) continue;
                                const double dist = Distance(target.position, combatUnits_[j].position);
                                if (dist <= cu.equipCleaveRange) {
                                    int cleaveDmg = static_cast<int>(damage * cu.equipCleaveDamage);
                                    DealDamage(combatUnits_[j], cleaveDmg, Unit::DamageType::Physical);
                                }
                            }
                        }

                        // Equipment: splash attack (普攻变为周围1格AOE)
                        if (cu.equipSplashAttack) {
                            for (int j = 0; j < static_cast<int>(combatUnits_.size()); ++j) {
                                if (j == targetIndex || combatUnits_[j].isEnemy != target.isEnemy) continue;
                                if (!combatUnits_[j].unit->IsAlive()) continue;
                                const double dist = Distance(target.position, combatUnits_[j].position);
                                if (dist <= 1.0) {
                                    DealDamage(combatUnits_[j], damage, Unit::DamageType::Magical);
                                }
                            }
                        }
                    }

                    // Lifesteal (from skill + equipment)
                    double totalLifesteal = cu.lifestealRatio;
                    if (cu.equipLifesteal > 0) {
                        totalLifesteal += cu.equipLifesteal / 100.0;
                    }
                    if (totalLifesteal > 0) {
                        cu.unit->Heal(static_cast<int>(atkValue * totalLifesteal));
                    }

                    // Assassin synergy: reset skill and full mana when killing enemy
                    if (isAssassin && assassinSynergyActive && !target.unit->IsAlive()) {
                        cu.unit->SetMana(cu.unit->GetMaxMana());
                    }

                    if (!target.unit->IsAlive()) {
                        target.state = CombatState::Dead;
                    }
                }

                // Mana per attack
                cu.unit->AddMana(cu.unit->GetManaPerAttack());

                cu.attackTimer = cu.attackCooldown;
                cu.attackCooldown = 1.0 / max(0.1, cu.attackSpeed);
            }
        } else {
            cu.state = CombatState::Moving;
            const QPointF direction = Normalized(target.position - cu.position);
            double currentMoveSpeed = cu.moveSpeed;
            if (cu.moveSpeedDebuff > 0) {
                currentMoveSpeed *= (1.0 - cu.moveSpeedDebuff);
            }
            const double maxStep = currentMoveSpeed * kBattleTickSeconds;
            const double step = std::min(maxStep, std::max(0.0, distance - cu.attackRange * 0.9));
            cu.position += direction * step;
        }
    }

    ResolveCombatSeparation();
    update();
}

void GameWidget::FinishBattle(bool playerWon) {
    battleTimer_->stop();

    combatUnits_.clear();
    visualEffects_.clear();
    isBattleActive_ = false;
    battleCountdownTimer_ = kBattleTimeLimit;
    battleTimeExpired_ = false;
    RefreshShopAfterBattle();

    const DifficultyConfig cfg = GetDifficultyConfig(difficulty_);

    // Base income + battle reward
    int totalIncome = 4 + (playerWon ? 3 : 1);

    // Interest: 1 gold per 10 gold
    int interest = testPlayer_.GetGold() / 10;
    totalIncome += interest;

    // Win streak bonus: 1 gold per consecutive win
    if (playerWon) {
        ++winStreak_;
        totalIncome += winStreak_;
    } else {
        winStreak_ = 0;
    }

    testPlayer_.AddGold(totalIncome);

    // Apply discounts (not upgraded this round → next round cheaper)
    shop_.ApplyDiscount();
    if (deployUpgradeDiscount_ < (8 + 1) / 2) {
        ++deployUpgradeDiscount_;
    }

    if (playerWon) {
        RestoreBoardUnitsAfterBattle(false);
        if (currentEnemyRound_ + 1 >= cfg.totalRounds) {
            isGameWon_ = true;
            isGameOver_ = true;
            ClearEnemyUnits();
            QMessageBox::information(this, QString::fromUtf8("游戏胜利"), QString::fromUtf8("所有预设敌人都已被击败，游戏结束"));
        } else {
            ++currentEnemyRound_;
            SpawnTestEnemies();
            int baseIncome = 4 + 3;
            int incomeBreakdown = baseIncome + interest + winStreak_;
            QString msg = QString::fromUtf8("胜利！获得 %1 金币\n基础:%2 + 利息:%3 + 连胜:%4\n下一轮敌人已出现").arg(totalIncome).arg(baseIncome).arg(interest).arg(winStreak_);
            QMessageBox::information(this, QString::fromUtf8("战斗结束"), msg);
        }
    } else {
        RestoreBoardUnitsAfterBattle(true);
        // Calculate damage: find damage phase for this round
        const int round = currentEnemyRound_ + 1;
        const DamagePhase* phase = nullptr;
        for (const DamagePhase& p : cfg.damagePhases) {
            if (round >= p.roundStart && round <= p.roundEnd) {
                phase = &p;
                break;
            }
        }
        int totalDamage = phase ? phase->baseDamage : 2;
        if (phase) {
            for (const CombatUnit& cu : combatUnits_) {
                if (cu.isEnemy && cu.unit && cu.unit->IsAlive()) {
                    const int star = cu.unit->GetStar();
                    if (star >= 3) totalDamage += phase->star3Damage;
                    else if (star == 2) totalDamage += phase->star2Damage;
                    else totalDamage += phase->star1Damage;
                }
            }
        }
        testPlayer_.SetHp(testPlayer_.GetHp() - totalDamage);
        if (testPlayer_.GetHp() <= 0) {
            isGameOver_ = true;
            QMessageBox::information(this, QString::fromUtf8("游戏失败"), QString::fromUtf8("生命值已归零，游戏结束"));
        } else {
            QMessageBox::information(this, QString::fromUtf8("战斗结束"),
                QString::fromUtf8("失败！获得 %1 金币，扣除 %2 生命值").arg(totalIncome).arg(totalDamage));
        }
    }

    MarkGameChanged();
    update();
}

int GameWidget::FindNearestEnemyIndex(int sourceIndex) const {
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(combatUnits_.size())) {
        return -1;
    }

    const CombatUnit& source = combatUnits_[sourceIndex];
    int bestIndex = -1;
    double bestDistance = std::numeric_limits<double>::max();
    for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
        const CombatUnit& candidate = combatUnits_[i];
        if (i == sourceIndex || candidate.isEnemy == source.isEnemy || candidate.unit == nullptr || !candidate.unit->IsAlive()) {
            continue;
        }

        const double distance = Distance(source.position, candidate.position);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

int GameWidget::FindLowestMaxHpEnemyIndex(int sourceIndex) const {
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(combatUnits_.size())) {
        return -1;
    }

    const CombatUnit& source = combatUnits_[sourceIndex];
    int bestIndex = -1;
    int lowestMaxHp = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
        const CombatUnit& candidate = combatUnits_[i];
        if (i == sourceIndex || candidate.isEnemy == source.isEnemy || candidate.unit == nullptr || !candidate.unit->IsAlive()) {
            continue;
        }

        int maxHp = candidate.unit->GetMaxHp();
        if (maxHp < lowestMaxHp) {
            lowestMaxHp = maxHp;
            bestIndex = i;
        }
    }

    return bestIndex;
}

int GameWidget::GetCombatAttack(const CombatUnit& combatUnit) const {
    if (combatUnit.unit == nullptr) {
        return 0;
    }
    return combatUnit.unit->GetAtk() + combatUnit.bonusAtk + combatUnit.synergyBonusAtk;
}


void GameWidget::DealDamage(CombatUnit& target, int damage, Unit::DamageType damageType) {
    if (target.unit == nullptr || damage <= 0) {
        return;
    }

    // Apply temporary defense modifiers (synergy buff + debuff) before damage,
    // then restore original defense after — preventing permanent stat changes from base defense (not GetDefense()
    // which includes equipment bonuses) to prevent permanently baking equipment
    // defense into baseDefense_.
    const int originalBaseDef = target.unit->GetBaseDefense();
    int effectiveDef = originalBaseDef + target.buffDefense - target.defenseDebuff;
    effectiveDef = max(0, effectiveDef);
    target.unit->SetDefense(effectiveDef);

    int remainingDamage = damage;

    // Barrier absorbs damage first
    if (target.barrier > 0) {
        if (remainingDamage <= target.barrier) {
            target.barrier -= remainingDamage;
            target.unit->SetDefense(originalBaseDef);
            return;  // All absorbed
        }
        remainingDamage -= target.barrier;
        target.barrier = 0;
    }

    const int finalDamage = std::max(1, static_cast<int>(remainingDamage * target.damageTakenMultiplier));
    target.unit->TakeDamage(finalDamage, damageType);

    target.unit->SetDefense(originalBaseDef);

    // Equipment: revive (M3茧甲) — trigger when unit dies
    if (!target.unit->IsAlive() && target.equipReviveHpPercent > 0 && !target.hasRevived) {
        target.hasRevived = true;
        int reviveHp = target.unit->GetMaxHp() * target.equipReviveHpPercent / 100;
        target.unit->SetHp(reviveHp);
        target.state = CombatState::Idle;
    }

    // Mana on hit (damage received)
    int manaOnHit = max(1, finalDamage / 100);
    manaOnHit = min(manaOnHit, 15);
    target.unit->AddMana(static_cast<int>(manaOnHit * target.unit->GetManaOnHitMultiplier()));

    if (!target.unit->IsAlive()) {
        target.state = CombatState::Dead;
    }
}

void GameWidget::ApplySynergyBonuses() {
    const vector<SynergyStatus> synergies = GetVisibleSynergies();
    for (const SynergyStatus& synergy : synergies) {
        if (synergy.count < synergy.required) continue;

        // Find the definition for this synergy
        const SynergyDefinition* def = nullptr;
        for (const SynergyDefinition& d : GetSynergyDefinitions()) {
            if (d.id == synergy.id) {
                def = &d;
                break;
            }
        }
        if (def == nullptr) continue;

        const bool isTier4 = (def->required4 > 0 && synergy.count >= def->required4);

        // Helper to check if a profession is in this synergy
        auto matchesProf = [&](const string& prof) -> bool {
            for (const char* p : def->professions) if (p == prof) return true;
            return false;
        };

        if (synergy.id == "steel") {
            // 钢铁战线: 全队防御+15 (base), 战士/坦克受伤-20%, 每秒回2%HP (tier4)
            for (CombatUnit& cu : combatUnits_) {
                if (cu.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
                cu.buffDefense += 15;
            }
            if (isTier4) {
                for (CombatUnit& cu : combatUnits_) {
                    if (cu.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
                    if (matchesProf(cu.unit->GetProfession())) {
                        cu.damageTakenMultiplier *= 0.8;
                        cu.baseDamageTakenMultiplier = cu.damageTakenMultiplier;
                    }
                }
            }
        } else if (synergy.id == "elemental") {
            // 元素火力: 全队ATK+15% (base), 技能伤害+30%, 攻速+25% (tier4)
            for (CombatUnit& cu : combatUnits_) {
                if (cu.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
                cu.synergyBonusAtk += static_cast<int>(cu.unit->GetAtk() * 0.15);
            }
            if (isTier4) {
                for (CombatUnit& cu : combatUnits_) {
                    if (cu.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
                    if (matchesProf(cu.unit->GetProfession())) {
                        cu.attackSpeed *= 1.25;
                    }
                }
            }
        } else if (synergy.id == "assassin") {
            // 刺客羁绊：索敌改变（优先锁定最低生命值上限敌人）、杀死敌人后刷新技能
            // 机制在 UpdateBattle 和 DealDamage 后处理，此处无需额外属性加成
        } else if (synergy.id == "support") {
            // 辅助: 全队回蓝+25%, 受治疗效果+25% (heal bonus handled in HealAlly skill)
            // 回蓝加成: 战斗中自然回蓝乘以1.25
            // handled in UpdateBattle
            for (CombatUnit& cu : combatUnits_) {
                if (cu.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
                // Store as a flag: units with this synergy active get 25% more mana regen
                // We'll just directly increase natural regen
            }
        }
    }

    }

void GameWidget::CastSkill(int sourceIndex) {
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(combatUnits_.size())) {
        return;
    }

    CombatUnit& caster = combatUnits_[sourceIndex];
    if (caster.unit == nullptr || !caster.unit->IsAlive()) {
        return;
    }

    caster.state = CombatState::Casting;
    const Unit::SkillParams params = caster.unit->GetSkillParams();
    caster.unit->ResetMana();

    // Equipment: skill damage bonus multiplier
    const double skillDmgMult = 1.0 + caster.equipSkillDamageBonus / 100.0;

    // Map SkillParams::Effect to VisualEffect::Type
    auto spawnEffect = [&](const QPointF& targetPos = QPointF()) {
        VisualEffect::Type vfxType;
        switch (params.effect) {
        case Unit::SkillParams::Effect::Shockwave:    vfxType = VisualEffect::Type::Shockwave; break;
        case Unit::SkillParams::Effect::HealCross:    vfxType = VisualEffect::Type::HealCross; break;
        case Unit::SkillParams::Effect::DaggerStrike: vfxType = VisualEffect::Type::DaggerStrike; break;
        default: return;
        }
        SpawnVisualEffect(vfxType, caster.position, params.effectDuration, targetPos);
    };

    switch (params.type) {
    case Unit::SkillParams::Type::AoE:
        spawnEffect();
        for (auto& target : combatUnits_) {
            if (target.isEnemy == caster.isEnemy || target.unit == nullptr || !target.unit->IsAlive()) {
                continue;
            }
            if (Distance(caster.position, target.position) <= params.aoeRange) {
                DealDamage(target, static_cast<int>(GetCombatAttack(caster) * params.ratio * skillDmgMult), params.damageType);
            }
        }
        break;

    case Unit::SkillParams::Type::Cleave: {
        spawnEffect();
        // Attack all enemies in attack range
        for (auto& target : combatUnits_) {
            if (target.isEnemy == caster.isEnemy || target.unit == nullptr || !target.unit->IsAlive()) {
                continue;
            }
            if (Distance(caster.position, target.position) <= caster.attackRange) {
                DealDamage(target, static_cast<int>(GetCombatAttack(caster) * params.ratio * skillDmgMult), params.damageType);
            }
        }
        break;
    }

    case Unit::SkillParams::Type::HealSelf:
        spawnEffect();
        caster.unit->Heal(static_cast<int>(caster.unit->GetMaxHp() * params.healRatio));
        break;

    case Unit::SkillParams::Type::AtkBuff:
        spawnEffect();
        caster.bonusAtk = static_cast<int>(caster.unit->GetAtk() * params.atkBuffRatio);
        caster.buffTimer = params.duration;
        if (params.atkSpeedBuffRatio > 0) {
            caster.attackSpeed *= (1.0 + params.atkSpeedBuffRatio);
        }
        if (params.lifestealOnAttackRatio > 0) {
            caster.lifestealRatio = params.lifestealOnAttackRatio;
        }
        break;

    case Unit::SkillParams::Type::Nuke: {
        const int targetIndex = FindNearestEnemyIndex(sourceIndex);
        if (targetIndex != -1) {
            CombatUnit& target = combatUnits_[targetIndex];
            spawnEffect(target.position);
            DealDamage(target, static_cast<int>(GetCombatAttack(caster) * params.ratio * skillDmgMult), params.damageType);
        }
        break;
    }

    case Unit::SkillParams::Type::MultiHit:
        spawnEffect();
        // Queue multi-hit on this unit's next attacks
        caster.multiHitCount = params.hitCount;
        caster.multiHitRatio = params.ratio;
        break;

    case Unit::SkillParams::Type::DamageReduction:
        spawnEffect();
        caster.damageTakenMultiplier *= (1.0 - params.dmgReductionPct);
        caster.dmgReductionTimer = params.duration;
        caster.hasDmgReduction = true;
        break;

    case Unit::SkillParams::Type::Barrier:
        spawnEffect();
        caster.barrier += params.barrierFlat;
        caster.reflectDamage = params.reflectDamage;
        caster.reflectAoeRange = params.reflectAoeRange;
        break;

    case Unit::SkillParams::Type::BarrierAura:
        spawnEffect();
        caster.barrier += static_cast<int>(caster.unit->GetMaxHp() * params.barrierMaxHpRatio);
        caster.barrierSelfHealRatio = params.selfHealOnTickRatio;
        caster.teamDmgReductionRatio = params.teamDmgReductionRatio;
        caster.barrierAuraTimer = params.duration;
        break;

    case Unit::SkillParams::Type::TeamBuff:
        spawnEffect();
        for (auto& cu : combatUnits_) {
            if (cu.isEnemy == caster.isEnemy && cu.unit != nullptr && cu.unit->IsAlive()) {
                cu.synergyBonusAtk += static_cast<int>(cu.unit->GetAtk() * params.atkBuffRatio);
                // Defense buff handled in combat update
                cu.buffDefense += params.defenseBuffFlat;
            }
        }
        break;

    case Unit::SkillParams::Type::MultiTargetNuke: {
        // Find target with highest ATK, preferring mage/support
        int bestTarget = -1;
        double highestAtk = -1;
        for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
            auto& cu = combatUnits_[i];
            if (cu.isEnemy == caster.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
            double atkVal = GetCombatAttack(cu);
            if (params.preferMageSupport && (cu.unit->GetProfession() == "法师" || cu.unit->GetProfession() == "辅助")) {
                atkVal *= 2.0;  // Weighted preference
            }
            if (atkVal > highestAtk) {
                highestAtk = atkVal;
                bestTarget = i;
            }
        }
        if (bestTarget != -1) {
            auto& target = combatUnits_[bestTarget];
            spawnEffect(target.position);
            int dmg = static_cast<int>(GetCombatAttack(caster) * params.ratio * skillDmgMult);
            if (params.ignoreDefenseRatio > 0) {
                // Partially ignore defense
                int effectiveDefense = static_cast<int>(target.unit->GetDefense() * (1.0 - params.ignoreDefenseRatio));
                dmg = max(1, dmg - effectiveDefense);
            }
            DealDamage(target, dmg, params.damageType);
        }
        break;
    }

    case Unit::SkillParams::Type::DebuffEnemies:
        spawnEffect();
        for (auto& cu : combatUnits_) {
            if (cu.isEnemy != caster.isEnemy && cu.unit != nullptr && cu.unit->IsAlive()) {
                cu.atkDebuffRatio = params.atkDebuffRatio;
                cu.defenseDebuff = params.defenseDebuffFlat;
                cu.moveSpeedDebuff = params.moveSpeedDebuffRatio;
                cu.debuffTimer = params.duration;
            }
        }
        break;

    case Unit::SkillParams::Type::Bounce: {
        // Bounce to nearest enemies in sequence
        spawnEffect();
        vector<int> hitTargets;
        int currentTarget = FindNearestEnemyIndex(sourceIndex);
        for (int b = 0; b < params.hitCount && currentTarget != -1; ++b) {
            if (currentTarget == -1) break;
            auto& target = combatUnits_[currentTarget];
            DealDamage(target, static_cast<int>(GetCombatAttack(caster) * params.ratio * skillDmgMult), params.damageType);
            hitTargets.push_back(currentTarget);
            // Find next nearest that hasn't been hit
            double nearestDist = 1e9;
            int nearestIdx = -1;
            for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
                if (combatUnits_[i].isEnemy == caster.isEnemy || !combatUnits_[i].unit->IsAlive()) continue;
                bool alreadyHit = false;
                for (int h : hitTargets) { if (h == i) { alreadyHit = true; break; } }
                if (alreadyHit) continue;
                double d = Distance(combatUnits_[currentTarget].position, combatUnits_[i].position);
                if (d < nearestDist) { nearestDist = d; nearestIdx = i; }
            }
            currentTarget = nearestIdx;
        }
        break;
    }

    case Unit::SkillParams::Type::HealAlly: {
        // Heal lowest HP ally: target max HP × healRatio
        int lowestHpAlly = -1;
        double minHpRatio = 1.0;
        for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
            auto& cu = combatUnits_[i];
            if (cu.isEnemy != caster.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
            double ratio = static_cast<double>(cu.unit->GetHp()) / cu.unit->GetMaxHp();
            if (ratio < minHpRatio) {
                minHpRatio = ratio;
                lowestHpAlly = i;
            }
        }
        // Try caster itself if no ally found
        if (lowestHpAlly == -1) {
            int healAmount = static_cast<int>(caster.unit->GetMaxHp() * params.healRatio);
            caster.unit->Heal(healAmount);
        } else {
            int healAmount = static_cast<int>(combatUnits_[lowestHpAlly].unit->GetMaxHp() * params.healRatio);
            combatUnits_[lowestHpAlly].unit->Heal(healAmount);
        }
        spawnEffect();
        break;
    }

    case Unit::SkillParams::Type::TeleportNuke: {
        // Teleport to lowest HP enemy behind them
        int lowestHpTarget = -1;
        double minHp = 1e9;
        for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
            auto& cu = combatUnits_[i];
            if (cu.isEnemy == caster.isEnemy || cu.unit == nullptr || !cu.unit->IsAlive()) continue;
            if (cu.unit->GetHp() < minHp) {
                minHp = cu.unit->GetHp();
                lowestHpTarget = i;
            }
        }
        if (lowestHpTarget != -1) {
            auto& target = combatUnits_[lowestHpTarget];
            // Teleport behind target
            QPointF dir = Normalized(target.position - caster.position);
            if (dir.x() == 0 && dir.y() == 0) dir = QPointF(0, -1);
            caster.position = target.position - dir * 0.5;
            spawnEffect(target.position);
            DealDamage(target, static_cast<int>(GetCombatAttack(caster) * params.ratio * skillDmgMult), params.damageType);
        }
        break;
    }

    case Unit::SkillParams::Type::NextAttackBuff:
        spawnEffect();
        caster.nextAttackRatio = params.nextAtkRatio;
        caster.nextAttackGuaranteedCrit = params.nextAtkGuaranteedCrit;
        break;

    case Unit::SkillParams::Type::PersistAura:
        spawnEffect();
        caster.hasPersistAura = true;
        caster.persistAuraTimer = params.duration;
        caster.persistAtkSpeedBuff = params.atkSpeedBuffRatio;
        caster.persistManaRegenBuff = params.manaRegenBuffRatio;
        break;

    default:
        break;
    }
}

void GameWidget::SpawnVisualEffect(VisualEffect::Type type, const QPointF& position, double duration, const QPointF& targetPosition) {
    visualEffects_.push_back({type, position, 0.0, duration, targetPosition});
}

bool GameWidget::TryFusion(shared_ptr<Unit>& fusedUnit) {
    // Scan both bench and player's board for 3 matching name+star units.
    // Collect all candidate units with their locations.
    struct UnitRef {
        bool onBoard;   // false = on bench
        int row;        // board row (if onBoard)
        int col;        // board col (if onBoard)
        int slot;       // bench slot (if !onBoard)
        shared_ptr<Unit> unit;
    };

    auto collectNameStar = [&](const string& name, int star) -> vector<UnitRef> {
        vector<UnitRef> result;

        // Bench
        for (int s = 0; s < Bench::kSize; ++s) {
            shared_ptr<Unit> u = bench_.GetUnitAt(s);
            if (u && u->GetName() == name && u->GetStar() == star) {
                result.push_back({false, -1, -1, s, u});
            }
        }

        // Player's board
        for (int r = Board::kRows / 2; r < Board::kRows; ++r) {
            for (int c = 0; c < Board::kCols; ++c) {
                shared_ptr<Unit> u = board_.GetUnitAt(r, c);
                if (u && u->GetName() == name && u->GetStar() == star) {
                    result.push_back({true, r, c, -1, u});
                }
            }
        }

        return result;
    };

    for (int s = 0; s < Bench::kSize; ++s) {
        shared_ptr<Unit> candidate = bench_.GetUnitAt(s);
        if (candidate == nullptr || candidate->GetStar() >= 3) continue;

        const string& name = candidate->GetName();
        const int star = candidate->GetStar();
        vector<UnitRef> matches = collectNameStar(name, star);

        if (matches.size() < 3) continue;

        // Use first 3 matches; keep one as the survivor (prefer bench unit).
        // Remove the other 2 from their locations.
        matches[0].unit->UpgradeStar();

        // Remove the 2 units we're sacrificing
        for (int idx = 1; idx <= 2; ++idx) {
            ReturnUnitEquipmentsToInventory(matches[idx].unit);
            if (matches[idx].onBoard) {
                board_.RemoveUnit(matches[idx].row, matches[idx].col);
            } else {
                bench_.RemoveUnit(matches[idx].slot);
            }
        }

        // Place the survivor back on bench at the rightmost empty slot
        // (it may have come from board or bench, but we always put result on bench)
        int rightmostEmpty = -1;
        for (int bs = Bench::kSize - 1; bs >= 0; --bs) {
            if (bench_.IsEmpty(bs)) {
                rightmostEmpty = bs;
                break;
            }
        }
        if (rightmostEmpty != -1) {
            // Remove from current location (if it was on board)
            if (matches[0].onBoard) {
                board_.RemoveUnit(matches[0].row, matches[0].col);
            } else {
                bench_.RemoveUnit(matches[0].slot);
            }
            bench_.PlaceUnit(rightmostEmpty, matches[0].unit);
        }

        fusedUnit = matches[0].unit;

        // Chain fusion: check if the upgraded unit can fuse further
        shared_ptr<Unit> nextFusion;
        if (TryFusion(nextFusion)) {
            fusedUnit = nextFusion;
        }
        return true;
    }
    return false;
}

shared_ptr<Unit> GameWidget::CreateFusionRewardUnit() const {
    const int rewardLevel = min(shop_.GetLevel() + 1, shop_.GetMaxLevel());
    Shop rewardShop(rewardLevel, shop_.GetMaxLevel(), 1);

    AddBaseUnitsToPool(rewardShop);
    if (rewardLevel >= 2) {
        AddAdvancedUnitsToPool(rewardShop);
    }

    rewardShop.RefreshShop();
    return rewardShop.BuyUnit(0);
}

void GameWidget::DrawVisualEffects(QPainter& painter, int cellSize) {
    for (const VisualEffect& effect : visualEffects_) {
        const double progress = effect.timer / effect.duration;

        if (effect.type == VisualEffect::Type::Shockwave) {
            // Expanding ring that fades out
            const double radius = 0.3 + progress * 1.8;
            const int pixelRadius = static_cast<int>(radius * cellSize);
            const QPointF screenPos = GetBoardCellRect(0, 0).topLeft()
                + QPointF(effect.position.x() * cellSize, effect.position.y() * cellSize);

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(Qt::NoBrush);

            // Outer ring
            int alpha = static_cast<int>(180 * (1.0 - progress));
            painter.setPen(QPen(QColor(255, 200, 50, alpha), 4));
            painter.drawEllipse(screenPos, pixelRadius, pixelRadius);

            // Inner glow ring
            int innerAlpha = static_cast<int>(120 * (1.0 - progress * 0.7));
            painter.setPen(QPen(QColor(255, 255, 200, innerAlpha), 2));
            painter.drawEllipse(screenPos, pixelRadius / 2, pixelRadius / 2);
            painter.restore();
        } else if (effect.type == VisualEffect::Type::HealCross) {
            // Green cross that pulses and rises
            const QPointF screenPos = GetBoardCellRect(0, 0).topLeft()
                + QPointF(effect.position.x() * cellSize, (effect.position.y() - 0.3 * progress) * cellSize);
            const int crossSize = cellSize / 3;

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);

            int alpha = static_cast<int>(255 * (1.0 - progress * 0.5));
            int pulse = static_cast<int>(crossSize * (1.0 + progress * 0.8));

            painter.setPen(QPen(QColor(100, 255, 120, alpha), 5));
            painter.drawLine(QPointF(screenPos.x() - pulse, screenPos.y()),
                             QPointF(screenPos.x() + pulse, screenPos.y()));
            painter.drawLine(QPointF(screenPos.x(), screenPos.y() - pulse),
                             QPointF(screenPos.x(), screenPos.y() + pulse));

            // Outer glow cross
            painter.setPen(QPen(QColor(180, 255, 190, alpha / 2), 3));
            painter.drawLine(QPointF(screenPos.x() - pulse * 1.3, screenPos.y()),
                             QPointF(screenPos.x() + pulse * 1.3, screenPos.y()));
            painter.drawLine(QPointF(screenPos.x(), screenPos.y() - pulse * 1.3),
                             QPointF(screenPos.x(), screenPos.y() + pulse * 1.3));
            painter.restore();
        } else if (effect.type == VisualEffect::Type::DaggerStrike) {
            // Fast moving slash line from caster to target
            const QPointF startPos = GetBoardCellRect(0, 0).topLeft()
                + QPointF(effect.position.x() * cellSize, effect.position.y() * cellSize);
            const QPointF endPos = GetBoardCellRect(0, 0).topLeft()
                + QPointF(effect.targetPosition.x() * cellSize, effect.targetPosition.y() * cellSize);

            const double travel = std::min(1.0, progress * 3.0);
            QPointF midPos = startPos + (endPos - startPos) * travel;

            painter.save();
            painter.setRenderHint(QPainter::Antialiasing, true);

            int alpha = static_cast<int>(255 * (1.0 - progress * 0.6));
            // Slash line
            painter.setPen(QPen(QColor(200, 60, 60, alpha), 5));
            painter.drawLine(startPos, midPos);

            // Impact burst at end point
            if (progress > 0.3) {
                double burstProgress = (progress - 0.3) / 0.7;
                int burstRadius = static_cast<int>(cellSize * 0.4 * burstProgress);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen(QColor(255, 100, 100, static_cast<int>(180 * (1.0 - burstProgress))), 3));
                painter.drawEllipse(endPos, burstRadius, burstRadius);
            }
            painter.restore();
        }
    }
}

void GameWidget::DrawBuffIndicator(QPainter& painter, const QRect& cell, const CombatUnit& combatUnit) {
    // Draw a glowing aura around archers with active ATK buff
    if (combatUnit.buffTimer > 0.0) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        double pulseIntensity = 0.5 + 0.5 * std::sin(combatUnit.buffTimer * 8.0);
        int alpha = static_cast<int>(80 + 60 * pulseIntensity);

        QPen auraPen(QColor(255, 220, 80, alpha), 5);
        painter.setPen(auraPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(cell.adjusted(-4, -4, 4, 4), 6, 6);

        // Sparkle dots at corners
        for (int dx = -1; dx <= 1; dx += 2) {
            for (int dy = -1; dy <= 1; dy += 2) {
                QPointF corner(cell.center().x() + dx * cell.width() / 2,
                               cell.center().y() + dy * cell.height() / 2);
                int dotSize = static_cast<int>(3 + 3 * pulseIntensity);
                painter.setBrush(QColor(255, 240, 150, alpha));
                painter.setPen(Qt::NoPen);
                painter.drawEllipse(corner, dotSize, dotSize);
            }
        }

        painter.restore();
    }
}

void GameWidget::ResolveCombatSeparation() {
    for (int i = 0; i < static_cast<int>(combatUnits_.size()); ++i) {
        CombatUnit& first = combatUnits_[i];
        if (first.unit == nullptr || !first.unit->IsAlive()) {
            continue;
        }

        for (int j = i + 1; j < static_cast<int>(combatUnits_.size()); ++j) {
            CombatUnit& second = combatUnits_[j];
            if (second.unit == nullptr || !second.unit->IsAlive()) {
                continue;
            }

            QPointF diff = first.position - second.position;
            double distance = Distance(first.position, second.position);
            if (distance >= kMinUnitDistance) {
                continue;
            }

            if (distance <= 0.0001) {
                diff = QPointF(1.0, 0.0);
                distance = 1.0;
            }

            const QPointF direction = Normalized(diff);
            const double correction = (kMinUnitDistance - distance) * 0.5 * kSeparationStrength;
            first.position += direction * correction;
            second.position -= direction * correction;
            first.position.setX(std::max(0.3, std::min(7.7, first.position.x())));
            first.position.setY(std::max(0.3, std::min(7.7, first.position.y())));
            second.position.setX(std::max(0.3, std::min(7.7, second.position.x())));
            second.position.setY(std::max(0.3, std::min(7.7, second.position.y())));
        }
    }
}

void GameWidget::BeginUnitInteraction(const shared_ptr<Unit>& unit, const QPoint& position, bool sourceIsBench, int benchSlot, int boardRow, int boardCol) {
    if (unit == nullptr) {
        return;
    }

    hasPendingUnitClick_ = true;
    isDraggingUnit_ = false;
    dragSourceIsBench_ = sourceIsBench;
    pressedBenchSlot_ = benchSlot;
    pressedBoardRow_ = boardRow;
    pressedBoardCol_ = boardCol;
    dragHoverRow_ = -1;
    dragHoverCol_ = -1;
    dragHoverBenchSlot_ = -1;
    dragStartPos_ = position;
    dragPosition_ = position;
    draggingUnit_ = unit;
    selectedBenchSlot_ = sourceIsBench ? benchSlot : -1;
    selectedBoardRow_ = sourceIsBench ? -1 : boardRow;
    selectedBoardCol_ = sourceIsBench ? -1 : boardCol;
    update();
}

void GameWidget::ResetUnitInteraction() {
    hasPendingUnitClick_ = false;
    isDraggingUnit_ = false;
    dragSourceIsBench_ = false;
    pressedBenchSlot_ = -1;
    pressedBoardRow_ = -1;
    pressedBoardCol_ = -1;
    dragHoverRow_ = -1;
    dragHoverCol_ = -1;
    dragHoverBenchSlot_ = -1;
    draggingUnit_ = nullptr;
    selectedBenchSlot_ = -1;
    selectedBoardRow_ = -1;
    selectedBoardCol_ = -1;
}

void GameWidget::BeginEquipmentInteraction(const shared_ptr<Equipment>& equipment, const QPoint& position, int shopSlot) {
    ResetUnitInteraction();
    hasPendingEquipmentClick_ = true;
    isDraggingEquipment_ = false;
    pressedEquipmentShopSlot_ = shopSlot;
    pressedEquipmentInventorySlot_ = -1;
    pressedEquipmentOverflowIndex_ = -1;
    dragHoverRow_ = -1;
    dragHoverCol_ = -1;
    dragHoverBenchSlot_ = -1;
    dragStartPos_ = position;
    dragPosition_ = position;
    draggingEquipment_ = equipment;
    selectedEquipmentShopSlot_ = shopSlot;
    update();
}

void GameWidget::ResetEquipmentInteraction() {
    hasPendingEquipmentClick_ = false;
    isDraggingEquipment_ = false;
    pressedEquipmentShopSlot_ = -1;
    pressedEquipmentInventorySlot_ = -1;
    pressedEquipmentOverflowIndex_ = -1;
    draggingEquipment_ = nullptr;
    selectedEquipmentShopSlot_ = -1;
    selectedEquipmentInventorySlot_ = -1;
    selectedEquipmentOverflowIndex_ = -1;
}

void GameWidget::HandleDraggedUnitRelease() {
    if (dragSourceIsBench_) {
        if (dragHoverBenchSlot_ != -1) {
            MoveOrSwapBenchUnitToBench();
        } else if (dragHoverRow_ != -1 && dragHoverCol_ != -1) {
            MoveOrSwapBenchUnitToBoard();
        }
    } else {
        if (dragHoverBenchSlot_ != -1) {
            MoveOrSwapBoardUnitToBench();
        } else if (dragHoverRow_ != -1 && dragHoverCol_ != -1) {
            MoveOrSwapBoardUnitToBoard();
        }
    }
}

bool GameWidget::HandleDraggedEquipmentRelease() {
    if (draggingEquipment_ == nullptr) {
        return false;
    }

    shared_ptr<Unit> targetUnit = nullptr;
    if (dragHoverBenchSlot_ != -1) {
        targetUnit = bench_.GetUnitAt(dragHoverBenchSlot_);
    } else if (dragHoverRow_ != -1 && dragHoverCol_ != -1) {
        targetUnit = board_.GetUnitAt(dragHoverRow_, dragHoverCol_);
    }

    if (targetUnit == nullptr) {
        return false;
    }

    if (!targetUnit->AddEquipment(*draggingEquipment_)) {
        QMessageBox::warning(this, QString::fromUtf8("装备失败"), QString::fromUtf8("该单位的装备槽已满。"));
        return false;
    }

    if (pressedEquipmentInventorySlot_ != -1) {
        equipmentInventory_.RemoveEquipmentAt(pressedEquipmentInventorySlot_);
    } else if (pressedEquipmentOverflowIndex_ != -1) {
        equipmentInventory_.RemoveOverflowEquipmentAt(pressedEquipmentOverflowIndex_);
    } else {
        return false;
    }

    selectedEquipmentShopSlot_ = -1;
    return true;
}

void GameWidget::MoveOrSwapBenchUnitToBoard() {
    if (pressedBenchSlot_ == -1 || dragHoverRow_ == -1 || dragHoverCol_ == -1) {
        return;
    }

    if (board_.IsEmpty(dragHoverRow_, dragHoverCol_)) {
        if (GetPlayerBoardUnitCount() >= testPlayer_.GetPopulationLimit()) {
            QMessageBox::warning(this, QString::fromUtf8("上场失败"), QString::fromUtf8("场上人数已达上限"));
            return;
        }

        shared_ptr<Unit> movedUnit = bench_.RemoveUnitAndShiftRight(pressedBenchSlot_);
        if (movedUnit != nullptr) {
            board_.PlaceUnitAt(dragHoverRow_, dragHoverCol_, movedUnit);
        }
        return;
    }

    SwapBoardAndBenchUnits(dragHoverRow_, dragHoverCol_, pressedBenchSlot_);
}

void GameWidget::MoveOrSwapBoardUnitToBoard() {
    if (pressedBoardRow_ == -1 || pressedBoardCol_ == -1 || dragHoverRow_ == -1 || dragHoverCol_ == -1) {
        return;
    }

    if (pressedBoardRow_ == dragHoverRow_ && pressedBoardCol_ == dragHoverCol_) {
        return;
    }

    if (board_.IsEmpty(dragHoverRow_, dragHoverCol_)) {
        board_.MoveUnit(pressedBoardRow_, pressedBoardCol_, dragHoverRow_, dragHoverCol_);
    } else {
        board_.SwapUnits(pressedBoardRow_, pressedBoardCol_, dragHoverRow_, dragHoverCol_);
    }
}

void GameWidget::MoveOrSwapBenchUnitToBench() {
    if (pressedBenchSlot_ == -1 || dragHoverBenchSlot_ == -1 || pressedBenchSlot_ == dragHoverBenchSlot_) {
        return;
    }

    if (bench_.IsEmpty(dragHoverBenchSlot_)) {
        bench_.MoveUnit(pressedBenchSlot_, dragHoverBenchSlot_);
    } else {
        bench_.SwapUnits(pressedBenchSlot_, dragHoverBenchSlot_);
    }
}

void GameWidget::MoveOrSwapBoardUnitToBench() {
    if (pressedBoardRow_ == -1 || pressedBoardCol_ == -1 || dragHoverBenchSlot_ == -1) {
        return;
    }

    if (bench_.IsEmpty(dragHoverBenchSlot_)) {
        shared_ptr<Unit> movedUnit = board_.RemoveUnit(pressedBoardRow_, pressedBoardCol_);
        if (movedUnit != nullptr) {
            bench_.PlaceUnit(dragHoverBenchSlot_, movedUnit);
        }
    } else {
        SwapBoardAndBenchUnits(pressedBoardRow_, pressedBoardCol_, dragHoverBenchSlot_);
    }
}

void GameWidget::SwapBoardAndBenchUnits(int boardRow, int boardCol, int benchSlot) {
    shared_ptr<Unit> boardUnit = board_.RemoveUnit(boardRow, boardCol);
    shared_ptr<Unit> benchUnit = bench_.RemoveUnit(benchSlot);

    if (boardUnit != nullptr) {
        bench_.PlaceUnit(benchSlot, boardUnit);
    }

    if (benchUnit != nullptr) {
        board_.PlaceUnitAt(boardRow, boardCol, benchUnit);
    }
}

void GameWidget::HandleRefreshButton() {
    const int cost = shop_.GetRefreshCost();
    if (testPlayer_.GetGold() < cost) {
        QMessageBox::warning(this, QString::fromUtf8("刷新失败"), QString::fromUtf8("金币不足，刷新失败"));
        return;
    }

    if (testPlayer_.SpendGold(cost)) {
        selectedShopSlot_ = -1;
        selectedEquipmentShopSlot_ = -1;
        isShopFrozen_ = false;
        shop_.RefreshShop();
        RefreshEquipmentShop();
        MarkGameChanged();
        update();
    }
}

void GameWidget::HandleFreezeButton() {
    isShopFrozen_ = !isShopFrozen_;
    MarkGameChanged();
    update();
}

void GameWidget::HandleUpgradeButton() {
    if (shop_.GetLevel() >= shop_.GetMaxLevel()) {
        return;
    }

    const int cost = shop_.GetCurrentUpgradeCost();
    if (testPlayer_.GetGold() < cost) {
        QMessageBox::warning(this, QString::fromUtf8("升级失败"), QString::fromUtf8("金币不足，升级失败"));
        return;
    }

    if (!shop_.UpgradeLevel()) {
        QMessageBox::information(this, QString::fromUtf8("升级失败"), QString::fromUtf8("商店已达到最高等级"));
        return;
    }

    testPlayer_.SpendGold(cost);
    shop_.ResetDiscount();

    // 升级后重建单位池：清空旧池，重新添加基础+对应等级的单位
    shop_.ClearUnitPool();
    AddBaseUnitsToPool(shop_);
    AddAdvancedUnitsToPool(shop_);
    shop_.SetSellableUnitCount(5);
    shop_.RefreshShop();

    isShopFrozen_ = false;
    MarkGameChanged();
    update();
}

void GameWidget::HandleDeployUpgradeButton() {
    if (testPlayer_.GetPopulationLimit() >= kMaxPopulationLimit) {
        return;
    }

    // Fixed ladder: 4/6/8 based on current population limit
    const int pop = testPlayer_.GetPopulationLimit();
    int baseCost = 4;
    if (pop >= 3) baseCost = 8;
    else if (pop >= 2) baseCost = 6;
    const int cost = max((baseCost + 1) / 2, baseCost - deployUpgradeDiscount_);

    if (testPlayer_.GetGold() < cost) {
        QMessageBox::warning(this, QString::fromUtf8("升级失败"), QString::fromUtf8("金币不足，升级失败"));
        return;
    }

    testPlayer_.SpendGold(cost);
    testPlayer_.SetPopulationLimit(pop + 1);
    deployUpgradeDiscount_ = 0;
    deployUpgradeCost_ = (pop + 1 >= 4) ? 8 : ((pop + 1 >= 3) ? 6 : 4);
    MarkGameChanged();
    update();
}

void GameWidget::HandleSaveButton() {
    if (!canSaveCurrentGame_) {
        QMessageBox::information(this, QString::fromUtf8("无法存档"), QString::fromUtf8("快速游戏不能存档"));
        return;
    }

    if (SaveGame()) {
        hasUnsavedChanges_ = false;
        QMessageBox::information(this, QString::fromUtf8("存档成功"), QString::fromUtf8("当前游戏已保存"));
    } else {
        QMessageBox::warning(this, QString::fromUtf8("存档失败"), QString::fromUtf8("写入存档文件失败"));
    }
}

void GameWidget::HandleGameBackButton() {
    if (!canSaveCurrentGame_) {
        screenState_ = ScreenState::MainMenu;
        update();
        return;
    }

    if (hasUnsavedChanges_) {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(QString::fromUtf8("返回"));
        messageBox.setText(QString::fromUtf8("当前游戏尚未存档，是否先存档？"));
        QPushButton* yesButton = messageBox.addButton(QString::fromUtf8("是"), QMessageBox::YesRole);
        QPushButton* noButton = messageBox.addButton(QString::fromUtf8("否"), QMessageBox::NoRole);
        messageBox.exec();

        if (messageBox.clickedButton() == yesButton) {
            if (!SaveGame()) {
                QMessageBox::warning(this, QString::fromUtf8("存档失败"), QString::fromUtf8("写入存档文件失败"));
                return;
            }
            hasUnsavedChanges_ = false;
        } else if (messageBox.clickedButton() != noButton) {
            return;
        }
    }

    screenState_ = ScreenState::LoadSelect;
    update();
}

void GameWidget::SelectAndShowUnitDetail(const shared_ptr<Unit>& unit, int& selectedSlot, int slot, bool canBuy, int shopSlot) {
    if (unit == nullptr) {
        return;
    }

    selectedSlot = slot;
    update();
    ShowUnitDetailDialog(unit, canBuy, shopSlot);
    selectedSlot = -1;
    update();
}

void GameWidget::ShowSynthesisRecipeDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("合成配方"));
    dialog.setMinimumSize(400, 380);
    dialog.setStyleSheet(
        "QDialog { background-color: #2d3140; border-radius: 12px; }"
        "QLabel#Title { color: #f5f7fb; font-size: 18px; font-weight: 700; }"
        "QLabel#Sub { color: #b9c4d8; font-size: 13px; font-weight: 600; }"
        "QLabel#Desc { color: #ffffff; font-size: 13px; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(24, 20, 24, 20);
    mainLayout->setSpacing(14);

    auto addRecipe = [&](const QString& resultName, const QString& recipe1, const QString& recipe2, const QString& effect) {
        QLabel* title = new QLabel(resultName, &dialog);
        title->setObjectName("Title");
        mainLayout->addWidget(title);
        QLabel* recipe = new QLabel(QString::fromUtf8("配方: %1 + %2").arg(recipe1, recipe2), &dialog);
        recipe->setObjectName("Sub");
        mainLayout->addWidget(recipe);
        QLabel* desc = new QLabel(effect, &dialog);
        desc->setObjectName("Desc");
        desc->setWordWrap(true);
        mainLayout->addWidget(desc);
    };

    addRecipe(QString::fromUtf8("魔神杀刃"),
              QString::fromUtf8("血怒徽记"),
              QString::fromUtf8("幽影之刃"),
              QString::fromUtf8("攻击+70  吸血+35%  每次攻击额外造成2%对方生命值的伤害  攻击时无视30%防御"));
    addRecipe(QString::fromUtf8("永恒壁垒"),
              QString::fromUtf8("荆棘核心"),
              QString::fromUtf8("不朽图腾"),
              QString::fromUtf8("防御+80  魔抗+20   每秒恢复3%生命   反弹30%受到伤害"));
    addRecipe(QString::fromUtf8("虚空脉冲炮"),
              QString::fromUtf8("奥术洪流"),
              QString::fromUtf8("风暴连弩"),
              QString::fromUtf8("普攻额外回复20蓝量  技能伤害+30%  普攻变为周围1格的范围伤害"));

    mainLayout->addStretch();

    QHBoxLayout* btnLayout = new QHBoxLayout();
    QPushButton* closeBtn = new QPushButton(QString::fromUtf8("关闭"), &dialog);
    closeBtn->setStyleSheet("QPushButton { background-color: #5a8fd8; color: white; border: none; border-radius: 6px; padding: 8px 18px; font-weight: 700; }");
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}

void GameWidget::ShowSynergyDetailDialog(const SynergyStatus& synergy) {
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("羁绊详情"));
    dialog.setMinimumSize(360, 220);
    dialog.setStyleSheet(
        "QDialog { background-color: #24342b; border-radius: 10px; }"
        "QLabel#Title { color: #eaffb2; font-size: 22px; font-weight: 700; }"
        "QLabel#Key { color: #a8d8b0; font-size: 14px; font-weight: 600; }"
        "QLabel#Value { color: #ffffff; font-size: 15px; }"
        "QPushButton { background-color: #65b85f; color: white; border: none; border-radius: 6px; padding: 8px 18px; font-weight: 700; }"
        "QPushButton:hover { background-color: #78cb72; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(16);

    QLabel* title = new QLabel(QString::fromUtf8("羁绊详情"), &dialog);
    title->setObjectName("Title");
    mainLayout->addWidget(title);

    QGridLayout* infoLayout = new QGridLayout();
    infoLayout->setHorizontalSpacing(20);
    infoLayout->setVerticalSpacing(12);

    auto addRow = [&](int row, const QString& key, const QString& value) {
        QLabel* keyLabel = new QLabel(key, &dialog);
        keyLabel->setObjectName("Key");
        QLabel* valueLabel = new QLabel(value, &dialog);
        valueLabel->setObjectName("Value");
        valueLabel->setWordWrap(true);
        infoLayout->addWidget(keyLabel, row, 0);
        infoLayout->addWidget(valueLabel, row, 1);
    };

    const bool active = synergy.count >= synergy.required;
    addRow(0, QString::fromUtf8("名称"), synergy.label);
    addRow(1, QString::fromUtf8("加成"), synergy.description);
    addRow(2, QString::fromUtf8("状态"),
           QString("%1/%2，%3").arg(synergy.count).arg(synergy.required).arg(active ? QString::fromUtf8("已激活") : QString::fromUtf8("未激活")));

    mainLayout->addLayout(infoLayout);
    mainLayout->addStretch();

    QPushButton* closeButton = new QPushButton(QString::fromUtf8("关闭"), &dialog);
    mainLayout->addWidget(closeButton, 0, Qt::AlignRight);
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void GameWidget::ShowUnitDetailDialog(int shopSlot) {
    shared_ptr<Unit> unit = shop_.GetUnitAt(shopSlot);
    if (unit == nullptr) {
        return;
    }

    ShowUnitDetailDialog(unit, true, shopSlot);
}

void GameWidget::ShowEquipmentDetailDialog(int shopSlot) {
    if (shopSlot < 0 || shopSlot >= static_cast<int>(equipmentShopSlots_.size()) || equipmentShopSlots_[shopSlot] == nullptr) {
        return;
    }

    ShowEquipmentDetailDialog(equipmentShopSlots_[shopSlot], true, shopSlot);
}

void GameWidget::ShowEquipmentDetailDialog(const shared_ptr<Equipment>& equipment, bool canBuy, int shopSlot, int inventorySlot, int overflowIndex) {
    if (equipment == nullptr) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("装备详情"));
    dialog.setMinimumSize(340, 240);
    dialog.setStyleSheet(
        "QDialog { background-color: #2d3140; border-radius: 12px; }"
        "QLabel#Title { color: #f5f7fb; font-size: 22px; font-weight: 700; }"
        "QLabel#Key { color: #b9c4d8; font-size: 14px; font-weight: 600; }"
        "QLabel#Value { color: #ffffff; font-size: 15px; }"
        "QPushButton { background-color: #5a8fd8; color: white; border: none; border-radius: 6px; padding: 8px 18px; font-weight: 700; }"
        "QPushButton:hover { background-color: #6fa2ec; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(16);

    QLabel* title = new QLabel(QString::fromUtf8("装备详情"), &dialog);
    title->setObjectName("Title");
    mainLayout->addWidget(title);

    QGridLayout* infoLayout = new QGridLayout();
    infoLayout->setHorizontalSpacing(24);
    infoLayout->setVerticalSpacing(12);

    auto addRow = [&](int row, const QString& key, const QString& value) {
        QLabel* keyLabel = new QLabel(key, &dialog);
        keyLabel->setObjectName("Key");
        QLabel* valueLabel = new QLabel(value, &dialog);
        valueLabel->setObjectName("Value");
        valueLabel->setWordWrap(true);
        infoLayout->addWidget(keyLabel, row, 0);
        infoLayout->addWidget(valueLabel, row, 1);
    };

    addRow(0, QString::fromUtf8("名称"), QString::fromStdString(equipment->GetName()));
    addRow(1, QString::fromUtf8("星级"), QString::number(equipment->GetStar()));
    addRow(2, QString::fromUtf8("价格"), QString::number(equipment->GetPrice()));
    addRow(3, QString::fromUtf8("出售价"), QString::number(GetEquipmentSellPrice(*equipment)));
    addRow(4, QString::fromUtf8("效果"), GetEquipmentDescription(*equipment));
    addRow(5, QString::fromUtf8("位置"),
           canBuy ? QString::fromUtf8("装备商店")
                  : (overflowIndex != -1 ? QString::fromUtf8("溢出区") : QString::fromUtf8("装备区")));

    mainLayout->addLayout(infoLayout);
    mainLayout->addStretch();

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* buyButton = nullptr;
    QPushButton* sellButton = nullptr;
    if (canBuy) {
        buyButton = new QPushButton(QString::fromUtf8("购买"), &dialog);
        buttonLayout->addWidget(buyButton, 0, Qt::AlignLeft);
    } else {
        sellButton = new QPushButton(QString::fromUtf8("出售"), &dialog);
        buttonLayout->addWidget(sellButton, 0, Qt::AlignLeft);
    }
    QPushButton* closeButton = new QPushButton(QString::fromUtf8("关闭"), &dialog);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton, 0, Qt::AlignRight);
    mainLayout->addLayout(buttonLayout);

    if (buyButton != nullptr) {
        connect(buyButton, &QPushButton::clicked, &dialog, [this, shopSlot, &dialog]() {
            if (shopSlot < 0 || shopSlot >= static_cast<int>(equipmentShopSlots_.size()) || equipmentShopSlots_[shopSlot] == nullptr) {
                return;
            }

            shared_ptr<Equipment> shopEquipment = equipmentShopSlots_[shopSlot];
            if (testPlayer_.GetGold() < shopEquipment->GetPrice()) {
                QMessageBox::warning(&dialog, QString::fromUtf8("购买失败"), QString::fromUtf8("金币不足，购买失败"));
                return;
            }
            if (equipmentInventory_.IsFull()) {
                QMessageBox::warning(&dialog, QString::fromUtf8("购买失败"), QString::fromUtf8("装备区已满，购买失败"));
                return;
            }
            if (!equipmentInventory_.AddPurchasedEquipment(shopEquipment)) {
                QMessageBox::warning(&dialog, QString::fromUtf8("购买失败"), QString::fromUtf8("装备区已满，购买失败"));
                return;
            }

            testPlayer_.SpendGold(shopEquipment->GetPrice());
            equipmentShopSlots_[shopSlot] = nullptr;
            TryFuseEquipment();
            TrySynthesizeEquipment();
            MarkGameChanged();
            update();
            dialog.accept();
        });
    }

    if (sellButton != nullptr) {
        connect(sellButton, &QPushButton::clicked, &dialog, [this, inventorySlot, overflowIndex, &dialog]() {
            shared_ptr<Equipment> soldEquipment = nullptr;
            if (inventorySlot != -1) {
                soldEquipment = equipmentInventory_.RemoveEquipmentAt(inventorySlot);
            } else if (overflowIndex != -1) {
                soldEquipment = equipmentInventory_.RemoveOverflowEquipmentAt(overflowIndex);
            }

            if (soldEquipment != nullptr) {
                testPlayer_.AddGold(GetEquipmentSellPrice(*soldEquipment));
                MarkGameChanged();
                update();
                dialog.accept();
            }
        });
    }

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    dialog.exec();
}

void GameWidget::ShowUnitDetailDialog(const shared_ptr<Unit>& unit, bool canBuy, int shopSlot, int benchSlot) {
    if (unit == nullptr) {
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("单位信息"));
    dialog.setMinimumSize(360, 350);
    dialog.setStyleSheet(
        "QDialog { background-color: #26313f; border-radius: 12px; }"
        "QLabel#Title { color: #f5f7fb; font-size: 22px; font-weight: 700; }"
        "QLabel#Key { color: #9fb2c8; font-size: 14px; font-weight: 600; }"
        "QLabel#Value { color: #ffffff; font-size: 15px; }"
        "QPushButton { background-color: #5a8fd8; color: white; border: none; border-radius: 6px; padding: 8px 18px; font-weight: 700; }"
        "QPushButton:hover { background-color: #6fa2ec; }");

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(18);

    QLabel* title = new QLabel(QString::fromUtf8("单位信息"), &dialog);
    title->setObjectName("Title");
    mainLayout->addWidget(title);

    QGridLayout* infoLayout = new QGridLayout();
    infoLayout->setHorizontalSpacing(24);
    infoLayout->setVerticalSpacing(12);

    auto addRow = [&](int row, const QString& key, const QString& value) {
        QLabel* keyLabel = new QLabel(key, &dialog);
        keyLabel->setObjectName("Key");
        QLabel* valueLabel = new QLabel(value, &dialog);
        valueLabel->setObjectName("Value");
        infoLayout->addWidget(keyLabel, row, 0);
        infoLayout->addWidget(valueLabel, row, 1);
    };

    addRow(0, QString::fromUtf8("名称"), QString::fromStdString(unit->GetName()));
    addRow(1, QString::fromUtf8("职业"), QString::fromStdString(unit->GetProfession()));
    addRow(2, QString::fromUtf8("星级"), QString::number(unit->GetStar()));
    addRow(3, QString::fromUtf8("血量"), QString("%1 / %2").arg(unit->GetHp()).arg(unit->GetMaxHp()));
    addRow(4, QString::fromUtf8("法力"), QString("%1 / %2").arg(unit->GetMana()).arg(unit->GetMaxMana()));
    const int adjustedAtk = GetSynergyAdjustedAtk(unit);
    const QString atkText = adjustedAtk == unit->GetAtk()
                                ? QString::number(unit->GetAtk())
                                : QString("%1（基础 %2）").arg(adjustedAtk).arg(unit->GetAtk());
    addRow(5, QString::fromUtf8("攻击力"), atkText);
    addRow(6, QString::fromUtf8("物理防御"), QString::number(unit->GetDefense()));
    addRow(7, QString::fromUtf8("法术抗性"), QString("%1%").arg(unit->GetMagicResist()));
    addRow(8, QString::fromUtf8("价格"), QString::number(unit->GetPrice()));
    addRow(9, QString::fromUtf8("出售价"), QString::number(unit->GetSellPrice()));
    addRow(10, QString::fromUtf8("归属"), unit->GetOwner() == nullptr ? QString::fromUtf8("无") : QString::fromUtf8("玩家"));
    addRow(11, QString::fromUtf8("装备"), QString("%1 / %2").arg(unit->GetCurrentEquipmentCount()).arg(unit->GetMaxEquipmentCount()));
    addRow(12, QString::fromUtf8("装备列表"), GetEquipmentSummary(unit));
    addRow(13, QString::fromUtf8("羁绊"), GetUnitSynergyText(unit));
    addRow(14, QString::fromUtf8("状态"), unit->IsAlive() ? QString::fromUtf8("存活") : QString::fromUtf8("阵亡"));

    mainLayout->addLayout(infoLayout);
    mainLayout->addStretch();

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* buyButton = nullptr;
    QPushButton* sellButton = nullptr;
    if (canBuy) {
        buyButton = new QPushButton(QString::fromUtf8("购买"), &dialog);
        buttonLayout->addWidget(buyButton, 0, Qt::AlignLeft);
    } else if (benchSlot != -1) {
        sellButton = new QPushButton(QString::fromUtf8("出售"), &dialog);
        buttonLayout->addWidget(sellButton, 0, Qt::AlignLeft);
    }
    QPushButton* closeButton = new QPushButton(QString::fromUtf8("关闭"), &dialog);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton, 0, Qt::AlignRight);
    mainLayout->addLayout(buttonLayout);

    if (buyButton != nullptr) {
        connect(buyButton, &QPushButton::clicked, &dialog, [this, shopSlot, &dialog]() {
            shared_ptr<Unit> unit = shop_.GetUnitAt(shopSlot);
            if (unit == nullptr) {
                return;
            }

            if (testPlayer_.GetGold() < unit->GetPrice()) {
                QMessageBox::warning(&dialog, QString::fromUtf8("购买失败"), QString::fromUtf8("金币不足，购买失败"));
                return;
            }

            if (bench_.GetCurrentCount() >= Bench::kSize) {
                QMessageBox::warning(&dialog, QString::fromUtf8("购买失败"), QString::fromUtf8("备战区已满，购买失败"));
                return;
            }

            const int price = unit->GetPrice();
            shared_ptr<Unit> boughtUnit = shop_.BuyUnit(shopSlot);
            if (boughtUnit != nullptr && bench_.AddUnitToFirstEmptyFromRight(boughtUnit)) {
                testPlayer_.SpendGold(price);
                selectedShopSlot_ = -1;

                // Check for star fusion
                shared_ptr<Unit> fusedUnit;
                if (TryFusion(fusedUnit)) {
                    const int newStar = fusedUnit->GetStar();
                    shared_ptr<Unit> rewardUnit = CreateFusionRewardUnit();
                    const bool receivedReward = rewardUnit != nullptr && bench_.AddUnitToFirstEmptyFromRight(rewardUnit);

                    QString fusionMessage =
                        QString::fromUtf8("%1 已合成 %2星！")
                            .arg(QString::fromStdString(fusedUnit->GetName()))
                            .arg(newStar);
                    if (receivedReward) {
                        fusionMessage += QString::fromUtf8("\n合成奖励：获得一个免费 %1，此次额外刷新不影响当前商店。")
                                             .arg(QString::fromStdString(rewardUnit->GetName()));
                    } else {
                        fusionMessage += QString::fromUtf8("\n合成奖励刷新失败。");
                    }

                    QMessageBox::information(&dialog, QString::fromUtf8("合成成功"), fusionMessage);
                }

                MarkGameChanged();
                update();
                dialog.accept();
            }
        });
    }

    if (sellButton != nullptr) {
        connect(sellButton, &QPushButton::clicked, &dialog, [this, benchSlot, &dialog]() {
            shared_ptr<Unit> soldUnit = bench_.SellUnitAndShiftRight(benchSlot);
            if (soldUnit != nullptr) {
                ReturnUnitEquipmentsToInventory(soldUnit);
                testPlayer_.AddGold(soldUnit->GetSellPrice());
                selectedBenchSlot_ = -1;
                MarkGameChanged();
                update();
                dialog.accept();
            }
        });
    }

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();
}
