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

#include <cstdint>

namespace tibia {
namespace client {
    enum Operation : uint8_t
    {
        OPERATION_ADD, OPERATION_REMOVE, OPERATION_CLEAN
    };

    enum DrawFlags : uint32_t
    {
        DrawThings = 1 << 0,
        DrawLights = 1 << 1,
        DrawBars = 1 << 2,
        DrawNames = 1 << 3,
        DrawManaBar = 1 << 4,
        DrawHarmony = 1 << 5,
        DrawThingsAndLights = DrawThings | DrawLights,
        DrawCreatureInfo = DrawBars | DrawNames | DrawManaBar | DrawHarmony,
    };

    enum InventorySlot : uint8_t
    {
        InventorySlotHead = 1,
        InventorySlotNecklace,
        InventorySlotBackpack,
        InventorySlotArmor,
        InventorySlotRight,
        InventorySlotLeft,
        InventorySlotLegs,
        InventorySlotFeet,
        InventorySlotRing,
        InventorySlotAmmo,
        InventorySlotPurse,
        InventorySlotExt2,
        InventorySlotExt3,
        InventorySlotExt4,
        LastInventorySlot
    };

    enum Statistic : uint8_t
    {
        Health = 0,
        MaxHealth,
        FreeCapacity,
        Experience,
        Level,
        LevelPercent,
        Mana,
        MaxMana,
        MagicLevel,
        MagicLevelPercent,
        Soul,
        Stamina,
        LastStatistic
    };

    enum Skill : uint8_t
    {
        Fist = 0,
        Club,
        Sword,
        Axe,
        Distance,
        Shielding,
        Fishing,
        CriticalChance,
        CriticalDamage,
        LifeLeechChance,
        LifeLeechAmount,
        ManaLeechChance,
        ManaLeechAmount,
        Fatal,
        Dodge,
        Momentum,
        Transcendence,
        LastSkill
    };

    enum Direction : uint8_t
    {
        North = 0,
        East,
        South,
        West,
        NorthEast,
        SouthEast,
        SouthWest,
        NorthWest,
        InvalidDirection
    };

    enum FightModes : uint8_t
    {
        FightOffensive = 1,
        FightBalanced = 2,
        FightDefensive = 3
    };

    enum ChaseModes : uint8_t
    {
        DontChase = 0,
        ChaseOpponent = 1
    };

    enum PVPModes : uint8_t
    {
        WhiteDove = 0,
        WhiteHand = 1,
        YellowHand = 2,
        RedFist = 3
    };

    enum PlayerStates : uint64_t
    {
        IconNone = 0,
        IconPoison = 1,
        IconBurn = 2,
        IconEnergy = 4,
        IconDrunk = 8,
        IconManaShield = 16,
        IconParalyze = 32,
        IconHaste = 64,
        IconSwords = 128,
        IconDrowning = 256,
        IconFreezing = 512,
        IconDazzled = 1024,
        IconCursed = 2048,
        IconPartyBuff = 4096,
        IconPzBlock = 8192,
        IconPz = 16384,
        IconBleeding = 32768,
        IconHungry = 65536
    };

    enum PreyState_t : uint8_t
    {
        PREY_STATE_LOCKED = 0,
        PREY_STATE_INACTIVE = 1,
        PREY_STATE_ACTIVE = 2,
        PREY_STATE_SELECTION = 3,
        PREY_STATE_SELECTION_CHANGE_MONSTER = 4,
        PREY_STATE_LIST_SELECTION = 5,
        PREY_STATE_WILDCARD_SELECTION = 6,
    };

    enum PreyUnlockState_t : uint8_t
    {
        PREY_UNLOCK_STORE_AND_PREMIUM = 0,
        PREY_UNLOCK_STORE = 1,
        PREY_UNLOCK_NONE = 2,
    };
    enum class ForgeAction_t : uint8_t
    {
        FUSION = 0,
        TRANSFER = 1,
        DUST2SLIVER = 2,
        SLIVER2CORE = 3,
        INCREASELIMIT = 4,
    };


    enum Blessings : uint32_t
    {
        BlessingNone = 0,
        BlessingAdventurer = 1,              // bit 1
        BlessingTwistOfFate = 1 << 1,        // bit 2
        BlessingWisdomOfSolitude = 1 << 2,   // bit 3
        BlessingSparkOfPhoenix = 1 << 3,     // bit 4
        BlessingFireOfSuns = 1 << 4,         // bit 5
        BlessingSpiritualShielding = 1 << 5, // bit 6
        BlessingEmbraceOfTibia = 1 << 6,     // bit 7
        BlessingHeartOfMountain = 1 << 7,    // bit 8
        BlessingBloodOfMountain = 1 << 8,    // bit 9

