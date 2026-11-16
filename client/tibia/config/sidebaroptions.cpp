#include "sidebaroptions.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <climits>
#include <cmath>
#include <span>
#include <stdexcept>

namespace tibia::config {

namespace {

// Section keys, in CIP's order.
const QString PanelsManagerKey = QStringLiteral("sidebarPanelsMangerOptions");
const QString PanelsKey = QStringLiteral("sidebarPanelsOptions");
const QString WidgetsManagerKey = QStringLiteral("sidebarWidgetsMangerOptions");
const QString PreyKey = QStringLiteral("preyWidgetOptions");
const QString BattleListsKey = QStringLiteral("battleListsOptions");
const QString SkillsKey = QStringLiteral("skillsWidgetOptions");
const QString SpellListKey = QStringLiteral("spellListWidgetOptions");
const QString VipKey = QStringLiteral("vipWidgetOptions");
const QString HuntingSessionKey = QStringLiteral("huntingSessionAnalyserWidgetOptions");
const QString ImpactKey = QStringLiteral("impactAnalyserWidgetOptions");
const QString DamageInputKey = QStringLiteral("damageInputAnalyserWidgetOptions");
const QString LootAnalyserKey = QStringLiteral("lootAnalyserWidgetOptions");
const QString SupplyAnalyserKey = QStringLiteral("supplyAnalyserWidgetOptions");
const QString LootTrackerKey = QStringLiteral("lootTrackerWidgetOptions");
const QString XpAnalyserKey = QStringLiteral("xpAnalyserWidgetOptions");
const QString QuestTrackerKey = QStringLiteral("questTrackerWidgetOptions");
const QString BestiaryTrackerKey = QStringLiteral("bestiaryTrackerWidgetOptions");
const QString BosstiaryTrackerKey = QStringLiteral("bosstiaryTrackerWidgetOptions");
const QString BossTrackerKey = QStringLiteral("bossTrackerWidgetOptions");
const QString NpcTradeKey = QStringLiteral("npcTradeOptions");
const QString ContainersKey = QStringLiteral("containersOptions");
const QString UnjustifiedPointsKey = QStringLiteral("unjustifiedPointsOptions");
const QString AnalyticsSelectorKey = QStringLiteral("analyticsSelectorOptions");
// CIP's spelling.
const QString DepotSearchKey = QStringLiteral("deoptSearchWidgetOptions");
const QString PartyHuntKey = QStringLiteral("partyHuntAnalyserOptions");
const QString ImbuementTrackerKey = QStringLiteral("imbuementTrackerWidgetOptions");
const QString PlayerGuideKey = QStringLiteral("playerGuideWidgetOptions");

const QString ContentHeightKey = QStringLiteral("contentHeight");
const QString ContentMaximizedKey = QStringLiteral("contentMaximized");

using NameEntry = std::pair<int, const char*>;
using NameMap = std::span<const NameEntry>;

constexpr NameEntry PanelNames[] = { { 0, "minimap" }, { 1, "inventoryStatus" }, { 2, "hitpointManabar" }, { 3, "buttons" } };
constexpr NameEntry WidgetNames[] = {
    { 0, "skills" },
    { 1, "battleList" },
    { 3, "vip" },
    { 4, "unjustifiedPoints" },
    { 5, "prey" },
    { 6, "spellList" },
    { 8, "container" },
    { 10, "questTracker" },
    { 11, "bestiaryTracker" },
    { 12, "bosstiaryTracker" },
    { 13, "bossCooldowns" },
    { 14, "lootTracker" },
    { 15, "xpAnalyser" },
    { 16, "lootAnalyser" },
    { 17, "wasteAnalyser" },
    { 18, "impactAnalyser" },
    { 19, "damageInputAnalyser" },
    { 20, "huntingSessionAnalyser" },
    { 21, "analyticsSelector" },
    { 22, "partyHuntAnalyser" },
    { 23, "imbuementTracker" },
    { 24, "playerGuide" },
};
constexpr NameEntry SkillNames[] = {
    { 1, "experience" },    { 2, "level" },         { 0x13, "stamina" },      { 0x14, "offlinetraining" },
    { 4, "magiclevel" },    { 0xf, "fistfighting" }, { 0xc, "clubfighting" }, { 0xd, "swordfighting" },
    { 0xe, "axefighting" }, { 0xb, "distancefighting" }, { 10, "shielding" }, { 0x10, "fishing" },
};
constexpr NameEntry VipSortNames[] = { { 0, "byName" }, { 1, "byType" }, { 2, "byState" } };
constexpr NameEntry BattleListSortNames[] = {
    { 0, "byAgeAscending" },       { 1, "byAgeDescending" },       { 2, "byDistanceAscending" }, { 3, "byDistanceDescending" },
    { 4, "byHitpointsAscending" }, { 5, "byHitpointsDescending" }, { 6, "byNameAscending" },     { 7, "byNameDescending" },
};
constexpr NameEntry BattleListFilterNames[] = {
    { 0, "hidePlayers" },  { 6, "hideNPCs" },     { 7, "hideMonsters" },   { 8, "hideNonSkulledPlayers" },
    { 9, "hidePartyMembers" }, { 10, "hidePlayerSummons" }, { 1, "hideKnights" }, { 2, "hidePaladins" },
    { 3, "hideDruids" },   { 4, "hideSorcerers" }, { 5, "hideMonks" },     { 11, "hideMembersOfOwnGuild" },
};
constexpr NameEntry CreatureTrackerSortKeyNames[] = { { 0, "name" }, { 1, "completion" }, { 2, "remaining" } };
constexpr NameEntry BossTrackerSortKeyNames[] = { { 1, "name" }, { 0, "remainingCooldown" } };
constexpr NameEntry SortOrderNames[] = { { 0, "ascending" }, { 1, "descending" } };
constexpr NameEntry NpcTradeSortNames[] = { { 0, "name" }, { 3, "amount" }, { 1, "price" }, { 2, "weight" } };

/// Widget types whose entries carry an instance.
bool hasInstance(int type) { return type == static_cast<int>(SidebarWidgetType::BattleList) || type == static_cast<int>(SidebarWidgetType::Container); }

const char* nameOf(const NameMap& map, int value)
{
    for (const auto& [id, name] : map) {
        if (id == value)
            return name;
    }
    return nullptr;
}

bool valueOf(const NameMap& map, const QString& name, int* value)
{
    for (const auto& [id, n] : map) {
        if (name == QLatin1String(n)) {
            *value = id;
            return true;
        }
    }
    return false;
}

// CIP's checked readers: the default when the key is missing or of another type.
bool readBool(const QJsonObject& json, const QString& key, bool fallback)
{
    const QJsonValue value = json.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

int readInt(const QJsonObject& json, const QString& key, int min, int max, int fallback)
{
    const QJsonValue value = json.value(key);
    const int read = value.isDouble() ? value.toInt() : fallback;
    return std::min(std::max(min, read), max);
}

QString readString(const QJsonObject& json, const QString& key)
{
    const QJsonValue value = json.value(key);
    return value.isString() ? value.toString() : QString();
}

QJsonArray readArray(const QJsonObject& json, const QString& key)
{
    const QJsonValue value = json.value(key);
    return value.isArray() ? value.toArray() : QJsonArray();
}

void parseBase(TWidgetOptionsBase& options, const QJsonObject& json)
{
    options.setContentHeightAndMaximized(readInt(json, ContentHeightKey, 0, INT_MAX, 0), readBool(json, ContentMaximizedKey, true));
}

void serializeBase(const TWidgetOptionsBase& options, QJsonObject& json)
{
    json.insert(ContentHeightKey, options.contentHeight);
    json.insert(ContentMaximizedKey, options.contentMaximized);
}

QJsonObject serializeBaseOnly(const TWidgetOptionsBase& options)
{
    QJsonObject json;
    serializeBase(options, json);
    return json;
}

TWidgetOptionsBase parseBaseOnly(const QJsonObject& json)
{
    TWidgetOptionsBase options;
    parseBase(options, json);
    return options;
}

QJsonArray namesOf(const NameMap& map, const auto& values)
{
    QJsonArray names;
    for (const int value : values) {
        if (const char* name = nameOf(map, value))
            names.append(QLatin1String(name));
    }
    return names;
}

std::vector<int> valuesOf(const NameMap& map, const QJsonArray& names)
{
    std::vector<int> values;
    for (const QJsonValue& name : names) {
        int value = 0;
        if (valueOf(map, name.toString(), &value))
            values.push_back(value);
    }
    return values;
}

// sidebarPanelsMangerOptions
QJsonObject serializePanelsOrder(const QList<SidebarPanelType>& order)
{
    std::vector<int> values;
    for (const SidebarPanelType type : order)
        values.push_back(static_cast<int>(type));
    return { { QStringLiteral("panelsOrder"), namesOf(PanelNames, values) } };
}

QList<SidebarPanelType> parsePanelsOrder(const QJsonObject& json)
{
    QList<SidebarPanelType> order;
    for (const int value : valuesOf(PanelNames, readArray(json, QStringLiteral("panelsOrder"))))
        order.append(static_cast<SidebarPanelType>(value));
    return order;
}

// sidebarPanelsOptions
QJsonObject serializePanels(const SidebarPanelsOptions& o)
{
    return {
        { QStringLiteral("minimapZoomLevel"), static_cast<int>(std::round(static_cast<double>(o.minimapZoomLevel) * 100.0)) },
        { QStringLiteral("controlButtonsVisible"), o.controlButtonsVisible },
        { QStringLiteral("minimizeInventory"), o.minimizeInventory },
        { QStringLiteral("chaseModeEnabled"), o.chaseModeEnabled },
    };
}

SidebarPanelsOptions parsePanels(const QJsonObject& json)
{
    SidebarPanelsOptions o;
    o.minimapZoomLevel = static_cast<float>(readInt(json, QStringLiteral("minimapZoomLevel"), -100, 200, 0)) / 100.0f;
    o.controlButtonsVisible = readBool(json, QStringLiteral("controlButtonsVisible"), false);
    o.minimizeInventory = readBool(json, QStringLiteral("minimizeInventory"), false);
    o.chaseModeEnabled = readBool(json, QStringLiteral("chaseModeEnabled"), true);
    return o;
}

// sidebarWidgetsMangerOptions
QJsonObject serializeWidgetsManager(const TSidebarWidgetsManagerOptions& o)
{
    QJsonArray sidebars;
    for (const auto& widgets : o.openWidgetsOrderPerSidebar) {
        QJsonArray entries;
        for (const auto& [type, instance] : widgets) {
            const char* name = nameOf(WidgetNames, type);
            if (!name)
                continue;
            QJsonObject entry{ { QStringLiteral("type"), QLatin1String(name) } };
            if (hasInstance(type))
                entry.insert(QStringLiteral("instance"), instance);
            entries.append(entry);
        }
        sidebars.append(entries);
    }
    return { { QStringLiteral("openWidgetsOrderPerSidebar"), sidebars }, { QStringLiteral("leftSidebarCount"), o.leftSidebarCount } };
}

TSidebarWidgetsManagerOptions parseWidgetsManager(const QJsonObject& json)
{
    TSidebarWidgetsManagerOptions o;
    for (const QJsonValue& sidebar : readArray(json, QStringLiteral("openWidgetsOrderPerSidebar"))) {
        std::vector<std::pair<int, int>> widgets;
        for (const QJsonValue& value : sidebar.toArray()) {
            if (!value.isObject())
                continue;
            const QJsonObject entry = value.toObject();
            int type = 0;
            if (!valueOf(WidgetNames, entry.value(QStringLiteral("type")).toString(), &type))
                continue;
            int instance = 0;
            if (hasInstance(type)) {
                const QJsonValue read = entry.value(QStringLiteral("instance"));
                if (!read.isDouble())
                    continue;
                instance = read.toInt();
            }
            widgets.emplace_back(type, instance);
        }
        o.openWidgetsOrderPerSidebar.push_back(std::move(widgets));
    }
    o.leftSidebarCount = static_cast<quint8>(readInt(json, QStringLiteral("leftSidebarCount"), 0, 0xfd, 0));
    return o;
}

// battleListsOptions
QJsonObject serializeBattleList(const TBattleListOptions& o)
{
    QJsonObject json{
        { QStringLiteral("battleListSortOrder"), namesOf(BattleListSortNames, o.sortOrder) },
        { QStringLiteral("battleListFilters"), namesOf(BattleListFilterNames, o.filters) },
        { QStringLiteral("showFilters"), o.showFilters },
        { QStringLiteral("isPartyView"), o.isPartyView },
        { QStringLiteral("isPrimary"), o.isPrimary },
        { QStringLiteral("name"), o.name },
    };
    serializeBase(o, json);
    return json;
}

TBattleListOptions parseBattleList(const QJsonObject& json)
{
    TBattleListOptions o;
    std::vector<int> sortOrder = valuesOf(BattleListSortNames, readArray(json, QStringLiteral("battleListSortOrder")));
    if (sortOrder.empty())
        sortOrder.push_back(0);
    o.setSortOrder(sortOrder.front());
    o.filters = valuesOf(BattleListFilterNames, readArray(json, QStringLiteral("battleListFilters")));
    o.showFilters = readBool(json, QStringLiteral("showFilters"), true);
    o.isPartyView = readBool(json, QStringLiteral("isPartyView"), false);
    o.isPrimary = readBool(json, QStringLiteral("isPrimary"), false);
    o.name = readString(json, QStringLiteral("name"));
    parseBase(o, json);
    return o;
}

/// A map key as CIP reads it: the number as a byte, 0 when it is none.
int byteKey(const QString& key)
{
    bool ok = false;
    const qlonglong value = key.toLongLong(&ok);
    if (!ok || value != static_cast<int>(value))
        return 0;
    return static_cast<quint8>(value);
}

QJsonObject serializeBattleLists(const TBattleListsOptions& lists)
{
    QJsonObject json;
    for (const auto& [id, options] : lists)
        json.insert(QString::number(static_cast<quint8>(id)), serializeBattleList(options));
    return json;
}

TBattleListsOptions parseBattleLists(const QJsonObject& json)
{
    TBattleListsOptions lists;
    for (auto it = json.begin(); it != json.end(); ++it)
        lists[byteKey(it.key())] = parseBattleList(it.value().toObject());
    return lists;
}

// skillsWidgetOptions
QJsonObject serializeSkills(const TSkillsWidgetOptions& o)
{
    QJsonObject json{
        { QStringLiteral("invisibleProgressBars"), namesOf(SkillNames, o.invisibleProgressBars) },
        { QStringLiteral("offenceStatsVisible"), o.offenceStatsVisible },
        { QStringLiteral("defenceStatsVisible"), o.defenceStatsVisible },
        { QStringLiteral("miscStatsVisible"), o.miscStatsVisible },
    };
    serializeBase(o, json);
    return json;
}

TSkillsWidgetOptions parseSkills(const QJsonObject& json)
{
    TSkillsWidgetOptions o;
    for (const int skill : valuesOf(SkillNames, readArray(json, QStringLiteral("invisibleProgressBars"))))
        o.invisibleProgressBars.insert(skill);
    o.offenceStatsVisible = readBool(json, QStringLiteral("offenceStatsVisible"), true);
    o.defenceStatsVisible = readBool(json, QStringLiteral("defenceStatsVisible"), true);
    o.miscStatsVisible = readBool(json, QStringLiteral("miscStatsVisible"), true);
    parseBase(o, json);
    return o;
}

// spellListWidgetOptions; showUnkownSpells is CIP's spelling.
QJsonObject serializeSpellList(const TSpellListWidgetOptions& o)
{
    QJsonObject json{
        { QStringLiteral("showOnlyCurrentVocation"), o.showOnlyCurrentVocation },
        { QStringLiteral("showOnlyCurrentLevel"), o.showOnlyCurrentLevel },
        { QStringLiteral("showUnkownSpells"), o.showUnknownSpells },
        { QStringLiteral("showDruidSpells"), o.showDruidSpells },
        { QStringLiteral("showKnightSpells"), o.showKnightSpells },
        { QStringLiteral("showPaladinSpells"), o.showPaladinSpells },
        { QStringLiteral("showSorcererSpells"), o.showSorcererSpells },
        { QStringLiteral("showMonkSpells"), o.showMonkSpells },
        { QStringLiteral("showAttackSpellGroup"), o.showAttackSpellGroup },
        { QStringLiteral("showHealingSpellGroup"), o.showHealingSpellGroup },
        { QStringLiteral("showSupportSpellGroup"), o.showSupportSpellGroup },
        { QStringLiteral("showPremiumOnlySpells"), o.showPremiumOnlySpells },
        { QStringLiteral("showFreeSpells"), o.showFreeSpells },
    };
    serializeBase(o, json);
    return json;
}

TSpellListWidgetOptions parseSpellList(const QJsonObject& json)
{
    TSpellListWidgetOptions o;
    o.showOnlyCurrentVocation = readBool(json, QStringLiteral("showOnlyCurrentVocation"), false);
    o.showOnlyCurrentLevel = readBool(json, QStringLiteral("showOnlyCurrentLevel"), false);
    o.showUnknownSpells = readBool(json, QStringLiteral("showUnkownSpells"), false);
    o.showDruidSpells = readBool(json, QStringLiteral("showDruidSpells"), true);
    o.showKnightSpells = readBool(json, QStringLiteral("showKnightSpells"), true);
    o.showPaladinSpells = readBool(json, QStringLiteral("showPaladinSpells"), true);
    o.showSorcererSpells = readBool(json, QStringLiteral("showSorcererSpells"), true);
    o.showMonkSpells = readBool(json, QStringLiteral("showMonkSpells"), true);
    o.showAttackSpellGroup = readBool(json, QStringLiteral("showAttackSpellGroup"), true);
    o.showHealingSpellGroup = readBool(json, QStringLiteral("showHealingSpellGroup"), true);
    o.showSupportSpellGroup = readBool(json, QStringLiteral("showSupportSpellGroup"), true);
    o.showPremiumOnlySpells = readBool(json, QStringLiteral("showPremiumOnlySpells"), true);
    o.showFreeSpells = readBool(json, QStringLiteral("showFreeSpells"), true);
    parseBase(o, json);
    return o;
}

// vipWidgetOptions
QJsonObject serializeVip(const TVipWidgetOptions& o)
{
    QJsonObject json{
        { QStringLiteral("vipSortOrder"), namesOf(VipSortNames, o.vipSortOrder) },
        { QStringLiteral("hideOfflineVips"), o.hideOfflineVips },
        { QStringLiteral("showGrouped"), o.showGrouped },
    };
    serializeBase(o, json);
    return json;
}

TVipWidgetOptions parseVip(const QJsonObject& json)
{
    TVipWidgetOptions o;
    o.vipSortOrder = valuesOf(VipSortNames, readArray(json, QStringLiteral("vipSortOrder")));
    o.hideOfflineVips = readBool(json, QStringLiteral("hideOfflineVips"), false);
    o.showGrouped = readBool(json, QStringLiteral("showGrouped"), false);
    parseBase(o, json);
    return o;
}

// bestiaryTrackerWidgetOptions / bosstiaryTrackerWidgetOptions
QJsonObject serializeCreatureTracker(const TCreatureTrackerWidgetOptions& o)
{
    QJsonObject json;
    if (const char* name = nameOf(CreatureTrackerSortKeyNames, o.sortKey))
        json.insert(QStringLiteral("sortKey"), QLatin1String(name));
    if (const char* name = nameOf(SortOrderNames, o.sortOrder))
        json.insert(QStringLiteral("sortOrder"), QLatin1String(name));
    serializeBase(o, json);
    return json;
}

TCreatureTrackerWidgetOptions parseCreatureTracker(const QJsonObject& json)
{
    TCreatureTrackerWidgetOptions o;
    int value = 0;
    if (valueOf(CreatureTrackerSortKeyNames, json.value(QStringLiteral("sortKey")).toString(), &value))
        o.sortKey = value;
    if (valueOf(SortOrderNames, json.value(QStringLiteral("sortOrder")).toString(), &value))
        o.sortOrder = value;
    parseBase(o, json);
    return o;
}

// bossTrackerWidgetOptions
QJsonObject serializeBossTracker(const TBossTrackerWidgetOptions& o)
{
    QJsonObject json;
    if (const char* name = nameOf(BossTrackerSortKeyNames, o.sortKey))
        json.insert(QStringLiteral("sortKey"), QLatin1String(name));
    serializeBase(o, json);
    return json;
}

TBossTrackerWidgetOptions parseBossTracker(const QJsonObject& json)
{
    TBossTrackerWidgetOptions o;
    int value = 0;
    if (valueOf(BossTrackerSortKeyNames, json.value(QStringLiteral("sortKey")).toString(), &value))
        o.sortKey = value;
    parseBase(o, json);
    return o;
}

// npcTradeOptions; CIP throws on a sort field missing from its map.
QJsonObject serializeNpcTrade(const TNpcTradeOptions& o)
{
    const char* buying = nameOf(NpcTradeSortNames, o.sortFieldWhenBuying);
    const char* selling = nameOf(NpcTradeSortNames, o.sortFieldWhenSelling);
    if (!buying || !selling)
        throw std::out_of_range("map::at:  key not found");
    QJsonObject json{
        { QStringLiteral("allowSellingEquippedItems"), o.allowSellingEquippedItems },
        { QStringLiteral("ignoreCapacityWhenBuying"), o.ignoreCapacityWhenBuying },
        { QStringLiteral("purchaseShoppingBagWhenBuying"), o.purchaseShoppingBagWhenBuying },
        { QStringLiteral("showNameFilter"), o.showNameFilter },
        { QStringLiteral("allowLargeTradesWithoutWarning"), o.allowLargeTradesWithoutWarning },
        { QStringLiteral("sortFieldWhenBuying"), QLatin1String(buying) },
        { QStringLiteral("sortFieldWhenSelling"), QLatin1String(selling) },
    };
    serializeBase(o, json);
    return json;
}

TNpcTradeOptions parseNpcTrade(const QJsonObject& json)
{
    TNpcTradeOptions o;
    o.allowSellingEquippedItems = readBool(json, QStringLiteral("allowSellingEquippedItems"), false);
    o.ignoreCapacityWhenBuying = readBool(json, QStringLiteral("ignoreCapacityWhenBuying"), false);
    o.purchaseShoppingBagWhenBuying = readBool(json, QStringLiteral("purchaseShoppingBagWhenBuying"), false);
    o.showNameFilter = readBool(json, QStringLiteral("showNameFilter"), true);
    o.allowLargeTradesWithoutWarning = readBool(json, QStringLiteral("allowLargeTradesWithoutWarning"), false);
    int value = 0;
    if (valueOf(NpcTradeSortNames, readString(json, QStringLiteral("sortFieldWhenBuying")), &value))
        o.sortFieldWhenBuying = value;
    if (valueOf(NpcTradeSortNames, readString(json, QStringLiteral("sortFieldWhenSelling")), &value))
        o.sortFieldWhenSelling = value;
    parseBase(o, json);
    return o;
}

// containersOptions
QJsonObject serializeContainers(const ContainersOptions& containers)
{
    QJsonObject json;
    for (const auto& [id, options] : containers)
        json.insert(QString::number(static_cast<quint8>(id)), serializeBaseOnly(options));
    return json;
}

ContainersOptions parseContainers(const QJsonObject& json)
{
    ContainersOptions containers;
    for (auto it = json.begin(); it != json.end(); ++it)
        containers[byteKey(it.key())] = parseBaseOnly(it.value().toObject());
    return containers;
}

// deoptSearchWidgetOptions
QJsonObject serializeDepotSearch(const TDepotSearchWidgetOptions& o)
{
    QJsonObject json{ { QStringLiteral("showLockerOnly"), o.showLockerOnly } };
    serializeBase(o, json);
    return json;
}

TDepotSearchWidgetOptions parseDepotSearch(const QJsonObject& json)
{
    TDepotSearchWidgetOptions o;
    o.showLockerOnly = readBool(json, QStringLiteral("showLockerOnly"), false);
    parseBase(o, json);
    return o;
}

// imbuementTrackerWidgetOptions
QJsonObject serializeImbuementTracker(const TImbuementTrackerWidgetOptions& o)
{
    QJsonObject json{
        { QStringLiteral("showAlmostGone"), o.showAlmostGone },
        { QStringLiteral("showUsed"), o.showUsed },
        { QStringLiteral("showAlmostNew"), o.showAlmostNew },
        { QStringLiteral("showEmptySlots"), o.showEmptySlots },
    };
    serializeBase(o, json);
    return json;
}

TImbuementTrackerWidgetOptions parseImbuementTracker(const QJsonObject& json)
{
    TImbuementTrackerWidgetOptions o;
    o.showAlmostGone = readBool(json, QStringLiteral("showAlmostGone"), true);
    o.showUsed = readBool(json, QStringLiteral("showUsed"), true);
    o.showAlmostNew = readBool(json, QStringLiteral("showAlmostNew"), true);
    o.showEmptySlots = readBool(json, QStringLiteral("showEmptySlots"), true);
    parseBase(o, json);
    return o;
}

// playerGuideWidgetOptions
QJsonArray intArray(const std::set<int>& values)
{
    QJsonArray array;
    for (const int value : values)
        array.append(value);
    return array;
}

std::set<int> intSet(const QJsonArray& array)
{
    std::set<int> values;
    for (const QJsonValue& value : array)
        values.insert(value.toInt());
    return values;
}

QJsonObject serializePlayerGuide(const TPlayerGuideWidgetOptions& o)
{
    QJsonObject json{
        { QStringLiteral("pendingLevelSpecialPopups"), intArray(o.pendingLevelSpecialPopups) },
        { QStringLiteral("confirmedLevelSpecialPopups"), intArray(o.confirmedLevelSpecialPopups) },
    };
    serializeBase(o, json);
    return json;
}

TPlayerGuideWidgetOptions parsePlayerGuide(const QJsonObject& json)
{
    TPlayerGuideWidgetOptions o;
    o.pendingLevelSpecialPopups = intSet(readArray(json, QStringLiteral("pendingLevelSpecialPopups")));
    o.confirmedLevelSpecialPopups = intSet(readArray(json, QStringLiteral("confirmedLevelSpecialPopups")));
    parseBase(o, json);
    return o;
}

const QStringList& knownKeys()
{
    static const QStringList keys{
        PanelsManagerKey, PanelsKey,           WidgetsManagerKey, PreyKey,          BattleListsKey,       SkillsKey,
        SpellListKey,     VipKey,              HuntingSessionKey, ImpactKey,        DamageInputKey,       LootAnalyserKey,
        SupplyAnalyserKey, LootTrackerKey,     XpAnalyserKey,     QuestTrackerKey,  BestiaryTrackerKey,   BosstiaryTrackerKey,
        BossTrackerKey,   NpcTradeKey,         ContainersKey,     UnjustifiedPointsKey, AnalyticsSelectorKey, DepotSearchKey,
        PartyHuntKey,     ImbuementTrackerKey, PlayerGuideKey,
    };
    return keys;
}

/// The section's object when json has one (CIP's checked section read).
bool sectionOf(const QJsonObject& json, const QString& key, QJsonObject* section)
{
    const QJsonValue value = json.value(key);
    if (!value.isObject())
        return false;
    *section = value.toObject();
    return true;
}

}

void TBattleListOptions::setSortOrder(const int order)
{
    sortOrder.clear();
    sortOrder.push_back(order);
    sortOrder.push_back(0);
}

TSidebarOptions::TSidebarOptions(QObject* parent)
    : cip::tibia::config::TSidebarOptions(parent)
{
    reset();
    m_containersChangedTimer.setInterval(0);
    m_containersChangedTimer.setSingleShot(true);
    connect(&m_containersChangedTimer, &QTimer::timeout, this, &TSidebarOptions::onDelayOptionsForAllContainersChangedTriggerd);
}

void TSidebarOptions::reset()
{
    m_panelsOrder.clear();
    m_panels = {};
    // A battle list on the first right sidebar, the player guide and the quest
    // tracker on the second.
    m_widgetsManager = {};
    m_widgetsManager.openWidgetsOrderPerSidebar = {
        { { static_cast<int>(SidebarWidgetType::BattleList), PrimaryBattleListId } },
        { { static_cast<int>(SidebarWidgetType::PlayerGuide), 0 }, { static_cast<int>(SidebarWidgetType::QuestTracker), 0 } },
    };
    m_prey = {};
    m_battleLists.clear();
    m_skills = {};
    m_spellList = {};
    m_vip = {};
    m_huntingSessionAnalyser = {};
    m_impactAnalyser = {};
    m_damageInputAnalyser = {};
    m_lootAnalyser = {};
    m_wasteAnalyser = {};
    m_lootTracker = {};
    m_progressAnalyser = {};
    m_questTracker = {};
    m_bestiaryTracker = {};
    m_bosstiaryTracker = {};
    m_bossTracker = {};
    m_npcTrade = {};
    m_containers.clear();
    m_unjustifiedPoints = {};
    m_analyticsSelector = {};
    m_depotSearch = {};
    m_partyHuntAnalyser = {};
    m_imbuementTracker = {};
    m_playerGuide = {};
    m_unknownSections = {};
    ensurePartyBattleList();
    ensurePrimaryBattleList();
}

QString TSidebarOptions::fileName() { return QStringLiteral("sidebars"); }

QString TSidebarOptions::filePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/characterdata/%1/%2.json").arg(m_characterId).arg(fileName());
}

void TSidebarOptions::loadForCharacter(const quint32 characterId)
{
    if (characterId == m_characterId)
        return;
    save();
    m_characterId = characterId;
    QJsonObject root;
    if (characterId != 0) {
        QFile file(filePath());
        if (file.open(QIODevice::ReadOnly))
            root = QJsonDocument::fromJson(file.readAll()).object();
    }
    m_containersChangedTimer.stop();
    reset();
    fromJson(root);
    m_dirty = false;
}

void TSidebarOptions::save()
{
    if (!m_dirty || m_characterId == 0)
        return;
    QJsonObject root = m_unknownSections;
    const QJsonObject known = toJson();
    for (auto it = known.begin(); it != known.end(); ++it)
        root.insert(it.key(), it.value());
    const QString path = filePath();
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        if (file.commit())
            m_dirty = false;
    }
}

