#include "newsentrymodels.h"

#include <tuple>

namespace tibia {

TNewsEntryModel::TNewsEntryModel(QObject* parent)
    : TSortedVectorListModel(parent)
{}

TNewsEntryModel& TNewsEntryModel::instance()
{
    static TNewsEntryModel model;
    return model;
}

QHash<int, QByteArray> TNewsEntryModel::roleNames() const
{
    return {
        { CategoryIdRole, "categoryId" },       { HeadlineRole, "headline" },           { DisplayFromRole, "displayFrom" },
        { UnlockLevelRole, "unlockLevel" },     { IsUnlockedRole, "isUnlocked" },       { IsReadRole, "isRead" },
        { GroupHeaderIDRole, "groupHeaderID" }, { IsGroupHeaderRole, "isGroupHeader" }, { SubEntryModelRole, "subEntryModel" },
    };
}

QVariant TNewsEntryModel::itemData(const TAbstractNewsEntry& item, const int role) const
{
    switch (role) {
    case CategoryIdRole:
        return item.categoryId;
    case HeadlineRole:
        return item.headline;
    case DisplayFromRole:
        return item.displayFrom;
    case UnlockLevelRole:
        return static_cast<uint>(item.unlockLevel);
    case IsUnlockedRole:
        return item.isUnlocked;
    case IsReadRole:
        return item.isRead;
    case GroupHeaderIDRole:
        return item.groupHeaderID;
    case IsGroupHeaderRole:
        return item.isGroupHeader;
    case SubEntryModelRole:
        return QVariant::fromValue<QObject*>(item.subEntryModel.get());
    default:
        return {};
    }
}

void TNewsEntryModel::setEntries(std::vector<TAbstractNewsEntry> entries)
{
    const auto less = [](const TAbstractNewsEntry& a, const TAbstractNewsEntry& b) { return a.categoryId < b.categoryId; };
    const auto sameKey = [](const TAbstractNewsEntry& a, const TAbstractNewsEntry& b) { return a.categoryId == b.categoryId; };
    // The sub entry model is not compared: an unchanged header keeps its own.
    const auto equal = [](const TAbstractNewsEntry& a, const TAbstractNewsEntry& b) {
        return a.categoryId == b.categoryId && a.headline == b.headline && a.displayFrom == b.displayFrom && a.unlockLevel == b.unlockLevel
               && a.isUnlocked == b.isUnlocked && a.isRead == b.isRead && a.groupHeaderID == b.groupHeaderID;
    };
    mergeSortedItems(m_items, std::move(entries), less, sameKey, equal);
}

TNewsEntrySortProxyModel::TNewsEntrySortProxyModel(const uint groupHeaderID, const bool isSubEntryModel, QObject* parent)
    : cip::TNewsEntrySortProxyModel(parent)
    , m_groupHeaderID(groupHeaderID)
    , m_isSubEntryModel(isSubEntryModel)
{
    m_categoryIdEnumValue = TNewsEntryModel::CategoryIdRole;
    m_headlineEnumValue = TNewsEntryModel::HeadlineRole;
    m_isReadEnumValue = TNewsEntryModel::IsReadRole;
    TNewsEntryModel& model = TNewsEntryModel::instance();
    setSourceModel(&model);
    sort(0, Qt::AscendingOrder);
    // A finished update of the entries filters and sorts again.
    connect(&model, &cip::TNewsEntryModel::listUpdated, this, &QSortFilterProxyModel::invalidate);
    connect(&model, &cip::TNewsEntryModel::listUpdated, this, &cip::TNewsEntrySortProxyModel::countChanged);
}

bool TNewsEntrySortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    const QAbstractItemModel* model = sourceModel();
    if (!model)
        return false;
    const QModelIndex index = model->index(sourceRow, 0, sourceParent);
    const uint groupHeaderID = index.data(TNewsEntryModel::GroupHeaderIDRole).toUInt();
    const bool isGroupHeader = index.data(TNewsEntryModel::IsGroupHeaderRole).toBool();
    const bool isUnlocked = index.data(TNewsEntryModel::IsUnlockedRole).toBool();
    const auto* sub = qobject_cast<const TNewsEntrySortProxyModel*>(index.data(TNewsEntryModel::SubEntryModelRole).value<QObject*>());
    bool accepted = false;
    if (m_isSubEntryModel) {
        accepted = groupHeaderID == m_groupHeaderID;
    } else {
        // Top-level entries; a group header only while one of its entries shows.
        accepted = groupHeaderID == 0;
        if (isGroupHeader && sub && sub->isSubEntryModel())
            accepted = accepted && sub->rowCount() > 0;
    }
    return isUnlocked && accepted;
}

bool TNewsEntrySortProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // A group header sorts by its first entry's unlock level.
    const auto unlockLevel = [](const QModelIndex& index) {
        uint level = index.data(TNewsEntryModel::UnlockLevelRole).toUInt();
        if (index.data(TNewsEntryModel::IsGroupHeaderRole).toBool()) {
            if (const auto* sub = qobject_cast<const QAbstractItemModel*>(index.data(TNewsEntryModel::SubEntryModelRole).value<QObject*>()))
                level = sub->rowCount() == 0 ? 0u : sub->index(0, 0).data(TNewsEntryModel::UnlockLevelRole).toUInt();
        }
        return level;
    };
    const QDateTime leftDate = left.data(TNewsEntryModel::DisplayFromRole).toDateTime();
    const QDateTime rightDate = right.data(TNewsEntryModel::DisplayFromRole).toDateTime();
    const uint leftId = left.data(TNewsEntryModel::CategoryIdRole).toUInt();
    const uint rightId = right.data(TNewsEntryModel::CategoryIdRole).toUInt();
    const uint leftLevel = unlockLevel(left);
    const uint rightLevel = unlockLevel(right);
    const QString leftHeadline = left.data(TNewsEntryModel::HeadlineRole).toString();
    const QString rightHeadline = right.data(TNewsEntryModel::HeadlineRole).toString();

    if (leftLevel != rightLevel)
        return leftLevel < rightLevel;
    const QDateTime& first = m_newestFirst ? rightDate : leftDate;
    const QDateTime& second = m_newestFirst ? leftDate : rightDate;
    if (first != second)
        return first < second;
    if (const int byHeadline = QString::compare(leftHeadline, rightHeadline, Qt::CaseSensitive); byHeadline != 0)
        return byHeadline < 0;
    return leftId < rightId;
}

}
