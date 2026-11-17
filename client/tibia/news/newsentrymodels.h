#pragma once

#include "shared/simplevectorabstractlistmodel.h"
#include "shared/sortedvectorlistmodel.h"

#include <cip_gen.h>

#include <QDateTime>
#include <QString>

#include <memory>
#include <vector>

namespace tibia {

class TNewsEntrySortProxyModel;

/// CIP (anonymous namespace)::TAbstractNewsEntry: one row of the news entry
/// model. `categoryId` is the entry's id (the dialog calls an entry a category).
struct TAbstractNewsEntry
{
    uint categoryId = 0;
    QString headline;
    QDateTime displayFrom;
    ushort unlockLevel = 0;
    bool isUnlocked = false;
    bool isRead = false;
    /// The group header the entry belongs to; 0 for a top-level entry.
    uint groupHeaderID = 0;
    bool isGroupHeader = false;
    /// A group header's entries, filtered from the same entry model.
    std::shared_ptr<TNewsEntrySortProxyModel> subEntryModel;
};

/// CIP (anonymous namespace)::TNewsEntryModel: every news entry of the dialog's
/// tab, kept in id order. The dialog and the group headers look at it through
/// TNewsEntrySortProxyModel. CIP keeps one model for the client.
class TNewsEntryModel final
    : public shared::TSortedVectorListModel<shared::TSimpleVectorAbstractListModel<TAbstractNewsEntry, cip::TNewsEntryModel>>
{
    Q_OBJECT
public:
    enum Role
    {
        CategoryIdRole = 0x101,
        HeadlineRole,
        DisplayFromRole,
        UnlockLevelRole,
        IsUnlockedRole,
        IsReadRole,
        GroupHeaderIDRole,
        IsGroupHeaderRole,
        SubEntryModelRole,
    };

    static TNewsEntryModel& instance();

    QHash<int, QByteArray> roleNames() const override;
    /// Merges `entries` by id: rows of other ids go, new ids come, a row whose
    /// values differ is replaced and reported changed.
    void setEntries(std::vector<TAbstractNewsEntry> entries);

protected:
    QVariant itemData(const TAbstractNewsEntry& item, int role) const override;

private:
    explicit TNewsEntryModel(QObject* parent = nullptr);
};

/// CIP (anonymous namespace)::TNewsEntrySortProxyModel: the dialog's entry list
/// (top-level entries; a group header only with a visible entry) or a group
/// header's entries. Only unlocked entries show. Rows go by unlock level (a
/// group header by its first entry's), then date, headline and id.
class TNewsEntrySortProxyModel final : public cip::TNewsEntrySortProxyModel
{
    Q_OBJECT
public:
    TNewsEntrySortProxyModel(uint groupHeaderID, bool isSubEntryModel, QObject* parent = nullptr);

    int count() const override { return rowCount(); }
    bool isSubEntryModel() const { return m_isSubEntryModel; }
    /// The newest date first (the dialog's major updates tab).
    void setNewestFirst(bool newestFirst) { m_newestFirst = newestFirst; }

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    uint m_groupHeaderID = 0;
    bool m_isSubEntryModel = false;
    bool m_newestFirst = false;
};

}
