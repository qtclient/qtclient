#include "game/gameciptypes.h"
#include <tibia/game/localplayersessionthing.h>
#include <utility>
#include <algorithm>
#include <tibia/game/thingsession_forward.h>
#include <tibia/assets/thingtype.h>
#include <tuple>
#include <client/outfit.h>
#include "creatures/creatureprotocolapply.h"
#include "container/containerciptypes.h"

#include "creatures/creaturestorage.h"
#include "game/cooldownstorage.h"
#include "game/playerdata.h"
#include "shared/clockservice.h"
#include "game/playerskillstats.h"
#include "gamewindow/dialogmanager.h"
#include "gamewindow/gamewindowciptypes.h"
#include "protocol/protocolciptypes.h"

#include <client/const.h>
#include <client/staticdata.h>
#include <client/gamecore.h>
#include "protocol/protocolcodes.h"
#include "network/gameserverpacketprocessor.h"
#include "protocol/clientmessages.h"
#include <tibia/game/gamenotify.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/worldmap/worldmapstorage.h>

#include <protocol.pb.h>

#include <string>
#include <vector>

namespace tibia::game {

namespace {
FPSDropsLogger* g_qtclientFpsDropsLogger = nullptr;

std::vector<std::pair<std::string, std::string>> inspectionDetails(
    const google::protobuf::RepeatedPtrField<::tibia::protobuf::protocol::ProtocolInspectionDetail>& list)
{
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& d : list)
        out.emplace_back(d.label(), d.value());
    return out;
}

/// The appearance of an inspected object, as the dialogs show it.
template<typename Dest>
void fillInspectedAppearance(Dest& dest, const ::tibia::protobuf::protocol::ProtocolInventoryItem& item)
{
    const auto slot = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(item);
    dest.typeId = slot.typeId;
    dest.count = slot.stackable ? std::max<uint16_t>(1, slot.count) : 1;
    dest.tier = slot.tier;
    dest.fluidOrSubtype = slot.fluidOrSplash ? slot.fluid : 0;
    dest.fluidOrSplash = slot.fluidOrSplash;
    ThingType* type = tibia::game::rawThingType(slot.typeId, ThingCategoryItem);
    dest.hookSouth = type && type->isHookSouth();
    dest.hookEast = type && type->isHookEast();
    dest.hasProficiency = type && type->getProficiencyId() != 0;
}

double at(const google::protobuf::RepeatedField<double>& list, const int index)
{
    return index < list.size() ? list.Get(index) : 0.0;
}

template<typename List>
std::vector<CyclopediaCharacterOffenceStats::ElementModifier> elementModifiers(const List& list)
{
    std::vector<CyclopediaCharacterOffenceStats::ElementModifier> out;
    for (const auto& e : list)
        out.push_back({ static_cast<uint8_t>(e.key()), e.value() });
    return out;
}

template<typename List>
std::vector<CyclopediaCharacterOffenceStats::SkillBonus> skillBonuses(const List& list)
{
    std::vector<CyclopediaCharacterOffenceStats::SkillBonus> out;
    for (const auto& e : list)
        out.push_back({ static_cast<uint8_t>(e.skill_id()), e.first(), e.second() });
    return out;
}

CyclopediaCharacterOffenceStats offenceStats(const ::tibia::protobuf::protocol::ProtocolCharacterInfoOffenceStats& o)
{
    CyclopediaCharacterOffenceStats data{};
    data.critChanceTotal = at(o.crit_chance(), 0);
    data.critChanceEquipament = at(o.crit_chance(), 1);
    data.critChanceFlat = at(o.crit_chance(), 2);
    data.critChanceImbuement = at(o.crit_chance(), 3);
    data.critChanceWheel = at(o.crit_chance(), 4);
    data.critChanceConcoction = at(o.crit_chance(), 5);
    data.critDamageTotal = at(o.crit_damage(), 0);
    data.critDamageEquipament = at(o.crit_damage(), 1);
    data.critDamageFlat = at(o.crit_damage(), 2);
    data.critDamageImbuement = at(o.crit_damage(), 3);
    data.critDamageWheel = at(o.crit_damage(), 4);
    data.critDamageConcoction = at(o.crit_damage(), 5);
    data.lifeLeechTotal = at(o.life_leech(), 0);
    data.lifeLeechEquipament = at(o.life_leech(), 1);
    data.lifeLeechImbuement = at(o.life_leech(), 2);
    data.lifeLeechWheel = at(o.life_leech(), 3);
    data.lifeLeechEventBonus = at(o.life_leech(), 4);
    data.manaLeechTotal = at(o.mana_leech(), 0);
    data.manaLeechEquipament = at(o.mana_leech(), 1);
    data.manaLeechImbuement = at(o.mana_leech(), 2);
    data.manaLeechWheel = at(o.mana_leech(), 3);
    data.manaLeechEventBonus = at(o.mana_leech(), 4);
    data.onslaught = at(o.onslaught(), 0);
    data.onslaughtBase = at(o.onslaught(), 1);
    data.onslaughtBonus = at(o.onslaught(), 2);
    data.onslaughtEventBonus = at(o.onslaught(), 3);
    data.cleavePercent = o.cleave();
    for (const uint32_t value : o.perfect_shot())
        data.perfectShotDamage.push_back(static_cast<uint16_t>(value));
    const auto flat = [&o](const int i) { return static_cast<uint16_t>(i < o.flat_damage_size() ? o.flat_damage(i) : 0); };
    data.flatDamage = flat(0);
    data.flatDamageBase = flat(1);
    data.flatDamageWheel = flat(2);
    data.weaponAttack = static_cast<uint16_t>(o.weapon_attack());
    data.weaponFlatModifier = static_cast<uint16_t>(o.weapon_flat_modifier());
    data.weaponDamage = static_cast<uint16_t>(o.weapon_damage());
    data.weaponSkillType = static_cast<uint8_t>(o.weapon_skill_type());
    data.weaponSkillLevel = static_cast<uint16_t>(o.weapon_skill_level());
    data.weaponSkillModifier = static_cast<uint16_t>(o.weapon_skill_modifier());
    data.weaponElement = static_cast<uint8_t>(o.weapon_element());
    data.weaponElementDamage = o.converted_damage();
    data.weaponElementType = static_cast<uint8_t>(o.converted_element());
    for (const auto& a : o.accuracy())
        data.weaponAccuracy.push_back({ static_cast<uint8_t>(a.key()), a.value() });
    data.damagePowerfulFoes = o.powerful_foes();
    for (const auto& t : o.specific_targets())
        data.damageSpecificTargets.push_back({ t.name(), t.value() });
    data.damageElements = elementModifiers(o.crit_chance_elements());
    data.offensiveRuneDamage = o.offensive_runes();
    data.autoAttackDamage = o.auto_attack();
    data.critDamageElements = elementModifiers(o.crit_damage_elements());
    data.critDamageOffensiveRunes = o.crit_damage_runes();
    data.critDamageAutoAttack = o.crit_damage_auto_attack();
    data.lifeGainHit = static_cast<uint16_t>(o.life_on_hit());
    data.manaGainHit = static_cast<uint16_t>(o.mana_on_hit());
    data.lifeGainKill = static_cast<uint16_t>(o.life_on_kill());
    data.manaGainKill = static_cast<uint16_t>(o.mana_on_kill());
    data.extraDamageSkills = skillBonuses(o.extra_damage_skills());
    data.extraDamageSpells = skillBonuses(o.extra_damage_spells());
    data.extraHealingSpells = skillBonuses(o.extra_healing_spells());
    data.damageHighHp = o.damage_high_health();
    data.damageLowHp = o.damage_low_health();
    data.armorPenetration = o.armor_penetration();
    data.elementalPierce = elementModifiers(o.elemental_pierce());
    return data;
}

CyclopediaCharacterDefenceStats defenceStats(const ::tibia::protobuf::protocol::ProtocolCharacterInfoDefenceStats& d)
{
    CyclopediaCharacterDefenceStats data{};
    data.dodgeTotal = at(d.dodge(), 0);
    data.dodgeBase = at(d.dodge(), 1);
    data.dodgeBonus = at(d.dodge(), 2);
    data.dodgeWheel = at(d.dodge(), 4);
    data.magicShieldCapacity = d.magic_shield();
    data.magicShieldCapacityFlat = static_cast<uint16_t>(d.magic_shield_flat());
    data.magicShieldCapacityPercent = d.magic_shield_percent();
    data.reflectPhysical = static_cast<uint16_t>(d.reflect_physical());
    data.armor = static_cast<uint16_t>(d.armor());
    data.defense = static_cast<uint16_t>(d.defence());
    data.defenseEquipment = static_cast<uint16_t>(d.defence_equipment());
    data.defenseSkillType = static_cast<uint8_t>(d.defence_skill_type());
    data.shieldingSkill = static_cast<uint16_t>(d.shielding());
    data.defenseWheel = static_cast<uint16_t>(d.defence_wheel());
    data.mitigation = at(d.mitigation(), 0);
    data.mitigationBase = at(d.mitigation(), 1);
    data.mitigationEquipment = at(d.mitigation(), 2);
    data.mitigationShield = at(d.mitigation(), 3);
    data.mitigationWheel = at(d.mitigation(), 4);
    for (const auto& r : d.resistances()) {
        if (r.kind() == 4) // elemental resistances; the other kinds have no row
            data.resistances.push_back({ static_cast<uint8_t>(r.element()), r.value() });
    }
    return data;
}

CyclopediaCharacterMiscStats miscStats(const ::tibia::protobuf::protocol::ProtocolCharacterInfoMiscStats& m)
{
    CyclopediaCharacterMiscStats data{};
    data.momentumTotal = at(m.momentum(), 0);
    data.momentumBase = at(m.momentum(), 1);
    data.momentumBonus = at(m.momentum(), 2);
    data.momentumWheel = at(m.momentum(), 3);
    data.dodgeTotal = at(m.dodge(), 0);
    data.dodgeBase = at(m.dodge(), 1);
    data.dodgeBonus = at(m.dodge(), 2);
    data.dodgeWheel = at(m.dodge(), 3);
    data.damageReflectionTotal = at(m.damage_reflection(), 0);
    data.damageReflectionBase = at(m.damage_reflection(), 1);
    data.damageReflectionBonus = at(m.damage_reflection(), 2);
    data.haveBlesses = static_cast<uint8_t>(m.blessings());
    data.totalBlesses = static_cast<uint8_t>(m.total_blessings());
    for (const auto& c : m.concoctions())
        data.concoctions.push_back({ static_cast<uint16_t>(c.id()), c.duration() });
    for (const auto& f : m.foods())
        data.activeFoods.push_back({ static_cast<uint16_t>(f.id()), f.duration() });
    const auto augments = [](const auto& list) {
        std::vector<CyclopediaCharacterMiscStats::Augment> out;
        for (const auto& a : list)
            out.push_back({ static_cast<uint16_t>(a.spell_id()), static_cast<uint8_t>(a.type()), a.value() });
        return out;
    };
    data.weaponProficiencyAugments = augments(m.proficiency_augments());
    data.wheelAugments = augments(m.wheel_augments());
    data.equippedAugments = augments(m.equipped_augments());
    return data;
}

std::vector<ItemSummary> itemCounts(
    const google::protobuf::RepeatedPtrField<::tibia::protobuf::protocol::ProtocolCharacterInfoItemCount>& list)
{
    std::vector<ItemSummary> out;
    for (const auto& e : list)
        out.push_back({ static_cast<uint16_t>(e.object_id()), static_cast<uint8_t>(e.tier()), e.amount() });
    return out;
}

/// The 0xDA character information, one page of the character dialog.
void applyCharacterInfo(const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& m)
{
    const int type = static_cast<int>(m.info_type());
    if (m.error_code() != 0) {
        tibia::game::notify("onCyclopediaCharacterInfoError", type, static_cast<int>(m.error_code()));
        return;
    }
    switch (type) {
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION: {
        const auto& b = m.base();
        tibia::game::notify("onCyclopediaCharacterBaseInformation", b.name(), b.vocation(), static_cast<uint16_t>(b.level()),
                            creatures::outfitFromProtocol(b.outfit(), nullptr), b.own_character(), b.title());
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_GENERALSTATS: {
        const auto& g = m.general();
        CyclopediaCharacterGeneralStats stats{};
        stats.experience = g.experience();
        stats.level = static_cast<uint16_t>(g.level());
        stats.levelPercent = static_cast<uint16_t>(g.level_percent() / 100);
        stats.baseExpGain = static_cast<uint16_t>(g.base_xp_gain());
        stats.lowLevelExpBonus = static_cast<uint16_t>(g.low_level_xp_bonus());
        stats.XpBoostPercent = static_cast<uint16_t>(g.xp_boost_percent());
        stats.staminaExpBonus = static_cast<uint16_t>(g.stamina_xp_bonus());
        stats.XpBoostBonusRemainingTime = static_cast<uint16_t>(g.xp_boost_remaining());
        stats.canBuyXpBoost = g.can_buy_xp_boost() ? 1 : 0;
        stats.health = g.health();
        stats.maxHealth = g.max_health();
        stats.mana = g.mana();
        stats.maxMana = g.max_mana();
        stats.soul = static_cast<uint8_t>(g.soul());
        stats.staminaMinutes = static_cast<uint16_t>(g.stamina_minutes());
        stats.regenerationCondition = static_cast<uint16_t>(g.regeneration());
        stats.offlineTrainingTime = static_cast<uint16_t>(g.offline_training());
        stats.speed = static_cast<uint16_t>(g.speed());
        stats.baseSpeed = static_cast<uint16_t>(g.base_speed());
        stats.capacity = g.capacity();
        stats.baseCapacity = g.base_capacity();
        stats.freeCapacity = g.free_capacity();
        std::vector<std::vector<uint16_t>> skills;
        for (int i = 0; i < g.skills_size(); ++i) {
            const auto& s = g.skills(i);
            if (i == 0) { // the magic level leads the list
                stats.magicLevel = static_cast<uint16_t>(s.level());
                stats.baseMagicLevel = static_cast<uint16_t>(s.base_level());
                stats.loyaltyMagicLevel = static_cast<uint16_t>(s.loyalty_level());
                stats.magicLevelPercent = static_cast<uint16_t>(s.percent());
                continue;
            }
            skills.push_back({ static_cast<uint16_t>(s.level()), static_cast<uint16_t>(s.base_level()),
                               static_cast<uint16_t>(s.percent() / 100) });
        }
        std::vector<std::tuple<uint8_t, uint16_t>> combats;
        for (const auto& e : g.specialized_magic_levels())
            combats.emplace_back(static_cast<uint8_t>(e.element()), static_cast<uint16_t>(e.value()));
        tibia::game::processCyclopediaCharacterGeneralStats(stats, skills, combats);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS: {
        tibia::game::notify("onCyclopediaCharacterPagination", type, static_cast<int>(m.current_page()), static_cast<int>(m.page_count()));
        CyclopediaCharacterRecentDeaths data;
        for (const auto& d : m.deaths())
            data.entries.push_back({ d.timestamp(), d.cause() });
        tibia::game::processCyclopediaCharacterRecentDeaths(data);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS: {
        tibia::game::notify("onCyclopediaCharacterPagination", type, static_cast<int>(m.current_page()), static_cast<int>(m.page_count()));
        CyclopediaCharacterRecentPvPKills data;
        for (const auto& k : m.pvp_kills())
            data.entries.push_back({ k.timestamp(), k.description(), static_cast<uint8_t>(k.status()) });
        tibia::game::processCyclopediaCharacterRecentPvpKills(data);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_ACHIEVEMENTS: {
        std::vector<std::tuple<uint16_t, uint32_t, bool, std::string, std::string, uint8_t>> achievements;
        for (const auto& a : m.achievements())
            achievements.emplace_back(static_cast<uint16_t>(a.id()), a.timestamp(), a.secret(), a.name(), a.description(),
                                      static_cast<uint8_t>(a.grade()));
        tibia::game::notify("onCyclopediaCharacterAchievements", static_cast<int>(m.achievement_points()),
                            static_cast<int>(m.secret_achievements()), achievements);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_ITEMSUMMARY: {
        CyclopediaCharacterItemSummary data;
        data.inventory = itemCounts(m.inventory_items());
        data.store = itemCounts(m.store_items());
        data.stash = itemCounts(m.stash_items());
        data.depot = itemCounts(m.depot_items());
        data.inbox = itemCounts(m.inbox_items());
        tibia::game::processCyclopediaCharacterItemSummary(data);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_OUTFITSMOUNTS: {
        OutfitColorStruct colors{};
        colors.lookHead = static_cast<uint8_t>(m.outfit_colors().head());
        colors.lookBody = static_cast<uint8_t>(m.outfit_colors().body());
        colors.lookLegs = static_cast<uint8_t>(m.outfit_colors().legs());
        colors.lookFeet = static_cast<uint8_t>(m.outfit_colors().feet());
        colors.lookMountHead = static_cast<uint8_t>(m.mount_colors().head());
        colors.lookMountBody = static_cast<uint8_t>(m.mount_colors().body());
        colors.lookMountLegs = static_cast<uint8_t>(m.mount_colors().legs());
        colors.lookMountFeet = static_cast<uint8_t>(m.mount_colors().feet());
        std::vector<CharacterInfoOutfits> outfits;
        for (const auto& o : m.outfits())
            outfits.push_back({ static_cast<uint16_t>(o.look_type()), o.name(), static_cast<uint8_t>(o.addons()),
                                static_cast<uint8_t>(o.source()), o.current() });
        std::vector<CharacterInfoMounts> mounts;
        for (const auto& o : m.mounts())
            mounts.push_back({ static_cast<uint16_t>(o.look_type()), o.name(), static_cast<uint8_t>(o.source()), o.current() });
        std::vector<CharacterInfoFamiliar> familiars;
        for (const auto& o : m.familiars())
            familiars.push_back({ static_cast<uint16_t>(o.look_type()), o.name(), static_cast<uint8_t>(o.source()), o.current() });
        tibia::game::processCyclopediaCharacterAppearances(colors, outfits, mounts, familiars);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_STORESUMMARY: {
        std::vector<std::tuple<std::string, uint8_t>> blessings;
        for (const auto& b : m.blessings())
            blessings.emplace_back(b.name(), static_cast<uint8_t>(b.count()));
        std::vector<uint16_t> jobs;
        for (const uint32_t job : m.hireling_jobs())
            jobs.push_back(static_cast<uint16_t>(job));
        std::vector<std::tuple<uint16_t, std::string, uint8_t>> houseItems;
        for (const auto& h : m.house_items())
            houseItems.emplace_back(static_cast<uint16_t>(h.object_id()), h.name(), static_cast<uint8_t>(h.count()));
        std::vector<std::tuple<std::string, uint8_t>> hirelingOutfits;
        for (const auto& o : m.hireling_outfits())
            hirelingOutfits.emplace_back(o.name(), static_cast<uint8_t>(o.count()));
        tibia::game::notify("onParseCyclopediaStoreSummary", m.xp_boost_time(), m.daily_xp_boost_time(), blessings,
                            static_cast<uint8_t>(m.prey_slots()), static_cast<uint8_t>(m.prey_wildcards()), m.weekly_task_expansion(),
                            static_cast<uint8_t>(m.instant_rewards()), m.charm_expansion(), static_cast<uint8_t>(m.hirelings()), jobs,
                            houseItems, hirelingOutfits);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_INSPECTION: {
        CyclopediaCharacterInspection data;
        data.inspectionType = tibia::client::INSPECT_CYCLOPEDIA;
        for (const auto& o : m.inspected_objects()) {
            InspectionInventoryItem entry;
            entry.slot = static_cast<uint8_t>(o.slot());
            entry.name = o.name();
            fillInspectedAppearance(entry, o.item());
            entry.imbuements.assign(o.imbuement_icons().begin(), o.imbuement_icons().end());
            entry.descriptions = inspectionDetails(o.details());
            data.inventoryItems.push_back(std::move(entry));
        }
        data.playerName = m.player_name();
        data.outfit = creatures::outfitFromProtocol(m.outfit(), nullptr);
        data.playerDescriptions = inspectionDetails(m.player_details());
        tibia::game::processCyclopediaCharacterInspection(data);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_BADGES: {
        std::vector<std::tuple<uint32_t, std::string>> badges;
        for (const auto& b : m.badges())
            badges.emplace_back(b.id(), b.name());
        tibia::game::processCyclopediaCharacterGeneralStatsBadge(m.show_account() ? 1 : 0, m.online() ? 1 : 0, m.premium() ? 1 : 0,
                                                                 m.loyalty_title(), badges);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_TITLES: {
        std::vector<std::tuple<uint8_t, std::string, std::string, bool, bool>> titles;
        for (const auto& t : m.titles())
            titles.emplace_back(static_cast<uint8_t>(t.id()), t.name(), t.description(), t.permanent(), t.unlocked());
        tibia::game::notify("onCyclopediaCharacterTitles", static_cast<int>(m.current_title()), titles);
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_OFFENCESTATS:
        tibia::game::processCyclopediaCharacterOffenceStats(offenceStats(m.offence()));
        break;
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_DEFENCESTATS:
        tibia::game::processCyclopediaCharacterDefenceStats(defenceStats(m.defence()));
        break;
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_MISCSTATS:
        tibia::game::processCyclopediaCharacterMiscStats(miscStats(m.misc()));
        break;
    default:
        break;
    }
}
} // namespace

FPSDropsLogger::FPSDropsLogger(QObject* parent)
    : cip::tibia::game::FPSDropsLogger(parent)
{
    // Dialog stack is created after TGameClient ctor (ownProductSurface); bind
    // lazily once the singleton exists. Never owns login/session.
    m_gameStartedConn = connect(TGameNotify::instance(), &TGameNotify::gameStarted, this, [this] {
        ensureDialogStackBound();
    });
}

FPSDropsLogger::~FPSDropsLogger()
{
    if (g_qtclientFpsDropsLogger == this)
        g_qtclientFpsDropsLogger = nullptr;
    if (m_dialogConn)
        QObject::disconnect(m_dialogConn);
    if (m_gameStartedConn)
        QObject::disconnect(m_gameStartedConn);
}

void FPSDropsLogger::ensureDialogStackBound()
{
    if (m_dialogStackBound)
        return;
    gamewindow::TDialogStackController* stack = gamewindow::TDialogStackController::instance();
    if (!stack)
        return;
    m_dialogConn = connect(stack, &gamewindow::TDialogStackController::dialogVisible, this,
                           &FPSDropsLogger::onDialogVisibleChanged);
    m_dialogStackBound = true;
    onDialogVisibleChanged(!stack->isEmpty());
}

void FPSDropsLogger::onDialogVisibleChanged(const bool Visible)
{
    m_dialogVisible = Visible;
}

FPSDropsLogger* FPSDropsLogger::instance()
{
    return g_qtclientFpsDropsLogger;
}

namespace {
struct CipTypedKeep {
    TExperienceCalculator tExperienceCalculator;
    TGameSessionMetaInfo tGameSessionMetaInfo;
    TGameWorldVolumeStorage tGameWorldVolumeStorage;
    TGameserverGameSessionMetaInfo tGameserverGameSessionMetaInfo;
    TSessiondumpGameSessionMetaInfo tSessiondumpGameSessionMetaInfo;
    TTraderStorage tTraderStorage;
    TTutorialSessiondumpGameSessionMetaInfo tTutorialSessiondumpGameSessionMetaInfo;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

TGameProtocolMessageHandler* g_liveGameProtocolHandler = nullptr;

using Args = std::vector<std::any>;
using client::gamecore::argBool;
using client::gamecore::argInt;

using ::tibia::protobuf::protocol::GameserverMessageCloseRewardWall;
using ::tibia::protobuf::protocol::GameserverMessageHirelingNameChange;
using ::tibia::protobuf::protocol::GameserverMessageLoginAdvice;
using ::tibia::protobuf::protocol::GameserverMessageClientCheck;
using ::tibia::protobuf::protocol::GameserverMessageLoginChallenge;
using ::tibia::protobuf::protocol::GameserverMessageLoginError;
using ::tibia::protobuf::protocol::GameserverMessageLoginSuccess;
using ::tibia::protobuf::protocol::GameserverMessageLoginWait;
using ::tibia::protobuf::protocol::GameserverMessageMarketLeave;
using ::tibia::protobuf::protocol::GameserverMessagePartyLeft;
using ::tibia::protobuf::protocol::GameserverMessagePendingStateEntered;
using ::tibia::protobuf::protocol::GameserverMessagePing;
using ::tibia::protobuf::protocol::GameserverMessagePingBack;
using ::tibia::protobuf::protocol::GameserverMessageSessionDumpMarker;
using ::tibia::protobuf::protocol::GameserverMessageSessionDumpStart;
using ::tibia::protobuf::protocol::GameserverMessageSessionEndInformation;
using ::tibia::protobuf::protocol::GameserverMessageDead;
using ::tibia::protobuf::protocol::GameserverMessageSwitchPreset;
using ::tibia::protobuf::protocol::GameserverMessagePassiveAbilityData;
using ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog;
using ::tibia::protobuf::protocol::GameserverMessageUnlockedSpells;
using ::tibia::protobuf::protocol::GameserverMessageBlessings;
using ::tibia::protobuf::protocol::GameserverMessageOutfit;
using ::tibia::protobuf::protocol::GameserverMessageInspectionList;
using ::tibia::protobuf::protocol::GameserverMessageGameEvent;
using ::tibia::protobuf::protocol::GameserverMessageOutfitMemorial;
using ::tibia::protobuf::protocol::GameserverMessageConfigureCreaturePodium;
using ::tibia::protobuf::protocol::GameserverMessageEditList;
using ::tibia::protobuf::protocol::GameserverMessageEditText;
using ::tibia::protobuf::protocol::GameserverMessagePremiumTrigger;
using ::tibia::protobuf::protocol::GameserverMessageReadyForSecondaryConnection;
using ::tibia::protobuf::protocol::GameserverMessageInspectionState;
using ::tibia::protobuf::protocol::GameserverMessageShowMultiOfflineTrainingDialog;
using ::tibia::protobuf::protocol::GameserverMessageSnapBack;
using ::tibia::protobuf::protocol::GameserverMessageSpellDataModification;
using ::tibia::protobuf::protocol::GameserverMessageStop;
using ::tibia::protobuf::protocol::GameserverMessageWorldEntered;

} // namespace

TGameProtocolMessageHandler::TGameProtocolMessageHandler(QObject* parent)
    : cip::tibia::game::TGameProtocolMessageHandler(parent)
{
}

TGameProtocolMessageHandler* TGameProtocolMessageHandler::liveInstance()
{
    return g_liveGameProtocolHandler;
}

void TGameProtocolMessageHandler::setLiveInstance(TGameProtocolMessageHandler* handler)
{
    g_liveGameProtocolHandler = handler;
}

void TGameProtocolMessageHandler::handleSessionDumpStartMessage(
    const GameserverMessageSessionDumpStart& SessionDumpStartMessage)
{
    const std::string key =
        SessionDumpStartMessage.has_session_key() ? SessionDumpStartMessage.session_key() : std::string();
    tibia::game::notify("onSessionDumpStart", key);
}

void TGameProtocolMessageHandler::handlePendingStateEnteredMessage(
    const GameserverMessagePendingStateEntered& /*PendingStateEnteredMessage*/)
{
    tibia::game::localPlayerSetPendingGame(true);
    tibia::game::notify("onPendingGame");
    if (network::TGameserverNetworkPacketProcessor::live())
        protocol::gameclient::sendEnterGame();
}

void TGameProtocolMessageHandler::handleSessionEndInformationMessage(
    const GameserverMessageSessionEndInformation& Message)
{
    tibia::game::notify("onSessionEnd", static_cast<uint8_t>(Message.reason()));
}

void TGameProtocolMessageHandler::handleShowMultiOfflineTrainingDialogMessage(
    const GameserverMessageShowMultiOfflineTrainingDialog& /*Message*/)
{
    tibia::game::notify("onMultiOfflineTrainingDialog");
}

void TGameProtocolMessageHandler::handleLoginSuccessMessage(
    const GameserverMessageLoginSuccess& LoginSuccessMessage)
{
    const uint32_t playerId = LoginSuccessMessage.player_id();
    const uint16_t serverBeat = static_cast<uint16_t>(LoginSuccessMessage.beat_duration());

    tibia::creatures::TCreatureMovementSpeedCalculator& speedCalculator =
        tibia::worldmap::TWorldMapStorage::ensureStorage()->speedCalculator();
    speedCalculator.setSpeedFormula(LoginSuccessMessage.speed_a(), LoginSuccessMessage.speed_b(),
                                    LoginSuccessMessage.speed_c());
    speedCalculator.setServerBeat(serverBeat);

    bool canReportBugs = false;

    setExpertPvpMode(LoginSuccessMessage.expert_pvp() != 0);

    const std::string url =
        LoginSuccessMessage.has_store_url() ? LoginSuccessMessage.store_url() : std::string();
    tibia::game::notify("onStoreInit", url,
                        static_cast<uint16_t>(LoginSuccessMessage.coins_packet_size()));

    setCanExivaOptions(LoginSuccessMessage.exiva_options() != 0);

    tibia::game::localPlayerSetId(playerId);
    setServerBeat(serverBeat);
    setCanReportBugs(canReportBugs);
    tibia::game::notify("onLogin");
}

void TGameProtocolMessageHandler::onServerMessageEnqueued(int opcode)
{

    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;

    switch (opcode) {
    case 4: {
        if (!shadow->HasExtension(GameserverMessageSessionDumpStart::gameserver_message_session_dump_start))
            return;
        handleSessionDumpStartMessage(
            shadow->GetExtension(GameserverMessageSessionDumpStart::gameserver_message_session_dump_start));
        break;
    }
    case 5: {
        // CIP has queue receivedSessionDumpMarkerMessage / session signals only —
        // no handleSessionDumpMarkerMessage. Same side effects as parseSessionDumpMarker
        if (!shadow->HasExtension(
                GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageSessionDumpMarker::gameserver_message_session_dump_marker);
        tibia::game::notify("onSessionDumpMarker",
                            m.has_marker() ? m.marker() : std::string());
        break;
    }
    case 6: {
        // CIP: receivedPartyLeftMessage only — no handlePartyLeftMessage.
        if (!shadow->HasExtension(GameserverMessagePartyLeft::gameserver_message_party_left))
            return;
        tibia::game::notify("onPartyLeft");
        break;
    }
    case 10: {
        if (!shadow->HasExtension(
                GameserverMessagePendingStateEntered::gameserver_message_pending_state_entered))
            return;
        handlePendingStateEnteredMessage(shadow->GetExtension(
            GameserverMessagePendingStateEntered::gameserver_message_pending_state_entered));
        break;
    }
    case 15: {
        // CIP: receivedWorldEnteredMessage only — no handleWorldEnteredMessage.
        if (!shadow->HasExtension(GameserverMessageWorldEntered::gameserver_message_world_entered))
            return;
        processWorldEntered();
        break;
    }
    case 20: {
        // CIP: receivedLoginErrorMessage + gameLoginErrorMessage — no handleLoginErrorMessage.
        if (!shadow->HasExtension(GameserverMessageLoginError::gameserver_message_login_error))
            return;
        const auto& m = shadow->GetExtension(GameserverMessageLoginError::gameserver_message_login_error);
        tibia::game::notify("onLoginError", m.has_error() ? m.error() : std::string());
        break;
    }
    case 21: {
        if (!shadow->HasExtension(GameserverMessageLoginAdvice::gameserver_message_login_advice))
            return;
        const auto& m = shadow->GetExtension(GameserverMessageLoginAdvice::gameserver_message_login_advice);
        tibia::game::notify("onLoginAdvice", m.has_message() ? m.message() : std::string());
        break;
    }
    case 22: {
        if (!shadow->HasExtension(GameserverMessageLoginWait::gameserver_message_login_wait))
            return;
        const auto& m = shadow->GetExtension(GameserverMessageLoginWait::gameserver_message_login_wait);
        tibia::game::notify("onLoginWait", m.has_message() ? m.message() : std::string(),
                            static_cast<uint8_t>(m.time()));
        break;
    }
    case 23: {
        if (!shadow->HasExtension(GameserverMessageLoginSuccess::gameserver_message_login_success))
            return;
        handleLoginSuccessMessage(
            shadow->GetExtension(GameserverMessageLoginSuccess::gameserver_message_login_success));
        break;
    }
    case 40:
        if (shadow->HasExtension(GameserverMessageDead::gameserver_message_dead)) {
            const auto& m = shadow->GetExtension(GameserverMessageDead::gameserver_message_dead);
            // The penalty is only sent for a regular death; otherwise it stays 100.
            tibia::game::processDeath(static_cast<uint8_t>(m.death_type()),
                                      m.has_penalty() ? static_cast<uint8_t>(m.penalty()) : uint8_t(100));
        }
        break;
    case 11:
        if (shadow->HasExtension(GameserverMessageReadyForSecondaryConnection::gameserver_message_ready_for_secondary_connection))
            tibia::game::processReadyForSecondaryConnection(
                shadow->GetExtension(GameserverMessageReadyForSecondaryConnection::gameserver_message_ready_for_secondary_connection).token());
        break;
    case 119:
        if (shadow->HasExtension(GameserverMessageInspectionState::gameserver_message_inspection_state)) {
            const auto& m = shadow->GetExtension(GameserverMessageInspectionState::gameserver_message_inspection_state);
            tibia::game::processInspectionState(m.creature_id(), static_cast<uint8_t>(m.state()));
        }
        break;
    case 117:
        if (shadow->HasExtension(GameserverMessageGameEvent::gameserver_message_game_event)) {
            const auto& m = shadow->GetExtension(GameserverMessageGameEvent::gameserver_message_game_event);
            const auto type = static_cast<tibia::client::ClientEventType_t>(m.event_type());
            const auto small = static_cast<uint8_t>(m.small_value());
            const auto value = static_cast<uint16_t>(m.value());
            switch (m.event_type()) {
            case tibia::client::CLIENT_EVENT_TYPE_SIMPLE:
                if (small < tibia::client::CLIENT_EVENT_ATTACKSTOPPED) {
                    if (const auto& player = tibia::game::localPlayer())
                        player->takeScreenshot(small);
                } else {
                    tibia::game::notify("onClientEvent", type, static_cast<tibia::client::ClientEvent_t>(small));
                }
                break;
            case tibia::client::CLIENT_EVENT_TYPE_ACHIEVEMENT:
            case tibia::client::CLIENT_EVENT_TYPE_TITLE:
                tibia::game::notify("onClientEvent", type, m.text());
                break;
            case tibia::client::CLIENT_EVENT_TYPE_SKILL:
                tibia::game::notify("onClientEvent", type, small, value);
                break;
            case tibia::client::CLIENT_EVENT_TYPE_BESTIARY:
            case tibia::client::CLIENT_EVENT_TYPE_BOSSTIARY:
                tibia::game::notify("onClientEvent", type, value, small);
                break;
            case tibia::client::CLIENT_EVENT_TYPE_QUEST:
                tibia::game::notify("onClientEvent", type, m.text(), small);
                break;
            case tibia::client::CLIENT_EVENT_TYPE_COSMETIC:
                tibia::game::notify("onClientEvent", type, value, m.text(), small);
                break;
            case tibia::client::CLIENT_EVENT_TYPE_PROFICIENCY:
                tibia::game::notify("onClientEvent", type, value, m.text());
                break;
            case tibia::client::CLIENT_EVENT_TYPE_SPELL_UNLOCKED:
                tibia::game::notify("onClientEvent", type, static_cast<uint32_t>(m.large_value()));
                break;
            case tibia::client::CLIENT_EVENT_TYPE_LEADER_MONSTER:
                tibia::game::notify("onClientEvent", type, value, static_cast<uint32_t>(m.large_value()));
                break;
            default:
                tibia::game::notify("onClientEvent", type, value);
                break;
            }
        }
        break;
    case 118:
        if (shadow->HasExtension(GameserverMessageInspectionList::gameserver_message_inspection_list)) {
            const auto& m = shadow->GetExtension(GameserverMessageInspectionList::gameserver_message_inspection_list);
            const auto details = [](const auto& list) { return inspectionDetails(list); };
            const auto fill = [](auto& dest, const auto& item) { fillInspectedAppearance(dest, item); };
            if (m.window_type() == 1) {
                CyclopediaCharacterInspection data;
                data.inspectionType = tibia::client::INSPECT_CYCLOPEDIA;
                data.creatureId = m.creature_id();
                for (const auto& o : m.objects()) {
                    InspectionInventoryItem entry;
                    entry.slot = static_cast<uint8_t>(o.slot());
                    entry.name = o.name();
                    fill(entry, o.item());
                    entry.imbuements.assign(o.imbuement_icons().begin(), o.imbuement_icons().end());
                    entry.descriptions = details(o.details());
                    data.inventoryItems.push_back(std::move(entry));
                }
                data.playerName = m.player_name();
                // The server sends no outfit; the inspected creature shows its current one.
                if (auto* storage = tibia::creatures::TCreatureStorage::instance()) {
                    if (const auto row = storage->creatureAt(m.creature_id()))
                        data.outfit = row->outfit();
                }
                data.playerDescriptions = details(m.player_details());
                tibia::game::notify("onParseCharacterInspection", data);
                tibia::game::notify("onParseCyclopediaCharacterInspection", data);
            } else if (m.objects_size() > 0) {
                const auto& o = m.objects(0);
                ItemInspectionData data;
                data.inspectionType = static_cast<uint8_t>(m.inspection_type());
                data.creatureId = m.creature_id();
                data.name = o.name();
                fill(data, o.item());
                data.imbuements.assign(o.imbuement_icons().begin(), o.imbuement_icons().end());
                data.descriptions = details(o.details());
                tibia::game::processItemDetail(data);
            }
        }
        break;
    case 218:
        if (shadow->HasExtension(protobuf::protocol::GameserverMessageCyclopediaCharacterInfo::gameserver_message_cyclopedia_character_info))
            applyCharacterInfo(shadow->GetExtension(protobuf::protocol::GameserverMessageCyclopediaCharacterInfo::gameserver_message_cyclopedia_character_info));
        break;
    case 176:
        if (shadow->HasExtension(GameserverMessageOutfitMemorial::gameserver_message_outfit_memorial)) {
            const auto& m = shadow->GetExtension(GameserverMessageOutfitMemorial::gameserver_message_outfit_memorial);
            const auto names = [](const auto& list) { return std::vector<std::string>(list.begin(), list.end()); };
            OutfitMemorialData data;
            data.baseGoldenOutfitCost = m.base_golden_cost();
            data.oneAddonGoldenOutfitCost = m.one_addon_golden_cost();
            data.fullGoldenOutfitCost = m.full_golden_cost();
            data.baseGoldenOutfitOwners = names(m.base_golden_owners());
            data.oneAddonGoldenOutfitOwners = names(m.one_addon_golden_owners());
            data.fullGoldenOutfitOwners = names(m.full_golden_owners());
            data.baseTokenOutfitSilverToken = static_cast<uint16_t>(m.base_silver_tokens());
            data.baseTokenOutfitGoldToken = static_cast<uint16_t>(m.base_gold_tokens());
            data.oneAddonTokenOutfitSilverToken = static_cast<uint16_t>(m.one_addon_silver_tokens());
            data.oneAddonTokenOutfitGoldToken = static_cast<uint16_t>(m.one_addon_gold_tokens());
            data.fullTokenOutfitSilverToken = static_cast<uint16_t>(m.full_silver_tokens());
            data.fullTokenOutfitGoldToken = static_cast<uint16_t>(m.full_gold_tokens());
            data.baseTokenOutfitOwners = names(m.base_token_owners());
            data.oneAddonTokenOutfitOwners = names(m.one_addon_token_owners());
            data.fullTokenOutfitOwners = names(m.full_token_owners());
            tibia::game::processOutfitMemorial(data);
        }
        break;
    case 200:
        if (shadow->HasExtension(GameserverMessageOutfit::gameserver_message_outfit)) {
            const auto& m = shadow->GetExtension(GameserverMessageOutfit::gameserver_message_outfit);
            Outfit current = creatures::outfitFromProtocol(m.outfit(), nullptr);
            current.setMount(static_cast<uint16_t>(m.mount()));
            current.setMountColors(static_cast<uint8_t>(m.mount_colors().head()), static_cast<uint8_t>(m.mount_colors().body()),
                                   static_cast<uint8_t>(m.mount_colors().legs()), static_cast<uint8_t>(m.mount_colors().feet()));
            current.setFamiliar(static_cast<uint16_t>(m.familiar()));
            std::vector<std::tuple<uint16_t, std::string, uint8_t, uint8_t>> outfits;
            std::vector<uint32_t> outfitOffers, mountOffers, familiarOffers;
            for (const auto& o : m.outfits()) {
                outfits.emplace_back(static_cast<uint16_t>(o.look_type()), o.name(), static_cast<uint8_t>(o.addons()),
                                     static_cast<uint8_t>(o.mode()));
                outfitOffers.push_back(o.offer_id());
            }
            std::vector<std::tuple<uint16_t, std::string, uint8_t>> mounts;
            for (const auto& o : m.mounts()) {
                mounts.emplace_back(static_cast<uint16_t>(o.look_type()), o.name(), static_cast<uint8_t>(o.mode()));
                mountOffers.push_back(o.offer_id());
            }
            std::vector<std::tuple<uint16_t, std::string>> familiars;
            for (const auto& o : m.familiars()) {
                familiars.emplace_back(static_cast<uint16_t>(o.look_type()), o.name());
                familiarOffers.push_back(o.offer_id());
            }
            std::vector<std::tuple<uint16_t, uint16_t>> pairs;
            for (const auto& p : m.gender_pairs())
                pairs.emplace_back(static_cast<uint16_t>(p.male()), static_cast<uint16_t>(p.female()));
            Position podium;
            podium.x = static_cast<int>(m.podium_position().x());
            podium.y = static_cast<int>(m.podium_position().y());
            podium.z = static_cast<short>(m.podium_position().z());
            const bool podiumWindow = m.window_mode() == 5;
            tibia::game::notify("onOutfitWindowOptions", static_cast<uint8_t>(m.window_mode()), m.mounted(), m.random_mount(), pairs,
                                outfitOffers, mountOffers, familiarOffers, podium, static_cast<uint16_t>(m.podium_object()),
                                static_cast<uint8_t>(m.podium_stack_position()),
                                static_cast<uint8_t>(podiumWindow ? (m.podium_visible() ? 1 : 0) : 1),
                                static_cast<uint8_t>(podiumWindow ? (m.podium_show_outfit() ? 1 : 0) : 1),
                                static_cast<uint8_t>(podiumWindow ? m.podium_direction() : 2));
            tibia::game::processOpenOutfitWindow(current, outfits, mounts, familiars);
        }
        break;
    case 156:
        if (shadow->HasExtension(GameserverMessageBlessings::gameserver_message_blessings)) {
            const auto& m = shadow->GetExtension(GameserverMessageBlessings::gameserver_message_blessings);
            tibia::game::localPlayerSetBlessings(static_cast<uint16_t>(m.blessings()), static_cast<uint8_t>(m.visual_state()));
        }
        break;
    case 194:
        if (shadow->HasExtension(GameserverMessageConfigureCreaturePodium::gameserver_message_configure_creature_podium)) {
            const auto& m = shadow->GetExtension(GameserverMessageConfigureCreaturePodium::gameserver_message_configure_creature_podium);
            // An empty look is no outfit at all.
            const auto outfitOf = [](const ::tibia::protobuf::protocol::ProtocolOutfit& look) {
                return look.look_type() == 0 && look.look_type_ex() == 0 ? Outfit{} : creatures::outfitFromProtocol(look, nullptr);
            };
            std::vector<std::tuple<uint16_t, std::string, Outfit>> races;
            for (const auto& race : m.races())
                races.emplace_back(static_cast<uint16_t>(race.race_id()), race.name(), m.boss_podium() ? outfitOf(race.outfit()) : Outfit{});
            const Position pos(static_cast<int>(m.position().x()), static_cast<int>(m.position().y()),
                               static_cast<short>(m.position().z()));
            tibia::game::notify("onConfigureCreaturePodium", outfitOf(m.outfit()), static_cast<uint8_t>(m.boss_podium() ? 1 : 0), races, pos,
                                static_cast<uint16_t>(m.object_id()), static_cast<uint8_t>(m.stack_position()),
                                static_cast<uint8_t>(m.podium_visible() ? 1 : 0), static_cast<uint8_t>(m.monster_visible() ? 1 : 0),
                                static_cast<uint8_t>(m.direction()));
        }
        break;
    case 46:
        if (shadow->HasExtension(GameserverMessageUnlockedSpells::gameserver_message_unlocked_spells)) {
            std::vector<uint16_t> spellIds;
            for (const uint32_t id : shadow->GetExtension(GameserverMessageUnlockedSpells::gameserver_message_unlocked_spells).spell_ids())
                spellIds.push_back(static_cast<uint16_t>(id));
            tibia::game::notify("onUnlockedSpells", spellIds);
        }
        break;
    case 94:
        if (shadow->HasExtension(GameserverMessagePassiveAbilityData::gameserver_message_passive_ability_data)) {
            const auto& m = shadow->GetExtension(GameserverMessagePassiveAbilityData::gameserver_message_passive_ability_data);
            if (m.kind() != 0) // only the cooldown kind drives the action bar
                break;
            if (auto* storage = TCooldownStorage::instance())
                storage->applyPassiveAbilityData(static_cast<int>(m.ability()), m.remaining_ms(), m.total_ms(), m.decaying());
            else
                tibia::game::notify("onPassiveData", static_cast<int>(m.ability()), m.remaining_ms(), m.total_ms(), m.decaying());
        }
        break;
    case 237:
        if (shadow->HasExtension(GameserverMessageShowMessageDialog::gameserver_message_show_message_dialog)) {
            const auto& m = shadow->GetExtension(GameserverMessageShowMessageDialog::gameserver_message_show_message_dialog);
            const auto code = static_cast<uint8_t>(m.dialog_type());
            tibia::game::notify("onServerError", code, m.message());
            tibia::game::TGameNotify::instance()->emitServerError(code, QString::fromStdString(m.message()));
        }
        break;
    case 157:
        if (shadow->HasExtension(GameserverMessageSwitchPreset::gameserver_message_switch_preset))
            tibia::game::notify("onPreset", shadow->GetExtension(GameserverMessageSwitchPreset::gameserver_message_switch_preset).preset_id());
        break;
    case 24: {
        if (!shadow->HasExtension(
                GameserverMessageSessionEndInformation::gameserver_message_session_end_information))
            return;
        handleSessionEndInformationMessage(shadow->GetExtension(
            GameserverMessageSessionEndInformation::gameserver_message_session_end_information));
        break;
    }
    case 27: {
        if (!shadow->HasExtension(GameserverMessageShowMultiOfflineTrainingDialog::
                                      gameserver_message_show_multi_offline_training_dialog))
            return;
        handleShowMultiOfflineTrainingDialogMessage(shadow->GetExtension(
            GameserverMessageShowMultiOfflineTrainingDialog::
                gameserver_message_show_multi_offline_training_dialog));
        break;
    }
    case 29: // CIP Ping (0x1D) — Proto::GameServerPingBack = 29
    case 30: { // CIP PingBack (0x1E) — Proto::GameServerPing = 30
        // No CIP handlePing* on qtclient network H; queue has receivedPing(Back)Message only.
        // Preserve dispatch.gen.cpp GameClientPing swap → processPing / processPingBack.
        if (opcode == 29) {
            if (!shadow->HasExtension(GameserverMessagePing::gameserver_message_ping))
                return;
        } else if (!shadow->HasExtension(GameserverMessagePingBack::gameserver_message_ping_back)) {
            return;
        }
        const bool feature = true;
        // Proto enum names are inverted vs CIP comments: PingBack=29, Ping=30.
        if (((opcode == Proto::GameServerPing) && feature)
            || ((opcode == Proto::GameServerPingBack) && !feature)) {
            processPingBack();
        } else {
            processPing();
        }
        break;
    }
    case 31: {
        // CIP: receivedLoginChallengeMessage + gameLoginChallengeMessage only —
        // no handleLoginChallengeMessage. Same side effects as parseLoginChallenge.
        if (!shadow->HasExtension(
                GameserverMessageLoginChallenge::gameserver_message_login_challenge))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageLoginChallenge::gameserver_message_login_challenge);
        if (auto* connection = network::TGameserverNetworkPacketProcessor::live())
            connection->sendLogin(m.timestamp(), static_cast<uint8_t>(m.random()));
        break;
    }
    case 99: {
        if (!shadow->HasExtension(GameserverMessageClientCheck::gameserver_message_client_check))
            return;
        const std::string& data = shadow->GetExtension(GameserverMessageClientCheck::gameserver_message_client_check).data();
        std::vector<uint8_t> bytes(data.begin(), data.end());
        // Acknowledged with the same bytes (C 0x63).
        protocol::gameclient::sendClientCheck(bytes);
        tibia::game::notify("onClientCheck", bytes);
        break;
    }
    case 96: {
        // CIP: receivedSpellDataModificationMessage only — no handleSpellDataModificationMessage.
        // Same notify args as parseSpellDataModification (first entry spellId + mod).
        if (!shadow->HasExtension(GameserverMessageSpellDataModification::
                                      gameserver_message_spell_data_modification))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageSpellDataModification::gameserver_message_spell_data_modification);
        const uint16_t firstSpell = m.entries_size() > 0 ? uint16_t(m.entries(0).spell_id()) : 0;
        const uint8_t firstFlags = m.entries_size() > 0 ? uint8_t(m.entries(0).mod()) : 0;
        tibia::game::notify("onSpellDataModification", firstSpell, firstFlags);
        break;
    }
    case 181: {
        // The server refused the step: the player snaps back, facing `direction`.
        if (!shadow->HasExtension(GameserverMessageSnapBack::gameserver_message_snap_back))
            return;
        tibia::game::processWalkCancel(static_cast<tibia::client::Direction>(
            shadow->GetExtension(GameserverMessageSnapBack::gameserver_message_snap_back).direction()));
        break;
    }
    case 182:
        // The official client reads the wait time and does nothing with it.
        break;
    case 188: {
        // CIP: receivedStopMessage only — no handleStopMessage.
        if (!shadow->HasExtension(GameserverMessageStop::gameserver_message_stop))
            return;
        tibia::game::notify("onServerStop");
        break;
    }
    case 219: {
        // CIP: receivedHirelingNameChangeMessage only — no handleHirelingNameChangeMessage.
        if (!shadow->HasExtension(
                GameserverMessageHirelingNameChange::gameserver_message_hireling_name_change))
            return;
        const auto& m = shadow->GetExtension(
            GameserverMessageHirelingNameChange::gameserver_message_hireling_name_change);
        tibia::game::notify("onHirelingNameChange", m.creature_id(), m.object_id());
        break;
    }
    case 247: {
        // CIP: receivedMarketLeaveMessage only — no handleMarketLeaveMessage.
        // Same side effects as dispatch MarketLeave (empty pb → onMarketLeave).
        if (!shadow->HasExtension(GameserverMessageMarketLeave::gameserver_message_market_leave))
            return;
        tibia::game::notify("onMarketLeave");
        break;
    }
    default:
        break;
    }
}

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::s_instance = nullptr;

TPassiveAbilityInfoStorage::TPassiveAbilityInfoStorage(QObject* parent)
    : cip::tibia::game::TPassiveAbilityInfoStorage(parent)
    , m_model(new gamewindow::TPassiveAbilityListDataModel(this))
{
    s_instance = this;
    m_hooks.emplace_back(tibia::game::kNotifyScope, "onPassiveData", [this](GameObject*, const Args& args) {
        // onPassiveData(abilityId, remainingMs, totalMs, decaying) — cooldown
        // also lands in TCooldownStorage for action-bar overlays.
        const int abilityId = int(argInt(args, 0));
        m_currentCooldown = static_cast<uint32_t>(argInt(args, 1));
        m_maxCooldown = static_cast<uint32_t>(argInt(args, 2));
        m_canDecay = argBool(args, 3);
        std::vector<gamewindow::TPassiveAbilityListDataModel::Entry> entries;
        gamewindow::TPassiveAbilityListDataModel::Entry entry;
        entry.id = abilityId > 0 ? abilityId : 1;
        entry.name = abilityId == 1 ? QStringLiteral("Gift of Life") : QStringLiteral("Passive Ability");
        entry.iconUrl = QStringLiteral("image://passiveability-icons-32x32/%1").arg(entry.id);
        entry.usableByCurrentVocation = true;
        entries.push_back(std::move(entry));
        m_model->setEntries(std::move(entries));
        return std::any{};
    });
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::gameEnded, this, [this]() {
        m_currentCooldown = 0;
        m_maxCooldown = 0;
        m_canDecay = false;
        m_model->setEntries({});
    });
}

TPassiveAbilityInfoStorage* TPassiveAbilityInfoStorage::instance()
{
    return s_instance;
}

namespace {
TPlayerProtocolMessageHandler* g_livePlayerProtocolHandler = nullptr;
}

TPlayerProtocolMessageHandler* TPlayerProtocolMessageHandler::liveInstance()
{
    return g_livePlayerProtocolHandler;
}

void TPlayerProtocolMessageHandler::setLiveInstance(TPlayerProtocolMessageHandler* handler)
{
    g_livePlayerProtocolHandler = handler;
}

void TPlayerProtocolMessageHandler::handlePlayerDataBasicMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& message)
{
    // The premium flag and its end go to TPremiumProtocolMessageHandler.
    // A vocation beyond the five abandons the message.
    if (message.vocation() >= 6) {
        qWarning() << "PlayerDataBasic carries unknown vocation" << message.vocation();
        return;
    }
    if (TPlayerData* data = TPlayerData::liveInstance()) {
        const uint8_t oldVocation = static_cast<uint8_t>(tibia::game::localPlayerVocation());
        const std::vector<uint16_t> oldSpells = data->spells();
        data->setVocation(static_cast<uint8_t>(message.vocation()));
        data->setHasReachedMain(message.reached_main());
        data->setPromoted(message.promoted());
        data->setMagicShieldVocation(message.magic_shield());
        data->clearKnownSpells();
        for (const uint32_t spellId : message.spells())
            data->addKnownSpell(static_cast<uint16_t>(spellId));

        const auto vocation = static_cast<uint8_t>(tibia::game::localPlayerVocation());
        if (oldVocation != vocation)
            tibia::game::notify("onVocationChange", vocation, oldVocation);
        if (const std::vector<uint16_t> spells = data->spells(); spells != oldSpells)
            tibia::game::notify("onSpellsChange", spells, oldSpells);
    }
    tibia::game::notify("onHasReachedMain", message.reached_main());
    // Product chrome gates on mainland and wants the classic promoted vocation id.
    tibia::game::notify("onPlayerInfo", message.premium(), static_cast<int>(tibia::game::localPlayerVocation()),
                        tibia::game::localPlayerHasReachedMain());
}

void TPlayerProtocolMessageHandler::handlePlayerSkillsMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerSkills& message)
{
    if (message.skills_size() > 0) {
        const auto& magic = message.skills(0);
        tibia::game::localPlayerSetMagicLevelDetail(static_cast<uint16_t>(magic.loyalty()),
                                                    static_cast<uint16_t>(magic.percent_hundredths()));
        tibia::game::localPlayerSetMagicLevel(static_cast<uint16_t>(magic.level()),
                                              static_cast<uint16_t>(magic.percent_hundredths() / 100));
        tibia::game::localPlayerSetBaseMagicLevel(static_cast<uint16_t>(magic.base()));
    }
    for (int i = 1; i < message.skills_size(); ++i) {
        const auto skill = static_cast<tibia::client::Skill>(tibia::client::Fist + i - 1);
        if (skill > tibia::client::Fishing)
            break;
        const auto& entry = message.skills(i);
        tibia::game::localPlayerSetSkillDetail(skill, static_cast<uint16_t>(entry.loyalty()),
                                               static_cast<uint16_t>(entry.percent_hundredths()));
        tibia::game::localPlayerSetSkill(skill, static_cast<uint16_t>(entry.level()),
                                         static_cast<uint16_t>(entry.percent_hundredths() / 100));
        tibia::game::localPlayerSetBaseSkill(skill, static_cast<uint16_t>(entry.base()));
    }
    tibia::game::localPlayerSetBaseCapacity(message.base_capacity() / 100);
    tibia::game::localPlayerSetTotalCapacity(message.capacity() / 100);
    tibia::game::localPlayerSetFlatDamageHealing(static_cast<uint16_t>(message.flat_damage_healing()));
    tibia::game::localPlayerSetAttackInfo(static_cast<uint16_t>(message.attack()), static_cast<uint8_t>(message.attack_element()));
    tibia::game::localPlayerSetConvertedDamage(message.converted_damage(), static_cast<uint8_t>(message.converted_element()));
    tibia::game::localPlayerSetImbuements(message.life_leech(), message.mana_leech(), message.critical_chance(),
                                          message.critical_damage(), message.onslaught());
    tibia::game::localPlayerSetDefenseInfo(static_cast<uint16_t>(message.defense()), static_cast<uint16_t>(message.armor()),
                                           static_cast<uint16_t>(message.mantra()), message.mitigation(), message.dodge(),
                                           static_cast<uint16_t>(message.damage_reflection()));
    std::map<uint8_t, double> absorbs;
    for (const auto& absorb : message.absorbs())
        absorbs[static_cast<uint8_t>(absorb.combat())] = absorb.value();
    tibia::game::localPlayerSetCombatAbsorbValues(absorbs);
    tibia::game::localPlayerSetForgeBonuses(message.momentum(), message.transcendence(), message.amplification());
}

void TPlayerProtocolMessageHandler::handlePlayerDataCurrentMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataCurrent& message)
{
    using tibia::client::ExperienceRate_t;
    tibia::game::localPlayerSetHealth(message.health(), message.max_health());
    tibia::game::localPlayerSetFreeCapacity(message.free_capacity() / 100);
    tibia::game::localPlayerSetExperience(message.experience());
    tibia::game::localPlayerSetLevel(static_cast<uint16_t>(message.level()),
                                     static_cast<uint16_t>(message.level_percent_hundredths()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_BASE, static_cast<uint16_t>(message.base_xp_gain()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_LOWLEVEL, static_cast<uint16_t>(message.low_level_xp_bonus()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_XPBOOST, static_cast<uint16_t>(message.store_xp_boost()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_DAILYREWARD,
                                              static_cast<uint16_t>(message.daily_reward_xp_boost()));
    tibia::game::localPlayerSetExperienceRate(tibia::client::EXP_STANINAMULTIPLIER,
                                              static_cast<uint16_t>(message.stamina_xp_factor()));
    tibia::game::localPlayerSetMana(message.mana(), message.max_mana());
    tibia::game::localPlayerSetSoul(static_cast<uint8_t>(message.soul()));
    tibia::game::localPlayerSetBaseSpeed(static_cast<uint16_t>(message.base_speed()));
    tibia::game::localPlayerSetStamina(static_cast<uint16_t>(message.stamina_minutes()));
    tibia::game::localPlayerSetRegenerationTime(static_cast<uint16_t>(message.regeneration_seconds()));
    tibia::game::localPlayerSetOfflineTrainingTime(static_cast<uint16_t>(message.offline_training_minutes()));
    tibia::game::localPlayerSetStoreExpBoostTime(static_cast<uint16_t>(message.store_xp_boost_seconds()));
    tibia::game::localPlayerSetStoreExpBoostPurchasable(message.store_xp_boost_purchasable());
    tibia::game::localPlayerSetDailyRewardExpBoostTime(static_cast<uint16_t>(message.daily_reward_xp_boost_seconds()));
    tibia::game::localPlayerSetManaShield(message.mana_shield(), message.max_mana_shield());
    if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
        storage->syncLocalPlayerStatsFromMap();
}

void TPlayerProtocolMessageHandler::handlePlayerStateMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerState& message)
{
    if (TPlayerData* data = TPlayerData::liveInstance())
        data->setStates(std::bitset<30>(message.states() & 0x3fffffff), static_cast<int>(message.counter()));
    // The player-state icons still read the states from the local player.
    tibia::game::localPlayerSetStatesAndRottenBlood(message.states(), static_cast<uint8_t>(message.counter()));
}

void TPlayerProtocolMessageHandler::handleSetTacticsMessage(
    const ::tibia::protobuf::protocol::GameserverMessageSetTactics& message)
{
    // A PvP mode beyond the four abandons the message.
    if (message.pvp_mode() >= 4) {
        qWarning() << "SetTactics carries unknown PvP mode" << message.pvp_mode();
        return;
    }
    if (TPlayerData* data = TPlayerData::liveInstance()) {
        data->setChaseOpponents(message.chase_opponents());
        data->setSecureMode(message.secure_mode());
        data->setPvpMode(static_cast<int>(message.pvp_mode()));
    }
    tibia::game::processPlayerTactics(message.chase_opponents() ? tibia::client::ChaseOpponent : tibia::client::DontChase,
                                      message.secure_mode(), static_cast<tibia::client::PVPModes>(message.pvp_mode()));
}

void TPlayerProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::lastShadowServerMessage();
    if (!shadow)
        return;
    switch (opcode) {
    case 193:
        if (shadow->HasExtension(GameserverMessageVocationSpecificPlayerData::gameserver_message_vocation_specific_player_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageVocationSpecificPlayerData::gameserver_message_vocation_specific_player_data);
            switch (m.data_type()) {
            case tibia::client::TYPES_MONK_HARMONY:
                tibia::game::localPlayerSetHarmony(static_cast<uint8_t>(m.harmony()));
                if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
                    storage->syncLocalPlayerStatsFromMap();
                break;
            case tibia::client::TYPES_MONK_SERENE:
                tibia::game::localPlayerSetSerene(m.serene());
                if (tibia::creatures::TCreatureStorage* storage = tibia::creatures::TCreatureStorage::instance())
                    storage->syncLocalPlayerStatsFromMap();
                break;
            case tibia::client::TYPES_MONK_VIRTUE: {
                std::vector<uint16_t> spellIds;
                for (const uint32_t id : m.virtue_spell_ids())
                    spellIds.push_back(static_cast<uint16_t>(id));
                tibia::game::notify("onVirtuesYellowBorder", spellIds);
                break;
            }
            default:
                break;
            }
        }
        break;
    case 159:
        if (shadow->HasExtension(GameserverMessagePlayerDataBasic::gameserver_message_player_data_basic))
            handlePlayerDataBasicMessage(shadow->GetExtension(GameserverMessagePlayerDataBasic::gameserver_message_player_data_basic));
        break;
    case 160:
        if (shadow->HasExtension(GameserverMessagePlayerDataCurrent::gameserver_message_player_data_current))
            handlePlayerDataCurrentMessage(
                shadow->GetExtension(GameserverMessagePlayerDataCurrent::gameserver_message_player_data_current));
        break;
    case 161:
        if (shadow->HasExtension(GameserverMessagePlayerSkills::gameserver_message_player_skills))
            handlePlayerSkillsMessage(shadow->GetExtension(GameserverMessagePlayerSkills::gameserver_message_player_skills));
        break;
    case 162:
        if (shadow->HasExtension(GameserverMessagePlayerState::gameserver_message_player_state)) {
            handlePlayerStateMessage(shadow->GetExtension(GameserverMessagePlayerState::gameserver_message_player_state));
        }
        break;
    case 163:
        if (shadow->HasExtension(GameserverMessageUpdateTarget::gameserver_message_update_target)) {
            const auto& m = shadow->GetExtension(GameserverMessageUpdateTarget::gameserver_message_update_target);
            tibia::game::processUpdateTarget(m.sequence(), m.target());
        }
        break;
    case 164:
        if (shadow->HasExtension(GameserverMessageSpellDelay::gameserver_message_spell_delay)) {
            const auto& m = shadow->GetExtension(GameserverMessageSpellDelay::gameserver_message_spell_delay);
            if (auto* storage = TCooldownStorage::instance())
                storage->applySpellCooldown(static_cast<int>(m.spell_id()), m.delay());
        }
        break;
    case 165:
        if (shadow->HasExtension(GameserverMessageSpellGroupDelay::gameserver_message_spell_group_delay)) {
            const auto& m = shadow->GetExtension(GameserverMessageSpellGroupDelay::gameserver_message_spell_group_delay);
            if (auto* storage = TCooldownStorage::instance())
                storage->applySpellGroupCooldown(static_cast<int>(m.group_id()), m.delay());
        }
        break;
    case 166:
        if (shadow->HasExtension(GameserverMessageMultiUseDelay::gameserver_message_multi_use_delay)) {
            if (auto* storage = TCooldownStorage::instance())
                storage->applyMultiUseCooldown(
                    shadow->GetExtension(GameserverMessageMultiUseDelay::gameserver_message_multi_use_delay).delay());
        }
        break;
    case 167:
        if (shadow->HasExtension(GameserverMessageSetTactics::gameserver_message_set_tactics)) {
            handleSetTacticsMessage(shadow->GetExtension(GameserverMessageSetTactics::gameserver_message_set_tactics));
        }
        break;
    default:
        break;
    }
}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    // Session-parented qtclient twin (QApp already exists). Dialog stack binds later.
    if (!g_qtclientFpsDropsLogger)
        g_qtclientFpsDropsLogger = new FPSDropsLogger(parent);
    // Empty-moc / session-owned CIP QObject surfaces (live under the game session).
    new TBlessingsProtocolMessageHandler(parent);
    new TBossDifficultySelectionProtocolMessageHandler(parent);
    new TEditTextProtocolMessageHandler(parent);
    new TGameActionRouter(parent);
    new TGameEventProtocolMessageHandler(parent);
    {
        auto* handler = new TGameProtocolMessageHandler(parent);
        TGameProtocolMessageHandler::setLiveInstance(handler);
    }
    new TInternalGameActionRouter(parent);
    new TOutfitDialogStorage(parent);
    new TPassiveAbilityInfoStorage(parent);
    {
        auto* playerData = new TPlayerData(parent);
        TPlayerData::setLiveInstance(playerData);
    }
    new TPlayerGameActionHandler(parent);
    TPlayerProtocolMessageHandler::setLiveInstance(new TPlayerProtocolMessageHandler(parent));
    {
        auto* stats = new TPlayerSkillStats(parent);
        TPlayerSkillStats::setLiveInstance(stats);
    }
    new TPremiumProtocolMessageHandler(parent);
    new TPremiumTimeStorage(parent);
    new TServerModalDialogProtocolMessageHandler(parent);
    new TSpellBookProtocolMessageHandler(parent);
}

void TEditTextProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (shadow && opcode == 150 && shadow->HasExtension(GameserverMessageEditText::gameserver_message_edit_text)) {
        const auto& m = shadow->GetExtension(GameserverMessageEditText::gameserver_message_edit_text);
        const auto item = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(m.object());
        tibia::game::processEditText(m.edit_id(), item.typeId, static_cast<uint16_t>(m.max_length()), m.text(), m.author(), m.date(),
                                     m.recently_traded());
        return;
    }
    if (!shadow || opcode != 151 || !shadow->HasExtension(GameserverMessageEditList::gameserver_message_edit_list))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageEditList::gameserver_message_edit_list);
    tibia::game::processEditList(m.list_id(), static_cast<uint8_t>(m.list_type()), m.text());
}

namespace {
TPremiumTimeStorage* g_livePremiumTimeStorage = nullptr;
}

TPremiumTimeStorage::TPremiumTimeStorage(QObject* parent)
    : cip::tibia::game::TPremiumTimeStorage(parent)
{
    g_livePremiumTimeStorage = this;
}

TPremiumTimeStorage::~TPremiumTimeStorage()
{
    if (g_livePremiumTimeStorage == this)
        g_livePremiumTimeStorage = nullptr;
}

TPremiumTimeStorage* TPremiumTimeStorage::liveInstance()
{
    return g_livePremiumTimeStorage;
}

int TPremiumTimeStorage::premiumState() const
{
    if (hasUnlimitedPremium())
        return 2;
    return isPremium() ? 1 : 0;
}

uint16_t TPremiumTimeStorage::remainingDays() const
{
    if (m_receivedAt <= m_premiumUntil && m_premiumUntil - m_receivedAt != 0)
        return static_cast<uint16_t>((m_premiumUntil - m_receivedAt) / 86400);
    return 0;
}

void TPremiumTimeStorage::setPremium(const bool premium, const uint32_t premiumUntil)
{
    m_premium = premium;
    m_premiumUntil = premiumUntil;
    m_receivedAt = static_cast<uint32_t>(shared::TClockService::nowMillis() / 1000);
    Q_EMIT premiumTimeChanged();
}

void TPremiumTimeStorage::reset()
{
    m_premiumUntil = 0;
    m_premium = false;
    m_receivedAt = 0;
    Q_EMIT premiumTimeChanged();
}

void TPremiumProtocolMessageHandler::handlePlayerDataBasicMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic& message)
{
    const bool wasPremium = tibia::game::localPlayerIsPremium();
    if (TPremiumTimeStorage* storage = TPremiumTimeStorage::liveInstance())
        storage->setPremium(message.premium(), message.premium_until());
    if (wasPremium != message.premium())
        tibia::game::notify("onPremiumChange", message.premium());
    tibia::game::notify("onPremiumExpiration", message.premium(), message.premium_until());
}

void TPremiumProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (shadow && opcode == 159
        && shadow->HasExtension(::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic::gameserver_message_player_data_basic)) {
        handlePlayerDataBasicMessage(
            shadow->GetExtension(::tibia::protobuf::protocol::GameserverMessagePlayerDataBasic::gameserver_message_player_data_basic));
        return;
    }
    if (!shadow || opcode != 158 || !shadow->HasExtension(GameserverMessagePremiumTrigger::gameserver_message_premium_trigger))
        return;
    std::vector<uint8_t> triggers;
    for (const uint32_t trigger : shadow->GetExtension(GameserverMessagePremiumTrigger::gameserver_message_premium_trigger).triggers())
        triggers.push_back(static_cast<uint8_t>(trigger));
    tibia::game::notify("onPremiumTrigger", triggers);
}

void TBlessingsProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow || opcode != 155 || !shadow->HasExtension(GameserverMessageBlessingsDialog::gameserver_message_blessings_dialog))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageBlessingsDialog::gameserver_message_blessings_dialog);
    // The dialog derives the backpack and item loss itself (0xff: everything).
    const auto rawLoss = [](const uint32_t loss, const uint32_t tenth) { return static_cast<uint8_t>(tenth == 100 ? 0xff : loss); };
    BlessDialogData data;
    data.totalBless = static_cast<uint8_t>(m.blessings_size());
    for (const auto& b : m.blessings())
        data.blesses.push_back({ .blessBitwise = static_cast<uint16_t>(b.blessing()), .playerBlessCount = static_cast<uint8_t>(b.count()),
                                 .store = static_cast<uint8_t>(b.store_count()) });
    data.premium = m.premium() ? 1 : 0;
    data.promotion = static_cast<uint8_t>(m.promotion());
    data.pvpMinXpLoss = static_cast<uint8_t>(m.pvp_min_xp_loss());
    data.pvpMaxXpLoss = static_cast<uint8_t>(m.pvp_max_xp_loss());
    data.pveExpLoss = static_cast<uint8_t>(m.pve_xp_loss());
    data.equipPvpLoss = rawLoss(m.equip_pvp_loss(), m.equip_pvp_loss_tenth());
    data.equipPveLoss = rawLoss(m.equip_pve_loss(), m.equip_pve_loss_tenth());
    data.skull = m.skull() ? 1 : 0;
    data.aol = m.amulet_of_loss() ? 1 : 0;
    for (const auto& h : m.history())
        data.logs.push_back({ .timestamp = h.timestamp(), .colorMessage = static_cast<uint8_t>(h.color()), .historyMessage = h.text() });
    tibia::game::notify("onUpdateBlessDialog", data);
}

void TServerModalDialogProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow || opcode != 250 || !shadow->HasExtension(GameserverMessageShowModalDialog::gameserver_message_show_modal_dialog))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageShowModalDialog::gameserver_message_show_modal_dialog);
    std::vector<std::tuple<uint8_t, std::string>> buttons;
    for (const auto& b : m.buttons())
        buttons.emplace_back(static_cast<uint8_t>(b.choice_id()), b.text());
    std::vector<std::tuple<uint8_t, std::string>> choices;
    for (const auto& c : m.choices())
        choices.emplace_back(static_cast<uint8_t>(c.choice_id()), c.text());
    tibia::game::processModalDialog(m.dialog_id(), m.title(), m.message(), buttons, static_cast<uint8_t>(m.enter_button()),
                                    static_cast<uint8_t>(m.escape_button()), choices, m.priority());
}

void TBossDifficultySelectionProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow || opcode != 47
        || !shadow->HasExtension(GameserverMessageBossDifficultySelection::gameserver_message_boss_difficulty_selection))
        return;
    const auto& m = shadow->GetExtension(GameserverMessageBossDifficultySelection::gameserver_message_boss_difficulty_selection);
    // The classic wire carries the open part (state 0) and the update part
    // (states 0 and 2) flat in one message: its first u32 is the selection
    // context (proto: lowest_difficulty), the second u16 the lowest difficulty
    // (proto: selected_difficulty) and the trailing u16 the selected one.
    TBossDifficultySelectionDialogData data;
    if (m.state() == 0) {
        data.context = m.lowest_difficulty();
        data.isActiveUser = !m.spinner_locked();
        data.raceId = static_cast<int>(m.race_id());
        data.lowestDifficulty = static_cast<int>(m.selected_difficulty());
        data.highestDifficultyOfGroup = static_cast<int>(m.group_highest());
        data.personalHighestDifficulty = static_cast<int>(m.personal_highest());
        // The wire sends the bonus percent times 1000.
        data.currentBadLuckDropBonus = static_cast<float>(m.bad_luck()) / 1000.f;
        for (size_t i = 0; i < data.playerNames.size() && i < static_cast<size_t>(m.banners_size()); ++i)
            data.playerNames[i] = m.banners(static_cast<int>(i));
    }
    if (m.state() == 0 || m.state() == 2) {
        data.selectedDifficulty = static_cast<int>(m.difficulty());
        data.conModifiers.assign(m.red_entries().begin(), m.red_entries().end());
        data.proModifiers.assign(m.green_entries().begin(), m.green_entries().end());
    }
    switch (m.state()) {
        case 2: tibia::game::notify("onBossDifficultyUpdate", static_cast<const TBossDifficultySelectionDialogUpdateData&>(data)); break;
        case 1: tibia::game::notify("onBossDifficultyClose"); break;
        case 0: tibia::game::notify("onBossDifficultySelection", data); break;
        default: break;
    }
}