QJsonObject TSidebarOptions::toJson() const
{
    QJsonObject json;
    json.insert(PanelsManagerKey, serializePanelsOrder(m_panelsOrder));
    json.insert(PanelsKey, serializePanels(m_panels));
    json.insert(WidgetsManagerKey, serializeWidgetsManager(m_widgetsManager));
    json.insert(PreyKey, serializeBaseOnly(m_prey));
    json.insert(BattleListsKey, serializeBattleLists(m_battleLists));
    json.insert(SkillsKey, serializeSkills(m_skills));
    json.insert(SpellListKey, serializeSpellList(m_spellList));
    json.insert(VipKey, serializeVip(m_vip));
    json.insert(HuntingSessionKey, serializeBaseOnly(m_huntingSessionAnalyser));
    json.insert(ImpactKey, serializeBaseOnly(m_impactAnalyser));
    json.insert(DamageInputKey, serializeBaseOnly(m_damageInputAnalyser));
    json.insert(LootAnalyserKey, serializeBaseOnly(m_lootAnalyser));
    json.insert(SupplyAnalyserKey, serializeBaseOnly(m_wasteAnalyser));
    json.insert(LootTrackerKey, serializeBaseOnly(m_lootTracker));
    json.insert(XpAnalyserKey, serializeBaseOnly(m_progressAnalyser));
    json.insert(QuestTrackerKey, serializeBaseOnly(m_questTracker));
    json.insert(BestiaryTrackerKey, serializeCreatureTracker(m_bestiaryTracker));
    json.insert(BosstiaryTrackerKey, serializeCreatureTracker(m_bosstiaryTracker));
    json.insert(BossTrackerKey, serializeBossTracker(m_bossTracker));
    json.insert(NpcTradeKey, serializeNpcTrade(m_npcTrade));
    json.insert(ContainersKey, serializeContainers(m_containers));
    json.insert(UnjustifiedPointsKey, serializeBaseOnly(m_unjustifiedPoints));
    json.insert(AnalyticsSelectorKey, serializeBaseOnly(m_analyticsSelector));
    json.insert(DepotSearchKey, serializeDepotSearch(m_depotSearch));
    json.insert(PartyHuntKey, serializeBaseOnly(m_partyHuntAnalyser));
    json.insert(ImbuementTrackerKey, serializeImbuementTracker(m_imbuementTracker));
    json.insert(PlayerGuideKey, serializePlayerGuide(m_playerGuide));
    return json;
}

