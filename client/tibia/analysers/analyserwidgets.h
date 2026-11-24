#pragma once

#include "analysers/analyserstorages.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"
#include "shared/sortedvectorlistmodel.h"

#include <cip_gen.h>

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>

#include <functional>
#include <map>

namespace tibia {
namespace config {
class TSidebarOptions;
}
namespace game {
class TPremiumTimeStorage;
}
}

namespace tibia {
namespace gamewindow {
class TGenericEnterTextDialogController;
class TSidebarWidget;
class TSidebarWidgetsManager;
}
}

namespace tibia {
namespace analysers {

/// The text formats the analyser widgets share.
namespace format {
/// CIP / : grouped, with k / kk / kkk variants (a QML multi string).
QString multi(qint64 value);
/// QLocale::English grouping (CIP QLocale(0x4b)).
QString grouped(qint64 value);
/// CIP : a slot count, "Nk" from 1,000 and "Nkk" from 1,000,000 as shorter variants.
QString slotCount(quint32 count);
/// CIP : "yyyy-MM-dd, HH:mm:ss" (TextHelper.formatDateTime with seconds).
QString dateTime(qint64 secs);
/// CIP : minutes as "time_minutes_short" or "time_hours_and_minutes_short".
QString minutes(qint64 minutes);
/// CIPBIA damage type name / icon (as the character info's combat stats).
QString damageTypeName(int element);
QString damageTypeIcon(int element);
}

/// The analytics selector (CIP: TAnalyticsSelectorWidgetController, ctor): one
/// button per analyser, checked while its widget is open.
class TAnalyticsSelectorWidgetController final
    : public gamewindow::TSidebarWidgetController<cip::tibia::gamewindow::TAnalyticsSelectorWidgetController>
{
    Q_OBJECT
public:
    TAnalyticsSelectorWidgetController(gamewindow::TSidebarWidgetsManager* widgets, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                       QObject* parent = nullptr);

    void setSidebarOptions(config::TSidebarOptions* options);
    void setPremiumTimeStorage(game::TPremiumTimeStorage* premiumTimeStorage);
    bool isPremium() const override;
    bool lootTrackerVisible() const override;
    bool lootAnalyserVisible() const override;
    bool wasteAnalyserVisible() const override;
    bool impactAnalyserVisible() const override;
    bool damageInputAnalyserVisible() const override;
    bool progressAnalyserVisible() const override;
    bool huntingSessionAnalyserVisible() const override;
    bool partyHuntAnalyserVisible() const override;
    bool bossTrackerVisible() const override;

    QQuickItem* gameWindowQuickItem() const override;

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

public Q_SLOTS:
    /// CIP moc slots (not in cip_gen) — sidebar open/close fan-in.
    void onWidgetVisible(int WidgetType);
    void onWidgetHidden(int WidgetType);
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
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP class NOTIFY — qtclient already emits.
    void premiumTimeChanged();
    void widgetVisibilityChanged();

private Q_SLOTS:
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void onWidgetVisibility(int widgetType, bool visible);
    void toggle(int widgetType);

    QPointer<gamewindow::TSidebarWidgetsManager> m_widgets;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<game::TPremiumTimeStorage> m_premiumTimeStorage;
};

/// HuntingSessionWidget.qml (CIP: THuntingSessionWidgetController, ctor , metacall
/// ).
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

    QQuickItem* gameWindowQuickItem() const override;

    void saveWidgetOptions() override;
    void requestConfigurationContextMenu() override;

    /// CIP : the clipboard / text file export.
    QString exportText() const;
    /// CIP .
    QJsonObject exportJson() const;

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
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP class NOTIFY — qtclient already emits / forwards from storage.
    void characterConfigurationChanged();
    void damageOrHealingChanged();
    void experienceChanged();
    void gainOrWasteChanged();
    void killedMonstersChanged();
    void lootedItemsChanged();
    void sessionLengthChanged();

private Q_SLOTS:
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    qint64 perHour(qint64 total) const;
    QString exportFileName(const char* extension) const;

