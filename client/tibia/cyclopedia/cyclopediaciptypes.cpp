#include "cyclopedia/cyclopediaciptypes.h"
#include "cyclopedia/staticcontentinformation.h"
#include <cmath>
#include "protocol/protocolciptypes.h"
#include "client/staticdata.h"
#include "analysers/analyserstorages.h"
#include "cyclopedia/bosstiarystorage.h"
#include "cyclopedia/creaturetrackerstorage.h"
#include "analysers/itempricestorage.h"
#include "cyclopedia/characterinfostorage.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "game/inspectedobject.h"
#include "container/containerciptypes.h"
#include "creatures/creatureprotocolapply.h"
#include <tibia/assets/thingtype.h>
#include <tibia/client/outfit.h>
#include <tibia/game/thingsession_forward.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QUuid>

#include <algorithm>
#include <map>
#include <string>
#include <tuple>
#include <protocol.pb.h>
#include <tibia/game/gamesession.h>
#include <client/const.h>

#include <any>
#include <optional>
#include <vector>
#include <tibia/game/gamenotify.h>

namespace tibia::cyclopedia {

namespace {

using Args = std::vector<std::any>;

struct CipTypedKeep {
    TCyclopediaMapGameActionProvider tCyclopediaMapGameActionProvider;
};
CipTypedKeep& typedKeep()
{
    static CipTypedKeep keep;
    return keep;
}

}

void ownProductSurface(QObject* parent)
{
    static bool owned = false;
    if (owned)
        return;
    owned = true;
    (void)typedKeep();
    (void)parent;
}

QString monsterRaceName(const uint16_t raceId)
{
    return capitalizeWords(QString::fromStdString(tibia::game::raceData(raceId).name));
}

Outfit monsterRaceAppearance(const uint16_t raceId)
{
    return tibia::game::raceData(raceId).outfit;
}

namespace {
TMonsterRaceStorage* g_monsterRaceStorage = nullptr;
}

TMonsterRaceStorage::TMonsterRaceStorage(QObject* parent)
    : cip::tibia::cyclopedia::TMonsterRaceStorage(parent)
{
    g_monsterRaceStorage = this;
}

TMonsterRaceStorage::~TMonsterRaceStorage()
{
    if (g_monsterRaceStorage == this)
        g_monsterRaceStorage = nullptr;
}

const TMonsterRace* TMonsterRaceStorage::race(const uint16_t raceId) const
{
    const auto it = std::ranges::find(m_classificationRaces, raceId, &TMonsterRace::raceId);
    return it != m_classificationRaces.end() ? &*it : nullptr;
}

void TMonsterRaceStorage::setRaceDetails(TMonsterRace race)
{
    auto it = std::ranges::find(m_classificationRaces, race.raceId, &TMonsterRace::raceId);
    if (it != m_classificationRaces.end()) {
        *it = std::move(race);
    } else {
        m_classificationRaces.push_back(std::move(race));
        it = std::prev(m_classificationRaces.end());
    }
    const TMonsterRace& r = *it;
    Q_EMIT monsterRaceDetailsChanged(r.raceId);
    // The monster dialog still reads the details as one record.
    BestiaryMonsterData data{};
    data.id = r.raceId;
    data.bestClass = r.classificationName.toStdString();
    data.currentLevel = static_cast<uint8_t>(r.progress);
    data.AnimusMasteryBonus = static_cast<uint16_t>(std::lround(r.animusMasteryBonus * 1000.0));
    data.AnimusMasteryPoints = r.animusMasteryPoints;
    data.killCounter = r.kills;
    data.thirdDifficulty = r.firstUnlock;
    data.secondUnlock = r.secondUnlock;
    data.lastProgressKillCount = r.lastUnlock;
    data.difficulty = r.known ? 1 : 0;
    if (r.occurrence) {
        data.ocorrence = static_cast<uint8_t>(r.occurrence->occurrence);
        data.leaderKilled = r.occurrence->extra != 0;
        for (const TMonsterRace::Loot& l : r.occurrence->loot)
            data.loot.push_back({ .itemId = l.objectId, .diffculty = static_cast<uint8_t>(l.rarity),
                                  .specialEvent = static_cast<uint8_t>(l.specialEvent ? 1 : 0), .name = l.name.toStdString(),
                                  .amount = static_cast<uint8_t>(l.amount) });
    }
    if (r.characteristics) {
        data.charmValue = r.characteristics->charmValue;
        data.attackMode = static_cast<uint8_t>(r.characteristics->attackMode);
        data.castsSpells = r.characteristics->castsSpells;
        data.maxHealth = r.characteristics->maxHealth;
        data.experience = r.characteristics->experience;
        data.speed = r.characteristics->speed;
        data.armor = r.characteristics->armor;
        data.mitigation = r.characteristics->mitigation;
    }
    if (r.combat) {
        for (const auto& [element, value] : r.combat->elements)
            data.combat[static_cast<uint8_t>(element)] = static_cast<uint16_t>(value);
        if (!r.combat->locations.isEmpty())
            data.location = r.combat->locations.front().toStdString();
    }
    tibia::game::processUpdateBestiaryMonsterData(data);
}

TMonsterRaceStorage* TMonsterRaceStorage::instance()
{
    // One storage for the client's lifetime; each classification request replaces its races.
    if (!g_monsterRaceStorage)
        g_monsterRaceStorage = new TMonsterRaceStorage(QCoreApplication::instance());
    return g_monsterRaceStorage;
}

void TMonsterRaceStorage::setClassificationRaces(const QString& classificationName, std::vector<TMonsterRace> races)
{
    m_classificationName = classificationName;
    m_classificationRaces = std::move(races);
    Q_EMIT monsterClassificationMonstersChanged(m_classificationName);
    // The monster dialog still reads the overview rows.
    std::vector<BestiaryOverviewMonsters> rows;
    rows.reserve(m_classificationRaces.size());
    uint16_t animusMasteryPoints = 0;
    for (const TMonsterRace& race : m_classificationRaces) {
        rows.push_back({ .id = race.raceId, .currentLevel = static_cast<uint8_t>(race.progress),
                         .occurrence = static_cast<uint8_t>(race.occurrence ? race.occurrence->occurrence : 0),
                         .creatureAnimusMasteryBonus = static_cast<uint16_t>(std::lround(race.animusMasteryBonus * 1000.0)) });
        animusMasteryPoints = race.animusMasteryPoints;
    }
    tibia::game::processParseBestiaryOverview(m_classificationName.toStdString(), rows, animusMasteryPoints);
}

} // namespace tibia::cyclopedia

