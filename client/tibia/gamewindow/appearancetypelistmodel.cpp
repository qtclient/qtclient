#include "appearancetypelistmodel.h"

#include "analysers/itempricestorage.h"
#include "config/clientoptions.h"
#include "container/containerciptypes.h"
#include "container/containerslotinformation.h"
#include "container/qmlitemslotdata.h"
#include "cyclopedia/itemtrackingstorage.h"
#include "game/playerdata.h"
#include "platform/platform.h"
#include "proficiency/proficiencystorage.h"


#include <QLocale>

#include <algorithm>
#include <tibia/appearances/appearancetype.h>
#include <tibia/game/gamesession.h>
#include <tibia/game/thingsession_forward.h>

namespace tibia::gamewindow {

namespace {

/// CIP: cumulative counts shown on a slot stop at 65,000.
constexpr int MaximumShownCount = 65000;

}

TAppearanceTypeListModel::TAppearanceTypeListModel(const bool firstDisplayFlag, const bool secondDisplayFlag,
                                                   const bool useDisplayTypeId, config::TClientOptions* clientOptions,
                                                   QObject* parent)
    : TSortedVectorListModel(parent)
    , m_firstDisplayFlag(firstDisplayFlag)
    , m_secondDisplayFlag(secondDisplayFlag)
    , m_useDisplayTypeId(useDisplayTypeId)
    , m_clientOptions(clientOptions)
{
    if (m_clientOptions != nullptr)
        m_clientOptionsConnections.add(connect(m_clientOptions, &config::TClientOptions::appearanceTypeListModelOptionsChanged,
                                               this, &TAppearanceTypeListModel::onItemPriceVisualizationChanged));
    m_proficiencyTimer.setInterval(0);
    m_proficiencyTimer.setSingleShot(true);
    connect(&m_proficiencyTimer, &QTimer::timeout, this, &TAppearanceTypeListModel::onProficiencyTimerTimeout);
}

TAppearanceTypeListModel::TAppearanceTypeListModel(CountProvider counts, QObject* parent)
    : TAppearanceTypeListModel(false, false, false, nullptr, parent)
{
    m_counts = std::move(counts);
}

void TAppearanceTypeListModel::connectToStorages(analysers::TItemPriceStorage* itemPriceStorage,
                                                 std::shared_ptr<cyclopedia::TItemTrackingStorage> itemTrackingStorage,
                                                 std::shared_ptr<container::TObjectCountStorage> objectCountStorage,
                                                 proficiency::TProficiencyStorage* proficiencyStorage)
{
    m_storageConnections.disconnectAll();
    m_itemPriceStorage = itemPriceStorage;
    m_itemTrackingStorage = std::move(itemTrackingStorage);
    m_objectCountStorage = std::move(objectCountStorage);
    m_proficiencyStorage = proficiencyStorage;
    if (m_itemPriceStorage != nullptr)
        m_storageConnections.add(connect(m_itemPriceStorage, &analysers::TItemPriceStorage::configurationChangedFromUserInput, this,
                                         &TAppearanceTypeListModel::onItemPriceVisualizationChanged));
    if (m_itemTrackingStorage)
        m_storageConnections.add(connect(m_itemTrackingStorage.get(), &cyclopedia::TItemTrackingStorage::configurationChangedFromUserInput,
                                         this, &TAppearanceTypeListModel::onItemTrackingChanged));
    if (m_objectCountStorage)
        m_storageConnections.add(connect(m_objectCountStorage.get(), &container::TObjectCountStorage::objectCountsChanged, this,
                                         &TAppearanceTypeListModel::onObjectCountsChanged));
    // This connection is not dropped by a later connect or disconnect.
    if (m_proficiencyStorage != nullptr)
        connect(m_proficiencyStorage, &proficiency::TProficiencyStorage::proficiencyDataChanged, this,
                [this](const uint typeId) { onProficiencyDataChanged(static_cast<uint16_t>(typeId)); });
}

void TAppearanceTypeListModel::disconnectFromStorages()
{
    m_storageConnections.disconnectAll();
    m_itemPriceStorage = nullptr;
    m_itemTrackingStorage.reset();
    m_objectCountStorage.reset();
    m_proficiencyStorage = nullptr;
}

uint32_t TAppearanceTypeListModel::objectCount(const uint16_t typeId, const std::optional<uint8_t> tier) const
{
    if (!m_objectCountStorage)
        return 0;
    if (!tier)
        return m_objectCountStorage->count(typeId);
    return m_objectCountStorage->count(typeId, *tier);
}

QString TAppearanceTypeListModel::formatAmount(const qulonglong amount) { return QLocale(QLocale::English).toString(amount); }

int TAppearanceTypeListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int TAppearanceTypeListModel::count(const TAppearanceTypeEntry& entry) const
{
    // CIP: a list showing display type ids counts the type itself, any other
    // list counts the type's cyclopedia type.
    const uint16_t typeId = m_useDisplayTypeId ? entry.typeId : cyclopediaTypeID(entry);
    if (m_objectCountStorage)
        return static_cast<int>(objectCount(typeId, entry.hasTier ? std::optional<uint8_t>(entry.tier) : std::nullopt));
    return m_counts ? m_counts(typeId, entry.tier) : 0;
}

bool TAppearanceTypeListModel::hasCounts() const
{
    // Host: owners without an object count storage count through m_counts.
    return m_objectCountStorage || m_counts;
}

uint16_t TAppearanceTypeListModel::cyclopediaTypeID(const TAppearanceTypeEntry& entry) const
{
    const TAppearanceTypePtr type = tibia::game::thingType(entry.typeId, ThingCategoryItem);
    return type ? static_cast<uint16_t>(type->getCyclopediaType()) : 0;
}

QString TAppearanceTypeListModel::lootValueImageSource(const TAppearanceTypeEntry& entry, const int style) const
{
    // CIP: the price storage's loot value level of the cyclopedia type, shown
    // under (style 1) or over (style 2) the object as the options' loot style
    // says. Host: a list without client options shows none.
    if (m_itemPriceStorage == nullptr)
        return QString();
    const int level = m_itemPriceStorage->lootValueLevel(cyclopediaTypeID(entry));
    if (level == 0 || m_clientOptions == nullptr || m_clientOptions->colorizeLootStyle() != style)
        return QString();
    return QStringLiteral("image://%1/%2")
        .arg(style == 1 ? QStringLiteral("containerslot-glowingborders") : QStringLiteral("containerslot-coloredges"))
        .arg(level - 1);
}

uint16_t TAppearanceTypeListModel::displayTypeID(const TAppearanceTypeEntry& entry) const
{
    const TAppearanceTypePtr type = tibia::game::thingType(entry.typeId, ThingCategoryItem);
    if (!type)
        return entry.typeId;
    if (const appearances::TAppearanceMarketProperties* market = type->marketProperties(); m_useDisplayTypeId && market)
        return market->showAs;
    const auto cyclopediaType = static_cast<uint16_t>(type->getCyclopediaType());
    return cyclopediaType != 0 ? cyclopediaType : type->getId();
}

QVariant TAppearanceTypeListModel::data(const QModelIndex& index, const int role) const
{
    using Info = cip::tibia::qmlcomponents::TObjectAppearanceInstanceInfoQmlType;
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const TAppearanceTypeEntry& entry = m_entries[index.row()];
    // CIP: the count of the type (or of its tier) comes first, 0 without a count storage.
    const uint n = hasCounts() ? static_cast<uint>(count(entry)) : 0u;
    switch (role) {
    case TypeID:
        return static_cast<int>(entry.typeId);
    case DisplayTypeID:
        return static_cast<int>(displayTypeID(entry));
    case CumulativeCount:
        return std::min(n, 255u);
    case TypeCount:
        return n;
    case CountString:
        if (!hasCounts())
            return QString();
        return m_firstDisplayFlag ? container::ammoText(n) : container::countVariants(n);
    case CountAndNameTooltip: {
        if (entry.name.isEmpty())
            return QString();
        if (!hasCounts())
            return entry.name;
        const QString amount =
            n > static_cast<uint>(MaximumShownCount) && m_firstDisplayFlag ? QStringLiteral("65k+") : formatAmount(n);
        return QStringLiteral("%1x %2").arg(amount, entry.name);
    }
    case TypeName:
        return entry.name.isEmpty() ? qtTrId("dummy_unknown") : entry.name;
    case LiquideType:
        return QVariant::fromValue(static_cast<Info::ELiquidType>(0));
    case HookDirection:
        return QVariant::fromValue(static_cast<Info::EHookDirection>(0));
    case LootvalueImageSourceUnderItem:
        return lootValueImageSource(entry, 1);
    case LootvalueImageSourceOverItem:
        return lootValueImageSource(entry, 2);
    case HighlightText:
        return m_secondDisplayFlag && m_itemTrackingStorage && m_itemTrackingStorage->isTracked(cyclopediaTypeID(entry));
    case Classification:
        return entry.classification;
    case UpgradeTier:
        return entry.hasTier ? static_cast<int>(entry.tier) : 0;
    case MarketCategory:
        return entry.marketCategory;
    case DecoItemObjectID:
        return 0u;
    case TradeableInMarket: {
        // The type has market properties.
        const TAppearanceTypePtr type = tibia::game::thingType(entry.typeId, ThingCategoryItem);
        return type != nullptr && type->marketProperties() != nullptr;
    }
    case MarketAverageValue:
        return QVariant::fromValue<qulonglong>(m_itemPriceStorage != nullptr ? m_itemPriceStorage->marketPrice(entry.typeId) : 0);
    case NpcSellValue:
        return m_itemPriceStorage != nullptr ? static_cast<uint>(m_itemPriceStorage->npcBuyPrice(entry.typeId)) : 0u;
    case ProficiencyXP:
        return m_proficiencyStorage != nullptr ? static_cast<uint>(m_proficiencyStorage->xp(entry.typeId)) : 0u;
    case ProficiencyLevel:
        return m_proficiencyStorage != nullptr ? static_cast<uint>(m_proficiencyStorage->decorationLevel(entry.typeId)) : 0u;
    case HasProficiencyMastery:
        return m_proficiencyStorage != nullptr && m_proficiencyStorage->hasMastery(entry.typeId);
    default:
        return {};
    }
}

QHash<int, QByteArray> TAppearanceTypeListModel::roleNames() const
{
    return {
        { TypeID, "typeID" },
        { DisplayTypeID, "displayTypeID" },
        { CumulativeCount, "cumulativeCount" },
        { TypeCount, "typeCount" },
        { CountString, "countString" },
        { CountAndNameTooltip, "countAndNameTooltip" },
        { TypeName, "typeName" },
        { LiquideType, "liquideType" },
        { HookDirection, "hookDirection" },
        { LootvalueImageSourceUnderItem, "lootvalueImageSourceUnderItem" },
        { LootvalueImageSourceOverItem, "lootvalueImageSourceOverItem" },
        { HighlightText, "highlightText" },
        { Classification, "classification" },
        { UpgradeTier, "upgradeTier" },
        { MarketCategory, "marketcategory" },
        { DecoItemObjectID, "decoItemObjectID" },
        { TradeableInMarket, "tradeableInMarket" },
        { MarketAverageValue, "marketAverageValue" },
        { NpcSellValue, "npcSellValue" },
        { ProficiencyXP, "proficiencyXP" },
        { ProficiencyLevel, "proficiencyLevel" },
        { HasProficiencyMastery, "hasProficiencyMastery" },
    };
}

const TAppearanceTypeEntry& TAppearanceTypeListModel::entry(const int row) const
{
    static const TAppearanceTypeEntry Empty;
    if (row < 0 || row >= rowCount())
        return Empty;
    return m_entries[row];
}

namespace {

uint8_t tierKey(const TAppearanceTypeEntry& entry) { return entry.hasTier ? entry.tier : 0; }

}

bool TAppearanceTypeListModel::lessThan(const TAppearanceTypeEntry& left, const TAppearanceTypeEntry& right)
{
    if (left.typeId != right.typeId)
        return left.typeId < right.typeId;
    return tierKey(left) < tierKey(right);
}

bool TAppearanceTypeListModel::sameKey(const TAppearanceTypeEntry& left, const TAppearanceTypeEntry& right)
{
    return left.typeId == right.typeId && tierKey(left) == tierKey(right);
}

void TAppearanceTypeListModel::setItems(std::vector<TAppearanceTypeEntry> entries)
{
    // Kept rows are the same type and tier; a row whose tier presence or tier
    // differs is copied and reported changed.
    if (hasSameItems(entries))
        return;
    mergeSortedItems(m_entries, std::move(entries), lessThan, sameKey, sameItem);
}

bool TAppearanceTypeListModel::sameItem(const TAppearanceTypeEntry& left, const TAppearanceTypeEntry& right)
{
    return left.typeId == right.typeId && left.hasTier == right.hasTier && (!left.hasTier || left.tier == right.tier);
}

bool TAppearanceTypeListModel::hasSameItems(const std::vector<TAppearanceTypeEntry>& entries) const
{
    // CIP: the same items in the same order, or the same items once `entries`
    // is sorted.
    if (m_entries.size() != entries.size())
        return false;
    if (std::equal(m_entries.begin(), m_entries.end(), entries.begin(), sameItem))
        return true;
    std::vector<size_t> order(entries.size());
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&entries](const size_t a, const size_t b) { return lessThan(entries[a], entries[b]); });
    for (size_t i = 0; i < order.size(); ++i) {
        if (!sameItem(m_entries[i], entries[order[i]]))
            return false;
    }
    return true;
}

