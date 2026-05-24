# Project A — Synera: Auto Chess

A Qt Widgets based auto chess game developed with C++17. The project features real-time combat, equipment crafting, synergy systems, economy management, and JSON save/load support.

---

# Quick Start

## Build & Run

```bash
# Build with CMake
cmake --build build-mingw

# Run executable
./build-mingw/ProjectAQtBoard.exe
```

## Requirements

* CMake 3.16+
* Qt6 (Qt5 Widgets compatible)
* C++17 compatible compiler (MinGW / MSVC)
* Windows 10 / 11

---

# Game Overview

## Objective

Players must manage economy, buy units, build synergies, and equip their team in order to defeat all enemy waves within a limited number of rounds.

Core gameplay includes:

1. Buying units from the shop
2. Deploying units onto the board
3. Combining identical units for star upgrades
4. Equipping items
5. Watching real-time auto battles
6. Clearing all rounds for the selected difficulty

## Difficulty Modes

| Mode          | Rounds | Starting HP | Starting Gold |
| ------------- | ------ | ----------- | ------------- |
| Speed Mode    | 8      | 40          | 20            |
| Standard Mode | 10     | 60          | 15            |
| Hard Mode     | 13     | 80          | 12            |

After selecting a new save slot, the game opens a dedicated difficulty selection menu:

* Speed Mode (8 rounds)
* Standard Mode (10 rounds)
* Hard Mode (13 rounds)
* Back

The interface shares the same visual style and layout as the main menu and load-game menu.

---

# Core Systems

# Unit System

The game currently contains 17 named units divided into:

* Warrior
* Tank
* Archer
* Mage
* Assassin (plugin class)
* Support (plugin class)

## Main Classes

| Class   | Icon | Role                 |
| ------- | ---- | -------------------- |
| Warrior | ●    | Balanced melee       |
| Tank    | ■    | Frontline defense    |
| Archer  | ★    | Sustained ranged DPS |
| Mage    | ✚    | AoE magic damage     |

## Plugin Classes

| Class    | Icon | Role                 |
| -------- | ---- | -------------------- |
| Assassin | ▼    | Backline elimination |
| Support  | ◆    | Heal / Buff / Debuff |

## Star Upgrade System

Three identical units combine into a stronger version:

* 1★ → 2★
* 2★ → 3★

Stat scaling multipliers:

* 1★ : 1.0×
* 2★ : 1.8×
* 3★ : 3.2×

Affected stats:

* HP
* ATK
* Defense
* Magic Resistance

---

# Synergy System

## Iron Frontline (Warrior + Tank)

| Requirement | Effect                                                                  |
| ----------- | ----------------------------------------------------------------------- |
| 2 units     | Team Defense +15                                                        |
| 4 units     | Warriors/Tanks take 20% less damage and regenerate 2% max HP per second |

## Elemental Firepower (Mage + Archer)

| Requirement | Effect                               |
| ----------- | ------------------------------------ |
| 2 units     | Team ATK +15%                        |
| 4 units     | Skill damage +30%, attack speed +25% |

## Assassin

Requirement: 2 Assassins

Effects:

* Jump to enemy backline at battle start
* Critical chance +25%
* Critical damage +40%

## Support

Requirement: 2 Supports

Effects:

* Team mana regeneration +25%
* Healing received +25%

---

# Mana System

Default mana:

```text
0 / 100
```

## Mana From Attacks

| Class    | Mana per Attack |
| -------- | --------------- |
| Warrior  | 10              |
| Tank     | 8               |
| Archer   | 12              |
| Mage     | 10              |
| Assassin | 14              |
| Support  | 10              |

## Mana From Taking Damage

```text
Damage Taken ÷ 100
```

Maximum 15 mana per hit.

## Passive Mana Regeneration

All units regenerate 3 mana per second.

---

# Equipment System

## Basic Equipment (Cost: 2)

Available in Shop Levels 1–2.

| Equipment   | Effect               |
| ----------- | -------------------- |
| Long Sword  | ATK +20              |
| Cloth Armor | Defense +15          |
| Cloak       | Magic Resistance +15 |
| Ruby        | HP +250              |
| Sapphire    | Initial Mana +20     |

## Advanced Equipment (Cost: 3)

Available in Shop Levels 3–4.

