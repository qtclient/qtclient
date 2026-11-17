#pragma once

#include "cyclopedia/itemtrackingstorage.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

namespace tibia {
namespace analysers {
class TAnalyserStorages;
}
namespace config {
class TSidebarOptions;
}
}

namespace tibia {
namespace gamewindow {

class TGenericEnterTextDialogController;

/// One recent drop under a tracked item (CIP: TQmlDroppedLoot, ctor).
class TQmlDroppedLoot final : public cip::tibia::gamewindow::TQmlDroppedLoot
{
    Q_OBJECT
public:
    TQmlDroppedLoot(const cyclopedia::TDroppedLoot& drop, QObject* parent);

    /// The monster's outfit.
    uint creatureOutfitId() const override;
    QColor creatureHeadColor() const override;
    QColor creatureTorsoColor() const override;
    QColor creatureLegsColor() const override;
    QColor creatureDetailColor() const override;
    bool creatureFirstAddOn() const override;
    bool creatureSecondAddOn() const override;

private:
    Outfit m_outfit;
};

/// One tracked item (CIP: TQmlTrackedItem, built in).
class TQmlTrackedItem final : public cip::tibia::gamewindow::TQmlTrackedItem
{
    Q_OBJECT
public:
    TQmlTrackedItem(const cyclopedia::TTrackedItem& item, QObject* parent);

    QString itemValueString() const override;
    QString dropCountString() const override;
    void setItemValue(qint64 value);
    /// CIP : the new drop count.
    void setDropCount(quint32 count);
    /// CIP : the item's recent drops.
    void setLootDrops(const std::vector<cyclopedia::TDroppedLoot>& drops);

private:
    qint64 m_itemValue = 0;
};

/// The Drop Tracker sidebar widget, LootTrackingWidget.qml (CIP:
/// TLootTrackingWidgetController, ctor , connections).
class TLootTrackingWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TLootTrackingWidgetController>
{
    Q_OBJECT

public:
    TLootTrackingWidgetController(analysers::TAnalyserStorages* storages, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                  QObject* parent = nullptr);
    ~TLootTrackingWidgetController() override;

    /// lootTrackerWidgetOptions: contentHeight, contentMaximized (v37 / v38).
    void setSidebarOptions(config::TSidebarOptions* options);

    QList<QObject*> trackedItems() const override;

    QQuickItem* gameWindowQuickItem() const override;

public Q_SLOTS:
    void saveWidgetOptions() override;
    void onCloseClicked() override;
    void onRequestClose() override;
    void onRequestMoveToSidebar(int TargetSidebar) override;
    void onSetDragActive(bool Active) override;
    void onSidebarDestroyed() override;
    void onLevelUpClicked() override;
    void setCursorToResizeVertical(bool Enabled) override;
    void startContentSizeChangedTimer();

public:
    /// CIP : OPEN_CYCLOPEDIA on the Items tab.
    Q_INVOKABLE void requestOpenItemInfoDialog() override;
    /// CIP : remove this item / remove all (the argument is the type id).
    Q_INVOKABLE void requestContextMenu(int TrackedItemIndex) override;
    /// CIP : the auto-track value prompt.
    Q_INVOKABLE void requestConfigurationMenu() override;

Q_SIGNALS:
    /// CIP sidebar chrome (TSidebarWidgetControllerBase) — qtclient NOTIFY deepen.
    void contentSizeChanged();
    void positionInSidebarChanged();
    void sidebarIdChanged();
    void widgetClosed();
    /// CIP chrome (shared_ptr / TSidebarID skipped) — qtclient NOTIFY deepen; drag/move
    /// forwarded via TSidebarWidget dynamic emit.
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, TSidebarWidget* widget);

private Q_SLOTS:
    /// CIP (via onCharacterConfigurationLoaded).
    void onCharacterConfigurationLoaded();
    /// CIP onItemDropTracked .
    void onItemDropTracked(const cyclopedia::TTrackedItem& trackedItem, const cyclopedia::TDroppedLoot& loot);
    /// CIP onRecentLootDropsChanged .
    void onRecentLootDropsChanged();
    /// CIP onAutoTrackLootValueChanged .
    void onAutoTrackLootValueChanged(const QString& text);
    /// CIP onWidgetOptionsChanged: applies the stored contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    void rebuildTrackedItems();

    analysers::TAnalyserStorages* m_storages;
    cyclopedia::TItemTrackingStorage* m_storage;
    QPointer<config::TSidebarOptions> m_options;
    /// The auto-track value prompt.
    TGenericEnterTextDialogController* m_valueDialog;
};

/// Owns the Drop Tracker widget while it is open (EWidgetType 14).
class TLootTrackingWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TLootTrackingWidgetManager(analysers::TAnalyserStorages* storages, QQuickItem* gameWindowItem, QQmlEngine* engine,
                               QObject* parent = nullptr);
    ~TLootTrackingWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close(bool saveWidgetOptions = true);
    TSidebarWidget* current() const;


Q_SIGNALS:
    /// The controller's publishGameAction, forwarded.
    void publishGameAction(const ::tibia::input::IGameActionPtr& pGameAction);

private:
    analysers::TAnalyserStorages* m_storages;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TLootTrackingWidgetController> m_controller;
};

}
}
