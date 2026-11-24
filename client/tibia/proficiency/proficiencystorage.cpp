#include "proficiency/proficiencystorage.h"

#include <utility>

#include <tibia/appearances/appearancetype.h>
#include <tibia/assets/things.h>
#include "shared/tibiafilesystemhelper.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include <climits>
#include <tibia/game/gamenotify.h>
#include <tibia/game/tgamenotify.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::proficiency {

namespace {


TProficiencyStorage* g_instance = nullptr;

/// CIP DAT_1022f530c / / (also .. and ..): crossbows,
/// knight-only weapons, the rest.
constexpr std::array<uint32_t, 10> CrossbowXP { 0, 600, 8000, 30000, 150000, 650000, 2500000, 10000000, 20000000, 30000000 };
constexpr std::array<uint32_t, 10> KnightXP { 0, 1250, 20000, 80000, 300000, 1500000, 6000000, 20000000, 40000000, 60000000 };
constexpr std::array<uint32_t, 10> DefaultXP { 0, 1750, 25000, 100000, 400000, 2000000, 8000000, 30000000, 60000000, 90000000 };
constexpr int CrossbowWeaponType = 6;
constexpr int KnightProfession = 1;

TAppearanceTypePtr itemType(const uint16_t typeId)
{
    if (typeId == 0 || !tibia::game::isValidDatId(typeId, ThingCategoryItem))
        return nullptr;
    return tibia::game::thingType(typeId, ThingCategoryItem);
}

const std::array<uint32_t, 10>& tableOf(const uint16_t typeId)
{
    const TAppearanceTypePtr type = itemType(typeId);
    if (!type)
        return DefaultXP;
    if (type->getAppearanceWeaponType() == CrossbowWeaponType)
        return CrossbowXP;
    const std::vector<uint32_t>& professions = type->getRestrictVocation();
    if (professions.size() == 1 && professions.front() == KnightProfession)
        return KnightXP;
    return DefaultXP;
}

// JSON fields as CipSoft reads them: a missing key or a non-number throws.
QJsonValue field(const QJsonObject& json, const char* key)
{
    const auto it = json.constFind(QLatin1String(key));
    if (it == json.constEnd())
        throw std::out_of_range(std::string("key '") + key + "' not found");
    return *it;
}

double real(const QJsonObject& json, const char* key)
{
    const QJsonValue value = field(json, key);
    if (!value.isDouble())
        throw std::domain_error("type must be number");
    return value.toDouble();
}

int integer(const QJsonObject& json, const char* key)
{
    const QJsonValue value = field(json, key);
    if (value.isBool())
        return value.toBool() ? 1 : 0;
    if (!value.isDouble())
        throw std::domain_error("type must be number");
    return static_cast<int>(static_cast<int64_t>(value.toDouble()));
}

QString text(const QJsonObject& json, const char* key)
{
    const QJsonValue value = field(json, key);
    if (!value.isString())
        throw std::domain_error("type must be string");
    return value.toString();
}

TProficiencyPerk parsePerk(const QJsonObject& json)
{
    TProficiencyPerk perk;
    perk.type = static_cast<uint8_t>(integer(json, "Type"));
    switch (perk.type) {
    case 0: case 1: case 2:
    case 18: case 19: case 20: case 21: case 24:
        perk.value = integer(json, "Value");
        break;
    case 3:
        perk.skillId = integer(json, "SkillId");
        perk.value = integer(json, "Value");
        break;
    case 4:
        perk.element = integer(json, "DamageType");
        perk.value = integer(json, "Value");
        break;
    case 5:
        perk.augmentType = integer(json, "AugmentType");
        perk.spellId = integer(json, "SpellId");
        perk.value = real(json, "Value");
        break;
    case 6:
        perk.bestiaryId = integer(json, "BestiaryId");
        perk.bestiaryName = text(json, "BestiaryName");
        perk.value = real(json, "Value");
        break;
    case 9: case 13: case 31:
        perk.element = integer(json, "ElementId");
        perk.value = real(json, "Value");
        break;
    case 22:
        perk.value = integer(json, "Value");
        perk.range = static_cast<uint8_t>(integer(json, "Range"));
        break;
    case 25: case 26: case 27:
        perk.skillId = integer(json, "SkillId");
        perk.value = real(json, "Value");
        break;
    case 32:
        perk.element = integer(json, "ElementId");
        perk.multiplier = real(json, "Multiplier");
        perk.probability = real(json, "Probability");
        perk.missileId = integer(json, "MissileId");
        break;
    case 7: case 8: case 10: case 11: case 12: case 14: case 15: case 16: case 17:
    case 23: case 28: case 29: case 30: case 33: case 34: case 35:
        perk.value = real(json, "Value");
        break;
    default:
        throw std::runtime_error("Unknown WeaponPerk type: " + std::to_string(perk.type));
    }
    return perk;
}

}

