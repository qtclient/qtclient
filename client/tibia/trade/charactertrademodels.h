#pragma once

#include "trade/tradeciptypes.h"

#include <cip_gen.h>

#include <vector>

namespace tibia {

/// CharacterTradeNotifications.qml rows (CIP: TCharacterTradeNotificationsAbstractListModel).
class TCharacterTradeNotificationsAbstractListModel final
    : public cip::TCharacterTradeNotificationsAbstractListModel
{
    Q_OBJECT
public:
    enum Role { NotificationRole = Qt::UserRole + 1, StatusRole };

    explicit TCharacterTradeNotificationsAbstractListModel(QObject* parent = nullptr);

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(std::vector<trade::TCharacterTradeConditionEntry> entries);

private:
    std::vector<trade::TCharacterTradeConditionEntry> m_entries;
};

/// CharacterTradeHighlightPicker source rows (CIP: TTextualHighlightsAbstractListModel).
class TTextualHighlightsAbstractListModel final : public cip::TTextualHighlightsAbstractListModel
{
    Q_OBJECT
public:
    enum Role { CategoryIconRole = Qt::UserRole + 1, HighlightRole, HighlightIdRole, CategoryRole };

    explicit TTextualHighlightsAbstractListModel(QObject* parent = nullptr);

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(std::vector<trade::TCharacterTradeHighlightEntry> entries);
    const std::vector<trade::TCharacterTradeHighlightEntry>& entries() const { return m_entries; }

private:
    std::vector<trade::TCharacterTradeHighlightEntry> m_entries;
};

/// Filtered highlightsModel for CharacterTradeDialog (CIP: TTextualHighlightsSortProxyModel).
class TTextualHighlightsSortProxyModel final : public cip::TTextualHighlightsSortProxyModel
{
    Q_OBJECT
public:
    explicit TTextualHighlightsSortProxyModel(QObject* parent = nullptr);

    TTextualHighlightsAbstractListModel* source() const { return m_source; }

    QString filterString() const override;
    void setFilterString(const QString& v) override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TTextualHighlightsAbstractListModel* m_source;
};

/// CharacterTradeItemPicker source rows (CIP: TItemHighlightsAbstractListModel).
class TItemHighlightsAbstractListModel final : public cip::TItemHighlightsAbstractListModel
{
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1, NameRole, AmountRole, DescriptionRole, UpgradeTierRole };

    explicit TItemHighlightsAbstractListModel(QObject* parent = nullptr);

    int count() const override;
    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(std::vector<trade::TCharacterTradeItemEntry> entries);
    const std::vector<trade::TCharacterTradeItemEntry>& entries() const { return m_entries; }

private:
    std::vector<trade::TCharacterTradeItemEntry> m_entries;
};

/// Filtered itemsModel for CharacterTradeDialog (CIP: TItemHighlightsSortProxyModel).
class TItemHighlightsSortProxyModel final : public cip::TItemHighlightsSortProxyModel
{
    Q_OBJECT
public:
    explicit TItemHighlightsSortProxyModel(QObject* parent = nullptr);

    TItemHighlightsAbstractListModel* source() const { return m_source; }

    QString filterString() const override;
    void setFilterString(const QString& v) override;

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    TItemHighlightsAbstractListModel* m_source;
};

}