namespace {
TSpellBookProtocolMessageHandler* g_spellBookProtocolMessageHandler = nullptr;
/// The message dialog type that shows a spell book.
constexpr uint32_t SpellBookMessageDialogType = 80;
}

TSpellBookProtocolMessageHandler::TSpellBookProtocolMessageHandler(QObject* parent)
    : cip::tibia::game::TSpellBookProtocolMessageHandler(parent)
{
    g_spellBookProtocolMessageHandler = this;
}

TSpellBookProtocolMessageHandler::~TSpellBookProtocolMessageHandler()
{
    if (g_spellBookProtocolMessageHandler == this)
        g_spellBookProtocolMessageHandler = nullptr;
}

TSpellBookProtocolMessageHandler* TSpellBookProtocolMessageHandler::liveInstance() { return g_spellBookProtocolMessageHandler; }

void TSpellBookProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    if (opcode != 237 || !g_spellBookProtocolMessageHandler)
        return;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow || !shadow->HasExtension(GameserverMessageShowMessageDialog::gameserver_message_show_message_dialog))
        return;
    g_spellBookProtocolMessageHandler->handleShowMessageDialogMessage(
        shadow->GetExtension(GameserverMessageShowMessageDialog::gameserver_message_show_message_dialog));
}

void TSpellBookProtocolMessageHandler::handleShowMessageDialogMessage(
    const ::tibia::protobuf::protocol::GameserverMessageShowMessageDialog& Message)
{
    if (Message.dialog_type() == SpellBookMessageDialogType)
        Q_EMIT requestShowSpellBookDialog(QString::fromStdString(Message.message()), static_cast<uint16_t>(Message.extra_value()));
}

} // namespace tibia::game