        // Legacy / Compatibility
        LegacySpiritualShielding = 1 << 1,
        LegacyEmbraceOfTibia = 1 << 2,
        LegacyFireOfSuns = 1 << 3,
        LegacyWisdomOfSolitude = 1 << 4,
        LegacySparkOfPhoenix = 1 << 5
    };

    enum DeathType : uint8_t
    {
        DeathRegular = 0,
        DeathBlessed = 1
    };

    enum ResourceTypes_t : uint8_t
    {
        RESOURCE_BANK_BALANCE = 0, // 0x00
        RESOURCE_GOLD_EQUIPPED = 1, // 0x01
        // CIP RESOURCE_GUILD_BANK_GOLD_BALANCE (0xEE ResourceBalance).
        RESOURCE_GUILD_BANK_BALANCE = 2, // 0x02
        RESOURCE_NPC_TRADE = RESOURCE_GUILD_BANK_BALANCE, // legacy misnomer
        RESOURCE_PREY_WILDCARDS = 10, // 0x0A
        RESOURCE_DAILYREWARD_STREAK = 20, // 0x14
        RESOURCE_DAILYREWARD_JOKERS = 21, // 0x15
        RESOURCE_CHARM = 30, // 0x1E
        RESOURCE_MINOR_CHARM = 31, // 0x1F
        RESOURCE_MAX_CHARM = 32, // 0x20
        RESOURCE_MAX_MINOR_CHARM = 33, // 0x21
        RESOURCE_TASK_HUNTING = 50, // 0x32
        // CIP RESROUCE_NPC_TRADE_QUEST_FLAG_CURRENCY
        RESOURCE_NPC_STORAGE_TRADE = 60, // 0x3C
        RESOURCE_FORGE_DUST = 70, // 0x46
        RESOURCE_FORGE_SLIVER = 71, // 0x47
        RESOURCE_FORGE_CORES = 72, // 0x48
        RESOURCE_WHEEL_POINTS = 80, // 0x50
        RESOURCE_LESSER_GEMS = 81, // 0x51
        RESOURCE_REGULAR_GEMS = 82, // 0x52
        RESOURCE_GREATER_GEMS = 83, // 0x53
        RESOURCE_LESSER_FRAGMENT = 84, // 0x54
        RESOURCE_GREATER_FRAGMENT = 85, // 0x55
        RESOURCE_BOUNTY_POINTS = 86, // 0x56
        RESOURCE_SOULSEALS = 87, // 0x57
        // CipSoft Winter 2026 (External client switch @ with IRA/jokers).
        RESOURCE_BOUNTY_TASK_REROLL_TOKENS = 88, // 0x58
        RESOURCE_COSMETIC_TOKENS = 89, // 0x59
        RESOURE_COIN_NORMAL = 90, // 0x5A
        RESOURE_COIN_TRANSFERRABLE = 91, // 0x5B
        RESOURE_COIN_AUCTION = 92, // 0x5C
        RESOURE_COIN_TOURNAMENT = 93, // 0x5D
    };

    enum ExperienceRate_t : uint8_t
    {
        EXP_BASE = 0,
        EXP_VOUCHER = 1,
        EXP_LOWLEVEL = 2,
        EXP_XPBOOST = 3,
        EXP_STANINAMULTIPLIER = 4,
        EXP_DAILYREWARD = 5
    };

    enum MarketItemDescription : uint8_t
    {
        ITEM_DESC_ARMOR = 1,
        ITEM_DESC_ATTACK = 2,
        ITEM_DESC_CONTAINTER = 3,
        ITEM_DESC_DEFENSE = 4,
        ITEM_DESC_GENERAL = 5,
        ITEM_DESC_DECAY_TIME = 6,
        ITEM_DESC_COMBAT = 7,
        ITEM_DESC_MINLEVEL = 8,
        ITEM_DESC_MINMAGICLEVEL = 9,
        ITEM_DESC_VOCATION = 10,
        ITEM_DESC_RUNE = 11,
        ITEM_DESC_ABILITY = 12,
        ITEM_DESC_CHARGES = 13,
        ITEM_DESC_WEAPONTYPE = 14,
        ITEM_DESC_WEIGHT = 15,
        ITEM_DESC_AUGMENT = 16,
        ITEM_DESC_IMBUINGSLOTS = 17,
        ITEM_DESC_MAGICSHIELD = 18,
        ITEM_DESC_CLEAVE = 19,
        ITEM_DESC_REFLECTION = 20,
        ITEM_DESC_PERFECT = 21,
        ITEM_DESC_UPGRADECLASS = 22,
        ITEM_DESC_CURRENTTIER = 23,
        ITEM_DESC_ELEMENTALBOND = 24,
        ITEM_DESC_MANTRA = 25,
        ITEM_DESC_IMBUEMENTEFFECT = 26,

