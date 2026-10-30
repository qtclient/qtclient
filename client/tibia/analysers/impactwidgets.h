#pragma once

#include "analysers/analyserstorages.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QPointer>

#include <vector>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {
class TGenericEnterTextDialogController;
}

namespace tibia::analysers {

/// The per damage type lists (CIP: the impact model at vtable 0x10265d440, data 0x100cd0d30, and
/// the damage input type model at 0x10265da00, data 0x100cdeed0): roles 0x101 typeName,
/// typeImage, value / damageValue, valueString / damageValueString (English grouped),
/// percentageFromTotalString (fraction * 100, one decimal), typeIdentifier (the element).
class TDamageTypedModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Entry
    {
        int element = 0;
        qint64 value = 0;
        double fraction = 0.0;
    };

    TDamageTypedModel(bool damageInputRoles, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<Entry> entries);
    const std::vector<Entry>& entries() const { return m_entries; }

private:
    bool m_damageInputRoles;
    std::vector<Entry> m_entries;
};

/// The damage input source list (CIP: vtable 0x10265dfc0, data 0x100cdfb70): sourceName,
/// damageValue, damageValueString, percentageFromTotalString, typeIdentifier (the name).
class TDamageSourceModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    struct Entry
    {
        QString name;
        qint64 value = 0;
        double fraction = 0.0;
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setEntries(std::vector<Entry> entries);
    const std::vector<Entry>& entries() const { return m_entries; }

private:
    std::vector<Entry> m_entries;
};

/// ImpactWidget.qml (CIP: TImpactWidgetController, ctor 0x100cc92c0, menu 0x100ccb450).
class TImpactWidgetController final : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TImpactWidgetController>
{
    Q_OBJECT
public:
    TImpactWidgetController(TImpactStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TImpactWidgetController() override;

    void setSidebarOptions(config::TSidebarOptions* options);

    QString damageSumString() const override;
    QString maxDpsString() const override;
    double damagePerSecond() const override;
    QString dpsString() const override;
    bool isDpsGaugeVisible() const override;
    double dpsTargetValue() const override;
    QString dpsTargetValueString() const override;
    bool isDpsGraphVisible() const override;
    QString damageAllTimeMaxString() const override;
    bool isDamageTypesVisible() const override;
    QString healingSumString() const override;
    QString maxHpsString() const override;
    double healingPerSecond() const override;
    QString hpsString() const override;
    bool isHpsGaugeVisible() const override;
    double hpsTargetValue() const override;
    QString hpsTargetValueString() const override;
    bool isHpsGraphVisible() const override;
    QString healingAllTimeMaxString() const override;
    QObject* typedDamageModel() const override { return m_typedModel; }
    bool showSessionValues() const override;

    void onShowContextMenu() override;
    void onShowDpsGaugeContextMenu() override;
    void setDesiredIsDpsGaugeVisible(bool Visible) override;
    void onDpsTargetValueSelected(const QString& DpsTargetString) override;
    void showDpsTargetSelectionDialog() override;
    void onShowHpsGaugeContextMenu() override;
    void setDesiredIsHpsGaugeVisible(bool Visible) override;
    void onHpsTargetValueSelected(const QString& HpsTargetString) override;
    void showHpsTargetSelectionDialog() override;
    void onShowDpsGraphContextMenu() override;
    void setDesiredIsDpsGraphVisible(bool Visible) override;
    void onShowHpsGraphContextMenu() override;
    void setDesiredIsHpsGraphVisible(bool Visible) override;
    void saveWidgetOptions() override;

private:
    /// CIP 0x100cc9bc0: the damage per element of the shown period, over its total.
    void updateTypedModel();
    /// CIP 0x100cca8f0.
    void updateGraphs();
    void addGaugeEntries(TImpactStorage::Kind kind);
    void addGraphEntry(TImpactStorage::Kind kind);
    void showTargetDialog(TImpactStorage::Kind kind);
    double perSecond(TImpactStorage::Kind kind) const;

    TImpactStorage* m_storage;
    TDamageTypedModel* m_typedModel;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<gamewindow::TGenericEnterTextDialogController> m_targetDialog;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// DamageInputWidget.qml (CIP: TDamageInputWidgetController, update 0x100cd2700, menu
/// 0x100cd3ad0, clipboard 0x100cd4c60).
class TDamageInputWidgetController final
    : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TDamageInputWidgetController>
{
    Q_OBJECT
public:
    TDamageInputWidgetController(TDamageInputStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

    bool showDamageGraph() const override;
    bool showDamageTypeList() const override;
    bool showDamageSourceList() const override;
    bool showSessionValues() const override;
    QString totalDamageString() const override;
    QString maxDpsTakenString() const override;
    QObject* damageSourcesModel() const override { return m_sources; }
    QObject* damageTypesModel() const override { return m_types; }
    QObject* damageTypesFromSourceModel() const override { return m_fromSource; }
    QVariantList damageGraphModel() const override { return m_graph; }
    double damageTakenGraphMaxY() const override { return m_graphMaxY; }
    QString detailedDamageSource() const override { return m_detailedSource; }

    void onShowContextMenu() override;
    void onDamageSourceDetailsRequested(const QString& DamageSourceName) override;
    Q_INVOKABLE void saveWidgetOptions() override;

private:
    /// CIP 0x100cd2700.
    void updateLists();
    /// CIP 0x100cd46d0: the selected source's damage per element, or no selection.
    void updateDetail();
    void updateGraph();
    QString exportText() const;

    TDamageInputStorage* m_storage;
    TDamageSourceModel* m_sources;
    TDamageTypedModel* m_types;
    TDamageTypedModel* m_fromSource;
    QPointer<config::TSidebarOptions> m_options;
    QVariantList m_graph;
    double m_graphMaxY = 1.0;
    QString m_detailedSource;
};

/// PartyHuntWidget.qml's member list (CIP: TPartyHuntMembersListModel, vtable 0x102661da0).
class TPartyHuntMembersListModel final : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role
    {
        Name = Qt::UserRole + 1,
        CharacterId,
        IsLeader,
        IsInParty,
        Gain,
        Waste,
        Balance,
        Damage,
        Healing,
    };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setMembers(const std::vector<TPartyHuntStorage::Member>& members, quint32 leaderId);

private:
    std::vector<TPartyHuntStorage::Member> m_members;
    quint32 m_leaderId = 0;
};

/// PartyHuntWidget.qml (CIP: TPartyHuntWidgetController, menu 0x100d98480, clipboard 0x100d98e40).
class TPartyHuntWidgetController final : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TPartyHuntWidgetController>
{
    Q_OBJECT
public:
    TPartyHuntWidgetController(TPartyHuntStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

    QString sessionLengthString() const override;
    QString lootTypeString() const override;
    qulonglong totalGain() const override;
    qulonglong totalWaste() const override;
    qlonglong totalBalance() const override;

    void requestConfigurationContextMenu() override;
    void saveWidgetOptions() override;
    void onPartyHuntDataChanged() override;
    void onPartyLeaderIDChanged() override;

private:
    QString exportText() const;

    TPartyHuntStorage* m_storage;
    TPartyHuntMembersListModel* m_model;
    gamewindow::TPartyHuntMembersListModelSortFilterProxy* m_proxy;
    QPointer<config::TSidebarOptions> m_options;
};

}
