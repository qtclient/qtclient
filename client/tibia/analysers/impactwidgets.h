#pragma once

#include "analysers/analyserstorages.h"
#include "client/gamecore.h"
#include "gamewindow/gamewindowciptypes.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QPointer>

#include <vector>

namespace tibia {
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {
class TGenericEnterTextDialogController;
class TSidebarWidget;
}
}

namespace tibia {
namespace analysers {

/// The per damage type lists (CIP: the impact model at vtable , data , and
/// the damage input type model at , data): roles 0x101 typeName,
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

/// The damage input source list (CIP: vtable , data): sourceName,
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

/// ImpactWidget.qml (CIP: TImpactWidgetController, ctor , menu).
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
    double dpsGraphMaxY() const override;
    QVariantList dpsGraphData() const override;
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
    double hpsGraphMaxY() const override;
    QVariantList hpsGraphData() const override;
    QString healingAllTimeMaxString() const override;
    QObject* typedDamageModel() const override;
    bool showSessionValues() const override;

    QQuickItem* gameWindowQuickItem() const override;

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

public Q_SLOTS:
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome skipped from cip_gen (shared_ptr / TSidebarID) — qtclient deepen.
    void publishGameAction();
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);

private Q_SLOTS:
    /// CIP private slots — options persist + graph / damage refresh hooks.
    void onWidgetOptionsChanged();
    void onGraphValuesChanged();
    void onDamageValuesChanged();
    void onWidgetQuickItemDestroyed();

private:
    /// CIP : the damage per element of the shown period, over its total.
    void updateTypedModel();
    /// CIP .
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

/// DamageInputWidget.qml (CIP: TDamageInputWidgetController, update , menu
/// , clipboard).
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
    QObject* damageSourcesModel() const override;
    QObject* damageTypesModel() const override;
    QObject* damageTypesFromSourceModel() const override;
    QVariantList damageGraphModel() const override;
    double damageTakenGraphMaxY() const override;
    QString detailedDamageSource() const override;

    QQuickItem* gameWindowQuickItem() const override;

    void onShowContextMenu() override;
    void onDamageSourceDetailsRequested(const QString& DamageSourceName) override;
    Q_INVOKABLE void saveWidgetOptions() override;

public Q_SLOTS:
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome skipped from cip_gen (shared_ptr / TSidebarID) — qtclient deepen.
    void publishGameAction();
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP class NOTIFY — qtclient already emits.
    void damageTakenGraphChanged();
    void damageTakenValuesChanged();
    void detailedDamageSourceChanged();
    void widgetConfigurationChanged();

private Q_SLOTS:
    /// CIP private slot — persist content height when the widget size changes.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    /// CIP .
    void updateLists();
    /// CIP : the selected source's damage per element, or no selection.
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

/// PartyHuntWidget.qml's member list (CIP: TPartyHuntMembersListModel, vtable).
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

/// PartyHuntWidget.qml (CIP: TPartyHuntWidgetController, menu , clipboard).
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
    QVariant partyMemberModel() const override;

    QQuickItem* gameWindowQuickItem() const override;

    void requestConfigurationContextMenu() override;
    void saveWidgetOptions() override;
    void onPartyHuntDataChanged() override;
    void onPartyLeaderIDChanged() override;

public Q_SLOTS:
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome skipped from cip_gen (shared_ptr / TSidebarID) — qtclient deepen.
    void publishGameAction();
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);

private Q_SLOTS:
    /// CIP private slots — options persist + party-flag refresh (CIP typo Chagned).
    void onWidgetOptionsChanged();
    void tryConnectToPlayerPartyStateSignal();
    void onPlayerPartyFlagChagned();
    void onWidgetQuickItemDestroyed();

private:
    QString exportText() const;

    TPartyHuntStorage* m_storage;
    TPartyHuntMembersListModel* m_model;
    gamewindow::TPartyHuntMembersListModelSortFilterProxy* m_proxy;
    QPointer<config::TSidebarOptions> m_options;
    std::vector<client::gamecore::Hook> m_hooks;
    bool m_partyStateConnected = false;
};

}
}