        ITEM_DESC_FIRST = ITEM_DESC_ARMOR,
        ITEM_DESC_LAST = ITEM_DESC_IMBUEMENTEFFECT,
    };

    enum MarketAction : uint8_t
    {
        MARKETACTION_BUY = 0,
        MARKETACTION_SELL = 1
    };

    enum MarketRequest : uint16_t
    {
        MARKETREQUEST_OWN_HISTORY = 1,
        MARKETREQUEST_OWN_OFFERS = 2,
        MARKETREQUEST_ITEM_BROWSE = 3,
        OLD_MARKETREQUEST_MY_OFFERS = 0xFFFE,
        OLD_MARKETREQUEST_MY_HISTORY = 0xFFFF,
    };

    enum Supply_Stash_Actions_t : uint8_t
    {
        SUPPLY_STASH_ACTION_STOW_ITEM = 0,
        SUPPLY_STASH_ACTION_STOW_CONTAINER = 1,
        SUPPLY_STASH_ACTION_STOW_STACK = 2,
        SUPPLY_STASH_ACTION_WITHDRAW = 3
    };

    enum CyclopediaHouseAuctionType_t : uint8_t
    {
        CYCLOPEDIA_HOUSE_TYPE_NONE = 0,
        CYCLOPEDIA_HOUSE_TYPE_BID = 1,
        CYCLOPEDIA_HOUSE_TYPE_MOVEOUT = 2,
        CYCLOPEDIA_HOUSE_TYPE_TRANSFER = 3,
        CYCLOPEDIA_HOUSE_TYPE_CANCEL_MOVEOUT = 4,
        CYCLOPEDIA_HOUSE_TYPE_CANCEL_TRANSFER = 5,
        CYCLOPEDIA_HOUSE_TYPE_ACCEPT_TRANSFER = 6,
        CYCLOPEDIA_HOUSE_TYPE_REFECT_TRANSFER = 7,
    };

    enum CyclopediaCharacterInfoType_t : uint8_t
    {
        CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION = 0,
        CYCLOPEDIA_CHARACTERINFO_GENERALSTATS = 1,
        CYCLOPEDIA_CHARACTERINFO_COMBATSTATS = 2,
        CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS = 3,
        CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS = 4,
        CYCLOPEDIA_CHARACTERINFO_ACHIEVEMENTS = 5,
        CYCLOPEDIA_CHARACTERINFO_ITEMSUMMARY = 6,
        CYCLOPEDIA_CHARACTERINFO_OUTFITSMOUNTS = 7,
        CYCLOPEDIA_CHARACTERINFO_STORESUMMARY = 8,
        CYCLOPEDIA_CHARACTERINFO_INSPECTION = 9,
        CYCLOPEDIA_CHARACTERINFO_BADGES = 10,
        CYCLOPEDIA_CHARACTERINFO_TITLES = 11,
        CYCLOPEDIA_CHARACTERINFO_WHEEL = 12,
        CYCLOPEDIA_CHARACTERINFO_OFFENCESTATS = 13,
        CYCLOPEDIA_CHARACTERINFO_DEFENCESTATS = 14,
        CYCLOPEDIA_CHARACTERINFO_MISCSTATS = 15
    };

    enum InspectObjectTypes : uint8_t
    {
        INSPECT_NORMALOBJECT = 0,
        INSPECT_NPCTRADE = 1,
        INSPECT_PLAYERTRADE = 2,
        INSPECT_CYCLOPEDIA = 3,
        INSPECT_PROFICIENCY = 4
    };

    enum GameStoreInfoStatesType_t : uint8_t
    {
        STATE_NONE = 0,
        STATE_NEW = 1,
        STATE_SALE = 2,
        STATE_TIMED = 3
    };

    enum GroupsEditInfoType_t : uint8_t
    {
        VIP_GROUP_NONE = 0,
        VIP_GROUP_ADD = 1,
        VIP_GROUP_EDIT = 2,
        VIP_GROUP_REMOVE = 3,
    };