void TSidebarOptions::fromJson(const QJsonObject& json)
{
    QJsonObject s;
    if (sectionOf(json, PanelsManagerKey, &s))
        m_panelsOrder = parsePanelsOrder(s);
    if (sectionOf(json, PanelsKey, &s))
        m_panels = parsePanels(s);
    if (sectionOf(json, WidgetsManagerKey, &s))
        m_widgetsManager = parseWidgetsManager(s);
    if (sectionOf(json, PreyKey, &s))
        m_prey = parseBaseOnly(s);
    m_battleLists.clear();
    if (sectionOf(json, BattleListsKey, &s))
        m_battleLists = parseBattleLists(s);
    if (sectionOf(json, SkillsKey, &s))
        m_skills = parseSkills(s);
    if (sectionOf(json, SpellListKey, &s))
        m_spellList = parseSpellList(s);
    if (sectionOf(json, VipKey, &s))
        m_vip = parseVip(s);
    if (sectionOf(json, HuntingSessionKey, &s))
        m_huntingSessionAnalyser = parseBaseOnly(s);
    if (sectionOf(json, ImpactKey, &s))
        m_impactAnalyser = parseBaseOnly(s);
    if (sectionOf(json, DamageInputKey, &s))
        m_damageInputAnalyser = parseBaseOnly(s);
    if (sectionOf(json, LootAnalyserKey, &s))
        m_lootAnalyser = parseBaseOnly(s);
    if (sectionOf(json, SupplyAnalyserKey, &s))
        m_wasteAnalyser = parseBaseOnly(s);
    if (sectionOf(json, LootTrackerKey, &s))
        m_lootTracker = parseBaseOnly(s);
    if (sectionOf(json, XpAnalyserKey, &s))
        m_progressAnalyser = parseBaseOnly(s);
    if (sectionOf(json, QuestTrackerKey, &s))
        m_questTracker = parseBaseOnly(s);
    if (sectionOf(json, BestiaryTrackerKey, &s))
        m_bestiaryTracker = parseCreatureTracker(s);
    if (sectionOf(json, BosstiaryTrackerKey, &s))
        m_bosstiaryTracker = parseCreatureTracker(s);
    if (sectionOf(json, BossTrackerKey, &s))
        m_bossTracker = parseBossTracker(s);
    if (sectionOf(json, NpcTradeKey, &s))
        m_npcTrade = parseNpcTrade(s);
    if (sectionOf(json, ContainersKey, &s))
        m_containers = parseContainers(s);
    if (sectionOf(json, UnjustifiedPointsKey, &s))
        m_unjustifiedPoints = parseBaseOnly(s);
    if (sectionOf(json, AnalyticsSelectorKey, &s))
        m_analyticsSelector = parseBaseOnly(s);
    if (sectionOf(json, DepotSearchKey, &s))
        m_depotSearch = parseDepotSearch(s);
    if (sectionOf(json, PartyHuntKey, &s))
        m_partyHuntAnalyser = parseBaseOnly(s);
    if (sectionOf(json, ImbuementTrackerKey, &s))
        m_imbuementTracker = parseImbuementTracker(s);
    if (sectionOf(json, PlayerGuideKey, &s))
        m_playerGuide = parsePlayerGuide(s);
    ensurePartyBattleList();
    ensurePrimaryBattleList();

    m_unknownSections = json;
    for (const QString& key : knownKeys())
        m_unknownSections.remove(key);

    // CIP also publishes TGameActionSetTactics with the chase mode here;
    // qtclient's inventory panel applies it on sidebarPanelsOptionsChanged.
    Q_EMIT sidebarPanelsManagerOptionsChanged();
    Q_EMIT sidebarPanelsOptionsChanged();
    Q_EMIT sidebarWidgetsManagerOptionsChanged();
    Q_EMIT killTrackerWidgetOptionsChanged();
    for (const auto& [id, options] : m_battleLists)
        Q_EMIT battleListWidgetOptionsChanged(id);
    Q_EMIT skillsWidgetOptionsChanged();
    Q_EMIT spellListWidgetOptionsChanged();
    Q_EMIT vipWidgetOptionsChanged();
    Q_EMIT huntingSessionAnalyserWidgetOptionsChanged();
    Q_EMIT impactAnalyserWidgetOptionsChanged();
    Q_EMIT damageInputAnalyserWidgetOptionsChanged();
    Q_EMIT lootAnalyserWidgetOptionsChanged();
    Q_EMIT wasteAnalyserWidgetOptionsChanged();
    Q_EMIT lootTrackerWidgetOptionsChanged();
    Q_EMIT progressAnalyserWidgetOptionsChanged();
    Q_EMIT questTrackerWidgetOptionsChanged();
    Q_EMIT bestiaryTrackerWidgetOptionsChanged();
    Q_EMIT bosstiaryTrackerWidgetOptionsChanged();
    Q_EMIT bossTrackerWidgetOptionsChanged();
    Q_EMIT npcTradeOptionsChanged();
    Q_EMIT optionsForAllContainersChanged();
    Q_EMIT unjustifiedPointsWidgetOptionsChanged();
    Q_EMIT analyticsSelectorWidgetOptionsChanged();
    Q_EMIT depotSearchWidgetOptionsChanged();
    Q_EMIT partyHuntAnalyserhWidgetOptionsChanged();
    // CIP does not signal the imbuement tracker here.
    Q_EMIT playerGuideWidgetOptionsChanged();
}