void TAppearanceTypeListModel::notifyRolesChanged(const QList<int>& roles)
{
    if (rowCount() == 0)
        return;
    Q_EMIT dataChanged(index(0), index(rowCount() - 1), roles);
}

void TAppearanceTypeListModel::onItemPriceVisualizationChanged()
{
    notifyRolesChanged({ LootvalueImageSourceUnderItem, LootvalueImageSourceOverItem });
}

void TAppearanceTypeListModel::onItemTrackingChanged()
{
    notifyRolesChanged({ HighlightText });
}

void TAppearanceTypeListModel::onObjectCountsChanged()
{
    notifyRolesChanged({ CumulativeCount, TypeCount, CountString, CountAndNameTooltip });
}

void TAppearanceTypeListModel::onObjectProficiencyXPChanged()
{
    notifyRolesChanged({ ProficiencyXP, ProficiencyLevel });
}

void TAppearanceTypeListModel::onProficiencyDataChanged(uint16_t) { m_proficiencyTimer.start(); }

void TAppearanceTypeListModel::onProficiencyTimerTimeout() { notifyRolesChanged({ ProficiencyXP, ProficiencyLevel }); }

bool TAppearanceTypeListModelSortFilterProxy::isHandWeaponCategory(const int category)
{
    return category >= 0 && category < 0x1e && ((0x283f0000u >> category) & 1) != 0;
}