static TProficiencyLevel parseProficiencyLevel(const QJsonObject& json)
{
    const QJsonValue perks = field(json, "Perks");
    if (!perks.isArray())
        throw std::domain_error("type must be array");
    TProficiencyLevel level;
    for (const QJsonValue& perk : perks.toArray())
        level.perks.push_back(parsePerk(perk.toObject()));
    return level;
}

static TProficiencyInfo parseProficiency(const QJsonObject& json)
{
    TProficiencyInfo info;
    info.proficiencyId = integer(json, "ProficiencyId");
    info.version = integer(json, "Version");
    info.name = text(json, "Name");
    const QJsonValue levels = field(json, "Levels");
    if (!levels.isArray())
        throw std::domain_error("type must be array");
    for (const QJsonValue& level : levels.toArray())
        info.levels.push_back(parseProficiencyLevel(level.toObject()));
    return info;
}

std::vector<TProficiencyInfo> parseProficiencies(const QByteArray& contents)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(contents, &error);
    if (error.error != QJsonParseError::NoError)
        throw std::invalid_argument(error.errorString().toStdString());
    std::vector<TProficiencyInfo> proficiencies;
    for (const QJsonValue& entry : document.array())
        proficiencies.push_back(parseProficiency(entry.toObject()));
    return proficiencies;
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

TProficiencyStorage::TProficiencyStorage(std::shared_ptr<appearances::TAppearancesManager> appearancesManager)
    : m_appearancesManager(std::move(appearancesManager))
{
    g_instance = this;
    connect(tibia::game::TGameNotify::instance(), &tibia::game::TGameNotify::resourcesBalanceChanged, this,
            [this](qint64, qint64, int) { Q_EMIT resourceBalanceChanged(); });
}

void TProficiencyStorage::clear()
{
    m_currentProficiencyXP = 0;
    m_unspentPerk = false;
    m_data.clear();
    m_listRequested = false;
    m_lunarAscensionOrbs = 0;
    Q_EMIT unspentPerkChanged();
}

TProficiencyStorage::~TProficiencyStorage()
{
    if (g_instance == this)
        g_instance = nullptr;
}

TProficiencyStorage* TProficiencyStorage::instance() { return g_instance; }

void TProficiencyStorage::setProficiencyData(const uint16_t typeId, TProficiencyData data)
{
    m_data[typeId] = std::move(data);
    Q_EMIT proficiencyDataChanged(typeId);
    Q_EMIT proficiencyXPChanged();
}

void TProficiencyStorage::setReshapeOffers(TReshapeOffers offers)
{
    m_reshapeOffers = std::move(offers);
    Q_EMIT shapedPerkReshapeOffersChanged();
}

void TProficiencyStorage::setUnspentPerk(const bool unspent)
{
    if (m_unspentPerk == unspent)
        return;
    m_unspentPerk = unspent;
    Q_EMIT unspentPerkChanged();
}

void TProficiencyStorage::setProficiencyExperience(const uint32_t xp, const uint16_t typeId, const bool isCurrentWeapon)
{
    if (isCurrentWeapon && m_currentProficiencyXP != xp) {
        m_currentProficiencyXP = xp;
        Q_EMIT currentProficiencyProgressChanged();
    }
    if (typeId == 0)
        return;
    m_data[typeId].xp = xp;
    Q_EMIT proficiencyDataChanged(typeId);
    Q_EMIT proficiencyXPChanged();
}

void TProficiencyStorage::setLunarAscensionOrbs(const uint32_t orbs)
{
    if (m_lunarAscensionOrbs == orbs)
        return;
    m_lunarAscensionOrbs = orbs;
    Q_EMIT resourceBalanceChanged();
}

void TProficiencyStorage::loadTables() const
{
    if (m_tablesLoaded)
        return;
    const std::string path = tibia::game::getProficienciesFile();
    if (path.empty())
        return;
    m_tablesLoaded = true;
    try {
        readTables(path);
    } catch (const std::exception&) {
        m_tables.clear();
    }
}

void TProficiencyStorage::readTables(const std::string& path) const
{
    setTables(parseProficiencies(QByteArray::fromStdString(tibia::files::data().readFileContents(path))));
}

void TProficiencyStorage::setTables(const std::vector<TProficiencyInfo>& proficiencies) const
{
    m_tables.clear();
    for (const TProficiencyInfo& proficiency : proficiencies)
        m_tables.emplace(static_cast<uint32_t>(proficiency.proficiencyId), proficiency);
}

const TProficiencyInfo* TProficiencyStorage::info(const uint16_t typeId) const
{
    const TAppearanceTypePtr type = itemType(typeId);
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