    TAnalyserStorages* m_storages;
    QPointer<config::TSidebarOptions> m_options;
    QTimer m_lootedTimer;
};

/// ProgressWidget.qml (CIP: TProgressWidgetController, ctor , metacall).
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
    QVariantList xpHistory() const override;
    double xpGraphMaxY() const override;
    QString accumulatedXpString() const override;
    QString xpPerHourString() const override;
    QString baseAccumulatedXpString() const override;
    QString baseXpPerHourString() const override;
    QString experiencePointsNeededForNextLevel() const override;
    double playerLevelProgress() const override;
    QString timeToNextLevel() const override;

    QQuickItem* gameWindowQuickItem() const override;

    void onShowContextMenu() override;
    void onShowXPGaugeContextMenu() override;
    void onShowXPGraphContextMenu() override;
    void onXPGaugeTargetValueSelected(const QString& GaugeTargetString) override;
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
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);

private Q_SLOTS:
    void onWidgetOptionsChanged();
    /// CIP : texts at most once a second, the gauge every 50 ms, the graph every 10 s.
    void onExperienceInProgressStorageChanged();
    void appendXPGaugeContextMenuEntries();
    void appendXPGraphContextMenuEntries();
    /// CIP .
    void updateXPGraphData();
    void onWidgetQuickItemDestroyed();

private:
    TProgressStorage* m_storage;
    QPointer<config::TSidebarOptions> m_options;
    QPointer<gamewindow::TGenericEnterTextDialogController> m_targetDialog;
    std::vector<client::gamecore::Hook> m_hooks;
    qint64 m_lastTexts = 0;
    qint64 m_lastGauge = 0;
    qint64 m_lastGraph = 0;
};

/// The loot / waste widgets' item grid, kept in appearance id order.
class TGainWasteModel final : public shared::TSortedVectorListModel<cip::TGainWasteModel>
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

    explicit TGainWasteModel(QObject* parent = nullptr) : TSortedVectorListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    /// Merges the items into the rows by appearance id: runs of removed,
    /// inserted and changed rows are announced together.
    void setItems(std::vector<TGainWasteStorage::Item> items);

private:
    std::vector<TGainWasteStorage::Item> m_items;
};

/// LootAndWasteWidget.qml (CIP: TLootAndWasteWidgetController, ctor , metacall
/// ), the loot (type 0) or supply (type 1) analyser.
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
    QVariantList gainWasteHistory() const override;
    double gainWasteGraphMaxY() const override;
    QString translationPrefix() const override;
    QString imagePrefix() const override;
    bool isGaugeVisible() const override;
    bool isGraphVisible() const override;
    double gaugeTargetValue() const override;
    QString gaugeTargetValueString() const override;
    bool isGaugeMirrored() const override;
    QObject* displayedItems() const override;

    QQuickItem* gameWindowQuickItem() const override;

    void saveWidgetOptions() override;
    void requestConfigurationContextMenu() override;
    void requestGaugeContextMenu() override;
    void requestGraphContextMenu() override;

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
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, tibia::gamewindow::TSidebarWidget* widget);
    /// CIP class NOTIFY — qtclient already emits.
    void characterBasedWidgetOptionsChanged();
    void gainWasteValueChanged();

private Q_SLOTS:
    void onItemsChanged();
    /// CIP .
    void onItemsMetricsChanged();
    void onGaugeTargetValueChanged(const QString& TargetValueString);
    void onWidgetOptionsChanged();
    void onCharacterBasedLootWasteOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
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
/// the sidebar widgets manager.
class TAnalyserWidgets final : public QObject
{
    Q_OBJECT
public:
    TAnalyserWidgets(TAnalyserStorages* storages, gamewindow::TSidebarWidgetsManager* widgets, QQuickItem* gameWindowItem,
                     QQmlEngine* engine, std::function<config::TSidebarOptions*()> options, QObject* parent = nullptr);
    ~TAnalyserWidgets() override;

Q_SIGNALS:
    /// The widgets' publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

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
}