    enum Store_Type_Actions_t : uint8_t
    {
        OPEN_HOME = 0,
        OPEN_PREMIUM_BOOST = 1,
        OPEN_CATEGORY = 2,
        OPEN_USEFUL_THINGS = 3,
        OPEN_OFFER = 4,
        OPEN_SEARCH = 5,
    };

    enum WeaponProficiency_t : uint8_t
    {
        WEAPON_PROFICIENCY_ITEM_INFO = 0,
        WEAPON_PROFICIENCY_LIST_INFO = 1,
        WEAPON_PROFICIENCY_RESET_PERKS = 2,
        WEAPON_PROFICIENCY_APPLY_PERKS = 3,
        WEAPON_PROFICIENCY_MODIFY_SLOT = 4,
        WEAPON_PROFICIENCY_REFINE_SLOT = 5,
        WEAPON_PROFICIENCY_MAXIMISE_SLOT = 6,
        WEAPON_PROFICIENCY_RESHAPE_SLOT = 7,
        WEAPON_PROFICIENCY_PICK_OFFER = 8,
        WEAPON_PROFICIENCY_CLEAR_SLOT = 9
    };

    enum Imbuement_Window_t : uint8_t
    {
        IMBUEMENT_WINDOW_CHOICE = 0,
        IMBUEMENT_WINDOW_SELECT_ITEM = 1,
        IMBUEMENT_WINDOW_SCROLL = 2
    };

    enum Vocations_t : uint8_t
    {
        NONE = 0,
        KNIGHT = 1,
        PALADIN = 2,
        SORCERER = 3,
        DRUID = 4,
        MONK = 5,
        ELITE_KNIGHT = 11,
        ROYAL_PALADIN = 12,
        MASTER_SORCERER = 13,
        ELDER_DRUID = 14,
        EXALTED_MONK = 15,
        FIRST = KNIGHT,
        LAST = MONK,
    };

    enum PartyAnalyzerAction_t : uint8_t
    {
        PARTYANALYZERACTION_RESET = 0,
        PARTYANALYZERACTION_PRICETYPE = 1,
        PARTYANALYZERACTION_PRICEVALUE = 2,
    };

    enum VocationMonkTypes_t : uint8_t
    {
        TYPES_MONK_HARMONY = 0,
        TYPES_MONK_SERENE = 1,
        TYPES_MONK_VIRTUE = 2,
        TYPES_MONK_LAST
    };

    enum MagicEffectSources : uint8_t
    {
        // always 100% opacity, not configurable
        ME_SOURCE_DEFAULT = 0,
        // configurable with sliders in the client
        ME_SOURCE_OWN = 1,
        ME_SOURCE_OTHER_PLAYER = 2,
        ME_SOURCE_MONSTER = 3,
        ME_SOURCE_BOSS = 4,
        ME_SOURCE_LAST = ME_SOURCE_BOSS
    }; 

    enum class WheelSlots_t : uint8_t
    {
        SLOT_GREEN_200 = 1,
        SLOT_GREEN_TOP_150 = 2,
        SLOT_GREEN_TOP_100 = 3,

        SLOT_RED_TOP_100 = 4,
        SLOT_RED_TOP_150 = 5,
        SLOT_RED_200 = 6,

        SLOT_GREEN_BOTTOM_150 = 7,
        SLOT_GREEN_MIDDLE_100 = 8,
        SLOT_GREEN_TOP_75 = 9,

        SLOT_RED_TOP_75 = 10,
        SLOT_RED_MIDDLE_100 = 11,
        SLOT_RED_BOTTOM_150 = 12,

        SLOT_GREEN_BOTTOM_100 = 13,
        SLOT_GREEN_BOTTOM_75 = 14,
        SLOT_GREEN_50 = 15,

        SLOT_RED_50 = 16,
        SLOT_RED_BOTTOM_75 = 17,
        SLOT_RED_BOTTOM_100 = 18,

        SLOT_BLUE_TOP_100 = 19,
        SLOT_BLUE_TOP_75 = 20,
        SLOT_BLUE_50 = 21,

        SLOT_PURPLE_50 = 22,
        SLOT_PURPLE_TOP_75 = 23,
        SLOT_PURPLE_TOP_100 = 24,

        SLOT_BLUE_TOP_150 = 25,
        SLOT_BLUE_MIDDLE_100 = 26,
        SLOT_BLUE_BOTTOM_75 = 27,