bool TAppearanceTypeFilter::operator==(const TAppearanceTypeFilter& other) const
{
    return filterByLevel == other.filterByLevel && filterByVocation == other.filterByVocation
           && onlyOneHandWeapons == other.onlyOneHandWeapons && onlyTwoHandWeapons == other.onlyTwoHandWeapons
           && classificationMode == other.classificationMode && classification == other.classification
           && classificationName == other.classificationName && tierMode == other.tierMode && tier == other.tier
           && tierName == other.tierName && imbuementMode == other.imbuementMode && imbuementValue == other.imbuementValue
           && imbuement == other.imbuement;
}

bool TAppearanceTypeFilter::accepts(const TAppearanceTypeEntry& entry, const std::optional<uint8_t> entryTier, const int level,
                                    const int profession) const
{
    if (level < entry.requiredLevel && filterByLevel)
        return false;
    if (filterByVocation && entry.professions != 0
        && (profession < 1 || profession > 16 || (entry.professions & (1u << (profession - 1))) == 0))
        return false;
    if ((!(entry.hasClothSlot && entry.clothSlot == 6) && onlyOneHandWeapons)
        || (!(entry.hasClothSlot && entry.clothSlot == 0) && onlyTwoHandWeapons))
        return false;
    if (classificationMode != 0) {
        if (entry.classification == 0) {
            if (classificationMode == 3)
                return false;
        } else if (classificationMode == 1 || (classificationMode == 3 && entry.classification != classification)) {
            return false;
        }
    }
    if (tierMode != 0) {
        if (!entryTier) {
            if (tierMode == 3 && tier >= 1)
                return false;
        } else if (tierMode == 3 && tier != *entryTier) {
            return false;
        }
    }
    if (imbuementMode == 3)
        return entry.imbuements.contains(imbuement);
    return true;
}

