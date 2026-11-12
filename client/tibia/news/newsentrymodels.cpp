#include "newsentrymodels.h"

namespace tibia {

namespace {

uint categoryIdOf(const news::NewsItem& item)
{
    return item.id > 0 ? uint(item.id) : 0u;
}

/// Source rows for TNewsEntrySortProxyModel (not a CIP moc type).
class NewsEntryItemsModel final : public QAbstractListModel
{
public:
    using Roles = TNewsEntrySortProxyModel::Roles;

    explicit NewsEntryItemsModel(QObject* parent)
        : QAbstractListModel(parent)
    {}

    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : m_items.size();
    }

    QVariant data(const QModelIndex& index, const int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size())
            return {};
        const news::NewsItem& item = m_items.at(index.row());
        switch (role) {
        case Roles::CategoryIdRole:
            return categoryIdOf(item);
        case Roles::HeadlineRole:
            return item.headline;
        case Roles::IsReadRole:
            return item.isRead;
        default:
            return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override
    {
        return {
            { Roles::CategoryIdRole, "categoryId" },
            { Roles::HeadlineRole, "headline" },
            { Roles::IsReadRole, "isRead" },
        };
    }

    void setItems(QVector<news::NewsItem> items)
    {
        beginResetModel();
        m_items = std::move(items);
        endResetModel();
    }

    const news::NewsItem* itemByCategoryId(const uint id) const
    {
        for (const news::NewsItem& item : m_items) {
            if (categoryIdOf(item) == id)
                return &item;
        }
        return nullptr;
    }

private:
    QVector<news::NewsItem> m_items;
};

}

TNewsEntrySortProxyModel::TNewsEntrySortProxyModel(QObject* parent)
    : cip::TNewsEntrySortProxyModel(parent)
    , m_source(new NewsEntryItemsModel(this))
{
    m_categoryIdEnumValue = CategoryIdRole;
    m_headlineEnumValue = HeadlineRole;
    m_isReadEnumValue = IsReadRole;
    setSourceModel(m_source);
    setDynamicSortFilter(true);
    connect(m_source, &QAbstractItemModel::modelReset, this, &TNewsEntrySortProxyModel::refreshCount);
    connect(m_source, &QAbstractItemModel::rowsInserted, this, &TNewsEntrySortProxyModel::refreshCount);
    connect(m_source, &QAbstractItemModel::rowsRemoved, this, &TNewsEntrySortProxyModel::refreshCount);
    refreshCount();
}

int TNewsEntrySortProxyModel::count() const
{
    return m_count;
}

int TNewsEntrySortProxyModel::categoryIdEnumValue() const
{
    return m_categoryIdEnumValue;
}

int TNewsEntrySortProxyModel::headlineEnumValue() const
{
    return m_headlineEnumValue;
}

int TNewsEntrySortProxyModel::isReadEnumValue() const
{
    return m_isReadEnumValue;
}

void TNewsEntrySortProxyModel::setItems(QVector<news::NewsItem> items)
{
    static_cast<NewsEntryItemsModel*>(m_source)->setItems(std::move(items));
    refreshCount();
}

const news::NewsItem* TNewsEntrySortProxyModel::itemByCategoryId(const uint id) const
{
    return static_cast<const NewsEntryItemsModel*>(m_source)->itemByCategoryId(id);
}

void TNewsEntrySortProxyModel::refreshCount()
{
    const int next = rowCount();
    if (m_count == next)
        return;
    m_count = next;
    Q_EMIT countChanged();
}

TNewsEntryModel::TNewsEntryModel(QObject* parent)
    : cip::TNewsEntryModel(parent)
{}

TNewsEntryModel::~TNewsEntryModel()
{
    clearRows();
}

void TNewsEntryModel::clearRows()
{
    for (Row& row : m_rows)
        delete row.sub;
    m_rows.clear();
}

int TNewsEntryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant TNewsEntryModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const Row& row = m_rows.at(index.row());
    switch (role) {
    case CategoryIdRole:
        return categoryIdOf(row.item);
    case HeadlineRole:
        return row.item.headline;
    case IsReadRole:
        return row.item.isRead;
    case SubEntryModelRole:
        return row.sub && row.sub->count() > 0 ? QVariant::fromValue<QObject*>(row.sub) : QVariant();
    default:
        return {};
    }
}

QHash<int, QByteArray> TNewsEntryModel::roleNames() const
{
    return {
        { CategoryIdRole, "categoryId" },
        { HeadlineRole, "headline" },
        { IsReadRole, "isRead" },
        { SubEntryModelRole, "subEntryModel" },
    };
}

void TNewsEntryModel::setRoots(QVector<news::NewsItem> roots, QVector<QVector<news::NewsItem>> children)
{
    beginResetModel();
    clearRows();
    m_rows.reserve(roots.size());
    for (int i = 0; i < roots.size(); ++i) {
        Row row;
        row.item = std::move(roots[i]);
        if (i < children.size() && !children[i].isEmpty()) {
            row.sub = new TNewsEntrySortProxyModel(this);
            row.sub->setItems(std::move(children[i]));
        }
        m_rows.push_back(std::move(row));
    }
    endResetModel();
    Q_EMIT listUpdated();
}

const news::NewsItem* TNewsEntryModel::itemByCategoryId(const uint id) const
{
    for (const Row& row : m_rows) {
        if (categoryIdOf(row.item) == id)
            return &row.item;
        if (row.sub) {
            if (const news::NewsItem* child = row.sub->itemByCategoryId(id))
                return child;
        }
    }
    return nullptr;
}

}