namespace tibia::cyclopedia {

void TCyclopediaProtocolMessageHandler::handleMarketStatisticsMessage(
    const ::tibia::protobuf::protocol::GameserverMessageMarketStatistics& message)
{
    // The average prices of untiered objects; a repeated object keeps its last price.
    std::map<uint16_t, uint64_t> prices;
    for (const auto& entry : message.entries()) {
        if (entry.has_tier() && entry.tier() != 0)
            continue;
        prices[static_cast<uint16_t>(entry.object_id())] = entry.price();
    }
    tibia::game::notify("onItemsPrice", prices);
}

void TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaMessage(
    const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopedia& message)
{
    // The classifications in order: name, number of races, known races.
    std::vector<CyclopediaBestiaryRace> classes;
    classes.reserve(static_cast<size_t>(message.classes_size()));
    uint8_t index = 0;
    for (const auto& c : message.classes()) {
        CyclopediaBestiaryRace& entry = classes.emplace_back();
        entry.race = index++;
        entry.bestClass = c.name();
        entry.count = static_cast<uint16_t>(c.total());
        entry.unlockedCount = static_cast<uint16_t>(c.known());
    }
    tibia::game::processParseBestiaryRaces(classes);
}

void TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaNewDetailsMessage(
    const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaNewDetails& message)
{
    // monsterCyclopediaNewDetailsForMonster
    tibia::game::notify("onBestiaryEntryChanged", static_cast<uint16_t>(message.race_id()));
}

void TCyclopediaProtocolMessageHandler::handleCyclopediaHouseActionResulMessage(
    const ::tibia::protobuf::protocol::GameserverMessageCyclopediaHouseActionResult& message)
{
    // requestHouseActionResultDialog
    tibia::game::notify("onCyclopediaHouseAuctionMessage", message.house_id(), static_cast<uint8_t>(message.action_type()),
                        static_cast<uint8_t>(message.error_code()), message.outbidden());
}

void TCyclopediaProtocolMessageHandler::handleXpChangedMessage(const ::tibia::protobuf::protocol::GameserverMessageXpChanged& message)
{
    // The progress storage stamps the gain with the current time.
    tibia::game::notify("onUpdateExperience", message.raw_experience(), message.final_experience());
}

namespace {

using ::tibia::game::fillInspectedAppearance;
using ::tibia::game::inspectionDetails;

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
    data.critChanceEventBonus = at(o.crit_chance(), 6);
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
    for (size_t i = 0; i < data.dodge.size(); ++i)
        data.dodge[i] = at(d.dodge(), static_cast<int>(i));
    data.magicShieldCapacity = d.magic_shield();
    data.magicShieldCapacityFlat = static_cast<uint16_t>(d.magic_shield_flat());
    data.magicShieldCapacityPercent = d.magic_shield_percent();
    data.reflectPhysical = static_cast<uint16_t>(d.reflect_physical());
    data.armor = static_cast<uint16_t>(d.armor());
    data.mantra = static_cast<uint16_t>(d.mantra());
    data.defence = static_cast<uint16_t>(d.defence());
    data.defenceEquipment = static_cast<uint16_t>(d.defence_equipment());
    data.defenceSkillType = static_cast<uint8_t>(d.defence_skill_type());
    data.defenceSkill = static_cast<int16_t>(d.shielding());
    data.defenceWheel = static_cast<uint16_t>(d.defence_wheel());
    for (size_t i = 0; i < data.mitigation.size(); ++i)
        data.mitigation[i] = at(d.mitigation(), static_cast<int>(i));
    for (const auto& r : d.resistances())
        data.resistances.push_back({ static_cast<uint8_t>(r.kind()), static_cast<uint8_t>(r.element()), r.value() });
    return data;
}

CyclopediaCharacterMiscStats miscStats(const ::tibia::protobuf::protocol::ProtocolCharacterInfoMiscStats& m)
{
    CyclopediaCharacterMiscStats data{};
    data.momentumTotal = at(m.momentum(), 0);
    data.momentumBase = at(m.momentum(), 1);
    data.momentumBonus = at(m.momentum(), 2);
    data.momentumWheel = at(m.momentum(), 3);
    data.momentumEventBonus = at(m.momentum(), 4);
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

/// The character info page a wire info type answers.
int characterInfoDataType(const int protocolType)
{
    switch (protocolType) {
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_GENERALSTATS: return TCharacterInfoStorage::GeneralStats;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_COMBATSTATS: return TCharacterInfoStorage::OffenceStats;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS: return TCharacterInfoStorage::RecentDeaths;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS: return TCharacterInfoStorage::RecentPvpKills;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_ACHIEVEMENTS: return TCharacterInfoStorage::Achievements;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_ITEMSUMMARY: return TCharacterInfoStorage::ItemSummary;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_OUTFITSMOUNTS: return TCharacterInfoStorage::OutfitsMountsSummonSkins;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_STORESUMMARY: return TCharacterInfoStorage::StoreSummary;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_INSPECTION: return TCharacterInfoStorage::Inspection;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_BADGES: return TCharacterInfoStorage::Badges;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_TITLES: return TCharacterInfoStorage::Titles;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_OFFENCESTATS: return TCharacterInfoStorage::OffenceStats;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_DEFENCESTATS: return TCharacterInfoStorage::DefenceStats;
        case tibia::client::CYCLOPEDIA_CHARACTERINFO_MISCSTATS: return TCharacterInfoStorage::MiscStats;
        default: return TCharacterInfoStorage::BaseInformation;
    }
}

} // namespace

void TCyclopediaProtocolMessageHandler::handleCyclopediaCharacterInfoMessage(
    const ::tibia::protobuf::protocol::GameserverMessageCyclopediaCharacterInfo& m)
{
    const int type = static_cast<int>(m.info_type());
    TCharacterInfoStorage* storage = TCharacterInfoStorage::instance();
    if (m.error_code() != 0) {
        storage->reportError(characterInfoDataType(type), static_cast<int>(m.error_code()));
        return;
    }
    switch (type) {
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_BASEINFORMATION: {
        const auto& b = m.base();
        TCharacterBaseInformation information;
        information.name = QString::fromStdString(b.name());
        information.vocation = QString::fromStdString(b.vocation());
        information.level = static_cast<int>(b.level());
        information.outfit = creatures::outfitFromProtocol(b.outfit(), nullptr);
        information.ownCharacter = b.own_character();
        information.title = QString::fromStdString(b.title());
        storage->setBaseInformation(std::move(information));
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_GENERALSTATS: {
        storage->setGeneralStatistics(std::make_unique<::tibia::protobuf::protocol::ProtocolCharacterInfoGeneralStats>(m.general()));
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTDEATHS: {
        std::vector<RecentDeathEntry> deaths;
        for (const auto& d : m.deaths())
            deaths.push_back({ d.timestamp(), d.cause() });
        storage->setRecentDeaths(std::move(deaths), m.current_page(), m.page_count());
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_RECENTPVPKILLS: {
        std::vector<RecentPvPKillEntry> kills;
        for (const auto& k : m.pvp_kills())
            kills.push_back({ k.timestamp(), k.description(), static_cast<uint8_t>(k.status()) });
        storage->setRecentPvpKills(std::move(kills), m.current_page(), m.page_count());
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_ACHIEVEMENTS: {
        TAchievementStorage& achievements = storage->achievementStorage();
        achievements.achievements.clear();
        for (const auto& a : m.achievements()) {
            TAchievementStorage::TAchievement entry;
            entry.id = static_cast<uint16_t>(a.id());
            entry.timestamp = a.timestamp();
            entry.secret = a.secret();
            if (a.secret()) {
                entry.name = QString::fromStdString(a.name());
                entry.description = QString::fromStdString(a.description());
                entry.grade = static_cast<uint8_t>(a.grade());
            } else {
                // A regular achievement takes its texts and grade from the static data; an
                // unknown one is shown as unknown.
                const TStaticAchievement known = staticAchievement(entry.id);
                entry.name = known.name;
                entry.description = known.description;
                entry.grade = known.grade;
            }
            achievements.achievements.push_back(std::move(entry));
        }
        achievements.achievementPoints = static_cast<int>(m.achievement_points());
        achievements.secretAchievements = static_cast<int>(m.secret_achievements());
        storage->setAchievementsLoaded();
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_ITEMSUMMARY: {
        CyclopediaCharacterItemSummary data;
        data.inventory = itemCounts(m.inventory_items());
        data.store = itemCounts(m.store_items());
        data.stash = itemCounts(m.stash_items());
        data.depot = itemCounts(m.depot_items());
        data.inbox = itemCounts(m.inbox_items());
        storage->setItemSummary(std::move(data));
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_OUTFITSMOUNTS:
        storage->setOutfitsMountsSummonSkins(m);
        break;
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_STORESUMMARY:
        storage->setStoreSummary(m);
        break;
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
        storage->setInspection(std::move(data));
        break;
    }
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_BADGES:
        storage->setAccountInfos(m);
        break;
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_TITLES:
        storage->setTitlesInfo(m);
        break;
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_OFFENCESTATS:
        storage->setOffenceStats(offenceStats(m.offence()));
        break;
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_DEFENCESTATS:
        storage->setDefenceStats(defenceStats(m.defence()));
        break;
    case tibia::client::CYCLOPEDIA_CHARACTERINFO_MISCSTATS:
        storage->setMiscStats(miscStats(m.misc()));
        break;
    default:
        break;
    }
}

void TCyclopediaProtocolMessageHandler::handleKillTrackingMessage(
    const ::tibia::protobuf::protocol::GameserverMessageKillTracking& message)
{
    analysers::TAnalyserStorages* storages = analysers::TAnalyserStorages::instance();
    if (!storages)
        return;
    const QString monsterName = QString::fromStdString(message.name());
    const Outfit outfit = creatures::outfitFromProtocol(message.outfit(), nullptr);
    const qint64 now = analysers::TAnalyserStorages::nowMs();
    const auto timestamp = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() / 1000);
    const QUuid killIdentifier = QUuid::createUuid();
    std::vector<TDroppedLoot> drops;
    drops.reserve(static_cast<std::size_t>(message.items_size()));
    for (const auto& item : message.items()) {
        const auto slot = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(item);
        if (slot.empty())
            continue;
        TDroppedLoot loot;
        loot.killIdentifier = killIdentifier;
        loot.objectType = slot.typeId;
        // A cumulative object drops with its count, any other once.
        loot.count = slot.stackable ? std::max(1, static_cast<int>(slot.count)) : 1;
        loot.monsterName = monsterName;
        loot.outfit = outfit;
        loot.expirationTime = now + TItemTrackingStorage::RecentDropDurationMs;
        loot.timestamp = timestamp;
        drops.push_back(std::move(loot));
    }
    storages->itemTracking()->addDroppedLoot(drops);
    storages->huntingSession()->addKill(monsterName);
}

void TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaMonstersMessage(
    const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaMonsters& message)
{
    std::vector<TMonsterRace> races;
    for (const auto& entry : message.monsters()) {
        TMonsterRace race;
        race.raceId = static_cast<uint16_t>(entry.race_id());
        race.known = entry.known();
        race.progress = static_cast<int>(entry.progress());
        if (race.progress == 0) {
            race.name = qtTrId("monsters_unknown_name");
        } else {
            race.name = monsterRaceName(race.raceId);
            race.occurrence = TMonsterRace::Occurrence{ static_cast<int>(entry.occurrence()), static_cast<int>(entry.extra()) };
        }
        race.appearance = monsterRaceAppearance(race.raceId);
        race.animusMasteryBonus = entry.animus_mastery_bonus();
        race.animusMasteryPoints = static_cast<uint16_t>(message.animus_mastery_points());
        races.push_back(std::move(race));
    }
    if (TMonsterRaceStorage* storage = TMonsterRaceStorage::instance())
        storage->setClassificationRaces(QString::fromStdString(message.class_name()), std::move(races));
}

void TCyclopediaProtocolMessageHandler::handleMonsterCyclopediaRaceMessage(
    const ::tibia::protobuf::protocol::GameserverMessageMonsterCyclopediaRace& m)
{
    TMonsterRace race;
    race.raceId = static_cast<uint16_t>(m.race_id());
    race.classificationName = QString::fromStdString(m.class_name());
    race.progress = static_cast<int>(m.progress());
    race.animusMasteryBonus = m.animus_mastery_bonus();
    race.animusMasteryPoints = static_cast<uint16_t>(m.animus_mastery_points());
    race.name = race.progress == 0 ? qtTrId("monsters_unknown_name") : monsterRaceName(race.raceId);
    race.kills = m.kills();
    race.firstUnlock = static_cast<uint16_t>(m.first_unlock());
    race.secondUnlock = static_cast<uint16_t>(m.second_unlock());
    race.lastUnlock = static_cast<uint16_t>(m.last_unlock());
    race.known = m.difficulty();
    race.appearance = monsterRaceAppearance(race.raceId);
    if (race.progress != 0) {
        TMonsterRace::Occurrence occurrence;
        occurrence.occurrence = static_cast<int>(m.occurrence());
        occurrence.extra = static_cast<int>(m.leader_killed());
        occurrence.loot.reserve(static_cast<std::size_t>(m.loot_size()));
        for (const auto& l : m.loot())
            occurrence.loot.push_back({ .objectId = static_cast<uint16_t>(l.object_id()),
                                        .name = QString::fromStdString(l.name()),
                                        .rarity = l.has_rarity() ? static_cast<int>(l.rarity()) : 0,
                                        .amount = static_cast<int>(l.amount()),
                                        .specialEvent = l.special_event() });
        race.occurrence = std::move(occurrence);
        if (race.progress > 1) {
            race.characteristics = TMonsterRace::Characteristics{ .charmValue = static_cast<uint16_t>(m.charm_value()),
                                                                  .attackMode = static_cast<int>(m.attack_mode()),
                                                                  .castsSpells = m.casts_spells(),
                                                                  .maxHealth = m.max_health(),
                                                                  .experience = m.experience(),
                                                                  .speed = static_cast<uint16_t>(m.speed()),
                                                                  .armor = static_cast<uint16_t>(m.armor()),
                                                                  .mitigation = m.mitigation() };
            if (race.progress > 2) {
                TMonsterRace::Combat combat;
                for (const auto& e : m.elements())
                    combat.elements.emplace(static_cast<int>(e.element()), static_cast<int16_t>(e.value()));
                for (const std::string& location : m.locations())
                    combat.locations.append(QString::fromStdString(location));
                race.combat = std::move(combat);
            }
        }
    }
    if (TMonsterRaceStorage* storage = TMonsterRaceStorage::instance())
        storage->setRaceDetails(std::move(race));
}

void TCyclopediaProtocolMessageHandler::handlePartyHuntAnalyserMessage(
    const ::tibia::protobuf::protocol::GameserverMessagePartyHuntAnalyser& m)
{
    analysers::TAnalyserStorages* analysers = analysers::TAnalyserStorages::instance();
    if (!analysers)
        return;
    analysers::TPartyHuntStorage& storage = *analysers->partyHunt();
    storage.setSessionStart(m.session_start());
    if (m.price_type() == 1)
        storage.setLootType(1);
    else if (m.price_type() == 0)
        storage.setLootType(0);
    storage.setLeaderId(m.leader_id());
    std::vector<analysers::TPartyHuntStorage::Member> members;
    members.reserve(static_cast<std::size_t>(m.members_size()));
    for (const auto& member : m.members())
        members.push_back({ .id = member.member_id(),
                            .name = {},
                            .active = member.highlighted(),
                            .loot = static_cast<qint64>(member.loot()),
                            .supplies = static_cast<qint64>(member.supplies()),
                            .balance = 0,
                            .damage = static_cast<qint64>(member.damage()),
                            .healing = static_cast<qint64>(member.healing()) });
    std::unordered_map<quint32, QString> names;
    for (const auto& name : m.names())
        names[name.member_id()] = QString::fromStdString(name.name());
    storage.setMembers(members, names);
}

void TCyclopediaProtocolMessageHandler::handleImpactTrackingMessage(
    const ::tibia::protobuf::protocol::GameserverMessageImpactTracking& message)
{
    const auto type = static_cast<uint8_t>(message.type());
    // Damage dealt splits into its physical part and its elemental part.
    const auto element = static_cast<uint8_t>(type == 1 ? message.elemental_type() : message.element());
    tibia::game::notify("onImpactTracker", type, message.amount(), element, message.source(), message.elemental_amount());
}

void TCyclopediaProtocolMessageHandler::handleItemWastedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemWasted& message)
{
    tibia::game::notify("onSupplyTracker", static_cast<uint16_t>(message.object_id()));
}

void TCyclopediaProtocolMessageHandler::handleItemLootedMessage(const ::tibia::protobuf::protocol::GameserverMessageItemLooted& message)
{
    const auto slot = ::tibia::container::TInventorySlot::fromProtocolInventoryItem(message.item());
    const uint16_t count = slot.stackable ? slot.count : 1;
    tibia::game::notify("onLootStats", slot.typeId, count, message.name());
}

void TCyclopediaProtocolMessageHandler::handleResourceBalanceMessage(
    const ::tibia::protobuf::protocol::GameserverMessageResourceBalance& message)
{
    switch (message.resource_type()) {
    case tibia::client::RESOURCE_CHARM:
    case tibia::client::RESOURCE_MINOR_CHARM:
    case tibia::client::RESOURCE_MAX_CHARM:
    case tibia::client::RESOURCE_MAX_MINOR_CHARM:
        tibia::game::localPlayerSetResourceBalance(static_cast<tibia::client::ResourceTypes_t>(message.resource_type()),
                                                   static_cast<uint64_t>(static_cast<uint32_t>(message.small_value())));
        break;
    default:
        break;
    }
}

void TCyclopediaProtocolMessageHandler::onServerMessageEnqueued(const int opcode)
{
    using namespace ::tibia::protobuf::protocol;
    const auto* shadow = protocol::nativeServerMessage(opcode);
    if (!shadow)
        return;
    switch (opcode) {
    case 175:
        if (shadow->HasExtension(GameserverMessageXpChanged::gameserver_message_xp_changed))
            handleXpChangedMessage(shadow->GetExtension(GameserverMessageXpChanged::gameserver_message_xp_changed));
        break;
    case 206:
        if (shadow->HasExtension(GameserverMessageItemWasted::gameserver_message_item_wasted))
            handleItemWastedMessage(shadow->GetExtension(GameserverMessageItemWasted::gameserver_message_item_wasted));
        break;
    case 216:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopediaBonusEffects::gameserver_message_monster_cyclopedia_bonus_effects)) {
            const auto& m = shadow->GetExtension(GameserverMessageMonsterCyclopediaBonusEffects::gameserver_message_monster_cyclopedia_bonus_effects);
            BestiaryCharmsData data{};
            data.resetAllCharmsCost = m.reset_all_charms_cost();
            data.availableCharmSlots = m.has_available_charm_slots() ? static_cast<uint8_t>(m.available_charm_slots()) : 0xff;
            for (const auto& c : m.charms()) {
                CharmData charm{};
                charm.id = static_cast<uint8_t>(c.charm_id());
                charm.tier = static_cast<uint8_t>(c.tier());
                charm.asignedStatus = c.assigned();
                charm.unlocked = c.assigned() || c.tier() > 0;
                charm.raceId = static_cast<uint16_t>(c.race_id());
                charm.removeRuneCost = c.remove_cost();
                data.charms.push_back(std::move(charm));
            }
            for (const auto& f : m.finished_races())
                data.finishedMonsters.push_back(static_cast<uint16_t>(f.race_id()));
            tibia::game::processUpdateBestiaryCharmsData(data);
        }
        break;
    case 217:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopediaNewDetails::gameserver_message_monster_cyclopedia_new_details))
            handleMonsterCyclopediaNewDetailsMessage(
                shadow->GetExtension(GameserverMessageMonsterCyclopediaNewDetails::gameserver_message_monster_cyclopedia_new_details));
        break;
    case 238:
        if (shadow->HasExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance)) {
            const auto& m = shadow->GetExtension(GameserverMessageResourceBalance::gameserver_message_resource_balance);
            if (m.resource_type() >= tibia::client::RESOURCE_CHARM && m.resource_type() <= tibia::client::RESOURCE_MAX_MINOR_CHARM) {
                handleResourceBalanceMessage(m);
            } else {
                // The other balances belong to the game session's handlers.
                const auto type = static_cast<tibia::client::ResourceTypes_t>(m.resource_type());
                tibia::game::localPlayerSetResourceBalance(
                    type, m.has_small_value() ? static_cast<uint64_t>(static_cast<uint32_t>(m.small_value())) : m.value());
            }
        }
        break;
    case 195:
        if (shadow->HasExtension(GameserverMessageCyclopediaHouseActionResult::gameserver_message_cyclopedia_house_action_result))
            handleCyclopediaHouseActionResulMessage(
                shadow->GetExtension(GameserverMessageCyclopediaHouseActionResult::gameserver_message_cyclopedia_house_action_result));
        break;
    case 199:
        if (shadow->HasExtension(GameserverMessageCyclopediaCurrentHouseData::gameserver_message_cyclopedia_current_house_data)) {
            std::vector<CyclopediaHouseEntry> houses;
            for (const auto& h : shadow->GetExtension(GameserverMessageCyclopediaCurrentHouseData::gameserver_message_cyclopedia_current_house_data).houses()) {
                CyclopediaHouseEntry& house = houses.emplace_back();
                house.houseId = h.house_id();
                house.isRentable = h.rentable();
                house.state = static_cast<uint8_t>(h.state());
                house.bidderName = h.bidder_name();
                house.isBidder = h.is_bidder();
                house.bidDisableReason = static_cast<uint8_t>(h.bid_disable_reason());
                house.bidEnd = h.bid_end();
                house.highestBid = h.highest_bid();
                house.bidLimit = h.bid_limit();
                house.ownerName = h.owner_name();
                house.paidUntil = h.paid_until();
                house.isOwner = h.is_owner();
                house.moveOutDisableReason = static_cast<uint8_t>(h.move_out_disable_reason());
                house.transferDisableReason = static_cast<uint8_t>(h.transfer_disable_reason());
                house.transferDate = h.transfer_date();
                house.newOwnerName = h.new_owner_name();
                house.newOwnerAccepted = h.new_owner_accepted();
                house.transferPrice = h.transfer_price();
                house.isNewOwner = h.is_new_owner();
                house.acceptTransferDisableReason = static_cast<uint8_t>(h.accept_transfer_disable_reason());
                house.rejectTransferDisableReason = static_cast<uint8_t>(h.reject_transfer_disable_reason());
                house.cancelTransferDisableReason = static_cast<uint8_t>(h.cancel_transfer_disable_reason());
                house.moveOutDate = h.move_out_date();
                house.cancelMoveOutDisableReason = static_cast<uint8_t>(h.cancel_move_out_disable_reason());
            }
            tibia::game::notify("onCyclopediaHouseList", houses);
        }
        break;
    case 43:
        if (shadow->HasExtension(GameserverMessagePartyHuntAnalyser::gameserver_message_party_hunt_analyser))
            handlePartyHuntAnalyserMessage(shadow->GetExtension(GameserverMessagePartyHuntAnalyser::gameserver_message_party_hunt_analyser));
        break;
    case 198:
        if (shadow->HasExtension(GameserverMessageCyclopediaStaticHouseData::gameserver_message_cyclopedia_static_house_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageCyclopediaStaticHouseData::gameserver_message_cyclopedia_static_house_data);
            const std::vector<uint8_t> counts{ static_cast<uint8_t>(m.first_count()),  static_cast<uint8_t>(m.second_count()),
                                               static_cast<uint8_t>(m.third_count()),  static_cast<uint8_t>(m.fourth_count()),
                                               static_cast<uint8_t>(m.fifth_count()),  static_cast<uint8_t>(m.sixth_count()) };
            const std::vector<uint32_t> shown(m.shown_houses().begin(), m.shown_houses().end());
            const std::vector<uint32_t> others(m.other_houses().begin(), m.other_houses().end());
            tibia::game::notify("onCyclopediaHousesInfo", m.excluded_until(), counts, shown, others);
        }
        break;
    case 215:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopediaRace::gameserver_message_monster_cyclopedia_race))
            handleMonsterCyclopediaRaceMessage(shadow->GetExtension(GameserverMessageMonsterCyclopediaRace::gameserver_message_monster_cyclopedia_race));
        break;
    case 221:
        if (shadow->HasExtension(GameserverMessageCyclopediaMapData::gameserver_message_cyclopedia_map_data)) {
            const auto& m = shadow->GetExtension(GameserverMessageCyclopediaMapData::gameserver_message_cyclopedia_map_data);
            const Position pos(static_cast<int>(m.position().x()), static_cast<int>(m.position().y()), static_cast<short>(m.position().z()));
            switch (m.data_type()) {
            case 0:
                tibia::game::processAddAutomapFlag(pos, static_cast<uint8_t>(m.icon()), m.text());
                break;
            case 1: {
                std::vector<std::vector<int>> mains;
                for (const auto& a : m.main_areas())
                    mains.push_back({ static_cast<int>(a.area_id()), static_cast<int>(a.status()), static_cast<int>(a.progress()) });
                const std::vector<int> discovered(m.discovered_areas().begin(), m.discovered_areas().end());
                const std::vector<int> discoverable(m.discoverable_areas().begin(), m.discoverable_areas().end());
                tibia::game::notify("onCyclopediaMapDiscoveryData", mains, discovered, discoverable);
                break;
            }
            case 2:
                tibia::game::notify("onCyclopediaMapRaid", static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z));
                break;
            case 5: {
                std::vector<std::vector<int>> points;
                for (const auto& p : m.points())
                    points.push_back({ static_cast<int>(p.position().x()), static_cast<int>(p.position().y()), static_cast<int>(p.position().z()),
                                       static_cast<int>(p.state()) });
                tibia::game::notify("onCyclopediaMapDiscoveryArea", static_cast<int>(m.area_id()), static_cast<int>(m.flag() ? 1 : 0),
                                    static_cast<int>(m.target()), points);
                break;
            }
            case 6: {
                std::vector<int> passage{ static_cast<int>(pos.x), static_cast<int>(pos.y), static_cast<int>(pos.z), m.race_ids_size() };
                for (const uint32_t race : m.race_ids())
                    passage.push_back(static_cast<int>(race));
                tibia::game::notify("onCyclopediaMapPassage", passage);
                break;
            }
            case 9: {
                std::vector<std::vector<double>> areas;
                for (const auto& d : m.donations())
                    areas.push_back({ static_cast<double>(d.area_id()), d.improved_respawn() ? 1.0 : 0.0, static_cast<double>(d.donated()) });
                tibia::game::notify("onCyclopediaMapDonations", m.donation_goal(), areas);
                break;
            }
            case 10:
                tibia::game::notify("onCyclopediaMapCurrentArea", static_cast<int>(m.area_id()));
                break;
            case 11:
                tibia::game::notify("onCyclopediaMapExploringArea", static_cast<int>(m.area_id()));
                break;
            default: // imminent raids, sub area monsters, monster bestiary: no consumer yet
                break;
            }
        }
        break;
    case 214:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopediaMonsters::gameserver_message_monster_cyclopedia_monsters))
            handleMonsterCyclopediaMonstersMessage(
                shadow->GetExtension(GameserverMessageMonsterCyclopediaMonsters::gameserver_message_monster_cyclopedia_monsters));
        break;
    case 213:
        if (shadow->HasExtension(GameserverMessageMonsterCyclopedia::gameserver_message_monster_cyclopedia))
            handleMonsterCyclopediaMessage(shadow->GetExtension(GameserverMessageMonsterCyclopedia::gameserver_message_monster_cyclopedia));
        break;
    case 204:
        if (shadow->HasExtension(GameserverMessageImpactTracking::gameserver_message_impact_tracking))
            handleImpactTrackingMessage(shadow->GetExtension(GameserverMessageImpactTracking::gameserver_message_impact_tracking));
        break;
    case 205:
        if (shadow->HasExtension(GameserverMessageMarketStatistics::gameserver_message_market_statistics))
            handleMarketStatisticsMessage(shadow->GetExtension(GameserverMessageMarketStatistics::gameserver_message_market_statistics));
        break;
    case 207:
        if (shadow->HasExtension(GameserverMessageItemLooted::gameserver_message_item_looted))
            handleItemLootedMessage(shadow->GetExtension(GameserverMessageItemLooted::gameserver_message_item_looted));
        break;
    case 209:
        if (shadow->HasExtension(GameserverMessageKillTracking::gameserver_message_kill_tracking))
            handleKillTrackingMessage(shadow->GetExtension(GameserverMessageKillTracking::gameserver_message_kill_tracking));
        break;
    default:
        break;
    }
}