void TSidebarOptions::ensurePartyBattleList()
{
    bool found = false;
    for (auto& [id, options] : m_battleLists) {
        if (!options.isPartyView)
            continue;
        if (found)
            options.isPartyView = false;
        found = true;
    }
    if (found)
        return;
    TBattleListOptions party;
    party.isPartyView = true;
    m_battleLists[PartyBattleListId] = party;
}

void TSidebarOptions::ensurePrimaryBattleList()
{
    bool found = false;
    for (auto& [id, options] : m_battleLists) {
        if (!options.isPrimary)
            continue;
        if (found)
            options.isPrimary = false;
        found = true;
    }
    if (found)
        return;
    TBattleListOptions primary;
    primary.isPrimary = true;
    m_battleLists[PrimaryBattleListId] = primary;
}

void TSidebarOptions::changed()
{
    m_dirty = true;
    Q_EMIT configurationChangedFromUserInput();
}

void TSidebarOptions::onDelayOptionsForAllContainersChangedTriggerd()
{
    Q_EMIT optionsForAllContainersChanged();
    changed();
}

void TSidebarOptions::setPanelsOrder(const QList<SidebarPanelType>& order)
{
    if (order == m_panelsOrder)
        return;
    m_panelsOrder = order;
    Q_EMIT sidebarPanelsManagerOptionsChanged();
    changed();
}