TAppearanceTypeListModelSortFilterProxy::TAppearanceTypeListModelSortFilterProxy(const bool firstDisplayFlag,
                                                                                 const bool secondDisplayFlag,
                                                                                 const bool useDisplayTypeId,
                                                                                 config::TClientOptions* clientOptions,
                                                                                 QObject* parent)
    : cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy(parent)
    , m_model(firstDisplayFlag, secondDisplayFlag, useDisplayTypeId, clientOptions)
{
    m_typeIdEnumValue = TAppearanceTypeListModel::TypeID;
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
    setSourceModel(&m_model);
    tibia::platform::configureNaturalCollator(m_collator);
    m_filterTimer.setInterval(0);
    m_filterTimer.setSingleShot(true);
    connect(this, &TAppearanceTypeListModelSortFilterProxy::filterChanged, &m_filterTimer, qOverload<>(&QTimer::start));
    connect(&m_filterTimer, &QTimer::timeout, this, [this] {
        beginFilterChange();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
    });
    resetFilters();
}

void TAppearanceTypeListModelSortFilterProxy::connectToStorages(std::shared_ptr<game::TPlayerData> playerData,
                                                                analysers::TItemPriceStorage* itemPriceStorage,
                                                                std::shared_ptr<cyclopedia::TItemTrackingStorage> itemTrackingStorage,
                                                                std::shared_ptr<container::TObjectCountStorage> objectCountStorage,
                                                                proficiency::TProficiencyStorage* proficiencyStorage)
{
    m_playerData = std::move(playerData);
    m_model.connectToStorages(itemPriceStorage, std::move(itemTrackingStorage), std::move(objectCountStorage), proficiencyStorage);
}

