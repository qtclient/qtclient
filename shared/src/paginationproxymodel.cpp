#include "paginationproxymodel.h"

namespace tibia::shared {

TPaginationProxyModel::TPagination::TPagination(const quint16 currentPage, const quint8 entriesPerPage)
    : currentPage(currentPage + (currentPage == 0))
    , entriesPerPage(entriesPerPage + (entriesPerPage == 0))
{
}

quint16 TPaginationProxyModel::TPagination::numberOfPages(const int rowCount) const
{
    if (rowCount == 0 || entriesPerPage == 0)
        return 0;
    return quint16((unsigned(rowCount) - 1) / entriesPerPage + 1);
}

quint16 TPaginationProxyModel::TPagination::pageOfRow(const unsigned row) const
{
    if (entriesPerPage == 0)
        return 0;
    return quint16(row / entriesPerPage + 1);
}

TPaginationProxyModel::TPaginationProxyModel(const quint8 entriesPerPage)
    : QSortFilterProxyModel(nullptr)
    , m_pagination(1, entriesPerPage)
{
}

int TPaginationProxyModel::sourceRowCount() const
{
    return sourceModel()->rowCount(QModelIndex());
}

void TPaginationProxyModel::setSourceModel(QAbstractItemModel* model)
{
    if (sourceModel()) {
        disconnect(sourceModel(), &QAbstractItemModel::rowsRemoved, this, &TPaginationProxyModel::onRowsRemoved);
        disconnect(sourceModel(), &QAbstractItemModel::rowsInserted, this, &TPaginationProxyModel::onRowsInserted);
    }
    QSortFilterProxyModel::setSourceModel(model);
    connect(model, &QAbstractItemModel::rowsRemoved, this, &TPaginationProxyModel::onRowsRemoved);
    connect(model, &QAbstractItemModel::rowsInserted, this, &TPaginationProxyModel::onRowsInserted);
    Q_EMIT sourceModelChanged();
}

void TPaginationProxyModel::onRowsRemoved()
{
    beginFilterChange();
    if (sourceRowCount() == 0 && m_pagination.currentPage != 1) {
        beginFilterChange();
        m_pagination = TPagination(1, m_pagination.entriesPerPage);
        endFilterChange(Direction::Both);
        Q_EMIT paginationDataChanged();
    } else if (sourceRowCount() > 0 && m_pagination.currentPage >= m_pagination.pageOfRow(unsigned(sourceRowCount() - 1))) {
        const quint16 lastPage = m_pagination.pageOfRow(unsigned(sourceRowCount() - 1));
        if (m_pagination.currentPage != lastPage) {
            beginFilterChange();
            m_pagination = TPagination(lastPage, m_pagination.entriesPerPage);
            endFilterChange(Direction::Both);
            Q_EMIT paginationDataChanged();
        }
    }
    endFilterChange(Direction::Both);
    Q_EMIT paginationDataChanged();
}

void TPaginationProxyModel::onRowsInserted()
{
    beginFilterChange();
    endFilterChange(Direction::Both);
    Q_EMIT paginationDataChanged();
}

int TPaginationProxyModel::entriesPerPage() const
{
    return m_pagination.entriesPerPage;
}

void TPaginationProxyModel::setEntriesPerPage(const int entriesPerPage)
{
    if (m_pagination.entriesPerPage == quint8(entriesPerPage))
        return;
    beginFilterChange();
    m_pagination = TPagination(m_pagination.currentPage, quint8(entriesPerPage));
    endFilterChange(Direction::Both);
    Q_EMIT paginationDataChanged();
}

int TPaginationProxyModel::currentPage() const
{
    return m_pagination.currentPage;
}

void TPaginationProxyModel::setCurrentPage(const int currentPage)
{
    if (m_pagination.currentPage == quint16(currentPage))
        return;
    beginFilterChange();
    m_pagination = TPagination(quint16(currentPage), m_pagination.entriesPerPage);
    endFilterChange(Direction::Both);
    Q_EMIT paginationDataChanged();
}

int TPaginationProxyModel::numberOfPages() const
{
    const quint16 pages = m_pagination.numberOfPages(sourceRowCount());
    return pages > 1 ? pages : 1;
}

bool TPaginationProxyModel::hasNextPage() const
{
    return m_pagination.currentPage < numberOfPages();
}

bool TPaginationProxyModel::hasPreviousPage() const
{
    return m_pagination.currentPage > 1;
}

int TPaginationProxyModel::pageOfRow(const int row) const
{
    return m_pagination.pageOfRow(unsigned(row));
}

bool TPaginationProxyModel::filterAcceptsRow(const int sourceRow, const QModelIndex& sourceParent) const
{
    Q_UNUSED(sourceParent);
    return m_pagination.currentPage == m_pagination.pageOfRow(unsigned(sourceRow));
}

QVariantMap TPaginationProxyModel::rowData(QModelIndex Index)
{
    QVariantMap row;
    if (Index.row() < 0 || Index.column() < 0 || !Index.model())
        return row;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        row[QString::fromUtf8(it.value())] = data(Index, it.key());
    return row;
}

}