void TSidebarOptions::setSidebarPanelsOptions(const SidebarPanelsOptions& options)
{
    if (options == m_panels)
        return;
    m_panels = options;
    Q_EMIT sidebarPanelsOptionsChanged();
    changed();
}

void TSidebarOptions::setWidgetsManagerOptions(const TSidebarWidgetsManagerOptions& options)
{
    if (options == m_widgetsManager)
        return;
    m_widgetsManager = options;
    Q_EMIT sidebarWidgetsManagerOptionsChanged();
    changed();
}

SidebarWidgetsManagerOptions TSidebarOptions::sidebarWidgetsManagerOptions() const
{
    const QJsonObject json = serializeWidgetsManager(m_widgetsManager);
    return { m_widgetsManager.leftSidebarCount, json.value(QStringLiteral("openWidgetsOrderPerSidebar")).toArray() };
}

void TSidebarOptions::setSidebarWidgetsManagerOptions(const SidebarWidgetsManagerOptions& options)
{
    setWidgetsManagerOptions(parseWidgetsManager(
        { { QStringLiteral("openWidgetsOrderPerSidebar"), options.openWidgetsOrderPerSidebar }, { QStringLiteral("leftSidebarCount"), options.leftSidebarCount } }));
}

void TSidebarOptions::setPreyWidgetOptions(const TWidgetOptionsBase& options)
{
    if (options == m_prey)
        return;
    m_prey = options;
    Q_EMIT killTrackerWidgetOptionsChanged();
    changed();
}