        SLOT_PURPLE_BOTTOM_75 = 28,
        SLOT_PURPLE_MIDDLE_100 = 29,
        SLOT_PURPLE_TOP_150 = 30,

        SLOT_BLUE_200 = 31,
        SLOT_BLUE_BOTTOM_150 = 32,
        SLOT_BLUE_BOTTOM_100 = 33,

        SLOT_PURPLE_BOTTOM_100 = 34,
        SLOT_PURPLE_BOTTOM_150 = 35,
        SLOT_PURPLE_200 = 36,

        SLOT_FIRST = SLOT_GREEN_200,
        SLOT_LAST = SLOT_PURPLE_200
    };
}
}

enum ThingCategory : uint8_t
{
    ThingCategoryItem = 0,
    ThingCategoryCreature,
    ThingCategoryEffect,
    ThingCategoryMissile,
    ThingInvalidCategory,
    ThingExternalTexture,
    ThingLastCategory = ThingInvalidCategory,
};

enum PLAYER_ACTION : uint8_t
{
    PLAYER_ACTION_NONE = 0,
    PLAYER_ACTION_LOOK = 1,
    PLAYER_ACTION_USE = 2,
    PLAYER_ACTION_OPEN = 3,
    PLAYER_ACTION_AUTOWALK_HIGHLIGHT = 4
};

enum ITEM_CATEGORY : uint8_t
{
    ITEM_CATEGORY_ARMORS = 1,
    ITEM_CATEGORY_AMULETS = 2,
    ITEM_CATEGORY_BOOTS = 3,
    ITEM_CATEGORY_CONTAINERS = 4,
    ITEM_CATEGORY_DECORATION = 5,
    ITEM_CATEGORY_FOOD = 6,
    ITEM_CATEGORY_HELMETS_HATS = 7,
    ITEM_CATEGORY_LEGS = 8,
    ITEM_CATEGORY_OTHERS = 9,
    ITEM_CATEGORY_POTIONS = 10,
    ITEM_CATEGORY_RINGS = 11,
    ITEM_CATEGORY_RUNES = 12,
    ITEM_CATEGORY_SHIELDS = 13,
    ITEM_CATEGORY_TOOLS = 14,
    ITEM_CATEGORY_VALUABLES = 15,
    ITEM_CATEGORY_AMMUNITION = 16,
    ITEM_CATEGORY_AXES = 17,
    ITEM_CATEGORY_CLUBS = 18,
    ITEM_CATEGORY_DISTANCE_WEAPONS = 19,
    ITEM_CATEGORY_SWORDS = 20,
    ITEM_CATEGORY_WANDS_RODS = 21,
    ITEM_CATEGORY_PREMIUM_SCROLLS = 22,
    ITEM_CATEGORY_TIBIA_COINS = 23,
    ITEM_CATEGORY_CREATURE_PRODUCTS = 24,
    ITEM_CATEGORY_QUIVER = 25,
    ITEM_CATEGORY_SOUL_CORES = 26,
    ITEM_CATEGORY_FIST_WEAPONS = 27,
};


enum TileThingType : uint32_t
{
    FULL_GROUND = 1 << 0,
    NOT_WALKABLE = 1 << 1,
    NOT_PATHABLE = 1 << 2,
    NOT_SINGLE_DIMENSION = 1 << 3,
    BLOCK_PROJECTTILE = 1 << 4,
    HAS_DISPLACEMENT = 1 << 5,
    IS_NOT_PATHAB = 1 << 6,
    ELEVATION = 1 << 7,
    // IS_OPAQUE = 1 << 8,
    HAS_LIGHT = 1 << 9,
    HAS_TALL_THINGS = 1 << 10,
    HAS_WIDE_THINGS = 1 << 11,
    HAS_TALL_THINGS_2 = 1 << 12,
    HAS_WIDE_THINGS_2 = 1 << 13,
    HAS_WALL = 1 << 14,
    HAS_HOOK_EAST = 1 << 15,
    HAS_HOOK_SOUTH = 1 << 16,
    HAS_CREATURE = 1 << 17,
    HAS_COMMON_ITEM = 1 << 18,
    HAS_TOP_ITEM = 1 << 19,
    HAS_BOTTOM_ITEM = 1 << 20,
    HAS_GROUND_BORDER = 1 << 21,
    HAS_TOP_GROUND_BORDER = 1 << 22,
    HAS_THING_WITH_ELEVATION = 1 << 23,
    IGNORE_LOOK = 1 << 24,
    CORRECT_CORPSE = 1 << 25
};
