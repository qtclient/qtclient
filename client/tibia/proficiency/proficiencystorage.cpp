#include "proficiency/proficiencystorage.h"

#include <tibia/assets/thingtype.h>
#include <tibia/assets/things.h>
#include <framework/core/resourcemanager.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <climits>

namespace tibia::proficiency {

namespace {

using Args = std::vector<std::any>;

TProficiencyStorage* g_instance = nullptr;

/// CIP DAT_1022f530c / 0x1022f5334 / 0x1022f535c (also 0x1022ea830.. and 0x1022ea920..): crossbows,
/// knight-only weapons, the rest.
constexpr std::array<uint32_t, 10> CrossbowXP { 0, 600, 8000, 30000, 150000, 650000, 2500000, 10000000, 20000000, 30000000 };
constexpr std::array<uint32_t, 10> KnightXP { 0, 1250, 20000, 80000, 300000, 1500000, 6000000, 20000000, 40000000, 60000000 };
constexpr std::array<uint32_t, 10> DefaultXP { 0, 1750, 25000, 100000, 400000, 2000000, 8000000, 30000000, 60000000, 90000000 };
constexpr int CrossbowWeaponType = 6;
constexpr int KnightProfession = 1;

ThingTypePtr itemType(const uint16_t typeId)
{
    if (typeId == 0 || !g_things.isValidDatId(typeId, ThingCategoryItem))
        return nullptr;
    return g_things.getThingType(typeId, ThingCategoryItem);
}

const std::array<uint32_t, 10>& tableOf(const uint16_t typeId)
{
    const ThingTypePtr type = itemType(typeId);
    if (!type)
        return DefaultXP;
    if (type->getAppearanceWeaponType() == CrossbowWeaponType)
        return CrossbowXP;
    const std::vector<uint32_t>& professions = type->getRestrictVocation();
    if (professions.size() == 1 && professions.front() == KnightProfession)
        return KnightXP;
    return DefaultXP;
}

TProficiencyPerk parsePerk(const QJsonObject& json)
{
    TProficiencyPerk perk;
    perk.type = json.value(QLatin1String("Type")).toInt(-1);
    perk.value = json.value(QLatin1String("Value")).toDouble();
    perk.skillId = json.value(QLatin1String("SkillId")).toInt();
    perk.element = json.contains(QLatin1String("DamageType")) ? json.value(QLatin1String("DamageType")).toInt()
                                                              : json.value(QLatin1String("ElementId")).toInt();
    perk.augmentType = json.value(QLatin1String("AugmentType")).toInt();
    perk.spellId = json.value(QLatin1String("SpellId")).toInt();
    perk.bestiaryId = json.value(QLatin1String("BestiaryId")).toInt();
    perk.bestiaryName = json.value(QLatin1String("BestiaryName")).toString();
    perk.range = json.value(QLatin1String("Range")).toInt();
    perk.multiplier = json.value(QLatin1String("Multiplier")).toDouble();
    perk.probability = json.value(QLatin1String("Probability")).toDouble();
    return perk;
}

}

bool TProficiencyData::hasPick(const int level) const
{
    return std::ranges::any_of(picks, [level](const TPerkPick& pick) { return pick.picked && pick.level == level; });
}

void TProficiencyData::pick(const int level, const int perk)
{
    if (level < 0)
        return;
    if (picks.size() <= static_cast<size_t>(level))
        picks.resize(level + 1);
    picks[level] = { static_cast<uint8_t>(level), static_cast<uint8_t>(perk), true };
}

const TShapedSlot* TProficiencyData::shapedSlot(const int level, const int perk) const
{
    const auto it = std::ranges::find_if(shaped, [level, perk](const TShapedSlot& slot) { return slot.level == level && slot.perk == perk; });
    return it == shaped.end() ? nullptr : &*it;
}

TProficiencyStorage::TProficiencyStorage(QObject* parent)
    : QObject(parent)
{
    g_instance = this;
    m_hooks.emplace_back("g_game", "onWeaponProficiency", [this](LuaObject*, const Args& args) {
        if (args.size() < 3)
            return std::any{};
        const auto typeId = static_cast<uint16_t>(client::gamecore::argInt(args, 0));
        TProficiencyData data;
        data.xp = static_cast<uint32_t>(client::gamecore::argInt(args, 1));
        if (const auto* perks = std::any_cast<std::map<uint8_t, uint8_t>>(&args[2])) {
            for (const auto& [level, perk] : *perks)
                data.pick(level, perk);
        }
        if (args.size() > 4) {
            if (const auto* slots = std::any_cast<std::vector<std::map<std::string, uint16_t>>>(&args[4])) {
                for (const auto& slot : *slots) {
                    const auto field = [&slot](const char* key) {
                        const auto it = slot.find(key);
                        return it == slot.end() ? uint16_t{ 0 } : it->second;
                    };
                    data.shaped.push_back({ static_cast<uint8_t>(field("grade")), static_cast<uint8_t>(field("slot")),
                                            field("modifierEnum"), static_cast<uint8_t>(field("refineLevel")) });
                }
            }
        }
        m_data[typeId] = std::move(data);
        Q_EMIT proficiencyDataChanged(typeId);
        Q_EMIT proficiencyXPChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onWeaponProficiencyExperience", [this](LuaObject*, const Args& args) {
        const auto typeId = static_cast<uint16_t>(client::gamecore::argInt(args, 0));
        m_data[typeId].xp = static_cast<uint32_t>(client::gamecore::argInt(args, 1));
        Q_EMIT proficiencyXPChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onWeaponProficiencyReshape", [this](LuaObject*, const Args& args) {
        if (args.size() < 4)
            return std::any{};
        TReshapeOffers offers;
        offers.typeId = static_cast<uint16_t>(client::gamecore::argInt(args, 0));
        offers.level = static_cast<uint8_t>(client::gamecore::argInt(args, 1));
        offers.perk = static_cast<uint8_t>(client::gamecore::argInt(args, 2));
        if (const auto* entries = std::any_cast<std::vector<std::map<std::string, uint16_t>>>(&args[3])) {
            for (const auto& entry : *entries) {
                const auto field = [&entry](const char* key) {
                    const auto it = entry.find(key);
                    return it == entry.end() ? uint16_t{ 0 } : it->second;
                };
                offers.offers.push_back({ offers.level, offers.perk, field("modifierEnum"), static_cast<uint8_t>(field("refineLevel")) });
            }
        }
        m_reshapeOffers = std::move(offers);
        Q_EMIT reshapeOffersChanged();
        return std::any{};
    });
    m_hooks.emplace_back("g_game", "onGameEnd", [this](LuaObject*, const Args&) {
        m_data.clear();
        m_reshapeOffers.reset();
        m_listRequested = false;
        m_tables.clear();
        m_tablesLoaded = false;
        Q_EMIT proficiencyXPChanged();
        return std::any{};
    });
}

TProficiencyStorage::~TProficiencyStorage()
{
    if (g_instance == this)
        g_instance = nullptr;
}

TProficiencyStorage* TProficiencyStorage::instance() { return g_instance; }

void TProficiencyStorage::loadTables() const
{
    if (m_tablesLoaded)
        return;
    const std::string path = g_things.getProficienciesFile();
    if (path.empty())
        return;
    m_tablesLoaded = true;
    const QJsonDocument document = QJsonDocument::fromJson(QByteArray::fromStdString(tibia::files::data().readFileContents(path)));
    for (const QJsonValue& entry : document.array()) {
        const QJsonObject object = entry.toObject();
        TProficiencyInfo info;
        info.name = object.value(QLatin1String("Name")).toString();
        for (const QJsonValue& levelValue : object.value(QLatin1String("Levels")).toArray()) {
            TProficiencyLevel level;
            for (const QJsonValue& perk : levelValue.toObject().value(QLatin1String("Perks")).toArray())
                level.perks.push_back(parsePerk(perk.toObject()));
            info.levels.push_back(std::move(level));
        }
        m_tables[static_cast<uint32_t>(object.value(QLatin1String("ProficiencyId")).toInt())] = std::move(info);
    }
}

const TProficiencyInfo* TProficiencyStorage::info(const uint16_t typeId) const
{
    const ThingTypePtr type = itemType(typeId);
    if (!type || type->getProficiencyId() == 0)
        return nullptr;
    loadTables();
    const auto it = m_tables.find(type->getProficiencyId());
    return it == m_tables.end() ? nullptr : &it->second;
}

const TProficiencyData* TProficiencyStorage::data(const uint16_t typeId) const
{
    const auto it = m_data.find(typeId);
    return it == m_data.end() ? nullptr : &it->second;
}

uint32_t TProficiencyStorage::xp(const uint16_t typeId) const
{
    const TProficiencyData* entry = data(typeId);
    return entry ? entry->xp : 0;
}

int TProficiencyStorage::levelCount(const uint16_t typeId) const
{
    const TProficiencyInfo* table = info(typeId);
    return table ? std::min<int>(static_cast<int>(table->levels.size()) + 1, 9) : 9;
}

uint32_t TProficiencyStorage::threshold(const int index, const uint16_t typeId, const bool mastery)
{
    if (index < 0 || index >= 10)
        return INT_MAX;
    if (mastery && index == 1)
        return INT_MAX;
    return tableOf(typeId)[index];
}

uint32_t TProficiencyStorage::levelXP(const int level, const int tableLevels, const uint16_t typeId)
{
    const int index = (level <= tableLevels || level > 8) ? level : tableLevels + 2;
    return threshold(index, typeId);
}

int TProficiencyStorage::reachedLevel(const uint16_t typeId, const int count) const
{
    const int64_t current = static_cast<int32_t>(xp(typeId));
    const int last = std::max(count - 1, 0);
    int level = 0;
    while (level != count) {
        const int next = level + (level < count ? 1 : 0);
        const int index = (next > last && next <= 8) ? last + 2 : next;
        if (static_cast<int64_t>(threshold(index, typeId)) > current)
            break;
        level = next;
    }
    return level;
}

int TProficiencyStorage::level(const uint16_t typeId) const { return reachedLevel(typeId, levelCount(typeId)); }

int TProficiencyStorage::decorationLevel(const uint16_t typeId) const
{
    const TProficiencyInfo* table = info(typeId);
    const int cap = table ? std::min<int>(static_cast<int>(table->levels.size()), 9) : 8;
    return std::min(level(typeId), cap);
}

bool TProficiencyStorage::hasMastery(const uint16_t typeId) const
{
    return info(typeId) && level(typeId) == levelCount(typeId);
}

}
