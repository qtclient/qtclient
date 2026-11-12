#pragma once

#include "news/newsstorage.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QVector>

namespace tibia {

/// Flat child rows under a GROUP HEADER (CIP: TNewsEntrySortProxyModel — NewsDialog
/// `subEntryModel` count / role enum values).
class TNewsEntrySortProxyModel final : public cip::TNewsEntrySortProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int categoryIdEnumValue READ categoryIdEnumValue CONSTANT)
    Q_PROPERTY(int headlineEnumValue READ headlineEnumValue CONSTANT)
    Q_PROPERTY(int isReadEnumValue READ isReadEnumValue CONSTANT)
public:
    enum Roles { CategoryIdRole = Qt::UserRole + 1, HeadlineRole, IsReadRole };

    explicit TNewsEntrySortProxyModel(QObject* parent = nullptr);

    int count() const override;
    int categoryIdEnumValue() const override;
    int headlineEnumValue() const override;
    int isReadEnumValue() const override;

    void setItems(QVector<news::NewsItem> items);
    const news::NewsItem* itemByCategoryId(uint id) const;

Q_SIGNALS:
    void countChanged();

private:
    void refreshCount();

    QAbstractListModel* m_source = nullptr;
};

/// Tab-scoped entry list for NewsDialog.qml (CIP: TNewsEntryModel — categoryId /
/// headline / isRead / subEntryModel roles + listUpdated).
class TNewsEntryModel final : public cip::TNewsEntryModel
{
    Q_OBJECT
public:
    enum Roles { CategoryIdRole = Qt::UserRole + 1, HeadlineRole, IsReadRole, SubEntryModelRole };

    explicit TNewsEntryModel(QObject* parent = nullptr);
    ~TNewsEntryModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRoots(QVector<news::NewsItem> roots, QVector<QVector<news::NewsItem>> children);
    const news::NewsItem* itemByCategoryId(uint id) const;

Q_SIGNALS:
    /// CIP signal — qtclient owns emit (cip:: base also declares).
    void listUpdated();

private:
    struct Row {
        news::NewsItem item;
        TNewsEntrySortProxyModel* sub = nullptr;
    };
    void clearRows();

    QVector<Row> m_rows;
};

}