void TAppearanceTypeListModelSortFilterProxy::disconnectFromStorages()
{
    m_playerData.reset();
    m_model.disconnectFromStorages();
}

void TAppearanceTypeListModelSortFilterProxy::resetFilters()
{
    m_category = -1;
    m_typeIds.clear();
    m_name = QString();
    m_filter = {};
    m_typeId = 0;
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setCategoryFilter(const int category, const TAppearanceTypeFilter& filter,
                                                                const bool showLockerOnly)
{
    if (m_category == category && m_filter == filter && m_showLockerOnly == showLockerOnly)
        return;
    resetFilters();
    if (m_category != category) {
        m_category = category;
        Q_EMIT filterChanged();
    }
    if (m_filter != filter) {
        m_filter = filter;
        Q_EMIT filterChanged();
    }
    setShowLockerOnly(showLockerOnly);
}

void TAppearanceTypeListModelSortFilterProxy::setCategoryFilter(const int category, const bool filterByLevel, const bool filterByVocation,
                                                                const bool oneHand, const bool twoHand, TAppearanceTypeFilter filter,
                                                                const bool showLockerOnly)
{
    filter.filterByLevel = filterByLevel;
    filter.filterByVocation = filterByVocation;
    filter.onlyOneHandWeapons = oneHand;
    filter.onlyTwoHandWeapons = twoHand;
    setCategoryFilter(category, filter, showLockerOnly);
}

void TAppearanceTypeListModelSortFilterProxy::setNameFilter(const QString& name, const TAppearanceTypeFilter& filter,
                                                            const bool showLockerOnly)
{
    if (m_name != name || m_filter != filter) {
        resetFilters();
        if (m_filter != filter) {
            m_filter = filter;
            Q_EMIT filterChanged();
        }
        if (m_name != name) {
            m_name = name;
            Q_EMIT filterChanged();
        }
    }
    setShowLockerOnly(showLockerOnly);
}

void TAppearanceTypeListModelSortFilterProxy::setNameFilter(const QString& name, const bool filterByLevel, const bool filterByVocation,
                                                            const bool oneHand, const bool twoHand, TAppearanceTypeFilter filter,
                                                            const bool showLockerOnly)
{
    filter.filterByLevel = filterByLevel;
    filter.filterByVocation = filterByVocation;
    filter.onlyOneHandWeapons = oneHand;
    filter.onlyTwoHandWeapons = twoHand;
    setNameFilter(name, filter, showLockerOnly);
}

void TAppearanceTypeListModelSortFilterProxy::setTypeFilter(const uint16_t typeId)
{
    if (m_typeId == typeId)
        return;
    resetFilters();
    if (m_typeId != typeId) {
        m_typeId = typeId;
        Q_EMIT filterChanged();
    }
}

void TAppearanceTypeListModelSortFilterProxy::setCategory(const int category)
{
    if (m_category == category)
        return;
    m_category = category;
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setFilter(const TAppearanceTypeFilter& filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setName(const QString& name)
{
    if (m_name == name)
        return;
    m_name = name;
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setTypeId(const uint16_t typeId)
{
    if (m_typeId == typeId)
        return;
    m_typeId = typeId;
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setTypeIds(const std::set<int>& typeIds)
{
    if (m_typeIds == typeIds)
        return;
    m_typeIds = typeIds;
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setShowLockerOnly(const bool showLockerOnly)
{
    if (m_showLockerOnly == showLockerOnly)
        return;
    m_showLockerOnly = showLockerOnly;
    Q_EMIT showLockerOnlyChanged();
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setAcceptWithoutFilter(const bool accept)
{
    m_acceptWithoutFilter = accept;
    Q_EMIT filterChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setSortMode(const int sortMode)
{
    if (m_sortMode == sortMode)
        return;
    m_sortMode = sortMode;
    invalidate();
}

void TAppearanceTypeListModelSortFilterProxy::resetEntries(std::vector<TAppearanceTypeEntry> entries)
{
    beginFilterChange();
    m_model.reset(std::move(entries));
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void TAppearanceTypeListModelSortFilterProxy::resetTypes(const std::vector<TAppearanceTypeEntry>& types)
{
    beginFilterChange();
    std::vector<TAppearanceTypeEntry> entries;
    for (const TAppearanceTypeEntry& type : types) {
        TAppearanceTypeEntry entry = type;
        entry.hasTier = false;
        entry.tier = 0;
        entries.push_back(std::move(entry));
    }
    m_model.setItems(std::move(entries));
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

void TAppearanceTypeListModelSortFilterProxy::applyFilterNow()
{
    beginFilterChange();
    m_filterTimer.stop();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
}

int TAppearanceTypeListModelSortFilterProxy::rowOf(const uint16_t typeId, const uint8_t tier) const
{
    for (int row = 0; row < rowCount(); ++row) {
        if (typeIdAt(row) == typeId && tierAt(row) == tier)
            return row;
    }
    return -1;
}

int TAppearanceTypeListModelSortFilterProxy::acceptedIndexOf(const uint16_t typeId, const int tier) const
{
    int index = -1;
    const int count = m_model.rowCount();
    for (int sourceRow = 0; sourceRow < count; ++sourceRow) {
        if (!filterAcceptsRow(sourceRow, {}))
            continue;
        const QModelIndex source = m_model.index(sourceRow, 0);
        const auto rowTypeId = static_cast<uint16_t>(m_model.data(source, TAppearanceTypeListModel::TypeID).toInt());
        const int rowTier = m_model.data(source, TAppearanceTypeListModel::UpgradeTier).toInt();
        ++index;
        if (rowTypeId == typeId && rowTier == tier)
            break;
    }
    return index;
}

uint16_t TAppearanceTypeListModelSortFilterProxy::typeIdAt(const int row) const
{
    const QModelIndex source = mapToSource(index(row, 0));
    return source.isValid() ? m_model.entry(source.row()).typeId : 0;
}

uint8_t TAppearanceTypeListModelSortFilterProxy::tierAt(const int row) const
{
    const QModelIndex source = mapToSource(index(row, 0));
    if (!source.isValid())
        return 0;
    const TAppearanceTypeEntry& entry = m_model.entry(source.row());
    return entry.hasTier ? entry.tier : 0;
}

bool TAppearanceTypeListModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (!sourceModel() || !m_playerData)
        return false;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid() || sourceRow >= m_model.size())
        return false;
    if (!m_acceptWithoutFilter && m_name.isEmpty() && m_category == -1 && m_typeIds.empty() && m_typeId == 0)
        return false;
    const TAppearanceTypeEntry& entry = m_model.entry(sourceRow);

    bool accepted = m_name.isEmpty() || entry.name.indexOf(m_name, 0, Qt::CaseInsensitive) != -1;
    if (m_category != -1 && m_category != AllCategories) {
        const int category = entry.marketCategory;
        const bool matches = category == m_category || (category == AllWeaponsCategory && isHandWeaponCategory(m_category))
                             || (m_category == AllWeaponsCategory && isHandWeaponCategory(category));
        accepted = accepted && matches;
    }
    if (!m_typeIds.empty())
        accepted = accepted && m_typeIds.contains(entry.typeId);
    if (m_typeId != 0)
        accepted = accepted && index.data(TAppearanceTypeListModel::DisplayTypeID).toUInt() == m_typeId;

    const std::optional<uint8_t> tier = entry.hasTier ? std::optional<uint8_t>(entry.tier) : std::nullopt;
    accepted = m_filter.accepts(entry, tier, m_playerData->level(), m_playerData->vocation()) && accepted;
    if (m_showLockerOnly)
        accepted = accepted && index.data(TAppearanceTypeListModel::TypeCount).toUInt() != 0;
    return accepted;
}

bool TAppearanceTypeListModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    if (!sourceModel())
        return false;
    static const std::set<int> SortModes = { 1, 2, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
    const int mode = SortModes.contains(m_sortMode) ? m_sortMode : NameAscending;
    using M = TAppearanceTypeListModel;
    const int byName = m_collator.compare(left.data(M::TypeName).toString(), right.data(M::TypeName).toString());

    // Each order compares `descending ? right < left : left < right` and
    // falls back to the name when the values are equal.
    const auto order = [byName](const auto a, const auto b, const bool descending) {
        if (a == b)
            return byName < 0;
        return descending ? b < a : a < b;
    };
    switch (mode) {
    case NameAscending:
        return byName < 0;
    case AverageValueDescending:
    case AverageValueAscending:
        return order(left.data(M::MarketAverageValue).toULongLong(), right.data(M::MarketAverageValue).toULongLong(),
                     mode == AverageValueDescending);
    case TotalAverageValueDescending:
    case TotalAverageValueAscending:
        return order(left.data(M::MarketAverageValue).toULongLong() * left.data(M::TypeCount).toULongLong(),
                     right.data(M::MarketAverageValue).toULongLong() * right.data(M::TypeCount).toULongLong(),
                     mode == TotalAverageValueDescending);
    case NpcValueDescending:
    case NpcValueAscending:
        return order(left.data(M::NpcSellValue).toUInt(), right.data(M::NpcSellValue).toUInt(), mode == NpcValueDescending);
    case TotalNpcValueDescending:
    case TotalNpcValueAscending:
        return order(left.data(M::NpcSellValue).toUInt() * left.data(M::TypeCount).toUInt(),
                     right.data(M::NpcSellValue).toUInt() * right.data(M::TypeCount).toUInt(), mode == TotalNpcValueDescending);
    case CountDescending:
    case CountAscending:
        return order(left.data(M::TypeCount).toUInt(), right.data(M::TypeCount).toUInt(), mode == CountDescending);
    case ProficiencyDescending:
        return order(left.data(M::ProficiencyXP).toUInt(), right.data(M::ProficiencyXP).toUInt(), true);
    default:
        return byName > 0;
    }
}

}