const TBattleListOptions& TSidebarOptions::primaryBattleListOptions() const
{
    for (const auto& [id, options] : m_battleLists) {
        if (options.isPrimary)
            return options;
    }
    throw std::runtime_error("there are no options for the primary battle list");
}

void TSidebarOptions::setBattleListsOptions(const TBattleListsOptions& options)
{
    if (options == m_battleLists)
        return;
    m_battleLists = options;
    for (const auto& [id, list] : m_battleLists)
        Q_EMIT battleListWidgetOptionsChanged(id);
    changed();
}

void TSidebarOptions::setBattleListOptions(const int battleListId, const TBattleListOptions& options)
{
    m_battleLists[battleListId] = options;
    Q_EMIT battleListWidgetOptionsChanged(battleListId);
    changed();
}

void TSidebarOptions::removeBattleListOptions(const int battleListId)
{
    m_battleLists.erase(battleListId);
    changed();
}

void TSidebarOptions::makePrimaryBattleList(const int battleListId)
{
    const TBattleListOptions primary = primaryBattleListOptions();
    TBattleListsOptions lists = m_battleLists;
    std::erase_if(lists, [](const auto& entry) { return entry.second.isPrimary; });
    lists[battleListId] = primary;
    setBattleListsOptions(lists);
}

void TSidebarOptions::makePartyBattleList(const int battleListId)
{
    TBattleListOptions party;
    party.isPartyView = true;
    for (const auto& [id, options] : m_battleLists) {
        if (options.isPartyView) {
            party = options;
            break;
        }
    }
    TBattleListsOptions lists = m_battleLists;
    std::erase_if(lists, [](const auto& entry) { return entry.second.isPartyView; });
    // The primary list's options move to the primary id when the party list
    // takes its place.
    const auto it = lists.find(battleListId);
    if (it != lists.end() && it->second.isPrimary)
        lists[PrimaryBattleListId] = it->second;
    lists[battleListId] = party;
    setBattleListsOptions(lists);
}

