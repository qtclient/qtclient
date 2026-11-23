#include "appearancetypelistmodel.h"

#include "analysers/itempricestorage.h"
#include "platform/platform.h"
#include "proficiency/proficiencystorage.h"


#include <QLocale>

#include <algorithm>
#include <tibia/game/gamesession.h>

namespace tibia::gamewindow {

namespace {

/// CIP: cumulative counts shown on a slot stop at 65,000.
constexpr int MaximumShownCount = 65000;

}

TAppearanceTypeListModel::TAppearanceTypeListModel(CountProvider counts, QObject* parent)
    : TSortedVectorListModel(parent)
    , m_counts(std::move(counts))
{
    m_proficiencyTimer.setInterval(0);
    m_proficiencyTimer.setSingleShot(true);
    connect(&m_proficiencyTimer, &QTimer::timeout, this, &TAppearanceTypeListModel::onProficiencyTimerTimeout);
}

int TAppearanceTypeListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int TAppearanceTypeListModel::count(const TAppearanceTypeEntry& entry) const
{
    return m_counts ? m_counts(entry.typeId, entry.tier) : 0;
}

QVariant TAppearanceTypeListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const TAppearanceTypeEntry& entry = m_entries[index.row()];
    switch (role) {
    case TypeID:
    case DisplayTypeID:
        return entry.typeId;
    case CumulativeCount:
        return std::min(count(entry), 255);
    case TypeCount:
        return count(entry);
    case CountString: {
        const int n = count(entry);
        if (n < 10000)
            return QString::number(n);
        if (n < MaximumShownCount)
            return QStringLiteral("%1k").arg(n / 1000);
        return QStringLiteral("65k+");
    }
    case CountAndNameTooltip: {
        if (entry.name.isEmpty())
            return QString();
        const int n = count(entry);
        const QString amount = n > MaximumShownCount ? QStringLiteral("65k+") : QLocale(QLocale::English).toString(n);
        return QStringLiteral("%1x %2").arg(amount, entry.name);
    }
    case TypeName:
        return entry.name.isEmpty() ? qtTrId("dummy_unknown") : entry.name;
    case LiquideType:
    case HookDirection:
    case DecoItemObjectID:
        return 0;
    case MarketAverageValue: {
        analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
        return prices ? QVariant::fromValue<quint64>(prices->marketPrice(entry.typeId)) : QVariant::fromValue<quint64>(0);
    }
    case NpcSellValue: {
        const analysers::TItemPriceStorage* prices = analysers::TItemPriceStorage::instance();
        return prices ? static_cast<int>(prices->npcBuyPrice(entry.typeId)) : 0;
    }
    case ProficiencyXP: {
        const proficiency::TProficiencyStorage* proficiencies = proficiency::TProficiencyStorage::instance();
        return proficiencies ? static_cast<int>(proficiencies->xp(entry.typeId)) : 0;
    }
    case ProficiencyLevel: {
        const proficiency::TProficiencyStorage* proficiencies = proficiency::TProficiencyStorage::instance();
        return proficiencies ? proficiencies->decorationLevel(entry.typeId) : 0;
    }
    case LootvalueImageSourceUnderItem:
    case LootvalueImageSourceOverItem:
        return QString();
    case HighlightText:
        return false;
    case HasProficiencyMastery: {
        const proficiency::TProficiencyStorage* proficiencies = proficiency::TProficiencyStorage::instance();
        return proficiencies && proficiencies->hasMastery(entry.typeId);
    }
    case Classification:
        return entry.classification;
    case UpgradeTier:
        return entry.tier;
    case MarketCategory:
        return entry.marketCategory;
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
    const auto equal = [](const TAppearanceTypeEntry& a, const TAppearanceTypeEntry& b) {
        return a.typeId == b.typeId && a.hasTier == b.hasTier && (!a.hasTier || a.tier == b.tier);
    };
    mergeSortedItems(m_entries, std::move(entries), lessThan, sameKey, equal);
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

TAppearanceTypeListModelSortFilterProxy::TAppearanceTypeListModelSortFilterProxy(TAppearanceTypeListModel* model, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy(parent)
    , m_model(model)
{
    m_typeIdEnumValue = TAppearanceTypeListModel::TypeID;
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);
    setSourceModel(model);
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
    m_model->reset(std::move(entries));
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

uint16_t TAppearanceTypeListModelSortFilterProxy::typeIdAt(const int row) const
{
    const QModelIndex source = mapToSource(index(row, 0));
    return source.isValid() ? m_model->entry(source.row()).typeId : 0;
}

uint8_t TAppearanceTypeListModelSortFilterProxy::tierAt(const int row) const
{
    const QModelIndex source = mapToSource(index(row, 0));
    if (!source.isValid())
        return 0;
    const TAppearanceTypeEntry& entry = m_model->entry(source.row());
    return entry.hasTier ? entry.tier : 0;
}

bool TAppearanceTypeListModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    if (!sourceModel() || !tibia::game::hasLocalPlayer())
        return false;
    const QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index.isValid() || sourceRow >= m_model->size())
        return false;
    if (!m_acceptWithoutFilter && m_name.isEmpty() && m_category == -1 && m_typeIds.empty() && m_typeId == 0)
        return false;
    const TAppearanceTypeEntry& entry = m_model->entry(sourceRow);

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
    accepted = m_filter.accepts(entry, tier, tibia::game::localPlayerLevel(), tibia::game::localPlayerVocation() % 10) && accepted;
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
