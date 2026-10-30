#pragma once

#include "analysers/analyserstorages.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <map>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {
class TGenericEnterTextDialogController;
class TSidebarWidget;
class TSidebarWidgetsManager;
}

namespace tibia::analysers {

/// The text formats the analyser widgets share.
namespace format {
/// CIP 0x100ce8a20 / 0x100bfa5d0: grouped, with k / kk / kkk variants (a QML multi string).
QString multi(qint64 value);
/// QLocale::English grouping (CIP QLocale(0x4b)).
QString grouped(qint64 value);
/// CIP FUN_100a3c5e0: a slot count, "Nk" from 1,000 and "Nkk" from 1,000,000 as shorter variants.
QString slotCount(quint32 count);
/// CIP 0x10095c790: "yyyy-MM-dd, HH:mm:ss" (TextHelper.formatDateTime with seconds).
QString dateTime(qint64 secs);
/// CIP 0x10095e350: minutes as "time_minutes_short" or "time_hours_and_minutes_short".
QString minutes(qint64 minutes);
/// CIPBIA damage type name / icon (as the character info's combat stats).
QString damageTypeName(int element);
QString damageTypeIcon(int element);
}

/// The analytics selector (CIP: TAnalyticsSelectorWidgetController, ctor 0x100ce0b10): one
/// button per analyser, checked while its widget is open.
class TAnalyticsSelectorWidgetController final
    : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TAnalyticsSelectorWidgetController>
{
    Q_OBJECT
public:
    TAnalyticsSelectorWidgetController(gamewindow::TSidebarWidgetsManager* widgets, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                       QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    bool isPremium() const override;

    void saveWidgetOptions() override;
    void lootTrackerButtonClicked() override;
    void lootAnalyserButtonClicked() override;
    void wasteAnalyserButtonClicked() override;
    void impactAnalyserButtonClicked() override;
    void damageInputAnalyserButtonClicked() override;
    void huntingSessionAnalyserButtonClicked() override;
    void progressAnalyserButtonClicked() override;
    void partyHuntAnalyserButtonClicked() override;
    void bossTrackerButtonClicked() override;

private:
    void onWidgetVisibility(int widgetType, bool visible);
    void toggle(int widgetType);

    QPointer<gamewindow::TSidebarWidgetsManager> m_widgets;
    QPointer<config::TSidebarOptions> m_options;
    std::vector<client::gamecore::Hook> m_hooks;
};

/// HuntingSessionWidget.qml (CIP: THuntingSessionWidgetController, ctor 0x100ce22a0, metacall
/// 0x100825500).
class THuntingSessionWidgetController final
    : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::THuntingSessionWidgetController>
{
    Q_OBJECT
public:
    THuntingSessionWidgetController(TAnalyserStorages* storages, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);

    bool showBaseXp() const override;
    QString sessionLengthString() const override;
    QString accumulatedXpString() const override;
    QString xpPerHourString() const override;
    QString baseAccumulatedXpString() const override;
    QString baseXpPerHourString() const override;
    QString gainString() const override;
    QString wasteString() const override;
    qlonglong gainWasteSum() const override;
    QString gainWasteSumString() const override;
    QString damageString() const override;
    QString damagePerHourString() const override;
    QString healingString() const override;
    QString healingPerHourString() const override;
    QStringList lootedItems() const override;
    QStringList killedMonsters() const override;

    void saveWidgetOptions() override;
    void requestConfigurationContextMenu() override;

    /// CIP 0x100ce5710: the clipboard / text file export.
    QString exportText() const;
    /// CIP 0x100ce7790.
    QJsonObject exportJson() const;

private:
    qint64 perHour(qint64 total) const;
    QString exportFileName(const char* extension) const;

    TAnalyserStorages* m_storages;
    QPointer<config::TSidebarOptions> m_options;
    QTimer m_lootedTimer;
};

/// ProgressWidget.qml (CIP: TProgressWidgetController, ctor 0x100cec090, metacall 0x100839b60).
class TProgressWidgetController final : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TProgressWidgetController>
{
    Q_OBJECT
public:
    TProgressWidgetController(TProgressStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine, QObject* parent = nullptr);
    ~TProgressWidgetController() override;

    void setSidebarOptions(config::TSidebarOptions* options);

    bool isGaugeVisible() const override;
    bool isGraphVisible() const override;
    double xpGaugeTargetValue() const override;
    QString xpGaugeTargetValueString() const override;
    bool showBaseXp() const override;
    double xpGaugeValue() const override;
    QString accumulatedXpString() const override;
    QString xpPerHourString() const override;
    QString baseAccumulatedXpString() const override;
    QString baseXpPerHourString() const override;
    QString experiencePointsNeededForNextLevel() const override;
    double playerLevelProgress() const override;
    QString timeToNextLevel() const override;

    void onShowContextMenu() override;
    void onShowXPGaugeContextMenu() override;
    void onShowXPGraphContextMenu() override;
    void onXPGaugeTargetValueSelected(const QString& GaugeTargetString) override;
    void saveWidgetOptions() override;

private:
    /// CIP 0x100ced030: texts at most once a second, the gauge every 50 ms, the graph every 10 s.
    void onDataChanged();
    /// CIP 0x100ced100.
    void updateGraph();
    void addGaugeEntries();
    void addGraphEntry();

    TProgressStorage* m_storage;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<gamewindow::TGenericEnterTextDialogController> m_targetDialog;
    std::vector<client::gamecore::Hook> m_hooks;
    qint64 m_lastTexts = 0;
    qint64 m_lastGauge = 0;
    qint64 m_lastGraph = 0;
};

/// The loot / waste widgets' item grid (CIP: the model at vtable 0x10265ce40; data 0x100cc3c90).
class TGainWasteModel final : public cip::TGainWasteModel
{
    Q_OBJECT
public:
    enum Role
    {
        ItemName = Qt::UserRole + 1,
        ItemValueString,
        ItemValueForCountString,
        ItemCountString,
        AppearanceId,
        ItemCount,
        ItemLiquidType,
        ItemHookDirection,
    };

    using cip::TGainWasteModel::TGainWasteModel;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    /// CIP 0x100cc3100: inserts, removes and changes rows in place.
    void setItems(std::vector<TGainWasteStorage::Item> items);

private:
    std::vector<TGainWasteStorage::Item> m_items;
};

/// LootAndWasteWidget.qml (CIP: TLootAndWasteWidgetController, ctor 0x100cbfaf0, metacall
/// 0x10082a630), the loot (type 0) or supply (type 1) analyser.
class TLootAndWasteWidgetController final
    : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TLootAndWasteWidgetController>
{
    Q_OBJECT
public:
    TLootAndWasteWidgetController(TGainWasteStorage* storage, TGainWasteStorage::Side side, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                  QObject* parent = nullptr);
    ~TLootAndWasteWidgetController() override;

    void setSidebarOptions(config::TSidebarOptions* options);

    QString gainWasteValueString() const override;
    QString gainWastePerHourString() const override;
    double gainWastePerHour() const override;
    QVariantList gainWasteHistory() const override { return m_history; }
    double gainWasteGraphMaxY() const override { return m_maxY; }
    QString translationPrefix() const override;
    QString imagePrefix() const override;
    bool isGaugeVisible() const override;
    bool isGraphVisible() const override;
    double gaugeTargetValue() const override;
    QString gaugeTargetValueString() const override;
    bool isGaugeMirrored() const override { return m_side == TGainWasteStorage::Waste; }
    QObject* displayedItems() const override { return m_model; }

    void saveWidgetOptions() override;
    void requestConfigurationContextMenu() override;
    void requestGaugeContextMenu() override;
    void requestGraphContextMenu() override;

private:
    /// CIP 0x100cc0de0.
    void onMetricsChanged();
    void addGaugeEntries();
    void addGraphEntry();
    void showTargetDialog();

    TGainWasteStorage* m_storage;
    TGainWasteStorage::Side m_side;
    TGainWasteModel* m_model;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<gamewindow::TGenericEnterTextDialogController> m_targetDialog;
    QVariantList m_history;
    double m_maxY = 1.0;
    QTimer m_itemsTimer;
};

/// Owns the analyser widgets while they are open and registers them (EWidgetType 15 .. 22) with
/// the sidebar widgets manager; also handles RESET_HUNTING_SESSION, SAVE_TEXT_TO_FILE and
/// SAVE_JSON_TO_FILE.
class TAnalyserWidgets final : public QObject
{
    Q_OBJECT
public:
    TAnalyserWidgets(TAnalyserStorages* storages, gamewindow::TSidebarWidgetsManager* widgets, QQuickItem* gameWindowItem,
                     QQmlEngine* engine, std::function<config::TSidebarOptions*()> options, QObject* parent = nullptr);
    ~TAnalyserWidgets() override;

private:
    using Factory = std::function<QObject*()>;
    struct Slot
    {
        Factory make;
        std::function<gamewindow::TSidebarWidget*(QObject*)> widgetOf;
        QPointer<QObject> controller;
    };

    template <class Controller>
    void add(int widgetType, std::function<Controller*()> make);

    TAnalyserStorages* m_storages;
    QPointer<gamewindow::TSidebarWidgetsManager> m_widgets;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    std::function<config::TSidebarOptions*()> m_options;
    std::map<int, Slot> m_slots;
};

}
