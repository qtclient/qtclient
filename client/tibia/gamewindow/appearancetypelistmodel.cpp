#include "appearancetypelistmodel.h"

#include <tibia/world/game.h>
#include <tibia/world/localplayer.h>

#include <QLocale>

namespace tibia::gamewindow {

namespace {

/// CIP: cumulative counts shown on a slot stop at 65,000.
constexpr int MaximumShownCount = 65000;

}

TAppearanceTypeListModel::TAppearanceTypeListModel(CountProvider counts, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceTypeListModel(parent)
    , m_counts(std::move(counts))
{}

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
    case MarketAverageValue:
    case NpcSellValue:
    case ProficiencyXP:
    case ProficiencyLevel:
        return 0;
    case LootvalueImageSourceUnderItem:
    case LootvalueImageSourceOverItem:
        return QString();
    case HighlightText:
    case HasProficiencyMastery:
        return false;
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

void TAppearanceTypeListModel::reset(std::vector<TAppearanceTypeEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

void TAppearanceTypeListModel::onObjectCountsChanged()
{
    if (m_entries.empty())
        return;
    Q_EMIT dataChanged(index(0), index(static_cast<int>(m_entries.size()) - 1),
                       { CumulativeCount, TypeCount, CountString, CountAndNameTooltip });
}

bool TAppearanceTypeListModelSortFilterProxy::isHandWeaponCategory(const int category)
{
    return category >= 0 && category < 0x1e && ((0x283f0000u >> category) & 1) != 0;
}

TAppearanceTypeListModelSortFilterProxy::TAppearanceTypeListModelSortFilterProxy(TAppearanceTypeListModel* model, QObject* parent)
    : cip::tibia::gamewindow::TAppearanceTypeListModelSortFilterProxy(parent)
    , m_model(model)
{
    m_typeIdEnumValue = TAppearanceTypeListModel::TypeID;
#if !defined(Q_OS_WASM)
    // Wasm's POSIX collator logs and ignores both of these.
    m_collator.setNumericMode(true);
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
#endif
    setSourceModel(model);
    setDynamicSortFilter(false);
    sort(0);
}

void TAppearanceTypeListModelSortFilterProxy::setButtons(const bool filterByLevel, const bool filterByVocation, const bool oneHand,
                                                        const bool twoHand)
{
    m_filterByLevel = filterByLevel;
    m_filterByVocation = filterByVocation;
    m_onlyOneHandWeapons = oneHand;
    m_onlyTwoHandWeapons = twoHand;
}

void TAppearanceTypeListModelSortFilterProxy::setCategoryFilter(const int category, const bool filterByLevel, const bool filterByVocation,
                                                                const bool oneHand, const bool twoHand, const TAppearanceTypeFilter& filter,
                                                                const bool showLockerOnly)
{
    const bool lockerChanged = showLockerOnly != m_showLockerOnly;
    m_category = category;
    m_name.clear();
    m_typeId = 0;
    m_filter = filter;
    m_showLockerOnly = showLockerOnly;
    setButtons(filterByLevel, filterByVocation, oneHand, twoHand);
    invalidate();
    Q_EMIT filterChanged();
    if (lockerChanged)
        Q_EMIT showLockerOnlyChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setNameFilter(const QString& name, const bool filterByLevel, const bool filterByVocation,
                                                            const bool oneHand, const bool twoHand, const TAppearanceTypeFilter& filter,
                                                            const bool showLockerOnly)
{
    const bool lockerChanged = showLockerOnly != m_showLockerOnly;
    m_category = -1;
    m_name = name;
    m_typeId = 0;
    m_filter = filter;
    m_showLockerOnly = showLockerOnly;
    setButtons(filterByLevel, filterByVocation, oneHand, twoHand);
    invalidate();
    Q_EMIT filterChanged();
    if (lockerChanged)
        Q_EMIT showLockerOnlyChanged();
}

void TAppearanceTypeListModelSortFilterProxy::setTypeFilter(const uint16_t typeId)
{
    m_category = -1;
    m_name.clear();
    m_typeId = typeId;
    m_filter = {};
    invalidate();
    Q_EMIT filterChanged();
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

bool TAppearanceTypeListModelSortFilterProxy::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (sourceRow < 0 || sourceRow >= m_model->size())
        return false;
    if (m_name.isEmpty() && m_category == -1 && m_typeId == 0)
        return false;
    const TAppearanceTypeEntry& entry = m_model->entry(sourceRow);

    if (!m_name.isEmpty() && !entry.name.contains(m_name, Qt::CaseInsensitive))
        return false;
    if (m_category != -1 && m_category != AllCategories && entry.marketCategory != m_category) {
        const bool weapons = m_category == AllWeaponsCategory && isHandWeaponCategory(entry.marketCategory);
        if (!weapons)
            return false;
    }
    if (m_typeId != 0 && entry.typeId != m_typeId)
        return false;

    // CIP 0x100cac240.
    const LocalPlayerPtr& player = g_game.getLocalPlayer();
    if (m_filterByLevel && player && player->getLevel() < entry.requiredLevel)
        return false;
    if (m_filterByVocation && entry.professions != 0) {
        const int profession = player ? player->getVocation() % 10 : 0;
        if (profession < 1 || (entry.professions & (1u << (profession - 1))) == 0)
            return false;
    }
    if (m_onlyOneHandWeapons && !(entry.hasClothSlot && entry.clothSlot == 6))
        return false;
    if (m_onlyTwoHandWeapons && !(entry.hasClothSlot && entry.clothSlot == 0))
        return false;
    if (m_filter.classificationMode != 0) {
        if (entry.classification == 0) {
            if (m_filter.classificationMode == 3)
                return false;
        } else if (m_filter.classificationMode == 1
                   || (m_filter.classificationMode == 3 && entry.classification != m_filter.classification)) {
            return false;
        }
    }
    if (m_filter.tierMode == 3) {
        if (!entry.hasTier) {
            if (m_filter.tier >= 1)
                return false;
        } else if (entry.tier != m_filter.tier) {
            return false;
        }
    }
    if (m_showLockerOnly && m_model->data(m_model->index(sourceRow), TAppearanceTypeListModel::TypeCount).toInt() == 0)
        return false;
    return true;
}

bool TAppearanceTypeListModelSortFilterProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const TAppearanceTypeEntry& a = m_model->entry(left.row());
    const TAppearanceTypeEntry& b = m_model->entry(right.row());
    if (const int c = m_collator.compare(a.name, b.name); c != 0)
        return c < 0;
    if (a.typeId != b.typeId)
        return a.typeId < b.typeId;
    return a.tier < b.tier;
}

}