void TSidebarOptions::setSkillsWidgetOptions(const TSkillsWidgetOptions& options)
{
    if (options == m_skills)
        return;
    m_skills = options;
    Q_EMIT skillsWidgetOptionsChanged();
    changed();
}

void TSidebarOptions::setSpellListWidgetOptions(const TSpellListWidgetOptions& options)
{
    if (options == m_spellList)
        return;
    m_spellList = options;
    Q_EMIT spellListWidgetOptionsChanged();
    changed();
}

void TSidebarOptions::setVipWidgetOptions(const TVipWidgetOptions& options)
{
    if (options == m_vip)
        return;
    m_vip = options;
    Q_EMIT vipWidgetOptionsChanged();
    changed();
}

#define TIBIA_SIDEBAR_SETTER(Setter, Type, Member, Signal) \
    void TSidebarOptions::Setter(const Type& options)      \
    {                                                      \
        if (options == Member)                             \
            return;                                        \
        Member = options;                                  \
        Q_EMIT Signal();                                   \
        changed();                                         \
    }

TIBIA_SIDEBAR_SETTER(setHuntingSessionAnalyserWidgetOptions, TWidgetOptionsBase, m_huntingSessionAnalyser, huntingSessionAnalyserWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setImpactAnalyserWidgetOptions, TWidgetOptionsBase, m_impactAnalyser, impactAnalyserWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setDamageInputAnalyserWidgetOptions, TWidgetOptionsBase, m_damageInputAnalyser, damageInputAnalyserWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setLootAnalyserWidgetOptions, TWidgetOptionsBase, m_lootAnalyser, lootAnalyserWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setWasteAnalyserWidgetOptions, TWidgetOptionsBase, m_wasteAnalyser, wasteAnalyserWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setLootTrackerWidgetOptions, TWidgetOptionsBase, m_lootTracker, lootTrackerWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setProgressAnalyserWidgetOptions, TWidgetOptionsBase, m_progressAnalyser, progressAnalyserWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setQuestTrackerWidgetOptions, TWidgetOptionsBase, m_questTracker, questTrackerWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setBestiaryTrackerWidgetOptions, TCreatureTrackerWidgetOptions, m_bestiaryTracker, bestiaryTrackerWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setBosstiaryTrackerWidgetOptions, TCreatureTrackerWidgetOptions, m_bosstiaryTracker, bosstiaryTrackerWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setBossTrackerWidgetOptions, TBossTrackerWidgetOptions, m_bossTracker, bossTrackerWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setNpcTradeOptions, TNpcTradeOptions, m_npcTrade, npcTradeOptionsChanged)
TIBIA_SIDEBAR_SETTER(setUnjustifiedPointsWidgetOptions, TWidgetOptionsBase, m_unjustifiedPoints, unjustifiedPointsWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setAnalyticsSelectorWidgetOptions, TWidgetOptionsBase, m_analyticsSelector, analyticsSelectorWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setDepotSearchWidgetOptions, TDepotSearchWidgetOptions, m_depotSearch, depotSearchWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setPartyHuntAnalyserWidgetOptions, TWidgetOptionsBase, m_partyHuntAnalyser, partyHuntAnalyserhWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setImbuementTrackerWidgetOptions, TImbuementTrackerWidgetOptions, m_imbuementTracker, imbuementTrackerWidgetOptionsChanged)
TIBIA_SIDEBAR_SETTER(setPlayerGuideWidgetOptions, TPlayerGuideWidgetOptions, m_playerGuide, playerGuideWidgetOptionsChanged)