| Equipment          | Effect                              |
| ------------------ | ----------------------------------- |
| Vampiric Mask      | Heal 15% of dealt damage            |
| Berserker Axe      | Splash damage to nearby enemies     |
| M3 Cocoon Armor    | Revive once with 30% HP             |
| Armor Piercing Bow | Ignore 20% target defense           |
| Flame Cloak        | Deals AoE magic damage every second |

## Special Equipment (Cost: 3)

Available in Shop Levels 3–4.
Cannot be upgraded.

| Equipment        | Effect                                   |
| ---------------- | ---------------------------------------- |
| Bloodrage Emblem | ATK +25, Lifesteal +20%                  |
| Thorn Core       | Defense +35, reflect 20% physical damage |
| Arcane Torrent   | Initial Mana +50, Skill Damage +20%      |
| Immortal Totem   | HP +500, restore 2% max HP per second    |
| Shadow Blade     | ATK +25, ignore 20% defense              |
| Storm Repeater   | ATK +10, attacks hit twice               |

## Combined Equipment

### Demon Slayer Blade

Crafted from:

* Bloodrage Emblem
* Shadow Blade

Effects:

* ATK +70
* Lifesteal +35%
* Extra 2% max HP damage per attack
* Ignore 30% defense

### Eternal Bastion

Crafted from:

* Thorn Core
* Immortal Totem

Effects:

* Defense +80
* Magic Resistance +20
* Regenerate 3% max HP per second
* Reflect 30% received damage

### Void Pulse Cannon

Crafted from:

* Arcane Torrent
* Storm Repeater

Effects:

* Basic attacks restore 20 mana
* Skill damage +30%
* Attacks become AoE

---

# Shop & Economy

## Deployment Slots

* Starting slots: 3
* Maximum slots: 6

Upgrade costs:

| Current → Target | Gold |
| ---------------- | ---- |
| 3 → 4            | 4    |
| 4 → 5            | 6    |
| 5 → 6            | 8    |

## Shop Levels

* Starting level: 1
* Maximum level: 4

Higher shop levels unlock higher-cost units.

| Shop Level | Available Unit Cost |
| ---------- | ------------------- |
| 1          | 2-cost              |
| 2          | 3-cost              |
| 3          | 4-cost              |
| 4          | 5-cost              |

Upgrade costs:

| Current → Target | Gold |
| ---------------- | ---- |
| 1 → 2            | 5    |
| 2 → 3            | 7    |
| 3 → 4            | 10   |

## Discount System

Deployment slot upgrades and shop upgrades both share the same discount rule:

* If not upgraded this round
* Cost decreases by 1 next round
* Minimum cost is 50% of the original price (rounded up)

## Income Per Round

* Base income: 4 gold
* Win: +3 gold
* Lose: +1 gold
* No win/lose streak bonus

---

# Combat System

## Real-Time Battles

The game runs at 60 FPS.

Unit states include:

* Idle
* Moving
* Attacking
* Casting
* Dead

## AI Behavior

* Melee units chase nearby enemies
* Ranged units maintain attack distance
* Supports heal the lowest HP ally
* Assassins dive into the backline at battle start

## Defeat Damage

Each difficulty mode uses different defeat damage scaling.

Defeat damage = base damage + surviving enemy star damage.

---

# Save System

## Features

* 4 independent save slots
* JSON persistence
* Rename & overwrite support
* Resume combat progress from saves

## Saved Data

* Player HP
* Gold
* Shop level
* Current round
* Board state
* Bench state
* Equipment inventory
* Difficulty mode

---

# GUI & Interface

## Main Menu

```text
Main Menu
├─ New Game
│   └─ Difficulty Selection
│       ├─ Speed Mode
│       ├─ Standard Mode
│       ├─ Hard Mode
│       └─ Back
├─ Load Game
└─ Exit
```

## In-Game UI

* Round display
* Shop panel
* Equipment inventory
* Synergy panel
* Unit details
* Countdown timer
* Drag & drop equipment
* Right-click unequip

---

# Project Structure

```text
ProjectA/
├── assets/
├── src/
│   ├── core/
│   ├── gui/
│   └── save/ 
├── CMakeLists.txt
├── CMakePresets.json
├── .clang-format
├── .gitignore
├── README.md
└── README_ch.md
```

---

# Tech Stack

* C++17
* Qt Widgets
* QPainter
* CMake
* JSON

---

# Project Status

✅ Feature complete and fully playable

Last Updated: 2026-05-24
