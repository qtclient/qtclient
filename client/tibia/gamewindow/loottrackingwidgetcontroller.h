#pragma once

#include "cyclopedia/iteminfostorage.h"
#include "gamewindow/sidebarwidgetcontrollerbase.h"

#include <cip_gen.h>

#include <QPointer>

namespace tibia::config {
class TSidebarOptions;
}

namespace tibia::gamewindow {

class TGenericEnterTextDialogController;

/// One recent drop under a tracked item (CIP: TQmlDroppedLoot, ctor 0x100cbb610).
class TQmlDroppedLoot final : public cip::tibia::gamewindow::TQmlDroppedLoot
{
    Q_OBJECT
public:
    TQmlDroppedLoot(const cyclopedia::TItemInfoStorage::DroppedLoot& drop, QObject* parent);
};

/// One tracked item (CIP: TQmlTrackedItem, built in 0x100cba950).
class TQmlTrackedItem final : public cip::tibia::gamewindow::TQmlTrackedItem
{
    Q_OBJECT
public:
    TQmlTrackedItem(const cyclopedia::TItemInfoStorage::TrackedItem& item, qint64 value, QObject* parent);

    /// CIP 0x100cba020: the new drop count.
    void setDropCount(quint32 count);
    /// CIP 0x100cbb030: the item's recent drops.
    void setLootDrops(const std::vector<cyclopedia::TItemInfoStorage::DroppedLoot>& drops);
};

/// The Drop Tracker sidebar widget, LootTrackingWidget.qml (CIP:
/// TLootTrackingWidgetController, ctor 0x100cb9130, connections 0x100cb9830).
class TLootTrackingWidgetController final : public TSidebarWidgetController<cip::tibia::gamewindow::TLootTrackingWidgetController>
{
    Q_OBJECT
public:
    TLootTrackingWidgetController(cyclopedia::TItemInfoStorage* storage, QQuickItem* gameWindowItem, QQmlEngine* engine,
                                  QObject* parent = nullptr);
    ~TLootTrackingWidgetController() override;

    /// lootTrackerWidgetOptions: contentHeight, contentMaximized (v37 / v38).
    void setSidebarOptions(config::TSidebarOptions* options);

public Q_SLOTS:
    void saveWidgetOptions() override;

public:
    /// CIP 0x100cba370: OPEN_CYCLOPEDIA on the Items tab.
    Q_INVOKABLE void requestOpenItemInfoDialog() override;
    /// CIP 0x100cba470: remove this item / remove all (the argument is the type id).
    Q_INVOKABLE void requestContextMenu(int TrackedItemIndex) override;
    /// CIP 0x100cba7a0: the auto-track value prompt.
    Q_INVOKABLE void requestConfigurationMenu() override;

private:
    /// CIP 0x100cba950 (via onCharacterConfigurationLoaded 0x100cb9f80).
    void createLootTrackingList();
    /// CIP onItemDropTracked 0x100cba020.
    void onItemDropTracked(quint16 typeId, quint32 dropCount);
    /// CIP onRecentLootDropsChanged 0x100cba0d0.
    void onRecentLootDropsChanged();
    /// CIP onAutoTrackLootValueChanged 0x100cb9520.
    void onAutoTrackLootValueChanged(const QString& text);

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