namespace {

using namespace ::tibia::protobuf::protocol;

void sendHouseAction(const uint32_t action, const uint32_t houseId)
{
    auto message = protocol::newGameclientMessage(173);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action);
    m->set_action(action);
    m->set_house_id(houseId);
    protocol::sendGameclientMessage(std::move(message));
}

void sendMonsterBonusEffectAction(const uint32_t action, const std::optional<uint32_t> charmId, const std::optional<uint32_t> raceId)
{
    auto message = protocol::newGameclientMessage(228);
    auto* m = message->MutableExtension(GameclientMessageMonsterBonusEffectAction::gameclient_message_monster_bonus_effect_action);
    m->set_action(action);
    if (charmId)
        m->set_charm_id(*charmId);
    if (raceId)
        m->set_race_id(*raceId);
    protocol::sendGameclientMessage(std::move(message));
}

}

void TCyclopediaProtocolMessageHandler::sendInspectCyclopediaObject(const uint16_t objectId)
{
    auto message = protocol::newGameclientMessage(205);
    auto* m = message->MutableExtension(GameclientMessageInspectObject::gameclient_message_inspect_object);
    m->set_inspection_type(tibia::client::INSPECT_CYCLOPEDIA);
    m->set_object_id(objectId);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendMarketStatistics()
{
    protocol::sendGameclientMessage(protocol::newGameclientMessage(209));
}

void TCyclopediaProtocolMessageHandler::sendOpenMonsterCyclopedia()
{
    protocol::sendGameclientMessage(protocol::newGameclientMessage(225));
}

void TCyclopediaProtocolMessageHandler::sendOpenMonsterCyclopediaMonsters(const QString& classification)
{
    auto message = protocol::newGameclientMessage(226);
    auto* m = message->MutableExtension(GameclientMessageOpenMonsterCyclopediaMonsters::gameclient_message_open_monster_cyclopedia_monsters);
    m->set_request_type(0);
    m->set_category(classification.toStdString());
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendOpenMonsterCyclopediaMonsters(const std::vector<uint16_t>& raceIds)
{
    auto message = protocol::newGameclientMessage(226);
    auto* m = message->MutableExtension(GameclientMessageOpenMonsterCyclopediaMonsters::gameclient_message_open_monster_cyclopedia_monsters);
    m->set_request_type(1);
    m->mutable_race_ids()->Reserve(static_cast<int>(raceIds.size()));
    for (const uint16_t raceId : raceIds)
        m->add_race_ids(raceId);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendOpenMonsterCyclopediaRace(const uint32_t raceId)
{
    auto message = protocol::newGameclientMessage(227);
    message->MutableExtension(GameclientMessageOpenMonsterCyclopediaRace::gameclient_message_open_monster_cyclopedia_race)->set_race_id(raceId);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendUnlockCharm(const uint32_t charmId)
{
    sendMonsterBonusEffectAction(0, charmId, std::nullopt);
}

void TCyclopediaProtocolMessageHandler::sendAssignCharm(const uint32_t charmId, const uint32_t raceId)
{
    sendMonsterBonusEffectAction(1, charmId, raceId);
}

void TCyclopediaProtocolMessageHandler::sendRemoveCharm(const uint32_t charmId)
{
    sendMonsterBonusEffectAction(2, charmId, std::nullopt);
}

void TCyclopediaProtocolMessageHandler::sendResetAllCharms()
{
    sendMonsterBonusEffectAction(3, std::nullopt, std::nullopt);
}

void TCyclopediaProtocolMessageHandler::sendRequestMapArea(const uint32_t areaId)
{
    auto message = protocol::newGameclientMessage(219);
    message->MutableExtension(GameclientMessageCyclopediaMapAction::gameclient_message_cyclopedia_map_action)->set_area_id(areaId);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendMapDonation(const uint32_t areaId, const uint64_t amount)
{
    auto message = protocol::newGameclientMessage(219);
    auto* donation = message->MutableExtension(GameclientMessageCyclopediaMapAction::gameclient_message_cyclopedia_map_action)->mutable_donation();
    donation->set_area_id(areaId);
    donation->set_amount(static_cast<uint32_t>(amount));
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendResetMapAreaViewpoints(const uint32_t areaId)
{
    auto message = protocol::newGameclientMessage(219);
    message->MutableExtension(GameclientMessageCyclopediaMapAction::gameclient_message_cyclopedia_map_action)->set_reset_area_id(areaId);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendOpenCharacterInfo(const uint32_t characterId, const int infoType)
{
    auto message = protocol::newGameclientMessage(229);
    auto* m = message->MutableExtension(GameclientMessageOpenCyclopediaCharacterInfo::gameclient_message_open_cyclopedia_character_info);
    m->set_character_id(characterId);
    m->set_info_type(static_cast<uint32_t>(infoType));
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendOpenCharacterInfo(const uint32_t characterId, const int infoType,
                                                              const uint32_t entriesPerPage, const uint32_t page)
{
    auto message = protocol::newGameclientMessage(229);
    auto* m = message->MutableExtension(GameclientMessageOpenCyclopediaCharacterInfo::gameclient_message_open_cyclopedia_character_info);
    m->set_character_id(characterId);
    m->set_info_type(static_cast<uint32_t>(infoType));
    m->set_entries_per_page(entriesPerPage);
    m->set_page(page);
    protocol::sendGameclientMessage(std::move(message));
    if (infoType == tibia::client::CYCLOPEDIA_CHARACTERINFO_GENERALSTATS)
        sendOpenCharacterInfo(characterId, tibia::client::CYCLOPEDIA_CHARACTERINFO_WHEEL);
}

void TCyclopediaProtocolMessageHandler::sendRequestTownHouses(const QString& town)
{
    auto message = protocol::newGameclientMessage(173);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action);
    m->set_action(0);
    // "My Houses" asks for no town.
    m->set_name(town.compare(qtTrId("houses_my_houses_town_name")) == 0 ? std::string() : town.toStdString());
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendHouseBid(const uint32_t houseId, const uint64_t bid)
{
    auto message = protocol::newGameclientMessage(173);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action);
    m->set_action(1);
    m->set_house_id(houseId);
    m->set_bid(bid);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendHouseMoveOut(const uint32_t houseId, const uint32_t date)
{
    auto message = protocol::newGameclientMessage(173);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action);
    m->set_action(2);
    m->set_house_id(houseId);
    m->set_timestamp(date);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendHouseTransfer(const uint32_t houseId, const uint32_t date, const QString& newOwner,
                                                          const uint64_t price)
{
    auto message = protocol::newGameclientMessage(173);
    auto* m = message->MutableExtension(GameclientMessageCyclopediaHouseAction::gameclient_message_cyclopedia_house_action);
    m->set_action(3);
    m->set_house_id(houseId);
    m->set_timestamp(date);
    m->set_name(newOwner.toStdString());
    m->set_bid(price);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendHouseCancelMoveOut(const uint32_t houseId)
{
    sendHouseAction(4, houseId);
}

void TCyclopediaProtocolMessageHandler::sendHouseCancelTransfer(const uint32_t houseId)
{
    sendHouseAction(5, houseId);
}

void TCyclopediaProtocolMessageHandler::sendHouseAcceptTransfer(const uint32_t houseId)
{
    sendHouseAction(6, houseId);
}

void TCyclopediaProtocolMessageHandler::sendHouseRejectTransfer(const uint32_t houseId)
{
    sendHouseAction(7, houseId);
}

void TCyclopediaProtocolMessageHandler::sendResetPartyHuntAnalyser()
{
    auto message = protocol::newGameclientMessage(43);
    message->MutableExtension(GameclientMessagePartyHuntAnalyser::gameclient_message_party_hunt_analyser)->set_action(0);
    protocol::sendGameclientMessage(std::move(message));
}

void TCyclopediaProtocolMessageHandler::sendPartyHuntObjectPrices(const std::vector<uint16_t>& objectIds)
{
    if (objectIds.empty())
        return;
    auto message = protocol::newGameclientMessage(43);
    auto* m = message->MutableExtension(GameclientMessagePartyHuntAnalyser::gameclient_message_party_hunt_analyser);
    m->set_action(2);
    analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
    for (const uint16_t objectId : objectIds) {
        auto* entry = m->add_prices();
        entry->set_object_id(objectId);
        entry->set_price(static_cast<uint64_t>(prices->lootPrice(objectId)));
    }
    protocol::sendGameclientMessage(std::move(message));
}

void TBestiaryTrackerProtocolMessageHandler::handleBestiaryTrackerMessage(
    const ::tibia::protobuf::protocol::GameserverMessageBestiaryTracker& message)
{
    const int trackerType = static_cast<int>(message.tracker_type());
    std::vector<TCreatureTrackerEntry> entries;
    entries.reserve(static_cast<std::size_t>(message.entries_size()));
    // The pack contract's rows: race, kills, the three unlock thresholds, completed.
    std::vector<std::tuple<uint16_t, uint32_t, uint16_t, uint16_t, uint16_t, uint8_t>> rows;
    for (const auto& e : message.entries()) {
        rows.emplace_back(static_cast<uint16_t>(e.race_id()), e.kills(), static_cast<uint16_t>(e.first_unlock()),
                          static_cast<uint16_t>(e.second_unlock()), static_cast<uint16_t>(e.last_unlock()),
                          static_cast<uint8_t>(e.completed() ? 1 : 0));
        TCreatureTrackerEntry& entry = entries.emplace_back();
        entry.raceId = static_cast<uint16_t>(e.race_id());
        // The monster's or the boss's static race name, each word capitalised.
        entry.name = capitalizeWords(QString::fromStdString(tibia::game::raceData(static_cast<uint32_t>(entry.raceId)).name));
        entry.currentKills = e.kills();
        entry.killsForDetails1 = static_cast<uint16_t>(e.first_unlock());
        entry.killsForDetails2 = static_cast<uint16_t>(e.second_unlock());
        entry.killsForDetails3 = static_cast<uint16_t>(e.last_unlock());
        entry.status = e.completed() ? 1 : 0;
    }
    tibia::game::notify("onCreatureTracker", trackerType, entries);
    tibia::game::notify("onParseCyclopediaTracker", static_cast<uint8_t>(trackerType), rows);
}

}