#undef TIBIA_SIDEBAR_SETTER

void TSidebarOptions::setContainersOptions(const ContainersOptions& options)
{
    if (options == m_containers)
        return;
    m_containers = options;
    m_containersChangedTimer.start();
}

QJsonValue TSidebarOptions::section(const QString& key) const
{
    if (!knownKeys().contains(key))
        return m_unknownSections.value(key);
    return toJson().value(key);
}

void TSidebarOptions::setSection(const QString& key, const QJsonValue& value)
{
    if (!knownKeys().contains(key)) {
        if (m_unknownSections.value(key) == value)
            return;
        m_unknownSections.insert(key, value);
        changed();
        return;
    }
    const QJsonObject json = value.toObject();
    if (key == PanelsManagerKey)
        setPanelsOrder(parsePanelsOrder(json));
    else if (key == PanelsKey)
        setSidebarPanelsOptions(parsePanels(json));
    else if (key == WidgetsManagerKey)
        setWidgetsManagerOptions(parseWidgetsManager(json));
    else if (key == PreyKey)
        setPreyWidgetOptions(parseBaseOnly(json));
    else if (key == BattleListsKey)
        setBattleListsOptions(parseBattleLists(json));
    else if (key == SkillsKey)
        setSkillsWidgetOptions(parseSkills(json));
    else if (key == SpellListKey)
        setSpellListWidgetOptions(parseSpellList(json));
    else if (key == VipKey)
        setVipWidgetOptions(parseVip(json));
    else if (key == HuntingSessionKey)
        setHuntingSessionAnalyserWidgetOptions(parseBaseOnly(json));
    else if (key == ImpactKey)
        setImpactAnalyserWidgetOptions(parseBaseOnly(json));
    else if (key == DamageInputKey)
        setDamageInputAnalyserWidgetOptions(parseBaseOnly(json));
    else if (key == LootAnalyserKey)
        setLootAnalyserWidgetOptions(parseBaseOnly(json));
    else if (key == SupplyAnalyserKey)
        setWasteAnalyserWidgetOptions(parseBaseOnly(json));
    else if (key == LootTrackerKey)
        setLootTrackerWidgetOptions(parseBaseOnly(json));
    else if (key == XpAnalyserKey)
        setProgressAnalyserWidgetOptions(parseBaseOnly(json));
    else if (key == QuestTrackerKey)
        setQuestTrackerWidgetOptions(parseBaseOnly(json));
    else if (key == BestiaryTrackerKey)
        setBestiaryTrackerWidgetOptions(parseCreatureTracker(json));
    else if (key == BosstiaryTrackerKey)
        setBosstiaryTrackerWidgetOptions(parseCreatureTracker(json));
    else if (key == BossTrackerKey)
        setBossTrackerWidgetOptions(parseBossTracker(json));
    else if (key == NpcTradeKey)
        setNpcTradeOptions(parseNpcTrade(json));
    else if (key == ContainersKey)
        setContainersOptions(parseContainers(json));
    else if (key == UnjustifiedPointsKey)
        setUnjustifiedPointsWidgetOptions(parseBaseOnly(json));
    else if (key == AnalyticsSelectorKey)
        setAnalyticsSelectorWidgetOptions(parseBaseOnly(json));
    else if (key == DepotSearchKey)
        setDepotSearchWidgetOptions(parseDepotSearch(json));
    else if (key == PartyHuntKey)
        setPartyHuntAnalyserWidgetOptions(parseBaseOnly(json));
    else if (key == ImbuementTrackerKey)
        setImbuementTrackerWidgetOptions(parseImbuementTracker(json));
    else if (key == PlayerGuideKey)
        setPlayerGuideWidgetOptions(parsePlayerGuide(json));
}

}
