#pragma once

#include "cyclopedia/iteminfostorage.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

namespace tibia {
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
    TQmlDroppedLoot(const cyclopedia::TItemInfoStorage::DroppedLoot& drop, QObject* parent);
};

/// One tracked item (CIP: TQmlTrackedItem, built in).
class TQmlTrackedItem final : public cip::tibia::gamewindow::TQmlTrackedItem
{
    Q_OBJECT
public:
    TQmlTrackedItem(const cyclopedia::TItemInfoStorage::TrackedItem& item, qint64 value, QObject* parent);

    /// CIP : the new drop count.
    void setDropCount(quint32 count);
    /// CIP : the item's recent drops.
    void setLootDrops(const std::vector<cyclopedia::TItemInfoStorage::DroppedLoot>& drops);
};

/// The Drop Tracker sidebar widget, LootTrackingWidget.qml (CIP:
/// TLootTrackingWidgetController, ctor , connections).
class TLootTrackingWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TLootTrackingWidgetController>
{
    Q_OBJECT

public:
    TLootTrackingWidgetController(cyclopedia::TItemInfoStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
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
    void publishGameAction();
    void widgetDragChanged(uint sidebarId, bool active);
    void widgetWantsToMoveToSidebar(int targetSidebar, TSidebarWidget* widget);

private Q_SLOTS:
    /// CIP (via onCharacterConfigurationLoaded).
    void onCharacterConfigurationLoaded();
    /// CIP onItemDropTracked .
    void onItemDropTracked(quint16 typeId, quint32 dropCount);
    /// CIP onRecentLootDropsChanged .
    void onRecentLootDropsChanged();
    /// CIP onAutoTrackLootValueChanged .
    void onAutoTrackLootValueChanged(const QString& text);
    /// CIP onWidgetOptionsChanged — persist contentHeight / contentMaximized.
    void onWidgetOptionsChanged();
    void onWidgetQuickItemDestroyed();

private:
    cyclopedia::TItemInfoStorage* m_storage;
    QPointer<config::TSidebarOptions> m_options;
    /// CIP +0xc0.
    TGenericEnterTextDialogController* m_valueDialog = nullptr;
};

/// Owns the Drop Tracker widget while it is open (EWidgetType 14).
class TLootTrackingWidgetManager final : public QObject
{
    Q_OBJECT
public:
    TLootTrackingWidgetManager(cyclopedia::TItemInfoStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                               QObject* parent = nullptr);
    ~TLootTrackingWidgetManager() override;

    TSidebarWidget* open(config::TSidebarOptions* options);
    void close();
    TSidebarWidget* current() const;

private:
    cyclopedia::TItemInfoStorage* m_storage;
    QQuickItem* m_gameWindowItem;
    QQmlEngine* m_engine;
    QPointer<TLootTrackingWidgetController> m_controller;
};

}
}
