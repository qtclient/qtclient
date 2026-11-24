/*
 * Copyright (c) 2024-2026 qtclient contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "outfit.h"
#include "position.h"
#include "teamfinder/teamfinder_types.h"
#include "clockservice.h"

#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <array>
#include <vector>

struct AwareRange
{
    uint8_t left{ 0 };
    uint8_t top{ 0 };
    uint8_t right{ 0 };
    uint8_t bottom{ 0 };

    uint8_t horizontal() const { return left + right + 1; }
    uint8_t vertical() const { return top + bottom + 1; }

    QSize dimension() const { return { left * 2 + 1 , top * 2 + 1 }; }

    bool operator==(const AwareRange& other) const
    { return left == other.left && top == other.top && right == other.right && bottom == other.bottom; }
};

struct RaceType
{
    uint32_t raceId{ 0 };
    uint32_t category{ 0 };
    std::string name;
    Outfit outfit;
    bool hasCategory{ false };
    bool boss{ false };
    /// Boss record field 4: the team finder leaves these bosses out.
    bool notInTeamFinder{ false };
    /// Bestiary difficulty 0..5 (bosses: 6) and occurrence 0..3.
    uint8_t difficulty{ 0 };
    uint8_t occurrence{ 0 };
    /// Static data monster field 7 (the bounty task picker offers only monsters with it).
    bool flag7{ false };
};

/// CIP staticdata MonsterClass (the bestiary's classes).
struct MonsterClassType
{
    uint8_t id{ 0 };
    std::string name;
};

struct CyclopediaHouseType
{
    uint32_t id{ 0 };
    std::string name;
    std::string description;
    uint32_t rent{ 0 };
    uint32_t beds{ 0 };
    Position position;
    uint32_t size{ 0 };
    bool isGuildHall{ false };
    std::string city;
    bool isShop{ false };
};

struct CyclopediaAchievementType
{
    uint16_t id{ 0 };
    std::string name;
    std::string description;
    uint8_t grade{ 0 };
};

/// CIP staticdata Quest (Team Finder / Cyclopedia).
struct CyclopediaQuestType
{
    uint16_t id{ 0 };
    std::string name;
};

/// Everything the staticdata asset holds (CIP: TAssetHandler's static information).
struct TStaticInformation
{
    std::vector<RaceType> monsters;
    std::vector<MonsterClassType> monsterClasses;
    std::vector<RaceType> bosses;
    std::vector<CyclopediaAchievementType> achievements;
    std::vector<CyclopediaHouseType> houses;
    std::vector<CyclopediaQuestType> quests;
};

/// One house's area in the staticmapdata asset: its fields stay serialized.
struct THouseMapArea
{
    uint32_t houseId{ 0 };
    Position topLeft;
    uint32_t width{ 0 };
    uint32_t height{ 0 };
    uint32_t depth{ 0 };
    std::string serializedFields;
};

struct CyclopediaHouseEntry
{
    uint32_t houseId{ 0 };
    bool isRentable{ false };
    uint8_t state{ 0 };
    std::string bidderName;
    bool isBidder{ false };
    uint8_t bidDisableReason{ 0 };
    uint32_t bidEnd{ 0 };
    uint64_t highestBid{ 0 };
    uint64_t bidLimit{ 0 };
    std::string ownerName;
    uint32_t paidUntil{ 0 };
    bool isOwner{ false };
    uint8_t moveOutDisableReason{ 0 };
    uint8_t transferDisableReason{ 0 };
    uint32_t transferDate{ 0 };
    std::string newOwnerName;
    bool newOwnerAccepted{ false };
    uint64_t transferPrice{ 0 };
    bool isNewOwner{ false };
    uint8_t acceptTransferDisableReason{ 0 };
    uint8_t rejectTransferDisableReason{ 0 };
    uint8_t cancelTransferDisableReason{ 0 };
    uint32_t moveOutDate{ 0 };
    uint8_t cancelMoveOutDisableReason{ 0 };
};

/// Crystal/CIP OutfitMemorial (S 0xB0) body — golden + token outfit tabs.
struct OutfitMemorialData
{
    uint32_t baseGoldenOutfitCost{ 0 };
    uint32_t oneAddonGoldenOutfitCost{ 0 };
    uint32_t fullGoldenOutfitCost{ 0 };
    std::vector<std::string> baseGoldenOutfitOwners;
    std::vector<std::string> oneAddonGoldenOutfitOwners;
    std::vector<std::string> fullGoldenOutfitOwners;
    uint16_t baseTokenOutfitSilverToken{ 0 };
    uint16_t baseTokenOutfitGoldToken{ 0 };
    uint16_t oneAddonTokenOutfitSilverToken{ 0 };
    uint16_t oneAddonTokenOutfitGoldToken{ 0 };
    uint16_t fullTokenOutfitSilverToken{ 0 };
    uint16_t fullTokenOutfitGoldToken{ 0 };
    std::vector<std::string> baseTokenOutfitOwners;
    std::vector<std::string> oneAddonTokenOutfitOwners;
    std::vector<std::string> fullTokenOutfitOwners;
};

struct Imbuement
{
    uint32_t id;
    std::string name;
    std::string description;
    std::string group;
    uint8_t tier;
    uint16_t imageId;
    uint32_t duration;
    bool premiumOnly;
    /// typeId, count, name — no ItemPtr
    std::vector<std::tuple<uint16_t, uint16_t, std::string>> sources;
    uint32_t cost;
    uint8_t successRate;
    uint32_t protectionCost;
};

struct Light
{
    Light() = default;
    Light(const uint8_t intensity, const uint8_t color) : intensity(intensity), color(color) {}
    uint8_t intensity = 0;
    uint8_t color = 215;
};

struct BossCooldownData
{
    uint32_t bossRaceId;
    uint64_t cooldownTime;

    BossCooldownData(uint32_t raceId, uint64_t cooldown)
        : bossRaceId(raceId), cooldownTime(cooldown) {}
};

struct CharmData
{
    uint8_t id;
    std::string name;
    std::string description;
    uint16_t unlockPrice;
    bool unlocked;
    bool asignedStatus;
    uint16_t raceId;
    uint32_t removeRuneCost;
    uint8_t availableCharmSlots;
    uint8_t tier;
};

struct BestiaryCharmsData
{
    uint64_t resetAllCharmsCost;
    uint8_t availableCharmSlots;

    uint32_t points;
    std::vector<CharmData> charms;
    std::vector<uint16_t> finishedMonsters;
};

struct LootItem
{
    uint16_t itemId;
    uint8_t diffculty;
    uint8_t specialEvent;
    std::string name;
    uint8_t amount;
};

struct BestiaryMonsterData
{
    uint16_t id;
    std::string bestClass;
    uint8_t currentLevel;
    uint16_t AnimusMasteryBonus;
    uint16_t AnimusMasteryPoints;
    uint32_t killCounter;
    uint16_t thirdDifficulty;
    uint16_t secondUnlock;
    uint16_t lastProgressKillCount;
    uint8_t difficulty;
    uint8_t ocorrence;
    std::vector<LootItem> loot;
    uint16_t charmValue;
    uint8_t attackMode;
    uint32_t maxHealth;
    uint32_t experience;
    uint16_t speed;
    uint16_t armor;
    double mitigation;
    std::map<uint8_t, uint16_t> combat;
    std::string location;
    bool leaderKilled{ false };
    bool castsSpells{ false };
};

struct ItemSummary
{
    uint16_t itemId;
    uint8_t tier;
    uint32_t amount;
};

struct CyclopediaCharacterItemSummary
{
    std::vector<ItemSummary> inventory;
    std::vector<ItemSummary> store;
    std::vector<ItemSummary> stash;
    std::vector<ItemSummary> depot;
    std::vector<ItemSummary> inbox;
};

struct RecentPvPKillEntry
{
    uint32_t timestamp;
    std::string description;
    uint8_t status;
};

struct RecentDeathEntry
{
    uint32_t timestamp;
    std::string cause;
};

struct ItemInspectionData
{
    uint8_t inspectionType;
    uint32_t creatureId;
    std::string name;
    uint16_t typeId = 0;
    uint16_t count = 1;
    uint8_t tier = 0;
    uint8_t fluidOrSubtype = 0;
    bool fluidOrSplash = false;
    bool hookSouth = false;
    bool hookEast = false;
    bool hasProficiency = false;
    std::vector<uint16_t> imbuements;
    std::vector<std::pair<std::string, std::string>> descriptions;
};

struct InspectionInventoryItem
{
    uint8_t slot;
    std::string name;
    uint16_t typeId = 0;
    uint16_t count = 1;
    uint8_t tier = 0;
    uint8_t fluidOrSubtype = 0;
    bool fluidOrSplash = false;
    bool hookSouth = false;
    bool hookEast = false;
    bool hasProficiency = false;
    std::vector<uint16_t> imbuements;
    std::vector<std::pair<std::string, std::string>> descriptions;
};

struct CyclopediaCharacterInspection
{
    uint8_t inspectionType;
    uint32_t creatureId{ 0 };
    std::vector<InspectionInventoryItem> inventoryItems;
    std::string playerName;
    Outfit outfit;
    std::vector<std::pair<std::string, std::string>> playerDescriptions;
};

struct CyclopediaCharacterOffenceStats
{
    double critChanceTotal;
    double critChanceFlat;
    double critChanceEquipament;
    double critChanceImbuement;
    double critChanceWheel;
    double critChanceConcoction;
    double critChanceEventBonus;

    double critDamageTotal;
    double critDamageFlat;
    double critDamageEquipament;
    double critDamageImbuement;
    double critDamageWheel;
    double critDamageConcoction;

    double lifeLeechTotal;
    double lifeLeechEquipament;
    double lifeLeechImbuement;
    double lifeLeechWheel;
    double lifeLeechEventBonus;

    double manaLeechTotal;
    double manaLeechEquipament;
    double manaLeechImbuement;
    double manaLeechWheel;
    double manaLeechEventBonus;

    double onslaught;
    double onslaughtBase;
    double onslaughtBonus;
    double onslaughtEventBonus;

    double cleavePercent;

    std::vector<uint16_t> perfectShotDamage;

    uint16_t flatDamage;
    uint16_t flatDamageBase;
    uint16_t flatDamageWheel;

    uint16_t weaponAttack;
    uint16_t weaponFlatModifier;
    uint16_t weaponDamage;
    uint8_t weaponSkillType;
    uint16_t weaponSkillLevel;
    uint16_t weaponSkillModifier;
    uint8_t weaponElement;
    double weaponElementDamage;
    uint8_t weaponElementType;
    
    struct AccuracyData { uint8_t range; double chance; };
    std::vector<AccuracyData> weaponAccuracy;

    double damagePowerfulFoes;
    struct TargetBonus { std::string name; double value; };
    std::vector<TargetBonus> damageSpecificTargets;

    struct ElementModifier { uint8_t element; double value; };
    std::vector<ElementModifier> damageElements;

    double offensiveRuneDamage;
    double autoAttackDamage;

    std::vector<ElementModifier> critDamageElements;
    double critDamageOffensiveRunes;
    double critDamageAutoAttack;

    uint16_t lifeGainHit;
    uint16_t manaGainHit;
    uint16_t lifeGainKill;
    uint16_t manaGainKill;

    struct SkillBonus { uint8_t skillId; double valueA; double valueB; };
    std::vector<SkillBonus> extraDamageSkills;
    std::vector<SkillBonus> extraDamageSpells;
    std::vector<SkillBonus> extraHealingSpells;

    double damageHighHp;
    double damageLowHp;
    double armorPenetration;
    std::vector<ElementModifier> elementalPierce;
};

/// The defence stats of the character info message, as CIP's
/// CharacterInfoDefenceStats: the parts arrays are total first, then the
/// sources (see TPlayerDefenceStatsProvider).
struct CyclopediaCharacterDefenceStats
{
    /// Kind 4 is an element's total resistance, kinds 0..3 its parts from
    /// equipment, imbuements, the skill wheel and concoctions.
    struct ElementalResistance
    {
        uint8_t kind = 0;
        uint8_t element = 0;
        double value = 0;
    };

    std::vector<ElementalResistance> resistances;
    /// Total, equipment, amplification, event bonus, skill wheel.
    std::array<double, 5> dodge{};
    uint32_t magicShieldCapacity = 0;
    uint16_t magicShieldCapacityFlat = 0;
    double magicShieldCapacityPercent = 0;
    uint16_t reflectPhysical = 0;
    uint16_t armor = 0;
    uint16_t mantra = 0;
    uint16_t defence = 0;
    uint16_t defenceEquipment = 0;
    /// The defence the skill adds, by the protocol's skill type.
    int16_t defenceSkill = 0;
    uint16_t defenceWheel = 0;
    uint8_t defenceSkillType = 0;
    /// Total, equipment, defence, skill, skill wheel.
    std::array<double, 5> mitigation{};
};

struct CyclopediaCharacterMiscStats
{
    double momentumTotal;
    double momentumBase;
    double momentumBonus;
    double momentumWheel;
    double momentumEventBonus;

    double dodgeTotal;
    double dodgeBase;
    double dodgeBonus;
    double dodgeWheel;

    double damageReflectionTotal;
    double damageReflectionBase;
    double damageReflectionBonus;

    uint8_t haveBlesses;
    uint8_t totalBlesses;

    struct Concoction
    {
        uint16_t id;
        uint32_t duration;
    };

    struct Food
    {
        uint16_t id;
        uint32_t duration;
    };

    struct Augment
    {
        uint16_t spellId;
        uint8_t type;
        double value;
    };

    std::vector<Concoction> concoctions;
    std::vector<Food> activeFoods;
    std::vector<Augment> weaponProficiencyAugments;
    std::vector<Augment> wheelAugments;
    std::vector<Augment> equippedAugments;
};

struct ForgeGradeData
{
    uint8_t tier{ 0 };
    uint8_t exaltedCores{ 0 };
};

struct ForgeTierPrice
{
    uint8_t tier{ 0 };
    uint64_t price{ 0 };
};

struct ForgeClassTierPrices
{
    uint8_t classId{ 0 };
    std::vector<ForgeTierPrice> tiers;
};

struct ForgeConfigData
{
    std::vector<ForgeClassTierPrices> classPrices;
    std::vector<ForgeGradeData> fusionGrades;
    std::vector<ForgeTierPrice> convergenceFusionPrices;
    std::vector<ForgeTierPrice> convergenceTransferPrices;
    uint8_t dustPercent{ 0 };
    uint8_t dustToSliver{ 0 };
    uint8_t sliverToCore{ 0 };
    uint8_t dustPercentUpgrade{ 0 };
    uint16_t maxDustLevel{ 0 };
    uint16_t maxDustCap{ 0 };
    uint8_t normalDustFusion{ 0 };
    uint8_t convergenceDustFusion{ 0 };
    uint8_t normalDustTransfer{ 0 };
    uint8_t convergenceDustTransfer{ 0 };
    uint8_t fusionChanceBase{ 0 };
    uint8_t fusionChanceImproved{ 0 };
    uint8_t fusionReduceTierLoss{ 0 };
    bool hasConvergence{ false };
};
