#pragma once

#include <QSortFilterProxyModel>
#include <QVariantMap>

namespace tibia {
namespace shared {

/// CIP `shared::TPaginationProxyModel`: the source rows of one page
/// (1-based, `entriesPerPage` rows each).
class TPaginationProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel* sourceModel READ sourceModel WRITE setSourceModel NOTIFY sourceModelChanged FINAL)
    Q_PROPERTY(int entriesPerPage READ entriesPerPage WRITE setEntriesPerPage NOTIFY paginationDataChanged FINAL)
    Q_PROPERTY(int currentPage READ currentPage WRITE setCurrentPage NOTIFY paginationDataChanged FINAL)
    Q_PROPERTY(int numberOfPages READ numberOfPages NOTIFY paginationDataChanged FINAL)
    Q_PROPERTY(bool hasNextPage READ hasNextPage NOTIFY paginationDataChanged FINAL)
    Q_PROPERTY(bool hasPreviousPage READ hasPreviousPage NOTIFY paginationDataChanged FINAL)

public:
    explicit TPaginationProxyModel(quint8 entriesPerPage);

    void setSourceModel(QAbstractItemModel* sourceModel) override;
    int entriesPerPage() const;
    void setEntriesPerPage(int entriesPerPage);
    int currentPage() const;
    void setCurrentPage(int currentPage);
    /// The pages the source rows fill; at least 1.
    int numberOfPages() const;
    bool hasNextPage() const;
    bool hasPreviousPage() const;
    /// The page holding source row `row`.
    int pageOfRow(int row) const;
    /// Every role of `Index`, keyed by role name.
    Q_INVOKABLE QVariantMap rowData(QModelIndex Index);

Q_SIGNALS:
    void paginationDataChanged();
    void sourceModelChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    /// The page shown and the page size; neither is ever 0.
    struct TPagination
    {
        TPagination(quint16 currentPage, quint8 entriesPerPage);
        /// The pages `rowCount` rows fill (0 for none).
        quint16 numberOfPages(int rowCount) const;
        /// The page of row `row`.
        quint16 pageOfRow(unsigned row) const;

        quint16 currentPage = 1;
        quint8 entriesPerPage = 1;
    };

    void onRowsRemoved();
    void onRowsInserted();
    int sourceRowCount() const;

    TPagination m_pagination;
};

}
}
