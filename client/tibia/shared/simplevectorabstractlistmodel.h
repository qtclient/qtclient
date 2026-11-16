#pragma once

#include <QAbstractListModel>
#include <QVariant>

#include <vector>

namespace tibia {
namespace shared {

/// CIP `shared::TSimpleVectorAbstractListModel<T>`: a list model over a vector
/// of items. Every row is one item; the derived model maps an item and a role
/// to its value in `itemData`. `Base` is the QAbstractListModel (or a moc
/// subclass of it) the concrete model derives from.
template<typename T, typename Base = QAbstractListModel>
class TSimpleVectorAbstractListModel : public Base
{
public:
    using Base::Base;

    /// One row per item; the parent is not looked at.
    int rowCount(const QModelIndex& = {}) const override { return static_cast<int>(m_items.size()); }

    QVariant data(const QModelIndex& index, const int role) const override
    {
        if (index.row() >= 0 && index.column() >= 0 && index.model() && index.row() < static_cast<int>(m_items.size()))
            return itemData(m_items[static_cast<size_t>(index.row())], role);
        return defaultData(role);
    }

    /// Takes the new items. Rows past the new count are removed, rows past the
    /// old count inserted; then the rows that were already there before an
    /// insertion (or all remaining rows after a removal) are reported changed.
    virtual void setItems(const std::vector<T>& items)
    {
        int count = rowCount();
        const int newCount = static_cast<int>(items.size());
        if (newCount < count) {
            this->beginRemoveRows({}, newCount, count - 1);
            if (&m_items != &items)
                m_items = items;
            this->endRemoveRows();
            count = newCount;
        } else if (count < newCount) {
            this->beginInsertRows({}, count, newCount - 1);
            if (&m_items != &items)
                m_items = items;
            this->endInsertRows();
        } else if (&m_items != &items) {
            m_items = items;
        }
        if (count > 0)
            Q_EMIT this->dataChanged(this->index(0, 0), this->index(count - 1, 0));
    }

    virtual void clear() { setItems({}); }

    virtual const std::vector<T>& items() const { return m_items; }

protected:
    /// The value of a row that does not exist.
    virtual QVariant defaultData(int) const { return {}; }
    virtual QVariant itemData(const T& item, int role) const = 0;

    std::vector<T> m_items;
};

}
}
