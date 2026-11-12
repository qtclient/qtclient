#include "trade/charactertrademodels.h"

namespace tibia {

TCharacterTradeNotificationsAbstractListModel::TCharacterTradeNotificationsAbstractListModel(QObject* parent)
    : cip::TCharacterTradeNotificationsAbstractListModel(parent)
{}

int TCharacterTradeNotificationsAbstractListModel::count() const
{
    return m_count;
}

int TCharacterTradeNotificationsAbstractListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TCharacterTradeNotificationsAbstractListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const auto& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case NotificationRole: return e.notification;
        case StatusRole: return e.status;
        default: return {};
    }
}

QHash<int, QByteArray> TCharacterTradeNotificationsAbstractListModel::roleNames() const
{
    return { { NotificationRole, "notification" }, { StatusRole, "status" } };
}

void TCharacterTradeNotificationsAbstractListModel::setEntries(
    std::vector<trade::TCharacterTradeConditionEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    m_count = static_cast<int>(m_entries.size());
    endResetModel();
    Q_EMIT rowCountChanged();
}

TTextualHighlightsAbstractListModel::TTextualHighlightsAbstractListModel(QObject* parent)
    : cip::TTextualHighlightsAbstractListModel(parent)
{}

int TTextualHighlightsAbstractListModel::count() const
{
    return m_count;
}

int TTextualHighlightsAbstractListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TTextualHighlightsAbstractListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const auto& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case CategoryIconRole: return e.categoryIcon;
        case HighlightRole: return e.highlight;
        case HighlightIdRole: return e.highlightId;
        case CategoryRole: return e.category;
        default: return {};
    }
}

QHash<int, QByteArray> TTextualHighlightsAbstractListModel::roleNames() const
{
    return { { CategoryIconRole, "categoryicon" },
             { HighlightRole, "highlight" },
             { HighlightIdRole, "highlightid" },
             { CategoryRole, "category" } };
}

void TTextualHighlightsAbstractListModel::setEntries(std::vector<trade::TCharacterTradeHighlightEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    m_count = static_cast<int>(m_entries.size());
    endResetModel();
    Q_EMIT rowCountChanged();
}

TTextualHighlightsSortProxyModel::TTextualHighlightsSortProxyModel(QObject* parent)
    : cip::TTextualHighlightsSortProxyModel(parent)
    , m_source(new TTextualHighlightsAbstractListModel(this))
{
    setSourceModel(m_source);
}

QString TTextualHighlightsSortProxyModel::filterString() const
{
    return m_filterString;
}

void TTextualHighlightsSortProxyModel::setFilterString(const QString& v)
{
    if (::cip::unchanged(m_filterString, v))
        return;
    beginFilterChange();
    m_filterString = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterStringChanged();
}

bool TTextualHighlightsSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (m_filterString.isEmpty())
        return true;
    const QString highlight =
        m_source->data(m_source->index(sourceRow), TTextualHighlightsAbstractListModel::HighlightRole).toString();
    return highlight.contains(m_filterString, Qt::CaseInsensitive);
}

TItemHighlightsAbstractListModel::TItemHighlightsAbstractListModel(QObject* parent)
    : cip::TItemHighlightsAbstractListModel(parent)
{}

int TItemHighlightsAbstractListModel::count() const
{
    return m_count;
}

int TItemHighlightsAbstractListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant TItemHighlightsAbstractListModel::data(const QModelIndex& index, const int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_entries.size()))
        return {};
    const auto& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole: return e.id;
        case NameRole: return e.name;
        case AmountRole: return e.amount;
        case DescriptionRole: return e.description;
        case UpgradeTierRole: return e.upgradeTier;
        default: return {};
    }
}

QHash<int, QByteArray> TItemHighlightsAbstractListModel::roleNames() const
{
    return { { IdRole, "id" },
             { NameRole, "name" },
             { AmountRole, "amount" },
             { DescriptionRole, "description" },
             { UpgradeTierRole, "upgradetier" } };
}

void TItemHighlightsAbstractListModel::setEntries(std::vector<trade::TCharacterTradeItemEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    m_count = static_cast<int>(m_entries.size());
    endResetModel();
    Q_EMIT rowCountChanged();
}

TItemHighlightsSortProxyModel::TItemHighlightsSortProxyModel(QObject* parent)
    : cip::TItemHighlightsSortProxyModel(parent)
    , m_source(new TItemHighlightsAbstractListModel(this))
{
    setSourceModel(m_source);
}

QString TItemHighlightsSortProxyModel::filterString() const
{
    return m_filterString;
}

void TItemHighlightsSortProxyModel::setFilterString(const QString& v)
{
    if (::cip::unchanged(m_filterString, v))
        return;
    beginFilterChange();
    m_filterString = v;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    Q_EMIT filterStringChanged();
}

bool TItemHighlightsSortProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex&) const
{
    if (m_filterString.isEmpty())
        return true;
    const QModelIndex idx = m_source->index(sourceRow);
    const QString name = m_source->data(idx, TItemHighlightsAbstractListModel::NameRole).toString();
    if (name.contains(m_filterString, Qt::CaseInsensitive))
        return true;
    const QString description =
        m_source->data(idx, TItemHighlightsAbstractListModel::DescriptionRole).toString();
    return description.contains(m_filterString, Qt::CaseInsensitive);
}

}
